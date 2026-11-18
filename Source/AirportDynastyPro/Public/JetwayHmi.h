// The jetway's HMI, live on its console monitor (owner, 2026-11-18, to-do 115; docs/Jetway-HMI-Plan.md). Unreal's own
// screen - no web page: a panel that draws itself (Slate) on the monitor's face in 3D, so you see it from across the cab
// (dim, backlit) before you touch it. One at a time for performance: on the jetway whose console is nearest you. Up
// close (E: ADPHUD's OpenHmi) a "+" about 1.5 cm across is the pointer - the mouse or the pad's stick moves it, E / the
// left mouse button / the pad's X click what it is over.
// The owner's design (his "Jetway Monitor Functions" chat): a status strip on every screen (mode, docking, aircraft,
// safety, CCTV, emergency stop); HOME (the T-10 preparation), DOCKING (live plan view, values, the manual pad, AUTO DOCK
// / UNDOCK), CCTV, STATUS, ALARMS (+ history), MAINTENANCE (a system check that runs), SETTINGS; BACK; EXIT.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JetwayHmi.generated.h"

class UWidgetComponent;
class SJetwayHmiPanel;

namespace ADPHmi { constexpr float ScrW = 1600.f, ScrH = 1000.f; }   // (the HMI screen in pixels)

UCLASS()
class AIRPORTDYNASTYPRO_API AJetwayHmi : public AActor
{
	GENERATED_BODY()
public:
	AJetwayHmi();
	static AJetwayHmi* Get(UWorld* World);
	virtual void Tick(float Dt) override;
	/** The jetway whose monitor it is on now (INDEX_NONE: none near). */
	int32 Jetway() const { return J; }
	/** Held on jetway J (while you use it up close), or free to follow you (INDEX_NONE). */
	void Hold(int32 InJ) { Held = InJ; }
	/** Up close: the "+" moved by you; else it follows where you look at the screen. */
	void SetPointer(bool bOn);
	bool Pointer() const { return bPointer; }
	void MoveCursor(const FVector2D& DeltaPx);
	void ClickCursor();
	/** The "+" put at a spot (parts of the screen: 0..1) - tests, and the crew later. */
	void PutCursor(const FVector2D& At);
	/** The "+" glides onto a button (its id: "tab:pos", "cmd:UNDOCK", "jog:FWD", "exit" ...). */
	void AimAt(const FString& Id, float GlideTime = 0.9f);
	bool Gliding() const { return GlideT >= 0.f; }
	/** A button's action (the same ids): what a click on it does. */
	void Command(const FString& Id);
	/** Something for the alarm history (text, level 0 info / 1 warning / 2 alarm). */
	void Event(const FString& Text, int32 Level = 0);

	// ---- what the panel shows (it reads these as it draws) ----
	struct FEvt { FDateTime At; FString Text; int32 Level = 0; };
	TArray<FEvt> Events;
	FString Tab = TEXT("home"); TArray<FString> BackStack;
	FString Note;                                        // (why the last press was refused)
	float DimLevel = 0.10f;                              // the screen's brightness in the cab (Settings)
	bool bAutoLevel = true;
	float RunHours = 0.f; int32 Docks = 0, Steps = 0; float CheckT = -1.f; FDateTime LastCheck;
	FVector2D Cur = FVector2D(800.f, 500.f); bool bShowCursor = false;

private:
	UPROPERTY() TObjectPtr<UWidgetComponent> Screen;
	TSharedPtr<SJetwayHmiPanel> Panel;
	int32 J = INDEX_NONE, Held = INDEX_NONE, LastState = -1;
	float Look = 0.f, Bright = 0.10f, Shown = -1.f;
	bool bPointer = false, bAim = false;
	FVector2D GlideFrom, GlideTo; float GlideT = -1.f, GlideLen = 0.9f;
	void Make();
	void Watch(float Dt);
};
