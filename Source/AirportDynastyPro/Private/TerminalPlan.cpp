// Reads the terminal layout (Data/terminal-plan.json, real metres) shared with the Android edition.
#include "TerminalPlan.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#pragma region Loading

static float Num(const TSharedPtr<FJsonObject>& O, const TCHAR* K, float Def = 0.f)
{
	double V = Def;
	return O->TryGetNumberField(K, V) ? float(V) : Def;
}

static FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
{
	FString S;
	O->TryGetStringField(K, S);
	return S;
}

bool FTerminalPlan::Load()
{
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/terminal-plan.json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogTemp, Error, TEXT("Terminal plan not found: %s"), *Path);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Terminal plan could not be read: %s"), *Path);
		return false;
	}

	F2Y = Num(Root, TEXT("F2Y"), 6);
	F1Ceil = Num(Root, TEXT("F1CEIL"), 5.4f);
	F2Ceil = Num(Root, TEXT("F2CEIL"), 11.4f);
	Roof = Num(Root, TEXT("ROOF"), 12);

	const TSharedPtr<FJsonObject>* Sp;
	if (Root->TryGetObjectField(TEXT("spawn"), Sp))
	{
		Spawn.X = Num(*Sp, TEXT("x"));
		Spawn.Z = Num(*Sp, TEXT("z"));
	}

	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("rooms")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FPlanRoom R;
		R.Id = Str(O, TEXT("id")); R.Name = Str(O, TEXT("name")); R.Kind = Str(O, TEXT("kind")); R.Glass = Str(O, TEXT("glass"));
		R.Floor = int32(Num(O, TEXT("f"), 1));
		R.X0 = Num(O, TEXT("x0")); R.Z0 = Num(O, TEXT("z0")); R.X1 = Num(O, TEXT("x1")); R.Z1 = Num(O, TEXT("z1"));
		Rooms.Add(R);
	}
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("doors")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FPlanDoor D;
		D.Id = Str(O, TEXT("id")); D.Type = Str(O, TEXT("type"));
		D.A = Str(O, TEXT("a")); D.B = Str(O, TEXT("b"));
		D.Floor = int32(Num(O, TEXT("f"), 1));
		D.X = Num(O, TEXT("x")); D.Z = Num(O, TEXT("z")); D.Width = Num(O, TEXT("w"), 2);
		D.bAlongX = Str(O, TEXT("along")) != TEXT("z");
		Doors.Add(D);
	}
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("columns")))
	{
		const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
		if (A.Num() >= 3) Columns.Add({ A[0]->AsString(), float(A[1]->AsNumber()), float(A[2]->AsNumber()) });
	}
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("stairs")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FPlanStair S;
		S.Id = Str(O, TEXT("id")); S.Dir = Str(O, TEXT("dir"));
		S.X0 = Num(O, TEXT("x0")); S.Z0 = Num(O, TEXT("z0")); S.X1 = Num(O, TEXT("x1")); S.Z1 = Num(O, TEXT("z1"));
		Stairs.Add(S);
	}
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("lifts")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		Lifts.Add({ Str(O, TEXT("id")), Num(O, TEXT("x")), Num(O, TEXT("z")) });
	}
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("stands")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		Stands.Add({ Str(O, TEXT("id")), Num(O, TEXT("x")), Num(O, TEXT("nz")) });
	}
	return Rooms.Num() > 0;
}

#pragma endregion
