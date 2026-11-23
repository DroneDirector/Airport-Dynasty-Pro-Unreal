// The aircraft parked at the terminal's gates (ParkedAircraft.h).
#include "ParkedAircraft.h"
#include "Components/MaterialBillboardComponent.h"
#include "ADPKeep.h"
#include "ADPLog.h"
#include "Airfield.h"
#include "BabylonModel.h"
#include "TerminalPlan.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Airfield.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPBoards.h"
#include "EngineUtils.h"

namespace ADPParked
{
	bool HaveModel()
	{
		return FPackageName::DoesPackageExist(TEXT("/Game/Babylon/A320/a320/StaticMeshes/A320"));
	}

	// one A320 on a stand, its nose tyre on the stop bar; nullptr when the model is missing
	static ABabylonModel* ParkOne(UWorld* W, const FAirfieldData::FStand& St)
	{
		FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		{
			// its nose tyre on the stop bar, on the lead-in line; the nose toward the terminal (the stand's Dir)
			const float B = -St.Dir;                             // (from the nose back towards the tail: Airfield.cpp)
			const float Gx = St.X, Gy = St.Nose + B * NoseGearBehindNose;
			// the apron's surface under it: a look straight down at the nose tyre and between the main tyres (12.8 m
			// behind), the higher of the two (its tyres stand on it, never in it)
			auto Surface = [&](float X, float Y)
			{
				FHitResult Hit;
				const FVector From = FTerminalPlan::ToWorld(X, 30.f, Y), To = FTerminalPlan::ToWorld(X, -30.f, Y);
				FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPParked), true);
				return W->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q) ? Hit.ImpactPoint.Z / 100.0 : 0.0;
			};
			double Z = FMath::Max(Surface(Gx, Gy), Surface(Gx, Gy + B * 12.8f));
			if (FMath::Abs(Z) > 0.5) Z = 0.02;   // (something else met - a jetway: the apron is at about 0 there)
			// turned so the model's +y (its nose) runs along the stand toward the terminal: the loader puts a Blender
			// model's +y on the actor's right (BabylonModel.h); plan y is the world's -y
			const FVector Nose(0.0, -St.Dir, 0.0);             // (world: plan (0, Dir))
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Nose.X, Nose.Y));
			ABabylonModel* M = W->SpawnActor<ABabylonModel>(FTerminalPlan::ToWorld(Gx, 0.f, Gy) + FVector(0, 0, Z * 100.0), FRotator(0, Yaw, 0), S);
			if (!M || !M->Place(TEXT("A320"))) { if (M) M->Destroy(); ADP_ERROR(TEXT("Parked aircraft: the A320 is missing (Tools/import_model.py)")); return nullptr; }
			M->Tags.Add(FName(*(St.Id + TEXT("/A320"))));
			// heat haze behind the APU's exhaust at the tip of the tail cone (to-do 22b: the APU runs while it stands at the
			// gate): a shimmer of hot air, rising (M_HeatHaze). The model's nose is the actor's +Y; the A320 is 37.6 m long,
			// its nose tyre 5.1 m behind the nose, so the exhaust is about 32.5 m behind the nose tyre, 5 m up
			static UMaterialInterface* Haze = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Effects/M_HeatHaze.M_HeatHaze")));
			if (Haze)
			{
				UMaterialBillboardComponent* Hz = NewObject<UMaterialBillboardComponent>(M);
				Hz->SetupAttachment(M->GetRootComponent());
				Hz->SetRelativeLocation(FVector(0.f, 3520.f, 500.f));   // (the loader turns the model half round on the actor: its tail is the actor's +Y)
				Hz->AddElement(Haze, nullptr, false, 210.f, 170.f, nullptr);
				Hz->SetCastShadow(false);
				Hz->RegisterComponent();
			}
			return M;
		}
		return nullptr;
	}

	int32 Place(UWorld* W, const FAirfieldData& Field)
	{
		if (!W || !HaveModel()) return 0;
		int32 N = 0;
		for (const FAirfieldData::FStand& St : Field.Stands)
		{
			// the terminal's narrow-body stands (code C: 36 m span), not the wide-body stand or the cargo stands
			if (St.bCargo || St.Span > 40.f) continue;
			if (!ParkOne(W, St)) return N;
			++N;
		}
		ADP_LOG(TEXT("Parked aircraft: %d A320s at the terminal's stands, nose tyres on their stop bars"), N);
		return N;
	}

	// ---- the traffic stress test (to-do 74, owner 2026-11-17) ----------------------------------------------------------
	// -TrafficTest=N: every stand filled (the narrow-body ones already are, then the wide-body, cargo and hangar stands),
	// and what's left of N slides along the taxiways' centre lines, the runway and a straight 3 degree approach, round and
	// round. Test movement only: the same static A320 model, no flight logic, no animation; the normal game never does it.
	int32 TrafficTestCount()
	{
		int32 N = 0;
		return FParse::Value(FCommandLine::Get(), TEXT("TrafficTest="), N) ? FMath::Clamp(N, 0, 200) : 0;
	}

	namespace
	{
		struct FMover { TWeakObjectPtr<ABabylonModel> M; TArray<FVector> Path; float Speed = 10.f, At = 0.f, Len = 0.f; };
		TArray<FMover> GMovers;
		FTSTicker::FDelegateHandle GTick;

		// a plan point (m) on the ground, or Up metres above it, in the world
		FVector OnGround(float X, float Y, float Up = 0.f) { return FTerminalPlan::ToWorld(X, AAirfield::GroundAt(X, Y) + Up, Y); }

		void Pose(FMover& Mv)
		{
			ABabylonModel* M = Mv.M.Get();
			if (!M || Mv.Path.Num() < 2) return;
			float D = Mv.At;
			for (int32 i = 1; i < Mv.Path.Num(); ++i)
			{
				const FVector A = Mv.Path[i - 1], B = Mv.Path[i];
				const float L = float(FVector::Dist(A, B)) / 100.f;
				if (D > L && i < Mv.Path.Num() - 1) { D -= L; continue; }
				const FVector Dir = (B - A).GetSafeNormal();
				// the model's nose is the actor's -Y here as on the stands (the loader turns it half round: ParkOne)
				// (turned only round the vertical: the approach's 3 degrees don't matter for a cost test)
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Dir.X, Dir.Y));
				M->SetActorLocationAndRotation(A + (B - A) * FMath::Clamp(D / FMath::Max(L, 0.01f), 0.f, 1.f), FRotator(0.f, Yaw, 0.f));
				return;
			}
		}
	}

	int32 TrafficTest(UWorld* W, const FAirfieldData& Field, int32 N)
	{
		if (!W || N <= 0 || !HaveModel()) return 0;
		int32 Used = 0;
		// the narrow-body terminal stands are filled already (Place); the others now
		for (const FAirfieldData::FStand& St : Field.Stands)
		{
			if (Used >= N) break;
			if (!St.bCargo && St.Span <= 40.f) { ++Used; continue; }
			if (ParkOne(W, St)) ++Used;
		}
		for (const FAirfieldData::FHangarSite& H : Field.Hangars)
		{
			if (Used >= N) break;
			FAirfieldData::FStand St; St.Id = TEXT("TT-") + H.Id; St.X = H.X; St.Nose = H.Y + 40.f; St.Dir = -1.f;
			if (ParkOne(W, St)) ++Used;
		}
		// the paths: each taxiway's centre line, the runway (08: west to east), and the approach to it (10 km out, 3 degrees)
		struct FPath { TArray<FVector> P; float Speed = 10.f; };
		TArray<FPath> Paths;
		for (const FAirfieldData::FTwy& T : Field.Taxiways)
		{
			if (T.Pts.Num() < 2) continue;
			TArray<FVector> P; for (const FVector2f& Q : T.Pts) P.Add(OnGround(Q.X, Q.Y, 0.02f));
			FPath& Tw = Paths.AddDefaulted_GetRef(); Tw.P = MoveTemp(P); Tw.Speed = 10.f;
		}
		{ FPath& Rw = Paths.AddDefaulted_GetRef(); Rw.P.Add(OnGround(Field.RwyX0, Field.RwyY)); Rw.P.Add(OnGround(Field.RwyX1, Field.RwyY)); Rw.Speed = 60.f; }
		const float Far = 10000.f;
		{ FPath& Ap = Paths.AddDefaulted_GetRef(); Ap.P.Add(OnGround(Field.RwyX0 - Far, Field.RwyY, Far * FMath::Tan(FMath::DegreesToRadians(3.f)))); Ap.P.Add(OnGround(Field.RwyX0 + 300.f, Field.RwyY)); Ap.Speed = 70.f; }
		int32 Moving = 0;
		for (int32 k = 0; Used < N && Paths.Num(); ++k, ++Used)
		{
			const FPath& Pt = Paths[k % Paths.Num()];
			FMover Mv; Mv.Path = Pt.P; Mv.Speed = Pt.Speed;
			for (int32 i = 1; i < Mv.Path.Num(); ++i) Mv.Len += float(FVector::Dist(Mv.Path[i - 1], Mv.Path[i])) / 100.f;
			if (Mv.Len < 1.f) continue;
			// spread along the path when several share it
			Mv.At = FMath::Fmod(float(k / Paths.Num()) * 400.f, Mv.Len);
			FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ABabylonModel* M = W->SpawnActor<ABabylonModel>(Mv.Path[0], FRotator::ZeroRotator, S);
			if (!M || !M->Place(TEXT("A320"))) { if (M) M->Destroy(); break; }
			M->Tags.Add(TEXT("TrafficTest"));
			Mv.M = M; Pose(Mv);
			GMovers.Add(MoveTemp(Mv));
			++Moving;
		}
		if (Moving && !GTick.IsValid())
			GTick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float Dt)
			{
				Dt = FMath::Min(Dt, 0.1f);
				int32 Alive = 0;
				for (FMover& Mv : GMovers)
				{
					if (!Mv.M.IsValid()) continue;
					++Alive;
					Mv.At = FMath::Fmod(Mv.At + Mv.Speed * Dt, Mv.Len);
					Pose(Mv);
				}
				if (!Alive) { GMovers.Reset(); GTick.Reset(); return false; }
				return true;
			}));
		ADP_LOG(TEXT("Traffic test: %d aircraft (%d on stands, %d moving)"), Used, Used - Moving, Moving);
		return Used;
	}
}

namespace ADPParked
{
	namespace
	{
		TMap<FString, TPair<FString, TWeakObjectPtr<ABabylonModel>>> GLive;   // stand -> (registration, its model)
		FTSTicker::FDelegateHandle GLiveTick;
		bool GStaticGone = false;
	}
	TSet<FString> Coming;

	void Release(const FString& Reg)
	{
		for (auto It = GLive.CreateIterator(); It; ++It)
			if (It->Value.Key == Reg) { if (ABabylonModel* M = It->Value.Value.Get()) M->Destroy(); It.RemoveCurrent(); }
	}

	void StartLive(UWorld* W, const FAirfieldData& Field)
	{
		if (!W || !HaveModel()) return;
		TWeakObjectPtr<UWorld> WW(W);
		TArray<FAirfieldData::FStand> Stands = Field.Stands;
		if (GLiveTick.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(GLiveTick);
		GLive.Reset(); GStaticGone = false;
		GLiveTick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WW, Stands](float) -> bool
		{
			UWorld* Wd = WW.Get();
			if (!Wd) return false;
			UADPSim* Sim = UADPSim::Get(Wd);
			if (!Sim || !Sim->IsRunning()) return true;
			// a career runs: the static aircraft go once (the simulation's own take their places)
			if (!GStaticGone)
			{
				GStaticGone = true;
				for (TActorIterator<ABabylonModel> It(Wd); It; ++It)
					for (const FName& T : It->Tags) if (T.ToString().EndsWith(TEXT("/A320"))) { It->Destroy(); break; }
			}
			const FString Here = ADPBoards::HereIcao();
			TMap<FString, FString> Want;   // stand -> registration
			for (const FADPAircraft& A : Sim->World.Aircraft)
				if (A.Location == Here && !A.Stand.IsEmpty() && A.Status != TEXT("flying")) Want.Add(A.Stand, A.Reg);
			// gone or changed: removed
			for (auto It = GLive.CreateIterator(); It; ++It)
			{
				const FString* R = Want.Find(It->Key);
				// (just off blocks: it stands here until its moving model takes its place - SimTraffic releases it)
				if (!R && Sim->World.Flights.ContainsByPredicate([&](const FADPFlight& X) { return X.Reg == It->Value.Key && X.From == Here && (Sim->Now() - X.DepAt).GetTotalMinutes() < 2.0; })) continue;
				if (!R || *R != It->Value.Key) { if (ABabylonModel* M = It->Value.Value.Get()) M->Destroy(); It.RemoveCurrent(); }
			}
			// new ones: on their stands, nose tyres on the stop bars
			for (const auto& Wn : Want)
			{
				if (GLive.Contains(Wn.Key) || Coming.Contains(Wn.Value)) continue;
				const FAirfieldData::FStand* St = Stands.FindByPredicate([&](const FAirfieldData::FStand& X) { return X.Id == Wn.Key; });
				if (!St) continue;
				if (ABabylonModel* M = ParkOne(Wd, *St)) { M->Tags.Add(FName(*(TEXT("SIM/") + Wn.Value))); GLive.Add(Wn.Key, TPair<FString, TWeakObjectPtr<ABabylonModel>>(Wn.Value, M)); ADP_LOG(TEXT("Parked aircraft: %s at %s (the simulation's)"), *Wn.Value, *Wn.Key); }
			}
			return true;
		}), 5.f);
	}
}
