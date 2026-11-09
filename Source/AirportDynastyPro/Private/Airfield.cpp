// The airfield: read from Data/airfield.json and laid out as flat instanced slabs, each a few millimetres over the one
// under it so they never flicker through each other: grass, then aprons, taxiways, runway, then the markings.
#include "Airfield.h"
#include "TerminalBuilder.h"
#include "TerminalPlan.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#pragma region Loading

bool FAirfieldData::Load()
{
	FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/airfield.json"));
	if (!FPaths::FileExists(Path)) Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/airfield.json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { UE_LOG(LogTemp, Error, TEXT("Airfield not found: %s"), *Path); return false; }
	TSharedPtr<FJsonObject> R;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), R) || !R) { UE_LOG(LogTemp, Error, TEXT("Airfield could not be read")); return false; }
	auto N = [](const TSharedPtr<FJsonObject>& O, const TCHAR* K, double Def = 0) { double V = Def; O->TryGetNumberField(K, V); return float(V); };
	const TSharedPtr<FJsonObject> Rw = R->GetObjectField(TEXT("runway"));
	RwyX0 = N(Rw, TEXT("x0")); RwyX1 = N(Rw, TEXT("x1")); RwyY = N(Rw, TEXT("y")); RwyW = N(Rw, TEXT("w"), 60);
	const TSharedPtr<FJsonObject> Ys = R->GetObjectField(TEXT("ys"));
	AY = N(Ys, TEXT("AY")); BY = N(Ys, TEXT("BY")); TY = N(Ys, TEXT("TY")); KY = N(Ys, TEXT("KY"));
	const TSharedPtr<FJsonObject> Ap = R->GetObjectField(TEXT("airport"));
	Ap->TryGetNumberField(TEXT("lat"), Lat); Ap->TryGetNumberField(TEXT("lon"), Lon);
	ElevFt = N(Ap, TEXT("elevFt"), 130); Heading = N(Ap, TEXT("hdg"), 68);
	for (const TSharedPtr<FJsonValue>& V : R->GetArrayField(TEXT("taxiways")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FTwy T;
		T.Id = O->GetStringField(TEXT("id")); T.Kind = O->GetStringField(TEXT("kind")); T.W = N(O, TEXT("w"), 25);
		for (const TSharedPtr<FJsonValue>& P : O->GetArrayField(TEXT("pts")))
		{
			const TArray<TSharedPtr<FJsonValue>>& A = P->AsArray();
			T.Pts.Add(FVector2f(A[0]->AsNumber(), A[1]->AsNumber()));
		}
		Taxiways.Add(T);
	}
	for (const TSharedPtr<FJsonValue>& V : R->GetArrayField(TEXT("stands")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FStand S;
		S.Id = O->GetStringField(TEXT("id")); S.X = N(O, TEXT("x")); S.Nose = N(O, TEXT("nose")); S.Dir = N(O, TEXT("dir"), -1);
		S.Span = N(O, TEXT("span"), 36); S.Len = N(O, TEXT("len"), 44.5f);
		O->TryGetBoolField(TEXT("jetway"), S.bJetway); O->TryGetBoolField(TEXT("cargo"), S.bCargo);
		Stands.Add(S);
	}
	for (const TSharedPtr<FJsonValue>& V : R->GetArrayField(TEXT("holds")))
		Holds.Add(FVector2f(N(V->AsObject(), TEXT("x")), N(V->AsObject(), TEXT("y"))));
	for (const TSharedPtr<FJsonValue>& V : R->GetArrayField(TEXT("aprons")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		Aprons.Add({N(O, TEXT("x0")), N(O, TEXT("y0")), N(O, TEXT("x1")), N(O, TEXT("y1"))});
	}
	const TSharedPtr<FJsonObject> B = R->GetObjectField(TEXT("boundary")), C = R->GetObjectField(TEXT("cargoBuilding"));
	Boundary = {N(B, TEXT("x0")), N(B, TEXT("y0")), N(B, TEXT("x1")), N(B, TEXT("y1"))};
	Cargo = {N(C, TEXT("x0")), N(C, TEXT("y0")), N(C, TEXT("x1")), N(C, TEXT("y1"))};
	return true;
}

#pragma endregion

#pragma region Pieces

AAirfield::AAirfield()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

UInstancedStaticMeshComponent* AAirfield::Kit(const FName Mat)
{
	if (TObjectPtr<UInstancedStaticMeshComponent>* K = Kits.Find(Mat)) return *K;
	UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this, Mat);
	K->SetupAttachment(Root);
	K->SetStaticMesh(Cube);
	K->SetMaterial(0, ATerminalBuilder::KitMaterial(this, Mat));
	K->SetCollisionProfileName(TEXT("BlockAll"));
	K->RegisterComponent();
	AddInstanceComponent(K);
	Kits.Add(Mat, K);
	return K;
}

void AAirfield::Slab(const FName Mat, float X0, float Y0, float X1, float Y1, float Bottom, float Top)
{
	const FVector A = FTerminalPlan::ToWorld(FMath::Min(X0, X1), Bottom, FMath::Min(Y0, Y1));
	const FVector B = FTerminalPlan::ToWorld(FMath::Max(X0, X1), Top, FMath::Max(Y0, Y1));
	const FVector S = (B - A).GetAbs();
	Kit(Mat)->AddInstance(FTransform(FRotator::ZeroRotator, (A + B) * 0.5f, S / 100.f));
}

void AAirfield::Strip(const FName Mat, const FVector2f& A, const FVector2f& B, float W, float Top, float Thick)
{
	const FVector WA = FTerminalPlan::ToWorld(A.X, Top - Thick / 2, A.Y), WB = FTerminalPlan::ToWorld(B.X, Top - Thick / 2, B.Y);
	const FVector Dir = WB - WA;
	if (Dir.IsNearlyZero()) return;
	Kit(Mat)->AddInstance(FTransform(Dir.Rotation(), (WA + WB) * 0.5f, FVector(Dir.Size() / 100.f, W, Thick)));
}

#pragma endregion

#pragma region Build

bool AAirfield::Build(bool bModelSite)
{
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube || !D.Load()) return false;

	// the grass over the whole airport, levelled to the airport's elevation (the globe's own ground meets it at the edge)
	Slab("grass", D.Boundary.X0, D.Boundary.Y0, D.Boundary.X1, D.Boundary.Y1, -0.6f, -0.08f);
	// landside in front of the terminal: the kerb, the road and the car park
	if (!bModelSite)
	{
		Slab("asphalt", -160, -170, 392, -0.5f, -0.6f, -0.05f);
		// the aprons (concrete)
		for (const FAirfieldData::FRect& A : D.Aprons) Slab("apron", A.X0, A.Y0, A.X1, A.Y1, -0.6f, -0.04f);
	}
	// the taxiways: shoulders, pavement and the yellow centre line
	for (const FAirfieldData::FTwy& T : D.Taxiways)
		for (int32 i = 0; i + 1 < T.Pts.Num(); ++i)
		{
			const FVector2f A = T.Pts[i], B = T.Pts[i + 1], U = (B - A).GetSafeNormal();
			const bool bLane = T.Kind == TEXT("taxilane");
			if (!bLane) Strip("concrete", A - U * 20, B + U * 20, T.W + 35, -0.035f, 0.5f);
			Strip("asphalt", A - U * (T.W / 2), B + U * (T.W / 2), T.W, -0.03f, 0.5f);
			Strip("yellow", A, B, 0.3f, -0.02f, 0.01f);
		}
	// the runway: shoulders, pavement, centre line dashes, edge lines, threshold bars, designators, touchdown marks
	const float RY = D.RwyY, HW = D.RwyW / 2;
	Slab("concrete", D.RwyX0 - 60, RY - HW - 7.5f, D.RwyX1 + 60, RY + HW + 7.5f, -0.6f, -0.025f);
	Slab("asphalt", D.RwyX0, RY - HW, D.RwyX1, RY + HW, -0.6f, -0.02f);
	for (float X = D.RwyX0 + 400; X < D.RwyX1 - 400; X += 50) Slab("white", X, RY - 0.45f, X + 30, RY + 0.45f, -0.02f, -0.01f);
	for (const float S : {-1.f, 1.f}) Slab("white", D.RwyX0, RY + S * (HW - 0.6f) - 0.45f, D.RwyX1, RY + S * (HW - 0.6f) + 0.45f, -0.02f, -0.01f);
	for (const float End : {0.f, 1.f})
	{
		const float X0 = End ? D.RwyX1 - 6 - 30 : D.RwyX0 + 6, Dir = End ? -1.f : 1.f;
		// the threshold's piano keys: 16 bars, 30 m long
		for (int32 k = 0; k < 16; ++k)
		{
			const float Y = RY - HW + 3 + k * ((D.RwyW - 6) / 16) + (k >= 8 ? 1.8f : 0);
			Slab("white", X0, Y, X0 + 30, Y + 1.8f, -0.02f, -0.01f);
		}
		// the touchdown zone marks
		for (int32 k = 0; k < 6; ++k)
		{
			const float X = (End ? D.RwyX1 : D.RwyX0) + Dir * (400 + k * 150);
			for (const float S : {-1.f, 1.f}) Slab("white", X, RY + S * 10 - 1.5f, X + Dir * 22.5f, RY + S * 10 + 1.5f, -0.02f, -0.01f);
		}
		// the designator painted on the runway, read from an aircraft landing
		UTextRenderComponent* Num = NewObject<UTextRenderComponent>(this);
		Num->SetupAttachment(Root);
		Num->SetText(FText::FromString(End ? TEXT("26") : TEXT("08")));
		Num->SetHorizontalAlignment(EHTA_Center);
		Num->SetVerticalAlignment(EVRTA_TextCenter);
		Num->SetWorldSize(1800.f);
		Num->SetTextRenderColor(FColor::White);
		const FVector At = FTerminalPlan::ToWorld((End ? D.RwyX1 : D.RwyX0) + Dir * 60, 0.0f, RY);
		Num->SetRelativeLocationAndRotation(At, FRotator(90.f, End ? 0.f : 180.f, 0));
		Num->RegisterComponent();
		AddInstanceComponent(Num);
	}
	// the runway-holding positions: four yellow lines across the taxiway
	for (const FVector2f& H : D.Holds)
		for (int32 k = 0; k < 4; ++k) Slab("yellow", H.X - 12.5f, H.Y - 1.5f + k * 1.0f, H.X + 12.5f, H.Y - 1.2f + k * 1.0f, -0.02f, -0.01f);
	// the stands' lead-in lines on the apron (the terminal's own stands already have theirs)
	for (const FAirfieldData::FStand& S : D.Stands)
		if (S.bCargo || S.Id == TEXT("STAND-9")) Strip("yellow", FVector2f(S.X, S.Nose), FVector2f(S.X, S.Nose - S.Dir * (S.Len + 20)), 0.3f, -0.02f, 0.01f);
	// the cargo building
	Slab("liftClad", D.Cargo.X0, D.Cargo.Y0, D.Cargo.X1, D.Cargo.Y1, 0, 18);
	UE_LOG(LogTemp, Log, TEXT("Airfield built: %d taxiways, %d stands"), D.Taxiways.Num(), D.Stands.Num());
	return true;
}

#pragma endregion
