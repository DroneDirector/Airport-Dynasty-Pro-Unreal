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
#include "ADPLog.h"
#include "BabylonModel.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
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
	// yours at its panel: the same order by itself (legs, stairs, gates - and back)
	if (F.bYours)
	{
		if (F.YourN > 0.f) { F.JackWant = PsStroke; if (F.Jack >= PsStroke) F.NWant = F.YourN; F.GateWant = F.N >= F.YourN && F.bGatesWant ? PsGate : 0.f; }
		else { F.GateWant = 0.f; if (F.Gate <= 0.f) F.NWant = 0.f; if (F.N <= 0.f) F.JackWant = 0.f; if (StairsStowed(T)) F.bYours = false; }
	}
	const float WasN = F.N, WasJ = F.Jack, WasG = F.Gate;
	F.Jack = FMath::FInterpConstantTo(F.Jack, F.JackWant > 0.f || F.N > 0.f ? PsStroke : 0.f, Dt, 3.f);
	// (never without its legs on the ground, never with its gates out, never under people)
	if (F.Jack >= PsStroke && F.Gate <= 0.f && F.Users <= 0 && !F.bHelperUp) F.N = FMath::FInterpConstantTo(F.N, F.NWant, Dt, 0.4f);
	F.Gate = FMath::FInterpConstantTo(F.Gate, F.N > 0.f && F.N == F.NWant ? F.GateWant : 0.f, Dt, 12.f);
	const float Lw[2] = {F.N > WasN ? 20.f : F.N < WasN ? -20.f : 0.f, F.Jack > WasJ ? -20.f : F.Jack < WasJ ? 20.f : 0.f};
	for (int32 l = 0; l < 2; ++l) if (F.Lev[l] != Lw[l]) { F.Lev[l] = FMath::FInterpConstantTo(F.Lev[l], Lw[l], Dt, 80.f); Put(F.GLever[l], FTransform(PsRot(FVector::RightVector, F.Lev[l]), SvcP(T, l ? TEXT("PIV_LegLever") : TEXT("PIV_StairLever")))); }
	if (F.Jack != WasJ || F.bLive) Put(F.GJack, FTransform(PsUp * -F.Jack));
	if (F.Gate != WasG || F.N != WasN || F.bLive)
	{
		Put(F.GGate, FTransform(PsStep * F.N + FVector(F.Gate, 0.f, 0.f)));
		// (the front is open only with the gates out at an aircraft)
		if (UBoxComponent* B = F.Front.Get()) B->SetCollisionEnabled(F.Gate > 8.f ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
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
	return false;
}

// ------------------------------------------------------------------ the way in

bool AGSE::StairsHas(float StandX) const { for (const TPair<int32, FSvcJob>& P : SvcJobs) if (P.Value.Phase < 3 && P.Value.CtDoor == 0 && FMath::Abs(P.Value.StandX - StandX) < 1.f && PsSpec(P.Value.Svc).bStairs) return true; return false; }

bool AGSE::StairsBusy(float StandX, bool bLeaving, FString& Why) const
{
	for (const TPair<int32, FSvcJob>& P : SvcJobs)
	{
		const FSvcJob& J = P.Value; if (!PsSpec(J.Svc).bStairs || FMath::Abs(J.StandX - StandX) > 1.f || J.T > 2700.f) continue;
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
	const float R = T.KTurnR, X = S + PsSpec(J.Svc).InLat; const FVector2f Pre = At - Hd * (J.CtDoor ? 5.f : FMath::Max(PsShort / 100.f + 1.f, (X - At.X) - R / 100.f - 0.02f)), Sh = At - Hd * (PsShort / 100.f);
	const float Ys = Pre.Y + R / 100.f + (J.CtDoor ? 2.9f : 0.f);
	if (FromY > Ys + 0.5f) Leg(W, X, FromY, 0.f, -1.f, X, Ys, 0.f, -1.f, R);
	Leg(W, X, Ys, 0.f, -1.f, Pre.X, Pre.Y, Hd.X, Hd.Y, R * 0.98f); Leg(W, Pre.X, Pre.Y, Hd.X, Hd.Y, Sh.X, Sh.Y, Hd.X, Hd.Y, R);
	J.InWay = W; T.Ps->Target = Tr;
	return true;
}

bool AGSE::StairsLane(int32 Ti, float StandX) const
{
	// (nothing else moving in the lane between its wingtip and the next stand's: asked for two minutes, then the ordinary right of way)
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(StandX, S, N, Sp); const FVector Mid = Plan(S + PsSpec(Tractors[Ti].Svc).InLat, N + 25.f);
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
		// chocked, engines off, nothing moving, its helper out beside it (at most four minutes)
		WaitFor(Ci, [this, Job, Sx](float E)
		{
			if (E > 240.f) return true;
			float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(Sx, S, N, Sp); const ATugs* G = ATugs::Get(GetWorld());
			return E > 2.f && !PlaneNear(Plan(S, N), 1500.f) && (!G || G->ChocksIn(Sx)) && SvcAll(Job, [](const FSvcMan& X) { return X.Seat == 0 || X.bOut; });
		});
		Do(Ci, [this, Job, Ti, Stage]
		{
			FSvcJob* X = SvcJ(Job); if (!X) return;
			FTractor& T = Tractors[Ti]; FVector2f At, Hd; float Tr = 0.f, S = 0.f, N = 0.f, Sp = 36.f; StandGeo(X->StandX, S, N, Sp);
			if (X->bGo || PlaneNear(Plan(S, N), 1500.f) || !StairsDock(X->StandX, X->CtDoor, float(T.At.Z), At, Hd, Tr)) { ADP_WARN(TEXT("GSE: the passenger stairs go home: %s is not standing at its door"), *X->Reg); Stage(6); return; }
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
		StairsLever(Ci, Ti, 1, -20.f, [F] { F().JackWant = PsStroke; }, [F] { return F().Jack >= PsStroke; });
		Wait(Ci, 0.6f);
		StairsLever(Ci, Ti, 0, 20.f, [F] { F().NWant = F().Target; }, [F] { return F().N == F().NWant; });
		Do(Ci, [Stage] { Stage(2); }); break;
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
		Do(Ci, [this, Job] { Later([this, Job] { SvcLeave(Job); }); }); break;
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
	if (M->bSeated) return;
	At(FVector(60.f, 260.f, 0.f), 30.f);
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
	if (J->Phase == 2) { Do(Ci, [this, Job, m] { Later([this, Job, m] { if (m == 0) StairsWork(Job); else StairsHelper(Job); }); }); Wait(Ci, 0.4f); }
	else if (m == 1 && J->Phase != 2) SvcBoard(Job, 1);
	else if (J->Phase == 3) { J->Phase = 2; J->Stage = 6; Do(Ci, [this, Job] { Later([this, Job] { StairsWork(Job); }); }); Wait(Ci, 0.4f); }
}

// ------------------------------------------------------------------ yours

void AGSE::StairsToggle(int32 Ti, FString& Say)
{
	// E at its ground panel: legs down and up to the aircraft in front of it (none near: 3.5 m), or all the way down again
	if (!Tractors.IsValidIndex(Ti) || !Tractors[Ti].Ps) return;
	FTractor& T = Tractors[Ti]; FSvcStairs& F = *T.Ps;
	if (T.Job != EJob::None) { Say = TEXT("Its crew are at work"); return; }
	if (T.Player.IsValid() || T.Speed > 1.f) { Say = TEXT("Somebody is driving it"); return; }
	if (F.bYours && F.YourN > 0.f) { F.YourN = 0.f; F.bGatesWant = false; return; }
	if (FVector::Dist2D(T.At, T.Home) < 200.f) { Say = TEXT("Drive it out of its box first"); return; }
	F.YourN = (350.f - PsPlat) / 19.f; F.bGatesWant = false;
	for (const FAirfieldData::FStand& St : AAirfield::Shared().Stands)
	{
		if (FVector::Dist2D(Plan(St.X, St.Nose), T.At) > 6000.f) continue;
		for (int32 d = 0; d < 2; ++d)
		{
			FVector2f At, Hd; float N = 0.f; if (!StairsDock(St.X, d, float(T.At.Z), At, Hd, N)) continue;
			const FVector2f Me(float(T.At.X) / 100.f, float(-T.At.Y) / 100.f); if (FVector2f::Distance(Me, At) > 1.2f) continue;
			F.YourN = FMath::Max(0.01f, N); F.bGatesWant = FVector2f::Distance(Me, At) < 0.5f;
		}
	}
	F.bYours = true;
}
