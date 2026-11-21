#include "VehicleDirt.h"
#include "ADPLog.h"
#include "ADPKeep.h"
#include "BabylonModel.h"
#include "Puddles.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"

namespace
{
	// every dressed vehicle, for the wash hose (the states live as long as their vehicle; dead ones are dropped)
	TArray<TWeakPtr<FVehicleDirtState>>& VdAll() { static TArray<TWeakPtr<FVehicleDirtState>> A; return A; }

	// how much road dirt a kind of surface gathers, how worn it gets, its lacquer (the truck's numbers, FireTruck.cpp)
	struct VdFKind { float Dirt, Wear, CC; };
	VdFKind VdKindOf(const FString& N)
	{
		if (N.Contains(TEXT("Tyre")) || N.Contains(TEXT("Rubber"))) return {0.75f, 0.1f, 0.f};
		if (N.Contains(TEXT("Steel")) || N.Contains(TEXT("TreadPlate")) || N.Contains(TEXT("Rim")) || N.Contains(TEXT("FloorPlate"))) return {0.6f, 0.5f, 0.f};
		if (N.Contains(TEXT("Plastic")) || N.Contains(TEXT("Net")) || N.Contains(TEXT("Webbing"))) return {0.55f, 0.15f, 0.f};
		if (N.Contains(TEXT("Paint"))) return {0.45f, 0.35f, 0.35f};
		return {0.45f, 0.3f, 0.f};   // galvanised, aluminium, canopy, chevrons, the curtains' PVC
	}
	// left as they are: lit lenses and reflectors (their own material), mirrors, screens, the cab's inside (no road dirt
	// gets onto a seat), the GPU's cable (its loops and its flexible run share one look)
	bool VdSkip(const FString& N)
	{
		for (const TCHAR* K : {TEXT("Lens"), TEXT("Reflector"), TEXT("Mirror"), TEXT("Screen"), TEXT("Seat"), TEXT("FloorMat"), TEXT("Cable")})
			if (N.Contains(K)) return true;
		return false;
	}
	// a texture may go into a parameter only if it is of the same kind as the parameter's default (its sampler)
	bool VdFits(UMaterialInterface* Parent, const TCHAR* Param, UTexture* T)
	{
		UTexture* D = nullptr;
		if (!T || !Parent || !Parent->GetTextureParameterDefaultValue(FHashedMaterialParameterInfo(Param), D) || !D) return false;
		return D->SRGB == T->SRGB && D->CompressionSettings == T->CompressionSettings;
	}
}

void FVehicleDirt::Dress(ABabylonModel* Model, UObject* Outer, const FString& Name, float Start)
{
	if (!Model) return;
	S = MakeShared<FVehicleDirtState>(); S->M = Model; S->Name = Name;
	static UMaterialInterface* PaintM = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_VehicleDirt.M_VehicleDirt")));
	static UMaterialInterface* GlassM = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_TruckGlass.M_TruckGlass")));
	static bool bTold = false;
	if (!PaintM && !bTold) { bTold = true; ADP_WARN(TEXT("Vehicle dirt: /Game/Truck/M_VehicleDirt is missing (Tools/set_vehicle_dirt_material.py) - the vehicles keep their clean paint, only the glass gets the wet beads")); }
	auto V = [](UMaterialInterface* M, const TCHAR* N, const FLinearColor& D) { FLinearColor C = D; M->GetVectorParameterValue(FHashedMaterialParameterInfo(N), C); return C; };
	auto Sc = [](UMaterialInterface* M, const TCHAR* N, float D) { float F = D; M->GetScalarParameterValue(FHashedMaterialParameterInfo(N), F); return F; };
	auto Tx = [](UMaterialInterface* M, const TCHAR* N) { UTexture* T = nullptr; M->GetTextureParameterValue(FHashedMaterialParameterInfo(N), T); return T; };
	TArray<UInstancedStaticMeshComponent*> Parts; Model->Parts(Parts);
	TMap<UMaterialInterface*, UMaterialInterface*> Swap;
	int32 NP = 0, NG = 0, NN = 0;
	for (UInstancedStaticMeshComponent* K : Parts)
		for (int32 Si = 0; Si < K->GetNumMaterials(); ++Si)
		{
			UMaterialInterface* M = K->GetMaterial(Si); if (!M) continue;
			if (UMaterialInterface** F = Swap.Find(M)) { if (*F && *F != M) K->SetMaterial(Si, *F); continue; }
			UMaterialInstanceDynamic* Md = Cast<UMaterialInstanceDynamic>(M);
			const FString N = Md && Md->Parent ? Md->Parent->GetName() : M->GetName();
			UMaterialInterface* Put = nullptr;
			if (N == TEXT("M_TruckGlass"))
			{
				// (already the fire truck's glass - the tug's windows, the GPU's panel window): its beads with the rest
				UMaterialInstanceDynamic* G = Md ? Md : UMaterialInstanceDynamic::Create(M, Outer);
				if (!Md) G->SetVectorParameterValue(TEXT("BC"), FLinearColor(0.015f, 0.022f, 0.026f, 0.38f));
				S->Glass.Add(G); Put = G; ++NG;
			}
			else if (N.Contains(TEXT("Glass")) && !N.Contains(TEXT("Mirror")) && GlassM)
			{
				UMaterialInstanceDynamic* G = UMaterialInstanceDynamic::Create(GlassM, Outer);
				G->SetVectorParameterValue(TEXT("BC"), FLinearColor(0.015f, 0.022f, 0.026f, 0.38f));
				S->Glass.Add(G); Put = G; ++NG;
			}
			else if (!VdSkip(N) && !N.StartsWith(TEXT("M_Truck")) && PaintM)
			{
				const VdFKind Kd = VdKindOf(N);
				UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(PaintM, Outer);
				D->SetVectorParameterValue(TEXT("BC"), V(M, TEXT("BaseColorFactor"), FLinearColor::White));
				D->SetVectorParameterValue(TEXT("OS"), V(M, TEXT("BaseColorTexture_OffsetScale"), FLinearColor(0, 0, 1, 1)));
				if (UTexture* T = Tx(M, TEXT("BaseColorTexture"))) D->SetTextureParameterValue(TEXT("Base"), T);
				// (its own relief and its metal / roughness map, as imported - never flatter than it was)
				if (UTexture* T = Tx(M, TEXT("NormalTexture")); VdFits(PaintM, TEXT("Nrm"), T))
				{
					D->SetTextureParameterValue(TEXT("Nrm"), T); D->SetScalarParameterValue(TEXT("UseN"), 1.f); ++NN;
					D->SetVectorParameterValue(TEXT("NOS"), V(M, TEXT("NormalTexture_OffsetScale"), FLinearColor(0, 0, 1, 1)));
					D->SetScalarParameterValue(TEXT("NS"), Sc(M, TEXT("NormalScale"), 1.f));
				}
				if (UTexture* T = Tx(M, TEXT("MetallicRoughnessTexture")); VdFits(PaintM, TEXT("ORM"), T))
				{
					D->SetTextureParameterValue(TEXT("ORM"), T); D->SetScalarParameterValue(TEXT("UseORM"), 1.f);
					D->SetVectorParameterValue(TEXT("MOS"), V(M, TEXT("MetallicRoughnessTexture_OffsetScale"), FLinearColor(0, 0, 1, 1)));
				}
				D->SetScalarParameterValue(TEXT("R"), Sc(M, TEXT("RoughnessFactor"), 0.5f));
				D->SetScalarParameterValue(TEXT("M"), Sc(M, TEXT("MetallicFactor"), 0.f));
				D->SetScalarParameterValue(TEXT("CC"), Kd.CC); D->SetScalarParameterValue(TEXT("CCR"), 0.12f);
				D->SetScalarParameterValue(TEXT("Dirt"), Kd.Dirt); D->SetScalarParameterValue(TEXT("Wear"), Kd.Wear);
				S->Paint.Add(D); Put = D; ++NP;
			}
			Swap.Add(M, Put ? Put : M);
			if (Put && Put != M) K->SetMaterial(Si, Put);
		}
	// the vehicle's box in its own frame (washing; the areas' middle)
	const FBox LB = Model->CalculateComponentsBoundingBoxInLocalSpace(true);
	if (LB.IsValid) { S->Centre = LB.GetCenter(); S->HalfExt = LB.GetExtent(); }
	SetDirt(Start >= 0.f ? Start : FMath::FRandRange(0.2f, 0.5f));
	VdAll().Add(S);
	ADP_LOG(TEXT("Vehicle dirt: %s dressed - %d painted / metal / rubber surfaces (%d with their relief), %d glass; %.1f x %.1f m; dirt %.2f"),
		*Name, NP, NN, NG, S->HalfExt.X / 50.f, S->HalfExt.Y / 50.f, S->Goal[0]);
}

void FVehicleDirt::SetDirt(float V)
{
	if (!S) return;
	// (a little uneven: the low rear and the wheel areas dirtier)
	for (int32 i = 0; i < 12; ++i) S->Zone[i] = S->Goal[i] = FMath::Clamp(V * FMath::FRandRange(0.85f, 1.15f), 0.f, 2.f);
	S->Sent = -1.f;
}

void FVehicleDirt::Tick(float Dt, float SpeedMs)
{
	if (!IsValid() || Dt <= 0.f) return;
	FVehicleDirtState& D = *S; ABabylonModel* M = D.M.Get();
	// driven: dirt builds with the distance - clean dry paving very slowly (about fully dirty after 30 km of apron work),
	// in the wet much faster (spray), off the paving fastest (FireTruck.cpp: ~4 km); and a little with time out on the
	// ramp (rain, dust, de-icing fluid: about a month to be properly dirty standing)
	const float Wet = APuddles::Get() ? APuddles::Get()->GetWetness() : 0.f;
	const float Ms = FMath::Max(0.f, SpeedMs);
	D.Odometer += Ms * Dt;
	const float PerM = 0.000033f * (1.f + 3.f * Wet) + (D.bOffRoad ? 0.00025f : 0.f);
	const float PerS = 1.f / (30.f * 24.f * 3600.f);
	const float Most = D.bOffRoad ? 2.f : 1.f;
	float Changed = 0.f;
	for (float& Z : D.Goal) if (Z < Most) { const float Was = Z; Z = FMath::Min(Most, Z + Ms * Dt * PerM + Dt * PerS); Changed += Z - Was; }
	// the paint follows the jet over a few seconds (you see the dirt leave), builds back slowly
	for (int32 i = 0; i < 12; ++i) { const float Was = D.Zone[i]; D.Zone[i] = FMath::FInterpConstantTo(D.Zone[i], D.Goal[i], Dt, D.Zone[i] > D.Goal[i] ? 0.3f : 0.05f); Changed += FMath::Abs(D.Zone[i] - Was); }
	// the beads: drying (~20 min), blown off once it drives - faster the faster
	for (float& B : D.Beads) if (B > 0.f) { const float Was = B; B = FMath::Max(0.f, B - Dt / 1200.f - Dt * FMath::Max(0.f, Ms - 1.5f) / 45.f); Changed += Was - B; }
	D.Blow += Ms * Dt * 0.6f;
	// the materials: only when something changed (moved, washed, dried) - most vehicles stand still most of the time
	const FTransform Tr = M->GetActorTransform();
	const FVector O = Tr.TransformPosition(D.Centre), F = Tr.TransformVectorNoScale(FVector::ForwardVector);
	const bool bMoved = FVector::DistSquared(O, D.LastO) > 4.f || !F.Equals(D.LastF, 0.002f);
	D.Sent += Changed;
	if (!bMoved && D.Sent >= 0.f && D.Sent < 0.002f) return;
	D.Sent = 0.f; D.LastO = O; D.LastF = F;
	const float Gz = Tr.GetLocation().Z;
	const FLinearColor B0(D.Beads[0], D.Beads[1], D.Beads[2], D.Beads[3]), B1(D.Beads[4], D.Beads[5], D.Beads[6], D.Beads[7]), B2(D.Beads[8], D.Beads[9], D.Beads[10], D.Beads[11]);
	for (const TWeakObjectPtr<UMaterialInstanceDynamic>& W : D.Glass)
		if (UMaterialInstanceDynamic* G = W.Get())
		{
			G->SetVectorParameterValue(TEXT("Origin"), FLinearColor(O.X, O.Y, O.Z, 0.f)); G->SetVectorParameterValue(TEXT("AxisF"), FLinearColor(F.X, F.Y, F.Z, 0.f));
			G->SetVectorParameterValue(TEXT("B0"), B0); G->SetVectorParameterValue(TEXT("B1"), B1); G->SetVectorParameterValue(TEXT("B2"), B2); G->SetScalarParameterValue(TEXT("Blow"), D.Blow);
		}
	for (const TWeakObjectPtr<UMaterialInstanceDynamic>& W : D.Paint)
		if (UMaterialInstanceDynamic* P = W.Get())
		{
			P->SetVectorParameterValue(TEXT("Origin"), FLinearColor(O.X, O.Y, O.Z, 0.f)); P->SetVectorParameterValue(TEXT("AxisF"), FLinearColor(F.X, F.Y, F.Z, 0.f));
			P->SetScalarParameterValue(TEXT("GroundZ"), Gz);
			P->SetVectorParameterValue(TEXT("B0"), B0); P->SetVectorParameterValue(TEXT("B1"), B1); P->SetVectorParameterValue(TEXT("B2"), B2); P->SetScalarParameterValue(TEXT("Blow"), D.Blow);
			P->SetVectorParameterValue(TEXT("Z0"), FLinearColor(D.Zone[0], D.Zone[1], D.Zone[2], D.Zone[3]));
			P->SetVectorParameterValue(TEXT("Z1"), FLinearColor(D.Zone[4], D.Zone[5], D.Zone[6], D.Zone[7]));
			P->SetVectorParameterValue(TEXT("Z2"), FLinearColor(D.Zone[8], D.Zone[9], D.Zone[10], D.Zone[11]));
		}
}

void FVehicleDirt::Wash(const FVector& At, float Amount)
{
	if (!IsValid()) return;
	// the same areas as the paint's (FireTruck.cpp Wash): 2 m lengths, the two sides blended over 40 cm
	FVehicleDirtState& D = *S; const FTransform Tr = D.M->GetActorTransform();
	const FVector Lp = At - Tr.TransformPosition(D.Centre), F = Tr.TransformVectorNoScale(FVector::ForwardVector);
	const FVector Ar = FVector::CrossProduct(FVector::UpVector, F).GetSafeNormal();
	const float Fa = FMath::Clamp(float(FVector::DotProduct(Lp, F)) / 100.f + 6.f, 0.f, 11.999f) / 2.f;
	const float Sd = FMath::Clamp(float(FVector::DotProduct(Lp, Ar)) / 40.f + 0.5f, 0.f, 1.f);
	const int32 I0 = FMath::Min(int32(Fa), 5), I1 = FMath::Min(I0 + 1, 5);
	const float Ft = FMath::Clamp(Fa - I0, 0.f, 1.f);
	auto Take = [&](int32 I, float Wt) { D.Goal[I] = FMath::Max(0.f, D.Goal[I] - Amount * Wt * 2.f); };
	Take(I0, (1.f - Ft) * (1.f - Sd)); Take(I1, Ft * (1.f - Sd)); Take(6 + I0, (1.f - Ft) * Sd); Take(6 + I1, Ft * Sd);
	auto Wet = [&](int32 I, float Wt) { D.Beads[I] = FMath::Min(1.f, D.Beads[I] + Amount * 40.f * Wt); };
	Wet(I0, 1.f - Sd); Wet(I1, 1.f - Sd); Wet(6 + I0, Sd); Wet(6 + I1, Sd);
	if (I0 > 0) Wet(I0 - 1, 0.25f * (1.f - Sd));
	if (I1 < 5) Wet(I1 + 1, 0.25f * (1.f - Sd));
	D.Sent = -1.f;
}

bool FVehicleDirt::WashSegment(const FVector& A, const FVector& B, float Amount, FVector& HitP, FVector& HitN)
{
	TArray<TWeakPtr<FVehicleDirtState>>& L = VdAll();
	float Best = 2.f; TSharedPtr<FVehicleDirtState> Hit;
	for (int32 i = L.Num() - 1; i >= 0; --i)
	{
		TSharedPtr<FVehicleDirtState> D = L[i].Pin();
		if (!D || !D->M.IsValid()) { L.RemoveAtSwap(i); continue; }
		// the drop's way in the vehicle's own frame against its box (a slab test)
		const FTransform Tr = D->M->GetActorTransform();
		const FVector La = Tr.InverseTransformPosition(A) - D->Centre, Lb = Tr.InverseTransformPosition(B) - D->Centre, Dv = Lb - La;
		float T0 = 0.f, T1 = 1.f; int32 Axis = -1; float Sign = 0.f; bool bOk = true;
		for (int32 k = 0; k < 3 && bOk; ++k)
		{
			const float H = D->HalfExt[k], O = La[k], Dd = Dv[k];
			if (FMath::Abs(Dd) < 1e-5f) { if (FMath::Abs(O) > H) bOk = false; continue; }
			float a = (-H - O) / Dd, b = (H - O) / Dd; float Sa = -1.f; if (a > b) { Swap(a, b); Sa = 1.f; }
			if (a > T0) { T0 = a; Axis = k; Sign = Sa; }
			T1 = FMath::Min(T1, b); if (T0 > T1) bOk = false;
		}
		if (!bOk || T0 >= Best || Axis < 0) continue;   // (starting inside a box: no hit - the jet starts outside)
		Best = T0; Hit = D;
		FVector N = FVector::ZeroVector; N[Axis] = Sign;
		HitP = FMath::Lerp(A, B, T0); HitN = Tr.TransformVectorNoScale(N);
	}
	if (!Hit) return false;
	FVehicleDirt V; V.S = Hit; V.Wash(HitP, Amount);
	static double Said = 0.0; const double Now = FPlatformTime::Seconds();
	if (Now - Said > 2.0) { Said = Now; ADP_LOG(TEXT("Vehicle dirt: the hose washes the %s"), *Hit->Name); }
	return true;
}
