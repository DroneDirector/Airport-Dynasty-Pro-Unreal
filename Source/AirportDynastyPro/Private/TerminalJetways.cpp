// The jetways J-1 to J-4, laid out from the plan exactly as the Android edition works them out (plan.ts jetwayAt):
// the rotunda 13 m from the cab, the cab 5.65 m right of the stand's centre line at the front door, the fixed corridor
// from the pier's glass to the rotunda's edge. All pieces instanced.
#include "TerminalJetways.h"
#include "TerminalBuilder.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

#pragma region Sizes
namespace
{
	constexpr float TiltDeg = 5.f;     // the fixed corridor's slope down from the gate (jetway.ts JETWAY.tiltDeg)
	constexpr float Reach = 13.f;      // rotunda to cab, metres
	constexpr float RotR = 1.8f;       // the rotunda's radius
	constexpr float CabR = 1.6f;       // the cab's radius
	constexpr float Sill = 3.3f;       // the aircraft's front door sill height
	constexpr float Walk = 2.5f;       // the walkway's width inside
	constexpr float Inside = 2.4f;     // the height inside
}
#pragma endregion

#pragma region Setup

ATerminalJetways::ATerminalJetways()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

UInstancedStaticMeshComponent* ATerminalJetways::Kit(int32 Shape, const FName Mat)
{
	const FName Key(*FString::Printf(TEXT("%s_%s"), Shape ? TEXT("cyl") : TEXT("box"), *Mat.ToString()));
	if (TObjectPtr<UInstancedStaticMeshComponent>* K = Kits.Find(Key)) return *K;
	UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this, Key);
	K->SetupAttachment(Root);
	K->SetStaticMesh(Shape ? Cylinder : Cube);
	UMaterialInterface* Glass = Mat == "glass" ? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Glass.M_Glass")) : nullptr;
	K->SetMaterial(0, Glass ? Glass : ATerminalBuilder::KitMaterial(this, Mat));
	if (Glass) K->SetCastShadow(false);
	K->SetCollisionProfileName(TEXT("BlockAll"));
	K->RegisterComponent();
	AddInstanceComponent(K);
	Kits.Add(Key, K);
	return K;
}

void ATerminalJetways::Along(const FName Mat, const FVector& A, const FVector& B, float W, float H, float Lat, float Up, float Ext)
{
	const FVector WA = FTerminalPlan::ToWorld(A.X, A.Y, A.Z), WB = FTerminalPlan::ToWorld(B.X, B.Y, B.Z);
	const FVector Dir = WB - WA;
	const float Len = Dir.Size() / 100.f + Ext * 2;
	if (Len < 0.01f) return;
	const FRotator Rot = Dir.Rotation();
	const FVector Right = Rot.RotateVector(FVector(0, 1, 0)), UpV = Rot.RotateVector(FVector(0, 0, 1));
	const FVector At = (WA + WB) * 0.5f + Right * (Lat * 100.f) + UpV * (Up * 100.f);
	Kit(0, Mat)->AddInstance(FTransform(Rot, At, FVector(Len, W, H)));
}

void ATerminalJetways::Drum(const FName Mat, const FVector& At, float R, float H, float YawDeg, bool bWheel)
{
	const FRotator Rot = bWheel ? FRotator(0, YawDeg, 90.f) : FRotator(0, YawDeg, 0);
	Kit(1, Mat)->AddInstance(FTransform(Rot, FTerminalPlan::ToWorld(At.X, At.Y, At.Z), FVector(R * 2, R * 2, H)));
}

void ATerminalJetways::Section(const FVector& A, const FVector& B, float W, float Hgt)
{
	const float Half = W / 2;
	Along("jwStud", A, B, W, 0.12f, 0, -0.06f);                                   // the studded rubber floor
	for (const float S : {-1.f, 1.f})
	{
		Along("jwAlu", A, B, 0.06f, 0.9f, S * (Half + 0.03f), 0.45f);              // the lower panels
		Along("glass", A, B, 0.03f, Hgt - 1.0f, S * (Half + 0.03f), 0.9f + (Hgt - 1.0f) / 2); // the glass
		Along("jwTruss", A, B, 0.1f, 0.1f, S * (Half + 0.12f), 0.9f);               // the steel chords outside
		Along("jwTruss", A, B, 0.1f, 0.1f, S * (Half + 0.12f), Hgt + 0.05f);
		Along("yellow", A, B, 0.04f, 0.02f, S * (Half - 0.04f), 0.01f);            // the edge strips
	}
	Along("jwPanel", A, B, W + 0.2f, 0.14f, 0, Hgt + 0.07f);                       // the roof
	Along("white", A, B, W - 0.3f, 0.02f, 0, Hgt - 0.01f);                         // the ceiling with its LED line
}

#pragma endregion

#pragma region Build

void ATerminalJetways::Build(const FTerminalPlan& Plan)
{
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!Cube || !Cylinder) return;
	int32 N = 0;
	for (const FPlanDoor& D : Plan.Doors)
	{
		if (D.Type != TEXT("jetway")) continue;
		// J-n goes to STAND-n
		const FString StandId = TEXT("STAND-") + D.B.RightChop(2);
		if (const FPlanPoint* S = Plan.Stands.FindByPredicate([&](const FPlanPoint& P) { return P.Id == StandId; }))
		{
			BuildJetway(D, *S, Plan);
			++N;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Jetways built: %d"), N);
}

void ATerminalJetways::BuildJetway(const FPlanDoor& Door, const FPlanPoint& Stand, const FTerminalPlan& Plan)
{
	const float GateX = Door.X, PN = Door.Z, F2Y = Plan.F2Y;

	#pragma region Layout
	// as plan.ts jetwayAt: the cab 5.65 m right of the stand's centre line, the rotunda 13 m from it straight out from the door
	const float DX = Stand.X + 5.65f - GateX;
	const float DZ = FMath::Sqrt(FMath::Max(1.f, Reach * Reach - DX * DX));
	const float RZ = PN + 20.f + 5.5f - DZ;
	const float CorrLen = RZ - RotR - PN;
	const float RotY = F2Y - CorrLen * FMath::Tan(FMath::DegreesToRadians(TiltDeg)); // the rotunda's floor
	const FVector Gate(GateX, F2Y, PN), CorrEnd(GateX, RotY, RZ - RotR);
	const FVector Rot(GateX, RotY, RZ);
	const FVector CabC(GateX + DX, Sill, RZ + DZ);
	const FVector Dir = FVector(DX, 0, DZ).GetSafeNormal();
	const FVector TunA = Rot + Dir * RotR, TunB = FVector(CabC.X, Sill, CabC.Z) - Dir * CabR;
	#pragma endregion

	#pragma region Fixed corridor
	Along("jwAlu", Gate, Gate + FVector(0, 0, 0.01f), Walk + 0.6f, 0.15f, 0, Inside + 0.2f); // the gate trim over the door
	Section(Gate, CorrEnd, Walk, Inside);
	// its legs down to the apron
	for (const float T : {0.35f, 0.75f})
	{
		const FVector P = FMath::Lerp(Gate, CorrEnd, T);
		Drum("jwTruss", FVector(P.X, P.Y / 2, P.Z), 0.18f, P.Y);
	}
	#pragma endregion

	#pragma region Rotunda
	Drum("jwStud", FVector(Rot.X, RotY - 0.06f, Rot.Z), RotR, 0.12f);             // its floor
	Drum("jwAlu", FVector(Rot.X, RotY + Inside / 2, Rot.Z), RotR + 0.08f, 0.15f);  // a band round it
	Drum("jwPanel", FVector(Rot.X, RotY + Inside + 0.1f, Rot.Z), RotR + 0.1f, 0.2f); // its roof
	Drum("jwTruss", FVector(Rot.X, RotY / 2, Rot.Z), 0.45f, RotY);                 // its column
	Drum("jwAlu", FVector(Rot.X, RotY - 0.4f, Rot.Z), RotR * 0.8f, 0.6f);          // the drum under it
	// its round wall: clad panels, open towards the corridor and the tunnel
	const float CorrYaw = 90.f, TunYaw = FMath::RadiansToDegrees(FMath::Atan2(-DZ, DX));
	for (int32 i = 0; i < 16; ++i)
	{
		const float A = i * 22.5f + 11.25f;
		auto Near = [&](float Y) { return FMath::Abs(FMath::FindDeltaAngleDegrees(A, Y)) < 40.f; };
		if (Near(CorrYaw) || Near(TunYaw)) continue;
		const float Rad = FMath::DegreesToRadians(A);
		const FVector P = FTerminalPlan::ToWorld(Rot.X, RotY + Inside / 2, Rot.Z) + FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0) * RotR * 100.f;
		Kit(0, "jwPanel")->AddInstance(FTransform(FRotator(0, A + 90.f, 0), P, FVector(RotR * 0.42f, 0.08f, Inside)));
	}
	// the bellows from the corridor into the rotunda
	Along("jwBellows", CorrEnd - FVector(0, 0, 0.4f), CorrEnd + FVector(0, 0, 0.2f), Walk + 0.5f, Inside + 0.4f, 0, Inside / 2);
	#pragma endregion

	#pragma region Tunnel
	// three telescopic sections, the outer one widest, each overlapping the next
	const float Widths[3] = {Walk + 0.3f, Walk + 0.15f, Walk};
	for (int32 s = 0; s < 3; ++s)
	{
		const FVector A = FMath::Lerp(TunA, TunB, s / 3.f - (s ? 0.04f : 0.f));
		const FVector B = FMath::Lerp(TunA, TunB, (s + 1) / 3.f);
		Section(A, B, Widths[s], Inside + 0.1f - s * 0.05f);
	}
	#pragma endregion

	#pragma region Drive unit
	// two lift columns at two thirds of the tunnel, one each side, and the steered bogie with its four wheels
	const FVector DU = FMath::Lerp(TunA, TunB, 0.7f);
	const FVector Right = FVector(Dir.Z, 0, -Dir.X); // the tunnel's right, in the plan
	for (const float S : {-1.f, 1.f})
	{
		const FVector C = DU + Right * S * (Walk / 2 + 0.35f);
		Drum("jwTruss", FVector(C.X, (DU.Y - 0.2f) / 2 + 0.6f, C.Z), 0.2f, DU.Y - 0.8f);   // the leg
		Drum("jwAlu", FVector(C.X, DU.Y - 0.8f, C.Z), 0.28f, 1.2f);                       // the sleeve over it
	}
	Along("jwTruss", DU - Right * (Walk / 2 + 0.35f), DU + Right * (Walk / 2 + 0.35f), 0.3f, 0.3f, 0, -0.3f); // the cross beam
	const FVector Bog(DU.X, 0.75f, DU.Z);
	Along("yellow", Bog - Dir * 1.0f, Bog + Dir * 1.0f, 1.6f, 0.4f, 0, 0);
	const float WheelYaw = FMath::RadiansToDegrees(FMath::Atan2(-Dir.Z, Dir.X));
	for (const float F : {-0.6f, 0.6f})
		for (const float S : {-1.f, 1.f})
		{
			const FVector W = Bog + Dir * F + Right * S * 0.95f;
			Drum("jwTyre", FVector(W.X, 0.45f, W.Z), 0.45f, 0.35f, WheelYaw, true);
			Drum("jwRim", FVector(W.X, 0.45f, W.Z), 0.25f, 0.37f, WheelYaw, true);
		}
	#pragma endregion

	#pragma region Cab
	Drum("jwStud", FVector(CabC.X, Sill - 0.06f, CabC.Z), CabR, 0.12f);
	Drum("jwPanel", FVector(CabC.X, Sill + Inside + 0.1f, CabC.Z), CabR + 0.1f, 0.2f);
	for (int32 i = 0; i < 12; ++i)
	{
		const float A = i * 30.f + 15.f;
		const float Back = FMath::RadiansToDegrees(FMath::Atan2(Dir.Z, -Dir.X)); // towards the tunnel, in Unreal's yaw
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(A, Back)) < 40.f) continue;
		// towards the aircraft (the stand's centre line, -x of the cab): glass; the rest clad
		const bool bFront = FMath::Abs(FMath::FindDeltaAngleDegrees(A, 180.f)) < 70.f;
		const float Rad = FMath::DegreesToRadians(A);
		const FVector P = FTerminalPlan::ToWorld(CabC.X, Sill + Inside / 2, CabC.Z) + FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0) * CabR * 100.f;
		Kit(0, bFront ? FName("jwBellows") : FName("jwAlu"))->AddInstance(FTransform(FRotator(0, A + 90.f, 0), P, FVector(CabR * 0.55f, 0.08f, Inside)));
	}
	// the canopy reaching to the fuselage, and the operator's console
	Along("jwBellows", FVector(CabC.X - CabR, Sill, CabC.Z), FVector(CabC.X - CabR - 0.9f, Sill, CabC.Z), 2.4f, Inside + 0.2f, 0, Inside / 2);
	Along("jwStud", FVector(CabC.X - CabR, Sill, CabC.Z), FVector(CabC.X - CabR - 0.9f, Sill, CabC.Z), 1.6f, 0.1f, 0, -0.05f);
	Along("black", FVector(CabC.X - 0.6f, Sill, CabC.Z - 1.0f), FVector(CabC.X + 0.2f, Sill, CabC.Z - 1.0f), 0.5f, 1.0f, 0, 0.5f);
	// the cab's support under it
	Drum("jwTruss", FVector(CabC.X, Sill / 2, CabC.Z), 0.25f, Sill);
	#pragma endregion
}

#pragma endregion
