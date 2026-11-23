// The service vehicles at work (to-do 212; ServiceVehicles.h, docs/Hired-Crew-Plan.md): the cabin cleaning van first. Its
// driver and cleaners - people you have hired - walk to it, get in by their doors and sit, drive to the stand, take their
// things out of the back, go up the jetway's service stair when the last passenger is off, clean, come down with the full
// bags, load them, and drive back to the van's box. Nobody free: the job waits in the queue (UStaffPeople::AskTeam) and the
// flight with it. The other airlines' flights, and yours while you employ fewer than two: the agent, as data, as before.
// There is no cabin inside the aircraft yet (to-do 209): at its door they are out of sight and the rows are worked by time.
#include "GSE.h"
#include "ServiceVehicles.h"
#include "Tugs.h"
#include "StaffPeople.h"
#include "Turnaround.h"
#include "ServiceDrive.h"
#include "Puddles.h"
#include "Sim/ADPSim.h"
#include "JetwayRigs.h"
#include "VehicleRiders.h"
#include "CrewPose.h"
#include "Site.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "Dom/JsonObject.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"

namespace
{
	const float SvRowSeconds = 40.f;   // (one person, one row of six: rubbish, belts crossed, tables wiped, the floor)
	const float SvPatience = 2700.f;   // (45 min: stuck - the flight no longer waits for it)
	const FVector SvUp(0, 0, 1);
	FVector SvFace(const FVector& From, const FVector& To) { const FVector D = (To - From).GetSafeNormal2D(); return D.IsNearlyZero() ? FVector(1, 0, 0) : D; }
	const FServiceSpec& SvSpec(int32 i) { return ADPService::All()[i]; }
	const FServiceDoor& SvDoor(const FServiceSpec& S, int32 d) { return d < S.Doors.Num() ? S.Doors[d] : S.Cab[d - S.Doors.Num()].Door; }   // (its doors, then the Cab seats' own)
	bool SvIs(int32 i, const TCHAR* Task) { return FCString::Strcmp(SvSpec(i).Task, Task) == 0; }
}

const TArray<FServiceSpec>& ADPService::All()
{
	static TArray<FServiceSpec> L;
	if (!L.Num())
	{
		FServiceSpec S;
		S.Task = TEXT("clean"); S.Model = TEXT("CleaningVan"); S.Points = TEXT("cleaningvan"); S.Label = TEXT("cleaning van"); S.Role = TEXT("cleaning"); S.Team = EStaffTask::Cleaning; S.MaxPeople = 4;
		S.HomeX = -259.6f; S.HomeY = 91.f;
		// left of the aircraft behind the wing, ahead of the tailplane (the belt loader's mirror): in from the tail, never along a stand line
		S.Lat = 11.4f; S.Aft = 25.5f; S.TurnR = 620.f; S.MaxSteer = 36.f; S.DoorDeg = 68.f; S.bJetway = true;
		S.Doors = {{TEXT("SlideDoor"), TEXT("PIV_SlideDoor"), TEXT("IK_Handle_SlideDoor_Out"), TEXT("IK_Stand_SlideDoor"), 0.f, true},
			{TEXT("RearDoorR"), TEXT("PIV_RearDoorR"), TEXT("IK_Handle_RearDoorR_Out"), TEXT("IK_Stand_RearDoorR"), -100.f, false},
			{TEXT("RearDoorL"), TEXT("PIV_RearDoorL"), TEXT("IK_Handle_RearDoorL_Edge"), TEXT("IK_Stand_RearDoorL"), 100.f, false}};
		const FVector Shelf(-328.f, -42.f, 0.f), In(-165.f, 5.f, 58.5f), Cage(-215.f, 10.f, 58.5f);
		S.Items = {{TEXT("Stock_BagRoll_0"), true, false, Shelf}, {TEXT("Stock_Caddy_0"), true, false, Shelf}, {TEXT("Stock_Vacuum_0"), true, true, In}, {TEXT("Stock_BagRoll_1"), true, false, Shelf},
			{TEXT("Stock_BagFull_0"), false, true, Cage}, {TEXT("Stock_BagFull_1"), false, true, Cage}};
		S.Take = {0, 1, 2, 3}; S.Bring = {4, -1, -1, 5};
		S.Seats = {TEXT("IK_Seat_SeatRow2"), TEXT("IK_Seat_SeatRow1"), TEXT("IK_Seat_SeatRow0")};   // (the farthest from the door first)
		S.NameL = TEXT("IK_Name_L"); S.NameR = TEXT("IK_Name_R"); S.NameRear = TEXT("IK_Name_Rear");
		S.Cab = {{TEXT("IK_Seat_SeatPassenger"), {TEXT("DoorPassenger"), TEXT("PIV_DoorPassenger"), TEXT("IK_Handle_DoorPassenger_Out"), TEXT("IK_Stand_DoorPassenger"), -68.f, false}}};
		S.Wipers = {{TEXT("WiperL"), TEXT("PIV_Wiper_L"), FVector(210.6f, -4.f, 143.3f), FVector(176.3f, -50.f, 178.f)}, {TEXT("WiperR"), TEXT("PIV_Wiper_R"), FVector(204.2f, 68.f, 143.3f), FVector(178.2f, 22.f, 178.f)}};
		S.LiveryBody = TEXT("CV_Livery_Body_Paint"); S.LiveryMain = TEXT("CV_Livery_Main_Paint");
		L.Add(S);
	}
	return L;
}

// ------------------------------------------------------------------ the vehicle

const TCHAR* AGSE::SvcModel(int32 Svc) { return SvSpec(Svc).Model; }
FVector AGSE::SvcP(const FTractor& T, const TCHAR* Name) const { const FVector* V = T.Pt.Find(Name); return V ? *V : FVector::ZeroVector; }

void AGSE::SvcSetup()
{
	for (int32 i = 0; i < ADPService::All().Num(); ++i)
	{
		const FServiceSpec& S = SvSpec(i);
		FVehicleSpec V; if (!V.Load(S.Points)) continue;
		FTractor T; T.Svc = i; T.Pt = V.P; T.Name = S.Label;
		float Yw = 0.f; T.Home = T.At = PlanPose(S.HomeX, S.HomeY, S.HomeHx, S.HomeHy, Yw); T.HomeYaw = T.Yaw = Yw;
		static const TCHAR* W[] = {TEXT("FL"), TEXT("FR"), TEXT("RL"), TEXT("RR")};
		for (int32 w = 0; w < 4; ++w) T.KWheel[w] = V.At(*(FString(TEXT("PIV_Wheel_")) + W[w]));
		for (int32 w = 0; w < 2; ++w) T.KKing[w] = V.At(*(FString(TEXT("PIV_Steer_")) + W[w]));
		T.KHinge = V.At(TEXT("PIV_DoorDriver")); T.KWheelC = V.At(TEXT("PIV_SteeringWheel")); T.KStand = V.At(TEXT("IK_Stand_DoorDriver")); T.KHandle = V.At(TEXT("IK_Handle_DoorDriver_Out"));
		T.KStep = V.At(TEXT("IK_Step_DoorDriver")); T.KGrabOut = V.At(TEXT("IK_Grab_L")); T.KGrabIn = T.KGrabOut + FVector(8.f, 4.f, -12.f); T.KSeat = V.At(TEXT("IK_Seat_SeatDriver"));
		T.KBrake = V.At(TEXT("IK_Foot_Rest")); T.KAccel = V.At(TEXT("IK_Foot_Accel")); T.KHandL = V.At(TEXT("IK_Hand_Wheel_L")); T.KHandR = V.At(TEXT("IK_Hand_Wheel_R"));
		T.KRadF = float(T.KWheel[0].Z); T.KRadR = float(T.KWheel[2].Z); T.KWB = float(T.KWheel[0].X - T.KWheel[2].X); T.KTrack = float(FMath::Abs(T.KWheel[0].Y) * 2.0); T.KFloor = float(V.At(TEXT("IK_Aisle_Row0")).Z);
		T.KHalfL = S.HalfL; T.KHalfW = S.HalfW; T.KCentreX = 0.f; T.KTurnR = S.TurnR; T.KMaxSteer = S.MaxSteer; T.KDoorDeg = S.DoorDeg;
		T.SndStart = TEXT(""); T.SndLoop = TEXT("BLDriveLoop"); T.SndStop = TEXT(""); T.SndDoorOpen = TEXT("BLDoorOpen"); T.SndDoorClose = TEXT("BLDoorClose"); T.SndBeep = TEXT("BLReverseBeep");
		if (SpawnTractor(T))
		{
			Tractors.Add(T); SvcLivery(Tractors.Last());
			// (solid for you on foot - its parts have no collision of their own)
			UBoxComponent* B = NewObject<UBoxComponent>(T.M.Get()); B->SetupAttachment(T.M->GetRootComponent()); B->SetRelativeLocation(FVector(0.f, 0.f, 135.f)); B->SetBoxExtent(FVector(T.KHalfL - 4.f, T.KHalfW - 4.f, 105.f));
			B->SetCollisionEnabled(ECollisionEnabled::QueryOnly); B->SetCollisionObjectType(ECC_WorldDynamic); B->SetCollisionResponseToAllChannels(ECR_Ignore); B->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block); B->SetCanEverAffectNavigation(false); B->RegisterComponent(); T.M->AddInstanceComponent(B);
 ADP_LOG(TEXT("GSE: %s in its box at plan (%.0f, %.0f)"), *T.Name, S.HomeX, S.HomeY);
		}
	}
}

void AGSE::SvcDress(FTractor& T)
{
	const FServiceSpec& S = SvSpec(T.Svc); ABabylonModel* M = T.M.Get(); if (!M) return;
	for (const FServiceDoor& D : S.Doors) { FTractor::FSvcDoor X; X.Piv = SvcP(T, D.Pivot); X.G = M->BindGroup({FString(D.Prefix)}, true, FTransform(X.Piv)); T.SvcDoors.Add(X); }
	for (const FServiceCab& C : S.Cab) { FTractor::FSvcDoor X; X.Piv = SvcP(T, C.Door.Pivot); X.G = M->BindGroup({FString(C.Door.Prefix)}, true, FTransform(X.Piv)); T.SvcDoors.Add(X); }
	T.GPedal[0] = M->BindGroup({TEXT("Pedal_Accel")}, true, FTransform(SvcP(T, TEXT("PIV_Pedal_Accel")))); T.GPedal[1] = M->BindGroup({TEXT("Pedal_Brake")}, true, FTransform(SvcP(T, TEXT("PIV_Pedal_Brake"))));
	for (const FServiceWiper& W : S.Wipers) T.GWiper.Add(M->BindGroup({FString(W.Prefix)}, true, FTransform(SvcP(T, W.Pivot))));
	for (const FServiceItem& I : S.Items) { FTractor::FSvcItem X; X.Piv = SvcP(T, I.Part); X.bShow = I.bAtHome; X.G = M->BindGroup({FString(I.Part)}, true, FTransform(X.Piv)); T.SvcItems.Add(X); }
	// the airport's name, at run time
	FString Name; if (!ADPSite::AirportBlock()->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) return;
	auto Text = [&](const TCHAR* At, float Yaw, float Size)
	{
		if (!At || !T.Pt.Contains(At)) return;
		UTextRenderComponent* X = NewObject<UTextRenderComponent>(M);
		X->SetupAttachment(M->GetRootComponent()); X->SetText(FText::FromString(Name.ToUpper() + TEXT(" AIRPORT")));
		X->SetHorizontalAlignment(EHTA_Center); X->SetVerticalAlignment(EVRTA_TextCenter);
		X->SetWorldSize(FMath::Min(Size, Size * 15.f / float(Name.Len() + 8))); X->SetTextRenderColor(FColor(16, 30, 64));   // (a long name smaller: it stays on its panel)
		X->SetRelativeLocationAndRotation(SvcP(T, At) + FRotator(0.f, Yaw, 0.f).Vector() * 1.2f, FRotator(0.f, Yaw, 0.f));
		X->SetCollisionEnabled(ECollisionEnabled::NoCollision); X->RegisterComponent(); M->AddInstanceComponent(X);
		if (At == S.NameRear) X->ComponentTags.Add(TEXT("SvcRear"));
	};
	Text(S.NameL, -90.f, S.NameSize); Text(S.NameR, 90.f, S.NameSize); Text(S.NameRear, 180.f, S.NameRearSize);
}

void AGSE::SvcLivery(FTractor& T)
{
	// your airline's colours (FADPAirline LiveryBody / LiveryMain, "#rrggbb"; empty: the model's own) on its livery paint
	const UADPSim* Sim = UADPSim::Get(this); const FServiceSpec& S = SvSpec(T.Svc); FString B, Mn;
	if (Sim) for (const FADPAirline& A : Sim->World.Airlines) if (A.bPlayer) { B = A.LiveryBody; Mn = A.LiveryMain; }
	const FString Want = B + TEXT("|") + Mn; ABabylonModel* M = T.M.Get();
	if (!M || Want == T.Livery || (T.Livery.IsEmpty() && Want.Len() < 2)) return;
	T.Livery = Want;
	TArray<UInstancedStaticMeshComponent*> Ps; M->Parts(Ps);
	for (UInstancedStaticMeshComponent* K : Ps)
		for (int32 i = 0; i < K->GetNumMaterials(); ++i)
		{
			const UStaticMesh* Sm = K->GetStaticMesh(); const UMaterialInterface* Own = Sm ? Sm->GetMaterial(i) : nullptr; if (!Own) continue;
			const FString N = Own->GetName(); const FString* C = S.LiveryBody && N == S.LiveryBody ? &B : S.LiveryMain && N == S.LiveryMain ? &Mn : nullptr;
			if (!C || C->IsEmpty()) continue;
			UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(K->GetMaterial(i)); if (!D) D = K->CreateDynamicMaterialInstance(i, K->GetMaterial(i));   // (the dirt's own paint, or the model's)
			const FLinearColor L(FColor::FromHex(*C)); if (D) { D->SetVectorParameterValue(TEXT("BC"), L); D->SetVectorParameterValue(TEXT("BaseColorFactor"), L); }
		}
}

void AGSE::SvcRestock(FTractor& T)
{
	const FServiceSpec& S = SvSpec(T.Svc); T.bUsed = false; T.BoxT = 0.f;
	for (int32 i = 0; i < T.SvcItems.Num(); ++i) { T.SvcItems[i].bShow = S.Items[i].bAtHome; T.SvcItems[i].Ci = INDEX_NONE; T.SvcItems[i].bMoved = true; }
	ADP_LOG(TEXT("GSE: the %s restocked, shut in its box"), *T.Name);
}

void AGSE::SvcParts(FTractor& T, float Dt)
{
	const FServiceSpec& S = SvSpec(T.Svc); ABabylonModel* M = T.M.Get(); if (!M) return;
	// (full bags and used rolls go, new ones come, only while it stands shut in its box and nobody is in it)
	if (T.bUsed)
	{
		bool bShut = T.Job == EJob::None && !T.Player.IsValid() && FVector::Dist2D(T.At, T.Home) < 200.f; for (const FTractor::FSvcDoor& D : T.SvcDoors) if (D.Now > 0.f) bShut = false;
		if ((T.BoxT = bShut ? T.BoxT + Dt : 0.f) > 30.f) SvcRestock(T);
	}
	// the load doors you sent open or shut: one leaf after the other, the last to open the first to shut
	if (T.LoadWant != INDEX_NONE)
	{
		bool bDone = true; const int32 N = S.Doors.Num();
		for (int32 k = 1; k < N && bDone; ++k)
		{
			FTractor::FSvcDoor& D = T.SvcDoors[T.LoadWant ? k : N - k];
			if (D.Want != float(T.LoadWant)) { D.Want = float(T.LoadWant); if (T.LoadWant) Sound(*T.SndDoorOpen, TW(T, D.Piv)); }
			if (D.Now != D.Want) bDone = false;
		}
		if (bDone) T.LoadWant = INDEX_NONE;
	}
	// the pedals: pressed by you, or as it speeds up and slows
	if (!(SvcYours(T) && T.PlayerSeat == 0)) { const bool bBr = T.BrakeHold > 0.f || T.MeasSpeed < 0.05f; T.PedalB = FMath::FInterpTo(T.PedalB, T.bOn && bBr ? 0.5f : 0.f, Dt, 5.f); T.PedalA = FMath::FInterpTo(T.PedalA, T.bOn && !bBr && T.MeasSpeed > 0.1f ? 0.45f : 0.f, Dt, 5.f); }
	for (int32 p = 0; p < 2; ++p) if (T.GPedal[p] != INDEX_NONE && T.bOn) M->MoveGroup(T.GPedal[p], FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(-13.f * (p ? T.PedalB : T.PedalA))), SvcP(T, p ? TEXT("PIV_Pedal_Brake") : TEXT("PIV_Pedal_Accel"))));
	// the wipers: in rain with the drive on (a second and a half there and back, a pause in light rain); parked lying down
	if ((T.RainIn -= Dt) < 0.f) { T.RainIn = 1.f; const APuddles* P = APuddles::Get(); T.bRain = P && P->GetRain() > 0.03f; T.WipeNow = P ? P->GetRain() : 0.f; SvcLivery(T); }
	if (T.GWiper.Num() && ((T.bOn && T.bRain) || T.WipeT > 0.f))
	{
		T.WipeT += Dt; const float Cyc = T.WipeNow > 0.4f ? 1.5f : 3.5f; if (T.WipeT > Cyc) T.WipeT = T.bOn && T.bRain ? T.WipeT - Cyc : 0.f;
		const float E = T.WipeT < 1.5f ? 0.5f - 0.5f * FMath::Cos(2.f * PI * T.WipeT / 1.5f) : 0.f;
		for (int32 w = 0; w < T.GWiper.Num(); ++w)
			if (T.GWiper[w] != INDEX_NONE)
			{
				// (from its rest to wiped up, in the plane of both: along the glass; lifted a little over the glass's bulge)
				const FVector Pv = SvcP(T, S.Wipers[w].Pivot), A = (S.Wipers[w].Rest - Pv).GetSafeNormal(), B = (S.Wipers[w].Up - Pv).GetSafeNormal();
				const FVector Ax = FVector::CrossProduct(A, B).GetSafeNormal(); const float Ang = FMath::Acos(FMath::Clamp(float(FVector::DotProduct(A, B)), -1.f, 1.f));
				M->MoveGroup(T.GWiper[w], FTransform(FQuat(Ax, Ang * E), Pv + FVector(0.73f, 0.f, 0.68f) * (1.f * FMath::Sin(PI * E))));
			}
	}
	for (int32 d = 0; d < T.SvcDoors.Num(); ++d)
	{
		FTractor::FSvcDoor& D = T.SvcDoors[d]; const FServiceDoor& K = SvDoor(S, d);
		if (D.Now == D.Want) continue;
		D.Now = FMath::FInterpConstantTo(D.Now, D.Want, Dt, 0.75f);
		FTransform X(D.Piv);
		if (K.bSlide)
		{
			// (out of its opening first, then straight back along its rail)
			const float Sg = D.Piv.Y >= 0.f ? 1.f : -1.f, A = FMath::Clamp(D.Now / 0.15f, 0.f, 1.f), B = FMath::Clamp((D.Now - 0.15f) / 0.85f, 0.f, 1.f);
			X.SetLocation(D.Piv + FVector(-4.f * A - 126.f * B, 12.5f * A * Sg, 0.f));
		}
		else X.SetRotation(FQuat(SvUp, FMath::DegreesToRadians(K.Deg * FMath::SmoothStep(0.f, 1.f, D.Now))));
		if (D.G != INDEX_NONE) M->MoveGroup(D.G, X);
		if (S.NameRear && FCString::Strcmp(K.Prefix, TEXT("RearDoorL")) == 0)
			for (UActorComponent* C : M->GetComponents())
				if (C && C->ComponentHasTag(TEXT("SvcRear")))
				{ const FTransform Rel = FTransform(FRotator(0.f, 180.f, 0.f), SvcP(T, S.NameRear) - FVector(1.2f, 0.f, 0.f)) * FTransform(D.Piv).Inverse() * X; Cast<USceneComponent>(C)->SetRelativeLocationAndRotation(Rel.GetLocation(), Rel.GetRotation()); }
		if (D.Now == D.Want && D.Want == 0.f) Sound(*T.SndDoorClose, TW(T, D.Piv));
	}
	for (FTractor::FSvcItem& I : T.SvcItems)
	{
		if (I.G == INDEX_NONE || (I.Ci == INDEX_NONE && !I.bMoved)) continue;
		FTransform X(FQuat::Identity, I.Piv, I.bShow ? FVector::OneVector : FVector(0.001f));
		if (I.Ci != INDEX_NONE)
		{
			const USkeletalMeshComponent* B = Body(I.Ci); const ATugs* G = Tg();
			const bool bSeen = B && G && G->Crew[I.Ci].A.IsValid() && !G->Crew[I.Ci].A->IsHidden();
			if (bSeen) X = FTransform(FRotator(0.f, float(G->Crew[I.Ci].A->GetActorRotation().Yaw) + 90.f - T.Yaw, 0.f), FTransform(FRotator(0.f, T.Yaw, 0.f), T.At).InverseTransformPosition(B->GetBoneLocation(TEXT("hand_r")) - SvUp * 6.f), FVector::OneVector);
			else X.SetScale3D(FVector(0.001f));
		}
		I.bMoved = false; M->MoveGroup(I.G, X);
	}
}

bool AGSE::SvcPersonAhead(const FTractor& T, const FVector& Move) const
{
	const ATugs* G = Tg(); if (!G) return false;
	const FVector Sd(-Move.Y, Move.X, 0.f);
	for (const float y : {-85.f, 0.f, 85.f}) for (const float r : {70.f, 180.f}) if (G->PersonAhead(T.At + Move * T.KHalfL + Sd * y, Move, r)) return true;
	return false;
}

// ------------------------------------------------------------------ the jobs

bool AGSE::SvcFor(const TCHAR* Task, float StandX) const
{
	const UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); float S = 0.f, N = 0.f, Sn = 36.f;
	if (bTest || !Sp || !StandGeo(StandX, S, N, Sn) || Sn > 40.f) return false;   // (the A320 stands)
	for (const FTractor& T : Tractors)
		if (T.Svc != INDEX_NONE && SvIs(T.Svc, Task) && Sp->HiresRole(SvSpec(T.Svc).Role))
		{
			if (!SvSpec(T.Svc).bJetway) return true;
			const AJetwayRigs* J = AJetwayRigs::Get(GetWorld()); TArray<FVector> St; float Hw = 0.f; FVector Mid, Out;
			const int32 Ji = J ? J->Nearest(Plan(StandX + 5.f, N + 2.f)) : INDEX_NONE;
			return Ji != INDEX_NONE && J->ServiceSteps(Ji, St, Hw) && J->ServiceDoorFrame(Ji, Mid, Out);   // (no way up: the agent)
		}
	return false;
}

bool AGSE::SvcAsk(const TCHAR* Task, float StandX, const FString& Reg, const FString& Flight, const FDateTime& When)
{
	for (TPair<int32, FSvcJob>& P : SvcJobs) if (P.Value.Reg == Reg && P.Value.Phase < 3 && SvIs(P.Value.Svc, Task)) { P.Value.When = When; P.Value.Flight = Flight; return P.Value.Phase > 0; }
	for (int32 i = 0; i < ADPService::All().Num(); ++i)
		if (SvIs(i, Task))
		{
			FSvcJob J; J.Svc = i; J.StandX = StandX; J.Reg = Reg; J.Flight = Flight; J.When = When;
			float S = 0.f, N = 0.f, Sn = 36.f; const AJetwayRigs* R = AJetwayRigs::Get(GetWorld());
			if (R && StandGeo(StandX, S, N, Sn)) J.Jet = R->Nearest(Plan(StandX + 5.f, N + 2.f));
			SvcJobs.Add(SvcNext++, J); break;
		}
	return false;
}

void AGSE::SvcGo(const TCHAR* Task, float StandX)
{
	for (TPair<int32, FSvcJob>& P : SvcJobs) if (FMath::Abs(P.Value.StandX - StandX) < 1.f && SvIs(P.Value.Svc, Task)) P.Value.bGo = true;
}

bool AGSE::SvcBusy(const TCHAR* Task, float StandX) const
{
	for (const TPair<int32, FSvcJob>& P : SvcJobs)
	{
		const FSvcJob& J = P.Value;
		if (FMath::Abs(J.StandX - StandX) > 1.f || !SvIs(J.Svc, Task) || J.Phase >= 3 || J.T > SvPatience) continue;
		for (const FSvcMan& M : J.Men) if (!M.bGone && !M.bDown) return true;
	}
	return false;
}

void AGSE::SvcGone(float StandX) { for (auto It = SvcJobs.CreateIterator(); It; ++It) if (It.Value().Phase == 0 && FMath::Abs(It.Value().StandX - StandX) < 1.f) It.RemoveCurrent(); }

bool AGSE::SvcWaits(const FString& Flight, float StandX, FString& Why) const
{
	for (const TPair<int32, FSvcJob>& P : SvcJobs)
	{
		const FSvcJob& J = P.Value; const FServiceSpec& S = SvSpec(J.Svc);
		if ((J.Flight != Flight && FMath::Abs(J.StandX - StandX) > 1.f) || J.T > SvPatience) continue;
		const FTractor* Tv = Tractors.IsValidIndex(J.Ti) ? &Tractors[J.Ti] : nullptr; bool bDown = J.Phase == 2; for (const FSvcMan& M : J.Men) if (!M.bGone && !M.bDown) bDown = false;
		if (Tv && J.Phase == 2 && !SvcHere(P.Key))
		{
			// (yours, or left somewhere: its crew wait for it or fetch it; the flight only waits for the cabin)
			if (bDown && FVector::Dist2D(Tv->At, J.Place) > 6000.f) continue;
			Why = SvcYours(*Tv) ? FString::Printf(TEXT("you have the %s - its crew wait for it"), S.Label) : FString::Printf(TEXT("the %s's crew fetch it back"), S.Label); return true;
		}
		if (J.Phase == 0) { Why = FString::Printf(TEXT("the %s waits: %s"), S.Label, J.Why.IsEmpty() ? TEXT("its people are being called") : *J.Why); return true; }
		if (J.Phase < 3) { Why = FString::Printf(TEXT("the %s is still at work (%d of %d rows)"), S.Label, J.RowsDone, J.Rows); return true; }
		if (Tractors.IsValidIndex(J.Ti) && Tractors[J.Ti].Job == EJob::AtStand) { Why = FString::Printf(TEXT("the %s is leaving the stand"), S.Label); return true; }
	}
	return false;
}

bool AGSE::SvcAll(int32 Job, TFunctionRef<bool(const FSvcMan&)> Is) { FSvcJob* J = SvcJ(Job); if (!J) return true; for (const FSvcMan& M : J->Men) if (!M.bGone && !Is(M)) return false; return true; }

bool AGSE::SvcLost(int32 Ci, bool& bDrop)
{
	ATugs* G = Tg(); if (!G) return false;
	for (TPair<int32, FSvcJob>& P : SvcJobs)
		for (int32 m = 0; m < P.Value.Men.Num(); ++m)
		{
			FSvcMan& M = P.Value.Men[m]; if (M.Ci != Ci || M.bGone) continue;
			const int32 Job = P.Key;
			if (Tractors.IsValidIndex(P.Value.Ti)) for (FTractor::FSvcItem& I : Tractors[P.Value.Ti].SvcItems) if (I.Ci == Ci) { I.Ci = INDEX_NONE; I.bMoved = true; }
			ATugs::FCrew& C = G->Crew[Ci];
			if (C.bUp)
			{
				C.bUp = false; M.bIn = false;
				if (TPair<int8, int32>* U = SvcStairUse.Find(P.Value.Jet)) U->Value = FMath::Max(0, U->Value - 1);
				if (m == 0) { C.Q.Reset(); M.bDown = M.bStowed = true; Do(Ci, [this, Job] { Later([this, Job] { SvcLeave(Job); }); }); }   // (whoever takes over drives the others home)
			}
			if (m == 0) { C.bRelClean = true; return true; }        // (the vehicle stands until somebody takes over)
			M.bGone = true; bDrop = true; return true;              // (the others do their rows)
		}
	return false;
}

void AGSE::SvcTick(float Dt)
{
	ATugs* G = Tg(); UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); if (!G || !Sp || !SvcJobs.Num()) return;
	// (the cabin, row by row, by everyone who is in it)
	for (TPair<int32, FSvcJob>& P : SvcJobs)
	{
		FSvcJob& J = P.Value; const float Was = J.T; if (J.Phase > 0) J.T += Dt;
		if (Was <= SvPatience && J.T > SvPatience) ADP_WARN(TEXT("GSE: the %s at stand x %.0f has been at %s for 45 min - the flight no longer waits for it"), SvSpec(J.Svc).Label, J.StandX, *J.Reg);
		int32 In = 0; for (const FSvcMan& M : J.Men) if (M.bIn && !M.bGone) ++In;
		if (!In || J.RowsDone >= J.Rows) continue;
		for (J.RowT += Dt * In; J.RowT >= SvRowSeconds && J.RowsDone < J.Rows; J.RowT -= SvRowSeconds)
			if (++J.RowsDone % 10 == 0 || J.RowsDone == J.Rows) ADP_LOG(TEXT("GSE: %s cleaned to row %d of %d, %d people"), *J.Reg, J.RowsDone, J.Rows, In);
	}
	if ((SvcClock += Dt) < 2.f) return;
	SvcClock = 0.f;
	TArray<int32> Start;
	for (TPair<int32, FSvcJob>& P : SvcJobs)
	{
		FSvcJob& J = P.Value; if (J.Phase != 0) continue;
		const FServiceSpec& S = SvSpec(J.Svc);
		int32 Ti = INDEX_NONE; for (int32 i = 0; i < Tractors.Num() && Ti == INDEX_NONE; ++i) if (Tractors[i].Svc == J.Svc && Tractors[i].Job == EJob::None && Tractors[i].M.IsValid()) Ti = i;
		if (Ti == INDEX_NONE) { J.Why = FString::Printf(TEXT("no %s free"), S.Label); continue; }
		const FTractor& Tv = Tractors[Ti];
		if (SvcYours(Tv)) { J.Why = FString::Printf(TEXT("you have the %s"), S.Label); continue; }
		const bool bHome = FVector::Dist2D(Tv.At, Tv.Home) < 200.f && FMath::Abs(FMath::FindDeltaAngleDegrees(Tv.Yaw, Tv.HomeYaw)) < 20.f;
		if (bHome && Tv.bUsed) { J.Why = FString::Printf(TEXT("the %s is being emptied and restocked"), S.Label); continue; }
		{ bool bOpen = false; for (const FTractor::FSvcDoor& D : Tv.SvcDoors) if (D.Now > 0.01f) bOpen = true; if (bOpen && Tv.Player.IsValid()) { J.Why = FString::Printf(TEXT("you are getting into the %s"), S.Label); continue; } }
		if (!bHome) { TArray<FVector> W; if (!Apron(W, Tv.At, Plan(S.HomeX + S.HomeHx * 34.f, S.HomeY + S.HomeHy * 34.f), &Tv)) { J.Why = FString::Printf(TEXT("the %s stands where it cannot be driven from - bring it back to the apron"), S.Label); continue; } }
		const FString Key = FString::Printf(TEXT("%s:%s"), S.Task, *J.Reg), What = FString::Printf(TEXT("%s %s"), J.Flight.IsEmpty() ? *J.Reg : *J.Flight, UStaffPeople::TaskName(S.Team));
		TArray<UStaffPeople::FTaken> Team;
		if (!Sp->AskTeam(Key, S.Team, J.When - FTimespan::FromMinutes(20.0), Tractors[Ti].At, What, Team)) { J.Why = Sp->WaitsFor(Key); continue; }
		// (more of them, as many as no other waiting job needs)
		while (Team.Num() && Team.Num() < S.MaxPeople) { UStaffPeople::FTaken X; if (!Sp->AskExtra(Team.Last().Duty, Tractors[Ti].At, What, X)) break; Team.Add(X); }
		TArray<FSvcMan> Men;
		for (const UStaffPeople::FTaken& X : Team)
		{
			FSvcMan M; M.Ci = G->Enlist(X.A.Get(), X.Id, UStaffPeople::DutyName(X.Duty)); if (M.Ci == INDEX_NONE) continue;
			G->Crew[M.Ci].bGse = true; M.Seat = Men.Num(); Men.Add(M);
		}
		if (Men.Num() < 2) { for (const FSvcMan& M : Men) G->Discharge(M.Ci); J.Why = TEXT("no place for more people"); continue; }
		J.Men = Men; J.Ti = Ti; J.Phase = 1; J.Why.Reset(); Tractors[Ti].Job = EJob::Boarding; Start.Add(P.Key);
	}
	for (const int32 Job : Start) SvcStart(Job);
	for (auto It = SvcJobs.CreateIterator(); It; ++It) if (It.Value().Phase == 9) It.RemoveCurrent();
}

// ------------------------------------------------------------------ in and out

TFunction<void(float, float, FDriverPose&)> AGSE::SvcSeatFn(int32 Ti, int32 Seat) const
{
	const FServiceSpec& S = SvSpec(Tractors[Ti].Svc); const int32 c = Seat - 1 - S.Seats.Num();
	if (!S.Seats.IsValidIndex(Seat - 1) && !S.Cab.IsValidIndex(c)) return SeatFn(Ti);
	const FVector H = SvcP(Tractors[Ti], S.Cab.IsValidIndex(c) ? S.Cab[c].Seat : S.Seats[Seat - 1]) + SvUp * 10.f;
	return [this, Ti, H](float, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti];
		P = FDriverPose(); P.Facing = FRotator(0.f, T.Yaw, 0.f).Vector(); P.Pelvis = TW(T, H); P.Lean = -5.f;
		P.Foot[0] = TW(T, FVector(H.X + 50.f, H.Y - 11.f, T.KFloor)); P.Foot[1] = TW(T, FVector(H.X + 50.f, H.Y + 11.f, T.KFloor));
		P.bHand[0] = P.bHand[1] = true; P.Hand[0] = TW(T, H + FVector(30.f, -12.f, 4.f)); P.Hand[1] = TW(T, H + FVector(30.f, 12.f, 4.f)); P.Grip[0] = P.Grip[1] = 0.25f;
	};
}

TFunction<void(float, float, FDriverPose&)> AGSE::SvcClimbFn(int32 Ti, int32 Seat, bool bIn) const
{
	// through the crew's door: the step, up, along in front of the row to the seat, turned and down onto it
	const FTractor& T0 = Tractors[Ti]; const FServiceSpec& S = SvSpec(T0.Svc);
	const FVector Stand = SvcP(T0, S.Doors[0].Stand), Step = SvcP(T0, *(FString(TEXT("IK_Step_")) + S.Doors[0].Prefix)), H = SvcP(T0, S.Seats[Seat - 1]);
	const float Sg = Stand.Y >= 0.f ? -1.f : 1.f;   // (in: to the left from a right door)
	const int32 Hs = Sg < 0.f ? 1 : 0;              // (the hand on the opening's front edge)
	return [this, Ti, Seat, bIn, Stand, Step, H, Sg, Hs](float K0, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti]; const float K = bIn ? K0 : 1.f - K0;
		const FVector In = FRotator(0.f, T.Yaw + 90.f * Sg, 0.f).Vector(), Fwd = FRotator(0.f, T.Yaw, 0.f).Vector();
		const FVector Edge = TW(T, FVector(Step.X + 22.f, Step.Y, 150.f));
		auto Stoop = [&](const FVector& A, float Lean) { FDriverPose Q = ADPPose::Standing(TW(T, A), In, PH); Q.Pelvis.Z -= 16.f; Q.Lean = Lean; return Q; };
		auto Key = [&](int32 i)
		{
			FDriverPose Q;
			switch (i)
			{
			case 0: Q = ADPPose::Standing(TW(T, Stand), In, PH); break;
			case 1:
				Q = ADPPose::Standing(TW(T, FVector(Step.X, Step.Y - Sg * 30.f, 0.f)), In, PH); Q.Pelvis = TW(T, FVector(Step.X, Step.Y - Sg * 22.f, PH + 12.f)); Q.Lean = 14.f;
				Q.Foot[1 - Hs] = TW(T, Step + FVector(-8.f, 0.f, 0.f)); Q.Foot[Hs] = TW(T, FVector(Step.X + 10.f, Step.Y - Sg * 45.f, 0.f));
				Q.bHand[Hs] = true; Q.Hand[Hs] = Edge; Q.Grip[Hs] = 0.8f; break;
			case 2: Q = Stoop(FVector(Step.X + 4.f, Step.Y + Sg * 14.f, T.KFloor), 24.f); Q.bHand[Hs] = true; Q.Hand[Hs] = Edge; Q.Grip[Hs] = 0.6f; break;
			case 3: Q = Stoop(FVector(H.X + 59.f, (H.Y + Step.Y) * 0.5f, T.KFloor), 20.f); break;
			case 4: Q = Stoop(FVector(H.X + 52.f, H.Y, T.KFloor), 22.f); Q.Facing = (In * 0.15f + Fwd).GetSafeNormal2D(); Q.Pelvis = TW(T, FVector(H.X + 34.f, H.Y, T.KFloor + PH - 26.f)); break;
			default: SvcSeatFn(Ti, Seat)(1.f, PH, Q); break;
			}
			return Q;
		};
		const float X = K * 5.f; const int32 i = FMath::Clamp(FMath::FloorToInt(X), 0, 4);
		P = ADPPose::Lerp(Key(i), Key(i + 1), FMath::SmoothStep(0.f, 1.f, X - i));
	};
}

void AGSE::SvcDoor(int32 Ci, int32 Ti, int32 Door, bool bOpen)
{
	// the hand to the handle, the door moved (shutting: where the handle is now)
	const FServiceDoor D = SvSpec(Tractors[Ti].Svc).Doors[Door]; const FTractor& T0 = Tractors[Ti];
	const FVector Stand = SvcP(T0, D.Stand), H0 = SvcP(T0, D.Handle), Piv = SvcP(T0, D.Pivot);
	const FVector HO = D.bSlide ? H0 + FVector(-130.f, Piv.Y >= 0.f ? 12.5f : -12.5f, 0.f) : Piv + FRotator(0.f, D.Deg, 0.f).RotateVector(H0 - Piv);
	// (a slid-back door's handle is 1.3 m behind: beside it; a swung leaf's free edge: from between the two leaves)
	const FVector At = bOpen ? Stand : D.bSlide ? Stand + FVector(-105.f, 0.f, 0.f) : FVector(HO.X - 22.f, HO.Y + (Piv.Y > 0.f ? -45.f : 45.f), 0.f), Hand = bOpen ? H0 : HO;
	WalkTo(Ci, [this, Ti, At] { return TW(Tractors[Ti], At); }, 15.f);
	PoseAct(Ci, [this, Ti, At, Hand](float K, float PH, FDriverPose& P) { const FTractor& T = Tractors[Ti]; P = ADPPose::Reach(TW(T, At), SvFace(TW(T, At), TW(T, Hand)), 1, TW(T, Hand), 0.8f, PH, FMath::Sin(PI * K)); }, 1.1f);
	Do(Ci, [this, Ti, Door, bOpen] { FTractor& T = Tractors[Ti]; T.SvcDoors[Door].Want = bOpen ? 1.f : 0.f; if (bOpen) Sound(*T.SndDoorOpen, TW(T, T.SvcDoors[Door].Piv)); });
	WaitFor(Ci, [this, Ti, Door](float E) { const FTractor::FSvcDoor& X = Tractors[Ti].SvcDoors[Door]; return E > 5.f || X.Now == X.Want; });
}

void AGSE::SvcBoard(int32 Job, int32 m)
{
	FSvcMan* M = SvcM(Job, m); if (!M) return;
	const int32 Ci = M->Ci, Ti = SvcJ(Job)->Ti;
	const FVector Stand = SvcP(Tractors[Ti], SvSpec(Tractors[Ti].Svc).Doors[0].Stand);
	// one after the other, the farthest seat first; the last one shuts the door from his seat
	WaitFor(Ci, [this, Job, m](float) { return SvcAll(Job, [m](const FSvcMan& X) { return X.Seat == 0 || X.Seat >= m || X.bSeated; }); });
	if (m == 1) SvcDoor(Ci, Ti, 0, true);
	WalkTo(Ci, [this, Ti, Stand] { return TW(Tractors[Ti], Stand); }, 15.f);
	Do(Ci, [this, Ti] { Tractors[Ti].SvcDoors[0].Want = 1.f; });
	WaitFor(Ci, [this, Ti](float E) { return E > 5.f || Tractors[Ti].SvcDoors[0].Now > 0.99f; });
	PoseAct(Ci, SvcClimbFn(Ti, m, true), 5.f);
	Do(Ci, [this, Job, m, Ci, Ti]
	{
		if (ATugs* G = Tg()) G->Crew[Ci].Hold = SvcSeatFn(Ti, m);
		if (FSvcMan* X = SvcM(Job, m)) { X->bSeated = true; X->bOut = false; }
		if (SvcAll(Job, [](const FSvcMan& Y) { return Y.Seat == 0 || Y.bSeated; })) Tractors[Ti].SvcDoors[0].Want = 0.f;
	});
}

void AGSE::SvcAlight(int32 Job, int32 m)
{
	FSvcMan* M = SvcM(Job, m); if (!M) return;
	const int32 Ci = M->Ci, Ti = SvcJ(Job)->Ti;
	const FVector Off = SvcP(Tractors[Ti], SvSpec(Tractors[Ti].Svc).Doors[0].Stand);
	// the nearest to the door first
	WaitFor(Ci, [this, Job, m](float) { return SvcAll(Job, [m](const FSvcMan& X) { return X.Seat <= m || !X.bSeated; }); });
	Do(Ci, [this, Ti] { FTractor& T = Tractors[Ti]; if (T.SvcDoors[0].Want < 0.5f) Sound(*T.SndDoorOpen, TW(T, T.SvcDoors[0].Piv)); T.SvcDoors[0].Want = 1.f; });
	WaitFor(Ci, [this, Ti](float E) { return E > 4.f || Tractors[Ti].SvcDoors[0].Now > 0.99f; });
	Do(Ci, [this, Ci] { if (ATugs* G = Tg()) G->Crew[Ci].Hold = nullptr; });
	PoseAct(Ci, SvcClimbFn(Ti, m, false), 4.4f);
	WalkTo(Ci, [this, Ti, Off, m] { return TW(Tractors[Ti], Off + FVector(70.f * m, Off.Y >= 0.f ? 70.f : -70.f, 0.f)); }, 30.f);
	Do(Ci, [this, Job, m] { if (FSvcMan* X = SvcM(Job, m)) { X->bSeated = false; X->bOut = true; } });
	// (the last one out shuts it)
	if (m == 1) SvcDoor(Ci, Ti, 0, false);
}

// ------------------------------------------------------------------ there

void AGSE::SvcStart(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J) return;
	const int32 Ti = J->Ti; FTractor& T = Tractors[Ti];
	bWorked = true;
	T.Driver = J->Men[0].Ci; T.JobT = 0.f; T.StandX = J->StandX; T.Key = J->Flight.IsEmpty() ? J->Reg : J->Flight;
	ADP_LOG(TEXT("GSE: the %s to %s at stand x %.0f with %d people"), *T.Name, *T.Key, T.StandX, J->Men.Num());
	for (int32 m = 0; m < J->Men.Num(); ++m)
	{
		const int32 Ci = J->Men[m].Ci;
		HiredWay(Ci, T.At, 0.f);
		if (m > 0) { SvcBoard(Job, m); continue; }
		ClimbIn(Ci, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) X->bSeated = true; });
		WaitFor(Ci, [this, Job, Ti](float) { return SvcAll(Job, [](const FSvcMan& X) { return X.bSeated; }) && Tractors[Ti].SvcDoors[0].Now < 0.01f; });
		Do(Ci, [this, Job, Ti]
		{
			FTractor& V = Tractors[Ti]; Motor(V, true);
			if (!V.bUsed) { SvcApron(Job); return; }
			// (left somewhere else with full bags in it: to its box first, exchanged there, shut)
			V.Job = EJob::Out;
			SvcDriveHome(Job, [this, Job, Ti]
			{
				const double Go = GetWorld()->GetTimeSeconds() + 30.0;
				Tractors[Ti].Until = [this, Go] { return GetWorld()->GetTimeSeconds() > Go; };
				Tractors[Ti].AfterUntil = [this, Job, Ti] { SvcRestock(Tractors[Ti]); SvcApron(Job); };
			});
		});
	}
}

void AGSE::SvcApron(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J) return;
	const int32 Ti = J->Ti; FTractor& T = Tractors[Ti]; T.Job = EJob::Out;
	const FServiceSpec& Sv = SvSpec(J->Svc);
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(J->StandX, S, N, Sp);
	TArray<FVector> W;
	if (FVector::Dist2D(T.At, T.Home) < 200.f && FMath::Abs(FMath::FindDeltaAngleDegrees(T.Yaw, T.HomeYaw)) < 20.f) Leg(W, Sv.HomeX, Sv.HomeY, Sv.HomeHx, Sv.HomeHy, Sv.HomeX + Sv.HomeHx * 9.f, Sv.HomeY + Sv.HomeHy * 9.f, Sv.HomeHx, Sv.HomeHy, T.KTurnR);
	const float X = S + Sv.Lat, Y1 = N + Sv.Far + 6.f, Y2 = N + Sv.Aft;
	const FVector From = W.Num() ? W.Last() : T.At;
	if (!Apron(W, From, Plan(X, Y1), &T)) { RetryIn(Ti, 8.f, [this, Job] { SvcApron(Job); }); return; }
	Go(T, MoveTemp(W), false, [this, Job, Ti, S, N, X, Y1, Y2]
	{
		const FVector Nose = Plan(S, N);
		Tractors[Ti].Until = [this, Nose] { return !PlaneNear(Nose, 6000.f); };
		Tractors[Ti].AfterUntil = [this, Job, Ti, X, Y1, Y2]
		{
			TArray<FVector> W2; Leg(W2, X, Y1, 0.f, -1.f, X, Y2, 0.f, -1.f, Tractors[Ti].KTurnR);
			Go(Tractors[Ti], MoveTemp(W2), false, [this, Job] { SvcWork(Job); });
		};
	});
}

void AGSE::SvcWork(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J) return;
	J->Phase = 2; FTractor& T = Tractors[J->Ti]; T.Job = EJob::AtStand; Motor(T, false);
	J->Place = T.At; J->PlaceYaw = T.Yaw;
	if (J->bFetch)
	{
		// (fetched back after you had it: its crew go on with it)
		J->bFetch = false;
		ClimbOut(J->Men[0].Ci, J->Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) { X->bSeated = false; X->bOut = true; } Later([this, Job] { SvcReturn(Job, 0); }); });
		Wait(J->Men[0].Ci, 0.5f);
		return;
	}
	ADP_LOG(TEXT("GSE: the %s parked beside %s"), *T.Name, *T.Key);
	for (int32 m = 0; m < J->Men.Num(); ++m) if (!J->Men[m].bGone) SvcManAt(Job, m);
}

void AGSE::SvcTake(int32 Job, int32 m, bool bTake)
{
	// out of its place, or back into it: from the rear, or from inside (up over the rear step)
	FSvcMan* M = SvcM(Job, m); FSvcJob* J = SvcJ(Job); if (!M || !J) return;
	const int32 Ci = M->Ci, Ti = J->Ti; const FServiceSpec& Sv = SvSpec(J->Svc);
	const FVector Off(-420.f - 70.f * m, m % 2 ? 130.f : -130.f, 0.f);   // (clear of the back for the next one)
	if (!Sv.Take.IsValidIndex(m)) return;
	const int32 It = !bTake && Sv.Bring.IsValidIndex(m) && Sv.Bring[m] >= 0 ? Sv.Bring[m] : Sv.Take[m];
	const FServiceItem K = Sv.Items[It]; const FVector Place = Tractors[Ti].SvcItems[It].Piv, At = K.Stand, Rear(-375.f, 0.f, 0.f);
	WaitFor(Ci, [this, Job, m, bTake](float) { return SvcAll(Job, [m, bTake](const FSvcMan& X) { return X.Seat >= m || (bTake ? X.Item != INDEX_NONE : X.bStowed); }); });   // (one at a time at the back)
	auto Pass = [this, Job, m, Ci, Ti, It, bTake]
	{
		FTractor::FSvcItem& I = Tractors[Ti].SvcItems[It];
		I.Ci = bTake ? Ci : INDEX_NONE; I.bShow = true; I.bMoved = true;
		if (FSvcMan* X = SvcM(Job, m)) if (!bTake || !X->bStowed) X->Item = bTake ? It : INDEX_NONE;
	};
	auto Hand = [this, Ti, At, Place](float K2, float PH, FDriverPose& P) { const FTractor& T = Tractors[Ti]; P = ADPPose::Reach(TW(T, At), SvFace(TW(T, At), TW(T, Place)), 1, TW(T, Place), 0.8f, PH, 1.f); P.Lean = 16.f; P = ADPPose::Lerp(ADPPose::Standing(TW(T, At), SvFace(TW(T, At), TW(T, Place)), PH), P, ADPPose::Ramp(K2)); };
	if (!K.bDeep)
	{
		WalkTo(Ci, [this, Ti, At] { return TW(Tractors[Ti], At); }, 12.f);
		PoseAct(Ci, [Hand](float K2, float PH, FDriverPose& P) { Hand(K2 * 0.5f, PH, P); }, 0.9f); Do(Ci, Pass); PoseAct(Ci, [Hand](float K2, float PH, FDriverPose& P) { Hand(0.5f + K2 * 0.5f, PH, P); }, 0.8f);
	}
	else
	{
		const float Fl = Tractors[Ti].KFloor;
		auto Climb = [this, Ti, At, Rear, Fl](bool bIn)
		{
			return [this, Ti, At, Rear, Fl, bIn](float K0, float PH, FDriverPose& P)
			{
				// ground, the rear step, the load floor (bent under the door's head), along it
				const FTractor& T = Tractors[Ti]; const float X = (bIn ? K0 : 1.f - K0) * 4.f;
				const FVector Fw = FRotator(0.f, T.Yaw, 0.f).Vector();
				auto Key = [&](int32 i)
				{
					FDriverPose Q = ADPPose::Standing(TW(T, i == 0 ? Rear : i == 1 ? FVector(-345.f, 0.f, 0.f) : i == 2 ? FVector(-285.f, 0.f, Fl) : i == 3 ? FVector((At.X - 285.f) * 0.5f, At.Y * 0.5f, Fl) : At), Fw, PH);
					if (i == 1) { Q.Foot[0] = TW(T, FVector(-305.f, -10.f, 46.f)); Q.Pelvis = TW(T, FVector(-325.f, 0.f, PH + 14.f)); Q.Lean = 16.f; }
					if (i == 2) { Q.Pelvis.Z -= 14.f; Q.Lean = 22.f; Q.Foot[1] = TW(T, FVector(-305.f, 10.f, 46.f)); }
					return Q;
				};
				const int32 i = FMath::Clamp(FMath::FloorToInt(X), 0, 3);
				P = ADPPose::Lerp(Key(i), Key(i + 1), FMath::SmoothStep(0.f, 1.f, X - i));
			};
		};
		WalkTo(Ci, [this, Ti, Rear] { return TW(Tractors[Ti], Rear); }, 12.f);
		PoseAct(Ci, Climb(true), 4.f);
		PoseAct(Ci, [Hand](float K2, float PH, FDriverPose& P) { Hand(K2 * 0.5f, PH, P); }, 0.9f); Do(Ci, Pass); PoseAct(Ci, [Hand](float K2, float PH, FDriverPose& P) { Hand(0.5f + K2 * 0.5f, PH, P); }, 0.8f);
		PoseAct(Ci, Climb(false), 4.f);
	}
	if (!bTake) Do(Ci, [this, Job, m] { if (FSvcMan* X = SvcM(Job, m)) X->bStowed = true; });
	WalkTo(Ci, [this, Ti, Off] { return TW(Tractors[Ti], Off); }, 25.f);
}

void AGSE::SvcUp(int32 Job, int32 m, bool bUp)
{
	// the jetway's service stair: step by step, the left hand on the rail; its landing, the door, the tunnel, the aircraft's door
	FSvcMan* M = SvcM(Job, m); FSvcJob* J = SvcJ(Job); if (!M || !J) return;
	const int32 Ci = M->Ci, Jet = J->Jet;
	auto Rig = [this] { return AJetwayRigs::Get(GetWorld()); };
	auto StairPt = [this, Rig, Jet](bool bTop, float Out) { TArray<FVector> St; float Hw = 40.f; AJetwayRigs* R = Rig(); if (!R || !R->ServiceSteps(Jet, St, Hw)) return FVector(FVector::ZeroVector); const FVector Dn = SvFace(St.Last(), St[0]); return FVector(bTop ? St.Last() - Dn * Out : St[0] + Dn * Out); };
	auto DoorPt = [Rig, Jet](float Out) { FVector Md(0.f), O(0.f); AJetwayRigs* R = Rig(); if (R) R->ServiceDoorFrame(Jet, Md, O); return FVector(Md + O * Out); };
	auto CabPt = [Rig, Jet](float Fw) { FVector Md(0.f), F(0.f); AJetwayRigs* R = Rig(); if (R) R->CabView(Jet, Md, F); return FVector(Md + F * Fw); };
	auto Stair = [this, Ci, Jet, Rig](bool bUpward)
	{
		TArray<FVector> St0; float Hw0 = 40.f; AJetwayRigs* R0 = Rig(); const int32 Ns = R0 && R0->ServiceSteps(Jet, St0, Hw0) ? St0.Num() : 22;
		PoseAct(Ci, [this, Jet, Rig, bUpward](float K, float PH, FDriverPose& P)
		{
			TArray<FVector> St; float Hw = 40.f; AJetwayRigs* R = Rig(); if (!R || !R->ServiceSteps(Jet, St, Hw)) return;
			const int32 N = St.Num(); const FVector Dn = SvFace(St.Last(), St[0]), Face = bUpward ? -Dn : Dn;
			// (the place of step i; before the first: the ground, after the last: the landing)
			auto At = [&](int32 i) { if (i < 0) { FVector G = St[0] + Dn * (28.f * -i); G.Z = Plan(G.X / 100.f, -G.Y / 100.f).Z; return G; } return i >= N ? FVector(St[N - 1] - Dn * (28.f * (i - N + 1))) : St[i]; };
			// (one foot stands on step i, the other swings from i - 1 to i + 1; going down the same way backwards)
			const float U = (bUpward ? K : 1.f - K) * (N + 1) - 1.f; const int32 i = FMath::FloorToInt(U); const float F = U - i, Sm = FMath::SmoothStep(0.15f, 0.85f, F);
			const FVector Lt = ADPPose::LeftOf(Face); FVector Ft[2];
			for (int32 s = 0; s < 2; ++s)
			{
				const bool bStand = ((i & 1) == 0) == (s == 0);
				Ft[s] = (bStand ? At(i) : FMath::Lerp(At(i - 1), At(i + 1), Sm) + SvUp * (FMath::Sin(PI * Sm) * 9.f)) + Lt * (s == 0 ? 11.f : -11.f);
			}
			P = FDriverPose(); P.Facing = Face; P.Foot[0] = Ft[0]; P.Foot[1] = Ft[1]; P.Lean = bUpward ? 9.f : 2.f;
			const FVector Mid = FMath::Lerp(At(i), At(i + 1), F); P.Pelvis = FVector(Mid.X, Mid.Y, FMath::Min(Ft[0].Z, Ft[1].Z) + PH + 4.f);
			P.bHand[0] = true; P.Hand[0] = Mid + Lt * Hw + Face * 18.f + SvUp * 92.f; P.Grip[0] = 0.7f;   // (the right one carries)
		}, Ns * 0.62f);
	};
	// (one way at a time on it)
	auto Enter = [this, Ci, Jet](int8 Dir) { WaitFor(Ci, [this, Jet, Dir](float) { TPair<int8, int32>& U = SvcStairUse.FindOrAdd(Jet, TPair<int8, int32>(0, 0)); if (U.Value > 0 && U.Key != Dir) return false; U.Key = Dir; ++U.Value; return true; }); };
	auto Exit = [this, Ci, Jet] { Do(Ci, [this, Jet] { if (TPair<int8, int32>* U = SvcStairUse.Find(Jet)) U->Value = FMath::Max(0, U->Value - 1); }); };
	auto Level = [this, Ci](bool b) { Do(Ci, [this, Ci, b] { if (ATugs* G = Tg()) G->Crew[Ci].bUp = b; }); };
	auto Door = [this, Ci, Jet, Rig, DoorPt](bool bFromOut, bool bOpen)
	{
		PoseAct(Ci, [this, Ci, DoorPt, bFromOut](float K, float PH, FDriverPose& P)
		{
			// (standing where the walk ended, the left hand to the leaf's free side at handle height)
			const FVector H = DoorPt(bFromOut ? 6.f : -6.f), Out = (DoorPt(60.f) - DoorPt(0.f)).GetSafeNormal2D(); FVector St = DoorPt(bFromOut ? 62.f : -62.f);
			const ATugs* G = Tg(); St.Z = G && G->Crew[Ci].A.IsValid() ? G->Crew[Ci].A->GetActorLocation().Z : H.Z - 105.f;
			P = ADPPose::Reach(St, bFromOut ? -Out : Out, 0, FVector(H.X, H.Y, St.Z + 104.f), 0.7f, PH, FMath::Sin(PI * K));
		}, 0.9f);
		Do(Ci, [Rig, Jet, bOpen] { if (AJetwayRigs* R = Rig()) R->SetServiceDoor(Jet, bOpen); });
	};
	if (bUp)
	{
		Wait(Ci, 0.1f + 2.2f * m);   // (behind one another, not on one another's heels)
		Enter(1);
		WalkTo(Ci, [StairPt] { return StairPt(false, 130.f); }, 25.f); WalkTo(Ci, [StairPt] { return StairPt(false, 34.f); }, 12.f);
		Level(true); Stair(true);
		WalkTo(Ci, [StairPt] { return StairPt(true, 34.f); }, 20.f); WalkTo(Ci, [DoorPt] { return DoorPt(62.f); }, 15.f);
		Door(true, true); Wait(Ci, 1.4f);
		WalkTo(Ci, [DoorPt] { return DoorPt(-75.f); }, 20.f); Exit();
		WalkTo(Ci, [CabPt] { return CabPt(0.f); }, 40.f); WalkTo(Ci, [CabPt] { return CabPt(95.f); }, 25.f);
		// (no cabin yet, to-do 209: out of sight at the aircraft's door, as the passengers in the tunnel)
		Do(Ci, [this, Job, m, Ci] { if (ATugs* G = Tg()) if (AActor* A = G->Crew[Ci].A.Get()) A->SetActorHiddenInGame(true); if (FSvcMan* X = SvcM(Job, m)) X->bIn = true; });
		return;
	}
	Do(Ci, [this, Job, m, Ci] { if (FSvcMan* X = SvcM(Job, m)) X->bIn = false; if (ATugs* G = Tg()) if (AActor* A = G->Crew[Ci].A.Get()) A->SetActorHiddenInGame(false); });
	WalkTo(Ci, [CabPt] { return CabPt(0.f); }, 40.f); WalkTo(Ci, [DoorPt] { return DoorPt(-62.f); }, 15.f);
	Enter(2);
	Door(false, true); Wait(Ci, 1.4f);
	WalkTo(Ci, [DoorPt] { return DoorPt(62.f); }, 15.f); WalkTo(Ci, [StairPt] { return StairPt(true, 34.f); }, 12.f);
	Stair(false); Level(false); Exit();
	WalkTo(Ci, [StairPt] { return StairPt(false, 150.f); }, 25.f);
	Do(Ci, [this, Job, m, Jet, Rig] { if (FSvcMan* X = SvcM(Job, m)) X->bDown = true; if (SvcAll(Job, [](const FSvcMan& Y) { return Y.bDown; })) if (AJetwayRigs* R = Rig()) R->SetServiceDoor(Jet, false); });
}

void AGSE::SvcManAt(int32 Job, int32 m)
{
	FSvcJob* J = SvcJ(Job); FSvcMan* M = SvcM(Job, m); if (!J || !M) return;
	const int32 Ci = M->Ci, Ti = J->Ti, Jet = J->Jet; const FServiceSpec& Sv = SvSpec(J->Svc);
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(J->StandX, S, N, Sp);
	if (m == 0)
	{
		ClimbOut(Ci, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) { X->bSeated = false; X->bOut = true; } });
		WalkTo(Ci, [this, Ti] { return TW(Tractors[Ti], FVector(-345.f, -175.f, 0.f)); }, 30.f);   // (round the back's corner)
		for (int32 d = 1; d < Sv.Doors.Num(); ++d) SvcDoor(Ci, Ti, d, true);
	}
	else SvcAlight(Job, m);
	if (m > 0) WalkTo(Ci, [this, Ti, m] { return TW(Tractors[Ti], FVector(-430.f - 60.f * m, 130.f, 0.f)); }, 40.f);
	WaitFor(Ci, [this, Ti](float) { const FTractor& T = Tractors[Ti]; for (int32 d = 1; d < SvSpec(T.Svc).Doors.Num(); ++d) if (T.SvcDoors[d].Now < 0.99f) return false; return true; });
	SvcTake(Job, m, true);
	// (everyone off the aircraft, the jetway at it)
	WaitFor(Ci, [this, Job, Jet](float) { const FSvcJob* X = SvcJ(Job); AJetwayRigs* R = AJetwayRigs::Get(GetWorld()); return X && X->bGo && (!R || Jet == INDEX_NONE || R->State(Jet) == AJetwayRigs::EState::Docked); });
	const float Xw = S + Sp * 0.5f + 1.4f;   // (outside the wingtip, inside the stand's line)
	WalkTo(Ci, [this, Xw, N] { return Plan(Xw, N + 23.f); }, 50.f); WalkTo(Ci, [this, Xw, N] { return Plan(Xw, N + 6.f); }, 50.f);
	Do(Ci, [this, Job, m, Ci] { Later([this, Job, m] { SvcCabin(Job, m); }); });
	Wait(Ci, 0.5f);
}

void AGSE::SvcCabin(int32 Job, int32 m)
{
	// (planned here, at the stair's foot: the jetway stands now, so the stair has its steps)
	FSvcJob* J = SvcJ(Job); FSvcMan* M = SvcM(Job, m); if (!J || !M || M->bGone) return;
	const int32 Ci = M->Ci, Ti = J->Ti; const FServiceSpec& Sv = SvSpec(J->Svc);
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(J->StandX, S, N, Sp); const float Xw = S + Sp * 0.5f + 1.4f;
	if (Sv.bJetway) SvcUp(Job, m, true); else Do(Ci, [this, Job, m] { if (FSvcMan* X = SvcM(Job, m)) X->bIn = true; });
	WaitFor(Ci, [this, Job](float E) { const FSvcJob* X = SvcJ(Job); return !X || X->RowsDone >= X->Rows || E > 2400.f; });
	// the rubbish into bags: the full ones carried down
	Do(Ci, [this, Job, m, Ci, Ti]
	{
		const FServiceSpec& K = SvSpec(Tractors[Ti].Svc); FSvcMan* X = SvcM(Job, m);
		if (!X || !K.Bring.IsValidIndex(m) || K.Bring[m] < 0) return;
		if (X->Item != INDEX_NONE) { FTractor::FSvcItem& O = Tractors[Ti].SvcItems[X->Item]; O.Ci = INDEX_NONE; O.bShow = false; O.bMoved = true; }
		X->Item = K.Bring[m]; FTractor::FSvcItem& F = Tractors[Ti].SvcItems[X->Item]; F.Ci = Ci; F.bShow = true; F.bMoved = true; Tractors[Ti].bUsed = true;
	});
	Wait(Ci, 0.1f + 3.f * m);
	if (Sv.bJetway) SvcUp(Job, m, false); else Do(Ci, [this, Job, m] { if (FSvcMan* X = SvcM(Job, m)) { X->bIn = false; X->bDown = true; } });
	WalkTo(Ci, [this, Xw, N] { return Plan(Xw, N + 6.f); }, 50.f); WalkTo(Ci, [this, Xw, N] { return Plan(Xw, N + 23.f); }, 50.f);
	Do(Ci, [this, Job, m] { Later([this, Job, m] { SvcReturn(Job, m); }); });
	Wait(Ci, 0.5f);
}

bool AGSE::SvcHere(int32 Job) const
{
	const FSvcJob* J = SvcJobs.Find(Job); if (!J || !Tractors.IsValidIndex(J->Ti)) return false;
	const FTractor& T = Tractors[J->Ti];
	return T.Job == EJob::AtStand && !J->bFetch && !SvcYours(T) && T.Speed < 1.f && FVector::Dist2D(T.At, J->Place) < 1200.f;
}

void AGSE::SvcFetch(int32 Job)
{
	// you drove it off and left it: its driver walks to it and brings it back to its place
	FSvcJob* J = SvcJ(Job); if (!J || !J->Men.Num()) return;
	FTractor& T = Tractors[J->Ti]; const int32 Ci = J->Men[0].Ci, Ti = J->Ti;
	TArray<FVector> W;
	if (SvcHere(Job) || SvcYours(T) || T.Player.IsValid() || !Apron(W, T.At, J->Place, &T))
	{
		// (with you again, or where it cannot be driven from: asked again)
		Wait(Ci, 8.f); Do(Ci, [this, Job] { Later([this, Job] { SvcReturn(Job, 0); }); }); Wait(Ci, 0.5f); return;
	}
	J->bFetch = true; T.Job = EJob::Boarding; T.Driver = Ci;
	ADP_LOG(TEXT("GSE: the %s's driver fetches it back to stand x %.0f"), *T.Name, J->StandX);
	HiredWay(Ci, T.At, 0.f);
	ClimbIn(Ci, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) X->bSeated = true; });
	Do(Ci, [this, Job, Ti] { for (FTractor::FSvcDoor& D : Tractors[Ti].SvcDoors) D.Want = 0.f; Motor(Tractors[Ti], true); SvcApron(Job); });
}

void AGSE::SvcReturn(int32 Job, int32 m)
{
	// back at the vehicle - if it is there (you may have taken it meanwhile: they wait, the driver fetches it when you have left it)
	FSvcJob* J = SvcJ(Job); FSvcMan* M = SvcM(Job, m); if (!J || !M || M->bGone) return;
	const int32 Ci = M->Ci, Ti = J->Ti; const FServiceSpec& Sv = SvSpec(J->Svc);
	if (!SvcHere(Job))
	{
		if (m == 0) { WaitFor(Ci, [this, Job, Ti](float) { return SvcHere(Job) || (!Tractors[Ti].Player.IsValid() && Tractors[Ti].Speed < 1.f); }); Do(Ci, [this, Job] { Later([this, Job] { if (SvcHere(Job)) SvcReturn(Job, 0); else SvcFetch(Job); }); }); }
		else { WaitFor(Ci, [this, Job](float) { return SvcHere(Job); }); Do(Ci, [this, Job, m] { Later([this, Job, m] { SvcReturn(Job, m); }); }); }
		Wait(Ci, 0.5f);
		return;
	}
	// (its load doors: shut by you, or for the way back)
	if (m == 0) for (int32 d = 1; d < Sv.Doors.Num(); ++d) if (Tractors[Ti].SvcDoors[d].Want < 0.5f) SvcDoor(Ci, Ti, d, true);
	WalkTo(Ci, [this, Ti, m] { return TW(Tractors[Ti], FVector(-430.f - 60.f * m, m % 2 ? 130.f : -130.f, 0.f)); }, 40.f);
	WaitFor(Ci, [this, Ti](float E) { const FTractor& T = Tractors[Ti]; for (int32 d = 1; d < SvSpec(T.Svc).Doors.Num(); ++d) if (T.SvcDoors[d].Now < 0.99f) return E > 60.f; return true; });
	SvcTake(Job, m, false);
	if (m == 0)
	{
		WaitFor(Ci, [this, Job](float) { return SvcAll(Job, [](const FSvcMan& X) { return X.bStowed; }); });
		for (int32 d = Sv.Doors.Num() - 1; d >= 1; --d) SvcDoor(Ci, Ti, d, false);
		Do(Ci, [this, Job] { Later([this, Job] { SvcLeave(Job); }); });
		Wait(Ci, 0.5f);
	}
	else { WaitFor(Ci, [this, Job](float) { const FSvcJob* X = SvcJ(Job); return !X || X->Phase == 3; }); SvcBoard(Job, m); }
}

// ------------------------------------------------------------------ and back

void AGSE::SvcLeave(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J || J->Phase == 3) return;
	J->Phase = 3; const int32 Ti = J->Ti, Ci = J->Men[0].Ci;
	WalkTo(Ci, [this, Ti] { return TW(Tractors[Ti], FVector(-345.f, -175.f, 0.f)); }, 30.f);
	ClimbIn(Ci, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) X->bSeated = true; });
	WaitFor(Ci, [this, Job, Ti](float) { return SvcAll(Job, [](const FSvcMan& X) { return X.bSeated; }) && Tractors[Ti].SvcDoors[0].Now < 0.01f; });
	Do(Ci, [this, Job, Ti]
	{
		FSvcJob* X = SvcJ(Job); if (!X) return;
		FTractor& T = Tractors[Ti]; Motor(T, true);
		for (FTractor::FSvcDoor& D : T.SvcDoors) D.Want = 0.f;   // (a load door left open by somebody who is gone)
		// straight back behind the tail (the beeper), its back round to the side, then forward
		float S = 0.f, N = 0.f, Sp = 36.f, Yw = 0.f; StandGeo(X->StandX, S, N, Sp);
		const FServiceSpec& Sv = SvSpec(X->Svc); const FVector A = Plan(S + Sv.Lat, N + Sv.Far), B = PlanPose(S + Sv.Lat + 8.f, N + Sv.Far + 8.f, -1.f, 0.f, Yw);
		// (you left it a little off its place: straight on from there)
		if (FVector::Dist2D(T.At, Plan(S + Sv.Lat, N + Sv.Aft)) > 120.f || FMath::Abs(FMath::FindDeltaAngleDegrees(T.Yaw, X->PlaceYaw)) > 8.f) { T.Job = EJob::Back; SvcHome(Job); return; }
		TArray<FVector> R; for (int32 i = 1; i <= 30; ++i) R.Add(FMath::Lerp(T.At, A, i / 30.f));
		Dubins(R, A, T.Yaw + 180.f, B, Yw + 180.f, T.KTurnR);
		Go(T, MoveTemp(R), true, [this, Job, Ti] { Tractors[Ti].Job = EJob::Back; SvcHome(Job); });
	});
}

void AGSE::SvcDriveHome(int32 Job, TFunction<void()> Then)
{
	FSvcJob* J = SvcJ(Job); if (!J) return;
	const int32 Ti = J->Ti; FTractor& T = Tractors[Ti]; const FServiceSpec& Sv = SvSpec(J->Svc);
	// down beside its box's way (two turning circles off it, clear of the next stand's line), round to point away, straight back in: nose out
	TArray<FVector> H; const FVector2f Ho(Sv.HomeX, Sv.HomeY), Hd(Sv.HomeHx, Sv.HomeHy), Sd(Sv.HomeHy, -Sv.HomeHx);
	const FVector2f A = Ho + Sd * (2.f * Sv.TurnR / 100.f) + Hd * 15.f, In = A + Hd * 34.f, B = Ho + Hd * 15.f, C = Ho + Hd * 19.f;
	if (!Apron(H, T.At, Plan(In.X, In.Y), &T)) { RetryIn(Ti, 8.f, [this, Job, Then] { SvcDriveHome(Job, Then); }); return; }
	Leg(H, In.X, In.Y, -Hd.X, -Hd.Y, A.X, A.Y, -Hd.X, -Hd.Y, T.KTurnR);
	Leg(H, A.X, A.Y, -Hd.X, -Hd.Y, B.X, B.Y, Hd.X, Hd.Y, T.KTurnR);
	Leg(H, B.X, B.Y, Hd.X, Hd.Y, C.X, C.Y, Hd.X, Hd.Y, T.KTurnR);
	Go(T, MoveTemp(H), false, [this, Then, Ti]
	{
		FTractor& U = Tractors[Ti]; TArray<FVector> R;
		for (int32 i = 1; i <= 40; ++i) R.Add(FMath::Lerp(U.At, U.Home, i / 40.f));
		Go(U, MoveTemp(R), true, Then);
	});
}

void AGSE::SvcHome(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J) return;
	const int32 Ti = J->Ti;
	SvcDriveHome(Job, [this, Job, Ti]
		{
			FTractor& X = Tractors[Ti]; X.Job = EJob::Home; Motor(X, false); ADP_LOG(TEXT("GSE: the %s parked, switched off"), *X.Name);
			FSvcJob* J2 = SvcJ(Job); if (!J2) return;
			for (int32 m = 0; m < J2->Men.Num(); ++m)
			{
				const int32 Ci = J2->Men[m].Ci; if (J2->Men[m].bGone) continue;
				if (m > 0) { SvcAlight(Job, m); Do(Ci, [this, Job, m] { if (FSvcMan* Y = SvcM(Job, m)) Y->bGone = true; }); GiveBack(Ci); continue; }
				Wait(Ci, 2.f);
				ClimbOut(Ci, Ti, [this, Job] { if (FSvcMan* Y = SvcM(Job, 0)) { Y->bSeated = false; Y->bOut = true; } });
				WaitFor(Ci, [this, Job](float E) { return E > 120.f || SvcAll(Job, [](const FSvcMan& Y) { return Y.Seat == 0; }); });
				Do(Ci, [this, Job, Ti] { FTractor& Y = Tractors[Ti]; Y.Job = EJob::None; Y.Key.Reset(); Y.StandX = 0.f; Y.Driver = INDEX_NONE; if (FSvcJob* Z = SvcJ(Job)) Z->Phase = 9; });
				GiveBack(Ci);
			}
		});
}
