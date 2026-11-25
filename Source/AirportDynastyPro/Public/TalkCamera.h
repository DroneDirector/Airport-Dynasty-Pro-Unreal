// The talk view (to-do 298, owner): while you talk to someone (People.h's menu) the camera glides in on them - the whole
// face, the neck and the top of the head, like a profile picture - and back out afterwards, in first person and from behind
// alike. It changes only what AADPWalker::CalcCamera hands out: your control rotation, your view mode and your own cameras
// are never touched, so the view you had is the view you get back. Any other view (vehicles, drone, devices, photo, riding)
// isn't the walker's, and simply ends it. Costs one test of a bool when nobody talks.
// All the while it has a portrait lens (owner): you see them sharp to the edges of hair and shoulders and everything behind
// and in front softly out of focus, round, faded in and out with the glide.
// Console: adp.TalkCam 0|1; the player's own switch is in the settings (ADP.Graphics, TalkCam).
#pragma once

#include "CoreMinimal.h"

class AActor;
class AADPWalker;
class USkeletalMeshComponent;
struct FMinimalViewInfo;

namespace ADPTalkCam
{
	/** The talk menu opened on this person. */
	void Begin(AActor* Who, USkeletalMeshComponent* Body, const FString& Name);
	/** The talk menu closed, for whatever reason: the camera glides back. */
	void End(const TCHAR* Why);
	/** Your own view as it would be, made into the talk view (AADPWalker::CalcCamera). */
	void Apply(AADPWalker& Me, float Dt, FMinimalViewInfo& V);
	/** Each frame (AADPPeople::Tick): lets go when your own view isn't the one seen any more. */
	void Tick();
	/** Your view given back at once (the world goes). */
	void Drop(const TCHAR* Why);
	/** How far the talk camera is in, 0 your own view .. 1 (what they look at: TalkLook.h). */
	float Amount();
	/** The talk card: beside the face, which is always in the screen's middle. Given its scale and size, and how far it was dragged
	 *  (a share of the window; held so that the card stays in it): its way from the middle (Slate units) and its size there. */
	void PanelPlace(float Scale, const FVector2D& LocalSize, FVector2D& Drag, FVector2D& Move, float& Size);
	/** The player's switch, on or off, kept in their settings. */
	bool IsOn();
	void SetOn(bool bOn);
	void LoadSetting();
	/** The player set their own Blur just now: that one is theirs again (ADPHUD::SetBlur). */
	void BlurWasSet();
}
