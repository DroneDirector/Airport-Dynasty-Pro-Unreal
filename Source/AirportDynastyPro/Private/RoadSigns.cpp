// The roadside airport signs (RoadSigns.h).
#include "RoadSigns.h"
#include "ADPLog.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	TSharedPtr<FJsonObject> LoadJson(const TCHAR* Name)
	{
		FString Text; TSharedPtr<FJsonObject> J;
		if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data") / Name)) || FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data") / Name)))
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J);
		return J;
	}
	const int32 FW = 2048, FH = 1092;   // the face's picture (the panel's 3.0 x 1.6 m)
}

FVector ARoadSigns::TestAt = FVector::ZeroVector;
FVector ARoadSigns::TestFace = FVector::ForwardVector;

ARoadSigns::ARoadSigns()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ARoadSigns::AirportWords(FString& Name, FString& Word)
{
	Name = TEXT("Airport"); Word = TEXT("Airport");
	FString Country;
	if (TSharedPtr<FJsonObject> A = LoadJson(TEXT("airfield.json")))
	{
		const TSharedPtr<FJsonObject>* Ap = nullptr;
		if (A->TryGetObjectField(TEXT("airport"), Ap))
		{
			(*Ap)->TryGetStringField(TEXT("name"), Name);
			(*Ap)->TryGetStringField(TEXT("word"), Word);       // (the airport's own, when given)
			(*Ap)->TryGetStringField(TEXT("country"), Country);
			FString Icao; (*Ap)->TryGetStringField(TEXT("icao"), Icao);
			if (Country.IsEmpty() && Icao.Len() >= 2) Country = Icao.Left(2);
		}
	}
	if (Word == TEXT("Airport"))
	{
		// the local word by country (ICAO prefix): written first, then "Airport" for visitors - where English isn't spoken
		static const TMap<FString, FString> Local = {
			{TEXT("BI"), TEXT("Flugvöllur")}, {TEXT("XX"), TEXT("Flogvøllur")}, {TEXT("EK"), TEXT("Lufthavn")}, {TEXT("EN"), TEXT("Lufthavn")},
			{TEXT("ES"), TEXT("Flygplats")}, {TEXT("EF"), TEXT("Lentoasema")}, {TEXT("ED"), TEXT("Flughafen")}, {TEXT("LO"), TEXT("Flughafen")},
			{TEXT("LS"), TEXT("Flughafen")}, {TEXT("EH"), TEXT("Luchthaven")}, {TEXT("EB"), TEXT("Luchthaven")}, {TEXT("LF"), TEXT("Aéroport")},
			{TEXT("LE"), TEXT("Aeropuerto")}, {TEXT("LP"), TEXT("Aeroporto")}, {TEXT("LI"), TEXT("Aeroporto")}, {TEXT("EP"), TEXT("Lotnisko")}};
		if (const FString* L = Local.Find(Country)) Word = *L + TEXT(" · Airport");
		if (Name.EndsWith(TEXT("Airport"))) Name = Name.LeftChop(7).TrimEnd();
	}
}

bool ARoadSigns::Ready()
{
	if (Mesh) return true;
	Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/RoadSign/roadsign/StaticMeshes/roadsign.roadsign"));
	Base = LoadObject<UTexture2D>(nullptr, TEXT("/Game/RoadSign/T_RoadSign_AirportBase.T_RoadSign_AirportBase"));
	Glyphs = LoadObject<UTexture2D>(nullptr, TEXT("/Game/RoadSign/T_SignGlyphs.T_SignGlyphs"));
	FaceMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RoadSign/M_RoadSignFace.M_RoadSignFace"));
	GlyphData = LoadJson(TEXT("sign-glyphs.json"));
	if (!Mesh || !Base || !Glyphs || !FaceMat || !GlyphData.IsValid()) { ADP_WARN(TEXT("Road signs: the model or its pictures are missing (Tools/set_road_sign.py)")); Mesh = nullptr; return false; }
	FaceSlot = Mesh->GetMaterialIndex(TEXT("SignFace"));
	return true;
}

UTexture* ARoadSigns::Compose(const FString& Km)
{
	if (const TObjectPtr<UTextureRenderTarget2D>* F = ByKm.Find(Km)) return F->Get();
	UTextureRenderTarget2D* RT = UKismetRenderingLibrary::CreateRenderTarget2D(this, FW, FH, RTF_RGBA8_SRGB, FLinearColor::Black, true);
	if (!RT) return Base;
	FString Name, Word; AirportWords(Name, Word);
	UCanvas* C = nullptr; FVector2D Size; FDrawToRenderTargetContext Ctx;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, RT, C, Size, Ctx);
	if (C)
	{
		C->K2_DrawTexture(Base, FVector2D::ZeroVector, Size, FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor::White, BLEND_Opaque);
		const TSharedPtr<FJsonObject> G = GlyphData->GetObjectField(TEXT("glyphs"));
		const TArray<TSharedPtr<FJsonValue>>& AtlasSz = GlyphData->GetArrayField(TEXT("atlas"));
		const double AW = AtlasSz[0]->AsNumber(), AH = AtlasSz[1]->AsNumber(), Pad = GlyphData->GetNumberField(TEXT("pad")), GSize = GlyphData->GetNumberField(TEXT("size"));
		const double K = Size.X / FW;
		// a line of text in a box (picture pixels): as tall as the box allows, narrowed only if it is too long
		auto Line = [&](const FString& S, double CX, double Top, double BoxH, double MaxW)
		{
			double Wn = 0.0;
			for (const TCHAR Ch : S) { const TArray<TSharedPtr<FJsonValue>>* E; if (G->TryGetArrayField(FString(1, &Ch), E)) Wn += (*E)[4]->AsNumber(); else Wn += GSize * 0.5; }
			const double Sc = FMath::Min(BoxH / GSize, MaxW / FMath::Max(Wn, 1.0));
			double X = CX - Wn * Sc / 2;
			for (const TCHAR Ch : S)
			{
				const TArray<TSharedPtr<FJsonValue>>* E;
				if (!G->TryGetArrayField(FString(1, &Ch), E)) { X += GSize * 0.5 * Sc; continue; }
				const double gx = (*E)[0]->AsNumber(), gy = (*E)[1]->AsNumber(), gw = (*E)[2]->AsNumber(), gh = (*E)[3]->AsNumber(), adv = (*E)[4]->AsNumber();
				C->K2_DrawTexture(Glyphs, FVector2D((X - Pad * Sc) * K, (Top - Pad * Sc) * K), FVector2D(gw * Sc * K, gh * Sc * K),
					FVector2D(gx / AW, gy / AH), FVector2D(gw / AW, gh / AH), FLinearColor(0.92f, 0.94f, 0.96f, 1.f), BLEND_Translucent);
				X += adv * Sc;
			}
		};
		Line(Name, FW / 2.0, 150.0, 190.0, 1300.0);
		Line(Word, FW / 2.0, 470.0, 115.0, 1500.0);
		Line(Km + TEXT(" km"), FW / 2.0 + 160.0, 780.0, 180.0, 700.0);
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Ctx);
	Faces.Add(RT); ByKm.Add(Km, RT);
	ADP_LOG(TEXT("Road signs: a face for %s (%s, %s km)"), *Name, *Word, *Km);
	return RT;
}

bool ARoadSigns::AddSign(const FVector& At, const FVector& Face, const FString& Km)
{
	if (!Ready()) return false;
	// the model's face looks along its local -y (its Blender -y: the import keeps it; tested 2026-11-14)
	const FVector2D F = FVector2D(Face.X, Face.Y).GetSafeNormal();
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(F.Y, F.X)) + 90.f;
	TestAt = At; TestFace = FVector(F.X, F.Y, 0.0);
	UStaticMeshComponent* S = NewObject<UStaticMeshComponent>(this);
	S->SetupAttachment(Root);
	S->SetStaticMesh(Mesh);
	S->SetWorldLocationAndRotation(At, FRotator(0.f, Yaw, 0.f));
	S->SetCollisionProfileName(TEXT("BlockAll"));
	if (FaceSlot != INDEX_NONE)
		if (UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(FaceMat, this)) { D->SetTextureParameterValue(TEXT("Face"), Compose(Km)); S->SetMaterial(FaceSlot, D); }
	S->RegisterComponent();
	AddInstanceComponent(S);
	return true;
}
