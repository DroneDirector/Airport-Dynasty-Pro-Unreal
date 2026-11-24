// You work the cabin cleaning van (to-do 238): its sliding and rear doors by their handles, its kit (bag roll, caddy, vacuum, full
// bags) out of its places, up to the aircraft's door and back, by its crew's own movements and rules (GSEService.cpp). The cab is
// the shared one (CabControls.cpp). Inside the aircraft only its cleaners work: there is no cabin yet (to-do 209).
#include "GSE.h"
#include "GSEClean.h"
#include "ServiceVehicles.h"
#include "VehicleControls.h"
#include "JetwayRigs.h"
#include "StaffPeople.h"
#include "Tugs.h"
#include "CrewPose.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	enum : int32 { ClItem = 10, ClDoor = 30, ClCart = 31, ClAct = 20 };
	const FVector ClRear(-375., 0., 0.);
	const TCHAR* ClName[] = {TEXT("bag roll"), TEXT("caddy"), TEXT("vacuum cleaner"), TEXT("bag roll"), TEXT("full bag"), TEXT("full bag")};
	const float ClFoot[] = {0.5f, 28.f, -1.f, 0.5f, 55.5f, 55.5f};   // (its grip above the ground it stands on; the vacuum does not stand by itself)
	bool ClBag(int32 i) { return i >= 4; }
	bool ClDeep(int32 i) { return i == 2 || i >= 4; }
	const FServiceSpec& ClSpec(const AGSE::FTractor& T) { return ADPService::All()[T.Svc]; }
	FTransform ClDoorX(const AGSE::FTractor& T, int32 d)
	{
		// (the same as SvcParts)
		const AGSE::FTractor::FSvcDoor& D = T.SvcDoors[d]; const FServiceDoor& K = ClSpec(T).Doors[d]; FTransform X(D.Piv);
		if (K.bSlide) { const float Sg = D.Piv.Y >= 0.f ? 1.f : -1.f, A = FMath::Clamp(D.Now / 0.15f, 0.f, 1.f), B = FMath::Clamp((D.Now - 0.15f) / 0.85f, 0.f, 1.f); X.SetLocation(D.Piv + FVector(-4.f * A - 126.f * B, 12.5f * A * Sg, 0.f)); }
		else X.SetRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(K.Deg * FMath::SmoothStep(0.f, 1.f, D.Now))));
		return FTransform(-D.Piv) * X;
	}
}

void AGSE::CleanDress(FTractor& T)
{
	T.Cl = MakeShared<FSvcClean>(); T.Cl->Things.SetNum(T.SvcItems.Num());
	// (what you climb in for: worked from the rear sill; there is no handle of its own in the model)
	T.Pt.Add(TEXT("CLN_In_2"), FVector(-301., 34., 66.)); T.Pt.Add(TEXT("CLN_In_4"), FVector(-301., 8., 66.)); T.Pt.Add(TEXT("CLN_In_5"), FVector(-301., -14., 66.));
}

bool AGSE::CleanYours(const FTractor& T, int32 It) const { return T.Cl && T.Cl->Things.IsValidIndex(It) && T.Cl->Things[It].State != FSvcClean::Van; }

bool AGSE::CleanYouIn(const FTractor& T) const { return T.Cl && GetWorld()->GetTimeSeconds() - T.Cl->InAt < 0.5; }

bool AGSE::CleanStowed(const FTractor& T) const
{
	if (!T.Cl) return true;
	for (int32 d = 0; d < ClSpec(T).Doors.Num(); ++d) if (T.SvcDoors.IsValidIndex(d) && (T.SvcDoors[d].Now > 0.f || T.SvcDoors[d].Want > 0.f)) return false;
	return !T.Cl->Out() && T.Cl->FetchCi == INDEX_NONE;
}

bool AGSE::CleanHeld(const FTractor& T, FString& Why) const
{
	// a crew wait for you while you are at it: two minutes after you last did something they go on (what lies about is fetched first)
	if (!T.Cl || CleanStowed(T)) return false;
	const FSvcClean& C = *T.Cl; bool bLaid = C.FetchCi != INDEX_NONE; for (const FSvcClean::FThing& X : C.Things) if (X.State == FSvcClean::Laid) bLaid = true;
	if (C.IdleT < 120.f) { Why = TEXT("you work with the cleaning van"); return true; }
	if (bLaid && C.IdleT < 480.f) { Why = TEXT("a cleaner fetches what you left by the cleaning van"); return true; }
	return false;
}

bool AGSE::CleanInWay(const FTractor& T, int32 Door) const
{
	// you in a rear leaf's way: it waits
	if (!T.Cl || Door < 1 || Door >= ClSpec(T).Doors.Num() || FMath::Abs(T.YouAt.Z) > 150.) return false;
	const FVector& P = T.SvcDoors[Door].Piv; return T.YouAt.X < -285. && FVector::Dist2D(T.YouAt, P) < 116.;
}

void AGSE::CleanLay(FTractor& T, int32 It, const FVector& Feet, float Yaw)
{
	FSvcClean::FThing& X = T.Cl->Things[It]; X.State = FSvcClean::Laid; X.At = Feet + FRotator(0.f, Yaw, 0.f).Vector() * 45. + FVector(0., 0., FMath::Max(ClFoot[It], 28.f)); X.Yaw = Yaw;
	T.SvcItems[It].bMoved = true; T.Cl->IdleT = 0.f; ADP_LOG(TEXT("Cleaning van: you put the %s down on the apron"), ClName[It]);
}

void AGSE::CleanParts(FTractor& T, float Dt)
{
	FSvcClean& C = *T.Cl; C.IdleT += Dt; C.AskT = FMath::Max(0.f, C.AskT - Dt);
	APlayerController* PC = GetWorld()->GetFirstPlayerController(); APawn* Me = PC ? PC->GetPawn() : nullptr;
	// you step back out of a moving leaf's way, as its crew do
	for (int32 d = 1; d < ClSpec(T).Doors.Num(); ++d)
		if (Me && T.SvcDoors[d].Now != T.SvcDoors[d].Want && CleanInWay(T, d)) Me->AddActorWorldOffset(FRotator(0.f, T.Yaw, 0.f).RotateVector(FVector(-110. * Dt, 0., 0.)), true);
	// left two minutes: shut, and what lies about fetched
	if (T.Job == EJob::None && !T.Player.IsValid() && C.IdleT > 120.f && C.AskT <= 0.f && !CleanStowed(T)) { C.AskT = 10.f; CleanStow(T); }
}

bool AGSE::CleanItem(const FTractor& T, int32 It, FTransform& X) const
{
	if (!CleanYours(T, It)) return false;
	const FSvcClean::FThing& K = T.Cl->Things[It]; const FTransform Root(FRotator(0.f, T.Yaw, 0.f), T.At);
	if (K.State == FSvcClean::Cabin) { X = FTransform(FQuat::Identity, T.SvcItems[It].Piv, FVector(0.001f)); return true; }
	float Yaw = K.Yaw; if (K.State == FSvcClean::Hand) if (const APlayerController* PC = GetWorld()->GetFirstPlayerController()) if (const APawn* P = PC->GetPawn()) Yaw = P->GetActorRotation().Yaw;
	X = FTransform(FRotator(0.f, Yaw + 90.f - T.Yaw, 0.f), K.State == FSvcClean::Hand ? T.YouHand - FVector(0., 0., 6.) : Root.InverseTransformPosition(K.At));
	return true;
}

// ------------------------------------------------------------------ what you left, fetched by a cleaner

void AGSE::CleanGet(int32 Ci, int32 Ti, int32 It)
{
	// up from the ground, back into its place, by the crew's own reach and climb
	const FVector At = Tractors[Ti].Cl->Things[It].At; const FVector Ground = Plan(At.X / 100.f, -At.Y / 100.f);
	auto Side = [this, Ti, At, Ground] { const FVector D = (Tractors[Ti].At - At).GetSafeNormal2D(); FVector P = At + D * 48.f; P.Z = Ground.Z; return P; };
	WalkTo(Ci, Side, 14.f);
	PoseAct(Ci, [Side, At](float K, float PH, FDriverPose& P) { const FVector S = Side(), F = (At - S).GetSafeNormal2D(); P = ADPPose::Reach(S, F, 1, At, 0.8f, PH, FMath::Sin(PI * K)); P.Lean = 34.f * FMath::Sin(PI * K); }, 1.6f);
	Do(Ci, [this, Ci, Ti, It]
	{
		FTractor& T = Tractors[Ti]; if (T.Cl->Things[It].State != FSvcClean::Laid) return;   // (you were quicker)
		T.Cl->Things[It].State = FSvcClean::Van; T.SvcItems[It].Ci = Ci; T.SvcItems[It].bShow = T.SvcItems[It].bMoved = true;
		ADP_LOG(TEXT("Cleaning van: a cleaner fetches the %s you left"), ClName[It]);
	});
	SvcReach(Ci, Ti, It, [this, Ci, Ti, It] { FTractor::FSvcItem& I = Tractors[Ti].SvcItems[It]; if (I.Ci == Ci) { I.Ci = INDEX_NONE; I.bMoved = true; } });
}

void AGSE::CleanStow(FTractor& T)
{
	FSvcClean& C = *T.Cl; if (C.FetchCi != INDEX_NONE || C.Held() != INDEX_NONE) return;
	int32 It = INDEX_NONE; for (int32 i = 0; i < C.Things.Num(); ++i) if (C.Things[i].State == FSvcClean::Laid) It = i;
	if (It == INDEX_NONE) { T.LoadWant = 0; T.SvcDoors[0].Want = 0.f; return; }
	UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); ATugs* G = Tg(); if (!Sp || !G || T.Job != EJob::None) return;
	const int32 Ti = int32(&T - Tractors.GetData()); TArray<UStaffPeople::FTaken> Team;
	if (!Sp->AskTeam(TEXT("cleankit"), EStaffTask::CleanRelief, FDateTime(2000, 1, 1), C.Things[It].At, TEXT("cleaning kit left on the apron"), Team) || !Team.Num()) return;   // (asked again)
	const int32 Ci = G->Enlist(Team[0].A.Get(), Team[0].Id, UStaffPeople::DutyName(Team[0].Duty)); if (Ci == INDEX_NONE) { ADP_WARN(TEXT("Cleaning van: no place for one more person to fetch the kit")); return; }
	G->Crew[Ci].bGse = true; C.FetchCi = Ci; C.FetchIt = It;
	WaitFor(Ci, [this, Ti](float E) { return E > 1800.f || Tractors[Ti].Job == EJob::None; });
	Wait(Ci, 0.2f);
	for (int32 d = 1; d < ClSpec(T).Doors.Num(); ++d) if (T.SvcDoors[d].Now < 0.99f) SvcDoor(Ci, Ti, d, true);
	CleanGet(Ci, Ti, It);
	for (int32 d = ClSpec(T).Doors.Num() - 1; d >= 1; --d) SvcDoor(Ci, Ti, d, false);
	Do(Ci, [this, Ti] { if (FSvcClean* X = Tractors[Ti].Cl.Get()) { X->FetchCi = X->FetchIt = INDEX_NONE; X->AskT = 1.f; } });
	GiveBack(Ci);
}

bool AGSE::CleanLost(int32 Ci)
{
	for (FTractor& T : Tractors)
		if (T.Cl && T.Cl->FetchCi == Ci)
		{
			// (the one who fetched is gone: it stands where he was, the next one fetches it)
			const int32 It = T.Cl->FetchIt; T.Cl->FetchCi = T.Cl->FetchIt = INDEX_NONE; const ATugs* G = Tg();
			if (T.SvcItems.IsValidIndex(It) && T.SvcItems[It].Ci == Ci)
			{
				T.SvcItems[It].Ci = INDEX_NONE; const AActor* A = G ? G->Crew[Ci].A.Get() : nullptr; FVector F = A ? A->GetActorLocation() : T.At; F.Z = Plan(F.X / 100.f, -F.Y / 100.f).Z;
				CleanLay(T, It, F, A ? A->GetActorRotation().Yaw : T.Yaw); T.Cl->IdleT = 121.f; ADP_WARN(TEXT("Cleaning van: the cleaner who fetched the %s is gone - it stands where he was"), ClName[It]);
			}
			return true;
		}
	return false;
}

void AGSE::CleanLast(int32 Job)
{
	// the crew's last look: what you left near the van goes in with the rest, then the doors, then home
	FSvcJob* J = SvcJ(Job); if (!J || !J->Men.Num()) return;
	const int32 Ci = J->Men[0].Ci, Ti = J->Ti; FTractor& T = Tractors[Ti]; FSvcClean& C = *T.Cl;
	for (int32 i = 0; i < C.Things.Num(); ++i)
	{
		if (C.Things[i].State == FSvcClean::Laid && FVector::Dist2D(C.Things[i].At, T.At) < 6000.f) CleanGet(Ci, Ti, i);
		if (C.Things[i].State == FSvcClean::Cabin) { ADP_WARN(TEXT("Cleaning van: the %s was left in the aircraft (its cleaner is gone) - new from the stock"), ClName[i]); C.Things[i].State = FSvcClean::Van; T.SvcItems[i].bShow = false; T.SvcItems[i].bMoved = T.bUsed = true; }
	}
	for (int32 d = ClSpec(T).Doors.Num() - 1; d >= 1; --d) SvcDoor(Ci, Ti, d, false);
	Do(Ci, [this, Job] { Later([this, Job] { SvcLeave(Job); }); });
	Wait(Ci, 0.5f);
}

// ------------------------------------------------------------------ at the aircraft's door

bool AGSE::CleanDoorAt(const FTractor& T, FVector& At) const
{
	// you stand in the jetway at the aircraft its crew clean
	for (const TPair<int32, FSvcJob>& P : SvcJobs)
	{
		if (P.Value.Phase != 2 || !Tractors.IsValidIndex(P.Value.Ti) || &Tractors[P.Value.Ti] != &T || P.Value.Jet == INDEX_NONE) continue;
		const AJetwayRigs* R = AJetwayRigs::Get(GetWorld()); FVector Md, F; if (!R || R->State(P.Value.Jet) != AJetwayRigs::EState::Docked || !R->CabView(P.Value.Jet, Md, F)) return false;
		const FVector Feet = TW(T, T.YouAt); if (T.YouAt.Z < 150. || FMath::Abs(Feet.Z - Md.Z) > 260. || FVector::Dist2D(Feet, Md) > 500.) return false;
		At = Md + F * 95.; At.Z = Feet.Z + 105.; return true;
	}
	return false;
}

int32 AGSE::CleanDoorAct(const FTractor& T, FString& Say, FString& Why) const
{
	// what can be done there: hand in what you hold (its item), take a full bag down (ClAct + its item), or nothing and why
	const FSvcClean& C = *T.Cl; const int32 H = C.Held(); const FServiceSpec& S = ClSpec(T);
	for (const TPair<int32, FSvcJob>& P : SvcJobs)
	{
		const FSvcJob& J = P.Value; if (J.Phase != 2 || &Tractors[J.Ti] != &T) continue;
		bool bIn = false; for (const FSvcMan& M : J.Men) if (M.bIn && !M.bGone) bIn = true;
		if (H != INDEX_NONE)
		{
			if (ClBag(H)) { Say = TEXT("Full bag"); Why = TEXT("it goes into the van's cage"); return INDEX_NONE; }
			Say = FString::Printf(TEXT("Hand the %s in to the cleaners"), ClName[H]);
			const int32 m = S.Take.IndexOfByKey(H);
			if (!bIn) Why = TEXT("no cleaner is in the aircraft"); else if (!J.Men.IsValidIndex(m) || J.Men[m].bGone || !J.Men[m].bNoKit) Why = TEXT("they have what they need"); else if (J.RowsDone >= J.Rows) Why = TEXT("the cabin is done");
			return Why.IsEmpty() ? H : INDEX_NONE;
		}
		for (int32 m = 0; m < J.Men.Num(); ++m)
		{
			const int32 It = S.Bring.IsValidIndex(m) ? S.Bring[m] : INDEX_NONE; if (It < 0 || J.Men[m].bGone || !J.Men[m].bIn) continue;
			if (C.Things[It].State == FSvcClean::Van && T.SvcItems[It].Ci == INDEX_NONE && !T.SvcItems[It].bShow && J.RowsDone >= J.Rows * (It == 4 ? 0.5f : 0.9f)) { Say = TEXT("Take the full bag down to the van"); return ClAct + It; }
		}
		Say = TEXT("Cabin work"); Why = bIn ? FString::Printf(TEXT("the cleaners do it, row %d of %d"), FMath::Min(J.RowsDone + 1, J.Rows), J.Rows) : J.RowsDone >= J.Rows ? TEXT("done by the cleaners") : TEXT("the cleaners do it, they come up when everyone is off");
		return INDEX_NONE;
	}
	return INDEX_NONE;
}

// ------------------------------------------------------------------ its line of controls

bool AGSE::CleanMay(const FTractor& T, int32 It, FString& Why) const
{
	const FSvcClean& C = *T.Cl; const FTractor::FSvcItem& I = T.SvcItems[It]; const uint8 St = C.Things[It].State;
	if (T.Player.IsValid() || T.Speed > 1.f || T.Path.Num()) { Why = TEXT("the van is not parked"); return false; }
	if (St == FSvcClean::Laid) return true;
	if (T.Job != EJob::None && T.Job != EJob::AtStand) { Why = TEXT("its crew are about to drive"); return false; }
	if (St == FSvcClean::Van && (I.Ci != INDEX_NONE || C.FetchIt == It)) { Why = TEXT("a cleaner has it"); return false; }
	if (St == FSvcClean::Van && !I.bShow) { Why = TEXT("used up - new ones when the van stands shut in its box"); return false; }
	if (ClDeep(It))
	{
		// (one at a time in the load space)
		if (T.SvcDoors[1].Now < 0.99f || T.SvcDoors[2].Now < 0.99f) { Why = TEXT("both rear doors have to be open"); return false; }
		const ATugs* G = Tg(); auto In = [&](int32 Ci) { const AActor* A = G && G->Crew.IsValidIndex(Ci) ? G->Crew[Ci].A.Get() : nullptr; if (!A) return false; const FVector L = FRotator(0.f, T.Yaw, 0.f).UnrotateVector(A->GetActorLocation() - T.At); return L.X > -410. && L.X < 0. && FMath::Abs(L.Y) < 75.; };
		bool bIn = In(C.FetchCi); for (const TPair<int32, FSvcJob>& P : SvcJobs) if (P.Value.Phase >= 1 && P.Value.Phase < 9 && &Tractors[P.Value.Ti] == &T) for (const FSvcMan& M : P.Value.Men) if (!M.bGone && !M.bSeated && In(M.Ci)) bIn = true;
		if (bIn) { Why = TEXT("a cleaner is at the back"); return false; }
	}
	return true;
}

void AGSE::CleanPass(FTractor& T, int32 It)
{
	FSvcClean& C = *T.Cl; FSvcClean::FThing& X = C.Things[It]; FTractor::FSvcItem& I = T.SvcItems[It]; C.IdleT = 0.f; I.bMoved = true;
	if (X.State == FSvcClean::Hand) { X.State = FSvcClean::Van; I.Ci = INDEX_NONE; I.bShow = true; T.bUsed = T.bUsed || ClBag(It); ADP_LOG(TEXT("Cleaning van: you put the %s in its place"), ClName[It]); return; }
	FString Why; if (C.Held() != INDEX_NONE || !CleanMay(T, It, Why)) { AVehicleControls::Note(GetWorld(), Why); return; }
	X.State = FSvcClean::Hand; ADP_LOG(TEXT("Cleaning van: you take the %s"), ClName[It]);
}

TSharedPtr<FVehTable> AGSE::CleanControls()
{
	TSharedPtr<FVehTable> Tb = MakeShared<FVehTable>(); using K = FVehControl;
	auto Door = [&](int32 d, const TCHAR* Pt, const TCHAR* Lb) { K X; X.Part = TEXT(""); X.Point = Pt; X.Kind = K::Push; X.Label = Lb; X.Frame = 1 + d; X.Motion = d; X.bGrip = true; Tb->Controls.Add(X); };
	Door(0, TEXT("IK_Handle_SlideDoor_Out"), TEXT("Sliding door")); Door(1, TEXT("IK_Handle_RearDoorR_Out"), TEXT("Right rear door")); Door(2, TEXT("IK_Handle_RearDoorL_Edge"), TEXT("Left rear door"));
	auto Sw = [&](const TCHAR* Pt, const TCHAR* Lb, int32 Code) { K X; X.Part = TEXT(""); X.Point = Pt; X.Kind = K::Push; X.Label = Lb; X.Switch = Code; X.bGrip = X.bHelp = true; Tb->Controls.Add(X); };
	Sw(TEXT("Stock_BagRoll_0"), TEXT("bag roll"), ClItem); Sw(TEXT("Stock_Caddy_0"), TEXT("caddy"), ClItem + 1); Sw(TEXT("CLN_In_2"), TEXT("vacuum cleaner"), ClItem + 2); Sw(TEXT("Stock_BagRoll_1"), TEXT("bag roll"), ClItem + 3);
	Sw(TEXT("CLN_In_4"), TEXT("full bag"), ClItem + 4); Sw(TEXT("CLN_In_5"), TEXT("full bag"), ClItem + 5);
	Sw(TEXT("Stock_Vacuum_0"), TEXT("Aircraft's door"), ClDoor); Sw(TEXT("CLN_In_5"), TEXT("Cleaning cart"), ClCart);
	Tb->Range = 9500.f;
	Tb->Frame = [](const AGSE&, const FTractor& T, int32 Fr) { return Fr < 1 || !T.SvcDoors.IsValidIndex(Fr - 1) ? FTransform::Identity : ClDoorX(T, Fr - 1); };
	Tb->Now = [](const FTractor& T, int32 M) { return T.SvcDoors.IsValidIndex(M) ? T.SvcDoors[M].Now : 0.f; };
	Tb->Want = [](FTractor& T, int32 M, float To) { if (T.SvcDoors.IsValidIndex(M)) { T.LoadWant = INDEX_NONE; T.SvcDoors[M].Want = To; if (T.Cl) T.Cl->IdleT = 0.f; } };
	Tb->May = [](const AGSE& G, const FTractor& T, int32 M, float To, FString& Why)
	{
		// the crew's order: the right leaf first, the left one back first; nobody in the load space, no speed
		if (T.Player.IsValid() || T.Speed > 1.f || T.Path.Num()) Why = TEXT("the van is not parked");
		else if (!T.Cl || T.Cl->FetchCi != INDEX_NONE) Why = TEXT("a cleaner is at it");
		else if (T.Cl->Held() != INDEX_NONE) Why = TEXT("first put back what you carry");
		else if (M == 2 && To > 0.5f && T.SvcDoors[1].Now < 0.99f) Why = TEXT("the right one opens first");
		else if (M == 1 && To < 0.5f && T.SvcDoors[2].Now > 0.f) Why = TEXT("the left one shuts first");
		else if (G.CleanYouIn(T)) Why = TEXT("you are in the load space");
		return Why.IsEmpty();
	};
	Tb->Press = [](AGSE& G, int32 Ti, const FVehControl& C) { FTractor& T = G.Tractors[Ti]; FString Why; if (T.SvcDoors.IsValidIndex(C.Motion) && C.Frame > 0 && T.SvcDoors[C.Motion].Now < 0.01f && CtlTable(T.CtlKey())->May(G, T, C.Motion, 1.f, Why)) G.Sound(*T.SndDoorOpen, G.TW(T, T.SvcDoors[C.Motion].Piv)); };
	Tb->Say = [](const AGSE& G, const FTractor& T, const FVehControl& X) -> FString
	{
		if (!T.Cl || X.Switch == INDEX_NONE) return T.Cl ? FString(X.Label) + (T.SvcDoors.IsValidIndex(X.Motion) && T.SvcDoors[X.Motion].Now > 0.5f ? TEXT(": shut") : TEXT(": open")) : FString();
		const FSvcClean& C = *T.Cl; const int32 H = C.Held(); FVector At; FString Say, Why;
		if (X.Switch == ClDoor) { if (!G.CleanDoorAt(T, At)) return FString(); G.CleanDoorAct(T, Say, Why); return Say; }
		const bool bOpen = T.SvcDoors.Num() > 2 && T.SvcDoors[1].Now > 0.9f;
		if (X.Switch == ClCart) return bOpen && H == INDEX_NONE && T.SvcDoors[2].Now > 0.9f && !T.SvcItems[5].bShow ? FString(X.Label) : FString();
		const int32 It = X.Switch - ClItem; if (!C.Things.IsValidIndex(It) || (H != INDEX_NONE && H != It)) return FString();
		switch (C.Things[It].State)
		{
		case FSvcClean::Hand: return !bOpen ? FString() : FString::Printf(TEXT("%s"), ClBag(It) ? TEXT("Put the full bag into the cage") : *FString::Printf(TEXT("Put the %s back"), ClName[It]));
		case FSvcClean::Laid: return FString::Printf(TEXT("Pick up the %s"), ClName[It]);
		case FSvcClean::Cabin: return FString();
		default: return bOpen && !ClBag(It) ? FString::Printf(TEXT("Take the %s"), ClName[It]) : FString();
		}
	};
	Tb->Spot = [](const AGSE& G, const FTractor& T, const FVehControl& X, FVector& At)
	{
		if (!T.Cl) return false; if (X.Switch == ClDoor) return G.CleanDoorAt(T, At);
		const int32 It = X.Switch - ClItem; if (X.Switch == INDEX_NONE || !T.Cl->Things.IsValidIndex(It) || T.Cl->Things[It].State != FSvcClean::Laid) return false;
		At = T.Cl->Things[It].At; return true;
	};
	Tb->MaySwitch = [](AGSE& G, int32 Ti, int32 S, FString& Why)
	{
		const FTractor& T = G.Tractors[Ti]; if (!T.Cl) return false; FString Say;
		if (S == ClDoor) return G.CleanDoorAct(T, Say, Why) != INDEX_NONE;
		if (S == ClCart) { Why = TEXT("it needs two to lift it out - your cleaners leave it in the van"); return false; }
		return T.Cl->Things.IsValidIndex(S - ClItem) && G.CleanMay(T, S - ClItem, Why);
	};
	Tb->Switch = [](AGSE& G, int32 Ti, int32 S)
	{
		FTractor& T = G.Tractors[Ti]; if (!T.Cl) return false; FSvcClean& C = *T.Cl;
		if (S != ClDoor) { if (C.Things.IsValidIndex(S - ClItem)) G.CleanPass(T, S - ClItem); return true; }
		FString Say, Why; const int32 A = G.CleanDoorAct(T, Say, Why); if (A == INDEX_NONE) return false;
		C.IdleT = 0.f; const int32 It = A >= ClAct ? A - ClAct : A; T.SvcItems[It].bMoved = true;
		// (as the crew's: the rubbish in bags comes out full, the van's stock is used; handed in: out of sight, there is no cabin)
		if (A >= ClAct) { C.Things[It].State = FSvcClean::Hand; T.SvcItems[It].bShow = true; T.bUsed = true; ADP_LOG(TEXT("Cleaning van: you take a full bag down")); }
		else { C.Things[It].State = FSvcClean::Cabin; ADP_LOG(TEXT("Cleaning van: you hand the %s in at the aircraft's door"), ClName[It]); }
		return true;
	};
	Tb->Climb = [](const AGSE& G, int32 Ti, int32 S, float& Secs) -> TFunction<void(float, float, FDriverPose&)>
	{
		// the crew's way in over the rear step, the hand to its place, the same way out (the passing in the middle)
		const FTractor& T0 = G.Tractors[Ti]; const int32 It = S - ClItem; if (!T0.Cl || !T0.Cl->Things.IsValidIndex(It) || S == ClDoor || !ClDeep(It) || T0.Cl->Things[It].State == FSvcClean::Laid) return nullptr;
		AGSE* Me = const_cast<AGSE*>(&G); const FVector At = ClSpec(T0).Items[It].Stand, Place = T0.SvcItems[It].Piv;
		const TFunction<void(float, float, FDriverPose&)> In = Me->SvcDeepFn(Ti, At, true), Out = Me->SvcDeepFn(Ti, At, false), Hand = Me->SvcHandFn(Ti, At, Place);
		TSharedRef<bool> bDone = MakeShared<bool>(false); Secs = 9.7f;
		return [Me, Ti, It, In, Out, Hand, bDone](float Kk, float PH, FDriverPose& P)
		{
			FTractor& T = Me->Tractors[Ti]; const float X = Kk * 9.7f; if (Kk > 0.f) { T.Cl->InAt = Me->GetWorld()->GetTimeSeconds(); T.Cl->IdleT = 0.f; }
			if (X < 4.f) In(X / 4.f, PH, P); else if (X < 5.7f) Hand((X - 4.f) / 1.7f, PH, P); else Out(FMath::Min(1.f, (X - 5.7f) / 4.f), PH, P);
			if (X >= 4.9f && !*bDone) { *bDone = true; Me->CleanPass(T, It); }
		};
	};
	Tb->Held = [](const FTractor& T) -> const TCHAR* { const int32 i = T.Cl ? T.Cl->Held() : INDEX_NONE; return i != INDEX_NONE ? ClBag(i) ? TEXT("the full bag") : i == 2 ? TEXT("the vacuum cleaner") : i == 1 ? TEXT("the caddy") : TEXT("the bag roll") : nullptr; };
	Tb->Leash = [](const AGSE& G, const FTractor& T, FVector& From)
	{
		// (40 m round the van; to the aircraft and back while its crew are at the stand; never held by a van that drives)
		From = FVector(0., 0., 150.); const float Max = T.Job == EJob::AtStand ? 9500.f : 4000.f; return T.Speed > 1.f || T.Path.Num() || T.YouAt.Size() > Max + 300.f ? 0.f : Max;
	};
	Tb->HandSay = [](const AGSE& G, const FTractor& T, FString& Why) -> FString
	{
		const int32 i = T.Cl ? T.Cl->Held() : INDEX_NONE; if (i == INDEX_NONE) return FString();
		const FVector Feet = G.TW(T, T.YouAt); FHitResult Hit;
		if (ClFoot[i] < 0.f) Why = TEXT("it does not stand by itself - back to its dock, or up to the aircraft's door");
		else if (FMath::Abs(Feet.Z - G.Plan(Feet.X / 100.f, -Feet.Y / 100.f).Z) > 14. || G.GetWorld()->LineTraceSingleByChannel(Hit, Feet + FVector(0., 0., 215.), Feet + FVector(0., 0., 1400.), ECC_Visibility)) Why = TEXT("only in the open on the apron");
		return FString::Printf(TEXT("Put the %s down"), ClName[i]);
	};
	Tb->HandDo = [](AGSE& G, FTractor& T, bool bSqueeze)
	{
		FSvcClean& C = *T.Cl; const bool bNew = bSqueeze && !C.bSqueeze; C.bSqueeze = bSqueeze; const int32 i = C.Held(); FString Why; if (!bNew || i == INDEX_NONE) return;
		const FVehTable* Me = AGSE::CtlTable(T.CtlKey()); Me->HandSay(G, T, Why); if (!Why.IsEmpty()) return;
		const APlayerController* PC = G.GetWorld()->GetFirstPlayerController(); const APawn* P = PC ? PC->GetPawn() : nullptr; G.CleanLay(T, i, G.TW(T, T.YouAt), P ? P->GetActorRotation().Yaw : T.Yaw);
	};
	Tb->Drop = [](FTractor& T)
	{
		// (you are gone with it in your hand - E refuses every vehicle while you hold it: back in its place, logged)
		const int32 i = T.Cl ? T.Cl->Held() : INDEX_NONE; if (i == INDEX_NONE) return;
		T.Cl->Things[i].State = FSvcClean::Van; T.SvcItems[i].Ci = INDEX_NONE; T.SvcItems[i].bShow = T.SvcItems[i].bMoved = true; ADP_WARN(TEXT("Cleaning van: you are gone with the %s in your hand - put back in its place"), ClName[i]);
	};
	Tb->Stowed = [](const AGSE& G, const FTractor& T) { return G.CleanStowed(T); };
	Tb->Stow = [](AGSE& G, FTractor& T) { if (T.Cl) G.CleanStow(T); };
	return Tb;
}
