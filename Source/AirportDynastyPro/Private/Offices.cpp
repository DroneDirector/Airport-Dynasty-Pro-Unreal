// The old offices', operations room's and Academy's things in the terminal's rooms (Offices.h).
#include "Offices.h"
#include "Wardrobe.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "OpsMonitors.h"
#include "TerminalPlan.h"
#include "Engine/World.h"
#include "Components/RectLightComponent.h"
#include "Components/SpotLightComponent.h"

namespace ADPOffices
{
	/** Your smartphone lying on a room's desk: at (bx, by, bz) in the old Babylon room's own metres (its glass up, its
	 *  top (the cameras) away from where you sit), turned like the room plus a little (it lies as dropped). */
	static void PhoneOn(UWorld* W, ABabylonModel* Room, float RoomYaw, float Bx, float By, float Bz, float Turn)
	{
		if (!Room) return;
		FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// (a Babylon room's point (x, y, z) is (-x, z, y) under its model's root, metres to centimetres)
		const FVector At = Room->GetActorTransform().TransformPosition(FVector(-Bx, Bz, By) * 100.f);
		if (ABabylonModel* P = W->SpawnActor<ABabylonModel>(At, FRotator(0, RoomYaw + Turn, 0), S))
			if (!P->Place(TEXT("Smartphone"))) P->Destroy();
	}

	/** Your office (owner's render, 2026-11-26: Tools/render_owner_office.py, Blueprints/Terminal/Office/YourOffice-*.png):
	 *  its furniture one model, Art/OwnerOffice/make_owneroffice.py, in the render's own axes - X = plan x - 114, Y = plan z,
	 *  up from Floor 2's floor - so placed at plan (114, floor, 0) turned half round its (X, Y) is the plan's (114 + X, Y).
	 *  Its chairs sit at their own SitPoints (ABabylonModel::SeatSpots); its sit / IK / lie points in plan metres are in
	 *  Data/owner-office-points.json. Its lamps: the two pendant bars over the desk and the desk lamp. */
	static bool PlaceYours(UWorld* W)
	{
		const float FY = 6.01f;
		FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto P = [FY](float Bx, float By, float Bz) { return FTerminalPlan::ToWorld(114.f + Bx, FY + 0.002f + Bz, By); };   // (the model's 2 mm over the floor)
		ABabylonModel* M = W->SpawnActor<ABabylonModel>(P(0, 0, 0), FRotator(0, 180.f, 0), S);
		if (!M) return false;
		if (!M->Place(TEXT("OfficeOwner"))) { M->Destroy(); ADP_WARN(TEXT("Offices: your office's furniture (OfficeOwner) did not load")); return false; }
		M->Tags.Add(TEXT("OwnerOffice"));   // (your pet lives here: Pets.cpp PlaceHome)
		const FTransform R = M->GetModelRoot()->GetComponentTransform();
		const FLinearColor Warm(1.f, 0.93f, 0.84f);
		// the pendant bars: their LED strips (0.76 x 0.04 m, 2.52 m up, over the desk at y 4.3 -+ 0.45), facing down
		for (const float Y : {3.85f, 4.75f})
		{
			URectLightComponent* L = NewObject<URectLightComponent>(M);
			L->SetSourceWidth(76.f); L->SetSourceHeight(4.f);
			M->AddLamp(L, R.InverseTransformPosition(P(5.2f, Y, 2.515f)), 400.f, 600.f, Warm);
			L->SetWorldRotation(FRotator(-90.f, 0.f, 0.f));           // (down; its width along its Y: the bar, the plan's z)
		}
		// the desk lamp: its LED in the shade, shining down and towards you (the render's (0.342, 0, -0.94))
		{
			USpotLightComponent* L = NewObject<USpotLightComponent>(M);
			L->SetOuterConeAngle(50.f); L->SetInnerConeAngle(22.f); L->SetSourceRadius(4.f);
			M->AddLamp(L, R.InverseTransformPosition(P(5.328f, 4.96f, 1.13f)), 150.f, 300.f, Warm);
			const FVector Dir = P(5.328f + 0.342f, 4.96f, 1.13f - 0.94f) - P(5.328f, 4.96f, 1.13f);
			L->SetWorldRotation(Dir.Rotation());
		}
		// your phone on your desk right of the laptop, its top (the cameras) away from you (you face the plan's -x)
		if (ABabylonModel* Ph = W->SpawnActor<ABabylonModel>(P(5.1f, 4.55f, 0.7565f), FRotator(0, 107.f, 0), S))
			if (!Ph->Place(TEXT("Smartphone"))) Ph->Destroy();
		return true;
	}

	void Place(UWorld* W, const FString& Pet)
	{
		if (!W) return;
		// each office: the old office's middle in plan metres and its turn. On Floor 2 since 2026-11-21 (owner: the ground
		// floor is the departure baggage block now; Blueprints/Terminal/F2-Offices-v2.png, Art/TerminalAnnex/make_offices_f2.py):
		// the four offices side by side along the forecourt windows (x 118.7..156, z 0..9), each with the old office's
		// window side (its +z) turned to the forecourt wall (z 0) - yaw 180: you sit with your back to the windows, facing
		// the door onto the staff corridor; the things 5.9 m across and 0.3..6.1 m from the windows, clear of the walls,
		// the door's swing and the wardrobe. Yours is the west one: since 2026-11-26 (owner, Tools/render_owner_office.py)
		// x 114..126.3 on the Grand Hall window (its glass guard 0.32 m in), Office 2 x 126.3..139.1
		struct FAt { float X, Z, Yaw; bool bMine; };
		const float FY = 6.01f;                                       // Floor 2
		const FAt At[] = { {119.3f, 3.1f, 180.f, true}, {132.7f, 3.1f, 180.f, false}, {143.2f, 3.1f, 180.f, false}, {151.65f, 3.1f, 180.f, false} };
		FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		int32 Placed = 0;
		for (const FAt& A : At)
		{
			if (A.bMine) { if (PlaceYours(W)) ++Placed; continue; }
			const FString Model = A.bMine && (Pet == TEXT("dog") || Pet == TEXT("cat")) ? TEXT("Office") + Pet : TEXT("Office");
			ABabylonModel* M = W->SpawnActor<ABabylonModel>(FTerminalPlan::ToWorld(A.X, FY + 0.02f, A.Z), FRotator(0, A.Yaw, 0), S);
			if (M && M->Place(Model))
			{
				M->AddLights(Model); ++Placed;
				// your phone on your desk, right of the laptop (the leather top 0.79 m up; the cameras stand 3 mm proud)
				if (A.bMine) PhoneOn(W, M, A.Yaw, 0.42f, 0.7935f, 1.45f + 0.12f, 17.f);
			}
			else if (M) M->Destroy();
		}
		ADP_LOG(TEXT("Offices: the old office's things in %d offices (yours with the pet: %s)"), Placed, *Pet);
		// the wardrobe in every office (owner, 2026-11-12): against the partition wall at the room's far end from the
		// window (the corridor's, z 9 on Floor 2), its front (the cabinet's -y) into the room, so turned half round (the
		// plan's -z is Unreal's +y), at the office's east end, clear of the desk and of the door at its west end. Its back
		// stands 3 cm clear of the wall's face (the owner's rule for every wardrobe anywhere: never in the wall): the
		// partition is 0.12 m thick about its line (Art/TerminalAnnex/make_offices_f2.py), the cabinet 0.6 m deep
		// Yours (owner's render, 2026-11-26) against its east wall (the partition to Office 2, x 126.3) by the corridor, its
		// front to the plan's -x (yaw -90); z 6.1..7.3 (doors hinged at +-0.58) so its open doors stay clear of the room door
		// (x 123.9..125.1, its leaf down to z 7.8) and of the meeting table round (124.5, 4.0)
		struct FWd { float X, Z, Yaw; };
		const FWd Wd[] = { {126.3f - 0.06f - 0.3f - 0.03f, 6.7f, -90.f}, {137.8f, 9.f - 0.06f - 0.3f - 0.03f, 180.f},
			{146.f, 9.f - 0.06f - 0.3f - 0.03f, 180.f}, {154.6f, 9.f - 0.06f - 0.3f - 0.03f, 180.f} };
		int32 Wardrobes = 0;
		for (const FWd& A : Wd)
			if (AWardrobe* Wr = W->SpawnActor<AWardrobe>(FTerminalPlan::ToWorld(A.X, FY, A.Z), FRotator(0, A.Yaw, 0), S)) { if (Wr->Build()) ++Wardrobes; else Wr->Destroy(); }
		ADP_LOG(TEXT("Offices: %d wardrobes"), Wardrobes);
		// the operations room (Floor 2, x 118.7-133, z 12-26, its door south onto the staff corridor): the old room's desk
		// with the ultra-wide monitor, its window side (+z) to the room's west wall (onto the walkway by the Grand Hall's
		// window), the chair facing it (the old room is 5.4 m deep: its window wall 2.7 m from its middle)
		if (ABabylonModel* M = W->SpawnActor<ABabylonModel>(FTerminalPlan::ToWorld(121.4f, FY + 0.02f, 19.f), FRotator(0, -90.f, 0), S))
		{
			if (!M->Place(TEXT("Ops"))) M->Destroy();
			// your phone here too, right of the mouse (you sit on the room's -z side here: turned round)
			else PhoneOn(W, M, -90.f + 180.f, 0.62f, 0.776f, 1.25f - 0.2f, -14.f);
		}
		// its monitor, the one that matches your screen, where the old one stood (the desk's top 0.7725 m up, the screen
		// 1.40 m from the old room's middle towards its window); the models face their +z, turned to the chair
		if (AOpsMonitors* Mon = W->SpawnActor<AOpsMonitors>(FTerminalPlan::ToWorld(120.f, FY + 0.7925f, 19.f), FRotator(0, 90.f, 0), S)) Mon->Build();
		// the Academy (Floor 2, x 136-156, z 12-34.5, its doors south): the old classroom turned round, its front (the
		// whiteboard, its -x side) to the east wall, the whiteboard 15 cm clear of the wall's face and between two of its
		// windows (z 18.5..22.2, the windows at 16..18.2 and 22.5..24.7); the class's desks at z 16.6..24.1
		if (ABabylonModel* M = W->SpawnActor<ABabylonModel>(FTerminalPlan::ToWorld(141.23f, FY + 0.02f, 33.85f), FRotator(0, 180.f, 0), S))
			if (!M->Place(TEXT("Academy"))) M->Destroy();
	}
}
