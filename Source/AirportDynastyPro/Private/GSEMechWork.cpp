// Hangar step 4 (owner GO 2026-11-27, docs/Hired-Crew-Plan.md "Mechanics"): a check's hired mechanics work at the fault
// itself. Where: Data/aircraft-workpoints.json (Tools/make_aircraft_workpoints.py) - per type, the places to stand for each
// area (each main wheel, the nose gear, each flap, each engine, the cargo doors, the avionics bay, the tail), each with
// its facing, its hands' height and its role in the pair (one holds, one fastens), and the keep-out boxes nobody walks
// through (gear legs with their wheels, engines). A type without its own entry takes the entry of the model it is drawn
// with (aircraft-types.json "model"), else the A320's scaled to the model's real size with its engine count - logged
// once, so a type added later works without code. The fault (ADPMaint's table) names the areas; a single fault one side
// of them (by the registration), a routine check all of them. The work itself: a simple pose loop on hooks (the holder's
// hands steady at the part, the fastener's hand turning a tool beside them); the full animation can come later.
#include "GSE.h"
#include "Tugs.h"
#include "CrewPose.h"
#include "TruckDriverRig.h"
#include "BabylonModel.h"
#include "ADPLog.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPMaint.h"
#include "Sim/ADPWorldData.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	struct FMwPoint { FString Area, Side; FVector At = FVector::ZeroVector; float Face = 0.f, Hands = 1.f; bool bHold = true; };
	struct FMwKeep { FString Name; FVector2D C = FVector2D::ZeroVector, Half = FVector2D::ZeroVector; };
	struct FMwType { FVector BoxMin = FVector::ZeroVector, BoxMax = FVector::ZeroVector; TArray<FMwPoint> Pts; TArray<FMwKeep> Keep; };
	constexpr float MwApart = 75.f;   // (cm: two people's places never closer)
	constexpr float MwPad = 30.f;     // (cm: a place never inside a vehicle's outline and this much)

	const TMap<FString, FMwType>& MwTypes()
	{
		static TMap<FString, FMwType> Known; static bool bRead = false;
		if (bRead) return Known;
		bRead = true;
		FString Text; TSharedPtr<FJsonObject> J;
		if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/aircraft-workpoints.json"))) || FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data/aircraft-workpoints.json"))))
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J);
		if (!J) { ADP_WARN(TEXT("GSE: Data/aircraft-workpoints.json cannot be read - the mechanics stand by the nose")); return Known; }
		auto V3 = [](const TArray<TSharedPtr<FJsonValue>>* A) { return A && A->Num() >= 3 ? FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber()) : FVector::ZeroVector; };
		for (const TPair<FString, TSharedPtr<FJsonValue>>& E : J->Values)
		{
			const TSharedPtr<FJsonObject> T = E.Value.IsValid() ? E.Value->AsObject() : nullptr; if (!T) continue;
			const TSharedPtr<FJsonObject>* Bx = nullptr; const TSharedPtr<FJsonObject>* Ar = nullptr; const TArray<TSharedPtr<FJsonValue>>* Ks = nullptr;
			if (!T->TryGetObjectField(TEXT("areas"), Ar)) continue;
			FMwType& Ty = Known.Add(E.Key);
			if (T->TryGetObjectField(TEXT("box"), Bx))
			{
				const TArray<TSharedPtr<FJsonValue>> *Mn = nullptr, *Mx = nullptr;
				(*Bx)->TryGetArrayField(TEXT("min"), Mn); (*Bx)->TryGetArrayField(TEXT("max"), Mx);
				Ty.BoxMin = V3(Mn); Ty.BoxMax = V3(Mx);
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& A : (*Ar)->Values)
				for (const TSharedPtr<FJsonValue>& Pv : A.Value->AsArray())
				{
					const TSharedPtr<FJsonObject> P = Pv->AsObject(); if (!P) continue;
					const TArray<TSharedPtr<FJsonValue>>* At = nullptr; P->TryGetArrayField(TEXT("at"), At);
					FMwPoint W; W.Area = A.Key; W.Side = P->GetStringField(TEXT("side")); W.At = V3(At);
					double F = 0.0, Hn = 1.0; P->TryGetNumberField(TEXT("face"), F); P->TryGetNumberField(TEXT("hands"), Hn);
					W.Face = float(F); W.Hands = float(Hn); W.bHold = P->GetStringField(TEXT("role")) != TEXT("fasten");
					Ty.Pts.Add(W);
				}
			if (T->TryGetArrayField(TEXT("keepout"), Ks))
				for (const TSharedPtr<FJsonValue>& Kv : *Ks)
				{
					const TSharedPtr<FJsonObject> K = Kv->AsObject(); const TArray<TSharedPtr<FJsonValue>> *C = nullptr, *H = nullptr;
					if (!K || !K->TryGetArrayField(TEXT("c"), C) || !K->TryGetArrayField(TEXT("half"), H) || C->Num() < 2 || H->Num() < 2) continue;
					Ty.Keep.Add({K->GetStringField(TEXT("name")), FVector2D((*C)[0]->AsNumber(), (*C)[1]->AsNumber()), FVector2D((*H)[0]->AsNumber(), (*H)[1]->AsNumber())});
				}
		}
		ADP_LOG(TEXT("GSE: aircraft work points for %d types read"), Known.Num());
		return Known;
	}

	/** The model's own size now (its parts, in its own frame, cm) - as Data/aircraft-doors.json's reader does. */
	FBox MwModelBox(const AActor* Pl)
	{
		FBox B(ForceInit);
		const ABabylonModel* M = Cast<ABabylonModel>(Pl); if (!M) return B;
		TArray<UInstancedStaticMeshComponent*> Ps; M->Parts(Ps);
		for (const UInstancedStaticMeshComponent* K : Ps) if (K && K->GetInstanceCount() > 0) B += K->CalcBounds(K->GetComponentTransform().GetRelativeTransform(Pl->GetActorTransform())).GetBox();
		return B;
	}
}

void AGSE::WalkOutlines(TArray<FOutline>& Out) const
{
	Outlines(Out);
	for (const TPair<FString, FMechCrew>& K : MechCrews) if (K.Value.Phase < 4) Out.Append(K.Value.Keep);
}

bool AGSE::MechSpots(const FString& Reg, const FHallStair& H, int32 Want, TArray<FMechSpot>& Spots, TArray<FOutline>& Keep) const
{
	Spots.Reset(); Keep.Reset();
	const UADPSim* Sim = UADPSim::Get(GetWorld());
	if (!H.Plane.IsValid() || !Sim) { ADP_WARN(TEXT("GSE: %s in hangar %s - no aircraft model to work at, the mechanics stand by the nose"), *Reg, *H.Hall); return false; }
	const FADPAircraft* Ac = Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Reg; });
	return MechSpotsAt(Reg, H.Plane.Get(), Ac ? Ac->CheckFault : FString(), Want, Spots, Keep);
}

bool AGSE::MechSpotsAt(const FString& Reg, const AActor* Pl, const FString& FaultKey, int32 Want, TArray<FMechSpot>& Spots, TArray<FOutline>& Keep) const
{
	Spots.Reset(); Keep.Reset();
	const UADPSim* Sim = UADPSim::Get(GetWorld()); if (!Pl || !Sim) return false;
	const FADPAircraft* Ac = Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Reg; });
	const ADPMaint::FFault* Ft = ADPMaint::Fault(FaultKey);
	if (!Ac || !Ft) { ADP_WARN(TEXT("GSE: %s's job has no known fault (%s) - nobody works at its parts"), *Reg, *FaultKey); return false; }
	// the type's entry; else the model it is drawn with; else the A320's to the model's real size (logged once each)
	static TSet<FString> Told;
	const TMap<FString, FMwType>& All = MwTypes();
	const FADPTypeInfo* Ti = ADPWorldData::Type(Ac->TypeId);
	const FString Model = Ti && !Ti->Model.IsEmpty() ? Ti->Model : FString(TEXT("A320"));
	const FMwType* Ty = nullptr; bool bDerived = false;
	if (Model == Ac->TypeId || !All.Contains(Model)) Ty = All.Find(Ac->TypeId);
	if (!Ty && (Ty = All.Find(Model)) != nullptr && !Told.Contains(Ac->TypeId)) { Told.Add(Ac->TypeId); ADP_LOG(TEXT("GSE: %s is drawn as the %s model - its work points"), *Ac->TypeId, *Model); }
	if (!Ty && (Ty = All.Find(TEXT("A320"))) != nullptr)
	{
		bDerived = true;
		if (!Told.Contains(Ac->TypeId)) { Told.Add(Ac->TypeId); ADP_LOG(TEXT("GSE: %s has no work points in Data/aircraft-workpoints.json - the A320's, to its model's size and %d engines"), *Ac->TypeId, Ti ? Ti->Engines : 2); }
	}
	if (!Ty || !Ty->Pts.Num()) return false;
	// (to the model's own size where it differs from the one the points were measured on)
	FVector R(1.f);
	const FBox Mb = MwModelBox(Pl); const FVector Was = (Ty->BoxMax - Ty->BoxMin) * 100.f;
	if (Mb.IsValid) for (int32 a = 0; a < 3; ++a)
	{
		const double Is = Mb.Max[a] - Mb.Min[a], Q = Was[a] > 1.0 ? Is / Was[a] : 1.0;
		if (FMath::Abs(Q - 1.0) >= 0.02 && (bDerived || (Q > 0.6 && Q < 1.6))) R[a] = float(Q);
	}
	const FTransform X = Pl->GetActorTransform();
	auto W = [&](const FVector& Lm) { return X.TransformPosition(FVector(Lm.X * R.X, Lm.Y * R.Y, Lm.Z * R.Z) * 100.f); };
	auto Dir = [&](float Deg) { const float A = FMath::DegreesToRadians(Deg); return X.TransformVectorNoScale(FVector(FMath::Sin(A), -FMath::Cos(A), 0.f)).GetSafeNormal2D(); };
	// four engines on a two-engine entry (derived only): the outer pair as far out again as the inner is from its centre
	const bool bFour = bDerived && Ti && Ti->Engines >= 4;
	TArray<FMwPoint> Pts = Ty->Pts; TArray<FMwKeep> Ks = Ty->Keep;
	if (bFour)
	{
		for (const FMwPoint& P : Ty->Pts) if (P.Area == TEXT("engine")) { FMwPoint Q = P; Q.At.X += FMath::Sign(P.At.X) * 4.5f; Q.Side += TEXT("2"); Pts.Add(Q); }
		for (const FMwKeep& K : Ty->Keep) if (K.Name.Contains(TEXT("engine"))) { FMwKeep Q = K; Q.C.X += FMath::Sign(K.C.X) * 4.5; Q.Name += TEXT(" (outer)"); Ks.Add(Q); }
	}
	for (const FMwKeep& K : Ks)
	{
		const FVector C = W(FVector(K.C.X, K.C.Y, 0.0)), Ax = X.TransformVectorNoScale(FVector(1, 0, 0)).GetSafeNormal2D();
		Keep.Add({C, Ax, FVector(-Ax.Y, Ax.X, 0.f), float(K.Half.X * R.X * 100.0), float(K.Half.Y * R.Y * 100.0)});
	}
	// the fault's areas; a single fault one side of them (the same side for the same aircraft), a routine check all
	TArray<FString> Areas; FString(Ft->Areas).ParseIntoArray(Areas, TEXT(","));
	const bool bRoutine = FaultKey == TEXT("acheck") || FaultKey == TEXT("ccheck") || FaultKey == TEXT("dcheck");
	TArray<TArray<const FMwPoint*>> Groups;
	for (const FString& Ar : Areas)
	{
		TArray<FString> Sides; for (const FMwPoint& P : Pts) if (P.Area == Ar) Sides.AddUnique(P.Side);
		if (!Sides.Num()) { ADP_WARN(TEXT("GSE: %s - no work points for its %s"), *Ac->TypeId, *Ar); continue; }
		Sides.Sort();
		const int32 Pick = bRoutine ? INDEX_NONE : int32(GetTypeHash(Reg) % uint32(Sides.Num()));
		for (int32 s = 0; s < Sides.Num(); ++s)
		{
			if (Pick != INDEX_NONE && s != Pick) continue;
			TArray<const FMwPoint*>& G = Groups.AddDefaulted_GetRef();
			for (const FMwPoint& P : Pts) if (P.Area == Ar && P.Side == Sides[s]) G.Add(&P);
		}
	}
	// pairs by turns over the groups (a holder and a fastener together), never on a place another stands at or inside a
	// vehicle's outline (the hall's stair)
	TArray<FOutline> Veh; Outlines(Veh);
	auto Free = [&](const FVector& P)
	{
		for (const FMechSpot& S : Spots) if (FVector::Dist2D(S.At, P) < MwApart) return false;
		if (MechMe.Reg == Reg && FVector::Dist2D(MechMe.S.At, P) < MwApart) return false;   // (your place: GSEMechYou.cpp)
		for (const FOutline& O : Veh)
		{
			const FVector D = P - O.C;
			if (FMath::Abs(FVector::DotProduct(D, O.X)) < O.Hx + MwPad && FMath::Abs(FVector::DotProduct(D, O.Y)) < O.Hy + MwPad) return false;
		}
		return true;
	};
	for (int32 From = 0; Spots.Num() < Want; From += 2)
	{
		bool bAny = false;
		for (const TArray<const FMwPoint*>& G : Groups)
			for (int32 i = From; i < From + 2 && i < G.Num() && Spots.Num() < Want; ++i)
			{
				bAny = true;
				const FMwPoint& P = *G[i]; const FVector At = W(P.At);
				if (!Free(At)) continue;
				FMechSpot S; S.At = At; S.Face = Dir(P.Face); S.Hands = P.Hands * 100.f * R.Z; S.bHold = P.bHold;
				S.What = P.Side.IsEmpty() ? P.Area : FString::Printf(TEXT("%s %s"), *P.Area, *P.Side);
				Spots.Add(S);
			}
		if (!bAny) break;
	}
	if (Spots.Num() < Want) ADP_LOG(TEXT("GSE: %s's %s - %d work places for %d mechanics (the rest stand by the nose)"), *Reg, Ft->Name, Spots.Num(), Want);
	return Spots.Num() > 0;
}

void AGSE::MechHands(const FVector& Me, const FVector& Face, float Hands, bool bHold, float T, FVector& L, FVector& R)
{
	const float Hh = FMath::Clamp(Hands, 45.f, 195.f);
	const FVector Lf = ADPPose::LeftOf(Face), Base = Me + Face * (Hh > 150.f ? 30.f : 45.f) + FVector(0, 0, Hh);
	if (bHold)
	{
		const float Sway = FMath::Sin(T * 0.7f) * 1.5f;   // (steady: a part held in place)
		L = Base + Lf * (18.f + Sway); R = Base - Lf * (18.f - Sway);
	}
	else
	{
		const float A = T * 2.f * PI * 1.2f;   // (a tool turned: the right hand round in small circles, the left on the part)
		L = Base + Lf * 20.f; R = Base - Lf * 6.f + (Lf * FMath::Cos(A) + FVector(0, 0, 1) * FMath::Sin(A)) * 5.f;
	}
}

void AGSE::MechWork(int32 Ci, const FString& Reg, TSharedRef<FMechSpot> S)
{
	// at the place: turned to the part, then the work loop until the check is over (a simple pose on hooks - the full
	// animation of each job can come later)
	ATugs* G = Tg(); if (!G || !G->Crew.IsValidIndex(Ci)) return;
	TSharedRef<FVector> Me = MakeShared<FVector>(S->At);
	const FString Who = G->Crew[Ci].Name;
	Do(Ci, [this, Ci, Reg, S, Me, Who]
	{
		ATugs* Gt = Tg(); const AActor* A = Gt && Gt->Crew.IsValidIndex(Ci) ? Gt->Crew[Ci].A.Get() : nullptr; if (!A) return;
		S->bThere = true;   // (from now on the place is theirs: you cannot take it - GSEMechYou.cpp)
		const float Off = FVector::Dist2D(A->GetActorLocation(), S->At);
		*Me = FVector(A->GetActorLocation().X, A->GetActorLocation().Y, S->At.Z);   // (where they stopped: no jump into the place)
		if (!S->bWork) ADP_LOG(TEXT("GSE: %s stands by %s - no free place at its job"), *Who, *Reg);
		else if (Off > 100.f) ADP_WARN(TEXT("GSE: %s stopped %.1f m short of the %s on %s - works from there (stuck?)"), *Who, Off / 100.f, *S->What, *Reg);
		else ADP_LOG(TEXT("GSE: %s at the %s on %s (%s)"), *Who, *S->What, *Reg, S->bHold ? TEXT("holds") : TEXT("fastens"));
	});
	UWorld* Wd = GetWorld();
	auto Work = [Me, S, Wd](float E, float PH, FDriverPose& P)
	{
		// (the place read now: it may have been changed for them before they got there - you took theirs)
		const float T = Wd ? float(Wd->GetTimeSeconds()) : 0.f, Ew = S->bWork ? E : 0.f;   // (standing by: no work pose)
		FVector L, R; MechHands(*Me, S->Face, S->Hands, S->bHold, T, L, R);
		P = ADPPose::Both(*Me, S->Face, L, R, S->bHold ? 0.9f : 0.85f, PH, Ew, S->bHold ? 8.f : 10.f);
		if (!S->bHold) { P.LookHand = 1; P.LookW = 0.7f * Ew; }
	};
	PoseAct(Ci, [Work](float K, float PH, FDriverPose& P) { Work(FMath::SmoothStep(0.f, 1.f, K), PH, P); }, 0.8f);
	Hold(Ci, [Work](float, float PH, FDriverPose& P) { Work(1.f, PH, P); });
}
