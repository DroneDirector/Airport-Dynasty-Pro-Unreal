// The road network (RoadNetwork.h).
#include "RoadNetwork.h"
#include "ADPLog.h"
#include "Airfield.h"
#include "TerminalBuilder.h"
#include "MetaRoadActor.h"
#include "MetaRoadTypes.h"
#include "Components/RoadSplineComponent.h"
#include "MetaRoadMeshBuild/MetaRoadRuntimeBuildHost.h"
#include "MetaRoadMeshBuild/MetaRoadBuildSettings.h"
#include "MetaRoadMeshBuild/MetaRoadBuildSettingsBase.h"
#include "UObject/UnrealType.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	// the surfaces: the airfield's own ground materials (owner, 2026-11-14) - the same asphalt and concrete as the
	// aprons, with their puddles, wetness and snow (Tools/set_ground.py), lit by Lumen like the rest; MetaRoad's own for
	// the bus lane's colour and hatched areas
	UMaterialInterface* SurfaceFor(UObject* Outer, const FString& Type)
	{
		static TMap<FString, TWeakObjectPtr<UMaterialInterface>> Cache;
		if (const TWeakObjectPtr<UMaterialInterface>* F = Cache.Find(Type)) if (F->IsValid()) return F->Get();
		UMaterialInterface* M = nullptr;
		if (Type == TEXT("Bus")) M = LoadObject<UMaterialInterface>(nullptr, TEXT("/MetaRoad/Materials/MI_Bus.MI_Bus"));
		else if (Type == TEXT("Median") || Type == TEXT("Restricted")) M = LoadObject<UMaterialInterface>(nullptr, TEXT("/MetaRoad/Materials/MI_Restricted.MI_Restricted"));
		else M = ATerminalBuilder::KitMaterial(Outer, Type == TEXT("Sidewalk") ? FName(TEXT("gwalk")) : FName(TEXT("groad")));
		Cache.Add(Type, M);
		return M;
	}

	// centripetal Catmull-Rom through the plan points, sampled about every Step metres (closed: wraps round)
	TArray<FVector2D> SmoothPath(const TArray<FVector2D>& P, double Step, bool bClosed)
	{
		const int32 N = P.Num();
		auto At = [&](int32 i) -> FVector2D
		{
			if (bClosed) return P[(i % N + N) % N];
			if (i < 0) return P[0] * 2.0 - P[1];                 // (mirrored past the ends: the end runs straight out)
			if (i >= N) return P[N - 1] * 2.0 - P[N - 2];
			return P[i];
		};
		TArray<FVector2D> Out;
		const int32 Segs = bClosed ? N : N - 1;
		for (int32 i = 0; i < Segs; ++i)
		{
			const FVector2D P0 = At(i - 1), P1 = At(i), P2 = At(i + 1), P3 = At(i + 2);
			auto Knot = [](double T, const FVector2D& A, const FVector2D& B) { return T + FMath::Max(FMath::Sqrt((B - A).Size()), 1e-4); };
			const double T0 = 0, T1 = Knot(T0, P0, P1), T2 = Knot(T1, P1, P2), T3 = Knot(T2, P2, P3);
			const int32 K = FMath::Max(2, FMath::CeilToInt((P2 - P1).Size() / Step));
			for (int32 k = 0; k < K; ++k)
			{
				const double T = FMath::Lerp(T1, T2, double(k) / K);
				const FVector2D A1 = P0 * ((T1 - T) / (T1 - T0)) + P1 * ((T - T0) / (T1 - T0));
				const FVector2D A2 = P1 * ((T2 - T) / (T2 - T1)) + P2 * ((T - T1) / (T2 - T1));
				const FVector2D A3 = P2 * ((T3 - T) / (T3 - T2)) + P3 * ((T - T2) / (T3 - T2));
				const FVector2D B1 = A1 * ((T2 - T) / (T2 - T0)) + A2 * ((T - T0) / (T2 - T0));
				const FVector2D B2 = A2 * ((T3 - T) / (T3 - T1)) + A3 * ((T - T1) / (T3 - T1));
				Out.Add(B1 * ((T2 - T) / (T2 - T1)) + B2 * ((T - T1) / (T2 - T1)));
			}
		}
		if (!bClosed) Out.Add(P.Last());
		return Out;
	}

	// the road's height: our ground, averaged over about 12 m along the road (a road is graded, it doesn't follow every
	// bump), then never below the ground anywhere across it - Lift metres over the highest ground under its middle
	TArray<double> SmoothHeights(const TArray<double>& Z, const TArray<FVector2D>& D, double Lift)
	{
		TArray<double> Out; Out.SetNum(Z.Num());
		for (int32 k = 0; k < Z.Num(); ++k)
		{
			double Sum = 0, Wt = 0;
			for (int32 j = 0; j < Z.Num(); ++j)
			{
				const double Dist = (D[j] - D[k]).Size();
				if (Dist > 12.0) continue;
				const double W = 1.0 - Dist / 12.0; Sum += Z[j] * W; Wt += W;
			}
			Out[k] = FMath::Max(Sum / FMath::Max(Wt, 1e-6), Z[k]) + Lift;
		}
		return Out;
	}

	// super smooth (owner, 2026-11-14: "you can zoom very far and see each and every section" - like his plans): the
	// mesh's outlines kept within 1 cm of the true curve (MetaRoad's default 5 cm), a point at least every metre along
	// the road (default 3.75 m), the lane lines within 2 cm. Set by name: the property sets' classes are private to
	// the plugin.
	void MakeFine(AMetaRoad* Road)
	{
		UMetaRoadBuildSettings* S = UMetaRoadBuildSettings::GetForActor(Road, true);
		if (!S) return;
		S->EnsureDefaultPropertySets();
		static const TPair<const TCHAR*, double> Want[] = {{TEXT("ErrorTolerance"), 1.0}, {TEXT("MaxSegmentLength"), 100.0}, {TEXT("MaxSquareDistanceFromSpline"), 4.0}};
		int32 Set = 0;
		for (const TObjectPtr<UMetaRoadBuildSettingsBase>& Ptr : S->PropertySets)
		{
			UObject* PS = Ptr.Get();
			if (!PS) continue;
			for (const TPair<const TCHAR*, double>& W : Want)
				if (FDoubleProperty* Pr = FindFProperty<FDoubleProperty>(PS->GetClass(), W.Key)) { Pr->SetPropertyValue_InContainer(PS, W.Value); ++Set; }
		}
		// the pavements: MetaRoad builds them from the road-wide zone materials (Triangulation > Override Materials), not
		// the lane's own - so MI_gwalk goes in there for the Sidewalk zone
		if (UMaterialInterface* Walk = ATerminalBuilder::KitMaterial(Road, FName(TEXT("gwalk"))))
			for (const TObjectPtr<UMetaRoadBuildSettingsBase>& Ptr : S->PropertySets)
				if (UObject* PS = Ptr.Get()) if (PS->GetClass()->GetName() == TEXT("TriangulateRoadToolProperties"))
					if (FMapProperty* MP = FindFProperty<FMapProperty>(PS->GetClass(), TEXT("OverrideMaterials")))
					{
						FScriptMapHelper H(MP, MP->ContainerPtrToValuePtr<void>(PS));
						const FRoadZoneType Key = ERoadZoneTypes::Sidewalk;
						const int32 i = H.AddDefaultValue_Invalid_NeedsRehash();
						MP->KeyProp->CopyCompleteValue(H.GetKeyPtr(i), &Key);
						H.Rehash();
						if (FStructProperty* VS = CastField<FStructProperty>(MP->ValueProp))
							if (FObjectProperty* MM = FindFProperty<FObjectProperty>(VS->Struct, TEXT("MeshMaterial"))) { MM->SetObjectPropertyValue_InContainer(H.GetValuePtr(i), Walk); ++Set; }
					}
		// the kerbs: our own kerb stone (Tools/set_ground.py MI_gkerb), on the curb property set's override
		if (UMaterialInterface* Kerb = ATerminalBuilder::KitMaterial(Road, FName(TEXT("gkerb"))))
			for (const TObjectPtr<UMetaRoadBuildSettingsBase>& Ptr : S->PropertySets)
				if (UObject* PS = Ptr.Get()) if (PS->GetClass()->GetName() == TEXT("RoadCurbToolProperties"))
					if (FObjectProperty* Pr = FindFProperty<FObjectProperty>(PS->GetClass(), TEXT("OverrideMaterial"))) { Pr->SetObjectPropertyValue_InContainer(PS, Kerb); ++Set; }
		static bool bSaid = false;
		if (!bSaid) { bSaid = true; ADP_LOG(TEXT("Roads: fine curves (%d settings on %d property sets)"), Set, S->PropertySets.Num()); }
	}

	FRoadZoneType ZoneTypeFor(const FString& T)
	{
		if (T == TEXT("Sidewalk")) return ERoadZoneTypes::Sidewalk;
		if (T == TEXT("Parking")) return ERoadZoneTypes::Parking;
		if (T == TEXT("Shoulder")) return ERoadZoneTypes::Shoulder;
		if (T == TEXT("Median")) return ERoadZoneTypes::Median;
		if (T == TEXT("Restricted")) return ERoadZoneTypes::Restricted;
		if (T == TEXT("Biking")) return ERoadZoneTypes::Biking;
		if (T == TEXT("Bus")) return FRoadZoneType(TEXT("Bus"));
		return ERoadZoneTypes::Driving;
	}

	FRoadLane MakeLane(UObject* Outer, const TSharedPtr<FJsonObject>& J)
	{
		FRoadLane L;
		const FString Type = J->GetStringField(TEXT("type"));
		// its width: one number (m), or [[metres along the road, m], ...] where the plan widens or narrows - eased in
		// smoothly between the keys
		double W = 350.0;
		TArray<TPair<double, double>> WKeys;
		const TArray<TSharedPtr<FJsonValue>>* WA = nullptr;
		if (J->TryGetArrayField(TEXT("w"), WA)) { for (const TSharedPtr<FJsonValue>& K : *WA) { const auto& KA = K->AsArray(); WKeys.Add({KA[0]->AsNumber() * 100.0, KA[1]->AsNumber() * 100.0}); } }
		else W = J->GetNumberField(TEXT("w")) * 100.0;
		if (Type == TEXT("Sidewalk"))
		{
			L.RoadZone.InitializeAs<FRoadZoneSidewalk>();
			double H = 15.0; J->TryGetNumberField(TEXT("kerb"), H);
			L.RoadZone.GetMutable<FRoadZoneSidewalk>().DefaultHeight = H;
		}
		else L.RoadZone.InitializeAs<FRoadZoneDriving>();
		FRoadZone& Z = L.RoadZone.GetMutable<FRoadZone>();
		Z.ZoneType = ZoneTypeFor(Type);
		if (UMaterialInterface* M = SurfaceFor(Outer, Type)) { Z.bOverrideMaterial = true; Z.OverrideMaterial = M; }
		if (WKeys.Num() == 0) L.Width.AddKey(0.f, float(W));
		for (const TPair<double, double>& K : WKeys) { const FKeyHandle H = L.Width.AddKey(float(K.Key), float(K.Value)); L.Width.SetKeyInterpMode(H, RCIM_Cubic); L.Width.SetKeyTangentMode(H, RCTM_Auto); }
		if (WKeys.Num()) L.Width.AutoSetTangents();
		bool bInvert = false;
		if (J->TryGetBoolField(TEXT("invert"), bInvert) && bInvert) L.Direction = ERoadLaneDirection::Invert;
		return L;
	}
}

ARoadNetwork::ARoadNetwork()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

int32 ARoadNetwork::Build()
{
	UWorld* W = GetWorld();
	if (!W) return 0;
	FString Text;
	const FString File = FPaths::ProjectContentDir() / TEXT("Data/roads.json");
	if (!FFileHelper::LoadFileToString(Text, *File) && !FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data/roads.json"))))
	{
		ADP_LOG(TEXT("Roads: no Data/roads.json yet"));
		return 0;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) { ADP_ERROR(TEXT("Roads: Data/roads.json can't be read")); return 0; }
	const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("RoadTest"));
	Host = NewObject<UMetaRoadRuntimeBuildHost>(this);
	Host->MeshOutput = ERoadRuntimeMeshOutput::TransientStaticMesh;   // (built once, never changing: the cheaper draw path)
	int32 Built = 0;
	const double T0 = FPlatformTime::Seconds();
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("roads")))
	{
		const TSharedPtr<FJsonObject> R = V->AsObject();
		bool bIsTest = false; R->TryGetBoolField(TEXT("test"), bIsTest);
		if (bIsTest && !bTest) continue;
		const FString Id = R->GetStringField(TEXT("id"));
		// its path: plan metres, laid on our ground every few metres (the plugin's spline follows these points)
		TArray<FVector2D> P;
		TSet<int32> Sharp;   // ([x, y, 1]: a sharp turn on the plan - the only places the road has a corner)
		for (const TSharedPtr<FJsonValue>& Pt : R->GetArrayField(TEXT("pts"))) { const auto& A = Pt->AsArray(); if (A.Num() > 2 && A[2]->AsNumber() > 0) Sharp.Add(P.Num()); P.Add(FVector2D(A[0]->AsNumber(), A[1]->AsNumber())); }
		if (P.Num() < 2) continue;
		double Lift = 0.04; R->TryGetNumberField(TEXT("lift"), Lift);
		double Step = 2.0; R->TryGetNumberField(TEXT("step"), Step);
		// one smooth curve through the plan's points (owner, 2026-11-14: smooth curves like the plans, never a corner at
		// a point): centripetal Catmull-Rom - it passes through every point, never overshoots or loops - sampled finely
		bool bClosedPath = false; R->TryGetBoolField(TEXT("closed"), bClosedPath);
		TArray<FVector2D> Dense;
		if (Sharp.Num() == 0) Dense = SmoothPath(P, Step, bClosedPath);
		else
		{
			// smooth between the sharp points, each piece on its own (its ends run straight into the corner)
			int32 From = 0;
			for (int32 i = 1; i < P.Num(); ++i) if (Sharp.Contains(i) || i == P.Num() - 1)
			{
				TArray<FVector2D> Piece(&P[From], i - From + 1);
				TArray<FVector2D> D = SmoothPath(Piece, Step, false);
				if (Dense.Num()) D.RemoveAt(0);
				Dense.Append(D);
				From = i;
			}
		}
		const FVector Origin(Dense[0].X * 100.0, -Dense[0].Y * 100.0, 0.0);
		AMetaRoad* Road = UMetaRoadRuntimeBuildHost::SpawnRuntimeRoad(W, FTransform(Origin));
		if (!Road) { ADP_ERROR(TEXT("Roads: %s could not be made"), *Id); continue; }
		URoadSplineComponent* S = NewObject<URoadSplineComponent>(Road);
		S->OnComponentCreated();
		Road->AddInstanceComponent(S);
		S->AttachToComponent(Road->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		S->RegisterComponent();
		TArray<double> Z; for (const FVector2D& D : Dense) Z.Add(AAirfield::GroundAt(D.X, D.Y));
		Z = SmoothHeights(Z, Dense, Lift);
		TArray<FVector> Pts;
		for (int32 k = 0; k < Dense.Num(); ++k) Pts.Add(FVector(Dense[k].X * 100.0, -Dense[k].Y * 100.0, Z[k] * 100.0) - Origin);
		S->SetClosedLoop(bClosedPath, false);
		S->SetSplinePoints(Pts, ESplineCoordinateSpace::Local, true);
		// its lanes, left and right of the path (inside out); right-hand traffic
		FRoadLaneSection& Sec = S->RoadLayout.Sections.AddDefaulted_GetRef();
		const TSharedPtr<FJsonObject> Lanes = R->GetObjectField(TEXT("lanes"));
		for (const TSharedPtr<FJsonValue>& L : Lanes->GetArrayField(TEXT("left"))) Sec.Left.Add(MakeLane(this, L->AsObject()));
		for (const TSharedPtr<FJsonValue>& L : Lanes->GetArrayField(TEXT("right"))) Sec.Right.Add(MakeLane(this, L->AsObject()));
		S->UpdateRoadLayout();
		MakeFine(Road);
		if (Host->BuildRoad(Road)) { ++Built; Roads.Add(Road); }
		else ADP_ERROR(TEXT("Roads: %s did not build"), *Id);
	}
	ADP_LOG(TEXT("Roads: %d built in %.2f s (MetaRoad Free, at run time)"), Built, FPlatformTime::Seconds() - T0);
	return Built;
}
