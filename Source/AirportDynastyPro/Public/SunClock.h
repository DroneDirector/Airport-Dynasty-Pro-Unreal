// The real sun over the airport: where it is in the sky right now (the date and the time of day, at the airport's
// latitude and longitude, with the Earth's tilt and orbit), worked out every few seconds and given to the sun light,
// so the light, the shadows, the sky's colour and the length of the day are the real ones: low and short-lived in
// winter, high and long in summer, gone below the horizon at night. The real weather comes on top later.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SunClock.generated.h"

UCLASS()
class AIRPORTDYNASTYPRO_API ASunClock : public AActor
{
	GENERATED_BODY()

public:
	ASunClock();
	virtual void Tick(float Dt) override;
	virtual void BeginPlay() override;

	/** The place (degrees) and how the world is turned there (the runway's heading, as the globe is turned). */
	void Setup(double InLat, double InLon, float InHeading) { Lat = InLat; Lon = InLon; Heading = InHeading; Update(); }

	/** The sun's elevation and azimuth (degrees; azimuth from north, clockwise) at a moment (UTC). */
	static void SunAt(const FDateTime& Utc, double Lat, double Lon, double& Elevation, double& Azimuth);

	#pragma region Testing (Settings, Testing)
	/** The hour of the day to show (0..24, UTC as in Iceland and the Faroes in winter), or below 0 for the real time. */
	static float HourOverride;
	/** The weather theme: 0 clear, 1 few clouds, 2 broken, 3 overcast, 4 fog, 5 rain, 6 storm, 7 snow. */
	static int32 Theme;
	static const TCHAR* ThemeName(int32 T);
	/** Applies the time and the weather now (the sun, the clouds, the fog, how much sunlight comes through). */
	static void Refresh();
	#pragma endregion

private:
	double Lat = 63.452335, Lon = -18.710348;
	float Heading = 68.f, Clock = 0, SkyClock = 0;
	void Update();
	void ApplyWeather();
	static TWeakObjectPtr<ASunClock> Current;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> CloudMat;
};
