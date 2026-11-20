// The home town's buildings (TownBuildings.h).
#include "TownBuildings.h"
#include "Site.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "RoadNetwork.h"
#include "TerminalPlan.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ADPTown
{
	struct FBuilding { FString Name; FVector2D O, U, V; FVector4 Size; FVector2D SpotAt, SpotLook; };

	static const TArray<FBuilding>& Buildings()
	{
		static TArray<FBuilding> B;
		static bool bRead = false;
		if (bRead) return B;
		bRead = true;
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *ADPSite::File(TEXT("town-buildings.json")))) return B;
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return B;
		const TSharedPtr<FJsonObject>* BJ = nullptr;
		if (!Root->TryGetObjectField(TEXT("buildings"), BJ)) return B;
		auto V2 = [](const TSharedPtr<FJsonObject>& O, const TCHAR* K) { const auto& A = O->GetArrayField(K); return FVector2D(A[0]->AsNumber(), A[1]->AsNumber()); };
		for (const auto& It : (*BJ)->Values)
		{
			const TSharedPtr<FJsonObject> O = It.Value->AsObject();
			FBuilding X; X.Name = It.Key; X.O = V2(O, TEXT("o")); X.U = V2(O, TEXT("u")); X.V = V2(O, TEXT("v"));
			const auto& S = O->GetArrayField(TEXT("size")); X.Size = FVector4(S[0]->AsNumber(), S[1]->AsNumber(), S[2]->AsNumber(), S[3]->AsNumber());
			const TSharedPtr<FJsonObject> Sp = O->GetObjectField(TEXT("spot")); X.SpotAt = V2(Sp, TEXT("at")); X.SpotLook = V2(Sp, TEXT("look"));
			B.Add(X);
		}
		return B;
	}

	void Place(UWorld* W)
	{
		if (!W) return;
		FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (const FBuilding& B : Buildings())
		{
			// level, at the highest ground under it (its corners and middle): never sunk into a rise
			double Z = -1e9;
			for (int32 i = 0; i <= 2; ++i) for (int32 j = 0; j <= 2; ++j)
			{
				const double Bx = FMath::Lerp(B.Size.X, B.Size.Z, i * 0.5), By = FMath::Lerp(B.Size.Y, B.Size.W, j * 0.5);
				const FVector2D Q = B.O + B.U * Bx + B.V * By;
				Z = FMath::Max(Z, ARoadNetwork::GroundAt(Q.X, Q.Y));
			}
			// turned so the model's own y (its Blender north) runs along V: the loader puts a Blender model's +y on the
			// actor's right (BabylonModel.h), and plan y is the world's -y
			const FVector Vw(B.V.X, -B.V.Y, 0.0);
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Vw.X, Vw.Y));
			ABabylonModel* M = W->SpawnActor<ABabylonModel>(FTerminalPlan::ToWorld(B.O.X, Z, B.O.Y), FRotator(0, Yaw, 0), S);
			if (!M || !M->Place(B.Name)) { if (M) M->Destroy(); ADP_ERROR(TEXT("Town: %s - the model is missing (Tools/import_model.py)"), *B.Name); continue; }
			M->AddLights(B.Name);
			ADP_LOG(TEXT("Town: %s at plan (%.0f, %.0f), ground %.2f m, turned %.1f"), *B.Name, B.O.X, B.O.Y, Z, Yaw);
		}
	}

	bool Spot(const FString& Id, FVector& At, FVector& Look)
	{
		for (const FBuilding& B : Buildings())
			if (B.Name.Equals(Id, ESearchCase::IgnoreCase))
			{
				const double Z = ARoadNetwork::GroundAt(B.SpotAt.X, B.SpotAt.Y);
				At = FVector(B.SpotAt.X, Z, B.SpotAt.Y);
				Look = FVector(B.SpotLook.X, Z + 4.0, B.SpotLook.Y);
				return true;
			}
		return false;
	}
}
