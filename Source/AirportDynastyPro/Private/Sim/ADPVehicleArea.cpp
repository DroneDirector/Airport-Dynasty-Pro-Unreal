// The vehicle area - see Sim/ADPVehicleArea.h.
#include "Sim/ADPVehicleArea.h"
#include "Sim/ADPSimData.h"
#include "Airfield.h"
#include "ADPLog.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	struct FArea
	{
		TArray<ADPVehicleArea::FPlace> Places; TArray<ADPVehicleArea::FKind> Kinds; TArray<ADPVehicleArea::FCharger> Chargers;
		FArea()
		{
			FString Text; TSharedPtr<FJsonObject> J; const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
			if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/vehicle-area.json"))) && !FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data/vehicle-area.json"))))
			{ ADP_WARN(TEXT("Vehicle area: Data/vehicle-area.json not found")); return; }
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid()) { ADP_WARN(TEXT("Vehicle area: Data/vehicle-area.json is not readable")); return; }
			if (J->TryGetArrayField(TEXT("places"), L))
				for (const TSharedPtr<FJsonValue>& V : *L)
				{
					const TSharedPtr<FJsonObject>* O = nullptr; if (!V->TryGetObject(O)) continue;
					ADPVehicleArea::FPlace P; double X = 0, Y = 0, H = 0;
					(*O)->TryGetStringField(TEXT("name"), P.Name); (*O)->TryGetStringField(TEXT("for"), P.For); (*O)->TryGetStringField(TEXT("fixed"), P.Fixed); (*O)->TryGetBoolField(TEXT("charge"), P.bCharge);
					if (P.Name.IsEmpty() || !(*O)->TryGetNumberField(TEXT("x"), X) || !(*O)->TryGetNumberField(TEXT("y"), Y)) { ADP_WARN(TEXT("Vehicle area: a place without a name or position")); continue; }
					(*O)->TryGetNumberField(TEXT("heading"), H); P.X = float(X); P.Y = float(Y); P.Heading = float(H); Places.Add(MoveTemp(P));
				}
			if (J->TryGetArrayField(TEXT("chargers"), L))
				for (const TSharedPtr<FJsonValue>& V : *L)
				{
					const TSharedPtr<FJsonObject>* O = nullptr; if (!V->TryGetObject(O)) continue;
					ADPVehicleArea::FCharger C; double X = 0, Y = 0, H = 0;
					if (!(*O)->TryGetStringField(TEXT("bay"), C.Bay) || !(*O)->TryGetNumberField(TEXT("x"), X) || !(*O)->TryGetNumberField(TEXT("y"), Y)) { ADP_WARN(TEXT("Vehicle area: a charger without a bay or position")); continue; }
					(*O)->TryGetNumberField(TEXT("heading"), H); C.X = float(X); C.Y = float(Y); C.Heading = float(H); Chargers.Add(MoveTemp(C));
				}
			if (J->TryGetArrayField(TEXT("kinds"), L))
				for (const TSharedPtr<FJsonValue>& V : *L)
				{
					const TSharedPtr<FJsonObject>* O = nullptr; if (!V->TryGetObject(O)) continue;
					ADPVehicleArea::FKind K; (*O)->TryGetStringField(TEXT("key"), K.Key); (*O)->TryGetStringField(TEXT("label"), K.Label); (*O)->TryGetStringField(TEXT("points"), K.Points);
					(*O)->TryGetBoolField(TEXT("roof"), K.bRoof); (*O)->TryGetNumberField(TEXT("price"), K.Price); (*O)->TryGetStringArrayField(TEXT("places"), K.Places);
					if (!K.Key.IsEmpty() && K.Price > 0.0) Kinds.Add(MoveTemp(K));
				}
		}
	};
	const FArea& TheArea() { static FArea A; return A; }
}

const TArray<ADPVehicleArea::FPlace>& ADPVehicleArea::Places() { return TheArea().Places; }
const TArray<ADPVehicleArea::FCharger>& ADPVehicleArea::Chargers() { return TheArea().Chargers; }
const TArray<ADPVehicleArea::FKind>& ADPVehicleArea::Kinds() { return TheArea().Kinds; }
const ADPVehicleArea::FKind* ADPVehicleArea::Kind(const FString& Key) { return Kinds().FindByPredicate([&](const FKind& K) { return K.Key == Key; }); }
const ADPVehicleArea::FPlace* ADPVehicleArea::Place(const FString& Name) { return Places().FindByPredicate([&](const FPlace& P) { return P.Name == Name; }); }
bool ADPVehicleArea::Built() { return AAirfield::Shared().Stands.ContainsByPredicate([](const FAirfieldData::FStand& S) { return S.bRemote; }); }

const ADPVehicleArea::FPlace* ADPVehicleArea::FirstFree(const FADPWorld& W, const FKind& K)
{
	for (const FString& Sort : K.Places)
		for (const FPlace& P : Places())
		{
			if (P.For != Sort || !P.Fixed.IsEmpty()) continue;
			if (!W.Airlines.ContainsByPredicate([&](const FADPAirline& A) { return A.Spares.ContainsByPredicate([&](const FADPSpare& S) { return S.Place == P.Name; }); })) return &P;
		}
	return nullptr;
}
