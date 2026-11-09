// The walking-mode interface, as in the Android edition: the holographic map in the corner (M or a click opens it full
// screen with routes), a clean top-left bar (Map, Settings, Hide UI), Hide UI (H; a double-click or Esc brings it
// back), the settings panel (Tab: quality, render scale, frame limit, VSync, the performance overlay, scan my PC, the
// problem log) and the performance overlay (P). On the very first start the PC is scanned and the quality set from it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ADPHUD.generated.h"

class SHoloMap;
class SWidget;

UCLASS()
class AIRPORTDYNASTYPRO_API AADPHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	#pragma region Panels
	TSharedRef<SWidget> MakeTopBar();
	TSharedRef<SWidget> MakeSettings();
	TSharedRef<SWidget> MakePerf();
	void ToggleMap();
	void ToggleSettings();
	void TogglePerf();
	void HideUI();
	void ShowUI();
	/** The mouse free for the interface (map full screen, settings) or captured for looking round. */
	void UpdateMouse();
	void FirstStartScan();
	#pragma endregion

	TSharedPtr<SWidget> Root;
	TSharedPtr<SHoloMap> HoloMap;
	TSharedPtr<SWidget> MiniBox, FullBox, Settings, Perf, TopBar;
	bool bHidden = false, bSettings = false, bPerf = false;
	float FpsSmooth = 60.f;
};
