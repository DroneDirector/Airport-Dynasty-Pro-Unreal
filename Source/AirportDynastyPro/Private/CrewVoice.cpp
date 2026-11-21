#include "CrewVoice.h"
#include "ADPLog.h"
#include "UI/ADPHUD.h"
#include "G2P.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundAttenuation.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool ADPKokoroSay(const TArray<TArray<int64>>& Sentences, const FString& VoiceName, float Speed, TArray<int16>& Pcm);

namespace ADPCrewVoice
{
	struct FAsk { TWeakObjectPtr<AActor> Who; FString Voice; };
	static TMap<int32, FAsk> Asks;
	static int32 NextId = 100000;
	void Speak(const FAsk& Ask, TArray<TArray<int64>>&& Sentences);   // (on a worker thread)

	static USoundAttenuation* Att()
	{
		static USoundAttenuation* A = nullptr;
		if (!A)
		{
			A = NewObject<USoundAttenuation>(GetTransientPackage()); A->AddToRoot();
			FSoundAttenuationSettings& S = A->Attenuation;
			S.bAttenuate = true; S.bSpatialize = true; S.AttenuationShapeExtents = FVector(120.f); S.FalloffDistance = 2800.f;
			S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound; S.dBAttenuationAtMax = -50.f;
			// (a voice through the engines' noise and round the aircraft: muffled behind things - the sound placement rule)
			S.bEnableOcclusion = true; S.OcclusionTraceChannel = ECC_Visibility; S.OcclusionLowPassFilterFrequency = 1800.f; S.OcclusionVolumeAttenuation = 0.5f;
		}
		return A;
	}

	FString VoiceFor(const FString& Name, bool bFemale)
	{
		static const TCHAR* Men[] = {TEXT("bm_george"), TEXT("bm_lewis"), TEXT("bm_daniel"), TEXT("bm_fable"), TEXT("am_michael"), TEXT("am_adam")};
		static const TCHAR* Women[] = {TEXT("bf_alice"), TEXT("bf_isabella"), TEXT("bf_lily"), TEXT("af_sarah")};
		const uint32 H = GetTypeHash(Name);
		return bFemale ? Women[H % UE_ARRAY_COUNT(Women)] : Men[H % UE_ARRAY_COUNT(Men)];
	}

	void Say(AActor* Speaker, const FString& Words, const FString& Voice)
	{
		if (!Speaker || !Speaker->GetWorld()) return;
		const APlayerController* PC = Speaker->GetWorld()->GetFirstPlayerController();
		AADPHUD* H = PC ? Cast<AADPHUD>(PC->GetHUD()) : nullptr;
		if (!H) return;
		// (the words to sound numbers in Unreal itself - G2P.cpp; the menus' pronunciation only if ours can't)
		const FAsk Ask{Speaker, Voice};
		TWeakObjectPtr<AADPHUD> Hw(H);
		Async(EAsyncExecution::Thread, [Ask, Words, Hw]()
		{
			TArray<TArray<int64>> Sn = ADPG2P::Ready() ? ADPG2P::Ids(Words) : TArray<TArray<int64>>();
			if (!Sn.Num()) return;   // (no voice pack: the subtitle only - never the web layer, owner 2026-11-21)
			Speak(Ask, MoveTemp(Sn));
		});
	}

	void OnIds(int32 Id, const FString& IdsJson)
	{
		FAsk Ask0; if (!Asks.RemoveAndCopyValue(Id, Ask0) || !Ask0.Who.IsValid()) return;
		TArray<TSharedPtr<FJsonValue>> Lines;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(IdsJson), Lines) || !Lines.Num()) return;
		TArray<TArray<int64>> Sentences;
		for (const TSharedPtr<FJsonValue>& L : Lines)
		{
			const TArray<TSharedPtr<FJsonValue>>* A; if (!L.IsValid() || !L->TryGetArray(A)) continue;
			TArray<int64>& S = Sentences.AddDefaulted_GetRef(); for (const TSharedPtr<FJsonValue>& V : *A) S.Add(int64(V->AsNumber()));
		}
		Async(EAsyncExecution::Thread, [Ask0, Sentences = MoveTemp(Sentences)]() mutable { Speak(Ask0, MoveTemp(Sentences)); });
	}

	void Speak(const FAsk& Ask, TArray<TArray<int64>>&& Sentences)
	{
		{
			TArray<int16> Pcm;
			const bool bOk = ADPKokoroSay(Sentences, Ask.Voice, 1.0f, Pcm);
			AsyncTask(ENamedThreads::GameThread, [Ask, bOk, Pcm = MoveTemp(Pcm)]()
			{
				AActor* Who = Ask.Who.Get(); if (!bOk || !Who || !Pcm.Num()) return;
				USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(Who);
				W->SetSampleRate(24000); W->NumChannels = 1; W->Duration = Pcm.Num() / 24000.f; W->SoundGroup = SOUNDGROUP_Voice; W->bLooping = false;
				W->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
				// (from the speaker's head)
				UGameplayStatics::SpawnSoundAttached(W, Who->GetRootComponent(), NAME_None, FVector(0, 0, 165.f), EAttachLocation::KeepRelativeOffset, false, 1.f, 1.f, 0.f, Att(), nullptr, true);
				ADP_LOG(TEXT("Voice: %s says %.1f s (%s)"), *Who->GetName(), Pcm.Num() / 24000.f, *Ask.Voice);
			});
		}
	}
}
