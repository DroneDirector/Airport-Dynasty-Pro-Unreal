// The vehicles' own controls in your hands (VehicleControls.h): the general layer. A vehicle's table is made by AGSE::CtlTable.
#include "VehicleControls.h"
#include "ServiceDrive.h"
#include "CabControls.h"
#include "ServiceVehicles.h"
#include "ADPWalker.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "UI/InputGlyphs.h"
#include "CrewPose.h"
#include "DriverPoseAnim.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

namespace
{
	TWeakObjectPtr<AVehicleControls> VcOne;
	const FName VcTag(TEXT("VehSolid"));
	bool VcKey(const APlayerController* PC) { return PC && (PC->IsInputKeyDown(EKeys::E) || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Left)); }
	// the move keys / the left stick, forward and back: a held lever's way
	int32 VcWay(const APlayerController* PC)
	{
		if (!PC) return 0;
		const float S = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
		return PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up) || S > 0.4f ? 1 : PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down) || S < -0.4f ? -1 : 0;
	}
	FVector VcFeet(const APawn& P) { return P.GetActorLocation() - FVector(0., 0., P.GetSimpleCollisionHalfHeight()); }
}

AVehicleControls::AVehicleControls() { PrimaryActorTick.bCanEverTick = true; }

AVehicleControls* AVehicleControls::Get(UWorld* W)
{
	if (!VcOne.IsValid() || VcOne->GetWorld() != W) { VcOne = nullptr; if (W && AGSE::Find(W)) { VcOne = W->SpawnActor<AVehicleControls>(); if (VcOne.IsValid()) VcOne->AddTickPrerequisiteActor(const_cast<AGSE*>(AGSE::Find(W))); } }
	return VcOne.Get();
}

const FVehTable* AGSE::CtlTable(int32 Svc)
{
	// (a vehicle's table: its model's name here, its lines in its own file)
	static TMap<int32, TSharedPtr<FVehTable>> All;
	if (Svc == INDEX_NONE) return nullptr;
	if (const TSharedPtr<FVehTable>* F = All.Find(Svc)) return F->Get();
	if (Svc < 0) { TSharedPtr<FVehTable> Ld = Svc == -2 ? LoaderControls() : TractorControls(); CabAdd(*Ld, Svc); return All.Add(Svc, Ld).Get(); }
	const FServiceSpec& S = ADPService::All()[Svc];
	TSharedPtr<FVehTable> Tb = S.bAmbu ? AmbuControls() : S.bFuel ? FuelControls() : S.Wet == 1 ? WaterControls() : S.Wet == 2 ? ToiletControls() : S.bCater ? CaterControls() : S.bStairs ? StairsControls() : S.bDeicer ? DeicerControls() : TSharedPtr<FVehTable>();
	if (!Tb) Tb = MakeShared<FVehTable>();
	CabAdd(*Tb, Svc);   // (the cab controls all of them share)
	return All.Add(Svc, Tb).Get();
}

FString AVehicleControls::SayOf(const AGSE& G, const AGSE::FTractor& V, const FVehControl& C)
{
	const FVehTable* Tb = AGSE::CtlTable(V.CtlKey());
	return ADPCab::Is(C.Switch) ? G.CabSay(V, C) : Tb->Say ? Tb->Say(G, V, C) : FString(C.Label);
}

bool AVehicleControls::MayOf(AGSE& G, int32 V, int32 Code, FString& Why)
{
	if (ADPCab::Is(Code)) return G.CabMay(V, Code, Why);
	const FVehTable* Tb = AGSE::CtlTable(G.Tractors[V].CtlKey());
	return !Tb->MaySwitch || Code == INDEX_NONE || Tb->MaySwitch(G, V, Code, Why);
}

void AVehicleControls::Note(UWorld* W, const FString& Why)
{
	AVehicleControls* S = Get(W); if (!S || S->Do != EDo::None) return;
	S->Text = Why; S->TextAt = W->GetTimeSeconds();
}

void AVehicleControls::CabKey(AServiceDrive* Sd, int32 Code)
{
	AVehicleControls* S = Sd ? Get(Sd->GetWorld()) : nullptr; AGSE* G = S ? const_cast<AGSE*>(AGSE::Find(Sd->GetWorld())) : nullptr; if (!G || !G->Tractors.IsValidIndex(Sd->Vehicle())) return;
	const int32 V = Sd->Vehicle(); const FVehTable* Tb = AGSE::CtlTable(G->Tractors[V].CtlKey()); if (!Tb) return;
	const int32 On = Code == ADPCab::IndL || Code == ADPCab::IndR || Code == ADPCab::Beam ? ADPCab::Ind : Code == ADPCab::GearUp || Code == ADPCab::GearDown ? ADPCab::Gear : Code;
	const int32 c = Tb->Controls.IndexOfByPredicate([On](const FVehControl& C) { return C.bSeat && C.Switch == On; });
	if (c == INDEX_NONE && Code == ADPCab::Pto) return;   // (it has none)
	FString No; if (!G->CabMay(V, Code, No)) { Note(Sd->GetWorld(), No); return; }
	// (no part of its own in this model, or your hand is busy: at once)
	if (c == INDEX_NONE || S->Do != EDo::None || SayOf(*G, G->Tractors[V], Tb->Controls[c]).IsEmpty()) { G->CabSwitch(V, Code); ADP_LOG(TEXT("Vehicle: cab control %d of the %s by its key"), Code, *G->Tractors[V].Name); return; }
	S->Ti = V; S->Ci = c; S->User = Sd; S->Do = EDo::Reach; S->T = 0.f; S->bPushed = S->bEnd = false; S->KeyCode = Code; S->Text.Reset(); S->TextAt = -1.;
}

bool AVehicleControls::Crew(const AGSE& G, int32 V) const { return G.Tractors[V].Job != AGSE::EJob::None; }

FVector AVehicleControls::At(const AGSE& G, int32 V, const FVehControl& C, FVector* Out) const
{
	const AGSE::FTractor& Tv = G.Tractors[V]; const FVehTable* Tb = AGSE::CtlTable(Tv.CtlKey()); const FTransform X = ADPCab::Is(C.Switch) ? FTransform::Identity : Tb->Frame(G, Tv, C.Frame);
	if (Out) *Out = FRotator(0.f, Tv.Yaw, 0.f).RotateVector(X.TransformVectorNoScale(C.Out)).GetSafeNormal();
	if (FVector Sp; Tb->Spot && Tb->Spot(G, Tv, C, Sp)) return Sp;
	return G.TW(Tv, X.TransformPosition(G.SvcP(Tv, C.Point)));
}

float AVehicleControls::Target(AGSE& G, const FVehControl& C, int32 Dir) const
{
	const AGSE::FTractor& Tv = G.Tractors[Ti]; const FVehTable* Tb = AGSE::CtlTable(Tv.CtlKey()); const float N = Tb->Now(Tv, C.Motion);
	float E = C.B;
	if (C.Kind == FVehControl::Push) E = FMath::Abs(Tb->Now(Tv, C.Motion) - C.A) < FMath::Abs(C.B - C.A) * 0.5f ? C.B : C.A;
	else if (C.Kind != FVehControl::Hold || C.bTurns)
	{
		// (the way you push it; else from an end to the other one, from between the other way than last time)
		const float* L = Last.Find(TPair<int32, int32>(Ti, C.Motion));
		E = Dir ? (Dir > 0 ? C.B : C.A) : FMath::IsNearlyEqual(N, C.A, 0.05f) ? C.B : FMath::IsNearlyEqual(N, C.B, 0.05f) ? C.A : L && *L == C.B ? C.A : C.B;
	}
	return Tb->End ? Tb->End(Tv, C.Motion, E) : E;
}

FString AVehicleControls::Label(AGSE& G, FString& No) const
{
	No.Reset(); if (!G.Tractors.IsValidIndex(Ti)) return FString();
	const AGSE::FTractor& Tv = G.Tractors[Ti]; const FVehTable* Tb = AGSE::CtlTable(Tv.CtlKey()); if (!Tb || !Tb->Controls.IsValidIndex(Ci)) return FString();
	const FVehControl& C = Tb->Controls[Ci]; const TCHAR* Key = ADPInput::UsingPad() ? TEXT("X") : TEXT("E");
	const FString Lb = SayOf(G, Tv, C);
	if (Crew(G, Ti) && !C.bHelp && !(C.bSeat && G.CabMine(Tv))) No = TEXT("its crew are working with it");
	else if (C.Motion != INDEX_NONE) { const float E = Target(G, C, Way); if (!FMath::IsNearlyEqual(Tb->Now(Tv, C.Motion), E, 0.01f)) Tb->May(G, Tv, C.Motion, E, No); }
	else MayOf(G, Ti, C.Switch, No);
	if (!No.IsEmpty()) return FString::Printf(TEXT("%s: %s"), *Lb, *No);
	return C.Kind == FVehControl::Push ? FString::Printf(TEXT("%s   %s"), Key, *Lb) : C.Kind == FVehControl::Hold && !C.bTurns ? FString::Printf(TEXT("Hold %s   %s"), Key, *Lb)
		: FString::Printf(TEXT("Hold %s   %s   (%s the way)"), Key, *Lb, ADPInput::UsingPad() ? TEXT("left stick up / down:") : TEXT("W / S:"));
}

FString AVehicleControls::Prompt() { return VcOne.IsValid() ? VcOne->Text : FString(); }

void AVehicleControls::ChosenParts(TArray<TPair<UInstancedStaticMeshComponent*, int32>>& Out) { if (VcOne.IsValid() && VcOne->Ci != INDEX_NONE) Out.Append(VcOne->Glow); }

void AVehicleControls::Find(AGSE& G, APawn& Me)
{
	Ti = Ci = INDEX_NONE;
	const APlayerController* PC = Cast<APlayerController>(Me.GetController()); if (!PC) return;
	const AServiceDrive* Sd = Cast<AServiceDrive>(&Me); const AADPWalker* Wk = Cast<AADPWalker>(&Me);
	if (Sd ? !Sd->Seated() || Sd->IsBehind() || Sd->Leaving() : !Wk || Wk->IsLed() || Wk->IsHidden()) return;
	FVector Eye; FRotator Look; PC->GetPlayerViewPoint(Eye, Look); const FVector Dir = Look.Vector(), From = Sd ? Eye : Me.GetPawnViewLocation();
	const float Slack = Wk && Wk->IsThirdPerson() ? 24.f : 13.f; float Best = Slack;
	for (int32 i = 0; i < G.Tractors.Num(); ++i)
	{
		if (HoldTi != INDEX_NONE && i != HoldTi) continue;   // (its thing in your hand: first back in its place)
		const AGSE::FTractor& V = G.Tractors[i]; if (!V.M.IsValid() || FVector::DistSquared2D(V.At, From) > FMath::Square(V.KHalfL + 2600.f)) continue;
		const FVehTable* Tb = AGSE::CtlTable(V.CtlKey()); if (!Tb || FVector::DistSquared2D(V.At, From) > FMath::Square(V.KHalfL + Tb->Range) || (Sd ? V.Player.Get() != Sd : V.Player.IsValid())) continue;
		for (int32 c = 0; c < Tb->Controls.Num(); ++c)
		{
			const FVehControl& C = Tb->Controls[c]; if (C.bSeat != (Sd != nullptr) || (Sd && (C.bCabin ? i != Sd->Vehicle() || !Sd->InCabin() : !Sd->AtWheel())) || SayOf(G, V, C).IsEmpty()) continue;
			FVector Out; const FVector P = At(G, i, C, &Out), D = P - Eye;
			// in reach of your arm (bending down to a low one), from its own side, nearest to where you look
			const float Flat = FVector::Dist2D(P, From), Dz = float(From.Z - P.Z); if (Flat > (Sd ? 150.f : 125.f) || Dz > 150.f || Dz < -75.f) continue;
			if (!C.Out.IsNearlyZero() && FVector::DotProduct(From - P, Out) <= 0.) continue;
			const float Along = FVector::DotProduct(D, Dir); if (Along < 10.f) continue;
			const float Off = float((D - Dir * Along).Size()); if (Off < Best) { Best = Off; Ti = i; Ci = c; }
		}
	}
}

bool AVehicleControls::Interact(APawn* Who)
{
	AVehicleControls* S = Who ? Get(Who->GetWorld()) : nullptr; AGSE* G = S ? const_cast<AGSE*>(AGSE::Find(Who->GetWorld())) : nullptr; if (!G) return false;
	if (S->Do != EDo::None) return true;
	S->Find(*G, *Who);
	if (S->Ci == INDEX_NONE)
	{
		if (S->HoldTi == INDEX_NONE || !G->Tractors.IsValidIndex(S->HoldTi)) return false;
		const FVehTable* Th = AGSE::CtlTable(G->Tractors[S->HoldTi].CtlKey()); const TCHAR* Nm = Th && Th->Held ? Th->Held(G->Tractors[S->HoldTi]) : nullptr; if (!Nm) return false;
		FString Not; if (Th->HandSay && !Th->HandSay(*G, G->Tractors[S->HoldTi], Not).IsEmpty()) { S->bSqueeze = true; return true; }
		S->Text = FString::Printf(TEXT("You have %s in your hand - put it back first"), Nm); S->TextAt = Who->GetWorld()->GetTimeSeconds(); return true;
	}
	S->Way = 0; FString No; const FString L = S->Label(*G, No);
	const bool bSeat = Who->IsA<AServiceDrive>();
	if (!No.IsEmpty() && (S->Crew(*G, S->Ti) || AGSE::CtlTable(G->Tractors[S->Ti].CtlKey())->Controls[S->Ci].Kind == FVehControl::Push)) { S->Text = L; S->TextAt = Who->GetWorld()->GetTimeSeconds(); return !bSeat || ADPCab::Is(AGSE::CtlTable(G->Tractors[S->Ti].CtlKey())->Controls[S->Ci].Switch); }
	S->User = Who; S->KeyCode = INDEX_NONE; S->Do = EDo::Reach; S->T = 0.f; S->bPushed = S->bEnd = false;
	if (AADPWalker* Wk = Cast<AADPWalker>(Who)) { Wk->SetLed(true); S->bLedByMe = true; const FVehControl& Cc = AGSE::CtlTable(G->Tractors[S->Ti].CtlKey())->Controls[S->Ci]; Wk->SetPoint(Cc.Kind <= FVehControl::Hold && !Cc.bGrip); }
	return true;
}

void AVehicleControls::Stop(AGSE& G)
{
	// let go: what it moved stops where it is (a push's own motion runs on)
	const FVehTable* Tb = G.Tractors.IsValidIndex(Ti) ? AGSE::CtlTable(G.Tractors[Ti].CtlKey()) : nullptr;
	if (Tb && Tb->Controls.IsValidIndex(Ci) && Do == EDo::Work)
	{
		const FVehControl& C = Tb->Controls[Ci]; AGSE::FTractor& V = G.Tractors[Ti];
		if (C.Motion != INDEX_NONE && C.Kind != FVehControl::Push && !Crew(G, Ti)) Tb->Want(V, C.Motion, Tb->Now(V, C.Motion));
	}
	if (Do == EDo::Reach || Do == EDo::Work) { Do = EDo::Back; T = 0.f; }
}

void AVehicleControls::Arm(AGSE& G, APawn* Me)
{
	const FVehTable* Tb = G.Tractors.IsValidIndex(Ti) ? AGSE::CtlTable(G.Tractors[Ti].CtlKey()) : nullptr; if (!Tb || !Tb->Controls.IsValidIndex(Ci)) return;
	const FVehControl& C = Tb->Controls[Ci]; FVector Out; FVector P = At(G, Ti, C, &Out);
	if (AServiceDrive* Sd = Cast<AServiceDrive>(Me)) { Sd->SetReach(W, P); return; }
	AADPWalker* Wk = Cast<AADPWalker>(Me); if (!Wk) return;
	// a button: the fingertip on it, pushing in; a lever or a handle: the palm round it
	const FVector Sh = Wk->ShoulderNow(); FVector F = (P - Sh).GetSafeNormal(); if (!Out.IsNearlyZero()) F = FMath::Lerp(F, -Out, 0.5f).GetSafeNormal();
	const bool bBtn = C.Kind <= FVehControl::Hold && !C.bGrip; const float In = bBtn && Do == EDo::Work ? 0.5f : 0.f;
	const FVector Thumb = FVector::VectorPlaneProject(FVector::UpVector, F).GetSafeNormal();
	if (HoldTi != INDEX_NONE && Tb->GripL) { Wk->SetArm(1.f, HoldAt - HoldF * 8.5f, HoldF, FVector::UpVector); return; }   // (both hands are on it: it goes there with them)
	if (HoldTi != INDEX_NONE) { Wk->SetArm(1.f, FMath::Lerp(HoldAt - HoldF * 8.5f, P - F * 8.5f, W), HoldF, FVector::UpVector); return; }   // (with what you carry)
	Wk->SetArm(W, P - F * ((bBtn ? 18.f : 8.5f) - In), F, Thumb);
}

void AVehicleControls::Tick(float Dt)
{
	Super::Tick(Dt);
	AGSE* G = const_cast<AGSE*>(AGSE::Find(GetWorld())); APlayerController* PC = GetWorld()->GetFirstPlayerController(); APawn* Me = PC ? PC->GetPawn() : nullptr; if (!G) return;
	Hull(*G, Me, Dt); Carry(*G, Me);
	if (Do != EDo::None && (!User.IsValid() || User.Get() != Me || !G->Tractors.IsValidIndex(Ti)))
	{
		Stop(*G); Do = EDo::None; W = 0.f;
		if (AADPWalker* Wk = Cast<AADPWalker>(User.Get())) { if (bClimbing) ClimbEnd(*Wk); Wk->SetArm(0.f, FVector::ZeroVector, FVector::ForwardVector, FVector::UpVector); Wk->SetPoint(false); if (bLedByMe) Wk->SetLed(false); }
		bLedByMe = false;
	}
	if (!Me) { Text.Reset(); return; }
	if (Do == EDo::None)
	{
		if (GetWorld()->GetTimeSeconds() - TextAt < 1.5 && TextAt >= 0.) return;   // (a refusal stays to be read)
		Way = 0; Find(*G, *Me); FString No; Text = Ci == INDEX_NONE ? FString() : Label(*G, No);
		if ((Ci == INDEX_NONE || bSqueeze) && HoldTi != INDEX_NONE)
		{
			const FVehTable* Th = AGSE::CtlTable(G->Tractors[HoldTi].CtlKey()); const FString S = Th && Th->HandSay ? Th->HandSay(*G, G->Tractors[HoldTi], No = FString()) : FString();
			if (!S.IsEmpty()) Text = !No.IsEmpty() ? FString::Printf(TEXT("%s: %s"), *S, *No) : bSqueeze ? S : FString::Printf(TEXT("%s   %s"), ADPInput::UsingPad() ? TEXT("X") : TEXT("E"), *S);
		}
		if (Text.IsEmpty()) for (const AGSE::FTractor& V : G->Tractors) if (V.YouOn && GetWorld()->GetTimeSeconds() - V.YouStopAt < 0.5) { Text = FString::Printf(TEXT("The %s waits: you are standing on it"), *V.Name); break; }
	}
	else if (Do == EDo::Climb) { Text.Reset(); AADPWalker* Wk = Cast<AADPWalker>(Me); if (!Wk || !Climb(*G, *Wk, Dt)) { Do = EDo::Back; T = 1.f; W = 0.f; } }
	else
	{
		const FVehTable* Tb = AGSE::CtlTable(G->Tractors[Ti].CtlKey()); const FVehControl& C = Tb->Controls[Ci]; AGSE::FTractor& V = G->Tractors[Ti];
		T += Dt;
		const bool bCrew = Crew(*G, Ti) && !C.bHelp && !(C.bSeat && G->CabMine(V)), bHeld = VcKey(PC), bFar = !Cast<AServiceDrive>(Me) && FVector::Dist2D(At(*G, Ti, C), Me->GetPawnViewLocation()) > 190.f;   // (seated: in its cab; the pawn's own place is the vehicle's middle)
		if (Do == EDo::Reach)
		{
			W = FMath::SmoothStep(0.f, 0.3f, T);
			if (T >= 0.3f) { Do = EDo::Work; T = 0.f; }
		}
		else if (Do == EDo::Work)
		{
			W = 1.f; FString No; Way = VcWay(PC);
			if (bCrew || bFar) Stop(*G);
			else if (C.Kind == FVehControl::Push)
			{
				if (!bPushed)
				{
					const int32 Code = KeyCode != INDEX_NONE ? KeyCode : C.Switch; const bool bCab = ADPCab::Is(Code);
					bPushed = true; if (Tb->Press && (!bCab || FCString::Strncmp(C.Part, TEXT("Button_"), 7) == 0)) Tb->Press(*G, Ti, C);
					if (C.Motion != INDEX_NONE) { const float E = Target(*G, C, 0); if (Tb->May(*G, V, C.Motion, E, No)) { Tb->Want(V, C.Motion, E); Last.Add(TPair<int32, int32>(Ti, C.Motion), E); } }
					else if (!MayOf(*G, Ti, Code, No)) {}
					else if (bCab) G->CabSwitch(Ti, Code);
					else if (TFunction<void(float, float, FDriverPose&)> Fn = Tb->Climb && Me->IsA<AADPWalker>() ? Tb->Climb(*G, Ti, C.Switch, ClimbS) : nullptr) { ClimbFn = MoveTemp(Fn); Do = EDo::Climb; T = 0.f; W = 0.f; Cast<AADPWalker>(Me)->SetPoint(false); Cast<AADPWalker>(Me)->SetArm(0.f, FVector::ZeroVector, FVector::ForwardVector, FVector::UpVector); }
					else if (Tb->Switch) Tb->Switch(*G, Ti, C.Switch);
					ADP_LOG(TEXT("Vehicle: you press '%s' of the %s%s"), C.Label, *V.Name, No.IsEmpty() ? TEXT("") : *(TEXT(" - ") + No));
				}
				if (T > 0.3f && Do == EDo::Work) Stop(*G);
			}
			else if (!bHeld) Stop(*G);
			else
			{
				// (the way is fixed as it is taken: at the end it stops there, not back again)
				if (To == 0.f && !bPushed) ADP_LOG(TEXT("Vehicle: you work '%s' of the %s"), C.Label, *V.Name);
				if (Way != 0 || !bPushed) { const float E = Target(*G, C, Way); if (E != To || !bPushed) { To = E; bEnd = false; } bPushed = true; }
				if (Tb->Press) Tb->Press(*G, Ti, C);
				const float N = Tb->Now(V, C.Motion);
				if (FMath::IsNearlyEqual(N, To, 0.01f)) { if (!bEnd) { bEnd = true; Last.Add(TPair<int32, int32>(Ti, C.Motion), To); if (Tb->Worked) Tb->Worked(*G, Ti, C.Motion); } Tb->Want(V, C.Motion, To); Text = C.Label; }
				else if (Tb->May(*G, V, C.Motion, To, No)) { Tb->Want(V, C.Motion, To); Last.Add(TPair<int32, int32>(Ti, C.Motion), To); Text = C.Label; }
				else { Tb->Want(V, C.Motion, N); Text = FString::Printf(TEXT("%s: %s"), C.Label, *No); }
			}
		}
		else
		{
			W = 1.f - FMath::SmoothStep(0.f, 0.3f, T);
			if (T >= 0.3f)
			{
				Do = EDo::None; W = 0.f; To = 0.f;
				if (AADPWalker* Wk = Cast<AADPWalker>(Me)) { Wk->SetPoint(false); if (bLedByMe) Wk->SetLed(false); }
				bLedByMe = false;
			}
		}
		if (Ci != INDEX_NONE && Do != EDo::Climb) Arm(*G, Me);
	}
	if (GlowTi != Ti || GlowCi != Ci)
	{
		GlowTi = Ti; GlowCi = Ci; Glow.Reset();
		if (Ci != INDEX_NONE)
		{
			const FVehControl& C = AGSE::CtlTable(G->Tractors[Ti].CtlKey())->Controls[Ci];
			TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> Fd; if (C.Kind != FVehControl::Hand && *C.Part) G->Tractors[Ti].M->InstancesNamed(C.Part, true, Fd);
			const FString Pn(C.Part); for (const auto& F : Fd) { const FString& N = F.Get<2>(); if (N.Len() == Pn.Len() || !FChar::IsAlpha(N[Pn.Len()])) Glow.Emplace(F.Get<0>(), F.Get<1>()); }
		}
	}
}

void AVehicleControls::Hull(AGSE& G, APawn* Me, float Dt)
{
	// solid piece by piece while you are by it and it stands; else its one box. What you stand on carries you (the pieces are moved,
	// your movement follows its base); it never drives with you on it
	for (int32 i = 0; i < G.Tractors.Num(); ++i)
	{
		AGSE::FTractor& V = G.Tractors[i]; const FVehTable* Tb = AGSE::CtlTable(V.CtlKey()); ABabylonModel* M = V.M.Get(); if (!Tb || !M) continue;
		AADPWalker* Wk = Cast<AADPWalker>(Me); const bool bFoot = Wk && !Wk->IsHidden();
		const FVector L = bFoot ? FRotator(0.f, V.Yaw, 0.f).UnrotateVector(VcFeet(*Wk) - V.At) : FVector(1e6);
		V.YouOn = bFoot && Tb->Aboard && FMath::Abs(L.X) < V.KHalfL + 300.f && FMath::Abs(L.Y) < V.KHalfW + 50.f ? Tb->Aboard(V, L) : 0; V.YouAt = L;
		// left standing open: after two minutes without you it stows itself (nobody could take it for a flight)
		const float Away = Me ? float(FVector::Dist2D(Me->GetActorLocation(), V.At)) : 1e6f;
		if (V.Job == AGSE::EJob::None && !V.Player.IsValid() && Away > 4000.f && !Tb->Stowed(G, V)) { V.LeftOpen += Dt; if (V.LeftOpen > 120.f) { Tb->Stow(G, V); V.LeftOpen = 100.f; ADP_LOG(TEXT("Vehicle: the %s was left open - it stows itself"), *V.Name); } }
		else V.LeftOpen = 0.f;
		if (!Tb->Solids.Num()) continue;
		FHull& H = Hulls.FindOrAdd(i);
		const bool bOn = V.YouOn || (bFoot && Away < V.KHalfL + 900.f && V.Speed < 1.f && !V.Path.Num() && !V.Player.IsValid() && (H.bOn || Tb->bAlways || !Tb->Stowed(G, V)));
		if (bOn != H.bOn)
		{
			H.bOn = bOn;
			if (!H.Box.Num())
			{
				for (UActorComponent* K : M->GetComponents()) if (UBoxComponent* B = Cast<UBoxComponent>(K)) if (!B->ComponentHasTag(VcTag) && B->GetCollisionEnabled() != ECollisionEnabled::NoCollision) { H.Whole = B; break; }
				for (const FVehSolid& S : Tb->Solids)
				{
					UBoxComponent* B = NewObject<UBoxComponent>(M); B->SetupAttachment(M->GetRootComponent()); B->SetBoxExtent((S.Max - S.Min) * 0.5); B->ComponentTags.Add(VcTag);
					B->SetCollisionEnabled(ECollisionEnabled::NoCollision); B->SetCollisionObjectType(ECC_WorldDynamic); B->SetCollisionResponseToAllChannels(ECR_Ignore); B->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block); B->SetCanEverAffectNavigation(false);
					B->RegisterComponent(); M->AddInstanceComponent(B); H.Box.Add(B); H.On.Add(false);
				}
				if (Wk) Wk->GetCharacterMovement()->AddTickPrerequisiteActor(this);
			}
			if (UBoxComponent* B = H.Whole.Get()) B->SetCollisionEnabled(bOn ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
		}
		for (int32 k = 0; k < H.Box.Num(); ++k)
		{
			UBoxComponent* B = H.Box[k].Get(); if (!B) continue; const FVehSolid& S = Tb->Solids[k];
			const bool bIs = H.bOn && (!S.On || S.On(V));
			if (bIs || H.On[k])
			{
				const FTransform X = FTransform((S.Min + S.Max) * 0.5) * Tb->Frame(G, V, S.Frame);
				if (!B->GetRelativeTransform().Equals(X, 0.0005f)) B->SetRelativeLocationAndRotation(X.GetLocation(), X.GetRotation());
			}
			if (bIs != H.On[k]) { H.On[k] = bIs; B->SetCollisionEnabled(bIs ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision); }
		}
	}
}

void AVehicleControls::Carry(AGSE& G, APawn* Me)
{
	const int32 Was = HoldTi; HoldTi = INDEX_NONE; AADPWalker* Wk = Cast<AADPWalker>(Me); const bool bFoot = Wk && !Wk->IsHidden() && Wk->GetController();
	for (int32 i = 0; i < G.Tractors.Num(); ++i)
	{
		AGSE::FTractor& V = G.Tractors[i]; const FVehTable* Tb = AGSE::CtlTable(V.CtlKey()); if (!Tb || !Tb->Held || !V.M.IsValid() || !Tb->Held(V)) continue;
		if (!bFoot) { Tb->Drop(V); continue; }
		HoldTi = i;
		const FRotator Look = Wk->GetControlRotation(); const FVector Flat = FRotator(0.f, Look.Yaw, 0.f).Vector(), Sd = FRotator(0.f, Look.Yaw, 0.f).RotateVector(FVector::RightVector), Eye = Wk->GetPawnViewLocation();
		// (in front of you in your right hand, on the climb nearer to you)
		HoldAt = bClimbing ? Wk->GetActorLocation() + Wk->GetActorForwardVector() * 30.f + Wk->GetActorRightVector() * 24.f : Wk->IsThirdPerson() ? Wk->GetActorLocation() + Flat * 36.f + Sd * 20.f + FVector(0., 0., 28.) : Eye + Look.Vector() * 40.f + Sd * 17.f - FVector(0., 0., 22.);
		if (const float Pk = Tb->Pocket ? Tb->Pocket(V) : 0.f; Pk > 0.f) HoldAt = FMath::Lerp(HoldAt, Wk->GetActorLocation() + Flat * 12.f + Sd * 24.f - FVector(0., 0., 6.), double(FMath::SmoothStep(0.f, 1.f, Pk)));   // (into your pocket)
		HoldF = (Flat * 0.8f + FVector(0., 0., 0.2)).GetSafeNormal();
		FVector Gp; const bool bGrip = Tb->Grip && Tb->Grip(G, V, Gp); if (bGrip) HoldAt = G.TW(V, Gp);   // (a trolley: your hand is on its handle)
		FVector P = HoldAt; if (!bGrip && Ti == i && Do != EDo::None && Do != EDo::Climb && Tb->Controls.IsValidIndex(Ci)) P = FMath::Lerp(HoldAt, Tb->Controls[Ci].Motion == INDEX_NONE ? At(G, i, Tb->Controls[Ci]) : HoldAt - Sd * 37.f, W);   // (a lever worked with the thing kept in your left hand)
		V.YouHand = FTransform(FRotator(0.f, V.Yaw, 0.f), V.At).InverseTransformPosition(P);
		if (Do == EDo::None) Wk->SetArm(1.f, HoldAt - HoldF * 8.5f, HoldF, FVector::UpVector);
		if (FVector Lh; Tb->GripL && Tb->GripL(G, V, Lh)) Wk->SetArmL(1.f, G.TW(V, Lh) - HoldF * 8.5f);
		bSqueeze = bSqueeze && Do == EDo::None && VcKey(Cast<APlayerController>(Wk->GetController())); if (Tb->HandDo) Tb->HandDo(G, V, bSqueeze);
		// its hose or cable is at its end: you get no further with it
		FVector From; const float Max = Tb->Leash && !bClimbing ? Tb->Leash(G, V, From) : 0.f;
		if (Max > 0.f)
		{
			FVector D = P - G.TW(V, From); if (bGrip) { D = Wk->GetActorLocation() - G.TW(V, From); D.Z = 0.; }
			UCharacterMovementComponent* Mv = Wk->GetCharacterMovement();
			if (D.Size() > Max && Mv) { const FVector N = D.GetSafeNormal2D(); const double Off = FVector::DotProduct(Mv->Velocity, N); Mv->Velocity -= N * (FMath::Max(Off, 0.) + 30.); if (Do == EDo::None) { Text = TEXT("It reaches no further"); TextAt = GetWorld()->GetTimeSeconds() - 1.; } }
		}
		break;
	}
	if (HoldTi == INDEX_NONE) { bSqueeze = false; if (Was != INDEX_NONE && Wk) Wk->SetArmL(0.f, FVector::ZeroVector); }
	if (Was != INDEX_NONE && HoldTi == INDEX_NONE && Do == EDo::None && Wk) Wk->SetArm(0.f, FVector::ZeroVector, FVector::ForwardVector, FVector::UpVector);
}

bool AVehicleControls::Climb(AGSE& G, AADPWalker& Wk, float Dt)
{
	// up or down its steps as its crew climb them: first to where the climb begins, then your body by the same poses
	float Ph = 95.f; if (Rig.IsReady()) Ph = Rig.PelvisHeight();
	const float Hh = Wk.GetSimpleCollisionHalfHeight(); T += Dt;
	if (!bClimbing)
	{
		FDriverPose P; ClimbFn(0.f, Ph, P); FVector To2 = P.Pelvis - Wk.GetActorLocation(); To2.Z = 0.;
		if (To2.Size() > 6.f)
		{
			if (T > 2.5f) { Text = TEXT("You can't get to the steps from here"); TextAt = GetWorld()->GetTimeSeconds(); return false; }
			Wk.AddActorWorldOffset(To2.GetClampedToMaxSize(130.f * Dt), true); return true;
		}
		USkeletalMesh* Own = Wk.BodyMesh();
		if (!Rider && Own)
		{
			Rider = NewObject<UPoseableMeshComponent>(this); if (!RootComponent) { SetRootComponent(NewObject<USceneComponent>(this)); RootComponent->RegisterComponent(); }
			Rider->SetupAttachment(RootComponent); Rider->SetUsingAbsoluteLocation(true); Rider->SetUsingAbsoluteRotation(true); Rider->SetCollisionEnabled(ECollisionEnabled::NoCollision); Rider->RegisterComponent();
			Rider->SetSkinnedAssetAndUpdate(Own); Rider->SetVisibility(false); if (!Rig.Init(Rider)) ADP_ERROR(TEXT("Vehicle: your body couldn't be rigged for the steps"));
		}
		Wk.GetCharacterMovement()->DisableMovement(); bClimbing = true; T = 0.f; Human = nullptr;
		ADP_LOG(TEXT("Vehicle: you climb the steps of the %s"), G.Tractors.IsValidIndex(Ti) ? *G.Tractors[Ti].Name : TEXT("?"));
	}
	const float K = FMath::Clamp(T / FMath::Max(ClimbS, 1.f), 0.f, 1.f); FDriverPose P; ClimbFn(K, Ph, P);
	const FVector Fw = P.Facing.GetSafeNormal2D().IsNearlyZero() ? FVector(1, 0, 0) : P.Facing.GetSafeNormal2D();
	const float Z = float(FMath::Min(P.Foot[0].Z, P.Foot[1].Z));
	const FTransform Frame(FRotator(0.f, Fw.Rotation().Yaw - 90.f, 0.f), FVector(P.Pelvis.X, P.Pelvis.Y, Z));
	Wk.SetActorLocation(FVector(P.Pelvis.X, P.Pelvis.Y, P.Pelvis.Z - Ph + Hh + 2.f));
	if (Rider && Rig.IsReady())
	{
		P.Facing = FVector(0, 1, 0); P.Pelvis = Frame.InverseTransformPosition(P.Pelvis);
		for (int32 s = 0; s < 2; ++s) { P.Foot[s] = Frame.InverseTransformPosition(P.Foot[s]); P.Hand[s] = Frame.InverseTransformPosition(P.Hand[s]); }
		Rider->SetWorldLocationAndRotation(Frame.GetLocation(), Frame.GetRotation()); Rig.Pose(P);
		if (!Human) { Human = Wk.LendBody(Rider); if (Human) Wk.ShowHead(Wk.IsThirdPerson()); }
		if (Human) Human->Local = Rig.LocalPose();
	}
	if (K < 1.f) return true;
	ClimbEnd(Wk); return false;
}

void AVehicleControls::ClimbEnd(AADPWalker& Wk)
{
	bClimbing = false; if (Human) Wk.TakeBodyBack(); Human = nullptr;
	if (Wk.GetCharacterMovement()->MovementMode == MOVE_None) Wk.GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}
