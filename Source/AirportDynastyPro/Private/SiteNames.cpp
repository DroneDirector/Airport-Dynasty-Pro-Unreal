// The airport's name on the models' own pictures, written at run time from the active site (owner: never a baked name). The
// Babylon models carry pictures made for the Icelandic airport: the jetways' posters ("Welcome to ...", "Fly the south
// coast", the codes), the fire station's board and flag, the office screen's home base. At any other site each is drawn
// again here - the same picture with the site's own words over it - and given to every part that wears it.
#include "SiteNames.h"
#include "ADPLog.h"
#include "RoadSigns.h"
#include "Site.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "RenderingThread.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

namespace
{
	struct FSiteWords { FString Name, Full, Iata, Icao, Country; bool bFrench = false, bSwiss = false; };
	FSiteWords Words()
	{
		FSiteWords W; FString Word; ARoadSigns::AirportWords(W.Name, Word); W.Full = W.Name;
		if (const TSharedPtr<FJsonObject> A = ADPSite::AirportBlock())
		{
			FString C; A->TryGetStringField(TEXT("iata"), W.Iata); A->TryGetStringField(TEXT("icao"), W.Icao); A->TryGetStringField(TEXT("country"), C);
			A->TryGetStringField(TEXT("word"), Word);
			W.bSwiss = C == TEXT("CH"); W.bFrench = Word.StartsWith(TEXT("Aéroport"));
			if (W.bSwiss) W.Country = W.bFrench ? TEXT("Suisse") : TEXT("Switzerland");
		}
		if (W.Name.EndsWith(TEXT("Airport"))) W.Name = W.Name.LeftChop(7).TrimEnd();
		if (!W.Full.EndsWith(TEXT("Airport"))) W.Full += TEXT(" Airport");
		return W;
	}

	/** A picture fully loaded (and, in the editor, compiled) before it is drawn from: until then it draws as a white box. */
	void Ready(UTexture2D* T)
	{
		if (!T) return;
		T->SetForceMipLevelsToBeResident(60.f);
#if WITH_EDITOR
		FTextureCompilingManager::Get().FinishCompilation({T});
#endif
		T->WaitForStreaming();
		FlushRenderingCommands();
	}

	using FLine = TFunctionRef<void(const FString&, double, double, double, double, FLinearColor)>;   // (words, middle x, top, height, widest, colour)
	/** Draws a picture upright, then turned over as the models' pictures are kept (their v runs the other way). */
	UTexture* DrawPicture(UObject* Ctx, int32 Wd, int32 Ht, TFunctionRef<void(UCanvas*, FLine)> Paint)
	{
		UTextureRenderTarget2D* A = UKismetRenderingLibrary::CreateRenderTarget2D(Ctx, Wd, Ht, RTF_RGBA8_SRGB, FLinearColor::Black, false);
		UTextureRenderTarget2D* B = UKismetRenderingLibrary::CreateRenderTarget2D(Ctx, Wd, Ht, RTF_RGBA8_SRGB, FLinearColor::Black, false);   // (no smaller mips: they stayed black)
		UTexture2D* Glyphs = LoadObject<UTexture2D>(nullptr, TEXT("/Game/RoadSign/T_SignGlyphs.T_SignGlyphs"));
		FString Text; TSharedPtr<FJsonObject> J;
		if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/sign-glyphs.json"))) || FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data/sign-glyphs.json"))))
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J);
		if (!A || !B || !Glyphs || !J) return nullptr;
		Ready(Glyphs);
		const TMap<TCHAR, TArray<double>>& G = ARoadSigns::GlyphTable();
		const double AW = J->GetArrayField(TEXT("atlas"))[0]->AsNumber(), AH = J->GetArrayField(TEXT("atlas"))[1]->AsNumber(), Pad = J->GetNumberField(TEXT("pad")), GSize = J->GetNumberField(TEXT("size"));
		UCanvas* C = nullptr; FVector2D Size; FDrawToRenderTargetContext Cx;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(Ctx, A, C, Size, Cx);
		if (C)
			Paint(C, [&](const FString& S, double CX, double Top, double BoxH, double MaxW, FLinearColor Col)
			{
				double Wn = 0.0;
				for (const TCHAR Ch : S) { const TArray<double>* E = G.Find(Ch); Wn += E ? (*E)[4] : GSize * 0.4; }
				const double Sc = FMath::Min(BoxH / GSize, MaxW / FMath::Max(Wn, 1.0));
				double X = CX - Wn * Sc / 2;
				for (const TCHAR Ch : S)
				{
					const TArray<double>* E = G.Find(Ch);
					if (!E) { X += GSize * 0.4 * Sc; continue; }
					const double gx = (*E)[0], gy = (*E)[1], gw = (*E)[2], gh = (*E)[3];
					C->K2_DrawTexture(Glyphs, FVector2D(X - Pad * Sc, Top - Pad * Sc), FVector2D(gw * Sc, gh * Sc), FVector2D(gx / AW, gy / AH), FVector2D(gw / AW, gh / AH), Col, BLEND_Translucent);
					X += (*E)[4] * Sc;
				}
			});
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(Ctx, Cx);
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(Ctx, B, C, Size, Cx);
		if (C) C->K2_DrawTexture(A, FVector2D::ZeroVector, Size, FVector2D(0, 1), FVector2D(1, -1), FLinearColor::White, BLEND_Opaque);
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(Ctx, Cx);
		return B;
	}
	FLinearColor Rgb(int32 R, int32 G, int32 B) { return FLinearColor(FColor(uint8(R), uint8(G), uint8(B))); }

	/** The fire station's board (white letters on nothing): its words as text at the board's own place. */
	void Board(AActor* Model, UInstancedStaticMeshComponent* K, const FString& Full)
	{
		for (int32 i = 0; i < K->GetInstanceCount(); ++i)
		{
			FTransform T; K->GetInstanceTransform(i, T, true);
			const FBox Bx = K->GetStaticMesh()->GetBoundingBox(), Wb = Bx.TransformBy(T);
			const FVector Sz = Wb.GetSize(), Cn = Wb.GetCenter();
			const FVector Nm = -T.GetUnitAxis(EAxis::Y).GetSafeNormal2D();   // (a Babylon plane faces its own -z)
			const float H = Sz.Z, Wi = FMath::Max(Sz.X, Sz.Y);
			const FVector Right = FVector::CrossProduct(Nm, FVector::UpVector);   // (to the right of one who stands looking at it)
			const FString Ls[3] = { Full, TEXT("Aircraft Rescue and"), TEXT("Fire Fighting Station") };
			for (int32 l = 0; l < 3; ++l)
			{
				UTextRenderComponent* X = NewObject<UTextRenderComponent>(Model);
				X->SetText(FText::FromString(Ls[l])); X->SetHorizontalAlignment(EHTA_Center); X->SetVerticalAlignment(EVRTA_TextCenter);
				X->SetWorldSize(H * 0.31f); X->SetTextRenderColor(FColor(236, 237, 238));
				X->SetupAttachment(Model->GetRootComponent()); X->RegisterComponent(); Model->AddInstanceComponent(X);
				X->SetWorldLocationAndRotation(Cn + Nm * 2.f + Right * (Wi * 0.17f) + FVector(0, 0, H * (0.32f - 0.32f * l)), Nm.Rotation());
			}
		}
		K->SetVisibility(false);
	}

	/** The fire station's badge: the airport's code on it, written over on a white plate of its own. */
	void Badge(AActor* Model, UInstancedStaticMeshComponent* K, const FString& Icao)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Cube || !Base) return;
		for (int32 i = 0; i < K->GetInstanceCount(); ++i)
		{
			FTransform T; K->GetInstanceTransform(i, T, true);
			const FBox Wb = K->GetStaticMesh()->GetBoundingBox().TransformBy(T);
			const FVector Nm = -T.GetUnitAxis(EAxis::Y).GetSafeNormal2D(), At = Wb.GetCenter() - FVector(0, 0, Wb.GetSize().Z * 0.222f);
			const float H = Wb.GetSize().Z;
			UStaticMeshComponent* P = NewObject<UStaticMeshComponent>(Model);
			P->SetStaticMesh(Cube); UMaterialInstanceDynamic* Md = UMaterialInstanceDynamic::Create(Base, Model); Md->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.9f, 0.89f)); P->SetMaterial(0, Md);
			P->SetCollisionEnabled(ECollisionEnabled::NoCollision); P->SetCastShadow(false);
			P->SetupAttachment(Model->GetRootComponent()); P->RegisterComponent(); Model->AddInstanceComponent(P);
			P->SetWorldTransform(FTransform(Nm.Rotation(), At + Nm * 0.6f, FVector(0.6f, H * 0.31f, H * 0.11f) / 100.f));
			UTextRenderComponent* X = NewObject<UTextRenderComponent>(Model);
			X->SetText(FText::FromString(Icao)); X->SetHorizontalAlignment(EHTA_Center); X->SetVerticalAlignment(EVRTA_TextCenter);
			X->SetWorldSize(H * 0.12f); X->SetTextRenderColor(FColor(20, 24, 30));
			X->SetupAttachment(Model->GetRootComponent()); X->RegisterComponent(); Model->AddInstanceComponent(X);
			X->SetWorldLocationAndRotation(At + Nm * 1.3f, Nm.Rotation());
		}
	}
}

void ADPSiteNames::Apply(AActor* Model, const FString& ModelName)
{
	if (!Model || ADPSite::IsDefault()) return;
	const bool bJet = ModelName == TEXT("Jetways"), bFire = ModelName == TEXT("Firestation"), bOffice = ModelName.StartsWith(TEXT("Office"));
	if (!bJet && !bFire && !bOffice) return;
	const FSiteWords W = Words();
	// (first asked for, drawn a little later: the timer in ABabylonModel::Place)
	UTexture2D* White = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
	TMap<FString, TPair<UTexture*, UTexture*>> Made;   // (by material: the baked picture, ours)
	TArray<UInstancedStaticMeshComponent*> Ks; Model->GetComponents(Ks);
	int32 Parts = 0;
	for (UInstancedStaticMeshComponent* K : Ks)
		for (int32 S = 0; S < K->GetNumMaterials(); ++S)
		{
			UMaterialInterface* M = K->GetMaterial(S); if (!M) continue;
			UMaterialInstanceDynamic* Was = Cast<UMaterialInstanceDynamic>(M);
			const FString MN = Was && Was->Parent ? Was->Parent->GetName() : M->GetName();
			const bool bWelcome = bJet && MN == TEXT("pic_14030"), bFly = bJet && MN == TEXT("pic_14058"), bBoard = bFire && MN == TEXT("dm3440"),
				bFlag = bFire && W.bSwiss && MN == TEXT("dm4528"), bHub = bOffice && MN == TEXT("sm-80");
			if (bBoard) { Board(Model, K, W.Full); ++Parts; continue; }
			if (bFire && MN == TEXT("dm3527")) { Badge(Model, K, W.Icao); ++Parts; continue; }
			if (!bWelcome && !bFly && !bFlag && !bHub) continue;
			UMaterialInstanceDynamic* D = Was ? Was : K->CreateDynamicMaterialInstance(S, M);
			TArray<FMaterialParameterInfo> Infos; TArray<FGuid> Ids; D->GetAllTextureParameterInfo(Infos, Ids);
			TPair<UTexture*, UTexture*>& P = Made.FindOrAdd(MN);
			if (!P.Value)
			{
				for (const FMaterialParameterInfo& I : Infos)
				{
					UTexture* T = nullptr;
					if (!D->GetTextureParameterValue(FHashedMaterialParameterInfo(I), T) || !T) continue;
					const FString Tn = T->GetName();
					if (Tn.Contains(TEXT("14031")) || Tn.Contains(TEXT("14059")) || Tn.Contains(TEXT("4527")) || Tn.Contains(TEXT("scr-79")) || Tn.Contains(TEXT("scr_79"))) { P.Key = T; break; }
				}
				UTexture2D* Old = Cast<UTexture2D>(P.Key);
				if (!Old || !White) { ADP_WARN(TEXT("Site names: %s's picture was not found on %s"), *MN, *K->GetName()); continue; }
				Ready(Old);
				const int32 Tw = bFlag ? 360 : bHub ? 640 : 512, Th = bFlag ? 360 : bHub ? 407 : 1024;
				P.Value = DrawPicture(Model, Tw, Th, [&](UCanvas* C, FLine Line)
				{
					auto Rect = [&](double X0, double Y0, double X1, double Y1, FLinearColor Col) { C->K2_DrawTexture(White, FVector2D(X0, Y0), FVector2D(X1 - X0, Y1 - Y0), FVector2D::ZeroVector, FVector2D::UnitVector, Col, BLEND_Opaque); };
					if (bFlag)
					{
						Rect(0, 0, 360, 360, Rgb(218, 41, 28));
						Rect(145, 68, 215, 292, FLinearColor::White); Rect(68, 145, 292, 215, FLinearColor::White);
						return;
					}
					C->K2_DrawTexture(Old, FVector2D::ZeroVector, FVector2D(Tw, Th), FVector2D(0, 1), FVector2D(1, -1), FLinearColor::White, BLEND_Opaque);
					if (bHub) { Rect(344, 216, 428, 248, Rgb(23, 34, 49)); Line(W.Icao, 386, 217, 28, 80, Rgb(230, 178, 74)); return; }
					Rect(0, 590, 512, 1024, Rgb(28, 58, 138));
					if (bWelcome)
					{
						if (W.bFrench) { Line(TEXT("Bienvenue"), 256, 640, 70, 440, FLinearColor::White); Line(TEXT("Welcome to"), 256, 722, 44, 440, Rgb(200, 215, 240)); }
						else Line(TEXT("Welcome to"), 256, 660, 70, 440, FLinearColor::White);
						Line(W.Full, 256, 800, 82, 450, FLinearColor::White);
						Line((W.Country.IsEmpty() ? FString() : W.Country + TEXT("  ")) + W.Iata + TEXT(" / ") + W.Icao, 256, 920, 40, 440, Rgb(140, 200, 235));
					}
					else
					{
						Line(TEXT("Fly from"), 256, 660, 70, 440, FLinearColor::White); Line(W.Name, 256, 770, 84, 450, FLinearColor::White);
						Line(TEXT("Daily departures from ") + W.Iata, 256, 920, 36, 440, Rgb(140, 200, 235));
					}
				});
				if (!P.Value) continue;
			}
			for (const FMaterialParameterInfo& I : Infos) { UTexture* T = nullptr; if (D->GetTextureParameterValue(FHashedMaterialParameterInfo(I), T) && T == P.Key) D->SetTextureParameterValueByInfo(I, P.Value); }
			K->SetMaterial(S, D); ++Parts;
		}
	ADP_LOG(TEXT("Site names: %s - %d parts given %s's own words"), *ModelName, Parts, *W.Full);
}
