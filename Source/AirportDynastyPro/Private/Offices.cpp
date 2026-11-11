// The old offices', operations room's and Academy's things in the terminal's rooms (Offices.h).
#include "Offices.h"
#include "BabylonModel.h"
#include "OpsMonitors.h"
#include "TerminalPlan.h"
#include "Engine/World.h"

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

	void Place(UWorld* W, const FString& Pet)
	{
		if (!W) return;
		// each office: the old office's middle in plan metres and its turn. The old office's window side (its +z) goes to
		// the outer wall, its bookshelves (its +x) along one side: the west offices (doors east) turned -90 degrees, the
		// east ones (doors west) +90 (the old room is 6.4 m square: its middle 3.4 m from the outer wall)
		struct FAt { float X, Z, Yaw; bool bMine; };
		const FAt At[] = { {117.4f, 5.f, -90.f, true}, {117.4f, 15.f, -90.f, false}, {152.6f, 25.f, 90.f, false}, {152.6f, 35.f, 90.f, false} };
		FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		int32 Placed = 0;
		for (const FAt& A : At)
		{
			const FString Model = A.bMine && (Pet == TEXT("dog") || Pet == TEXT("cat")) ? TEXT("Office") + Pet : TEXT("Office");
			ABabylonModel* M = W->SpawnActor<ABabylonModel>(FTerminalPlan::ToWorld(A.X, 0.02f, A.Z), FRotator(0, A.Yaw, 0), S);
			if (M && M->Place(Model))
			{
				M->AddLights(Model); ++Placed;
				// your phone on your desk, right of the laptop (the leather top 0.79 m up; the cameras stand 3 mm proud)
				if (A.bMine) PhoneOn(W, M, A.Yaw, 0.42f, 0.7935f, 1.45f + 0.12f, 17.f);
			}
			else if (M) M->Destroy();
		}
		UE_LOG(LogTemp, Log, TEXT("Offices: the old office's things in %d offices (yours with the pet: %s)"), Placed, *Pet);
		// the operations room (x 114-132, z 22-34, its door east): the old room's desk with the ultra-wide monitor, its
		// window side (+z) to the west wall as the offices', the chair facing it (the old room is 5.4 m deep: its window
		// wall 2.7 m from its middle)
		if (ABabylonModel* M = W->SpawnActor<ABabylonModel>(FTerminalPlan::ToWorld(116.7f, 0.02f, 28.f), FRotator(0, -90.f, 0), S))
		{
			if (!M->Place(TEXT("Ops"))) M->Destroy();
			// your phone here too, right of the mouse (you sit on the room's -z side here: turned round)
			else PhoneOn(W, M, -90.f + 180.f, 0.62f, 0.776f, 1.25f - 0.2f, -14.f);
		}
		// its monitor, the one that matches your screen, where the old one stood (the desk's top 0.7725 m up, the screen
		// 1.40 m from the old room's middle towards its window: x 115.30); the models face their +z, turned to the chair
		if (AOpsMonitors* Mon = W->SpawnActor<AOpsMonitors>(FTerminalPlan::ToWorld(115.30f, 0.7925f, 28.f), FRotator(0, 90.f, 0), S)) Mon->Build();
		// the Academy (x 136-156, z 0-20, its doors west): the old classroom turned round, its front (the whiteboard, its
		// -x side) to the east wall and its back to the doors; the old room's middle (-10.1, 13.5) on the room's z middle,
		// the whiteboard 15 cm clear of the wall's face (the old room's own wall was thinner: the board sank into it)
		if (ABabylonModel* M = W->SpawnActor<ABabylonModel>(FTerminalPlan::ToWorld(141.23f, 0.02f, 23.5f), FRotator(0, 180.f, 0), S))
			if (!M->Place(TEXT("Academy"))) M->Destroy();
	}
}
