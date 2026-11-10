// The announcer's natural voice made by Unreal itself: Kokoro-82M (Apache License 2.0, the quantised ONNX build from
// kokoro-onnx, MIT) run on the processor through Unreal's Neural Network Engine (ONNX Runtime). The voice pack ships
// with the game in Content/Voice (Tools/GetVoicePack.ps1); the menus turn the words into the model's sound numbers
// (the game's own pronunciation rules and misaki's British dictionary) and hand them over sentence by sentence.
// Without the pack, or if the model can't run on this PC, the line goes to Windows' own female voice instead.
#include "UI/ADPHUD.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "UObject/StrongObjectPtr.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ADPKokoro
{
	static FCriticalSection Lock;
	static TStrongObjectPtr<UNNEModelData> Data;
	static TSharedPtr<UE::NNE::IModelInstanceCPU> Model;
	static TArray<float> Style;          // the voice (bf_emma): 510 x 256 floats, one row per sentence length
	static int32 State = 0;              // 0 not tried, 1 ready, -1 not available

	static FString Dir() { return FPaths::ProjectContentDir() / TEXT("Voice"); }

	/** The model and the voice, loaded once (on the speaking thread; the model data object on the game thread). */
	static bool Load()
	{
		if (State) return State > 0;
		State = -1;
		TArray<uint8> Onnx, Voice;
		// the full-precision model (several times faster on a PC's processor than the 8-bit one), else the 8-bit one
		const bool bFull = FFileHelper::LoadFileToArray(Onnx, *(Dir() / TEXT("kokoro-v1.0.onnx")));
		if ((!bFull && !FFileHelper::LoadFileToArray(Onnx, *(Dir() / TEXT("kokoro-v1.0-q8.onnx")))) || !FFileHelper::LoadFileToArray(Voice, *(Dir() / TEXT("voice-bf_emma.bin"))))
		{ UE_LOG(LogTemp, Warning, TEXT("Kokoro: the voice pack isn't in %s"), *Dir()); return false; }
		TWeakInterfacePtr<INNERuntimeCPU> Rt = UE::NNE::GetRuntime<INNERuntimeCPU>(TEXT("NNERuntimeORTCpu"));
		if (!Rt.IsValid()) { UE_LOG(LogTemp, Warning, TEXT("Kokoro: no ONNX runtime")); return false; }
		FEvent* Done = FPlatformProcess::GetSynchEventFromPool();
		AsyncTask(ENamedThreads::GameThread, [&Onnx, Done]()
		{
			UNNEModelData* D = NewObject<UNNEModelData>();
			D->Init(TEXT("onnx"), Onnx);
			Data.Reset(D);
			Done->Trigger();
		});
		Done->Wait();
		FPlatformProcess::ReturnSynchEventToPool(Done);
		if (Rt->CanCreateModelCPU(Data.Get()) == UE::NNE::EResultStatus::Ok)
			if (TSharedPtr<UE::NNE::IModelCPU> M = Rt->CreateModelCPU(Data.Get()))
				Model = M->CreateModelInstanceCPU();
		if (!Model.IsValid()) { UE_LOG(LogTemp, Warning, TEXT("Kokoro: the model couldn't be made")); return false; }
		Style.SetNumUninitialized(Voice.Num() / 4);
		FMemory::Memcpy(Style.GetData(), Voice.GetData(), Style.Num() * 4);
		if (Style.Num() < 256) return false;
		State = 1;
		UE_LOG(LogTemp, Log, TEXT("Kokoro: ready (%s model, %d inputs)"), bFull ? TEXT("full") : TEXT("8-bit"), Model->GetInputTensorDescs().Num());
		return true;
	}

	/** One sentence (the model's sound numbers) to 24 kHz sound. */
	static bool Synth(const TArray<int64>& Ids, float Speed, TArray<float>& Out)
	{
		const int32 N = FMath::Min(Ids.Num(), 509);
		TArray<int64> Tok; Tok.SetNumZeroed(N + 2);
		for (int32 i = 0; i < N; ++i) Tok[i + 1] = Ids[i];
		const int32 Row = FMath::Min(N, Style.Num() / 256 - 1);
		TArray<float> St; St.Append(Style.GetData() + Row * 256, 256);
		float Sp = Speed;
		// the inputs in the model's own order, by name (tokens, style, speed)
		TArray<UE::NNE::FTensorShape> Shapes;
		TArray<UE::NNE::FTensorBindingCPU> In;
		for (const UE::NNE::FTensorDesc& D : Model->GetInputTensorDescs())
		{
			const FString Nm = D.GetName().ToLower();
			if (Nm.Contains(TEXT("token")) || Nm.Contains(TEXT("input_ids"))) { const uint32 S[] = {1u, uint32(N + 2)}; Shapes.Add(UE::NNE::FTensorShape::Make(S)); In.Add({Tok.GetData(), uint64(Tok.Num()) * 8}); }
			else if (Nm.Contains(TEXT("style"))) { const uint32 S[] = {1u, 256u}; Shapes.Add(UE::NNE::FTensorShape::Make(S)); In.Add({St.GetData(), 256 * 4}); }
			else { const uint32 S[] = {1u}; Shapes.Add(UE::NNE::FTensorShape::Make(S)); In.Add({&Sp, 4}); }
		}
		if (Model->SetInputTensorShapes(Shapes) != UE::NNE::EResultStatus::Ok) return false;
		// the sound's length is only known after the run: room for far more than a sentence can need
		Out.SetNumZeroed(24000 * 2 + (N + 2) * 4800);
		UE::NNE::FTensorBindingCPU Res{Out.GetData(), uint64(Out.Num()) * 4};
		if (Model->RunSync(In, MakeArrayView(&Res, 1)) != UE::NNE::EResultStatus::Ok) return false;
		const TConstArrayView<UE::NNE::FTensorShape> OS = Model->GetOutputTensorShapes();
		const int32 Len = OS.Num() ? int32(OS[0].Volume()) : 0;
		if (Len <= 0 || Len > Out.Num()) return false;
		Out.SetNum(Len);
		return true;
	}
}

void AADPHUD::KokoroSpeak(int32 Id, const FString& IdsJson)
{
	if (Id != SpeechId) return;
	TArray<TSharedPtr<FJsonValue>> Lines;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(IdsJson), Lines) || !Lines.Num()) { SpeechFallback(Id); return; }
	TArray<TArray<int64>> Sentences;
	for (const TSharedPtr<FJsonValue>& L : Lines)
	{
		const TArray<TSharedPtr<FJsonValue>>* A;
		if (!L.IsValid() || !L->TryGetArray(A)) continue;
		TArray<int64>& S = Sentences.AddDefaulted_GetRef();
		for (const TSharedPtr<FJsonValue>& V : *A) S.Add(int64(V->AsNumber()));
	}
	TWeakObjectPtr<AADPHUD> Me(this);
	Async(EAsyncExecution::Thread, [Me, Id, Sentences = MoveTemp(Sentences)]()
	{
		TArray<int16> Pcm;
		bool bOk = false;
		{
			FScopeLock Guard(&ADPKokoro::Lock);
			if (ADPKokoro::Load())
			{
				bOk = true;
				const double T0 = FPlatformTime::Seconds();
				for (const TArray<int64>& S : Sentences)
				{
					if (!S.Num()) continue;
					TArray<float> A;
					if (!ADPKokoro::Synth(S, 0.94f, A)) { bOk = false; break; }
					for (const float V : A) Pcm.Add(int16(FMath::Clamp(V, -1.f, 1.f) * 32767.f));
					Pcm.AddZeroed(int32(24000 * 0.32f));   // the pause between sentences
				}
				UE_LOG(LogTemp, Log, TEXT("Kokoro: line %d, %.1f s of voice in %.2f s"), Id, Pcm.Num() / 24000.f, FPlatformTime::Seconds() - T0);
			}
		}
		AsyncTask(ENamedThreads::GameThread, [Me, Id, bOk, Pcm = MoveTemp(Pcm)]()
		{
			if (!Me.IsValid()) return;
			if (bOk && Pcm.Num()) Me->PlayAnnouncer(Id, Pcm); else Me->SpeechFallback(Id);
		});
	});
}
