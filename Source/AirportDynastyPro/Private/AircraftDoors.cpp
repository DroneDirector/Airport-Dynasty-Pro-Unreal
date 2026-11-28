// Data/aircraft-doors.json's doors, read once (AircraftDoors.h).
#include "AircraftDoors.h"
#include "ADPLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ADPDoors
{
	namespace
	{
		struct FType { TArray<FDoor> Doors; float Nose = 0.f; bool bNose = false; };
		const TMap<FString, FType>& All()
		{
			static TMap<FString, FType> Types; static bool bRead = false;
			if (bRead) return Types;
			bRead = true;
			FString Text; TSharedPtr<FJsonObject> J;
			if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/aircraft-doors.json"))) && !FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data/aircraft-doors.json"))))
			{ ADP_WARN(TEXT("Doors: Data/aircraft-doors.json cannot be read - the crews use their own door points")); return Types; }
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J) { ADP_WARN(TEXT("Doors: Data/aircraft-doors.json is not valid JSON")); return Types; }
			auto V3 = [](const TSharedPtr<FJsonObject>& O, const TCHAR* K, FVector& Out) { const TArray<TSharedPtr<FJsonValue>>* A = nullptr; if (!O || !O->TryGetArrayField(K, A) || A->Num() < 3) return false; Out = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber()); return true; };
			auto V2 = [](const TSharedPtr<FJsonObject>& O, const TCHAR* K, FVector2D& Out) { const TArray<TSharedPtr<FJsonValue>>* A = nullptr; if (!O || !O->TryGetArrayField(K, A) || A->Num() < 2) return false; Out = FVector2D((*A)[0]->AsNumber(), (*A)[1]->AsNumber()); return true; };
			int32 N = 0;
			for (const TPair<FString, TSharedPtr<FJsonValue>>& T : J->Values)
			{
				const TSharedPtr<FJsonObject> O = T.Value.IsValid() && T.Value->Type == EJson::Object ? T.Value->AsObject() : nullptr;
				const TArray<TSharedPtr<FJsonValue>>* Ds = nullptr; if (!O || !O->TryGetArrayField(TEXT("doors"), Ds)) continue;
				FType& Ty = Types.Add(T.Key); double Nose = 0.0; Ty.bNose = O->TryGetNumberField(TEXT("nose"), Nose); Ty.Nose = float(Nose);
				for (const TSharedPtr<FJsonValue>& Dv : *Ds)
				{
					const TSharedPtr<FJsonObject> D = Dv.IsValid() ? Dv->AsObject() : nullptr; FDoor Dr;
					if (!D || !D->TryGetStringField(TEXT("name"), Dr.Name) || !V2(D, TEXT("skin"), Dr.Skin)) continue;
					double Sill = 0.0, Turn = 0.0; D->TryGetNumberField(TEXT("sill"), Sill); D->TryGetNumberField(TEXT("turn"), Turn); Dr.Sill = float(Sill); Dr.Turn = float(Turn);
					if (!V2(D, TEXT("out"), Dr.Out)) { const double A = FMath::DegreesToRadians(Turn); Dr.Out = FVector2D((Dr.Skin.X < 0.0 ? -1.0 : 1.0) * FMath::Cos(A), FMath::Sin(A)); }
					const TSharedPtr<FJsonObject>* H = nullptr;
					if (D->TryGetObjectField(TEXT("handle"), H)) { V3(*H, TEXT("out"), Dr.HandleOut); V3(*H, TEXT("in"), Dr.HandleIn); }
					V3(D, TEXT("hinge"), Dr.Hinge);
					const FVector S(Dr.Skin.X, Dr.Skin.Y, Dr.Sill), O3(Dr.Out.X, Dr.Out.Y, 0.0);
					if (!V3(D, TEXT("outside"), Dr.Outside)) Dr.Outside = S + O3 * 60.0;
					if (!V3(D, TEXT("inside"), Dr.Inside)) Dr.Inside = S - O3 * 60.0;
					if (!V3(D, TEXT("dock"), Dr.Dock)) Dr.Dock = S + O3 * 5.0;
					const TSharedPtr<FJsonObject>* Fr = nullptr; FVector Mn, Mx;
					if (D->TryGetObjectField(TEXT("frame"), Fr) && V3(*Fr, TEXT("min"), Mn) && V3(*Fr, TEXT("max"), Mx)) Dr.Frame = FBox(Mn, Mx);
					Ty.Doors.Add(MoveTemp(Dr)); ++N;
				}
			}
			ADP_LOG(TEXT("Doors: %d doors of %d types from Data/aircraft-doors.json"), N, Types.Num());
			return Types;
		}
	}

	const FDoor* Find(const FString& Type, const FString& Name)
	{
		if (const FType* T = All().Find(Type)) for (const FDoor& D : T->Doors) if (D.Name.Equals(Name, ESearchCase::IgnoreCase)) return &D;
		static TSet<FString> Told; const FString K = Type + TEXT("/") + Name;
		if (!Told.Contains(K)) { Told.Add(K); ADP_LOG(TEXT("Doors: no measured door %s on the %s - its own points are used"), *Name, *Type); }
		return nullptr;
	}

	float AlongM(const FString& Type, const FString& Name, float Fallback)
	{
		const FDoor* D = Find(Type, Name); return D ? float(D->Skin.Y) / 100.f : Fallback;
	}

	bool NoseY(const FString& Type, float& Out)
	{
		const FType* T = All().Find(Type); if (!T || !T->bNose) return false;
		Out = T->Nose; return true;
	}
}
