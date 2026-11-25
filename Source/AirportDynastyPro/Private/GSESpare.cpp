// The ground vehicles you have bought, on their places at the vehicle area (to-do 277 E1; Sim/ADPVehicleArea.h): each comes when
// nobody looks at its place and the place is clear, at the start and when bought. Never popped in view, never over another.
#include "GSE.h"
#include "ServiceVehicles.h"
#include "ApronBus.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPVehicleArea.h"
#include "ADPLog.h"
#include "BabylonModel.h"

void AGSE::SpareTick(float Dt)
{
	if ((SpareT -= Dt) > 0.f) return;
	SpareT = 2.f;
	UADPSim* Sim = UADPSim::Get(this); if (!Sim || !ADPVehicleArea::Built()) return;
	for (const FADPAirline& A : Sim->World.Airlines)
		for (const FADPSpare& S : A.Spares)
		{
			const FString Id = S.Kind + TEXT("@") + S.Place;
			if (Tractors.ContainsByPredicate([&](const FTractor& T) { return T.bSpare && T.SpareId == Id; })) continue;
			auto Say = [&](const TCHAR* Why) { bool bWas = false; SpareSaid.Add(Id + Why, &bWas); if (!bWas) ADP_LOG(TEXT("GSE: the bought vehicle %s waits: %s"), *Id, Why); };
			const ADPVehicleArea::FKind* K = ADPVehicleArea::Kind(S.Kind); const ADPVehicleArea::FPlace* P = ADPVehicleArea::Place(S.Place);
			if (!K || !P) { Say(TEXT("no such kind or place in Data/vehicle-area.json")); continue; }
			const int32 i = ADPService::All().IndexOfByPredicate([&](const FServiceSpec& X) { return K->Points == X.Points && K->bRoof == X.bRoof; });
			if (i == INDEX_NONE) { Say(TEXT("no such service vehicle")); continue; }
			const FServiceSpec& Sv = ADPService::All()[i];
			const FVector At = Plan(P->X, P->Y);
			if (!TaxiUnseen(At)) { Say(TEXT("somebody sees its place")); continue; }
			// (a vehicle's middle within the place's own strip, with both half sizes and half a metre)
			const float a = FMath::DegreesToRadians(P->Heading); const FVector Fw(FMath::Cos(a), -FMath::Sin(a), 0.f), Sd(-Fw.Y, Fw.X, 0.f);
			if (Tractors.ContainsByPredicate([&](const FTractor& T) { const FVector D = T.At - At; return T.M.IsValid() && FMath::Abs(FVector::DotProduct(D, Fw)) < T.KHalfL + Sv.HalfL && FMath::Abs(FVector::DotProduct(D, Sd)) < T.KHalfW + Sv.HalfW + 50.f; }))
			{ Say(TEXT("another vehicle stands in its place")); continue; }
			if (Tractors.Num() >= Tractors.Max()) { Say(TEXT("no room for another vehicle")); continue; }   // (the entries never move: Setup reserves them)
			FSpareAt Sa; Sa.Id = Id; Sa.Owner = A.Id; Sa.X = P->X; Sa.Y = P->Y; Sa.Heading = P->Heading; Sa.Name = FString::Printf(TEXT("%s, place %s"), *K->Label, *P->Name);
			int32 n = 0;
			if (Sv.bBus)
			{
				AApronBuses* Bs = AApronBuses::Get(GetWorld()); if (!Bs) { Say(TEXT("no apron buses here")); continue; }
				Bs->Load(); n = Bs->All().IndexOfByPredicate([&](const AApronBuses::FBus& B) { return B.Name == Sa.Name; });
				if (n == INDEX_NONE) n = Bs->AddBus(Sa.Name, TEXT("depot ") + P->Name, P->X, P->Y, P->Heading);
			}
			if (!SvcSpawn(i, n, &Sa)) { Say(TEXT("its model is missing")); continue; }
			return;   // (one at a time)
		}
}
