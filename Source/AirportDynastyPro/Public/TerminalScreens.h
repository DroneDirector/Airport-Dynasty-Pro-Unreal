// The timetable screens (departures and arrivals) at the places the Android edition puts them (plan.ts SCREENS,
// fixtures.ts): the Grand Hall's two walls, the totems (both sides), the departure/arrival pairs, the single departure
// screens and every gate's screen. Until the shared world runs in this edition (batch 4) they show the Android edition's
// sample flights round the real time of day (fids.ts sample), refreshed every 4 seconds.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerminalPlan.h"
#include "TerminalScreens.generated.h"

class UInstancedStaticMeshComponent;
class UTextRenderComponent;
class UStaticMesh;

UCLASS()
class AIRPORTDYNASTYPRO_API ATerminalScreens : public AActor
{
	GENERATED_BODY()

public:
	ATerminalScreens();

	/** Puts up every screen. */
	void Build(const FTerminalPlan& Plan);

	virtual void Tick(float DeltaSeconds) override;

private:
	#pragma region Screens
	struct FAt { float X = 0, Y = 0, Z = 0, Yaw = 0; };
	/** One timetable on a screen face: its rows of text, filled each refresh. */
	struct FBoard
	{
		bool bArr = false;
		TArray<TArray<UTextRenderComponent*>> Rows; // per row: time, flight, place, status
	};
	TArray<FBoard> Boards;
	float Clock = 0;

	void Panel(const FAt& O, float W, float H, float Z, float Facing, bool bArr, int32 NRows);
	void GateScreen(const FAt& O, int32 Gate);
	void Bezel(const FAt& O, const FName Mat, float W, float H, float D, float X, float Y, float Z);
	UTextRenderComponent* Text(const FAt& O, const FString& Words, float X, float Y, float Z, float Facing, float Size, FColor Colour, bool bLeft);
	void Refresh();
	#pragma endregion

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> Kits;
};
