#include "JetwayRigs.h"
#include "BabylonModel.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Components/InstancedStaticMeshComponent.h"

float AJetwayRigs::Speed = 1.f;

namespace ADPJetwayInternal
{
	// his generator's measures (jetway.ts), metres
	constexpr float RR = 1.8f, CR = 1.7f, BD = 1.25f, OV = 1.5f;
	constexpr float SEC1 = 10.5f, SEC2 = 10.1f, SEC3 = 9.7f, ZW = SEC3 - 1.0f;
	constexpr float LMIN = SEC1 + 2 * BD, LMAX = SEC1 + SEC2 + SEC3 - 2 * OV + 2 * BD;
	constexpr float PARK = 0.5f;                                               // at rest: wheels half as far out (his rule)
	constexpr float TDrive = 75.f, TCrawl = 30.f, TCanopy = 15.f, TBack = 20.f, THome = 60.f;   // JW_TIMES
	float Ease(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3 - 2 * T); }
	FVector Flat(const FVector& V) { return FVector(V.X, V.Y, 0.f); }
	// a direction turned by A the way his generator turns (its yaw: atan2(x, z) in Babylon, x and y here)
	FVector TurnBab(const FVector& V, float A) { const float Y = FMath::Atan2(V.X, V.Y) + A; return FVector(FMath::Sin(Y), FMath::Cos(Y), 0.f); }
	struct FIk { FVector Dir, Axis; float Pitch = 0.f, Len = 0.f; };
	FIk Ik(const FVector& BoomO, const FVector& W)
	{
		FIk K; const FVector H = Flat(W - BoomO); K.Dir = H.GetSafeNormal();
		const float Dh = H.Size() - (RR - 0.25f) * 100.f, Dz = BoomO.Z - W.Z;
		K.Pitch = FMath::Atan2(Dz, Dh);
		K.Len = FMath::Sqrt(Dh * Dh + Dz * Dz) / 100.f + BD + SEC3 - ZW;
		K.Axis = (K.Dir * FMath::Cos(K.Pitch) - FVector::UpVector * FMath::Sin(K.Pitch)).GetSafeNormal();
		return K;
	}
	// the sections' places along the tunnel (m from the pitch joint), as his setPose
	void Slides(float Len, float& Z1, float& Z2, float& Z3)
	{
		Len = FMath::Clamp(Len, LMIN, LMAX);
		Z1 = BD; Z3 = Len - BD - SEC3;
		const float Lo = FMath::Max(Z1, Z3 + OV - SEC2), Hi = FMath::Min3(Z1 + SEC1 - OV, Z3, Z3 + SEC3 - SEC2);
		Z2 = Lo <= Hi ? (Lo + Hi) / 2 : (Z1 + Z3) / 2;
	}
}
using ADPJetwayInternal::FIk;

AJetwayRigs::AJetwayRigs()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

AJetwayRigs* AJetwayRigs::Get(UWorld* W)
{
	if (!W) return nullptr;
	for (TActorIterator<AJetwayRigs> It(W); It; ++It) return *It;
	return W->SpawnActor<AJetwayRigs>();
}

void AJetwayRigs::Build()
{
	using namespace ADPJetwayInternal;
	ABabylonModel* M = nullptr;
	for (TActorIterator<ABabylonModel> It(GetWorld()); It; ++It) if (It->Model == TEXT("Jetways") && It->NodeCount() > 0) { M = *It; break; }
	if (!M) return;
	bBuilt = true;
	const TCHAR* Names[NumGroups] = {TEXT("ROTUNDA-SLEEVE"), nullptr, TEXT("TUNNEL-1"), TEXT("TUNNEL-2"), TEXT("TUNNEL-3"), TEXT("CAB"), TEXT("CAB-SLEEVE"), TEXT("DRIVE-UNIT"), TEXT("LIFT-SLEEVES")};
	for (int32 n = 1; n <= 12; ++n)
	{
		const FString Id = FString::Printf(TEXT("J-%d"), n);
		int32 Root[NumGroups];
		for (int32 g = 0; g < NumGroups; ++g) Root[g] = Names[g] ? M->NodeIndex(Id + TEXT("/") + Names[g]) : INDEX_NONE;
		if (Root[Boom] == INDEX_NONE || Root[Tun1] == INDEX_NONE || Root[Tun3] == INDEX_NONE || Root[Cab] == INDEX_NONE) continue;
		// the pitch joint: the tunnel's first section's parent
		for (int32 i = 0; i < M->NodeCount() && Root[Pitch] == INDEX_NONE; ++i) if (M->NodeKids(i).Contains(Root[Tun1])) Root[Pitch] = i;
		FRig R; R.Id = Id; R.Model = M;
		// each part goes with the nearest moving part above it (a section's parts with the section, the cab's with the cab)
		TMap<int32, int32> PartGroup;
		TFunction<void(int32, int32)> Take = [&](int32 Node, int32 G)
		{
			for (int32 g = 0; g < NumGroups; ++g) if (Root[g] == Node && g != G) return;   // (another moving part's own)
			PartGroup.Add(Node, G);
			for (const int32 K : M->NodeKids(Node)) Take(K, G);
		};
		for (int32 g = 0; g < NumGroups; ++g) if (Root[g] != INDEX_NONE) { PartGroup.Add(Root[g], g); for (const int32 K : M->NodeKids(Root[g])) Take(K, g); }
		// the bellows follow the joints they hang on (squeezed and stretched later: plan part 3)
		auto Follow = [&](const TCHAR* Part, int32 G) { const int32 I = M->NodeIndex(Id + TEXT("/") + Part); if (I != INDEX_NONE) { PartGroup.Add(I, G); for (const int32 K : M->NodeKids(I)) Take(K, G); } };
		Follow(TEXT("BELLOWS-ROTUNDA"), Boom); Follow(TEXT("BELLOWS-ROTUNDA-LINING"), Boom);
		Follow(TEXT("BELLOWS-CAB"), CabSleeve); Follow(TEXT("BELLOWS-CAB-LINING"), CabSleeve);
		Follow(TEXT("LIFT-CABLE"), Sleeves);
		for (int32 g = 0; g < NumGroups; ++g) if (Root[g] != INDEX_NONE) R.G[g].Docked = M->NodeModel(Root[g]);
		int32 Parts = 0;
		for (const TPair<int32, int32>& O : PartGroup)
		{
			const TPair<UInstancedStaticMeshComponent*, int32> In = M->NodeInstance(O.Key);
			if (!In.Key || In.Value == INDEX_NONE) continue;
			FPart P; P.K = In.Key; P.Ix = In.Value; In.Key->GetInstanceTransform(In.Value, P.Docked, false);
			R.G[O.Value].Parts.Add(P); ++Parts;
		}
		// the docked geometry, read from his export
		R.BoomO = R.G[Boom].Docked.GetLocation();
		R.Wd = R.G[Tun3].Docked.TransformPosition(FVector(0.f, ZW * 100.f, 0.f));
		const FIk D = ADPJetwayInternal::Ik(R.BoomO, R.Wd);
		R.DirD = D.Dir; R.AxisD = D.Axis; R.LenD = D.Len; R.PitchD = D.Pitch;
		R.PD = R.BoomO + R.DirD * (RR - 0.25f) * 100.f;
		R.CabOff = R.G[Cab].Docked.GetLocation() - (R.PD + R.AxisD * R.LenD * 100.f + R.DirD * (CR - 0.25f) * 100.f);
		const int32 Con = M->NodeIndex(Id + TEXT("/CAB-CONSOLE"));
		R.Ctl = Con != INDEX_NONE ? M->NodeModel(Con).GetLocation() : R.G[Cab].Docked.GetLocation();
		// at rest: pulled half way back along the line to the door, a little lower (his PARK), never shorter than it can be
		R.Wp = R.BoomO + R.DirD * Flat(R.Wd - R.BoomO).Size() * PARK; R.Wp.Z = R.Wd.Z - 35.f;
		{ const FIk Q = ADPJetwayInternal::Ik(R.BoomO, R.Wp); if (Q.Len < LMIN + 0.2f) R.Wp += Q.Dir * (LMIN + 0.2f - Q.Len) * 100.f; }
		// the cab faces along the tunnel on the way and turns square to the fuselage in the final crawl
		{ const float TunYaw = FMath::Atan2(R.DirD.X, R.DirD.Y), Front0 = -PI / 2;
		  R.Turn0 = FMath::Sign(FMath::Atan2(FMath::Sin(TunYaw - Front0), FMath::Cos(TunYaw - Front0))) * 0.3f; }
		// (a check that the export and his measures agree: the third section's place along the tunnel)
		float Z1, Z2, Z3; ADPJetwayInternal::Slides(R.LenD, Z1, Z2, Z3);
		const float Along = FVector::DotProduct(R.G[Tun3].Docked.GetLocation() - R.PD, R.AxisD) / 100.f;
		ADP_LOG(TEXT("Jetways: %s rigged, %d parts; reach %.1f m docked, %.1f m at rest; section 3 at %.2f m (his measures: %.2f)"),
			*Id, Parts, R.LenD, ADPJetwayInternal::Ik(R.BoomO, R.Wp).Len, Along, Z3);
		Rigs.Add(MoveTemp(R));
	}
	if (!Rigs.Num()) ADP_WARN(TEXT("Jetways: none found to rig in the Jetways model"));
}

FVector AJetwayRigs::PathAt(const FRig& R, float S) const
{
	// a gentle curve from the rest point to the door, bowing away from the building (his path)
	const FVector Mid = (R.Wp + R.Wd) / 2; FVector Dd = ADPJetwayInternal::Flat(R.Wd - R.Wp); const float Dl = FMath::Max(1.f, (float)Dd.Size()); Dd /= Dl;
	FVector Perp(-Dd.Y, Dd.X, 0.f); if (Perp.Y < 0) Perp = -Perp;
	const FVector C = ADPJetwayInternal::Flat(Mid) + Perp * Dl * 0.18f;
	const float A = (1 - S) * (1 - S), B = 2 * S * (1 - S), Cc = S * S;
	FVector P = R.Wp * A + C * B + R.Wd * Cc;
	P.Z = R.Wp.Z + (R.Wd.Z - R.Wp.Z) * ADPJetwayInternal::Ease(S);
	return P;
}

void AJetwayRigs::Apply(FRig& R)
{
	using namespace ADPJetwayInternal;
	const FVector W = PathAt(R, R.S);
	const FIk K = ADPJetwayInternal::Ik(R.BoomO, W);
	const FQuat QYaw = FQuat::FindBetweenNormals(R.DirD, K.Dir);
	const FQuat QTun = FQuat::FindBetweenNormals(QYaw.RotateVector(R.AxisD), K.Axis) * QYaw;
	const FQuat QTurn = FQuat::FindBetweenNormals(K.Dir, TurnBab(K.Dir, R.Turn));
	const FVector PN = R.BoomO + K.Dir * (RR - 0.25f) * 100.f;
	auto Turned = [](const FTransform& D, const FQuat& Q, const FVector& PivotOld, const FVector& PivotNew)
	{ return FTransform(Q * D.GetRotation(), PivotNew + Q.RotateVector(D.GetLocation() - PivotOld), D.GetScale3D()); };
	float Z1d, Z2d, Z3d, Z1, Z2, Z3; Slides(R.LenD, Z1d, Z2d, Z3d); Slides(K.Len, Z1, Z2, Z3);
	FTransform N[NumGroups];
	N[Boom] = Turned(R.G[Boom].Docked, QYaw, R.BoomO, R.BoomO);
	N[Pitch] = Turned(R.G[Pitch].Docked, QTun, R.PD, PN);
	const float Dz[3] = {Z1 - Z1d, Z2 - Z2d, Z3 - Z3d};
	for (int32 i = 0; i < 3; ++i) { N[Tun1 + i] = Turned(R.G[Tun1 + i].Docked, QTun, R.PD, PN); N[Tun1 + i].AddToTranslation(K.Axis * Dz[i] * 100.f); }
	const FVector CabAt = PN + K.Axis * K.Len * 100.f + K.Dir * (CR - 0.25f) * 100.f + QYaw.RotateVector(R.CabOff);
	N[Cab] = FTransform(QTurn * QYaw * R.G[Cab].Docked.GetRotation(), CabAt, R.G[Cab].Docked.GetScale3D());
	N[CabSleeve] = Turned(R.G[CabSleeve].Docked, QYaw, R.G[Cab].Docked.GetLocation(), CabAt);
	{ FVector WdF = R.Wd; WdF.Z = R.G[Bogie].Docked.GetLocation().Z; FVector WF = W; WF.Z = WdF.Z; N[Bogie] = Turned(R.G[Bogie].Docked, QYaw, WdF, WF); }
	N[Sleeves] = Turned(R.G[Sleeves].Docked, QTun, R.Wd, W);
	TSet<UInstancedStaticMeshComponent*> Dirty;
	for (int32 g = 0; g < NumGroups; ++g)
		for (const FPart& P : R.G[g].Parts)
		{
			P.K->UpdateInstanceTransform(P.Ix, P.Docked.GetRelativeTransform(R.G[g].Docked) * N[g], false, false, true);
			Dirty.Add(P.K);
		}
	for (UInstancedStaticMeshComponent* K2 : Dirty) K2->MarkRenderStateDirty();
}

void AJetwayRigs::Tick(float Dt)
{
	using namespace ADPJetwayInternal;
	ADP_PERF_SCOPE("the jetways (each frame)", 2.f);
	Super::Tick(Dt);
	if (!bBuilt) { if ((Look += Dt) > 1.f) { Look = 0.f; Build(); } return; }
	const float D = Dt * Speed;
	for (FRig& R : Rigs)
	{
		if (R.St == EState::Docking)
		{
			R.T += D; const float A = TDrive, B = A + TCrawl, C = B + TCanopy;
			if (R.T < A) R.S = 0.92f * Ease(R.T / A);
			else if (R.T < B) { const float U = (R.T - A) / TCrawl; R.S = 0.92f + 0.08f * U; R.Turn = R.Turn0 * (1 - Ease(U)); }
			else if (R.T < C) { R.S = 1.f; R.Turn = 0.f; }                     // (the canopy and the shutter: plan part 4)
			else { R.St = EState::Docked; ADP_LOG(TEXT("Jetways: %s docked"), *R.Id); }
			Apply(R);
		}
		else if (R.St == EState::Undocking)
		{
			R.T += D; const float A = TCanopy, B = A + TBack, C = B + THome;
			if (R.T < A) {}
			else if (R.T < B) { const float U = (R.T - A) / TBack; R.S = 1.f - 0.08f * Ease(U); R.Turn = R.Turn0 * Ease(U); }
			else if (R.T < C) R.S = 0.92f * (1.f - Ease((R.T - B) / THome));
			else { R.S = 0.f; R.St = EState::Parked; ADP_LOG(TEXT("Jetways: %s at rest, half way back"), *R.Id); }
			Apply(R);
		}
	}
}

int32 AJetwayRigs::Nearest(const FVector& At, float MaxCm) const
{
	int32 Best = INDEX_NONE; float BD2 = MaxCm * MaxCm;
	for (int32 i = 0; i < Rigs.Num(); ++i) { const float D2 = FVector::DistSquared(At, Console(i)); if (D2 < BD2) { BD2 = D2; Best = i; } }
	return Best;
}
AJetwayRigs::EState AJetwayRigs::State(int32 J) const { return Rigs.IsValidIndex(J) ? Rigs[J].St : EState::Parked; }
FString AJetwayRigs::Name(int32 J) const { return Rigs.IsValidIndex(J) ? Rigs[J].Id : FString(); }
bool AJetwayRigs::Dock(int32 J)
{
	if (!Rigs.IsValidIndex(J) || Rigs[J].St != EState::Parked) return false;
	Rigs[J].St = EState::Docking; Rigs[J].T = 0.f; ADP_LOG(TEXT("Jetways: %s driving out to the door"), *Rigs[J].Id); return true;
}
bool AJetwayRigs::Undock(int32 J)
{
	if (!Rigs.IsValidIndex(J) || Rigs[J].St != EState::Docked) return false;
	Rigs[J].St = EState::Undocking; Rigs[J].T = 0.f; ADP_LOG(TEXT("Jetways: %s pulling back"), *Rigs[J].Id); return true;
}
void AJetwayRigs::Toggle(int32 J) { if (State(J) == EState::Parked) Dock(J); else if (State(J) == EState::Docked) Undock(J); }
FVector AJetwayRigs::Console(int32 J) const
{
	if (!Rigs.IsValidIndex(J) || !Rigs[J].Model.IsValid()) return FVector::ZeroVector;
	const FRig& R = Rigs[J];
	// the console rides with the cab: where the cab is now (the parts' first copy shows it)
	FTransform CabNow = R.G[Cab].Docked;
	if (R.G[Cab].Parts.Num()) { FTransform T; R.G[Cab].Parts[0].K->GetInstanceTransform(R.G[Cab].Parts[0].Ix, T, false); CabNow = R.G[Cab].Docked * R.G[Cab].Parts[0].Docked.Inverse() * T; }
	return R.Model->GetActorTransform().TransformPosition(CabNow.TransformPosition(R.G[Cab].Docked.InverseTransformPosition(R.Ctl)));
}
