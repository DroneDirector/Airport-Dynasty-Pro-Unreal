// The talk view (TalkCamera.h).
#include "TalkCamera.h"
#include "ADPWalker.h"
#include "ADPLog.h"
#include "People.h"
#include "PeopleLayers.h"
#include "GroomComponent.h"
#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"

namespace ADPTalkCam
{
namespace
{
	int32 GTalkCam = 1;
	FAutoConsoleVariableRef CTalkCam(TEXT("adp.TalkCam"), GTalkCam, TEXT("The camera glides in on the person you talk to: 1 on, 0 off."));

	constexpr float GlideIn = 0.9f, GlideOut = 0.7f;
	constexpr float FaceSide = 20.f;            // (degrees off the face, towards you)
	constexpr float FaceFill = 0.75f;           // (of the free height: the head's top to the collarbones)
	constexpr float MinVert = 24.f, MaxVert = 44.f;   // (the view's angle up and down)
	constexpr float LensBall = 9.f;             // (the camera as a ball, cm)

	enum class EPhase : uint8 { None, Wait, On, Out };

	/** Where they are: the eyes' middle, the face's way, the head's top and the collarbones. */
	struct FFace { FVector Eye = FVector::ZeroVector; float Yaw = 0.f, Top = 0.f, Low = 0.f; };
	/** A place for the camera: degrees round the face from its front, height over the eyes, distance. */
	struct FSpot { float Side = 0.f, Up = 0.f, Dist = 85.f; };

	struct FTalk
	{
		EPhase Phase = EPhase::None;
		TWeakObjectPtr<AActor> Who, NextWho; TWeakObjectPtr<USkeletalMeshComponent> Body, Face, NextBody;
		FString Name, NextName;
		TWeakObjectPtr<APlayerController> PC;
		TArray<TWeakObjectPtr<AActor>> Hid; bool bHid = false;
		FFace F; FSpot At, Want;
		float Crown = 18.f, Hair = 3.f;
		float A = 0.f, Vel = 0.f, A0 = 0.f, V0 = 0.f, A1 = 0.f, CT = 0.f, CDur = 1.f;   // (the glide)
		int32 WaitFrames = 0; float CheckIn = 0.f, BlockedFor = 0.f, SizeIn = 0.f, Held = 0.f;
		uint64 LastFrame = 0; FVector LastBase = FVector::ZeroVector, LastCam = FVector::ZeroVector; bool bLast = false;
	} GT;

	// the menu's panel, as the HUD last drew it
	float GPanelScale = 1.f; FVector2D GPanelSize(604.0, 320.0);

	FVector2f ScreenPx()
	{
		FVector2f Px(1920.f, 1080.f);
		if (GEngine && GEngine->GameViewport) { FVector2D S(0, 0); GEngine->GameViewport->GetViewportSize(S); if (S.X > 8 && S.Y > 8) Px = FVector2f(S); }
		return Px;
	}

	struct FLayout { float Nx = 0.f, Ny = 0.f, FreeH = 0.8f, FreeW = 1.f, Shift = 0.f; };

	/** The face to the left, the panel beside it, both about the screen's middle however wide it is; the subtitle's room under. */
	FLayout MakeLayout()
	{
		FLayout L; const FVector2f Px = ScreenPx(); const float S = FMath::Max(GPanelScale, 0.1f), Vw = Px.X, Vh = Px.Y;
		const float Top = 0.02f * Vh, Bottom = FMath::Max(Vh - 185.f * S, Vh * 0.6f), Zone = Bottom - Top;   // (the subtitle: 120 up, two lines)
		const float Pw = FMath::Min(float(GPanelSize.X) * S, Vw * 0.6f), Gap = 28.f * S, Edge = 24.f * S, Fw = Zone * 0.9f, All = Fw + Gap + Pw;
		float Fc, Pc;
		if (All + 2.f * Edge <= Vw) { const float L0 = (Vw - All) * 0.5f; Fc = L0 + Fw * 0.5f; Pc = L0 + Fw + Gap + Pw * 0.5f; L.FreeW = Fw / Vh; }
		else { const float Room = FMath::Max(Vw - Edge - Pw - Gap, 0.2f * Vw); Fc = Room * 0.5f; Pc = Vw - Edge - Pw * 0.5f; L.FreeW = Room / Vh; }
		L.Nx = 2.f * Fc / Vw - 1.f; L.Ny = 1.f - (Top + Bottom) / Vh; L.FreeH = Zone / Vh; L.Shift = (Pc - Vw * 0.5f) / S;
		return L;
	}

	void StartGlide(float To, float Time) { GT.A0 = GT.A; GT.V0 = GT.Vel; GT.A1 = To; GT.CT = 0.f; GT.CDur = FMath::Max(Time, 0.01f); }
	/** Smooth at both ends, and from whatever speed it has when it turns round half-way. */
	void StepGlide(float Dt)
	{
		const float Was = GT.A; GT.CT += Dt;
		const float U = FMath::Clamp(GT.CT / GT.CDur, 0.f, 1.f), U2 = U * U, U3 = U2 * U;
		GT.A = FMath::Clamp((2.f * U3 - 3.f * U2 + 1.f) * GT.A0 + (U3 - 2.f * U2 + U) * GT.CDur * GT.V0 + (3.f * U2 - 2.f * U3) * GT.A1, 0.f, 1.f);
		GT.Vel = U >= 1.f ? 0.f : (GT.A - Was) / Dt;
	}

	void HideMe(AADPWalker* Me, bool bOn)
	{
		if (bOn == GT.bHid) return;
		GT.bHid = bOn;
		APlayerController* PC = GT.PC.Get();
		if (!bOn) { if (PC) for (const TWeakObjectPtr<AActor>& A : GT.Hid) if (A.IsValid()) PC->HiddenActors.Remove(A.Get()); GT.Hid.Reset(); return; }
		if (!PC || !Me) return;
		// (hidden from this view only: nothing about your body itself changes)
		TArray<AActor*> All; Me->GetAttachedActors(All, true, true); All.AddUnique(Me);
		if (AActor* H = Me->HumanActor()) { TArray<AActor*> More; H->GetAttachedActors(More, true, true); All.AddUnique(H); for (AActor* M : More) All.AddUnique(M); }
		for (AActor* A : All) if (A && !PC->HiddenActors.Contains(A)) { PC->HiddenActors.Add(A); GT.Hid.Add(A); }
	}

	/** What the portrait lens needs whatever the player's quality is: lent while you talk, given back after, never saved. */
	struct FLent { const TCHAR* Name; float Least; FString Was, Is; bool bHeld = false; };
	FLent GLent[] = {{TEXT("r.DepthOfFieldQuality"), 2.f}, {TEXT("r.DOF.Kernel.MaxBackgroundRadius"), 0.02f}, {TEXT("r.DOF.Kernel.MaxForegroundRadius"), 0.012f}, {TEXT("r.DOF.Gather.RingCount"), 4.f}, {TEXT("r.DOF.Recombine.Quality"), 1.f}};

	void SetLent(IConsoleVariable* V, const FString& To)
	{
		// (with the say it has already: whoever sets it next still can)
		V->Set(*To, EConsoleVariableFlags(FMath::Max(uint32(V->GetFlags()) & uint32(ECVF_SetByMask), uint32(ECVF_SetByScalability))));
	}
	void LendLens()
	{
		for (FLent& L : GLent)
		{
			if (L.bHeld) continue;
			IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(L.Name);
			if (!V || V->GetFloat() >= L.Least - 0.0001f) continue;
			L.Was = V->GetString(); SetLent(V, FString::SanitizeFloat(L.Least, 0)); L.Is = V->GetString(); L.bHeld = true;
		}
	}
	void ReturnLens()
	{
		for (FLent& L : GLent)
		{
			if (!L.bHeld) continue;
			L.bHeld = false;
			// (changed by the player meanwhile: theirs stays)
			if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(L.Name)) if (V->GetString() == L.Is && L.Is != L.Was) SetLent(V, L.Was);
		}
	}

	/** The portrait: as open as keeps 10 cm either side of the middle of the head sharp, whatever the distance and the screen. */
	void Portrait(FMinimalViewInfo& V, float Aspect, const FVector& At, float VertDeg)
	{
		FPostProcessSettings& P = V.PostProcessSettings;
		if (V.PostProcessBlendWeight <= 0.f) { P = FPostProcessSettings(); V.PostProcessBlendWeight = 1.f; }
		// the eyes' middle and the shoulders are 4 cm either side of this
		const FVector Mid = GT.F.Eye - FRotator(0.f, GT.F.Yaw, 0.f).Vector() * 4.0;
		const float D = FMath::Max(float(FVector::Dist(At, Mid)), 30.f), Open = 0.0028f * 2.f * D * FMath::Tan(FMath::DegreesToRadians(VertDeg * 0.5f)) * (D + 10.f) / 10.f;   // (the lens opening, cm)
		// a full-frame picture, 24 mm high whatever the screen's shape
		const float Tv = FMath::Tan(FMath::DegreesToRadians(V.FOV * 0.5f)) / Aspect, Focal = 1.2f / FMath::Max(Tv, 0.01f);
		const float Stop = FMath::Clamp(Focal / FMath::Max(Open * GT.A, 0.01f), 1.f, 32.f);
		P.bOverride_DepthOfFieldSensorWidth = true; P.DepthOfFieldSensorWidth = FMath::Clamp(24.f * Aspect, 10.f, 200.f);
		P.bOverride_DepthOfFieldFstop = true; P.DepthOfFieldFstop = Stop;
		P.bOverride_DepthOfFieldMinFstop = true; P.DepthOfFieldMinFstop = Stop;   // (wide open: round)
		P.bOverride_DepthOfFieldBladeCount = true; P.DepthOfFieldBladeCount = 9;
		P.bOverride_DepthOfFieldSqueezeFactor = true; P.DepthOfFieldSqueezeFactor = 1.f;
		// (no blur by distance from the camera: it would soften the face too)
		P.bOverride_DepthOfFieldDepthBlurRadius = true; P.DepthOfFieldDepthBlurRadius = 0.f;
		P.bOverride_DepthOfFieldFocalDistance = true; P.DepthOfFieldFocalDistance = FMath::Max(float((Mid - V.Location) | V.Rotation.Vector()), 20.f);
		// (the edges a very little darker; 0.4 is what the game has)
		P.bOverride_VignetteIntensity = true; P.VignetteIntensity = FMath::Lerp(0.4f, 0.55f, GT.A);
	}

	void AllOver()
	{
		HideMe(nullptr, false); ReturnLens();
		const TWeakObjectPtr<AActor> N = GT.NextWho; const TWeakObjectPtr<USkeletalMeshComponent> B = GT.NextBody; const FString Nm = GT.NextName;
		GT = FTalk();
		if (N.IsValid() && B.IsValid()) Begin(N.Get(), B.Get(), Nm);
	}

	/** How high the head's top is over the head bone (the face's own mesh as made), and what hair or a cap adds. */
	void MeasureHead()
	{
		AActor* Who = GT.Who.Get(); USkeletalMeshComponent* Body = GT.Body.Get(); if (!Who || !Body) return;
		GT.Face = nullptr;
		TInlineComponentArray<UPrimitiveComponent*> All(Who);
		for (UPrimitiveComponent* P : All) if (USkeletalMeshComponent* S = Cast<USkeletalMeshComponent>(P)) if (S->GetName().StartsWith(TEXT("Face")) && S->IsRegistered() && S->GetSkeletalMeshAsset()) GT.Face = S;
		GT.Crown = 18.f;
		if (USkeletalMeshComponent* Fc = GT.Face.Get())
		{
			const USkeletalMesh* M = Fc->GetSkeletalMeshAsset(); const FReferenceSkeleton& R = M->GetRefSkeleton(); int32 B = R.FindBoneIndex(TEXT("head"));
			if (B != INDEX_NONE)
			{
				FTransform X = FTransform::Identity; for (; B != INDEX_NONE; B = R.GetParentIndex(B)) X = X * R.GetRefBonePose()[B];
				const FBoxSphereBounds Bd = M->GetImportedBounds();
				GT.Crown = FMath::Clamp(float((Bd.Origin.Z + Bd.BoxExtent.Z - X.GetLocation().Z) * Fc->GetComponentScale().Z), 14.f, 22.f);
			}
		}
		const FVector Head = Body->GetSocketLocation(TEXT("head")); const double Z = Head.Z + GT.Crown;
		float Hair = 2.f;
		for (UPrimitiveComponent* P : All)
		{
			if (!P->IsRegistered() || !P->IsVisible()) continue;
			FString N = P->GetName(); const UStaticMeshComponent* St = Cast<UStaticMeshComponent>(P); const USkeletalMeshComponent* Sk = Cast<USkeletalMeshComponent>(P);
			if (St && St->GetStaticMesh()) N += St->GetStaticMesh()->GetName(); else if (Sk && Sk->GetSkeletalMeshAsset()) N += Sk->GetSkeletalMeshAsset()->GetName();
			bool bHat = false;
			if (St || Sk) for (const TCHAR* H : {TEXT("Helmet"), TEXT("Hat"), TEXT("Cap"), TEXT("Beret"), TEXT("Turban"), TEXT("Hijab"), TEXT("Headset")}) bHat |= N.Contains(H, ESearchCase::CaseSensitive);
			const float Over = float(P->Bounds.Origin.Z + P->Bounds.BoxExtent.Z - Z); const bool bNear = FVector::Dist2D(P->Bounds.Origin, Head) < 35.0;
			if (P->IsA<UGroomComponent>() && bNear) Hair = FMath::Max(Hair, FMath::Clamp(Over, 0.f, 8.f));
			else if (bHat) Hair = FMath::Max(Hair, St && bNear ? FMath::Clamp(Over, 0.f, 14.f) : 7.f);
		}
		GT.Hair = Hair;
	}

	bool MeasureFace(FFace& O)
	{
		USkeletalMeshComponent* B = GT.Body.Get(); if (!B || !GT.Who.IsValid() || B->GetBoneIndex(TEXT("head")) == INDEX_NONE) return false;
		const FVector Head = B->GetSocketLocation(TEXT("head"));
		FVector Fwd = B->GetRightVector().GetSafeNormal2D();   // (the body's mesh looks along its own +Y)
		if (B->GetBoneIndex(TEXT("clavicle_l")) != INDEX_NONE && B->GetBoneIndex(TEXT("clavicle_r")) != INDEX_NONE)
		{
			O.Low = float((B->GetSocketLocation(TEXT("clavicle_l")).Z + B->GetSocketLocation(TEXT("clavicle_r")).Z) * 0.5 - 1.0);
			const FVector Left = (B->GetSocketLocation(TEXT("upperarm_l")) - B->GetSocketLocation(TEXT("upperarm_r"))).GetSafeNormal2D();
			if (!Left.IsNearlyZero()) Fwd = (FVector::UpVector ^ Left).GetSafeNormal2D();
		}
		else O.Low = float(Head.Z - 14.0);
		O.Eye = Head + FVector(0, 0, 7.0) + Fwd * 9.0;
		USkeletalMeshComponent* Fc = GT.Face.Get();
		if (Fc && Fc->GetBoneIndex(TEXT("FACIAL_L_Eye")) != INDEX_NONE && Fc->GetBoneIndex(TEXT("FACIAL_R_Eye")) != INDEX_NONE)
		{
			const FVector L = Fc->GetSocketLocation(TEXT("FACIAL_L_Eye")), R = Fc->GetSocketLocation(TEXT("FACIAL_R_Eye")), M = (L + R) * 0.5;
			// (a face not posed yet stands elsewhere: the body's head is believed then)
			if (FVector::Dist(M, Head) < 25.0) O.Eye = M;   // (which way: the shoulders, never the eyes - the head follows the camera, TalkLook.h, the camera must not follow the head)
		}
		O.Yaw = float(Fwd.Rotation().Yaw); O.Top = float(Head.Z) + GT.Crown + GT.Hair + 1.5f;
		// (a head bent or lying: never a picture upside down)
		O.Low = FMath::Min(O.Low, float(O.Eye.Z) - 14.f); O.Top = FMath::Max(O.Top, float(O.Eye.Z) + 10.f);
		return true;
	}

	/** The smoothed face follows: not at all for breathing's little moves, quicker the further it is behind. */
	void FollowFace(float Dt)
	{
		FFace N; if (!MeasureFace(N)) return;
		const FVector D = N.Eye - GT.F.Eye; const float L = float(D.Size()); const double ZWas = GT.F.Eye.Z;
		if (L > 1.2f) GT.F.Eye += D * double((L - 1.2f) / L * (1.f - FMath::Exp(-Dt * FMath::GetMappedRangeValueClamped(FVector2f(2.f, 30.f), FVector2f(2.5f, 10.f), L))));
		const float Y = float(FRotator::NormalizeAxis(N.Yaw - GT.F.Yaw)), Ay = FMath::Abs(Y);
		if (Ay > 5.f) GT.F.Yaw = float(FRotator::NormalizeAxis(GT.F.Yaw + Y * ((Ay - 5.f) / Ay) * (1.f - FMath::Exp(-Dt * 2.2f))));
		// (the top and the collarbones go with the eyes; their distance from them very slowly: the picture's size never pumps)
		const float K = 1.f - FMath::Exp(-Dt * 1.2f), Moved = float(GT.F.Eye.Z - ZWas), Off = float(GT.F.Eye.Z - N.Eye.Z);
		GT.F.Top += Moved; GT.F.Low += Moved;
		GT.F.Top += (N.Top + Off - GT.F.Top) * K; GT.F.Low += (N.Low + Off - GT.F.Low) * K;
	}

	FVector SpotPos(const FFace& F, const FSpot& S) { return F.Eye + FRotator(0.f, F.Yaw + S.Side, 0.f).Vector() * S.Dist + FVector(0, 0, S.Up); }

	/** The whole view from a spot: where, which way, and the angle up and down. */
	void SpotView(const FFace& F, const FSpot& S, FVector& Pos, FRotator& Rot, float& Vdeg)
	{
		const FLayout L = MakeLayout(); const FVector2f Px = ScreenPx(); const float Aspect = Px.X / Px.Y;
		Pos = SpotPos(F, S);
		// the middle of the picture: half-way from the head's top to the collarbones, a little behind the eyes
		const FVector Mid = FVector(F.Eye.X, F.Eye.Y, (F.Top + F.Low) * 0.5) - FRotator(0.f, F.Yaw, 0.f).Vector() * 4.0;
		const float D = FMath::Max(float(FVector::Dist(Pos, Mid)), 20.f);
		const float H = FMath::Max((F.Top - F.Low) / (FaceFill * L.FreeH), 34.f / FMath::Max(L.FreeW, 0.1f));
		Vdeg = FMath::Clamp(FMath::RadiansToDegrees(2.f * FMath::Atan(H * 0.5f / D)), MinVert, MaxVert);
		const float Tv = FMath::Tan(FMath::DegreesToRadians(Vdeg * 0.5f)), Th = Tv * Aspect;
		Rot = (Mid - Pos).Rotation();
		Rot.Yaw -= FMath::RadiansToDegrees(FMath::Atan(L.Nx * Th)); Rot.Pitch -= FMath::RadiansToDegrees(FMath::Atan(L.Ny * Tv)); Rot.Roll = 0.f;
	}

	FCollisionQueryParams MakeQuery(AADPWalker& Me)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(TalkCam), true);
		auto Skip = [&Q](AActor* A) { if (!A) return; Q.AddIgnoredActor(A); TArray<AActor*> At; A->GetAttachedActors(At, true, true); Q.AddIgnoredActors(At); };
		Skip(&Me); Skip(Me.HumanActor()); Skip(GT.Who.Get());
		return Q;
	}

	/** Nothing solid (walls, glass, counters, things) nor anybody else between A and B, for a ball of R. */
	bool IsClear(AADPWalker& Me, const FCollisionQueryParams& Q, const FVector& A, const FVector& B, float R)
	{
		UWorld* W = Me.GetWorld(); FHitResult H; const FCollisionShape Ball = FCollisionShape::MakeSphere(R);
		if (W->SweepSingleByChannel(H, A, B, FQuat::Identity, ECC_Visibility, Ball, Q) || W->SweepSingleByChannel(H, A, B, FQuat::Identity, ECC_Camera, Ball, Q)) return false;
		if (AADPPeople* Pp = AADPPeople::Get(W))
			for (const AADPPeople::FWho& P : Pp->Everyone())
			{
				const AActor* O = P.A.Get(); if (!O || O == GT.Who.Get() || O == Me.HumanActor() || !IsValid(P.Body) || O->IsHidden()) continue;
				if (FVector::DistSquared(O->GetActorLocation(), A) > FMath::Square(900.0)) continue;
				const FVector L = P.Body->GetSocketLocation(TEXT("head"));
				const FVector N = FMath::ClosestPointOnSegment(FVector(L.X, L.Y, 0), FVector(A.X, A.Y, 0), FVector(B.X, B.Y, 0));
				if (FVector::Dist2D(N, L) < 27.0 + R && FMath::Min(A.Z, B.Z) < L.Z + 28.0 && FMath::Max(A.Z, B.Z) > L.Z - 190.0) return false;
			}
		return true;
	}

	/** The whole head seen from there: the eyes, the top and the collarbones. */
	bool SeesHead(AADPWalker& Me, const FCollisionQueryParams& Q, const FVector& Cam)
	{
		for (const FVector& P : {GT.F.Eye, FVector(GT.F.Eye.X, GT.F.Eye.Y, GT.F.Top - 2.0), FVector(GT.F.Eye.X, GT.F.Eye.Y, GT.F.Low + 2.0)})
		{
			const FVector D = P - Cam; const double L = D.Size(); if (L < 25.0) return false;
			if (!IsClear(Me, Q, Cam, Cam + D * ((L - 12.0) / L), 4.f)) return false;
		}
		return true;
	}

	/** Not through the person talked to. */
	bool PastThem(const FVector& A, const FVector& B)
	{
		const FVector H = GT.F.Eye - FRotator(0.f, GT.F.Yaw, 0.f).Vector() * 9.0;
		return FVector::Dist2D(FMath::ClosestPointOnSegment(FVector(H.X, H.Y, 0), FVector(A.X, A.Y, 0), FVector(B.X, B.Y, 0)), H) > 26.0;
	}

	/** The best free place: towards you first, then the other side, higher and lower, then closer. */
	bool PickSpot(AADPWalker& Me, const FVector& From, FSpot& Best)
	{
		const FCollisionQueryParams Q = MakeQuery(Me);
		float A = FMath::Clamp(float(FRotator::NormalizeAxis((From - GT.F.Eye).Rotation().Yaw - GT.F.Yaw)), -FaceSide, FaceSide);
		const float Sg = A < 0.f ? -1.f : 1.f; A = Sg * FMath::Max(FMath::Abs(A), 8.f);
		const float Sides[] = {A, -A, 0.f, Sg * 20.f, -Sg * 20.f, Sg * 34.f, -Sg * 34.f, Sg * 48.f, -Sg * 48.f};
		for (const float Dist : {85.f, 74.f, 62.f})
			for (const float Up : {0.f, 12.f, -12.f, 22.f})
				for (const float Side : Sides)
				{
					FSpot S; S.Side = Side; S.Up = Up; S.Dist = Dist; const FVector P = SpotPos(GT.F, S);
					if (!PastThem(From, P) || !IsClear(Me, Q, P, P + FVector(0, 0, 0.5), LensBall) || !SeesHead(Me, Q, P) || !IsClear(Me, Q, From, P, 5.f)) continue;
					Best = S; return true;
				}
		return false;
	}
}

void Begin(AActor* Who, USkeletalMeshComponent* Body, const FString& Name)
{
	if (!GTalkCam || !Who || !Body) return;
	if (GT.Phase != EPhase::None && GT.Who != Who)
	{
		// (someone else before the camera is back: it goes back first, then in on them)
		GT.NextWho = Who; GT.NextBody = Body; GT.NextName = Name;
		if (GT.Phase == EPhase::Wait) AllOver();
		else if (GT.Phase == EPhase::On) End(TEXT("you talk to someone else"));
		return;
	}
	if (GT.Phase == EPhase::On || GT.Phase == EPhase::Wait) return;
	if (GT.Phase == EPhase::Out) { GT.NextWho = nullptr; GT.Phase = EPhase::On; GT.BlockedFor = GT.CheckIn = 0.f; StartGlide(1.f, GlideIn); ADP_LOG(TEXT("TalkCam: in again on %s"), *GT.Name); return; }
	GT.Phase = EPhase::Wait; GT.Who = Who; GT.Body = Body; GT.Name = Name; GT.WaitFrames = 0; GT.LastFrame = GFrameCounter;
	ADPLayers::Whole(Who);
}

void End(const TCHAR* Why)
{
	if (GT.Phase == EPhase::Out) GT.NextWho = nullptr;   // (the next one's menu closed too)
	if (GT.Phase == EPhase::None || GT.Phase == EPhase::Out) return;
	ADP_LOG(TEXT("TalkCam: back from %s after %.1f s - %s"), *GT.Name, GT.Held, Why);
	if (GT.Phase == EPhase::Wait) { AllOver(); return; }
	GT.Phase = EPhase::Out; StartGlide(0.f, GlideOut * FMath::Clamp(GT.A, 0.4f, 1.f));
}

void Drop(const TCHAR* Why)
{
	if (GT.Phase == EPhase::None) return;
	ADP_LOG(TEXT("TalkCam: your view given back at once (%s)"), Why);
	if (GT.A > 0.f && GT.PC.IsValid() && GT.PC->PlayerCameraManager) GT.PC->PlayerCameraManager->SetGameCameraCutThisFrame();
	GT.NextWho = nullptr; AllOver();
}

void Tick()
{
	if (GT.Phase == EPhase::None) return;
	if (GFrameCounter > GT.LastFrame + 4) { Drop(TEXT("another view than your own")); return; }
	if (!GTalkCam) End(TEXT("switched off"));
}

float Amount() { return GT.Phase == EPhase::None ? 0.f : GT.A; }

float PanelShift(float Scale, const FVector2D& LocalSize)
{
	if (GT.Phase == EPhase::None) return 0.f;
	if (Scale > 0.05f) GPanelScale = Scale;
	if (LocalSize.X > 50.0) GPanelSize = LocalSize;
	return MakeLayout().Shift * GT.A;
}

void Apply(AADPWalker& Me, float Dt, FMinimalViewInfo& V)
{
	if (GT.Phase == EPhase::None) return;
	APlayerController* PC = Cast<APlayerController>(Me.GetController());
	if (!PC || !PC->IsLocalController()) return;
	GT.PC = PC; GT.LastFrame = GFrameCounter;
	if (GT.bLast && FVector::Dist(GT.LastBase, V.Location) > 300.0) { Drop(TEXT("you were moved")); return; }
	GT.LastBase = V.Location; GT.bLast = true;
	if (PC->PlayerCameraManager) { const AActor* Nv = PC->PlayerCameraManager->PendingViewTarget.Target; if (Nv && Nv != &Me) End(TEXT("another view comes")); }
	if (GT.Phase != EPhase::None && GT.Phase != EPhase::Out && !GT.Who.IsValid()) End(TEXT("the person is gone"));
	if (GT.Phase == EPhase::None) return;

	if (GT.Phase == EPhase::Wait)
	{
		// (they have just turned to you: their pose is the new one after a frame or two)
		if (++GT.WaitFrames < 3) return;
		MeasureHead();
		if (!MeasureFace(GT.F)) { ADP_WARN(TEXT("TalkCam: %s has no head bone to look at - your normal view stays"), *GT.Name); AllOver(); return; }
		const float Far = float(FVector::Dist(GT.F.Eye, V.Location));
		if (Far > 900.f) { ADP_WARN(TEXT("TalkCam: %s is %.0f cm away - your normal view stays"), *GT.Name, Far); AllOver(); return; }
		if (!PickSpot(Me, V.Location, GT.Want)) { ADP_WARN(TEXT("TalkCam: no free place for the camera by %s (%.0f %.0f %.0f) - your normal view stays"), *GT.Name, GT.F.Eye.X, GT.F.Eye.Y, GT.F.Eye.Z); AllOver(); return; }
		GT.At = GT.Want; GT.Phase = EPhase::On; GT.A = GT.Vel = 0.f; StartGlide(1.f, GlideIn); GT.LastCam = V.Location;
		FVector P; FRotator R; float Vd; SpotView(GT.F, GT.At, P, R, Vd);
		ADP_LOG(TEXT("TalkCam: in on %s from %s - %.0f cm, %.0f deg to the side, %.0f up, view %.1f deg, head top to collarbones %.1f cm (crown %.1f, hair or hat %.1f), face there %d"), *GT.Name, Me.IsThirdPerson() ? TEXT("behind") : TEXT("your eyes"), GT.At.Dist, GT.At.Side, GT.At.Up, Vd, GT.F.Top - GT.F.Low, GT.Crown, GT.Hair, GT.Face.IsValid() ? 1 : 0);
	}

	const float AWas = GT.A; const FSpot AtWas = GT.At;
	if (Dt > 0.f)
	{
		GT.Held += Dt;
		if (GT.Who.IsValid()) { if ((GT.SizeIn -= Dt) <= 0.f) { GT.SizeIn = 0.5f; MeasureHead(); } FollowFace(Dt); }
		const float K = 1.f - FMath::Exp(-Dt * 4.f);
		GT.At.Side += (GT.Want.Side - GT.At.Side) * K; GT.At.Up += (GT.Want.Up - GT.At.Up) * K; GT.At.Dist += (GT.Want.Dist - GT.At.Dist) * K;
		StepGlide(Dt);
	}
	if (GT.Phase == EPhase::Out && GT.A <= 0.0005f) { AllOver(); return; }
	if (FVector::Dist(GT.F.Eye, V.Location) > 1200.0) { Drop(TEXT("too far from them")); return; }

	FVector Pos; FRotator Rot; float Vd; SpotView(GT.F, GT.At, Pos, Rot, Vd);
	FVector Cam = FMath::Lerp(V.Location, Pos, double(GT.A));

	// the way there, round and back, and whatever comes in between meanwhile
	if (Dt > 0.f)
	{
		if (FVector::DistSquared(Cam, GT.LastCam) > 0.04 && !IsClear(Me, MakeQuery(Me), GT.LastCam, Cam, 3.f))
		{
			if (GT.Phase == EPhase::Out) { Drop(TEXT("something in the way back")); return; }
			// (back along the way just come; nothing is drawn from inside the thing)
			GT.A = AWas; GT.At = AtWas; GT.Vel = 0.f; End(TEXT("something came in the way"));
			SpotView(GT.F, GT.At, Pos, Rot, Vd); Cam = GT.LastCam;
		}
		else if (GT.Phase == EPhase::On && (GT.CheckIn -= Dt) <= 0.f)
		{
			GT.CheckIn = 0.15f;
			const FCollisionQueryParams Q = MakeQuery(Me); const FVector P = SpotPos(GT.F, GT.Want);
			if (IsClear(Me, Q, P, P + FVector(0, 0, 0.5), 6.f) && SeesHead(Me, Q, P)) GT.BlockedFor = 0.f;
			else if ((GT.BlockedFor += 0.15f) > 0.4f)
			{
				FSpot N; GT.BlockedFor = 0.f;
				if (PickSpot(Me, Cam, N)) { GT.Want = N; ADP_LOG(TEXT("TalkCam: something in front of %s - round to %.0f deg, %.0f up, %.0f cm"), *GT.Name, N.Side, N.Up, N.Dist); }
				else End(TEXT("something in front of them and no other place free"));
			}
		}
	}
	GT.LastCam = Cam;

	// your own body out of this picture while the camera is by it or looks past it
	const FVector Mine = Me.GetActorLocation(), MineTo = FMath::ClosestPointOnSegment(FVector(Mine.X, Mine.Y, 0), FVector(Cam.X, Cam.Y, 0), FVector(GT.F.Eye.X, GT.F.Eye.Y, 0));
	const bool bNearMe = FVector::Dist2D(Cam, Mine) < (GT.bHid ? 54.0 : 48.0), bInFront = GT.A > 0.5f && FVector::Dist2D(MineTo, Mine) < (GT.bHid ? 46.0 : 40.0);
	HideMe(&Me, GT.A > 0.02f && (bNearMe || bInFront));

	const FRotator To = (Rot - V.Rotation).GetNormalized();
	const FVector2f Px = ScreenPx(); const float Aspect = Px.X / Px.Y;
	const float T0 = FMath::Tan(FMath::DegreesToRadians(V.FOV * 0.5f)), T1 = FMath::Tan(FMath::DegreesToRadians(Vd * 0.5f)) * Aspect;
	V.Location = Cam;
	V.Rotation = FRotator(V.Rotation.Pitch + To.Pitch * GT.A, V.Rotation.Yaw + To.Yaw * GT.A, V.Rotation.Roll + To.Roll * GT.A);
	// (in logarithm: an even zoom, as a lens makes it)
	V.FOV = FMath::Clamp(FMath::RadiansToDegrees(2.f * FMath::Atan(T0 * FMath::Pow(T1 / T0, GT.A))), 5.f, 170.f);
	if (GT.A > 0.02f) V.PerspectiveNearClipPlane = 4.f;
	// the portrait lens, all the while (a phone or tablet in your hands has the focus itself: Devices.cpp)
	LendLens();
	if (!V.PostProcessSettings.bOverride_DepthOfFieldFstop || V.PostProcessBlendWeight <= 0.f) Portrait(V, Aspect, Pos, Vd);
}

bool IsOn() { return GTalkCam != 0; }

void SetOn(bool bOn)
{
	GTalkCam = bOn ? 1 : 0;
	GConfig->SetBool(TEXT("ADP.Graphics"), TEXT("TalkCam"), bOn, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni);
}

void LoadSetting()
{
	bool bOn = true;
	if (GConfig->GetBool(TEXT("ADP.Graphics"), TEXT("TalkCam"), bOn, GGameUserSettingsIni)) GTalkCam = bOn ? 1 : 0;
}

void BlurWasSet() { GLent[0].bHeld = false; }
}
