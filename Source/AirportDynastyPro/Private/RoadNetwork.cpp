// The road network (RoadNetwork.h).
#include "RoadNetwork.h"
#include "ADPLog.h"
#include "Airfield.h"
#include "TerminalBuilder.h"
#include "StreetLights.h"
#include "RoadSigns.h"
#include "ProceduralMeshComponent.h"
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
	struct FPaintBuf { TArray<FVector> V; TArray<int32> I; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> C; };
	// the surfaces: the airfield's own ground materials (owner, 2026-11-14) - the same asphalt and concrete as the
	// aprons, with their puddles, wetness and snow (Tools/set_ground.py), lit by Lumen like the rest; MetaRoad's own for
	// the bus lane's colour and hatched areas
	UMaterialInterface* SurfaceFor(UObject* Outer, const FString& Type)
	{
		static TMap<FString, TWeakObjectPtr<UMaterialInterface>> Cache;
		if (const TWeakObjectPtr<UMaterialInterface>* F = Cache.Find(Type)) if (F->IsValid()) return F->Get();
		UMaterialInterface* M = nullptr;
		if (Type == TEXT("Island")) M = ATerminalBuilder::KitMaterial(Outer, FName(TEXT("ggrass")));
		else if (Type == TEXT("Apron")) M = ATerminalBuilder::KitMaterial(Outer, FName(TEXT("gconcrete")));
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
		if (T == TEXT("Bus")) return ERoadZoneTypes::Driving;
	if (T == TEXT("Island") || T == TEXT("Apron")) return ERoadZoneTypes::Median;
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
		if (Type == TEXT("Sidewalk") || Type == TEXT("Island") || Type == TEXT("Apron"))
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
	int32 Crossings = 0;
	TMap<FName, FPaintBuf> Paint;   // every road's markings, the car parks, the roundabouts' middles: by material
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
		if (!Host->BuildRoad(Road)) { ADP_ERROR(TEXT("Roads: %s did not build"), *Id); continue; }
		++Built; Roads.Add(Road);
		// ---- along the road: where each point is (m), the way it runs, its right (Unreal: x forward, y right) ----
		TArray<double> Al; Al.Add(0.0);
		for (int32 k = 1; k < Pts.Num(); ++k) Al.Add(Al.Last() + FVector::Dist2D(Pts[k], Pts[k - 1]) / 100.0);
		const double RoadLen = Al.Last();
		auto OnRoad = [&](double D, FVector& OP, FVector& Fw, FVector& Rt)
		{
			D = FMath::Clamp(D, 0.0, RoadLen);
			int32 Lo = 0, Hi = Al.Num() - 1;
			while (Hi - Lo > 1) { const int32 Md = (Lo + Hi) / 2; if (Al[Md] <= D) Lo = Md; else Hi = Md; }
			const int32 Sg = FMath::Min(Lo, Al.Num() - 2);
			const double F = (D - Al[Sg]) / FMath::Max(Al[Sg + 1] - Al[Sg], 1e-6);
			OP = Origin + FMath::Lerp(Pts[Sg], Pts[Sg + 1], F);
			Fw = (Pts[Sg + 1] - Pts[Sg]).GetSafeNormal2D(); Rt = FVector(-Fw.Y, Fw.X, 0.0);
		};
		// a lane edge: how far out from the path edge K is on a side (0 = the path, 1 = the first lane's outer edge...), at
		// D m along - from MetaRoad's own width curves, so everything laid by it follows the lanes' gradual widening
		auto Edge = [&](bool bRight, int32 K, double D)
		{
			const TArray<FRoadLane>& Ls = bRight ? Sec.Right : Sec.Left;
			double O = 0.0;
			for (int32 i = 0; i < FMath::Min(K, Ls.Num()); ++i) O += Ls[i].Width.Eval(float(D * 100.0)) / 100.0;
			return O;
		};
		auto SideOf = [](const TSharedPtr<FJsonObject>& O) { FString Sd = TEXT("right"); O->TryGetStringField(TEXT("side"), Sd); return Sd != TEXT("left"); };
		// a signed offset (m, + to the right) at D: {side, edge, off (further out)}
		auto Off = [&](const TSharedPtr<FJsonObject>& O, double D)
		{
			const bool bR = SideOf(O); double K = 0, Ex = 0; O->TryGetNumberField(TEXT("edge"), K); O->TryGetNumberField(TEXT("off"), Ex);
			return (bR ? 1.0 : -1.0) * (Edge(bR, int32(K), D) + Ex);
		};
		// the built surface under a point (the road's mesh, its pavements' tops, or the ground off it)
		auto Surface = [&](FVector Q)
		{
			FHitResult Hit; FCollisionQueryParams QP(SCENE_QUERY_STAT(ADPRoadPaint), true);
			if (W->LineTraceSingleByChannel(Hit, Q + FVector(0, 0, 200), Q - FVector(0, 0, 200), ECC_Visibility, QP)) Q.Z = Hit.ImpactPoint.Z;
			return Q;
		};
		auto Grime = [](double D) { return 0.3f + 0.5f * float(FMath::Frac(FMath::Sin(D * 12.9898) * 43758.5453)); };
		// paint: a strip along the road from D0 to D1 between two offsets (functions of D), on the surface, 6 mm up
		auto Strip = [&](FName Mat, double D0, double D1, TFunctionRef<double(double)> OA, TFunctionRef<double(double)> OB, double Step, float Gr)
		{
			if (D1 <= D0) return;
			FPaintBuf& B = Paint.FindOrAdd(Mat);
			const int32 N = FMath::Max(1, FMath::CeilToInt((D1 - D0) / Step));
			const int32 Base = B.V.Num();
			for (int32 s = 0; s <= N; ++s)
			{
				const double D = FMath::Lerp(D0, D1, double(s) / N);
				FVector P_, F_, R_; OnRoad(D, P_, F_, R_);
				for (const double O : {OA(D), OB(D)})
				{
					B.V.Add(Surface(P_ + R_ * (O * 100.0)) + FVector(0, 0, 0.6) - GetActorLocation());
					B.N.Add(FVector::UpVector); B.UV.Add(FVector2D(O, D)); B.C.Add(FLinearColor(1.f, Gr, 1.f, 1.f));
				}
			}
			for (int32 s = 0; s < N; ++s) { const int32 a = Base + s * 2; B.I.Append({a, a + 2, a + 1, a + 1, a + 2, a + 3}); }
		};
		auto ColourOf = [](const TSharedPtr<FJsonObject>& O) { FString Cl = TEXT("white"); O->TryGetStringField(TEXT("color"), Cl); return FName(*(FString(TEXT("paint")) + Cl)); };
		// ---- its markings ("paint": lines, dashes, the boxes of the bus and taxi stands, give-way teeth) ----
		const TArray<TSharedPtr<FJsonValue>>* PJ = nullptr;
		if (R->TryGetArrayField(TEXT("paint"), PJ))
			for (const TSharedPtr<FJsonValue>& PV : *PJ)
			{
				const TSharedPtr<FJsonObject> O = PV->AsObject();
				FString Kind = TEXT("line"); O->TryGetStringField(TEXT("kind"), Kind);
				double From = 0.0, To = RoadLen, Wl = 0.15; O->TryGetNumberField(TEXT("from"), From); O->TryGetNumberField(TEXT("to"), To); O->TryGetNumberField(TEXT("w"), Wl);
				To = FMath::Min(To, RoadLen); From = FMath::Max(From, 0.0);
				const FName Mat = ColourOf(O);
				if (Kind == TEXT("line") || Kind == TEXT("dash"))
				{
					double Dash = 1e9, Gap = 0.0; O->TryGetNumberField(TEXT("dash"), Dash); O->TryGetNumberField(TEXT("gap"), Gap);
					for (double D = From; D < To; D += Dash + Gap)
						Strip(Mat, D, FMath::Min(D + Dash, To), [&](double d) { return Off(O, d) - Wl / 2; }, [&](double d) { return Off(O, d) + Wl / 2; }, 1.0, Grime(D));
				}
				else if (Kind == TEXT("boxes"))
				{
					// a row of boxes along a lane (a bus or a taxi each): its two long sides, its two ends
					const bool bR = SideOf(O); double K = 0, Nb = 1, Len = 6, Gp = 1, In = 0.25; O->TryGetNumberField(TEXT("lane"), K); O->TryGetNumberField(TEXT("n"), Nb);
					O->TryGetNumberField(TEXT("len"), Len); O->TryGetNumberField(TEXT("gap"), Gp); O->TryGetNumberField(TEXT("inset"), In);
					const double Sg = bR ? 1.0 : -1.0;
					auto E0 = [&](double d) { return Sg * (Edge(bR, int32(K), d) + In); };
					auto E1 = [&](double d) { return Sg * (Edge(bR, int32(K) + 1, d) - In); };
					for (int32 b = 0; b < int32(Nb); ++b)
					{
						const double S0 = From + b * (Len + Gp), S1 = S0 + Len;
						if (S1 > To) break;
						Strip(Mat, S0, S1, [&](double d) { return E0(d) - Sg * Wl / 2; }, [&](double d) { return E0(d) + Sg * Wl / 2; }, 1.0, Grime(S0));
						Strip(Mat, S0, S1, [&](double d) { return E1(d) - Sg * Wl / 2; }, [&](double d) { return E1(d) + Sg * Wl / 2; }, 1.0, Grime(S0));
						Strip(Mat, S0, S0 + Wl, E0, E1, 1.0, Grime(S0));
						Strip(Mat, S1 - Wl, S1, E0, E1, 1.0, Grime(S0));
					}
				}
				else if (Kind == TEXT("teeth"))
				{
					// give way: a row of triangles across the lanes at D, their points toward the cars coming (dir: the cars' way)
					double At = 0, Dir = 1, Sz = 0.6, A0 = 0, A1 = 0; O->TryGetNumberField(TEXT("at"), At); O->TryGetNumberField(TEXT("dir"), Dir); O->TryGetNumberField(TEXT("size"), Sz);
					O->TryGetNumberField(TEXT("o0"), A0); O->TryGetNumberField(TEXT("o1"), A1);
					FPaintBuf& B = Paint.FindOrAdd(Mat);
					FVector P_, F_, R_; OnRoad(At, P_, F_, R_);
					const FVector Back = F_ * (Dir >= 0 ? -1.0 : 1.0);
					for (double X = FMath::Min(A0, A1); X + Sz <= FMath::Max(A0, A1) + 1e-3; X += Sz * 1.5)
					{
						const int32 Bi = B.V.Num();
						const FVector Qs[3] = {P_ + R_ * (X * 100.0), P_ + R_ * ((X + Sz) * 100.0), P_ + R_ * ((X + Sz / 2) * 100.0) + Back * (Sz * 1.5 * 100.0)};
						for (const FVector& Q : Qs) { B.V.Add(Surface(Q) + FVector(0, 0, 0.6) - GetActorLocation()); B.N.Add(FVector::UpVector); B.UV.Add(FVector2D(X, At)); B.C.Add(FLinearColor(1.f, Grime(At), 1.f, 1.f)); }
						B.I.Append({Bi, Bi + 1, Bi + 2});
					}
				}
			}
		// ---- its zebra crossings ("crossings": [{at, w (3), o0, o1: from - to across it, m, + to the right}]): white bars
		// 0.5 m wide with 0.5 m gaps, each running along the road (default: kerb to kerb across the carriageway) ----
		const TArray<TSharedPtr<FJsonValue>>* CJ = nullptr;
		if (R->TryGetArrayField(TEXT("crossings"), CJ))
			for (const TSharedPtr<FJsonValue>& CV : *CJ)
			{
				const TSharedPtr<FJsonObject> O = CV->AsObject();
				const double At = O->GetNumberField(TEXT("at")); double Wd = 3.0; O->TryGetNumberField(TEXT("w"), Wd);
				auto Carriage = [&](bool bR)
				{
					int32 i = 0;
					for (const TSharedPtr<FJsonValue>& L : Lanes->GetArrayField(bR ? TEXT("right") : TEXT("left")))
					{ const FString T = L->AsObject()->GetStringField(TEXT("type")); if (T == TEXT("Sidewalk") || T == TEXT("Island")) break; ++i; }
					return Edge(bR, i, At);
				};
				double A0 = -Carriage(false), A1 = Carriage(true); O->TryGetNumberField(TEXT("o0"), A0); O->TryGetNumberField(TEXT("o1"), A1);
				const double Lo = FMath::Min(A0, A1), Hi = FMath::Max(A0, A1);
				const int32 Bars = FMath::Max(1, FMath::FloorToInt((Hi - Lo - 0.25) / 1.0));
				const double Start = Lo + ((Hi - Lo) - (Bars * 1.0 - 0.5)) / 2.0;
				for (int32 b = 0; b < Bars; ++b)
				{
					const double X0 = Start + b * 1.0;
					Strip(FName(TEXT("paintwhite")), At - Wd / 2, At + Wd / 2, [X0](double) { return X0; }, [X0](double) { return X0 + 0.5; }, Wd / 4, Grime(At));
				}
				++Crossings;
			}
		// ---- its street lights ("lights": [{side, edge, off, spacing, from, to, toward: road / out}]): a row of posts on a
		// lane edge (the back of a pavement, an island's middle, past a road's edge), standing on what is there ----
		const TArray<TSharedPtr<FJsonValue>>* LJ = nullptr;
		if (R->TryGetArrayField(TEXT("lights"), LJ))
			for (const TSharedPtr<FJsonValue>& LV : *LJ)
			{
				if (!Lights) Lights = W->SpawnActor<AStreetLights>(FVector::ZeroVector, FRotator::ZeroRotator);
				const TSharedPtr<FJsonObject> O = LV->AsObject();
				double Spacing = 30.0, From = -1, To = RoadLen; FString Toward = TEXT("road");
				O->TryGetNumberField(TEXT("spacing"), Spacing); O->TryGetNumberField(TEXT("from"), From); O->TryGetNumberField(TEXT("to"), To); O->TryGetStringField(TEXT("toward"), Toward);
				if (From < 0) From = Spacing * 0.5;
				int32 Placed = 0;
				for (double D = From; D <= FMath::Min(To, RoadLen); D += Spacing)
				{
					FVector P_, F_, R_; OnRoad(D, P_, F_, R_);
					const double Ofs = Off(O, D);
					const FVector Out = R_ * (Ofs >= 0 ? 1.0 : -1.0);
					if (Lights->AddPost(Surface(P_ + R_ * (Ofs * 100.0)), Toward == TEXT("out") ? Out : -Out)) ++Placed;
				}
				ADP_LOG(TEXT("Roads: %s - %d street lights (every %.0f m)"), *Id, Placed, Spacing);
			}
		// ---- its airport signs ("signs": [{at, km, dir: 1 for the cars driving along the path, -1 against it}]): on those
		// cars' right, 1.5 m out past the road's outer edge, on the ground, facing them ----
		const TArray<TSharedPtr<FJsonValue>>* SJ = nullptr;
		if (R->TryGetArrayField(TEXT("signs"), SJ) && SJ->Num())
		{
			if (!Signs) Signs = W->SpawnActor<ARoadSigns>(FVector::ZeroVector, FRotator::ZeroRotator);
			for (const TSharedPtr<FJsonValue>& SV : *SJ)
			{
				const TSharedPtr<FJsonObject> O = SV->AsObject();
				const double D = O->GetNumberField(TEXT("at")); double Dir = 1.0; O->TryGetNumberField(TEXT("dir"), Dir);
				FString Km = TEXT("2"); O->TryGetStringField(TEXT("km"), Km);
				FVector P_, F_, R_; OnRoad(D, P_, F_, R_);
				const FVector Fw = F_ * (Dir >= 0 ? 1.0 : -1.0), Rt(-Fw.Y, Fw.X, 0.0);
				FVector SAt = P_ + Rt * ((Edge(Dir >= 0, 99, D) + 1.5) * 100.0);
				SAt.Z = AAirfield::GroundAt(SAt.X / 100.0, -SAt.Y / 100.0) * 100.0;
				Signs->AddSign(SAt, -Fw, Km);
			}
		}
		// ---- a roundabout's middle ("disc": {r, kerb}): grass, raised, filling the ring's inside ----
		const TSharedPtr<FJsonObject>* DJ = nullptr;
		if (R->TryGetObjectField(TEXT("disc"), DJ))
		{
			double Rd = 5.0, Kb = 0.15; (*DJ)->TryGetNumberField(TEXT("r"), Rd); (*DJ)->TryGetNumberField(TEXT("kerb"), Kb);
			FVector2D Cn(0, 0); for (const FVector2D& D : Dense) Cn += D; Cn /= Dense.Num();
			const double Zc = AAirfield::GroundAt(Cn.X, Cn.Y) + Lift + 0.03 + Kb - 0.01;
			FPaintBuf& B = Paint.FindOrAdd(FName(TEXT("ggrass")));
			const int32 Bi = B.V.Num();
			B.V.Add(FVector(Cn.X * 100.0, -Cn.Y * 100.0, Zc * 100.0) - GetActorLocation()); B.N.Add(FVector::UpVector); B.UV.Add(FVector2D(0.5, 0.5)); B.C.Add(FLinearColor::White);
			const int32 Seg = 48;
			for (int32 k = 0; k <= Seg; ++k)
			{
				const double A = 2 * PI * k / Seg;
				B.V.Add(FVector((Cn.X + Rd * FMath::Cos(A)) * 100.0, -(Cn.Y + Rd * FMath::Sin(A)) * 100.0, Zc * 100.0) - GetActorLocation());
				B.N.Add(FVector::UpVector); B.UV.Add(FVector2D(0.5 + 0.5 * FMath::Cos(A), 0.5 + 0.5 * FMath::Sin(A))); B.C.Add(FLinearColor::White);
				if (k) B.I.Append({Bi, Bi + k, Bi + k + 1});
			}
		}
	}
	// ---- the car parks ("lots": [{x0, y0, x1, y1 (plan m), lift, rows: [{x0, x1, y, d, dir, w}]}]): tarmac on our ground,
	// the spaces' white lines (one between every two, one across their heads) ----
	const TArray<TSharedPtr<FJsonValue>>* LotJ = nullptr;
	if (Root->TryGetArrayField(TEXT("lots"), LotJ))
		for (const TSharedPtr<FJsonValue>& LV : *LotJ)
		{
			const TSharedPtr<FJsonObject> O = LV->AsObject();
			const double X0 = O->GetNumberField(TEXT("x0")), Y0 = O->GetNumberField(TEXT("y0")), X1 = O->GetNumberField(TEXT("x1")), Y1 = O->GetNumberField(TEXT("y1"));
			double Lf = 0.03; O->TryGetNumberField(TEXT("lift"), Lf);
			auto LotAt = [&](double X, double Y, double Up) { return FVector(X * 100.0, -Y * 100.0, (AAirfield::GroundAt(X, Y) + Lf + Up) * 100.0) - GetActorLocation(); };
			auto LotGrime = [](double D) { return 0.3f + 0.5f * float(FMath::Frac(FMath::Sin(D * 12.9898) * 43758.5453)); };
			auto Rect = [&](FName Mat, double A0, double B0, double A1, double B1, double Up, double Step)
			{
				FPaintBuf& B = Paint.FindOrAdd(Mat);
				const int32 Nx = FMath::Max(1, FMath::CeilToInt((A1 - A0) / Step)), Ny = FMath::Max(1, FMath::CeilToInt((B1 - B0) / Step));
				const int32 Bi = B.V.Num();
				for (int32 j = 0; j <= Ny; ++j) for (int32 i = 0; i <= Nx; ++i)
				{
					const double X = FMath::Lerp(A0, A1, double(i) / Nx), Y = FMath::Lerp(B0, B1, double(j) / Ny);
					B.V.Add(LotAt(X, Y, Up)); B.N.Add(FVector::UpVector); B.UV.Add(FVector2D(0.4, Y / 4.0)); B.C.Add(FLinearColor(1.f, LotGrime(X + Y), 1.f, 1.f));
				}
				for (int32 j = 0; j < Ny; ++j) for (int32 i = 0; i < Nx; ++i) { const int32 a = Bi + j * (Nx + 1) + i; B.I.Append({a, a + 1, a + Nx + 2, a, a + Nx + 2, a + Nx + 1}); }
			};
			Rect(FName(TEXT("groad")), X0, Y0, X1, Y1, 0.0, 4.0);
			int32 Spaces = 0;
			const TArray<TSharedPtr<FJsonValue>>* RJ = nullptr;
			if (O->TryGetArrayField(TEXT("rows"), RJ))
				for (const TSharedPtr<FJsonValue>& RV : *RJ)
				{
					const TSharedPtr<FJsonObject> Rw = RV->AsObject();
					const double RX0 = Rw->GetNumberField(TEXT("x0")), RX1 = Rw->GetNumberField(TEXT("x1")), RY = Rw->GetNumberField(TEXT("y"));
					double Dd = 5.0, Dr = 1.0, Ww = 2.5; Rw->TryGetNumberField(TEXT("d"), Dd); Rw->TryGetNumberField(TEXT("dir"), Dr); Rw->TryGetNumberField(TEXT("w"), Ww);
					const double YH = RY + Dr * Dd;
					const int32 Nb = FMath::FloorToInt((RX1 - RX0) / Ww);
					for (int32 b = 0; b <= Nb; ++b) { const double X = RX0 + b * Ww; Rect(FName(TEXT("paintwhite")), X - 0.06, FMath::Min(RY, YH), X + 0.06, FMath::Max(RY, YH), 0.006, 2.5); }
					Rect(FName(TEXT("paintwhite")), RX0, YH - 0.06, RX0 + Nb * Ww, YH + 0.06, 0.006, 3.0);
					Spaces += Nb;
				}
			ADP_LOG(TEXT("Roads: a car park %.0f x %.0f m with %d spaces"), X1 - X0, Y1 - Y0, Spaces);
		}
	// ---- every road's paint, the car parks and the roundabouts' middles: one mesh, a section per material ----
	if (Paint.Num())
	{
		UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this, TEXT("RoadPaint"));
		PM->SetupAttachment(RootComponent); PM->SetCollisionEnabled(ECollisionEnabled::QueryOnly); PM->RegisterComponent();
		AddInstanceComponent(PM);
		int32 Sct = 0;
		for (TPair<FName, FPaintBuf>& It : Paint)
		{
			FPaintBuf& B = It.Value;
			// (wound to face up: Unreal shows a triangle from the side its corners run clockwise on - as AAirfield::Tri)
			for (int32 t = 0; t + 2 < B.I.Num(); t += 3) if (FVector::CrossProduct(B.V[B.I[t + 1]] - B.V[B.I[t]], B.V[B.I[t + 2]] - B.V[B.I[t]]).Z > 0) Swap(B.I[t + 1], B.I[t + 2]);
			PM->CreateMeshSection_LinearColor(Sct, B.V, B.I, B.N, B.UV, B.C, TArray<FProcMeshTangent>(), It.Key == TEXT("ggrass") || It.Key == TEXT("groad"));
			PM->SetMaterial(Sct, ATerminalBuilder::KitMaterial(this, It.Key));
			++Sct;
		}
	}
	ADP_LOG(TEXT("Roads: %d built, %d zebra crossings, in %.2f s (MetaRoad Free, at run time)"), Built, Crossings, FPlatformTime::Seconds() - T0);
	return Built;
}
