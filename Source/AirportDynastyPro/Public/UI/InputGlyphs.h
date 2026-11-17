// The controls in one table: every action with its keyboard key and its gamepad button, and how each kind of gamepad
// shows that button (Xbox: A B X Y, LB RB, View, Menu; PlayStation: ✕ ○ □ △, L1 R1, Create, Options; others: plain
// names). The game knows which gamepad is plugged in (by its maker's id: Sony, Microsoft, or another) and which was used
// last (keyboard and mouse, or the gamepad), and shows the right symbols everywhere (the Buttons panel, the key bindings
// in Settings, the holographic map's legend). The table is checked at start: no key or button is ever used twice.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class SWidget;

namespace ADPInput
{
	enum class EPad : uint8 { None, Xbox, PlayStation, Generic };

	struct FAction
	{
		FString Name;
		FKey Keyboard;          // EKeys::Invalid when it's the mouse or several keys
		FString KeyboardLabel;
		FKey Pad;               // EKeys::Invalid when it's a stick
		FString XboxLabel, PlayStationLabel, GenericLabel;
		// its group in Settings > Controls (owner, 2026-11-17: "categorize these things ... and automatically categorize new
		// key bindings"): one of Categories(); left empty, CategoryOf() works it out from the name, else "Other"
		FString Category;
	};

	/** The groups Settings > Controls shows, in order: On foot, Driving, Fire truck, Flying, At the airport, Menus and
	 *  interface, Other. */
	const TArray<FString>& Categories();
	/** An action's group: its own Category, else worked out from its name (truck, drone, door, map...), else "Other". */
	FString CategoryOf(const FAction& A);
	/** Everything Settings > Controls lists: every action, the drone's own, and the ones used at things (doors, seats,
	 *  door panels, the wash hose), each once, with its group. */
	TArray<FAction> AllForSettings();

	/** Every action, in the order the Buttons panel and Settings list them. */
	const TArray<FAction>& Actions();
	/** Drone mode's own controls (DroneCam.h), listed in the Buttons panel while you fly. */
	const TArray<FAction>& DroneActions();
	/** The fire station's bay doors (only listed within 25 m of the station). */
	const FAction& BayDoorsAction();
	/** The wash hose: in your hand (spray, hang up) or within reach of its hook (take it). */
	const TArray<FAction>& HoseActions(bool bHeld, bool bAtMount);
	/** A seat: sit down by one, stand up from it. */
	const FAction& SeatAction(bool bSitting);
	/** A door panel: step up to it and use its buttons. */
	const FAction& PanelAction();
	/** At a hinged door you face: open or close it (E / X). */
	const FAction& DoorAction(bool bOpen);
	/** The action called Name (nullptr when there's none). */
	const FAction* Find(const FString& Name);
	/** Logs (and puts in the problem log) any key or button used by two actions; true when there's none. */
	bool CheckNoConflicts();

	/** The gamepad plugged in: Sony's (PlayStation), Microsoft's or any XInput pad (Xbox), another, or none. */
	EPad DetectPad();
	/** What's in use: a gamepad (true) or keyboard and mouse (false); set from the last key or button pressed. */
	bool& UsingPad();
	EPad& CurrentPad();

	/** An action's symbol as a small widget for the device in use (or a given pad): a coloured badge with its label. */
	TSharedRef<SWidget> Glyph(const FAction& A, bool bPad, EPad Pad);
	/** The same as plain text (for drawn legends). */
	FString Label(const FAction& A, bool bPad, EPad Pad);
}
