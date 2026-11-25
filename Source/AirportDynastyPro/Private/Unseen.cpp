// Where a person may appear or vanish (Unseen.h, to-do 294).
#include "Unseen.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "PaxNav.h"
#include "People.h"
#include "RoadNetwork.h"
#include "TerminalPlan.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ObjectKey.h"
#include "UObject/UObjectHash.h"

namespace UnseenInternal
{
	using ADPUnseen::ESee;
	using ADPUnseen::FPlace;
	constexpr float UsTurn = 2500.f;      // (nearer than this a turn of the head would show it)
	constexpr float UsPx = 2.f;
	constexpr float UsLandZ = -21.f;      // (plan z: the forecourt's kerb on the terminal's side; the car park's side begins at -51.25)
	constexpr float UsParkZ = -53.f;

	struct FUsCam { FVector At, X, Y, Z; float TanH = 1.f, TanV = 1.f, PxPerTan = 1000.f; bool bPlayer = true; };
	typedef TArray<FUsCam, TInlineAllocator<8>> FUsCams;

	struct FUsState
	{
		TWeakObjectPtr<UWorld> W;
		TArray<TWeakObjectPtr<USceneCaptureComponent2D>> Caps; double CapsAt = -100.0;
		TMap<FObjectKey, bool> Mats;      // (material: does it hide?)
		TArray<FVector2f> Grid; int32 Gi = 0; bool bGrid = false, bDone = false;
		TArray<FPlace> Places; int32 Ci = 0, Li = 0; uint32 N = 0;
		int32 NoWay = 0;
	};

	FUsState& UsState(UWorld* W)
	{
		static FUsState S;
		if (S.W.Get() != W) { S = FUsState(); S.W = W; }
		return S;
	}

	FCollisionQueryParams UsParams(UWorld* W, AActor* Ignore)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPUnseen), true);
		Q.bReturnFaceIndex = true;
		if (Ignore) Q.AddIgnoredActor(Ignore);
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It) if (It->IsValid()) if (APawn* P = (*It)->GetPawn()) Q.AddIgnoredActor(P);
		return Q;
	}

	bool UsMatHides(FUsState& S, const UMaterialInterface* M)
	{
		if (!M) return false;
		const FObjectKey Key(M);
		if (const bool* B = S.Mats.Find(Key)) return *B;
		const FString N = M->GetPathName().ToLower();
		const bool b = M->GetBlendMode() == BLEND_Opaque && !N.Contains(TEXT("glass")) && !N.Contains(TEXT("glaz")) && !N.Contains(TEXT("window"));
		S.Mats.Add(Key, b);
		return b;
	}

	// what was hit hides what is behind it: solid, drawn, no person
	bool UsHides(FUsState& S, const FHitResult& H)
	{
		UPrimitiveComponent* C = H.GetComponent();
		if (!C || !C->IsVisible() || C->bHiddenInGame || (C->GetOwner() && C->GetOwner()->IsHidden())) return false;
		if (C->IsA<USkeletalMeshComponent>() || Cast<APawn>(C->GetOwner())) return false;
		int32 Sec = 0;
		if (const UMaterialInterface* M = H.FaceIndex >= 0 ? C->GetMaterialFromCollisionFaceIndex(H.FaceIndex, Sec) : nullptr) return UsMatHides(S, M);
		const int32 Num = C->GetNumMaterials();
		for (int32 i = 0; i < Num; ++i) if (!UsMatHides(S, C->GetMaterial(i))) return false;
		return Num > 0;
	}

	bool UsClear(UWorld* W, FUsState& S, const FVector& From, const FVector& To, const FCollisionQueryParams& Q0)
	{
		const FVector D = (To - From).GetSafeNormal();
		const double Len = FVector::Dist(From, To);
		double T = 60.0;   // (a thing held before the eye hides nobody)
		FCollisionQueryParams Q = Q0;
		for (int32 Step = 0; Step < 8 && T < Len; ++Step)
		{
			FHitResult H;
			if (!W->LineTraceSingleByChannel(H, From + D * T, To, ECC_Visibility, Q)) return true;
			if (!H.bStartPenetrating && UsHides(S, H)) return false;
			if (H.bStartPenetrating || Cast<USkeletalMeshComponent>(H.GetComponent())) { if (H.GetComponent()) Q.AddIgnoredComponent(H.GetComponent()); }
			else T += H.Distance + 4.0;
		}
		return true;   // (nothing solid found: taken as seen)
	}

	void UsCameras(UWorld* W, FUsState& S, FUsCams& Out)
	{
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager) continue;
			int32 Vx = 0, Vy = 0; PC->GetViewportSize(Vx, Vy);
			if (Vx < 16 || Vy < 16) { Vx = 1920; Vy = 1080; }
			FUsCam C; C.At = PC->PlayerCameraManager->GetCameraLocation();
			const FRotationMatrix M(PC->PlayerCameraManager->GetCameraRotation());
			C.X = M.GetUnitAxis(EAxis::X); C.Y = M.GetUnitAxis(EAxis::Y); C.Z = M.GetUnitAxis(EAxis::Z);
			C.TanH = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(PC->PlayerCameraManager->GetFOVAngle(), 5.f, 160.f) * 0.5f));
			C.PxPerTan = Vx * 0.5f / C.TanH;
			C.TanV = C.TanH * FMath::Max(float(Vy) / Vx, 0.5625f);
			Out.Add(C);
		}
		const int32 NumPl = Out.Num();
		const double Now = W->GetTimeSeconds();
		if (Now - S.CapsAt > 3.0)
		{
			S.CapsAt = Now; S.Caps.Reset();
			TArray<UObject*> Os; GetObjectsOfClass(USceneCaptureComponent2D::StaticClass(), Os, true, RF_ClassDefaultObject);
			for (UObject* O : Os) if (USceneCaptureComponent2D* C = Cast<USceneCaptureComponent2D>(O)) if (C->GetWorld() == W && C->IsRegistered()) S.Caps.Add(C);
		}
		// (a screen or mirror shows something only while you are near it, or its camera runs on its own)
		for (const TWeakObjectPtr<USceneCaptureComponent2D>& Cw : S.Caps)
		{
			const USceneCaptureComponent2D* K = Cw.Get();
			if (!K || !K->TextureTarget || !K->IsVisible() || !K->ShowFlags.SkeletalMeshes) continue;
			bool bOn = K->bCaptureEveryFrame;
			for (int32 i = 0; i < NumPl && !bOn; ++i) bOn = FVector::DistSquared(Out[i].At, K->GetComponentLocation()) < FMath::Square(4000.f);
			if (!bOn) continue;
			FUsCam C; C.bPlayer = false; C.At = K->GetComponentLocation();
			C.X = K->GetForwardVector(); C.Y = K->GetRightVector(); C.Z = K->GetUpVector();
			C.TanH = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(K->FOVAngle, 5.f, 160.f) * 0.5f)); C.TanV = C.TanH;
			C.PxPerTan = FMath::Max(64.f, float(K->TextureTarget->SizeX)) * 0.5f / C.TanH;
			Out.Add(C);
		}
	}

	// a body's points: the middle line (head, middle, feet) and both sides (a lamp post hides nobody)
	void UsPoints(const FVector& Feet, float H, float Half, const FVector& Eye, FVector (&P)[7])
	{
		const FVector Side = FVector::CrossProduct(FVector::UpVector, (Feet - Eye).GetSafeNormal2D()) * Half;
		const FVector Head = Feet + FVector(0, 0, H - 8.f), Mid = Feet + FVector(0, 0, H * 0.55f);
		P[0] = Head; P[1] = Mid; P[2] = Feet + FVector(0, 0, 12.f);
		P[3] = Mid + Side; P[4] = Mid - Side; P[5] = Head + Side * 0.6f; P[6] = Head - Side * 0.6f;
	}

	bool UsFree(UWorld* W, const FVector& Feet, const FCollisionQueryParams& Q)
	{
		return !W->OverlapBlockingTestByChannel(Feet + FVector(0, 0, 95.f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeCapsule(30.f, 60.f), Q);
	}

	// something solid and wide beside the point, as tall as a person (two neighbouring looks out of twelve, at the eyes and the knee)
	bool UsCover(UWorld* W, FUsState& S, const FVector& Feet, const FCollisionQueryParams& Q)
	{
		bool b[12];
		for (int32 k = 0; k < 12; ++k)
		{
			const FVector D = FVector(FMath::Cos(k * UE_PI / 6.f), FMath::Sin(k * UE_PI / 6.f), 0.f) * 260.f;
			const FVector Hi = Feet + FVector(0, 0, 178.f), Lo = Feet + FVector(0, 0, 60.f);
			FHitResult H;
			b[k] = W->LineTraceSingleByChannel(H, Hi, Hi + D, ECC_Visibility, Q) && UsHides(S, H) && W->LineTraceSingleByChannel(H, Lo, Lo + D, ECC_Visibility, Q) && UsHides(S, H);
			if (k > 0 && b[k] && b[k - 1]) return true;
		}
		return b[0] && b[11];
	}

	// the way between the place and the terminal (or the zebra crossing) keeps to its side of the forecourt's road
	bool UsWay(UWorld* W, const FVector& P, bool bTerminalSide)
	{
		auto Pt = [](float X, float Z) { return FTerminalPlan::ToWorld(X, float(ARoadNetwork::GroundAt(X, Z)) + 0.03f, Z); };
		FVector To = bTerminalSide ? Pt(75.f, -8.f) : Pt(74.1f, -53.f);
		if (!bTerminalSide)
		{
			double Best = 1.0e18;
			for (const TPair<FVector, FVector>& Z : ARoadNetwork::ZebraCrossings)
				for (const FVector& E : {Z.Key, Z.Value})
					if (E.Y > 5000.0 && E.Y < 5600.0 && FVector::DistSquared2D(E, P) < Best) { Best = FVector::DistSquared2D(E, P); To = Pt(float(E.X / 100.0), float(-E.Y / 100.0) - 1.5f); }
		}
		TArray<FVector> Pts;
		const PaxNav::EPath R = PaxNav::Path(W, P, To, Pts);
		if (!(R == PaxNav::Full || (R == PaxNav::Partial && Pts.Num() >= 2 && FVector::Dist2D(Pts.Last(), To) < 150.f))) return false;
		for (const FVector& V : Pts) { const double Z = -V.Y / 100.0; if (bTerminalSide ? Z < UsLandZ : Z > -50.5) return false; }
		return true;
	}

	void UsScan(UWorld* W, FUsState& S)
	{
		if (!S.bGrid)
		{
			S.bGrid = true;
			for (float X = -43.5f; X <= 342.f; X += 3.f) for (float Z = -13.f; Z <= -5.f; Z += 2.f) S.Grid.Add(FVector2f(X, Z));   // (the pavement along the terminal, in the navmesh)
			for (float X = 19.f; X <= 131.f; X += 3.f) for (float Z = -148.f; Z <= UsParkZ; Z += 3.f) S.Grid.Add(FVector2f(X, Z));
		}
		if (S.Gi >= S.Grid.Num())
		{
			S.bDone = true;
			int32 Cv = 0, Te = 0; for (const FPlace& P : S.Places) { Cv += P.bCover ? 1 : 0; Te += P.bTerminalSide ? 1 : 0; }
			ADP_LOG(TEXT("Unseen: %d landside places found (%d behind something solid; %d on the terminal's side, %d on the car park's), %d points without a way to the terminal"), S.Places.Num(), Cv, Te, S.Places.Num() - Te, S.NoWay);
			return;
		}
		const FVector2f G = S.Grid[S.Gi]; const int32 Ix = S.Gi++;
		const FCollisionQueryParams Q = UsParams(W, nullptr);
		FVector P = FTerminalPlan::ToWorld(G.X, float(ARoadNetwork::GroundAt(G.X, G.Y)) + 0.03f, G.Y);
		FHitResult H;
		if (W->LineTraceSingleByChannel(H, P + FVector(0, 0, 150.f), P - FVector(0, 0, 100.f), ECC_Visibility, Q) && H.ImpactNormal.Z > 0.8f) P.Z = H.ImpactPoint.Z;
		FVector On;
		if (!PaxNav::Nearest(W, P, 60.f, On)) return;
		P.X = On.X; P.Y = On.Y;
		if (!UsFree(W, P, Q)) return;
		const bool bCover = UsCover(W, S, P, Q);
		if (!bCover && Ix % 13) return;   // (a few open ones too: hidden by whatever stands between, the terminal mostly)
		for (const FPlace& O : S.Places) if (FVector::DistSquared2D(O.P, P) < FMath::Square(220.f)) return;
		const bool bTe = G.Y > UsLandZ;
		if (!UsWay(W, P, bTe)) { ++S.NoWay; return; }
		FPlace N; N.P = P; N.bTerminalSide = bTe; N.bCover = bCover; S.Places.Add(N);
	}
}

ADPUnseen::ESee ADPUnseen::Look(UWorld* W, const FVector& Feet, float Height, float Half, AActor* Ignore)
{
	using namespace UnseenInternal;
	if (!W) return ESee::Seen;
	FUsState& S = UsState(W);
	FUsCams Cams; UsCameras(W, S, Cams);
	const FCollisionQueryParams Q = UsParams(W, Ignore);
	ESee Res = ESee::Far;
	for (const FUsCam& C : Cams)
	{
		FVector P[7]; UsPoints(Feet, Height, Half, C.At, P);
		const float D = float(FVector::Dist(C.At, P[1]));
		if (FMath::Max(Height, Half * 2.f) / FMath::Max(D, 1.f) * C.PxPerTan < UsPx) continue;
		bool bIn = false;
		for (const FVector& V : P)
		{
			const FVector R = V - C.At; const double F = FVector::DotProduct(R, C.X);
			if (F > -50.0 && FMath::Abs(FVector::DotProduct(R, C.Y)) < FMath::Max(F, 0.0) * C.TanH * 1.2 + 150.0 && FMath::Abs(FVector::DotProduct(R, C.Z)) < FMath::Max(F, 0.0) * C.TanV * 1.2 + 150.0) { bIn = true; break; }
		}
		if (!C.bPlayer && !bIn) continue;
		bool bClear = false;
		for (const FVector& V : P) if (UsClear(W, S, C.At, V, Q)) { bClear = true; break; }
		if (!bClear) { if (Res == ESee::Far) Res = ESee::Covered; continue; }
		if (bIn || D < UsTurn) return ESee::Seen;
		Res = ESee::OutOfFrame;
	}
	return Res;
}

bool ADPUnseen::Budget()
{
	static uint64 Frame = 0; static int32 Used = 0;
	if (Frame != GFrameCounter) { Frame = GFrameCounter; Used = 0; }
	return ++Used <= 2;
}

bool ADPUnseen::May(UWorld* W, const FVector& Feet, float Height, float Half, AActor* Ignore)
{
	ADP_PERF_SCOPE("unseen: one look", 1.f);
	return Budget() && Look(W, Feet, Height, Half, Ignore) != ESee::Seen;
}

void ADPUnseen::Tick(UWorld* W)
{
	using namespace UnseenInternal;
	ADP_PERF_SCOPE("unseen: the landside places", 1.f);
	FUsState& S = UsState(W);
	if (!S.bDone && (PaxNav::Ready(W) || PaxNav::Failed(W))) UsScan(W, S);
	if (!S.Places.Num()) return;
	++S.N;
	// every third frame: is the next place hidden now (round and round, so the choice is made from fresh looks)
	if (S.N % 3 == 0)
	{
		if (!Budget()) return;
		S.Li = (S.Li + 1) % S.Places.Num();
		FPlace& P = S.Places[S.Li]; P.See = Look(W, P.P); P.SeenAt = W->GetTimeSeconds();
	}
	// every tenth or so: what stands beside it now (coaches, buses and cars come and go)
	else if (S.N % 10 == 1 && S.bDone)
	{
		S.Ci = (S.Ci + 1) % S.Places.Num();
		S.Places[S.Ci].bCover = UsCover(W, S, S.Places[S.Ci].P, UsParams(W, nullptr));
	}
}

int32 ADPUnseen::Appear(UWorld* W, FPlace& Out, bool& bNone)
{
	using namespace UnseenInternal;
	FUsState& S = UsState(W); bNone = false;
	const double Now = W->GetTimeSeconds();
	// behind something near and covered first, then covered by whatever is between, then outside the picture
	int32 Best = INDEX_NONE; float Bs = -1.f;
	for (int32 i = 0; i < S.Places.Num(); ++i)
	{
		const FPlace& P = S.Places[i];
		if (P.See == ESee::Seen || P.SeenAt < 0.0 || Now - P.UsedAt < 8.0 || P.HeldTo > Now) continue;
		const float Sc = (P.See == ESee::Covered ? 4.f : P.See == ESee::Far ? 2.f : 1.f) + (P.bCover ? 3.f : 0.f) + FMath::FRand() * 2.f;
		if (Sc > Bs) { Bs = Sc; Best = i; }
	}
	if (Best == INDEX_NONE) { bNone = true; return INDEX_NONE; }
	if (!Budget()) return INDEX_NONE;
	ADP_PERF_SCOPE("unseen: one place", 1.f);
	FPlace& P = S.Places[Best];
	P.See = Look(W, P.P); P.SeenAt = Now;
	if (P.See == ESee::Seen) return INDEX_NONE;   // (seen after all: the next one the next frame)
	const AADPPeople* Pe = AADPPeople::Get(W);
	P.UsedAt = Now;
	if ((Pe && Pe->AnyoneNear(P.P, 140.f)) || !UsFree(W, P.P, UsParams(W, nullptr))) return INDEX_NONE;   // (somebody or something stands there)
	Out = P;
	return Best;
}

int32 ADPUnseen::Leave(UWorld* W, const FVector& From, bool bTerminalSide, int32 Not, FPlace& Out)
{
	using namespace UnseenInternal;
	FUsState& S = UsState(W);
	const double Now = W->GetTimeSeconds();
	int32 Best = INDEX_NONE; float Bs = MAX_flt;
	for (int32 i = 0; i < S.Places.Num(); ++i)
	{
		const FPlace& P = S.Places[i];
		if (i == Not || P.HeldTo > Now || Now - P.UsedAt < 8.0) continue;
		// (hidden ones first, something solid beside them first, then near; a crossing counts 60 m; and some luck)
		const float Sc = float(FVector::Dist2D(From, P.P)) / 100.f + (P.bTerminalSide != bTerminalSide ? 60.f : 0.f) + (P.See == ESee::Seen ? 200.f : P.See == ESee::OutOfFrame ? 50.f : 0.f) + (P.bCover ? 0.f : 60.f) + FMath::FRand() * 70.f;
		if (Sc < Bs) { Bs = Sc; Best = i; }
	}
	if (Best == INDEX_NONE) return INDEX_NONE;
	S.Places[Best].HeldTo = Now + 420.0; Out = S.Places[Best];
	return Best;
}

void ADPUnseen::Release(UWorld* W, int32 Place)
{
	UnseenInternal::FUsState& S = UnseenInternal::UsState(W);
	if (S.Places.IsValidIndex(Place)) { S.Places[Place].HeldTo = -1000.0; S.Places[Place].UsedAt = W->GetTimeSeconds(); }
}

bool ADPUnseen::NoPlaces(UWorld* W)
{
	const UnseenInternal::FUsState& S = UnseenInternal::UsState(W);
	return S.bDone && !S.Places.Num();
}

FString ADPUnseen::Line(UWorld* W)
{
	const UnseenInternal::FUsState& S = UnseenInternal::UsState(W); int32 Cv = 0, Hd = 0;
	for (const FPlace& P : S.Places) { Cv += P.bCover ? 1 : 0; Hd += P.See != ESee::Seen ? 1 : 0; }
	return FString::Printf(TEXT("%d places (%d behind something), %d hidden now"), S.Places.Num(), Cv, Hd);
}
