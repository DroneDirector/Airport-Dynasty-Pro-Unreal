// Walk anywhere on Earth (WalkAnywhere.h).
#include "WalkAnywhere.h"
#include "BabylonModel.h"
#include "Airfield.h"
#include "TowerADC.h"
#include "LightMasts.h"
#include "Ocean.h"
#include "GrassField.h"
#include "CesiumGeoreference.h"
#include "Cesium3DTileset.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"

namespace
{
	/** The globe's own georeference (Globe.cpp spawns it); Cesium's "default" one is another, at 0 N 0 E. */
	ACesiumGeoreference* ADPGlobeGeo(UWorld* W)
	{
		if (W) for (TActorIterator<ACesiumGeoreference> It(W); It; ++It) if (!It->GetName().Contains(TEXT("Default"))) return *It;
		return nullptr;
	}
}

TWeakObjectPtr<AWalkAnywhere> AWalkAnywhere::One;

namespace
{
	const AWalkAnywhere::FPlace PlaceList[] = {
		{TEXT("Tórshavn, Faroe Islands"), 62.0107, -6.7716}, {TEXT("Slættaratindur, Faroe Islands"), 62.2989, -7.0119},
		{TEXT("Gásadalur, Faroe Islands"), 62.1100, -7.4330}, {TEXT("Reykjavík, Iceland"), 64.1466, -21.9426},
		{TEXT("Skógafoss, Iceland"), 63.5321, -19.5114}, {TEXT("Vík, Iceland"), 63.4186, -19.0060},
		{TEXT("Copenhagen, Denmark"), 55.6761, 12.5683}, {TEXT("Bergen, Norway"), 60.3913, 5.3221},
		{TEXT("Edinburgh, Scotland"), 55.9533, -3.1883}, {TEXT("Zermatt, Switzerland"), 46.0207, 7.7491},
		{TEXT("Grand Canyon, USA"), 36.0570, -112.1390}, {TEXT("Mount Fuji, Japan"), 35.3606, 138.7274},
	};
}

AWalkAnywhere::AWalkAnywhere() { PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = 0.25f; }

TArrayView<const AWalkAnywhere::FPlace> AWalkAnywhere::Places() { return MakeArrayView(PlaceList); }

bool AWalkAnywhere::Away() { return One.IsValid() && One->bAway; }

FString AWalkAnywhere::Status()
{
	if (!Away()) return TEXT("At the airport.");
	return One->bPlaced ? TEXT("Standing at ") + One->Where + TEXT(".") : TEXT("Going to ") + One->Where + TEXT(": finding the ground…");
}

void AWalkAnywhere::SetAirport(bool bShow)
{
	if (!bShow)
	{
		SetAside.Reset();
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			if (It->IsA<ABabylonModel>() || It->IsA<AAirfield>() || It->IsA<ATowerADC>() || It->IsA<ALightMasts>() || It->IsA<AOcean>()
				|| It->GetClass()->GetName().StartsWith(TEXT("Terminal")))
			{
				It->SetActorHiddenInGame(true);
				// not solid either, every part of it (the instanced lists keep their own bodies otherwise)
				TArray<UPrimitiveComponent*> Parts; It->GetComponents(Parts);
				for (UPrimitiveComponent* C : Parts) if (C->GetCollisionEnabled() != ECollisionEnabled::NoCollision) { WasSolid.Add(C, C->GetCollisionEnabled()); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
				SetAside.Add(*It);
			}
	}
	else
	{
		for (AActor* A : SetAside) if (A) A->SetActorHiddenInGame(false);
		for (auto& It : WasSolid) if (It.Key.IsValid()) It.Key->SetCollisionEnabled(It.Value);
		SetAside.Reset(); WasSolid.Reset();
	}
	AGrassField::bAway = !bShow;
}

void AWalkAnywhere::Go(UWorld* W, double Lon, double Lat, const FString& Name)
{
	if (!W) return;
	if (!One.IsValid()) One = W->SpawnActor<AWalkAnywhere>();
	AWalkAnywhere* Me = One.Get();
	ACesiumGeoreference* G = ADPGlobeGeo(W);
	APlayerController* PC = W->GetFirstPlayerController();
	if (!Me || !G || !PC || !PC->GetPawn()) return;
	if (!Me->bAway)
	{
		Me->OldOrigin = G->GetOriginLongitudeLatitudeHeight();
		Me->OldPawn = PC->GetPawn()->GetActorLocation();
		Me->SetAirport(false);
	}
	Me->bAway = true; Me->bPlaced = false; Me->Wait = 0; Me->Where = Name;
	// the place in the middle of the world; you held up in the air until its ground is in
	G->SetOriginLongitudeLatitudeHeight(FVector(Lon, Lat, 0.0));
	APawn* P = PC->GetPawn();
	P->SetActorLocation(FVector(0, 0, 900000.f), false, nullptr, ETeleportType::TeleportPhysics);
	P->DisableInput(PC);
	UE_LOG(LogTemp, Log, TEXT("Walk anywhere: going to %s (%.4f, %.4f)"), *Name, Lat, Lon);
}

bool AWalkAnywhere::GoText(UWorld* W, const FString& Text)
{
	FString T = Text.TrimStartAndEnd();
	for (const FPlace& Pl : Places())
		if (FString(Pl.Name).StartsWith(T, ESearchCase::IgnoreCase) && T.Len() >= 3) { Go(W, Pl.Lon, Pl.Lat, Pl.Name); return true; }
	TArray<FString> Parts;
	T.Replace(TEXT(";"), TEXT(",")).ParseIntoArray(Parts, TEXT(","), true);
	if (Parts.Num() != 2 || !Parts[0].TrimStartAndEnd().IsNumeric() || !Parts[1].TrimStartAndEnd().IsNumeric()) return false;
	const double Lat = FCString::Atod(*Parts[0].TrimStartAndEnd()), Lon = FCString::Atod(*Parts[1].TrimStartAndEnd());
	if (FMath::Abs(Lat) > 85.0 || FMath::Abs(Lon) > 180.0) return false;
	Go(W, Lon, Lat, FString::Printf(TEXT("%.4f, %.4f"), Lat, Lon));
	return true;
}

void AWalkAnywhere::Back(UWorld* W)
{
	AWalkAnywhere* Me = One.Get();
	if (!Me || !Me->bAway) return;
	if (ACesiumGeoreference* G = ADPGlobeGeo(W)) G->SetOriginLongitudeLatitudeHeight(Me->OldOrigin);
	Me->SetAirport(true);
	if (APlayerController* PC = W->GetFirstPlayerController())
		if (APawn* P = PC->GetPawn()) { P->SetActorLocation(Me->OldPawn, false, nullptr, ETeleportType::TeleportPhysics); P->EnableInput(PC); }
	Me->bAway = false; Me->bPlaced = false;
	UE_LOG(LogTemp, Log, TEXT("Walk anywhere: back at the airport"));
}

void AWalkAnywhere::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bAway || bPlaced) return;
	Wait += Dt;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P) return;
	// the ground under the middle, once the terrain's tiles there are in (their shapes are solid)
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WalkAnywhere), true, P);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, FVector(0, 0, 900000.f), FVector(0, 0, -1100000.f), ECC_Visibility, Q);
	if (FMath::FloorToInt(Wait / 5.f) != FMath::FloorToInt((Wait - Dt) / 5.f))
		UE_LOG(LogTemp, Log, TEXT("Walk anywhere: %.0f s, looking down finds %s at %.0f m; you at %.0f m"), Wait, bHit && Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("nothing"), Hit.ImpactPoint.Z / 100.f, P->GetActorLocation().Z / 100.f);
	if (bHit && Hit.GetActor() && Hit.GetActor()->IsA<ACesium3DTileset>())
	{
		// a moment more for the finer tiles to come in, then down onto them
		if (Wait < 4.f) return;
		P->SetActorLocation(Hit.ImpactPoint + FVector(0, 0, P->GetSimpleCollisionHalfHeight() + 30.f), false, nullptr, ETeleportType::TeleportPhysics);
		P->EnableInput(PC);
		bPlaced = true;
		UE_LOG(LogTemp, Log, TEXT("Walk anywhere: standing at %s, %.0f m"), *Where, Hit.ImpactPoint.Z / 100.f);
	}
	else if (Wait > 90.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Walk anywhere: no ground at %s (the world's terrain isn't on this PC?)"), *Where);
		Back(GetWorld());
	}
}
