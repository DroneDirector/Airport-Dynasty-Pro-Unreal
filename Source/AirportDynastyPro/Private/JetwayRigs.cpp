#include "JetwayRigs.h"
#include "AircraftDoors.h"
#include "InteractOutline.h"
#include "Sound/SoundAttenuation.h"
#include "BabylonModel.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "ADPAnimations.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ADPKeep.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Engine/DirectionalLight.h"
#include "Kismet/GameplayStatics.h"
#include "SunClock.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/PlayerCameraManager.h"

float AJetwayRigs::Speed = 1.f;
bool AJetwayRigs::bBeaconTest = false;
FString AJetwayRigs::LastNote;
float AJetwayRigs::SignGlow = 3.f;
int32 AJetwayRigs::BeaconForce = -1;

namespace ADPJetwayInternal
{
	// his generator's measures (jetway.ts), metres
	constexpr float RR = 1.8f, CR = 1.7f, BD = 1.25f, OV = 1.5f;
	constexpr float SEC1 = 10.5f, SEC2 = 10.1f, SEC3 = 9.7f, ZW = SEC3 - 1.0f;
	constexpr float LMIN = SEC1 + 2 * BD, LMAX = SEC1 + SEC2 + SEC3 - 2 * OV + 2 * BD;
	constexpr float PARK = 0.5f;                                               // at rest: wheels half as far out (his rule)
	// real speeds (owner 2026-11-16: "real speeds"): an apron-drive bridge travels at up to 60 ft/min = 0.30 m/s
	// (Oshkosh AeroTech Jetway sheet; AviaBridge 3T: 0.4 m/s), its cab turns 145 degrees a minute (2.4 a second), it
	// lifts 1.1 m a minute; the last 1.5 m to the door (and the first 1.5 m away) at a creep, 0.05 m/s, as the operator
	// inches it in; the canopy 15 s each way (his generator)
	constexpr float VMax = 0.30f, VCreep = 0.05f, Acc = 0.10f, Creep = 1.5f, CabTurn = 145.f / 60.f * PI / 180.f, Lift = 1.1f / 60.f, TCanopy = 15.f;
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
		// (the real-world order, owner 2026-11-17: the smallest section at the rotunda slides inside the middle one, which
		//  slides inside the cab end's - so the middle one never ends before the first one's front: Z1 + SEC1 - SEC2)
		const float Lo = FMath::Max3(Z1, Z1 + SEC1 - SEC2, Z3 + OV - SEC2), Hi = FMath::Min3(Z1 + SEC1 - OV, Z3, Z3 + SEC3 - SEC2);
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
	// the outside skin in dimmed copies of its finishes (owner, 2026-11-16: the outside washed out under the
	// floodlights, the inside bright as it is - "if the same is used outside as inside, make a copy for the outside"):
	// the cladding, roof and underside (outside only), jwPanel everywhere but the cab's console and the corridor, jwWall
	// on the rotunda's drum, the sleeves, the cab's doors and front (the corridor's, tunnels' and cab's own walls are
	// inside). The loader tones whites to 0.78; the outside copies to 0.35. One copy per finish, shared.
	{
		TMap<UMaterialInterface*, UMaterialInstanceDynamic*> Copy; TSet<UInstancedStaticMeshComponent*> Done; int32 N = 0;
		for (int32 I = 0; I < M->NodeCount(); ++I)
		{
			const FString& Nm = M->NodeName(I);
			const bool bClad = Nm.Contains(TEXT(":jwClad")) || Nm.EndsWith(TEXT(":jwRoof")) || Nm.EndsWith(TEXT(":jwUnder"));
			const bool bPanel = Nm.EndsWith(TEXT(":jwPanel")) && !Nm.Contains(TEXT("CONSOLE")) && !Nm.Contains(TEXT("CORRIDOR"));
			const bool bAlu = Nm.EndsWith(TEXT(":jwAluB")) && (Nm.Contains(TEXT("/CAB:")) || Nm.Contains(TEXT("/TUNNEL-")) || Nm.Contains(TEXT("SLEEVE")) || Nm.Contains(TEXT("/ROTUNDA:")) || Nm.Contains(TEXT("/LIFT-")) || Nm.Contains(TEXT("DRIVE-YOKE")) || Nm.Contains(TEXT("/CAB-FRONT")));
			const bool bWall = Nm.EndsWith(TEXT(":jwWall")) && (Nm.Contains(TEXT("ROTUNDA-DRUM")) || Nm.Contains(TEXT("SLEEVE")) || Nm.Contains(TEXT("CAB-DOOR")) || Nm.Contains(TEXT("CAB-FRONT")));
			if (!bClad && !bPanel && !bWall && !bAlu) continue;
			const TPair<UInstancedStaticMeshComponent*, int32> In = M->NodeInstance(I);
			if (!In.Key || Done.Contains(In.Key)) continue;
			Done.Add(In.Key);
			for (int32 m = 0; m < In.Key->GetNumMaterials(); ++m)
			{
				UMaterialInterface* Mi = In.Key->GetMaterial(m);
				if (!Mi) continue;
				const FString Mn = Mi->GetName();
				if (!Mn.Contains(TEXT("jwPanel")) && !Mn.Contains(TEXT("jwAluB")) && !Mn.Contains(TEXT("jwWall")) && !Mn.Contains(TEXT("jwClad")) && !Mn.Contains(TEXT("jwRoof")) && !Mn.Contains(TEXT("jwUnder"))) continue;
				// (only the part's own finish: since 2026-11-17 a part may also carry the inside walls' light grey plate in a
				//  second slot - Tools/jetway_finish.py - and the inside stays bright)
				{ FString Own; if (Nm.Split(TEXT(":"), nullptr, &Own, ESearchCase::IgnoreCase, ESearchDir::FromEnd) && !Own.IsEmpty() && !Mn.Contains(Own)) continue; }
				UMaterialInstanceDynamic*& D = Copy.FindOrAdd(Mi);
				if (!D)
				{
					// (made from the real material asset under the loader's copy, with the copy's settings carried over -
					//  a copy of a copy lost its textures and showed a checkerboard)
					UMaterialInstanceDynamic* Src = Cast<UMaterialInstanceDynamic>(Mi);
					UMaterialInterface* Root = Src && Src->Parent ? Src->Parent.Get() : Mi;
					D = UMaterialInstanceDynamic::Create(Root, this);
					if (Src) D->CopyParameterOverrides(Src);
					FLinearColor Bc(1, 1, 1, 1); D->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("BC")), Bc);
					const FLinearColor Dim = Bc * (0.35f / FMath::Max(0.05f, Bc.GetLuminance()));   // (owner: "way dimmer" outside)
					D->SetVectorParameterValue(TEXT("BC"), FLinearColor(Dim.R, Dim.G, Dim.B, 1.f)); D->SetVectorParameterValue(TEXT("BaseColorFactor"), FLinearColor(Dim.R, Dim.G, Dim.B, 1.f));
				}
				In.Key->SetMaterial(m, D); ++N;
			}
		}
		ADP_LOG(TEXT("Jetways: outside skin in its dimmed copies (%d parts, %d finishes)"), N, Copy.Num());
	}
	const TCHAR* Names[NumGroups] = {TEXT("ROTUNDA-SLEEVE"), nullptr, TEXT("TUNNEL-1"), TEXT("TUNNEL-2"), TEXT("TUNNEL-3"), TEXT("CAB"), TEXT("CAB-SLEEVE"), TEXT("DRIVE-UNIT"), TEXT("LIFT-SLEEVES")};
	TArray<FString> Ids;
	for (int32 i = 0; i < M->NodeCount(); ++i)
	{
		const FString& Nm = M->NodeName(i);
		if (Nm.StartsWith(TEXT("J-")) && !Nm.Contains(TEXT("/")) && !Nm.Contains(TEXT(":")) && M->NodeIndex(Nm + TEXT("/ROTUNDA-SLEEVE")) != INDEX_NONE) Ids.AddUnique(Nm);
	}
	Ids.Sort([](const FString& A, const FString& B) { const int32 a = FCString::Atoi(*A.RightChop(2)), b = FCString::Atoi(*B.RightChop(2)); return a != b ? a < b : A < B; });
	for (const FString& Id : Ids)
	{
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
		// the jetway's own loose parts (straight under J-n): the lift cables ride with the lift; the two air hoses on the
		// lift beam ride with it (the two under the rotunda stay); the bellows, their linings and the floor plates across
		// them stretch between the parts they join; the service stair hangs from its landing (below)
		const int32 JRoot = M->NodeIndex(Id);
		TArray<int32> StretchNodes, StairNodes;
		if (JRoot != INDEX_NONE)
			for (const int32 K : M->NodeKids(JRoot))
			{
				const FString& Nm = M->NodeName(K);
				if (Nm.EndsWith(TEXT("/LIFT-CABLE"))) PartGroup.Add(K, Sleeves);
				else if (Nm.EndsWith(TEXT("/PCA-HOSE")) && FVector::Dist2D(M->NodeModel(K).GetLocation(), M->NodeModel(Root[Boom]).GetLocation()) > 400.f) PartGroup.Add(K, Sleeves);
				else if (Nm.Contains(TEXT("/BELLOWS-")) || Nm.StartsWith(TEXT("box#"))) StretchNodes.Add(K);
				else if (Nm.EndsWith(TEXT("/SERVICE-STAIR"))) { TFunction<void(int32)> All = [&](int32 I) { StairNodes.Add(I); for (const int32 C : M->NodeKids(I)) All(C); }; All(K); }
			}
		for (int32 g = 0; g < NumGroups; ++g) if (Root[g] != INDEX_NONE) R.G[g].Docked = M->NodeModel(Root[g]);
		int32 Parts = 0;
		// the cab's two front door leaves (J-n/CAB-DOOR, each hung on its outer edge): their own parts, turned about the hinge
		TMap<int32, int32> LeafOf;
		{
			const int32 CabNd = M->NodeIndex(Id + TEXT("/CAB"));
			if (CabNd != INDEX_NONE)
				for (const int32 K : M->NodeKids(CabNd))
					if (M->NodeName(K) == Id + TEXT("/CAB-DOOR"))
					{
						const int32 Li = R.Leaves.Num(); FLeaf Lf; Lf.H = M->NodeModel(K).GetLocation(); R.Leaves.Add(Lf);
						TArray<int32> Todo{K}; while (Todo.Num()) { const int32 I = Todo.Pop(); LeafOf.Add(I, Li); Todo.Append(M->NodeKids(I)); }
					}
		}
		// the service door's leaf (J-n/SERVICE-DOOR under the cab-end section, hung on its vertical edge; Tools/jetway_part5.py)
		TSet<int32> SvcOf;
		TMap<int32, int32> SwayOf;
		for (const TCHAR* Hn : {TEXT("/HOSE-A"), TEXT("/HOSE-B")})
		{
			const int32 HoseIx = M->NodeIndex(Id + Hn); const int32 HoseEnd = M->NodeIndex(Id + Hn + TEXT("/END"));
			if (HoseIx == INDEX_NONE || HoseEnd == INDEX_NONE) continue;
			FRig::FSway S; S.H = M->NodeModel(HoseIx).GetLocation(); S.Ax = (M->NodeModel(HoseEnd).GetLocation() - S.H).GetSafeNormal();
			const bool bLoop = FCString::Strstr(Hn, TEXT("B")) != nullptr;
			S.Amp = bLoop ? 0.075f : 0.03f; S.Ph = R.Sways.Num() * 1.9f + Rigs.Num() * 0.77f;
			const int32 Si = R.Sways.Add(S);
			TArray<int32> Todo{HoseIx}; while (Todo.Num()) { const int32 I = Todo.Pop(); SwayOf.Add(I, Si); Todo.Append(M->NodeKids(I)); }
		}
		{
			const int32 Sv = M->NodeIndex(Id + TEXT("/SERVICE-DOOR"));
			if (Sv != INDEX_NONE) { R.Svc.H = M->NodeModel(Sv).GetLocation(); TArray<int32> Todo{Sv}; while (Todo.Num()) { const int32 I = Todo.Pop(); SvcOf.Add(I); Todo.Append(M->NodeKids(I)); } }
		}
		for (const TPair<int32, int32>& O : PartGroup)
		{
			const TPair<UInstancedStaticMeshComponent*, int32> In = M->NodeInstance(O.Key);
			if (!In.Key || In.Value == INDEX_NONE) continue;
			FPart P; P.K = In.Key; P.Ix = In.Value; In.Key->GetInstanceTransform(In.Value, P.Docked, false);
			if (const int32* Li = LeafOf.Find(O.Key)) { R.Leaves[*Li].Parts.Add(P); ++Parts; continue; }
			if (SvcOf.Contains(O.Key)) { R.Svc.Parts.Add(P); ++Parts; continue; }
			if (const int32* Si = SwayOf.Find(O.Key)) { R.Sways[*Si].Parts.Add(P); ++Parts; continue; }
			R.G[O.Value].Parts.Add(P); ++Parts;
		}
		// which way each shuts: the turn (his 1.53 rad) that brings the leaf's middle in towards the doorway's middle
		if (R.Leaves.Num() == 2)
		{
			const FVector Mid = (R.Leaves[0].H + R.Leaves[1].H) / 2;
			for (FLeaf& Lf : R.Leaves)
			{
				FBox Bx(ForceInit); for (const FPart& P : Lf.Parts) if (P.K && P.K->GetStaticMesh()) Bx += P.K->GetStaticMesh()->GetBoundingBox().TransformBy(P.Docked);
				if (!Bx.IsValid) continue;
				const FVector C = Bx.GetCenter();
				auto Turned = [&](float A) { return Lf.H + FQuat(FVector::UpVector, A).RotateVector(C - Lf.H); };
				Lf.Close = FVector::Dist2D(Turned(1.53f), Mid) < FVector::Dist2D(Turned(-1.53f), Mid) ? 1.53f : -1.53f;
			}
		}
		if (R.Svc.Parts.Num())
		{
			FBox Bx(ForceInit); for (const FPart& P : R.Svc.Parts) if (P.K && P.K->GetStaticMesh()) Bx += P.K->GetStaticMesh()->GetBoundingBox().TransformBy(P.Docked);
			R.SvcMid = Bx.IsValid ? Bx.GetCenter() : R.Svc.H;
			// (owner, 2026-11-19: the service door opens only with E / X - outlined when you look at it, so you know it can
			// be used; all its pieces one thing)
			for (const FPart& P : R.Svc.Parts) AInteractOutline::Add(P.K, P.Ix, (uint64(0x5356) << 32) + uint64(Rigs.Num()));
		}
		// the docked geometry, read from his export
		R.BoomO = R.G[Boom].Docked.GetLocation();
		R.Wd = R.G[Tun3].Docked.TransformPosition(FVector(0.f, ZW * 100.f, 0.f));
		const FIk D = ADPJetwayInternal::Ik(R.BoomO, R.Wd);
		R.DirD = D.Dir; R.AxisD = D.Axis; R.LenD = D.Len; R.PitchD = D.Pitch;
		R.PD = R.BoomO + R.DirD * (RR - 0.25f) * 100.f;
		if (R.Svc.Parts.Num())
		{
			// which way it swings: outwards (its middle further from the tunnel's axis), a quarter turn and a bit
			auto Off = [&](const FVector& X) { const FVector V = X - R.PD; return (V - R.AxisD * FVector::DotProduct(V, R.AxisD)).Size(); };
			auto TurnedAt = [&](float A) { return R.Svc.H + FQuat(FVector::UpVector, A).RotateVector(R.SvcMid - R.Svc.H); };
			R.Svc.Close = Off(TurnedAt(1.45f)) > Off(TurnedAt(-1.45f)) ? 1.45f : -1.45f;   // (for the service door: its open angle)
		}
		R.CabOff = R.G[Cab].Docked.GetLocation() - (R.PD + R.AxisD * R.LenD * 100.f + R.DirD * (CR - 0.25f) * 100.f);
		// its home point: the export docks J-1's way; a J-x/CAB-TARGET@<type>.<door> mark says where this one's cab docks
		// (the new gates' bridges: swung to their doors, the wide-bodies' sills higher) - the tunnel telescopes to it
		R.WdExp = R.Wd;
		if (JRoot != INDEX_NONE)
			for (const int32 K : M->NodeKids(JRoot))
			{
				FString Nm = M->NodeName(K), Tag;
				if (!Nm.Split(TEXT("/CAB-TARGET@"), nullptr, &Tag)) continue;
				R.Wd += M->NodeModel(K).GetLocation() - R.G[Cab].Docked.GetLocation();
				FString Ty, Dr; if (Tag.Split(TEXT("."), &Ty, &Dr)) { R.BaseType = Ty; R.Door = Dr; }
				break;
			}
		// the stretched parts: each one's length along whichever of its own axes lies along the tunnel; its ends ride with
		// the rotunda and the first section (near the rotunda) or the last section and the cab's sleeve (near the cab);
		// the thin strips at the plates' ends just ride with the nearer end
		for (const int32 I : StretchNodes)
		{
			const TPair<UInstancedStaticMeshComponent*, int32> In = M->NodeInstance(I);
			if (!In.Key || In.Value == INDEX_NONE || !In.Key->GetStaticMesh()) continue;
			FStretch X; X.P.K = In.Key; X.P.Ix = In.Value; In.Key->GetInstanceTransform(In.Value, X.P.Docked, false);
			const FBox Bx = In.Key->GetStaticMesh()->GetBoundingBox(); const FVector C = X.P.Docked.TransformPosition(Bx.GetCenter());
			float Best = -1.f;
			for (int32 a = 0; a < 3; ++a) { const float Al = FMath::Abs(FVector::DotProduct(X.P.Docked.GetRotation().GetAxisX() * (a == 0) + X.P.Docked.GetRotation().GetAxisY() * (a == 1) + X.P.Docked.GetRotation().GetAxisZ() * (a == 2), R.AxisD)); if (Al > Best) { Best = Al; X.Axis = a; } }
			const FVector Ax = X.Axis == 0 ? X.P.Docked.GetRotation().GetAxisX() : X.Axis == 1 ? X.P.Docked.GetRotation().GetAxisY() : X.P.Docked.GetRotation().GetAxisZ();
			const float Half = Bx.GetExtent()[X.Axis] * FMath::Abs(X.P.Docked.GetScale3D()[X.Axis]);
			FVector E1 = C - Ax * Half, E2 = C + Ax * Half; if (FVector::Dist(E1, R.BoomO) > FVector::Dist(E2, R.BoomO)) Swap(E1, E2);
			X.E1 = E1; X.E2 = E2;
			const bool bNearRot = FVector::Dist(C, R.BoomO) < FVector::Dist(C, R.G[Cab].Docked.GetLocation());
			X.A = bNearRot ? Boom : Tun3; X.B = bNearRot ? Tun1 : CabSleeve;
			if (Half * 2.f < 30.f) { const int32 G = FVector::Dist(C, R.G[X.A].Docked.GetLocation()) < FVector::Dist(C, R.G[X.B].Docked.GetLocation()) ? X.A : X.B; R.G[G].Parts.Add(X.P); ++Parts; continue; }
			R.Stretch.Add(X); ++Parts;
		}
		// the service stair: its top on its landing (on the last section), its foot 0.3 m over the apron, 7.2 m long; the
		// steps, posts and wheel forks stay level whatever its slope
		R.Ground = R.G[Bogie].Docked.GetLocation().Z;
		if (StairNodes.Num())
		{
			R.StairTop = M->NodeModel(StairNodes[0]).GetLocation();
			FVector Foot = R.StairTop; float FarD = 0.f;
			for (const int32 I : StairNodes) { const FVector L = M->NodeModel(I).GetLocation(); const float Dd = FVector::Dist(L, R.StairTop); if (M->NodeName(I).Contains(TEXT("STAIR-FORK")) && Dd > FarD) { FarD = Dd; Foot = L; } }
			R.StairU = (Foot - R.StairTop).GetSafeNormal();
			R.StairSl = FMath::Asin(FMath::Clamp(-R.StairU.Z, -1.f, 1.f));
			{ const int32 Ld = M->NodeIndex(Id + TEXT("/STAIR-LANDING"));
			  ADP_LOG(TEXT("Jetways: %s stair top %s, foot %s, landing %s, slope %.1f deg, ground %.0f"), *Id, *R.StairTop.ToString(), *Foot.ToString(),
				Ld != INDEX_NONE ? *M->NodeModel(Ld).GetLocation().ToString() : TEXT("-"), FMath::RadiansToDegrees(R.StairSl), R.Ground); }
			for (const int32 I : StairNodes)
			{
				const TPair<UInstancedStaticMeshComponent*, int32> In = M->NodeInstance(I);
				if (!In.Key || In.Value == INDEX_NONE) continue;
				FPart P; P.K = In.Key; P.Ix = In.Value; In.Key->GetInstanceTransform(In.Value, P.Docked, false);
				const FString& Nm = M->NodeName(I);
				if (Nm.Contains(TEXT("/STEP"))) R.StepIx.Add(R.Stair.Num());
				R.Stair.Add(P); R.StairLevel.Add(Nm.Contains(TEXT("/STEP")) || Nm.Contains(TEXT("STAIR-POST")) || Nm.Contains(TEXT("STAIR-FORK")) || Nm.Contains(TEXT("STAIR-WHEEL"))); ++Parts;
			}
		}
		const int32 Con = M->NodeIndex(Id + TEXT("/CAB-CONSOLE"));
		R.Ctl = Con != INDEX_NONE ? M->NodeModel(Con).GetLocation() : R.G[Cab].Docked.GetLocation();
		// the console's flat monitor (the HMI, to-do 115; Tools/jetway_monitor.py): its housing outlined within reach like the
		// other things you can use (E); the screen's place from its own node (the console's frame)
		{
			const int32 Hs = M->NodeIndex(Id + TEXT("/CAB-CONSOLE:jwDark#monitor"));
			const TPair<UInstancedStaticMeshComponent*, int32> In = Hs != INDEX_NONE ? M->NodeInstance(Hs) : TPair<UInstancedStaticMeshComponent*, int32>(nullptr, INDEX_NONE);
			if (In.Key && In.Value != INDEX_NONE) { R.Scr.K = In.Key; R.Scr.Ix = In.Value; In.Key->GetInstanceTransform(In.Value, R.Scr.Docked, false); AInteractOutline::Add(In.Key, In.Value, (uint64(0x4A57) << 32) + uint64(Rigs.Num())); }
			else ADP_WARN(TEXT("Jetways: %s has no flat console monitor (CAB-CONSOLE:jwDark#monitor - Tools/jetway_monitor.py)"), *Id);
		}
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
		// the path's length, and where along it each metre is (driving at real speeds needs distances)
		R.WdBase = R.Wd; MakeArc(R); R.D = R.PathL;
		// the drive unit's wheels: the parts named J-n/WHEEL:* grouped by the wheel they belong to (their parent); each
		// wheel's hub its parts' middle, its axle the way the wheel is thinnest (a tyre is narrow across its tread)
		{
			TMap<int32, int32> WheelOf;
			for (int32 i = 0; i < M->NodeCount(); ++i)
				for (const int32 K : M->NodeKids(i))
				{
					if (!M->NodeName(K).StartsWith(Id + TEXT("/WHEEL:"))) continue;
					const TPair<UInstancedStaticMeshComponent*, int32> In = M->NodeInstance(K);
					if (!In.Key || In.Value == INDEX_NONE) continue;
					int32* W = WheelOf.Find(i); if (!W) { W = &WheelOf.Add(i, R.Wheels.Num()); R.Wheels.AddDefaulted(); }
					FPart P; P.K = In.Key; P.Ix = In.Value; In.Key->GetInstanceTransform(In.Value, P.Docked, false);
					R.Wheels[*W].Parts.Add(P);
				}
			for (FRig::FWheel& Wh : R.Wheels)
			{
				FBox B(ForceInit); for (const FPart& P : Wh.Parts) if (P.K->GetStaticMesh()) B += P.K->GetStaticMesh()->GetBoundingBox().TransformBy(P.Docked);
				if (!B.IsValid) continue;
				Wh.Hub = B.GetCenter();
				// (the axle: of the level ways round the hub, the one across which the wheel is narrowest - tried every 5 degrees)
				float Best = 1e9f; FVector BestA = FVector::RightVector;
				for (int32 a = 0; a < 36; ++a)
				{
					const float An = FMath::DegreesToRadians(a * 5.f); const FVector A(FMath::Cos(An), FMath::Sin(An), 0.f);
					float Lo = 1e9f, Hi = -1e9f;
					for (const FPart& P : Wh.Parts) if (P.K->GetStaticMesh()) { const FBox Bp = P.K->GetStaticMesh()->GetBoundingBox(); for (int32 k = 0; k < 8; ++k) { const float Dd = FVector::DotProduct(P.Docked.TransformPosition(FVector(k & 1 ? Bp.Max.X : Bp.Min.X, k & 2 ? Bp.Max.Y : Bp.Min.Y, k & 4 ? Bp.Max.Z : Bp.Min.Z)) - Wh.Hub, A); Lo = FMath::Min(Lo, Dd); Hi = FMath::Max(Hi, Dd); } }
					if (Hi - Lo < Best) { Best = Hi - Lo; BestA = A; }
				}
				Wh.Axle = BestA; Wh.Fwd = FVector::CrossProduct(FVector::UpVector, Wh.Axle).GetSafeNormal();
				Wh.Rad = FMath::Clamp((B.Max.Z - B.Min.Z) * 0.5f, 20.f, 90.f);
			}
			R.G[Bogie].Parts.RemoveAll([&](const FPart& P) { for (const FRig::FWheel& Wh : R.Wheels) for (const FPart& Q : Wh.Parts) if (Q.K == P.K && Q.Ix == P.Ix) return true; return false; });
			ADP_LOG(TEXT("Jetways: %s drive unit - %d wheels roll (radius %.0f cm)"), *Id, R.Wheels.Num(), R.Wheels.Num() ? R.Wheels[0].Rad : 0.f);
		}
		// the outlines the holographic map draws (MapShapes): each group's parts as one box in the group's own docked space
		for (int32 g = 0; g < NumGroups; ++g)
		{
			R.MapBox[g] = FBox(ForceInit);
			for (const FPart& P : R.G[g].Parts) if (P.K && P.K->GetStaticMesh()) R.MapBox[g] += P.K->GetStaticMesh()->GetBoundingBox().TransformBy(P.Docked.GetRelativeTransform(R.G[g].Docked));
		}
		// (its fixed parts: the corridor from the gate and the rotunda it stands on)
		for (const TCHAR* Fx : {TEXT("/CORRIDOR:"), TEXT("/ROTUNDA:")})
		{
			TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> In; M->InstancesNamed(Id + Fx, true, In);
			FBox B(ForceInit);
			for (const auto& I : In) { UInstancedStaticMeshComponent* K = I.Get<0>(); FTransform T; if (K && K->GetStaticMesh() && K->GetInstanceTransform(I.Get<1>(), T, false)) { B += K->GetStaticMesh()->GetBoundingBox().TransformBy(T); R.MapHost = K; } }
			if (B.IsValid) R.MapFixed.Add(B);
		}
		AddBeacons(R, M, Id);
		AddSignLights(R, M, Id);
		Rigs.Add(MoveTemp(R));
	}
	if (!Rigs.Num()) ADP_WARN(TEXT("Jetways: none found to rig in the Jetways model"));
	// at rest, half way back, where no aircraft stands (his rule: docked only to an aircraft)
	for (int32 J = 0; J < Rigs.Num(); ++J)
		if (!AircraftAt(J)) { FRig& R = Rigs[J]; R.St = EState::Parked; R.D = 0.f; R.S = 0.f; R.Shut = 0.f; R.Turn = R.Turn0; R.V = 0.f; Apply(R); ADP_LOG(TEXT("Jetways: %s at rest (no aircraft at its stand)"), *R.Id); }
		else
		{
			// docked from the start: at this aircraft's own door (its type's place along the fuselage and sill)
			FRig& R = Rigs[J]; FString Ty;
			if (AActor* Pl = AircraftAtStand(J, &Ty)) { SetDoor(R, Pl, Ty); R.S = 1.f; R.D = R.PathL; R.Turn = 0.f; Apply(R); }
		}
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
	FVector W = PathAt(R, R.S);
	if (R.Lat != 0.f || R.HOff != 0.f)   // (manual: steered sideways, lifted or lowered)
	{ const FVector Fl = FVector(W.X - R.BoomO.X, W.Y - R.BoomO.Y, 0.f).GetSafeNormal(); W += FVector(-Fl.Y, Fl.X, 0.f) * R.Lat + FVector(0, 0, R.HOff); }
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
	{ FVector WdF = R.WdExp; WdF.Z = R.G[Bogie].Docked.GetLocation().Z; FVector WF = W; WF.Z = WdF.Z; N[Bogie] = Turned(R.G[Bogie].Docked, QYaw, WdF, WF); }
	N[Sleeves] = Turned(R.G[Sleeves].Docked, QTun, R.WdExp, W);
	TSet<UInstancedStaticMeshComponent*> Dirty;
	for (int32 g = 0; g < NumGroups; ++g)
		for (const FPart& P : R.G[g].Parts)
		{
			P.K->UpdateInstanceTransform(P.Ix, P.Docked.GetRelativeTransform(R.G[g].Docked) * N[g], false, false, true);
			Dirty.Add(P.K);
		}
	auto Ride = [&](int32 G, const FVector& X) { return N[G].TransformPosition(R.G[G].Docked.InverseTransformPosition(X)); };
	// the wheels roll as far as the drive unit travels (owner, 2026-11-18): the way its hub moved this frame along the
	// way the wheels point, divided by their radius, is how far they turn - forwards or backwards, at any speed
	if (R.Wheels.Num())
	{
		const FRig::FWheel& W0 = R.Wheels[0];
		const FVector Hub = Ride(Bogie, W0.Hub), Fwd = N[Bogie].TransformVectorNoScale(R.G[Bogie].Docked.InverseTransformVectorNoScale(W0.Fwd));
		if (!R.bRollStart) R.Roll = FMath::Fmod(R.Roll + FVector::DotProduct(Hub - R.LastHub, Fwd) / W0.Rad, 2.f * PI);
		R.LastHub = Hub; R.bRollStart = false;
		for (const FRig::FWheel& Wh : R.Wheels)
		{
			const FQuat Q(Wh.Axle, R.Roll);
			for (const FPart& P : Wh.Parts)
			{
				const FTransform Pd(Q * P.Docked.GetRotation(), Wh.Hub + Q.RotateVector(P.Docked.GetLocation() - Wh.Hub), P.Docked.GetScale3D());
				P.K->UpdateInstanceTransform(P.Ix, Pd.GetRelativeTransform(R.G[Bogie].Docked) * N[Bogie], false, false, true); Dirty.Add(P.K);
			}
		}
	}
	// the cab's front doors: shut (turned about their hinges) as far as R.Shut says, riding with the cab
	{
		const float U = FMath::Clamp(R.Shut, 0.f, 1.f), Open = U * U * (3 - 2 * U);
		for (const FLeaf& Lf : R.Leaves)
		{
			const FQuat Q(FVector::UpVector, Lf.Close * (1.f - Open));
			for (const FPart& P : Lf.Parts)
			{
				const FTransform Pd(Q * P.Docked.GetRotation(), Lf.H + Q.RotateVector(P.Docked.GetLocation() - Lf.H), P.Docked.GetScale3D());
				P.K->UpdateInstanceTransform(P.Ix, Pd.GetRelativeTransform(R.G[Cab].Docked) * N[Cab], false, false, true); Dirty.Add(P.K);
			}
		}
	}
	ApplySvc(R, N[Tun3]);
	ApplySway(R, N[Tun3], GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f, !ADPAnim::HoseSway);
	// the stretched parts: their ends ride with the two parts they join; each turns to lie between them and grows or
	// shrinks along its length - its own middle landing on the new middle (the bellows are built in place, far from
	// their origin: scaled about the origin they would be thrown far away)
	for (const FStretch& X : R.Stretch)
	{
		const FVector E1 = Ride(X.A, X.E1), E2 = Ride(X.B, X.E2), Dd = X.E2 - X.E1, Dn = E2 - E1;
		const FQuat Q = FQuat::FindBetweenNormals(Dd.GetSafeNormal(), Dn.GetSafeNormal());
		FVector Sc = X.P.Docked.GetScale3D(); Sc[X.Axis] *= Dn.Size() / FMath::Max(1.f, (float)Dd.Size());
		const FVector Cl = X.P.K->GetStaticMesh()->GetBoundingBox().GetCenter();
		const FQuat Rq = Q * X.P.Docked.GetRotation();
		X.P.K->UpdateInstanceTransform(X.P.Ix, FTransform(Rq, (E1 + E2) / 2 - Rq.RotateVector(Sc * Cl), Sc), false, false, true); Dirty.Add(X.P.K);
	}
	// the service stair: its top on its landing, its foot on the apron (its slope from the landing's height), the steps,
	// posts and wheel forks level
	if (R.Stair.Num())
	{
		const FVector Top = Ride(Tun3, R.StairTop);
		const float H = FMath::Max(1.f, (float)(Top.Z - R.Ground - 30.f)), Sl = FMath::Asin(FMath::Clamp(H / 720.f, 0.15f, 0.98f));
		const FVector Uy = QYaw.RotateVector(R.StairU), Hz = FVector(Uy.X, Uy.Y, 0.f).GetSafeNormal();
		const FVector Un = Hz * FMath::Cos(Sl) - FVector::UpVector * FMath::Sin(Sl);
		const FQuat QP = FQuat::FindBetweenNormals(Uy, Un), Q = QP * QYaw;
		for (int32 i = 0; i < R.Stair.Num(); ++i)
		{
			const FPart& P = R.Stair[i];
			const FVector L = Top + Q.RotateVector(P.Docked.GetLocation() - R.StairTop);
			const FQuat Rot = R.StairLevel[i] ? QYaw * P.Docked.GetRotation() : Q * P.Docked.GetRotation();
			P.K->UpdateInstanceTransform(P.Ix, FTransform(Rot, L, P.Docked.GetScale3D()), false, false, true); Dirty.Add(P.K);
		}
	}
	// the gate sign's soft lights ride with the cab
	for (int32 i = 0; i < R.SignLight.Num(); ++i) if (R.SignLight[i]) R.SignLight[i]->SetRelativeLocation(N[Cab].TransformPosition(R.G[Cab].Docked.InverseTransformPosition(R.SignAt[i])));
	// the beacons' glow and light ride with them
	for (int32 b = 0; b < R.BcnK.Num(); ++b)
	{
		FTransform T; R.BcnK[b]->GetInstanceTransform(R.BcnIx[b], T, false);
		const uint8 Kd = R.BcnKind[b];
		if (R.BcnLight[b])
		{
			R.BcnLight[b]->SetRelativeLocation(T.TransformPosition(Kd == 3 ? FVector(0.f, -14.f, 0.f) : Kd >= 4 && Kd <= 6 ? FVector(0.f, 0.f, -18.f) : FVector(0.f, 0.f, 70.f)));
			if (R.BcnFixed[b]) R.BcnLight[b]->SetRelativeRotation((T.GetRotation() * R.BcnRel[b]).Rotator());
		}
		if (R.BcnFill.IsValidIndex(b) && R.BcnFill[b]) R.BcnFill[b]->SetRelativeLocation(T.TransformPosition(FVector(0.f, 0.f, 50.f)));
		if (Kd == 7) for (UAudioComponent* A : {R.SBeep, R.SMotor, R.STyres, R.SLift}) if (A) A->SetRelativeLocation(T.GetLocation() + FVector(0, 0, 60.f));
	}
	(void)Dirty;   // (no full rebuild: each moved instance already went to the GPU on its own - the full rebuild re-sent every part of these lists every frame, owner 2026-11-17 lag)
}

void AJetwayRigs::Tick(float Dt)
{
	using namespace ADPJetwayInternal;
	ADP_PERF_SCOPE("the jetways (each frame)", 2.f);
	Super::Tick(Dt);
	if (!bBuilt) { if ((Look += Dt) > 1.f) { Look = 0.f; Build(); } return; }
	const float D = Dt * Speed;
	BcnClock += Dt;
	const float Now = GetWorld()->GetTimeSeconds();
	FVector Eye = FVector(0, 0, 1e9f);
	if (APlayerController* PCe = GetWorld()->GetFirstPlayerController()) if (PCe->PlayerCameraManager) Eye = PCe->PlayerCameraManager->GetCameraLocation();
	for (FRig& R : Rigs)
	{
		const bool bMoving = ((R.St == EState::Docking || R.St == EState::Undocking) && R.Wait <= 0.f) || (R.St == EState::Manual && R.JogT > 0.f);
		if (R.Svc.Parts.Num() && !FMath::IsNearlyEqual(R.SvcOpen, R.SvcWant))
		{
			// (the service door swings at its own pace, about 1.2 s, also while the jetway stands)
			const float SvcWas = R.SvcOpen;
			R.SvcOpen = R.SvcWant > R.SvcOpen ? FMath::Min(R.SvcWant, R.SvcOpen + Dt / 1.2f) : FMath::Max(R.SvcWant, R.SvcOpen - Dt / 1.2f);
			// its sounds (owner, 2026-11-19; a real steel door, CC0 - Art/Sounds/Jetway/SOURCES.md): the latch and the swing as
			// it starts to open, the slam and the latch as it shuts - heard from the door (distance, walls between)
			if ((SvcWas <= 0.f && R.SvcOpen > 0.f) || (SvcWas > 0.f && R.SvcOpen <= 0.f))
			{
				static USoundBase* SOpen = ADPKeep(LoadObject<USoundBase>(nullptr, TEXT("/Game/JetwayBeacon/Sounds/JwSvcOpen.JwSvcOpen")));
				static USoundBase* SShut = ADPKeep(LoadObject<USoundBase>(nullptr, TEXT("/Game/JetwayBeacon/Sounds/JwSvcClose.JwSvcClose")));
				static USoundAttenuation* SAtt = nullptr;
				if (!SAtt)
				{
					SAtt = ADPKeep(NewObject<USoundAttenuation>(GetTransientPackage()));
					FSoundAttenuationSettings& A = SAtt->Attenuation;
					A.bAttenuate = true; A.bSpatialize = true; A.AttenuationShapeExtents = FVector(150.f); A.FalloffDistance = 4000.f;
					A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound; A.dBAttenuationAtMax = -60.f;
					A.bEnableOcclusion = true; A.OcclusionTraceChannel = ECC_Visibility; A.OcclusionLowPassFilterFrequency = 900.f; A.OcclusionVolumeAttenuation = 0.35f; A.OcclusionInterpolationTime = 0.1f;
				}
				USoundBase* S = R.SvcOpen > 0.f ? SOpen : SShut;
				if (S && R.Model.IsValid())
				{
					const FVector At = R.Model->GetActorTransform().TransformPosition(NowOf(R, Tun3).TransformPosition(R.G[Tun3].Docked.InverseTransformPosition(R.SvcMid)));
					UGameplayStatics::PlaySoundAtLocation(this, S, At, 1.f, 1.f, 0.f, SAtt);
				}
			}
			if (!bMoving) ApplySvc(R, NowOf(R, Tun3));
		}
		// the yellow hoses sway a little (Settings > Graphics > Animations); only near the camera, and once back to still
		if (R.Sways.Num() && !bMoving && R.Model.IsValid())
		{
			const bool bOn = ADPAnim::HoseSway && FVector::DistSquared(Eye, R.Model->GetActorTransform().TransformPosition(R.Sways[0].H)) < FMath::Square(9000.f);
			if (bOn || !R.bSwayRest) { ApplySway(R, NowOf(R, Tun3), Now, !bOn); R.bSwayRest = !bOn; }
		}
		Beacons(R, bMoving);
		Sounds(R, bMoving, Dt);
		// the wait to drive (Syracuse spec: 3 s of beacons and bell before it moves - people step clear)
		if (R.Wait > 0.f) { R.Wait -= Dt; continue; }
		// everyone standing in it moves with it (owner, 2026-11-16: you and every MetaHuman as one with the jetway, also
		// while walking in it): the part of the jetway under each person noted before it moves, each carried after
		TArray<FRider> Riders;
		const bool bJog = R.St == EState::Manual && R.JogT > 0.f;
		if (bMoving || bJog) GatherRiders(R, Riders);
		if (bJog)
		{
			const float Step = FMath::Min(R.JogT, D); R.JogT -= Step;
			const float V = VMax;   // (a manual step: the drive's own speed, 0.3 m/s - about 45 cm a step)
			if (R.JogDir == TEXT("FWD")) R.D = FMath::Min(R.PathL, R.D + V * Step);
			else if (R.JogDir == TEXT("BACK")) R.D = FMath::Max(0.f, R.D - V * Step);
			else if (R.JogDir == TEXT("LEFT")) R.Lat = FMath::Clamp(R.Lat + V * 100.f * Step, -250.f, 250.f);
			else if (R.JogDir == TEXT("RIGHT")) R.Lat = FMath::Clamp(R.Lat - V * 100.f * Step, -250.f, 250.f);
			else if (R.JogDir == TEXT("UP")) R.HOff = FMath::Clamp(R.HOff + Lift * 100.f * 4.f * Step, -80.f, 80.f);
			else if (R.JogDir == TEXT("DOWN")) R.HOff = FMath::Clamp(R.HOff - Lift * 100.f * 4.f * Step, -80.f, 80.f);
			else if (R.JogDir == TEXT("CABL")) R.Turn = FMath::Clamp(R.Turn + CabTurn * Step, -0.6f, 0.6f);
			else if (R.JogDir == TEXT("CABR")) R.Turn = FMath::Clamp(R.Turn - CabTurn * Step, -0.6f, 0.6f);
			R.S = SAt(R, R.D); R.V = R.JogT > 0.f ? V : 0.f;
			Apply(R);
		}
		if (R.St == EState::Docking && (R.Lat != 0.f || R.HOff != 0.f))
		{ const float E = 30.f * D; R.Lat = FMath::Abs(R.Lat) <= E ? 0.f : R.Lat - FMath::Sign(R.Lat) * E; R.HOff = FMath::Abs(R.HOff) <= E ? 0.f : R.HOff - FMath::Sign(R.HOff) * E; }
		if (R.St == EState::Docking)
		{
			const float Left = R.PathL - R.D;
			if (Left > 0.f)
			{
				// speed up to full speed, ease down to a creep for the last 1.5 m (stopping in time at Acc)
				const float Want = Left > Creep ? FMath::Min(VMax, FMath::Sqrt(VCreep * VCreep + 2 * Acc * (Left - Creep))) : VCreep;
				R.V = FMath::Min(Want, R.V + Acc * D);
				// never lift or lower faster than the lift columns can (1.1 m a minute)
				const float Dh = FMath::Abs(PathAt(R, SAt(R, FMath::Min(R.PathL, R.D + 0.5f))).Z - PathAt(R, R.S).Z) / 100.f;
				if (Dh > 1e-3f) R.V = FMath::Min(R.V, Lift * 0.5f / Dh);
				R.D = FMath::Min(R.PathL, R.D + R.V * D);
			}
			// the cab squares up to the fuselage in the creep, at its own turning speed
			if (R.PathL - R.D < Creep) R.Turn = FMath::Sign(R.Turn) * FMath::Max(0.f, FMath::Abs(R.Turn) - CabTurn * D);
			R.S = SAt(R, R.D);
			R.Shut = 0.f;
			if (R.D >= R.PathL && FMath::Abs(R.Turn) < 1e-4f) { R.T += D; R.Shut = FMath::Clamp(R.T / TCanopy, 0.f, 1.f); if (R.T > TCanopy) { R.St = EState::Docked; R.V = 0.f; R.Shut = 1.f; ADP_LOG(TEXT("Jetways: %s docked"), *R.Id); } }   // (at the door: the canopy out and the doors open)
			Apply(R);
		}
		else if (R.St == EState::Undocking)
		{
			R.T += D;
			R.Shut = 1.f - FMath::Clamp(R.T / TCanopy, 0.f, 1.f);   // (the doors shut before it drives away)
			if (R.T > TCanopy)
			{
				const float Gone = R.PathL - R.D;
				if (Gone < Creep) R.Turn = FMath::Sign(R.Turn0) * FMath::Min(FMath::Abs(R.Turn0), FMath::Abs(R.Turn) + CabTurn * D);
				const float Want = Gone < Creep ? VCreep : FMath::Min(VMax, FMath::Sqrt(VCreep * VCreep + 2 * Acc * R.D));
				R.V = FMath::Min(Want, R.V + Acc * D);
				R.D = FMath::Max(0.f, R.D - R.V * D);
				R.S = SAt(R, R.D);
				if (R.D <= 0.f) { R.S = 0.f; R.V = 0.f; R.St = EState::Parked; ADP_LOG(TEXT("Jetways: %s at rest, half way back"), *R.Id); }
			}
			Apply(R);
		}
		if (bMoving || bJog) LogLeftBehind(R, Riders);
		CarryRiders(Riders);
	}
	MoveHeld();   // (the one at the console and their close camera, after everything of the jetway has moved)
}

int32 AJetwayRigs::Nearest(const FVector& At, float MaxCm) const
{
	int32 Best = INDEX_NONE; float BD2 = MaxCm * MaxCm;
	for (int32 i = 0; i < Rigs.Num(); ++i) { const float D2 = FVector::DistSquared(At, Console(i)); if (D2 < BD2) { BD2 = D2; Best = i; } }
	return Best;
}
AJetwayRigs::EState AJetwayRigs::State(int32 J) const { return Rigs.IsValidIndex(J) ? Rigs[J].St : EState::Parked; }
FString AJetwayRigs::Name(int32 J) const { return Rigs.IsValidIndex(J) ? Rigs[J].Id : FString(); }
AActor* AJetwayRigs::AircraftAtStand(int32 J, FString* Type) const
{
	// a parked aircraft (ParkedAircraft.cpp tags each "<stand>/<TYPE>") whose middle is within 40 m of the docked door
	// point - the jetway's own stand
	FVector Ro, Dr; if (!Frame(J, Ro, Dr)) return nullptr;
	for (TActorIterator<ABabylonModel> It(GetWorld()); It; ++It)
		for (const FName& T : It->Tags)
		{
			const FString S = T.ToString(); int32 Slash;
			if (!S.FindLastChar(TEXT('/'), Slash) || S.StartsWith(TEXT("MOVE/")) || S.StartsWith(TEXT("SIM/")) || FVector::Dist2D(It->GetActorLocation(), Dr) > 4000.f) continue;   // (MOVE/ and SIM/ carry a registration, not a type)
			if (Type) *Type = S.Mid(Slash + 1);
			return *It;
		}
	return nullptr;
}

bool AJetwayRigs::AircraftAt(int32 J) const { return AircraftAtStand(J) != nullptr; }

namespace ADPJetwayInternal
{
	// each type's front door (L1), from the makers' airport planning documents (Airbus AC rev 46, Boeing D6-58325-7 rev C
	// and D6-58327 rev H, Embraer APM 190-1 rev 21): nose tip to the door's centre, the sill's height (middle of the
	// loaded-to-empty range), the fuselage's width. Every type stops with its nose on the same line (each its own stop bar)
	struct FDoorType { const TCHAR* Type; float NoseToDoor, Sill, Width; const TCHAR* Door = TEXT("L1"); };
	const FDoorType DoorTypes[] = {
		// the wide-body gates (13: A380 M1L, M2L, U1L on the upper deck; 14-16: B777 L1, L2), approximate, from the
		// makers' airport planning documents; also the types their stop bars take (B777-9, A350-1000, B787-10)
		{TEXT("A380"), 10.3f, 5.20f, 7.14f, TEXT("M1L")}, {TEXT("A380"), 22.5f, 5.20f, 7.14f, TEXT("M2L")}, {TEXT("A380"), 14.5f, 8.00f, 5.92f, TEXT("U1L")},
		{TEXT("B777"), 6.6f, 4.95f, 6.20f, TEXT("L1")}, {TEXT("B777"), 18.2f, 4.95f, 6.20f, TEXT("L2")},
		{TEXT("B777-300ER"), 6.6f, 4.95f, 6.20f, TEXT("L1")}, {TEXT("B777-300ER"), 18.2f, 4.95f, 6.20f, TEXT("L2")},
		{TEXT("B777-9"), 6.6f, 5.05f, 6.20f, TEXT("L1")}, {TEXT("B777-9"), 18.4f, 5.05f, 6.20f, TEXT("L2")},
		{TEXT("A350-1000"), 6.4f, 4.90f, 5.96f, TEXT("L1")}, {TEXT("A350-1000"), 16.2f, 4.90f, 5.96f, TEXT("L2")},
		{TEXT("B787-10"), 6.2f, 4.70f, 5.77f, TEXT("L1")}, {TEXT("B787-10"), 15.3f, 4.70f, 5.77f, TEXT("L2")},
		{TEXT("A318"), 5.04f, 3.41f, 3.95f}, {TEXT("A319"), 5.04f, 3.42f, 3.95f}, {TEXT("A320"), 5.04f, 3.43f, 3.95f}, {TEXT("A321"), 5.02f, 3.45f, 3.95f},
		{TEXT("B737"), 5.03f, 2.67f, 3.76f}, {TEXT("B737-700"), 5.03f, 2.67f, 3.76f}, {TEXT("B737-800"), 5.03f, 2.67f, 3.76f}, {TEXT("B737-900ER"), 5.03f, 2.67f, 3.76f},
		{TEXT("B757"), 5.05f, 3.90f, 3.76f}, {TEXT("B757-200"), 5.05f, 3.90f, 3.76f}, {TEXT("E190"), 5.18f, 2.62f, 3.01f}, {TEXT("E195"), 5.18f, 2.62f, 3.01f},
	};
	const FDoorType* DoorOf(const FString& T, const FString& Door = TEXT("L1"))
	{ for (const FDoorType& D : DoorTypes) if (T.Equals(D.Type, ESearchCase::IgnoreCase) && Door.Equals(D.Door, ESearchCase::IgnoreCase)) return &D; return nullptr; }
}

void AJetwayRigs::MakeArc(FRig& R) const
{
	R.Arc.SetNum(65); R.Arc[0] = 0.f;
	FVector Pv = PathAt(R, 0.f); for (int32 i = 1; i <= 64; ++i) { const FVector Pn = PathAt(R, i / 64.f); R.Arc[i] = R.Arc[i - 1] + (Pn - Pv).Size() / 100.f; Pv = Pn; }
	R.PathL = R.Arc[64];
}

void AJetwayRigs::SetDoor(FRig& R, AActor* Plane, const FString& Type)
{
	// the docked point moved from the export's (made for the A320's door) to this type's door: along the fuselage by the
	// door's place, in towards the middle by half the width difference, up or down by the sill's (the lift columns)
	using namespace ADPJetwayInternal;
	const FDoorType* Base = DoorOf(R.BaseType, R.Door); const FDoorType* T = DoorOf(Type, R.Door);
	// (a type drawn with a model whose doors were measured on it - Data/aircraft-doors.json, Tools/make_aircraft_doors.py:
	//  that door; the table keeps the published figures the jetway's export was made for)
	FDoorType Meas = {TEXT(""), 0.f, 0.f, 0.f}; float NoseCm = 0.f;
	if (T) if (const ADPDoors::FDoor* Md = ADPDoors::Find(Type, R.Door)) if (ADPDoors::NoseY(Type, NoseCm))
	{ Meas = *T; Meas.NoseToDoor = float(Md->Skin.Y - NoseCm) / 100.f; Meas.Sill = Md->Sill / 100.f; Meas.Width = float(FMath::Abs(Md->Skin.X)) / 50.f; T = &Meas; }
	R.Wd = R.WdBase;
	if (Plane && Base && T)
	{
		const FTransform A = R.Model->GetActorTransform();
		const FVector Back = A.InverseTransformVectorNoScale(Plane->GetActorTransform().TransformVectorNoScale(FVector(0.f, 1.f, 0.f))).GetSafeNormal2D();   // (its tail: the actor's +Y)
		FVector In = A.InverseTransformPosition(Plane->GetActorLocation()) - R.WdBase; In.Z = 0.f; In -= Back * FVector::DotProduct(In, Back); In = In.GetSafeNormal();
		R.Wd += Back * (T->NoseToDoor - Base->NoseToDoor) * 100.f + In * (Base->Width - T->Width) * 50.f + FVector(0, 0, (T->Sill - Base->Sill) * 100.f);
	}
	R.DockedType = Type;
	MakeArc(R);
	ADP_LOG(TEXT("Jetways: %s docks to a %s - its door point moved %.2f m (sill %+.2f m)"), *R.Id, *Type, FVector::Dist(R.Wd, R.WdBase) / 100.f, (R.Wd.Z - R.WdBase.Z) / 100.f);
}

bool AJetwayRigs::Dock(int32 J)
{
	LastNote.Reset();
	if (!Rigs.IsValidIndex(J) || (Rigs[J].St != EState::Parked && Rigs[J].St != EState::Manual)) return false;
	const bool bFromManual = Rigs[J].St == EState::Manual; const float DWas = Rigs[J].D;
	FString Type; AActor* Plane = AircraftAtStand(J, &Type);
	if (!Plane) { LastNote = FString::Printf(TEXT("No aircraft at gate %s"), *Rigs[J].Id.RightChop(2)); ADP_LOG(TEXT("Jetways: %s - no aircraft at its stand, not docking"), *Rigs[J].Id); return false; }
	// commercial jets from the A318's size up to what the stand's markings take (A321, B757); smaller ones refused
	if (!ADPJetwayInternal::DoorOf(Type, Rigs[J].Door)) { LastNote = FString::Printf(TEXT("The %s is too small for this jetway"), *Type); ADP_LOG(TEXT("Jetways: %s - a %s isn't a type it docks to"), *Rigs[J].Id, *Type); return false; }
	// (where it is parked: the path from where it stands, to this type's door - only while at rest, the path is its own)
	{ FRig& R = Rigs[J]; SetDoor(R, Plane, Type); R.D = bFromManual ? FMath::Min(DWas, R.PathL) : 0.f; R.JogT = 0.f; }
	Rigs[J].St = EState::Docking; Rigs[J].T = 0.f; Rigs[J].V = 0.f; if (!bFromManual) Rigs[J].Turn = Rigs[J].Turn0; Rigs[J].Wait = 3.f; ADP_LOG(TEXT("Jetways: %s driving out to the door"), *Rigs[J].Id); return true;
}
bool AJetwayRigs::Undock(int32 J)
{
	LastNote.Reset();
	if (!Rigs.IsValidIndex(J) || (Rigs[J].St != EState::Docked && Rigs[J].St != EState::Manual)) return false;
	if (Rigs[J].St == EState::Manual) { Rigs[J].St = EState::Undocking; Rigs[J].T = 100.f; Rigs[J].V = 0.f; Rigs[J].Wait = 3.f; Rigs[J].JogT = 0.f; ADP_LOG(TEXT("Jetways: %s back to rest from a manual position"), *Rigs[J].Id); return true; }
	Rigs[J].St = EState::Undocking; Rigs[J].T = 0.f; Rigs[J].V = 0.f; Rigs[J].Wait = 3.f; ADP_LOG(TEXT("Jetways: %s pulling back"), *Rigs[J].Id); return true;
}
bool AJetwayRigs::Jog(int32 J, const FString& Dir)
{
	LastNote.Reset();
	if (!Rigs.IsValidIndex(J)) return false;
	FRig& R = Rigs[J];
	if (R.St == EState::Docked) { LastNote = TEXT("Docked at the aircraft: UNDOCK first"); return false; }
	if (R.St == EState::Docking || R.St == EState::Undocking) { LastNote = TEXT("Driving automatically: wait or STOP"); return false; }
	R.St = EState::Manual; R.JogDir = Dir; R.JogT = 1.5f; R.Shut = 0.f;
	ADP_LOG(TEXT("Jetways: %s manual step %s"), *R.Id, *Dir);
	return true;
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
bool AJetwayRigs::ScreenView(int32 J, FVector& Mid, FVector& Normal, FVector& Up, FVector2D& Size) const
{
	if (!Rigs.IsValidIndex(J) || !Rigs[J].Scr.K) return false;
	const FRig& R = Rigs[J];
	FTransform T; R.Scr.K->GetInstanceTransform(R.Scr.Ix, T, true);   // (where the console is now, the cab having moved)
	// the flat monitor's screen in the console's own measures (glTF metres, Tools/jetway_monitor.py): its middle at
	// (0, 1.24, 0.20), tilted 15 degrees back, facing the operator (-z); glTF (x, y, z) is Unreal (x, z, y) in cm
	const float Tl = FMath::DegreesToRadians(15.f);
	Mid = T.TransformPosition(FVector(0.f, 20.f, 124.f));
	Normal = T.TransformVectorNoScale(FVector(0.f, -FMath::Cos(Tl), FMath::Sin(Tl))).GetSafeNormal();
	Up = T.TransformVectorNoScale(FVector(0.f, FMath::Sin(Tl), FMath::Cos(Tl))).GetSafeNormal();
	Size = FVector2D(36.f, 22.5f);
	return true;
}
int32 AJetwayRigs::ScreenOf(const UPrimitiveComponent* C, int32 Item) const
{
	for (int32 J = 0; J < Rigs.Num(); ++J) if (Rigs[J].Scr.K == C && (Item == INDEX_NONE || Rigs[J].Scr.Ix == Item)) return J;
	return INDEX_NONE;
}
FString AJetwayRigs::HmiState(int32 J) const
{
	if (!Rigs.IsValidIndex(J)) return TEXT("{}");
	const FRig& R = Rigs[J];
	static const TCHAR* St[] = { TEXT("PARKED"), TEXT("DOCKING"), TEXT("DOCKED"), TEXT("UNDOCKING"), TEXT("MANUAL") };
	FString Type; AActor* Plane = AircraftAtStand(J, &Type);
	FVector Ro, Dr; const bool bF = Frame(J, Ro, Dr);
	FVector CabMid, Front; const bool bC = CabView(J, CabMid, Front);
	const float Ext = bF && bC ? FVector::Dist2D(Ro, CabMid) / 100.f : 0.f;
	const float Apron = R.Model.IsValid() ? R.Model->GetActorTransform().TransformPosition(FVector(R.G[Bogie].Docked.GetLocation().X, R.G[Bogie].Docked.GetLocation().Y, R.Ground)).Z : 0.f;
	const float Height = bC ? (CabMid.Z - Apron) / 100.f : 0.f;   // (the cab's middle over the apron)
	auto Esc = [](FString X) { return X.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")); };
	return FString::Printf(TEXT("{\"id\":\"%s\",\"gate\":\"%s\",\"state\":\"%s\",\"plane\":%s,\"type\":\"%s\",\"door\":\"%s\",\"progress\":%.3f,\"ext\":%.2f,\"turn\":%.1f,\"height\":%.2f,\"speed\":%.2f,\"len\":%.2f,\"lat\":%.2f,\"lift\":%.2f}"),
		*Esc(R.Id), *Esc(Name(J)), St[(int32)R.St], Plane ? TEXT("true") : TEXT("false"), *Esc(Type.IsEmpty() ? R.DockedType : Type), *Esc(R.Door),
		R.S, Ext, FMath::RadiansToDegrees(R.Turn), Height, R.V, R.PathL, R.Lat / 100.f, R.HOff / 100.f);
}
bool AJetwayRigs::HmiInfo(int32 J, FJetwayHmiInfo& O) const
{
	if (!Rigs.IsValidIndex(J)) return false;
	const FRig& R = Rigs[J];
	FString Type; O.bPlane = AircraftAtStand(J, &Type) != nullptr;
	FVector Ro, Dr, CabMid, Front; const bool bF = Frame(J, Ro, Dr), bC = CabView(J, CabMid, Front);
	const float Apron = R.Model.IsValid() ? R.Model->GetActorTransform().TransformPosition(FVector(R.G[Bogie].Docked.GetLocation().X, R.G[Bogie].Docked.GetLocation().Y, R.Ground)).Z : 0.f;
	O.Id = R.Id; O.Type = Type.IsEmpty() ? R.DockedType : Type; O.Door = R.Door; O.State = (int32)R.St;
	O.bDoorsOpen = R.Shut > 0.5f; O.bMoving = ((R.St == EState::Docking || R.St == EState::Undocking) && R.Wait <= 0.f && R.V > 0.001f) || (R.St == EState::Manual && R.JogT > 0.f);
	O.Progress = R.S; O.Ext = bF && bC ? FVector::Dist2D(Ro, CabMid) / 100.f : 0.f; O.TurnDeg = FMath::RadiansToDegrees(R.Turn);
	O.Height = bC ? (CabMid.Z - Apron) / 100.f : 0.f; O.Speed = R.V; O.PathLen = R.PathL; O.Lat = R.Lat / 100.f; O.Lift = R.HOff / 100.f; O.Wait = R.Wait;
	return true;
}
bool AJetwayRigs::Frame(int32 J, FVector& Rotunda, FVector& Door) const
{
	if (!Rigs.IsValidIndex(J) || !Rigs[J].Model.IsValid()) return false;
	const FTransform A = Rigs[J].Model->GetActorTransform();
	Rotunda = A.TransformPosition(Rigs[J].BoomO); Door = A.TransformPosition(Rigs[J].Wd); return true;
}
float AJetwayRigs::SAt(const FRig& R, float Dm) const
{
	if (R.Arc.Num() < 2 || R.PathL <= 0.f) return 1.f;
	Dm = FMath::Clamp(Dm, 0.f, R.PathL);
	for (int32 i = 1; i < R.Arc.Num(); ++i) if (R.Arc[i] >= Dm) { const float A = (Dm - R.Arc[i - 1]) / FMath::Max(1e-4f, R.Arc[i] - R.Arc[i - 1]); return (i - 1 + A) / (R.Arc.Num() - 1); }
	return 1.f;
}

void AJetwayRigs::AddBeacons(FRig& R, ABabylonModel* M, const FString& Id)
{
	// where: the cab's footprint (its parts' corners, along the way it faces - from the cab to its front - and across),
	// the two poles 25 cm in from its front corners standing on the roof, the third hanging under the cab's front; the
	// roof and the underside found by looking straight down and up at the jetway itself
	static UStaticMesh* Mesh = ADPKeep(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/JetwayBeacon/SM_JwBeacon.SM_JwBeacon")));
	static UMaterialInterface* GlowM = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Lamps/M_LampGlow.M_LampGlow")));
	if (!Mesh || !R.G[Cab].Parts.Num()) { ADP_WARN(TEXT("Jetways: no beacon mesh (Tools/import_jetway_beacon.py) or no cab parts on %s"), *Id); return; }
	UInstancedStaticMeshComponent* Host = R.G[Cab].Parts[0].K;
	const int32 Fr = M->NodeIndex(Id + TEXT("/CAB-FRONT"));
	const FVector CabL = R.G[Cab].Docked.GetLocation();
	// (its front: away from its sleeve, the joint to the tunnel behind it; the front frame's own origin sits mid-cab)
	(void)Fr;
	// (its front: towards the doorway frame with the rubber seal round it - measured from the frame's own meshes)
	FVector Seal = FVector::ZeroVector; int32 NSeal = 0;
	{
		TArray<int32> Todo; const int32 Cn = M->NodeIndex(Id + TEXT("/CAB-FRONT")); if (Cn != INDEX_NONE) Todo.Add(Cn);   // (the doorway frame with its rubber seal)
		while (Todo.Num())
		{
			const int32 I = Todo.Pop(); Todo.Append(M->NodeKids(I));
			const TPair<UInstancedStaticMeshComponent*, int32> In = M->NodeInstance(I);
			if (!In.Key || In.Value == INDEX_NONE || !In.Key->GetStaticMesh()) continue;
			FTransform PT; In.Key->GetInstanceTransform(In.Value, PT, false);
			Seal += PT.TransformPosition(In.Key->GetStaticMesh()->GetBoundingBox().GetCenter()); ++NSeal;
		}
	}
	// (from the middle of the cab's metal body - the cab's own origin sits at its front, by the door)
	FVector BodyMid = CabL; FBox Fb(ForceInit);
	{
		FBox Bb(ForceInit); const int32 Cb = M->NodeIndex(Id + TEXT("/CAB"));
		if (Cb != INDEX_NONE) for (const int32 K : M->NodeKids(Cb))
		{
			if (!M->NodeName(K).StartsWith(Id + TEXT("/CAB:"))) continue;
			const TPair<UInstancedStaticMeshComponent*, int32> In = M->NodeInstance(K);
			if (!In.Key || In.Value == INDEX_NONE || !In.Key->GetStaticMesh()) continue;
			FTransform PT; In.Key->GetInstanceTransform(In.Value, PT, false); Bb += In.Key->GetStaticMesh()->GetBoundingBox().TransformBy(PT);
		}
		if (Bb.IsValid) BodyMid = Bb.GetCenter();
		TArray<int32> T2; const int32 Cf = M->NodeIndex(Id + TEXT("/CAB-FRONT")); if (Cf != INDEX_NONE) T2.Add(Cf);
		while (T2.Num()) { const int32 I = T2.Pop(); T2.Append(M->NodeKids(I)); const TPair<UInstancedStaticMeshComponent*, int32> In = M->NodeInstance(I); if (!In.Key || In.Value == INDEX_NONE || !In.Key->GetStaticMesh()) continue; FTransform PT; In.Key->GetInstanceTransform(In.Value, PT, false); Fb += In.Key->GetStaticMesh()->GetBoundingBox().TransformBy(PT); }
		ADP_LOG(TEXT("Jetways: %s cab body box %s .. %s; front frame box %s .. %s; sleeve at %s; cab node at %s"), *Id, *Bb.Min.ToString(), *Bb.Max.ToString(), *Fb.Min.ToString(), *Fb.Max.ToString(), *R.G[CabSleeve].Docked.GetLocation().ToString(), *CabL.ToString());
	}
	// the front: the way his CAB-FRONT part's own place lies from the cab's (the owner's picture, 2026-11-16: with this
	// way the lights stood right on the rubber seal - so this is the front; they go 80 cm further back, on the metal)
	FVector F = Fr != INDEX_NONE ? (M->NodeModel(Fr).GetLocation() - CabL).GetSafeNormal2D() : FVector::ZeroVector; if (F.IsNearlyZero()) F = R.DirD;
	(void)Seal; (void)NSeal;
	// the cab's whole outline along that way (all its parts: the seal at the very front)
	float FAll = -1e9f, LA0 = 1e9f, LA1 = -1e9f;
	for (const FPart& P : R.G[Cab].Parts)
	{
		if (!P.K || !P.K->GetStaticMesh()) continue;
		const FBox B = P.K->GetStaticMesh()->GetBoundingBox();
		for (int32 c = 0; c < 8; ++c)
		{
			const FVector V = P.Docked.TransformPosition(FVector(c & 1 ? B.Max.X : B.Min.X, c & 2 ? B.Max.Y : B.Min.Y, c & 4 ? B.Max.Z : B.Min.Z)) - CabL;
			FAll = FMath::Max(FAll, (float)FVector::DotProduct(V, F));
			const float b = FVector::DotProduct(V, FVector(-F.Y, F.X, 0.f)); LA0 = FMath::Min(LA0, b); LA1 = FMath::Max(LA1, b);
		}
	}
	const FVector L(-F.Y, F.X, 0.f);
	R.CabMidD = BodyMid; R.FrontD = F;
	// the cab's own metal body only (the meshes straight under J-n/CAB: not its front frame, the rubber canopy or the
	// doors), so the roof lights stand on the metal housing just behind the rubber seal (owner, 2026-11-16)
	float F0 = 1e9f, F1 = -1e9f, L0 = 1e9f, L1 = -1e9f, Z0 = 1e9f, Z1 = -1e9f;
	const int32 CabN = M->NodeIndex(Id + TEXT("/CAB"));
	if (CabN != INDEX_NONE)
		for (const int32 K : M->NodeKids(CabN))
		{
			if (!M->NodeName(K).StartsWith(Id + TEXT("/CAB:"))) continue;
			const TPair<UInstancedStaticMeshComponent*, int32> In = M->NodeInstance(K);
			if (!In.Key || In.Value == INDEX_NONE || !In.Key->GetStaticMesh()) continue;
			FTransform PT; In.Key->GetInstanceTransform(In.Value, PT, false);
			const FBox B = In.Key->GetStaticMesh()->GetBoundingBox();
			for (int32 c = 0; c < 8; ++c)
			{
				const FVector V = PT.TransformPosition(FVector(c & 1 ? B.Max.X : B.Min.X, c & 2 ? B.Max.Y : B.Min.Y, c & 4 ? B.Max.Z : B.Min.Z)) - CabL;
				const float a = FVector::DotProduct(V, F), b = FVector::DotProduct(V, L);
				F0 = FMath::Min(F0, a); F1 = FMath::Max(F1, a); L0 = FMath::Min(L0, b); L1 = FMath::Max(L1, b); Z0 = FMath::Min(Z0, (float)V.Z); Z1 = FMath::Max(Z1, (float)V.Z);
			}
		}
	if (F1 < F0) { ADP_WARN(TEXT("Jetways: %s - no cab body meshes found for the beacons"), *Id); return; }
	// where the seal's frame starts, going forward (the lights stand on the metal 25 cm behind it: owner)
	float SealA = F1;
	if (Fb.IsValid) { SealA = 1e9f; for (int32 c = 0; c < 8; ++c) SealA = FMath::Min(SealA, (float)FVector::DotProduct(FVector(c & 1 ? Fb.Max.X : Fb.Min.X, c & 2 ? Fb.Max.Y : Fb.Min.Y, 0.f) - FVector(CabL.X, CabL.Y, 0.f), F)); }
	const FTransform W = Host->GetComponentTransform();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(JetwayBeacon), true);
	// the jetway's own surface (its model only) straight down (Dir -1) or up (+1) from a point (local): its local z
	auto Hit = [&](const FVector& Local, float Dir, float& Z) -> bool
	{
		TArray<FHitResult> Hs; const FVector A = W.TransformPosition(Local), B2 = W.TransformPosition(Local + FVector(0, 0, Dir * 400.f));
		GetWorld()->LineTraceMultiByChannel(Hs, A, B2, ECC_Visibility, Q);
		for (const FHitResult& H : Hs) if (H.GetActor() == Host->GetOwner()) { Z = W.InverseTransformPosition(H.ImpactPoint).Z; return true; }
		FHitResult H1; FCollisionQueryParams Q2 = Q; for (int32 k = 0; k < 6; ++k)
		{
			if (!GetWorld()->LineTraceSingleByChannel(H1, A, B2, ECC_Visibility, Q2)) break;
			if (H1.GetActor() == Host->GetOwner()) { Z = W.InverseTransformPosition(H1.ImpactPoint).Z; return true; }
			Q2.AddIgnoredActor(H1.GetActor());
		}
		return false;
	};
	// the copies, one list per mesh/colour: the yellow beacons (under the cab, at the lift columns; R.Bcn is also the
	// parent every light hangs on), the red roof beacons, the doorway bar, the floodlights, the crew's red and green lamp
	static UStaticMesh* BarMesh = ADPKeep(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/JetwayBeacon/SM_JwLightBar.SM_JwLightBar")));
	static UStaticMesh* RedMesh = ADPKeep(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/JetwayBeacon/SM_JwBeaconRed.SM_JwBeaconRed")));
	static UStaticMesh* FloodMesh = ADPKeep(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/JetwayBeacon/SM_JwFloodWhite.SM_JwFloodWhite")));
	static UStaticMesh* LampRedMesh = ADPKeep(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/JetwayBeacon/SM_JwLampRed.SM_JwLampRed")));
	static UStaticMesh* LampGreenMesh = ADPKeep(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/JetwayBeacon/SM_JwLampGreen.SM_JwLampGreen")));
	auto MakeK = [&](UStaticMesh* Me) -> UInstancedStaticMeshComponent*
	{
		if (!Me) return nullptr;
		UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this);
		K->SetStaticMesh(Me); K->SetMobility(EComponentMobility::Movable);
		K->SetupAttachment(Host); K->SetRelativeTransform(FTransform::Identity);
		K->SetNumCustomDataFloats(1); K->SetCastShadow(true);
		K->RegisterComponent();
		return K;
	};
	R.Bcn = MakeK(Mesh); R.BcnRed = MakeK(RedMesh ? RedMesh : Mesh); R.Bar = MakeK(BarMesh);
	UInstancedStaticMeshComponent* FloodK = MakeK(FloodMesh), *SigRedK = MakeK(LampRedMesh), *SigGreenK = MakeK(LampGreenMesh);
	const FLinearColor Amber(1.f, 0.22f, 0.f), Red(1.f, 0.0f, 0.0f), White(1.f, 0.95f, 0.86f);
	// the doorway frame's face over the door (the bar and the crew's lamp go on it): found once
	FVector FaceP = FVector::ZeroVector; bool bFace = false;
	{
		const FVector From = CabL + F * (FAll + 120.f) + L * ((LA0 + LA1) / 2) + FVector(0, 0, 230.f);
		TArray<FHitResult> Hs; GetWorld()->LineTraceMultiByChannel(Hs, W.TransformPosition(From), W.TransformPosition(From - F * 400.f), ECC_Visibility, Q);
		for (const FHitResult& H : Hs) if (H.GetActor() == Host->GetOwner()) { FaceP = W.InverseTransformPosition(H.ImpactPoint) + F * 0.5f; bFace = true; break; }
	}
	const FQuat FaceRot = FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(L.Y, L.X)), 0.f).Quaternion();   // (length across the cab, the lens facing out)
	// the lift columns (J-n/LIFT-LEG under the drive unit): the amber lamps at their feet
	TArray<FVector> Legs;
	for (int32 I = 0; I < M->NodeCount(); ++I) if (M->NodeName(I) == Id + TEXT("/LIFT-LEG")) Legs.Add(M->NodeModel(I).GetLocation());
	const FVector BogieAt = R.G[Bogie].Docked.GetLocation();
	// the kinds: 0, 1 roof red; 2 under the cab; 3 the doorway bar; 4, 5 cab floodlights; 6 the tunnel's floodlight;
	// 7, 8 the lift columns' ambers; 9 the crew's red, 10 the crew's green
	for (int32 b = 0; b <= 10; ++b)
	{
		UInstancedStaticMeshComponent* K = b <= 1 ? R.BcnRed : b == 2 || b == 7 || b == 8 ? R.Bcn : b == 3 ? R.Bar : b <= 6 ? FloodK : b == 9 ? SigRedK : SigGreenK;
		if (!K) continue;
		int32 G = Cab; FVector P = CabL; bool bFound = false; float Zh = 0.f; FQuat Rot = FQuat::Identity; float Scale = 1.5f;
		FQuat LightRel = FQuat::Identity; bool bFixedLight = false;
		if (b <= 1)
		{
			// (owner's red dots, 2026-11-16: by the doorway's edge of the roof, both the same distance in from their sides)
			for (float a = FAll - 100.f; !bFound && a > FAll - 300.f; a -= 15.f)
				for (float In = 30.f; !bFound && In < (LA1 - LA0) / 2; In += 10.f)
				{
					float Za = 0.f, Zb = 0.f;
					const FVector Pa = CabL + F * a + L * (LA0 + In), Pb = CabL + F * a + L * (LA1 - In);
					if (Hit(FVector(Pa.X, Pa.Y, CabL.Z + Z1 + 60.f), -1.f, Za) && Za > CabL.Z + Z1 - 40.f && Hit(FVector(Pb.X, Pb.Y, CabL.Z + Z1 + 60.f), -1.f, Zb) && Zb > CabL.Z + Z1 - 40.f)
					{ bFound = true; P = b == 0 ? Pa : Pb; Zh = b == 0 ? Za : Zb; }
				}
			P.Z = Zh;
		}
		else if (b == 2) { P = CabL + F * ((F0 + F1) / 2) + L * ((L0 + L1) / 2); bFound = Hit(FVector(P.X, P.Y, CabL.Z + Z0 - 60.f), 1.f, Zh); P.Z = Zh; Rot = FQuat(F, PI); }
		else if (b == 3) { bFound = bFace; P = FaceP; Rot = FaceRot; Scale = 1.f; LightRel = FRotator(-30.f, -90.f, 0.f).Quaternion(); bFixedLight = true; }
		else if (b == 4 || b == 5)
		{
			// under the cab's front, left and right, aimed ahead and down at the apron (Syracuse spec: ~10 m ahead)
			P = CabL + F * (F0 + (F1 - F0) * 0.75f) + L * (b == 4 ? L0 + 60.f : L1 - 60.f);
			bFound = Hit(FVector(P.X, P.Y, CabL.Z + Z0 - 60.f), 1.f, Zh); P.Z = Zh;
			Rot = FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(F.Y, F.X)), 0.f).Quaternion(); Scale = 1.f;
			LightRel = FRotator(-32.f, 0.f, 0.f).Quaternion(); bFixedLight = true;
		}
		else if (b == 6)
		{
			// under the last section, just behind the drive unit, aimed down at the wheels
			G = Tun3; P = R.Wd - R.AxisD * 180.f;
			float Zu = 0.f; bFound = Hit(FVector(P.X, P.Y, P.Z - 250.f), 1.f, Zu); P.Z = Zu;
			Rot = FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(R.DirD.Y, R.DirD.X)), 0.f).Quaternion(); Scale = 1.f;
			LightRel = FRotator(-70.f, 0.f, 0.f).Quaternion(); bFixedLight = true;
		}
		else if (b == 7 || b == 8)
		{
			// at the feet of the lift columns, on the drive unit's yoke, a little outboard
			G = Bogie;
			if (Legs.Num() >= 2) { const FVector Leg = Legs[b - 7]; const FVector Out = FVector(Leg.X - BogieAt.X, Leg.Y - BogieAt.Y, 0.f).GetSafeNormal(); P = Leg + Out * 26.f; bFound = true; }   // (26 cm: its base plate fully on the yoke's plate - owner, 2026-11-17: it stuck out)
			Scale = 1.f;
		}
		else
		{
			// the crew's lamp (SAE AS7995 / ShinMaywa EP4501798A1: seen through the door's window): on the doorway frame's
			// face, beside the door at window height - red above, green below
			// (on the face over the doorway, just left of the light bar - facing the aircraft door's window: red, then green)
			bFound = bFace; Rot = FaceRot; Scale = 1.f;
			P = FaceP - L * (b == 9 ? 74.f : 90.f);
		}
		if (!bFound) { ADP_WARN(TEXT("Jetways: %s lamp %d - no place found, left out"), *Id, b); continue; }
		const FTransform T(Rot, P, FVector(Scale));
		const int32 Ix = K->AddInstance(T, false);
		R.BcnK.Add(K); R.BcnIx.Add(Ix); R.BcnSide.Add(uint8(b == 1 || b == 8 ? 1 : 0)); R.BcnKind.Add(uint8(b));
		R.BcnRel.Add(LightRel); R.BcnFixed.Add(bFixedLight);
		FPart Pt; Pt.K = K; Pt.Ix = Ix; Pt.Docked = T; R.G[G].Parts.Add(Pt);
		// the light each throws (shadows on: nothing shines through the cab's walls into it - owner)
		USceneComponent* LiOut = nullptr;
		if (b <= 8)
		{
			USpotLightComponent* Sp = NewObject<USpotLightComponent>(this);
			const bool bFlood = b >= 4 && b <= 6, bBar = b == 3, bUnder = b == 2, bLeg = b >= 7;
			Sp->SetOuterConeAngle(bFlood ? 48.f : bBar ? 60.f : bUnder ? 50.f : bLeg ? 40.f : 30.f); Sp->SetInnerConeAngle(bFlood ? 22.f : bBar ? 30.f : bUnder ? 8.f : 10.f);
			Sp->SetupAttachment(R.Bcn);
			Sp->SetRelativeLocation(T.TransformPosition(bBar ? FVector(0.f, -14.f, 0.f) : bFlood ? FVector(0.f, 0.f, -18.f) : FVector(0, 0, 70.f)));
			if (bFixedLight) Sp->SetRelativeRotation((T.GetRotation() * LightRel).Rotator());
			Sp->SetIntensityUnits(ELightUnits::Lumens);
			Sp->SetIntensity(bFlood ? (b == 6 ? 3500.f : 7000.f) : bBar ? 360.f : bUnder ? 1080.f : bLeg ? 900.f : 9000.f);
			Sp->SetLightColor(bFlood ? White : b <= 1 ? Red : Amber);
			Sp->SetAttenuationRadius(bFlood ? 2400.f : bBar ? 900.f : bUnder ? 1200.f : bLeg ? 800.f : 2800.f);
			if (bUnder || bBar || bFlood || bLeg) Sp->SetSourceRadius(bFlood ? 12.f : 40.f);
			Sp->SetCastShadows(true); Sp->SetVisibility(false); Sp->RegisterComponent();
			LiOut = Sp;
		}
		// the roof lamps' faint glow on the roof round them (owner: much fainter than on the ground)
		UPointLightComponent* Fill = nullptr;
		if (b <= 1)
		{
			Fill = NewObject<UPointLightComponent>(this); Fill->SetupAttachment(R.Bcn);
			Fill->SetRelativeLocation(T.TransformPosition(FVector(0, 0, 50.f)));
			Fill->SetIntensityUnits(ELightUnits::Lumens); Fill->SetIntensity(350.f); Fill->SetLightColor(Red);
			Fill->SetAttenuationRadius(320.f); Fill->SetSourceRadius(8.f); Fill->SetSpecularScale(0.3f); Fill->SetCastShadows(true); Fill->SetVisibility(false); Fill->RegisterComponent();
		}
		R.BcnGlow.Add(nullptr); R.BcnLight.Add(LiOut); R.BcnFill.Add(Fill);
		K->SetCustomDataValue(Ix, 0, 0.f, false);
		ADP_LOG(TEXT("Jetways: %s lamp %d at %s"), *Id, b, *P.ToString());
	}
	for (UInstancedStaticMeshComponent* K : TArray<UInstancedStaticMeshComponent*>{R.Bcn, R.BcnRed, R.Bar, FloodK, SigRedK, SigGreenK}) if (K) K->MarkRenderStateDirty();
	// its sounds, at the drive unit (CC0 recordings from Freesound - Art/Sounds/Jetway/SOURCES.md): the warning beeper
	// (the wait to drive and while it drives), the drive motor and the tyres (with its speed), the lift and canopy motors
	// (levelling at the door)
	auto Snd = [&](const TCHAR* Name) -> UAudioComponent*
	{
		USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/JetwayBeacon/Sounds/%s.%s"), Name, Name));
		if (!S) return nullptr;
		UAudioComponent* A = NewObject<UAudioComponent>(this);
		A->SetupAttachment(R.Bcn); A->SetRelativeLocation(BogieAt + FVector(0, 0, 80.f)); A->SetSound(S); A->bAutoActivate = false;
		A->bAllowSpatialization = true; A->bOverrideAttenuation = true;
		A->AttenuationOverrides.bAttenuate = true; A->AttenuationOverrides.bSpatialize = true;
		// heard out to 250 m (owner, 2026-11-16): full close by, fading gradually the further you are, gone at 250 m
		A->AttenuationOverrides.AttenuationShapeExtents = FVector(600.f); A->AttenuationOverrides.FalloffDistance = 25000.f - 600.f;
		A->AttenuationOverrides.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound; A->AttenuationOverrides.dBAttenuationAtMax = -60.f;
		A->SetLowPassFilterEnabled(true); A->SetLowPassFilterFrequency(20000.f);
		A->RegisterComponent();
		return A;
	};
	AddCameras(R, Host, CabL, F, L, F0, F1, L0, L1, Z0, Z1, FAll, Hit);
	R.SBeep = Snd(TEXT("JwBeeper")); R.SMotor = Snd(TEXT("JwMotor")); R.STyres = Snd(TEXT("JwTyres")); R.SLift = Snd(TEXT("JwLift"));
	ADP_LOG(TEXT("Jetways: %s lamps, floodlights, crew lamp and sounds placed (cab %.1f x %.1f m)"), *Id, (F1 - F0) / 100.f, (L1 - L0) / 100.f);
}

void AJetwayRigs::Beacons(FRig& R, bool bMoving)
{
	// what each lamp does (research 2026-11-16: Syracuse PBB spec, Darwin procedures, Oshkosh, SAE AS7995):
	//  - the red roof beacons (owner's design): turning while it drives
	//  - the amber beacon under the cab: turning whenever the bridge is powered - moving or docked ("may move at any moment")
	//  - the amber beacons at the lift columns: turning in the 3 s wait to drive and while it drives
	//  - the doorway light: lit while it moves and while docked
	//  - the floodlights (under the cab, ahead; under the tunnel, the wheels): on while in use when it is dark
	//  - the crew's lamp by the door: red while it moves, green once docked (safe to open), off at rest
	// The turning beam: his generator's 75 turns a minute, the dome flashing as it passes the eye.
	if (!R.BcnK.Num()) return;
	const bool bWaiting = R.Wait > 0.f, bDriving = bMoving || bWaiting;
	const bool bInUse = bDriving || R.St == EState::Docked;
	static TWeakObjectPtr<ADirectionalLight> Sun;
	if (!Sun.IsValid()) Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass()));
	const float El = Sun.IsValid() ? -Sun->GetActorRotation().Pitch : 30.f;
	const bool bDark = ASunClock::Theme == 4 || ASunClock::Theme == 6 || El < 3.f;
	const uint32 Key = (bDriving ? 1 : 0) | (bInUse ? 2 : 0) | (bDark ? 4 : 0) | (R.St == EState::Docked ? 8 : 0) | (bBeaconTest ? 16 : 0) | (uint32(BeaconForce + 1) << 5);
	if (!bDriving && !bBeaconTest && Key == R.LampKey) return;   // (nothing turning and nothing changed: nothing to do)
	R.LampKey = Key;
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const FVector Eye = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	const float Turn = FMath::Fmod(BcnClock * 75.f / 60.f, 1.f) * 360.f;
	const bool bT = bBeaconTest;
	for (int32 b = 0; b < R.BcnK.Num(); ++b)
	{
		const uint8 Kind = R.BcnKind[b];
		bool bOn = false;
		switch (Kind)
		{
		case 0: case 1: bOn = bMoving || bT; break;
		case 2: bOn = bInUse || bT; break;
		case 3: bOn = bInUse || bT; break;
		case 4: case 5: case 6: bOn = (bInUse || bT) && bDark; break;
		case 7: case 8: bOn = bDriving || bT; break;
		case 9: bOn = bDriving || (bT && R.St != EState::Docked); break;
		case 10: bOn = R.St == EState::Docked && !bDriving; break;
		}
		if (BeaconForce == 0) bOn = false;
		const bool bTurning = Kind <= 2 || Kind == 7 || Kind == 8;
		float Lens = bOn ? 1.f : 0.f;
		if (bOn && bTurning)
			if (USpotLightComponent* Sp = Cast<USpotLightComponent>(R.BcnLight[b]))
			{
				Sp->SetRelativeRotation(FRotator(Kind == 2 ? -40.f : Kind >= 7 ? -20.f : -8.f, Turn + (R.BcnSide[b] ? 180.f : 0.f) + (Kind == 2 ? 90.f : 0.f), 0.f));
				const FVector Dir = Sp->GetForwardVector(), To = (Eye - Sp->GetComponentLocation()).GetSafeNormal2D();
				const float Face = FMath::Max(0.f, (float)FVector::DotProduct(Dir.GetSafeNormal2D(), To));
				Lens = BeaconForce == 1 ? 1.f : 0.25f + 0.75f * FMath::Pow(Face, 6.f);
			}
		R.BcnK[b]->SetCustomDataValue(R.BcnIx[b], 0, Lens, true);
		if (R.BcnLight[b] && R.BcnLight[b]->IsVisible() != bOn) R.BcnLight[b]->SetVisibility(bOn);
		if (UPointLightComponent* Fl = R.BcnFill.IsValidIndex(b) ? Cast<UPointLightComponent>(R.BcnFill[b]) : nullptr)
		{
			if (Fl->IsVisible() != bOn) Fl->SetVisibility(bOn);
			if (bOn) Fl->SetIntensity(350.f * (0.5f + 0.5f * Lens));   // (pulsing faintly with the turning lamp)
		}
	}
	R.bBcnOn = bMoving;
}

void AJetwayRigs::Sounds(FRig& R, bool bMoving, float Dt)
{
	// the beeper while it waits to drive and while it drives; the motor and the tyres rising and falling with its speed
	// (the motor's pitch too); the lift and canopy motors while it levels at the door (the canopy time)
	const bool bDriving = bMoving || R.Wait > 0.f;
	auto Run = [](UAudioComponent* A, bool bOn, float Vol, float Pitch)
	{
		if (!A) return;
		if (bOn && !A->IsPlaying()) A->Play(FMath::FRand() * 2.f);
		if (!bOn && A->IsPlaying()) A->FadeOut(0.4f, 0.f);
		if (bOn) { A->SetVolumeMultiplier(FMath::Max(0.001f, Vol)); A->SetPitchMultiplier(Pitch); }
	};
	const float Sp = FMath::Clamp(R.V / 0.30f, 0.f, 1.f);
	const bool bLevel = (R.St == EState::Docking && R.D >= R.PathL) || (R.St == EState::Undocking && R.T <= 15.f);
	// where you hear it from (owner, 2026-11-16): outside as it is; inside a jetway quieter and muffled (through its walls);
	// in the terminal by the gates quieter and more muffled still, but there; the change gliding as you walk between
	float Want = 1.f, WantLp = 20000.f;
	if ((bDriving || bLevel) && GetWorld())
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const FVector Ear = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
		bool bInJetway = false;
		for (int32 J = 0; J < Rigs.Num() && !bInJetway; ++J)
		{
			FVector Ro, Dr, Mid, Fr;
			if (!Frame(J, Ro, Dr) || !CabView(J, Mid, Fr)) continue;
			const FVector A = Ro, B = Mid; const FVector AB = B - A;
			const float U = FMath::Clamp(FVector::DotProduct(Ear - A, AB) / FMath::Max(1.f, (float)AB.SizeSquared()), 0.f, 1.f);
			const FVector Near = A + AB * U;
			if (FVector::Dist2D(Ear, Near) < 200.f && Ear.Z > Near.Z - 150.f && Ear.Z < Near.Z + 330.f) bInJetway = true;
		}
		if (bInJetway) { Want = 0.55f; WantLp = 2600.f; }
		else
		{
			// under a roof and not in a jetway: in the terminal
			FHitResult H; FCollisionQueryParams Qr(SCENE_QUERY_STAT(JetwayEar), false);
			if (const APawn* Me = PC ? PC->GetPawn() : nullptr) Qr.AddIgnoredActor(Me);
			if (GetWorld()->LineTraceSingleByChannel(H, Ear, Ear + FVector(0, 0, 3000.f), ECC_Visibility, Qr)) { Want = 0.28f; WantLp = 1300.f; }
		}
	}
	const float K = FMath::Clamp(Dt * 2.5f, 0.f, 1.f);
	R.Env += (Want - R.Env) * K; R.EnvLp += (WantLp - R.EnvLp) * K;
	for (UAudioComponent* A : {R.SBeep, R.SMotor, R.STyres, R.SLift}) if (A && A->IsPlaying()) A->SetLowPassFilterFrequency(R.EnvLp);
	Run(R.SBeep, bDriving, 0.55f * R.Env, 1.f);
	Run(R.SMotor, bMoving && R.V > 0.005f, (0.25f + 0.55f * Sp) * R.Env, 0.85f + 0.3f * Sp);
	Run(R.STyres, bMoving && R.V > 0.02f, 0.6f * Sp * R.Env, 0.9f + 0.2f * Sp);
	Run(R.SLift, bLevel, 0.5f * R.Env, 1.f);
	(void)Dt;
}

void AJetwayRigs::AddSignLights(FRig& R, ABabylonModel* M, const FString& Id)
{
	// the gate-number sign (J-n/GATE-NUMBER on the cab's roof): its own face glowing faintly - the blue board in its own
	// blue, the number picture from its own picture - far weaker than the beacons, no lamp, no spot on it (owner,
	// 2026-11-16). The terminal's materials glow by EmissiveTexture x EC x EB: the face's own colour and picture fed in.
	TSet<UInstancedStaticMeshComponent*> Done;
	for (int32 I = 0; I < M->NodeCount(); ++I)
	{
		if (!M->NodeName(I).StartsWith(Id + TEXT("/GATE-NUMBER"))) continue;
		const TPair<UInstancedStaticMeshComponent*, int32> In = M->NodeInstance(I);
		if (!In.Key || Done.Contains(In.Key)) continue;
		Done.Add(In.Key);
		// (owner: the sign only gives light, never takes it - no lamp, sun or beacon lights it; only its own glow shows)
		In.Key->SetLightingChannels(false, false, false);
		for (int32 m = 0; m < In.Key->GetNumMaterials(); ++m)
		{
			UMaterialInterface* Mi = In.Key->GetMaterial(m);
			if (!Mi || Mi->GetName().Contains(TEXT("AluB"))) continue;   // (the frame and posts stay plain metal)
			UMaterialInstanceDynamic* D = In.Key->CreateDynamicMaterialInstance(m, Mi);
			if (!D) continue;
			FLinearColor Bc(1, 1, 1, 1); D->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("BC")), Bc);
			UTexture* Base = nullptr; D->GetTextureParameterValue(FHashedMaterialParameterInfo(TEXT("Base")), Base);
			if (Base) D->SetTextureParameterValue(TEXT("Emi"), Base);
			D->SetVectorParameterValue(TEXT("EC"), Bc);
			D->SetScalarParameterValue(TEXT("EB"), SignGlow);
		}
	}
	ADP_LOG(TEXT("Jetways: %s gate sign glowing faintly (%d parts)"), *Id, Done.Num());
}
bool AJetwayRigs::CabView(int32 J, FVector& Mid, FVector& Front) const
{
	if (!Rigs.IsValidIndex(J) || !Rigs[J].Model.IsValid()) return false;
	const FRig& R = Rigs[J];
	FTransform CabNow = R.G[Cab].Docked;
	if (R.G[Cab].Parts.Num()) { FTransform T; R.G[Cab].Parts[0].K->GetInstanceTransform(R.G[Cab].Parts[0].Ix, T, false); CabNow = R.G[Cab].Docked * R.G[Cab].Parts[0].Docked.Inverse() * T; }
	const FTransform A = R.Model->GetActorTransform();
	const FTransform Rel = R.G[Cab].Docked.Inverse() * CabNow;   // (docked -> now)
	Mid = A.TransformPosition(Rel.TransformPosition(R.CabMidD));
	Front = A.TransformVector(Rel.TransformVector(R.FrontD)).GetSafeNormal2D();
	return true;
}

void AJetwayRigs::GatherRiders(const FRig& R, TArray<FRider>& Out) const
{
	// each character whose floor is one of this jetway's moving parts (a tunnel section, the cab, a floor plate across
	// the bellows): that part's copy and where it is now
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Ch = *It;
		const UCharacterMovementComponent* Mv = Ch ? Ch->GetCharacterMovement() : nullptr;
		if (!Mv || Ch == HeldAt.Who.Get() || !Mv->CurrentFloor.IsWalkableFloor()) continue;   // (the one at the console: MoveHeld)
		UInstancedStaticMeshComponent* K = Cast<UInstancedStaticMeshComponent>(Mv->CurrentFloor.HitResult.GetComponent());
		const int32 Ix = Mv->CurrentFloor.HitResult.Item;
		if (!K || Ix == INDEX_NONE) continue;
		bool bOurs = false;
		for (int32 g = 0; g < NumGroups && !bOurs; ++g) if (g != Boom) for (const FPart& P : R.G[g].Parts) if (P.K == K && P.Ix == Ix) { bOurs = true; break; }
		for (const FStretch& X : R.Stretch) if (!bOurs && X.P.K == K && X.P.Ix == Ix) bOurs = true;
		if (!bOurs) continue;
		FRider Rd; Rd.Ch = Ch; Rd.K = K; Rd.Ix = Ix; K->GetInstanceTransform(Ix, Rd.Before, true);
		Out.Add(Rd);
	}
}

// ---- at one with the jetway while you use its screen (owner, 2026-11-25, to-do 265) ---------------------------------
bool AJetwayRigs::ScreenNow(int32 J, FTransform& Out) const
{
	if (!Rigs.IsValidIndex(J) || !Rigs[J].Scr.K) return false;
	FTransform T; if (!Rigs[J].Scr.K->GetInstanceTransform(Rigs[J].Scr.Ix, T, true)) return false;
	Out = FTransform(T.GetRotation(), T.GetLocation());   // (its place and turn; the copy's scale plays no part)
	return true;
}

bool AJetwayRigs::HoldAt(int32 J, AActor* Cam, ACharacter* Who)
{
	LetGo();
	FTransform S; if (!ScreenNow(J, S)) { ADP_WARN(TEXT("Jetways: jetway %d has no screen to hold anyone at"), J); return false; }
	HeldAt.J = J; HeldAt.Cam = Cam; HeldAt.Who = Who;
	if (Cam) HeldAt.CamRel = FTransform(Cam->GetActorQuat(), Cam->GetActorLocation()).GetRelativeTransform(S);
	if (Who)
	{
		if (UCharacterMovementComponent* Mv = Who->GetCharacterMovement()) { Mv->StopMovementImmediately(); Mv->DisableMovement(); }
		HeldAt.WhoRel = FTransform(Who->GetActorQuat(), Who->GetActorLocation()).GetRelativeTransform(S);
	}
	return true;
}

void AJetwayRigs::LetGo()
{
	if (ACharacter* Who = HeldAt.Who.Get())
		if (UCharacterMovementComponent* Mv = Who->GetCharacterMovement())
			if (Mv->MovementMode == MOVE_None) Mv->SetMovementMode(MOVE_Walking);
	HeldAt = FHeld();
}

void AJetwayRigs::MoveHeld()
{
	if (HeldAt.J == INDEX_NONE) return;
	FTransform S; if (!ScreenNow(HeldAt.J, S)) { ADP_WARN(TEXT("Jetways: the screen of jetway %d is gone while in use"), HeldAt.J); LetGo(); return; }
	if (AActor* Cam = HeldAt.Cam.Get()) { const FTransform C = HeldAt.CamRel * S; Cam->SetActorLocationAndRotation(C.GetLocation(), C.GetRotation()); }
	if (ACharacter* Who = HeldAt.Who.Get())
	{
		const FTransform P = HeldAt.WhoRel * S;
		const FRotator Was = Who->GetActorRotation();
		const float Turn = FMath::FindDeltaAngleDegrees((float)Was.Yaw, (float)P.Rotator().Yaw);
		if (FVector::DistSquared(P.GetLocation(), Who->GetActorLocation()) < 1e-6f && FMath::Abs(Turn) < 1e-4f) return;
		Who->SetActorLocationAndRotation(P.GetLocation(), FRotator(Was.Pitch, Was.Yaw + Turn, Was.Roll), false, nullptr, ETeleportType::TeleportPhysics);
		if (AController* C = Who->GetController()) if (C->IsPlayerController()) C->SetControlRotation(C->GetControlRotation() + FRotator(0.f, Turn, 0.f));
	}
}

void AJetwayRigs::LogLeftBehind(const FRig& R, const TArray<FRider>& Riders)
{
	// looked at once every 2 s for each moving jetway: anyone inside a tunnel section or the cab who isn't in Riders
	float& Last = LeftLog.FindOrAdd(R.Id, -10.f);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - Last < 2.f) return;
	Last = Now;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Ch = *It;
		if (!Ch || Ch->IsHidden() || Ch == HeldAt.Who.Get() || Riders.ContainsByPredicate([Ch](const FRider& X) { return X.Ch.Get() == Ch; })) continue;
		for (const int32 g : {int32(Tun1), int32(Tun2), int32(Tun3), int32(Cab)})
		{
			if (!R.MapBox[g].IsValid || !R.G[g].Parts.Num() || !R.G[g].Parts[0].K) continue;
			const FVector L = (NowOf(R, g) * R.G[g].Parts[0].K->GetComponentTransform()).InverseTransformPosition(Ch->GetActorLocation());
			if (!R.MapBox[g].IsInside(L)) continue;
			const UCharacterMovementComponent* Mv = Ch->GetCharacterMovement();
			const UPrimitiveComponent* Fl = Mv ? Mv->CurrentFloor.HitResult.GetComponent() : nullptr;
			ADP_WARN(TEXT("Jetways: %s moves and does not carry %s (in %s; floor %s item %d, movement mode %d)"), *R.Id, *Ch->GetName(), g == Cab ? TEXT("the cab") : TEXT("the tunnel"),
				Fl ? *Fl->GetName() : TEXT("none"), Mv ? Mv->CurrentFloor.HitResult.Item : -1, Mv ? (int32)Mv->MovementMode.GetValue() : -1);
			break;
		}
	}
}

void AJetwayRigs::CarryRiders(const TArray<FRider>& Riders) const
{
	// how far and how much their part moved and turned since: the person moved and turned the same (their view too)
	for (const FRider& Rd : Riders)
	{
		if (!Rd.Ch.IsValid() || !Rd.K) continue;
		FTransform After; Rd.K->GetInstanceTransform(Rd.Ix, After, true);
		const FTransform Delta = Rd.Before.Inverse() * After;
		if (Delta.Equals(FTransform::Identity, 0.01f)) continue;
		ACharacter* Ch = Rd.Ch.Get();
		const FVector At = After.TransformPosition(Rd.Before.InverseTransformPosition(Ch->GetActorLocation()));
		const float Yaw = (After.GetRotation() * Rd.Before.GetRotation().Inverse()).Rotator().Yaw;
		Ch->SetActorLocationAndRotation(At, Ch->GetActorRotation() + FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (AController* C = Ch->GetController()) if (C->IsPlayerController()) C->SetControlRotation(C->GetControlRotation() + FRotator(0.f, Yaw, 0.f));
	}
}

void AJetwayRigs::AddCameras(FRig& R, UInstancedStaticMeshComponent* Host, const FVector& CabL, const FVector& F, const FVector& L, float F0, float F1, float L0, float L1, float Z0, float Z1, float FAll, TFunctionRef<bool(const FVector&, float, float&)> Hit)
{
	// the jetway's CCTV (owner, 2026-11-18; Art/JetwayCamera): 1 under the last section looking at the drive unit's
	// wheels (a dome), 2 on the cab's ceiling looking at the aircraft door (a dome), 3 under the cab's front looking out over
	// the apron ahead (a dome), 4 on the first tunnel's roof by the rotunda looking along the bridge to the cab (a
	// bullet), 5 on the first tunnel's ceiling looking down the tunnel (a dome). Each rides with its part.
	static UStaticMesh* BulletM = ADPKeep(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/JetwayCamera/SM_JwCamBullet.SM_JwCamBullet")));
	static UStaticMesh* DomeM = ADPKeep(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/JetwayCamera/SM_JwCamDome.SM_JwCamDome")));
	if (!BulletM || !DomeM) { ADP_WARN(TEXT("Jetways: no CCTV camera meshes (Tools/import_jetway_camera.py)")); return; }
	auto MakeK = [&](UStaticMesh* Me)
	{
		UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this);
		K->SetStaticMesh(Me); K->SetMobility(EComponentMobility::Movable); K->SetupAttachment(Host); K->SetRelativeTransform(FTransform::Identity);
		K->SetCollisionEnabled(ECollisionEnabled::NoCollision); K->SetCastShadow(true); K->RegisterComponent(); return K;
	};
	UInstancedStaticMeshComponent* BK = MakeK(BulletM), *DK = MakeK(DomeM);
	R.CamHost = Host;
	auto Yaw = [](const FVector& V) { return FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X)), 0.f).Quaternion(); };
	auto Pitched = [](const FVector& V, float Deg) { const FVector H = FVector(V.X, V.Y, 0.f).GetSafeNormal(); return (H * FMath::Cos(FMath::DegreesToRadians(Deg)) + FVector::UpVector * FMath::Sin(FMath::DegreesToRadians(Deg))).GetSafeNormal(); };
	auto Put = [&](int32 G, bool bDome, const FVector& At, const FVector& View, const TCHAR* Name)
	{
		const FTransform T(bDome ? Yaw(View) : Yaw(View), At);
		UInstancedStaticMeshComponent* K = bDome ? DK : BK;
		const int32 Ix = K->AddInstance(T, false);
		FPart Pt; Pt.K = K; Pt.Ix = Ix; Pt.Docked = T; R.G[G].Parts.Add(Pt);
		FRig::FCam C; C.G = G; C.Lens = bDome ? At - FVector(0, 0, 6.f) : At + T.TransformVector(FVector(17.f, 0.f, 15.5f)); C.View = View; C.Name = Name;
		R.Cams.Add(C);
	};
	// the first tunnel section's outline (docked): along the tunnel, its middle, its top
	FBox Tb(ForceInit); for (const FPart& P : R.G[Tun1].Parts) if (P.K && P.K->GetStaticMesh() && P.K != BK && P.K != DK) Tb += P.K->GetStaticMesh()->GetBoundingBox().TransformBy(P.Docked);
	const FVector Ax = R.AxisD.GetSafeNormal();
	float A0 = 1e9f, A1 = -1e9f; if (Tb.IsValid) for (int32 c = 0; c < 8; ++c) { const float a = FVector::DotProduct(FVector(c & 1 ? Tb.Max.X : Tb.Min.X, c & 2 ? Tb.Max.Y : Tb.Min.Y, c & 4 ? Tb.Max.Z : Tb.Min.Z) - Tb.GetCenter(), Ax); A0 = FMath::Min(A0, a); A1 = FMath::Max(A1, a); }
	float Z = 0.f;
	// 1 the drive unit's wheels
	{ FVector P = R.Wd - Ax * 400.f; if (Hit(FVector(P.X, P.Y, P.Z - 250.f), 1.f, Z)) { P.Z = Z; const FVector Tg(R.Wd.X, R.Wd.Y, P.Z - 420.f); Put(Tun3, true, P, (Tg - P).GetSafeNormal(), TEXT("BOGIE")); } }
	// 2 the aircraft door, from the cab's ceiling behind it
	{ FVector P = CabL + F * (F0 + (F1 - F0) * 0.3f) + L * ((L0 + L1) / 2); if (Hit(FVector(P.X, P.Y, CabL.Z + Z0 + 120.f), 1.f, Z) && Z < CabL.Z + Z1 + 1.f) { P.Z = Z; Put(Cab, true, P, Pitched(F, -24.f), TEXT("AIRCRAFT DOOR")); } }
	// 3 the apron ahead, from under the cab's front
	{ FVector P = CabL + F * (F0 + (F1 - F0) * 0.85f) + L * ((L0 + L1) / 2); if (Hit(FVector(P.X, P.Y, CabL.Z + Z0 - 60.f), 1.f, Z)) { P.Z = Z; Put(Cab, true, P, Pitched(F, -30.f), TEXT("APRON")); } }
	if (Tb.IsValid)
	{
		const FVector Back = Tb.GetCenter() + Ax * (A0 + 0.18f * (A1 - A0));
		// 4 along the bridge, from the first tunnel's roof by the rotunda
		if (Hit(FVector(Back.X, Back.Y, Tb.Max.Z + 80.f), -1.f, Z)) Put(Tun1, false, FVector(Back.X, Back.Y, Z), Pitched(R.DirD, -6.f), TEXT("BRIDGE EXTERIOR"));
		// 5 down the tunnel, from its ceiling
		// (owner, 2026-11-19: it sat on a ceiling lamp. The first tunnel's lamp panels (Tools/jetway_*.py, its J-x/LED: 1.12 m
		// each, at 0.44-1.56, 3.04-4.16, 5.64-6.76, 8.24-9.36 m of its 10.5 m) - at 18 % from the rotunda end it was on the one
		// at 8.24-9.36. Now in the middle of the gap between the last two, 3.00 m from the rotunda end, still centred.)
		{
			const FVector S = Tb.GetCenter() + Ax * (A0 + 300.f);
			if (Hit(FVector(S.X, S.Y, Tb.GetCenter().Z - 20.f), 1.f, Z) && Z < Tb.Max.Z - 5.f) Put(Tun1, true, FVector(S.X, S.Y, Z), Pitched(Ax, -8.f), TEXT("TUNNEL"));
		}
	}
	for (UInstancedStaticMeshComponent* K : {BK, DK}) K->MarkRenderStateDirty();
	ADP_LOG(TEXT("Jetways: %s CCTV - %d cameras placed"), *R.Id, R.Cams.Num());
}

bool AJetwayRigs::CamView(int32 J, int32 K, FVector& At, FRotator& Rot, FString& Name) const
{
	if (!Rigs.IsValidIndex(J) || !Rigs[J].Cams.IsValidIndex(K) || !Rigs[J].CamHost) return false;
	const FRig& R = Rigs[J]; const FRig::FCam& C = R.Cams[K];
	const FTransform Rel = R.G[C.G].Docked.Inverse() * NowOf(R, C.G);   // (docked -> now)
	const FTransform W = R.CamHost->GetComponentTransform();
	At = W.TransformPosition(Rel.TransformPosition(C.Lens));
	Rot = W.TransformVector(Rel.TransformVector(C.View)).Rotation();
	Name = C.Name;
	return true;
}

FTransform AJetwayRigs::NowOf(const FRig& R, int32 G) const
{
	if (!R.G[G].Parts.Num()) return R.G[G].Docked;
	FTransform T; R.G[G].Parts[0].K->GetInstanceTransform(R.G[G].Parts[0].Ix, T, false);
	return R.G[G].Parts[0].Docked.GetRelativeTransform(R.G[G].Docked).Inverse() * T;
}

void AJetwayRigs::ApplySvc(const FRig& R, const FTransform& T3) const
{
	if (!R.Svc.Parts.Num()) return;
	const float U = FMath::Clamp(R.SvcOpen, 0.f, 1.f), E = U * U * (3 - 2 * U);
	const FQuat Q(FVector::UpVector, R.Svc.Close * E);
	for (const FPart& P : R.Svc.Parts)
	{
		const FTransform Pd(Q * P.Docked.GetRotation(), R.Svc.H + Q.RotateVector(P.Docked.GetLocation() - R.Svc.H), P.Docked.GetScale3D());
		P.K->UpdateInstanceTransform(P.Ix, Pd.GetRelativeTransform(R.G[Tun3].Docked) * T3, false, false, true);   // (no full rebuild of the list: each moved copy goes on its own - 2026-11-18, the hoses brought the 11-17 lag back)
	}
}

bool AJetwayRigs::ServiceDoorNear(const FVector& Feet, const FVector& Facing, bool& bOpen) const
{
	for (const FRig& R : Rigs)
	{
		if (!R.Svc.Parts.Num() || !R.Model.IsValid()) continue;
		const FVector Mid = R.Model->GetActorTransform().TransformPosition(NowOf(R, Tun3).TransformPosition(R.G[Tun3].Docked.InverseTransformPosition(R.SvcMid)));
		const float D = FVector::Dist2D(Feet, Mid);
		if (D > 170.f || FMath::Abs(Feet.Z - (Mid.Z - 100.f)) > 200.f) continue;
		if (D > 60.f && FVector::DotProduct((Mid - Feet).GetSafeNormal2D(), Facing.GetSafeNormal2D()) < 0.42f) continue;
		bOpen = R.SvcWant > 0.5f; return true;
	}
	return false;
}

bool AJetwayRigs::ServiceDoorFrame(int32 I, FVector& Mid, FVector& Out) const
{
	if (!Rigs.IsValidIndex(I)) return false;
	const FRig& R = Rigs[I];
	if (!R.Svc.Parts.Num() || !R.Model.IsValid()) return false;
	const FTransform W = NowOf(R, Tun3) * R.Model->GetActorTransform();
	const FTransform& Dk = R.G[Tun3].Docked;
	// the leaf's hinge and middle (shut) give the wall's line; out = across it, away from the section's own axis
	const FVector H = W.TransformPosition(Dk.InverseTransformPosition(R.Svc.H)), M = W.TransformPosition(Dk.InverseTransformPosition(R.SvcMid));
	FVector Along = (M - H).GetSafeNormal2D(); Out = FVector::CrossProduct(FVector::UpVector, Along).GetSafeNormal2D();
	if (FVector::DotProduct(Out, M - W.GetLocation()) < 0.f) Out = -Out;
	Mid = M; return true;
}

bool AJetwayRigs::ServiceSteps(int32 I, TArray<FVector>& Steps, float& HalfW) const
{
	Steps.Reset(); HalfW = 40.f;
	if (!Rigs.IsValidIndex(I)) return false;
	const FRig& R = Rigs[I];
	for (const int32 k : R.StepIx)
	{
		const FPart& P = R.Stair[k]; if (!P.K || !P.K->GetStaticMesh()) continue;
		FTransform T; P.K->GetInstanceTransform(P.Ix, T, true);
		const FBox L = P.K->GetStaticMesh()->GetBoundingBox(), B = L.TransformBy(T);
		Steps.Add(FVector(B.GetCenter().X, B.GetCenter().Y, B.Max.Z));
		HalfW = float((L.GetExtent() * T.GetScale3D().GetAbs()).GetMax());
	}
	Steps.Sort([](const FVector& A, const FVector& B) { return A.Z < B.Z; });
	for (int32 i = Steps.Num() - 1; i > 0; --i) if (Steps[i].Z - Steps[i - 1].Z < 4.f) Steps.RemoveAt(i);   // (a tread of several pieces)
	return Steps.Num() >= 6;
}

void AJetwayRigs::SetServiceDoor(int32 I, bool bOpen) { if (Rigs.IsValidIndex(I)) Rigs[I].SvcWant = bOpen ? 1.f : 0.f; }
ABabylonModel* AJetwayRigs::RigModel(int32 I) const { return Rigs.IsValidIndex(I) ? Rigs[I].Model.Get() : nullptr; }

bool AJetwayRigs::UseServiceDoor(const FVector& Feet, const FVector& Facing)
{
	for (FRig& R : Rigs)
	{
		if (!R.Svc.Parts.Num() || !R.Model.IsValid()) continue;
		const FVector Mid = R.Model->GetActorTransform().TransformPosition(NowOf(R, Tun3).TransformPosition(R.G[Tun3].Docked.InverseTransformPosition(R.SvcMid)));
		const float D = FVector::Dist2D(Feet, Mid);
		if (D > 170.f || FMath::Abs(Feet.Z - (Mid.Z - 100.f)) > 200.f) continue;
		if (D > 60.f && FVector::DotProduct((Mid - Feet).GetSafeNormal2D(), Facing.GetSafeNormal2D()) < 0.42f) continue;
		R.SvcWant = R.SvcWant > 0.5f ? 0.f : 1.f;
		ADP_LOG(TEXT("Jetways: %s service door %s"), *R.Id, R.SvcWant > 0.5f ? TEXT("opening") : TEXT("closing"));
		return true;
	}
	return false;
}

void AJetwayRigs::ApplySway(const FRig& R, const FTransform& T3, float Time, bool bStill) const
{
	for (const FRig::FSway& S : R.Sways)
	{
		// two slow waves, like air moving under the bridge: a few degrees for the loop, less for the one in the pipe
		const float A = bStill ? 0.f : S.Amp * (0.7f * FMath::Sin(Time * 0.85f + S.Ph) + 0.3f * FMath::Sin(Time * 2.1f + S.Ph * 1.7f));
		const FQuat Q(S.Ax, A);
		for (const FPart& P : S.Parts)
		{
			const FTransform Pd(Q * P.Docked.GetRotation(), S.H + Q.RotateVector(P.Docked.GetLocation() - S.H), P.Docked.GetScale3D());
			P.K->UpdateInstanceTransform(P.Ix, Pd.GetRelativeTransform(R.G[Tun3].Docked) * T3, false, false, true);   // (no full rebuild of the list: each moved copy goes on its own - 2026-11-18, the hoses brought the 11-17 lag back)
		}
	}
}

void AJetwayRigs::MapShapes(TArray<FVector4f>& Out) const
{
	// each jetway as it stands now: the rotunda, the three tunnel sections and the cab, each the box its parts fill (its
	// floor's outline), turned and moved with it; plan metres (x east, z north) as the map draws
	for (const FRig& R : Rigs)
	{
		if (R.MapHost)
			for (const FBox& B : R.MapFixed)
			{
				const FTransform W = R.MapHost->GetComponentTransform(); FVector2f C[4]; int32 i = 0;
				for (const FVector2D& K : {FVector2D(B.Min.X, B.Min.Y), FVector2D(B.Max.X, B.Min.Y), FVector2D(B.Max.X, B.Max.Y), FVector2D(B.Min.X, B.Max.Y)})
				{ const FVector P = W.TransformPosition(FVector(K.X, K.Y, B.Min.Z)); C[i++] = FVector2f(P.X / 100.f, -P.Y / 100.f); }
				for (int32 k = 0; k < 4; ++k) Out.Add(FVector4f(C[k].X, C[k].Y, C[(k + 1) % 4].X, C[(k + 1) % 4].Y));
			}
		for (const int32 g : {int32(Boom), int32(Tun1), int32(Tun2), int32(Tun3), int32(Cab)})
		{
			const FBox& B = R.MapBox[g];
			if (!B.IsValid || !R.G[g].Parts.Num() || !R.G[g].Parts[0].K) continue;
			const FTransform W = NowOf(R, g) * R.G[g].Parts[0].K->GetComponentTransform();
			FVector2f C[4]; int32 i = 0;
			for (const FVector2D& K : {FVector2D(B.Min.X, B.Min.Y), FVector2D(B.Max.X, B.Min.Y), FVector2D(B.Max.X, B.Max.Y), FVector2D(B.Min.X, B.Max.Y)})
			{
				const FVector P = W.TransformPosition(FVector(K.X, K.Y, B.Min.Z));
				C[i++] = FVector2f(P.X / 100.f, -P.Y / 100.f);
			}
			for (int32 k = 0; k < 4; ++k) Out.Add(FVector4f(C[k].X, C[k].Y, C[(k + 1) % 4].X, C[(k + 1) % 4].Y));
		}
	}
}
