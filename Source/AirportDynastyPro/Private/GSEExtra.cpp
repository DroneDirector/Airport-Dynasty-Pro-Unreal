// One more of the kinds that have no place at the vehicle area (to-do 318): pushback and hangar tugs, baggage tractors, belt
// loaders, maintenance vans, fire trucks. Bought, it parks beside its own kind - the same row, its length and 1.5 m on (or beside
// one, its width and 1.5 m) - off the runway strip, taxiways, roads, stands, red lines and walls, never over another vehicle or one's
// home; brought by its kind's own spawn code when nobody sees the spot, and from then in its kind's pool like the others.
#include "GSE.h"
#include "Tugs.h"
#include "FireTruck.h"
#include "Airfield.h"
#include "ServiceVehicles.h"
#include "VehiclesOwned.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPSimData.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "BabylonModel.h"
#include "Engine/World.h"

namespace
{
	const TCHAR* ExTag = TEXT("ADPExtra:");
	FVector2f ExDir(float H) { const float a = FMath::DegreesToRadians(H); return FVector2f(FMath::Cos(a), FMath::Sin(a)); }
	FVector2f ExPlan(const FVector& W) { return FVector2f(float(W.X / 100.0), float(-W.Y / 100.0)); }
	float ExSeg(const FVector2f& P, const FVector2f& A, const FVector2f& B)
	{
		const FVector2f D = B - A; const float L2 = D.SizeSquared();
		const float t = L2 > 1e-4f ? FMath::Clamp(FVector2f::DotProduct(P - A, D) / L2, 0.f, 1.f) : 0.f;
		return FVector2f::Distance(P, A + D * t);
	}
	// (two boxes closer than Gap: separating axes on the plan)
	bool ExHit(const AGSE::FExtraBox& A, const AGSE::FExtraBox& B, float Gap)
	{
		const FVector2f Ax[4] = {ExDir(A.H), ExDir(A.H + 90.f), ExDir(B.H), ExDir(B.H + 90.f)};
		auto R = [](const AGSE::FExtraBox& X, const FVector2f& N) { return FMath::Abs(FVector2f::DotProduct(ExDir(X.H), N)) * X.L + FMath::Abs(FVector2f::DotProduct(ExDir(X.H + 90.f), N)) * X.W; };
		for (const FVector2f& N : Ax) if (FMath::Abs(FVector2f::DotProduct(B.P - A.P, N)) >= R(A, N) + R(B, N) + Gap) return false;
		return true;
	}
	// (a model's half length and width, metres, from its parts in its own frame)
	void ExSize(const AActor* A, float& L, float& W, float DefL, float DefW)
	{
		L = DefL; W = DefW; if (!A) return;
		const FBox B = A->CalculateComponentsBoundingBoxInLocalSpace(true);
		if (B.IsValid && B.GetExtent().X > 50.f && B.GetExtent().Y > 30.f) { L = float(B.GetExtent().X / 100.0); W = float(B.GetExtent().Y / 100.0); }
	}
}

const TArray<ADPOwned::FExtraKind>& ADPOwned::Extras()
{
	// (prices: the baggage tractor's from the turnaround's count; the others real list prices of new ones, rounded)
	static const TArray<FExtraKind> K = {
		{TEXT("pushback"), TEXT("pushback tug"), 320000.0}, {TEXT("hangartug"), TEXT("hangar tug"), 210000.0},
		{TEXT("tractor"), TEXT("baggage tractor"), 65000.0}, {TEXT("beltloader"), TEXT("belt loader"), 95000.0},
		{TEXT("maint"), TEXT("maintenance van"), 55000.0}, {TEXT("firetruck"), TEXT("fire truck"), 950000.0}};
	return K;
}

const ADPOwned::FExtraKind* ADPOwned::Extra(const FString& KeyOrLabel)
{
	return Extras().FindByPredicate([&](const FExtraKind& K) { return KeyOrLabel == K.Key || KeyOrLabel == K.Label; });
}

bool ADPOwned::ExtraSpot(UWorld* W, const FADPWorld& Wd, const FString& Key, FADPSpare& Out, FString& Why)
{
	const AGSE* G = W ? AGSE::Find(W) : nullptr;
	if (!G) { Why = TEXT("the airport's vehicles are not here yet"); return false; }
	return G->ExtraSpot(Wd, Key, Out, Why);
}

bool AGSE::ExtraAnchors(const FString& Key, TArray<FExtraBox>& Out) const
{
	auto Add = [&Out](const FVector& At, float Yaw, float L, float W, float StandX) { FExtraBox B; B.P = ExPlan(At); B.H = -Yaw; B.L = L; B.W = W; B.StandX = StandX; Out.Add(B); };
	if (Key == TEXT("tractor") || Key == TEXT("beltloader") || Key == TEXT("maint"))
		for (const FTractor& T : Tractors)
		{
			if (!T.M.IsValid() || T.bAway || T.bSpare || T.TugIx != INDEX_NONE) continue;
			const bool bIs = Key == TEXT("tractor") ? T.IsTrain() : Key == TEXT("beltloader") ? T.bLoader && T.Svc == INDEX_NONE : T.Svc != INDEX_NONE && ADPService::All()[T.Svc].bMaint;
			if (!bIs) continue;
			float L, W; ExSize(T.M.Get(), L, W, 1.8f, 0.8f); if (T.KHalfL > 50.f && T.KHalfW > 30.f) { L = T.KHalfL / 100.f; W = T.KHalfW / 100.f; }
			Add(T.Home, T.HomeYaw, L, W, 0.f);
		}
	else if (Key == TEXT("pushback") || Key == TEXT("hangartug"))
	{
		if (const ATugs* G = Tg())
			for (const ATugs::FTug& U : G->Tugs)
			{
				if (!U.M.IsValid() || U.bHangar != (Key == TEXT("hangartug"))) continue;
				float L, W; ExSize(U.M.Get(), L, W, 2.55f, 1.26f); Add(U.Home, U.HomeYaw, L, W, U.StandX);
			}
	}
	else if (Key == TEXT("firetruck"))
		for (const TWeakObjectPtr<AFireTruck>& F : AFireTruck::All)
			if (const AFireTruck* T = F.Get()) { float L, W; ExSize(T, L, W, 5.5f, 1.3f); Add(T->GetActorLocation(), T->GetActorRotation().Yaw, L, W, 0.f); }
	return Out.Num() > 0;
}

bool AGSE::ExtraDims(const FString& Key, float& L, float& W) const
{
	TArray<FExtraBox> A; if (!ExtraAnchors(Key, A)) return false;
	L = A[0].L; W = A[0].W; return true;
}

void AGSE::ExtraBoxes(TArray<FExtraBox>& Out, const FADPWorld* Wd, const FString& SkipId) const
{
	// every vehicle where it is and where it lives, the carts, the GPUs, and the bought ones not brought yet
	auto Add = [&Out](const FVector& At, float Yaw, float L, float W) { FExtraBox B; B.P = ExPlan(At); B.H = -Yaw; B.L = L; B.W = W; Out.Add(B); };
	for (const FTractor& T : Tractors)
	{
		if (!T.M.IsValid() || T.bAway || T.TugIx != INDEX_NONE) continue;
		float L, W; ExSize(T.M.Get(), L, W, 2.f, 1.f); if (T.KHalfL > 50.f && T.KHalfW > 30.f) { L = T.KHalfL / 100.f; W = T.KHalfW / 100.f; }
		Add(T.At, T.Yaw, L, W); if (FVector::Dist2D(T.At, T.Home) > 100.f) Add(T.Home, T.HomeYaw, L, W);
	}
	for (const FCart& C : Carts) if (C.M.IsValid()) Add(C.At, C.Yaw, 1.8f, 0.9f);
	if (const ATugs* G = Tg())
		for (const ATugs::FTug& U : G->Tugs)
		{
			if (!U.M.IsValid()) continue;
			float L, W; ExSize(U.M.Get(), L, W, 2.55f, 1.26f);
			Add(U.At, U.Yaw, L, W); if (FVector::Dist2D(U.At, U.Home) > 100.f) Add(U.Home, U.HomeYaw, L, W);
			if (const ABabylonModel* Gp = U.Gpu.M.Get()) { float Lg, Wg; ExSize(Gp, Lg, Wg, 1.5f, 0.8f); Add(Gp->GetActorLocation(), Gp->GetActorRotation().Yaw, Lg, Wg); }
		}
	for (const TWeakObjectPtr<AFireTruck>& F : AFireTruck::All) if (const AFireTruck* T = F.Get()) { float L, W; ExSize(T, L, W, 5.5f, 1.3f); Add(T->GetActorLocation(), T->GetActorRotation().Yaw, L, W); }
	const UADPSim* Sim = UADPSim::Get(GetWorld()); if (!Wd && Sim) Wd = &Sim->World;
	if (Wd)
		for (const FADPAirline& A : Wd->Airlines)
			for (const FADPSpare& S : A.Spares)
			{
				const FString Id = S.Kind + TEXT("@") + S.Place;
				FExtraBox B; if (Id == SkipId || ExtraDone.Contains(Id) || !ADPOwned::Extra(S.Kind) || !ExtraDims(S.Kind, B.L, B.W)) continue;
				B.P = FVector2f(S.X, S.Y); B.H = S.Heading; Out.Add(B);
			}
}

bool AGSE::ExtraFits(const FExtraBox& B, const TArray<FExtraBox>& Others, FString& Why) const
{
	const FAirfieldData& D = AAirfield::Shared(); const float R = FMath::Sqrt(B.L * B.L + B.W * B.W);
	if (B.P.X > D.RwyX0 - 60.f && B.P.X < D.RwyX1 + 60.f && FMath::Abs(B.P.Y - D.RwyY) < 107.5f + R) { Why = TEXT("on the runway strip"); return false; }
	for (const FAirfieldData::FTwy& T : D.Taxiways) for (int32 k = 0; k + 1 < T.Pts.Num(); ++k) if (ExSeg(B.P, T.Pts[k], T.Pts[k + 1]) < T.W * 0.5f + R) { Why = FString::Printf(TEXT("on taxiway %s"), *T.Id); return false; }
	for (const FAirfieldData::FRoad& T : D.Roads) for (int32 k = 0; k + 1 < T.Pts.Num(); ++k) if (ExSeg(B.P, T.Pts[k], T.Pts[k + 1]) < T.W * 0.5f + R) { Why = TEXT("on a road"); return false; }
	for (const FAirfieldData::FStand& S : D.Stands)
	{
		// (where its aircraft stands, 9 m ahead of the nose wheel and 3 m round it: the tug in front of the nose stays clear)
		const FVector2f Lc = S.Frame().Local(B.P);
		if (FMath::Abs(Lc.X) < S.Span * 0.5f + 3.f + R && Lc.Y > -9.f - R && Lc.Y < S.Len + 3.f + R) { Why = FString::Printf(TEXT("on stand %s"), *S.Id); return false; }
	}
	// (the red lines and zones, once a frame: the same for every spot looked at)
	static uint64 LnFrame = ~0ull; static TArray<FAirfieldData::FStandLine> LnC[2]; static TArray<TPair<const FAirfieldData::FStand*, FBox2f>> ZnC[2];
	if (LnFrame != GFrameCounter) { LnFrame = GFrameCounter; for (int32 c = 0; c < 2; ++c) { LnC[c].Reset(); ZnC[c].Reset(); D.StandLines(c == 1, LnC[c], ZnC[c]); } }
	for (int32 c = 0; c < 2; ++c)
	{
		const TArray<FAirfieldData::FStandLine>& Ln = LnC[c]; const TArray<TPair<const FAirfieldData::FStand*, FBox2f>>& Zn = ZnC[c];
		for (const FAirfieldData::FStandLine& L : Ln) if (ExSeg(B.P, L.A, L.B) < R + 0.3f) { Why = TEXT("on a stand's red line"); return false; }
		for (const TPair<const FAirfieldData::FStand*, FBox2f>& Z : Zn) if (Z.Value.ExpandBy(R).IsInside(B.P)) { Why = TEXT("in a jetway's red zone"); return false; }
	}
	for (const FExtraBox& O : Others) if (ExHit(B, O, 1.4f)) { Why = TEXT("another vehicle stands there"); return false; }
	// (walls, buildings, posts: a box of it from 30 cm over the ground to 2.1 m)
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPExtraSpot), false);
	const FVector C = Plan(B.P.X, B.P.Y) + FVector(0.f, 0.f, 120.f);
	if (GetWorld()->OverlapAnyTestByChannel(C, FRotator(0.f, -B.H, 0.f).Quaternion(), ECC_WorldStatic, FCollisionShape::MakeBox(FVector(B.L * 100.f, B.W * 100.f, 90.f)), Q)) { Why = TEXT("against a wall or a building"); return false; }
	return true;
}

bool AGSE::ExtraSpot(const FADPWorld& Wd, const FString& Key, FADPSpare& Out, FString& Why) const
{
	ADP_PERF_SCOPE("Vehicles owned: a spot for one more", 2.f);
	const ADPOwned::FExtraKind* K = ADPOwned::Extra(Key); if (!K) { Why = TEXT("no such vehicle"); return false; }
	TArray<FExtraBox> An; if (!ExtraAnchors(Key, An)) { Why = FString::Printf(TEXT("there is no %s here to park one beside"), K->Label); return false; }
	TArray<FExtraBox> Ob; ExtraBoxes(Ob, &Wd, FString());
	FString Last;
	// (nearest first: one on in the row, then one beside, then two on ...)
	for (int32 k = 1; k <= 4; ++k) for (int32 Side = 0; Side < 2; ++Side) for (const FExtraBox& A : An) for (int32 s = -1; s <= 1; s += 2)
	{
		FExtraBox B = A; B.P = A.P + (Side == 0 ? ExDir(A.H) * (2.f * A.L + 1.5f) : ExDir(A.H + 90.f) * (2.f * A.W + 1.5f)) * float(s * k);
		FString W; if (!ExtraFits(B, Ob, W)) { Last = W; continue; }
		int32 n = 1; for (const FADPAirline& Al : Wd.Airlines) for (const FADPSpare& S : Al.Spares) if (S.Kind == Key) ++n;
		Out = FADPSpare(); Out.Kind = Key; Out.Place = FString::Printf(TEXT("%s-%d"), *Key, n); Out.X = B.P.X; Out.Y = B.P.Y; Out.Heading = B.H;
		return true;
	}
	Why = FString::Printf(TEXT("no free spot next to the %ss: every place beside them is taken or not allowed (the last one: %s)"), K->Label, *Last);
	ADP_LOG(TEXT("Vehicles owned: no spot for one more %s - %s"), K->Label, *Why);
	return false;
}

void AGSE::ExtraTick()
{
	UADPSim* Sim = UADPSim::Get(this); if (!Sim) return;
	for (const FADPAirline& A : Sim->World.Airlines)
		for (const FADPSpare& S : A.Spares)
		{
			const ADPOwned::FExtraKind* K = ADPOwned::Extra(S.Kind); if (!K) continue;
			const FString Id = S.Kind + TEXT("@") + S.Place; if (ExtraDone.Contains(Id)) continue;
			auto Say = [&](const TCHAR* Why) { bool bWas = false; SpareSaid.Add(Id + Why, &bWas); if (!bWas) ADP_LOG(TEXT("GSE: the bought %s %s waits: %s"), K->Label, *Id, Why); };
			FExtraBox B; B.P = FVector2f(S.X, S.Y); B.H = S.Heading;
			if (!ExtraDims(S.Kind, B.L, B.W)) { Say(TEXT("none of its kind here to park beside")); continue; }
			const FVector At = Plan(S.X, S.Y);
			if (!TaxiUnseen(At)) { Say(TEXT("somebody sees its spot")); continue; }
			// (the roads, stands and walls were checked when it was bought: now only nobody on its spot)
			TArray<FExtraBox> Ob; ExtraBoxes(Ob, &Sim->World, Id);
			if (Ob.ContainsByPredicate([&](const FExtraBox& O) { return ExHit(B, O, 0.5f); })) { Say(TEXT("another vehicle stands on its spot")); continue; }
			const float a = FMath::DegreesToRadians(S.Heading); bool bOk = false;
			const bool bTr = S.Kind == TEXT("tractor"), bBl = S.Kind == TEXT("beltloader");
			if ((bTr || bBl || S.Kind == TEXT("maint")) && Tractors.Num() >= Tractors.Max()) { Say(TEXT("no room for another vehicle")); continue; }   // (the entries never move)
			if (bTr || bBl)
			{
				int32 n = 1; for (const FTractor& O : Tractors) if (bBl ? O.bLoader : O.IsTrain()) ++n;
				FTractor T; float Yw = 0.f; T.Home = T.At = PlanPose(S.X, S.Y, FMath::Cos(a), FMath::Sin(a), Yw); T.HomeYaw = T.Yaw = Yw;
				T.Name = (bBl ? TEXT("belt loader ") : TEXT("tractor ")) + FString::FromInt(n); SetKind(T, bBl); T.SpareId = Id; T.Owner = A.Id;
				if (SpawnTractor(T)) { Tractors.Add(T); Pose(Tractors.Last()); bOk = true; }
			}
			else if (S.Kind == TEXT("maint"))
			{
				const int32 i = ADPService::All().IndexOfByPredicate([](const FServiceSpec& X) { return X.bMaint; });
				int32 n = 0; for (const FTractor& O : Tractors) if (O.Svc != INDEX_NONE && ADPService::All()[O.Svc].bMaint) ++n;
				FSpareAt Sa; Sa.Id = Id; Sa.Owner = A.Id; Sa.X = S.X; Sa.Y = S.Y; Sa.Heading = S.Heading; Sa.Name = FString::Printf(TEXT("maintenance van %d"), n + 1);
				if (i != INDEX_NONE && SvcSpawn(i, n, &Sa)) { Tractors.Last().bSpare = false; bOk = true; }   // (parked on the hangar apron like the others, not at the vehicle area)
			}
			else if (S.Kind == TEXT("pushback") || S.Kind == TEXT("hangartug"))
			{
				// (a pushback tug joins the stand whose tug it stands beside)
				TArray<FExtraBox> An; ExtraAnchors(S.Kind, An); float Sx = 0.f, Bd = 1e9f;
				for (const FExtraBox& X : An) if (FVector2f::Distance(X.P, B.P) < Bd) { Bd = FVector2f::Distance(X.P, B.P); Sx = X.StandX; }
				if (ATugs* G = Tg()) bOk = G->ExtraAdd(S.Kind == TEXT("hangartug"), S.X, S.Y, S.Heading, Sx, Id);
			}
			else if (S.Kind == TEXT("firetruck"))
			{
				FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AFireTruck* F = GetWorld()->SpawnActor<AFireTruck>(At, FRotator(0.f, -S.Heading, 0.f), Sp);   // (drivable, the station's own truck: FireTruck.h)
				if (F && F->Setup(TEXT("Firetruck1"))) { F->Tags.Add(FName(*(FString(ExTag) + Id))); bOk = true; }
				else if (F) F->Destroy();
			}
			if (!bOk) { Say(TEXT("its model is missing or its kind has no room")); continue; }
			ExtraDone.Add(Id);
			ADP_LOG(TEXT("GSE: the bought %s %s stands on its spot at plan (%.1f, %.1f), heading %.0f"), K->Label, *Id, S.X, S.Y, S.Heading);
			return;   // (one at a time)
		}
}

bool ATugs::ExtraAdd(bool bHangar, float X, float Y, float Heading, float StandX, const FString& Id)
{
	if (Tugs.Num() >= Tugs.Max()) { ADP_WARN(TEXT("Tugs: no room for the bought tug %s"), *Id); return false; }   // (the entries never move: Setup reserves them)
	FTug T; T.bHangar = bHangar; T.ExtraId = Id; T.StandX = bHangar ? -100000.f : StandX;   // (the hangar tug: at no stand, TugTow.cpp HtNone)
	if (!bHangar)
	{
		const int32 S = StandTug(StandX); if (S == INDEX_NONE) { ADP_WARN(TEXT("Tugs: the bought tug %s has no stand at x %.0f"), *Id, StandX); return false; }
		T.StandNose = Tugs[S].StandNose; T.StandDir = Tugs[S].StandDir;
	}
	T.Home = T.At = Plan(X, Y); T.HomeYaw = T.Yaw = -Heading;
	if (!SpawnAt(T, bHangar ? TEXT("HangarTug") : TEXT("PushbackTug"))) return false;
	Pose(T); Tugs.Add(MoveTemp(T));
	ADP_LOG(TEXT("Tugs: the bought %s %d (%s) at plan (%.1f, %.1f)%s"), bHangar ? TEXT("hangar tug") : TEXT("pushback tug"), Tugs.Num() - 1, *Id, X, Y, bHangar ? TEXT("") : *FString::Printf(TEXT(", for the stand at x %.0f"), StandX));
	return true;
}

FString ATugs::WearId(int32 i) const
{
	if (!Tugs.IsValidIndex(i)) return FString();
	if (!Tugs[i].ExtraId.IsEmpty()) return TEXT("bought:") + Tugs[i].ExtraId;
	int32 n = 0; for (int32 j = 0; j <= i; ++j) if (Tugs[j].bHangar == Tugs[i].bHangar && Tugs[j].ExtraId.IsEmpty()) ++n;
	return FString::Printf(TEXT("own:%s %d"), Tugs[i].bHangar ? TEXT("hangar tug") : TEXT("pushback tug"), n);
}

FString AGSE::FireWearId(const AActor* F)
{
	if (!F) return FString();
	for (const FName& T : F->Tags) if (T.ToString().StartsWith(ExTag)) return TEXT("bought:") + T.ToString().Mid(FCString::Strlen(ExTag));
	int32 n = 0;
	for (const TWeakObjectPtr<AFireTruck>& O : AFireTruck::All)
	{
		const AFireTruck* T = O.Get(); if (!T) continue;
		if (!T->Tags.ContainsByPredicate([](const FName& N) { return N.ToString().StartsWith(ExTag); })) ++n;
		if (T == F) break;
	}
	return FString::Printf(TEXT("own:fire truck %d"), n);
}
