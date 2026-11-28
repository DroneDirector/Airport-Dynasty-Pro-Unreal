// The A320's doors, APU inlet flap, RAT and flap mechanisms (A320Parts.h).
#include "A320Parts.h"
#include "BabylonModel.h"
#include "Turnaround.h"
#include "ADPLog.h"
#include "UObject/WeakObjectPtrTemplates.h"

namespace ADPA320
{
	namespace PartsData
	{
		// model root frame, cm: x right, y aft, z up (FlightGear's x aft, y right, z up in metres, fitted to the model)
		const FVector HingeAx(0.00013f, 0.00442f, -0.99999f), Aft(0.00005f, 0.99999f, 0.00442f), Right(1.f, -0.00005f, 0.00013f);
		struct FPax { const TCHAR* Hinge; const TCHAR* Door; FVector P; float Side; float Out5, Out10; };
		// (DoorHingeL2/R2 carry L4/R4; the door slides out by mid-way, then forward 1 m; the hinge arm swings 70 deg)
		const FPax Pax[4] = {
			{ TEXT("DoorHingeL1"), TEXT("DoorL1"), FVector(-160.05f, -63.62f, 424.98f), -1.f, 0.225f, 0.20f },
			{ TEXT("DoorHingeR1"), TEXT("DoorR1"), FVector(159.79f, -63.64f, 425.02f), 1.f, 0.225f, 0.20f },
			{ TEXT("DoorHingeL2"), TEXT("DoorL4"), FVector(-155.30f, 2400.16f, 435.86f), -1.f, 0.25f, 0.40f },
			{ TEXT("DoorHingeR2"), TEXT("DoorR4"), FVector(155.28f, 2400.15f, 435.90f), 1.f, 0.25f, 0.40f } };
		const FVector2f THinge[] = { {0.f, 0.f}, {0.5f, 10.f}, {0.85f, 45.f}, {1.f, 70.f} };   // (deg; the left side negative)
		const FVector2f THingeX[] = { {0.f, 0.f}, {0.5f, -0.15f}, {1.f, -0.15f} };              // (m along the fuselage)
		const FVector2f TDoorX[] = { {0.f, 0.f}, {0.5f, 0.f}, {1.f, -1.f} };
		// cargo 1/2: 125 deg up about the top edge; 3 (bulk): 55 deg up inward after sliding 2.5-5 cm in
		const FVector CargoAx(-0.00005f, -0.99999f, -0.00442f);
		const FVector CargoP[2] = { FVector(187.83f, 279.23f, 333.43f), FVector(186.88f, 1728.79f, 338.46f) };
		const FVector BulkP(153.01f, 2148.43f, 291.43f), BulkAx(-0.06258f, 0.99803f, 0.00506f);
		const FVector2f TBulk[] = { {0.f, 0.f}, {0.1f, 0.f}, {1.f, 55.f} };
		const FVector2f TBulkIn[] = { {0.f, 0.f}, {0.1f, 0.025f}, {1.f, 0.05f} };
		const FVector ApuP(0.04f, 2904.28f, 365.12f), ApuAx(-1.f, 0.00005f, -0.00013f);          // (-10 deg x the flap)
		const FVector RatP(-168.95f, 1105.92f, 187.54f), RatAx(-0.96597f, -0.00111f, 0.25866f);  // (+90 deg: stowed)
		// FlightGear's own speeds (Nasal/libraries.nas, APU.nas): passenger doors 5 s, cargo 10 s, bulk 3 s, APU flap 12 s
		const float Secs[PartNum] = { 5.f, 5.f, 5.f, 5.f, 10.f, 10.f, 3.f, 12.f };

		template <int32 N> float Tab(const FVector2f (&T)[N], float X)
		{
			if (X <= T[0].X) return T[0].Y;
			for (int32 i = 1; i < N; ++i) if (X <= T[i].X) return FMath::Lerp(T[i - 1].Y, T[i].Y, (X - T[i - 1].X) / FMath::Max(T[i].X - T[i - 1].X, 1e-4f));
			return T[N - 1].Y;
		}
		FTransform Turn(const FVector& H, const FVector& Ax, float Deg)
		{
			const FQuat Q(Ax.GetSafeNormal(), FMath::DegreesToRadians(Deg));
			return FTransform(Q, H - Q.RotateVector(H));
		}

		struct FState
		{
			int32 GHinge[4] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE }, GDoor[4] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
			int32 GCargo[3] = { INDEX_NONE, INDEX_NONE, INDEX_NONE }, GApu = INDEX_NONE;
			float K[PartNum] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
			FWant Want;
		};
		TMap<TWeakObjectPtr<ABabylonModel>, FState> States;
		TSet<FString> Holds;   // "Reg/part" kept open (Hold)

		void PosePart(ABabylonModel* B, const FState& S, int32 Part)
		{
			if (Part < Cargo1)
			{
				const FPax& D = Pax[Part]; const float k = S.K[Part];
				B->MoveGroup(S.GHinge[Part], Turn(D.P, HingeAx, D.Side * Tab(THinge, k)) * FTransform(Aft * (100.f * Tab(THingeX, k))));
				const FVector2f TOut[] = { {0.f, 0.f}, {0.5f, D.Out5}, {1.f, D.Out10} };
				B->MoveGroup(S.GDoor[Part], FTransform(Aft * (100.f * Tab(TDoorX, k)) + Right * (100.f * D.Side * Tab(TOut, k))));
			}
			else if (Part < Cargo3) B->MoveGroup(S.GCargo[Part - Cargo1], Turn(CargoP[Part - Cargo1], CargoAx, 125.f * S.K[Part]));
			else if (Part == Cargo3) B->MoveGroup(S.GCargo[2], Turn(BulkP, BulkAx, Tab(TBulk, S.K[Part])) * FTransform(Right * (-100.f * Tab(TBulkIn, S.K[Part]))));
			else B->MoveGroup(S.GApu, Turn(ApuP, ApuAx, -10.f * S.K[Part]));
		}
	}

	void Rest(ABabylonModel* M)
	{
		using namespace PartsData;
		if (!M || States.Contains(M)) return;
		for (auto It = States.CreateIterator(); It; ++It) if (!It->Key.IsValid()) It.RemoveCurrent();
		FState& S = States.Add(M);
		const FTransform I = FTransform::Identity;
		int32 Found = 0;
		for (int32 i = 0; i < 4; ++i)
		{
			S.GHinge[i] = M->BindGroup({Pax[i].Hinge}, false, I); S.GDoor[i] = M->BindGroup({Pax[i].Door}, false, I);
			Found += (S.GHinge[i] != INDEX_NONE) + (S.GDoor[i] != INDEX_NONE);
		}
		for (int32 i = 0; i < 3; ++i) { S.GCargo[i] = M->BindGroup({FString::Printf(TEXT("Cargo%d"), i + 1)}, false, I); Found += S.GCargo[i] != INDEX_NONE; }
		S.GApu = M->BindGroup({TEXT("ApuFlap")}, false, I); Found += S.GApu != INDEX_NONE;
		// the RAT: FlightGear's model has it out; stowed (arm 0) it turns 90 deg up into its bay
		const int32 GRat = M->BindGroup({TEXT("Rat"), TEXT("RatSpinner")}, false, I);
		M->MoveGroup(GRat, Turn(RatP, RatAx, 90.f));
		// the flap mechanisms: hidden, as in FlightGear (a select on /fdm/jsbsim/zero, never true)
		const int32 GMech = M->BindGroup({TEXT("FlapMechanismL"), TEXT("FlapMechanismR")}, false, I);
		M->MoveGroup(GMech, FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector));
		// the side braces' lock actuators: hidden too (FlightGear selects them off the same way)
		const int32 GLock = M->BindGroup({TEXT("SideBraceUp_L.LockActuator1"), TEXT("SideBraceUp_L.LockActuator2"), TEXT("SideBraceUp_R.LockActuator1"), TEXT("SideBraceUp_R.LockActuator2")}, false, I);
		M->MoveGroup(GLock, FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector));
		M->FlushMoves();
		static int32 Told = 0;
		if (Told++ < 3) ADP_LOG(TEXT("A320 parts: %d of 16 doors/flap bound, RAT %s, flap mechanisms %s"), Found, GRat != INDEX_NONE ? TEXT("stowed") : TEXT("missing"), GMech != INDEX_NONE ? TEXT("hidden") : TEXT("missing"));
	}

	void Set(ABabylonModel* M, const FWant& W)
	{
		if (!M) return;
		if (!PartsData::States.Contains(M)) Rest(M);
		if (PartsData::FState* S = PartsData::States.Find(M)) S->Want = W;
	}

	bool Step(ABabylonModel* M, float Dt)
	{
		using namespace PartsData;
		FState* S = M ? States.Find(M) : nullptr;
		if (!S || Dt <= 0.f) return false;
		bool bMoved = false;
		for (int32 p = 0; p < PartNum; ++p)
		{
			const float To = FMath::Clamp(S->Want.K[p], 0.f, 1.f);
			if (S->K[p] == To) continue;
			S->K[p] = To > S->K[p] ? FMath::Min(To, S->K[p] + Dt / Secs[p]) : FMath::Max(To, S->K[p] - Dt / Secs[p]);
			PosePart(M, *S, p); bMoved = true;
		}
		if (bMoved) M->FlushMoves();
		return bMoved;
	}

	FWant FromTurnaround(const FString& Reg, bool bApu)
	{
		FWant W; W.K[Apu] = bApu ? 1.f : 0.f;
		const ADPTurnaround::FTurn* Tr = ADPTurnaround::All().FindByPredicate([&Reg](const ADPTurnaround::FTurn& X) { return X.Reg == Reg; });
		if (!Tr) return W;
		auto Task = [Tr](const TCHAR* Id) { return Tr->Tasks.FindByPredicate([Id](const ADPTurnaround::FTask& X) { return X.Id == Id; }); };
		auto Busy = [&Task](const TCHAR* Id) { const ADPTurnaround::FTask* T = Task(Id); return T && T->bStarted && !T->bDone; };
		// L1: open once the jetway is on (its task opens it), shut when "doors closed, jetway off" starts
		const ADPTurnaround::FTask* Br = Task(TEXT("bridge")); const ADPTurnaround::FTask* Dc = Task(TEXT("doors"));
		W.K[PL1] = Br && Br->bDone && !(Dc && Dc->bStarted) ? 1.f : 0.f;
		W.K[PR1] = Busy(TEXT("cat1")) ? 1.f : 0.f;   // (catering at the front right door)
		W.K[PR4] = Busy(TEXT("cat2")) ? 1.f : 0.f;   // (and at the rear right one)
		W.K[Cargo1] = Busy(TEXT("fwd")) ? 1.f : 0.f;
		W.K[Cargo2] = Busy(TEXT("aft")) ? 1.f : 0.f;
		W.K[Cargo3] = Busy(TEXT("bulk")) ? 1.f : 0.f;
		for (int32 p = 0; p < PartNum; ++p) if (PartsData::Holds.Contains(FString::Printf(TEXT("%s/%d"), *Reg, p))) W.K[p] = 1.f;
		return W;
	}

	float Now(ABabylonModel* M, EPart P)
	{
		const PartsData::FState* S = M ? PartsData::States.Find(M) : nullptr;
		return S && P >= 0 && P < PartNum ? S->K[P] : 0.f;
	}

	float NowFor(const FString& Reg, EPart P)
	{
		const FName Tag(*(TEXT("SIM/") + Reg));
		for (const auto& It : PartsData::States)
			if (ABabylonModel* M = It.Key.Get()) if (M->Tags.Contains(Tag)) return P >= 0 && P < PartNum ? It.Value.K[P] : 0.f;
		return -1.f;
	}

	void Hold(const FString& Reg, EPart P, bool bOn)
	{
		const FString Key = FString::Printf(TEXT("%s/%d"), *Reg, int32(P));
		if (bOn == PartsData::Holds.Contains(Key)) return;
		if (bOn) PartsData::Holds.Add(Key); else PartsData::Holds.Remove(Key);
		ADP_LOG(TEXT("A320 parts: %s door %d %s"), *Reg, int32(P), bOn ? TEXT("held open") : TEXT("released"));
	}
}
