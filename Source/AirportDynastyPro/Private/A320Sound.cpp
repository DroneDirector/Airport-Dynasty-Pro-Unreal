// The A320's sounds (A320Sound.h): FlightGear's A320-family mix, carried into the game.
#include "A320Sound.h"
#include "A320Parts.h"
#include "BabylonModel.h"
#include "Turnaround.h"
#include "ADPKeep.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/WeakObjectPtrTemplates.h"

namespace ADPA320Snd
{
	namespace SndData
	{
		// the model's frame (cm from the nose wheel's ground point: x right, y aft, z up - SimTraffic's Haze, A320Parts):
		// the CFM56s' fans and their cores' ends, the gear bays, the flap tracks, the APU's exhaust, the packs' bay, the cabin
		const float EngX[2] = {-562.f, 562.f};
		const FVector FanAt(0.f, 760.f, 179.f), CoreAt(0.f, 1150.f, 179.f), RevAt(0.f, 950.f, 179.f);
		const FVector GearBayAt(0.f, 1180.f, 260.f), MainWheelAt(0.f, 1260.f, 40.f), NoseWheelAt(0.f, 0.f, 60.f);
		const FVector FlapAt(0.f, 1320.f, 290.f), ApuOutAt(0.f, 3300.f, 470.f), PackBayAt(0.f, 1050.f, 120.f), CabinAt(0.f, 1170.f, 520.f);
		// the passenger doors (A320Parts' hinges) - an announcement is heard just outside one that is open
		const FVector PaxDoorAt[4] = {FVector(-160.f, -64.f, 425.f), FVector(160.f, -64.f, 425.f), FVector(-155.f, 2400.f, 436.f), FVector(155.f, 2400.f, 436.f)};
		// the cabin's inside, cockpit to the rear galley
		const FBox CabinBox(FVector(-195.f, -480.f, 300.f), FVector(195.f, 2650.f, 580.f));

		// FlightGear's tables (Systems/cfm56-sound.xml, the outside view; N1 / N2 in %)
		struct FTab { int32 Num; float X[7]; float Y[7]; };
		float Look(const FTab& T, float In)
		{
			if (In <= T.X[0]) return T.Y[0];
			for (int32 i = 1; i < T.Num; ++i) if (In <= T.X[i]) return FMath::Lerp(T.Y[i - 1], T.Y[i], (In - T.X[i - 1]) / FMath::Max(T.X[i] - T.X[i - 1], 1e-3f));
			return T.Y[T.Num - 1];
		}
		const FTab IdleV = {6, {12.f, 18.9f, 41.7f, 55.f, 57.2f, 70.f}, {0.f, 0.9f, 0.673f, 0.297f, 0.042f, 0.f}};
		const FTab IdleP = {2, {18.9f, 42.5f}, {1.1f, 1.65f}};
		const FTab SpoolV = {5, {31.f, 50.f, 69.f, 76.4f, 88.f}, {0.f, 0.07f, 0.98f, 0.886f, 0.172f}};
		const FTab LowSpoolV = {5, {31.f, 50.f, 69.f, 76.4f, 88.f}, {0.f, 0.464f, 0.24f, 0.205f, 0.043f}};
		const FTab SpoolP = {2, {31.f, 95.f}, {0.63f, 2.f}};
		const FTab BuzzV = {5, {75.f, 80.f, 87.f, 89.f, 95.f}, {0.f, 0.894f, 0.995f, 1.f, 0.936f}};
		const FTab BuzzP = {2, {31.f, 95.f}, {0.315f, 1.f}};
		const FTab RumbleV = {3, {60.f, 90.f, 100.f}, {0.f, 0.25f, 0.83f}};            // (by N2)
		const FTab RumbleP = {2, {60.f, 100.f}, {0.5f, 1.f}};
		const FTab RumbleDV = {6, {62.f, 74.f, 88.f, 90.f, 94.f, 99.f}, {0.f, 0.05f, 0.55f, 0.8f, 0.83f, 0.85f}};
		const FTab RevV = {3, {30.f, 70.f, 100.f}, {0.f, 0.8f, 0.995f}};               // (by the reverser x N2)
		const FTab RevP = {2, {60.f, 90.f}, {0.3f, 1.f}};
		// (the wing-aft view: heard in the cabin)
		const FTab CabIdleV = {6, {12.f, 18.9f, 41.7f, 53.f, 57.2f, 70.f}, {0.f, 0.9f, 0.673f, 0.357f, 0.042f, 0.f}};
		const FTab CabSpoolV = {5, {31.f, 50.f, 69.f, 76.4f, 88.f}, {0.f, 0.07f, 0.765f, 0.72f, 0.125f}};
		// the CFM56-5B: idle N1 19.5 % / N2 58 %, take-off about 95 / 100
		constexpr float IdleN1 = 19.5f, ToN1 = 95.f;
		float N2Of(float N1) { return N1 <= IdleN1 ? 58.f * N1 / IdleN1 : 58.f + (N1 - IdleN1) * 42.f / (ToN1 - IdleN1); }

		USoundBase* SndA320(const TCHAR* Name)
		{
			static TMap<FString, USoundBase*> Cache;
			if (USoundBase** F = Cache.Find(Name)) return *F;
			USoundBase* Got = ADPKeep(LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/A320/Sounds/%s.%s"), Name, Name)));
			if (!Got) ADP_WARN(TEXT("A320 sound: %s missing (/Game/A320/Sounds)"), Name);
			Cache.Add(Name, Got); return Got;
		}
		// one attenuation per reach: Inner cm at full, then the fall-off over FalloffCm (natural for the far-carrying engine
		// layers, linear for the close-only ones, so they are gone by their reach); occlusion (walls, the terminal) muffles
		USoundAttenuation* AttA320(float InnerCm, float FalloffCm, bool bLinear, bool bOcclude)
		{
			static TMap<FString, USoundAttenuation*> Cache;
			const FString Key = FString::Printf(TEXT("%.0f/%.0f/%d%d"), InnerCm, FalloffCm, bLinear, bOcclude);
			if (USoundAttenuation** F = Cache.Find(Key)) return *F;
			USoundAttenuation* At = ADPKeep(NewObject<USoundAttenuation>(GetTransientPackage()));
			FSoundAttenuationSettings& St = At->Attenuation;
			St.bAttenuate = true; St.bSpatialize = true; St.AttenuationShapeExtents = FVector(InnerCm); St.FalloffDistance = FalloffCm;
			St.DistanceAlgorithm = bLinear ? EAttenuationDistanceModel::Linear : EAttenuationDistanceModel::NaturalSound; St.dBAttenuationAtMax = -60.f;
			St.bEnableOcclusion = bOcclude; St.OcclusionTraceChannel = ECC_Visibility; St.OcclusionLowPassFilterFrequency = 900.f; St.OcclusionVolumeAttenuation = 0.35f; St.OcclusionInterpolationTime = 0.15f;
			Cache.Add(Key, At); return At;
		}

		// the loops each aircraft may have playing
		enum ELoop : int32 { LIdle, LIdleDist, LSpool, LLowSpool, LBuzz, LRumble, LRumbleDist, LReverse, LCabIdle, LCabSpool, LCabRev, LCabHum, EngLoops,
			LApu = EngLoops * 2, LPacks, LCabin, LRoll, LPush, LFlap, LoopNum };
		// the announcements (Sounds/Announce): the departure's first, then the arrival's
		enum EAnn : int32 { AnBoarding, AnWelcome, AnCptWelcome, AnCptDoors, AnSafety, AnCptTakeoff, AnSeatbelt, AnDescent, AnCptLanding, AnLanded, AnCabCall, AnNum };
		const TCHAR* AnnFile[AnNum] = {TEXT("Ann_fa_boarding"), TEXT("Ann_fa_welcome"), TEXT("Ann_cpt_welcome"), TEXT("Ann_cpt_doors"), TEXT("Ann_fa_safety"), TEXT("Ann_cpt_takeoff"),
			TEXT("Ann_fa_seatbelt10k"), TEXT("Ann_fa_descent"), TEXT("Ann_cpt_landing"), TEXT("Ann_fa_landed"), TEXT("Ann_cab_call")};
		constexpr uint32 DepBits = (1u << AnDescent) - 1u, ArrBits = ((1u << AnNum) - 1u) & ~DepBits;

		struct FEng { uint8 Run = 0; float T = 0.f, N1 = 0.f; };   // Run 0 off, 1 starting, 2 running
		struct FState
		{
			bool bInit = false;
			FEng E[2];
			TWeakObjectPtr<UAudioComponent> L[LoopNum];
			uint8 GearSt = 255; float FlapWas = -1.f, FlapRun = 0.f, HeightWas = -1.f, ScreechT = -1.f;
			TWeakObjectPtr<UAudioComponent> Screech;
			bool bPacks = false;
		};
		TMap<TWeakObjectPtr<ABabylonModel>, FState> States;
		// the announcements go with the registration (the parked model hands over to the moving one at the push)
		struct FAnnState { uint32 Done = 0; bool bSeen = false; TArray<int32> Queue; TWeakObjectPtr<UAudioComponent> C; TWeakObjectPtr<ABabylonModel> On; float Gap = 0.f; int32 Pending = -1; uint64 Frame = 0; };
		TMap<FString, FAnnState> Anns;

		struct FHere { FVector World = FVector::ZeroVector, Local = FVector::ZeroVector; bool bIn = false; bool bAtDoor = false; };
		bool ListenerAt(UWorld* W, FVector& Out)
		{
			APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr; if (!PC) return false;
			FVector Fr, Rt; PC->GetAudioListenerPosition(Out, Fr, Rt); return true;
		}
		FHere Where(ABabylonModel* B, const FVector& Lis)
		{
			FHere H; H.World = Lis; H.Local = B->GetActorTransform().InverseTransformPosition(Lis);
			H.bIn = CabinBox.IsInside(H.Local);
			for (int32 d = 0; d < 4 && !H.bIn; ++d)
				H.bAtDoor |= ADPA320::Now(B, ADPA320::EPart(ADPA320::PL1 + d)) > 0.3f && FVector::Dist(H.Local, PaxDoorAt[d]) < 800.f;
			return H;
		}
		// a loop at its part: playing while it is loud enough and you are within its reach, else stopped
		void Loop(FState& S, ABabylonModel* B, const FHere& H, int32 Key, const TCHAR* Name, const FVector& Local, float Vol, float Pitch, float ReachM, USoundAttenuation* At)
		{
			UAudioComponent* C = S.L[Key].Get();
			const bool bWant = Vol > 0.004f && FVector::Dist(H.Local, Local) < ReachM * 100.f;
			if (!bWant) { if (C) C->FadeOut(0.4f, 0.f); S.L[Key] = nullptr; return; }
			if (!C)
			{
				USoundBase* Sb = SndA320(Name); if (!Sb) return;
				C = UGameplayStatics::SpawnSoundAttached(Sb, B->GetRootComponent(), NAME_None, Local, EAttachLocation::KeepRelativeOffset, false, Vol, Pitch, FMath::FRand() * 2.f, At, nullptr, true);
				S.L[Key] = C; if (!C) return;
			}
			C->SetVolumeMultiplier(Vol); C->SetPitchMultiplier(FMath::Clamp(Pitch, 0.2f, 2.f));
		}
		UAudioComponent* Shot(ABabylonModel* B, const FHere& H, const TCHAR* Name, const FVector& Local, float Vol, float ReachM, USoundAttenuation* At, bool bStays = false)
		{
			if (Vol <= 0.004f || FVector::Dist(H.Local, Local) > ReachM * 100.f) return nullptr;
			USoundBase* Sb = SndA320(Name); if (!Sb) return nullptr;
			// (bStays: at its place in the world, so it plays on when the model is taken away - the shut-down at the stand)
			if (bStays) return UGameplayStatics::SpawnSoundAtLocation(B, Sb, B->GetActorTransform().TransformPosition(Local), FRotator::ZeroRotator, Vol, 1.f, 0.f, At);
			return UGameplayStatics::SpawnSoundAttached(Sb, B->GetRootComponent(), NAME_None, Local, EAttachLocation::KeepRelativeOffset, false, Vol, 1.f, 0.f, At, nullptr, true);
		}
		void Ask(FAnnState& A, int32 Which)
		{
			const uint32 Bit = 1u << Which; if (A.Done & Bit) return;
			A.Done |= Bit; A.Done &= Which < AnDescent ? ~ArrBits : ~DepBits;   // (a new flight: the other half's are said again next time)
			if (A.bSeen) A.Queue.Add(Which);
		}
		// the cabin's speakers (in the ceiling between L1 and L4): the cabin chime, then the words; heard in the cabin, quietly
		// just outside an open door, nowhere else
		void Speak(const FString& Reg, ABabylonModel* B, const FHere& H, float Dt)
		{
			FAnnState& A = Anns.FindOrAdd(Reg);
			if (A.Frame == GFrameCounter && A.On.Get() != B) return;   // (two models of one registration for a moment: the one speaking)
			A.Frame = GFrameCounter;
			const float Vol = H.bIn ? 0.6f : H.bAtDoor ? 0.12f : 0.f;   // (FlightGear's 0.6 inside)
			UAudioComponent* C = A.C.Get();
			if (C && A.On.Get() == B) { C->SetVolumeMultiplier(FMath::Max(Vol, 0.001f)); if (C->IsPlaying()) return; }
			if (C && A.On.Get() != B) { C->Stop(); }
			A.C = nullptr;
			if ((A.Gap -= Dt) > 0.f) return;
			USoundAttenuation* At = AttA320(1400.f, 1200.f, true, false);
			if (A.Pending >= 0)
			{
				const int32 Which = A.Pending; A.Pending = -1; A.Gap = 1.5f;
				if (USoundBase* Sb = SndA320(AnnFile[Which]))
					A.C = UGameplayStatics::SpawnSoundAttached(Sb, B->GetRootComponent(), NAME_None, CabinAt, EAttachLocation::KeepRelativeOffset, false, FMath::Max(Vol, 0.001f), 1.f, 0.f, At, nullptr, true);
				A.On = B;
				ADP_LOG(TEXT("A320 sound: %s cabin announcement %s"), *Reg, AnnFile[Which]);
				return;
			}
			if (!A.Queue.Num()) return;
			A.Pending = A.Queue[0]; A.Queue.RemoveAt(0);
			if (A.Pending == AnCabCall) return;   // (the call from the cockpit is its own chime)
			if (USoundBase* Sb = SndA320(TEXT("CabinChime")))
				A.C = UGameplayStatics::SpawnSoundAttached(Sb, B->GetRootComponent(), NAME_None, CabinAt, EAttachLocation::KeepRelativeOffset, false, FMath::Max(Vol, 0.001f), 1.f, 0.f, At, nullptr, true);
			A.On = B; A.Gap = 0.4f;
		}
		void Tidy()
		{
			static double Last = 0.0; const double T = FPlatformTime::Seconds();
			if (T - Last < 10.0) return; Last = T;
			for (auto It = States.CreateIterator(); It; ++It) if (!It->Key.IsValid()) It.RemoveCurrent();
		}

		// the APU, the packs and the cabin's hum: at the gate and on the push (the APU's inlet flap open: A320Parts)
		void Ground(FState& S, ABabylonModel* B, const FHere& H, bool bOnGround, bool bEngRun)
		{
			const float Apu = ADPA320::Now(B, ADPA320::Apu);
			// (FlightGear: x320_apu outside, its volume with the APU's speed - here the flap's travel as it spools up)
			Loop(S, B, H, LApu, TEXT("ApuLoop"), ApuOutAt, 0.85f * FMath::Clamp(Apu, 0.f, 1.f) * (H.bIn ? 0.25f : 1.f), 0.8f + 0.2f * Apu, 300.f, AttA320(500.f, 25000.f, false, true));
			const bool bPacks = bOnGround && Apu >= 0.99f;
			if (bPacks != S.bPacks)
			{
				Shot(B, H, bPacks ? TEXT("PackIn") : TEXT("PackOut"), PackBayAt, 0.7f, 30.f, AttA320(200.f, 2800.f, true, true));
				S.bPacks = bPacks;
			}
			Loop(S, B, H, LPacks, TEXT("PacksLoop"), PackBayAt, bPacks ? (H.bIn ? 0.35f : 0.5f) : 0.f, 1.f, 30.f, AttA320(200.f, 2800.f, true, true));
			Loop(S, B, H, LCabin, TEXT("CabinLoop"), CabinAt, H.bIn && (bPacks || bEngRun) ? 0.45f : 0.f, 1.f, 25.f, AttA320(1400.f, 1000.f, true, false));
		}
	}
	using namespace SndData;

	void Moving(ABabylonModel* B, const FString& Reg, const FIn& In, float Dt)
	{
		ADP_PERF_SCOPE("A320 sound", 0.3f);
		if (!B || Dt <= 0.f) return;
		FVector Lis; if (!ListenerAt(B->GetWorld(), Lis)) return;
		Tidy();
		FState& S = States.FindOrAdd(B);
		const FHere H = Where(B, Lis);
		const bool bWant = In.bEngines && In.Stage != 7;   // (on the stand: shut down once the aircraft stops)
		if (!S.bInit)
		{
			// met already running (an arrival, a test aircraft in the air) or cold on the push
			S.bInit = true;
			for (FEng& E : S.E) { E.Run = bWant ? 2 : 0; E.N1 = bWant ? IdleN1 + FMath::Max(0.f, In.Power - 0.15f) / 0.85f * (ToN1 - IdleN1) : 0.f; }
			S.GearSt = In.GearSt; S.FlapWas = In.Flap; S.HeightWas = In.Height;
		}
		const float Ext = H.bIn ? 0.3f : 1.f, Cab = H.bIn ? 1.f : 0.f;
		const float KtNow = In.Speed / 0.5144f;
		bool bAnyRun = false;
		for (int32 e = 0; e < 2; ++e)
		{
			FEng& E = S.E[e];
			const FVector Fan = FanAt + FVector(EngX[e], 0.f, 0.f), Core = CoreAt + FVector(EngX[e], 0.f, 0.f), RevP0 = RevAt + FVector(EngX[e], 0.f, 0.f);
			// the start: engine 2 first, engine 1 25 s later (the push), each: starter, light-off and up to idle in about 45 s
			if (bWant && E.Run == 0) { E.Run = 1; E.T = e == 1 ? 0.f : -25.f; }
			if (!bWant && E.Run != 0)
			{
				// shut down: the run-down once, at its place (it plays on if the model is taken away)
				E.Run = 0;
				Shot(B, H, TEXT("EngShutdown"), Fan, Ext * 0.9f, 800.f, AttA320(1500.f, 60000.f, false, true), true);
				if (H.bIn) Shot(B, H, TEXT("CabShutdown"), Fan, 0.8f, 60.f, AttA320(1400.f, 1000.f, true, false));
				ADP_LOG(TEXT("A320 sound: %s engine %d shut down"), *Reg, e + 1);
			}
			if (E.Run == 1)
			{
				const float Was = E.T; E.T += Dt;
				if (Was < 0.f && E.T >= 0.f) { Shot(B, H, TEXT("EngStarter"), Fan, Ext * 0.8f, 600.f, AttA320(1500.f, 50000.f, false, true)); if (H.bIn) Shot(B, H, TEXT("CabStarter"), Fan, 0.8f, 60.f, AttA320(1400.f, 1000.f, true, false)); ADP_LOG(TEXT("A320 sound: %s engine %d starting"), *Reg, e + 1); }
				if (Was < 18.f && E.T >= 18.f) { Shot(B, H, TEXT("EngStartup"), Fan, Ext * 0.9f, 800.f, AttA320(1500.f, 60000.f, false, true)); if (H.bIn) Shot(B, H, TEXT("CabStartup"), Fan, 0.8f, 60.f, AttA320(1400.f, 1000.f, true, false)); }
				E.N1 = IdleN1 * FMath::Clamp((E.T - 25.f) / 20.f, 0.f, 1.f);
				if (E.T >= 45.f) E.Run = 2;
			}
			else if (E.Run == 2)
			{
				const float Target = IdleN1 + FMath::Max(0.f, In.Power - 0.15f) / 0.85f * (ToN1 - IdleN1);
				E.N1 = FMath::FInterpConstantTo(E.N1, Target, Dt, Target > E.N1 ? 14.f : 18.f);   // (about 5 s idle to take-off, as the haze)
			}
			else E.N1 = FMath::FInterpConstantTo(E.N1, 0.f, Dt, 4.f);
			bAnyRun |= E.Run == 2;
			// the layers by FlightGear's tables (silent while the engine is off: its run-down is the shut-down's own)
			const bool bOn = E.Run != 0;
			const float n1 = bOn ? E.N1 : 0.f, n2 = N2Of(n1), rev = In.Rev * n2;
			Loop(S, B, H, e * EngLoops + LIdle, TEXT("EngIdleLoop"), Fan, Ext * Look(IdleV, n1), Look(IdleP, n1), 900.f, AttA320(1500.f, 80000.f, false, true));
			Loop(S, B, H, e * EngLoops + LIdleDist, TEXT("EngIdleDistLoop"), Fan, Ext * 0.8f * Look(IdleV, n1), Look(IdleP, n1) * 0.9f, 3000.f, AttA320(8000.f, 300000.f, false, true));
			Loop(S, B, H, e * EngLoops + LSpool, TEXT("EngSpoolLoop"), Fan, Ext * Look(SpoolV, n1), Look(SpoolP, n1), 1500.f, AttA320(2000.f, 140000.f, false, true));
			Loop(S, B, H, e * EngLoops + LLowSpool, TEXT("EngLowSpoolLoop"), Fan, Ext * Look(LowSpoolV, n1), Look(SpoolP, n1), 1500.f, AttA320(2000.f, 140000.f, false, true));
			Loop(S, B, H, e * EngLoops + LBuzz, TEXT("EngBuzzLoop"), Fan, Ext * Look(BuzzV, n1), Look(BuzzP, n1), 2000.f, AttA320(2000.f, 190000.f, false, true));
			Loop(S, B, H, e * EngLoops + LRumble, TEXT("EngRumbleLoop"), Core, Ext * 1.2f * Look(RumbleV, n2), Look(RumbleP, n2), 2500.f, AttA320(3000.f, 240000.f, false, true));
			Loop(S, B, H, e * EngLoops + LRumbleDist, TEXT("EngRumbleDistLoop"), Core, Ext * 1.2f * Look(RumbleDV, n1), Look(RumbleP, n2), 6000.f, AttA320(20000.f, 580000.f, false, true));
			Loop(S, B, H, e * EngLoops + LReverse, TEXT("EngReverseLoop"), RevP0, Ext * 1.1f * Look(RevV, rev), Look(RevP, rev), 3000.f, AttA320(8000.f, 290000.f, false, true));
			// in the cabin: the wing-aft recordings
			Loop(S, B, H, e * EngLoops + LCabIdle, TEXT("CabEngIdleLoop"), Fan, Cab * 0.5f * Look(CabIdleV, n1), Look(IdleP, n1), 60.f, AttA320(1400.f, 1000.f, true, false));
			Loop(S, B, H, e * EngLoops + LCabSpool, TEXT("CabEngSpoolLoop"), Fan, Cab * 0.5f * Look(CabSpoolV, n1), Look(SpoolP, n1), 60.f, AttA320(1400.f, 1000.f, true, false));
			Loop(S, B, H, e * EngLoops + LCabRev, TEXT("CabEngReverseLoop"), RevP0, Cab * 0.5f * Look(RevV, rev), Look(RevP, rev), 60.f, AttA320(1400.f, 1000.f, true, false));
			Loop(S, B, H, e * EngLoops + LCabHum, TEXT("CabEngHumLoop"), Fan, Cab * 0.25f * Look(RumbleDV, n1), Look(RumbleP, n2), 60.f, AttA320(1400.f, 1000.f, true, false));
		}
		// the gear: motor and clunks from the bays as it starts to move (heard close only)
		if (In.GearSt != S.GearSt)
		{
			if (In.GearSt == 1 || In.GearSt == 3) Shot(B, H, TEXT("GearMotor"), GearBayAt, 0.9f, 60.f, AttA320(500.f, 5500.f, true, true));
			S.GearSt = In.GearSt;
		}
		// the flap motor while the flaps move (close outside, and in the cabin)
		const bool bFlapMoving = S.FlapWas >= 0.f && FMath::Abs(In.Flap - S.FlapWas) > 1e-3f;
		S.FlapRun = bFlapMoving ? 0.3f : S.FlapRun - Dt; S.FlapWas = In.Flap;
		Loop(S, B, H, LFlap, TEXT("FlapMotorLoop"), FlapAt, S.FlapRun > 0.f ? (H.bIn ? 0.5f : 0.8f) : 0.f, 1.f, 40.f, AttA320(400.f, 3600.f, true, false));
		// the touchdown: the mains' thump and the tyres' screech (a second and a half of it)
		const bool bGround = In.Height < 0.05f;
		if (S.HeightWas > 0.05f && bGround && In.Stage == 5)
		{
			Shot(B, H, TEXT("TouchMain"), MainWheelAt, Ext * 1.f, 400.f, AttA320(2000.f, 38000.f, false, true));
			S.Screech = Shot(B, H, TEXT("TyreScreech"), MainWheelAt, Ext * 0.8f, 400.f, AttA320(1000.f, 39000.f, false, true));
			S.ScreechT = 1.5f;
			ADP_LOG(TEXT("A320 sound: %s touchdown at %.0f kt"), *Reg, KtNow);
		}
		if (S.ScreechT > 0.f && (S.ScreechT -= Dt) <= 0.f) if (UAudioComponent* Sc = S.Screech.Get()) Sc->FadeOut(0.6f, 0.f);
		S.HeightWas = In.Height;
		// the wheels rolling (by the speed), the tug's push
		Loop(S, B, H, LRoll, TEXT("RollLoop"), MainWheelAt, bGround && KtNow > 2.f ? Ext * FMath::Clamp(KtNow / 80.f, 0.1f, 1.f) : 0.f, FMath::Clamp(0.6f + KtNow / 120.f, 0.6f, 1.5f), 150.f, AttA320(800.f, 14000.f, false, true));
		Loop(S, B, H, LPush, TEXT("PushbackLoop"), NoseWheelAt, In.Stage == 0 && In.Speed > 0.05f ? Ext * 0.8f : 0.f, 1.f, 50.f, AttA320(500.f, 4500.f, true, true));
		Ground(S, B, H, bGround, bAnyRun);
		// the cabin's announcements by the flight's moments
		FAnnState& A = Anns.FindOrAdd(Reg);
		if (In.Stage == 0 && In.Speed > 0.05f) Ask(A, AnSafety);
		if (In.Stage == 2) Ask(A, AnCptTakeoff);
		if (In.Stage == 3 && In.Height > 3048.f) Ask(A, AnSeatbelt);
		if (In.Stage == 4 && In.Height > 400.f) Ask(A, AnDescent);
		if (In.Stage == 4 && In.Height < 300.f) { Ask(A, AnDescent); Ask(A, AnCptLanding); }
		if (In.Stage == 6) Ask(A, AnLanded);
		if (In.Stage == 7) Ask(A, AnCabCall);
		A.bSeen = true;   // (met in the middle of a flight: what is already past is not said)
		Speak(Reg, B, H, Dt);
	}

	void Parked(ABabylonModel* B, const FString& Reg, float Dt)
	{
		ADP_PERF_SCOPE("A320 sound parked", 0.2f);
		if (!B || Dt <= 0.f) return;
		FVector Lis; if (!ListenerAt(B->GetWorld(), Lis)) return;
		Tidy();
		FState& S = States.FindOrAdd(B);
		const FHere H = Where(B, Lis);
		Ground(S, B, H, true, false);
		// the boarding's announcements from its turnaround (looked at once a second): boarding while they board, the welcome
		// once all are aboard, the captain's, then "doors" as they shut for the push
		FAnnState& A = Anns.FindOrAdd(Reg);
		static TMap<FString, float> Polls; float& P = Polls.FindOrAdd(Reg);
		if ((P -= Dt) <= 0.f)
		{
			P = 1.f;
			if (const ADPTurnaround::FTurn* Tr = ADPTurnaround::All().FindByPredicate([&Reg](const ADPTurnaround::FTurn& X) { return X.Reg == Reg; }))
			{
				auto Task = [Tr](const TCHAR* Id) { return Tr->Tasks.FindByPredicate([Id](const ADPTurnaround::FTask& X) { return X.Id == Id; }); };
				const ADPTurnaround::FTask* Bd = Task(TEXT("board")); const ADPTurnaround::FTask* Dc = Task(TEXT("doors"));
				if (Bd && Bd->bStarted) Ask(A, AnBoarding);
				if ((Bd && Bd->bDone) || (Dc && Dc->bStarted)) { Ask(A, AnBoarding); Ask(A, AnWelcome); Ask(A, AnCptWelcome); }
				if (Dc && Dc->bStarted) Ask(A, AnCptDoors);
			}
			A.bSeen = true;   // (what had happened before the game was loaded is not said again)
		}
		Speak(Reg, B, H, Dt);
	}
}
