// The 3D viewer in Unreal (the PC version; replaces the Babylon edition's viewer in the web view, which ran a second
// 3D engine behind the menus): following a real flight of the game on Unreal's own globe (Cesium, the satellite
// pictures, the real sun, Unreal's volumetric clouds), the aircraft the Babylon edition's own airliner (imported as it
// is, Nanite). The game sends where the flight is four times a second; between those the aircraft glides smoothly
// (drawn a quarter of a second behind, as the Babylon viewer did), banking into turns and pitching with its climb.
// The camera circles it with the mouse and comes closer or further with the wheel; the airport's own 3D steps aside
// while you watch (hidden: nothing of it is drawn) and comes back when you leave.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlightViewer.generated.h"

class ACesiumGeoreference;
class ACameraActor;

/** Where a flight is and what it is, as the game describes it (its 3D viewer feed). */
struct FFlightSample
{
	FString Id, Callsign, Reg, Type, From, To, FromName, ToName, Airline, Phase;
	double Lat = 0, Lon = 0, AltM = 0;
	float Hdg = 0, Gs = 0, Vs = 0;
	int32 Pax = 0, PaxMax = 0;
	bool bOnGround = false;
	double At = 0;   // when it came (seconds)
};

UCLASS()
class AIRPORTDYNASTYPRO_API AFlightViewer : public AActor
{
	GENERATED_BODY()

public:
	AFlightViewer();
	virtual void Tick(float Dt) override;

	/** Start watching (the airport hidden, the camera on the aircraft once the first position comes). */
	void Open();
	/** Back to the airport as it was. */
	void Close();
	/** A position from the game (JSON: {me: {...}}). */
	void OnSample(const FString& Json);
	bool IsOpen() const { return bOpen; }
	/** The flight followed now (for the information box); empty before the first position. */
	const FFlightSample& Now() const { return Shown; }
	bool HasFlight() const { return bHave; }

private:
	void Hide(bool bHide);
	void Place(const FFlightSample& S, float Dt);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> Model;
	UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Gear;
	UPROPERTY() TObjectPtr<ACameraActor> Cam;
	UPROPERTY() TObjectPtr<ACesiumGeoreference> Geo;
	UPROPERTY() TArray<TObjectPtr<AActor>> Hidden;
	FVector OldOrigin = FVector::ZeroVector;
	FFlightSample S0, S1, Shown;
	bool bOpen = false, bHave = false, bTwo = false;
	float Yaw = 0.f, Pitch = -8.f, Dist = 6400.f, Roll = 0.f;   // the camera round the aircraft (degrees, cm)
	float LastHdg = 0.f;
};
