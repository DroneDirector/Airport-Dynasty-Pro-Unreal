// The fixed-views performance run (to-do 269): a text file, one step a line, so a new run needs no compile:
//   view <x> <height> <z> <yaw> <pitch>   the camera there, held (plan metres and degrees as in ADP-Perf.log)
//   fly <x> <height> <z> <yaw> <pitch> <seconds>   the camera moved there in that time, measured while it moves
//   cmd <console command>
//   wait <seconds>
//   day <of the year, 1-365; the summer day if not given>, hour <0-24>, weather <0 clear .. 2 broken, 3 overcast .. 7 snow>
//   measure <seconds> <name>              frame, game, render and graphics-card times into ADP.log ("Views: ...")
//   gpu                                   every pass of one frame into the log (ProfileGPU; not in the packaged game)
//   census                                instances in the distance field scene, by mesh
//   shot <name>
//   hmi <jetway> [tab]  /  hmi close      the jetway's screen used (its camera page: hmi 0 cam; g4: gate 4 jetway)
//   eat <MB>                              so much graphics memory taken and used every frame, as another program would
//   speed <career> <world>                the career's clock and the world's (1 1: as played)
//   follow <seconds> <name>               beside a walking passenger with most people around, clear of walls and people, measured
//   people <n> <seconds>                  waits until n people are about; if they don't come, the run ends there, as failed
//   look <metres, 0: the whole of it> <side, degrees from its front> <camera height over its foot> <name>   the nearest shown one of that
//                                         name, with a free line of sight, held; if there is none the next pictures are left out
//      names: a vehicle's ("Apron bus 1", spare, charging, broken, dirty, washed), its model's (PushbackTug), seated, person, person:apron (landside, hall,
//      upstairs), person@<name> (beside it), aboard@<name> (on or in it), mast, jetway, tower, an actor's, or a mesh's (its nearest instance)
//   rel <forward> <left> <up> <yaw, + left> <pitch> <name>   the camera in its frame (inside the bus), held
//   until <seconds> <name>                waits until it is there
//   shot4k <name>                         3840 x 2160 into Saved/Screenshots/run/<name>.png, left out if the subject was missing
//   talk / talkend                        a talk with the nearest person, by E as the player's
//   lives                                 what is true now, into the log
//   weather live                          the real weather again (hour live: the real time)
// The camera never stands in or goes through anything: a view in something solid is left out, a flight ends before it.
//   park / parkoff                        an A320 on every free narrow-body stand at the terminal, for a picture; away again
// (2026-11-28, the owner's whole visual test in one start, Tools/views/full-test-1128.txt:)
//   framegen on|off                       our own frame generation switched live (ADPFrameGen::SetActive); its state into the log
//   shots <file> / shotcam <name>         named camera shots (Tools/views/camera-shots-1128.txt: "<name> <any step line>"),
//                                         the named one run as the next step
//   jetway <gate> park|dock               the gate's jetway driven home quickly / to its aircraft at the real speed (JetwayRigs)
//   jetway <gate> cam wide|side|cab|cabin|gate [seconds]   a camera by the jetway as it stands now (seconds: flown there)
//   jetway <gate> state / jetway beacons auto|on|flash     its state and beacons into the log / the beacons as usual,
//                                         held lit, or all flashing (JetwayRigs::BeaconForce, bBeaconTest)
//   board <gate> <n> [gap]                n passengers on free floor before the gate's jetway door, boarding at once (PaxFlow)
//   fire doors open|shut / fire lights on|off   the bay doors (their own motors) / every truck's blue lights and beams
//   fire drive / fire park                the first truck taken from its door, out along the UIDrive route at 30 km/h in its
//                                         chase view; stopped and left again (camera steps are left out while in it)
//   hangar <plan x> open|shut [speed]     the hangar nearest that x, its doors sent; speed: how many times the real 10 min
//   coach town|terminal                   every coach sent to the town hub / back to its own stand (Coaches::SendOnRoute)
//   coachwait <seconds> hub|home|near <m> waits until no coach is on its way / one is that near its stand
//   quit
#include "PerfViews.h"
#include "PeopleLayers.h"
#include "RoadNetwork.h"
#include "MetaRoadActor.h"
#include "People.h"
#include "PaxAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "ADPLog.h"
#include "Containers/Ticker.h"
#include "Misc/FileHelper.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "RHI.h"
#include "RenderCore.h"
#include "UnrealClient.h"
#include "ADPFrameGen.h"
#include "SunClock.h"
#include "PaxFlow.h"
#include "FireTruck.h"
#include "Hangar.h"
#include "Coaches.h"
#include "GraphicsMemory.h"
#include "Sim/ADPSim.h"
#include "Kismet/GameplayStatics.h"
#include "JetwayHmi.h"
#include "UI/ADPHUD.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectIterator.h"
#include "UObject/StrongObjectPtr.h"
#include "BabylonModel.h"
#include "Airfield.h"
#include "ParkedAircraft.h"
#include "GSE.h"
#include "BusCharge.h"
#include "GlassRain.h"
#include "JetwayRigs.h"
#include "LightMasts.h"
#include "StaffPeople.h"
#include "TerminalPlan.h"
#include "TowerADC.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPSimData.h"

namespace ADPPerfViewsInternal
{
	TArray<FString> Steps; int32 At = 0;
	bool bHeld = false; FVector Want = FVector::ZeroVector, From = FVector::ZeroVector; FRotator Rot = FRotator::ZeroRotator, RotFrom = FRotator::ZeroRotator;
	TMap<FString, FString> Shots; TWeakObjectPtr<AFireTruck> FireT; double FireEnterAt = 0.0, FireRouteAt = 0.0, FireExitAt = 0.0;
	TArray<FVector> CoachHome; int32 CoachMode = 0; float CoachNear = 0.f; double CoachEnd = 0.0;
	double Until = 0.0, FlyStart = 0.0, FlyLen = 0.0; FString Name; TArray<float> Fr, Gp, Rn, Gm; uint64 Shown0 = 0, Made0 = 0; double MStart = 0.0;

	TArray<TStrongObjectPtr<UTextureRenderTarget2D>> Eaten;
	TArray<TWeakObjectPtr<ABabylonModel>> ForPicture;   // (park)

	UWorld* ViewsWorld()
	{
		if (GEngine) for (const FWorldContext& C : GEngine->GetWorldContexts()) if ((C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE) && C.World()) return C.World();
		return nullptr;
	}
	// the people about and how fast each goes (looked at twice a second)
	TMap<TWeakObjectPtr<AActor>, FVector> Was; TMap<TWeakObjectPtr<AActor>, FVector> Going; double LookedAt = 0.0;
	TWeakObjectPtr<AActor> Subject; bool bFollow = false, bCut = false; float Stood = 0.f; int32 Need = 0; float Side = 35.f;
	/** The part of the airport the feet are looked at in (plan metres): landside, hall, apron, upstairs ("" anywhere). */
	FString Where;
	bool Here(const FVector& A)
	{
		const double Z = -A.Y / 100.0, H = A.Z / 100.0;
		return Where.IsEmpty() || (Where == TEXT("landside") ? Z < -5.0 : Where == TEXT("hall") ? Z > -3.0 && Z < 45.0 && H < 2.0 : Where == TEXT("apron") ? Z > 50.0 && H < 2.5 : H > 2.5 && Z > -3.0);
	}
	void LookAtPeople(UWorld* W)
	{
		const double Now = FPlatformTime::Seconds(), D = Now - LookedAt;
		if (D < 0.5) return;
		LookedAt = Now; Going.Reset(); TMap<TWeakObjectPtr<AActor>, FVector> Is;
		if (AADPPeople* P = AADPPeople::Get(W))
			for (const AADPPeople::FWho& Q : P->Everyone())
				if (AActor* A = Q.A.Get(); A && !A->IsHidden())
				{
					Is.Add(A, A->GetActorLocation());
					if (const FVector* L = Was.Find(A)) Going.Add(A, (A->GetActorLocation() - *L) / D); else Going.Add(A, FVector::ZeroVector);
				}
		if (!Where.IsEmpty())   // (the feet's views: the crew and whoever else walks, too)
			for (TObjectIterator<UPaxAnimInstance> It; It; ++It)
				if (const USkeletalMeshComponent* C = It->GetSkelMeshComponent(); C && C->GetWorld() == W)
					if (AActor* A = C->GetOwner(); A && !A->IsHidden() && !A->IsA<APawn>() && !(A->GetAttachParentActor() && A->GetAttachParentActor()->IsA<APawn>()) && !A->GetName().Contains(TEXT("_player_")) && !Is.Contains(A))
					{
						Is.Add(A, A->GetActorLocation());
						if (const FVector* L = Was.Find(A)) Going.Add(A, (A->GetActorLocation() - *L) / D); else Going.Add(A, FVector::ZeroVector);
					}
		Was = MoveTemp(Is);
		// (anybody outside with their feet under a road, a pavement or a car park: to-do 287)
		static double SunkAt = 0.0; if (Now - SunkAt < 2.0) return;
		SunkAt = Now;
		int32 Out = 0, Sunk = 0; FString Who;
		for (const auto& E : Was)
		{
			const AActor* A = E.Key.Get(); if (!A) continue;
			FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(ViewsSunk), true, A);
			for (int32 k = 0; k < 4; ++k)
			{
				if (!W->LineTraceSingleByChannel(H, E.Value + FVector(0, 0, 170.f), E.Value - FVector(0, 0, 20.f), ECC_Visibility, Q) || !H.GetActor()) break;
				if (!H.GetActor()->IsA<AMetaRoad>() && !H.GetActor()->IsA<ARoadNetwork>()) { Q.AddIgnoredActor(H.GetActor()); continue; }
				++Out; if (H.ImpactPoint.Z > E.Value.Z + 10.f) { ++Sunk; Who += FString::Printf(TEXT(" %s %.0f cm at %.1f %.1f"), *A->GetName(), H.ImpactPoint.Z - E.Value.Z, E.Value.X / 100.f, -E.Value.Y / 100.f); }
				break;
			}
		}
		if (Sunk) ADP_ERROR(TEXT("Views: %d of %d people on the roads are under them:%s"), Sunk, Out, *Who.Left(300));
		else if (Out) { static int32 Said = 0; if (Said++ % 15 == 0) ADP_LOG(TEXT("Views: %d people on the roads, pavements and crossings, all on top of them"), Out); }
	}
	int32 Around(const FVector& Spot, double R) { int32 N = 0; for (const auto& E : Was) if (FVector::Dist(E.Value, Spot) < R) ++N; return N; }
	bool Solid(UWorld* W, const FVector& Spot, const AActor* Me)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ViewsCam), false, Me);
		return W->OverlapBlockingTestByChannel(Spot, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(22.f), Q) || Around(Spot - FVector(0, 0, 90.f), 70.0) > 0;
	}
	bool NextSubject()
	{
		AActor* Best = nullptr; int32 Most = -1;
		for (const auto& E : Going) if (AActor* A = E.Key.Get()) if (A != Subject.Get() && E.Value.Size2D() > 30.0 && Here(A->GetActorLocation())) { const int32 N = Around(A->GetActorLocation(), 2500.0); if (N > Most) { Most = N; Best = A; } }
		if (!Best) return false;
		Subject = Best; bCut = true;
		ADP_LOG(TEXT("Views: following %s at %.1f %.1f %.1f, %d people within 25 m"), *Best->GetName(), Best->GetActorLocation().X / 100.f, Best->GetActorLocation().Z / 100.f, -Best->GetActorLocation().Y / 100.f, Most);
		return true;
	}
	/** Beside and behind the one followed, the whole person in view: the first of a ring of places with a free line of sight. */
	bool FollowAt(UWorld* W, const AActor* Me, float Dt)
	{
		AActor* A = Subject.Get();
		const FVector* G = A ? Going.Find(A) : nullptr;
		if (!A || !G) { if (!NextSubject()) return false; A = Subject.Get(); G = Going.Find(A); if (!G) return false; }
		const bool bLow = !Where.IsEmpty();   // (the feet's views: close and low)
		if (bLow && (!Here(A->GetActorLocation()) || (G->Size2D() < 10.0 && (Stood += Dt) > 4.f))) { Stood = 0.f; if (NextSubject()) { A = Subject.Get(); G = Going.Find(A); if (!G) return true; } }
		else if (G->Size2D() >= 10.0) Stood = 0.f;
		const FVector Mid = A->GetActorLocation() + FVector(0, 0, bLow ? 38.f : 110.f), Fw = G->Size2D() > 30.0 ? G->GetSafeNormal2D() : A->GetActorRightVector().GetSafeNormal2D();
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ViewsCam), false, Me); Q.AddIgnoredActor(A);
		static const float Turn[] = { 0.f, -70.f, 40.f, -110.f, 100.f, -30.f, 150.f };
		const float Outs[2][3] = { { 480.f, 330.f, 230.f }, { 230.f, 180.f, 300.f } };
		for (float Out : Outs[bLow ? 1 : 0])
			for (float T : Turn)
			{
				const FVector C = Mid - Fw.RotateAngleAxis(Side + T, FVector::UpVector) * Out + FVector(0, 0, bLow ? 12.f : 70.f);
				FHitResult H;
				if (W->SweepSingleByChannel(H, Mid, C, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(24.f), Q) || Solid(W, C, Me)) continue;
				// (gliding there; if something stands between, it cuts over instead of going through it)
				const bool bWay = !bCut && FVector::Dist(Want, C) < 900.0 && !W->SweepSingleByChannel(H, Want, C, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(20.f), Q);
				Want = bWay ? FMath::VInterpTo(Want, C, Dt, 2.5f) : C; bCut = false;
				const FRotator To = (Mid - Want).Rotation(); Rot = bWay ? FMath::RInterpTo(Rot, To, Dt, 4.f) : To;
				if (T != 0.f && bWay) Side = FRotator::NormalizeAxis(Side + T * FMath::Min(1.f, Dt));
				return true;
			}
		return true;   // (no free place just now: the camera stays where it is)
	}
	void Place(APlayerController* PC)
	{
		APawn* P = PC->GetPawn(); if (!P || !PC->PlayerCameraManager) return;
		if (ACharacter* Ch = Cast<ACharacter>(P)) { Ch->SetActorEnableCollision(false); if (Ch->GetCharacterMovement()) { Ch->GetCharacterMovement()->StopMovementImmediately(); Ch->GetCharacterMovement()->DisableMovement(); } }
		FVector W = Want; FRotator R = Rot;
		if (FlyLen > 0.0)
		{
			const double T0 = FMath::Clamp((FPlatformTime::Seconds() - FlyStart) / FlyLen, 0.0, 1.0), T = T0 * T0 * (3.0 - 2.0 * T0); W = FMath::Lerp(From, Want, T); R = FMath::Lerp(RotFrom, Rot, (float)T);
			FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(ViewsCam), false, P);
			const FVector Ahead = W + (Want - From).GetSafeNormal() * 60.0;
			if (P->GetWorld()->SweepSingleByChannel(H, W, Ahead, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(22.f), Q) || Around(Ahead - FVector(0, 0, 90.f), 70.0) > 0)
			{
				if (H.bBlockingHit) ADP_LOG(TEXT("Views: the flight ends before %s"), H.GetActor() ? *H.GetActor()->GetName() : TEXT("something"));
				Want = W; Rot = R; FlyLen = 0.0;   // (it waits there)
			}
		}
		const FVector D = W - PC->PlayerCameraManager->GetCameraLocation();
		P->SetActorLocation(P->GetActorLocation() + D, false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(R);
	}
	FString Stat(TArray<float> V)
	{
		if (!V.Num()) return TEXT("-");
		V.Sort(); double S = 0; for (float f : V) S += f;
		return FString::Printf(TEXT("%.1f (1%% %.1f, worst %.1f)"), S / V.Num(), V[FMath::Min(V.Num() - 1, int32(V.Num() * 0.99f))], V.Last());
	}
	FString Mem()
	{
		FTextureMemoryStats M; RHIGetTextureMemoryStats(M);
		return FString::Printf(TEXT("textures %lld of %lld MB, %s"), (int64)(M.StreamingMemorySize / 1048576), (int64)(M.TexturePoolSize / 1048576), *ADPGraphicsMemory::Line());
	}
	void Report(const TCHAR* What)
	{
		int32 People = 0;
		People = Was.Num(); int32 Walking = 0; for (const auto& E : Going) if (E.Value.Size2D() > 30.0) ++Walking;
		const APlayerController* Pc = ViewsWorld() ? ViewsWorld()->GetFirstPlayerController() : nullptr;
		const int32 Close = Pc && Pc->PlayerCameraManager ? Around(Pc->PlayerCameraManager->GetCameraLocation(), 3000.0) : 0;
		uint64 S = 0, M = 0; ADPFrameGen::Counts(S, M); const double Sec = FMath::Max(0.1, FPlatformTime::Seconds() - MStart);
		ADP_LOG(TEXT("Views: %-46s frame %s | gpu %s | render %s | game %s | shown %.0f/s (%.0f made) | %d people, %d walking, %d within 30 m | graphics memory %s"), What, *Stat(Fr), *Stat(Gp), *Stat(Rn), *Stat(Gm), (S - Shown0) / Sec, (M - Made0) / Sec, People, Walking, Close, *Mem());
		ADP_LOG(TEXT("Views: layers: %s"), *ADPLayers::Counts());
	}
	void BeginMeasure(double Sec, const FString& N) { Name = N; Fr.Reset(); Gp.Reset(); Rn.Reset(); Gm.Reset(); MStart = FPlatformTime::Seconds(); Until = MStart + Sec; ADPFrameGen::Counts(Shown0, Made0); }

	// ---- the pictures' subjects (look, rel, until)
	struct FSub
	{
		TWeakObjectPtr<AActor> Who;                // (what it belongs to; none: a place only)
		FString Nm; FBox Box = FBox(ForceInit);    // (world)
		FVector Fw = FVector::ForwardVector, Stays = FVector::ZeroVector;
	};
	FSub Sub; bool bSub = false, bMissing = false, bRel = false, bFovLook = false;
	FVector RelAt = FVector::ZeroVector, LookOff = FVector::ZeroVector; FRotator RelRot = FRotator::ZeroRotator;
	FString Waits, Shot; int32 ShotAt = 0, TalkAt = 0; double NextLook = 0.0;

	FBox PersonBox(const AActor* Pa) { const FVector Foot = Pa->GetActorLocation(); return FBox(Foot - FVector(38, 38, 0), Foot + FVector(38, 38, 182)); }
	void Bodies(UWorld* W, TArray<AActor*>& Out)
	{
		for (const auto& E : Was) if (AActor* Pa = E.Key.Get()) if (!Pa->IsHidden()) Out.AddUnique(Pa);
		if (AADPPeople* Pp = AADPPeople::Get(W)) for (const AADPPeople::FWho& Q : Pp->Everyone()) if (AActor* Pa = Q.A.Get()) if (!Pa->IsHidden()) Out.AddUnique(Pa);
		for (TObjectIterator<UPaxAnimInstance> It; It; ++It)   // (the crew too; never the player's own body)
		{
			const USkeletalMeshComponent* C = It->GetSkelMeshComponent();
			AActor* Pa = C && C->GetWorld() == W ? C->GetOwner() : nullptr;
			if (Pa && !Pa->IsHidden() && !Pa->IsA<APawn>() && !(Pa->GetAttachParentActor() && Pa->GetAttachParentActor()->IsA<APawn>())) Out.AddUnique(Pa);
		}
	}
	bool FindSub(UWorld* W, const FString& N, const FVector& Close, FSub& Out)
	{
		double Best = 1e18; bool bFound = false;
		const auto Offer = [&](AActor* Who, const FString& Nm, const FBox& Box, const FVector& Fw)
		{
			if (!Box.IsValid) return;
			const double D = FVector::DistSquared(Box.GetCenter(), Close); if (D >= Best) return;
			Best = D; bFound = true; Out = FSub(); Out.Who = Who; Out.Nm = Nm; Out.Box = Box; Out.Fw = Fw.GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
			Out.Stays = Who ? Who->GetActorLocation() : Box.GetCenter();
		};
		const auto Person = [&](AActor* Pa) { Offer(Pa, Pa->GetName(), PersonBox(Pa), Pa->GetActorRightVector()); };
		FString Kind, Of;
		if (N.Split(TEXT("@"), &Kind, &Of))   // (somebody beside it, or on it)
		{
			FSub To; if (!FindSub(W, Of, Close, To)) return false;
			TArray<AActor*> Ps; Bodies(W, Ps);
			for (AActor* Pa : Ps)
			{
				const FVector Foot = Pa->GetActorLocation();
				const bool bIn = To.Box.IsInsideXY(Foot) && Foot.Z > To.Box.Min.Z + 15.f && Foot.Z < To.Box.Max.Z;
				if (Kind == TEXT("aboard") ? bIn : (!bIn && To.Box.ExpandBy(FVector(400, 400, 100)).IsInside(Foot))) Person(Pa);
			}
			return bFound;
		}
		if (N == TEXT("person") || N.StartsWith(TEXT("person:")))
		{
			const FString Before = Where; Where = N.Mid(7);
			TArray<AActor*> Ps; Bodies(W, Ps);
			for (AActor* Pa : Ps) if (Here(Pa->GetActorLocation())) Person(Pa);
			Where = Before;
			return bFound;
		}
		if (N == TEXT("seated"))
		{
			if (AADPPeople* Pp = AADPPeople::Get(W)) for (const AADPPeople::FWho& Q : Pp->Everyone()) if (AActor* Pa = Q.A.Get()) if (!Pa->IsHidden() && Pp->IsHeld(Pa)) Person(Pa);
			return bFound;
		}
		if (N == TEXT("mast"))
		{
			for (const FVector2D& S : ALightMasts::Spots()) { const FVector Foot = FTerminalPlan::ToWorld(S.X, 0.f, S.Y); Offer(nullptr, N, FBox(Foot - FVector(60, 60, 0), Foot + FVector(60, 60, 1500)), FVector(0, 1, 0)); }
			return bFound;
		}
		if (N == TEXT("jetway"))
		{
			if (const AJetwayRigs* J = AJetwayRigs::Get(W))
				for (int32 i = 0; i < J->Num(); ++i)
				{
					FVector Ro, Dr; if (!J->Frame(i, Ro, Dr)) continue;
					FBox Bj(ForceInit); Bj += Ro; Bj += Dr; Offer(nullptr, J->Name(i), Bj.ExpandBy(FVector(200, 200, 180)), Dr - Ro);
				}
			return bFound;
		}
		if (N == TEXT("tower"))
		{
			const FVector Foot = FTerminalPlan::ToWorld(ATowerADC::AtX, 0.f, ATowerADC::AtZ);
			Offer(nullptr, N, FBox(Foot - FVector(700, 700, 0), Foot + FVector(700, 700, (ATowerADC::CabY + 7.f) * 100.f)), FVector(0, 1, 0));
			return bFound;
		}
		const bool bState = N == TEXT("spare") || N == TEXT("broken") || N == TEXT("charging") || N == TEXT("dirty") || N == TEXT("washed");
		if (const AGSE* Gse = AGSE::Find(W))
			for (const AGSE::FTractor& T : Gse->Vehicles())
			{
				ABabylonModel* M = T.M.Get(); if (!M || M->IsHidden() || T.bAway) continue;
				float Dirt = -1.f; if (const FVehicleDirtState* Ds = T.Dirt.S.Get()) { Dirt = 0.f; for (const float Zn : Ds->Zone) Dirt += Zn / 12.f; }
				const bool bIs = N == TEXT("dirty") ? Dirt > 0.35f : N == TEXT("washed") ? Dirt >= 0.f && Dirt < 0.08f : N == TEXT("spare") ? T.bSpare : N == TEXT("broken") ? T.Brk != 0 : N == TEXT("charging") ? (T.Ch.IsValid() && T.Ch->Plug == FBusCharge::Inlet) : T.Name.Contains(N);
				if (bIs) Offer(M, T.Name, M->GetComponentsBoundingBox(true), M->GetActorForwardVector());
			}
		if (bFound || bState) return bFound;
		for (TActorIterator<ABabylonModel> It(W); It; ++It) if (!It->IsHidden() && It->ModelName().Contains(N)) Offer(*It, It->ModelName(), It->GetComponentsBoundingBox(true), It->GetActorForwardVector());
		if (bFound) return true;
		for (TActorIterator<AActor> It(W); It; ++It) if (!It->IsHidden() && !It->IsA<APawn>() && (It->GetName().Contains(N) || It->GetClass()->GetName().Contains(N))) Offer(*It, It->GetName(), It->GetComponentsBoundingBox(true), It->GetActorForwardVector());
		if (bFound) return true;
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			UStaticMeshComponent* C = *It; const UStaticMesh* Mesh = C->GetWorld() == W && C->IsRegistered() && C->IsVisible() ? C->GetStaticMesh().Get() : nullptr;
			if (!Mesh || !Mesh->GetName().Contains(N)) continue;
			if (const UInstancedStaticMeshComponent* Many = Cast<UInstancedStaticMeshComponent>(C))
			{
				const FBox One = Mesh->GetBoundingBox(); FTransform X;
				for (int32 i = 0; i < Many->GetInstanceCount(); ++i) if (Many->GetInstanceTransform(i, X, true) && X.GetScale3D().GetAbsMax() > 0.01) Offer(C->GetOwner(), Mesh->GetName(), One.TransformBy(X), X.GetUnitAxis(EAxis::X));
			}
			else Offer(C->GetOwner(), Mesh->GetName(), C->Bounds.GetBox(), C->GetForwardVector());
		}
		return bFound;
	}
	void SubLog(const FSub& S) { const FVector Mid = S.Box.GetCenter(); ADP_LOG(TEXT("Views: subject %s at %.1f %.1f %.1f"), *S.Nm, Mid.X / 100.f, S.Box.Min.Z / 100.f, -Mid.Y / 100.f); }
	/** Nothing but the subject itself between the camera and it (its middle and its four sides as seen from there). */
	bool ClearTo(UWorld* W, const AActor* Me, const FSub& S, const FVector& C)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ViewsCam), false, Me);
		if (AActor* Own = S.Who.Get())
			if (Own->GetComponentsBoundingBox(true).GetSize().Size() < S.Box.GetSize().Size() * 3.0)   // (a mesh of something far bigger: that stays in the way)
			{
				TArray<AActor*> Parts; Own->GetAttachedActors(Parts, true, true); Parts.Add(Own); Q.AddIgnoredActors(Parts);
			}
		const FVector Mid = S.Box.GetCenter(), E = S.Box.GetExtent(), Rt = FVector::CrossProduct(FVector::UpVector, Mid - C).GetSafeNormal2D();
		const double Wide = (FMath::Abs(Rt.X) * E.X + FMath::Abs(Rt.Y) * E.Y) * 0.75;
		const FBox By = S.Box.ExpandBy(12.0);
		const FVector Ends[5] = { Mid, Mid + Rt * Wide, Mid - Rt * Wide, Mid + FVector(0, 0, E.Z * 0.8), Mid - FVector(0, 0, E.Z * 0.6) };
		FHitResult H;
		for (const FVector& T : Ends) if (W->LineTraceSingleByChannel(H, C, T, ECC_Visibility, Q) && !By.IsInside(FVector(H.ImpactPoint))) return false;
		return !(W->SweepSingleByChannel(H, C, Mid, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(20.f), Q) && !By.IsInside(FVector(H.ImpactPoint)));
	}
	/** How far off the whole box fits the 16:9 picture, 60 degrees across. */
	double FitOff(const FSub& S, const FVector& Dir)
	{
		const FRotationMatrix M((-Dir).Rotation()); const FVector Ax = M.GetUnitAxis(EAxis::X), Ay = M.GetUnitAxis(EAxis::Y), Az = M.GetUnitAxis(EAxis::Z), E = S.Box.GetExtent();
		const double Tw = FMath::Tan(FMath::DegreesToRadians(30.0)), Th = Tw * 9.0 / 16.0; double Off = 60.0;
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector P((i & 1) ? E.X : -E.X, (i & 2) ? E.Y : -E.Y, (i & 4) ? E.Z : -E.Z);
			Off = FMath::Max(Off, FMath::Max(FMath::Abs(FVector::DotProduct(P, Ay)) / Tw, FMath::Abs(FVector::DotProduct(P, Az)) / Th) * 1.12 - FVector::DotProduct(P, Ax));
		}
		return Off;
	}
	bool LookFrom(UWorld* W, const AActor* Me, const FSub& S, double Dist, double Deg, double High, FVector& Cam)
	{
		const double Turns[] = { 0., 25., -25., 50., -50., 80., -80., 120., -120., 160. }, Further[] = { 1., 1.3, 0.8 }, Ups[] = { 0., 120., 300. };
		const FVector Mid = S.Box.GetCenter();
		for (const double Fu : Further)
		{
			if (Dist <= 0.0 && Fu < 1.0) continue;   // (the whole of it has to fit)
			for (const double Up : Ups) for (const double Tu : Turns)
			{
				const FVector Flat = S.Fw.RotateAngleAxis(Deg + Tu, FVector::UpVector), Rise(0.0, 0.0, S.Box.Min.Z + High * 100.0 + Up - Mid.Z);
				double D = Dist * 100.0;
				if (Dist <= 0.0) { D = 300.0; for (int32 i = 0; i < 4; ++i) D = FitOff(S, (Flat * D + Rise).GetSafeNormal()); }
				const FVector C = Mid + Flat * D * Fu + Rise;
				if (Solid(W, C, Me) || !ClearTo(W, Me, S, C)) continue;
				Cam = C; return true;
			}
		}
		return false;
	}
	/** Each frame: the camera stays with a subject that moves. */
	void KeepSub(UWorld* W, const AActor* Me)
	{
		const AActor* Own = Sub.Who.Get(); if (!Own) return;
		if (bRel)
		{
			const FTransform X(Own->GetActorRotation(), Own->GetActorLocation());
			Want = X.TransformPosition(RelAt); Rot = (X.GetRotation() * RelRot.Quaternion()).Rotator(); Rot.Roll = 0.0; return;
		}
		const FVector Mv = Own->GetActorLocation() - Sub.Stays; if (Mv.SizeSquared() < 1.0) return;
		Sub.Stays = Own->GetActorLocation(); Sub.Box = Sub.Box.ShiftBy(Mv);
		const FVector C = Sub.Box.GetCenter() + LookOff; FHitResult H; const FCollisionQueryParams Q(SCENE_QUERY_STAT(ViewsCam), false, Me);
		if (!W->SweepSingleByChannel(H, Want, C, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(20.f), Q) && !Solid(W, C, Me)) Want = C;
		Rot = (Sub.Box.GetCenter() - Want).Rotation();
	}
	void FreeFov(APlayerController* PC) { if (bFovLook) { PC->PlayerCameraManager->UnlockFOV(); bFovLook = false; } }
	void LockFov(APlayerController* PC, float Deg) { PC->PlayerCameraManager->SetFOV(Deg); bFovLook = true; }
	/** Where one stands to talk: before them, a step and a half off; false if there is no free place. */
	bool TalkSpot(UWorld* W, const AActor* Me, const AActor* Pa)
	{
		const FVector Head = Pa->GetActorLocation() + FVector(0, 0, 158); FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ViewsCam), false, Me); Q.AddIgnoredActor(Pa);
		for (const double Tu : { 0., 25., -25., 50., -50., 80., -80. })
		{
			const FVector C = Head + Pa->GetActorRightVector().GetSafeNormal2D().RotateAngleAxis(Tu, FVector::UpVector) * 160.0;
			if (W->SweepSingleByChannel(H, Head, C, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(22.f), Q) || Solid(W, C, Me)) continue;
			Want = C; Rot = (Head - FVector(0, 0, 25) - C).Rotation(); return true;
		}
		return false;
	}
	void Lives(UWorld* W)
	{
		const IConsoleVariable* Rain = IConsoleManager::Get().FindConsoleVariable(TEXT("adp.GlassRain"));
		ADP_LOG(TEXT("Views: lives: weather %s%s, rain on the glass %d (filled %.2f, %.3f a second), %d people about"), ASunClock::ThemeName(ASunClock::Theme), ASunClock::bTestHold ? TEXT(" (held)") : TEXT(""),
			Rain ? Rain->GetInt() : -1, UGlassRain::Fill, UGlassRain::FillRate, Was.Num());
		ADP_LOG(TEXT("Views: lives: people %s"), *ADPLayers::Counts());
		for (const bool bArr : { false, true })
		{
			const TArray<FADPBoardRow>& Rows = ADPBoards::Shown(W, bArr); FString Ln;
			for (int32 i = 0; i < FMath::Min(4, Rows.Num()); ++i) Ln += FString::Printf(TEXT(" | %s %s %s %s"), *Rows[i].Time(), *Rows[i].Flight, *Rows[i].Place, *Rows[i].Status);
			ADP_LOG(TEXT("Views: lives: board %s: %d rows, turn %d%s"), bArr ? TEXT("arrivals") : TEXT("departures"), Rows.Num(), ADPBoards::Turn(), *Ln);
		}
		FString Rem; int32 NRem = 0, NAir = 0;
		for (TActorIterator<ABabylonModel> It(W); It; ++It)
			if (!It->IsHidden() && It->ModelName() == TEXT("A320"))
			{
				++NAir; const FVector Foot = It->GetActorLocation();
				if (-Foot.Y > 27000.0 && -Foot.Y < 37000.0 && Foot.Z < 1000.0) { ++NRem; Rem += FString::Printf(TEXT(" at %.0f %.0f"), Foot.X / 100.0, -Foot.Y / 100.0); }
			}
		ADP_LOG(TEXT("Views: lives: aircraft %d, on the remote apron %d%s"), NAir, NRem, *Rem);
		if (const AGSE* Gse = AGSE::Find(W))
		{
			FString Buses, Charge, Spare, Broken; int32 NAll = 0;
			for (const AGSE::FTractor& T : Gse->Vehicles())
			{
				if (!T.M.IsValid()) continue;
				++NAll; const FString Pl = FString::Printf(TEXT(" | %s (%.0f %.0f%s)"), *T.Name, T.At.X / 100.0, -T.At.Y / 100.0, T.bAway ? TEXT(", away") : TEXT(""));
				if (T.Bu.IsValid())
				{
					Buses += Pl + FString::Printf(TEXT(" %.1f m/s"), T.Speed / 100.f);
					if (T.Ch.IsValid()) Buses += FString::Printf(TEXT(", battery %.0f %%"), T.Ch->Soc * 100.f);
				}
				if (T.Ch.IsValid() && (T.Ch->Plug != FBusCharge::Holster || T.Ch->Stage != FBusCharge::Idle || !T.Ch->Why.IsEmpty())) Charge += Pl + FString::Printf(TEXT(" plug %d stage %d %s"), int32(T.Ch->Plug), int32(T.Ch->Stage), *T.Ch->Why);
				if (T.bSpare) Spare += Pl;
				if (T.Brk) Broken += Pl + FString::Printf(TEXT(" stage %d: %s, %s"), int32(T.Brk), *T.BrkFault, T.bBrkFix ? TEXT("can be mended") : TEXT("can not be mended"));
			}
			ADP_LOG(TEXT("Views: lives: %d ground vehicles; buses%s"), NAll, Buses.IsEmpty() ? TEXT(" none") : *Buses);
			ADP_LOG(TEXT("Views: lives: charging%s"), Charge.IsEmpty() ? TEXT(" none") : *Charge);
			ADP_LOG(TEXT("Views: lives: spares%s"), Spare.IsEmpty() ? TEXT(" none") : *Spare);
			ADP_LOG(TEXT("Views: lives: broken%s"), Broken.IsEmpty() ? TEXT(" none") : *Broken);
		}
		else ADP_WARN(TEXT("Views: lives: no ground vehicles"));
		const UADPSim* Sim = UADPSim::Get(W); const UStaffPeople* St = UStaffPeople::Get(W);
		if (Sim && St)
		{
			TMap<FString, int32> All, Work; TMap<FString, TMap<FString, int32>> How;
			for (const FADPPerson& P : Sim->World.People)
			{
				if (P.AirlineId != UADPSim::Me()) continue;
				FString DoingNow, Spot; if (!St->Status(P.Id, DoingNow, Spot)) DoingNow = St->OnShift(P.Id) ? TEXT("on shift, not in 3D") : TEXT("off shift or no job in 3D");
				++All.FindOrAdd(P.Role); if (St->ActorOf(P.Id)) ++Work.FindOrAdd(P.Role);
				++How.FindOrAdd(P.Role).FindOrAdd(DoingNow.Left(50));
			}
			for (const auto& E : All)
			{
				FString Ln; int32 Nth = 0;
				for (const auto& D : How[E.Key]) { if (++Nth > 6) break; Ln += FString::Printf(TEXT(" | %d %s"), D.Value, *D.Key); }
				ADP_LOG(TEXT("Views: lives: hired %s %d, %d there%s"), *E.Key, E.Value, Work.FindRef(E.Key), *Ln);
			}
			if (!All.Num()) ADP_LOG(TEXT("Views: lives: hired nobody"));
			const FString Wt = St->Waiting().Replace(TEXT("\n"), TEXT("; ")).Left(400);
			ADP_LOG(TEXT("Views: lives: jobs wait: %s"), *Wt);
		}
		else ADP_WARN(TEXT("Views: lives: no career, or no staff"));
		ADP_LOG(TEXT("Views: lives: the feet: see the Feet: lines (-FeetCheck)"));
	}

	// the views run's own timed actions (fire truck, coach waits); false while a wait holds the steps
	bool Pending(UWorld* W, APlayerController* PC)
	{
		const double Now = FPlatformTime::Seconds();
		if (FireEnterAt > 0.0 && Now >= FireEnterAt)
		{
			FireEnterAt = 0.0;
			if (AFireTruck* T = FireT.Get(); T && PC->GetPawn()) { T->Enter(PC->GetPawn()); FireRouteAt = Now + 5.0; ADP_LOG(TEXT("Views: fire truck %s: in by its door"), *T->GetName()); }
			else ADP_WARN(TEXT("Views: fire truck gone before it could be entered"));
		}
		if (FireRouteAt > 0.0 && Now >= FireRouteAt)
		{
			FireRouteAt = 0.0;
			if (AFireTruck* T = Cast<AFireTruck>(PC->GetPawn()))
			{
				const FVector2D S(T->GetActorLocation().X / 100.0, -T->GetActorLocation().Y / 100.0);
				T->SetTestRoute({S, FVector2D(S.X, 26.0), FVector2D(-1205.0, 40.0), FVector2D(-1205.0, 190.0), FVector2D(-1190.0, 196.0), FVector2D(-420.0, 196.0), FVector2D(-230.0, 196.0)}, 30.f);
				T->SetView(true); ADP_LOG(TEXT("Views: fire drive: out along the route at 30 km/h, chase view"));
			}
			else ADP_WARN(TEXT("Views: fire drive: not in the truck after 5 s (too far from its door?) - it stays"));
		}
		if (FireExitAt > 0.0 && Now >= FireExitAt)
		{
			FireExitAt = 0.0;
			if (AFireTruck* T = Cast<AFireTruck>(PC->GetPawn())) { ADP_LOG(TEXT("Views: fire park: stopped at %.0f km/h, out"), T->SpeedKmh()); T->Exit(); }
		}
		if (CoachMode)
		{
			ACoaches* C = ACoaches::Find(); bool bDone = !C; float Best = 1e9f;
			if (C)
			{
				bool bAny = false;
				for (int32 c = 0; c < C->Num(); ++c)
				{
					bAny |= C->OnRoute(c);
					if (C->ModelOf(c) && CoachHome.IsValidIndex(c)) Best = FMath::Min(Best, float(FVector::Dist2D(C->ModelOf(c)->GetActorLocation(), CoachHome[c]) / 100.0));
				}
				bDone = CoachMode == 3 ? Best <= CoachNear : !bAny;
			}
			if (bDone) ADP_LOG(TEXT("Views: coachwait done (%s; the nearest coach %.0f m from its stand)"), CoachMode == 3 ? TEXT("near") : CoachMode == 1 ? TEXT("at the hub") : TEXT("home"), Best);
			else if (Now >= CoachEnd) { ADP_WARN(TEXT("Views: coachwait timed out - the coaches still on their way (nearest %.0f m from its stand)"), Best); bDone = true; }
			if (bDone) CoachMode = 0;
			return bDone;
		}
		return true;
	}
	// the camera put at To, looking at Aim, held or flown there in Sec
	void AimAt(UWorld* W, APlayerController* PC, const FVector& To, const FVector& Aim, double Sec, const FString& Label)
	{
		bSub = bRel = bMissing = false; FreeFov(PC);
		if (Sec <= 0.0 && Solid(W, To, PC->GetPawn())) { ADP_WARN(TEXT("Views: %s is inside something - left out"), *Label); bMissing = true; return; }
		From = PC->PlayerCameraManager->GetCameraLocation(); RotFrom = Rot; Want = To; Rot = (Aim - To).Rotation(); Rot.Roll = 0.f; bHeld = true; FlyLen = 0.0;
		ADP_LOG(TEXT("Views: %s at %.1f %.1f %.1f %.0f %.0f"), *Label, To.X / 100.0, To.Z / 100.0, -To.Y / 100.0, Rot.Yaw, Rot.Pitch);
		if (Sec > 0.0) { FlyLen = Sec; FlyStart = FPlatformTime::Seconds(); BeginMeasure(Sec, TEXT("fly ") + Label); }
	}
	void JetLog(AJetwayRigs* J, int32 Ji)
	{
		static const TCHAR* St[] = { TEXT("parked"), TEXT("docking"), TEXT("docked"), TEXT("undocking"), TEXT("manual") };
		const int32 S = int32(J->State(Ji));
		ADP_LOG(TEXT("Views: jetway %d %s, beacons %s%s, hour %.1f"), Ji, S >= 0 && S < 5 ? St[S] : TEXT("?"),
			AJetwayRigs::BeaconForce == 1 ? TEXT("held lit") : AJetwayRigs::BeaconForce == 0 ? TEXT("held dark") : TEXT("as usual (flashing while it moves)"),
			AJetwayRigs::bBeaconTest ? TEXT(", all flashing (test)") : TEXT(""), ASunClock::HourOverride);
	}

	bool Tick(float Dt)
	{
		UWorld* W = ViewsWorld(); APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		if (!PC || !PC->PlayerCameraManager) return true;
		LookAtPeople(W);
		if (bFollow) FollowAt(W, PC->GetPawn(), Dt);
		else if (bSub && bHeld) KeepSub(W, PC->GetPawn());
		if (bHeld) Place(PC);
		if (!Pending(W, PC)) return true;
		for (TStrongObjectPtr<UTextureRenderTarget2D>& T : Eaten) UKismetRenderingLibrary::ClearRenderTarget2D(W, T.Get(), FLinearColor::Black);
		if (!Name.IsEmpty()) { Fr.Add(Dt * 1000.f); Gp.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles())); Rn.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime)); Gm.Add(FPlatformTime::ToMilliseconds(GGameThreadTime)); }
		if (Need > 0)
		{
			if (Was.Num() >= Need) { ADP_LOG(TEXT("Views: %d people are about"), Was.Num()); Need = 0; Until = 0.0; }
			else if (FPlatformTime::Seconds() >= Until) { ADP_ERROR(TEXT("Views: FAILED - %d people about instead of %d, the run ends here"), Was.Num(), Need); FPlatformMisc::RequestExit(false); return false; }
		}
		if (!Waits.IsEmpty())   // (until)
		{
			FSub S; const double Now = FPlatformTime::Seconds();
			if (Now < NextLook && Now < Until) return true;
			NextLook = Now + 1.0;
			if (FindSub(W, Waits, bHeld ? Want : PC->PlayerCameraManager->GetCameraLocation(), S)) SubLog(S);
			else if (Now < Until) return true;
			else ADP_WARN(TEXT("Views: %s did not come"), *Waits);
			Waits.Reset(); Until = 0.0;
		}
		if (FPlatformTime::Seconds() < Until) return true;
		if (ShotAt > 0)   // (shot4k: three seconds for the streaming, then the picture, then two more)
		{
			const FString Png = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/run") / (Shot + TEXT(".png")));
			if (ShotAt == 1)
			{
				PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 3840x2160 filename=\"%s\""), *Png)); ADP_LOG(TEXT("Views: picture %s"), *Png);
				ShotAt = 2; Until = FPlatformTime::Seconds() + 2.0; return true;
			}
			ShotAt = 0; if (!bSub) FreeFov(PC);
			if (!IFileManager::Get().FileExists(*Png)) ADP_WARN(TEXT("Views: the picture %s is not written yet"), *Shot);
		}
		if (TalkAt > 0)   // (they are before you: E)
		{
			TalkAt = 0; AADPPeople* Pp = AADPPeople::Get(W); AADPHUD* Hd = Cast<AADPHUD>(PC->GetHUD());
			FVector Eye; FRotator Lk; PC->GetPlayerViewPoint(Eye, Lk); FHitResult H; const FCollisionQueryParams Q(SCENE_QUERY_STAT(ViewsCam), false, PC->GetPawn());
			const bool bHit = W->LineTraceSingleByChannel(H, Eye, Eye + Lk.Vector() * 290.0, ECC_Visibility, Q);
			if (Pp && Hd && ((bHit && Pp->Find(H.GetActor())) || (!bHit && Pp->Beside(PC->GetPawn())))) Hd->Interact();
			if (Pp && Pp->MenuOpen()) { bHeld = false; ADP_LOG(TEXT("Views: talk open with %s"), *Pp->MenuTitle); Until = FPlatformTime::Seconds() + 3.0; return true; }
			bMissing = true; ADP_WARN(TEXT("Views: subject missing: the talk did not open (%s)"), bHit && H.GetActor() ? *H.GetActor()->GetName() : TEXT("nobody before you"));
		}
		if (!Name.IsEmpty()) { Report(*Name); Name.Reset(); bFollow = false; }
		if (At >= Steps.Num()) return false;
		const FString L = Steps[At++].TrimStartAndEnd();
		FString K, A; if (!L.Split(TEXT(" "), &K, &A)) K = L;
		TArray<FString> V; A.ParseIntoArrayWS(V);
		if (Cast<AFireTruck>(PC->GetPawn()) && (K == TEXT("view") || K == TEXT("fly") || K == TEXT("look") || K == TEXT("rel") || K == TEXT("shotcam") || (K == TEXT("jetway") && A.Contains(TEXT(" cam")))))
		{ ADP_WARN(TEXT("Views: %s left out - the camera is in the fire truck (fire park first)"), *L); return true; }
		if ((K == TEXT("view") && V.Num() >= 5) || (K == TEXT("fly") && V.Num() >= 6))
		{
			const FVector To(FCString::Atof(*V[0]) * 100.f, -FCString::Atof(*V[2]) * 100.f, FCString::Atof(*V[1]) * 100.f);
			bSub = bRel = bMissing = false; FreeFov(PC);
			if (K == TEXT("view") && Solid(W, To, PC->GetPawn())) { ADP_WARN(TEXT("Views: %s is inside something - left out"), *A); bMissing = true; return true; }
			From = PC->PlayerCameraManager->GetCameraLocation(); RotFrom = Rot;
			Want = To;
			Rot = FRotator(FCString::Atof(*V[4]), FCString::Atof(*V[3]), 0.f); bHeld = true; FlyLen = 0.0;
			if (K == TEXT("fly")) { FlyLen = FCString::Atof(*V[5]); FlyStart = FPlatformTime::Seconds(); BeginMeasure(FlyLen, TEXT("fly to ") + A); }
			else ADP_LOG(TEXT("Views: at %s"), *A);
		}
		else if (K == TEXT("cmd")) { ADP_LOG(TEXT("Views: %s"), *A); PC->ConsoleCommand(A); }
		else if (K == TEXT("look") && V.Num() > 3)
		{
			const FString Nm = FString::JoinBy(TArrayView<FString>(V).RightChop(3), TEXT(" "), [](const FString& Word) { return Word; });
			bSub = bRel = false; bMissing = true; FreeFov(PC); FVector C; FSub S;
			if (!bHeld) Want = PC->PlayerCameraManager->GetCameraLocation();
			if (!FindSub(W, Nm, Want, S)) ADP_WARN(TEXT("Views: subject missing: %s is not there"), *Nm);
			else if (!LookFrom(W, PC->GetPawn(), S, FCString::Atof(*V[0]), FCString::Atof(*V[1]), FCString::Atof(*V[2]), C)) { SubLog(S); ADP_WARN(TEXT("Views: subject missing: no clear view of %s"), *Nm); }
			else
			{
				SubLog(S); Sub = S; LookOff = C - S.Box.GetCenter(); Want = C; Rot = (-LookOff).Rotation(); bSub = bHeld = true; bMissing = bFollow = false; FlyLen = 0.0; LockFov(PC, 60.f);
				ADP_LOG(TEXT("Views: at %.1f %.1f %.1f %.0f %.0f"), C.X / 100.0, C.Z / 100.0, -C.Y / 100.0, Rot.Yaw, Rot.Pitch);
			}
		}
		else if (K == TEXT("rel") && V.Num() > 5)
		{
			const FString Nm = FString::JoinBy(TArrayView<FString>(V).RightChop(5), TEXT(" "), [](const FString& Word) { return Word; });
			bSub = bRel = false; bMissing = true; FreeFov(PC); FSub S;
			if (!bHeld) Want = PC->PlayerCameraManager->GetCameraLocation();
			if (!FindSub(W, Nm, Want, S) || !S.Who.IsValid()) ADP_WARN(TEXT("Views: subject missing: %s is not there"), *Nm);
			else
			{
				const FVector RelTo(FCString::Atof(*V[0]) * 100.0, -FCString::Atof(*V[1]) * 100.0, FCString::Atof(*V[2]) * 100.0);
				const FVector C = FTransform(S.Who->GetActorRotation(), S.Who->GetActorLocation()).TransformPosition(RelTo);
				if (Around(C - FVector(0, 0, 120), 45.0) > 0) ADP_WARN(TEXT("Views: subject missing: somebody is where the camera goes in %s"), *Nm);
				else { SubLog(S); Sub = S; RelAt = RelTo; RelRot = FRotator(FCString::Atof(*V[4]), -FCString::Atof(*V[3]), 0.0); bSub = bRel = bHeld = true; bMissing = bFollow = false; FlyLen = 0.0; KeepSub(W, PC->GetPawn()); LockFov(PC, 90.f); }
			}
		}
		else if (K == TEXT("until") && V.Num() > 1) { FString Sec; A.Split(TEXT(" "), &Sec, &Waits); Waits.TrimStartAndEndInline(); NextLook = 0.0; Until = FPlatformTime::Seconds() + FCString::Atof(*V[0]); }
		else if (K == TEXT("shot4k") && V.Num())
		{
			if (bMissing) { ADP_WARN(TEXT("Views: picture %s left out, its subject is missing"), *V[0]); return true; }
			Shot = V[0]; ShotAt = 1; Until = FPlatformTime::Seconds() + 3.0;
			const AADPPeople* Pp = AADPPeople::Get(W);
			if (!bSub && !(Pp && Pp->MenuOpen())) LockFov(PC, 80.f);   // (the window's own is as wide as two screens)
		}
		else if (K == TEXT("talk"))
		{
			bSub = bRel = bFollow = false; bMissing = true; FreeFov(PC); FlyLen = 0.0;
			AADPPeople* Pp = AADPPeople::Get(W); const FVector Was1 = Want, You = bHeld ? Want : PC->PlayerCameraManager->GetCameraLocation(); const FRotator Rot1 = Rot;
			TArray<AActor*> By;
			if (Pp)
				for (const AADPPeople::FWho& Q : Pp->Everyone())
				{
					AActor* Pa = Q.A.Get(); if (!Pa || Pa->IsHidden() || Q.bUnruly || Pp->IsHeld(Pa)) continue;
					const FVector* Sp = Going.Find(Pa); if (!Sp || Sp->Size2D() < 10.0) By.Add(Pa);
				}
			By.Sort([&You](const AActor& One, const AActor& Two) { return FVector::DistSquared(One.GetActorLocation(), You) < FVector::DistSquared(Two.GetActorLocation(), You); });
			for (int32 i = 0; i < FMath::Min(8, By.Num()) && !TalkAt; ++i)
				if (TalkSpot(W, PC->GetPawn(), By[i])) { TalkAt = 1; bHeld = true; bMissing = false; Until = FPlatformTime::Seconds() + 1.5; ADP_LOG(TEXT("Views: talk: before %s"), *By[i]->GetName()); }
			if (!TalkAt) { Want = Was1; Rot = Rot1; ADP_WARN(TEXT("Views: subject missing: nobody to talk to (%d stand)"), By.Num()); }
		}
		else if (K == TEXT("talkend"))
		{
			AADPPeople* Pp = AADPPeople::Get(W); AADPHUD* Hd = Cast<AADPHUD>(PC->GetHUD());
			if (Pp && Hd && Pp->MenuOpen()) Hd->Interact();
			if (Pp && Pp->MenuOpen()) { ADP_WARN(TEXT("Views: E did not end the talk")); Pp->Close(); }
			Until = FPlatformTime::Seconds() + 2.5;
		}
		else if (K == TEXT("lives")) Lives(W);
		else if (K == TEXT("weather") && A == TEXT("live")) ASunClock::ReleaseTheme();
		else if (K == TEXT("hour") && A == TEXT("live")) { ASunClock::HourOverride = -1.f; ASunClock::DayOverride = -1; ASunClock::Refresh(); }
		else if (K == TEXT("hour")) { if (ASunClock::DayOverride < 0) ASunClock::DayOverride = 172; ASunClock::HourOverride = FCString::Atof(*A); ASunClock::Refresh(); }
		else if (K == TEXT("day")) { ASunClock::DayOverride = FCString::Atoi(*A); ASunClock::Refresh(); }
		else if (K == TEXT("weather")) { if (!ASunClock::ManualWeatherAllowed()) ADP_WARN(TEXT("Views: the weather is live here, it can not be set")); ASunClock::HoldTheme(FCString::Atoi(*A)); ADP_LOG(TEXT("Views: weather %s"), ASunClock::ThemeName(ASunClock::Theme)); }
		else if (K == TEXT("wait")) Until = FPlatformTime::Seconds() + FCString::Atof(*A);
		else if (K == TEXT("measure") && V.Num()) { FString S, N; const bool bN = A.Split(TEXT(" "), &S, &N); BeginMeasure(FCString::Atof(*V[0]), bN ? N : A); }
		else if (K == TEXT("gpu")) { PC->ConsoleCommand(TEXT("ProfileGPU")); Until = FPlatformTime::Seconds() + 2.0; }
		else if (K == TEXT("hmi") && V.Num())
		{
			AADPHUD* H = Cast<AADPHUD>(PC->GetHUD());
			if (H && V[0] == TEXT("close")) H->EndHmi();
			else if (H) { bHeld = false; APaxFlow* Pg = APaxFlow::Get(W); H->OpenHmi(V[0].StartsWith(TEXT("g")) && Pg ? Pg->GateJetway(FCString::Atoi(*V[0].Mid(1))) : FCString::Atoi(*V[0])); if (AJetwayHmi* M = AJetwayHmi::Get(W); M && V.Num() > 1) M->Command(TEXT("tab:") + V[1]); }
			ADP_LOG(TEXT("Views: hmi %s"), *A);
		}
		else if (K == TEXT("eat"))
		{
			for (int32 Mb = FCString::Atoi(*A); Mb > 0; Mb -= 128)
			{
				UTextureRenderTarget2D* T = NewObject<UTextureRenderTarget2D>(); T->InitCustomFormat(4096, 4096, PF_FloatRGBA, true); T->UpdateResourceImmediate(true);
				Eaten.Add(TStrongObjectPtr<UTextureRenderTarget2D>(T));
			}
			ADP_LOG(TEXT("Views: %s MB taken; %s"), *A, *ADPGraphicsMemory::Line());
		}
		else if (K == TEXT("speed") && V.Num())
		{
			if (UADPSim* S = UADPSim::Get(W)) S->SetSpeed(FCString::Atof(*V[0]));
			UGameplayStatics::SetGlobalTimeDilation(W, V.Num() > 1 ? FCString::Atof(*V[1]) : 1.f); ADP_LOG(TEXT("Views: speed %s"), *A);
		}
		else if (K == TEXT("part") && V.Num() > 1)
		{
			// (one part of everyone off or on: what it costs - all, face, body, hair, shadow, clothes)
			const bool bOn = FCString::Atoi(*V[1]) != 0; const FString Wh = V[0]; int32 N = 0;
			if (AADPPeople* P = AADPPeople::Get(W)) for (const AADPPeople::FWho& Q : P->Everyone()) if (AActor* Pa = Q.A.Get())
			{
				TArray<UPrimitiveComponent*> Ps; Pa->GetComponents<UPrimitiveComponent>(Ps);
				for (UPrimitiveComponent* C : Ps)
				{
					const FString Cn = C->GetName(), Cl = C->GetClass()->GetName();
					const bool bHair = Cl.Contains(TEXT("Groom")), bFace = Cn.StartsWith(TEXT("Face")), bBody = Cn.StartsWith(TEXT("Body"));
					if (Wh == TEXT("shadow")) { C->SetCastShadow(bOn); ++N; }
					else if (Wh == TEXT("all") || (Wh == TEXT("hair") && bHair) || (Wh == TEXT("face") && bFace) || (Wh == TEXT("body") && bBody) || (Wh == TEXT("clothes") && !bHair && !bFace && !bBody)) { C->SetHiddenInGame(!bOn); ++N; }
				}
			}
			ADP_LOG(TEXT("Views: part %s %d (%d parts)"), *Wh, bOn ? 1 : 0, N);
		}
		else if (K == TEXT("people") && V.Num() > 1) { Need = FCString::Atoi(*V[0]); Until = FPlatformTime::Seconds() + FCString::Atof(*V[1]); }
		else if ((K == TEXT("follow") || K == TEXT("feet")) && V.Num() > (K == TEXT("feet") ? 1 : 0))
		{
			Where = K == TEXT("feet") ? FString(*V[1]) : FString();   // (feet <seconds> <landside, hall, apron, upstairs> <name>)
			FString S2, N2; const bool bN = A.Split(TEXT(" "), &S2, &N2);
			bSub = bRel = false; FreeFov(PC);
			if (!NextSubject()) ADP_WARN(TEXT("Views: nobody walks (%d people)"), Was.Num());
			if (Subject.IsValid() || !Where.IsEmpty()) { Want = PC->PlayerCameraManager->GetCameraLocation(); bHeld = bFollow = true; FlyLen = 0.0; }
			BeginMeasure(FCString::Atof(*V[0]), bN ? N2 : A);
		}
		else if (K == TEXT("textures"))
		{
			// how sharp the textures of these kit materials are just now (to-do 248)
			for (const FString& N : V)
				if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Materials/Kit/MI_%s.MI_%s"), *N, *N)))
				{
					TArray<UTexture*> Tx; M->GetUsedTextures(Tx); FString Ln;
					for (UTexture* T : Tx) if (UTexture2D* T2 = Cast<UTexture2D>(T)) Ln += FString::Printf(TEXT(" %s %d of %d%s"), *T2->GetName(), T2->GetSizeX() >> FMath::Max(0, T2->GetNumMips() - T2->GetNumResidentMips()), T2->GetSizeX(), T2->IsStreamable() ? TEXT("") : TEXT(" (always)"));
					ADP_LOG(TEXT("Views: textures of %s:%s"), *N, *Ln);
				}
				else ADP_WARN(TEXT("Views: MI_%s is missing"), *N);
		}
		else if (K == TEXT("census"))
		{
			// how many instances of each mesh are in the distance field scene (its buffers are sized by them)
			TMap<FString, FIntPoint> By; int64 All = 0, Df = 0, Tiny = 0;
			for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
			{
				UStaticMeshComponent* C = *It;
				if (C->GetWorld() != W || !C->IsRegistered() || !C->IsVisible() || !C->GetStaticMesh()) continue;
				const UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(C);
				const int32 Nn = I ? I->GetInstanceCount() : 1; All += Nn;
				if (!C->bAffectDistanceFieldLighting) continue;
				const float Rad = C->GetStaticMesh()->GetBounds().SphereRadius;
				Df += Nn; if (Rad < 15.f) Tiny += Nn;
				FIntPoint& E = By.FindOrAdd(FString::Printf(TEXT("%s / %s %s r=%.0f"), C->GetOwner() ? *C->GetOwner()->GetClass()->GetName() : TEXT("-"), *C->GetStaticMesh()->GetName(), C->bAffectDynamicIndirectLighting ? TEXT("lumen") : TEXT("-"), Rad));
				E.X += Nn; ++E.Y;
			}
			By.ValueSort([](const FIntPoint& P, const FIntPoint& Q) { return P.X > Q.X; });
			ADP_LOG(TEXT("Views: census %lld instances, %lld in the distance fields (%lld under 15 cm)"), All, Df, Tiny);
			int32 k = 0; for (const auto& E : By) { if (++k > 40) break; ADP_LOG(TEXT("Views: census %7d in %4d  %s"), E.Value.X, E.Value.Y, *E.Key); }
		}
		else if (K == TEXT("shot")) FScreenshotRequest::RequestScreenshot(A, false, false);
		else if (K == TEXT("shotui")) FScreenshotRequest::RequestScreenshot(A, true, false);   // (with the stats on it)
		else if (K == TEXT("park"))
		{
			TActorIterator<AAirfield> F(W);
			if (!F) { ADP_WARN(TEXT("Views: no airfield, nothing parked")); return true; }
			TSet<FName> Taken; TSet<ABabylonModel*> Before;
			for (TActorIterator<ABabylonModel> It(W); It; ++It) { Before.Add(*It); Taken.Append(It->Tags); }
			FAirfieldData Free = F->Data();
			Free.Stands.RemoveAll([&](const FAirfieldData::FStand& St) { return St.bRemote || Taken.Contains(FName(*(St.Id + TEXT("/A320")))); });
			const int32 N = ADPParked::Place(W, Free);
			for (TActorIterator<ABabylonModel> It(W); It; ++It) if (!Before.Contains(*It)) ForPicture.Add(*It);
			ADP_LOG(TEXT("Views: %d aircraft parked for the picture"), N);
		}
		else if (K == TEXT("parkoff")) { for (const auto& M : ForPicture) if (M.IsValid()) M->Destroy(); ForPicture.Reset(); }
		else if (K == TEXT("framegen"))
		{
			const bool bOn = A != TEXT("off");
			if (ADPFrameGen::Installed()) ADPFrameGen::SetActive(bOn); else ADP_WARN(TEXT("Views: framegen: not in place this start (AMD frame generation chosen?) - not switched"));
			ADP_LOG(TEXT("Views: frame generation asked %s - in place %d, on %d, %d shown per frame drawn"), bOn ? TEXT("ON") : TEXT("OFF"), ADPFrameGen::Installed() ? 1 : 0, ADPFrameGen::Enabled() ? 1 : 0, ADPFrameGen::Multiplier());
		}
		else if (K == TEXT("shots") || (K == TEXT("shotcam") && !Shots.Num()))
		{
			const FString F = FPaths::ProjectDir() / (K == TEXT("shots") ? A : FString(TEXT("Tools/views/camera-shots-1128.txt")));
			TArray<FString> Ls; FFileHelper::LoadFileToStringArray(Ls, *F); Shots.Reset();
			for (const FString& X : Ls) { FString Nm, Do; const FString T = X.TrimStartAndEnd(); if (!T.StartsWith(TEXT("#")) && T.Split(TEXT(" "), &Nm, &Do)) Shots.Add(Nm, Do.TrimStartAndEnd()); }
			ADP_LOG(TEXT("Views: %d camera shots from %s"), Shots.Num(), *F);
			if (K == TEXT("shotcam") && Shots.Num()) --At;   // (and the shot itself now)
			else if (K == TEXT("shotcam")) { ADP_WARN(TEXT("Views: no camera shots - %s left out"), *A); bMissing = true; }
		}
		else if (K == TEXT("shotcam"))
		{
			if (const FString* Do = Shots.Find(A)) { ADP_LOG(TEXT("Views: shot %s"), *A); Steps.Insert(*Do, At); }
			else { ADP_WARN(TEXT("Views: no camera shot called %s"), *A); bMissing = true; }
		}
		else if (K == TEXT("jetway") && V.Num() > 1)
		{
			AJetwayRigs* J = AJetwayRigs::Get(W); APaxFlow* Pf = APaxFlow::Get(W);
			if (V[0] == TEXT("beacons")) { AJetwayRigs::BeaconForce = V[1] == TEXT("on") ? 1 : -1; AJetwayRigs::bBeaconTest = V[1] == TEXT("flash"); ADP_LOG(TEXT("Views: jetway beacons %s"), *V[1]); return true; }
			const int32 Ji = Pf ? Pf->GateJetway(FCString::Atoi(*V[0])) : INDEX_NONE;
			FVector Ro, Dr;
			if (!J || Ji == INDEX_NONE || !J->Frame(Ji, Ro, Dr)) { ADP_WARN(TEXT("Views: gate %s has no jetway here%s"), *V[0], Pf ? TEXT("") : TEXT(" (no passenger flow)")); bMissing = true; return true; }
			if (V[1] == TEXT("park")) { AJetwayRigs::Speed = 12.f; if (J->Undock(Ji)) ADP_LOG(TEXT("Views: jetway %d driving home"), Ji); else ADP_LOG(TEXT("Views: jetway %d not sent home: %s"), Ji, *AJetwayRigs::LastNote); }
			else if (V[1] == TEXT("dock")) { AJetwayRigs::Speed = 1.f; if (J->Dock(Ji)) ADP_LOG(TEXT("Views: jetway %d docking from rest, real speed"), Ji); else ADP_WARN(TEXT("Views: jetway %d does not dock: %s"), Ji, *AJetwayRigs::LastNote); }
			else if (V[1] == TEXT("cam") && V.Num() > 2)
			{
				const FVector Dir = (Dr - Ro).GetSafeNormal2D(), Perp(-Dir.Y, Dir.X, 0.f), Mj = (Ro + Dr) / 2.f; const double Sec = V.Num() > 3 ? FCString::Atof(*V[3]) : 0.0;
				const FString Lb = FString::Printf(TEXT("jetway %d %s"), Ji, *V[2]);
				if (V[2] == TEXT("wide")) { AimAt(W, PC, Mj + Perp * 3300.f + Dir * 1700.f + FVector(0, 0, 900), Mj + Dir * 1100.f - FVector(0, 0, 200), Sec, Lb); LockFov(PC, 55.f); }
				else if (V[2] == TEXT("side")) AimAt(W, PC, Mj + Perp * 1600.f + FVector(0, 0, 250), Mj, Sec, Lb);
				else if (V[2] == TEXT("cab")) { FVector E = Ro + Dir * 450.f; E.Z = Ro.Z + 60.f; AimAt(W, PC, E, FVector(Dr.X, Dr.Y, E.Z), 0.0, Lb); }
				else if (V[2] == TEXT("cabin")) { FVector Mid, Fw; if (J->CabView(Ji, Mid, Fw)) AimAt(W, PC, Mid - Fw.GetSafeNormal2D() * 150.f + FVector(0, 0, 40), Mid + Fw.GetSafeNormal2D() * 300.f, 0.0, Lb); else { ADP_WARN(TEXT("Views: jetway %d has no cab view"), Ji); bMissing = true; } }
				else if (V[2] == TEXT("gate")) { FVector D; if (Pf->GateDoor(FCString::Atoi(*V[0]), D)) AimAt(W, PC, D + FVector(0, 1000, 260), D + FVector(0, 0, 120), Sec, Lb); else { ADP_WARN(TEXT("Views: gate %s has no door"), *V[0]); bMissing = true; } }
				else ADP_WARN(TEXT("Views: jetway camera %s not known"), *V[2]);
			}
			JetLog(J, Ji);
		}
		else if (K == TEXT("board") && V.Num() > 1)
		{
			APaxFlow* Pf = APaxFlow::Get(W); FString Why;
			if (!Pf) ADP_WARN(TEXT("Views: board: no passenger flow here"));
			else if (!Pf->TestBoard(FCString::Atoi(*V[0]), FCString::Atoi(*V[1]), V.Num() > 2 ? FCString::Atof(*V[2]) : 3.f, Why)) ADP_WARN(TEXT("Views: board: nobody - %s"), *Why);
			else ADP_LOG(TEXT("Views: board: passengers at gate %s%s%s"), *V[0], Why.IsEmpty() ? TEXT("") : TEXT(" - "), *Why);
		}
		else if (K == TEXT("fire") && V.Num())
		{
			AFireTruck* T = nullptr; int32 Nt = 0; for (const TWeakObjectPtr<AFireTruck>& X : AFireTruck::All) if (X.IsValid()) { ++Nt; if (!T) T = X.Get(); }
			if (V[0] == TEXT("doors")) { const bool bOpen = A.Contains(TEXT("open")); AFireStationDoors::CommandAll(bOpen); ADP_LOG(TEXT("Views: fire station doors %s"), bOpen ? TEXT("opening") : TEXT("shutting")); }
			else if (!T) ADP_WARN(TEXT("Views: no fire truck here"));
			else if (V[0] == TEXT("lights")) { const bool bOn = A.Contains(TEXT("on")); for (const TWeakObjectPtr<AFireTruck>& X : AFireTruck::All) if (X.IsValid()) X->SetTestLights(bOn, bOn); ADP_LOG(TEXT("Views: %d fire trucks' lights %s"), Nt, bOn ? TEXT("on, blue flashing") : TEXT("off")); }
			else if (V[0] == TEXT("drive"))
			{
				if (APawn* Me = PC->GetPawn())
				{
					bHeld = bSub = bRel = false; FlyLen = 0.0; FreeFov(PC); const FVector D = T->DoorSpot(); const FRotator Face(0.f, (T->GetActorLocation() - D).Rotation().Yaw, 0.f);
					Me->SetActorEnableCollision(true); Me->TeleportTo(D, Face); PC->SetControlRotation(Face);
				}
				FireT = T; FireEnterAt = FPlatformTime::Seconds() + 1.0; ADP_LOG(TEXT("Views: fire drive: at truck %s's door (%d trucks; only this one is driven, the others light up where they stand)"), *T->GetName(), Nt);
			}
			else if (V[0] == TEXT("park")) { if (AFireTruck* In = Cast<AFireTruck>(PC->GetPawn())) { In->SetTestInput(0.f, 1.f, 0.f); FireExitAt = FPlatformTime::Seconds() + 4.0; ADP_LOG(TEXT("Views: fire park: braking")); } else ADP_WARN(TEXT("Views: fire park: not in a truck")); }
		}
		else if (K == TEXT("hangar") && V.Num() > 1)
		{
			AHangar* Best = nullptr; double Bd = 1e9; const double Hx = FCString::Atof(*V[0]) * 100.0;
			for (TActorIterator<AHangar> It(W); It; ++It) if (FMath::Abs(It->GetActorLocation().X - Hx) < Bd) { Bd = FMath::Abs(It->GetActorLocation().X - Hx); Best = *It; }
			if (!Best) { ADP_WARN(TEXT("Views: no hangar here")); return true; }
			AHangar::DoorSpeed = V.Num() > 2 ? FMath::Max(0.1f, (float)FCString::Atof(*V[2])) : 1.f;
			Best->Command(V[1] == TEXT("open"));
			ADP_LOG(TEXT("Views: hangar %s (%.0f m off) doors %s at %.0fx, now %.0f %% open"), *Best->GetName(), Bd / 100.0, *V[1], AHangar::DoorSpeed, Best->DoorOpen() * 100.f);
		}
		else if (K == TEXT("coach") && V.Num())
		{
			ACoaches* C = ACoaches::Get(W);
			if (!C || !C->Num()) { ADP_WARN(TEXT("Views: no coaches here")); return true; }
			const bool bTown = V[0] == TEXT("town"); int32 Sent = 0;
			if (bTown) { CoachHome.Reset(); for (int32 c = 0; c < C->Num(); ++c) CoachHome.Add(C->ModelOf(c) ? C->ModelOf(c)->GetActorLocation() : FVector::ZeroVector); }
			for (int32 c = 0; c < C->Num(); ++c) Sent += C->SendOnRoute(c, bTown) ? 1 : 0;   // (each one that can not go says why itself)
			ADP_LOG(TEXT("Views: %d of %d coaches sent %s"), Sent, C->Num(), bTown ? TEXT("to the town hub") : TEXT("back to their stands"));
		}
		else if (K == TEXT("coachwait") && V.Num() > 1)
		{
			CoachMode = V[1] == TEXT("near") ? 3 : V[1] == TEXT("hub") ? 1 : 2; CoachNear = V.Num() > 2 ? FCString::Atof(*V[2]) : 0.f;
			CoachEnd = FPlatformTime::Seconds() + FCString::Atof(*V[0]); ADP_LOG(TEXT("Views: waiting for the coaches (%s)"), *A);
		}
		else if (K == TEXT("quit")) { ADP_LOG(TEXT("Views: done")); FPlatformMisc::RequestExit(false); return false; }
		else if (!L.IsEmpty() && !L.StartsWith(TEXT("#"))) ADP_WARN(TEXT("Views: not understood: %s"), *L);
		return true;
	}
}

void ADPPerfViews::Begin(const FString& File)
{
	using namespace ADPPerfViewsInternal;
	if (!FFileHelper::LoadFileToStringArray(Steps, *File)) { ADP_ERROR(TEXT("Views: no file %s"), *File); return; }
	At = 0; Until = 0.0; Name.Reset();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
	ADP_LOG(TEXT("Views: %d steps from %s"), Steps.Num(), *File);
}
