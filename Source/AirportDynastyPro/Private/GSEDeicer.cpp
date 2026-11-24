// The de-icing truck at work (to-do 212; the vehicle, its driving and its people are the shared ones of GSEService.cpp): a
// hired driver and a hired operator. When the real weather asks for it (DiWeather below) it comes for your own flight once the
// doors are shut and the jetway is back: down beside the left wing, behind the left tailplane, the right one, beside the right
// wing - straight lanes it drives in forwards and backs out of, the boom always stowed while it drives. The operator climbs up
// to the spray cabin at the first stop. At each stop he does what the boom reaches from there (each place has its pose: solved
// on the aircraft that stands there - published A320 measures, scaled by its bounds), hot type I first and, while something
// falls, type IV over it at once. Then the holdover time runs; a flight still there when it is over is done again.
// (The stops of deicer-parts.json are the reach search's: poses a truck of this turning circle cannot drive between. The
// lanes here are drivable and reach the same surfaces.)
#include "GSEDeicer.h"
#include "GSE.h"
#include "Airfield.h"
#include "ServiceVehicles.h"
#include "ServiceDrive.h"
#include "VehicleControls.h"
#include "Tugs.h"
#include "CrewPose.h"
#include "Puddles.h"
#include "Site.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "CableComponent.h"
#include "Sim/ADPWeather.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	using FDi = FSvcDeicer;
	const FVector DiAxis(215.f, 0.f, 0.f), DiPivot(245.f, 0.f, 286.f), DiTip(-290.f, 0.f, 286.f), DiNozzle[2] = {FVector(-494.8f, -6.f, 117.f), FVector(-494.8f, 6.f, 117.f)};
	const FVector DiUp(0, 0, 1);
	const float DiLane = 20.43f, DiTail = 12.6f, DiGiveUp = 900.f;   // (the lanes, m off the centre line; 15 min for a lane to come free)
	const float DiRateI = 110.f / 60.f, DiRateIV = 45.f / 60.f;      // l/s
	const int32 DiCabin = 1;                                           // (SvcDoors: the spray cabin's door)

	FQuat DiQ(const FVector& Ax, float Deg) { return FQuat(Ax, FMath::DegreesToRadians(Deg)); }
	FTransform DiAbout(const FQuat& Q, const FVector& P) { return FTransform(Q, P - Q.RotateVector(P)); }
	// the superstructure's frames in the truck's root, from how it was modelled (stowed)
	FTransform DiTur(const FDi::FPose& P) { return DiAbout(DiQ(DiUp, P.Slew), DiAxis); }
	FTransform DiBase(const FDi::FPose& P) { return DiAbout(DiQ(FVector::RightVector, P.Lift), DiPivot) * DiTur(P); }
	FTransform DiCab(const FDi::FPose& P)
	{
		const FVector Tip = DiPivot + DiQ(FVector::RightVector, P.Lift).RotateVector(DiTip - FVector(P.Ext, 0.f, 0.f) - DiPivot);
		return FTransform(Tip - DiTip) * DiTur(P);
	}
	FTransform DiLink(int32 i, float Ext)
	{
		// its hose chain: the lower run, the loop, back on the upper run (the base boom's frame)
		const float S = 9.5f * (i + 0.5f), X = 55.f - Ext * 0.5f, L = X + 245.f, R = 10.5f;
		if (S < L) return FTransform(FVector(-245.f + S, 0.f, 315.f));
		const float A = (S - L) / R;
		if (A < PI) return FTransform(FQuat(FVector::RightVector, -A), FVector(X + R * FMath::Sin(A), 0.f, 325.5f - R * FMath::Cos(A)));
		return FTransform(FQuat(FVector::RightVector, -PI), FVector(X - (S - L - PI * R), 0.f, 336.f));
	}

	// ---- the aircraft: published A320 measures in metres (x behind the nose, y from the centre line, z up)
	double DiWingZ(double y) { return 3.3 + (FMath::Abs(y) - 1.98) / 14.92 * 1.6; }
	void DiWingX(double y, double& Le, double& Te) { const double s = (FMath::Abs(y) - 1.98) / 14.92; Le = 11.9 + 7.5 * s; Te = 18.6 + 2.3 * s; }
	double DiSlab(const FVector& P, double y0, double y1, double Le, double Te, double Z, double H)
	{
		const double dx = FMath::Max3(Le - P.X, P.X - Te, 0.0), dy = FMath::Max3(y0 - P.Y, P.Y - y1, 0.0), dz = FMath::Max(FMath::Abs(P.Z - Z) - H, 0.0);
		return FMath::Sqrt(dx * dx + dy * dy + dz * dz);
	}
	double DiDist(const FVector& P0)
	{
		const FVector P(P0.X, FMath::Abs(P0.Y), P0.Z); double D = 1e6, Le, Te;
		{ const double x = FMath::Clamp(P.X, 2.0, 33.0), Zc = x > 24.0 ? 3.9 + (x - 24.0) * 0.13 : 3.9, R = x > 24.0 ? FMath::Lerp(1.98, 0.6, (x - 24.0) / 9.0) : 1.98; D = FMath::Min(D, FMath::Sqrt(FMath::Square(P.X - x) + P.Y * P.Y + FMath::Square(P.Z - Zc)) - R); }
		{ const double y = FMath::Clamp(P.Y, 1.98, 17.4); DiWingX(y, Le, Te); D = FMath::Min(D, DiSlab(P, y, y, Le, Te, DiWingZ(y), 0.3)); }
		D = FMath::Min(D, DiSlab(P, 17.2, 17.95, 19.7, 21.6, 6.1, 1.2));                                             // (the winglet)
		{ const double x = FMath::Clamp(P.X, 10.4, 14.6); D = FMath::Min(D, FMath::Sqrt(FMath::Square(P.X - x) + FMath::Square(P.Y - 5.75) + FMath::Square(P.Z - 2.0)) - 1.15); }
		{ const double y = FMath::Clamp(P.Y, 0.4, 6.22), s = (y - 1.0) / 5.2; D = FMath::Min(D, DiSlab(P, y, y, 31.2 + 3.7 * s, 34.6 + 1.8 * s, 5.2 + 0.7 * s, 0.2)); }
		{ const double z = FMath::Clamp(P.Z, 5.2, 11.85), s = (z - 5.9) / 5.86; D = FMath::Min(D, DiSlab(P, 0.0, 0.22, 27.9 + 6.3 * s, 35.0 + 1.3 * s, z, 0.0)); }
		return D;
	}
	FVector DiScale(const FBox& B) { return FVector(B.GetSize().X / 3580.0, B.GetSize().Y / 3757.0, B.Max.Z / 1176.0); }
	// (the model's own frame: the nose wheel at its root, x its right, y aft)
	FVector DiReal(const FDi& F, const FVector& W) { const FVector L = F.Plane->GetActorTransform().InverseTransformPosition(W), S = DiScale(F.Box); return FVector((L.Y - F.Box.Min.Y) / S.Y, L.X / S.X, L.Z / S.Z) / 100.0; }
	FVector DiWorld(const FDi& F, const FVector& R) { const FVector S = DiScale(F.Box); return F.Plane->GetActorTransform().TransformPosition(FVector(R.Y * 100.0 * S.X, F.Box.Min.Y + R.X * 100.0 * S.Y, R.Z * 100.0 * S.Z)); }
	bool DiBounds(const AActor* Pl, FBox& B)
	{
		const ABabylonModel* M = Cast<ABabylonModel>(Pl); B = FBox(ForceInit); if (!M) return false;
		TArray<UInstancedStaticMeshComponent*> Ps; M->Parts(Ps);
		for (const UInstancedStaticMeshComponent* K : Ps) if (K && K->GetInstanceCount() > 0) B += K->CalcBounds(K->GetComponentTransform().GetRelativeTransform(Pl->GetActorTransform())).GetBox();
		if (!B.IsValid) return false;
		const FVector S = DiScale(B); return S.GetMin() > 0.75 && S.GetMax() < 1.35;
	}
	/** How far the boom and its cabin are from the aircraft (m; less than 0: too near). */
	double DiClear(const FDi& F, const FTransform& Root, const FDi::FPose& P)
	{
		const FTransform Cab = DiCab(P) * Root, Base = DiBase(P) * Root; double C = 1e6;
		for (const float Z : {130.f, 190.f, 260.f}) C = FMath::Min(C, DiDist(DiReal(F, Cab.TransformPosition(FVector(-400.f, 0.f, Z)))) - 0.9 - 1.0);
		for (int32 i = 0; i <= 10; ++i) C = FMath::Min(C, DiDist(DiReal(F, Base.TransformPosition(FMath::Lerp(DiPivot, DiTip - FVector(P.Ext, 0.f, 0.f), i / 10.f)))) - 0.25 - 0.75);
		return C;
	}
	bool DiWay(const FDi& F, const FTransform& Root, const FDi::FPose& A, const FDi::FPose& B)
	{
		for (int32 i = 0; i <= 10; ++i) { const float K = i / 10.f; if (DiClear(F, Root, {FMath::Lerp(A.Slew, B.Slew, K), FMath::Lerp(A.Lift, B.Lift, K), FMath::Lerp(A.Ext, B.Ext, K)}) < 0.0) return false; }
		return true;
	}
	/** From one pose to another: straight, or up, in, round, out and down; false: neither is clear. */
	bool DiRoute(const FDi& F, const FTransform& Root, const FDi::FPose& A, const FDi::FPose& B, TArray<FDi::FPose>& Out, bool& bDirect)
	{
		Out.Reset(); bDirect = A.Lift >= 15.f && B.Lift >= 15.f && DiWay(F, Root, A, B); if (bDirect) { Out.Add(B); return true; }
		// (never slewed below 15 degrees of lift: straight up first, straight down last)
		const FDi::FPose W[] = {A, {A.Slew, 60.f, A.Ext}, {A.Slew, 60.f, 0.f}, {B.Slew, 60.f, 0.f}, {B.Slew, 60.f, B.Ext}, B};
		for (int32 i = 1; i < 6; ++i) { if (i > 1 && !DiWay(F, Root, W[i - 1], W[i])) { Out.Reset(); return false; } Out.Add(W[i]); }
		return true;
	}
	/** Where the nozzle must point for that place, in the cabin's frame (pan, tilt); how far it is. */
	float DiAim(const FTransform& Cab, const FVector& At, float& Pan, float& Tilt)
	{
		const FVector D = Cab.InverseTransformPosition(At) - FVector(-466.f, 0.f, 117.f);
		Pan = FMath::RadiansToDegrees(FMath::Atan2(-D.Y, -D.X)); Tilt = FMath::RadiansToDegrees(FMath::Atan2(D.Z + 18.0, D.Size2D())); return float(D.Size());   // (+18: it drops a little on its way)
	}
	bool DiSolve(const FDi& F, const FTransform& Root, int8 Side, const FVector& Real, bool bTop, const FDi::FPose& Last, FDi::FPose& Best, const UWorld* W, const AActor* Me)
	{
		const FVector At = DiWorld(F, Real); float Bc = 1e9f; bool bAny = false;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPDeicer), false, Me); if (F.Plane.IsValid()) Q.AddIgnoredActor(F.Plane.Get());
		FCollisionObjectQueryParams Ob; Ob.AddObjectTypesToQuery(ECC_WorldStatic); Ob.AddObjectTypesToQuery(ECC_WorldDynamic);
		for (int32 s = 0; s < 16; ++s) for (int32 l = 0; l < 8; ++l) for (int32 e = 0; e < 7; ++e)
		{
			// (the aircraft's right: the truck, nose to the terminal, has it on its left - the boom round by a right-handed turn)
			FDi::FPose P; P.Slew = Side * (35.f + 7.5f * s); P.Lift = 25.f + 5.f * l; P.Ext = 100.f * e; const FTransform Cab = DiCab(P) * Root;
			float Pn, Tl; const float D = DiAim(Cab, At, Pn, Tl); if (D < 200.f || D > 780.f || FMath::Abs(Pn) > 56.f || Tl < -34.f || Tl > 43.f) continue;
			if (bTop && Cab.TransformPosition(DiNozzle[0]).Z < At.Z + 60.f) continue;
			const float Cst = FMath::Abs(D - 400.f) * 0.02f + FMath::Abs(P.Slew - Last.Slew) * 0.08f + FMath::Abs(P.Lift - Last.Lift) * 0.08f + FMath::Abs(P.Ext - Last.Ext) * 0.01f;
			if (Cst >= Bc || DiClear(F, Root, P) < 0.0) continue;
			if (W->OverlapAnyTestByObjectType(Cab.TransformPosition(FVector(-400.f, 0.f, 200.f)), FQuat::Identity, Ob, FCollisionShape::MakeSphere(150.f), Q)) continue;   // (a jetway, a building)
			Bc = Cst; Best = P; bAny = true;
		}
		return bAny;
	}
	bool DiStowed(const FDi& F) { return F.Now.Slew == 0.f && F.Now.Lift == 0.f && F.Now.Ext == 0.f; }
	void DiHome(const FDi& F, TArray<FDi::FPose>& R)
	{
		R.Reset(); if (F.Now.Slew != 0.f || F.Now.Ext > 0.f) { R.Add({F.Now.Slew, 60.f, F.Now.Ext}); R.Add({F.Now.Slew, 60.f, 0.f}); R.Add({0.f, 60.f, 0.f}); }
		R.Add({0.f, 0.f, 0.f});
	}

	struct FDiWant { FVector R; bool bTop; };
	// the places of the aircraft a stop has to reach
	void DiWants(TArray<FDiWant>& Want, int32 Part, bool bWing, double Sg, bool bSnow)
	{
		const bool bRear = Part == 0;
	if (bWing)
	{
		// the wing from its tip inwards (from behind: the rear half, from ahead: the front half)
		for (int32 i = 0; i < 9; ++i) { const double y = FMath::Lerp(16.6, bRear ? 4.3 : 2.6, i / 8.0); double Le, Te; DiWingX(y, Le, Te); Want.Add({FVector(FMath::Lerp(Le, Te, bRear ? 0.78 : 0.25), Sg * y, DiWingZ(y) + 0.3), true}); }
	}
	else if (Part == 2)
	{
		// the wing's root from behind, and the fuselage's top over it when snow lies
		for (int32 i = 0; i < 3; ++i) { const double y = FMath::Lerp(6.2, 2.5, i / 2.0); double Le, Te; DiWingX(y, Le, Te); Want.Add({FVector(FMath::Lerp(Le, Te, 0.78), Sg * y, DiWingZ(y) + 0.3), true}); }
		if (bSnow) for (int32 i = 0; i < 4; ++i) Want.Add({FVector(22.5 - i * 1.6, Sg * 0.9, 5.65), true});
	}
	else
	{
		for (int32 i = 0; i < 4; ++i) { const double y = FMath::Lerp(5.9, 1.4, i / 3.0), s = (y - 1.0) / 5.2; Want.Add({FVector(FMath::Lerp(31.2 + 3.7 * s, 34.6 + 1.8 * s, bRear ? 0.75 : 0.25), Sg * y, 5.2 + 0.7 * s + 0.25), true}); }
		for (int32 i = 0; i < 5; ++i) { const double s = FMath::Lerp(0.05, 0.95, i / 4.0); Want.Add({FVector(FMath::Lerp(27.9 + 6.3 * s, 35.0 + 1.3 * s, bRear ? 0.7 : 0.3), Sg * 0.25, 5.9 + 5.86 * s), false}); }
		if (bSnow) for (int32 i = 0; i < 2; ++i) Want.Add({FVector(bRear ? 30.0 - i : 27.0 - i * 2.0, Sg * 0.6, 5.5), true});
	}
	}
	// ---- the weather: does it ask for it, and how long the fluid then holds
	struct FDiWx { bool bNeed = false, bFalls = false, bSnow = false; float bColdWet = false, bIcing = false, bNone = false; float HoldMin = 45.f; FString Why; double At = -1e9; };
	const FDiWx& DiWeather(const UWorld* W)
	{
		static FDiWx X; const double T = W->GetTimeSeconds(); if (T - X.At < 20.0 && T >= X.At) return X;
		X = FDiWx(); X.At = T;
		const APuddles* Pu = APuddles::Get(); if (!Pu) return X;
		FString Icao = TEXT("LSGB"); double La = 46.2, Lo = 7.0; const TSharedPtr<FJsonObject> A = ADPSite::AirportBlock();
		if (A) { A->TryGetStringField(TEXT("icao"), Icao); A->TryGetNumberField(TEXT("lat"), La); A->TryGetNumberField(TEXT("lon"), Lo); }
		const FADPWx R = ADPWeather::At(Icao, La, Lo, FDateTime::UtcNow());
		const float Tc = Pu->GetAirTemp(), Rain = Pu->GetRain(); const bool bFreezing = R.bFreezing || R.Raw.Contains(TEXT("FZ")), bPellets = R.Raw.Contains(TEXT(" PL")) || R.Raw.Contains(TEXT("-PL")) || R.Raw.Contains(TEXT("+PL")), bSnowing = R.bSnow || bPellets;
		// (plain rain above 0 C never asks for it)
		const bool bFzRain = Tc <= 3.f && ((bFreezing && R.bRain) || (Rain > 0.03f && Tc <= 0.f && !bSnowing));
		X.bFalls = bFzRain || (Tc <= 3.f && bSnowing);
		X.bSnow = Tc <= 3.f && (Pu->GetSnowOnPavingCm() > 0.3f || bSnowing);
		// the dew point of the report ("M02/M04")
		float Dew = -99.f;
		{
			TArray<FString> Tok; R.Raw.ParseIntoArrayWS(Tok); auto Num = [](FString S, float& V) { const bool bM = S.RemoveFromStart(TEXT("M")); if (S.Len() != 2 || !S.IsNumeric()) return false; V = FCString::Atof(*S) * (bM ? -1.f : 1.f); return true; };
			for (const FString& K : Tok) { FString L, Rt; float V0 = 0.f, V1 = 0.f; if (K.Split(TEXT("/"), &L, &Rt) && Num(L, V0) && Num(Rt, V1)) { Dew = V1; break; } }
		}
		const bool bMoist = Dew > -90.f && Tc - Dew <= 2.5f, bFrost = Tc <= 0.f && (bMoist || R.bFog), bFzFog = R.bFog && (Tc <= 0.f || R.Raw.Contains(TEXT("FZFG")));
		X.bColdWet = Tc > 0.f && Tc <= 10.f && Rain > 0.03f && bMoist; X.bIcing = Tc <= 3.f && (bMoist || R.bFog);
		X.bNeed = X.bFalls || X.bSnow || bFrost || bFzFog;
		X.Why = bFzRain ? TEXT("freezing rain") : bPellets && X.bFalls ? TEXT("ice pellets") : X.bFalls ? TEXT("snow") : X.bSnow ? TEXT("snow or slush lying") : bFzFog ? TEXT("freezing fog") : TEXT("frost");
		// (holdover times, in short: type IV over type I while something falls, type I alone for frost. NOT checked against the FAA 2026-27
		// tables: their site refuses automatic reading. The minutes are kept as they were until somebody reads them off by hand.)
		X.HoldMin = !X.bFalls ? (bFzFog ? 60.f : X.bColdWet ? 35.f : 45.f) : bFzRain ? 25.f : R.Raw.Contains(TEXT("+SN")) ? 20.f : R.Raw.Contains(TEXT("-SN")) ? 45.f : 30.f;
		X.bNone = bFrost && !X.bFalls && !X.bSnow && !bFzFog && Tc < -10.f;   // (active frost below -10 C: no holdover)
		if (Tc < -14.f) X.HoldMin *= 0.6f;
		return X;
	}
}

// ------------------------------------------------------------------ when

void AGSE::DeiceTick(float StandX, const FString& Reg, const FString& Flight, const FDateTime& Now, const FDateTime& OffBlocks, bool bSoon, bool bDoors, const FDateTime& OnBlocks)
{
	if (!bSoon && !bDoors) return;
	DeiceNow = Now; const FString Key = Reg + TEXT("/") + Flight;
	for (const TPair<int32, FSvcJob>& P : SvcJobs) if (ADPService::All()[P.Value.Svc].bDeicer && FMath::Abs(P.Value.StandX - StandX) < 1.f) { if (bDoors) SvcGo(TEXT("deice"), StandX); return; }
	const FDeiced* D = Deiced.Find(Key); if (D && (Now < D->Until || D->Rounds >= 3)) return;
	// looked over before it goes: the truck only when something falls or lies on it
	// TODO(DeiceLying): lying snow is taken to lie on the aircraft too - when it last fell is not known against the aircraft's arrival
	FDiWx X = DiWeather(GetWorld());
	if (!X.bNeed && X.bColdWet && OnBlocks.GetTicks() > 0 && Now >= OnBlocks && (Now - OnBlocks).GetTotalMinutes() <= 60.0) { X.bNeed = true; X.Why = TEXT("rain on wings cold from the flight"); }
	if (!X.bNeed && X.bIcing) { static TSet<FString> Looked; bool bWas = false; Looked.Add(Key, &bWas); if (!bWas) ADP_LOG(TEXT("GSE: %s looked over in icing weather: clean, no de-icing"), *Flight); }
	if (!X.bNeed || !SvcFor(TEXT("deice"), StandX)) return;
	if (D && D->Rounds) ADP_LOG(TEXT("GSE: %s is de-iced again: its holdover time is over"), *Flight); else ADP_LOG(TEXT("GSE: %s needs de-icing: %s"), *Flight, *X.Why);
	SvcAsk(TEXT("deice"), StandX, Reg, Flight, OffBlocks);
	if (bDoors) SvcGo(TEXT("deice"), StandX);
}

// ------------------------------------------------------------------ its parts

void AGSE::DeicerDress(FTractor& T)
{
	ABabylonModel* M = T.M.Get(); if (!M) return;
	T.Di = MakeShared<FSvcDeicer>(); FSvcDeicer& F = *T.Di;
	auto Pre = [&](const TCHAR* N, const FVector& P) { return M->BindGroup({N}, true, FTransform(P)); };
	const FVector O = FVector::ZeroVector;
	F.GTur = Pre(TEXT("Tur"), O); F.GBase = Pre(TEXT("BoomBase"), O); F.GSec[0] = Pre(TEXT("BoomSec2"), O); F.GSec[1] = Pre(TEXT("BoomSec3"), O);
	F.GCab = M->BindGroupWhere([](const FString& N, int32, const FVector&) { return N.StartsWith(TEXT("Cabin")) && !N.StartsWith(TEXT("CabinDoor")) && !N.StartsWith(TEXT("CabinWiper")); }, FTransform::Identity);
	F.GDoor = Pre(TEXT("CabinDoor"), O); F.GWip = Pre(TEXT("CabinWiper"), SvcP(T, TEXT("PIV_CabinWiper")));
	F.GJoy[0] = Pre(TEXT("Joy_L"), SvcP(T, TEXT("PIV_Joy_L"))); F.GJoy[1] = Pre(TEXT("Joy_R"), SvcP(T, TEXT("PIV_Joy_R"))); F.GSpray = Pre(TEXT("SprayPedal"), SvcP(T, TEXT("PIV_SprayPedal")));
	F.GPan = Pre(TEXT("NozPan"), SvcP(T, TEXT("PIV_NozPan"))); F.GTilt = Pre(TEXT("NozTilt"), SvcP(T, TEXT("PIV_NozTilt")));
	F.GBarrel = Pre(TEXT("LiftBarrel"), SvcP(T, TEXT("PIV_Lift"))); F.GRod = Pre(TEXT("LiftRod"), SvcP(T, TEXT("PIV_LiftEye")));
	for (int32 i = 0; i < 70; ++i) F.GChain.Add(M->BindGroup({FString::Printf(TEXT("ChainLink_%02d"), i)}, true, DiLink(i, 0.f)));
	static const TCHAR* HoseN[] = {TEXT("HoseLift"), TEXT("HoseTip"), TEXT("HoseNozzle")};
	for (int32 h = 0; h < 3; ++h)
	{
		F.GHose[h] = Pre(HoseN[h], SvcP(T, *FString::Printf(TEXT("IK_%s_A"), HoseN[h])));
		UCableComponent* C = NewObject<UCableComponent>(M, *(FString(HoseN[h]) + TEXT("Cable"))); C->SetupAttachment(M->GetRootComponent());
		C->CableWidth = 4.2f; C->NumSegments = 14; C->NumSides = 10; C->SolverIterations = 12; C->SubstepTime = 0.012f; C->bEnableCollision = false; C->bEnableStiffness = true; C->CableLength = 90.f;
		TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> G; M->InstancesNamed(HoseN[h], true, G); if (G.Num()) C->SetMaterial(0, G[0].Get<0>()->GetMaterial(0));
		C->SetVisibility(false); C->bAutoActivate = false; C->RegisterComponent(); M->AddInstanceComponent(C); C->SetActive(false); C->SetComponentTickEnabled(false); F.Hose[h] = C;
	}
	// the side panels, the valves, the caps, the hand hose
	auto One = [&](const TCHAR* N, const FVector& P) { const FString Nm(N), Pf = Nm + TEXT("_"); return M->BindGroupWhere([Nm, Pf](const FString& S, int32, const FVector&) { return S == Nm || S.StartsWith(Pf); }, FTransform(P)); };
	{
		static const TCHAR* Dn[] = {TEXT("PanelDoorL"), TEXT("PanelDoorR"), TEXT("HeaterDoor")}; static const TCHAR* Vn[] = {TEXT("Valve_W"), TEXT("Valve_I"), TEXT("Valve_IV"), TEXT("Valve_Hand")}; static const TCHAR* Cn[] = {TEXT("FillCap_W"), TEXT("FillCap_I"), TEXT("FillCap_IV")}; static const TCHAR* Gn[] = {TEXT("GaugeNeedle_TempI"), TEXT("GaugeNeedle_PressI"), TEXT("GaugeNeedle_PressIV"), TEXT("GaugeNeedle_Heater")};
		auto Pv = [&](const TCHAR* N) { return SvcP(T, *(FString(TEXT("PIV_")) + N)); };
		for (int32 i = 0; i < 3; ++i) { F.GPDoor[i] = One(Dn[i], Pv(Dn[i])); F.GCap[i] = One(Cn[i], Pv(Cn[i])); }
		for (int32 i = 0; i < 4; ++i) { F.GValve[i] = One(Vn[i], Pv(Vn[i])); F.GNeedle[i] = One(Gn[i], Pv(Gn[i])); }
		F.GHand = One(TEXT("HandNozzle"), Pv(TEXT("HandNozzle"))); F.GTail = One(TEXT("HoseTail"), SvcP(T, TEXT("IK_HoseOut")));
		UCableComponent* C = NewObject<UCableComponent>(M, TEXT("HandHoseCable")); C->SetupAttachment(M->GetRootComponent());
		C->CableWidth = 3.f; C->NumSegments = 24; C->NumSides = 8; C->SolverIterations = 12; C->SubstepTime = 0.012f; C->bEnableCollision = false; C->bEnableStiffness = true; C->CableLength = 90.f;
		TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> Hg; M->InstancesNamed(TEXT("HoseTail"), true, Hg); if (Hg.Num()) C->SetMaterial(0, Hg[0].Get<0>()->GetMaterial(0));
		C->SetVisibility(false); C->bAutoActivate = false; C->RegisterComponent(); M->AddInstanceComponent(C); C->SetActive(false); C->SetComponentTickEnabled(false); F.Line = C;
	}
	// the fluid: the wash hose's drops and spray, each type with its own colour in the stream
	auto Ism = [&](const TCHAR* Name, const TCHAR* Mesh, const TCHAR* Mat, bool bData)
	{
		UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(M, Name); K->SetupAttachment(M->GetRootComponent());
		K->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Mesh)); if (UMaterialInterface* Mi = LoadObject<UMaterialInterface>(nullptr, Mat)) K->SetMaterial(0, Mi); else ADP_WARN(TEXT("GSE: the de-icing truck's %s is missing"), Mat);
		K->SetCollisionEnabled(ECollisionEnabled::NoCollision); K->SetCastShadow(false); if (bData) K->SetNumCustomDataFloats(2);
		K->SetUsingAbsoluteLocation(true); K->SetUsingAbsoluteRotation(true); K->SetUsingAbsoluteScale(true); K->SetCanEverAffectNavigation(false); K->RegisterComponent(); K->SetWorldTransform(FTransform::Identity); M->AddInstanceComponent(K); return K;
	};
	const TCHAR* Ball = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	F.Stream = Ism(TEXT("DeiceJet"), Ball, TEXT("/Game/Truck/M_WaterDrop.M_WaterDrop"), true); F.Mist = Ism(TEXT("DeiceMist"), TEXT("/Engine/BasicShapes/Plane.Plane"), TEXT("/Game/Truck/M_WaterMist.M_WaterMist"), true);
	F.Tint[0] = Ism(TEXT("DeiceI"), Ball, TEXT("/Game/Babylon/Deicer/deicer/Materials/DI_FluidI.DI_FluidI"), false); F.Tint[1] = Ism(TEXT("DeiceIV"), Ball, TEXT("/Game/Babylon/Deicer/deicer/Materials/DI_FluidIV.DI_FluidIV"), false);
}

void AGSE::DeicerRestock(FTractor& T) { if (T.Di) { T.Di->Water = 2600.f; T.Di->TankI = T.Di->TankIV = 2400.f; T.Di->bBroken = false; } }
bool AGSE::DeicerStowed(const FTractor& T) const { return !T.Di || DiStowed(*T.Di); }
FTransform AGSE::DeicerCab(const FTractor& T) const { return (T.Di ? DiCab(T.Di->Now) : FTransform::Identity) * FTransform(FRotator(0.f, T.Yaw, 0.f), T.At); }

void AGSE::DeicerParts(FTractor& T, float Dt)
{
	ABabylonModel* M = T.M.Get(); if (!M || !T.Di) return;
	FSvcDeicer& F = *T.Di; const FTransform Root(FRotator(0.f, T.Yaw, 0.f), T.At);
	const float Dr = T.SvcDoors.IsValidIndex(DiCabin) ? T.SvcDoors[DiCabin].Now : 0.f; if (Dr > 0.f) F.bLive = true;
	DeicerPanel(T, Dt);
	DeicerYours(T, Dt);
	if (!F.bLive && F.Drops.Num() == 0) return;
	// along its route: one pose after the other, each axis at its own speed, easing in and out
	if (F.Route.Num() && !F.Halt)
	{
		const FSvcDeicer::FPose& G = F.Route[0];
		auto Step = [Dt](float& V, float& Vel, float To, float Max, float Acc)
		{
			const float D = To - V, Want = FMath::Sign(D) * FMath::Min(Max, FMath::Sqrt(2.f * Acc * FMath::Abs(D)) + 0.02f * Max); Vel = FMath::FInterpConstantTo(Vel, Want, Dt, Acc);
			const float S = Vel * Dt; if (D == 0.f || (FMath::Abs(S) >= FMath::Abs(D) && Vel * D >= 0.f)) { V = To; Vel = 0.f; } else V += S;
			return V == To;
		};
		const bool bS = Step(F.Now.Slew, F.SlewV, G.Slew, 11.f, 7.f), bL = Step(F.Now.Lift, F.LiftV, G.Lift, 6.f, 5.f), bE = Step(F.Now.Ext, F.ExtV, G.Ext, 55.f, 40.f);
		if (bS && bL && bE) F.Route.RemoveAt(0);
	}
	const FSvcDeicer::FPose Old = F.Was; F.Was = F.Now; const float Inv = 1.f / FMath::Max(Dt, 0.001f);
	const bool bMoved = F.Now.Slew != Old.Slew || F.Now.Lift != Old.Lift || F.Now.Ext != Old.Ext;
	const FTransform Tur = DiTur(F.Now), Base = DiBase(F.Now), Cab = DiCab(F.Now), Out3(FVector(-F.Now.Ext, 0.f, 0.f));
	if (F.bAiming) { if (DiAim(Cab * Root, F.AimAt, F.PanWant, F.TiltWant) > 900.f) F.PanWant = F.TiltWant = 0.f; }
	else if (F.Jet < 0 && !F.bYours) F.PanWant = F.TiltWant = 0.f;
	F.Pan = FMath::FInterpConstantTo(F.Pan, FMath::Clamp(F.PanWant, -58.f, 58.f), Dt, 30.f); F.Tilt = FMath::FInterpConstantTo(F.Tilt, FMath::Clamp(F.TiltWant, -35.f, 44.f), Dt, 24.f);
	F.Joy[0] = FMath::FInterpTo(F.Joy[0], FMath::Clamp((F.Now.Slew - Old.Slew) * Inv * 1.6f, -16.f, 16.f), Dt, 8.f); F.Joy[1] = FMath::FInterpTo(F.Joy[1], FMath::Clamp((F.Now.Lift - Old.Lift) * Inv * 2.6f, -16.f, 16.f), Dt, 8.f);
	auto Put = [M](int32 G, const FTransform& X) { if (G != INDEX_NONE) M->MoveGroup(G, X); };
	Put(F.GTur, Tur); Put(F.GBase, Base); Put(F.GSec[0], FTransform(FVector(-F.Now.Ext * 0.5f, 0.f, 0.f)) * Base); Put(F.GSec[1], Out3 * Base); Put(F.GCab, Cab);
	const FVector Foot = SvcP(T, TEXT("PIV_Lift")), Eye0 = SvcP(T, TEXT("PIV_LiftEye")), Eye = DiPivot + DiQ(FVector::RightVector, F.Now.Lift).RotateVector(Eye0 - DiPivot);
	const FQuat Ram = FQuat::FindBetweenVectors(Eye0 - Foot, Eye - Foot); Put(F.GBarrel, FTransform(Ram, Foot) * Tur); Put(F.GRod, FTransform(Ram, Eye) * Tur);
	if (bMoved) for (int32 i = 0; i < F.GChain.Num(); ++i) Put(F.GChain[i], DiLink(i, F.Now.Ext) * Base);
	Put(F.GDoor, FTransform(FVector(56.f * FMath::SmoothStep(0.f, 1.f, Dr), 0.f, 0.f)) * Cab);
	// (its wiper: in rain and while it sprays)
	if ((T.bRain && F.bOpIn) || F.bWipe || F.Jet >= 0 || F.WipT > 0.f) { F.WipT += Dt; if (F.WipT > 2.2f) F.WipT = 0.f; }
	Put(F.GWip, FTransform(DiQ(FVector::ForwardVector, F.WipT < 1.6f ? 50.f * (0.5f - 0.5f * FMath::Cos(2.f * PI * F.WipT / 1.6f)) : 0.f), SvcP(T, TEXT("PIV_CabinWiper"))) * Cab);
	for (int32 j = 0; j < 2; ++j) Put(F.GJoy[j], FTransform(DiQ(j ? FVector::RightVector : FVector::ForwardVector, F.Joy[j]), SvcP(T, j ? TEXT("PIV_Joy_R") : TEXT("PIV_Joy_L"))) * Cab);
	Put(F.GSpray, FTransform(DiQ(FVector::RightVector, F.Jet >= 0 ? 10.f : 0.f), SvcP(T, TEXT("PIV_SprayPedal"))) * Cab);
	const FVector PP = SvcP(T, TEXT("PIV_NozPan")), PT = SvcP(T, TEXT("PIV_NozTilt")); const FTransform PanX = DiAbout(DiQ(DiUp, F.Pan), PP) * Cab, TiltX = DiAbout(DiQ(FVector::RightVector, F.Tilt), PT) * PanX;
	Put(F.GPan, FTransform(PP) * PanX); Put(F.GTilt, FTransform(PT) * TiltX);
	// the slack hoses: modelled while it rests, hanging cables when it moves
	const bool bRest = DiStowed(F) && FMath::Abs(F.Pan) < 0.1f && FMath::Abs(F.Tilt) < 0.1f;
	const FVector A0[] = {SvcP(T, TEXT("IK_HoseLift_A")), SvcP(T, TEXT("IK_HoseTip_A")), SvcP(T, TEXT("IK_HoseNozzle_A"))};
	const FVector Ha[] = {Tur.TransformPosition(A0[0]), (Out3 * Base).TransformPosition(A0[1]), Cab.TransformPosition(A0[2])};
	const FVector Hb[] = {Base.TransformPosition(SvcP(T, TEXT("IK_HoseLift_B"))), Cab.TransformPosition(SvcP(T, TEXT("IK_HoseTip_B"))), TiltX.TransformPosition(SvcP(T, TEXT("IK_HoseNozzle_B")))};
	for (int32 h = 0; h < 3; ++h)
	{
		Put(F.GHose[h], FTransform(FQuat::Identity, A0[h], bRest ? FVector::OneVector : FVector(0.001f)));
		UCableComponent* C = F.Hose[h]; if (!C) continue;
		if (C->IsVisible() == bRest) { C->SetVisibility(!bRest); C->SetActive(!bRest); C->SetComponentTickEnabled(!bRest); }
		if (bRest) continue;
		const FVector P0 = Root.TransformPosition(Ha[h]), P1 = Root.TransformPosition(Hb[h]); C->SetWorldLocation(P0); C->EndLocation = C->GetComponentTransform().InverseTransformPosition(P1); C->CableLength = FMath::Max(float(FVector::Dist(P0, P1)) * 1.12f, h == 2 ? 40.f : 70.f);
	}
	// the boom's hydraulics (the belt loader's own recordings)
	if (bMoved)
	{
		const bool bUp = F.Now.Lift >= Old.Lift;
		if (F.Snd && F.bSndUp != bUp) { F.Snd->Stop(); F.Snd = nullptr; }
		if (!F.Snd) { const TCHAR* Nm = bUp ? TEXT("BLBoomUpLoop") : TEXT("BLBoomDownLoop"); F.bSndUp = bUp; if (USoundBase* Sd = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/BeltLoader/Sounds/%s.%s"), Nm, Nm))) F.Snd = UGameplayStatics::SpawnSoundAttached(Sd, M->GetRootComponent(), NAME_None, FVector(200.f, 0.f, 250.f), EAttachLocation::KeepRelativeOffset, false, 0.8f); }
	}
	else if (F.Snd) { F.Snd->Stop(); F.Snd = nullptr; Sound(TEXT("BLBoomStop"), T.At + DiUp * 250.f); }
	DeicerSpray(T, Dt, TiltX * Root);
	if (bRest && F.Ph == FSvcDeicer::Idle && !F.Route.Num() && F.Drops.Num() == 0 && !bMoved && !F.bYours && F.WipT <= 0.f && F.Jet < 0 && Dr <= 0.f) F.bLive = false;
}

void AGSE::DeicerSpray(FTractor& T, float Dt, const FTransform& Noz)
{
	FSvcDeicer& F = *T.Di; UWorld* W = GetWorld(); if (!F.Stream || !F.Mist || !F.Tint[0] || !F.Tint[1]) return;
	const FVector Dir = Noz.TransformVectorNoScale(FVector(-1.f, 0.f, 0.f));
	FVector Eye = T.At; if (APlayerController* PC = W->GetFirstPlayerController()) if (PC->PlayerCameraManager) Eye = PC->PlayerCameraManager->GetCameraLocation();
	const int32 Jk = F.bHandJet ? 0 : F.Jet;
	if (Jk >= 0)
	{
		// (water and type I mixed by the cold; an empty tank: nothing comes)
		const float Tc = APuddles::Get() ? APuddles::Get()->GetAirTemp() : 0.f, Mix = Tc > -3.f ? 0.45f : Tc > -10.f ? 0.55f : 0.75f; bool bDry = false;
		if (Jk == 0) { const float L = (F.bHandJet ? 0.7f : DiRateI) * Dt; bDry = F.TankI < L || F.Water < L; if (!bDry) { F.TankI -= L * Mix; F.Water -= L * (1.f - Mix); F.UsedI += L; } }
		else { const float L = DiRateIV * Dt; bDry = F.TankIV < L; if (!bDry) { F.TankIV -= L; F.UsedIV += L; } }
		if (bDry) { if (!F.bBroken) ADP_WARN(TEXT("GSE: the %s is out of type %s"), *T.Name, Jk ? TEXT("IV") : TEXT("I")); F.bBroken = true; F.Jet = -1; F.bHandJet = false; }
		else if (FVector::DistSquared(Eye, T.At) < FMath::Square(25000.0))
		{
			F.Emit = FMath::Min(F.Emit + Dt * 150.f, 20.f); const FVector At = F.bHandJet ? F.HandAt : Noz.TransformPosition(DiNozzle[Jk]), Jd = F.bHandJet ? F.HandDir : Dir; const float V = F.bHandJet ? 1200.f : Jk ? 1750.f : 2100.f;
			while (F.Emit >= 1.f) { F.Emit -= 1.f; FSvcDeicer::FDrop D; D.Kind = uint8(Jk); D.Q = (F.bHandJet ? 0.7f : Jk ? DiRateIV : DiRateI) / 150.f; D.V = FMath::VRandCone(Jd, FMath::DegreesToRadians(Jk ? 5.f : 3.f)) * V * FMath::FRandRange(0.93f, 1.04f); D.P = At + D.V * FMath::FRand() * Dt; D.Seed = FMath::FRand(); F.Drops.Add(D); }
		}
	}
	if (!F.Drops.Num()) { if (F.Stream->GetInstanceCount()) F.Stream->ClearInstances(); if (F.Mist->GetInstanceCount()) F.Mist->ClearInstances(); for (UInstancedStaticMeshComponent* K : F.Tint) if (K->GetInstanceCount()) K->ClearInstances(); return; }
	const bool bPl = F.Plane.IsValid() && F.Box.IsValid; TArray<FSvcDeicer::FDrop> New; TArray<FTransform> Jt, Tn[2], Ms; TArray<float> Jd, Md;
	for (FSvcDeicer::FDrop& D : F.Drops)
	{
		D.Life += Dt; D.V.Z -= 980.f * Dt * (D.Kind == 2 ? -0.02f : 0.35f + 0.65f * FMath::Min(1.f, D.Life)); D.V *= FMath::Max(0.f, 1.f - (D.Kind == 2 ? 2.6f : 0.4f) * Dt); D.P += D.V * Dt;
		if (D.Kind == 2)
		{
			const float K = D.Life / 1.3f; if (K >= 1.f) { D.Life = -1.f; continue; }
			const FVector To = (Eye - D.P).GetSafeNormal(); Ms.Add(FTransform(FQuat(To, D.Seed * 2.f * PI) * FRotationMatrix::MakeFromZ(To).ToQuat(), D.P, FVector(0.4f + 1.1f * FMath::Sqrt(K)))); Md.Add(D.Seed); Md.Add(0.45f * FMath::Square(1.f - K) * FMath::Min(1.f, D.Life * 10.f)); continue;
		}
		const bool bHit = bPl && DiDist(DiReal(F, D.P)) < 0.03;
		if (D.Life > 1.6f || D.P.Z < T.At.Z || bHit)
		{
			// (yours: the places near it get their share; cold type I doesn't count, type IV only over type I)
			if (bHit && D.Kind < 2 && T.Job == EJob::None && (D.Kind || F.TempI >= 60.f)) for (FSvcDeicer::FSpot& S : F.Spots) if ((D.Kind == 0 || S.L[0] >= 2.5f) && FVector::DistSquared(DiWorld(F, S.R), D.P) < FMath::Square(150.0)) S.L[D.Kind] += D.Q;
			// (mist where it lands; the hot type I steams more)
			if (FMath::FRand() < (D.Kind ? 0.14f : 0.3f)) { FSvcDeicer::FDrop N; N.Kind = 2; N.Seed = FMath::FRand(); N.P = D.P - D.V.GetSafeNormal() * 12.f; N.V = FVector(0.f, 0.f, 60.f) + FMath::VRand() * 80.f; New.Add(N); }
			D.Life = -1.f; continue;
		}
		const float Wd = (1.4f + D.Life * 5.f) / 100.f; const FRotator R = D.V.Rotation();
		Jt.Add(FTransform(R, D.P, FVector(0.28f, Wd, Wd))); Jd.Add(FMath::Clamp(0.35f + D.Life, 0.f, 0.85f)); Jd.Add(0.8f);
		if (D.Seed < 0.5f) Tn[D.Kind].Add(FTransform(R, D.P, FVector(0.20f, Wd * 0.45f, Wd * 0.45f)));
	}
	F.Drops.RemoveAll([](const FSvcDeicer::FDrop& D) { return D.Life < 0.f; }); F.Drops.Append(New);
	auto Show = [](UInstancedStaticMeshComponent* K, const TArray<FTransform>& X, const TArray<float>* D)
	{
		K->ClearInstances(); if (!X.Num()) return;
		K->AddInstances(X, false, true, false); if (D) for (int32 i = 0; i < X.Num(); ++i) K->SetCustomData(i, TArrayView<const float>(&(*D)[i * 2], 2), false); K->MarkRenderStateDirty();
	};
	Show(F.Stream, Jt, &Jd); Show(F.Mist, Ms, &Md); Show(F.Tint[0], Tn[0], nullptr); Show(F.Tint[1], Tn[1], nullptr);
}

// ------------------------------------------------------------------ its way round the aircraft

bool AGSE::DeicerLane(int32 Ti, float StandX, int32 Stop, FString& Why) const
{
	// its lane from the apron to that stop: nobody else's vehicle, no jetway, building or neighbour's wing in it
	float S = 0.f, N = 0.f, Sp = 36.f; if (!StandGeo(StandX, S, N, Sp)) return false;
	const FTractor& T = Tractors[Ti]; const FServiceSpec& Sv = ADPService::All()[T.Svc]; const FSvcDeicer::FStop& P = T.Di->Stops[Stop];
	const FVector A = Plan(S + P.Lat, N + Sv.Far), E = Plan(S + P.Lat, N + P.Aft - T.KHalfL / 100.f); const FVector D = (E - A).GetSafeNormal2D(), X(-D.Y, D.X, 0.f); const float L = float(FVector::Dist2D(A, E));
	TArray<FOutline> Os; Outlines(Os);
	for (const FOutline& O : Os)
	{
		if (FVector::Dist2D(O.C, T.At) < 60.f) continue;
		const float Al = float(FVector::DotProduct(O.C - A, D)), Sd = FMath::Abs(float(FVector::DotProduct(O.C - A, X))), Hx = FMath::Abs(float(FVector::DotProduct(O.X, X))) * O.Hx + FMath::Abs(float(FVector::DotProduct(O.Y, X))) * O.Hy;
		if (Al > -700.f && Al < L + O.Hx + 60.f && Sd < Hx + T.KHalfW + 40.f) { Why = TEXT("another vehicle is in its way"); return false; }
	}
	if (PlaneNear(Plan(S, N), 5000.f)) { Why = TEXT("an aircraft is moving there"); return false; }
	// (the neighbour: its wingtip)
	const FVector Mid = (A + E) * 0.5f;
	if (P.bWing)
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			for (const FName& Tg2 : It->Tags)
			{
				const FString Ts = Tg2.ToString(); if (!Ts.StartsWith(TEXT("SIM/")) && !Ts.StartsWith(TEXT("MOVE/"))) continue;
				if (*It == T.Di->Plane.Get() || FVector::Dist2D(It->GetActorLocation(), Mid) > 7000.f) continue;
				FBox B; DiBounds(*It, B); if (!B.IsValid) continue;
				const FVector Dl = It->GetActorLocation() - A; if (FMath::Abs(FVector::DotProduct(Dl, D)) > 8000.f) continue;
				if (FMath::Abs(float(FVector::DotProduct(Dl, X))) - float(B.GetSize().X) * 0.5f - 146.f < 45.f) { Why = TEXT("the next stand's aircraft is too near"); return false; }
			}
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPDeicerLane), false, T.M.Get()); if (T.Di->Plane.IsValid()) Q.AddIgnoredActor(T.Di->Plane.Get());
	FCollisionObjectQueryParams Ob; Ob.AddObjectTypesToQuery(ECC_WorldStatic); Ob.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<FOverlapResult> Hits; GetWorld()->OverlapMultiByObjectType(Hits, Mid + DiUp * 210.f, FRotationMatrix::MakeFromX(D).ToQuat(), Ob, FCollisionShape::MakeBox(FVector(L * 0.5f, 150.f, 110.f)), Q);
	for (const FOverlapResult& H : Hits) if (H.Component.IsValid() && FVector::Dist2D(H.Component->Bounds.Origin, T.At) > 620.f && !Cast<USkeletalMeshComponent>(H.Component.Get()) && H.Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision && H.Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block) { Why = FString::Printf(TEXT("%s stands in its lane"), H.GetActor() ? *H.GetActor()->GetName() : TEXT("something")); return false; }
	return true;
}

void AGSE::DeicerApron(int32 Job)
{
	// to the next stop's lane (the first of a lane), and in
	FSvcJob* J = SvcJ(Job); if (!J || !Tractors.IsValidIndex(J->Ti) || !Tractors[J->Ti].Di) return;
	const int32 Ti = J->Ti; FTractor& T = Tractors[Ti]; FSvcDeicer& F = *T.Di; const FServiceSpec& Sv = ADPService::All()[J->Svc];
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(J->StandX, S, N, Sp);
	if (F.Ph == FSvcDeicer::Idle)
	{
		const AActor* Pl = WetPlane(J->StandX);
		if (!Pl || !DiBounds(Pl, F.Box)) { ADP_WARN(TEXT("GSE: the %s does not go to %s: %s"), *T.Name, *J->Reg, Pl ? TEXT("not an A320's size") : TEXT("the aircraft is gone")); FDeiced& Dn = Deiced.FindOrAdd(J->Reg + TEXT("/") + J->Flight); Dn.Rounds = 3; J->Phase = 3; T.Job = EJob::Back; SvcHome(Job); return; }
		F.Plane = Pl; F.bLive = true; F.bBroken = false; F.Stops.Reset(); F.Stop = F.Done = F.Missed = F.Skipped = 0; F.UsedI = F.UsedIV = 0.f; F.Ph = FSvcDeicer::Wait; F.PhT = 0.f; J->Stage = 0;
		const FVector Sc = DiScale(F.Box); const float Ny = float(F.Box.Min.Y) / 100.f, Wing = FMath::Max(DiLane, float(F.Box.GetSize().X) / 200.f + 2.3f), Tl = DiTail * float(Sc.X);
		// (a stop: the truck's middle for its turret so far behind the nose)
		auto Add = [&](float Lat, float Xn, int8 Side, bool bW, bool bOut, uint8 Part) { FSvcDeicer::FStop P; P.Part = Part; P.Lat = Lat; P.Aft = Ny + Xn * float(Sc.Y) + 2.15f; P.Side = Side; P.bWing = bW; P.bOut = bOut; F.Stops.Add(P); };
		Add(Wing, 25.5f, -1, true, false, 0); Add(Wing, 13.5f, -1, true, true, 1);
		Add(Tl, 34.f, -1, false, false, 0); Add(Tl, 28.5f, -1, false, false, 1); Add(Tl, 25.8f, -1, false, true, 2);
		Add(-Tl, 34.f, 1, false, false, 0); Add(-Tl, 28.5f, 1, false, false, 1); Add(-Tl, 25.8f, 1, false, true, 2);
		Add(-Wing, 25.5f, 1, true, false, 0); Add(-Wing, 13.5f, 1, true, true, 1);
	}
	if (!F.Stops.IsValidIndex(F.Stop)) { DeicerEnd(Job); return; }
	T.Job = EJob::Out;
	const FSvcDeicer::FStop P = F.Stops[F.Stop]; TArray<FVector> W; const float Y1 = N + Sv.Far + 6.f;
	if (FVector::Dist2D(T.At, T.Home) < 200.f && FMath::Abs(FMath::FindDeltaAngleDegrees(T.Yaw, T.HomeYaw)) < 20.f) Leg(W, Sv.HomeX, Sv.HomeY, Sv.HomeHx, Sv.HomeHy, Sv.HomeX + Sv.HomeHx * 9.f, Sv.HomeY + Sv.HomeHy * 9.f, Sv.HomeHx, Sv.HomeHy, T.KTurnR);
	const FVector From = W.Num() ? W.Last() : T.At;
	if (!Apron(W, From, Plan(S + P.Lat, Y1), &T)) { RetryIn(Ti, 8.f, [this, Job] { DeicerApron(Job); }); return; }
	F.Ph = FSvcDeicer::Drive;
	Go(T, MoveTemp(W), false, [this, Job, Ti, P, Nose = Plan(S, N), Y1]
	{
		FSvcDeicer& F1 = *Tractors[Ti].Di; F1.Ph = FSvcDeicer::Wait; F1.PhT = 0.f; F1.LaneT = -10.f; F1.bLaneFree = false;
		// its turn: the doors shut, the jetway back, its lane free (waited for, then left out)
		Tractors[Ti].Until = [this, Job, Ti, P, Nose]
		{
			FSvcJob* X = SvcJ(Job); FSvcDeicer& F2 = *Tractors[Ti].Di; if (!X) return true;
			if (!F2.Plane.IsValid() || FVector::Dist2D(F2.Plane->GetActorLocation(), Nose) > 900.f || X->T > 7200.f) return true;
			if (!X->bGo) { F2.PhT = 0.f; F2.LaneT = -10.f; X->Why = TEXT("the doors are not shut yet"); return false; }
			if (F2.PhT - F2.LaneT < 3.f) return false;
			F2.LaneT = F2.PhT; FString Why; F2.bLaneFree = DeicerLane(Ti, X->StandX, F2.Stop, Why); X->Why = Why;
			return F2.bLaneFree || F2.PhT > (Why.StartsWith(TEXT("the next")) ? 300.f : Why.EndsWith(TEXT("in its lane")) ? 30.f : DiGiveUp);
		};
		Tractors[Ti].AfterUntil = [this, Job, Ti, P, Nose, Y1]
		{
			FSvcJob* X = SvcJ(Job); if (!X) return; FTractor& V = Tractors[Ti]; FSvcDeicer& F2 = *V.Di;
			if (!F2.Plane.IsValid() || FVector::Dist2D(F2.Plane->GetActorLocation(), Nose) > 900.f || !X->bGo) { ADP_WARN(TEXT("GSE: the %s gives up at %s: %s"), *V.Name, *X->Reg, X->bGo ? TEXT("the aircraft is gone") : TEXT("its doors were never shut")); F2.Plane.Reset(); DeicerEnd(Job); return; }
			if (!F2.bLaneFree)
			{
				// (this lane cannot be had)
				ADP_WARN(TEXT("GSE: the %s leaves out %s of %s: %s"), *V.Name, P.bWing ? (P.Side > 0 ? TEXT("the right wing") : TEXT("the left wing")) : TEXT("one side of the tail"), *X->Reg, *X->Why);
				do { ++F2.Skipped; ++F2.Stop; } while (F2.Stops.IsValidIndex(F2.Stop) && F2.Stops[F2.Stop].Lat == P.Lat);
				DeicerApron(Job); return;
			}
			X->Why.Reset(); DeicerIn(Job, Y1);
		};
	});
}

void AGSE::DeicerIn(int32 Job, float Y1)
{
	// down its lane to the stop, straight
	FSvcJob* J = SvcJ(Job); if (!J) return; const int32 Ti = J->Ti; FTractor& T = Tractors[Ti]; FSvcDeicer& F = *T.Di; const FSvcDeicer::FStop P = F.Stops[F.Stop];
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(J->StandX, S, N, Sp); J->Phase = 2; F.Ph = FSvcDeicer::Drive; T.Job = EJob::Out;
	TArray<FVector> W; if (Y1 > 0.f) Leg(W, S + P.Lat, Y1, 0.f, -1.f, S + P.Lat, N + P.Aft, 0.f, -1.f, T.KTurnR); else { const FVector E = Plan(S + P.Lat, N + P.Aft); for (int32 i = 1; i <= 24; ++i) W.Add(FMath::Lerp(T.At, E, i / 24.f)); }
	Go(T, MoveTemp(W), false, [this, Job, Ti]
	{
		FSvcJob* X = SvcJ(Job); if (!X) return; FTractor& V = Tractors[Ti]; V.Job = EJob::AtStand; X->Place = V.At; X->PlaceYaw = V.Yaw; V.Di->Ph = FSvcDeicer::OpIn; V.Di->PhT = 0.f;
		ADP_LOG(TEXT("GSE: the %s at stop %d of %d round %s"), *V.Name, V.Di->Stop + 1, V.Di->Stops.Num(), *X->Reg);
		if (!V.Di->bOpIn && !V.Di->bOpBusy) DeicerOpIn(Job);
	});
}

void AGSE::DeicerOut(int32 Job, bool bEnd)
{
	// backwards out of the lane, round beyond the tail
	FSvcJob* J = SvcJ(Job); if (!J) return; const int32 Ti = J->Ti; FTractor& T = Tractors[Ti]; const FServiceSpec& Sv = ADPService::All()[J->Svc];
	float S = 0.f, N = 0.f, Sp = 36.f, Yw = 0.f, Ya = T.Yaw; StandGeo(J->StandX, S, N, Sp); const float Lat = T.Di->Stops[FMath::Min(T.Di->Stop, T.Di->Stops.Num() - 1)].Lat;
	T.Di->Ph = FSvcDeicer::Drive; PlanPose(S + Lat, N + Sv.Far, 0.f, -1.f, Ya);
	const FVector A = Plan(S + Lat, N + Sv.Far), B = PlanPose(S + Lat + 8.f, N + Sv.Far + 8.f, -1.f, 0.f, Yw); TArray<FVector> R;
	for (int32 i = 1; i <= 40; ++i) R.Add(FMath::Lerp(T.At, A, i / 40.f));
	Dubins(R, A, Ya + 180.f, B, Yw + 180.f, T.KTurnR);
	if (!bEnd) T.Job = EJob::Out;
	Go(T, MoveTemp(R), true, [this, Job, Ti, bEnd, Lat]
	{
		FSvcDeicer& F = *Tractors[Ti].Di;
		if (bEnd) { F.Ph = FSvcDeicer::Idle; Tractors[Ti].Job = EJob::Back; SvcHome(Job); return; }
		while (F.Stops.IsValidIndex(F.Stop) && F.Stops[F.Stop].Lat == Lat) ++F.Stop;
		F.Ph = FSvcDeicer::Wait; DeicerApron(Job);
	});
}

void AGSE::DeicerNote(FTractor& T, const FString& Reg, const FString& Flight, bool bOk, const FString& How)
{
	// what was done to it, by its crew or by you: how long it holds. (No holdover in active frost below -10 C: looked at again 20 min later, our number)
	FSvcDeicer& F = *T.Di; const FDiWx& X = DiWeather(GetWorld()); const float Min = X.bNone ? 20.f : X.HoldMin;
	FDeiced& D = Deiced.FindOrAdd(Reg + TEXT("/") + Flight); ++D.Rounds; D.Until = DeiceNow + FTimespan::FromMinutes(bOk ? Min : 30.f);
	if (bOk) ADP_LOG(TEXT("GSE: %s de-iced %s (%s): %.0f l type I, %.0f l type IV, %s %.0f min; tanks %.0f / %.0f / %.0f l"), *Reg, *How, *X.Why, F.UsedI, F.UsedIV, X.bNone ? TEXT("no holdover, looked at again in") : TEXT("holds"), Min, F.Water, F.TankI, F.TankIV);
	else ADP_WARN(TEXT("GSE: %s is not de-iced: the truck could not get to it"), *Reg);
	if (F.Water < 1100.f || F.TankI < 900.f || F.TankIV < 700.f) T.bUsed = true;   // (filled up in its box)
}

void AGSE::DeicerEnd(int32 Job)
{
	// done (or given up): what it did is noted, the operator down and into the cab, and home
	FSvcJob* J = SvcJ(Job); if (!J) return; const int32 Ti = J->Ti; FTractor& T = Tractors[Ti]; FSvcDeicer& F = *T.Di;
	if (J->Stage == 0)
	{
		J->Stage = 1; DeicerNote(T, J->Reg, J->Flight, F.Done > 0, FString::Printf(TEXT("from %d of %d stops, %d places not reached"), F.Done, F.Stops.Num(), F.Missed));
	}
	F.bAiming = false; F.Jet = -1; F.Ph = FSvcDeicer::OpOut; F.PhT = 0.f;
}

void AGSE::DeicerGone(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J) return; FTractor& T = Tractors[J->Ti]; J->Phase = 3;
	if (T.Job == EJob::AtStand) { DeicerOut(Job, true); return; }
	T.Di->Ph = FSvcDeicer::Idle; T.Job = EJob::Back; SvcHome(Job);
}

// ------------------------------------------------------------------ the work at a stop

void AGSE::DeicerPlan(FSvcJob& J)
{
	FTractor& T = Tractors[J.Ti]; FSvcDeicer& F = *T.Di; const FSvcDeicer::FStop P = F.Stops[F.Stop]; const FTransform Root(FRotator(0.f, T.Yaw, 0.f), T.At);
	const bool bSnow = DiWeather(GetWorld()).bSnow; const double Sg = P.Side; F.Aims.Reset(); F.Aim = 0; F.Pass = 0; F.Dwell = 0.f;
	TArray<FDiWant> Want; DiWants(Want, P.Part, P.bWing, Sg, bSnow);
	FSvcDeicer::FPose Last; Last.Slew = P.Side * 70.f; Last.Lift = 45.f; Last.Ext = 200.f;
	for (const FDiWant& Q : Want)
	{
		FSvcDeicer::FAim A; A.At = DiWorld(F, Q.R);
		if (!DiSolve(F, Root, P.Side, Q.R, Q.bTop, Last, A.Q, GetWorld(), T.M.Get())) { ++F.Missed; continue; }
		Last = A.Q; F.Aims.Add(A);
	}
	ADP_LOG(TEXT("GSE: de-icing stop %d: %d of %d places reached"), F.Stop + 1, F.Aims.Num(), Want.Num());
}

void AGSE::DeicerTick(int32 Job, float Dt)
{
	FSvcJob* Jp = SvcJ(Job); if (!Jp || !Tractors.IsValidIndex(Jp->Ti) || !Tractors[Jp->Ti].Di || Jp->Phase == 0 || Jp->Phase == 9) return;
	FSvcJob& J = *Jp; FTractor& T = Tractors[J.Ti]; FSvcDeicer& F = *T.Di; F.PhT += Dt; const FTransform Root(FRotator(0.f, T.Yaw, 0.f), T.At);
	if (F.Ph == FSvcDeicer::Idle) return;
	F.bLive = true; const bool bDriver = J.Men.Num() && J.Men[0].bSeated;
	auto Stow = [&] { F.Jet = -1; F.bAiming = false; F.Dwell = 0.f; F.Ph = FSvcDeicer::Stow; F.PhT = 0.f; bool bD = false; FSvcDeicer::FPose Up; Up.Lift = 60.f; if (F.Plane.IsValid() && F.Now.Slew != 0.f && DiRoute(F, Root, F.Now, Up, F.Route, bD)) F.Route.Add({0.f, 0.f, 0.f}); else DiHome(F, F.Route); };
	switch (F.Ph)
	{
	case FSvcDeicer::OpIn:
		if (T.Job != EJob::AtStand) return;
		if (F.bOpIn && bDriver && T.SvcDoors[DiCabin].Now < 0.01f && F.PhT > 1.f && F.Plane.IsValid()) { DeicerPlan(J); F.Ph = FSvcDeicer::Work; F.PhT = 0.f; }
		else if (F.PhT > 420.f || !F.Plane.IsValid()) { ADP_WARN(TEXT("GSE: the %s's operator is not up in his cabin - it goes home"), *T.Name); F.Ph = FSvcDeicer::Leave; Later([this, Job] { DeicerEnd(Job); }); }
		return;
	case FSvcDeicer::Work:
	{
		if (!F.bOpIn || F.bBroken || F.PhT > 900.f || !F.Plane.IsValid()) { Stow(); return; }
		if (!F.Aims.IsValidIndex(F.Aim))
		{
			F.Jet = -1;
			if (F.Aims.Num() && F.Pass == 0 && DiWeather(GetWorld()).bFalls) { F.Pass = 1; F.Aim = 0; F.Dwell = 0.f; return; }
			if (F.Aims.Num()) ++F.Done;
			Stow(); return;
		}
		const FSvcDeicer::FAim& A = F.Aims[F.Aim];
		if (F.Dwell <= 0.f)
		{
			// (on to it: the jet stays on when the boom goes straight there)
			F.Dwell = 0.001f; bool bD = false;
			const bool bAt = F.Now.Slew == A.Q.Slew && F.Now.Lift == A.Q.Lift && F.Now.Ext == A.Q.Ext;
			if (!bAt && !DiRoute(F, Root, F.Now, A.Q, F.Route, bD)) { ++F.Missed; ++F.Aim; F.Dwell = 0.f; F.Jet = -1; return; }
			if (!bAt && !bD) F.Jet = -1;
		}
		const bool bHop = F.Route.Num() > 1; F.bAiming = !bHop;
		F.AimAt = F.AimAt.IsZero() || FVector::DistSquared(F.AimAt, A.At) > FMath::Square(900.0) ? A.At : FMath::VInterpConstantTo(F.AimAt, A.At, Dt, 130.f);
		float Pn, Tl; const float D = DiAim(DiCab(F.Now) * Root, F.AimAt, Pn, Tl); const bool bOn = !bHop && D > 150.f && D < 850.f && FMath::Abs(Pn - F.Pan) < 7.f && FMath::Abs(Tl - F.Tilt) < 7.f;
		F.Jet = bOn ? F.Pass : -1;
		if (!F.Route.Num() && bOn && FVector::DistSquared(F.AimAt, A.At) < 400.0) { F.Dwell += Dt; if (F.Dwell > (F.Pass ? 2.f : 3.5f)) { ++F.Aim; F.Dwell = 0.f; } }
		else if (!F.Route.Num()) { F.Dwell += Dt * 0.15f; if (F.Dwell > 3.5f) { ++F.Missed; ++F.Aim; F.Dwell = 0.f; } }   // (it cannot be aimed at: the next one)
		return;
	}
	case FSvcDeicer::Stow:
		if (F.Route.Num()) return;
		if (!DiStowed(F)) { DiHome(F, F.Route); return; }
		if (!bDriver) return;
		if (F.bBroken || !F.Plane.IsValid()) { F.Ph = FSvcDeicer::Leave; Later([this, Job] { DeicerEnd(Job); }); return; }
		if (!F.bOpIn) { F.Ph = FSvcDeicer::OpIn; F.PhT = 0.f; return; }   // (somebody else is coming up: the stop again)
		if (F.Stop + 1 >= F.Stops.Num()) { F.Ph = FSvcDeicer::Leave; Later([this, Job] { DeicerEnd(Job); }); return; }
		F.Ph = FSvcDeicer::Drive;
		if (F.Stops[F.Stop].bOut) { Later([this, Job] { DeicerOut(Job, false); }); return; }
		{
			FString Why;
			if (!DeicerLane(J.Ti, J.StandX, F.Stop + 1, Why)) { ADP_WARN(TEXT("GSE: the %s leaves out a stop at %s: %s"), *T.Name, *J.Reg, *Why); ++F.Skipped; Later([this, Job] { DeicerOut(Job, false); }); return; }
		}
		++F.Stop; Later([this, Job] { DeicerIn(Job, 0.f); });
		return;
	case FSvcDeicer::OpOut:
		if (F.bOpIn && !F.bOpBusy) { DeicerOpOut(Job); return; }
		if (F.bOpIn || F.bOpBusy || !bDriver || !DiStowed(F)) return;
		if (!SvcAll(Job, [](const FSvcMan& X) { return X.bGone || X.bSeated; }) || T.SvcDoors[0].Now > 0.01f || T.SvcDoors[DiCabin].Now > 0.01f) return;
		F.Ph = FSvcDeicer::Leave; Later([this, Job] { DeicerGone(Job); });
		return;
	default: return;
	}
}

// ------------------------------------------------------------------ its operator

TFunction<void(float, float, FDriverPose&)> AGSE::DeicerClimbFn(int32 Ti, bool bUp) const
{
	// up the rear left steps with a hand on each rail, through the cabin's door, round and down onto its seat (only while it is stowed)
	return [this, Ti, bUp](float K0, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti]; const float K = bUp ? K0 : 1.f - K0; const FVector Fc = FRotator(0.f, T.Yaw + 90.f, 0.f).Vector(), Bk = FRotator(0.f, T.Yaw + 180.f, 0.f).Vector();
		const FVector S0 = SvcP(T, TEXT("IK_Step_Deck_0")), S1 = SvcP(T, TEXT("IK_Step_Deck_1")), S2 = SvcP(T, TEXT("IK_Step_Deck_2")), GF = SvcP(T, TEXT("IK_Grab_DeckSteps_F")), GR = SvcP(T, TEXT("IK_Grab_DeckSteps_R")), Lf(10.f, 0.f, 0.f), Sill = SvcP(T, TEXT("IK_CabinStep")), In = SvcP(T, TEXT("IK_Cabin_In"));
		auto Key = [&](int32 i)
		{
			if (i == 0) return ADPPose::Standing(TW(T, SvcP(T, TEXT("IK_Stand_DeckSteps"))), Fc, PH);
			if (i == 5) { FDriverPose R = ADPPose::Standing(TW(T, In), Bk, PH - 8.f); R.Lean = 8.f; return R; }
			if (i >= 6) { FDriverPose R; DeicerSeatFn(Ti)(1.f, PH, R); return R; }
			FDriverPose Q; Q.Facing = Fc; Q.Lean = 8.f; Q.bHand[0] = Q.bHand[1] = true; Q.Grip[0] = Q.Grip[1] = 0.9f; const float Hz = FMath::Min(22.f * i, 60.f); Q.Hand[0] = TW(T, GF + DiUp * Hz); Q.Hand[1] = TW(T, GR + DiUp * Hz);
			if (i == 1) { Q.Pelvis = TW(T, FVector(S0.X, S0.Y - 26.f, PH + 4.f)); Q.Foot[0] = TW(T, S0 + Lf); Q.Foot[1] = TW(T, FVector(S0.X, S0.Y - 40.f, 0.f) - Lf); }
			else if (i == 2) { Q.Pelvis = TW(T, FVector(S0.X, S0.Y - 8.f, S0.Z + PH - 4.f)); Q.Foot[0] = TW(T, S0 + Lf); Q.Foot[1] = TW(T, S1 - Lf); }
			else if (i == 3) { Q.Pelvis = TW(T, FVector(S0.X, S1.Y + 8.f, S1.Z + PH - 4.f)); Q.Foot[0] = TW(T, S2 + Lf); Q.Foot[1] = TW(T, S1 - Lf); }
			else { Q.Pelvis = TW(T, FVector(S2.X, S2.Y + 6.f, S2.Z + PH - 3.f)); Q.Foot[0] = TW(T, S2 + Lf); Q.Foot[1] = TW(T, Sill - Lf); Q.Hand[1] = TW(T, SvcP(T, TEXT("IK_Grab_Cabin"))); Q.bHand[0] = false; Q.Lean = 16.f; }
			return Q;
		};
		const float X = K * 6.f; const int32 i = FMath::Clamp(FMath::FloorToInt(X), 0, 5); P = ADPPose::Lerp(Key(i), Key(i + 1), FMath::SmoothStep(0.f, 1.f, X - i));
	};
}

TFunction<void(float, float, FDriverPose&)> AGSE::DeicerSeatFn(int32 Ti) const
{
	// on the cabin's seat, looking out; both hands on the joysticks, the right foot on the spray pedal
	return [this, Ti](float, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti]; const FTransform X = DeicerCab(T); const FVector H = SvcP(T, TEXT("IK_Seat_CabinSeat")); const FSvcDeicer* F = T.Di.Get(); if (!F) return;
		P = FDriverPose(); P.Facing = X.TransformVectorNoScale(FVector(-1.f, 0.f, 0.f)); P.Pelvis = X.TransformPosition(H + DiUp * 10.f); P.Lean = -4.f;
		// (seated looking back: the truck's right side is on his left)
		for (int32 s = 0; s < 2; ++s)
		{
			const int32 j = s == 0 ? 1 : 0; P.Foot[s] = X.TransformPosition(SvcP(T, j ? TEXT("IK_Foot_Cabin_R") : TEXT("IK_Foot_Cabin_L")));
			const FVector Pv = SvcP(T, j ? TEXT("PIV_Joy_R") : TEXT("PIV_Joy_L")), Tp = SvcP(T, j ? TEXT("IK_Joy_R") : TEXT("IK_Joy_L"));
			P.bHand[s] = true; P.Grip[s] = 0.75f; P.Hand[s] = X.TransformPosition(Pv + DiQ(j ? FVector::RightVector : FVector::ForwardVector, F->Joy[j]).RotateVector(Tp - Pv) + FVector(5.f, 0.f, 2.f));
		}
	};
}

void AGSE::DeicerOpIn(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J || J->Men.Num() < 2 || !Tractors.IsValidIndex(J->Ti)) return;
	const int32 Ci = J->Men[1].Ci, Ti = J->Ti; Tractors[Ti].Di->bOpBusy = true;
	if (J->Men[1].bSeated) SvcAlight(Job, 1);
	WalkTo(Ci, [this, Ti] { return TW(Tractors[Ti], SvcP(Tractors[Ti], TEXT("IK_Stand_DeckSteps"))); }, 15.f);
	WaitFor(Ci, [this, Ti](float) { return DeicerStowed(Tractors[Ti]); });
	Do(Ci, [this, Ti] { FTractor& T = Tractors[Ti]; T.Di->bLive = true; T.SvcDoors[DiCabin].Want = 1.f; Sound(*T.SndDoorOpen, TW(T, T.SvcDoors[DiCabin].Piv)); });
	WaitFor(Ci, [this, Ti](float E) { return E > 5.f || Tractors[Ti].SvcDoors[DiCabin].Now > 0.99f; });
	PoseAct(Ci, DeicerClimbFn(Ti, true), 9.f);
	Do(Ci, [this, Job, Ci, Ti]
	{
		if (ATugs* G = Tg()) G->Crew[Ci].Hold = DeicerSeatFn(Ti);
		FTractor& T = Tractors[Ti]; T.Di->bOpIn = true; T.Di->bOpBusy = false; T.SvcDoors[DiCabin].Want = 0.f; if (FSvcMan* X = SvcM(Job, 1)) { X->bOut = false; X->bSeated = false; }
	});
}

void AGSE::DeicerOpOut(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J || J->Men.Num() < 2 || !Tractors.IsValidIndex(J->Ti)) return;
	const int32 Ci = J->Men[1].Ci, Ti = J->Ti; Tractors[Ti].Di->bOpBusy = true;
	WaitFor(Ci, [this, Ti](float) { return DeicerStowed(Tractors[Ti]) && Tractors[Ti].MeasSpeed < 0.05f; });
	Do(Ci, [this, Ti] { FTractor& T = Tractors[Ti]; T.Di->bLive = true; T.SvcDoors[DiCabin].Want = 1.f; Sound(*T.SndDoorOpen, TW(T, T.SvcDoors[DiCabin].Piv)); });
	WaitFor(Ci, [this, Ti](float E) { return E > 5.f || Tractors[Ti].SvcDoors[DiCabin].Now > 0.99f; });
	Do(Ci, [this, Ci] { if (ATugs* G = Tg()) G->Crew[Ci].Hold = nullptr; });
	PoseAct(Ci, DeicerClimbFn(Ti, false), 8.f);
	Do(Ci, [this, Job, Ti] { FTractor& T = Tractors[Ti]; T.Di->bOpIn = T.Di->bOpBusy = false; T.SvcDoors[DiCabin].Want = 0.f; if (FSvcMan* X = SvcM(Job, 1)) X->bOut = true; });
	SvcBoard(Job, 1);
}

void AGSE::DeicerLost(int32 Job, int32 m)
{
	// somebody of them vanished: the boom is stowed (the tick), whoever takes over goes on from this stop
	FSvcJob* J = SvcJ(Job); ATugs* G = Tg(); if (!J || !G || !Tractors.IsValidIndex(J->Ti) || !Tractors[J->Ti].Di) return;
	const int32 Ci = J->Men[m].Ci, Ti = J->Ti; ATugs::FCrew& C = G->Crew[Ci]; FSvcDeicer& F = *Tractors[Ti].Di; C.bUp = false; J->Men[m].bSeated = false; C.Q.Reset(); C.Hold = nullptr;
	if (m == 0)
	{
		ClimbIn(Ci, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) X->bSeated = true; });
		if (J->Phase == 1 && Tractors[Ti].Job == EJob::Boarding) Do(Ci, [this, Job, Ti] { Motor(Tractors[Ti], true); Later([this, Job] { SvcApron(Job); }); });
		return;
	}
	F.bOpIn = F.bOpBusy = false; F.Jet = -1; F.bAiming = false; F.PhT = 0.f;
	if (J->Phase == 2 && F.Ph != FSvcDeicer::OpOut && F.Ph != FSvcDeicer::Leave && Tractors[Ti].Job == EJob::AtStand) Do(Ci, [this, Job] { Later([this, Job] { DeicerOpIn(Job); }); }); else SvcBoard(Job, 1);
	Wait(Ci, 0.4f);
}

// ------------------------------------------------------------------ you in its cabin


// ------------------------------------------------------------------ by hand (to-do 238): the panel, the hand hose, the work that counts

namespace
{
	enum : int32 { DsMDoor = 0, DsMValve = 3, DsMCap = 7 };                                        // (motions: L, R, heater; water, I, IV, hand; W, I, IV)
	enum : int32 { DsPumpOn = 1, DsPumpOff, DsHeatOn, DsHeatOff, DsLight, DsStopPanel, DsStopCab, DsHand, DsLamp, DsWipe, DsHorn };
	enum : int32 { DsFRoot, DsFCab, DsFDoorL, DsFDoorR, DsFHeater };
	const TCHAR* const DsDoor[] = {TEXT("PanelDoorL"), TEXT("PanelDoorR"), TEXT("HeaterDoor")};
	const TCHAR* const DsValve[] = {TEXT("Valve_W"), TEXT("Valve_I"), TEXT("Valve_IV"), TEXT("Valve_Hand")};
	const TCHAR* const DsCap[] = {TEXT("FillCap_W"), TEXT("FillCap_I"), TEXT("FillCap_IV")};
	const float DsHot = 60.f, DsLI = 2.5f, DsLIV = 1.f, DsPart = 0.85f;                           // type I counts from 60 C; litres a place needs; the share of the places that counts as done (ours)
	FTransform DsHinge(const AGSE::FTractor& T, int32 i, const FVector& P) { return FTransform(DiQ(FVector::UpVector, (i == 1 ? 110.f : -110.f) * T.Di->DN[i]), P); }
	bool DsOpen(const FDi& F, int32 i) { return F.VN[i] < 45.f; }
	// what keeps a fluid from coming (null: nothing); 0 type I, 1 type IV, 2 the hand hose
	const TCHAR* DsDry(const AGSE::FTractor& T, int32 Jet)
	{
		const FDi& F = *T.Di;
		if (F.Halt) return TEXT("an emergency stop is pressed"); if (!T.bPto) return TEXT("the power take-off is off: its lever is in the driver's cab"); if (!F.bPump) return TEXT("the pump is off: its button is on the side panel");
		if (Jet == 1) return DsOpen(F, 2) ? F.TankIV < 2.f ? TEXT("the type IV tank is empty") : nullptr : TEXT("the type IV valve is shut");
		if (!DsOpen(F, 0)) return TEXT("the water valve is shut"); if (!DsOpen(F, 1)) return TEXT("the type I valve is shut");
		if (Jet == 2 && !DsOpen(F, 3)) return TEXT("the hand hose's valve is shut");
		return F.TankI < 2.f || F.Water < 2.f ? TEXT("the type I or the water tank is empty") : nullptr;
	}
	bool DsDone(const FDi::FSpot& S, bool bIV) { return S.L[0] >= DsLI && (!bIV || S.L[1] >= DsLIV); }
}

bool AGSE::DeicerShut(const FTractor& T) const
{
	if (!T.Di) return true; const FSvcDeicer& F = *T.Di; if (!DiStowed(F) || F.Halt || F.Hand || !F.bPump || !F.bHeat) return false;
	for (const float V : F.DN) if (V > 0.01f) return false; for (const float V : F.VN) if (V > 1.f) return false; for (const float V : F.CN) if (V > 1.f) return false;
	return true;
}

/** You work with it at that aircraft (until two minutes after you left it, 40 m away): its crew wait. */
bool AGSE::DeicerYou(float StandX) const
{
	for (const FTractor& T : Tractors) if (T.Di && T.Job == EJob::None && T.Di->YouStandX > -1e5f && FMath::Abs(T.Di->YouStandX - StandX) < 1.f && T.Di->LeftT < 120.f) return true;
	return false;
}

/** The aircraft it stands at by you, the places of it to treat, and when enough of them are: the flight is de-iced, as by its crew. */
void AGSE::DeicerNear(FTractor& T, float Dt)
{
	FSvcDeicer& F = *T.Di; const APlayerController* PC = GetWorld()->GetFirstPlayerController(); const APawn* Me = PC ? PC->GetPawn() : nullptr;
	F.LeftT = !T.Player.IsValid() && !F.Hand && (!Me || FVector::Dist2D(Me->GetActorLocation(), T.At) > 4000.0) ? F.LeftT + Dt : 0.f;
	if ((F.NearT += Dt) < 1.f) return;
	F.NearT = 0.f;
	if (!T.Player.IsValid() && FVector::Dist2D(T.At, T.Home) < 200.f) { F.YouStandX = -1e9f; F.Spots.Reset(); F.Plane.Reset(); return; }
	double Bd = 4500.0; const AActor* Best = nullptr; FBox Bb(ForceInit);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It) for (const FName& N : It->Tags) { const FString Ts = N.ToString(); if (!Ts.StartsWith(TEXT("SIM/")) && !Ts.StartsWith(TEXT("MOVE/"))) continue; FBox B; const double Ds = FVector::Dist2D(It->GetActorLocation(), T.At); if (Ds < Bd && DiBounds(*It, B)) { Bd = Ds; Best = *It; Bb = B; } }
	if (!Best) { F.Plane.Reset(); F.Spots.Reset(); F.YouStandX = -1e9f; return; }
	if (F.Plane.Get() != Best) { F.Plane = Best; F.Box = Bb; F.Spots.Reset(); }
	F.YouStandX = -1e9f; const FVector Nose = DiWorld(F, FVector::ZeroVector);
	for (const auto& St : AAirfield::Shared().Stands) if (FVector::Dist2D(Plan(St.X, St.Nose), Nose) < 900.0) F.YouStandX = St.X;
	const FDiWx& X = DiWeather(GetWorld());
	if (F.Spots.Num() && F.SpotEnd < DeiceNow) { AVehicleControls::Note(GetWorld(), TEXT("What you sprayed no longer holds: from the start again")); F.Spots.Reset(); }
	if (!F.Spots.Num())
	{
		F.Told = 0; F.SpotEnd = FDateTime::MaxValue();
		for (const double Sg : {1.0, -1.0}) for (int32 P = 0; P < 5; ++P) { TArray<FDiWant> W; DiWants(W, P < 2 ? P : P == 2 ? 2 : P - 3, P < 2, Sg, X.bSnow); for (const FDiWant& Q : W) { FSvcDeicer::FSpot& S = F.Spots.AddDefaulted_GetRef(); S.R = Q.R; } }
		return;
	}
	int32 Ok = 0; for (const FSvcDeicer::FSpot& S : F.Spots) Ok += DsDone(S, X.bFalls) ? 1 : 0;
	if (!Ok) return;
	if (F.SpotEnd == FDateTime::MaxValue()) F.SpotEnd = DeiceNow + FTimespan::FromMinutes(30.0);
	const int32 Pc = FMath::FloorToInt32(100.f * Ok / F.Spots.Num()); const bool bAll = Pc >= FMath::RoundToInt32(100.f * DsPart);
	if (Pc / 10 > F.Told && !bAll) { F.Told = Pc / 10; AVehicleControls::Note(GetWorld(), FString::Printf(TEXT("%d %% of the aircraft treated%s"), Pc, X.bFalls ? TEXT(" (type I, then type IV)") : TEXT(""))); }
	if (!bAll || F.YouStandX < -1e5f) return;
	for (TPair<int32, FSvcJob>& P : SvcJobs)
	{
		FSvcJob& J = P.Value; if (J.Phase != 0 || !ADPService::All()[J.Svc].bDeicer || FMath::Abs(J.StandX - F.YouStandX) > 1.f) continue;
		if (!J.bGo) { if (F.Told < 11) { F.Told = 11; AVehicleControls::Note(GetWorld(), TEXT("All of it treated: it counts once the aircraft's doors are shut")); } return; }
		DeicerNote(T, J.Reg, J.Flight, true, TEXT("by you"));
		AVehicleControls::Note(GetWorld(), FString::Printf(TEXT("%s is de-iced"), J.Flight.IsEmpty() ? *J.Reg : *J.Flight));
		J.Phase = 9; F.Spots.Reset(); F.UsedI = F.UsedIV = 0.f; return;
	}
}

void AGSE::DeicerPanel(FTractor& T, float Dt)
{
	ABabylonModel* M = T.M.Get(); FSvcDeicer& F = *T.Di; if (!M) return;
	if (T.Job == EJob::None) DeicerNear(T, Dt); else { F.Spots.Reset(); F.YouStandX = -1e9f; F.bHandJet = false; }
	const float Tc = APuddles::Get() ? APuddles::Get()->GetAirTemp() : 0.f;
	F.TempI = F.bHeat && !F.Halt ? FMath::Min(82.f, F.TempI + Dt * 3.f / 60.f) : FMath::Max(Tc, F.TempI - Dt * 0.1f / 60.f);
	auto Put = [M](int32 G, const FTransform& X) { if (G != INDEX_NONE) M->MoveGroup(G, X); };
	auto Go = [Dt](float& N, float W, float V) { if (N == W) return false; N = FMath::FInterpConstantTo(N, W, Dt, V); return true; };
	for (int32 i = 0; i < 3; ++i)
	{
		if (Go(F.DN[i], F.DW[i], 0.75f)) Put(F.GPDoor[i], DsHinge(T, i, SvcP(T, *(FString(TEXT("PIV_")) + DsDoor[i]))));
		if (Go(F.CN[i], F.CW[i], 90.f)) Put(F.GCap[i], FTransform(DiQ(FVector::RightVector, F.CN[i]), SvcP(T, *(FString(TEXT("PIV_")) + DsCap[i]))));
	}
	for (int32 i = 0; i < 4; ++i) if (Go(F.VN[i], F.VW[i], 110.f)) Put(F.GValve[i], FTransform(DiQ(FVector::RightVector, F.VN[i]), SvcP(T, *(FString(TEXT("PIV_")) + DsValve[i]))));
	// the gauges, while somebody can see them
	if (F.DN[0] > 0.02f || F.DN[2] > 0.02f)
	{
		const bool bRun = T.bPto && F.bPump && !F.Halt; const float V[] = {F.TempI, bRun && DsOpen(F, 0) && DsOpen(F, 1) ? F.Jet == 0 || F.bHandJet ? 72.f : 90.f : 0.f, bRun && DsOpen(F, 2) ? F.Jet == 1 ? 50.f : 66.f : 0.f, F.bHeat && !F.Halt ? 82.f : F.TempI};
		const float R0[] = {82.f, 0.f, 0.f, 82.f}, Sc[] = {100.f, 120.f, 120.f, 120.f}; static const TCHAR* Nd[] = {TEXT("PIV_GaugeNeedle_TempI"), TEXT("PIV_GaugeNeedle_PressI"), TEXT("PIV_GaugeNeedle_PressIV"), TEXT("PIV_GaugeNeedle_Heater")};
		for (int32 i = 0; i < 4; ++i) { const float O = F.Gauge[i]; F.Gauge[i] = FMath::FInterpTo(O, FMath::Max(0.f, V[i]), Dt, 2.5f); if (FMath::Abs(O - F.Gauge[i]) > 0.005f) Put(F.GNeedle[i], FTransform(DiQ(FVector::RightVector, 270.f * (F.Gauge[i] - R0[i]) / Sc[i]), SvcP(T, Nd[i]))); }
	}
	// the hand nozzle: in your hand it points where you look, its hose behind it
	if (F.Hand || F.bHandOut)
	{
		const FVector P0 = SvcP(T, TEXT("PIV_HandNozzle")); FVector Pos = P0; FQuat Q = FQuat::Identity; const FTransform Root(FRotator(0.f, T.Yaw, 0.f), T.At);
		if (F.Hand)
		{
			FVector Dir = FVector(0.f, SvcP(T, TEXT("PIV_HandNozzle")).Y, 0.f).GetSafeNormal();
			if (const APlayerController* PC = GetWorld()->GetFirstPlayerController()) { FVector Ey; FRotator Lk; PC->GetPlayerViewPoint(Ey, Lk); Dir = Root.InverseTransformVectorNoScale(Lk.Vector()); }
			Q = FRotationMatrix::MakeFromX(-Dir).ToQuat(); Pos = T.YouHand - Q.RotateVector(SvcP(T, TEXT("IK_HandNozzle_Hand")) - P0);
			F.HandDir = Root.TransformVectorNoScale(Dir); F.HandAt = Root.TransformPosition(Pos + Q.RotateVector(SvcP(T, TEXT("IK_HandNozzle_Mouth")) - P0));
		}
		else F.bHandJet = false;
		F.bHandOut = F.Hand != 0; Put(F.GHand, FTransform(Q, Pos)); Put(F.GTail, FTransform(FQuat::Identity, SvcP(T, TEXT("IK_HoseOut")), F.Hand ? FVector(0.001f) : FVector::OneVector));
		if (UCableComponent* C = F.Line)
		{
			if (C->IsVisible() != F.bHandOut) { C->SetVisibility(F.bHandOut); C->SetActive(F.bHandOut); C->SetComponentTickEnabled(F.bHandOut); }
			const FVector A = Root.TransformPosition(SvcP(T, TEXT("IK_HoseOut"))), B = Root.TransformPosition(Pos + Q.RotateVector(SvcP(T, TEXT("IK_HandNozzle_Hose")) - P0)); C->SetWorldLocation(A); C->EndLocation = C->GetComponentTransform().InverseTransformPosition(B); C->CableLength = float(FVector::Dist(A, B)) * 1.08f + 30.f;
		}
		if (F.bHandJet && DsDry(T, 2)) F.bHandJet = false;
	}
	if (F.bHandJet || F.bWipe) F.bLive = true;
}

TSharedPtr<FVehTable> AGSE::DeicerControls()
{
	using K = FVehControl; TSharedPtr<FVehTable> Tb = MakeShared<FVehTable>(); TArray<K>& Cs = Tb->Controls;
	auto Mo = [&Cs](const TCHAR* Part, const TCHAR* Pt, const TCHAR* Lb, int32 Fr, const FVector& Out, int32 M, float B) { K X; X.Part = Part; X.Point = Pt; X.Kind = K::Hand; X.Label = Lb; X.Frame = Fr; X.Out = Out; X.Motion = M; X.A = 0.f; X.B = B; X.bGrip = true; Cs.Add(X); };
	auto Sw = [&Cs](const TCHAR* Part, const TCHAR* Pt, const TCHAR* Lb, int32 Fr, const FVector& Out, int32 S, bool bCab) { K X; X.Part = Part; X.Point = Pt; X.Kind = K::Push; X.Label = Lb; X.Frame = Fr; X.Out = Out; X.Switch = S; X.bSeat = X.bCabin = bCab; Cs.Add(X); };
	const FVector L(0.f, -1.f, 0.f), R(0.f, 1.f, 0.f), U(0.f, 0.f, 0.f);
	Mo(DsDoor[0], TEXT("IK_Handle_PanelDoorL"), TEXT("Pump panel's door"), DsFDoorL, U, DsMDoor, 1.f);
	Mo(DsDoor[1], TEXT("IK_Handle_PanelDoorR"), TEXT("Hose reel's door"), DsFDoorR, U, DsMDoor + 1, 1.f);
	Mo(DsDoor[2], TEXT("IK_Handle_HeaterDoor"), TEXT("Heater's door"), DsFHeater, U, DsMDoor + 2, 1.f);
	Mo(DsValve[0], TEXT("IK_Valve_W"), TEXT("Water valve"), DsFRoot, L, DsMValve, 90.f); Mo(DsValve[1], TEXT("IK_Valve_I"), TEXT("Type I valve"), DsFRoot, L, DsMValve + 1, 90.f);
	Mo(DsValve[2], TEXT("IK_Valve_IV"), TEXT("Type IV valve"), DsFRoot, L, DsMValve + 2, 90.f); Mo(DsValve[3], TEXT("IK_Valve_Hand"), TEXT("Hand hose's valve"), DsFRoot, U, DsMValve + 3, 90.f);
	Mo(DsCap[0], TEXT("IK_FillCap_W"), TEXT("Water tank's cap"), DsFRoot, U, DsMCap, 100.f); Mo(DsCap[1], TEXT("IK_FillCap_I"), TEXT("Type I tank's cap"), DsFRoot, U, DsMCap + 1, 100.f); Mo(DsCap[2], TEXT("IK_FillCap_IV"), TEXT("Type IV tank's cap"), DsFRoot, U, DsMCap + 2, 100.f);
	// (the panel's and the cabin's buttons are one piece with their plates: only your hand goes to them)
	Sw(TEXT(""), TEXT("IK_Button_PumpOn"), TEXT("Pump on"), DsFRoot, L, DsPumpOn, false); Sw(TEXT(""), TEXT("IK_Button_PumpOff"), TEXT("Pump off"), DsFRoot, L, DsPumpOff, false);
	Sw(TEXT(""), TEXT("IK_Button_HeatOn"), TEXT("Heater on"), DsFRoot, L, DsHeatOn, false); Sw(TEXT(""), TEXT("IK_Button_HeatOff"), TEXT("Heater off"), DsFRoot, L, DsHeatOff, false);
	Sw(TEXT(""), TEXT("IK_Button_Light"), TEXT("Work lamps"), DsFRoot, L, DsLight, false); Sw(TEXT(""), TEXT("IK_Button_EStop"), TEXT("Emergency stop"), DsFRoot, L, DsStopPanel, false);
	Sw(TEXT("HandNozzle"), TEXT("IK_HandNozzle_Hand"), TEXT("Take the hand nozzle"), DsFRoot, U, DsHand, false);
	Sw(TEXT(""), TEXT("IK_Cabin_Btn0"), TEXT("Work lamps"), DsFCab, U, DsLamp, true); Sw(TEXT(""), TEXT("IK_Cabin_Btn1"), TEXT("Cabin wiper"), DsFCab, U, DsWipe, true); Sw(TEXT(""), TEXT("IK_Cabin_Btn2"), TEXT("Horn"), DsFCab, U, DsHorn, true); Sw(TEXT(""), TEXT("IK_Cabin_EStop"), TEXT("Emergency stop"), DsFCab, U, DsStopCab, true);
	float HL = 400.f, HW = 125.f; for (const FServiceSpec& S : ADPService::All()) if (S.bDeicer) { HL = S.HalfL; HW = S.HalfW; }
	Tb->Solids.Add({FVector(-HL, -HW, 0.f), FVector(HL, HW, 320.f), DsFRoot, nullptr});
	for (int32 i = 0; i < 3; ++i) { const float Xa = i == 2 ? 160.f : 62.f, Xb = i == 2 ? 264.f : 132.f, Y = i == 1 ? 117.f : -121.f; Tb->Solids.Add({FVector(Xa, Y, 112.f), FVector(Xb, Y + 4.f, 212.f), DsFDoorL + i, [i](const FTractor& T) { return T.Di && T.Di->DN[i] > 0.02f; }}); }
	Tb->Frame = [](const AGSE& G, const FTractor& T, int32 Fr) -> FTransform
	{
		if (!T.Di || Fr == DsFRoot) return FTransform::Identity; if (Fr == DsFCab) return DiCab(T.Di->Now);
		const int32 i = Fr - DsFDoorL; const FVector P = G.SvcP(T, *(FString(TEXT("PIV_")) + DsDoor[i])); return FTransform(FQuat::Identity, -P) * DsHinge(T, i, P);
	};
	Tb->Now = [](const FTractor& T, int32 M) { const FDi& F = *T.Di; return M < DsMValve ? F.DN[M] : M < DsMCap ? F.VN[M - DsMValve] : F.CN[M - DsMCap]; };
	Tb->Want = [](FTractor& T, int32 M, float To) { FDi& F = *T.Di; (M < DsMValve ? F.DW[M] : M < DsMCap ? F.VW[M - DsMValve] : F.CW[M - DsMCap]) = To; };
	Tb->May = [](const AGSE&, const FTractor& T, int32 M, float To, FString& Why)
	{
		if (!T.Di) return false; if (T.Speed > 1.f || T.Path.Num()) { Why = TEXT("it is not parked"); return false; }
		if (M == 1 && To < T.Di->DN[1] && T.Di->Hand) { Why = TEXT("the hand nozzle is out"); return false; }
		return true;
	};
	Tb->Say = [](const AGSE&, const FTractor& T, const K& X) -> FString
	{
		if (!T.Di) return FString(); const FDi& F = *T.Di;
		if (X.Motion >= DsMValve && X.Motion < DsMCap) { const int32 v = X.Motion - DsMValve; if (F.DN[v == 3 ? 1 : 0] < 0.9f && (v < 3 || F.DN[0] < 0.9f)) return FString(); return FString(X.Label) + (DsOpen(F, v) ? TEXT(" (open)") : TEXT(" (shut)")); }
		if (X.Motion != INDEX_NONE) return X.Label;
		if (X.Switch >= DsPumpOn && X.Switch <= DsStopPanel && F.DN[0] < 0.9f) return FString();
		if (X.Switch == DsStopPanel || X.Switch == DsStopCab) return F.Halt & (X.Switch == DsStopPanel ? 1 : 2) ? TEXT("Pull the emergency stop out") : X.Label;
		if (X.Switch == DsHand) return F.DN[1] < 0.9f ? FString() : FString(F.Hand ? TEXT("Put the hand nozzle back") : X.Label);
		if (X.Switch == DsPumpOn || X.Switch == DsPumpOff) return FString::Printf(TEXT("%s (it is %s; water %.0f l, type I %.0f l, type IV %.0f l)"), X.Label, F.bPump ? TEXT("on") : TEXT("off"), F.Water, F.TankI, F.TankIV);
		if (X.Switch == DsHeatOn || X.Switch == DsHeatOff) return FString::Printf(TEXT("%s (it is %s; type I %.0f C)"), X.Label, F.bHeat ? TEXT("on") : TEXT("off"), F.TempI);
		return X.Label;
	};
	Tb->MaySwitch = [](AGSE& G, int32 Ti, int32 S, FString& Why)
	{
		const FTractor& T = G.Tractors[Ti]; if (S != DsHand || !T.Di) return true;
		if (T.Speed > 1.f || T.Path.Num() || T.Player.IsValid()) { Why = TEXT("it is not parked"); return false; }
		return true;
	};
	Tb->Switch = [](AGSE& G, int32 Ti, int32 S)
	{
		FTractor& T = G.Tractors[Ti]; if (!T.Di || S < DsPumpOn || S > DsHorn) return false; FDi& F = *T.Di;
		switch (S)
		{
		case DsPumpOn: if (F.Halt) AVehicleControls::Note(G.GetWorld(), TEXT("An emergency stop is pressed")); else F.bPump = true; break;
		case DsPumpOff: F.bPump = false; break;
		case DsHeatOn: if (F.Halt) AVehicleControls::Note(G.GetWorld(), TEXT("An emergency stop is pressed")); else F.bHeat = true; break;
		case DsHeatOff: F.bHeat = false; break;
		case DsLight: case DsLamp: T.bSwWork = !T.bSwWork; break;
		// (one circuit: boom, pump and heater stand still; pulled out again, pump and heater have to be switched on)
		case DsStopPanel: F.Halt ^= 1; if (F.Halt) F.bPump = F.bHeat = false; break;
		case DsStopCab: F.Halt ^= 2; if (F.Halt) F.bPump = F.bHeat = false; break;
		case DsHand: F.Hand = F.Hand ? 0 : 1; F.bHandOut = true; F.bHandJet = false; break;
		case DsWipe: F.bWipe = !F.bWipe; break;
		case DsHorn: G.Sound(TEXT("BTHorn"), G.TW(T, FVector(T.KCentreX - T.KHalfL, 0.f, 200.f))); break;   // (the electric tractor's: no other one yet)
		default: break;
		}
		return true;
	};
	Tb->Held = [](const FTractor& T) -> const TCHAR* { return T.Di && T.Di->Hand ? TEXT("the hand nozzle") : nullptr; };
	Tb->Leash = [](const AGSE& G, const FTractor& T, FVector& From) { From = G.SvcP(T, TEXT("IK_HoseOut")); return 1700.f; };
	Tb->Drop = [](FTractor& T) { if (T.Di) { T.Di->Hand = 0; T.Di->bHandOut = true; T.Di->bHandJet = false; } };
	Tb->HandSay = [](const AGSE&, const FTractor& T, FString& Not) -> FString { if (!T.Di || !T.Di->Hand) return FString(); if (const TCHAR* Dr = DsDry(T, 2)) Not = Dr; return TEXT("Spray type I while you hold it"); };
	Tb->HandDo = [](AGSE&, FTractor& T, bool bHeld) { if (T.Di) T.Di->bHandJet = bHeld && T.Di->Hand && !DsDry(T, 2); };
	Tb->Stowed = [](const AGSE& G, const FTractor& T) { return G.DeicerShut(T); };
	Tb->Stow = [](AGSE&, FTractor& T)
	{
		if (!T.Di) return; FDi& F = *T.Di; F.Halt = 0; F.bPump = F.bHeat = true; F.bWipe = F.bHandJet = false; F.Hand = 0; F.bHandOut = true; F.bLive = true;
		for (float& V : F.DW) V = 0.f; for (float& V : F.VW) V = 0.f; for (float& V : F.CW) V = 0.f;
		if (!F.Route.Num() && !DiStowed(F)) DiHome(F, F.Route);
	};
	return Tb;
}

void AGSE::DeicerYours(FTractor& T, float Dt)
{
	FSvcDeicer& F = *T.Di; const AServiceDrive* D = T.PlayerSeat == 2 ? T.Player.Get() : nullptr;
	if (!D || !D->Seated())
	{
		if (F.bYours) { F.bYours = false; F.Jet = -1; F.SlewV = F.LiftV = F.ExtV = 0.f; }
		// (left standing up: after two minutes without you it stows itself - DeicerControls)
		return;
	}
	F.bLive = true; const FTransform Root(FRotator(0.f, T.Yaw, 0.f), T.At);
	F.bYours = true;   // (the aircraft it stands at: DeicerNear)
	// lift and slew by the move keys / left stick, the boom out and in by Shift / Ctrl or the triggers; never into anything, never round below 15 degrees
	FSvcDeicer::FPose N = F.Now; auto Vel = [Dt](float& V, float In, float Max, float Acc) { V = FMath::FInterpConstantTo(V, In * Max, Dt, Acc); return V * Dt; };
	const float Pw = T.bPto && !F.Halt && T.Speed < 1.f && !T.Path.Num() ? 1.f : 0.f;   // (the boom and the pumps need the power take-off; home it always goes)
	if (!T.bPto && (FMath::Abs(D->BoomLift()) > 0.2f || FMath::Abs(D->BoomSlew()) > 0.2f || FMath::Abs(D->BoomOut()) > 0.2f || D->SprayKey() >= 0)) AVehicleControls::Note(GetWorld(), TEXT("The power take-off is off: its lever is in the driver's cab"));
	if (F.Halt && (FMath::Abs(D->BoomLift()) > 0.2f || FMath::Abs(D->BoomSlew()) > 0.2f || FMath::Abs(D->BoomOut()) > 0.2f)) AVehicleControls::Note(GetWorld(), TEXT("An emergency stop is pressed"));
	if (!F.Route.Num())
	{
		N.Lift = FMath::Clamp(N.Lift + Vel(F.LiftV, Pw * D->BoomLift(), 6.f, 8.f), N.Slew == 0.f && N.Ext <= 0.f ? 0.f : 15.f, 60.f);
		N.Slew = FMath::Clamp(N.Slew + Vel(F.SlewV, N.Lift >= 15.f ? Pw * D->BoomSlew() : 0.f, 11.f, 12.f), -150.f, 150.f);
		N.Ext = FMath::Clamp(N.Ext + Vel(F.ExtV, N.Lift >= 15.f ? Pw * D->BoomOut() : 0.f, 55.f, 60.f), 0.f, 600.f);
		if (FMath::Abs(N.Slew) < 0.4f && D->BoomSlew() == 0.f) N.Slew = 0.f;
		if (N.Slew != F.Now.Slew || N.Lift != F.Now.Lift || N.Ext != F.Now.Ext)
		{
			bool bOk = true;
			if (F.Plane.IsValid()) { const double C1 = DiClear(F, Root, N); bOk = C1 >= 0.0 || C1 > DiClear(F, Root, F.Now); }
			FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPDeicerYou), false, T.M.Get()); if (F.Plane.IsValid()) Q.AddIgnoredActor(F.Plane.Get()); FCollisionObjectQueryParams Ob; Ob.AddObjectTypesToQuery(ECC_WorldStatic);
			if (bOk && N.Lift > 15.f && GetWorld()->OverlapAnyTestByObjectType((DiCab(N) * Root).TransformPosition(FVector(-400.f, 0.f, 200.f)), FQuat::Identity, Ob, FCollisionShape::MakeSphere(140.f), Q)) bOk = false;
			if (bOk) F.Now = N; else F.SlewV = F.LiftV = F.ExtV = 0.f;
		}
	}
	// the nozzle where you look, type I / type IV while held
	F.PanWant = D->LookY(); F.TiltWant = D->LookP(); int32 Jt = D->SprayKey(); if (Jt >= 0) if (const TCHAR* Dr = DsDry(T, Jt)) { AVehicleControls::Note(GetWorld(), FString(TEXT("Nothing comes: ")) + Dr); Jt = -1; } F.Jet = F.Now.Lift >= 15.f && !F.Route.Num() ? Jt : -1; if (Jt < 0) F.bBroken = false;
	if (F.Water < 1100.f || F.TankI < 900.f || F.TankIV < 700.f) T.bUsed = true;
}
