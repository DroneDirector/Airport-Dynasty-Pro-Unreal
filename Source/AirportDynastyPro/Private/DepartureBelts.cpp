#include "DepartureBelts.h"
#include "ADPLog.h"
#include "ADPKeep.h"
#include "ADPAnimations.h"
#include "PerfLog.h"
#include "TerminalPlan.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPBoards.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Misc/CommandLine.h"

// Plan metres: x east, z north (airside), height up - FTerminalPlan::ToWorld(x, height, z). The layout follows the
// approved blueprint (Departure-Baggage-v2) with the check-in islands 3.5 m east (Security-Escalators-v3).
namespace
{
	const float ISLX[] = {47.5f, 74.5f};               // the islands' west ends (desks at +1.05 + k * 2.08, k 0..11)
	constexpr float DESKZ = 16.4f, COLZ = 18.6f;        // the desks' line, the collectors' line
	constexpr float BELT_DX = 0.78f;                    // a desk's weigh belt: east of its (narrowed) desk
	constexpr float WEIGH_TOP = 0.40f, FLOOR_TOP = 0.75f, TUNNEL_Y = -2.0f;   // (the belts' tops: weigh belts, the block's belts, the tunnel's)
	constexpr float ML = 11.f, MR = 1.8f;               // a make-up carousel's straight, its plates' centre-line radius
	constexpr float MPER = 2.f * ML + 2.f * PI * MR;    // 33.3 m round
	const FVector2f MAKEUP[] = {FVector2f(124.f, 33.5f), FVector2f(145.f, 33.5f)};
	const TCHAR* DEPBAGM[] = {TEXT("SM_Bag0"), TEXT("SM_Bag1"), TEXT("SM_Bag2"), TEXT("SM_Bag3")};
	// where hands go on each bag kind (its own frame, cm; its bottom at 0): the carry handle, the two sides; two hands?
	struct FGripDef { FVector Handle, L, R; bool bTwo; float Kg0, Kg1; };
	const FGripDef GRIP[] = {
		{FVector(0, -25.5f, 16.8f), FVector(-35, 0, 17), FVector(35, 0, 17), true, 14.f, 28.f},     // big hard case
		{FVector(0, -22.f, 14.4f), FVector(-27.5f, 0, 14), FVector(27.5f, 0, 14), false, 8.f, 18.f}, // small hard case
		{FVector(0, -28.f, 19.2f), FVector(-40, 0, 19), FVector(40, 0, 19), true, 12.f, 26.f},      // soft case
		{FVector(0, 0, 36.f), FVector(-14, 0, 35), FVector(14, 0, 35), false, 5.f, 15.f}};          // duffel (its straps)
	enum EZone { ZIsl1, ZIsl2, ZLink, ZScrA, ZScrB, ZSort, ZCar1, ZCar2, ZNum };
	TWeakObjectPtr<ADepartureBelts> GDep;
	float DepYawOf(const FVector2f& D) { const FVector Wd = FTerminalPlan::ToWorld(D.X, 0, D.Y); return FMath::RadiansToDegrees(FMath::Atan2(Wd.Y, Wd.X)) + 90.f; }
	// the 20 desks (owner 2026-11-21, Checkin-Security-Flow-v1: the 4 places opposite the security doors are walk-throughs
	// - island 1 k 6 and 7, island 2 k 1 and 2), numbered 1..20 west to east: their island, their place k on it
	constexpr int32 NDESK = 20;
	bool IsWalkThrough(int32 Isl, int32 k) { return Isl == 0 ? (k == 6 || k == 7) : (k == 1 || k == 2); }
	FIntPoint DeskSlotOf(int32 Desk)
	{
		int32 n = 0;
		for (int32 Isl = 0; Isl < 2; ++Isl) for (int32 k = 0; k < 12; ++k) if (!IsWalkThrough(Isl, k) && ++n == Desk) return FIntPoint(Isl, k);
		return FIntPoint(0, 0);
	}
	float DeskX(int32 Desk) { const FIntPoint P = DeskSlotOf(FMath::Clamp(Desk, 1, NDESK)); return ISLX[P.X] + 1.05f + P.Y * 2.08f; }
}

ADepartureBelts::ADepartureBelts()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

ADepartureBelts* ADepartureBelts::Get(UWorld* Wo)
{
	if (GDep.IsValid()) return GDep.Get();
	if (!Wo) return nullptr;
	for (TActorIterator<ADepartureBelts> It(Wo); It; ++It) { GDep = *It; return *It; }
	return nullptr;
}

FVector ADepartureBelts::W(float X, float H, float Z) { return FTerminalPlan::ToWorld(X, H, Z); }
FVector ADepartureBelts::WDir(const FVector& D) { return FTerminalPlan::ToWorld(D.X, D.Y, D.Z).GetSafeNormal(); }
// (an FBX part: Blender +X is the mesh's +X, Blender +Y its -Y - so the mesh's -Y goes along the direction)
FRotator ADepartureBelts::RotAlong(const FVector& Wd) { return FRotationMatrix::MakeFromYZ(-Wd, FVector::UpVector).Rotator(); }

#pragma region Building

UInstancedStaticMeshComponent* ADepartureBelts::Kit(const TCHAR* Name, bool bMoving, bool bCollide)
{
	const FName Key(Name);
	if (TObjectPtr<UInstancedStaticMeshComponent>* K = Kits.Find(Key)) return *K;
	const FString Dir = FString(Name).StartsWith(TEXT("SM_Bag")) || FString(Name) == TEXT("SM_BeltPlate") || FString(Name) == TEXT("SM_Flap") || FString(Name) == TEXT("SM_BeltScreen") ? TEXT("/Game/Baggage") : TEXT("/Game/Baggage/Dep");
	UStaticMesh* M = ADPKeep(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Dir, Name, Name)));
	if (!M) { ADP_WARN(TEXT("DepBelts: part %s missing (Tools/import_dep_baggage.py)"), Name); Kits.Add(Key, nullptr); return nullptr; }
	UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this);
	K->SetStaticMesh(M); K->SetupAttachment(GetRootComponent());
	K->SetMobility(bMoving ? EComponentMobility::Movable : EComponentMobility::Static);
	if (bCollide) { if (bMoving) K->SetCollisionEnabled(ECollisionEnabled::QueryOnly); else K->SetCollisionProfileName(TEXT("BlockAll")); }
	else K->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	K->RegisterComponent(); Keep.Add(K); Kits.Add(Key, K);
	return K;
}

void ADepartureBelts::Put(const TCHAR* Name, const FVector& P, const FRotator& R, const FVector& Sc)
{
	if (UInstancedStaticMeshComponent* K = Kit(Name)) K->AddInstance(FTransform(R, W(P.X, P.Y, P.Z), Sc), true);
}

int32 ADepartureBelts::AddSeg(const TArray<FVector>& Pts, int32 Zone, float Speed, bool bClosed)
{
	FSeg G; G.P = Pts; G.Zone = Zone; G.Speed = Speed; G.bClosed = bClosed;
	G.Cum.Add(0.f);
	const int32 N = bClosed ? Pts.Num() : Pts.Num() - 1;
	for (int32 i = 0; i < N; ++i) G.Cum.Add(G.Cum.Last() + FVector::Dist(Pts[i], Pts[(i + 1) % Pts.Num()]));
	G.Len = G.Cum.Last();
	return Segs.Add(G);
}

ADepartureBelts::FZone& ADepartureBelts::AddZone(const FString& Name, const FVector& Ear)
{
	FZone& Z = Zones.AddDefaulted_GetRef(); Z.Name = Name; Z.Ear = Ear; Z.ClackIn = FMath::FRandRange(3.f, 7.f);
	return Z;
}

void ADepartureBelts::LayConveyor(int32 Si, float TrimStart, float TrimEnd, bool bGL, bool bGR, const TArray<FVector2f>& Gaps, float ExFrom, float ExTo, bool bLegs)
{
	const FSeg& G = Segs[Si];
	const int32 NP = G.P.Num(), NPieces = G.bClosed ? NP : NP - 1;
	auto HDir = [&](int32 i) { const FVector D = G.P[(i + 1) % NP] - G.P[i]; return FVector2f(D.X, D.Z).GetSafeNormal(); };
	for (int32 i = 0; i < NPieces; ++i)
	{
		FVector A = G.P[i], B = G.P[(i + 1) % NP];
		const FVector2f H = HDir(i);
		// a 90-degree turn at either end: a corner piece there (placed at the piece's end), the piece stops short of it
		const bool bTurnA = (i > 0 || G.bClosed) && FVector2f::DotProduct(HDir((i - 1 + NPieces) % NPieces), H) < 0.5f;
		const bool bTurnB = (i < NPieces - 1 || G.bClosed) && FVector2f::DotProduct(HDir((i + 1) % NPieces), H) < 0.5f;
		const FVector D = (B - A).GetSafeNormal();
		float T0 = bTurnA ? 0.5f : 0.f, T1 = bTurnB ? 0.5f : 0.f;
		if (i == 0) T0 += TrimStart;
		if (i == NPieces - 1) T1 += TrimEnd;
		if (bTurnB) Corner(FVector2f(B.X, B.Z), B.Y, H, HDir((i + 1) % NPieces));
		const float SegLen = FVector::Dist(A, B), Lp = SegLen - T0 - T1;
		if (Lp < 0.05f) continue;
		const FVector A2 = A + D * T0;
		const int32 N = FMath::Max(1, FMath::RoundToInt(Lp / 2.f));
		const float Step = Lp / N;
		const FRotator R = RotAlong(WDir(D)), RH = RotAlong(WDir(FVector(H.X, 0, H.Y)));
		const FVector2f Nl(-H.Y, H.X);   // (the left side, looking along the belt)
		for (int32 k = 0; k < N; ++k)
		{
			const FVector S0 = A2 + D * (k * Step), Mid = S0 + D * (Step / 2);
			const float SAt = G.Cum[i] + T0 + (k + 0.5f) * Step;
			const bool bEx = ExFrom >= 0.f && SAt > ExFrom - Step / 2 && SAt < ExTo + Step / 2;
			if (FMath::Max(S0.Y, (S0 + D * Step).Y) < -0.05f) continue;   // (under the floor - the tunnel: nothing to draw)
			Put(TEXT("SM_DepConv"), S0, R, FVector(1, Step / 2.f, 1));
			for (int32 sd = 0; sd < 2; ++sd)
			{
				if (bEx || !(sd == 0 ? bGL : bGR)) continue;
				const FVector2f Off = Nl * (sd == 0 ? 0.555f : -0.555f);
				const FVector2f GA(S0.X + Off.X, S0.Z + Off.Y), GB = GA + FVector2f(D.X, D.Z) * Step;
				bool bGap = false;
				for (const FVector2f& Gp : Gaps)
				{
					const FVector2f AB = GB - GA; const float T = FMath::Clamp(FVector2f::DotProduct(Gp - GA, AB) / FMath::Max(AB.SizeSquared(), 1e-4f), 0.f, 1.f);
					if (FVector2f::Distance(GA + AB * T, Gp) < 0.9f) { bGap = true; break; }
				}
				if (!bGap) Put(TEXT("SM_DepGuard"), FVector(GA.X, S0.Y, GA.Y), R, FVector(1, Step / 2.f, 1));
			}
			if (bLegs && !bEx && Mid.Y < 1.3f && Mid.Y > 0.3f) Put(TEXT("SM_DepLeg"), FVector(Mid.X, 0, Mid.Z), RH, FVector(1, 1, FMath::Max(0.05f, Mid.Y - 0.28f)));
		}
	}
}

void ADepartureBelts::Corner(const FVector2f& At, float H, const FVector2f& In, const FVector2f& Out)
{
	// its guard walls are on the part's +Y and +X sides: turned so they are the outer sides (ahead, and away from the exit)
	const FVector WIn = WDir(FVector(In.X, 0, In.Y)), WOut = WDir(FVector(Out.X, 0, Out.Y));
	FRotator Best; float BestScore = -10.f;
	for (const FVector2f C : {In, -In, Out, -Out})
	{
		const FRotator R = RotAlong(WDir(FVector(C.X, 0, C.Y)));
		const FQuat Q = R.Quaternion();
		const float Sc = FVector::DotProduct(-Q.GetAxisY(), WIn) + FVector::DotProduct(Q.GetAxisX(), -WOut);
		if (Sc > BestScore) { BestScore = Sc; Best = R; }
	}
	Put(TEXT("SM_DepCorner"), FVector(At.X, H, At.Y), Best);
	if (H < 1.3f) Put(TEXT("SM_DepLeg"), FVector(At.X, 0, At.Y), RotAlong(WIn), FVector(1, 1, FMath::Max(0.05f, H - 0.28f)));
}

void ADepartureBelts::Curtain(const FVector& Hinge, const FVector& PlanDir, float Width, float Len, int32 Seg, int32 Strips)
{
	// rubber strips hung across the belt from a hinge line (as the arrivals' hoods): the bags push them aside
	if (!FlapK) return;
	const FVector2f H = FVector2f(PlanDir.X, PlanDir.Z).GetSafeNormal(), Nl(-H.Y, H.X);
	const FQuat Base = RotAlong(WDir(FVector(H.X, 0, H.Y))).Quaternion();
	const FVector Axis = WDir(FVector(Nl.X, 0, Nl.Y));
	const float Wd = Width / Strips;
	for (int32 k = 0; k < Strips; ++k)
	{
		const float O = -Width / 2 + (k + 0.5f) * Wd;
		FFlap F; F.Seg = Seg; F.Hinge = W(Hinge.X + Nl.X * O, Hinge.Y, Hinge.Z + Nl.Y * O); F.Base = Base; F.Axis = Axis; F.Len = Len;
		F.Ix = FlapK->AddInstance(FTransform(Base, F.Hinge, FVector(Wd * 0.94f / 0.12f, 1, Len / 0.47f)), true);
		Flaps.Add(F);
	}
}

int32 ADepartureBelts::AddArm(int32 Si, float At, const FVector2f& Push)
{
	// a paddle diverter: its post on the far side, upstream; the paddle lies along the belt's edge and swings 40 degrees
	// across it to push a bag off to the push side
	FVector D3; const FVector P = SegPos(Segs[Si], At, D3);
	const FVector Dp = (Segs[Si].P[FMath::Min(1, Segs[Si].P.Num() - 1)] - Segs[Si].P[0]);
	FVector2f D; { FVector Dd; SegPos(Segs[Si], At - 0.2f, Dd); const FVector A0 = SegPos(Segs[Si], At - 0.2f, Dd), A1 = SegPos(Segs[Si], At + 0.2f, Dd); D = FVector2f(A1.X - A0.X, A1.Z - A0.Z).GetSafeNormal(); }
	(void)Dp;
	const FVector2f Piv = FVector2f(P.X, P.Z) - D * 1.3f - Push * 0.75f;
	FArm A; A.Pivot = W(Piv.X, 0, Piv.Y); A.IdleDir = WDir(FVector(D.X, 0, D.Y));
	const FVector2f Act = D * FMath::Cos(FMath::DegreesToRadians(40.f)) + Push * FMath::Sin(FMath::DegreesToRadians(40.f));
	A.PushDir = WDir(FVector(Act.X, 0, Act.Y));
	Put(TEXT("SM_DiverterPost"), FVector(Piv.X, 0, Piv.Y), RotAlong(A.IdleDir));
	if (ArmK) A.Ix = ArmK->AddInstance(FTransform(RotAlong(A.IdleDir), A.Pivot), true);
	return Arms.Add(A);
}

void ADepartureBelts::Build()
{
	GDep = this;
	if (!Kit(TEXT("SM_DepConv")) || !Kit(TEXT("SM_WeighBelt")) || !Kit(TEXT("SM_BeltPlate"), true)) { ADP_WARN(TEXT("DepBelts: the parts are missing - not built")); return; }
	bTest = FParse::Param(FCommandLine::Get(), TEXT("DepBagTest"));
	FlapK = Kit(TEXT("SM_Flap"), true, false);
	ArmK = Kit(TEXT("SM_DiverterArm"), true, false);
	for (const TCHAR* N : DEPBAGM) BagMeshes.Add(Kit(N, true, true));
	FreeIx.SetNum(BagMeshes.Num());

	// ---- the motors' zones (each its own start / run / stop and sounds, where it is) ----
	AddZone(TEXT("check-in 1-10"), FVector(61.f, 0.6f, COLZ)); AddZone(TEXT("check-in 11-20"), FVector(87.f, 0.6f, COLZ));
	AddZone(TEXT("tunnel + rise"), FVector(118.f, 0.6f, COLZ)); AddZone(TEXT("screening A"), FVector(129.5f, 1.f, 7.f));
	AddZone(TEXT("screening B"), FVector(129.5f, 1.f, 13.f)); AddZone(TEXT("sorter"), FVector(135.f, 1.f, 24.6f));
	AddZone(TEXT("make-up 1"), FVector(MAKEUP[0].X, 0.6f, MAKEUP[0].Y)); AddZone(TEXT("make-up 2"), FVector(MAKEUP[1].X, 0.6f, MAKEUP[1].Y));

	// ---- the belts (their centre lines: plan x, belt top, plan z) ----
	// the hall part (owner 2026-11-21, approved Checkin-Security-Flow-v1): each desk's takeaway drops through a hatch behind
	// the desk into the service tunnel 2 m under the hall (TUNNEL_Y), which runs east (continuous under the walk-throughs),
	// under the wall, and rises in the block from x 115.5 to the floor (crossing it at about x 120) - nothing of it on
	// the hall's floor
	const int32 L = AddSeg({FVector(ISLX[0], TUNNEL_Y, COLZ), FVector(115.5f, TUNNEL_Y, COLZ), FVector(122.f, FLOOR_TOP, COLZ), FVector(123.5f, FLOOR_TOP, COLZ),
		FVector(123.5f, FLOOR_TOP, 7.f)}, ZLink, 0.8f);
	const int32 SA = AddSeg({FVector(123.5f, FLOOR_TOP, 7.f), FVector(139.5f, FLOOR_TOP, 7.f)}, ZScrA);
	const int32 SB = AddSeg({FVector(124.f, FLOOR_TOP, 13.f), FVector(139.5f, FLOOR_TOP, 13.f)}, ZScrB);
	const int32 M = AddSeg({FVector(139.5f, FLOOR_TOP, 7.f), FVector(139.5f, FLOOR_TOP, 23.4f)}, ZSort, 0.8f);
	const int32 R = AddSeg({FVector(140.f, FLOOR_TOP, 17.5f), FVector(143.f, FLOOR_TOP, 17.5f), FVector(143.f, FLOOR_TOP, 11.5f)}, ZSort, 0.5f);
	const int32 O = AddSeg({FVector(115.f, FLOOR_TOP, 23.4f), FVector(155.5f, FLOOR_TOP, 23.4f), FVector(155.5f, FLOOR_TOP, 25.8f), FVector(115.f, FLOOR_TOP, 25.8f)}, ZSort, 1.0f, true);
	int32 K[2], Z[2];
	for (int32 c = 0; c < 2; ++c)
	{
		const float X = MAKEUP[c].X;
		K[c] = AddSeg({FVector(X, FLOOR_TOP, 26.3f), FVector(X, 0.62f, 30.95f), FVector(X, 0.46f, MAKEUP[c].Y - MR)}, ZSort, 0.7f);
		FSeg& G = Segs[AddSeg({FVector(MAKEUP[c].X, 0.45f, MAKEUP[c].Y)}, c == 0 ? ZCar1 : ZCar2, 0.46f, true)];
		G.bOval = true; G.C = MAKEUP[c]; G.Len = MPER; G.bAlong = true; G.Carousel = c; G.Gap = 1.0f;
		Z[c] = Segs.Num() - 1;
		Segs[K[c]].Next = Z[c]; Segs[K[c]].NextS = ML / 2;   // (the chute drops onto the south straight's middle)
		Segs[K[c]].Gap = 1.0f;
	}
	Segs[L].Next = SA; Segs[SA].Next = M; Segs[SB].Next = M; Segs[SB].NextS = 6.f; Segs[M].Next = O; Segs[M].NextS = 139.5f - 115.f;
	Segs[O].Gap = 1.0f; Segs[M].Gap = 1.0f;
	Segs[SA].CTAt = 129.5f - 123.5f; Segs[SB].CTAt = 129.5f - 124.f;
	// the diverters: the distribution belt shares the bags between the two lines; the merge sends the CT's rejects to
	// the search room; the sorter pushes each bag down its flight's chute
	{
		const float AtB = Segs[L].Cum[3] + (COLZ - 13.f);
		FDivert Dv; Dv.At = AtB; Dv.To = SB; Dv.ToS = 0.f; Dv.Kind = EDiv::Alternate; Dv.Arm = AddArm(L, AtB, FVector2f(1, 0)); Segs[L].Div.Add(Dv);
		FDivert Rj; Rj.At = 17.5f - 7.f; Rj.To = R; Rj.ToS = 0.f; Rj.Kind = EDiv::Reject; Rj.Arm = AddArm(M, Rj.At, FVector2f(1, 0)); Segs[M].Div.Add(Rj);
		const float North0 = Segs[O].Cum[2];   // (the north leg starts at x 155.5, running west)
		for (int32 c = 1; c >= 0; --c)
		{
			FDivert Dc; Dc.At = North0 + (155.5f - MAKEUP[c].X); Dc.To = K[c]; Dc.ToS = 0.f; Dc.Kind = EDiv::Carousel; Dc.Carousel = c;
			Dc.Arm = AddArm(O, Dc.At, FVector2f(0, 1)); Segs[O].Div.Add(Dc);
		}
	}
	// the desks' weigh + tag belts (one per desk, all 20), each down into the tunnel (continuous under the walk-throughs)
	for (int32 d = 1; d <= NDESK; ++d)
	{
		const float BX = DeskX(d) + BELT_DX;
		// (its belt, then down through the hatch behind the desk into the tunnel)
		const int32 Si = AddSeg({FVector(BX, WEIGH_TOP, DESKZ), FVector(BX, WEIGH_TOP, 18.2f), FVector(BX, -1.2f, 18.75f), FVector(BX, TUNNEL_Y, COLZ)}, DeskSlotOf(d).X == 0 ? ZIsl1 : ZIsl2, 0.5f);
		FSeg& G = Segs[Si]; G.bAlong = true; G.Desk = d; G.Gap = 1.2f; G.Next = L; G.NextS = BX - ISLX[0];
		FDesk& Dk = Desks.AddDefaulted_GetRef(); Dk.At = FVector2f(BX, DESKZ); Dk.Seg = Si;
		Put(TEXT("SM_WeighBelt"), FVector(BX, 0, 15.95f), FRotator::ZeroRotator);
		// the weight display's digits (facing the passenger)
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(this); T->SetupAttachment(GetRootComponent()); T->RegisterComponent(); Keep.Add(T);
		T->SetWorldLocationAndRotation(W(BX + 0.33f, 0.98f, 15.93f), FRotator(0, 90.f, 0));
		T->SetHorizontalAlignment(EHTA_Center); T->SetVerticalAlignment(EVRTA_TextCenter); T->SetWorldSize(5.f);
		T->SetTextRenderColor(FColor(60, 255, 90)); T->SetText(FText::FromString(TEXT("0.0 kg"))); Dk.Display = T;
		// the hatch behind it: the stainless hood (as the arrivals' feed hoods) the takeaway belt runs into, its strip
		// curtain the bag pushes through, the dark shaft down to the tunnel
		Put(TEXT("SM_DeskHatch"), FVector(BX, 0, 18.1f), FRotator::ZeroRotator);
		Curtain(FVector(BX, 0.77f, 18.12f), FVector(0, 0, 1), 0.58f, 0.35f, Si, 6);
	}
	SegBags.SetNum(Segs.Num());

	// ---- the machinery ----
	const TArray<FVector2f> NoGaps;
	// the tunnel's rise into the block (its frame where it comes up through the floor), on to the distribution belt
	LayConveyor(L, 0.f, 0.5f, true, true, {FVector2f(124.f, 13.f), FVector2f(122.9f, 14.3f)});
	{
		const FSeg& G = Segs[L]; const FVector D = (G.P[2] - G.P[1]).GetSafeNormal();
		const FVector Cross = G.P[1] + D * ((0.f - G.P[1].Y) / FMath::Max(D.Y, 0.01f));
		Put(TEXT("SM_TubeMouth"), Cross + FVector(0, 0.02f, 0), RotAlong(WDir(D)));
		Curtain(Cross + FVector(0, 0.95f, 0), D, 1.2f, 0.6f, L);
	}
	Corner(FVector2f(123.5f, 7.f), FLOOR_TOP, FVector2f(0, -1), FVector2f(1, 0));
	// the screening lines, each through its CT scanner (no guards or legs under the housing)
	for (const int32 Si : {SA, SB})
	{
		const float Zl = Segs[Si].P[0].Z;
		LayConveyor(Si, Si == SA ? 0.5f : 0.f, 0.5f, true, true, NoGaps, Segs[Si].CTAt - 2.5f, Segs[Si].CTAt + 2.5f);
		Put(TEXT("SM_CTScanner"), FVector(129.5f, 0, Zl), RotAlong(WDir(FVector(1, 0, 0))));
		for (const float sd : {-1.f, 1.f}) Curtain(FVector(129.5f + sd * 2.24f, 1.35f, Zl), FVector(1, 0, 0), 1.08f, 0.55f, Si);
	}
	Corner(FVector2f(139.5f, 7.f), FLOOR_TOP, FVector2f(1, 0), FVector2f(0, 1));
	LayConveyor(M, 0.5f, 0.5f, true, true, {FVector2f(139.f, 13.f), FVector2f(140.f, 17.5f), FVector2f(139.f, 16.2f)});
	LayConveyor(R, 0.f, 0.f, true, true, NoGaps);
	Put(TEXT("SM_EndStop"), FVector(143.f, FLOOR_TOP, 11.5f), RotAlong(WDir(FVector(0, 0, -1))));
	Put(TEXT("SM_SearchTable"), FVector(145.2f, 0, 12.6f), FRotator::ZeroRotator);
	LayConveyor(O, 0.f, 0.f, true, true, {FVector2f(139.5f, 22.9f), FVector2f(124.f, 26.3f), FVector2f(145.f, 26.3f), FVector2f(125.3f, 25.3f), FVector2f(146.3f, 25.3f)});
	// the chutes, the carousels (their still body, their plates, the hood's flaps where the chute comes in, the screens)
	UInstancedStaticMeshComponent* PlateK = nullptr;
	for (int32 c = 0; c < 2; ++c)
	{
		const FVector2f C = MAKEUP[c];
		LayConveyor(K[c], 0.f, 0.f, true, true, NoGaps);
		Put(TEXT("SM_ChuteHood"), FVector(C.X, 0, 30.95f), FRotator::ZeroRotator);
		Curtain(FVector(C.X, 0.95f, 31.0f), FVector(0, 0, 1), 1.18f, 0.3f, K[c]);
		Put(TEXT("SM_MakeupBody"), FVector(C.X, 0, C.Y), FRotator::ZeroRotator);
		FCarousel& Kc = Carousels.AddDefaulted_GetRef(); Kc.C = C; Kc.Seg = Z[c];
		Kc.Plates = NewObject<UInstancedStaticMeshComponent>(this); Kc.Plates->SetStaticMesh(Kit(TEXT("SM_BeltPlate"), true)->GetStaticMesh());
		Kc.Plates->SetupAttachment(GetRootComponent()); Kc.Plates->SetMobility(EComponentMobility::Movable); Kc.Plates->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Kc.Plates->RegisterComponent(); Keep.Add(Kc.Plates);
		const int32 NP = FMath::FloorToInt(MPER / 0.3f);
		for (int32 k = 0; k < NP; ++k) Kc.Plates->AddInstance(PlateAt(Kc, k * MPER / NP), true);
		Put(TEXT("SM_BeltScreen"), FVector(C.X, 3.56f, C.Y), FRotator::ZeroRotator);
		for (const float sd : {-1.f, 1.f})
		{
			UTextRenderComponent* T = NewObject<UTextRenderComponent>(this); T->SetupAttachment(GetRootComponent()); T->RegisterComponent(); Keep.Add(T);
			T->SetWorldLocationAndRotation(W(C.X, 3.56f, C.Y + sd * 0.07f), FRotator(0, sd < 0 ? 90.f : -90.f, 0));
			T->SetHorizontalAlignment(EHTA_Center); T->SetVerticalAlignment(EVRTA_TextCenter); T->SetWorldSize(14.f);
			T->SetTextRenderColor(FColor(245, 196, 40)); T->SetText(FText::FromString(FString::Printf(TEXT("MAKE-UP %d"), c + 1)));
			Kc.Screens.Add(T);
		}
	}
	if (UInstancedStaticMeshComponent* Pk = Kits.FindRef(FName(TEXT("SM_BeltPlate")))) Pk->SetVisibility(false);   // (the shared one: only its mesh was wanted)
	(void)PlateK;
	if (FlapK) FlapK->MarkRenderStateDirty();

	// ---- the sounds (the arrivals' belt recordings), heard as from where you stand ----
	USoundBase* SStart = LoadObject<USoundBase>(nullptr, TEXT("/Game/Baggage/Sounds/BeltStart.BeltStart"));
	USoundBase* SRun = LoadObject<USoundBase>(nullptr, TEXT("/Game/Baggage/Sounds/BeltRun.BeltRun"));
	USoundBase* SClack = LoadObject<USoundBase>(nullptr, TEXT("/Game/Baggage/Sounds/BeltClack.BeltClack"));
	USoundAttenuation* Att = NewObject<USoundAttenuation>(this); Keep.Add(Att);
	Att->Attenuation.bAttenuate = true; Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Att->Attenuation.AttenuationShapeExtents = FVector(600.f); Att->Attenuation.FalloffDistance = 4000.f;
	Att->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound; Att->Attenuation.bSpatialize = true;
	auto Audio = [&](USoundBase* S, const FVector& At) -> UAudioComponent*
	{
		if (!S) return nullptr;
		UAudioComponent* A = NewObject<UAudioComponent>(this); A->SetupAttachment(GetRootComponent()); A->SetSound(S); A->bAutoActivate = false;
		A->AttenuationSettings = Att; A->SetWorldLocation(At); A->SetLowPassFilterEnabled(true); A->RegisterComponent(); Keep.Add(A); return A;
	};
	for (FZone& Zn : Zones) { const FVector At = W(Zn.Ear.X, Zn.Ear.Y, Zn.Ear.Z); Zn.SStart = Audio(SStart, At); Zn.SRun = Audio(SRun, At); Zn.SClack = Audio(SClack, At); }

	ADP_LOG(TEXT("DepBelts: built - %d belts (20 desk weigh belts into the tunnel), %d diverters, %d flaps, 2 CT lines, 2 make-up carousels; islands at x %.1f / %.1f%s"),
		Segs.Num(), Arms.Num(), Flaps.Num(), ISLX[0], ISLX[1], bTest ? TEXT("; -DepBagTest: passengers check bags in") : TEXT(""));
}

#pragma endregion

#pragma region Bags

FVector ADepartureBelts::SegPos(const FSeg& G, float S, FVector& Dir) const
{
	if (G.bOval)
	{
		// round the carousel: the south straight west -> east, the east bend, the north straight east -> west, the west bend
		S = FMath::Fmod(S, MPER); if (S < 0.f) S += MPER;
		FVector2f P, D;
		if (S < ML) { P = FVector2f(G.C.X - ML / 2 + S, G.C.Y - MR); D = FVector2f(1, 0); }
		else if ((S -= ML) < PI * MR) { const float A = -HALF_PI + S / MR; P = FVector2f(G.C.X + ML / 2 + MR * FMath::Cos(A), G.C.Y + MR * FMath::Sin(A)); D = FVector2f(-FMath::Sin(A), FMath::Cos(A)); }
		else if ((S -= PI * MR) < ML) { P = FVector2f(G.C.X + ML / 2 - S, G.C.Y + MR); D = FVector2f(-1, 0); }
		else { S -= ML; const float A = HALF_PI + S / MR; P = FVector2f(G.C.X - ML / 2 + MR * FMath::Cos(A), G.C.Y + MR * FMath::Sin(A)); D = FVector2f(-FMath::Sin(A), FMath::Cos(A)); }
		Dir = WDir(FVector(D.X, 0, D.Y));
		return FVector(P.X, G.P[0].Y, P.Y);
	}
	const int32 NP = G.P.Num();
	if (G.bClosed) { S = FMath::Fmod(S, G.Len); if (S < 0.f) S += G.Len; }
	else S = FMath::Clamp(S, 0.f, G.Len);
	int32 i = 0; while (i + 2 < G.Cum.Num() && S > G.Cum[i + 1]) ++i;
	const FVector A = G.P[i], B = G.P[(i + 1) % NP];
	const float T = (S - G.Cum[i]) / FMath::Max(G.Cum[i + 1] - G.Cum[i], 1e-4f);
	Dir = WDir(B - A);
	return FMath::Lerp(A, B, T);
}

float ADepartureBelts::Wrap(const FSeg& G, float D) const
{
	if (!G.bClosed) return D;
	D = FMath::Fmod(D, G.Len); if (D > G.Len / 2) D -= G.Len; if (D < -G.Len / 2) D += G.Len;
	return D;
}

bool ADepartureBelts::Free(int32 Si, float S, float Gap, int32 Except, bool bBoth) const
{
	const FSeg& G = Segs[Si];
	for (const int32 j : SegBags[Si])
	{
		if (j == Except) continue;
		const FBag& O = Bags[j];
		if (O.State != EState::Belt || O.Seg != Si) continue;
		const float D = Wrap(G, O.S - S);
		if (bBoth ? FMath::Abs(D) < Gap : (D > -0.01f && D < Gap)) return false;
	}
	return true;
}

FTransform ADepartureBelts::PlateAt(const FCarousel& Kc, float S) const
{
	// (as the claim belts' plates: tipped 6 degrees in the bends so they overlap like shingles, flat on the straights)
	FVector Dw; const FVector P = SegPos(Segs[Kc.Seg], S, Dw);
	const FVector2f D(Dw.X, -Dw.Y);
	const float Out = FMath::Max(0.f, FMath::Abs(P.X - Kc.C.X) - ML / 2);
	const float Kk = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((Out + 0.3f) / 0.6f, 0.f, 1.f));
	return FTransform(FRotator(0, DepYawOf(D), 6.f * Kk), W(P.X, 0.425f + 0.015f * Kk + 0.01f, P.Z));
}

FTransform ADepartureBelts::PlaceOnSeg(const FBag& B) const
{
	const FSeg& G = Segs[B.Seg];
	// the bag's heading: the belt's direction smoothed over 0.6 m (so it turns through a corner, not in a jump)
	FVector D0, D1, Dd; const FVector P = SegPos(G, B.S, Dd);
	const FVector Pa = SegPos(G, G.bClosed ? B.S - 0.3f : FMath::Max(0.f, B.S - 0.3f), D0), Pb = SegPos(G, G.bClosed ? B.S + 0.3f : FMath::Min(G.Len, B.S + 0.3f), D1);
	FVector Wd = (W(Pb.X, Pb.Y, Pb.Z) - W(Pa.X, Pa.Y, Pa.Z)).GetSafeNormal(); if (Wd.IsNearlyZero()) Wd = Dd;
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Wd).GetSafeNormal();
	const FQuat Q = (G.bAlong ? FRotationMatrix::MakeFromXZ(Wd, FVector::UpVector) : FRotationMatrix::MakeFromYZ(Wd, FVector::UpVector)).ToQuat() * FQuat(FVector::UpVector, FMath::DegreesToRadians(B.Yaw));
	const FVector At = W(P.X, P.Y + 0.005f, P.Z) + Side * (B.Side * 100.f);
	return FTransform(Q, At);
}

int32 ADepartureBelts::NewBag(const FDepBagTag& InTag)
{
	FBag B; B.Id = NextId++; B.Kind = FMath::RandRange(0, BagMeshes.Num() - 1);
	B.Yaw = FMath::FRandRange(-8.f, 8.f); B.Side = FMath::FRandRange(-0.06f, 0.06f);
	B.Tag = InTag; if (B.Tag.Kg <= 0.f) B.Tag.Kg = FMath::RoundToFloat(FMath::FRandRange(GRIP[B.Kind].Kg0, GRIP[B.Kind].Kg1) * 10.f) / 10.f;
	B.Desk = InTag.Desk;
	B.bRejectRoll = FMath::FRand() < (bTest ? 0.08f : 0.02f);   // (about 2 % need a look by hand; more in the test, so it shows)
	UInstancedStaticMeshComponent* K = BagMeshes[B.Kind];
	if (FreeIx[B.Kind].Num()) B.Ix = FreeIx[B.Kind].Pop();
	else B.Ix = K ? K->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, 0, -100000.f), FVector(0.001f)), true) : 0;
	const int32 Ix = FreeSlots.Num() ? FreeSlots.Pop() : Bags.AddDefaulted();
	Bags[Ix] = B; IdToIx.Add(B.Id, Ix);
	return B.Id;
}

void ADepartureBelts::HideBag(FBag& B)
{
	if (UInstancedStaticMeshComponent* K = BagMeshes[B.Kind]) K->UpdateInstanceTransform(B.Ix, FTransform(FRotator::ZeroRotator, FVector(0, 0, -100000.f), FVector(0.001f)), true, false, true);
}

void ADepartureBelts::Enter(FBag& B, int32 Si, float S)
{
	B.Seg = Si; B.S = S; B.State = EState::Belt; B.bMoved = true;
	SegBags[Si].Add(IdToIx.FindRef(B.Id));
	if (Segs[Si].bOval) { B.Lap = 0.f; ++Delivered; B.Side = FMath::FRandRange(-0.08f, 0.08f); }
	if (Segs[Si].bOval) ADP_LOG(TEXT("DepBelts: bag #%d (%s, %s, desk %d) on make-up %d"), B.Id, *B.Tag.Pax, *B.Tag.Flight, B.Tag.Desk, Segs[Si].Carousel + 1);
}

ADepartureBelts::FBag* ADepartureBelts::Find(int32 Id) { const int32* I = IdToIx.Find(Id); return I && Bags.IsValidIndex(*I) && Bags[*I].State != EState::Gone ? &Bags[*I] : nullptr; }
const ADepartureBelts::FBag* ADepartureBelts::Find(int32 Id) const { const int32* I = IdToIx.Find(Id); return I && Bags.IsValidIndex(*I) && Bags[*I].State != EState::Gone ? &Bags[*I] : nullptr; }

int32 ADepartureBelts::CheckIn(const FString& Pax, const FString& Flight, int32 Desk)
{
	if (!Desks.IsValidIndex(Desk - 1)) return INDEX_NONE;
	FDepBagTag T; T.Pax = Pax; T.Flight = Flight; T.Desk = Desk;
	const int32 Id = NewBag(T);
	FBag* B = Find(Id); B->State = EState::Waiting;   // (onto the belt as soon as it is free)
	AssignCarousel(Flight); ++Checked;
	ADP_LOG(TEXT("DepBelts: check-in bag #%d - %s, %s, desk %d, %.1f kg"), Id, *Pax, *Flight, Desk, B->Tag.Kg);
	return Id;
}

int32 ADepartureBelts::PassengerDrop(AActor* Passenger, int32 Desk, const FString& Pax, const FString& Flight, USceneComponent* Hand, FName Socket, bool bAutoSetDown)
{
	if (!Desks.IsValidIndex(Desk - 1)) return INDEX_NONE;
	FDepBagTag T; T.Pax = Pax; T.Flight = Flight; T.Desk = Desk;
	const int32 Id = NewBag(T);
	FBag* B = Find(Id); B->State = EState::Held;
	if (Hand) { B->Holder = Hand; B->Socket = Socket; B->Offset = FTransform(-GRIP[B->Kind].Handle); }
	else if (Passenger && Passenger->GetRootComponent()) { B->Holder = Passenger->GetRootComponent(); B->Offset = FTransform(FRotator(0, 90.f, 0), FVector(22.f, 30.f, -62.f)); }
	else B->World = FTransform(FRotator(0, -90.f, 0), W(Desks[Desk - 1].At.X, 0.55f, DESKZ - 0.9f));   // (no passenger: held just in front of the belt)
	B->Wait = bAutoSetDown ? 0.9f : -1.f;   // (Held with Wait >= 0: set down by itself when it runs out)
	AssignCarousel(Flight); ++Checked;
	ADP_LOG(TEXT("DepBelts: %s lifts bag #%d onto desk %d's belt (%s, %.1f kg)"), *Pax, Id, Desk, *Flight, B->Tag.Kg);
	return Id;
}

bool ADepartureBelts::SetDownOnDesk(int32 Id, int32 Desk)
{
	FBag* B = Find(Id); if (!B || (B->State != EState::Held && B->State != EState::Slot)) return false;
	if (Desk > 0) B->Tag.Desk = Desk;
	if (!Desks.IsValidIndex(B->Tag.Desk - 1)) return false;
	B->State = EState::Lowering; B->T = -1.f; B->From = B->World;   // (T < 0: waiting for the belt to be free)
	return true;
}

FTransform ADepartureBelts::DeskSlot(int32 Desk) const
{
	if (!Desks.IsValidIndex(Desk - 1)) return FTransform::Identity;
	FBag B; B.Seg = Desks[Desk - 1].Seg; B.S = 0.f;
	return PlaceOnSeg(B);
}

FVector ADepartureBelts::DeskStandPoint(int32 Desk) const
{
	if (!Desks.IsValidIndex(Desk - 1)) return FVector::ZeroVector;
	return W(Desks[Desk - 1].At.X, 0.f, 15.35f);
}

int32 ADepartureBelts::TakeBag(const FVector& Near, USceneComponent* Hand, FName Socket, float Reach)
{
	int32 Best = INDEX_NONE; float BestD = Reach;
	for (const FBag& B : Bags)
	{
		if (B.State != EState::Belt) continue;
		const float D = FVector::Dist(B.World.GetLocation() + FVector(0, 0, 15.f), Near);
		if (D < BestD) { BestD = D; Best = B.Id; }
	}
	if (Best != INDEX_NONE) TakeBagById(Best, Hand, Socket);
	return Best;
}

bool ADepartureBelts::TakeBagById(int32 Id, USceneComponent* Hand, FName Socket)
{
	FBag* B = Find(Id); if (!B) return false;
	B->State = EState::Held; B->Holder = Hand; B->Socket = Socket; B->Wait = -1.f;
	B->Offset = FTransform(-GRIP[B->Kind].Handle);
	if (!Hand) HideBag(*B);   // (carried off out of sight)
	ADP_LOG(TEXT("DepBelts: bag #%d (%s, %s) taken off"), Id, *B->Tag.Pax, *B->Tag.Flight);
	return true;
}

bool ADepartureBelts::PutOnCarousel(int32 Id, int32 Carousel)
{
	FBag* B = Find(Id); if (!B || !Carousels.IsValidIndex(Carousel)) return false;
	const int32 Si = Carousels[Carousel].Seg; const FSeg& G = Segs[Si];
	// the place on the plates nearest the bag, then the nearest free one either way
	const FVector Here = B->World.GetLocation();
	float BestS = 0.f, BestD = MAX_flt;
	for (float S = 0.f; S < MPER; S += 0.25f) { FVector Dd; const FVector P = SegPos(G, S, Dd); const float D = FVector::DistSquared(W(P.X, P.Y, P.Z), Here); if (D < BestD) { BestD = D; BestS = S; } }
	for (float Off = 0.f; Off < MPER / 2; Off += 0.25f)
		for (const float Sg : {1.f, -1.f})
			if (Free(Si, BestS + Sg * Off, G.Gap, IdToIx.FindRef(Id), true)) { Enter(*B, Si, FMath::Fmod(BestS + Sg * Off + MPER, MPER)); B->Holder = nullptr; return true; }
	return false;
}

bool ADepartureBelts::PutOnSlot(int32 Id, USceneComponent* Slot, FName Socket, const FTransform& Offset)
{
	FBag* B = Find(Id); if (!B || !Slot) return false;
	B->State = EState::Slot; B->Holder = Slot; B->Socket = Socket; B->Offset = Offset;
	return true;
}

void ADepartureBelts::RemoveBag(int32 Id)
{
	const int32* I = IdToIx.Find(Id); if (!I || !Bags.IsValidIndex(*I)) return;
	FBag& B = Bags[*I]; if (B.State == EState::Gone) return;
	HideBag(B); FreeIx[B.Kind].Add(B.Ix); B.State = EState::Gone; B.Holder = nullptr;
	FreeSlots.Add(*I); IdToIx.Remove(Id);
}

const FDepBagTag* ADepartureBelts::Tag(int32 Id) const { const FBag* B = Find(Id); return B ? &B->Tag : nullptr; }
FTransform ADepartureBelts::BagTransform(int32 Id) const { const FBag* B = Find(Id); return B ? B->World : FTransform::Identity; }

FDepBagGrips ADepartureBelts::Grips(int32 Id) const
{
	FDepBagGrips G; const FBag* B = Find(Id); if (!B) return G;
	const FGripDef& D = GRIP[B->Kind];
	G.Handle = B->World.TransformPosition(D.Handle); G.Left = B->World.TransformPosition(D.L); G.Right = B->World.TransformPosition(D.R); G.bTwoHands = D.bTwo;
	return G;
}

TArray<int32> ADepartureBelts::BagsOf(const FString& Flight) const
{
	TArray<int32> Out; for (const FBag& B : Bags) if (B.State != EState::Gone && B.Tag.Flight == Flight) Out.Add(B.Id); return Out;
}

TArray<int32> ADepartureBelts::BagsOnCarousel(const FString& Flight) const
{
	TArray<int32> Out; for (const FBag& B : Bags) if (B.State == EState::Belt && Segs[B.Seg].bOval && B.Tag.Flight == Flight) Out.Add(B.Id); return Out;
}

int32 ADepartureBelts::CarouselOf(const FString& Flight) const { const int32* C = FlightCarousel.Find(Flight); return C ? *C : INDEX_NONE; }
FVector ADepartureBelts::CarouselCentre(int32 C) const { return Carousels.IsValidIndex(C) ? W(Carousels[C].C.X, 0.f, Carousels[C].C.Y) : FVector::ZeroVector; }

int32 ADepartureBelts::AssignCarousel(const FString& Flight)
{
	if (const int32* C = FlightCarousel.Find(Flight)) return *C;
	if (Carousels.Num() == 0) return INDEX_NONE;
	int32 Best = 0; for (int32 c = 1; c < Carousels.Num(); ++c) if (Carousels[c].Flights.Num() < Carousels[Best].Flights.Num()) Best = c;
	Carousels[Best].Flights.Add(Flight); FlightCarousel.Add(Flight, Best);
	ADP_LOG(TEXT("DepBelts: flight %s -> make-up %d"), *Flight, Best + 1);
	return Best;
}

void ADepartureBelts::PollDepartures()
{
	// the departures from here coming up (the simulation's timetable, as the turnarounds read it), every 5 s
	Deps.Reset();
	UADPSim* Sim = UADPSim::Get(this);
	const bool bSim = Sim && Sim->IsRunning();
	FDateTime Now = FDateTime::Now();
	if (bSim)
	{
		Now = Sim->Now();
		const FString Here = ADPBoards::HereIcao();
		for (const FADPFlight& F : Sim->World.Flights)
			if (F.From == Here && F.DepAt > Now - FTimespan::FromMinutes(5.0) && F.DepAt < Now + FTimespan::FromHours(4.0)) Deps.Add({F.Id, F.DepAt});
		Deps.Sort([](const FDep& A, const FDep& B) { return A.DepAt < B.DepAt; });
	}
	// a carousel's flight is let go once it has left and none of its bags are on the plates any more
	for (FCarousel& Kc : Carousels)
	{
		for (int32 f = Kc.Flights.Num() - 1; f >= 0; --f)
		{
			const FString& Fl = Kc.Flights[f];
			if (!bSim || Fl.StartsWith(TEXT("TEST"))) continue;
			const FDep* D = Deps.FindByPredicate([&](const FDep& X) { return X.Id == Fl; });
			if (D && D->DepAt > Now) continue;
			if (BagsOf(Fl).Num() == 0) { ADP_LOG(TEXT("DepBelts: flight %s gone - make-up %d free of it"), *Fl, int32(&Kc - Carousels.GetData()) + 1); FlightCarousel.Remove(Fl); Kc.Flights.RemoveAt(f); }
		}
		FString Txt = FString::Printf(TEXT("MAKE-UP %d"), int32(&Kc - Carousels.GetData()) + 1);
		for (int32 f = 0; f < FMath::Min(3, Kc.Flights.Num()); ++f) Txt += (f == 0 ? TEXT("\n") : TEXT("   ")) + Kc.Flights[f];
		if (Txt != Kc.Shown) { Kc.Shown = Txt; for (UTextRenderComponent* T : Kc.Screens) if (T) T->SetText(FText::FromString(Txt)); }
	}
}

void ADepartureBelts::TestTick(float Dt)
{
	// -DepBagTest: a passenger checks a bag in every couple of seconds at a random desk, for a departure coming up (in
	// 15 min to 4 h), or a test flight when the simulation isn't running; half of them lifted from the hand onto the belt
	if ((TestIn -= Dt) > 0.f) return;
	TestIn = FMath::FRandRange(1.4f, 2.6f);
	const int32 Desk = FMath::RandRange(1, Desks.Num());
	if (Desks[Desk - 1].Queue.Num() > 1) return;
	FString Flight;
	UADPSim* Sim = UADPSim::Get(this);
	if (Sim && Sim->IsRunning() && Deps.Num())
	{
		TArray<const FDep*> Soon; for (const FDep& D : Deps) if (D.DepAt > Sim->Now() + FTimespan::FromMinutes(15.0)) Soon.Add(&D);
		if (Soon.Num()) Flight = Soon[FMath::RandRange(0, FMath::Min(3, Soon.Num()) - 1)]->Id;
	}
	if (Flight.IsEmpty()) { static const TCHAR* TF[] = {TEXT("TEST 101"), TEXT("TEST 202"), TEXT("TEST 303")}; Flight = TF[FMath::RandRange(0, 2)]; }
	const FString Pax = FString::Printf(TEXT("P%05d"), FMath::RandRange(1, 99999));
	if (FMath::RandBool()) PassengerDrop(nullptr, Desk, Pax, Flight); else CheckIn(Pax, Flight, Desk);
}

void ADepartureBelts::AutoClear(float Dt)
{
	// until the ground crew load the carts themselves (TakeBag / PutOnSlot): a flight's bags are lifted off its carousel
	// once they have been round once and it leaves within 25 minutes (or has left); in the test, after 2.5 rounds. The
	// search room's bags are cleared after a look (45 s).
	if ((ClearIn -= Dt) > 0.f) return;
	ClearIn = 2.f;
	UADPSim* Sim = UADPSim::Get(this);
	const bool bSim = Sim && Sim->IsRunning();
	for (int32 c = 0; c < Carousels.Num(); ++c)
		for (const int32 j : SegBags[Carousels[c].Seg])
		{
			const FBag& B = Bags[j]; if (B.State != EState::Belt || B.Seg != Carousels[c].Seg) continue;
			bool bGo = bTest && B.Lap > 2.5f * MPER;
			if (!bGo && bSim && B.Lap > MPER && !B.Tag.Flight.StartsWith(TEXT("TEST")))
			{
				const FDep* D = Deps.FindByPredicate([&](const FDep& X) { return X.Id == B.Tag.Flight; });
				bGo = !D || (D->DepAt - Sim->Now()).GetTotalMinutes() < 25.0;
			}
			if (bGo) { ++Cleared; ADP_LOG(TEXT("DepBelts: bag #%d (%s) lifted off make-up %d onto a cart"), B.Id, *B.Tag.Flight, c + 1); RemoveBag(B.Id); break; }
		}
	for (FBag& B : Bags)
		if (B.State == EState::Belt && Segs[B.Seg].Next == INDEX_NONE && !Segs[B.Seg].bClosed && B.T > 45.f)
		{ ADP_LOG(TEXT("DepBelts: bag #%d (%s, %s) searched and cleared - carried on by hand"), B.Id, *B.Tag.Pax, *B.Tag.Flight); ++Cleared; RemoveBag(B.Id); break; }
}

#pragma endregion

#pragma region Running

void ADepartureBelts::SetZone(int32 Zi, bool bOn)
{
	FZone& Zn = Zones[Zi]; if (Zn.bRun == bOn) return;
	Zn.bRun = bOn; Zn.Idle = 0.f;
	if (bOn) { Zn.Ramp = 0.f; if (Zn.SStart) Zn.SStart->Play(); if (Zn.SRun) Zn.SRun->FadeIn(0.8f, 1.f, FMath::FRand() * 10.f); }
	else if (Zn.SRun) Zn.SRun->FadeOut(1.5f, 0.f);   // (the motor running down)
	ADP_LOG(TEXT("DepBelts: %s %s"), *Zn.Name, bOn ? TEXT("started") : TEXT("stopped"));
}

void ADepartureBelts::Tick(float Dt)
{
	Super::Tick(Dt);
	if (Segs.Num() == 0) return;
	ADP_PERF_SCOPE("DepartureBelts::Tick", 1.5f);
	Dt = FMath::Min(Dt, 0.1f);
	if ((DepPoll -= Dt) <= 0.f) { DepPoll = 5.f; PollDepartures(); }
	if (bTest) TestTick(Dt);
	if (bAutoClear) AutoClear(Dt);

	// who is on which belt (each bag looks only at its own belt's bags)
	for (TArray<int32>& L : SegBags) L.Reset();
	TArray<bool> ZoneBusy; ZoneBusy.Init(false, Zones.Num());
	TArray<bool> DeskBusy; DeskBusy.Init(false, Desks.Num());
	for (int32 i = 0; i < Bags.Num(); ++i)
	{
		const FBag& B = Bags[i];
		if (B.State == EState::Belt) { SegBags[B.Seg].Add(i); ZoneBusy[Segs[B.Seg].Zone] = true; if (Segs[B.Seg].Desk > 0 && B.S < 1.3f) DeskBusy[Segs[B.Seg].Desk - 1] = true; }
		else if ((B.State == EState::Lowering && B.T >= 0.f) && Desks.IsValidIndex(B.Tag.Desk - 1)) DeskBusy[B.Tag.Desk - 1] = true;
	}
	// bags waiting for their desk's belt (checked in without a passenger): onto it when it is clear, weighed there
	for (int32 i = 0; i < Bags.Num(); ++i)
	{
		FBag& B = Bags[i];
		if (B.State != EState::Waiting || !Desks.IsValidIndex(B.Tag.Desk - 1) || DeskBusy[B.Tag.Desk - 1]) continue;
		DeskBusy[B.Tag.Desk - 1] = true; Enter(B, Desks[B.Tag.Desk - 1].Seg, 0.f); B.Wait = 2.5f;
	}
	// the motors: on when a bag is on their part, up to speed in 0.8 s; off after a while with nothing on them
	for (int32 z = 0; z < Zones.Num(); ++z)
	{
		FZone& Zn = Zones[z];
		if (ZoneBusy[z] && !Zn.bRun) SetZone(z, true);
		if (Zn.bRun)
		{
			Zn.Ramp = FMath::Min(1.f, Zn.Ramp + Dt / 0.8f);
			Zn.Idle = ZoneBusy[z] ? 0.f : Zn.Idle + Dt;
			if (Zn.Idle > (z >= ZCar1 ? 40.f : 20.f)) SetZone(z, false);
			if ((Zn.ClackIn -= Dt) <= 0.f) { Zn.ClackIn = FMath::FRandRange(3.f, 8.f); if (Zn.SClack && FMath::FRand() < 0.5f) Zn.SClack->Play(); }
		}
		else Zn.Ramp = FMath::Max(0.f, Zn.Ramp - Dt / 1.5f);
		Sounds(Zn, Dt);
	}
	// every bag: pinned to what carries it - a belt place, a hand, a cart's slot
	for (int32 i = 0; i < Bags.Num(); ++i)
	{
		FBag& B = Bags[i];
		switch (B.State)
		{
		case EState::Gone: case EState::Waiting: continue;
		case EState::Held: case EState::Slot:
		{
			if (USceneComponent* H = B.Holder.Get()) { B.World = B.Offset * H->GetSocketTransform(B.Socket); B.bMoved = true; }
			else if (B.State == EState::Held && B.Wait < 0.f) continue;   // (carried off out of sight, or still)
			if (B.State == EState::Held && B.Wait >= 0.f && (B.Wait -= Dt) < 0.f) SetDownOnDesk(B.Id);
			break;
		}
		case EState::Lowering:
		{
			// from the hand straight down onto the belt's slot (eased, 0.7 s) once the belt is clear; then pinned there
			const int32 d = B.Tag.Desk - 1;
			if (B.T < 0.f) { if (DeskBusy[d]) break; DeskBusy[d] = true; B.T = 0.f; B.From = B.World; }
			B.T += Dt;
			const float A = FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp(B.T / 0.7f, 0.f, 1.f), 2.f);
			FBag Tmp; Tmp.Seg = Desks[d].Seg; Tmp.S = 0.f; Tmp.Yaw = B.Yaw; const FTransform To = PlaceOnSeg(Tmp);
			FTransform T; T.Blend(B.From, To, A); T.SetLocation(T.GetLocation() + FVector(0, 0, 6.f * FMath::Sin(A * PI)));
			B.World = T; B.bMoved = true; B.Holder = nullptr;
			if (B.T >= 0.7f) { Enter(B, Desks[d].Seg, 0.f); B.Wait = 2.5f; }
			break;
		}
		case EState::Belt:
		{
			FSeg& G = Segs[B.Seg];
			if (B.Wait > 0.f) { B.Wait -= Dt; if (B.bMoved) { B.World = PlaceOnSeg(B); } break; }   // (on the scale: weighed, the tag printed)
			const float V = G.Speed * Zones[G.Zone].Ramp * Dt;
			if (V <= 0.f) break;
			float NewS = B.S + V;
			bool bDone = false;
			for (FDivert& Dv : G.Div)
			{
				const float Cross = G.bClosed ? Wrap(G, Dv.At - B.S) : Dv.At - B.S;
				if (!(Cross > 0.f && Cross <= V)) continue;
				bool bWant = false;
				if (Dv.Kind == EDiv::Reject) bWant = B.bReject;
				else if (Dv.Kind == EDiv::Carousel) { if (B.Carousel == INDEX_NONE) B.Carousel = AssignCarousel(B.Tag.Flight); bWant = B.Carousel == Dv.Carousel; }
				else bWant = Dv.bFlip;
				if (!bWant) { if (Dv.Kind == EDiv::Alternate) Dv.bFlip = !Dv.bFlip; continue; }
				if (Free(Dv.To, Dv.ToS, Segs[Dv.To].Gap, i, true))
				{
					if (Dv.Kind == EDiv::Alternate) Dv.bFlip = !Dv.bFlip;
					if (Arms.IsValidIndex(Dv.Arm)) Arms[Dv.Arm].Hold = 1.2f;
					if (Dv.Kind == EDiv::Reject) { ++Rejected; ADP_LOG(TEXT("DepBelts: bag #%d (%s, %s) to the level 3 search room"), B.Id, *B.Tag.Pax, *B.Tag.Flight); }
					Enter(B, Dv.To, Dv.ToS); bDone = true; break;
				}
				if (Dv.Kind == EDiv::Reject) { NewS = B.S + Cross - 0.001f; }   // (a reject waits for the spur; a full chute: once round the sorter again)
			}
			if (bDone) break;
			if (Free(B.Seg, NewS, G.Gap, i, false)) B.S = G.bClosed ? FMath::Fmod(NewS, G.Len) : NewS;
			if (G.bOval) B.Lap += V;
			if (G.CTAt >= 0.f && !B.bScanned && B.S >= G.CTAt)
			{
				B.bScanned = true; B.bReject = B.bRejectRoll;
				if (B.bReject) ADP_LOG(TEXT("DepBelts: CT %s flagged bag #%d (%s, %s) - level 3"), G.Zone == ZScrA ? TEXT("A") : TEXT("B"), B.Id, *B.Tag.Pax, *B.Tag.Flight);
			}
			if (!G.bClosed && B.S >= G.Len)
			{
				B.S = G.Len;
				if (G.Next != INDEX_NONE && Free(G.Next, G.NextS, Segs[G.Next].Gap, i, true)) Enter(B, G.Next, G.NextS);
			}
			if (!G.bClosed && G.Next == INDEX_NONE) { if (B.S >= G.Len - 0.45f) { B.S = G.Len - 0.45f; B.T += Dt; } }   // (the end of the search room's spur)
			B.bMoved = true;
			break;
		}
		default: break;
		}
		if (B.bMoved)
		{
			if (B.State == EState::Belt) B.World = PlaceOnSeg(B);
			if (UInstancedStaticMeshComponent* K = BagMeshes[B.Kind]) K->UpdateInstanceTransform(B.Ix, B.World, true, false, true);
			B.bMoved = false;
		}
	}
	// the weight displays: the bag on the scale
	for (int32 d = 0; d < Desks.Num(); ++d)
	{
		float Kg = 0.f;
		for (const int32 j : SegBags[Desks[d].Seg]) if (Bags[j].State == EState::Belt && Bags[j].Seg == Desks[d].Seg && Bags[j].S < 0.6f) Kg = Bags[j].Tag.Kg;
		if (Kg != Desks[d].ShownKg && Desks[d].Display) { Desks[d].ShownKg = Kg; Desks[d].Display->SetText(FText::FromString(FString::Printf(TEXT("%.1f kg"), Kg))); }
	}
	// the make-up carousels' plates going round while they run
	for (int32 c = 0; c < Carousels.Num(); ++c)
	{
		FCarousel& Kc = Carousels[c]; const FZone& Zn = Zones[Segs[Kc.Seg].Zone];
		if (Zn.Ramp <= 0.f || !Kc.Plates) continue;
		Kc.Phase = FMath::Fmod(Kc.Phase + Segs[Kc.Seg].Speed * Zn.Ramp * Dt, MPER);
		const int32 NP = Kc.Plates->GetInstanceCount();
		for (int32 k = 0; k < NP; ++k) Kc.Plates->UpdateInstanceTransform(k, PlateAt(Kc, k * MPER / NP + Kc.Phase), true, false, true);
	}
	// the diverters' paddles: out across the belt while a bag is pushed off, then back along its edge
	for (FArm& A : Arms)
	{
		const float Want = A.Hold > 0.f ? 1.f : 0.f; A.Hold = FMath::Max(0.f, A.Hold - Dt);
		if (FMath::IsNearlyEqual(A.A, Want)) continue;
		A.A = FMath::FInterpConstantTo(A.A, Want, Dt, 1.f / 0.35f);
		const FVector Dir = FMath::Lerp(A.IdleDir, A.PushDir, FMath::SmoothStep(0.f, 1.f, A.A)).GetSafeNormal();
		if (ArmK) ArmK->UpdateInstanceTransform(A.Ix, FTransform(RotAlong(Dir), A.Pivot), true, false, true);
	}
	// the strip curtains (CT tunnels, the link's mouths, the chutes' hoods): a bag pushing through swings them aside
	for (FFlap& F : Flaps)
	{
		float Want = 0.f;
		if (ADPAnim::BeltFlaps) for (const int32 j : SegBags[F.Seg])
		{
			const FBag& B = Bags[j]; if (B.State != EState::Belt || B.Seg != F.Seg) continue;
			const FVector Bp = B.World.GetLocation() + FVector(0, 0, 15.f);
			if (FMath::Abs(Bp.X - F.Hinge.X) > 120.f || FMath::Abs(Bp.Y - F.Hinge.Y) > 120.f) continue;
			const float D = FMath::Min(FVector::Dist(Bp, F.Hinge - FVector(0, 0, F.Len * 40.f)), FVector::Dist(Bp, F.Hinge - FVector(0, 0, F.Len * 85.f)));
			if (D < 55.f) Want = FMath::Max(Want, 1.1f * (1.f - D / 55.f));
		}
		const float Acc = 60.f * (Want - F.A) - 5.f * F.V;
		F.V += Acc * Dt; F.A += F.V * Dt;
		if (FMath::Abs(F.A) > 0.002f || FMath::Abs(F.V) > 0.002f)
		{
			FTransform T; FlapK->GetInstanceTransform(F.Ix, T, true); T.SetRotation(FQuat(F.Axis, F.A) * F.Base);
			FlapK->UpdateInstanceTransform(F.Ix, T, true, false, true);
		}
	}
	// a line in the log now and then: how many bags where
	static float Summary = 0.f;
	if ((Summary += Dt) > 60.f)
	{
		Summary = 0.f; int32 On = 0; for (const FBag& B : Bags) if (B.State != EState::Gone) ++On;
		if (On || Checked) ADP_LOG(TEXT("DepBelts: %d bags in the system; checked in %d, on the make-up carousels %d, to the search room %d, lifted off %d"), On, Checked, Delivered, Rejected, Cleared);
	}
}

void ADepartureBelts::Sounds(FZone& Zn, float Dt)
{
	// heard as from where you are (owner, 2026-11-16): the attenuation fades it with distance; a wall between you and the
	// motor muffles it (looked at twice a second, only while it is playing)
	const bool bPlaying = (Zn.SRun && Zn.SRun->IsPlaying()) || (Zn.SStart && Zn.SStart->IsPlaying()) || (Zn.SClack && Zn.SClack->IsPlaying());
	if (!bPlaying) return;
	if ((Zn.EarIn -= Dt) <= 0.f)
	{
		Zn.EarIn = 0.5f;
		float Want = 1.f, WantLp = 20000.f;
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (PC->PlayerCameraManager)
			{
				const FVector Ear = PC->PlayerCameraManager->GetCameraLocation(), Src = W(Zn.Ear.X, Zn.Ear.Y + 0.4f, Zn.Ear.Z);
				FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(DepBeltEar), false, this);
				if (const APawn* Me = PC->GetPawn()) Q.AddIgnoredActor(Me);
				if (FVector::Dist(Ear, Src) < 6000.f && GetWorld()->LineTraceSingleByChannel(H, Ear, Src, ECC_Visibility, Q)) { Want = 0.4f; WantLp = 1400.f; }
			}
		Zn.Env = Want; Zn.EnvLp = WantLp;
	}
	for (UAudioComponent* A : {Zn.SStart.Get(), Zn.SRun.Get(), Zn.SClack.Get()})
		if (A && A->IsPlaying()) { A->SetLowPassFilterFrequency(Zn.EnvLp); A->SetVolumeMultiplier(FMath::Max(0.001f, (A == Zn.SClack ? 0.6f : 0.85f) * Zn.Env)); }
}

#pragma endregion
