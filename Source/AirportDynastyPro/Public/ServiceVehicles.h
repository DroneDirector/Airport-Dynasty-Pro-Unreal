// The service vehicles of a turnaround (to-do 212): one table line per vehicle, everything else shared. A vehicle is one of
// AGSE's (the same driving, lamps, wheels, dirt and ground crew as the baggage tractors and the belt loader); GSEService.cpp
// gives it its people, its doors, its carried things and its work. How to add the next one: docs/Hired-Crew-Plan.md.
#pragma once

#include "CoreMinimal.h"

enum class EStaffTask : uint8;

struct FServiceDoor { const TCHAR *Prefix, *Pivot, *Handle, *Stand; float Deg; bool bSlide; };   // (Deg: to open, the root's yaw)
struct FServiceCab { const TCHAR* Seat; FServiceDoor Door; };   // (a seat with its own door that its crew never use: yours, also to ride along)
struct FServiceWiper { const TCHAR *Prefix, *Pivot; FVector Rest, Up; };   // (its blade's end lying down and wiped up, root cm)
struct FServiceItem { const TCHAR* Part; bool bAtHome, bDeep; FVector Stand; };                   // (its mesh name is also its point; bAtHome: there at the start; bDeep: taken from inside, up over the rear step)
struct FServiceSpec
{
	const TCHAR *Task = nullptr, *Model = nullptr, *Points = nullptr, *Label = nullptr, *Role = nullptr;   // the turnaround task, the model, Data/<Points>-riders.json, the name in the log, the sim's role
	EStaffTask Team = EStaffTask(0);                        // its smallest team (StaffPeople::Needs)
	int32 MaxPeople = 2;                                    // with the driver (seats)
	float HomeX = 0.f, HomeY = 0.f, HomeHx = 0.f, HomeHy = 1.f;   // its parking box (plan m; Airfield.cpp draws it)
	float HomeRoad = 0.f;                                   // m along its way to a road it comes along: it backs round that corner into its box (0: the loop beside the box)
	float InLat = 0.f, InAft = 0.f, InR = 0.f;              // not straight in: down this line from Far, then (from InAft on) over to Lat in two bends of InR cm, backed out the same way (0: straight)
	bool bFuel = false;                                     // the refueller (GSEFuel.cpp)
	bool bCater = false;                                    // the catering high-loader (GSECater.cpp)
	uint8 Wet = 0;                                          // 1 the potable water truck, 2 the toilet truck (GSEWater.cpp): backed under the tail from behind
	bool bRoof = false;                                     // (the stairs with a roof over them: first in rain and snow)
	bool bDeicer = false;                                   // the de-icing truck (GSEDeicer.cpp): its own lanes round the aircraft, stop by stop
	bool bAmbu = false;                                     // the ambulift (GSEAmbulift.cpp)
	bool bBus = false;                                      // the electric apron buses (GSEBus.cpp): one line for all of them, their places in Data/apron-buses.json
	bool bStairs = false;                                   // the passenger stairs (GSEStairs.cpp): one table line a vehicle, each with its own box
	float Lat = 0.f, Aft = 0.f, Far = 40.f;                 // its place at the stand: m left (+) of the centre line, m behind the nose wheel; driven up straight from Far
	float HalfL = 300.f, HalfW = 103.f, TurnR = 600.f, MaxSteer = 35.f, DoorDeg = 70.f;
	bool bJetway = false;                                   // the work is in the cabin: up the jetway's service stair
	TArray<FServiceDoor> Doors;                             // 0 the crew's, 1.. the load space's (opened in this order)
	TArray<FServiceItem> Items;
	TArray<int32> Take, Bring;                              // what person 0 (the driver), 1, 2 .. takes, and carries back instead (-1 the same)
	TArray<const TCHAR*> Seats;                             // person 1, 2, 3's seat (IK_Seat_*), through door 0
	TArray<FServiceCab> Cab;
	TArray<FServiceWiper> Wipers;                           // (pedals: parts Pedal_Accel / Pedal_Brake about PIV_Pedal_*)
	const TCHAR *LiveryBody = nullptr, *LiveryMain = nullptr;   // its paint slots (material names) that take your airline's colours
	float PlayerSpeed = 830.f;                              // cm/s at most when you drive it
	const TCHAR *NameL = nullptr, *NameR = nullptr, *NameRear = nullptr; float NameSize = 17.f, NameRearSize = 6.f;   // where the airport's name goes
};

namespace ADPService { const TArray<FServiceSpec>& All(); }
