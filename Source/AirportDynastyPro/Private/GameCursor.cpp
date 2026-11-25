#include "GameCursor.h"
#include "ADPLog.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Misc/ConfigCacheIni.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
	float GOwnSpeed = -1.f;

	/** Windows' pointer speed (its slider's 20 steps) and "Enhance pointer precision", looked up every 2 s. */
	void WindowsPointer(float& Scale, bool& bAccel)
	{
		static float S = 1.f; static bool bA = true;
#if PLATFORM_WINDOWS
		static double At = -10.0;
		const double Now = FPlatformTime::Seconds();
		if (Now - At > 2.0)
		{
			At = Now;
			static const float Steps[21] = { 1.f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f, 1.f, 1.25f, 1.5f, 1.75f, 2.f, 2.25f, 2.5f, 2.75f, 3.f, 3.25f, 3.5f };
			int32 N = 10; int32 M[3] = { 6, 10, 1 };
			if (!::SystemParametersInfoW(SPI_GETMOUSESPEED, 0, &N, 0) || !::SystemParametersInfoW(SPI_GETMOUSE, 0, M, 0)) ADP_WARN(TEXT("Cursor: Windows' pointer settings can't be read"));
			const float S2 = Steps[FMath::Clamp(N, 1, 20)]; const bool bA2 = M[2] != 0;
			static bool bSaid = false;
			if (!bSaid || S2 != S || bA2 != bA) { bSaid = true; ADP_LOG(TEXT("Cursor: Windows' pointer speed step %d (x%.3f), precision enhanced %d, the game's own speed x%.2f"), N, S2, bA2 ? 1 : 0, ADPGameCursor::Speed()); }
			S = S2; bA = bA2;
		}
#endif
		Scale = S; bAccel = bA;
	}
}

float ADPGameCursor::Speed()
{
	if (GOwnSpeed < 0.f) { GOwnSpeed = 1.f; GConfig->GetFloat(TEXT("ADP.Input"), TEXT("CursorSpeed"), GOwnSpeed, GGameUserSettingsIni); GOwnSpeed = FMath::Clamp(GOwnSpeed, 0.25f, 4.f); }
	return GOwnSpeed;
}

void ADPGameCursor::SetSpeed(float S)
{
	GOwnSpeed = FMath::Clamp(S, 0.25f, 4.f);
	GConfig->SetFloat(TEXT("ADP.Input"), TEXT("CursorSpeed"), GOwnSpeed, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni);
	ADP_LOG(TEXT("Cursor: the game's own speed set to x%.2f"), GOwnSpeed);
}

FVector2D ADPGameCursor::Move(const APlayerController* PC, float Dt, bool bStick)
{
	if (!PC || !PC->PlayerInput) return FVector2D::ZeroVector;
	// the mouse's own counts (GetInputMouseDelta is cut to 7 %, scaled by the view's zoom and smoothed)
	FVector2D D(PC->PlayerInput->GetRawKeyValue(EKeys::MouseX), -PC->PlayerInput->GetRawKeyValue(EKeys::MouseY));
	float S = 1.f; bool bA = true; WindowsPointer(S, bA);
	static double Vs = 0.0;
	const float T = FMath::Clamp(Dt, 0.002f, 0.1f);
	Vs = FMath::Lerp(Vs, D.Size() / T, FMath::Clamp(T / 0.04f, 0.f, 1.f));
	// "Enhance pointer precision": a little under 1 to 1 moved slowly, up to 2.4 times in a flick (over the last moments, so
	// that a single long or short frame doesn't make it jump)
	if (bA) S *= FMath::Lerp(0.8f, 2.4f, FMath::SmoothStep(150.f, 4000.f, (float)Vs));
	D *= S;
	if (bStick)
	{
		const FVector2D St(PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX), -PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY));
		const float L = FMath::Min(1.f, (float)St.Size());
		if (L > 0.15f)
		{
			int32 W = 0, H = 1080; PC->GetViewportSize(W, H);
			const float K = (L - 0.15f) / 0.85f;
			D += St / St.Size() * (K * K) * (H / 0.8f) * FMath::Min(Dt, 0.05f);
		}
	}
	return D * Speed();
}

FVector2D ADPGameCursor::SizeOnWindow(const APlayerController* PC, const FVector& Mid, const FVector& Right, const FVector& Up, const FVector2D& Size)
{
	if (!PC) return FVector2D::ZeroVector;
	FVector2D A, B, C, E;
	if (!PC->ProjectWorldLocationToScreen(Mid - Right * Size.X * 0.5f, A, false) || !PC->ProjectWorldLocationToScreen(Mid + Right * Size.X * 0.5f, B, false)
		|| !PC->ProjectWorldLocationToScreen(Mid - Up * Size.Y * 0.5f, C, false) || !PC->ProjectWorldLocationToScreen(Mid + Up * Size.Y * 0.5f, E, false)) return FVector2D::ZeroVector;
	return FVector2D(FVector2D::Distance(A, B), FVector2D::Distance(C, E));
}
