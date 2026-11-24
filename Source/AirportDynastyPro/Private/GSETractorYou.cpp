// To-do 238: the baggage tractor and its carts worked by you - the hitch pins, the drawbars (the carts' and the GPU's) and the
// carts' curtains by your own hands, with the movements of its crew (GSECrew.cpp); a crew go on with it when you leave it.
#include "GSE.h"
#include "VehicleControls.h"
#include "ADPLog.h"
#include "Tugs.h"
#include "BabylonModel.h"
#include "CrewPose.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace
{
	const FVector TrTPin(-162.f, 0.f, 40.f), TrTPinTop(-162.f, 0.f, 62.5f), TrCJaw(-127.f, 0.f, 40.f), TrCPinTop(-127.f, 0.f, 59.55f), TrCAxle(80.f, 0.f, 38.9f), TrGHandle(170.f, 0.f, 48.f), TrGEye(205.f, 0.f, 40.f);
	const float TrEyeToAxle = 121.f, TrLeave = 4000.f, TrWait = 120.f;
	enum : int32 { TrPin = 1, TrPinF, TrBarOn, TrBarOff, TrGpuOn, TrGpuOff, TrStrapA, TrStrapB, TrHem };
	FVector TrYawV(float Yaw) { return FRotator(0.f, Yaw, 0.f).Vector(); }
	const APawn* TrMe(const UWorld* W) { const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr; return PC ? PC->GetPawn() : nullptr; }
	float TrTurn(float A, float B) { return FMath::Abs(FMath::FindDeltaAngleDegrees(A, B)); }
}

bool AGSE::TractorFree(const FTractor& T) const
{
	// (one you drove off with: first home again; TractorHome)
	return T.IsTrain() && T.Job == EJob::None && T.M.IsValid() && !T.Player.IsValid() && T.Train.Num() == 0 && T.TowGpu == INDEX_NONE && T.YBar == INDEX_NONE && T.Pin < 0.02f && T.PinWant < 0.02f
		&& FVector::Dist2D(T.At, T.Home) < 150.f && TrTurn(T.Yaw, T.HomeYaw) < 15.f;
}

bool AGSE::CartFree(int32 c) const { return Carts.IsValidIndex(c) && Carts[c].Tractor == INDEX_NONE && !Carts[c].bOut; }

FVector AGSE::TrainHitch(const FTractor& T) const { const int32 c = TrainLast(T); return Carts.IsValidIndex(c) ? CW(Carts[c], TrCJaw) : TW(T, TrTPin); }

int32 AGSE::HitchCart(const FTractor& T) const
{
	// a free cart whose eye comes down into the last hitch (its drawbar stands up till it is laid in)
	if (T.YBar != INDEX_NONE || T.TowGpu != INDEX_NONE) return INDEX_NONE;
	const FVector H = TrainHitch(T); const int32 l = TrainLast(T); const float HYaw = l != INDEX_NONE ? Carts[l].Yaw : T.Yaw;
	for (int32 c = 0; c < Carts.Num(); ++c)
	{
		const FCart& C = Carts[c]; if (C.Tractor != INDEX_NONE || !C.M.IsValid() || C.Lift > -70.f) continue;
		const FVector D = H - (C.At + TrYawV(C.Yaw) * (TrCAxle.X + TrEyeToAxle)); const float Along = D | TrYawV(C.Yaw);
		if (Along > -14.f && Along < 45.f && FMath::Abs(D | TrYawV(C.Yaw + 90.f)) < 35.f && FMath::Abs(D.Z) < 100.f && TrTurn(HYaw, C.Yaw) < 35.f) return c;
	}
	return INDEX_NONE;
}

int32 AGSE::HitchGpu(const FTractor& T) const
{
	const ATugs* G = Tg(); if (!G || T.Train.Num() || T.YBar != INDEX_NONE) return INDEX_NONE;
	const FVector H = TW(T, TrTPin);
	for (int32 g = 0; g < G->Tugs.Num(); ++g)
	{
		const ABabylonModel* M = G->Tugs[g].Gpu.M.Get(); if (!M) continue;
		const FVector D = H - M->GetActorTransform().TransformPosition(TrGEye);
		if (D.Size2D() < 30.f && FMath::Abs(D.Z) < 100.f && TrTurn(M->GetActorRotation().Yaw, T.Yaw) < 35.f) return g;
	}
	return INDEX_NONE;
}

bool AGSE::CurtainNear(const FTractor& T, int32& c, int32& s) const
{
	const APawn* Me = TrMe(GetWorld()); c = INDEX_NONE; s = 0; if (!Me) return false;
	float Best = 320.f;
	for (const int32 k : T.Train)
	{
		if (!Carts.IsValidIndex(k) || !Carts[k].M.IsValid()) continue;
		const FVector L = FTransform(FRotator(0.f, Carts[k].Yaw, 0.f), Carts[k].At).InverseTransformPosition(Me->GetActorLocation());
		const float D = L.Size2D(); if (FMath::Abs(L.Y) < 78.f || D >= Best) continue;
		Best = D; c = k; s = L.Y < 0.f ? 0 : 1;
	}
	return c != INDEX_NONE;
}

bool AGSE::TractorShut(FTractor& T, bool bSay)
{
	const TCHAR* Why = nullptr;
	if (T.YBar != INDEX_NONE) Why = TEXT("The cart's drawbar lies in the hitch: put its pin in first");
	else if (T.Pin > 0.02f || T.PinWant > 0.02f) Why = TEXT("The tractor's hitch pin is up: put it in first");
	else for (const int32 c : T.Train)
	{
		if (!Carts.IsValidIndex(c)) continue;
		const FCart& C = Carts[c];
		if (C.Pin > 0.02f || C.PinWant > 0.02f) Why = TEXT("A cart's hitch pin is up: put it in first");
		else for (int32 s = 0; s < 2; ++s) if (C.Curtain[s] > 0.01f || C.CurtainWant[s] > 0.01f || !C.bStraps[s] || C.StrapOff[s]) Why = TEXT("A cart's curtain is not closed and strapped");
	}
	if (Why && bSay) { const float Now = GetWorld()->GetTimeSeconds(); if (Now > T.YNoteT) { T.YNoteT = Now + 4.f; AVehicleControls::Note(GetWorld(), Why); } }
	return Why == nullptr;
}

bool AGSE::TractorBack(FTractor& T, bool bSay)
{
	if (T.Train.Num() == 0) return true;
	if (bSay) { const float Now = GetWorld()->GetTimeSeconds(); if (Now > T.YNoteT) { T.YNoteT = Now + 4.f; AVehicleControls::Note(GetWorld(), TEXT("Not backwards with carts behind: their drawbars would fold")); } }
	return false;
}

bool AGSE::TrainOwn(const FTractor& T, const FVector& At) const
{
	if (!T.IsTrain()) return false;
	for (int32 c = 0; c < Carts.Num(); ++c)
	{
		const FCart& C = Carts[c]; if (FVector::Dist2D(At, C.At) > 30.f) continue;
		if (T.Train.Contains(c)) return true;
		// the free cart you back onto (its drawbar stands up): as far as its eye comes into the hitch
		if (C.Tractor != INDEX_NONE || C.Lift > -70.f || T.Train.Num()) return false;
		const FVector D = TW(T, TrTPin) - (C.At + TrYawV(C.Yaw) * (TrCAxle.X + TrEyeToAxle));
		return D.Size2D() < 220.f && TrTurn(T.Yaw, C.Yaw) < 50.f && (!T.bReverse || (D | TrYawV(C.Yaw)) > -8.f);
	}
	return false;
}

bool AGSE::TrainDrag(FTractor& T, const FCollisionQueryParams& Q, float Dt)
{
	if (T.Train.Num() == 0 && T.TowGpu == INDEX_NONE) return true;
	struct FWas { FVector At = FVector::ZeroVector; float Yaw = 0.f, Axle = 0.f; }; TArray<FWas> Was;
	for (const int32 c : T.Train) { FWas W; if (Carts.IsValidIndex(c)) { W.At = Carts[c].At; W.Yaw = Carts[c].Yaw; W.Axle = Carts[c].AxleYaw; } Was.Add(W); }
	Train(T, Dt);
	TArray<FOutline> Ol; Outlines(Ol);
	auto Hit = [&](const FCart& C) -> bool
	{
		const FVector F = TrYawV(C.Yaw), R = TrYawV(C.Yaw + 90.f), Cc = C.At - F * 2.f;
		for (const FOutline& O : Ol)
		{
			if (FVector::Dist2D(O.C, Cc) < 30.f || TrainOwn(T, O.C) || FVector::Dist2D(O.C, T.At + TrYawV(T.Yaw) * T.KCentreX) < 60.f) continue;
			const FVector D = O.C - Cc; if (D.Size2D() > 140.f + 83.f + O.Hx + O.Hy) continue;
			const FVector Ax[4] = {F, R, O.X, O.Y}; bool bApart = false;
			for (const FVector& A : Ax) if (FMath::Abs(D | A) > 140.f * FMath::Abs(F | A) + 80.f * FMath::Abs(R | A) + O.Hx * FMath::Abs(O.X | A) + O.Hy * FMath::Abs(O.Y | A)) { bApart = true; break; }
			if (!bApart) return true;
		}
		return GetWorld()->OverlapBlockingTestByChannel(FVector(Cc.X, Cc.Y, C.At.Z + 130.f), FQuat(FRotator(0.f, C.Yaw, 0.f)), ECC_Pawn, FCollisionShape::MakeBox(FVector(136.f, 78.f, 70.f)), Q);
	};
	for (int32 k = 0; k < T.Train.Num(); ++k)
	{
		if (!Carts.IsValidIndex(T.Train[k]) || FVector::Dist2D(Carts[T.Train[k]].At, Was[k].At) < 0.01f || !Hit(Carts[T.Train[k]])) continue;
		// (a cart against something: the whole train stands as it was)
		for (int32 j = 0; j < T.Train.Num(); ++j) if (Carts.IsValidIndex(T.Train[j])) { FCart& C = Carts[T.Train[j]]; C.At = Was[j].At; C.Yaw = Was[j].Yaw; C.AxleYaw = Was[j].Axle; PoseCart(C); }
		const float Now = GetWorld()->GetTimeSeconds(); if (Now > T.YNoteT) { T.YNoteT = Now + 4.f; AVehicleControls::Note(GetWorld(), FString::Printf(TEXT("Cart %d of the train is against something"), k + 1)); }
		return false;
	}
	return true;
}

// ------------------------------------------------------------------ by your hands

FString AGSE::TractorSay(int32 Ti, int32 S) const
{
	const FTractor& T = Tractors[Ti]; const int32 l = TrainLast(T), f = T.Train.Num() > 1 ? T.Train[T.Train.Num() - 2] : INDEX_NONE;
	if (!T.IsTrain() || FMath::Abs(T.Speed) > 1.f) return FString();
	const float PinR = l != INDEX_NONE ? Carts[l].PinWant : T.PinWant, PinF = l == INDEX_NONE ? 0.f : f != INDEX_NONE ? Carts[f].PinWant : T.PinWant;
	const TCHAR* R = TEXT(""); int32 c, s;
	switch (S)
	{
	case TrPin: R = PinR > 0.5f ? TEXT("Put the hitch pin in") : TEXT("Pull the hitch pin up"); break;
	case TrPinF: if (l != INDEX_NONE && T.YBar == INDEX_NONE) R = PinF > 0.5f ? TEXT("Put the last cart's hitch pin in") : TEXT("Pull the last cart's hitch pin up"); break;
	case TrBarOn: if (HitchCart(T) != INDEX_NONE) R = TEXT("Lay the cart's drawbar into the hitch"); break;
	case TrBarOff: if (T.YBar != INDEX_NONE || (l != INDEX_NONE && PinF > 0.5f)) R = TEXT("Lift the cart's drawbar out of the hitch"); break;
	case TrGpuOn: if (T.TowGpu == INDEX_NONE && HitchGpu(T) != INDEX_NONE) R = TEXT("Lift the GPU's drawbar into the hitch"); break;
	case TrGpuOff: if (T.TowGpu != INDEX_NONE) R = TEXT("Lift the GPU's drawbar out of the hitch"); break;
	case TrStrapA: case TrStrapB:
		if (!CurtainNear(T, c, s) || Carts[c].Curtain[s] > 0.01f || Carts[c].CurtainWant[s] > 0.01f) break;
		R = ((Carts[c].StrapOff[s] | (Carts[c].bStraps[s] ? 0 : 3)) & (S == TrStrapA ? 1 : 2)) ? TEXT("Hook the curtain's two straps") : TEXT("Unhook the curtain's two straps"); break;
	case TrHem: if (CurtainNear(T, c, s)) R = Carts[c].CurtainWant[s] > 0.5f ? TEXT("Roll the curtain down") : TEXT("Roll the curtain up"); break;
	}
	return R;
}

bool AGSE::TractorMay(int32 Ti, int32 S, FString& Why) const
{
	const FTractor& T = Tractors[Ti]; const int32 l = TrainLast(T), f = T.Train.Num() > 1 ? T.Train[T.Train.Num() - 2] : INDEX_NONE;
	auto No = [&Why](const TCHAR* W) { Why = W; return false; };
	const float PinR = l != INDEX_NONE ? Carts[l].Pin : T.Pin, PinF = l == INDEX_NONE ? 0.f : f != INDEX_NONE ? Carts[f].Pin : T.Pin;
	auto Busy = [this, &T](int32 g) { for (const FTractor& U : Tractors) if (&U != &T && U.TowGpu == g) return true; return false; };
	const ATugs* G = Tg(); int32 c, s;
	auto GpuBusy = [&](int32 g) -> bool { return !G || !G->Tugs.IsValidIndex(g) || AircraftAt(G->Tugs[g].StandX) || G->Tugs[g].Gpu.bConnected || G->Tugs[g].Gpu.bInHand || Busy(g); };
	switch (S)
	{
	case TrPinF: return PinF > 0.5f || PinR < 0.02f ? true : No(TEXT("put the pin behind it in first"));
	case TrBarOn: return PinR > 0.95f ? true : No(TEXT("pull the hitch pin up first"));
	case TrBarOff:
	{
		if (T.YBar != INDEX_NONE) return PinR > 0.95f ? true : No(TEXT("pull the hitch pin up first"));
		if (l == INDEX_NONE || PinF < 0.95f) return No(TEXT("pull its hitch pin up first"));
		for (int32 k = 0; k < 2; ++k) if (Carts[l].Curtain[k] > 0.01f || Carts[l].StrapOff[k] || !Carts[l].bStraps[k]) return No(TEXT("close its curtains first"));
		// (nothing left standing in a lane: only back where the cart was taken from)
		if (!Carts[l].bOut || FVector::Dist2D(Carts[l].At, Carts[l].Place) > 200.f || TrTurn(Carts[l].Yaw, Carts[l].PlaceYaw) > 20.f) return No(TEXT("it cannot be left here: bring it back to its place beside the tractors"));
		return true;
	}
	case TrGpuOn: return T.Pin < 0.95f ? No(TEXT("pull the hitch pin up first")) : GpuBusy(HitchGpu(T)) ? No(TEXT("the GPU is in use: its stand must be empty")) : true;
	case TrGpuOff:
		if (T.Pin < 0.95f) return No(TEXT("pull the hitch pin up first"));
		if (const FGpuRig* R = GpuRigs.Find(T.TowGpu)) if (G && G->Tugs.IsValidIndex(T.TowGpu) && G->Tugs[T.TowGpu].Gpu.M.IsValid())
		{
			const ABabylonModel* M = G->Tugs[T.TowGpu].Gpu.M.Get();
			if (R->bBound && (FVector::Dist2D(M->GetActorLocation(), R->Home) > 60.f || TrTurn(M->GetActorRotation().Yaw, R->HomeYaw) > 8.f)) return No(TEXT("it stays on the hitch: push it back into its place at the stand"));
		}
		return true;
	case TrHem:
		if (!CurtainNear(T, c, s)) return false;
		return Carts[c].CurtainWant[s] > 0.5f || !Carts[c].bStraps[s] ? true : No(TEXT("unhook its straps first"));
	}
	return true;
}

FVector AGSE::TractorSpot(int32 Ti, int32 S) const
{
	const FTractor& T = Tractors[Ti]; const int32 l = TrainLast(T), f = T.Train.Num() > 1 ? T.Train[T.Train.Num() - 2] : INDEX_NONE;
	int32 c = INDEX_NONE, s = 0;
	switch (S)
	{
	case TrPin: return l != INDEX_NONE ? CW(Carts[l], TrCPinTop) : TW(T, TrTPinTop);
	case TrPinF: return f != INDEX_NONE ? CW(Carts[f], TrCPinTop) : TW(T, TrTPinTop);
	case TrBarOn: c = HitchCart(T); return c != INDEX_NONE ? CartBarHandle(c) : TrainHitch(T);
	case TrBarOff: c = T.YBar != INDEX_NONE ? T.YBar : l; return c != INDEX_NONE ? CartBarHandle(c) : TrainHitch(T);
	case TrGpuOn: c = HitchGpu(T); return c != INDEX_NONE ? GpuPoint(c, TrGHandle) : TrainHitch(T);
	case TrGpuOff: return T.TowGpu != INDEX_NONE ? GpuPoint(T.TowGpu, TrGHandle) : TrainHitch(T);
	}
	if (!CurtainNear(T, c, s)) return TrainHitch(T);
	const float Ys = s == 0 ? -82.f : 82.f;
	return CW(Carts[c], S == TrHem ? FVector(0.f, Ys, FMath::Lerp(60.f, 150.f, Carts[c].Curtain[s])) : FVector(S == TrStrapA ? -58.f : 58.f, Ys, 58.5f));
}

TFunction<void(float, float, FDriverPose&)> AGSE::TractorAct(int32 Ti, int32 S, float& Secs) const
{
	// the crew's own movement; what it does begins as your hands get there (K > 0) and is heard at its end
	const FTractor& T = Tractors[Ti]; const int32 l = TrainLast(T), f = T.Train.Num() > 1 ? T.Train[T.Train.Num() - 2] : INDEX_NONE;
	TFunction<void(float, float, FDriverPose&)> Fn; int32 c = INDEX_NONE, s = 0; float Want = 0.f;
	if (S == TrPin || S == TrPinF)
	{
		c = S == TrPin ? l : f; Want = (c == INDEX_NONE ? T.PinWant : Carts[c].PinWant) > 0.5f ? 0.f : 1.f;
		Fn = HitchPinPose(Ti, c, Want > 0.5f ? 1.f : 0.9f); Secs = Want > 0.5f ? 1.5f : 1.3f;
	}
	else if (S == TrBarOn || S == TrBarOff) { c = S == TrBarOn ? HitchCart(T) : T.YBar != INDEX_NONE ? T.YBar : l; if (c == INDEX_NONE) return nullptr; Fn = CartBarPose(c); Secs = 3.f; }
	else if (S == TrGpuOn || S == TrGpuOff) { c = S == TrGpuOn ? HitchGpu(T) : T.TowGpu; if (c == INDEX_NONE) return nullptr; Fn = GpuBarPose(c); Secs = 2.4f; }
	else if (S == TrHem) { if (!CurtainNear(T, c, s)) return nullptr; Want = Carts[c].CurtainWant[s] > 0.5f ? 0.f : 1.f; Fn = CurtainPose(c, s); Secs = 2.8f; }
	else
	{
		if (!CurtainNear(T, c, s)) return nullptr;
		const bool bA = S == TrStrapA; const float Sx = bA ? -58.f : 58.f;
		TFunction<void(float, float, FDriverPose&)> A = StrapPose(c, s, bA ? -85.f : 30.f, Sx), B = StrapPose(c, s, bA ? -30.f : 85.f, Sx);
		Fn = [A, B](float K, float PH, FDriverPose& P) { if (K < 0.5f) A(K * 2.f, PH, P); else B(K * 2.f - 1.f, PH, P); }; Secs = 2.f;
	}
	TSharedRef<int32> Step = MakeShared<int32>(0); TWeakObjectPtr<AGSE> Me(const_cast<AGSE*>(this));
	return [Me, Fn, Step, Ti, S, c, s, Want](float K, float PH, FDriverPose& P)
	{
		AGSE* G = Me.Get(); if (!G || !G->Tractors.IsValidIndex(Ti)) return;
		if (K > 0.f && *Step == 0) { *Step = 1; G->TractorDo(Ti, S, c, s, Want, false); }
		if (K >= 1.f && *Step == 1) { *Step = 2; G->TractorDo(Ti, S, c, s, Want, true); }
		Fn(K, PH, P);
	};
}

void AGSE::TractorDo(int32 Ti, int32 S, int32 c, int32 s, float Want, bool bEnd)
{
	FTractor& T = Tractors[Ti]; T.YLeftT = 0.f;
	if (T.Job != EJob::None) return;   // (a crew took it meanwhile)
	switch (S)
	{
	case TrPin: case TrPinF:
	{
		const FVector At = c == INDEX_NONE ? TW(T, TrTPinTop) : CW(Carts[c], TrCPinTop);
		if (!bEnd) { (c == INDEX_NONE ? T.PinWant : Carts[c].PinWant) = Want; if (Want > 0.5f) Sound(c == INDEX_NONE ? TEXT("BTPinOut") : TEXT("BCPinOut"), At); return; }
		if (Want > 0.5f) return;
		Sound(c == INDEX_NONE ? TEXT("BTHitchPin") : TEXT("BCPinIn"), At);
		if (S != TrPin || !Carts.IsValidIndex(T.YBar)) return;
		// through the eye: it hangs on the train
		FCart& C = Carts[T.YBar]; C.Lift = C.LiftWant = 0.f; C.bMoved = true; T.Train.AddUnique(T.YBar); C.Tractor = Ti; T.YBar = INDEX_NONE;
		ADP_LOG(TEXT("GSE: a cart coupled to %s by you (train of %d)"), *T.Name, T.Train.Num());
		return;
	}
	case TrBarOn:
		if (bEnd) { Sound(TEXT("BCDrawbarClank"), TrainHitch(T)); return; }
		if (Carts[c].Tractor != INDEX_NONE) return;   // (a crew took it meanwhile)
		if (!Carts[c].bOut) { Carts[c].bOut = true; Carts[c].Place = Carts[c].At; Carts[c].PlaceYaw = Carts[c].Yaw; }
		Carts[c].Tractor = Ti; Carts[c].LiftWant = 0.f; Carts[c].bMoved = true; T.YBar = c;
		return;
	case TrBarOff:
		if (bEnd) return;
		T.Train.Remove(c); if (T.YBar == c) T.YBar = INDEX_NONE;
		Carts[c].Tractor = INDEX_NONE; Carts[c].LiftWant = -80.f; Carts[c].Flight.Reset(); Carts[c].bOut = false; Carts[c].bMoved = true;
		Sound(TEXT("BCDrawbarClank"), CW(Carts[c], FVector(201.f, 0.f, 40.f)));
		ADP_LOG(TEXT("GSE: a cart taken off %s by you (train of %d)"), *T.Name, T.Train.Num());
		return;
	case TrGpuOn: case TrGpuOff:
		if (!bEnd) return;
		T.TowGpu = S == TrGpuOn ? c : INDEX_NONE; Sound(TEXT("BCDrawbarClank"), GpuPoint(c, TrGEye));
		ADP_LOG(TEXT("GSE: the GPU %s %s by you"), S == TrGpuOn ? TEXT("coupled to") : TEXT("taken off"), *T.Name);
		return;
	case TrHem: if (!bEnd) { Carts[c].CurtainWant[s] = Want; Carts[c].bMoved = true; Sound(Want > 0.5f ? TEXT("BCCurtainUp") : TEXT("BCCurtainDown"), CW(Carts[c], FVector(0.f, s == 0 ? -82.f : 82.f, 110.f))); } return;
	case TrStrapA: case TrStrapB:
	{
		FCart& C = Carts[c]; Sound(TEXT("BCCurtainStrap"), CW(C, FVector(S == TrStrapA ? -58.f : 58.f, s == 0 ? -82.f : 82.f, 58.5f))); if (!bEnd) return;
		if (!C.bStraps[s] && C.StrapOff[s] == 0) C.StrapOff[s] = 3;
		C.StrapOff[s] ^= S == TrStrapA ? 1 : 2; C.bStraps[s] = C.StrapOff[s] != 3; C.bMoved = true;
		return;
	}
	}
}

TSharedPtr<FVehTable> AGSE::TractorControls()
{
	using K = FVehControl;
	TSharedPtr<FVehTable> Tb = MakeShared<FVehTable>();
	auto Add = [&Tb](const TCHAR* Lb, int32 S) { K X; X.Part = TEXT(""); X.Point = TEXT("TR_Hitch"); X.Kind = K::Push; X.Label = Lb; X.Out = FVector::ZeroVector; X.Switch = S; Tb->Controls.Add(X); };
	Add(TEXT("Hitch pin"), TrPin); Add(TEXT("Last cart's hitch pin"), TrPinF); Add(TEXT("Cart's drawbar"), TrBarOn); Add(TEXT("Cart's drawbar"), TrBarOff);
	Add(TEXT("GPU's drawbar"), TrGpuOn); Add(TEXT("GPU's drawbar"), TrGpuOff); Add(TEXT("Curtain straps"), TrStrapA); Add(TEXT("Curtain straps"), TrStrapB); Add(TEXT("Curtain"), TrHem);
	Tb->Range = 2600.f;
	Tb->Frame = [](const AGSE&, const FTractor&, int32) { return FTransform::Identity; };
	Tb->Now = [](const FTractor&, int32) { return 0.f; };
	Tb->Want = [](FTractor&, int32, float) {};
	Tb->May = [](const AGSE&, const FTractor&, int32, float, FString&) { return true; };
	Tb->Stowed = [](const AGSE&, const FTractor&) { return true; };
	Tb->Stow = [](AGSE&, FTractor&) {};
	Tb->Aboard = [](const FTractor&, const FVector&) -> uint8 { return 0; };
	auto Idx = [](const AGSE& G, const FTractor& T) { return static_cast<int32>(&T - G.Tractors.GetData()); };
	Tb->Spot = [Idx](const AGSE& G, const FTractor& T, const K& X, FVector& W) { if (X.bSeat || X.Switch < TrPin || X.Switch > TrHem) return false; W = G.TractorSpot(Idx(G, T), X.Switch); return true; };
	Tb->Say = [Idx](const AGSE& G, const FTractor& T, const K& X) -> FString { return X.bSeat || X.Switch < TrPin || X.Switch > TrHem ? FString(X.Label) : G.TractorSay(Idx(G, T), X.Switch); };
	Tb->MaySwitch = [](AGSE& G, int32 Ti, int32 S, FString& Why) { return G.TractorMay(Ti, S, Why); };
	Tb->Switch = [](AGSE&, int32, int32) { return false; };
	Tb->Climb = [](const AGSE& G, int32 Ti, int32 S, float& Secs) -> TFunction<void(float, float, FDriverPose&)> { if (S < TrPin || S > TrHem) return nullptr; return G.TractorAct(Ti, S, Secs); };
	return Tb;
}

// ------------------------------------------------------------------ left by you

FString AGSE::TrainFlight(const FTractor& T) const
{
	// the flight whose bags are truly in its carts (none: it is nobody's train); what you load by hand sets Flight with the bag
	for (const int32 c : T.Train) if (Carts.IsValidIndex(c)) for (const int32 B : Carts[c].Bags) if (B != INDEX_NONE) return Carts[c].Flight.IsEmpty() ? FString(TEXT("?")) : Carts[c].Flight;
	return FString();
}

void AGSE::YouTractors(float Dt)
{
	for (int32 i = 0; i < Tractors.Num(); ++i)
	{
		FTractor& T = Tractors[i]; if (!T.IsTrain() || T.Job != EJob::None || !T.M.IsValid()) continue;
		if (Carts.IsValidIndex(T.YBar))
		{
			// its drawbar laid in: the cart drawn to the hitch as the eye comes down, never at once
			FCart& C = Carts[T.YBar]; const FVector E = CW(C, TrCAxle) + TrYawV(C.AxleYaw) * TrEyeToAxle, H = TrainHitch(T);
			const FVector D(H.X - E.X, H.Y - E.Y, 0.f); if (D.Size() > 0.3f) CartFollow(C, E + D.GetClampedToMaxSize(20.f * Dt));
		}
		const APawn* Me = TrMe(GetWorld());
		if (T.Player.IsValid() || TractorFree(T) || !Me || FVector::Dist2D(Me->GetActorLocation(), T.At) < TrLeave) { T.YLeftT = 0.f; continue; }
		T.YLeftT += Dt; if (T.YLeftT < TrWait) continue;
		T.YLeftT = TrWait - 10.f;   // (asked again in ten seconds)
		TractorTake(i);
	}
}

/** Where a crew can drive it home from: 0 nowhere, 1 the apron, 2 the hall's lane westward, 3 beside the tractors' boxes. */
int32 AGSE::TractorWhere(const FTractor& T) const
{
	const float X = T.At.X / 100.f, Y = -T.At.Y / 100.f, Hx = T.Home.X / 100.f, Hy = -T.Home.Y / 100.f;
	if (Y >= 100.f) return 1;
	if (X > 145.f && X < 262.f && Y > 54.5f && Y < 59.5f && TrTurn(T.Yaw, 180.f) < 30.f) return 2;
	if (X > Hx - 12.f && X < Hx + 4.f && FMath::Abs(Y - Hy) < 6.f && TrTurn(T.Yaw, T.HomeYaw) < 60.f) return 3;
	return 0;
}

void AGSE::TractorTake(int32 Ti)
{
	const FString Fl = TrainFlight(Tractors[Ti]);
	const int32 Where = TractorWhere(Tractors[Ti]);
	if (!Fl.IsEmpty() || Where == 0 || (Tractors[Ti].TowGpu != INDEX_NONE && Where != 1))
	{
		if (Tractors[Ti].YTryT == 0.f) ADP_WARN(TEXT("GSE: %s stays where you left it (%s)"), *Tractors[Ti].Name, Fl.IsEmpty() ? TEXT("no way home from there for its crew") : TEXT("bags in its carts"));
		Tractors[Ti].YTryT = 1.f; return;
	}
	Tractors[Ti].YTryT = 0.f;
	const int32 Ci = Borrow(Tractors[Ti].At, nullptr, Ti); if (Ci == INDEX_NONE) return;   // (nobody free: asked again)
	FTractor& T = Tractors[Ti];
	T.Driver = Ci; T.Job = EJob::Boarding; T.JobT = 0.f; T.Key.Reset(); T.StandX = 0.f; T.YLeftT = 0.f; bWorked = true;
	ADP_LOG(TEXT("GSE: a crew fetch %s home from where you left it (%d carts%s)"), *T.Name, T.Train.Num(), T.TowGpu != INDEX_NONE ? TEXT(", the GPU") : TEXT(""));
	// what you left half done: the pins in (the cart whose drawbar lies in is on the train then), the curtains closed
	auto PinIn = [this, Ci, Ti](int32 Front)
	{
		WalkTo(Ci, [this, Ti, Front] { return Front == INDEX_NONE ? TW(Tractors[Ti], FVector(-205.f, -40.f, 0.f)) : CW(Carts[Front], FVector(-175.f, -45.f, 0.f)); }, 15.f);
		Do(Ci, [this, Ti, Front] { (Front == INDEX_NONE ? Tractors[Ti].PinWant : Carts[Front].PinWant) = 0.f; });
		PoseAct(Ci, HitchPinPose(Ti, Front, 0.9f), 1.3f);
		Do(Ci, [this, Ti, Front]
		{
			FTractor& U = Tractors[Ti]; Sound(Front == INDEX_NONE ? TEXT("BTHitchPin") : TEXT("BCPinIn"), Front == INDEX_NONE ? TW(U, TrTPinTop) : CW(Carts[Front], TrCPinTop));
			if (U.YBar != INDEX_NONE && Front == TrainLast(U)) { Carts[U.YBar].Lift = Carts[U.YBar].LiftWant = 0.f; U.Train.AddUnique(U.YBar); U.YBar = INDEX_NONE; }
		});
	};
	// (each open side of each cart: Curtains works one side of the carts it is given)
	const TArray<int32> All = T.Train;
	for (const int32 c : All) for (int32 s = 0; s < 2; ++s)
	{
		if (!Carts.IsValidIndex(c) || (Carts[c].CurtainWant[s] < 0.01f && Carts[c].bStraps[s] && Carts[c].StrapOff[s] == 0)) continue;
		const FVector To = CW(Carts[c], FVector(0.f, s == 0 ? -1000.f : 1000.f, 0.f));
		T.Train = {c}; Curtains(Ci, Ti, false, nullptr, &To); T.Train = All;
	}
	for (int32 k = T.Train.Num() - 1; k >= 0; --k) if (const int32 c = T.Train[k]; Carts.IsValidIndex(c) && (Carts[c].PinWant > 0.5f || (T.YBar != INDEX_NONE && k == T.Train.Num() - 1))) PinIn(c);
	if (T.PinWant > 0.5f || (T.YBar != INDEX_NONE && T.Train.Num() == 0)) PinIn(INDEX_NONE);
	ClimbIn(Ci, Ti, [this, Ti] { FTractor& U = Tractors[Ti]; U.Job = EJob::Back; U.bPark = false; U.bReverse = false; Motor(U, true); TractorHome(Ti); });
}

void AGSE::TractorHome(int32 Ti)
{
	FTractor& T = Tractors[Ti];
	if (T.TowGpu != INDEX_NONE) { TractorGpu(Ti); return; }
	const int32 Where = TractorWhere(T);
	if (Where == 1) { RouteIn(T, T.At, T.Yaw, [this, Ti] { TractorIn(Ti); }); return; }
	if (Where == 2)
	{
		TArray<FVector> W; float Yw = 0.f; const FVector A = PlanPose(137.f, 57.f, -1.f, 0.f, Yw);
		Dubins(W, T.At, T.Yaw, A, Yw, 400.f);
		Leg(W, 137.f, 57.f, -1.f, 0.f, 133.f, 53.f, 0.f, -1.f, 400.f); Leg(W, 133.f, 53.f, 0.f, -1.f, 133.f, 50.f, 0.f, -1.f, 400.f);
		Go(T, MoveTemp(W), false, [this, Ti] { TractorIn(Ti); });
		return;
	}
	// beside the tractors' boxes already: the carts off here, nose in
	if (T.Train.Num()) { TractorDrop(Ti); return; }
	ToBox(Ti);
}

void AGSE::TractorIn(int32 Ti)
{
	if (Tractors[Ti].Train.Num()) { TrainDrop(Ti); return; }
	FTractor& T = Tractors[Ti]; const FVector2f H(T.Home.X / 100.f, -T.Home.Y / 100.f);
	TArray<FVector> B; Leg(B, 133.f, 50.f, 0.f, -1.f, H.X - 7.f, H.Y, 1.f, 0.f, 300.f);
	Go(T, MoveTemp(B), false, [this, Ti] { ToBox(Ti); });
}

void AGSE::TractorDrop(int32 Ti)
{
	FTractor& U = Tractors[Ti]; U.Job = EJob::Uncoupling; const int32 Ci = U.Driver;
	ClimbOut(Ci, Ti, nullptr);
	const TArray<int32> Tr = U.Train;
	for (int32 k = Tr.Num() - 1; k >= 0; --k) UncoupleCart(Ci, Ti, Tr[k], k == 0 ? INDEX_NONE : Tr[k - 1], nullptr);
	ClimbIn(Ci, Ti, [this, Ti] { ToBox(Ti); });
}

void AGSE::TractorGpu(int32 Ti)
{
	// the GPU you drew off: round in front of its place and pushed straight back into it, as after a job (GpuApron)
	FTractor& T = Tractors[Ti]; const int32 Gate = T.TowGpu; T.Job = EJob::GpuMove;
	const ATugs* G = Tg(); if (!G || !G->Tugs.IsValidIndex(Gate) || !G->Tugs[Gate].Gpu.M.IsValid()) { T.TowGpu = INDEX_NONE; TractorHome(Ti); return; }
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(G->Tugs[Gate].StandX, S, N, Sp);
	if (AircraftAt(S)) { RetryIn(Ti, 10.f, [this, Ti] { TractorGpu(Ti); }); return; }
	const FGpuRig* R = GpuRigs.Find(Gate); const ABabylonModel* M = G->Tugs[Gate].Gpu.M.Get();
	const bool bHome = R && R->bBound; const FVector Hm = bHome ? R->Home : M->GetActorLocation(); const float Gy = bHome ? R->HomeYaw : M->GetActorRotation().Yaw;
	const FVector Gf = TrYawV(Gy);
	FVector Hook = Hm + Gf * (205.f + 162.f); Hook = Plan(Hook.X / 100.f, -Hook.Y / 100.f);
	FVector Ahead = Hook + Gf * 400.f; Ahead = Plan(Ahead.X / 100.f, -Ahead.Y / 100.f);
	TArray<FVector> W;
	if (FVector::Dist2D(T.At, Ahead) < 600.f && TrTurn(T.Yaw, Gy) < 25.f && ((Ahead - T.At) | Gf) >= 0.f) Dubins(W, T.At, T.Yaw, Ahead, Gy, 400.f);
	else
	{
		if (!Apron(W, T.At, Plan(S - 5.f, N + 16.f), &T)) { RetryIn(Ti, 8.f, [this, Ti] { TractorGpu(Ti); }); return; }
		float Yd = 0.f; const FVector D0 = PlanPose(S - 5.f, N + 16.f, 0.f, -1.f, Yd); Dubins(W, D0, Yd, Ahead, Gy, 400.f);
	}
	Go(T, MoveTemp(W), false, [this, Ti, Gate, Hook, Gy]
	{
		RetryIn(Ti, 3.f, [this, Ti, Gate, Hook, Gy]
		{
			FTractor& Z = Tractors[Ti]; TArray<FVector> B; Dubins(B, Z.At, Z.Yaw + 180.f, Hook, Gy + 180.f, 400.f);
			Go(Z, MoveTemp(B), true, [this, Ti, Gate]
			{
				FTractor& Q = Tractors[Ti]; Q.Job = EJob::GpuUncouple; const int32 Ci = Q.Driver;
				if (FGpuRig* Rg = GpuRigs.Find(Gate)) if (Rg->bBound) if (ATugs* G2 = Tg()) if (G2->Tugs[Gate].Gpu.M.IsValid()) G2->Tugs[Gate].Gpu.M->SetActorLocationAndRotation(Rg->Home, FRotator(0.f, Rg->HomeYaw, 0.f));
				ClimbOut(Ci, Ti, nullptr);
				CoupleGpu(Ci, Ti, Gate, false, nullptr);
				ClimbIn(Ci, Ti, [this, Ti, Gate] { GpuLeave(Ti, Gate); });
			});
		});
	});
}
