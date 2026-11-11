// The controls table, the gamepad detection (Windows raw input device list: Sony 054C, Microsoft 045E, others) and the
// button badges, drawn by the game itself (coloured symbols, no makers' logos).
#include "UI/InputGlyphs.h"
#include "ADPLog.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"

#pragma region The table
namespace ADPInput
{
	const TArray<FAction>& Actions()
	{
		static const TArray<FAction> A = {
			{TEXT("Walk"),            EKeys::Invalid,   TEXT("W A S D"), EKeys::Invalid,                   TEXT("Left stick"), TEXT("Left stick"), TEXT("Left stick")},
			{TEXT("Look round"),      EKeys::Invalid,   TEXT("Mouse"),   EKeys::Invalid,                   TEXT("Right stick"), TEXT("Right stick"), TEXT("Right stick")},
			{TEXT("Run"),             EKeys::LeftShift, TEXT("Shift"),   EKeys::Gamepad_LeftThumbstick,    TEXT("LS"), TEXT("L3"), TEXT("L stick press")},
			{TEXT("Jump"),            EKeys::SpaceBar,  TEXT("Space"),   EKeys::Gamepad_FaceButton_Bottom, TEXT("A"), TEXT("✕"), TEXT("Button 1")},
			{TEXT("Crouch"),          EKeys::C,         TEXT("C"),       EKeys::Gamepad_FaceButton_Right,  TEXT("B"), TEXT("○"), TEXT("Button 2")},
			{TEXT("Use or press"),    EKeys::E,         TEXT("E"),       EKeys::Gamepad_FaceButton_Left,   TEXT("X"), TEXT("□"), TEXT("Button 3")},
			{TEXT("Change view (your eyes / from behind; in a truck: driver's seat / behind it)"), EKeys::V, TEXT("V"), EKeys::Gamepad_DPad_Up, TEXT("D-pad up"), TEXT("D-pad up"), TEXT("D-pad up")},
			{TEXT("Siren on / off (in a fire truck)"), EKeys::B, TEXT("B"), EKeys::Gamepad_DPad_Left, TEXT("D-pad left"), TEXT("D-pad left"), TEXT("D-pad left")},
			{TEXT("High beams on / off (in a truck)"), EKeys::L, TEXT("L"), EKeys::Gamepad_DPad_Down, TEXT("D-pad down"), TEXT("D-pad down"), TEXT("D-pad down")},
			{TEXT("Blue warning lights on / off (in a fire truck)"), EKeys::N, TEXT("N"), EKeys::Gamepad_RightThumbstick, TEXT("RS"), TEXT("R3"), TEXT("R stick press")},
			{TEXT("Menus"),           EKeys::G,         TEXT("G"),       EKeys::Gamepad_FaceButton_Top,    TEXT("Y"), TEXT("△"), TEXT("Button 4")},
			{TEXT("Map"),             EKeys::M,         TEXT("M"),       EKeys::Gamepad_RightShoulder,     TEXT("RB"), TEXT("R1"), TEXT("Right shoulder")},
			{TEXT("Hide UI"),         EKeys::H,         TEXT("H"),       EKeys::Gamepad_LeftShoulder,      TEXT("LB"), TEXT("L1"), TEXT("Left shoulder")},
			{TEXT("Settings"),        EKeys::Tab,       TEXT("Tab"),     EKeys::Gamepad_Special_Right,     TEXT("Menu"), TEXT("Options"), TEXT("Start")},
			{TEXT("Performance"),     EKeys::P,         TEXT("P"),       EKeys::Invalid,                   TEXT("-"), TEXT("-"), TEXT("-")},
			{TEXT("Free pointer (drag boxes)"), EKeys::MiddleMouseButton, TEXT("Middle mouse"), EKeys::Invalid, TEXT("-"), TEXT("-"), TEXT("-")},
			{TEXT("Back / show UI"),  EKeys::Escape,    TEXT("Esc"),     EKeys::Invalid,                   TEXT("-"), TEXT("-"), TEXT("-")},
			// the Buttons panel: its own key and button, used for nothing else on any device
			{TEXT("Buttons"),         EKeys::F1,        TEXT("F1"),      EKeys::Gamepad_Special_Left,      TEXT("View"), TEXT("Create"), TEXT("Select")},
		};
		return A;
	}

	const FAction* Find(const FString& Name)
	{
		return Actions().FindByPredicate([&](const FAction& A) { return A.Name == Name; });
	}

	bool CheckNoConflicts()
	{
		// also the keys bound in DefaultInput.ini for walking (W A S D, the sticks), which the table must never reuse
		TSet<FKey> Seen = {EKeys::W, EKeys::A, EKeys::S, EKeys::D};
		bool bOk = true;
		for (const FAction& A : Actions())
			for (const FKey& K : {A.Keyboard, A.Pad})
			{
				if (!K.IsValid()) continue;
				if (Seen.Contains(K)) { ADP_ERROR(TEXT("Controls: %s is used twice (again by %s)"), *K.ToString(), *A.Name); bOk = false; }
				Seen.Add(K);
			}
		return bOk;
	}
}
#pragma endregion

#pragma region The gamepad in use
namespace ADPInput
{
	EPad DetectPad()
	{
		// every raw input device's maker: Sony's gamepads are PlayStation, Microsoft's Xbox; any other gamepad generic
		UINT Count = 0;
		if (GetRawInputDeviceList(nullptr, &Count, sizeof(RAWINPUTDEVICELIST)) != 0 || Count == 0) return EPad::None;
		TArray<RAWINPUTDEVICELIST> List;
		List.SetNum(Count);
		if (GetRawInputDeviceList(List.GetData(), &Count, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) return EPad::None;
		EPad Found = EPad::None;
		for (const RAWINPUTDEVICELIST& D : List)
		{
			if (D.dwType != RIM_TYPEHID) continue;
			RID_DEVICE_INFO Info = {};
			Info.cbSize = sizeof(Info);
			UINT Size = sizeof(Info);
			if (GetRawInputDeviceInfoW(D.hDevice, RIDI_DEVICEINFO, &Info, &Size) == (UINT)-1) continue;
			// generic desktop gamepads and joysticks only (usage page 1, usage 4 or 5)
			if (Info.hid.usUsagePage != 1 || (Info.hid.usUsage != 4 && Info.hid.usUsage != 5)) continue;
			if (Info.hid.dwVendorId == 0x054C) return EPad::PlayStation;
			Found = Info.hid.dwVendorId == 0x045E ? EPad::Xbox : (Found == EPad::None ? EPad::Generic : Found);
		}
		// Xbox pads show up through XInput too (they may not be listed as HID gamepads)
		return Found;
	}

	bool& UsingPad() { static bool b = false; return b; }
	EPad& CurrentPad() { static EPad P = EPad::None; return P; }
}
#pragma endregion

#pragma region Badges
namespace ADPInput
{
	FString Label(const FAction& A, bool bPad, EPad Pad)
	{
		if (!bPad) return A.KeyboardLabel;
		return Pad == EPad::PlayStation ? A.PlayStationLabel : Pad == EPad::Xbox ? A.XboxLabel : A.GenericLabel;
	}

	static FLinearColor BadgeColour(const FAction& A, EPad Pad)
	{
		// the face buttons in their usual colours (Xbox: A green, B red, X blue, Y yellow; PlayStation: ✕ blue, ○ red, □ pink, △ green)
		const FKey K = A.Pad;
		if (Pad == EPad::Xbox)
		{
			if (K == EKeys::Gamepad_FaceButton_Bottom) return FLinearColor(0.2f, 0.62f, 0.2f);
			if (K == EKeys::Gamepad_FaceButton_Right) return FLinearColor(0.78f, 0.16f, 0.14f);
			if (K == EKeys::Gamepad_FaceButton_Left) return FLinearColor(0.12f, 0.36f, 0.8f);
			if (K == EKeys::Gamepad_FaceButton_Top) return FLinearColor(0.85f, 0.66f, 0.1f);
		}
		if (Pad == EPad::PlayStation)
		{
			if (K == EKeys::Gamepad_FaceButton_Bottom) return FLinearColor(0.35f, 0.55f, 0.95f);
			if (K == EKeys::Gamepad_FaceButton_Right) return FLinearColor(0.9f, 0.3f, 0.32f);
			if (K == EKeys::Gamepad_FaceButton_Left) return FLinearColor(0.9f, 0.45f, 0.75f);
			if (K == EKeys::Gamepad_FaceButton_Top) return FLinearColor(0.25f, 0.75f, 0.6f);
		}
		return FLinearColor(0.08f, 0.24f, 0.4f);
	}

	TSharedRef<SWidget> Glyph(const FAction& A, bool bPad, EPad Pad)
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(bPad ? BadgeColour(A, Pad) : FLinearColor(0.08f, 0.24f, 0.4f))
			.Padding(FMargin(7, 2))
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(FLinearColor::White)
				.Text(FText::FromString(Label(A, bPad, Pad)))
			];
	}
}
#pragma endregion
