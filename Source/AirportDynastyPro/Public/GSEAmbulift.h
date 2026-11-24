// The ambulift's own moving parts, its two wheelchairs and their passengers (to-do 212, Private/GSEAmbulift.cpp the parts,
// Private/GSEAmbuliftWork.cpp the work): what a FTractor of AGSE needs besides the shared ones. Everybody who works a part - its
// crew, you - only sets a Want; AGSE::AmbuParts moves the part there at its real speed and AmbuMay keeps the interlocks.
#pragma once

#include "CoreMinimal.h"

class ABabylonModel;
struct FStreamableHandle;

struct FSvcAmbu
{
	// legs, lift (cm), platform (0 folded .. 90), its rails (0 .. 1: the left one first), gates and bridge (cm out), tail platform
	// (0 stands .. 90), its arms (0 floor .. -82 ground), its rails, the flap (0 flat, 90 stop, -12 ramp), the doors (0 .. 1)
	enum EPart : uint8 { Legs, Lift, Plat, Rails, Gates, Bridge, Tail, Arms, TRails, Flap, DoorF, DoorR, Parts };
	float Now[Parts] = {}, Want[Parts] = {}; bool bMove[Parts] = {};
	int32 GLeg[4] = {-1, -1, -1, -1}, GSc[2] = {-1, -1}, GRam[2] = {-1, -1}, GRod[2] = {-1, -1}, GDeck = -1, GBridge = -1, GRail[2] = {-1, -1}, GGate[2] = {-1, -1}, GPb[2] = {-1, -1}, GPr[2] = {-1, -1}, GDoorF[2] = {-1, -1}, GDoorR[2] = {-1, -1}, GArms = -1, GTail = -1, GTRail[2] = {-1, -1}, GFlap = -1, GBody = -1, GLev[4] = {-1, -1, -1, -1};
	TArray<TPair<int32, FVector>> Keys;       // the buttons: their group and pivot (AmKey's order)
	float Press[32] = {}, Lev[4] = {}, LevTo[4] = {};
	bool bLive = true, bStop = false, bBlue = false;
	float Sill = 387.f;
	// the wheelchairs: in their bays, or pushed
	struct FChair
	{
		TWeakObjectPtr<ABabylonModel> M; int32 GWheel[2] = {-1, -1}, GCast[2] = {-1, -1};
		FVector At = FVector::ZeroVector; float Yaw = 0.f, Pitch = 0.f, Spin = 0.f, BayYaw = 0.f; bool bBay = true, bHide = false;   // (world, kept up to date in the bay too)
		int32 Pax = -1; TWeakObjectPtr<AActor> Who; FString Face; TSharedPtr<FStreamableHandle> Load; bool bAsked = false;
	} Chair[2];
	// somebody walking a line, maybe with a wheelchair ahead (points: the vehicle's own frame, z 0 the ground, 1 the body's floor,
	// 2 the tail lift's deck; or the world)
	struct FRoll { bool bOn = false, bLocal = true; TArray<FVector> P; float S = 0.f, L = 0.f, V = 0.f; int32 Chair = -1; };
	TMap<int32, FRoll> Rolls;                 // by crew
	// the job
	bool bArr = false; int32 N = 1, Room = -1; bool bRoom = false; float RoomX = 0.f;
};
