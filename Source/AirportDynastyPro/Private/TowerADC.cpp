// The control tower, built at run time as in the Android edition's tower.ts (same sizes and places): its curved shells
// (shaft, lobby, cab glass, floors, desk ring, roof) as procedural meshes with both faces and collision, the straight
// pieces as instanced boxes and cylinders, the lift from TerminalLifts, and the doors and desk flap moving each frame.
#include "TowerADC.h"
#include "TerminalBuilder.h"
#include "TerminalLifts.h"
#include "TerminalPlan.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "SunClock.h"
#include "Engine/DirectionalLight.h"
#include "Components/SpotLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

#pragma region Sizes
namespace
{
	// as tower.ts TOWER: metres, the tower's own frame (x east, y up, z north, its foot at the origin)
	constexpr float ShaftR = 3.6f, LobbyTop = 17.3f, GlassY = 18.45f, CabTop = 21.4f, RoofTop = 22.0f;
	constexpr float CabR0 = 6.0f, CabR1 = 6.7f, RoofR = 7.5f, DeskR0 = 4.75f, DeskR1 = 5.65f;
	constexpr int32 Positions = 24, Chairs = 12;
	constexpr float LiftZ = 2.45f, LiftOut = 0.95f;
	constexpr float DoorA = PI / 8, DoorW = 1.0f, DoorH = 2.25f;
	constexpr float LR = ShaftR + 0.8f;                    // the lobby level's radius
	constexpr float EX0 = -6.6f, EX1 = -3.0f, EZ = 1.75f;  // the entrance hall
	constexpr float SR = 1.35f, Turn = PI * 1.85f;          // the spiral stair: its radius, how far round it turns
	constexpr int32 Steps = 22;
	constexpr float TwoPi = 2.f * PI;
	float Ease(float K) { return K * K * (3.f - 2.f * K); }
}
#pragma endregion

#pragma region Setup

// where it stands and its levels (plan metres; tower.ts TOWER_AT / TOWER)
const float ATowerADC::AtX = -205.f;
const float ATowerADC::AtZ = 120.f;
const float ATowerADC::CabY = 17.6f;
const float ATowerADC::LobbyY = 13.5f;

ATowerADC::ATowerADC()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

USceneComponent* ATowerADC::Node(USceneComponent* Parent, const FVector& At, const FRotator& Rot, const FName Name)
{
	USceneComponent* N = NewObject<USceneComponent>(this, Name);
	N->SetupAttachment(Parent);
	N->SetMobility(EComponentMobility::Movable);
	N->SetRelativeLocationAndRotation(At, Rot);
	N->RegisterComponent();
	AddInstanceComponent(N);
	return N;
}

UMaterialInterface* ATowerADC::Mat(const FName Name)
{
	if (TObjectPtr<UMaterialInterface>* F = Mats.Find(Name)) return *F;
	UMaterialInterface* M = nullptr;
	if (Name == "GLASS") M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Glass.M_Glass"));
	// the glass seen from inside: very clear, hardly a reflection (no white haze over the view)
	if (Name == "INGLASS" || Name == "INRAILGLASS")
		if (UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Glass.M_Glass")))
		{
			UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(G, this);
			D->SetScalarParameterValue(TEXT("Specular"), 0.04f);
			D->SetScalarParameterValue(TEXT("Opacity"), 0.03f);
			D->SetScalarParameterValue(TEXT("Roughness"), 0.0f);
			M = D;
		}
	// the balcony's glass railing: clearer and less mirror-like (seen from the cab at a shallow angle the plain glass
	// mirrored the bright sky as a white band in front of the windows)
	if (Name == "RAILGLASS")
		if (UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Glass.M_Glass")))
		{
			UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(G, this);
			D->SetScalarParameterValue(TEXT("Specular"), 0.1f);
			D->SetScalarParameterValue(TEXT("Opacity"), 0.06f);
			M = D;
		}
	// the screens: dark monitor glass with a faint teal picture (the plain light teal read as one glowing band in daylight)
	if (Name == "tealUI")
		if (UMaterialInterface* Base = ATerminalBuilder::KitMaterial(this, Name))
		{
			UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, this);
			D->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.003f, 0.011f, 0.015f));
			D->SetScalarParameterValue(TEXT("Roughness"), 0.22f);
			M = D;
		}
	// the desk's top matte, as control desks are (the near-black gloss mirrored the bright sky as a white band)
	if (Name == "lacquer")
		if (UMaterialInterface* Base = ATerminalBuilder::KitMaterial(this, Name))
		{
			UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, this);
			D->SetScalarParameterValue(TEXT("Roughness"), 0.7f);
			D->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.03f, 0.03f, 0.035f));
			M = D;
		}
	if (!M)
	{
		// the Android edition's names that the PC kit has under another name
		static const TMap<FName, FName> Same = { {"timber", "oak"}, {"whiteGloss", "white"}, {"screenBlack", "black"}, {"light", "lamp"}, {"GLASS", "white"}, {"RAILGLASS", "white"}, {"INGLASS", "white"}, {"INRAILGLASS", "white"} };
		const FName* K = Same.Find(Name);
		M = ATerminalBuilder::KitMaterial(this, K ? *K : Name);
	}
	Mats.Add(Name, M);
	return M;
}

UInstancedStaticMeshComponent* ATowerADC::Kit(UStaticMesh* Mesh, const FName MatName, bool bCollide)
{
	const FString Key = FString::Printf(TEXT("%s|%s|%d"), *Mesh->GetName(), *MatName.ToString(), bCollide ? 1 : 0);
	if (TObjectPtr<UInstancedStaticMeshComponent>* K = Kits.Find(Key)) return *K;
	UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this);
	K->SetupAttachment(Frame);
	K->SetMobility(EComponentMobility::Movable);
	K->SetStaticMesh(Mesh);
	K->SetMaterial(0, Mat(MatName));
	K->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	if (MatName == "GLASS" || MatName == "RAILGLASS") K->SetCastShadow(false);
	K->RegisterComponent();
	AddInstanceComponent(K);
	Kits.Add(Key, K);
	return K;
}

void ATowerADC::Box(const FName M, float W, float H, float D, float X, float Y, float Z, float Yaw, bool bCollide, float PitchDeg)
{
	// the engine cube is 100 cm with its pivot in the middle; a turn of Yaw (tower.ts rotation.y) is the same yaw here
	Kit(Cube, M, bCollide)->AddInstance(FTransform(FRotator(PitchDeg, FMath::RadiansToDegrees(Yaw), 0), U(X, Y, Z), FVector(W, D, H)));
}

void ATowerADC::BoxIn(const FName M, float W, float H, float D, float OX, float OY, float OZ, float Yaw, float LX, float LY, float LZ, bool bCollide)
{
	// a point in a frame turned by Yaw (tower.ts rotation.y): local x goes to (cos, -sin), local z to (sin, cos)
	const float C = FMath::Cos(Yaw), S = FMath::Sin(Yaw);
	Box(M, W, H, D, OX + LX * C + LZ * S, OY + LY, OZ - LX * S + LZ * C, Yaw, bCollide);
}

void ATowerADC::Cyl(const FName M, float R, float H, float X, float Y, float Z, bool bCollide)
{
	// the engine cylinder is 100 cm across and 100 cm tall, its pivot in the middle
	Kit(Cylinder, M, bCollide)->AddInstance(FTransform(FRotator::ZeroRotator, U(X, Y, Z), FVector(R * 2, R * 2, H)));
}

UStaticMeshComponent* ATowerADC::Part(USceneComponent* Parent, UStaticMesh* Mesh, const FName M, float W, float H, float D, float X, float Y, float Z, bool bCollide)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Parent);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	C->SetMaterial(0, Mat(M));
	C->SetRelativeLocation(U(X, Y, Z));
	C->SetRelativeScale3D(FVector(W, D, H));
	C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	if (M == "GLASS") C->SetCastShadow(false);
	C->RegisterComponent();
	AddInstanceComponent(C);
	return C;
}

#pragma endregion

#pragma region Mesh building

void ATowerADC::Tri(FBuf& B, const FVector& A, const FVector& Bp, const FVector& C)
{
	// both faces: (A, B, C) with its normal, and (A, C, B) with the opposite one, set back 1.5 cm behind it (two faces
	// in the very same place fight over every pixel: the inside, lit by the lamps, showed through the outside as a glow)
	FVector N = FVector::CrossProduct(Bp - A, C - A);
	if (!N.Normalize()) return;
	auto UV = [&N](const FVector& P) { return FMath::Abs(N.Z) > 0.7f ? FVector2D(P.X, P.Y) / 300.f : FVector2D(P.X + P.Y, P.Z) / 300.f; };
	for (const int32 Side : {0, 1})
	{
		const FVector Nn = Side ? -N : N;
		const FVector Back = Side ? -N * 1.5 : FVector::ZeroVector;
		const int32 I = B.V.Num();
		B.V.Add(A + Back); B.V.Add((Side ? C : Bp) + Back); B.V.Add((Side ? Bp : C) + Back);
		for (int32 k = 0; k < 3; ++k) { B.N.Add(Nn); B.UV.Add(UV(B.V[I + k])); }
		B.T.Add(I); B.T.Add(I + 1); B.T.Add(I + 2);
	}
}

void ATowerADC::Ring(FBuf& B, float R0, float Y0, float R1, float Y1, float A0, float A1, int32 N)
{
	// tower.ts angles: a point at angle a is (cos a, sin a) in (x, z)
	for (int32 i = 0; i < N; ++i)
	{
		const float Aa = A0 + (A1 - A0) * i / N, Ab = A0 + (A1 - A0) * (i + 1) / N;
		const FVector P0 = U(FMath::Cos(Aa) * R0, Y0, FMath::Sin(Aa) * R0), P1 = U(FMath::Cos(Ab) * R0, Y0, FMath::Sin(Ab) * R0);
		const FVector Q0 = U(FMath::Cos(Aa) * R1, Y1, FMath::Sin(Aa) * R1), Q1 = U(FMath::Cos(Ab) * R1, Y1, FMath::Sin(Ab) * R1);
		Tri(B, P0, P1, Q0);
		Tri(B, P1, Q1, Q0);
	}
}

void ATowerADC::DiscPiece(FBuf& B, float R, float Y, const TArray<FVector>& Cuts)
{
	// the disc as a polygon, cut by each half-plane in turn (kept where nx·x + nz·z ≤ d; a cut is (nx, nz, d))
	TArray<FVector2D> Poly;
	for (int32 i = 0; i < 96; ++i) { const float A = TwoPi * i / 96; Poly.Add(FVector2D(FMath::Cos(A) * R, FMath::Sin(A) * R)); }
	for (const FVector& Cut : Cuts)
	{
		TArray<FVector2D> Out;
		auto F = [&Cut](const FVector2D& P) -> double { return Cut.X * P.X + Cut.Y * P.Y - Cut.Z; };
		for (int32 i = 0; i < Poly.Num(); ++i)
		{
			const FVector2D A = Poly[i], Bq = Poly[(i + 1) % Poly.Num()];
			const double Fa = F(A), Fb = F(Bq);
			if (Fa <= 0) Out.Add(A);
			if ((Fa <= 0) != (Fb <= 0)) { const double T = Fa / (Fa - Fb); Out.Add(A + (Bq - A) * T); }
		}
		Poly = Out;
	}
	if (Poly.Num() < 3) return;
	FVector2D C(0.0, 0.0);
	for (const FVector2D& P : Poly) C += P;
	C /= Poly.Num();
	for (int32 i = 0; i < Poly.Num(); ++i)
	{
		const FVector2D& P = Poly[i], &Q = Poly[(i + 1) % Poly.Num()];
		Tri(B, U(C.X, Y, C.Y), U(Q.X, Y, Q.Y), U(P.X, Y, P.Y));
	}
}

void ATowerADC::Tube(FBuf& B, const TArray<FVector>& Path, float Radius, int32 Sides)
{
	// Path in tower metres (x, y, z); round it a ring of Sides points, joined ring to ring
	for (int32 i = 0; i + 1 < Path.Num(); ++i)
	{
		const FVector A = U(Path[i].X, Path[i].Y, Path[i].Z), Bp = U(Path[i + 1].X, Path[i + 1].Y, Path[i + 1].Z);
		const FVector Dir = (Bp - A).GetSafeNormal();
		if (Dir.IsNearlyZero()) continue;
		const FVector Side = FVector::CrossProduct(Dir, FMath::Abs(Dir.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
		const FVector Up = FVector::CrossProduct(Side, Dir);
		const double R = Radius * 100.0;
		for (int32 k = 0; k < Sides; ++k)
		{
			const float T0 = TwoPi * k / Sides, T1 = TwoPi * (k + 1) / Sides;
			const FVector O0 = (Side * FMath::Cos(T0) + Up * FMath::Sin(T0)) * R, O1 = (Side * FMath::Cos(T1) + Up * FMath::Sin(T1)) * R;
			Tri(B, A + O0, Bp + O0, A + O1);
			Tri(B, A + O1, Bp + O0, Bp + O1);
		}
	}
}

UProceduralMeshComponent* ATowerADC::Shells(USceneComponent* Parent, TMap<FName, FBuf>& Set, bool bCollide, bool bShadow, const FName Name)
{
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this, Name);
	P->SetupAttachment(Parent);
	P->SetMobility(EComponentMobility::Movable);
	P->bUseComplexAsSimpleCollision = true;
	int32 Section = 0;
	for (TPair<FName, FBuf>& E : Set)
	{
		if (E.Value.T.Num() == 0) continue;
		P->CreateMeshSection_LinearColor(Section, E.Value.V, E.Value.T, E.Value.N, E.Value.UV, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), bCollide);
		P->SetMaterial(Section, Mat(E.Key));
		++Section;
	}
	P->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	P->SetCastShadow(bShadow);
	P->RegisterComponent();
	AddInstanceComponent(P);
	Set.Empty();
	return P;
}

#pragma endregion

#pragma region Build

void ATowerADC::Build()
{
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!Cube || !Cylinder) return;
	// the tower's frame: its foot at its place on the plan (facing as the plan's x and z)
	Frame = Node(Root, FTerminalPlan::ToWorld(AtX, 0, AtZ), FRotator::ZeroRotator, TEXT("ADC_TOWER"));

	BuildFoot();
	BuildShaftAndLobby();
	BuildStair();
	BuildCab();
	BuildDesk();
	BuildRoof();
	BuildNightLights();

	Shells(Frame, Solid, true, true, TEXT("ADC_SHELLS"));
	// the glass split: its outward faces (seen from outside) and its inward faces (seen from inside, a clearer glass);
	// a face is outward when its normal points away from the tower's middle
	{
		TMap<FName, FBuf> Out, In;
		for (TPair<FName, FBuf>& E : Glass)
		{
			const FBuf& B = E.Value;
			FBuf& O = Out.FindOrAdd(E.Key);
			FBuf& I = In.FindOrAdd(E.Key == "RAILGLASS" ? FName("INRAILGLASS") : FName("INGLASS"));
			for (int32 t = 0; t + 2 < B.T.Num(); t += 3)
			{
				const int32 A = B.T[t];
				const FVector Mid = (B.V[B.T[t]] + B.V[B.T[t + 1]] + B.V[B.T[t + 2]]) / 3.0;
				const bool bOut = FVector::DotProduct(FVector(B.N[A].X, B.N[A].Y, 0), FVector(Mid.X, Mid.Y, 0)) >= 0;
				FBuf& D = bOut ? O : I;
				for (int32 k = 0; k < 3; ++k)
				{
					const int32 S = B.T[t + k];
					D.T.Add(D.V.Num()); D.V.Add(B.V[S]); D.N.Add(B.N[S]); D.UV.Add(B.UV[S]);
				}
			}
		}
		Glass.Empty();
		GlassOutside = Shells(Frame, Out, true, false, TEXT("ADC_GLASS_OUT"));
		Shells(Frame, In, false, false, TEXT("ADC_GLASS_IN"));
	}
	Shells(Frame, Trim, false, true, TEXT("ADC_TRIM"));

	// the lift, from the ground floor to the lift lobby, its doors facing west, against the shaft's north wall
	if (ATerminalLifts* Lifts = GetWorld()->SpawnActor<ATerminalLifts>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		FLiftSpec S;
		S.P.Id = TEXT("ADC");
		S.P.X = AtX; S.P.Z = AtZ + LiftZ; S.P.Face = -PI / 2;
		S.Lo = 0.4f; S.Hi = LobbyY; S.Top = LobbyY + 2.75f;
		S.Out = LiftOut; S.Car = 0.76f; S.Door = 0.45f; S.Speed = 2.0f;
		S.Names[0] = TEXT("G"); S.Names[1] = TEXT("ATC");
		S.Sign = TEXT("TOWER LIFT");
		Lifts->AddLift(S);
	}

	// light inside: the ground floor, the lift lobby, the cab (Lumen does the rest)
	for (const FVector3f& L : {FVector3f(-1.5f, 3.2f, -1.f), FVector3f(0, LobbyTop - 0.3f, -2.f), FVector3f(0, CabTop - 0.4f, 0), FVector3f(3.5f, CabTop - 0.4f, 0), FVector3f(-3.5f, CabTop - 0.4f, 0)})
	{
		UPointLightComponent* P = NewObject<UPointLightComponent>(this);
		P->SetupAttachment(Frame);
		P->SetMobility(EComponentMobility::Movable);
		P->SetRelativeLocation(U(L.X, L.Y, L.Z));
		P->SetIntensityUnits(ELightUnits::Lumens);
		P->SetIntensity(700.f);
		P->SetAttenuationRadius(900.f);
		P->SetLightColor(FLinearColor(1.f, 0.93f, 0.84f));
		P->SetCastShadows(true);   // kept inside the tower
		P->RegisterComponent();
		AddInstanceComponent(P);
	}
	// what Unreal's lighting (Lumen) can't see in the curved shells, told to it with invisible pieces: the roof and the
	// cab's floor (thin discs) and the shaft's wall (a ring of thin slabs), so no open sky floods the inside
	if (UMaterialInterface* Occ = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Lumen/M_LumenOccluder.M_LumenOccluder")))
	{
		auto Hidden = [&](UStaticMesh* Mesh, const FTransform& T)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
			C->SetupAttachment(Frame);
			C->SetStaticMesh(Mesh);
			C->SetMaterial(0, Occ);
			C->SetRelativeTransform(T);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(false);
			C->RegisterComponent();
			AddInstanceComponent(C);
		};
		// (the engine cylinder is 100 cm across and tall, its pivot in the middle)
		Hidden(Cylinder, FTransform(FRotator::ZeroRotator, U(0, RoofTop - 0.25f, 0), FVector(RoofR * 2, RoofR * 2, 0.4f)));
		Hidden(Cylinder, FTransform(FRotator::ZeroRotator, U(0, CabY - 0.3f, 0), FVector(CabR0 * 2, CabR0 * 2, 0.4f)));
		for (int32 i = 0; i < 16; ++i)
		{
			const float A = TwoPi * i / 16, R = ShaftR + 0.05f;
			const float Side = 2.f * R * FMath::Tan(PI / 16) + 0.05f;
			Hidden(Cube, FTransform(FRotator(0, -FMath::RadiansToDegrees(A), 0), U(FMath::Cos(A) * R, LobbyTop / 2, FMath::Sin(A) * R), FVector(0.3f, Side, LobbyTop)));
		}
	}
	// the forecourt: paved like the apron from the tower's foot east to the apron's west edge (plan x -160), so the
	// tower stands on the airfield's paving, not alone in the grass
	Box("apron", 62.f, 0.1f, 26.f, 19.f, -0.03f, 0.f);
	UE_LOG(LogTemp, Log, TEXT("Control tower built at plan x %.0f z %.0f"), AtX, AtZ);
}

#pragma region The foot
void ATowerADC::BuildFoot()
{
	// the plinth and its terrazzo apron
	Ring(Buf(Solid, "terrazzo"), 0.f, 0.02f, ShaftR + 1.2f, 0.02f, 0, TwoPi, 64);
	Cyl("concrete", ShaftR + 0.6f, 0.4f, 0, 0.2f, 0);
	// the entrance hall: its side walls and roof, the ramp up to the plinth, its floor on the plinth
	for (const float SZ : {-1.f, 1.f}) Box("white", EX1 - EX0, 3.6f, 0.2f, (EX0 + EX1) / 2, 1.8f, SZ * (EZ + 0.1f));
	Box("white", EX1 - EX0, 0.2f, EZ * 2 + 0.4f, (EX0 + EX1) / 2, 3.5f, 0);
	{
		const float Run = -4.2f - EX0, L = FMath::Sqrt(Run * Run + 0.38f * 0.38f);
		Box("terrazzo", L, 0.06f, EZ * 2, (EX0 - 4.2f) / 2, 0.19f, 0, 0, true, FMath::RadiansToDegrees(FMath::Atan2(0.38f, Run)));
	}
	Box("terrazzo", EX1 + 4.2f, 0.02f, EZ * 2, (EX1 - 4.2f) / 2, 0.41f, 0);
	// the glass front: fixed panes either side of the doors, the glass over them, the door head
	for (const float SZ : {-1.f, 1.f}) Box("GLASS", 0.06f, 2.4f, EZ - 0.8f, EX0 + 0.04f, 1.2f, SZ * (0.8f + (EZ - 0.8f) / 2));
	Box("GLASS", 0.06f, 1.0f, EZ * 2, EX0 + 0.04f, 2.9f, 0);
	Box("black", 0.1f, 0.08f, EZ * 2, EX0 + 0.04f, 2.42f, 0);
	// the two sliding leaves (walked through: they open as you come)
	for (const float SZ : {-1.f, 1.f})
	{
		USceneComponent* N = Node(Frame, U(EX0 + 0.12f, 0, SZ * 0.4f), FRotator::ZeroRotator, SZ < 0 ? FName("ADC_ENTRANCE_DOOR_S") : FName("ADC_ENTRANCE_DOOR_N"));
		Part(N, Cube, "GLASS", 0.04f, 2.38f, 0.8f, 0, 1.19f, 0, false);
		Part(N, Cube, "steel", 0.06f, 2.38f, 0.05f, 0, 1.19f, -SZ * 0.38f, false);
		EntranceDoors.Add(N);
	}
	// the timber canopy over it, and the sign
	const float Ex = -ShaftR - 1.6f;
	Box("timber", 4.6f, 0.25f, 6.4f, Ex - 0.2f, 3.75f, 0);
	Box("white", 4.62f, 0.08f, 6.42f, Ex - 0.2f, 3.92f, 0);
	Box("navy", 0.04f, 0.42f, 3.0f, EX0 - 0.04f, 3.2f, 0, 0, false);
	UTextRenderComponent* W = NewObject<UTextRenderComponent>(this);
	W->SetupAttachment(Frame);
	W->SetText(FText::FromString(TEXT("CONTROL TOWER · ATC")));
	W->SetHorizontalAlignment(EHTA_Center);
	W->SetVerticalAlignment(EVRTA_TextCenter);
	W->SetWorldSize(26.f);
	W->SetTextRenderColor(FColor(242, 244, 247));
	W->SetRelativeLocationAndRotation(U(EX0 - 0.07f, 3.2f, 0), FRotator(0, 180.f, 0)); // facing west, out of the entrance
	W->RegisterComponent();
	AddInstanceComponent(W);
}
#pragma endregion

#pragma region The shaft and the lift lobby
void ATowerADC::BuildShaftAndLobby()
{
	// the shaft: white, the way in from the entrance hall on its west side, a slot of glass up the apron (east) side
	const float GapA = FMath::Asin(EZ / ShaftR);
	Ring(Buf(Solid, "white"), ShaftR, 0.4f, ShaftR, LobbyY, PI + GapA, PI - GapA + TwoPi, 96);
	Ring(Buf(Solid, "white"), ShaftR, 3.6f, ShaftR, LobbyY, PI - GapA, PI + GapA, 12);
	Ring(Buf(Glass, "GLASS"), ShaftR + 0.01f, 1.2f, ShaftR + 0.01f, LobbyY - 0.4f, -0.28f, 0.28f, 12);
	// the timber fins all round (not over the glass slot or the way in)
	for (int32 i = 0; i < 28; ++i)
	{
		const float A = TwoPi * i / 28;
		if (FMath::Abs(FMath::Atan2(FMath::Sin(A), FMath::Cos(A))) < 0.32f) continue;
		if (FMath::Abs(FMath::Atan2(FMath::Sin(A - PI), FMath::Cos(A - PI))) < GapA + 0.12f) continue;
		Box("timber", 0.16f, LobbyY - 1.6f, 0.42f, FMath::Cos(A) * (ShaftR + 0.2f), 0.8f + (LobbyY - 1.6f) / 2 + 0.4f, FMath::Sin(A) * (ShaftR + 0.2f), -A);
	}
	// the escape stair up the shaft's south side (seen, not walked)
	for (int32 s = 0; s < 60; ++s)
	{
		const float A = s * 0.42f, R = 2.4f, Y = 0.6f + s * (LobbyY - 0.8f) / 60;
		if (FMath::Sin(A) > 0.3f) continue;
		Box("concrete", 1.0f, 0.06f, 0.32f, FMath::Cos(A) * R, Y, FMath::Sin(A) * R, -A, false);
	}
	// the lobby level: a touch wider, a band of windows round it
	Ring(Buf(Solid, "timber"), ShaftR, LobbyY - 0.2f, LR, LobbyY - 0.2f, 0, TwoPi, 96);
	Ring(Buf(Solid, "white"), LR, LobbyY - 0.2f, LR, LobbyY + 1.1f, 0, TwoPi, 96);
	Ring(Buf(Glass, "GLASS"), LR, LobbyY + 1.1f, LR, LobbyY + 2.9f, 0, TwoPi, 96);
	Ring(Buf(Solid, "white"), LR, LobbyY + 2.9f, LR, LobbyTop, 0, TwoPi, 96);
	for (int32 i = 0; i < 20; ++i) { const float A = TwoPi * i / 20; Box("black", 0.06f, 1.8f, 0.06f, FMath::Cos(A) * LR, LobbyY + 2.0f, FMath::Sin(A) * LR, 0, false); }
	// its floor, open over the lift's shaft (the car rises into it); its ceiling round the stair's opening
	{
		const float Z0 = LiftZ - LiftOut, Z1 = LiftZ + LiftOut;
		FBuf& F = Buf(Solid, "carpetGrey");
		DiscPiece(F, LR, LobbyY + 0.01f, {FVector(0.0, 1.0, Z0)});
		DiscPiece(F, LR, LobbyY + 0.01f, {FVector(0.0, -1.0, -Z0), FVector(1.0, 0.0, -LiftOut)});
		DiscPiece(F, LR, LobbyY + 0.01f, {FVector(0.0, -1.0, -Z0), FVector(-1.0, 0.0, -LiftOut)});
		DiscPiece(F, LR, LobbyY + 0.01f, {FVector(0.0, -1.0, -Z1), FVector(1.0, 0.0, LiftOut), FVector(-1.0, 0.0, LiftOut)});
	}
	Ring(Buf(Solid, "plaster"), 1.45f, LobbyTop - 0.01f, LR, LobbyTop - 0.01f, 0, TwoPi, 96);
	// lockers along the wall, a small kitchen
	for (int32 i = 0; i < 6; ++i) { const float A = PI * 0.95f + i * 0.13f, R = LR - 0.35f; Box("steel", 0.55f, 2.0f, 0.5f, FMath::Cos(A) * R, LobbyY + 1.0f, FMath::Sin(A) * R, -A + PI / 2); }
	{
		const float A = PI * 1.45f, R = LR - 0.38f;
		Box("walnut", 2.2f, 0.9f, 0.6f, FMath::Cos(A) * R, LobbyY + 0.45f, FMath::Sin(A) * R, -A + PI / 2);
		Box("white", 2.25f, 0.04f, 0.65f, FMath::Cos(A) * R, LobbyY + 0.92f, FMath::Sin(A) * R, -A + PI / 2);
	}
	for (int32 i = 0; i < 8; ++i) { const float A = TwoPi * i / 8; Cyl("light", 0.1f, 0.02f, FMath::Cos(A) * 2.9f, LobbyTop - 0.02f, FMath::Sin(A) * 2.9f, false); }
}
#pragma endregion

#pragma region The spiral stair
void ATowerADC::BuildStair()
{
	// round its column, from the lobby's floor up through the cab's: oak treads (walked on: each step rises 19 cm),
	// slim balusters, a walnut handrail; a landing at the top by the opening in the glass balustrade
	const float Rise = CabY - LobbyY;
	Cyl("steel", 0.14f, Rise + 1.1f, 0, LobbyY + (Rise + 1.1f) / 2, 0);
	for (int32 s = 0; s < Steps; ++s)
	{
		const float A = (float)s / Steps * Turn, Y = LobbyY + (s + 1) * Rise / Steps;
		Box("oak", SR - 0.15f, 0.05f, 0.42f, FMath::Cos(A) * (SR / 2 + 0.07f), Y - 0.025f, FMath::Sin(A) * (SR / 2 + 0.07f), -A);
		Cyl("steel", 0.012f, 1.0f, FMath::Cos(A) * (SR - 0.06f), Y + 0.5f, FMath::Sin(A) * (SR - 0.06f), false);
	}
	TArray<FVector> Rail;
	for (int32 k = 0; k <= 60; ++k) { const float A = k / 60.f * Turn; Rail.Add(FVector(FMath::Cos(A) * (SR - 0.06f), LobbyY + 1.0f + k / 60.f * Rise, FMath::Sin(A) * (SR - 0.06f))); }
	Tube(Buf(Trim, "walnut"), Rail, 0.025f);
	Ring(Buf(Solid, "oak"), 0.14f, CabY, 1.45f, CabY, Turn, TwoPi + 0.45f, 24);
	// the opening in the cab's floor: its edge, and the glass balustrade round it (open at the stair's top)
	Ring(Buf(Solid, "white"), 1.45f, CabY - 0.3f, 1.45f, CabY, 0, TwoPi, 48);
	Ring(Buf(Glass, "GLASS"), 1.45f, CabY, 1.45f, CabY + 1.0f, 0.45f, Turn, 48);
}
#pragma endregion

#pragma region The cab and its balcony
void ATowerADC::BuildCab()
{
	const float RR = RoofR - 0.1f;
	// under the cab (wider than everything below it), its edge, the walkway (the balcony) with its glass railing
	Ring(Buf(Solid, "timber"), LR, CabY - 0.32f, RR, CabY - 0.32f, 0, TwoPi, 128);
	Ring(Buf(Solid, "white"), RR, CabY - 0.32f, RR, CabY - 0.05f, 0, TwoPi, 128);
	Ring(Buf(Solid, "rubber"), CabR0, CabY - 0.05f, RR, CabY - 0.05f, 0, TwoPi, 128);
	Ring(Buf(Glass, "RAILGLASS"), RoofR - 0.12f, CabY - 0.05f, RoofR - 0.12f, CabY + 1.05f, 0, TwoPi, 128);
	{
		TArray<FVector> Top;
		for (int32 k = 0; k <= 128; ++k) { const float A = TwoPi * k / 128; Top.Add(FVector(FMath::Cos(A) * (RoofR - 0.12f), CabY + 1.07f, FMath::Sin(A) * (RoofR - 0.12f))); }
		Tube(Buf(Solid, "steel"), Top, 0.035f, 10);
		// the railing's posts
		for (int32 i = 0; i < 32; ++i) { const float A = TwoPi * i / 32; Cyl("steel", 0.025f, 1.1f, FMath::Cos(A) * (RoofR - 0.14f), CabY + 0.5f, FMath::Sin(A) * (RoofR - 0.14f), false); }
	}
	// the cab's floor, round the stair's opening
	Ring(Buf(Solid, "carpetGrey"), 1.45f, CabY, CabR0, CabY, 0, TwoPi, 128);
	// the door out to the walkway (east, between two mullions): the upstand and the glass leave a gap for it
	const float DHalf = DoorW / 2 / CabR0 + 0.02f, DA0 = DoorA - DHalf, DA1 = DoorA + DHalf, DTop = CabY + DoorH;
	auto RAt = [](float Y) { return CabR0 + 0.02f + (CabR1 - CabR0 - 0.02f) * (Y - GlassY) / (CabTop - GlassY); };
	Ring(Buf(Solid, "white"), CabR0, CabY, CabR0 + 0.04f, GlassY, DA1, DA0 + TwoPi, 120);
	Ring(Buf(Solid, "whiteGloss"), CabR0 - 0.08f, CabY, CabR0 - 0.04f, GlassY, DA1, DA0 + TwoPi, 120);
	Ring(Buf(Glass, "GLASS"), CabR0 + 0.02f, GlassY, CabR1, CabTop, DA1, DA0 + TwoPi, 124);
	Ring(Buf(Glass, "GLASS"), RAt(DTop + 0.08f), DTop + 0.08f, CabR1, CabTop, DA0, DA1, 6);
	// the glass's thin black mullions, leaning out with it
	for (int32 i = 0; i < 24; ++i)
	{
		const float A = TwoPi * i / 24;
		Tube(Buf(Solid, "black"), {FVector(FMath::Cos(A) * (CabR0 + 0.02f), GlassY, FMath::Sin(A) * (CabR0 + 0.02f)), FVector(FMath::Cos(A) * CabR1, CabTop, FMath::Sin(A) * CabR1)}, 0.035f, 6);
	}
	// the door: its frame, threshold and step, and its glass leaf hinged on its left, opening out onto the walkway
	{
		const float Dr = CabR0 + 0.02f, Yaw = PI / 2 - DoorA, W = DHalf * 2 * Dr;
		const float OX = FMath::Cos(DoorA) * Dr, OZ = FMath::Sin(DoorA) * Dr;
		for (const float SX : {-1.f, 1.f}) BoxIn("black", 0.08f, DoorH + 0.08f, 0.14f, OX, CabY, OZ, Yaw, SX * (W / 2 - 0.04f), (DoorH + 0.08f) / 2, 0, true);
		BoxIn("black", W, 0.08f, 0.14f, OX, CabY, OZ, Yaw, 0, DoorH + 0.04f, 0, true);
		BoxIn("steel", W, 0.03f, 0.2f, OX, CabY, OZ, Yaw, 0, 0.015f, 0, true);
		Ring(Buf(Solid, "steel"), CabR0 - 0.05f, CabY - 0.04f, CabR0 + 0.25f, CabY - 0.04f, DA0, DA1, 6);
		const float HX = -(W / 2 - 0.1f), C = FMath::Cos(Yaw), S = FMath::Sin(Yaw);
		// the hinge: in the door's frame at x = HX, turned with the door
		const FVector HingeAt(OX + HX * C, CabY, OZ - HX * S);
		USceneComponent* Door = Node(Frame, U(HingeAt.X, HingeAt.Y, HingeAt.Z), FRotator(0, FMath::RadiansToDegrees(Yaw), 0), TEXT("ADC_BALCONY_DOOR"));
		BalconyLeaf = Node(Door, FVector::ZeroVector, FRotator::ZeroRotator, TEXT("ADC_BALCONY_DOOR_LEAF"));
		Part(BalconyLeaf, Cube, "GLASS", W - 0.2f, DoorH - 0.1f, 0.03f, -HX, DoorH / 2, 0, false);
		Part(BalconyLeaf, Cube, "black", W - 0.16f, 0.06f, 0.05f, -HX, 0.06f, 0, false);
		Part(BalconyLeaf, Cube, "black", W - 0.16f, 0.06f, 0.05f, -HX, DoorH - 0.06f, 0, false);
		for (const float SZ : {-1.f, 1.f}) Part(BalconyLeaf, Cube, "chrome", 0.03f, 0.5f, 0.03f, W / 2 - 0.22f - HX, 1.05f, SZ * 0.07f, false);
		// which way the leaf turns to swing out (its free end towards the walkway)
		const FVector Out = U(FMath::Cos(DoorA), 0, FMath::Sin(DoorA));
		const FVector Tip = FRotator(0, -80.f, 0).RotateVector(FVector(100.f, 0, 0));
		const FVector TipWorld = FRotator(0, FMath::RadiansToDegrees(Yaw), 0).RotateVector(Tip);
		DoorSign = FVector::DotProduct(TipWorld, Out) > 0 ? -1.f : 1.f;
	}
	// the cab's lights
	for (int32 i = 0; i < 16; ++i) { const float A = TwoPi * i / 16, R = CabR1 - 0.4f; Cyl("light", 0.08f, 0.02f, FMath::Cos(A) * R, CabTop - 0.02f, FMath::Sin(A) * R, false); }
}
#pragma endregion

#pragma region The control desk
void ATowerADC::BuildDesk()
{
	// one continuous desk round the cab; in front of the balcony door its top is a lift-up flap (the same lacquer and
	// lip, so the ring still reads as one), the way through under it to the door
	const float DY = CabY, R0 = DeskR0, R1 = DeskR1, FHalf = 0.5f / R0, FA0 = DoorA - FHalf, FA1 = DoorA + FHalf;
	Ring(Buf(Solid, "lacquer"), R0, DY + 0.78f, R1, DY + 0.78f, FA1, FA0 + TwoPi, 186);
	Ring(Buf(Solid, "walnut"), R0, DY + 0.08f, R0, DY + 0.78f, FA1, FA0 + TwoPi, 186);
	Ring(Buf(Solid, "white"), R1, DY + 0.08f, R1, DY + 0.78f, FA1, FA0 + TwoPi, 186);
	Ring(Buf(Solid, "black"), R0 + 0.06f, DY, R0 + 0.06f, DY + 0.08f, FA1, FA0 + TwoPi, 186);
	Ring(Buf(Solid, "black"), R1 - 0.22f, DY + 0.78f, R1 - 0.22f, DY + 0.86f, FA1, FA0 + TwoPi, 186);   // the screens' riser
	{
		TArray<FVector> Lip;
		for (int32 k = 0; k <= 186; ++k) { const float A = FA1 + (FA0 + TwoPi - FA1) * k / 186; Lip.Add(FVector(FMath::Cos(A) * R0, DY + 0.76f, FMath::Sin(A) * R0)); }
		Tube(Buf(Solid, "walnut"), Lip, 0.03f);
	}
	for (const float EA : {FA0, FA1}) Box("walnut", R1 - R0, 0.7f, 0.04f, FMath::Cos(EA) * (R0 + R1) / 2, DY + 0.43f, FMath::Sin(EA) * (R0 + R1) / 2, -EA);
	// the flap, on its hinge at the desk's back
	{
		const FVector HingeAt = U(FMath::Cos(DoorA) * R1, DY + 0.78f, FMath::Sin(DoorA) * R1);
		FlapHinge = Node(Frame, HingeAt, FRotator::ZeroRotator, TEXT("ADC_DESK_FLAP"));
		USceneComponent* In = Node(FlapHinge, -HingeAt, FRotator::ZeroRotator);
		TMap<FName, FBuf> F;
		Ring(Buf(F, "lacquer"), R0, DY + 0.78f, R1, DY + 0.78f, FA0 + 0.002f, FA1 - 0.002f, 6);
		TArray<FVector> Lip;
		for (int32 k = 0; k <= 8; ++k) { const float A = FA0 + 0.002f + (FA1 - FA0 - 0.004f) * k / 8; Lip.Add(FVector(FMath::Cos(A) * R0, DY + 0.76f, FMath::Sin(A) * R0)); }
		Tube(Buf(F, "walnut"), Lip, 0.03f);
		Shells(In, F, false, true, TEXT("ADC_DESK_FLAP_TOP"));
		// it lifts about the desk's back edge, its front rising towards the glass
		const FVector Axis = U(-FMath::Sin(DoorA), 0, FMath::Cos(DoorA)).GetSafeNormal();
		const FVector Front = U(-FMath::Cos(DoorA), 0, -FMath::Sin(DoorA)).GetSafeNormal();
		const float A = FMath::DegreesToRadians(83.f);
		FlapOpen = FQuat(Axis, A);
		if (FlapOpen.RotateVector(Front).Z < 0) FlapOpen = FQuat(Axis, -A);
	}
	// the screens: three per position, all following the circle (none on the flap), a keyboard, radio and strip at each
	for (int32 p = 0; p < Positions; ++p)
		for (int32 k = -1; k <= 1; ++k)
		{
			const float A = TwoPi * p / Positions + k * (TwoPi / Positions / 3);
			if (FMath::Abs(FMath::Atan2(FMath::Sin(A - DoorA), FMath::Cos(A - DoorA))) < FHalf + 0.05f) continue;
			const float R = R1 - 0.25f, CX = FMath::Cos(A) * R, CZ = FMath::Sin(A) * R, Yaw = PI / 2 - A, Y = DY + 0.86f;
			BoxIn("steel", 0.06f, 0.22f, 0.04f, CX, Y, CZ, Yaw, 0, 0.11f, 0.02f);
			BoxIn("screenBlack", 0.5f, 0.32f, 0.03f, CX, Y, CZ, Yaw, 0, 0.38f, 0);
			BoxIn("tealUI", 0.47f, 0.29f, 0.005f, CX, Y, CZ, Yaw, 0, 0.38f, -0.018f);
			if (k == 0)
			{
				BoxIn("black", 0.44f, 0.02f, 0.15f, CX, Y, CZ, Yaw, 0, -0.07f, -0.42f);
				BoxIn("screenBlack", 0.22f, 0.05f, 0.16f, CX, Y, CZ, Yaw, 0.36f, -0.06f, -0.3f);
				BoxIn("tealUI", 0.4f, 0.03f, 0.12f, CX, Y, CZ, Yaw, -0.36f, -0.065f, -0.32f);
			}
		}
	// the chairs: one at each second position, facing out to the glass, each its own object (swap for a better model:
	// same origin, the floor under the seat, facing out)
	UStaticMesh* Cyl1 = Cylinder;
	for (int32 c = 0; c < Chairs; ++c)
	{
		float A = TwoPi * c / Chairs + PI / Positions;
		const float Da = FMath::Atan2(FMath::Sin(A - DoorA), FMath::Cos(A - DoorA));
		if (FMath::Abs(Da) < 0.3f) A += FMath::Sign(Da) * 0.07f;   // a clear way to the flap
		const float R = R0 - 0.55f;
		USceneComponent* Ch = Node(Frame, U(FMath::Cos(A) * R, DY, FMath::Sin(A) * R), FRotator(0, FMath::RadiansToDegrees(-A + PI / 2), 0), FName(*FString::Printf(TEXT("ADC_CHAIR_%02d"), c + 1)));
		Ch->ComponentTags.Add(TEXT("SwappableChair"));
		Part(Ch, Cyl1, "chrome", 0.62f, 0.04f, 0.62f, 0, 0.06f, 0, true);          // the five-star base, as a low disc
		Part(Ch, Cyl1, "chrome", 0.05f, 0.38f, 0.05f, 0, 0.26f, 0, false);        // the column
		Part(Ch, Cube, "leatherBlack", 0.5f, 0.09f, 0.48f, 0, 0.49f, 0, true);    // the seat
		Part(Ch, Cube, "leatherBlack", 0.48f, 0.66f, 0.08f, 0, 0.88f, -0.26f, true);
		Part(Ch, Cube, "leatherBlack", 0.3f, 0.16f, 0.08f, 0, 1.29f, -0.31f, false);
		for (const float SX : {-1.f, 1.f})
		{
			Part(Ch, Cube, "black", 0.05f, 0.03f, 0.3f, SX * 0.27f, 0.7f, 0, false);
			Part(Ch, Cube, "black", 0.03f, 0.2f, 0.03f, SX * 0.27f, 0.6f, -0.05f, false);
		}
	}
	// the supervisor's console by the stair
	Box("walnut", 1.6f, 0.78f, 0.7f, -2.4f, DY + 0.39f, 0, PI / 2);
	Box("lacquer", 1.64f, 0.04f, 0.74f, -2.4f, DY + 0.8f, 0, PI / 2);
}
#pragma endregion

#pragma region The roof
void ATowerADC::BuildRoof()
{
	Ring(Buf(Solid, "timber"), 0, CabTop, RoofR, CabTop, 0, TwoPi, 128);
	Ring(Buf(Solid, "white"), RoofR, CabTop, RoofR - 0.25f, RoofTop, 0, TwoPi, 128);
	Ring(Buf(Solid, "white"), 0, RoofTop, RoofR - 0.25f, RoofTop, 0, TwoPi, 128);
	// the mast with its antennas and the red obstruction light, the anemometer, a small radar dome
	Cyl("steel", 0.1f, 4.2f, 0, RoofTop + 2.1f, 0);
	Cyl("red", 0.18f, 0.25f, 0, RoofTop + 4.3f, 0, false);
	for (const FVector2f& HW : {FVector2f(1.2f, 1.4f), FVector2f(2.2f, 1.0f), FVector2f(3.1f, 0.7f)}) Box("steel", HW.Y, 0.04f, 0.04f, 0, RoofTop + HW.X, 0, 0.6f, false);
	Cyl("steel", 0.05f, 1.8f, 3.2f, RoofTop + 0.9f, 2.0f, false);
	Cyl("white", 0.7f, 0.7f, -3.0f, RoofTop + 0.35f, -1.6f);
}
#pragma endregion

#pragma endregion

#pragma region The doors and the flap

void ATowerADC::BuildNightLights()
{
	auto Spot = [this](const FVector& At, const FRotator& Aim, float Lumens, float Outer, float Reach)
	{
		USpotLightComponent* L = NewObject<USpotLightComponent>(this);
		L->SetupAttachment(Frame);
		L->SetMobility(EComponentMobility::Movable);
		L->SetRelativeLocationAndRotation(At, Aim);
		L->SetIntensityUnits(ELightUnits::Lumens);
		L->SetIntensity(Lumens);
		L->SetOuterConeAngle(Outer); L->SetInnerConeAngle(Outer * 0.55f);
		L->SetAttenuationRadius(Reach);
		L->SetUseTemperature(true); L->SetTemperature(3500.f);
		L->SetCastShadows(false);
		L->SetVisibility(false);
		L->RegisterComponent();
		AddInstanceComponent(L);
		NightLights.Add(L);
	};
	// the balcony: 24 small recessed downlights in the overhang's underside, just inside its edge, leaning a little
	// outwards so their cones fall on the walkway and the railing, not on the cab's windows behind
	const float BR = RoofR - 0.35f;
	for (int32 k = 0; k < 24; ++k)
	{
		const float A = TwoPi * (k + 0.5f) / 24;
		const FVector At = U(FMath::Cos(A) * BR, CabTop - 0.03f, FMath::Sin(A) * BR);
		Cyl("lamp", 0.06f, 0.02f, FMath::Cos(A) * BR, CabTop - 0.02f, FMath::Sin(A) * BR, false);
		const FVector Out = U(FMath::Cos(A), 0, FMath::Sin(A)).GetSafeNormal();
		const FVector Dir = (FVector(0, 0, -1) + Out * 0.18f).GetSafeNormal();
		Spot(At, Dir.Rotation(), 260.f, 26.f, 600.f);
	}
	// the entrance: six under the timber canopy, in two rows of three
	const float Ex = -ShaftR - 1.6f - 0.2f;
	for (const float DX : {-1.1f, 1.1f})
		for (const float DZ : {-2.1f, 0.f, 2.1f})
		{
			Cyl("lamp", 0.07f, 0.02f, Ex + DX, 3.61f, DZ, false);
			Spot(U(Ex + DX, 3.6f, DZ), FRotator(-90.f, 0, 0), 420.f, 40.f, 700.f);
		}
}

void ATowerADC::Tick(float Dt)
{
	Super::Tick(Dt);
	// the outside lights: on at sunset, off just after sunrise (the light masts' sensor), and in fog and storms
	{
		static TWeakObjectPtr<ADirectionalLight> Sun;
		if (!Sun.IsValid()) Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass()));
		const float El = Sun.IsValid() ? -Sun->GetActorRotation().Pitch : 30.f;
		const bool bWant = ASunClock::Theme == 4 || ASunClock::Theme == 6 || (bNightOn ? El < 3.f : El < 0.f);
		if (bWant != bNightOn) { bNightOn = bWant; for (USpotLightComponent* L : NightLights) if (L) L->SetVisibility(bNightOn); }
	}
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player || !Frame) return;
	// the player in the tower's frame (metres: x east, y the feet's height, z north)
	const FVector P = Player->GetActorLocation();
	const float LX = (float)(P.X / 100.0) - AtX, LZ = (float)(-P.Y / 100.0) - AtZ, PY = (float)((P.Z - Player->GetSimpleCollisionHalfHeight()) / 100.0);
	auto Toward = [Dt](float K, bool bOpen, float Speed) { return FMath::Clamp(K + (bOpen ? Dt : -Dt) * Speed, 0.f, 1.f); };

	EK = Toward(EK, PY < 2.5f && FMath::Abs(LX - EX0) < 3.2f && FMath::Abs(LZ) < 2.6f, 1.6f);
	const bool bAtCab = PY > CabY - 1.2f && PY < CabY + 2.5f;
	const float DCX = FMath::Cos(DoorA) * CabR0, DCZ = FMath::Sin(DoorA) * CabR0;
	BK = Toward(BK, bAtCab && FMath::Sqrt(FMath::Square(LX - DCX) + FMath::Square(LZ - DCZ)) < 2.3f, 1.2f);
	FK = Toward(FK, bAtCab && FMath::Sqrt(FMath::Square(LX - FMath::Cos(DoorA) * 5.f) + FMath::Square(LZ - FMath::Sin(DoorA) * 5.f)) < 2.1f, 1.4f);
	// inside the tower (the foot, the lift lobby, the cab): the glass's outward faces not drawn, only its clear inner ones
	{
		const float R = FMath::Sqrt(LX * LX + LZ * LZ);
		const bool bInside = (PY < LobbyY - 1.f && R < ShaftR + 0.2f) || (PY >= LobbyY - 1.f && PY < CabY - 0.3f && R < LR) || (PY >= CabY - 0.3f && PY < RoofTop && R < CabR0);
		if (bInside != bInsideLast && GlassOutside) { GlassOutside->SetVisibility(!bInside); bInsideLast = bInside; }
	}

	for (int32 i = 0; i < EntranceDoors.Num(); ++i)
		if (USceneComponent* D = EntranceDoors[i]) D->SetRelativeLocation(U(EX0 + 0.12f, 0, (i ? 1.f : -1.f) * (0.4f + Ease(EK) * 0.78f)));
	if (BalconyLeaf) BalconyLeaf->SetRelativeRotation(FRotator(0, DoorSign * 89.f * Ease(BK), 0));
	if (FlapHinge) FlapHinge->SetRelativeRotation(FQuat::Slerp(FQuat::Identity, FlapOpen, Ease(FK)));
}

#pragma endregion
