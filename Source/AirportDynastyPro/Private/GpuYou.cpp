// To-do 238: the ground power unit worked by you - its doors, its panel, its plug and cable - with the crew's own movements
// (TugCrew.cpp GpuConnectNow / GpuDisconnectNow) and the same states, so what you do counts for the turn-round and the push. Its
// controls are lines of its gate's tug (TugYou.cpp TugControls). What you leave half done the crew finish from where it is.
#include "GSE.h"
#include "Tugs.h"
#include "VehicleControls.h"
#include "CrewPose.h"
#include "BabylonModel.h"
#include "ADPLog.h"
#include "StaffPeople.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace
{
	enum : int32 { GpDoor = 311, GpStart, GpStop, GpOn, GpOff, GpEStop, GpKey, GpSideL, GpSideR, GpTake, GpPut, GpPlug, GpPull, GpLift };   // (to 324: AGSE::GpuIs)
	enum : int32 { BtStart, BtStop, BtOn, BtOff, BtEStop, BtKey };
	const TCHAR* GpPart[] = {TEXT("PanelBtn_Start"), TEXT("PanelBtn_Stop"), TEXT("PanelBtn_ContactorOn"), TEXT("PanelBtn_ContactorOff"), TEXT("EStopButton"), TEXT("PanelKey")};
	// (the model's own places, Blender metres: x to the drawbar, y left, z up)
	const FVector GpAt[] = {FVector(-0.98f, 0.24f, 1.10f), FVector(-0.98f, 0.14f, 1.10f), FVector(-0.98f, -0.14f, 1.10f), FVector(-0.98f, -0.24f, 1.10f), FVector(-1.156f, -0.47f, 1.20f), FVector(-0.98f, 0.f, 1.10f)};
	const FVector GpPlugGrip(1.18f, -0.24f, 1.165f), GpGland(1.06f, -0.45f, 0.78f), GpHinge(-1.11f, 0.388f, 1.14f), GpHandle(-1.13f, -0.30f, 1.12f);
	const FVector GpSideHinge[] = {FVector(0.488f, 0.63f, 1.23f), FVector(0.488f, -0.63f, 1.23f)}, GpSideHandle[] = {FVector(-0.63f, 0.65f, 1.23f), FVector(-0.63f, -0.65f, 1.23f)};
	constexpr float GpReach = 2100.f;                       // (22.6 m of cable, its slack off)
	constexpr float GpLeave = 4000.f, GpWait = 120.f;       // (you 40 m off, or away from it for 2 min: the crew finish it)
	constexpr float GpWarm = 12.f;                          // (the crew's warming up)
	FVector GpL(const FVector& B) { return FVector(B.X, -B.Y, B.Z) * 100.f; }
	FVector GpSwing(const FVector& Hinge, const FVector& P, float Deg) { const FVector H = GpL(Hinge); return H + FRotator(0.f, Deg, 0.f).RotateVector(GpL(P) - H); }
	const APawn* GpMe(const UObject* O) { const UWorld* W = O->GetWorld(); const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr; return PC ? PC->GetPawn() : nullptr; }
	ATugs::FCrewAct GpDo(TFunction<void()> F) { ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::Do; A.Fn = MoveTemp(F); return A; }
	ATugs::FCrewAct GpWalk(TFunction<FVector()> To, float Stop) { ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::Walk; A.To = MoveTemp(To); A.Stop = Stop; return A; }
}

// ------------------------------------------------------------------ its states

bool ATugs::GpuCrewAt(const FTug& T, FString* Who) const
{
	// one of the crew has begun at it (waiting for you to be done: not begun)
	auto Live = [this](int32 c) { return Crew.IsValidIndex(c) && !Crew[c].bSlotFree && (Crew[c].Q.Num() || Crew[c].bLost); };
	const FTug::FGpu& G = T.Gpu;
	const int32 c = G.bWorkGo && Live(G.Worker) ? G.Worker : G.bInHand && !G.bYou && !G.bLying && Crew.IsValidIndex(G.Holder) ? G.Holder : INDEX_NONE;
	if (c != INDEX_NONE && Who) *Who = Crew[c].Name;
	return c != INDEX_NONE;
}

bool ATugs::GpuDone(const FTug& T) const
{
	const FTug::FGpu& G = T.Gpu;
	return !G.bInHand && !G.bEStop && G.PanelWant <= 0.f && G.SideWant[0] == 0.f && G.SideWant[1] == 0.f && (G.bConnected ? G.bPower : G.RunT < 0.f);
}

void ATugs::GpuEngine(FTug& T, bool bOn)
{
	FTug::FGpu& G = T.Gpu;
	if (bOn) { G.RunT = 0.f; Sound(TEXT("GPUStart"), GpuW(T, 0.f, 0.f, 0.8f)); ADP_LOG(TEXT("Tugs: GPU started at x %.0f by you"), T.StandX); return; }
	G.RunT = -1.f; if (G.Idle) { G.Idle->Stop(); G.Idle = nullptr; }
	Sound(TEXT("GPUStop"), GpuW(T, 0.f, 0.f, 0.8f));
}

void ATugs::GpuLay(FTug& T)
{
	// out of your hand: the plug lies on the ground where you are (bInHand stays: the cable is out)
	FTug::FGpu& G = T.Gpu; if (!G.bYou) return;
	G.bYou = false; G.bYouOwn = false; G.bLying = true; G.bInHand = true; G.bLeftOpen = true; G.Holder = INDEX_NONE; G.HelpT = 0.f;
	G.LieAt = G.YouPos; G.LieAt.Z = Plan(G.YouPos.X / 100.f, -G.YouPos.Y / 100.f).Z + 7.f;
	G.LieDir = (G.LieAt - GpuW(T, GpGland.X, GpGland.Y, GpGland.Z)).GetSafeNormal2D(); if (G.LieDir.IsNearlyZero()) G.LieDir = FVector(1, 0, 0);
	ADP_LOG(TEXT("Tugs: GPU at x %.0f - the plug laid down by you"), T.StandX);
}

/** What you work of it: its parts under your hand, whether you are still at it, the crew that finish what you left. */
void ATugs::GpuYouTick(FTug& T, float Dt)
{
	FTug::FGpu& G = T.Gpu; ABabylonModel* M = G.M.Get(); if (!M) return;
	const APawn* Me = GpMe(this); const float Far = Me ? FVector::Dist2D(Me->GetActorLocation(), M->GetActorLocation()) : 1e6f;
	if (!G.bParts && Far < 3000.f)
	{
		G.bParts = true;
		for (int32 b = 0; b < 6; ++b)
		{
			TArray<FString> N{FString(GpPart[b])}; if (b == BtEStop) N.Add(TEXT("EStopStem"));
			G.BtnG[b] = M->BindGroup(N, false, FTransform(GpL(GpAt[b])));
			if (G.BtnG[b] == INDEX_NONE) ADP_WARN(TEXT("Tugs: the GPU at x %.0f has no part %s"), T.StandX, GpPart[b]);
		}
		for (int32 s = 0; s < 2; ++s) G.SideG[s] = M->BindGroup({FString(s ? TEXT("DoorGenR") : TEXT("DoorEngineL"))}, true, FTransform(GpL(GpSideHinge[s])));
	}
	if (G.bParts)
	{
		for (int32 b = 0; b < 6; ++b)
		{
			G.BtnT[b] = FMath::Max(0.f, G.BtnT[b] - Dt);
			const float Want = b == BtKey ? (G.bKey ? 0.f : 1.f) : b == BtEStop && G.bEStop ? 1.f : G.BtnT[b] > 0.f ? 1.f : 0.f;
			if (G.BtnG[b] == INDEX_NONE || FMath::IsNearlyEqual(G.BtnNow[b], Want, 0.001f)) continue;
			G.BtnNow[b] = FMath::FInterpConstantTo(G.BtnNow[b], Want, Dt, 8.f);
			const FVector At = GpL(GpAt[b]);
			M->MoveGroup(G.BtnG[b], b == BtKey ? FTransform(FRotator(0.f, 0.f, -60.f * G.BtnNow[b]), At) : FTransform(At + FVector((b == BtEStop ? 0.6f : 0.4f) * G.BtnNow[b], 0.f, 0.f)));
		}
		for (int32 s = 0; s < 2; ++s)
			if (G.SideG[s] != INDEX_NONE && !FMath::IsNearlyEqual(G.SideAng[s], G.SideWant[s], 0.1f))
			{
				G.SideAng[s] = FMath::FInterpTo(G.SideAng[s], G.SideWant[s], Dt, 4.f); if (FMath::Abs(G.SideAng[s] - G.SideWant[s]) < 0.5f) G.SideAng[s] = G.SideWant[s];
				M->MoveGroup(G.SideG[s], FTransform(FRotator(0.f, G.SideAng[s], 0.f), GpL(GpSideHinge[s])));
				if (G.SideAng[s] == 0.f) Sound(TEXT("GPUDoorClose"), GpuW(T, GpSideHandle[s].X, GpSideHandle[s].Y, 1.2f));
			}
	}
	// the plug in your hand: where your hand is
	if (G.bYou)
	{
		if (GetWorld()->GetTimeSeconds() - G.YouPoseAt < 0.1) G.YouPos = G.YouPose;
		else if (const AGSE* Gs = AGSE::Find(GetWorld())) Gs->GpuHand(int32(&T - Tugs.GetData()), G.YouPos);
	}
	// you, at it: until it is all done, or you are gone
	if (G.bYouOwn && !G.bYou)
	{
		const float FarR = Me ? FVector::Dist2D(Me->GetActorLocation(), Receptacle(T)) : 1e6f;
		G.YouLeft = Far > 1000.f && FarR > 1000.f ? G.YouLeft + Dt : 0.f;
		const bool bDone = GpuDone(T);
		if (bDone || Far > GpLeave || G.YouLeft > GpWait)
		{
			G.bYouOwn = false; G.YouLeft = 0.f; G.HelpT = 0.f; G.bLeftOpen = !bDone;
			if (!bDone) ADP_LOG(TEXT("Tugs: GPU at x %.0f - you left it half done"), T.StandX);
		}
	}
	if ((G.YouClock -= Dt) > 0.f) return;
	G.YouClock = 2.f;
	const bool bAny = G.bYou || G.bLying || G.bYouConn || G.bYouOwn || G.bLeftOpen;
	if (bAny || Far < 3000.f) { const AGSE* Gs = AGSE::Find(GetWorld()); G.bYouPlane = Gs && Gs->AircraftAt(T.StandX); }
	if (!bAny) return;
	if (!G.bConnected) G.bYouConn = false;
	if (G.bYouConn && !G.bYouPlane && !GpuCrewAt(T))
	{
		// (you put it on and its aircraft is gone, its flight called off: never a plug in the air)
		if (++G.YouGone >= 2)
		{
			ADP_WARN(TEXT("Tugs: GPU at x %.0f - the aircraft is gone, the plug lies on the ground"), T.StandX);
			G.bYouConn = G.bConnected = G.bPower = false; G.YouGone = 0; G.YouPos = Receptacle(T); G.bYou = G.bInHand = true; GpuLay(T);
		}
	}
	else G.YouGone = 0;
	GpuHelp(T);
}

/** What you left half done: one of the crew who is free finishes it. */
void ATugs::GpuHelp(FTug& T)
{
	FTug::FGpu& G = T.Gpu; const int32 Tug = int32(&T - Tugs.GetData());
	if (G.bLeftOpen && GpuDone(T)) G.bLeftOpen = false;
	if (!G.bLeftOpen || G.bYouOwn || G.bYou || GpuCrewAt(T) || T.GpuCi != INDEX_NONE) { G.HelpT = 0.f; return; }
	if (Crew.IsValidIndex(G.Worker) && !Crew[G.Worker].bSlotFree && (Crew[G.Worker].Q.Num() || Crew[G.Worker].bLost)) return;   // (one of them is coming for it already)
	if ((G.HelpT += 2.f) < 10.f) return;   // (you may still take it up again)
	const bool bOn = G.bYouPlane && T.Key.IsEmpty() && !G.bYouOff;
	auto Free = [this](int32 c) { return Crew.IsValidIndex(c) && !Crew[c].bSlotFree && !Crew[c].bLeave && !Crew[c].bLost && !Crew[c].bGse && !Crew[c].Q.Num() && !Crew[c].Hold && !Crew[c].Follow && Crew[c].A.IsValid(); };
	int32 Ci = INDEX_NONE; bool bTaken = false;
	if (Free(T.Handler)) Ci = T.Handler;
	if (Ci == INDEX_NONE)
	{
		UStaffPeople* Sp = UStaffPeople::Get(GetWorld());
		if (Sp && (T.bHired || T.Ck.bHired))
		{
			TArray<UStaffPeople::FTaken> Team;
			if (Sp->AskTeam(FString::Printf(TEXT("gpuleft:%d"), Tug), EStaffTask::Gpu, T.Ck.When, G.M->GetActorLocation(), TEXT("ground power left half done"), Team) && Team.Num()) Ci = Enlist(Team[0].A.Get(), Team[0].Id, TEXT("ground power handler"));
		}
		else for (int32 c = 0; c < Crew.Num() && Ci == INDEX_NONE; ++c) if (Crew[c].bWalker && Crew[c].Tug == INDEX_NONE && !Crew[c].bHired && Free(c)) { Ci = c; Crew[c].bGse = true; }
		bTaken = Ci != INDEX_NONE;
	}
	if (Ci == INDEX_NONE) { if (FMath::Fmod(G.HelpT, 60.f) < 2.f) ADP_LOG(TEXT("Tugs: GPU at x %.0f - left half done, nobody free"), T.StandX); return; }
	G.HelpT = 0.f; FCrew& C = Crew[Ci];
	ADP_LOG(TEXT("Tugs: GPU at x %.0f - %s finishes what you left (%s)"), T.StandX, *C.Name, bOn ? TEXT("onto the aircraft") : TEXT("off and stowed"));
	if (bTaken) SafeWay(C, T.StandX);
	if (bOn) GpuConnect(Tug, C); else GpuDisconnect(Tug, C);
	if (!bTaken) return;
	if (C.bHired) { C.Q.Add(GpWalk([this, Sx = T.StandX] { return Plan(Sx - 18.5f, 104.f); }, 80.f)); LeaveAct(C); }
	else { C.Q.Add(GpWalk([At = C.Spot] { return At; }, 30.f)); C.Q.Add(GpDo([this, Ci] { Crew[Ci].bGse = false; Crew[Ci].AnchorYaw = Crew[Ci].SpotYaw; })); }
}

/** The crew's work at it: they wait while you are at it, then do what is still to be done (planned then). */
void ATugs::GpuConnect(int32 Tug, FCrew& C) { GpuQueue(Tug, C, true); }
void ATugs::GpuDisconnect(int32 Tug, FCrew& C) { GpuQueue(Tug, C, false); }

void ATugs::GpuQueue(int32 Tug, FCrew& C, bool bOn)
{
	const int32 Ci = int32(&C - Crew.GetData()); FTug::FGpu& G = Tugs[Tug].Gpu;
	if (!GpuCrewAt(Tugs[Tug])) { G.Worker = Ci; G.bWorkGo = false; }
	// (never two people at one cable: not while you or another of them is at it)
	FCrewAct W; W.Kind = FCrewAct::WaitFor;
	W.Until = [this, Tug, Ci](float) { const FTug& U = Tugs[Tug]; return !U.Gpu.bYouOwn && !U.Gpu.bYou && (!GpuCrewAt(U) || U.Gpu.Worker == Ci); };
	C.Q.Add(W);
	C.Q.Add(GpDo([this, Tug, Ci, bOn] { GpuPlan(Tug, Ci, bOn); }));
}

void ATugs::GpuPlan(int32 Tug, int32 Ci, bool bOn)
{
	FCrew& C = Crew[Ci]; FTug::FGpu& G = Tugs[Tug].Gpu; G.Worker = Ci; G.bWorkGo = true; G.bLeftOpen = false;
	if (bOn && (G.bLying || G.bYouConn) && !G.bYouPlane) bOn = false;   // (its aircraft gone meanwhile)
	TArray<FCrewAct> Was = MoveTemp(C.Q); C.Q.Reset();
	if (bOn) GpuConnectNow(Tug, C); else GpuDisconnectNow(Tug, C);
	// (the side doors you left open)
	for (int32 s = 0; s < 2; ++s)
		if (G.SideWant[s] != 0.f)
		{
			const float Sy = s ? -1.35f : 1.35f;
			C.Q.Add(GpWalk([this, Tug, Sy] { return GpuW(Tugs[Tug], -0.63f, Sy, 0.f); }, 15.f));
			FCrewAct A; A.Kind = FCrewAct::Pose; A.Dur = 1.f;
			A.PoseFn = [this, Tug, s, Sy](float K, float PH, FDriverPose& P)
			{
				const FTug& U = Tugs[Tug]; const FVector At = GpuW(U, -0.63f, Sy, 0.f);
				P = ADPPose::Reach(At, (GpuW(U, -0.63f, 0.f, 0.f) - At).GetSafeNormal2D(), 1, GpuW(U, GpSideHandle[s].X, GpSideHandle[s].Y, GpSideHandle[s].Z), 0.8f, PH, ADPPose::Ramp(K));
			};
			C.Q.Add(A); C.Q.Add(GpDo([this, Tug, s] { Tugs[Tug].Gpu.SideWant[s] = 0.f; }));
		}
	C.Q.Add(GpDo([this, Tug, Ci] { FTug::FGpu& H = Tugs[Tug].Gpu; if (H.Worker == Ci) { H.Worker = INDEX_NONE; H.bWorkGo = false; } }));
	// (behind the act that is running now, ahead of what they had to do after)
	TArray<FCrewAct> Now = MoveTemp(C.Q); C.Q = MoveTemp(Was); C.Q.Insert(MoveTemp(Now), FMath::Min(1, C.Q.Num()));
}

/** The plug picked up from the ground by one of the crew. */
void ATugs::GpuPickUp(int32 Tug, FCrew& C)
{
	const int32 Ci = int32(&C - Crew.GetData());
	auto By = [this, Tug] { const FTug& U = Tugs[Tug]; const FVector S = (GpuW(U, 1.70f, -0.55f, 0.f) - U.Gpu.LieAt).GetSafeNormal2D(); FVector P = U.Gpu.LieAt + S * 50.f; P.Z = Plan(P.X / 100.f, -P.Y / 100.f).Z; return P; };
	C.Q.Add(GpWalk(By, 12.f));
	FCrewAct A; A.Kind = FCrewAct::Pose; A.Dur = 2.f;
	A.PoseFn = [this, Tug, By](float K, float PH, FDriverPose& P) { const FVector At = By(), H = Tugs[Tug].Gpu.LieAt; P = ADPPose::Squat(At, (H - At).GetSafeNormal2D(), H, PH, ADPPose::Ramp(K)); };
	C.Q.Add(A);
	C.Q.Add(GpDo([this, Tug, Ci] { FTug::FGpu& G = Tugs[Tug].Gpu; G.bLying = false; G.bInHand = true; G.Holder = Ci; }));
}

// ------------------------------------------------------------------ you, on foot

bool AGSE::GpuHand(int32 TugIx, FVector& At) const
{
	for (const FTractor& T : Tractors) if (T.TugIx == TugIx && !T.YouHand.IsZero()) { At = TW(T, T.YouHand); return true; }
	return false;
}

FVector AGSE::GpuSpot(const FTractor& T, int32 S) const
{
	const ATugs* Gt = Tg(); if (!Gt || !Gt->Tugs.IsValidIndex(T.TugIx)) return T.At;
	const ATugs::FTug& U = Gt->Tugs[T.TugIx]; const ATugs::FTug::FGpu& G = U.Gpu; const ABabylonModel* M = G.M.Get(); if (!M) return T.At;
	const FTransform X = M->GetActorTransform();
	switch (S)
	{
	case GpDoor: return X.TransformPosition(GpSwing(GpHinge, GpHandle, G.PanelAng));
	case GpSideL: case GpSideR: return X.TransformPosition(GpSwing(GpSideHinge[S - GpSideL], GpSideHandle[S - GpSideL], G.SideAng[S - GpSideL]));
	case GpTake: case GpPut: return X.TransformPosition(GpL(GpPlugGrip));
	case GpPlug: case GpPull: return Gt->Receptacle(U) - FVector(0, 0, 12.f);
	case GpLift: return G.LieAt + FVector(0, 0, 25.f);
	default: return X.TransformPosition(GpL(GpAt[S == GpStart ? BtStart : S == GpStop ? BtStop : S == GpOn ? BtOn : S == GpOff ? BtOff : S == GpEStop ? BtEStop : BtKey]));
	}
}

FString AGSE::GpuSay(const FTractor& T, int32 S) const
{
	const ATugs* Gt = Tg(); if (!Gt || !Gt->Tugs.IsValidIndex(T.TugIx)) return FString();
	const ATugs::FTug::FGpu& G = Gt->Tugs[T.TugIx].Gpu; const ABabylonModel* M = G.M.Get(); const APawn* Me = GpMe(this);
	if (!M || !Me || FVector::DistSquared2D(M->GetActorLocation(), Me->GetActorLocation()) > FMath::Square(3000.f)) return FString();
	const FVector L = M->GetActorTransform().InverseTransformPosition(Me->GetActorLocation());
	const bool bBack = L.X < -115.f && !G.bYou, bPanel = bBack && G.PanelAng > 60.f;
	const TCHAR* Say = TEXT("");
	switch (S)
	{
	case GpDoor: if (bBack) Say = G.PanelWant > 0.f ? TEXT("Close the panel door") : TEXT("Open the panel door"); break;
	case GpStart: if (bPanel) Say = TEXT("Start the diesel"); break;
	case GpStop: if (bPanel) Say = TEXT("Stop the diesel"); break;
	case GpOn: if (bPanel) Say = TEXT("Power on"); break;
	case GpOff: if (bPanel) Say = TEXT("Power off"); break;
	case GpEStop: if (bBack) Say = G.bEStop ? TEXT("Turn the emergency stop out") : TEXT("Emergency stop"); break;
	case GpKey: if (bPanel) Say = G.bKey ? TEXT("Turn the key off") : TEXT("Turn the key on"); break;
	case GpSideL: if (L.Y < -88.f && !G.bYou) Say = G.SideWant[0] != 0.f ? TEXT("Close the engine door") : TEXT("Open the engine door"); break;
	case GpSideR: if (L.Y > 88.f && !G.bYou) Say = G.SideWant[1] != 0.f ? TEXT("Close the generator door") : TEXT("Open the generator door"); break;
	case GpTake: if (!G.bInHand && !G.bConnected && L.X > 115.f) Say = TEXT("Take the plug"); break;
	case GpPut: if (G.bYou && L.X > 115.f) Say = TEXT("Plug back in its socket"); break;
	case GpPlug: if (G.bYou) Say = TEXT("Plug in the ground power"); break;
	case GpPull: if (G.bConnected) Say = TEXT("Unplug the ground power"); break;
	case GpLift: if (G.bLying) Say = TEXT("Pick the plug up"); break;
	}
	return Say;
}

bool AGSE::GpuMay(int32 Ti, int32 S, FString& Why) const
{
	auto No = [&Why](const TCHAR* X) { Why = X; return false; };
	const FTractor& T = Tractors[Ti]; const ATugs* Gt = Tg(); if (!Gt || GpuSay(T, S).IsEmpty()) return No(TEXT("not now"));
	const ATugs::FTug& U = Gt->Tugs[T.TugIx]; const ATugs::FTug::FGpu& G = U.Gpu;
	if (S == GpEStop && !G.bEStop) return true;   // (always, for anyone)
	if (FString Who; Gt->GpuCrewAt(U, &Who)) { Why = FString::Printf(TEXT("%s is working at it"), *Who); return false; }
	for (const FTractor& O : Tractors) if (O.TowGpu == T.TugIx) return No(TEXT("it is hitched to a tractor"));
	switch (S)
	{
	case GpStart: if (!G.bKey) return No(TEXT("turn the key on first")); if (G.bEStop) return No(TEXT("the emergency stop is in")); if (G.RunT >= 0.f) return No(TEXT("it is running")); break;
	case GpStop: if (G.RunT < 0.f) return No(TEXT("it is not running")); if (G.bPower) return No(TEXT("power off first")); break;
	case GpOn:
		if (G.bPower) return No(TEXT("the power is on"));
		if (!G.bConnected) return No(TEXT("plug the cable into the aircraft first"));
		if (G.RunT < 0.f) return No(TEXT("start the diesel first"));
		if (G.RunT < GpWarm) return No(TEXT("it is still warming up"));
		break;
	case GpOff: if (!G.bPower) return No(TEXT("the power is off")); break;
	case GpKey: if (G.bKey && G.RunT >= 0.f) return No(TEXT("stop the diesel first")); break;
	case GpPull: if (G.bPower) return No(TEXT("power off at the panel first")); break;
	case GpPlug:
		if (G.bPower) return No(TEXT("power off at the panel first"));
		if (!G.bYouPlane) return No(TEXT("no aircraft on blocks here"));
		if (Gt->ChkMesh.Num() && !(U.Ck.Stage >= 1 && U.Ck.Stage <= 4 && U.Ck.Items[0].Where == 3)) return No(TEXT("the chocks must be in first"));
		break;
	}
	if (S >= GpTake)
	{
		const APawn* Me = GpMe(this); const float D = Me ? FVector::Dist2D(Me->GetActorLocation(), GpuSpot(T, S)) : 1e6f;
		if (D > 100.f) return No(TEXT("step closer"));
		if (D < 35.f) return No(TEXT("step back a little"));
	}
	return true;
}

bool AGSE::GpuSwitch(int32 Ti, int32 S)
{
	// a door, a button, the key: under your hand
	if (S < GpDoor || S > GpSideR) return false;
	if (FString Why; !GpuMay(Ti, S, Why)) return false;   // (a crew was quicker)
	ATugs* Gt = Tg(); ATugs::FTug& U = Gt->Tugs[Tractors[Ti].TugIx]; ATugs::FTug::FGpu& G = U.Gpu;
	if (!Gt->GpuCrewAt(U)) { G.bYouOwn = true; G.YouLeft = 0.f; }
	const FVector At = GpuSpot(Tractors[Ti], S);
	switch (S)
	{
	case GpDoor: G.PanelWant = G.PanelWant > 0.f ? 0.f : 100.f; if (G.PanelWant > 0.f) Gt->Sound(TEXT("GPUDoorOpen"), At); break;
	case GpSideL: case GpSideR:
	{
		float& W = G.SideWant[S - GpSideL]; W = W != 0.f ? 0.f : S == GpSideL ? 95.f : -95.f;
		if (W != 0.f) Gt->Sound(TEXT("GPUDoorOpen"), At);
		break;
	}
	case GpStart: G.BtnT[BtStart] = 0.4f; G.bYouOff = false; Gt->GpuEngine(U, true); break;
	case GpStop: G.BtnT[BtStop] = 0.4f; Gt->GpuEngine(U, false); ADP_LOG(TEXT("Tugs: GPU stopped at x %.0f by you"), U.StandX); break;
	case GpOn:
		G.BtnT[BtOn] = 0.4f; G.bPower = true; G.bYouOff = false;
		ADP_LOG(TEXT("Tugs: ground power on the aircraft at x %.0f by you"), U.StandX);
		AVehicleControls::Note(GetWorld(), TEXT("The aircraft is on ground power - close the panel door"));
		break;
	case GpOff: G.BtnT[BtOff] = 0.4f; G.bPower = false; G.bYouOff = true; ADP_LOG(TEXT("Tugs: ground power off at x %.0f by you"), U.StandX); break;
	case GpKey: G.bKey = !G.bKey; break;
	case GpEStop:
		G.bEStop = !G.bEStop;
		if (G.bEStop) { G.bPower = false; if (G.RunT >= 0.f) Gt->GpuEngine(U, false); ADP_LOG(TEXT("Tugs: GPU at x %.0f - emergency stop by you"), U.StandX); }
		break;
	}
	return true;
}

TFunction<void(float, float, FDriverPose&)> AGSE::GpuAct(int32 Ti, int32 S, float& Secs) const
{
	// the plug: the crew's own movements, from where you stand
	const APawn* Me = GpMe(this); const ATugs* Gt0 = Tg(); if (!Me || !Gt0 || S < GpTake || S > GpLift) return nullptr;
	if (FString Why; !GpuMay(Ti, S, Why)) return nullptr;
	FVector At = Me->GetActorLocation(); At.Z = Gt0->Plan(At.X / 100.f, -At.Y / 100.f).Z;
	const FVector Hands = S == GpLift ? Gt0->Tugs[Tractors[Ti].TugIx].Gpu.LieAt : GpuSpot(Tractors[Ti], S), Face = (Hands - At).GetSafeNormal2D();
	Secs = S == GpPlug ? 2.2f : S == GpPull || S == GpLift ? 2.f : S == GpPut ? 1.2f : 1.f;
	TWeakObjectPtr<AGSE> Self(const_cast<AGSE*>(this)); TSharedRef<bool> Done = MakeShared<bool>(false);
	return [Self, Ti, S, At, Face, Hands, Done](float K, float PH, FDriverPose& P)
	{
		P = S == GpLift ? ADPPose::Squat(At, Face, Hands, PH, ADPPose::Ramp(K)) : ADPPose::Reach(At, Face, 1, Hands, 1.f, PH, ADPPose::Ramp(K));
		AGSE* Gs = Self.Get(); ATugs* Gt = Gs ? Gs->Tg() : nullptr; const int32 Ix = Gs && Gs->Tractors.IsValidIndex(Ti) ? Gs->Tractors[Ti].TugIx : INDEX_NONE; if (!Gt || !Gt->Tugs.IsValidIndex(Ix)) return;
		ATugs::FTug& U = Gt->Tugs[Ix]; ATugs::FTug::FGpu& G = U.Gpu; const double Now = Gs->GetWorld()->GetTimeSeconds();
		if (G.bYou) { G.YouPose = P.Hand[1]; G.YouPoseAt = Now; }   // (it goes with your hand)
		if (*Done || K < 0.75f) return;
		*Done = true;
		if (FString Why; !Gs->GpuMay(Ti, S, Why) && !Why.StartsWith(TEXT("step "))) { AVehicleControls::Note(Gs->GetWorld(), Why); return; }
		G.bYouOwn = true; G.YouLeft = 0.f; G.bLeftOpen = false;
		switch (S)
		{
		case GpTake: G.bYouOff = false; G.bYou = G.bInHand = true; G.Holder = INDEX_NONE; break;
		case GpLift: G.bLying = false; G.bYou = G.bInHand = true; G.Holder = INDEX_NONE; break;
		case GpPut: G.bYou = G.bInHand = false; G.Holder = INDEX_NONE; break;
		case GpPlug: G.bYou = G.bInHand = false; G.bConnected = G.bYouConn = true; G.bYouOff = false; Gt->Sound(TEXT("GPUPlugConnect"), Hands); break;
		case GpPull: G.bConnected = G.bYouConn = false; G.bYouOff = true; G.bYou = G.bInHand = true; G.Holder = INDEX_NONE; Gt->Sound(TEXT("GPUPlugDisconnect"), Hands); break;
		}
		if (G.bYou) { G.YouPos = G.YouPose = P.Hand[1]; G.YouPoseAt = Now; }
		ADP_LOG(TEXT("Tugs: GPU at x %.0f - the plug %s by you"), U.StandX, S == GpTake ? TEXT("taken") : S == GpLift ? TEXT("picked up") : S == GpPut ? TEXT("put back in its socket") : S == GpPlug ? TEXT("put into the aircraft") : TEXT("pulled off the aircraft"));
	};
}

void AGSE::GpuAdd(FVehTable& Tb)
{
	using K = FVehControl;
	for (int32 S = GpDoor; S <= GpLift; ++S)
	{
		K X; X.Part = TEXT(""); X.Point = TEXT("TR_Hitch"); X.Kind = K::Push; X.Label = TEXT("Ground power"); X.Switch = S;
		X.bGrip = S == GpDoor || S == GpKey || S == GpSideL || S == GpSideR; Tb.Controls.Add(X);
	}
	Tb.Range = 60000.f;   // (it stands at its gate wherever the gate's tug is; GpuSay looks whether you are near it)
	Tb.Held = [](const FTractor& T) -> const TCHAR* { const ATugs* Gt = T.M.IsValid() ? ATugs::Get(T.M->GetWorld()) : nullptr; return Gt && Gt->Tugs.IsValidIndex(T.TugIx) && Gt->Tugs[T.TugIx].Gpu.bYou ? TEXT("the ground power plug") : nullptr; };
	Tb.Drop = [](FTractor& T) { ATugs* Gt = T.M.IsValid() ? ATugs::Get(T.M->GetWorld()) : nullptr; if (Gt && Gt->Tugs.IsValidIndex(T.TugIx)) Gt->GpuLay(Gt->Tugs[T.TugIx]); };
	Tb.Leash = [](const AGSE& G, const FTractor& T, FVector& From) -> float
	{
		const ATugs* Gt = G.Tg(); if (!Gt || !Gt->Tugs.IsValidIndex(T.TugIx)) return 1e9f;
		From = FTransform(FRotator(0.f, T.Yaw, 0.f), T.At).InverseTransformPosition(Gt->GpuW(Gt->Tugs[T.TugIx], GpGland.X, GpGland.Y, GpGland.Z));
		return GpReach;
	};
	Tb.HandSay = [](const AGSE&, const FTractor&, FString&) -> FString { return TEXT("Lay the plug down"); };
	Tb.HandDo = [](AGSE& G, FTractor& T, bool bDown) { ATugs* Gt = G.Tg(); if (bDown && Gt && Gt->Tugs.IsValidIndex(T.TugIx)) Gt->GpuLay(Gt->Tugs[T.TugIx]); };
}
