// The passenger stairs (to-do 212, docs/Hired-Crew-Plan.md): where a stand has no jetway. A driver and a helper - people you
// have hired - bring them to the aircraft's left door once it is chocked: stop short, creep in, jacks down, the upper flight
// out along its slope to the sill of the aircraft that really stands there, its gates forward by hand. People walk the real
// treads with a hand on the rail, you do on your own feet. Before off blocks everything in reverse, and home. Open or
// roofed, one per table line (GSEService.cpp), any number of them.
#include "GSEStairs.h"
#include "GSE.h"
#include "Airfield.h"
#include "ServiceVehicles.h"
#include "Tugs.h"
#include "CrewPose.h"
#include "VehicleControls.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const FVector PsUp(0, 0, 1), PsStep(28.f, 0.f, 19.f);   // (one tread up: so far goes the upper flight)
	const float PsPlat = 232.f, PsFront = 195.f, PsStroke = 14.1f, PsGate = 14.f, PsPerN = 33.84f, PsStepS = 0.62f, PsShort = 150.f;
	FQuat PsRot(const FVector& Ax, float Deg) { return FQuat(Ax, FMath::DegreesToRadians(Deg)); }
	const FServiceSpec& PsSpec(int32 i) { return ADPService::All()[i]; }

	// the door of the aircraft that stands there, from its own data (Data/aircraft-doors.json), moved with the model as it is
	// now: the file keeps the model's bounds from when its doors were measured
	struct FPsDoor { FVector Skin = FVector::ZeroVector; float Turn = 0.f; };
	bool PsDoorOf(const AActor* Pl, const TCHAR* Type, int32 Door, FPsDoor& Out)
	{
		static TSharedPtr<FJsonObject> J; static bool bRead = false;
		if (!bRead) { bRead = true; FString Text; if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data/aircraft-doors.json")))) FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J); if (!J) ADP_WARN(TEXT("GSE: Data/aircraft-doors.json cannot be read - no passenger stairs")); }
		const TSharedPtr<FJsonObject>* T = nullptr; const TArray<TSharedPtr<FJsonValue>>* Ds = nullptr;
		if (!J || !J->TryGetObjectField(Type, T) || !(*T)->TryGetArrayField(TEXT("doors"), Ds) || !Ds->IsValidIndex(Door)) return false;
		const TSharedPtr<FJsonObject> D = (*Ds)[Door]->AsObject(); const TArray<TSharedPtr<FJsonValue>>* Sk = nullptr; if (!D || !D->TryGetArrayField(TEXT("skin"), Sk) || Sk->Num() < 2) return false;
		double Sill = 0.0, Turn = 0.0; D->TryGetNumberField(TEXT("sill"), Sill); D->TryGetNumberField(TEXT("turn"), Turn);
		Out.Skin = FVector((*Sk)[0]->AsNumber(), (*Sk)[1]->AsNumber(), Sill); Out.Turn = float(Turn);
		const TSharedPtr<FJsonObject>* Bx = nullptr; const TArray<TSharedPtr<FJsonValue>> *Mn = nullptr, *Mx = nullptr;
		const ABabylonModel* M = Cast<ABabylonModel>(Pl);
		if (!M || !(*T)->TryGetObjectField(TEXT("box"), Bx) || !(*Bx)->TryGetArrayField(TEXT("min"), Mn) || !(*Bx)->TryGetArrayField(TEXT("max"), Mx) || Mn->Num() < 3 || Mx->Num() < 3) return true;
		FBox B(ForceInit); TArray<UInstancedStaticMeshComponent*> Ps; M->Parts(Ps);
		for (const UInstancedStaticMeshComponent* K : Ps) if (K && K->GetInstanceCount() > 0) B += K->CalcBounds(K->GetComponentTransform().GetRelativeTransform(Pl->GetActorTransform())).GetBox();
		if (!B.IsValid) return true;
		// (across, along, up - each as the model's own size there has changed; its wheels stay on the ground, its nose wheel at the root)
		for (int32 a = 0; a < 3; ++a)
		{
			const double Was = (*Mx)[a]->AsNumber() - (*Mn)[a]->AsNumber(), Is = B.Max[a] - B.Min[a], R = Was > 1.0 ? Is / Was : 1.0;
			if (FMath::Abs(R - 1.0) < 0.02 || R < 0.6 || R > 1.6) continue;   // (the same; or not this model)
			Out.Skin[a] *= R;
		}
		return true;
	}
}

// ------------------------------------------------------------------ its parts

void AGSE::StairsDress(FTractor& T)
{
	ABabylonModel* M = T.M.Get(); if (!M) return;
	T.Ps = MakeShared<FSvcStairs>(); FSvcStairs& F = *T.Ps;
	auto Pre = [&](const FString& N, const FVector& P) { return M->BindGroup({N}, true, FTransform(P)); };
	F.GUp = M->BindGroupWhere([](const FString& N, int32, const FVector&) { for (const TCHAR* P : {TEXT("StairUp"), TEXT("StepLights_Up"), TEXT("NamePanel_Plat")}) if (N.StartsWith(P)) return true; return false; }, FTransform::Identity);
	F.GGate = Pre(TEXT("GateSlide_"), FVector::ZeroVector); F.GJack = Pre(TEXT("Stab_"), FVector::ZeroVector);
	F.GLever[0] = Pre(TEXT("Lever_StairLever"), SvcP(T, TEXT("PIV_StairLever"))); F.GLever[1] = Pre(TEXT("Lever_LegLever"), SvcP(T, TEXT("PIV_LegLever")));
	F.GNeedle = Pre(TEXT("Needle_Speed"), SvcP(T, TEXT("PIV_Needle_Speed")));
	for (int32 s = 0; s < 2; ++s) for (int32 i = 0; i < 4; ++i) F.GLift[s][i] = Pre(FString::Printf(TEXT("Lift_%s_%d"), s ? TEXT("R") : TEXT("L"), i), SvcP(T, s ? TEXT("PIV_Lift_R") : TEXT("PIV_Lift_L")));
	for (int32 k = 2; k <= 10; ++k) F.GTread.Add(Pre(FString::Printf(TEXT("TreadLow_%02d"), k), SvcP(T, *FString::Printf(TEXT("PIV_TreadLow_%02d"), k))));
	// its own chocks and cones, each about its base
	F.GChock[0] = Pre(TEXT("Chock_0"), SvcP(T, TEXT("PIV_Chock_0"))); F.GChock[1] = Pre(TEXT("Chock_1"), SvcP(T, TEXT("PIV_Chock_1")));
	F.GCone[0] = Pre(TEXT("Cone_0"), SvcP(T, TEXT("PIV_Cone_0"))); F.GCone[1] = Pre(TEXT("Cone_1"), SvcP(T, TEXT("PIV_Cone_1")));
	if (F.GChock[0] == INDEX_NONE || F.GCone[0] == INDEX_NONE) ADP_WARN(TEXT("GSE: the %s model has no own chocks or cones (Chock_0 %d, Cone_0 %d)"), *T.Name, F.GChock[0], F.GCone[0]);
	// solid for you on foot: a ramp along each flight's nosings, the platform, its rails, the cab and the body
	auto Add = [&](bool bUp, const FVector& At, const FVector& Ext, float Pitch = 0.f)
	{
		UBoxComponent* B = NewObject<UBoxComponent>(M); B->SetupAttachment(M->GetRootComponent()); B->SetMobility(EComponentMobility::Movable); B->SetRelativeLocationAndRotation(At, FRotator(Pitch, 0.f, 0.f)); B->SetBoxExtent(Ext);
		B->SetCollisionEnabled(ECollisionEnabled::QueryOnly); B->SetCollisionObjectType(ECC_WorldDynamic); B->SetCollisionResponseToAllChannels(ECR_Ignore); B->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block); B->SetCanEverAffectNavigation(false);
		B->RegisterComponent(); M->AddInstanceComponent(B);
		if (bUp) { FSvcStairs::FSolid S; S.B = B; S.At = At; F.Up.Add(S); }
		return B;
	};
	const FVector P(-0.5615f, 0.f, 0.82748f); const float An = 34.16f;
	auto Flight = [&](bool bUp, const FVector& A, const FVector& E, float Hw)
	{
		const FVector Mid = (A + E) * 0.5f; const float Hl = float(FVector::Dist(A, E)) * 0.5f;
		Add(bUp, Mid - P * 6.f, FVector(Hl, Hw + 6.f, 6.f), An);
		for (const float s : {-1.f, 1.f}) Add(bUp, Mid + P * 46.f + FVector(0.f, s * (Hw + 3.f), 0.f), FVector(Hl - 12.f, 2.5f, 42.f), An);
	};
	Flight(false, FVector(-304.2f, 0.f, 0.f), FVector(5.f, 0.f, 209.8f), 68.f);
	Flight(true, FVector(-247.f, 0.f, 42.f), FVector(33.f, 0.f, PsPlat), 63.f);
	Add(true, FVector(114.f, 0.f, PsPlat - 6.f), FVector(81.f, 95.f, 6.f));
	for (const float s : {-1.f, 1.f}) { Add(true, FVector(114.f, s * 94.f, PsPlat + 54.f), FVector(81.f, 3.f, 54.f)); Add(true, FVector(38.f, s * 82.f, PsPlat + 54.f), FVector(4.f, 12.f, 54.f)); }
	F.Front = Add(true, FVector(193.f, 0.f, PsPlat + 60.f), FVector(3.f, 92.f, 60.f));
	// (its own cones, set out: solid for you where they stand, off until then)
	for (int32 i = 0; i < 2; ++i) { UBoxComponent* B = Add(false, SvcP(T, i ? TEXT("IK_ConePlace_1") : TEXT("IK_ConePlace_0")) + FVector(0.f, 0.f, 38.f), FVector(22.f, 22.f, 38.f)); B->SetCollisionEnabled(ECollisionEnabled::NoCollision); F.ConeBox[i] = B; }
	Add(false, FVector(212.f, 0.f, 112.f), FVector(84.f, 99.f, 92.f)); Add(false, FVector(52.f, 0.f, 75.f), FVector(76.f, 99.f, 60.f)); Add(false, FVector(-130.f, 0.f, 37.f), FVector(106.f, 99.f, 22.f));
}

bool AGSE::StairsStowed(const FTractor& T) const { return !T.Ps || (T.Ps->N <= 0.f && T.Ps->NWant <= 0.f && T.Ps->Jack <= 0.f && T.Ps->JackWant <= 0.f && T.Ps->Gate <= 0.f); }

void AGSE::StairsParts(FTractor& T, float Dt)
{
	ABabylonModel* M = T.M.Get(); if (!M || !T.Ps) return;
	FSvcStairs& F = *T.Ps;
	auto Put = [M](int32 G, const FTransform& X) { if (G != INDEX_NONE) M->MoveGroup(G, X); };
	// the cab's dial: 270 degrees for 30 km/h
	const float Kmh = FMath::Clamp(T.Speed * 0.036f, 0.f, 32.f);
	if (FMath::Abs(Kmh - F.WasSpeed) > 0.1f) { F.WasSpeed = Kmh; Put(F.GNeedle, FTransform(PsRot(FVector::ForwardVector, -9.f * Kmh), SvcP(T, TEXT("PIV_Needle_Speed")))); }
	if (T.Job == EJob::None) YouStairs(T, Dt);
	// left open by you: it stows itself, in its crew's order
	if (F.bYours) { F.GateWant = 0.f; if (F.Gate <= 0.f) F.NWant = 0.f; if (F.N <= 0.f) F.JackWant = 0.f; if (StairsStowed(T)) F.bYours = false; }
	const float WasN = F.N, WasJ = F.Jack, WasG = F.Gate;
	const float Jw = F.N > 0.f ? PsStroke : F.JackWant;
	if (F.Jack != Jw && !F.bStop && !StairsHeld(T, 0, Jw)) F.Jack = FMath::FInterpConstantTo(F.Jack, Jw, Dt, 3.f);
	// (never without its legs on the ground, never with its gates out, never under people)
	if (F.N != F.NWant && F.Jack >= PsStroke && F.Gate <= 0.f && !F.bStop && !StairsHeld(T, 1, F.NWant)) F.N = FMath::FInterpConstantTo(F.N, F.NWant, Dt, 0.4f);
	F.Gate = FMath::FInterpConstantTo(F.Gate, F.N > 0.f && F.N == F.NWant ? F.GateWant : 0.f, Dt, 12.f);
	const float Lw[2] = {F.N > WasN ? 20.f : F.N < WasN ? -20.f : 0.f, F.Jack > WasJ ? -20.f : F.Jack < WasJ ? 20.f : 0.f};
	for (int32 l = 0; l < 2; ++l) if (F.Lev[l] != Lw[l]) { F.Lev[l] = FMath::FInterpConstantTo(F.Lev[l], Lw[l], Dt, 80.f); Put(F.GLever[l], FTransform(PsRot(FVector::RightVector, F.Lev[l]), SvcP(T, l ? TEXT("PIV_LegLever") : TEXT("PIV_StairLever")))); }
	if (F.Jack != WasJ || F.bLive) Put(F.GJack, FTransform(PsUp * -F.Jack));
	if (F.Gate != WasG || F.N != WasN || F.bLive)
	{
		Put(F.GGate, FTransform(PsStep * F.N + FVector(F.Gate, 0.f, 0.f)));
		// (the front is open only with the gates out at an aircraft)
		if (UBoxComponent* B = F.Front.Get()) B->SetCollisionEnabled(F.Gate > 8.f && (T.Job != EJob::None || F.YouStandX > -1e5f) ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
	}
	if (F.N != WasN || F.bLive)
	{
		const FVector U = PsStep * F.N;
		Put(F.GUp, FTransform(U));
		for (const FSvcStairs::FSolid& S : F.Up) if (UBoxComponent* B = S.B.Get()) B->SetRelativeLocation(S.At + U);
		for (UActorComponent* C : M->GetComponents()) if (C && C->ComponentHasTag(TEXT("SvcUp"))) Cast<USceneComponent>(C)->AddRelativeLocation(PsStep * (F.N - WasN));
		// (the lower treads open as the upper flight leaves them, over its last 3 cm)
		for (int32 k = 2; k <= 10; ++k) Put(F.GTread[k - 2], FTransform(PsRot(FVector::RightVector, 34.16f * FMath::Clamp((F.N - (k - 1)) * PsPerN / 3.f + 1.f, 0.f, 1.f)), SvcP(T, *FString::Printf(TEXT("PIV_TreadLow_%02d"), k))));
		for (int32 s = 0; s < 2; ++s)
		{
			const FVector Pv = SvcP(T, s ? TEXT("PIV_Lift_R") : TEXT("PIV_Lift_L")), D0 = SvcP(T, s ? TEXT("IK_LiftTop_R") : TEXT("IK_LiftTop_L")) - Pv, D = D0 + U; const float Ex = float(D.Size() - D0.Size());
			for (int32 i = 0; i < 4; ++i) Put(F.GLift[s][i], FTransform(FQuat::FindBetweenVectors(D0, D), Pv + D.GetSafeNormal() * (Ex * i / 3.f)));
		}
	}
	// its own chocks and cones: in their trays, in a crew's hand (by their grip), in yours, or set out on the ground
	if (F.bLive || F.GearMoving())
	{
		const ATugs* GearG = Tg(); const USkeletalMeshComponent* GearB = F.GearCi != INDEX_NONE ? Body(F.GearCi) : nullptr;
		const bool bGearBody = GearB && GearG && GearG->Crew.IsValidIndex(F.GearCi) && GearG->Crew[F.GearCi].A.IsValid() && !GearG->Crew[F.GearCi].A->IsHidden();
		const FTransform GearRoot(FRotator(0.f, T.Yaw, 0.f), T.At);
		const float GearFace = bGearBody ? float(GearG->Crew[F.GearCi].A->GetActorRotation().Yaw) + 90.f - T.Yaw : 0.f;
		auto GearAt = [&](uint8 S, const TCHAR* Ik, const TCHAR* Pv, const TCHAR* Place) -> FTransform
		{
			const FVector Base = SvcP(T, Pv), Grip = SvcP(T, Ik) - Base;   // (the grip stands straight over the base)
			if (bGearBody && (S == FSvcStairs::InRight || S == FSvcStairs::InLeft))
				return FTransform(FRotator(0.f, GearFace, 0.f), GearRoot.InverseTransformPosition(GearB->GetBoneLocation(S == FSvcStairs::InRight ? TEXT("hand_r") : TEXT("hand_l"))) - Grip);
			if (S == FSvcStairs::InYours) return FTransform(T.YouHand - Grip);
			return FTransform(S == FSvcStairs::AtPlace ? SvcP(T, Place) : Base);
		};
		Put(F.GChock[0], GearAt(F.Chock[0], TEXT("IK_Chock_0"), TEXT("PIV_Chock_0"), TEXT("IK_ChockPlace_0"))); Put(F.GChock[1], GearAt(F.Chock[1], TEXT("IK_Chock_1"), TEXT("PIV_Chock_1"), TEXT("IK_ChockPlace_1")));
		Put(F.GCone[0], GearAt(F.Cone[0], TEXT("IK_Cone_0"), TEXT("PIV_Cone_0"), TEXT("IK_ConePlace_0"))); Put(F.GCone[1], GearAt(F.Cone[1], TEXT("IK_Cone_1"), TEXT("PIV_Cone_1"), TEXT("IK_ConePlace_1")));
		for (int32 i = 0; i < 2; ++i) if (UBoxComponent* B = F.ConeBox[i].Get()) B->SetCollisionEnabled(F.Cone[i] == FSvcStairs::AtPlace ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
	F.bLive = false;
}

// ------------------------------------------------------------------ the treads

void AGSE::StairsSteps(const FTractor& T, TArray<FVector>& Out) const
{
	// the feet's places from the first tread to the platform's edge as they stand now (root cm, middle)
	Out.Reset(); if (!T.Ps) return;
	const float N = T.Ps->N; const int32 Lo = FMath::FloorToInt(N), Last = Lo + (N - Lo > 0.2f ? 1 : 0);   // (the next lower one is just under the first upper one: not a step of its own)
	for (int32 k = 0; k <= FMath::Min(Last, 10); ++k) Out.Add(SvcP(T, *FString::Printf(TEXT("IK_TreadLow_%02d"), k)));
	for (int32 k = 0; k < 10; ++k) Out.Add(SvcP(T, *FString::Printf(TEXT("IK_TreadUp_%02d"), k)) + PsStep * N);
	Out.Add(FVector(51.f, 0.f, PsPlat) + PsStep * N);
}

float AGSE::StairsSeconds(int32 Ti) const { if (!Tractors.IsValidIndex(Ti) || !Tractors[Ti].Ps) return 14.f; TArray<FVector> St; StairsSteps(Tractors[Ti], St); return St.Num() * PsStepS; }
void AGSE::StairsEnter(int32 Ti, int32 By) { if (Tractors.IsValidIndex(Ti) && Tractors[Ti].Ps) { FSvcStairs& F = *Tractors[Ti].Ps; F.Users = FMath::Max(0, F.Users + By); F.UserT = 0.f; } }

void AGSE::StairsPose(int32 Ti, bool bUpward, float K, float PH, FDriverPose& P) const
{
	if (!Tractors.IsValidIndex(Ti) || !Tractors[Ti].Ps) return;
	const FTractor& T = Tractors[Ti]; const FSvcStairs& F = *T.Ps; TArray<FVector> St; StairsSteps(T, St); const int32 N = St.Num(); if (N < 3) return;
	const FVector Fw = FRotator(0.f, T.Yaw, 0.f).Vector(), Face = bUpward ? Fw : -Fw; const float Side = bUpward ? 1.f : -1.f;   // (to the right of the way: y right of the vehicle)
	auto At = [&](int32 i)
	{
		const FVector L = i < 0 ? FVector(St[0].X - 30.f * -i, 0.f, 0.f) : i >= N ? FVector(St[N - 1].X + 28.f * (i - N + 1), 0.f, St[N - 1].Z) : St[i];
		FVector W = TW(T, L + FVector(0.f, 26.f * Side, 0.f)); if (i < 0) W.Z = Plan(W.X / 100.f, -W.Y / 100.f).Z; return W;
	};
	// (one foot stands on step i, the other swings from i - 1 to i + 1; down the same way backwards)
	const float U = (bUpward ? K : 1.f - K) * (N + 1) - 1.f; const int32 i = FMath::FloorToInt(U); const float Fr = U - i, Sm = FMath::SmoothStep(0.15f, 0.85f, Fr);
	const FVector Lt = ADPPose::LeftOf(Face); FVector Ft[2];
	for (int32 s = 0; s < 2; ++s)
	{
		const bool bStand = ((i & 1) == 0) == (s == 0);
		Ft[s] = (bStand ? At(i) : FMath::Lerp(At(i - 1), At(i + 1), Sm) + PsUp * (FMath::Sin(PI * Sm) * 9.f)) + Lt * (s == 0 ? 10.f : -10.f);
	}
	P = FDriverPose(); P.Facing = Face; P.Foot[0] = Ft[0]; P.Foot[1] = Ft[1]; P.Lean = bUpward ? 9.f : 2.f;
	const FVector Mid = FMath::Lerp(At(i), At(i + 1), Fr); P.Pelvis = FVector(Mid.X, Mid.Y, FMath::Min(Ft[0].Z, Ft[1].Z) + PH + 4.f);
	// the right hand on the rail beside it: the lower tube, then from where the upper flight begins its own (they run past each other)
	const float X = float(FRotator(0.f, T.Yaw, 0.f).UnrotateVector(Mid - T.At).X) + (bUpward ? 20.f : -14.f);
	const float XUp = -247.f + 28.f * F.N, XEnd = 33.f + 28.f * F.N, Hang = FMath::SmoothStep(0.f, 14.f, X - XUp);
	if (X < -262.f || X > XEnd + 60.f) return;
	const float Z = 19.8f + (FMath::Min(X, XEnd) + 275.f) * 0.67857f + 98.5f + 3.2f * Hang, Y = FMath::Lerp(72.2f, 65.5f, Hang);
	P.bHand[1] = true; P.Hand[1] = TW(T, FVector(FMath::Min(X, XEnd + 14.f), Y * Side, Z)); P.Grip[1] = 0.7f;
}

bool AGSE::StairsReady(float StandX, int32 Door, FStairsWay& Out) const
{
	for (const TPair<int32, FSvcJob>& Pr : SvcJobs)
	{
		const FSvcJob& J = Pr.Value; if (J.Phase != 2 || J.Stage != 3 || J.CtDoor != Door || FMath::Abs(J.StandX - StandX) > 1.f || !PsSpec(J.Svc).bStairs || !Tractors.IsValidIndex(J.Ti)) continue;
		const FTractor& T = Tractors[J.Ti]; if (!T.Ps || !T.Ps->bReady) continue;
		TArray<FVector> St; StairsSteps(T, St); Out.Steps.Reset(); for (const FVector& V : St) Out.Steps.Add(TW(T, V));
		const FVector U = PsStep * T.Ps->N; Out.Ti = J.Ti; Out.Foot = TW(T, SvcP(T, TEXT("IK_Foot"))); Out.Foot.Z = Plan(Out.Foot.X / 100.f, -Out.Foot.Y / 100.f).Z;
		Out.Platform = TW(T, SvcP(T, TEXT("IK_Plat_Mid")) + U); Out.Door = TW(T, SvcP(T, TEXT("IK_Plat_Front")) + U); Out.Up = FRotator(0.f, T.Yaw, 0.f).Vector();
		return true;
	}
	// (your own, served: to-do 238)
	const int32 Y = StairsYou(StandX, Door);
	if (Tractors.IsValidIndex(Y) && Tractors[Y].Ps && Tractors[Y].Ps->bServed)
	{
		const FTractor& T = Tractors[Y];
		TArray<FVector> St; StairsSteps(T, St); Out.Steps.Reset(); for (const FVector& V : St) Out.Steps.Add(TW(T, V));
		const FVector U = PsStep * T.Ps->N; Out.Ti = Y; Out.Foot = TW(T, SvcP(T, TEXT("IK_Foot"))); Out.Foot.Z = Plan(Out.Foot.X / 100.f, -Out.Foot.Y / 100.f).Z;
		Out.Platform = TW(T, SvcP(T, TEXT("IK_Plat_Mid")) + U); Out.Door = TW(T, SvcP(T, TEXT("IK_Plat_Front")) + U); Out.Up = FRotator(0.f, T.Yaw, 0.f).Vector();
		return true;
	}
	return false;
}

// ------------------------------------------------------------------ the way in

bool AGSE::StairsHas(float StandX) const { for (const TPair<int32, FSvcJob>& P : SvcJobs) if (P.Value.Phase < 3 && P.Value.CtDoor == 0 && FMath::Abs(P.Value.StandX - StandX) < 1.f && PsSpec(P.Value.Svc).bStairs) return true; return false; }

bool AGSE::StairsBusy(float StandX, bool bLeaving, FString& Why) const
{
	for (const TPair<int32, FSvcJob>& P : SvcJobs)
	{
		const FSvcJob& J = P.Value; if (!PsSpec(J.Svc).bStairs || FMath::Abs(J.StandX - StandX) > 1.f || J.T > 2700.f) continue;
		// (yours: ready with the gates out at its door; before off blocks they have to be gone)
		const int32 Yi = J.Phase == 0 ? StairsYou(J.StandX, J.CtDoor) : INDEX_NONE;
		if (Yi != INDEX_NONE && bLeaving) { Why = TEXT("your passenger stairs still stand at its door"); return true; }
		if (Yi != INDEX_NONE && J.CtDoor == 0) { if (Tractors[Yi].Ps->bServed) continue; Why = TEXT("you bring the passenger stairs to its door"); return true; }
		if (!bLeaving)
		{
			if (J.CtDoor != 0 || J.Phase >= 3 || J.Stage >= 3) continue;
			Why = J.Phase == 0 ? FString::Printf(TEXT("the passenger stairs wait: %s"), J.Why.IsEmpty() ? TEXT("their people are being called") : *J.Why) : FString(TEXT("the passenger stairs are on their way to the door")); return true;
		}
		if ((J.Phase > 0 && J.Phase < 3) || (J.Phase == 3 && Tractors.IsValidIndex(J.Ti) && Tractors[J.Ti].Job == EJob::AtStand)) { Why = TEXT("the passenger stairs are being taken away"); return true; }
	}
	return false;
}

bool AGSE::StairsDock(float StandX, int32 Door, float GroundZ, FVector2f& At, FVector2f& Hd, float& N) const
{
	// where its middle goes and which way it faces; how many treads up (3 cm under the sill)
	const AActor* Pl = WetPlane(StandX); FPsDoor D; if (!Pl || !PsDoorOf(Pl, TEXT("A320"), Door, D)) return false;
	const float A = FMath::DegreesToRadians(D.Turn); const FVector O(-FMath::Cos(A), FMath::Sin(A), 0.f);   // (out of the skin, to the left)
	const FTransform X = Pl->GetActorTransform(); const FVector Ws = X.TransformPosition(D.Skin);
	const float Rise = float(Ws.Z) - 3.f - GroundZ - PsPlat; N = FMath::Clamp(Rise / 19.f, 0.f, 10.f);
	if (Rise < -20.f || Rise > 210.f) ADP_WARN(TEXT("GSE: a sill %.0f cm up is out of the passenger stairs' reach (%.0f .. %.0f)"), float(Ws.Z) - GroundZ, PsPlat, PsPlat + 190.f);
	const FVector Dw = X.TransformVectorNoScale(O).GetSafeNormal2D(), Wo = Ws + Dw * (PsFront + 28.f * N + 1.f);
	At = FVector2f(float(Wo.X) / 100.f, float(-Wo.Y) / 100.f); Hd = FVector2f(float(-Dw.X), float(Dw.Y)).GetSafeNormal();
	return true;
}

bool AGSE::StairsIn(FSvcJob& J, float FromY, TArray<FVector>& W)
{
	// down the lane beside the left wingtip, round in front of the wing (the rear door: behind it) square to the door, stopped short of it
	float S = 0.f, N = 0.f, Sp = 36.f, Tr = 0.f; StandGeo(J.StandX, S, N, Sp); FVector2f At, Hd;
	const FTractor& T = Tractors[J.Ti]; if (!T.Ps || !StairsDock(J.StandX, J.CtDoor, float(T.At.Z), At, Hd, Tr)) return false;
	{ const FAirfieldData::FStandFrame Fr = StandFrame(S); At = Fr.Turn(At); Hd = Fr.Way(Hd.X, Hd.Y); }   // (terminal-way, as FromY)
	const float R = T.KTurnR, X = S + PsSpec(J.Svc).InLat; const FVector2f Pre = At - Hd * (J.CtDoor ? 5.f : FMath::Max(PsShort / 100.f + 1.f, (X - At.X) - R / 100.f - 0.02f)), Sh = At - Hd * (PsShort / 100.f);
	const float Ys = Pre.Y + R / 100.f + (J.CtDoor ? 2.9f : 0.f);
	if (FromY > Ys + 0.5f) LegT(S, W, X, FromY, 0.f, -1.f, X, Ys, 0.f, -1.f, R);
	LegT(S, W, X, Ys, 0.f, -1.f, Pre.X, Pre.Y, Hd.X, Hd.Y, R * 0.98f); LegT(S, W, Pre.X, Pre.Y, Hd.X, Hd.Y, Sh.X, Sh.Y, Hd.X, Hd.Y, R);
	J.InWay = W; T.Ps->Target = Tr;
	return true;
}

bool AGSE::StairsLane(int32 Ti, float StandX) const
{
	// (nothing else moving in the lane between its wingtip and the next stand's: asked for two minutes, then the ordinary right of way)
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(StandX, S, N, Sp); const FVector Mid = PlanT(S, S + PsSpec(Tractors[Ti].Svc).InLat, N + 25.f);
	for (int32 i = 0; i < Tractors.Num(); ++i)
	{
		const FTractor& O = Tractors[i]; if (i == Ti || !O.M.IsValid()) continue;
		if (O.Path.Num() && O.Speed > 1.f && FVector::Dist2D(O.At, Mid) < 2000.f) return false;
	}
	return true;
}

// ------------------------------------------------------------------ the work

void AGSE::StairsTick(FSvcJob& J, float Dt)
{
	if (!Tractors.IsValidIndex(J.Ti) || !Tractors[J.Ti].Ps) return;
	FSvcStairs& F = *Tractors[J.Ti].Ps;
	if (J.Phase == 2 && J.Stage == 3) J.T = 1.f;   // (standing there is its work: the flight's patience counts the bringing and the taking away)
	if (F.Users > 0 && (F.UserT += Dt) > 180.f) { ADP_WARN(TEXT("GSE: %d people were counted on the passenger stairs for 3 min with nobody new - counted off"), F.Users); F.Users = 0; F.UserT = 0.f; }
	// (the aircraft gone without a word: they go home)
	const double Now = GetWorld()->GetTimeSeconds();
	if (J.Phase == 2 && !J.bGo && J.Stage == 3 && FMath::FloorToInt32(Now / 15.0) != FMath::FloorToInt32((Now - Dt) / 15.0) && !WetPlane(J.StandX)) J.bGo = true;
}

void AGSE::StairsLever(int32 Ci, int32 Ti, int32 Lever, float Tip, TFunction<void()> Fn, TFunction<bool()> Done)
{
	// from the seat: the right hand to the lever beside it, held while it works (it tips with what it moves: StairsParts)
	auto Ps = [this, Ti, Lever](float E, float PH, FDriverPose& P)
	{
		SeatFn(Ti)(1.f, PH, P); const FTractor& T = Tractors[Ti]; if (!T.Ps) return;
		const FVector Pv = SvcP(T, Lever ? TEXT("PIV_LegLever") : TEXT("PIV_StairLever")), H = TW(T, Pv + PsRot(FVector::RightVector, T.Ps->Lev[Lever]).RotateVector(SvcP(T, Lever ? TEXT("IK_LegLever") : TEXT("IK_StairLever")) - Pv));
		P.Hand[1] = P.bHand[1] ? FMath::Lerp(P.Hand[1], H, E) : H; P.bHand[1] = true; P.Grip[1] = FMath::Lerp(0.5f, 0.8f, E);
	};
	PoseAct(Ci, [Ps](float K, float PH, FDriverPose& P) { Ps(FMath::SmoothStep(0.f, 1.f, K), PH, P); }, 0.8f);
	Do(Ci, [this, Ci, Ps, Fn] { if (ATugs* G = Tg()) G->Crew[Ci].Hold = [Ps](float, float PH, FDriverPose& P) { Ps(1.f, PH, P); }; Fn(); });
	WaitFor(Ci, [Done](float E) { return E > 240.f || Done(); });
	PoseAct(Ci, [Ps](float K, float PH, FDriverPose& P) { Ps(1.f - FMath::SmoothStep(0.f, 1.f, K), PH, P); }, 0.7f);
	Do(Ci, [this, Ci, Ti] { if (ATugs* G = Tg()) G->Crew[Ci].Hold = SeatFn(Ti); });
}

void AGSE::StairsWork(int32 Job)
{
	// the driver's, from his seat (Stage: 0 stopped short, 1 in its place, 2 up, 3 open, 4 being closed, 5 gates shut and its helper off, 6 down)
	FSvcJob* J = SvcJ(Job); if (!J || J->Phase != 2 || !J->Men.Num() || !Tractors.IsValidIndex(J->Ti) || !Tractors[J->Ti].Ps) return;
	const int32 Ci = J->Men[0].Ci, Ti = J->Ti; const float Sx = J->StandX;
	auto F = [this, Ti]() -> FSvcStairs& { return *Tractors[Ti].Ps; };
	auto Stage = [this, Job](int32 n) { if (FSvcJob* X = SvcJ(Job)) X->Stage = n; Later([this, Job] { StairsWork(Job); }); };
	// (a new driver after one was lost: in first)
	if (!J->Men[0].bSeated) ClimbIn(Ci, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) X->bSeated = true; });
	switch (J->Stage)
	{
	case 0:
		F().bStop = false;   // (left pressed: pulled out)
		// chocked, engines off, nothing moving, its helper out beside it (at most four minutes)
		WaitFor(Ci, [this, Job, Sx](float E)
		{
			if (E > 240.f) return true;
			float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(Sx, S, N, Sp); const ATugs* G = ATugs::Get(GetWorld());
			return E > 2.f && !PlaneNear(PlanT(S, S, N), 1500.f) && (!G || G->ChocksIn(Sx)) && SvcAll(Job, [](const FSvcMan& X) { return X.Seat == 0 || X.bOut; });
		});
		Do(Ci, [this, Job, Ti, Stage]
		{
			FSvcJob* X = SvcJ(Job); if (!X) return;
			FTractor& T = Tractors[Ti]; FVector2f At, Hd; float Tr = 0.f, S = 0.f, N = 0.f, Sp = 36.f; StandGeo(X->StandX, S, N, Sp);
			if (X->bGo || PlaneNear(PlanT(S, S, N), 1500.f) || !StairsDock(X->StandX, X->CtDoor, float(T.At.Z), At, Hd, Tr)) { ADP_WARN(TEXT("GSE: the passenger stairs go home: %s is not standing at its door"), *X->Reg); Stage(6); return; }
			T.Ps->Target = Tr; Motor(T, true);
			TArray<FVector> W; const FVector E = Plan(At.X, At.Y); const int32 n = FMath::Max(4, FMath::CeilToInt32(FVector::Dist2D(T.At, E) / 12.f)); for (int32 i = 1; i <= n; ++i) W.Add(FMath::Lerp(T.At, E, float(i) / n));
			X->InWay.Append(W);
			Go(T, MoveTemp(W), false, [this, Job, Ti, Stage]
			{
				FSvcJob* Y = SvcJ(Job); if (!Y) return; FTractor& V = Tractors[Ti]; Motor(V, false); V.Job = EJob::AtStand; Y->Place = V.At; Y->PlaceYaw = V.Yaw;
				FVector2f At2, Hd2; float Tr2 = 0.f; if (StairsDock(Y->StandX, Y->CtDoor, float(V.At.Z), At2, Hd2, Tr2)) V.Ps->Target = Tr2;   // (the ground there)
				ADP_LOG(TEXT("GSE: the %s at %s's %s left door, %.1f treads up to its sill"), *V.Name, *Y->Reg, Y->CtDoor ? TEXT("rear") : TEXT("front"), V.Ps->Target); Stage(1);
			});
		});
		break;
	case 1:
		// (both its own chocks at the wheel before the legs go down: its helper sets them, StairsHelper - the same limit as yours)
		if (!F().Chocked() && F().Jack < PsStroke)
		{
			WaitFor(Ci, [F](float E) { return E > 240.f || F().Chocked(); });
			Do(Ci, [F, Stage] { if (!F().Chocked()) ADP_WARN(TEXT("GSE: the passenger stairs wait 4 min for their own wheel chocks - waiting on")); Stage(1); }); break;
		}
		StairsLever(Ci, Ti, 1, -20.f, [F] { F().JackWant = PsStroke; }, [F] { return F().Jack >= PsStroke; });
		Wait(Ci, 0.6f);
		StairsLever(Ci, Ti, 0, 20.f, [F] { F().NWant = F().Target; }, [F] { return F().N == F().NWant; });
		Do(Ci, [F, Stage] { Stage(F().N == F().NWant ? 2 : 1); }); break;   // (you stand on it still: again)
	case 2:   // (its helper goes up and opens the gates)
		WaitFor(Ci, [F](float E) { return F().bReady || E > 300.f; });
		Do(Ci, [this, Job, F, Stage] { FSvcStairs& X = F(); if (!X.bReady) { X.GateWant = PsGate; X.bReady = true; } if (const FSvcJob* Y = SvcJ(Job)) ADP_LOG(TEXT("GSE: the passenger stairs are ready at %s"), *Y->Reg); Stage(3); }); break;
	case 3:
		WaitFor(Ci, [this, Job](float) { const FSvcJob* X = SvcJ(Job); return !X || X->bGo; });
		Do(Ci, [F, Stage] { F().bReady = false; Stage(4); }); break;
	case 4:   // (nobody new; the last ones off; the gates shut and the helper down - at most four minutes)
		WaitFor(Ci, [F](float E) { return E > 240.f || (!F().bHelperUp && F().Gate <= 0.f && F().Users <= 0); });
		Do(Ci, [F, Stage] { F().GateWant = 0.f; Stage(5); }); break;
	case 5:
		WaitFor(Ci, [F](float E) { return E > 10.f || F().Gate <= 0.f; });
		StairsLever(Ci, Ti, 0, -20.f, [F] { F().NWant = 0.f; }, [F] { return F().N <= 0.f; });
		// (somebody on it still: it waits, it never drives off with its stairs up)
		Do(Ci, [F, Stage] { Stage(F().N <= 0.f ? 6 : 5); }); break;
	default:
		StairsLever(Ci, Ti, 1, 20.f, [F] { F().JackWant = 0.f; }, [F] { return F().Jack <= 0.f; });
		// (its helper puts its own chocks and cones back first: StairsHelper; at most four minutes, then said so and put back)
		WaitFor(Ci, [F](float E) { return E > 240.f || F().Jack > 0.f || F().GearIn(); });
		Do(Ci, [this, Job, F, Stage]
		{
			if (F().Jack > 0.f) { Stage(6); return; }
			FSvcStairs& X = F();
			if (!X.GearIn()) { for (uint8* Gs : {&X.Chock[0], &X.Chock[1], &X.Cone[0], &X.Cone[1]}) *Gs = FSvcStairs::InTray; X.GearCi = INDEX_NONE; X.bLive = true; ADP_WARN(TEXT("GSE: the passenger stairs left with their chocks or cones still out: put back in their trays")); }
			Later([this, Job] { SvcLeave(Job); });
		}); break;
	}
	Wait(Ci, 0.4f);
}

void AGSE::StairsHelper(int32 Job)
{
	FSvcJob* J = SvcJ(Job); FSvcMan* M = SvcM(Job, 1); ATugs* Gt = Tg(); if (!J || !M || !Gt || M->bGone || J->Phase < 2 || !Tractors.IsValidIndex(J->Ti) || !Tractors[J->Ti].Ps) return;
	const int32 Ci = M->Ci, Ti = J->Ti; const bool bIsUp = Gt->Crew[Ci].bUp;
	auto F = [this, Ti]() -> FSvcStairs& { return *Tractors[Ti].Ps; };
	auto At = [this, Ci, Ti](const FVector& L, float Stop) { WalkTo(Ci, [this, Ti, L] { return TW(Tractors[Ti], L); }, Stop); };
	auto On = [this, Ci, Ti, F](const TCHAR* Name, float Dy) { const FVector L = SvcP(Tractors[Ti], Name) + FVector(0.f, Dy, 0.f); WalkTo(Ci, [this, Ti, F, L] { return TW(Tractors[Ti], L + PsStep * F().N); }, 12.f); };
	auto Again = [this, Ci, Job] { Do(Ci, [this, Ci, Job] { if (ATugs* G = Tg()) G->Crew[Ci].Hold = nullptr; Later([this, Job] { StairsHelper(Job); }); }); Wait(Ci, 0.4f); };
	// its own chocks and cones, carried by hand like the refueller's and the catering truck's: out once it stands at the door
	// (before the legs go down), back once they are up; round the back of the stairs, the aircraft stands before it; what is
	// done already is left; the same limits as yours (StairsControls)
	auto Gear = [this, Ci, Ti, F, At](bool bOut)
	{
		auto Pt = [this, Ti](const TCHAR* N) { return SvcP(Tractors[Ti], N); };
		auto GHand = [this, Ci, Ti](const FVector& GSt, int32 Side, const FVector& GTo, float Dur, TFunction<void()> Fn, bool bLow)
		{
			auto GPs = [this, Ti, GSt, Side, GTo, bLow](float E, float PH, FDriverPose& P)
			{
				const FTractor& T = Tractors[Ti]; const FVector A = TW(T, GSt), Hd = TW(T, GTo), Fc = (Hd - A).GetSafeNormal2D();
				P = bLow ? ADPPose::Stoop(A, Fc, Side, Hd, 0.9f, PH, E) : ADPPose::Reach(A, Fc, Side, Hd, 0.9f, PH, E);
			};
			PoseAct(Ci, [GPs](float K, float PH, FDriverPose& P) { GPs(FMath::SmoothStep(0.f, 1.f, K), PH, P); }, Dur * 0.5f);
			Do(Ci, MoveTemp(Fn));
			PoseAct(Ci, [GPs](float K, float PH, FDriverPose& P) { GPs(1.f - FMath::SmoothStep(0.f, 1.f, K), PH, P); }, Dur * 0.5f);
		};
		auto GGo = [At](const FVector& L) { At(L, 12.f); };
		auto GSet = [F](bool bCone, int32 i, uint8 S) { return [F, bCone, i, S] { FSvcStairs& X = F(); (bCone ? X.Cone[i] : X.Chock[i]) = S; X.bLive = true; }; };
		const FSvcStairs& C = F(); const uint8 Is = bOut ? FSvcStairs::AtPlace : FSvcStairs::InTray;
		const bool bCh = C.Chock[0] != Is || C.Chock[1] != Is, bCo = C.Cone[0] != Is || C.Cone[1] != Is; if (!bCh && !bCo) return;
		const FVector GrCh[2] = {Pt(TEXT("IK_Chock_0")) - Pt(TEXT("PIV_Chock_0")), Pt(TEXT("IK_Chock_1")) - Pt(TEXT("PIV_Chock_1"))}, GrCo[2] = {Pt(TEXT("IK_Cone_0")) - Pt(TEXT("PIV_Cone_0")), Pt(TEXT("IK_Cone_1")) - Pt(TEXT("PIV_Cone_1"))};
		const FVector TrCh[2] = {Pt(TEXT("IK_Chock_0")), Pt(TEXT("IK_Chock_1"))}, TrCo[2] = {Pt(TEXT("IK_Cone_0")), Pt(TEXT("IK_Cone_1"))};
		const FVector PlCh[2] = {Pt(TEXT("IK_ChockPlace_0")) + GrCh[0], Pt(TEXT("IK_ChockPlace_1")) + GrCh[1]}, PlCo[2] = {Pt(TEXT("IK_ConePlace_0")) + GrCo[0], Pt(TEXT("IK_ConePlace_1")) + GrCo[1]};
		const FVector StCh = Pt(TEXT("IK_Stand_Chocks")), StPl = Pt(TEXT("IK_Stand_ChockPlace")), StCo = Pt(TEXT("IK_Stand_Cones"));
		const FVector StP0 = Pt(TEXT("IK_Stand_ConePlace_0")), StP1 = Pt(TEXT("IK_Stand_ConePlace_1")), BackL(-470.f, -200.f, 0.f), BackR(-470.f, 200.f, 0.f);   // (behind the stairs' foot, clear of the rear cone)
		Do(Ci, [F, Ci] { F().GearCi = Ci; });
		if (bOut)
		{
			// (the chocks first, from the left side box to the left front wheel; then the cones, over the right front fender, set out fore and aft on the left)
			if (bCh)
			{
				GGo(BackR); GGo(BackL); GGo(StCh);
				if (C.Chock[0] == FSvcStairs::InTray) GHand(StCh, 1, TrCh[0], 1.4f, GSet(false, 0, FSvcStairs::InRight), false);
				if (C.Chock[1] == FSvcStairs::InTray) GHand(StCh, 0, TrCh[1], 1.4f, GSet(false, 1, FSvcStairs::InLeft), false);
				GGo(StPl);
				if (C.Chock[0] != FSvcStairs::AtPlace) GHand(StPl, 1, PlCh[0], 1.8f, GSet(false, 0, FSvcStairs::AtPlace), true);
				if (C.Chock[1] != FSvcStairs::AtPlace) GHand(StPl, 0, PlCh[1], 1.8f, GSet(false, 1, FSvcStairs::AtPlace), true);
				if (bCo) { GGo(BackL); GGo(BackR); }
			}
			if (bCo)
			{
				GGo(StCo);
				if (C.Cone[1] == FSvcStairs::InTray) GHand(StCo, 1, TrCo[1], 1.4f, GSet(true, 1, FSvcStairs::InRight), false);
				if (C.Cone[0] == FSvcStairs::InTray) GHand(StCo, 0, TrCo[0], 1.4f, GSet(true, 0, FSvcStairs::InLeft), false);
				GGo(BackR); GGo(BackL); GGo(StP1);
				if (C.Cone[1] != FSvcStairs::AtPlace) GHand(StP1, 1, PlCo[1], 1.6f, GSet(true, 1, FSvcStairs::AtPlace), true);
				GGo(StP0);
				if (C.Cone[0] != FSvcStairs::AtPlace) GHand(StP0, 0, PlCo[0], 1.6f, GSet(true, 0, FSvcStairs::AtPlace), true);
			}
			GGo(BackL); GGo(BackR);   // (back to the right side, where the driver sees him)
		}
		else
		{
			GGo(BackR); GGo(BackL);
			if (bCh)
			{
				GGo(StPl);
				if (C.Chock[0] == FSvcStairs::AtPlace) GHand(StPl, 1, PlCh[0], 1.8f, GSet(false, 0, FSvcStairs::InRight), true);
				if (C.Chock[1] == FSvcStairs::AtPlace) GHand(StPl, 0, PlCh[1], 1.8f, GSet(false, 1, FSvcStairs::InLeft), true);
				GGo(StCh);
				if (C.Chock[0] != FSvcStairs::InTray) GHand(StCh, 1, TrCh[0], 1.4f, GSet(false, 0, FSvcStairs::InTray), false);
				if (C.Chock[1] != FSvcStairs::InTray) GHand(StCh, 0, TrCh[1], 1.4f, GSet(false, 1, FSvcStairs::InTray), false);
			}
			if (bCo)
			{
				GGo(StP0);
				if (C.Cone[0] == FSvcStairs::AtPlace) GHand(StP0, 1, PlCo[0], 1.6f, GSet(true, 0, FSvcStairs::InRight), true);
				GGo(StP1);
				if (C.Cone[1] == FSvcStairs::AtPlace) GHand(StP1, 0, PlCo[1], 1.6f, GSet(true, 1, FSvcStairs::InLeft), true);
			}
			GGo(BackL); GGo(BackR);
			if (bCo)
			{
				GGo(StCo);   // (the lower one goes on its post first, the other on top of it)
				if (C.Cone[0] != FSvcStairs::InTray) GHand(StCo, 1, TrCo[0], 1.4f, GSet(true, 0, FSvcStairs::InTray), false);
				if (C.Cone[1] != FSvcStairs::InTray) GHand(StCo, 0, TrCo[1], 1.4f, GSet(true, 1, FSvcStairs::InTray), false);
			}
		}
		Do(Ci, [F, bOut] { F().GearCi = INDEX_NONE; ADP_LOG(TEXT("GSE: the passenger stairs' chocks and cones are %s"), bOut ? TEXT("set out") : TEXT("back in their trays")); });
	};
	// the gates by hand: the left one, then the right one (one shaft)
	auto Gates = [this, Ci, Ti, F, On](bool bOpen)
	{
		for (int32 s = 0; s < 2; ++s)
		{
			const TCHAR* Sn = s ? TEXT("IK_Stand_Gate_R") : TEXT("IK_Stand_Gate_L");
			On(Sn, 0.f);
			const FVector St = SvcP(Tractors[Ti], Sn), Hn = SvcP(Tractors[Ti], s ? TEXT("IK_Handle_Gate_R") : TEXT("IK_Handle_Gate_L"));
			if (s == 0) Do(Ci, [F, bOpen] { F().GateWant = bOpen ? PsGate : 0.f; });
			PoseAct(Ci, [this, Ti, F, St, Hn, s](float K, float PH, FDriverPose& P)
			{
				const FTractor& T = Tractors[Ti]; const FVector U = PsStep * F().N, H = TW(T, Hn + U + FVector(F().Gate, 0.f, 0.f));
				P = ADPPose::Reach(TW(T, St + U), FRotator(0.f, T.Yaw, 0.f).Vector(), s ? 1 : 0, H, 0.9f, PH, FMath::SmoothStep(0.f, 0.3f, K) * (1.f - FMath::SmoothStep(0.8f, 1.f, K)));
			}, 2.6f);
		}
	};
	auto Walk = [this, Ci, Ti, F](bool bUp)
	{
		Do(Ci, [this, Ci, Ti, F] { StairsEnter(Ti, 1); if (ATugs* G = Tg()) G->Crew[Ci].bUp = true; F().bHelperUp = true; });
		PoseAct(Ci, [this, Ti, bUp](float K, float PH, FDriverPose& P) { StairsPose(Ti, bUp, K, PH, P); }, StairsSeconds(Ti));
		Do(Ci, [this, Ci, Ti, F, bUp] { StairsEnter(Ti, -1); if (!bUp) { if (ATugs* G = Tg()) G->Crew[Ci].bUp = false; F().bHelperUp = false; } });
	};
	if (bIsUp)
	{
		// the last ones off first, the gates shut, down
		WaitFor(Ci, [this, Job](float) { const FSvcJob* X = SvcJ(Job); return !X || X->Stage >= 4 || X->Phase != 2; });
		WaitFor(Ci, [F](float E) { return E > 120.f || F().Users <= 0; });
		Gates(false);
		On(TEXT("IK_Plat_Nose"), -26.f);
		Walk(false);
		At(FVector(-420.f, 60.f, 0.f), 25.f); At(FVector(-380.f, 180.f, 0.f), 40.f);
	}
	else if (J->Stage < 2)
	{
		// out, aside on the right where the driver sees him
		if (M->bSeated) SvcAlight(Job, 1); else Do(Ci, [this, Job] { if (FSvcMan* X = SvcM(Job, 1)) X->bOut = true; });
		At(FVector(60.f, 260.f, 0.f), 30.f);
		// (once it stands at the door: its own chocks and cones out, before the legs go down)
		if (J->Stage == 0) { WaitFor(Ci, [this, Job](float) { const FSvcJob* X = SvcJ(Job); return !X || X->Stage >= 1 || X->Phase != 2; }); Again(); return; }
		Gear(true);
		At(FVector(60.f, 260.f, 0.f), 30.f);
		WaitFor(Ci, [this, Job](float) { const FSvcJob* X = SvcJ(Job); return !X || X->Stage >= 2 || X->Phase != 2; });
		Again(); return;
	}
	else if (J->Stage < 4 && J->Phase == 2)
	{
		// round to the foot of the stairs, up, the gates; then as the agent beside the door, out of people's way
		At(FVector(-380.f, 180.f, 0.f), 40.f); At(SvcP(Tractors[Ti], TEXT("IK_Foot")) + FVector(-20.f, 26.f, 0.f), 12.f);
		Walk(true);
		Gates(true);
		Do(Ci, [F] { F().bReady = true; });
		On(TEXT("IK_Plat_Agent"), 0.f);
		Do(Ci, [this, Ci, Ti, F] { if (ATugs* G = Tg()) G->Crew[Ci].Hold = [this, Ti, F](float, float PH, FDriverPose& P) { const FTractor& T = Tractors[Ti]; P = ADPPose::Standing(TW(T, SvcP(T, TEXT("IK_Plat_Agent")) + PsStep * F().N), FRotator(0.f, T.Yaw - 90.f, 0.f).Vector(), PH); }; });
		WaitFor(Ci, [this, Job](float) { const FSvcJob* X = SvcJ(Job); return !X || X->Stage >= 4 || X->Phase != 2; });
		Again(); return;
	}
	else if (J->Stage == 4 && J->Phase == 2 && F().Gate > 0.f)
	{
		// (you left them open: up, the gates shut, down)
		Do(Ci, [this, Job] { if (FSvcMan* X = SvcM(Job, 1)) X->bOut = true; });
		At(FVector(-380.f, 180.f, 0.f), 40.f); At(SvcP(Tractors[Ti], TEXT("IK_Foot")) + FVector(-20.f, 26.f, 0.f), 12.f);
		Walk(true);
		Gates(false);
		On(TEXT("IK_Plat_Nose"), -26.f);
		Walk(false);
		At(FVector(-420.f, 60.f, 0.f), 25.f); At(FVector(-380.f, 180.f, 0.f), 40.f);
	}
	if (M->bSeated) return;
	// (its legs up: its own chocks and cones back in their trays, then aboard)
	if (J->Phase == 2 && J->Stage >= 6 && F().Jack <= 0.f && F().JackWant <= 0.f && !F().GearIn()) Gear(false);
	At(FVector(60.f, 260.f, 0.f), 30.f);
	if (J->Phase == 2 && !F().GearIn() && !(J->Stage >= 6 && F().Jack <= 0.f && F().JackWant <= 0.f))
	{
		WaitFor(Ci, [this, Job, F](float) { const FSvcJob* X = SvcJ(Job); return !X || X->Phase != 2 || (X->Stage >= 6 && F().Jack <= 0.f && F().JackWant <= 0.f); });
		Again(); return;
	}
	WaitFor(Ci, [this, Job](float) { const FSvcJob* X = SvcJ(Job); return !X || X->Phase == 3; });
	SvcBoard(Job, 1);
}

void AGSE::StairsLost(int32 Job, int32 m)
{
	// somebody of them vanished: whoever takes over goes on from the stage (a ground crew member: the ordinary relief)
	FSvcJob* J = SvcJ(Job); ATugs* G = Tg(); if (!J || !G) return;
	const int32 Ci = J->Men[m].Ci; ATugs::FCrew& C = G->Crew[Ci];
	if (m == 1 && Tractors.IsValidIndex(J->Ti) && Tractors[J->Ti].Ps) Tractors[J->Ti].Ps->bHelperUp = false;   // (counted as on the treads: StairsTick counts it off)
	C.bUp = false; J->Men[m].bSeated = false; C.Q.Reset(); C.Hold = nullptr;
	if (Tractors.IsValidIndex(J->Ti) && Tractors[J->Ti].Ps)
	{
		FSvcStairs& F = *Tractors[J->Ti].Ps;   // (what was in his hands: back in its tray)
		for (uint8* Gs : {&F.Chock[0], &F.Chock[1], &F.Cone[0], &F.Cone[1]}) if (*Gs == FSvcStairs::InRight || *Gs == FSvcStairs::InLeft) { *Gs = FSvcStairs::InTray; F.bLive = true; }
		if (F.GearCi == Ci) F.GearCi = INDEX_NONE;
	}
	if (J->Phase == 2) { Do(Ci, [this, Job, m] { Later([this, Job, m] { if (m == 0) StairsWork(Job); else StairsHelper(Job); }); }); Wait(Ci, 0.4f); }
	else if (m == 1 && J->Phase != 2) SvcBoard(Job, 1);
	else if (J->Phase == 3) { J->Phase = 2; J->Stage = 6; Do(Ci, [this, Job] { Later([this, Job] { StairsWork(Job); }); }); Wait(Ci, 0.4f); }
}

// ------------------------------------------------------------------ yours

namespace
{
	enum : int32 { PsMLegs, PsMStairs, PsMGates };                                                     // (motions)
	enum : int32 { PsSPlat = 1, PsSGround, PsSCab, PsSChock0 = 10, PsSChock1, PsSChockSet0, PsSChockSet1, PsSCone0, PsSCone1, PsSConeSet0, PsSConeSet1 };   // (the three stops: one circuit; its chocks and cones)
	enum : int32 { PsFChassis, PsFUp, PsFGate, PsFStair, PsFLeg, PsFChockAt0, PsFChockAt1, PsFConeAt0, PsFConeAt1 };                                        // (pieces)
	bool PsStands(const FSvcStairs& F) { return F.YouStandX > -1e5f; }
	// (a chock or cone switch: which one, a cone or not, at its place or in its tray)
	int32 PsGearIx(int32 S, bool& bCone, bool& bSet) { const int32 k = S - PsSChock0; bCone = k >= 4; bSet = k % 4 >= 2; return k % 2; }
	bool PsIsGear(int32 S) { return S >= PsSChock0 && S <= PsSConeSet1; }
}

bool AGSE::StairsHeld(const FTractor& T, int32 M, float To) const
{
	if (!T.Ps) return false; const FSvcStairs& F = *T.Ps; const UWorld* W = GetWorld();
	// never with anybody on it
	if (T.YouOn && W) T.YouStopAt = W->GetTimeSeconds();
	if (T.YouOn || F.Users > 0 || F.bHelperUp) return true;
	if (M == PsMLegs)
	{
		// (a foot under a pad; the pads' places: passengerstairs-riders.json)
		if (To <= F.Jack) return false;
		for (const TCHAR* P : {TEXT("IK_Stab_FL"), TEXT("IK_Stab_FR"), TEXT("IK_Stab_RL"), TEXT("IK_Stab_RR")}) { const FVector C = SvcP(T, P); if (CaterUnder(T, C - FVector(24.f, 24.f, 60.f), C + FVector(24.f, 24.f, 170.f))) return true; }
		return false;
	}
	// up, by you: not into the aircraft or anything else before its bumper and its rails (its crew stand where StairsDock says)
	if (M != PsMStairs || T.Job != EJob::None || To <= F.N || !W) return false;
	FCollisionQueryParams Q; Q.AddIgnoredActor(T.M.Get()); if (const APlayerController* PC = W->GetFirstPlayerController()) if (PC->GetPawn()) Q.AddIgnoredActor(PC->GetPawn());
	const FVector U = PsStep * F.N;
	for (const float Y : {-90.f, 0.f, 90.f}) for (const float Z : {-8.f, 55.f, 105.f, 200.f})
	{
		if (Z > 150.f && !PsSpec(T.Svc).bRoof) continue;
		const float X = Z < 0.f ? PsFront + 4.f : Z > 150.f ? 187.f : 178.f;   // (the bumper's, the rails' and the visor's front)
		FHitResult Hit; if (W->LineTraceSingleByChannel(Hit, TW(T, U + FVector(90.f, Y, PsPlat + Z)), TW(T, U + FVector(X, Y, PsPlat + Z + 3.f)), ECC_Pawn, Q) && !Hit.bStartPenetrating) return true;
	}
	return false;
}

int32 AGSE::StairsYou(float StandX, int32 Door) const
{
	for (int32 i = 0; i < Tractors.Num(); ++i)
	{
		const FTractor& T = Tractors[i]; if (!T.Ps || T.Job != EJob::None) continue; const FSvcStairs& F = *T.Ps;
		if ((PsStands(F) && FMath::Abs(F.YouStandX - StandX) < 1.f && (Door < 0 || F.YouDoor == Door)) || (F.bServed && FMath::Abs(F.ServedX - StandX) < 1.f && (Door < 0 || F.ServedDoor == Door))) return i;
	}
	return INDEX_NONE;
}

const AGSE::FSvcJob* AGSE::StairsJob(float StandX, int32 Door) const
{
	for (const TPair<int32, FSvcJob>& P : SvcJobs) if (P.Value.Phase == 0 && PsSpec(P.Value.Svc).bStairs && P.Value.CtDoor == Door && FMath::Abs(P.Value.StandX - StandX) < 1.f) return &P.Value;
	return nullptr;
}

void AGSE::YouStairs(FTractor& T, float Dt)
{
	FSvcStairs& F = *T.Ps; const APlayerController* PC = GetWorld()->GetFirstPlayerController(); const APawn* Me = PC ? PC->GetPawn() : nullptr;
	const bool bAway = !T.Player.IsValid() && !T.YouOn && (!Me || FVector::Dist2D(Me->GetActorLocation(), T.At) > 4000.f);
	F.LeftT = bAway ? F.LeftT + Dt : 0.f;
	if (F.Users > 0 && (F.UserT += Dt) > 180.f) { ADP_WARN(TEXT("GSE: %d people were counted on your passenger stairs for 3 min with nobody new - counted off"), F.Users); F.Users = 0; F.UserT = 0.f; }
	const bool bHome = FVector::Dist2D(T.At, T.Home) < 200.f;
	if ((F.DockT += Dt) < 1.f || (!T.Player.IsValid() && bHome && !F.bServed)) return;
	F.DockT = 0.f;
	// which aircraft's door it stands at, or how far the nearest one is (once a second, parked; only the stands round it)
	const float Was = F.YouStandX; F.YouStandX = -1e6f; F.bNear = false; double Best = 800.0, ServedD = 1e6;
	if (T.Speed < 5.f)
		for (const FAirfieldData::FStand& St : AAirfield::Shared().Stands)
		{
			if (FVector::Dist2D(Plan(St.X, St.Nose), T.At) > 4500.f) continue;
			for (int32 d = 0; d < 2; ++d)
			{
				FVector2f At, Hd; float N = 0.f; if (!StairsDock(St.X, d, float(T.At.Z), At, Hd, N)) break;   // (no aircraft there)
				const FVector Fw = FRotator(0.f, T.Yaw, 0.f).Vector(), D = Plan(At.X, At.Y) - T.At; const double Dist = D.Size2D();
				if (F.bServed && FMath::Abs(F.ServedX - St.X) < 1.f && F.ServedDoor == d) ServedD = Dist;
				if (Dist > Best) continue;
				Best = Dist; F.bNear = true; F.Ahead = float(D | Fw); F.Aside = float(D.X * -Fw.Y + D.Y * Fw.X);
				F.Turn = FMath::FindDeltaAngleDegrees(T.Yaw, FMath::RadiansToDegrees(FMath::Atan2(-Hd.Y, Hd.X)));
				// (as near as the gates reach: 5 cm too far in, 40 cm short; square to it)
				if (F.Ahead > -5.f && F.Ahead < 40.f && FMath::Abs(F.Aside) < 35.f && FMath::Abs(F.Turn) < 5.f) { F.YouStandX = St.X; F.YouDoor = d; F.YouN = N; }
				else F.YouStandX = -1e6f;
			}
		}
	else ServedD = F.bServed ? 400.0 : 1e6;
	if (PsStands(F) != (Was > -1e5f)) F.bLive = true;   // (the platform's front: open or shut)
	if (PsStands(F) && !F.bServed && F.Gate >= PsGate * 0.99f && FMath::Abs(F.N - F.YouN) <= 0.5f)
	{
		F.bServed = true; F.ServedX = F.YouStandX; F.ServedDoor = F.YouDoor; T.bUsed = true;
		ADP_LOG(TEXT("GSE: your %s is ready at the %s left door of the aircraft at stand x %.0f"), *T.Name, F.YouDoor ? TEXT("rear") : TEXT("front"), F.YouStandX);
	}
	if (F.bServed && ServedD > 800.0)
	{
		// taken away by you: done, when they were sent away (the aircraft gone without a word: its crew's job is gone with it)
		for (auto It = SvcJobs.CreateIterator(); It; ++It)
		{
			const FSvcJob& J = It.Value(); if (J.Phase != 0 || !PsSpec(J.Svc).bStairs || J.CtDoor != F.ServedDoor || FMath::Abs(J.StandX - F.ServedX) > 1.f) continue;
			if (J.bGo || !WetPlane(J.StandX)) { ADP_LOG(TEXT("GSE: %s had its passenger stairs from you"), *J.Reg); It.RemoveCurrent(); }
			break;
		}
		F.bServed = false; F.ServedX = -1e6f;
	}
}

void AGSE::StairsTake(int32 Job)
{
	// what you left at an aircraft's door: its driver and helper walk there and go on from how it stands
	FSvcJob* J = SvcJ(Job); if (!J || !Tractors.IsValidIndex(J->Ti) || !Tractors[J->Ti].Ps) return;
	FTractor& T = Tractors[J->Ti]; FSvcStairs& F = *T.Ps; float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(J->StandX, S, N, Sp);
	TArray<FVector> W;
	// (the way out again, as if they had come in)
	if (StairsIn(*J, N + PsSpec(J->Svc).Far, W) && W.Num()) { const FVector A = W.Last(); for (int32 i = 1; i <= 12; ++i) J->InWay.Add(FMath::Lerp(A, T.At, i / 12.f)); }
	else { J->InWay.Reset(); J->bGo = true; }
	T.Driver = J->Men[0].Ci; T.JobT = 0.f; T.StandX = J->StandX; T.Key = J->Flight.IsEmpty() ? J->Reg : J->Flight; bWorked = true;
	T.Job = EJob::AtStand; J->Phase = 2; J->Place = T.At; J->PlaceYaw = T.Yaw; J->Stage = !J->bGo ? 1 : F.N > 0.f || F.Gate > 0.f ? 4 : 6;
	F.bYours = F.bServed = F.bStop = F.bReady = false; F.YouStandX = F.ServedX = -1e6f; F.LeftT = 0.f; F.bLive = true;
	for (uint8* Gs : {&F.Chock[0], &F.Chock[1], &F.Cone[0], &F.Cone[1]}) if (*Gs == FSvcStairs::InYours) *Gs = FSvcStairs::InTray;   // (what you had in your hand: in its tray; what you set out, they take on from)
	ADP_LOG(TEXT("GSE: a crew go on with the %s you left at %s"), *T.Name, *J->Reg);
	for (int32 m = 0; m < J->Men.Num(); ++m) HiredWay(J->Men[m].Ci, T.At, 0.f);
	StairsWork(Job); StairsHelper(Job);
}

TSharedPtr<FVehTable> AGSE::StairsControls()
{
	using K = FVehControl;
	TSharedPtr<FVehTable> Tb = MakeShared<FVehTable>(); TArray<K>& Cs = Tb->Controls;
	auto Mo = [&Cs](const TCHAR* Part, const TCHAR* Pt, K::EKind Kind, const TCHAR* Lb, int32 Fr, const FVector& Out, int32 M, float B, bool bSeat) { K X; X.Part = Part; X.Point = Pt; X.Kind = Kind; X.Label = Lb; X.Frame = Fr; X.Out = Out; X.Motion = M; X.A = 0.f; X.B = B; X.bSeat = bSeat; X.bGrip = true; Cs.Add(X); };
	auto Sw = [&Cs](const TCHAR* Pt, int32 Fr, const FVector& Out, int32 S, bool bSeat) { K X; X.Part = TEXT(""); X.Point = Pt; X.Kind = K::Push; X.Label = TEXT("Emergency stop"); X.Frame = Fr; X.Out = Out; X.Switch = S; X.bSeat = bSeat; Cs.Add(X); };
	const FVector No = FVector::ZeroVector, R(0, 1, 0), L(0, -1, 0);
	Mo(TEXT("Lever_LegLever"), TEXT("IK_LegLever"), K::Lever, TEXT("Stabilisers down / up"), PsFLeg, No, PsMLegs, PsStroke, true);
	Mo(TEXT("Lever_StairLever"), TEXT("IK_StairLever"), K::Lever, TEXT("Stairs up / down"), PsFStair, No, PsMStairs, 10.f, true);
	Mo(TEXT("GateSlide_L"), TEXT("IK_Handle_Gate_L"), K::Hand, TEXT("Gates out / in"), PsFGate, R, PsMGates, PsGate, false);
	Mo(TEXT("GateSlide_R"), TEXT("IK_Handle_Gate_R"), K::Hand, TEXT("Gates out / in"), PsFGate, L, PsMGates, PsGate, false);
	Sw(TEXT("IK_CabEStop"), PsFChassis, No, PsSCab, true); Sw(TEXT("IK_GroundEStop"), PsFChassis, R, PsSGround, false); Sw(TEXT("IK_PlatEStop"), PsFUp, R, PsSPlat, false);
	// its own chocks and cones (to-do 238): taken from the tray or from where they stand, set out at the left front wheel and fore and aft
	auto GSw = [&Cs](const TCHAR* Part, const TCHAR* Pt, const TCHAR* Lb, int32 Fr, const FVector& Out, int32 S) { K X; X.Part = Part; X.Point = Pt; X.Kind = K::Push; X.Label = Lb; X.Frame = Fr; X.Out = Out; X.Switch = S; Cs.Add(X); };
	for (int32 i = 0; i < 2; ++i)
	{
		const TCHAR* Ch = i ? TEXT("Chock_1") : TEXT("Chock_0"); const TCHAR* ChIk = i ? TEXT("IK_Chock_1") : TEXT("IK_Chock_0"); const TCHAR* Co = i ? TEXT("Cone_1") : TEXT("Cone_0"); const TCHAR* CoIk = i ? TEXT("IK_Cone_1") : TEXT("IK_Cone_0");
		GSw(Ch, ChIk, TEXT("Wheel chock"), PsFChassis, L, PsSChock0 + i); GSw(Ch, ChIk, TEXT("Wheel chock"), PsFChockAt0 + i, L, PsSChockSet0 + i);
		GSw(Co, CoIk, TEXT("Warning cone"), PsFChassis, R, PsSCone0 + i); GSw(Co, CoIk, TEXT("Warning cone"), PsFConeAt0 + i, L, PsSConeSet0 + i);
	}
	// (the cones set out stand in your way: their own solid boxes, StairsDress / StairsParts - no table solids, its flights are solid already)

	Tb->Frame = [](const AGSE& G, const FTractor& T, int32 Fr) -> FTransform
	{
		const FSvcStairs& F = *T.Ps;
		auto Round = [&G, &T](const TCHAR* Pv, float Deg) { const FVector P = G.SvcP(T, Pv); return FTransform(FQuat::Identity, -P) * FTransform(PsRot(FVector::RightVector, Deg), P); };
		switch (Fr)
		{
		case PsFUp: return FTransform(PsStep * F.N);
		case PsFGate: return FTransform(PsStep * F.N + FVector(F.Gate, 0.f, 0.f));
		case PsFStair: return Round(TEXT("PIV_StairLever"), F.Lev[0]);
		case PsFLeg: return Round(TEXT("PIV_LegLever"), F.Lev[1]);
		case PsFChockAt0: return FTransform(G.SvcP(T, TEXT("IK_ChockPlace_0")) - G.SvcP(T, TEXT("PIV_Chock_0")));
		case PsFChockAt1: return FTransform(G.SvcP(T, TEXT("IK_ChockPlace_1")) - G.SvcP(T, TEXT("PIV_Chock_1")));
		case PsFConeAt0: return FTransform(G.SvcP(T, TEXT("IK_ConePlace_0")) - G.SvcP(T, TEXT("PIV_Cone_0")));
		case PsFConeAt1: return FTransform(G.SvcP(T, TEXT("IK_ConePlace_1")) - G.SvcP(T, TEXT("PIV_Cone_1")));
		default: return FTransform::Identity;
		}
	};
	Tb->Now = [](const FTractor& T, int32 M) { const FSvcStairs& F = *T.Ps; return M == PsMLegs ? F.Jack : M == PsMStairs ? F.N : F.Gate; };
	Tb->Want = [](FTractor& T, int32 M, float To) { FSvcStairs& F = *T.Ps; F.bYours = false; (M == PsMLegs ? F.JackWant : M == PsMStairs ? F.NWant : F.GateWant) = To; };
	Tb->End = [](const FTractor& T, int32 M, float E) { return M == PsMStairs && E > 1.f && PsStands(*T.Ps) ? FMath::Max(T.Ps->YouN, 0.01f) : E; };   // (at a door: 3 cm under its sill, as its crew)
	Tb->May = [](const AGSE& G, const FTractor& T, int32 M, float To, FString& Why)
	{
		const FSvcStairs& F = *T.Ps; const bool bOn = T.YouOn || F.Users > 0 || F.bHelperUp;
		if (T.Speed > 1.f || T.Path.Num()) Why = TEXT("it is not parked");
		else if (M != PsMGates && F.bStop) Why = TEXT("the emergency stop is pressed");
		else if (M == PsMLegs)
		{
			if (To > F.Jack && FVector::Dist2D(T.At, T.Home) < 200.f) Why = TEXT("drive it out of its box first");
			else if (To > F.Jack && !F.Chocked()) Why = TEXT("the wheel chocks first");
			else if (To < F.Jack && (F.N > 0.f || F.NWant > 0.f)) Why = TEXT("the stairs are up");
			else if (G.StairsHeld(T, M, To)) Why = bOn ? TEXT("somebody is on the stairs") : TEXT("somebody stands at a stabiliser");
		}
		else if (M == PsMStairs)
		{
			if (F.Jack < PsStroke) Why = TEXT("the stabilisers are not down");
			else if (F.Gate > 0.f || F.GateWant > 0.f) Why = TEXT("its gates are out");
			else if (G.StairsHeld(T, M, To)) Why = bOn ? TEXT("somebody is on the stairs") : TEXT("there is something in front of the platform");
		}
		else if (T.YouOn != 3) Why = TEXT("it is done from the platform");
		else if (To > F.Gate)
		{
			if (F.N <= 0.f) Why = TEXT("the stairs are down");
			else if (!PsStands(F)) Why = TEXT("it does not stand at an aircraft's door");
			else if (FMath::Abs(F.N - F.YouN) > 0.5f) Why = TEXT("the platform is not level with the sill");
		}
		else if (F.bServed) { const FSvcJob* J = G.StairsJob(F.ServedX, F.ServedDoor); if (J && !J->bGo && G.WetPlane(F.ServedX)) Why = TEXT("the aircraft's door is open, they stay until it is shut"); }
		return Why.IsEmpty();
	};
	Tb->Say = [](const AGSE&, const FTractor& T, const K& X) -> FString
	{
		const FSvcStairs& F = *T.Ps;
		if (X.Motion == PsMGates) return F.N > 0.f ? FString(X.Label) : FString();
		if (X.Motion == PsMStairs)
		{
			const float Cm = PsPlat + 19.f * F.N; const TCHAR* Dn = F.YouDoor ? TEXT("rear") : TEXT("front");
			if (PsStands(F)) return FString::Printf(TEXT("%s (platform %.2f m, at the %s left door: %.2f m)"), X.Label, Cm / 100.f, Dn, (PsPlat + 19.f * F.YouN) / 100.f);
			if (F.bNear) return FString::Printf(TEXT("%s (platform %.2f m; the door: %.1f m %s, %.1f m to the %s, turn %.0f degrees %s)"), X.Label, Cm / 100.f, FMath::Abs(F.Ahead) / 100.f, F.Ahead < 0.f ? TEXT("back") : TEXT("ahead"), FMath::Abs(F.Aside) / 100.f, F.Aside < 0.f ? TEXT("left") : TEXT("right"), FMath::Abs(F.Turn), F.Turn < 0.f ? TEXT("left") : TEXT("right"));
			return FString::Printf(TEXT("%s (platform %.2f m)"), X.Label, Cm / 100.f);
		}
		if (PsIsGear(X.Switch))
		{
			bool bCone = false, bSet = false; const int32 i = PsGearIx(X.Switch, bCone, bSet); const uint8 Gs = bCone ? F.Cone[i] : F.Chock[i];
			if (Gs == FSvcStairs::InYours) return bCone ? (bSet ? TEXT("Set the cone here") : TEXT("Put the cone back on its post")) : (bSet ? TEXT("Put the chock at the wheel") : TEXT("Put the chock back in its tray"));
			if (Gs == (bSet ? FSvcStairs::AtPlace : FSvcStairs::InTray) && !F.GearHeld() && (!bCone || bSet || i == 1 || F.Cone[1] != FSvcStairs::InTray)) return bCone ? (bSet ? TEXT("Take the cone") : TEXT("Take the warning cone")) : (bSet ? TEXT("Take the chock") : TEXT("Take the wheel chock"));
			return FString();
		}
		if (X.Motion == INDEX_NONE && F.bStop) return TEXT("Pull the emergency stop out");
		return X.Label;
	};
	Tb->MaySwitch = [](AGSE& G, int32 Ti, int32 S, FString& Why)
	{
		if (!PsIsGear(S) || !G.Tractors.IsValidIndex(Ti) || !G.Tractors[Ti].Ps) return true;
		const FTractor& T = G.Tractors[Ti]; const FSvcStairs& F = *T.Ps;
		bool bCone = false, bSet = false; const int32 i = PsGearIx(S, bCone, bSet); const uint8 Gs = bCone ? F.Cone[i] : F.Chock[i], Place = bSet ? FSvcStairs::AtPlace : FSvcStairs::InTray;
		const TCHAR* Have = F.GearHeld();
		if (T.Job != EJob::None) Why = TEXT("its crew works with it");
		else if (Gs != FSvcStairs::InYours && Have) Why = FString::Printf(TEXT("you have %s in your hand"), Have);
		else if (Gs != Place && Gs != FSvcStairs::InYours) Why = TEXT("it is not there");
		else if (!bCone && (F.Jack > 0.f || F.JackWant > 0.f) && (Gs == FSvcStairs::AtPlace || !bSet)) Why = TEXT("the stabilisers up first");
		else if (bCone && i == 0 && !bSet && Gs == FSvcStairs::InTray && F.Cone[1] == FSvcStairs::InTray) Why = TEXT("the top cone first");
		else if (bCone && i == 1 && !bSet && Gs == FSvcStairs::InYours && F.Cone[0] != FSvcStairs::InTray) Why = TEXT("the lower cone back first");
		else if (bCone && i == 0 && !bSet && Gs == FSvcStairs::InYours && F.Cone[1] == FSvcStairs::InTray) Why = TEXT("the top cone is on its post");
		return Why.IsEmpty();
	};
	Tb->Switch = [](AGSE& G, int32 Ti, int32 S)
	{
		if (PsIsGear(S))
		{
			FSvcStairs& F = *G.Tractors[Ti].Ps; bool bCone = false, bSet = false; const int32 i = PsGearIx(S, bCone, bSet); uint8& Gs = bCone ? F.Cone[i] : F.Chock[i]; const uint8 Place = bSet ? FSvcStairs::AtPlace : FSvcStairs::InTray;
			Gs = Gs == FSvcStairs::InYours ? Place : FSvcStairs::InYours; F.GearCi = INDEX_NONE; F.bLive = true;
			ADP_LOG(TEXT("GSE: passenger stairs: you %s %s %d"), Gs == FSvcStairs::InYours ? TEXT("take") : bSet ? TEXT("set out") : TEXT("put back"), bCone ? TEXT("cone") : TEXT("chock"), i);
			return true;
		}
		if (S < PsSPlat || S > PsSCab) return false;
		FSvcStairs& F = *G.Tractors[Ti].Ps; F.bStop = !F.bStop; F.bYours = false;
		if (F.bStop) { F.NWant = F.N; F.JackWant = F.Jack; }
		return true;
	};
	// your feet: 1 the lower flight, 2 the upper one, 3 the platform (solid already, where they are: StairsDress, StairsParts)
	Tb->Aboard = [](const FTractor& T, const FVector& P) -> uint8
	{
		if (!T.Ps) return 0; const float N = T.Ps->N, X = float(P.X), Zn = 19.8f + (X + 275.f) * 0.67857f;   // (both flights' nosing line)
		if (X >= 33.f + 28.f * N && X < 215.f + 28.f * N && FMath::Abs(P.Y) < 95.f && FMath::Abs(P.Z - PsPlat - 19.f * N) < 40.f) return 3;
		if (X < -300.f || X > 40.f + 28.f * N || FMath::Abs(P.Y) > 72.f || P.Z < Zn - 25.f || P.Z > Zn + 60.f) return 0;
		return X < -247.f + 28.f * N ? 1 : 2;
	};
	Tb->Stowed = [](const AGSE& G, const FTractor& T) { return G.StairsStowed(T) && !T.Ps->bStop && T.Ps->GearIn(); };
	// what you carry of its own: a chock or a cone, 30 m from it at most; let go, it goes back in its tray
	Tb->Held = [](const FTractor& T) -> const TCHAR* { return T.Ps && T.Job == EJob::None ? T.Ps->GearHeld() : nullptr; };
	Tb->Leash = [](const AGSE& G, const FTractor& T, FVector& From) { if (!T.Ps || !T.Ps->GearHeld()) return 0.f; From = G.SvcP(T, TEXT("IK_Stand_Chocks")); return 3000.f; };
	Tb->Drop = [](FTractor& T) { if (T.Ps) for (uint8* Gs : {&T.Ps->Chock[0], &T.Ps->Chock[1], &T.Ps->Cone[0], &T.Ps->Cone[1]}) if (*Gs == FSvcStairs::InYours) { *Gs = FSvcStairs::InTray; T.Ps->bLive = true; } };
	// left alone: at an aircraft that has asked for stairs its crew go on with it (StairsTake), and at an open door it stays
	Tb->Stow = [](AGSE& G, FTractor& T)
	{
		FSvcStairs& F = *T.Ps; if ((PsStands(F) && G.StairsJob(F.YouStandX, F.YouDoor)) || F.Users > 0) return;
		F.bStop = false; F.bYours = !G.StairsStowed(T);
		if (!F.GearIn()) { for (uint8* Gs : {&F.Chock[0], &F.Chock[1], &F.Cone[0], &F.Cone[1]}) *Gs = FSvcStairs::InTray; F.bLive = true; ADP_LOG(TEXT("GSE: your %s's chocks and cones back in their trays"), *T.Name); }
	};
	return Tb;
}
