// The hangar halls' own passenger stair (to-do 311, docs/Hired-Crew-Plan.md "Brake rider"). A copy of the motorised passenger
// stairs (the same model, the same table line, another instance), one per hangar hall, parked inside against the side wall
// on the aircraft's left, clear of the wingtip's and the tail's way and of the door track. Called while the tow brings the
// aircraft in: two hired ground crew walk into the hall while its door is open, board it, and once the aircraft stands there
// drive it round to its front left door and dock it with the passenger stairs' own code (StairsWork, stopped short, crept on).
// The brake rider climbs down as at a stand (MaintClimb) and waits in the hall by the wall; the stair backs off to its place,
// its crew walk out and the hall door shuts. For the tow back the same the other way round: the stair to the door, the rider
// up, the stair back to its place, then the tug comes in. Its jobs are ordinary service jobs whose StandX is the hall's key.
#include "GSE.h"
#include "GSEStairs.h"
#include "Hangar.h"
#include "Tugs.h"
#include "ServiceVehicles.h"
#include "Airfield.h"
#include "ADPLog.h"
#include "Sim/ADPSim.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	const FServiceSpec& HsSpec(int32 i) { return ADPService::All()[i]; }
	constexpr float HsKey0 = -100000.f, HsKeyStep = 100.f;   // (the halls' keys: far from any stand)
	constexpr float HsWall = 2.1f, HsAft = 10.f, HsWaitIn = 1.2f, HsWaitAft = 18.f;    // (m: its middle from the side wall, behind the nose tyre; the rider's wait)
}

const AGSE::FHallStair* AGSE::HallOf(float StandX) const
{
	if (StandX > HsKey0 + 1.f) return nullptr;
	for (const FHallStair& H : HallStairs) if (FMath::Abs(H.Key - StandX) < 1.f) return &H;
	return nullptr;
}

AGSE::FHallStair* AGSE::HallNamed(const FString& Hall)
{
	for (FHallStair& H : HallStairs) if (H.Hall == Hall) return &H;
	return nullptr;
}

bool AGSE::HallTi(int32 Ti) const { return HallStairs.ContainsByPredicate([Ti](const FHallStair& H) { return H.Ti == Ti; }); }

FVector AGSE::HallP(const FHallStair& H, float X, float Y) const { return Plan(H.Hx + X, H.Yn + Y); }

float AGSE::HallOpen(const FHallStair& H) const
{
	if (!H.Door.IsValid())
		for (TActorIterator<AHangar> It(GetWorld()); It; ++It) if (FMath::Abs(It->GetActorLocation().X - H.Hx * 100.f) < 200.f) H.Door = *It;
	const AHangar* Hg = Cast<AHangar>(H.Door.Get());
	return Hg ? Hg->DoorOpen() : 1.f;
}

void AGSE::HallStairsSetup()
{
	int32 Line = INDEX_NONE;
	for (int32 i = 0; i < ADPService::All().Num(); ++i) if (HsSpec(i).bStairs && !HsSpec(i).bRoof) { Line = i; break; }
	if (Line == INDEX_NONE) return;
	const TArray<FAirfieldData::FHangarSite>& All = AAirfield::Shared().Hangars;
	for (int32 h = 0; h < All.Num(); ++h)
	{
		const FAirfieldData::FHangarSite& Hs = All[h]; const FHangarSpec& Sp = FHangarSpec::Get(Hs.Size);
		FHallStair H; H.Hall = Hs.Id; H.Key = HsKey0 - HsKeyStep * h; H.Hx = Hs.X; H.Yn = Hs.Y + Sp.Lip() + Sp.TailIn(); H.Front = -Sp.TailIn();   // (the door line, from the nose tyre) H.Half = Sp.W * 0.5f;
		H.Side = 1.f;   // (towed in tail first, its nose to the door: its left is plan +x)
		H.Park = FVector2f(H.Hx + H.Side * (H.Half - HsWall), H.Yn + HsAft);
		FSpareAt At; At.Name = FString::Printf(TEXT("hangar %s stair"), *Hs.Id); At.X = H.Park.X; At.Y = H.Park.Y; At.Heading = -90.f;   // (facing the hall door)
		if (!SvcSpawn(Line, 0, &At)) { ADP_WARN(TEXT("GSE: the stair of hangar %s could not be placed"), *Hs.Id); continue; }
		FTractor& T = Tractors.Last(); T.bSpare = false; T.SpareId.Reset(); T.Owner.Reset();   // (the hall's own: never a bought one)
		H.Ti = Tractors.Num() - 1; HallStairs.Add(H);
		ADP_LOG(TEXT("GSE: %s parked inside at plan (%.1f, %.1f)"), *T.Name, H.Park.X, H.Park.Y);
	}
}

bool AGSE::HallBusy(const FString& Hall) const
{
	const FHallStair* H = HallStairs.FindByPredicate([&](const FHallStair& X) { return X.Hall == Hall; }); if (!H) return false;
	for (const TPair<int32, FSvcJob>& P : SvcJobs)
	{
		if (FMath::Abs(P.Value.StandX - H->Key) < 1.f && P.Value.Phase < 9) return true;
		const float* K = MaintHallX.Find(P.Key); if (K && FMath::Abs(*K - H->Key) < 1.f && P.Value.Stage < 8) return true;
	}
	for (const TPair<FString, FMechCrew>& M : MechCrews) if (M.Value.Hall == Hall && (M.Value.Phase == 1 || M.Value.Phase == 3)) return true;   // (its mechanics walking in or out)
	return false;
}

bool AGSE::HallCrowd(int32 Job) const
{
	// (anybody but its own crew within 7 m of it or of its place: it waits before it backs)
	const FSvcJob* J = SvcJobs.Find(Job); const ATugs* G = Tg(); if (!J || !G || !Tractors.IsValidIndex(J->Ti)) return false;
	const FHallStair* H = HallOf(J->StandX); if (!H) return false;
	const FVector A = Tractors[J->Ti].At, B = HallP(*H, H->Park.X - H->Hx, H->Park.Y - H->Yn);
	for (int32 c = 0; c < G->Crew.Num(); ++c)
	{
		if (J->Men.ContainsByPredicate([c](const FSvcMan& M) { return M.Ci == c && !M.bGone; })) continue;
		const AActor* P = G->Crew[c].A.Get(); if (!P || P->IsHidden()) continue;
		if (FVector::Dist2D(P->GetActorLocation(), A) < 700.f || FVector::Dist2D(P->GetActorLocation(), B) < 700.f) return true;
	}
	return false;
}

void AGSE::HallStart(int32 Job)
{
	// its crew walk in through the open hall door, board it, and once the aircraft stands in the hall drive it to the door
	FSvcJob* J = SvcJ(Job); const FHallStair* H = J ? HallOf(J->StandX) : nullptr; if (!J || !H || !J->Men.Num()) return;
	const int32 Ti = J->Ti; FTractor& T = Tractors[Ti];
	bWorked = true;
	T.Driver = J->Men[0].Ci; T.JobT = 0.f; T.StandX = J->StandX; T.Key = J->Flight.IsEmpty() ? J->Reg : J->Flight;
	ADP_LOG(TEXT("GSE: the %s for %s - %d people walk to hangar %s"), *T.Name, *T.Key, J->Men.Num(), *H->Hall);
	const float Sd = H->Side, Hf = H->Half; const FVector Gate = HallP(*H, Sd * (Hf - 8.f), H->Front - 8.f), In = HallP(*H, Sd * (Hf - 4.f), H->Front + 3.f);
	const float Key = H->Key;
	for (int32 m = 0; m < J->Men.Num(); ++m)
	{
		const int32 Ci = J->Men[m].Ci;
		auto Rest = [this, Job, m, Ci, Ti, Gate, In, Key]
		{
			WalkTo(Ci, [Gate] { return Gate; }, 60.f);
			WaitFor(Ci, [this, Key](float) { const FHallStair* X = HallOf(Key); return !X || HallOpen(*X) > 0.6f; });   // (never through a shut door)
			WalkTo(Ci, [In] { return In; }, 40.f);
			if (m > 0) { SvcBoard(Job, m); return; }
			ClimbIn(Ci, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) X->bSeated = true; });
			WaitFor(Ci, [this, Job, Ti, Key](float)
			{
				const FHallStair* X = HallOf(Key); const FSvcJob* Y = SvcJobs.Find(Job);
				if (Y && Y->bGo) return true;
				return X && X->Plane.IsValid() && SvcAll(Job, [](const FSvcMan& M) { return M.bSeated; }) && Tractors[Ti].SvcDoors[0].Now < 0.01f;
			});
			Do(Ci, [this, Job, Ti]
			{
				FSvcJob* X = SvcJ(Job); const FHallStair* Hs = X ? HallOf(X->StandX) : nullptr; if (!X || !Hs) return;
				FTractor& V = Tractors[Ti]; FVector2f At, Hd; float Tr = 0.f;
				if (X->bGo || !V.Ps || !StairsDock(X->StandX, 0, float(V.At.Z), At, Hd, Tr))
				{
					ADP_WARN(TEXT("GSE: the %s stays in its place - %s is not standing in the hall"), *V.Name, *X->Reg);
					V.Job = EJob::AtStand; X->Phase = 2; X->Stage = 6; StairsWork(Job); return;   // (gates and jacks as they are: home)
				}
				if ((At.X - Hs->Hx) * Hs->Side < 0.f) ADP_WARN(TEXT("GSE: %s's front left door is on the far side from the %s - it drives round the nose"), *X->Reg, *V.Name);
				// round from its place to square in front of the door (a curve of the plan: forward out of its place, turned to the door)
				const FVector2f P0(float(V.At.X) / 100.f, float(-V.At.Y) / 100.f), Pre = At - Hd * 4.f, Sh = At - Hd * 1.5f, C(P0.X, Pre.Y);
				TArray<FVector> W;
				for (int32 i = 1; i <= 40; ++i) { const float k = i / 40.f; const FVector2f Q = (1.f - k) * (1.f - k) * P0 + 2.f * k * (1.f - k) * C + k * k * Pre; W.Add(Plan(Q.X, Q.Y)); }
				for (int32 i = 1; i <= 10; ++i) { const FVector2f Q = FMath::Lerp(Pre, Sh, i / 10.f); W.Add(Plan(Q.X, Q.Y)); }
				X->InWay = W; V.Ps->Target = Tr; V.Job = EJob::Out; Motor(V, true);
				ADP_LOG(TEXT("GSE: the %s drives round to %s's front left door"), *V.Name, *X->Reg);
				Go(V, MoveTemp(W), false, [this, Job] { SvcWork(Job); });
			});
		};
		// (to-do 311: from the stands' side over the runway only in a maintenance van, never on foot)
		const FVector Drop = HallP(*H, Sd * (Hf + 6.f), H->Front - 20.f);
		ATugs* G = Tg(); const bool bOver = G && G->Crew.IsValidIndex(Ci) && G->Crew[Ci].A.IsValid() && Across(G->Crew[Ci].A->GetActorLocation(), Gate);
		if (bOver && VanRide(Ci, Drop, TEXT("hangar stair crew"), Rest)) continue;
		if (bOver) ADP_WARN(TEXT("GSE: there is no maintenance van - %s walks to hangar %s"), *G->Crew[Ci].Name, *H->Hall);
		HiredWay(Ci, Gate, 0.f);
		Rest();
	}
}

void AGSE::HallLeave(int32 Job)
{
	// gates shut, jacks up: straight back from the door, round backwards into its place, switched off; its crew walk out
	FSvcJob* J = SvcJ(Job); if (!J || J->Phase == 3 || !J->Men.Num()) return;
	const FHallStair* H = HallOf(J->StandX); if (!H) return;
	J->Phase = 3; const int32 Ti = J->Ti, Ci = J->Men[0].Ci;
	if (!J->Men[0].bSeated) ClimbIn(Ci, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) X->bSeated = true; });
	WaitFor(Ci, [this, Job, Ti](float) { return SvcAll(Job, [](const FSvcMan& X) { return X.bSeated || X.bGone; }) && Tractors[Ti].SvcDoors[0].Now < 0.01f; });
	WaitFor(Ci, [this, Job](float E)
	{
		const bool bBusy = HallCrowd(Job);
		if (bBusy && FMath::Fmod(E, 30.f) < 0.05f) { const FSvcJob* X = SvcJobs.Find(Job); ADP_LOG(TEXT("GSE: the %s waits - somebody stands in its way"), X && Tractors.IsValidIndex(X->Ti) ? *Tractors[X->Ti].Name : TEXT("hangar stair")); }
		return E > 120.f || !bBusy;
	});
	Do(Ci, [this, Job, Ti]
	{
		FSvcJob* X = SvcJ(Job); const FHallStair* Hs = X ? HallOf(X->StandX) : nullptr; if (!X || !Hs) return;
		FTractor& T = Tractors[Ti]; Motor(T, true); T.Job = EJob::Back;
		for (FTractor::FSvcDoor& D : T.SvcDoors) D.Want = 0.f;
		const FVector Fw = FRotator(0.f, T.Yaw, 0.f).Vector(); const FVector2f Hd = FVector2f(float(Fw.X), float(-Fw.Y)).GetSafeNormal();
		const FVector2f A(float(T.At.X) / 100.f, float(-T.At.Y) / 100.f), Pre = A - Hd * 2.5f, P = Hs->Park, C(P.X, Pre.Y);
		TArray<FVector> W;
		for (int32 i = 1; i <= 10; ++i) { const FVector2f Q = FMath::Lerp(A, Pre, i / 10.f); W.Add(Plan(Q.X, Q.Y)); }
		for (int32 i = 1; i <= 40; ++i) { const float k = i / 40.f; const FVector2f Q = (1.f - k) * (1.f - k) * Pre + 2.f * k * (1.f - k) * C + k * k * P; W.Add(Plan(Q.X, Q.Y)); }
		ADP_LOG(TEXT("GSE: the %s backs off %s's door to its place in the hall"), *T.Name, *X->Reg);
		Go(T, MoveTemp(W), true, [this, Job, Ti]
		{
			FTractor& V = Tractors[Ti]; V.Job = EJob::Home; Motor(V, false); ADP_LOG(TEXT("GSE: the %s parked in the hall, switched off"), *V.Name);
			FSvcJob* Y = SvcJ(Job); const FHallStair* Hz = Y ? HallOf(Y->StandX) : nullptr; if (!Y || !Hz) return;
			// (out of the hall the way they came: past the front of its place, out through the door)
			const FVector By = HallP(*Hz, Hz->Side * (Hz->Half - 2.f), HsAft - 6.f), Out = HallP(*Hz, Hz->Side * (Hz->Half - 8.f), Hz->Front - 8.f);
			// (to-do 311: back over the runway in a maintenance van, never on foot)
			auto Back = [this](int32 C) { Do(C, [this, C] { Later([this, C] { if (!VanRide(C, VanDropSouth(INDEX_NONE, 0.f), TEXT("hangar stair crew"), [this, C] { GiveBack(C); })) GiveBack(C); }); }); };
			for (int32 m = 0; m < Y->Men.Num(); ++m)
			{
				const int32 Cm = Y->Men[m].Ci; if (Y->Men[m].bGone) continue;
				if (m > 0)
				{
					SvcAlight(Job, m);
					WalkTo(Cm, [By] { return By; }, 50.f); WalkTo(Cm, [Out] { return Out; }, 60.f);
					Do(Cm, [this, Job, m] { if (FSvcMan* Z = SvcM(Job, m)) Z->bGone = true; }); Back(Cm); continue;
				}
				Wait(Cm, 2.f);
				ClimbOut(Cm, Ti, [this, Job] { if (FSvcMan* Z = SvcM(Job, 0)) { Z->bSeated = false; Z->bOut = true; } });
				WaitFor(Cm, [this, Job](float E) { return E > 120.f || SvcAll(Job, [](const FSvcMan& Z) { return Z.Seat == 0; }); });
				WalkTo(Cm, [By] { return By; }, 50.f); WalkTo(Cm, [Out] { return Out; }, 60.f);
				Do(Cm, [this, Job, Ti] { FTractor& Z = Tractors[Ti]; Z.Job = EJob::None; Z.Key.Reset(); Z.StandX = 0.f; Z.Driver = INDEX_NONE; if (FSvcJob* Q = SvcJ(Job)) Q->Phase = 9; });
				Back(Cm);
			}
		});
	});
}

// ------------------------------------------------------------------ the brake rider in the hall

void AGSE::MaintHall(const FString& Reg, const FString& Hall, AActor* Plane, bool bIn)
{
	FHallStair* H = HallNamed(Hall); if (!H) return;
	TPair<int32, FSvcJob>* P = nullptr;
	for (TPair<int32, FSvcJob>& X : SvcJobs) if (X.Value.Reg == Reg && HsSpec(X.Value.Svc).bMaint && X.Value.Phase == 2) P = &X;
	if (!P || !P->Value.Men.Num() || P->Value.Men[0].bGone) return;
	const FSvcJob& J = P->Value;
	if (!bIn)
	{
		// (the stair called while the door is open: its crew are in before it shuts)
		if (J.Men[0].bIn && J.Stage == 5 && !StairsHas(H->Key))
		{
			const UADPSim* Sim = UADPSim::Get(GetWorld()); const FDateTime Now = Sim ? Sim->Now() : FDateTime::UtcNow();
			if (SvcAsk(TEXT("stairs"), H->Key, Reg, TEXT("TOW/") + Reg, Now + FTimespan::FromMinutes(20.0), 0.f, Now, false) || StairsHas(H->Key))
				ADP_LOG(TEXT("GSE: %s's tow calls the stair of hangar %s"), *Reg, *Hall);
		}
		return;
	}
	if (Plane) H->Plane = Plane;
	MechStart(Reg, *H);   // (the check's other mechanics, GSEMechanics.cpp)
	if (J.Men[0].bIn && J.Stage == 5 && !MaintHallX.Contains(P->Key))
	{
		MaintHallX.Add(P->Key, H->Key);
		ADP_LOG(TEXT("GSE: %s in hangar %s - the brake rider gets off by the hall's stair"), *Reg, *Hall);
	}
}

int32 AGSE::MaintBack(const FString& Reg, const FString& Hall, AActor* Plane, FString& Why)
{
	FHallStair* H = HallNamed(Hall);
	TPair<int32, FSvcJob>* P = nullptr;
	for (TPair<int32, FSvcJob>& X : SvcJobs) if (X.Value.Reg == Reg && HsSpec(X.Value.Svc).bMaint && X.Value.Phase == 2) P = &X;
	if (!P || !H || !P->Value.Men.Num() || P->Value.Men[0].bGone) { Why = TEXT("no brake rider in the hall"); return 3; }
	const int32 Job = P->Key; FSvcJob& J = P->Value;
	if (Plane) H->Plane = Plane;
	if (J.Stage == 8 && MechHolds(Reg, Why)) return 1;   // (the check's mechanics out of the hall before the tow comes in)
	if (J.Stage == 8)
	{
		J.Stage = 10; const float Key = H->Key;
		ADP_LOG(TEXT("GSE: %s's tow back - the brake rider calls the stair of hangar %s"), *Reg, *Hall);
		Later([this, Job, Key] { MaintStairs(Job, Key, true); });
	}
	if (J.Stage == 10) { Why = TEXT("its brake rider climbs aboard by the hall's stair"); return 1; }
	if (J.Stage == 11)
	{
		if (StairsHas(H->Key)) { SvcGo(TEXT("stairs"), H->Key); Why = TEXT("the hall's stair backs off its door"); return 2; }
		FString Sw; if (StairsBusy(H->Key, true, Sw)) { Why = TEXT("the hall's stair backs off its door"); return 2; }
		J.Stage = 5; ADP_LOG(TEXT("GSE: %s - its door clear, the brake rider ready for the tow back"), *Reg);
		return 3;
	}
	if (J.Men[0].bIn && !MaintHallX.Contains(Job)) return 3;   // (he never got off)
	Why = TEXT("its brake rider is on his way down"); return 1;
}
