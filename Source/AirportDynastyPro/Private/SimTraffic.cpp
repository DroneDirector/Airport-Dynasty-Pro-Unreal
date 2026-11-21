// The simulation's flights at our airport in 3D - see SimTraffic.h.
#include "SimTraffic.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "TerminalPlan.h"
#include "WindSocks.h"
#include "Site.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPBoards.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"

namespace
{
	constexpr float KtMs = 0.5144f;
	constexpr float Wheelbase = 12.64f;        // A320: nose gear to main gear
	constexpr float MaxTaxi = 25.f * KtMs, TurnTaxi = 10.f * KtMs, StandTaxi = 5.f * KtMs, PushSpeed = 3.f * KtMs;
	constexpr float Vr = 140.f * KtMs, Vapp = 135.f * KtMs, Glide = 3.f;
	float Heading(const FVector2f& From, const FVector2f& To) { return FMath::Atan2(To.Y - From.Y, To.X - From.X); }
}

ASimTraffic::ASimTraffic()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ASimTraffic::Start(UWorld* W, const FAirfieldData& Field)
{
	if (!W || !FPackageName::DoesPackageExist(TEXT("/Game/Babylon/A320/a320/StaticMeshes/A320"))) return;
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ASimTraffic* T = W->SpawnActor<ASimTraffic>(FVector::ZeroVector, FRotator::ZeroRotator, P)) T->F = Field;
}

bool ASimTraffic::RunwayToX1() const
{
	// the runway's x0 -> x1 direction has the plan's x axis, true heading = the site's runway heading; into the wind
	double Hdg = 336.0; if (TSharedPtr<FJsonObject> A = ADPSite::AirportBlock()) A->TryGetNumberField(TEXT("hdg"), Hdg);
	const float Head = FMath::Cos(FMath::DegreesToRadians(AWindSocks::WindFrom - float(Hdg)));
	return Head >= -0.05f;   // (calm or a headwind on x0 -> x1: that way)
}

void ASimTraffic::Sync()
{
	UADPSim* Sim = UADPSim::Get(this);
	if (!Sim || !Sim->IsRunning()) return;
	const FString Here = ADPBoards::HereIcao();
	const FDateTime Now = Sim->Now();
	const bool bToX1 = RunwayToX1();
	const float RY = F.RwyY, X0 = F.RwyX0, X1 = F.RwyX1;
	const float Hold = 107.5f;   // (the holding point's distance from the runway's centre line)
	for (const FADPFlight& Fl : Sim->World.Flights)
	{
		if (Seen.Contains(Fl.Id)) continue;
		const FADPAircraft* A = Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Fl.Reg; });
		// departures from here: from off-blocks, while it can still be on the ground or climbing (12 minutes)
		if (Fl.From == Here && (Now - Fl.DepAt).GetTotalMinutes() >= 0.0 && (Now - Fl.DepAt).GetTotalMinutes() < 12.0)
		{
			Seen.Add(Fl.Id);
			const FAirfieldData::FStand* St = F.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return !S.bCargo && S.Span <= 40.f; });
			// (it was parked at a stand: the one ParkedAircraft showed it at - the simulation's own record)
			for (const FAirfieldData::FStand& S : F.Stands) if (S.Id == Fl.FromStand || (Fl.FromStand.IsEmpty() && A && S.Id == A->Stand)) St = &S;
			if (!St) continue;
			FMove M; M.FlightId = Fl.Id; M.Reg = Fl.Reg; M.bDeparture = true; M.Stand = St->Id; M.Phase = EPhase::Push;
			// at the stand: nose tyre on the stop bar, the body behind it, pushed straight back onto the taxilane
			M.Nose = FVector2f(St->X, St->Nose - St->Dir * 5.1f); M.Main = M.Nose - FVector2f(0.f, St->Dir * Wheelbase);
			M.Path = {FVector2f(St->X, F.TY + St->Dir * Wheelbase)};
			Moves.Add(M);
		}
		// arrivals here: from about 9 minutes before on-blocks (the approach, the landing, the taxi in)
		const FDateTime OnBlocks = Fl.DepAt + FTimespan::FromMinutes(Fl.DurMin + Fl.HoldMin);
		if (Fl.To == Here && Fl.Diverted.IsEmpty() && (OnBlocks - Now).GetTotalMinutes() <= 9.0 && (OnBlocks - Now).GetTotalMinutes() > 0.0)
		{
			Seen.Add(Fl.Id);
			FMove M; M.FlightId = Fl.Id; M.Reg = Fl.Reg; M.bDeparture = false; M.Phase = EPhase::Approach;
			M.Stand = A ? A->Stand : FString();
			// a straight-in 3 degree approach to the touchdown zone (500 m past the threshold), 7 km out
			const float Dir = bToX1 ? 1.f : -1.f, Thr = bToX1 ? X0 : X1, TD = Thr + Dir * 500.f, From = 7000.f;
			M.Nose = FVector2f(TD - Dir * From, RY); M.Main = M.Nose - FVector2f(Dir * Wheelbase, 0.f);
			M.Height = From * FMath::Tan(FMath::DegreesToRadians(Glide)); M.Speed = Vapp;
			M.Path = {FVector2f(TD, RY)};
			Moves.Add(M);
		}
	}
	// models for new movements
	for (FMove& M : Moves)
		if (!M.Model.IsValid())
		{
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ABabylonModel* B = GetWorld()->SpawnActor<ABabylonModel>(FVector::ZeroVector, FRotator::ZeroRotator, P);
			if (B && B->Place(TEXT("A320"))) { B->Tags.Add(FName(*(TEXT("MOVE/") + M.Reg))); M.Model = B; Pose(M); ADP_LOG(TEXT("Traffic: %s %s in 3D (%s)"), *M.Reg, M.bDeparture ? TEXT("departing") : TEXT("arriving"), *M.Stand); }
			else if (B) B->Destroy();
		}
}

void ASimTraffic::Pose(FMove& M)
{
	ABabylonModel* B = M.Model.Get();
	if (!B) return;
	// the actor sits on the nose tyre (as on the stands: ParkedAircraft.cpp), the nose pointing from the main gear to it
	const FVector2f D = (M.Nose - M.Main).GetSafeNormal();
	const FVector World = FTerminalPlan::ToWorld(M.Nose.X, AAirfield::GroundAt(M.Nose.X, M.Nose.Y) + M.Height, M.Nose.Y);
	const FVector Dir(D.X, -D.Y, 0.f);   // (plan (x, y) -> world (x, -y))
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Dir.X, Dir.Y));
	// (level for now: the nose-up attitude in the climb and the flare comes with the model's own axes checked - 191 next)
	B->SetActorLocationAndRotation(World, FRotator(0.f, Yaw, 0.f));
}

bool ASimTraffic::Advance(FMove& M, float Dt)
{
	const bool bToX1 = RunwayToX1();
	const float Dir = bToX1 ? 1.f : -1.f, RY = F.RwyY, X0 = F.RwyX0, X1 = F.RwyX1, Hold = 107.5f;
	const float LineUp = bToX1 ? X0 + 20.f : X1 - 20.f;
	// the target speed: by the phase, the turn coming (the heading change over the next 60 m) and the stand
	float Want = MaxTaxi;
	if (M.Phase == EPhase::Push) Want = PushSpeed;
	else if (M.Phase == EPhase::Roll) Want = Vr * 1.1f;
	else if (M.Phase == EPhase::Climb) Want = 160.f * KtMs;
	else if (M.Phase == EPhase::Approach) Want = Vapp;
	else if (M.Phase == EPhase::Rollout) Want = 20.f * KtMs;
	else if (M.Step < M.Path.Num())
	{
		const float H0 = Heading(M.Main, M.Nose);
		float Look = 0.f; FVector2f P = M.Nose; float Turn = 0.f;
		for (int32 i = M.Step; i < M.Path.Num() && Look < 60.f; ++i)
		{
			Look += FVector2f::Distance(P, M.Path[i]);
			Turn = FMath::Max(Turn, FMath::Abs(FMath::FindDeltaAngleRadians(H0, Heading(P, M.Path[i]))));
			P = M.Path[i];
		}
		if (Turn > FMath::DegreesToRadians(20.f)) Want = TurnTaxi;
		if (M.Phase == EPhase::TaxiIn && M.Step == M.Path.Num() - 1 && FVector2f::Distance(M.Nose, M.Path.Last()) < 60.f) Want = StandTaxi;
	}
	const float Acc = M.Phase == EPhase::Roll ? 2.2f : M.Phase == EPhase::Rollout ? -2.f : 0.6f;
	if (M.Phase == EPhase::Rollout) M.Speed = FMath::Max(Want, M.Speed + Acc * Dt);
	else M.Speed = FMath::FInterpConstantTo(M.Speed, Want, Dt, FMath::Abs(Acc) + 0.4f);
	// the nose wheel along the waypoints; the main gear trails it at the wheelbase (backwards when pushed)
	float Left = M.Speed * Dt;
	while (Left > 0.f && M.Step < M.Path.Num())
	{
		const FVector2f To = M.Path[M.Step] - (M.Phase == EPhase::Push ? M.Main : M.Nose);
		const float D = To.Size();
		const FVector2f U = D > 1e-3f ? To / D : FVector2f::ZeroVector;
		const float Go = FMath::Min(D, Left);
		if (M.Phase == EPhase::Push)
		{
			// pushed: the main gear leads backwards along the line, the nose follows it at the wheelbase
			M.Main += U * Go; M.Nose = M.Main + (M.Nose - M.Main).GetSafeNormal() * Wheelbase;
		}
		else { M.Nose += U * Go; M.Main = M.Nose - (M.Nose - M.Main).GetSafeNormal() * Wheelbase; }
		Left -= Go;
		if (D <= Go + 1e-3f) ++M.Step;
	}
	if (M.Phase == EPhase::Climb || M.Phase == EPhase::Approach)
	{
		// the climb (about 8 degrees) and the 3 degree approach down to the touchdown zone
		if (M.Phase == EPhase::Climb) M.Height += M.Speed * Dt * FMath::Tan(FMath::DegreesToRadians(8.f));
		else M.Height = FMath::Max(0.f, FMath::Abs(M.Nose.X - M.Path.Last().X) * FMath::Tan(FMath::DegreesToRadians(Glide)));
	}
	Pose(M);
	if (M.Step < M.Path.Num()) return false;
	// the next phase: its waypoints
	M.Step = 0; M.Path.Reset();
	switch (M.Phase)
	{
	case EPhase::Push:
		// the tug lets go; taxi along T to the connector, up to A, along A to the holding point of the runway end in use
		M.Phase = EPhase::Taxi;
		M.Path = {FVector2f(M.Nose.X + Dir * 40.f, F.TY), FVector2f(bToX1 ? -130.f : 362.f, F.TY), FVector2f(bToX1 ? -130.f : 362.f, F.AY),
			FVector2f(LineUp - Dir * 40.f, F.AY), FVector2f(LineUp, RY - Hold)};
		return false;
	case EPhase::Taxi:
		// lined up on the centre line, then the roll
		M.Phase = EPhase::Roll; M.Path = {FVector2f(LineUp, RY), FVector2f(LineUp + Dir * 1700.f, RY)};
		return false;
	case EPhase::Roll:
		M.Phase = EPhase::Climb; M.Path = {FVector2f(LineUp + Dir * 9000.f, RY)};
		return false;
	case EPhase::Climb:
		return true;
	case EPhase::Approach:
		// touched down: the roll-out to the exit
		M.Phase = EPhase::Rollout; M.Height = 0.f; M.Path = {FVector2f(81.f, RY)};
		return false;
	case EPhase::Rollout:
	{
		// off at the exit to A, along A, down to T, along T and nose in to the stand (its stop bar)
		M.Phase = EPhase::TaxiIn;
		const FAirfieldData::FStand* St = F.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == M.Stand; });
		if (!St) St = F.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return !S.bCargo && S.Span <= 40.f; });
		if (!St) return true;
		M.Path = {FVector2f(81.f, F.AY), FVector2f(362.f, F.AY), FVector2f(362.f, F.TY), FVector2f(St->X, F.TY), FVector2f(St->X, St->Nose - St->Dir * 5.1f)};
		return false;
	}
	default: return true;
	}
}

void ASimTraffic::Tick(float Dt)
{
	Super::Tick(Dt);
	if ((Poll += Dt) >= 2.f) { Poll = 0.f; Sync(); }
	for (int32 i = Moves.Num() - 1; i >= 0; --i)
	{
		FMove& M = Moves[i];
		if (!M.Model.IsValid()) continue;
		if (Advance(M, FMath::Min(Dt, 0.1f)))
		{
			// an arrival waits at its stand until the parked aircraft (the simulation's, ParkedAircraft.cpp) takes over
			if (!M.bDeparture && M.Clock < 600.f)
			{
				bool bParked = false;
				for (TActorIterator<ABabylonModel> It(GetWorld()); It && !bParked; ++It) bParked = It->Tags.Contains(FName(*(TEXT("SIM/") + M.Reg)));
				if (!bParked) { M.Clock += Dt; M.Phase = EPhase::Done; continue; }
			}
			if (ABabylonModel* B = M.Model.Get()) B->Destroy();
			Moves.RemoveAt(i);
		}
	}
}
