// Walk anywhere on Earth: the globe's middle moved to the place you pick (a named place or a latitude and longitude),
// you put down on the real ground there once its tiles are in (the terrain is solid; the grass grows by the land
// cover), the airport set aside meanwhile (hidden and not solid: it stays where it was). Back puts everything as it was.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WalkAnywhere.generated.h"

UCLASS()
class AIRPORTDYNASTYPRO_API AWalkAnywhere : public AActor
{
	GENERATED_BODY()

public:
	AWalkAnywhere();
	virtual void Tick(float Dt) override;

	/** Go and stand at a longitude and latitude (degrees). */
	static void Go(UWorld* W, double Lon, double Lat, const FString& Name);
	/** "63.45, -18.71" (latitude first, as maps write it) or a place's name from Places(). */
	static bool GoText(UWorld* W, const FString& Text);
	/** Back to the airport. */
	static void Back(UWorld* W);
	/** Away from the airport now. */
	static bool Away();
	/** Where you are, for Settings ("Standing in Tórshavn", "Looking for the ground…"). */
	static FString Status();
	/** Named places to pick from (name, latitude, longitude). */
	struct FPlace { const TCHAR* Name; double Lat, Lon; };
	static TArrayView<const FPlace> Places();

private:
	void SetAirport(bool bShow);
	FVector OldOrigin = FVector::ZeroVector;
	FVector OldPawn = FVector::ZeroVector;
	bool bAway = false, bPlaced = false;
	float Wait = 0;
	FString Where;
	UPROPERTY() TArray<TObjectPtr<AActor>> SetAside;
	TMap<TWeakObjectPtr<class UPrimitiveComponent>, ECollisionEnabled::Type> WasSolid;
	static TWeakObjectPtr<AWalkAnywhere> One;
};
