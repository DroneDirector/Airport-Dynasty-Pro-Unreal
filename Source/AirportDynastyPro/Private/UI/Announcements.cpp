// The airport's announcements, from the airline game's real departures (its timetable rows, sent over by the menus):
// boarding, delays, the final call, late passengers paged by name, and the gate closing, worded as real airports do.
// The announcer is always a woman; her voice is made by the menus (the game's natural voice, or the PC's own female
// voice) and played here through the loudspeakers: a narrow intercom sound, a little drive, the hall's echo. How loud
// she is depends on where you are: natural and clear in the public areas (the main hall, the long walkway, the gates
// upstairs and the ground-boarding gates below, food, tax-free, the bar), faint in offices, Operations and the crew
// rooms, nothing in the toilets.
//
// Late passengers are AI logic for now (no people are shown yet): at about 25 minutes before departure a flight may
// have one or two passengers not yet at the gate. They are paged by name; on hearing it they set off, walking briskly
// when there is time, running when there isn't; whoever reaches the gate before it closes boards. The gate closes 15
// minutes before departure whatever happens (the usual closing time at airports), with its own call. Passengers
// already waiting at a gate are never late.
#include "UI/ADPHUD.h"
#include "SWebBrowser.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Misc/Base64.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <sapi.h>
#include <shlwapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#pragma region Where you are
FString AADPHUD::ZoneKind() const
{
	if (bInLounge) return TEXT("lounge");
	const APawn* P = GetOwningPawn();
	if (!P) return TEXT("hall");
	const FVector L = P->GetActorLocation();
	const float X = L.X / 100.f, Z = -L.Y / 100.f, Feet = (L.Z - P->GetSimpleCollisionHalfHeight()) / 100.f;
	const FString Floor = Feet > Plan.F2Y * 0.5f ? TEXT("F2-") : TEXT("F1-");
	for (const FPlanRoom& R : Plan.Rooms)
		if (R.Id.StartsWith(Floor) && X >= R.X0 && X <= R.X1 && Z >= R.Z0 && Z <= R.Z1) return R.Kind;
	return FString();   // outside: the kerb, the apron
}

float AADPHUD::ZoneGain() const
{
	const FString K = ZoneKind();
	if (K == TEXT("wc") || K == TEXT("clean")) return 0.f;
	if (K == TEXT("office") || K == TEXT("ops") || K == TEXT("crew") || K == TEXT("academy") || K == TEXT("plant") || K == TEXT("staffentr")) return 0.1f;
	if (K == TEXT("corridor") || K == TEXT("passage")) return 0.2f;
	if (K == TEXT("lounge")) return 0.3f;
	if (K == TEXT("museum")) return 0.6f;
	if (K == TEXT("ramp")) return 0.1f;   // ramp services: staff rooms inside the building
	if (K.IsEmpty()) return 0.f;   // outside the building: not heard at all (owner, 2026-11-09)
	return 1.f;   // the hall, the entrances, security, airside, the walkway, the gates, food, tax-free, the bar…
}

float AADPHUD::ZoneMuffle() const
{
	// through a closed door and a wall only the low part of a voice comes through; in the open it is full and clear
	const FString K = ZoneKind();
	if (K == TEXT("office") || K == TEXT("ops") || K == TEXT("crew") || K == TEXT("academy") || K == TEXT("plant") || K == TEXT("staffentr")) return 700.f;
	if (K == TEXT("corridor") || K == TEXT("passage")) return 1400.f;
	if (K == TEXT("lounge")) return 1800.f;
	if (K == TEXT("museum")) return 4000.f;
	if (K == TEXT("ramp") || K.IsEmpty()) return 700.f;
	return 20000.f;
}
#pragma endregion

#pragma region The loudspeakers
namespace
{
	struct FBiquad
	{
		float B0 = 1, B1 = 0, B2 = 0, A1 = 0, A2 = 0, X1 = 0, X2 = 0, Y1 = 0, Y2 = 0;
		static FBiquad Make(bool bHigh, float Hz, float Rate, float Q = 0.707f)
		{
			FBiquad F;
			const float W = 2 * PI * Hz / Rate, C = FMath::Cos(W), A = FMath::Sin(W) / (2 * Q), N = 1 + A;
			if (bHigh) { F.B0 = (1 + C) / 2 / N; F.B1 = -(1 + C) / N; F.B2 = (1 + C) / 2 / N; }
			else { F.B0 = (1 - C) / 2 / N; F.B1 = (1 - C) / N; F.B2 = (1 - C) / 2 / N; }
			F.A1 = -2 * C / N; F.A2 = (1 - A) / N;
			return F;
		}
		float Run(float X) { const float Y = B0 * X + B1 * X1 + B2 * X2 - A1 * Y1 - A2 * Y2; X2 = X1; X1 = X; Y2 = Y1; Y1 = Y; return Y; }
	};

	/** A voice through a public-address system: the speaker's narrow band, a little drive, a far loudspeaker's echo and
	 *  the hall's reverberation. 24 kHz mono in and out, with a tail added for the echo to die away. */
	TArray<int16> Intercom(const TArray<int16>& In, int32 Rate)
	{
		const int32 Tail = Rate * 1.4f, N = In.Num() + Tail;
		TArray<float> X; X.SetNumZeroed(N);
		FBiquad Hp = FBiquad::Make(true, 330.f, Rate), Hp2 = FBiquad::Make(true, 330.f, Rate), Lp = FBiquad::Make(false, 3600.f, Rate), Lp2 = FBiquad::Make(false, 3600.f, Rate);
		const float Drive = 1.9f, Norm = 1.f / FMath::Tanh(Drive);
		for (int32 i = 0; i < In.Num(); ++i)
		{
			float S = In[i] / 32768.f;
			S = Lp2.Run(Lp.Run(Hp2.Run(Hp.Run(S))));
			X[i] = FMath::Tanh(S * Drive * 1.6f) * Norm * 0.8f;
		}
		// a second loudspeaker further down the hall
		const int32 Slap = Rate * 0.13f;
		TArray<float> Y = X;
		for (int32 i = Slap; i < N; ++i) Y[i] += X[i - Slap] * 0.09f;
		// the hall: four damped combs and two all-passes (Schroeder)
		const int32 CombLen[4] = {int32(Rate * 0.0353f), int32(Rate * 0.0367f), int32(Rate * 0.0338f), int32(Rate * 0.0322f)};
		TArray<float> Wet; Wet.SetNumZeroed(N);
		for (int32 c = 0; c < 4; ++c)
		{
			TArray<float> Buf; Buf.SetNumZeroed(CombLen[c] * 3);
			const int32 L = Buf.Num();
			float Damp = 0;
			for (int32 i = 0, j = 0; i < N; ++i, j = (j + 1) % L)
			{
				const float Out = Buf[j];
				Damp = Out * 0.6f + Damp * 0.4f;
				Buf[j] = Y[i] + Damp * 0.62f;   // a short tail (owner, 2026-11-09: half the reverb of the first version)
				Wet[i] += Out * 0.25f;
			}
		}
		for (const float Len : {0.0051f, 0.0126f})
		{
			TArray<float> Buf; Buf.SetNumZeroed(FMath::Max(1, int32(Rate * Len)));
			for (int32 i = 0, j = 0; i < N; ++i, j = (j + 1) % Buf.Num())
			{
				const float B = Buf[j], In2 = Wet[i];
				Buf[j] = In2 + B * 0.5f;
				Wet[i] = B - In2 * 0.5f;
			}
		}
		TArray<int16> Out; Out.SetNumUninitialized(N);
		float Peak = 0.01f;
		for (int32 i = 0; i < N; ++i) { Y[i] = Y[i] * 0.95f + Wet[i] * 0.085f; Peak = FMath::Max(Peak, FMath::Abs(Y[i])); }
		const float G = 0.85f / Peak;
		for (int32 i = 0; i < N; ++i) Out[i] = int16(FMath::Clamp(Y[i] * G, -1.f, 1.f) * 32767);
		return Out;
	}
	/** Windows' own speech (SAPI): a female English voice (British first), 24 kHz 16-bit mono, into memory. */
	TArray<int16> WindowsVoice(const FString& Words)
	{
		TArray<int16> Out;
		if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return Out;
		ISpVoice* Voice = nullptr; ISpObjectTokenCategory* Cat = nullptr; IEnumSpObjectTokens* List = nullptr; ISpObjectToken* Token = nullptr; ISpStream* Stream = nullptr; IStream* Mem = nullptr;
		if (SUCCEEDED(CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void**)&Voice)))
		{
			if (SUCCEEDED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, IID_ISpObjectTokenCategory, (void**)&Cat)) && SUCCEEDED(Cat->SetId(SPCAT_VOICES, 0)))
			{
				for (const wchar_t* Want : {L"Gender=Female;Language=809", L"Gender=Female;Language=409", L"Gender=Female"})
				{
					if (SUCCEEDED(Cat->EnumTokens(Want, nullptr, &List)) && List && List->Next(1, &Token, nullptr) == S_OK) break;
					if (List) { List->Release(); List = nullptr; }
				}
				if (Token) Voice->SetVoice(Token);
			}
			Voice->SetRate(-1);
			WAVEFORMATEX F = {}; F.wFormatTag = WAVE_FORMAT_PCM; F.nChannels = 1; F.nSamplesPerSec = 24000; F.wBitsPerSample = 16; F.nBlockAlign = 2; F.nAvgBytesPerSec = 48000;
			Mem = SHCreateMemStream(nullptr, 0);
			if (Mem && SUCCEEDED(CoCreateInstance(CLSID_SpStream, nullptr, CLSCTX_ALL, IID_ISpStream, (void**)&Stream)) && SUCCEEDED(Stream->SetBaseStream(Mem, SPDFID_WaveFormatEx, &F)) && SUCCEEDED(Voice->SetOutput(Stream, 1)))
			{
				Voice->Speak(*Words, SPF_DEFAULT, nullptr);
				STATSTG St = {};
				LARGE_INTEGER Zero = {};
				if (SUCCEEDED(Mem->Stat(&St, STATFLAG_NONAME)) && SUCCEEDED(Mem->Seek(Zero, STREAM_SEEK_SET, nullptr)))
				{
					Out.SetNumUninitialized(int32(St.cbSize.QuadPart / 2));
					ULONG Got = 0;
					Mem->Read(Out.GetData(), Out.Num() * 2, &Got);
					Out.SetNum(Got / 2);
				}
			}
		}
		if (Stream) Stream->Release(); if (Mem) Mem->Release(); if (Token) Token->Release(); if (List) List->Release(); if (Cat) Cat->Release(); if (Voice) Voice->Release();
		CoUninitialize();
		return Out;
	}
}

void AADPHUD::SpeechPcm(int32 Id, const FString& B64)
{
	if (Id != SpeechId) return;   // an older line, overtaken
	TArray<uint8> Bytes;
	if (!FBase64::Decode(B64, Bytes) || Bytes.Num() < 4) return;
	TArray<int16> Pcm; Pcm.SetNumUninitialized(Bytes.Num() / 2);
	FMemory::Memcpy(Pcm.GetData(), Bytes.GetData(), Pcm.Num() * 2);
	PlayAnnouncer(Id, Pcm);
}

void AADPHUD::SpeechFallback(int32 Id)
{
	if (Id != SpeechId) return;
	UE_LOG(LogTemp, Log, TEXT("Announcer: line %d with Windows' voice"), Id);
	const FString Words = SpokenText;
	TWeakObjectPtr<AADPHUD> Me(this);
	Async(EAsyncExecution::Thread, [Me, Id, Words]()
	{
		TArray<int16> Pcm = WindowsVoice(Words);
		UE_LOG(LogTemp, Log, TEXT("Announcer: Windows' voice made %d samples"), Pcm.Num());
		AsyncTask(ENamedThreads::GameThread, [Me, Id, Pcm = MoveTemp(Pcm)]() { if (Me.IsValid() && Pcm.Num()) Me->PlayAnnouncer(Id, Pcm); });
	});
}

void AADPHUD::PlayAnnouncer(int32 Id, const TArray<int16>& Pcm)
{
	if (Id != SpeechId) return;
	const TArray<int16> Pa = Intercom(Pcm, 24000);
	UE_LOG(LogTemp, Log, TEXT("Announcer: %.1f s of voice, heard at %.0f%% here (%s)"), Pcm.Num() / 24000.f, CallGain() * 100.f, *ZoneKind());
	// -UICall: the call as heard in the main hall (the chime, then the announcer) also written to Saved/Announcer.wav
	if (FParse::Param(FCommandLine::Get(), TEXT("UICall")))
	{
		TArray<int16> All;
		for (const float Hz : {784.f, 523.f})
			for (int32 i = 0; i < 24000 * 0.8f; ++i)
			{
				const float T = i / 24000.f, Env = FMath::Exp(-T * 3.2f) * FMath::Min(1.f, T * 80.f);
				All.Add(int16((FMath::Sin(2 * PI * Hz * T) + 0.3f * FMath::Sin(4 * PI * Hz * T)) * Env * 0.35f * 32767));
			}
		All.AddZeroed(24000 / 10);
		All.Append(Pa);
		TArray<uint8> W;
		auto U32 = [&W](uint32 V) { W.Append(reinterpret_cast<uint8*>(&V), 4); };
		auto U16 = [&W](uint16 V) { W.Append(reinterpret_cast<uint8*>(&V), 2); };
		W.Append((const uint8*)"RIFF", 4); U32(36 + All.Num() * 2); W.Append((const uint8*)"WAVEfmt ", 8); U32(16); U16(1); U16(1); U32(24000); U32(48000); U16(2); U16(16);
		W.Append((const uint8*)"data", 4); U32(All.Num() * 2); W.Append(reinterpret_cast<const uint8*>(All.GetData()), All.Num() * 2);
		FFileHelper::SaveArrayToFile(W, *(FPaths::ProjectSavedDir() / TEXT("Announcer.wav")));
	}
	USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
	W->SetSampleRate(24000);
	W->NumChannels = 1;
	W->Duration = Pa.Num() / 24000.f;
	W->SoundGroup = SOUNDGROUP_Voice;
	W->bLooping = false;
	W->QueueAudio(reinterpret_cast<const uint8*>(Pa.GetData()), Pa.Num() * sizeof(int16));
	if (VoiceOut) VoiceOut->Stop();
	VoiceOut = UGameplayStatics::CreateSound2D(this, W, 1.f, 1.f, 0.f, nullptr, false, false);
	if (VoiceOut) { VoiceOut->SetVolumeMultiplier(FMath::Max(0.001f, HeardGain)); VoiceOut->Play(); }
	CaptionTime = FMath::Max(CaptionTime, W->Duration + 1.f);
}

void AADPHUD::Speak(const FString& Words)
{
	if (!Browser.IsValid()) { UE_LOG(LogTemp, Warning, TEXT("Announcer: no menus page to make the voice")); return; }
	++SpeechId;
	UE_LOG(LogTemp, Log, TEXT("Announcer: line %d asked for"), SpeechId);
	// flight numbers as announcers say them: letter by letter, digit by digit (ATN201: "A T N 2 0 1")
	FString Said = Words;
	{
		FRegexMatcher M(FRegexPattern(TEXT("\\b([A-Z]{2,3} ?\\d{1,4})\\b")), Words);
		TArray<FString> Codes;
		while (M.FindNext()) Codes.AddUnique(M.GetCaptureGroup(1));
		for (const FString& C : Codes)
		{
			FString Sp;
			for (const TCHAR Ch : C) if (Ch != TEXT(' ')) { Sp.AppendChar(Ch); Sp.AppendChar(TEXT(' ')); }
			Said = Said.Replace(*C, *Sp.TrimEnd(), ESearchCase::CaseSensitive);
		}
	}
	SpokenText = Said;
	FString Safe = Said.Replace(TEXT("\\"), TEXT(" ")).Replace(TEXT("'"), TEXT("\\'"));
	// the menus make the voice; if their speech code isn't there (or fails at once), Windows' voice is asked for
	Browser->ExecuteJavascript(FString::Printf(TEXT("try { if (window.__ueSay) window.__ueSay(%d, '%s', %.2f); else window.ue.adp.speechfail(%d); } catch (e) { window.ue.adp.speechfail(%d); }"), SpeechId, *Safe, ZoneGain(), SpeechId, SpeechId));
}
#pragma endregion

#pragma region The calls
namespace
{
	int32 Minutes(const FString& HhMm)
	{
		FString H, M;
		if (!HhMm.Split(TEXT(":"), &H, &M) || !H.IsNumeric()) return -1;
		return FCString::Atoi(*H) * 60 + FCString::Atoi(*M.Left(2));
	}
	FString Clock(int32 Min) { Min = (Min % 1440 + 1440) % 1440; return FString::Printf(TEXT("%02d:%02d"), Min / 60, Min % 60); }

	FString PassengerName(uint32 Seed)
	{
		static const TCHAR* Women[] = {TEXT("Anna"), TEXT("Sofie"), TEXT("Emma"), TEXT("Maria"), TEXT("Sarah"), TEXT("Laura"), TEXT("Hanna"), TEXT("Julia"), TEXT("Elin"), TEXT("Charlotte"), TEXT("Olivia"), TEXT("Ingrid")};
		static const TCHAR* Men[] = {TEXT("Jon"), TEXT("Peter"), TEXT("Michael"), TEXT("David"), TEXT("Thomas"), TEXT("Andreas"), TEXT("James"), TEXT("Lukas"), TEXT("Rasmus"), TEXT("Oliver"), TEXT("Daniel"), TEXT("Erik")};
		static const TCHAR* Last[] = {TEXT("Hansen"), TEXT("Jacobsen"), TEXT("Olsen"), TEXT("Petersen"), TEXT("Smith"), TEXT("Johnson"), TEXT("Müller"), TEXT("Andersson"), TEXT("Nielsen"), TEXT("Brown"), TEXT("Joensen"), TEXT("Poulsen"), TEXT("Davies"), TEXT("Larsen"), TEXT("Sigurdsson"), TEXT("Thomsen")};
		const bool bWoman = (Seed & 1) != 0;
		const FString First = bWoman ? Women[(Seed >> 1) % 12] : Men[(Seed >> 1) % 12];
		return FString::Printf(TEXT("%s %s %s"), bWoman ? TEXT("Ms") : TEXT("Mr"), *First, Last[(Seed >> 5) % 16]);
	}
}

void AADPHUD::OnGameFids(const FString& Json)
{
	TSharedPtr<FJsonObject> O;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), O) || !O.IsValid()) return;
	const TArray<TSharedPtr<FJsonValue>>* Dep = nullptr;
	if (!O->TryGetArrayField(TEXT("dep"), Dep)) return;
	const FDateTime Now = FDateTime::Now();
	const int32 NowMin = Now.GetHour() * 60 + Now.GetMinute();
	for (const TSharedPtr<FJsonValue>& V : *Dep)
	{
		const TSharedPtr<FJsonObject>* R = nullptr;
		if (!V.IsValid() || !V->TryGetObject(R)) continue;
		FString Flight, Place, Status;
		(*R)->TryGetStringField(TEXT("flight"), Flight);
		(*R)->TryGetStringField(TEXT("place"), Place);
		(*R)->TryGetStringField(TEXT("status"), Status);
		// the times come as moments (milliseconds since 1970, UTC): the minute of the day here
		double SchedMs = 0, ExpMs = 0;
		if (!(*R)->TryGetNumberField(TEXT("sched"), SchedMs) || Flight.IsEmpty()) continue;
		if (!(*R)->TryGetNumberField(TEXT("exp"), ExpMs)) ExpMs = SchedMs;
		const FTimespan Local = FDateTime::Now() - FDateTime::UtcNow();
		auto MinOf = [&](double Ms) { const FDateTime T = FDateTime(1970, 1, 1) + FTimespan::FromMilliseconds(Ms) + Local; return T.GetHour() * 60 + T.GetMinute(); };
		const FString Sched = Clock(MinOf(SchedMs)), Exp = Clock(MinOf(ExpMs));
		// the place as people say it: "EKCH Copenhagen" is said "Copenhagen"
		{ FString Code, Name; if (Place.Split(TEXT(" "), &Code, &Name) && Code.Len() >= 3 && Code.Len() <= 4 && Code.ToUpper() == Code && !Name.IsEmpty()) Place = Name; }
		const bool bNew = !Departures.Contains(Flight);
		FDeparture& D = Departures.FindOrAdd(Flight);
		D.Place = Place;
		D.Sched = Minutes(Sched);
		const int32 Was = D.Exp;
		D.Exp = Minutes(Exp) >= 0 ? Minutes(Exp) : D.Sched;
		D.Gate = int32(GetTypeHash(Flight) % 8) + 1;
		const FString S = Status.ToLower();
		if (bNew)
		{
			// what has already happened isn't called again: only what comes from now on
			const int32 Left = ((D.Exp - NowMin) % 1440 + 2160) % 1440 - 720;
			if (Left <= 40) D.bBoarding = true;
			if (Left <= 25) D.bPaged = true;
			if (Left <= 20) D.bFinal = true;
			if (Left <= 15) D.bClosed = true;
			D.Status = S;
			continue;
		}
		if (S != D.Status)
		{
			if (S.Contains(TEXT("cancel")))
				Announce(FString::Printf(TEXT("%s regrets to announce that flight %s to %s has been cancelled. Passengers are requested to contact the %s service desk in the main hall."), *AirlineName(), *Flight, *Place, *AirlineName()));
			else if (S.Contains(TEXT("delay")) && D.Exp != Was)
				Announce(FString::Printf(TEXT("%s regrets to announce that flight %s to %s is delayed. The new expected departure time is %s. We apologise for any inconvenience."), *AirlineName(), *Flight, *Place, *Clock(D.Exp)));
			D.Status = S;
		}
	}
}

FString AADPHUD::AirlineName() const { return Airline.IsEmpty() ? FString(TEXT("Airport Dynasty")) : Airline; }

void AADPHUD::RunCalls()
{
	auto Announce2 = [](const FString& W) { Announce(W, 2); };
	const FDateTime Now = FDateTime::Now();
	const float NowMin = Now.GetHour() * 60 + Now.GetMinute() + Now.GetSecond() / 60.f;
	const FString Who = AirlineName();
	// now and then (every half hour to an hour): a passenger asked to the information desk, or a notice for everyone
	const float T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (NextInfoAt < 0) NextInfoAt = T + FMath::FRandRange(25.f, 50.f) * 60.f;
	else if (T >= NextInfoAt)
	{
		NextInfoAt = T + FMath::FRandRange(30.f, 60.f) * 60.f;
		const int32 Turn = InfoTurn++;
		// a passenger with plenty of time: their flight leaves an hour or more from now
		FString Place;
		for (const TPair<FString, FDeparture>& It : Departures) { const float L = FMath::Fmod(It.Value.Exp - NowMin + 2160.f, 1440.f) - 720.f; if (L >= 60.f) { Place = It.Value.Place; break; } }
		if (Turn % 2 == 0)
		{
			const FString Name = PassengerName(GetTypeHash(Now.ToString()) ^ 0x5bd1e995u);
			Announce(Place.IsEmpty()
				? FString::Printf(TEXT("This is a passenger announcement. Would %s please make your way to the information desk in the main hall, where a message is waiting for you. Thank you."), *Name)
				: FString::Printf(TEXT("This is a passenger announcement. Would passenger %s, travelling to %s, please contact the information desk in the main hall. Thank you."), *Name, *Place), 2);
		}
		else
		{
			static const TCHAR* Notices[] = {
				TEXT("For safety and security reasons, please do not leave your baggage unattended at any time. Unattended baggage may be removed and destroyed by security."),
				TEXT("Ladies and gentlemen, smoking, including electronic cigarettes, is not permitted anywhere inside the terminal building. Thank you for your cooperation."),
				TEXT("Passengers are reminded that liquids in hand luggage must be in containers of one hundred millilitres or less, carried in a single clear plastic bag."),
				TEXT("A black wallet has been handed in near the check-in desks. Would the owner please contact the information desk in the main hall."),
				TEXT("Please keep your personal belongings with you at all times, and report any suspicious items to airport staff."),
			};
			Announce(Notices[(Turn / 2) % UE_ARRAY_COUNT(Notices)], 3);
		}
	}
	for (TPair<FString, FDeparture>& It : Departures)
	{
		FDeparture& D = It.Value;
		const FString& F = It.Key;
		float Left = FMath::Fmod(D.Exp - NowMin + 2160.f, 1440.f) - 720.f;
		if (Left < -30.f) continue;
		if (!D.bBoarding && Left <= 40.f)
		{
			D.bBoarding = true;
			Announce(FString::Printf(TEXT("Ladies and gentlemen, %s flight %s to %s is now boarding at gate %d. Please have your boarding pass and passport ready."), *Who, *F, *D.Place, D.Gate));
		}
		if (!D.bPaged && Left <= 25.f)
		{
			D.bPaged = true;
			// one or two passengers not yet at the gate (some flights have none); the same each time for this flight today
			const uint32 Seed = GetTypeHash(F + Now.ToString(TEXT("%Y%m%d")));
			const int32 Count = Seed % 100 < 45 ? 1 : Seed % 100 < 60 ? 2 : 0;
			for (int32 i = 0; i < Count; ++i)
			{
				FLatePassenger P;
				const uint32 S2 = Seed * 2654435761u + i * 97u;
				P.Name = PassengerName(S2);
				// how far they are from the gate when they hear it: two to sixteen minutes' brisk walk
				const float WalkMin = 2.f + (S2 >> 8) % 1500 / 100.f;
				const float Time = Left - 15.f;   // until the gate closes
				P.bRun = WalkMin > Time * 0.75f;
				P.ArriveAt = NowMin + (P.bRun ? WalkMin * 0.55f : WalkMin);
				D.Late.Add(P);
				Announce2(FString::Printf(TEXT("This is a passenger announcement. Would passenger %s, travelling to %s on %s flight %s, please proceed immediately to gate %d, where your flight is ready to depart."), *P.Name, *D.Place, *Who, *F, D.Gate));
			}
		}
		for (FLatePassenger& P : D.Late) if (!P.bBoarded && !D.bClosed && NowMin >= P.ArriveAt) P.bBoarded = true;
		if (!D.bFinal && Left <= 20.f)
		{
			D.bFinal = true;
			FString Names;
			for (const FLatePassenger& P : D.Late) if (!P.bBoarded) Names += (Names.IsEmpty() ? TEXT("") : TEXT(" and ")) + P.Name;
			Announce(FString::Printf(TEXT("This is the final call for %s flight %s to %s. %sWould all remaining passengers please proceed immediately to gate %d. This gate will close in five minutes."),
				*Who, *F, *D.Place, Names.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("Would %s please make your way to gate %d now. "), *Names, D.Gate), D.Gate));
		}
		if (!D.bClosed && Left <= 15.f)
		{
			D.bClosed = true;
			Announce(FString::Printf(TEXT("%s flight %s to %s is now closed. Gate %d is closed."), *Who, *F, *D.Place, D.Gate));
			for (const FLatePassenger& P : D.Late)
				UE_LOG(LogTemp, Log, TEXT("Flight %s: %s %s"), *F, *P.Name, P.bBoarded ? (P.bRun ? TEXT("ran to the gate and boarded") : TEXT("walked to the gate and boarded")) : TEXT("did not reach the gate in time"));
		}
	}
}
#pragma endregion

#pragma region The preview pack (-UIPack)
// One of each kind of call, made with the announcer's voice and the loudspeakers as heard in the main hall, written to
// Saved/AnnouncementPack.wav (the chime before each, a pause between), for listening to outside the game.
void AADPHUD::WritePreviewPack()
{
	const TCHAR* Lines[] = {
		TEXT("Ladies and gentlemen, Atlantic North flight A T N 2 0 1 to Copenhagen is now boarding at gate 3. Please have your boarding pass and passport ready."),
		TEXT("Atlantic North regrets to announce that flight A T N 5 4 0 to Edinburgh is delayed. The new expected departure time is 14:35. We apologise for any inconvenience."),
		TEXT("This is a passenger announcement. Would passenger Mr Jon Hansen, travelling to Oslo on Atlantic North flight A T N 3 1 8, please proceed immediately to gate 6, where your flight is ready to depart."),
		TEXT("This is the final call for Atlantic North flight A T N 3 1 8 to Oslo. Would Mr Jon Hansen please make your way to gate 6 now. Would all remaining passengers please proceed immediately to gate 6. This gate will close in five minutes."),
		TEXT("Atlantic North flight A T N 3 1 8 to Oslo is now closed. Gate 6 is closed."),
		TEXT("This is a passenger announcement. Would passenger Ms Sofie Joensen, travelling to Reykjavik, please contact the information desk in the main hall. Thank you."),
		TEXT("For safety and security reasons, please do not leave your baggage unattended at any time. Unattended baggage may be removed and destroyed by security."),
	};
	TArray<int16> All;
	for (const TCHAR* L : Lines)
	{
		for (const float Hz : {784.f, 523.f})
			for (int32 i = 0; i < 24000 * 0.8f; ++i)
			{
				const float T = i / 24000.f, Env = FMath::Exp(-T * 3.2f) * FMath::Min(1.f, T * 80.f);
				All.Add(int16((FMath::Sin(2 * PI * Hz * T) + 0.3f * FMath::Sin(4 * PI * Hz * T)) * Env * 0.35f * 32767));
			}
		All.AddZeroed(24000 / 10);
		// Windows' speech runs on a thread of its own (it can't start on the game's thread)
		const FString Line = L;
		All.Append(Intercom(Async(EAsyncExecution::Thread, [Line]() { return WindowsVoice(Line); }).Get(), 24000));
		All.AddZeroed(24000);
	}
	TArray<uint8> W;
	auto U32 = [&W](uint32 V) { W.Append(reinterpret_cast<uint8*>(&V), 4); };
	auto U16 = [&W](uint16 V) { W.Append(reinterpret_cast<uint8*>(&V), 2); };
	W.Append((const uint8*)"RIFF", 4); U32(36 + All.Num() * 2); W.Append((const uint8*)"WAVEfmt ", 8); U32(16); U16(1); U16(1); U32(24000); U32(48000); U16(2); U16(16);
	W.Append((const uint8*)"data", 4); U32(All.Num() * 2); W.Append(reinterpret_cast<const uint8*>(All.GetData()), All.Num() * 2);
	FFileHelper::SaveArrayToFile(W, *(FPaths::ProjectSavedDir() / TEXT("AnnouncementPack.wav")));
	UE_LOG(LogTemp, Log, TEXT("Announcement pack written: %.0f s"), All.Num() / 24000.f);
}
#pragma endregion
