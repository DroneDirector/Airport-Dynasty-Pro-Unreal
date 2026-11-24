// The service vehicles at work (to-do 212; ServiceVehicles.h, docs/Hired-Crew-Plan.md): the cabin cleaning van first. Its
// driver and cleaners - people you have hired - walk to it, get in by their doors and sit, drive to the stand, take their
// things out of the back, go up the jetway's service stair when the last passenger is off, clean, come down with the full
// bags, load them, and drive back to the van's box. Nobody free: the job waits in the queue (UStaffPeople::AskTeam) and the
// flight with it. The other airlines' flights, and yours while you employ fewer than two: the agent, as data, as before.
// There is no cabin inside the aircraft yet (to-do 209): at its door they are out of sight and the rows are worked by time.
#include "GSE.h"
#include "ServiceVehicles.h"
#include "GSEFuel.h"
#include "GSECater.h"
#include "GSEWater.h"
#include "GSEStairs.h"
#include "GSEDeicer.h"
#include "GSEAmbulift.h"
#include "Airfield.h"
#include "Misc/PackageName.h"
#include "Tugs.h"
#include "StaffPeople.h"
#include "Turnaround.h"
#include "ServiceDrive.h"
#include "Puddles.h"
#include "Sim/ADPSim.h"
#include "JetwayRigs.h"
#include "VehicleRiders.h"
#include "CrewPose.h"
#include "Site.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "Dom/JsonObject.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"

namespace
{
	const float SvRowSeconds = 40.f;   // (one person, one row of six: rubbish, belts crossed, tables wiped, the floor)
	const float SvPatience = 2700.f;   // (45 min: stuck - the flight no longer waits for it)
	const FVector SvUp(0, 0, 1);
	FVector SvFace(const FVector& From, const FVector& To) { const FVector D = (To - From).GetSafeNormal2D(); return D.IsNearlyZero() ? FVector(1, 0, 0) : D; }
	const FServiceSpec& SvSpec(int32 i) { return ADPService::All()[i]; }
	const FServiceDoor& SvDoor(const FServiceSpec& S, int32 d) { return d < S.Doors.Num() ? S.Doors[d] : S.Cab[d - S.Doors.Num()].Door; }   // (its doors, then the Cab seats' own)
	bool SvIs(int32 i, const TCHAR* Task) { return FCString::Strcmp(SvSpec(i).Task, Task) == 0; }
}

const TArray<FServiceSpec>& ADPService::All()
{
	static TArray<FServiceSpec> L;
	if (!L.Num())
	{
		FServiceSpec S;
		S.Task = TEXT("clean"); S.Model = TEXT("CleaningVan"); S.Points = TEXT("cleaningvan"); S.Label = TEXT("cleaning van"); S.Role = TEXT("cleaning"); S.Team = EStaffTask::Cleaning; S.MaxPeople = 4;
		S.HomeX = -259.6f; S.HomeY = 91.f;
		// left of the aircraft behind the wing, ahead of the tailplane (the belt loader's mirror): in from the tail, never along a stand line
		S.Lat = 11.4f; S.Aft = 25.5f; S.TurnR = 620.f; S.MaxSteer = 36.f; S.DoorDeg = 68.f; S.bJetway = true;
		S.Doors = {{TEXT("SlideDoor"), TEXT("PIV_SlideDoor"), TEXT("IK_Handle_SlideDoor_Out"), TEXT("IK_Stand_SlideDoor"), 0.f, true},
			{TEXT("RearDoorR"), TEXT("PIV_RearDoorR"), TEXT("IK_Handle_RearDoorR_Out"), TEXT("IK_Stand_RearDoorR"), -100.f, false},
			{TEXT("RearDoorL"), TEXT("PIV_RearDoorL"), TEXT("IK_Handle_RearDoorL_Edge"), TEXT("IK_Stand_RearDoorL"), 100.f, false}};
		const FVector Shelf(-328.f, -42.f, 0.f), In(-165.f, 5.f, 58.5f), Cage(-215.f, 10.f, 58.5f);
		S.Items = {{TEXT("Stock_BagRoll_0"), true, false, Shelf}, {TEXT("Stock_Caddy_0"), true, false, Shelf}, {TEXT("Stock_Vacuum_0"), true, true, In}, {TEXT("Stock_BagRoll_1"), true, false, Shelf},
			{TEXT("Stock_BagFull_0"), false, true, Cage}, {TEXT("Stock_BagFull_1"), false, true, Cage}};
		S.Take = {0, 1, 2, 3}; S.Bring = {4, -1, -1, 5};
		S.Seats = {TEXT("IK_Seat_SeatRow2"), TEXT("IK_Seat_SeatRow1"), TEXT("IK_Seat_SeatRow0")};   // (the farthest from the door first)
		S.NameL = TEXT("IK_Name_L"); S.NameR = TEXT("IK_Name_R"); S.NameRear = TEXT("IK_Name_Rear");
		S.Cab = {{TEXT("IK_Seat_SeatPassenger"), {TEXT("DoorPassenger"), TEXT("PIV_DoorPassenger"), TEXT("IK_Handle_DoorPassenger_Out"), TEXT("IK_Stand_DoorPassenger"), -68.f, false}}};
		S.Wipers = {{TEXT("WiperL"), TEXT("PIV_Wiper_L"), FVector(210.6f, -4.f, 143.3f), FVector(176.3f, -50.f, 178.f)}, {TEXT("WiperR"), TEXT("PIV_Wiper_R"), FVector(204.2f, 68.f, 143.3f), FVector(178.2f, 22.f, 178.f)}};
		S.LiveryBody = TEXT("CV_Livery_Body_Paint"); S.LiveryMain = TEXT("CV_Livery_Main_Paint");
		L.Add(S);
		{
			// the refueller: under the right wing's outer part facing the terminal, its platform below the coupling; the way
			// in and out beside the wingtip and under the wing (3.18 m under 3.7 m), clear of the cones, the engine, the belt
			// loader and the baggage train by 1.1 m and more (swept with its mirrors)
			FServiceSpec F;
			F.Task = TEXT("fuel"); F.Model = TEXT("Refueller"); F.Points = TEXT("refueller"); F.Label = TEXT("refueller"); F.Role = TEXT("fuel"); F.Team = EStaffTask::Fuel; F.MaxPeople = 1; F.bFuel = true;
			F.HomeX = -260.2f; F.HomeY = 146.f; F.HomeRoad = 25.f;   // (backed round the corner of the road to the tower, y 171)
			F.Lat = -11.08f; F.Aft = 7.45f; F.InLat = -19.5f; F.InAft = 26.7f; F.InR = 900.f;
			F.HalfL = 510.f; F.HalfW = 123.f; F.TurnR = 660.f; F.MaxSteer = 38.f; F.DoorDeg = 68.f; F.PlayerSpeed = 830.f;
			F.Doors = {{TEXT("PanelDoorL"), TEXT("PIV_PanelDoorL"), TEXT("IK_Handle_PanelDoorL_Out"), TEXT("IK_Stand_PanelDoorL"), -110.f, false},
				{TEXT("PanelDoorR"), TEXT("PIV_PanelDoorR"), TEXT("IK_Handle_PanelDoorR_Out"), TEXT("IK_Stand_PanelDoorR"), 110.f, false}};
			F.Cab = {{TEXT("IK_Seat_SeatPassenger"), {TEXT("DoorPassenger"), TEXT("PIV_DoorPassenger"), TEXT("IK_Handle_DoorPassenger_Out"), TEXT("IK_Stand_DoorPassenger"), -68.f, false}}};
			F.Wipers = {{TEXT("WiperL"), TEXT("PIV_Wiper_L"), FVector(500.5f, -9.f, 186.4f), FVector(487.f, -60.f, 258.f)}, {TEXT("WiperR"), TEXT("PIV_Wiper_R"), FVector(496.8f, 94.f, 186.4f), FVector(489.f, 43.f, 258.f)}};
			F.NameL = TEXT("IK_Name_L"); F.NameR = TEXT("IK_Name_R"); F.NameRear = TEXT("IK_Name_Rear"); F.NameSize = 24.f;
			L.Add(F);
		}
		{
			// the catering high-loader: square on to the front right service door, then the rear one; down the middle between
			// the two wingtips (2.2 m from each with its mirrors), never under a wing; its crew of two in the cab
			FServiceSpec C;
			C.Task = TEXT("cat1"); C.Model = TEXT("CateringTruck"); C.Points = TEXT("cateringtruck"); C.Label = TEXT("catering truck"); C.Role = TEXT("catering"); C.Team = EStaffTask::Catering; C.MaxPeople = 2; C.bCater = true;
			C.HomeX = -253.5f; C.HomeY = 146.f; C.HomeRoad = 25.f;   // (beside the refueller, its own arc of the same way home)
			C.Lat = -7.34f; C.Aft = -0.3f; C.InLat = -20.43f; C.InAft = 6.3f; C.InR = 660.f;
			C.HalfL = 456.f; C.HalfW = 123.f; C.TurnR = 660.f; C.MaxSteer = 38.f; C.DoorDeg = 68.f;
			C.Doors = {{TEXT("DoorPassenger"), TEXT("PIV_DoorPassenger"), TEXT("IK_Handle_DoorPassenger_Out"), TEXT("IK_Stand_DoorPassenger"), -68.f, false},
				{TEXT("BoxDoorL"), TEXT("PIV_BoxDoorL"), TEXT("IK_Handle_BoxDoorL_Out"), TEXT("IK_Stand_BoxDoorL"), 100.f, false}};
			C.Seats = {TEXT("IK_Seat_SeatPassenger")};
			C.Wipers = {{TEXT("WiperL"), TEXT("PIV_Wiper_L"), FVector(446.8f, -9.f, 186.4f), FVector(433.3f, -60.f, 258.f)}, {TEXT("WiperR"), TEXT("PIV_Wiper_R"), FVector(443.1f, 94.f, 186.4f), FVector(435.3f, 43.f, 258.f)}};
			C.NameL = TEXT("IK_Name_L"); C.NameR = TEXT("IK_Name_R"); C.NameRear = TEXT("IK_Name_Rear"); C.NameSize = 30.f; C.NameRearSize = 10.f;
			C.LiveryBody = TEXT("CT_Livery_Body_Paint"); C.LiveryMain = TEXT("CT_Livery_Main_Paint");
			L.Add(C);
		}
		for (int32 k = 1; k <= 2; ++k)
		{
			// the potable water truck and the toilet truck: backed under the tail to the two panels (GSEWater.cpp), one at a time;
			// the water truck's box on the west side, the toilet truck's at the east end
			FServiceSpec W; const bool bT = k == 2;
			W.Wet = uint8(k); W.Task = bT ? TEXT("toilet") : TEXT("water"); W.Model = bT ? TEXT("ToiletTruck") : TEXT("WaterTruck"); W.Points = bT ? TEXT("toilettruck") : TEXT("watertruck"); W.Label = bT ? TEXT("toilet truck") : TEXT("water truck"); W.Role = TEXT("water"); W.Team = bT ? EStaffTask::Toilet : EStaffTask::Water; W.MaxPeople = 1;
			W.HomeX = bT ? 554.6f : -256.4f; W.HomeY = bT ? 110.f : 117.f;
			W.Lat = bT ? -0.85f : 0.6f; W.Aft = bT ? 28.32f : 28.6f;
			W.HalfL = 291.f; W.HalfW = 101.f; W.TurnR = 480.f; W.MaxSteer = 38.f; W.DoorDeg = 68.f;
			W.Doors = {{TEXT("DoorPassenger"), TEXT("PIV_DoorPassenger"), TEXT("IK_Handle_DoorPassenger_Out"), TEXT("IK_Stand_DoorPassenger"), -68.f, false}};
			if (!bT) W.Doors.Add({TEXT("PanelDoorL"), TEXT("PIV_PanelDoorL"), TEXT("IK_Handle_PanelDoorL"), TEXT("IK_Stand_PanelDoorL"), 110.f, false});   // (its fill point)
			W.Seats = {TEXT("IK_Seat_SeatPassenger")};
			W.Wipers = {{TEXT("WiperL"), TEXT("PIV_Wiper_L"), FVector(279.6f, -4.f, 152.4f), FVector(255.5f, -41.f, 203.f)}, {TEXT("WiperR"), TEXT("PIV_Wiper_R"), FVector(273.8f, 76.f, 152.4f), FVector(261.f, 39.f, 203.f)}};
			W.NameL = TEXT("IK_Name_L"); W.NameR = TEXT("IK_Name_R"); W.NameRear = bT ? TEXT("IK_Name_Rear") : TEXT("IK_NamePanel_Rear"); W.NameSize = 13.f; W.NameRearSize = 8.f;
			W.LiveryBody = bT ? TEXT("TT_Livery_Body_Paint") : TEXT("WT_Livery_Body_Paint"); W.LiveryMain = bT ? TEXT("TT_Livery_Main_Paint") : TEXT("WT_Livery_Main_Paint");
			L.Add(W);
		}
		for (int32 k = 0; k < 2; ++k)
		{
			// (open, and the same with a roof over them - that one only when its model is there)
			if (k == 1 && !FPackageName::DoesPackageExist(TEXT("/Game/Babylon/PassengerStairsRoof/passengerstairsroof/StaticMeshes/StairUp"))) continue;
			// the passenger stairs: square on to the front left door, down the lane beside the left wingtip; only where no jetway
			// is. A second one (the rear door too) is another line of this with its own box
			FServiceSpec P;
			P.bStairs = true; P.Task = TEXT("stairs"); P.bRoof = k == 1; P.Model = P.bRoof ? TEXT("PassengerStairsRoof") : TEXT("PassengerStairs"); P.Points = TEXT("passengerstairs"); P.Label = P.bRoof ? TEXT("roofed passenger stairs") : TEXT("passenger stairs"); P.Role = TEXT("ground"); P.Team = EStaffTask::Stairs; P.MaxPeople = 2;
			P.HomeX = P.bRoof ? 551.f : 557.9f; P.HomeY = 140.f;   // (both at the east end, the toilet truck's way out between them)
			P.Lat = 6.19f; P.Aft = -0.3f; P.InLat = 20.43f; P.InAft = 6.3f; P.InR = 460.f;
			P.HalfL = 299.f; P.HalfW = 101.f; P.TurnR = 450.f; P.MaxSteer = 35.f; P.DoorDeg = 68.f; P.PlayerSpeed = 700.f;
			P.Doors = {{TEXT("DoorPassenger"), TEXT("PIV_DoorPassenger"), TEXT("IK_Handle_DoorPassenger_Out"), TEXT("IK_Stand_DoorPassenger"), -68.f, false}};
			P.Seats = {TEXT("IK_Seat_SeatPassenger")};
			P.Wipers = {{TEXT("WiperL"), TEXT("PIV_Wiper_L"), FVector(273.5f, -6.f, 115.7f), FVector(262.5f, -74.f, 177.f)}, {TEXT("WiperR"), TEXT("PIV_Wiper_R"), FVector(271.7f, 75.f, 115.7f), FVector(263.5f, 7.f, 177.f)}};
			P.NameL = TEXT("IK_Name_L"); P.NameR = TEXT("IK_Name_R"); P.NameSize = 13.f; P.LiveryBody = TEXT("PS_Livery_Body_Paint"); P.LiveryMain = TEXT("PS_Livery_Main_Paint");
			L.Add(P);
		}
		{
			// the de-icing truck: its own lanes round the aircraft (GSEDeicer.cpp); backed into its box at the east end from the north
			FServiceSpec D;
			D.bDeicer = true; D.Task = TEXT("deice"); D.Model = TEXT("Deicer"); D.Points = TEXT("deicer"); D.Label = TEXT("de-icing truck"); D.Role = TEXT("ground"); D.Team = EStaffTask::Deice; D.MaxPeople = 2;
			D.HomeX = 561.5f; D.HomeY = 101.5f; D.Lat = 20.43f; D.Aft = 20.f; D.Far = 42.f;
			D.HalfL = 503.f; D.HalfW = 121.f; D.TurnR = 680.f; D.MaxSteer = 38.f; D.DoorDeg = 68.f; D.PlayerSpeed = 830.f;
			D.Doors = {{TEXT("DoorPassenger"), TEXT("PIV_DoorPassenger"), TEXT("IK_Handle_DoorPassenger_Out"), TEXT("IK_Stand_DoorPassenger"), -68.f, false}};
			D.Seats = {TEXT("IK_Seat_SeatPassenger")};
			D.Cab = {{TEXT("IK_Seat_CabinSeat"), {TEXT("CabinDoor"), TEXT("PIV_CabinDoor"), TEXT("IK_Handle_CabinDoor_Out"), TEXT("IK_Stand_DeckSteps"), 0.f, true}}};   // (the spray cabin: its door moves with it)
			D.Wipers = {{TEXT("WiperL"), TEXT("PIV_Wiper_L"), FVector(491.1f, -9.f, 177.7f), FVector(477.6f, -60.f, 249.3f)}, {TEXT("WiperR"), TEXT("PIV_Wiper_R"), FVector(489.4f, 90.f, 177.7f), FVector(481.6f, 39.f, 249.3f)}};
			D.NameL = TEXT("IK_Name_L"); D.NameR = TEXT("IK_Name_R"); D.NameSize = 22.f; D.LiveryBody = TEXT("DI_Livery_Body_Paint"); D.LiveryMain = TEXT("DI_Livery_Main_Paint");
			L.Add(D);
		}
		{
			// the ambulift: down the caterers' lane to R1 (GSEAmbuliftWork.cpp), backed into its box at the east end from the north
			FServiceSpec D;
			D.bAmbu = true; D.Task = TEXT("ambulift"); D.Model = TEXT("Ambulift"); D.Points = TEXT("ambulift"); D.Label = TEXT("ambulift"); D.Role = TEXT("ground"); D.Team = EStaffTask::Ambulift; D.MaxPeople = 2;
			D.HomeX = 550.6f; D.HomeY = 92.f; D.Lat = -20.43f; D.InLat = -20.43f; D.Aft = 14.f; D.InAft = 30.f; D.Far = 42.f; D.InR = 620.f;
			D.HalfL = 504.f; D.HalfW = 121.f; D.TurnR = 620.f; D.MaxSteer = 38.f; D.DoorDeg = 68.f; D.PlayerSpeed = 830.f;
			D.Doors = {{TEXT("DoorPassenger"), TEXT("PIV_DoorPassenger"), TEXT("IK_Handle_DoorPassenger_Out"), TEXT("IK_Stand_DoorPassenger"), -68.f, false}};
			D.Seats = {TEXT("IK_Seat_SeatPassenger")};
			D.NameL = TEXT("IK_Name_L"); D.NameR = TEXT("IK_Name_R"); D.NameRear = TEXT("IK_Name_Rear"); D.NameSize = 20.f; D.NameRearSize = 8.f; D.LiveryBody = TEXT("AL_Livery_Body_Paint"); D.LiveryMain = TEXT("AL_Livery_Main_Paint");
			L.Add(D);
		}
	}
	return L;
}

// ------------------------------------------------------------------ the vehicle

const TCHAR* AGSE::SvcModel(int32 Svc) { return SvSpec(Svc).Model; }
FVector AGSE::SvcP(const FTractor& T, const TCHAR* Name) const { const FVector* V = T.Pt.Find(Name); return V ? *V : FVector::ZeroVector; }

void AGSE::SvcSetup()
{
	for (int32 i = 0; i < ADPService::All().Num(); ++i)
	{
		const FServiceSpec& S = SvSpec(i);
		FVehicleSpec V; if (!V.Load(S.Points)) continue;
		FTractor T; T.Svc = i; T.Pt = V.P; T.Name = S.Label;
		float Yw = 0.f; T.Home = T.At = PlanPose(S.HomeX, S.HomeY, S.HomeHx, S.HomeHy, Yw); T.HomeYaw = T.Yaw = Yw;
		static const TCHAR* W[] = {TEXT("FL"), TEXT("FR"), TEXT("RL"), TEXT("RR")};
		for (int32 w = 0; w < 4; ++w) T.KWheel[w] = V.At(*(FString(TEXT("PIV_Wheel_")) + W[w]));
		for (int32 w = 0; w < 2; ++w) T.KKing[w] = V.At(*(FString(TEXT("PIV_Steer_")) + W[w]));
		T.KHinge = V.At(TEXT("PIV_DoorDriver")); T.KWheelC = V.At(TEXT("PIV_SteeringWheel")); T.KStand = V.At(TEXT("IK_Stand_DoorDriver")); T.KHandle = V.At(TEXT("IK_Handle_DoorDriver_Out"));
		T.KStep = V.At(TEXT("IK_Step_DoorDriver")); T.KGrabOut = V.At(TEXT("IK_Grab_L")); T.KGrabIn = T.KGrabOut + FVector(8.f, 4.f, -12.f); T.KSeat = V.At(TEXT("IK_Seat_SeatDriver"));
		T.KBrake = V.At(TEXT("IK_Foot_Rest")); T.KAccel = V.At(TEXT("IK_Foot_Accel")); T.KHandL = V.At(TEXT("IK_Hand_Wheel_L")); T.KHandR = V.At(TEXT("IK_Hand_Wheel_R"));
		T.KRadF = float(T.KWheel[0].Z); T.KRadR = float(T.KWheel[2].Z); T.KWB = float(T.KWheel[0].X - T.KWheel[2].X); T.KTrack = float(FMath::Abs(T.KWheel[0].Y) * 2.0); T.KFloor = S.Wet ? 82.f : float(V.At(TEXT("IK_Aisle_Row0")).Z);
		if (S.bStairs || S.bAmbu) T.Pt.Remove(TEXT("IK_Step_DoorDriver_Low"));   // (one low step, not a truck's two)
		T.KHalfL = S.HalfL; T.KHalfW = S.HalfW; T.KCentreX = 0.f; T.KTurnR = S.TurnR; T.KMaxSteer = S.MaxSteer; T.KDoorDeg = S.DoorDeg;
		T.SndStart = TEXT(""); T.SndLoop = TEXT("BLDriveLoop"); T.SndStop = TEXT(""); T.SndDoorOpen = TEXT("BLDoorOpen"); T.SndDoorClose = TEXT("BLDoorClose"); T.SndBeep = TEXT("BLReverseBeep");
		if (SpawnTractor(T))
		{
			Tractors.Add(T); SvcLivery(Tractors.Last());
			if (S.bStairs) { ADP_LOG(TEXT("GSE: %s in its box at plan (%.0f, %.0f)"), *T.Name, S.HomeX, S.HomeY); continue; }   // (solid by its own boxes: you can walk up them)
			// (solid for you on foot - its parts have no collision of their own)
			UBoxComponent* B = NewObject<UBoxComponent>(T.M.Get()); B->SetupAttachment(T.M->GetRootComponent()); B->SetRelativeLocation(FVector(0.f, 0.f, S.bFuel ? 165.f : S.bCater ? 190.f : 135.f)); B->SetBoxExtent(FVector(T.KHalfL - 4.f, T.KHalfW - 4.f, S.bFuel ? 135.f : S.bCater ? 160.f : 105.f));
			B->SetCollisionEnabled(ECollisionEnabled::QueryOnly); B->SetCollisionObjectType(ECC_WorldDynamic); B->SetCollisionResponseToAllChannels(ECR_Ignore); B->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block); B->SetCanEverAffectNavigation(false); B->RegisterComponent(); T.M->AddInstanceComponent(B);
 ADP_LOG(TEXT("GSE: %s in its box at plan (%.0f, %.0f)"), *T.Name, S.HomeX, S.HomeY);
		}
	}
}

void AGSE::SvcDress(FTractor& T)
{
	const FServiceSpec& S = SvSpec(T.Svc); ABabylonModel* M = T.M.Get(); if (!M) return;
	for (const FServiceDoor& D : S.Doors) { FTractor::FSvcDoor X; X.Piv = SvcP(T, D.Pivot); X.G = M->BindGroup({FString(D.Prefix)}, true, FTransform(X.Piv)); T.SvcDoors.Add(X); }
	for (const FServiceCab& C : S.Cab) { FTractor::FSvcDoor X; X.Piv = SvcP(T, C.Door.Pivot); X.G = M->BindGroup({FString(C.Door.Prefix)}, true, FTransform(X.Piv)); T.SvcDoors.Add(X); }
	T.GPedal[0] = M->BindGroup({TEXT("Pedal_Accel")}, true, FTransform(SvcP(T, TEXT("PIV_Pedal_Accel")))); T.GPedal[1] = M->BindGroup({TEXT("Pedal_Brake")}, true, FTransform(SvcP(T, TEXT("PIV_Pedal_Brake"))));
	for (const FServiceWiper& W : S.Wipers) T.GWiper.Add(M->BindGroup({FString(W.Prefix)}, true, FTransform(SvcP(T, W.Pivot))));
	for (const FServiceItem& I : S.Items) { FTractor::FSvcItem X; X.Piv = SvcP(T, I.Part); X.bShow = I.bAtHome; X.G = M->BindGroup({FString(I.Part)}, true, FTransform(X.Piv)); T.SvcItems.Add(X); }
	if (S.bFuel) FuelDress(T);
	if (S.bCater) CaterDress(T);
	if (S.Wet) WetDress(T);
	if (S.bStairs) StairsDress(T);
	if (S.bDeicer) DeicerDress(T);
	if (S.bAmbu) AmbuDress(T);
	// the airport's name, at run time
	FString Name; if (!ADPSite::AirportBlock()->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) return;
	auto Text = [&](const TCHAR* At, float Yaw, float Size)
	{
		if (!At || !T.Pt.Contains(At)) return;
		UTextRenderComponent* X = NewObject<UTextRenderComponent>(M);
		X->SetupAttachment(M->GetRootComponent()); X->SetText(FText::FromString(Name.ToUpper() + TEXT(" AIRPORT")));
		X->SetHorizontalAlignment(EHTA_Center); X->SetVerticalAlignment(EVRTA_TextCenter);
		X->SetWorldSize(FMath::Min(Size, Size * 15.f / float(Name.Len() + 8))); X->SetTextRenderColor(FColor(16, 30, 64));   // (a long name smaller: it stays on its panel)
		X->SetRelativeLocationAndRotation(SvcP(T, At) + FRotator(0.f, Yaw, 0.f).Vector() * 1.2f, FRotator(0.f, Yaw, 0.f));
		X->SetCollisionEnabled(ECollisionEnabled::NoCollision); X->RegisterComponent(); M->AddInstanceComponent(X);
		if ((S.bCater || S.bAmbu) && (At == S.NameL || At == S.NameR)) X->ComponentTags.Add(TEXT("SvcBody"));   // (it rides with the body)
		if (At == S.NameRear) X->ComponentTags.Add(TEXT("SvcRear"));
		if (S.bStairs && FCString::Strstr(At, TEXT("_Plat_"))) X->ComponentTags.Add(TEXT("SvcUp"));   // (they ride with the platform)
	};
	if (S.bStairs) { Text(TEXT("IK_Name_Plat_L"), -90.f, 10.f); Text(TEXT("IK_Name_Plat_R"), 90.f, 10.f); Text(TEXT("IK_Name_Front"), 0.f, 8.f); }
	Text(S.NameL, -90.f, S.NameSize); Text(S.NameR, 90.f, S.NameSize); Text(S.NameRear, 180.f, S.NameRearSize);
}

void AGSE::SvcLivery(FTractor& T)
{
	// your airline's colours (FADPAirline LiveryBody / LiveryMain, "#rrggbb"; empty: the model's own) on its livery paint
	const UADPSim* Sim = UADPSim::Get(this); const FServiceSpec& S = SvSpec(T.Svc); FString B, Mn;
	if (Sim) for (const FADPAirline& A : Sim->World.Airlines) if (A.bPlayer) { B = A.LiveryBody; Mn = A.LiveryMain; }
	const FString Want = B + TEXT("|") + Mn; ABabylonModel* M = T.M.Get();
	if (!M || Want == T.Livery || (T.Livery.IsEmpty() && Want.Len() < 2)) return;
	T.Livery = Want;
	TArray<UInstancedStaticMeshComponent*> Ps; M->Parts(Ps);
	for (UInstancedStaticMeshComponent* K : Ps)
		for (int32 i = 0; i < K->GetNumMaterials(); ++i)
		{
			const UStaticMesh* Sm = K->GetStaticMesh(); const UMaterialInterface* Own = Sm ? Sm->GetMaterial(i) : nullptr; if (!Own) continue;
			const FString N = Own->GetName(); const FString* C = S.LiveryBody && N == S.LiveryBody ? &B : S.LiveryMain && N == S.LiveryMain ? &Mn : nullptr;
			if (!C || C->IsEmpty()) continue;
			UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(K->GetMaterial(i)); if (!D) D = K->CreateDynamicMaterialInstance(i, K->GetMaterial(i));   // (the dirt's own paint, or the model's)
			const FLinearColor L(FColor::FromHex(*C)); if (D) { D->SetVectorParameterValue(TEXT("BC"), L); D->SetVectorParameterValue(TEXT("BaseColorFactor"), L); }
		}
}

void AGSE::SvcRestock(FTractor& T)
{
	const FServiceSpec& S = SvSpec(T.Svc); T.bUsed = false; T.BoxT = 0.f;
	for (int32 i = 0; i < T.SvcItems.Num(); ++i) { T.SvcItems[i].bShow = S.Items[i].bAtHome; T.SvcItems[i].Ci = INDEX_NONE; T.SvcItems[i].bMoved = true; }
	if (S.bCater) CaterRestock(T);
	if (S.bDeicer) DeicerRestock(T);
	if (S.bAmbu) AmbuRestock(T);
	ADP_LOG(TEXT("GSE: the %s %s, shut in its box"), *T.Name, S.bFuel || S.bDeicer || S.Wet == 1 ? TEXT("filled up") : S.Wet == 2 ? TEXT("emptied and refilled") : TEXT("restocked"));
}

void AGSE::SvcParts(FTractor& T, float Dt)
{
	const FServiceSpec& S = SvSpec(T.Svc); ABabylonModel* M = T.M.Get(); if (!M) return;
	// (full bags and used rolls go, new ones come, only while it stands shut in its box and nobody is in it)
	if (T.bUsed)
	{
		bool bShut = T.Job == EJob::None && !T.Player.IsValid() && FVector::Dist2D(T.At, T.Home) < 200.f; for (const FTractor::FSvcDoor& D : T.SvcDoors) if (D.Now > 0.f) bShut = false;
		if ((T.BoxT = bShut ? T.BoxT + Dt : 0.f) > 30.f) SvcRestock(T);
	}
	// the load doors you sent open or shut: one leaf after the other, the last to open the first to shut
	if (T.LoadWant != INDEX_NONE)
	{
		bool bDone = true; const int32 N = S.Doors.Num();
		for (int32 k = 1; k < N && bDone; ++k)
		{
			FTractor::FSvcDoor& D = T.SvcDoors[T.LoadWant ? k : N - k];
			if (D.Want != float(T.LoadWant)) { D.Want = float(T.LoadWant); if (T.LoadWant) Sound(*T.SndDoorOpen, TW(T, D.Piv)); }
			if (D.Now != D.Want) bDone = false;
		}
		if (bDone) T.LoadWant = INDEX_NONE;
	}
	// the pedals: pressed by you, or as it speeds up and slows
	if (!(SvcYours(T) && T.PlayerSeat == 0)) { const bool bBr = T.BrakeHold > 0.f || T.MeasSpeed < 0.05f; T.PedalB = FMath::FInterpTo(T.PedalB, T.bOn && bBr ? 0.5f : 0.f, Dt, 5.f); T.PedalA = FMath::FInterpTo(T.PedalA, T.bOn && !bBr && T.MeasSpeed > 0.1f ? 0.45f : 0.f, Dt, 5.f); }
	for (int32 p = 0; p < 2; ++p) if (T.GPedal[p] != INDEX_NONE && T.bOn) M->MoveGroup(T.GPedal[p], FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(-13.f * (p ? T.PedalB : T.PedalA))), SvcP(T, p ? TEXT("PIV_Pedal_Brake") : TEXT("PIV_Pedal_Accel"))));
	// the wipers: in rain with the drive on (a second and a half there and back, a pause in light rain); parked lying down
	if ((T.RainIn -= Dt) < 0.f) { T.RainIn = 1.f; const APuddles* P = APuddles::Get(); T.bRain = P && P->GetRain() > 0.03f; T.WipeNow = P ? P->GetRain() : 0.f; SvcLivery(T); }
	if (T.GWiper.Num() && ((T.bOn && T.bRain) || T.WipeT > 0.f))
	{
		T.WipeT += Dt; const float Cyc = T.WipeNow > 0.4f ? 1.5f : 3.5f; if (T.WipeT > Cyc) T.WipeT = T.bOn && T.bRain ? T.WipeT - Cyc : 0.f;
		const float E = T.WipeT < 1.5f ? 0.5f - 0.5f * FMath::Cos(2.f * PI * T.WipeT / 1.5f) : 0.f;
		for (int32 w = 0; w < T.GWiper.Num(); ++w)
			if (T.GWiper[w] != INDEX_NONE)
			{
				// (from its rest to wiped up, in the plane of both: along the glass; lifted a little over the glass's bulge)
				const FVector Pv = SvcP(T, S.Wipers[w].Pivot), A = (S.Wipers[w].Rest - Pv).GetSafeNormal(), B = (S.Wipers[w].Up - Pv).GetSafeNormal();
				const FVector Ax = FVector::CrossProduct(A, B).GetSafeNormal(); const float Ang = FMath::Acos(FMath::Clamp(float(FVector::DotProduct(A, B)), -1.f, 1.f));
				M->MoveGroup(T.GWiper[w], FTransform(FQuat(Ax, Ang * E), Pv + FVector(0.73f, 0.f, 0.68f) * (1.f * FMath::Sin(PI * E))));
			}
	}
	for (int32 d = 0; d < T.SvcDoors.Num(); ++d)
	{
		FTractor::FSvcDoor& D = T.SvcDoors[d]; const FServiceDoor& K = SvDoor(S, d);
		if (D.Now == D.Want) continue;
		D.Now = FMath::FInterpConstantTo(D.Now, D.Want, Dt, 0.75f);
		FTransform X(D.Piv);
		if (K.bSlide)
		{
			// (out of its opening first, then straight back along its rail)
			const float Sg = D.Piv.Y >= 0.f ? 1.f : -1.f, A = FMath::Clamp(D.Now / 0.15f, 0.f, 1.f), B = FMath::Clamp((D.Now - 0.15f) / 0.85f, 0.f, 1.f);
			X.SetLocation(D.Piv + FVector(-4.f * A - 126.f * B, 12.5f * A * Sg, 0.f));
		}
		else X.SetRotation(FQuat(SvUp, FMath::DegreesToRadians(K.Deg * FMath::SmoothStep(0.f, 1.f, D.Now))));
		if (D.G != INDEX_NONE) M->MoveGroup(D.G, X);
		if (S.NameRear && FCString::Strcmp(K.Prefix, TEXT("RearDoorL")) == 0)
			for (UActorComponent* C : M->GetComponents())
				if (C && C->ComponentHasTag(TEXT("SvcRear")))
				{ const FTransform Rel = FTransform(FRotator(0.f, 180.f, 0.f), SvcP(T, S.NameRear) - FVector(1.2f, 0.f, 0.f)) * FTransform(D.Piv).Inverse() * X; Cast<USceneComponent>(C)->SetRelativeLocationAndRotation(Rel.GetLocation(), Rel.GetRotation()); }
		if (D.Now == D.Want && D.Want == 0.f) Sound(*T.SndDoorClose, TW(T, D.Piv));
	}
	if (S.bFuel) FuelParts(T, Dt);
	if (S.bCater) CaterParts(T, Dt);
	if (S.Wet) WetParts(T, Dt);
	if (S.bStairs) StairsParts(T, Dt);
	if (S.bDeicer) DeicerParts(T, Dt);
	if (S.bAmbu) AmbuParts(T, Dt);
	for (FTractor::FSvcItem& I : T.SvcItems)
	{
		if (I.G == INDEX_NONE || (I.Ci == INDEX_NONE && !I.bMoved)) continue;
		FTransform X(FQuat::Identity, I.Piv, I.bShow ? FVector::OneVector : FVector(0.001f));
		if (I.Ci != INDEX_NONE)
		{
			const USkeletalMeshComponent* B = Body(I.Ci); const ATugs* G = Tg();
			const bool bSeen = B && G && G->Crew[I.Ci].A.IsValid() && !G->Crew[I.Ci].A->IsHidden();
			if (bSeen) X = FTransform(FRotator(0.f, float(G->Crew[I.Ci].A->GetActorRotation().Yaw) + 90.f - T.Yaw, 0.f), FTransform(FRotator(0.f, T.Yaw, 0.f), T.At).InverseTransformPosition(B->GetBoneLocation(TEXT("hand_r")) - SvUp * 6.f), FVector::OneVector);
			else X.SetScale3D(FVector(0.001f));
		}
		I.bMoved = false; M->MoveGroup(I.G, X);
	}
}

bool AGSE::SvcPersonAhead(const FTractor& T, const FVector& Move) const
{
	const ATugs* G = Tg(); if (!G) return false;
	const FVector Sd(-Move.Y, Move.X, 0.f);
	for (const float y : {-85.f, 0.f, 85.f}) for (const float r : {70.f, 180.f}) if (G->PersonAhead(T.At + Move * T.KHalfL + Sd * y, Move, r)) return true;
	return false;
}

// ------------------------------------------------------------------ the jobs

bool AGSE::SvcFor(const TCHAR* Task, float StandX) const
{
	const UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); float S = 0.f, N = 0.f, Sn = 36.f;
	if (bTest || !Sp || !StandGeo(StandX, S, N, Sn) || Sn > 40.f) return false;   // (the A320 stands)
	for (const FTractor& T : Tractors)
		if (T.Svc != INDEX_NONE && SvIs(T.Svc, Task) && Sp->HiresRole(SvSpec(T.Svc).Role))
		{
			if (SvSpec(T.Svc).bDeicer) { for (const FAirfieldData::FStand& St : AAirfield::Shared().Stands) if (FMath::Abs(St.X - StandX) < 1.f) return !St.bCargo; return false; }
			if (SvSpec(T.Svc).bAmbu && !T.Am) continue;
			if (SvSpec(T.Svc).bStairs)
			{
				// (only where no jetway is: never at a gate that has one)
				for (const FAirfieldData::FStand& St : AAirfield::Shared().Stands) if (FMath::Abs(St.X - StandX) < 1.f) return !St.bJetway && !St.bCargo;
				return false;
			}
			if (!SvSpec(T.Svc).bJetway) return true;
			const AJetwayRigs* J = AJetwayRigs::Get(GetWorld()); TArray<FVector> St; float Hw = 0.f; FVector Mid, Out;
			const int32 Ji = J ? J->Nearest(Plan(StandX + 5.f, N + 2.f)) : INDEX_NONE;
			return Ji != INDEX_NONE && J->ServiceSteps(Ji, St, Hw) && J->ServiceDoorFrame(Ji, Mid, Out);   // (no way up: the agent)
		}
	return false;
}

bool AGSE::SvcAsk(const TCHAR* Task, float StandX, const FString& Reg, const FString& Flight, const FDateTime& When, float Litres)
{
	for (TPair<int32, FSvcJob>& P : SvcJobs) if (P.Value.Reg == Reg && P.Value.Phase < 3 && SvIs(P.Value.Svc, Task)) { P.Value.When = When; P.Value.Flight = Flight; return P.Value.Phase > 0; }
	for (int32 i = 0; i < ADPService::All().Num(); ++i)
		if (SvIs(i, Task))
		{
			FSvcJob J; J.Svc = i; J.Litres = FMath::Clamp(Litres, 500.f, 19500.f); J.StandX = StandX; J.Reg = Reg; J.Flight = Flight; J.When = When; if (SvSpec(i).bCater) J.Rows = 11;
			if (SvSpec(i).Wet) J.Litres = SvSpec(i).Wet == 2 ? 360.f : 240.f;   // (seconds: draining and rinsing; filling)
			float S = 0.f, N = 0.f, Sn = 36.f; const AJetwayRigs* R = AJetwayRigs::Get(GetWorld());
			if (R && StandGeo(StandX, S, N, Sn)) J.Jet = R->Nearest(Plan(StandX + 5.f, N + 2.f));
			SvcJobs.Add(SvcNext++, J);
			if (SvSpec(i).bStairs)
			{
				// (more than one vehicle: the rear left door too, if one is free for it - the flight waits only for the front one)
				int32 Nv = 0; for (const FServiceSpec& Q : ADPService::All()) if (Q.bStairs) ++Nv;
				if (Nv > 1) { J.CtDoor = 1; SvcJobs.Add(SvcNext++, J); }
			}
			break;
		}
	return false;
}

void AGSE::SvcGo(const TCHAR* Task, float StandX)
{
	for (TPair<int32, FSvcJob>& P : SvcJobs) if (FMath::Abs(P.Value.StandX - StandX) < 1.f && SvIs(P.Value.Svc, Task)) P.Value.bGo = true;
}

bool AGSE::SvcBusy(const TCHAR* Task, float StandX) const
{
	for (const TPair<int32, FSvcJob>& P : SvcJobs)
	{
		const FSvcJob& J = P.Value;
		// (the catering's two tasks are one job: the front door's until that is done, the rear one's after it)
		if (SvSpec(J.Svc).bCater) { const int32 Bit = FCString::Strcmp(Task, TEXT("cat1")) == 0 ? 1 : FCString::Strcmp(Task, TEXT("cat2")) == 0 ? 2 : 0; if (Bit && FMath::Abs(J.StandX - StandX) < 1.f && J.Phase < 3 && J.T <= SvPatience && !(J.CtDone & Bit)) return true; continue; }
		if (FMath::Abs(J.StandX - StandX) > 1.f || !SvIs(J.Svc, Task) || J.Phase >= 3 || J.T > SvPatience) continue;
		for (const FSvcMan& M : J.Men) if (!M.bGone && !M.bDown) return true;
	}
	return false;
}

void AGSE::SvcGone(float StandX) { for (auto It = SvcJobs.CreateIterator(); It; ++It) if (It.Value().Phase == 0 && FMath::Abs(It.Value().StandX - StandX) < 1.f) It.RemoveCurrent(); }

bool AGSE::SvcWaits(const FString& Flight, float StandX, FString& Why) const
{
	for (const TPair<int32, FSvcJob>& P : SvcJobs)
	{
		const FSvcJob& J = P.Value; const FServiceSpec& S = SvSpec(J.Svc);
		if ((J.Flight != Flight && FMath::Abs(J.StandX - StandX) > 1.f) || J.T > (S.bDeicer ? 3.f : 1.f) * SvPatience) continue;
		if (S.bAmbu)
		{
			if (AmbuBusy(J.StandX, J.CtDoor == 1, Why)) return true;
			const FTractor* Av = Tractors.IsValidIndex(J.Ti) ? &Tractors[J.Ti] : nullptr;
			if (Av && (J.Phase == 2 || (J.Phase == 3 && Av->Job == EJob::AtStand)) && FVector::Dist2D(Av->At, J.Place) < 6000.f) { Why = TEXT("the ambulift is leaving the stand"); return true; }
			continue;
		}
		if (S.bDeicer)
		{
			const FTractor* Dv = Tractors.IsValidIndex(J.Ti) ? &Tractors[J.Ti] : nullptr; if (J.Phase > 3 || (J.Phase == 3 && !(Dv && Dv->Job == EJob::AtStand))) continue;
			Why = J.Phase == 0 ? FString::Printf(TEXT("de-icing waits: %s"), J.Why.IsEmpty() ? TEXT("its people are being called") : *J.Why) : !J.Why.IsEmpty() ? FString::Printf(TEXT("the de-icing truck waits: %s"), *J.Why) : J.Phase == 1 ? FString(TEXT("the de-icing truck is on its way")) : J.Phase == 3 ? FString(TEXT("the de-icing truck is driving off")) : Dv && Dv->Di ? FString::Printf(TEXT("it is being de-iced (stop %d of %d)"), Dv->Di->Stop + 1, Dv->Di->Stops.Num()) : FString(TEXT("it is being de-iced"));
			return true;
		}
		const FTractor* Tv = Tractors.IsValidIndex(J.Ti) ? &Tractors[J.Ti] : nullptr; bool bDown = J.Phase == 2; for (const FSvcMan& M : J.Men) if (!M.bGone && !M.bDown) bDown = false;
		if (Tv && J.Phase == 2 && !SvcHere(P.Key) && !(S.bCater && Tv->Job == EJob::Out))
		{
			// (yours, or left somewhere: its crew wait for it or fetch it; the flight only waits for the cabin)
			if (bDown && FVector::Dist2D(Tv->At, J.Place) > 6000.f) continue;
			Why = SvcYours(*Tv) ? FString::Printf(TEXT("you have the %s - its crew wait for it"), S.Label) : FString::Printf(TEXT("the %s's crew fetch it back"), S.Label); return true;
		}
		if (S.bStairs) { if (J.Phase < 3 ? J.Phase > 0 || J.CtDoor == 0 : Tv && Tv->Job == EJob::AtStand) { Why = J.Phase == 0 ? FString::Printf(TEXT("the passenger stairs wait: %s"), J.Why.IsEmpty() ? TEXT("their people are being called") : *J.Why) : FString(J.Phase < 3 && J.Stage < 3 ? TEXT("the passenger stairs are coming") : TEXT("the passenger stairs are being taken away")); return true; } continue; }
		if (J.Phase == 0) { Why = FString::Printf(TEXT("the %s waits: %s"), S.Label, J.Why.IsEmpty() ? TEXT("its people are being called") : *J.Why); return true; }
		if (S.bFuel && J.Phase < 3) { Why = J.Phase < 2 ? FString(TEXT("the refueller is on its way")) : J.Stage < 4 ? FString(TEXT("the refueller is being connected")) : J.Stage > 4 ? FString(TEXT("the refueller is being disconnected")) : FString::Printf(TEXT("it is being fuelled (%.0f of %.0f l)"), J.Pumped, J.Litres); return true; }
		if (S.bCater && J.Phase < 3) { Why = J.Phase < 2 ? FString(TEXT("the catering truck is on its way")) : !J.Why.IsEmpty() ? FString::Printf(TEXT("the catering truck waits: %s"), *J.Why) : FString::Printf(TEXT("the catering truck is at the %s right door (%d of %d trolleys in)"), J.CtDoor ? TEXT("rear") : TEXT("front"), J.RowsDone, J.Rows); return true; }
		if (S.Wet && J.Phase < 3) { Why = !J.Why.IsEmpty() ? FString::Printf(TEXT("the %s waits: %s"), S.Label, *J.Why) : J.Phase < 2 ? FString::Printf(TEXT("the %s is on its way"), S.Label) : S.Wet == 2 ? FString::Printf(TEXT("its toilets are being emptied (%.0f %%)"), 100.f * J.Pumped / J.Litres) : FString::Printf(TEXT("it is being filled with water (%.0f of 200 l)"), 200.f * J.Pumped / J.Litres); return true; }
		if (J.Phase < 3) { Why = FString::Printf(TEXT("the %s is still at work (%d of %d rows)"), S.Label, J.RowsDone, J.Rows); return true; }
		if (Tractors.IsValidIndex(J.Ti) && Tractors[J.Ti].Job == EJob::AtStand) { Why = FString::Printf(TEXT("the %s is leaving the stand"), S.Label); return true; }
	}
	return false;
}

bool AGSE::SvcAll(int32 Job, TFunctionRef<bool(const FSvcMan&)> Is) { FSvcJob* J = SvcJ(Job); if (!J) return true; for (const FSvcMan& M : J->Men) if (!M.bGone && !Is(M)) return false; return true; }

bool AGSE::SvcLost(int32 Ci, bool& bDrop)
{
	ATugs* G = Tg(); if (!G) return false;
	for (TPair<int32, FSvcJob>& P : SvcJobs)
		for (int32 m = 0; m < P.Value.Men.Num(); ++m)
		{
			FSvcMan& M = P.Value.Men[m]; if (M.Ci != Ci || M.bGone) continue;
			const int32 Job = P.Key;
			if (Tractors.IsValidIndex(P.Value.Ti)) for (FTractor::FSvcItem& I : Tractors[P.Value.Ti].SvcItems) if (I.Ci == Ci) { I.Ci = INDEX_NONE; I.bMoved = true; }
			ATugs::FCrew& C = G->Crew[Ci];
			if (SvSpec(P.Value.Svc).bCater) { if (!CaterLost(Job, m)) bDrop = true; return true; }
			if (SvSpec(P.Value.Svc).bStairs) { StairsLost(Job, m); return true; }
			if (SvSpec(P.Value.Svc).bDeicer) { DeicerLost(Job, m); return true; }
			if (SvSpec(P.Value.Svc).bAmbu) { AmbuLost(Job, m); return true; }
			if (SvSpec(P.Value.Svc).Wet)
			{
				WetLost(Job); C.bUp = false; C.bRelWet = true;
				if (P.Value.Phase == 2) { C.Q.Reset(); C.Hold = nullptr; Do(Ci, [this, Job] { Later([this, Job] { WetWork(Job); }); }); }
				return true;
			}
			if (SvSpec(P.Value.Svc).bFuel)
			{
				FuelLost(Job); C.bUp = false; C.bRelFuel = true;
				if (P.Value.Phase == 2) { C.Q.Reset(); Do(Ci, [this, Job] { Later([this, Job] { SvcFuelWork(Job); }); }); }   // (from the last stage finished)
				return true;
			}
			if (C.bUp)
			{
				C.bUp = false; M.bIn = false;
				if (TPair<int8, int32>* U = SvcStairUse.Find(P.Value.Jet)) U->Value = FMath::Max(0, U->Value - 1);
				if (m == 0) { C.Q.Reset(); M.bDown = M.bStowed = true; Do(Ci, [this, Job] { Later([this, Job] { SvcLeave(Job); }); }); }   // (whoever takes over drives the others home)
			}
			if (m == 0) { C.bRelClean = true; return true; }        // (the vehicle stands until somebody takes over)
			M.bGone = true; bDrop = true; return true;              // (the others do their rows)
		}
	return false;
}

void AGSE::SvcTick(float Dt)
{
	ATugs* G = Tg(); UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); if (!G || !Sp || !SvcJobs.Num()) return;
	// (the cabin, row by row, by everyone who is in it)
	for (TPair<int32, FSvcJob>& P : SvcJobs)
	{
		FSvcJob& J = P.Value; const float Was = J.T; if (J.Phase > 0 && !(SvSpec(J.Svc).bAmbu && J.Phase == 1 && CaterAtFront(J.StandX))) J.T += Dt;   // (the ambulift waiting for the caterers at the same door: not its time)
	
		if (Was <= SvPatience && J.T > SvPatience) ADP_WARN(TEXT("GSE: the %s at stand x %.0f has been at %s for 45 min - the flight no longer waits for it"), SvSpec(J.Svc).Label, J.StandX, *J.Reg);
		FuelTick(J, Dt); WetTick(J, Dt); if (SvSpec(J.Svc).bStairs) StairsTick(J, Dt);
		if (SvSpec(J.Svc).bDeicer) DeicerTick(P.Key, Dt);
		if (SvSpec(J.Svc).bCater || SvSpec(J.Svc).Wet || SvSpec(J.Svc).bStairs || SvSpec(J.Svc).bDeicer || SvSpec(J.Svc).bAmbu) continue;
		int32 In = 0; for (const FSvcMan& M : J.Men) if (M.bIn && !M.bGone) ++In;
		if (!In || J.RowsDone >= J.Rows) continue;
		for (J.RowT += Dt * In; J.RowT >= SvRowSeconds && J.RowsDone < J.Rows; J.RowT -= SvRowSeconds)
			if (++J.RowsDone % 10 == 0 || J.RowsDone == J.Rows) ADP_LOG(TEXT("GSE: %s cleaned to row %d of %d, %d people"), *J.Reg, J.RowsDone, J.Rows, In);
	}
	if ((SvcClock += Dt) < 2.f) return;
	SvcClock = 0.f;
	TArray<int32> Start;
	for (TPair<int32, FSvcJob>& P : SvcJobs)
	{
		FSvcJob& J = P.Value; if (J.Phase != 0) continue;
		const FServiceSpec& S = SvSpec(J.Svc);
		int32 Ti = INDEX_NONE; for (int32 i = 0; i < Tractors.Num() && Ti == INDEX_NONE; ++i) if (Tractors[i].Svc == J.Svc && Tractors[i].Job == EJob::None && Tractors[i].M.IsValid()) Ti = i;
		if (S.bStairs)
		{
			// (the roofed one first in rain and snow, the open one first when it is dry; a rear door only when no front door waits)
			const APuddles* Pu = APuddles::Get(); const bool bWeather = Pu && (Pu->GetRain() > 0.03f || Pu->GetSnowOnPavingCm() > 0.3f); int32 Best = -1; Ti = INDEX_NONE;
			bool bFront = false; if (J.CtDoor) for (const TPair<int32, FSvcJob>& O : SvcJobs) if (O.Value.Phase == 0 && O.Value.CtDoor == 0 && SvSpec(O.Value.Svc).bStairs) bFront = true;
			for (int32 i = 0; i < Tractors.Num() && !bFront; ++i)
			{
				const FTractor& V = Tractors[i]; if (V.Svc == INDEX_NONE || !SvSpec(V.Svc).bStairs || V.Job != EJob::None || !V.M.IsValid()) continue;
				const int32 Sc = (SvcYours(V) || !StairsStowed(V) ? 0 : 2) + (SvSpec(V.Svc).bRoof == bWeather ? 1 : 0); if (Sc > Best) { Best = Sc; Ti = i; }
			}
		}
		if (Ti == INDEX_NONE) { J.Why = FString::Printf(TEXT("no %s free"), S.Label); continue; }
		const FTractor& Tv = Tractors[Ti]; J.Svc = Tv.Svc;   // (any of the stairs)
		if (S.bStairs && !StairsStowed(Tv)) { J.Why = TEXT("the stairs are up"); continue; }
		if (S.bAmbu && (!AmbuStowed(Tv) || Tv.Player.IsValid())) { J.Why = TEXT("you have the ambulift"); continue; }
		if (SvcYours(Tv) || (S.bDeicer && (Tv.Player.IsValid() || !DeicerStowed(Tv)))) { J.Why = FString::Printf(TEXT("you have the %s"), S.Label); continue; }
		const bool bHome = FVector::Dist2D(Tv.At, Tv.Home) < 200.f && FMath::Abs(FMath::FindDeltaAngleDegrees(Tv.Yaw, Tv.HomeYaw)) < 20.f;
		if (bHome && Tv.bUsed) { J.Why = FString::Printf(TEXT("the %s is being %s"), S.Label, S.bFuel || S.bDeicer || S.Wet == 1 ? TEXT("filled up") : S.Wet == 2 ? TEXT("emptied") : TEXT("emptied and restocked")); continue; }
		{ bool bOpen = false; for (const FTractor::FSvcDoor& D : Tv.SvcDoors) if (D.Now > 0.01f) bOpen = true; if (bOpen && Tv.Player.IsValid()) { J.Why = FString::Printf(TEXT("you are getting into the %s"), S.Label); continue; } }
		if (!bHome) { TArray<FVector> W; if (!Apron(W, Tv.At, Plan(S.HomeX + S.HomeHx * 34.f, S.HomeY + S.HomeHy * 34.f), &Tv)) { J.Why = FString::Printf(TEXT("the %s stands where it cannot be driven from - bring it back to the apron"), S.Label); continue; } }
		const FString Key = FString::Printf(TEXT("%s:%s%s"), S.Task, *J.Reg, (S.bStairs || S.bAmbu) && J.CtDoor ? TEXT(":2") : TEXT("")), What = FString::Printf(TEXT("%s %s"), J.Flight.IsEmpty() ? *J.Reg : *J.Flight, UStaffPeople::TaskName(S.Team));
		TArray<UStaffPeople::FTaken> Team;
		if (!Sp->AskTeam(Key, S.Team, J.When - FTimespan::FromMinutes(20.0), Tractors[Ti].At, What, Team)) { J.Why = Sp->WaitsFor(Key); continue; }
		// (more of them, as many as no other waiting job needs)
		while (Team.Num() && Team.Num() < S.MaxPeople) { UStaffPeople::FTaken X; if (!Sp->AskExtra(Team.Last().Duty, Tractors[Ti].At, What, X)) break; Team.Add(X); }
		TArray<FSvcMan> Men;
		for (const UStaffPeople::FTaken& X : Team)
		{
			FSvcMan M; M.Ci = G->Enlist(X.A.Get(), X.Id, UStaffPeople::DutyName(X.Duty)); if (M.Ci == INDEX_NONE) continue;
			G->Crew[M.Ci].bGse = true; M.Seat = Men.Num(); Men.Add(M);
		}
		if (Men.Num() < FMath::Min(2, S.MaxPeople)) { for (const FSvcMan& M : Men) G->Discharge(M.Ci); J.Why = TEXT("no place for more people"); continue; }
		J.Men = Men; J.Ti = Ti; J.Phase = 1; J.Why.Reset(); Tractors[Ti].Job = EJob::Boarding; Start.Add(P.Key);
	}
	for (const int32 Job : Start) SvcStart(Job);
	for (auto It = SvcJobs.CreateIterator(); It; ++It) if (It.Value().Phase == 9) It.RemoveCurrent();
}

// ------------------------------------------------------------------ in and out

TFunction<void(float, float, FDriverPose&)> AGSE::SvcSeatFn(int32 Ti, int32 Seat) const
{
	const FServiceSpec& S = SvSpec(Tractors[Ti].Svc); const int32 c = Seat - 1 - S.Seats.Num();
	if (!S.Seats.IsValidIndex(Seat - 1) && !S.Cab.IsValidIndex(c)) return SeatFn(Ti);
	if (S.bDeicer && S.Cab.IsValidIndex(c)) return DeicerSeatFn(Ti);
	const FVector H = SvcP(Tractors[Ti], S.Cab.IsValidIndex(c) ? S.Cab[c].Seat : S.Seats[Seat - 1]) + SvUp * 10.f;
	return [this, Ti, H](float, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti];
		P = FDriverPose(); P.Facing = FRotator(0.f, T.Yaw, 0.f).Vector(); P.Pelvis = TW(T, H); P.Lean = -5.f;
		P.Foot[0] = TW(T, FVector(H.X + 50.f, H.Y - 11.f, T.KFloor)); P.Foot[1] = TW(T, FVector(H.X + 50.f, H.Y + 11.f, T.KFloor));
		P.bHand[0] = P.bHand[1] = true; P.Hand[0] = TW(T, H + FVector(30.f, -12.f, 4.f)); P.Hand[1] = TW(T, H + FVector(30.f, 12.f, 4.f)); P.Grip[0] = P.Grip[1] = 0.25f;
	};
}

TFunction<void(float, float, FDriverPose&)> AGSE::SvcClimbFn(int32 Ti, int32 Seat, bool bIn) const
{
	// through the crew's door: the step, up, along in front of the row to the seat, turned and down onto it
	const FTractor& T0 = Tractors[Ti]; const FServiceSpec& S = SvSpec(T0.Svc);
	if (IsTall(T0) || S.bStairs || S.bAmbu) return SvcClimbAny(Ti, Seat, bIn);   // (a truck's cab: the driver's two steps, mirrored)
	const FVector Stand = SvcP(T0, S.Doors[0].Stand), Step = SvcP(T0, *(FString(TEXT("IK_Step_")) + S.Doors[0].Prefix)), H = SvcP(T0, S.Seats[Seat - 1]);
	const float Sg = Stand.Y >= 0.f ? -1.f : 1.f;   // (in: to the left from a right door)
	const int32 Hs = Sg < 0.f ? 1 : 0;              // (the hand on the opening's front edge)
	return [this, Ti, Seat, bIn, Stand, Step, H, Sg, Hs](float K0, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti]; const float K = bIn ? K0 : 1.f - K0;
		const FVector In = FRotator(0.f, T.Yaw + 90.f * Sg, 0.f).Vector(), Fwd = FRotator(0.f, T.Yaw, 0.f).Vector();
		const FVector Edge = TW(T, FVector(Step.X + 22.f, Step.Y, 150.f));
		auto Stoop = [&](const FVector& A, float Lean) { FDriverPose Q = ADPPose::Standing(TW(T, A), In, PH); Q.Pelvis.Z -= 16.f; Q.Lean = Lean; return Q; };
		auto Key = [&](int32 i)
		{
			FDriverPose Q;
			switch (i)
			{
			case 0: Q = ADPPose::Standing(TW(T, Stand), In, PH); break;
			case 1:
				Q = ADPPose::Standing(TW(T, FVector(Step.X, Step.Y - Sg * 30.f, 0.f)), In, PH); Q.Pelvis = TW(T, FVector(Step.X, Step.Y - Sg * 22.f, PH + 12.f)); Q.Lean = 14.f;
				Q.Foot[1 - Hs] = TW(T, Step + FVector(-8.f, 0.f, 0.f)); Q.Foot[Hs] = TW(T, FVector(Step.X + 10.f, Step.Y - Sg * 45.f, 0.f));
				Q.bHand[Hs] = true; Q.Hand[Hs] = Edge; Q.Grip[Hs] = 0.8f; break;
			case 2: Q = Stoop(FVector(Step.X + 4.f, Step.Y + Sg * 14.f, T.KFloor), 24.f); Q.bHand[Hs] = true; Q.Hand[Hs] = Edge; Q.Grip[Hs] = 0.6f; break;
			case 3: Q = Stoop(FVector(H.X + 59.f, (H.Y + Step.Y) * 0.5f, T.KFloor), 20.f); break;
			case 4: Q = Stoop(FVector(H.X + 52.f, H.Y, T.KFloor), 22.f); Q.Facing = (In * 0.15f + Fwd).GetSafeNormal2D(); Q.Pelvis = TW(T, FVector(H.X + 34.f, H.Y, T.KFloor + PH - 26.f)); break;
			default: SvcSeatFn(Ti, Seat)(1.f, PH, Q); break;
			}
			return Q;
		};
		const float X = K * 5.f; const int32 i = FMath::Clamp(FMath::FloorToInt(X), 0, 4);
		P = ADPPose::Lerp(Key(i), Key(i + 1), FMath::SmoothStep(0.f, 1.f, X - i));
	};
}

void AGSE::SvcDoor(int32 Ci, int32 Ti, int32 Door, bool bOpen)
{
	// the hand to the handle, the door moved (shutting: where the handle is now)
	const FServiceDoor D = SvSpec(Tractors[Ti].Svc).Doors[Door]; const FTractor& T0 = Tractors[Ti];
	const FVector Stand = SvcP(T0, D.Stand), H0 = SvcP(T0, D.Handle), Piv = SvcP(T0, D.Pivot);
	const FVector HO = D.bSlide ? H0 + FVector(-130.f, Piv.Y >= 0.f ? 12.5f : -12.5f, 0.f) : Piv + FRotator(0.f, D.Deg, 0.f).RotateVector(H0 - Piv);
	// (a slid-back door's handle is 1.3 m behind: beside it; a swung leaf's free edge: from between the two leaves)
	const FVector At = bOpen ? Stand : D.bSlide ? Stand + FVector(-105.f, 0.f, 0.f) : FVector(HO.X - 22.f, HO.Y + (Piv.Y > 0.f ? -45.f : 45.f), 0.f), Hand = bOpen ? H0 : HO;
	WalkTo(Ci, [this, Ti, At] { return TW(Tractors[Ti], At); }, 15.f);
	PoseAct(Ci, [this, Ti, At, Hand](float K, float PH, FDriverPose& P) { const FTractor& T = Tractors[Ti]; P = ADPPose::Reach(TW(T, At), SvFace(TW(T, At), TW(T, Hand)), 1, TW(T, Hand), 0.8f, PH, FMath::Sin(PI * K)); }, 1.1f);
	Do(Ci, [this, Ti, Door, bOpen] { FTractor& T = Tractors[Ti]; T.SvcDoors[Door].Want = bOpen ? 1.f : 0.f; if (bOpen) Sound(*T.SndDoorOpen, TW(T, T.SvcDoors[Door].Piv)); });
	WaitFor(Ci, [this, Ti, Door](float E) { const FTractor::FSvcDoor& X = Tractors[Ti].SvcDoors[Door]; return E > 5.f || X.Now == X.Want; });
}

void AGSE::SvcBoard(int32 Job, int32 m)
{
	FSvcMan* M = SvcM(Job, m); if (!M) return;
	const int32 Ci = M->Ci, Ti = SvcJ(Job)->Ti;
	const FVector Stand = SvcP(Tractors[Ti], SvSpec(Tractors[Ti].Svc).Doors[0].Stand);
	// one after the other, the farthest seat first; the last one shuts the door from his seat
	WaitFor(Ci, [this, Job, m](float) { return SvcAll(Job, [m](const FSvcMan& X) { return X.Seat == 0 || X.Seat >= m || X.bSeated; }); });
	if (m == 1) SvcDoor(Ci, Ti, 0, true);
	WalkTo(Ci, [this, Ti, Stand] { return TW(Tractors[Ti], Stand); }, 15.f);
	Do(Ci, [this, Ti] { Tractors[Ti].SvcDoors[0].Want = 1.f; });
	WaitFor(Ci, [this, Ti](float E) { return E > 5.f || Tractors[Ti].SvcDoors[0].Now > 0.99f; });
	PoseAct(Ci, SvcClimbFn(Ti, m, true), 5.f);
	Do(Ci, [this, Job, m, Ci, Ti]
	{
		if (ATugs* G = Tg()) G->Crew[Ci].Hold = SvcSeatFn(Ti, m);
		if (FSvcMan* X = SvcM(Job, m)) { X->bSeated = true; X->bOut = false; }
		if (SvcAll(Job, [](const FSvcMan& Y) { return Y.Seat == 0 || Y.bSeated; })) Tractors[Ti].SvcDoors[0].Want = 0.f;
	});
}

void AGSE::SvcAlight(int32 Job, int32 m)
{
	FSvcMan* M = SvcM(Job, m); if (!M) return;
	const int32 Ci = M->Ci, Ti = SvcJ(Job)->Ti;
	const FVector Off = SvcP(Tractors[Ti], SvSpec(Tractors[Ti].Svc).Doors[0].Stand);
	// the nearest to the door first
	WaitFor(Ci, [this, Job, m](float) { return SvcAll(Job, [m](const FSvcMan& X) { return X.Seat <= m || !X.bSeated; }); });
	Do(Ci, [this, Ti] { FTractor& T = Tractors[Ti]; if (T.SvcDoors[0].Want < 0.5f) Sound(*T.SndDoorOpen, TW(T, T.SvcDoors[0].Piv)); T.SvcDoors[0].Want = 1.f; });
	WaitFor(Ci, [this, Ti](float E) { return E > 4.f || Tractors[Ti].SvcDoors[0].Now > 0.99f; });
	Do(Ci, [this, Ci] { if (ATugs* G = Tg()) G->Crew[Ci].Hold = nullptr; });
	PoseAct(Ci, SvcClimbFn(Ti, m, false), 4.4f);
	WalkTo(Ci, [this, Ti, Off, m] { return TW(Tractors[Ti], Off + FVector(70.f * m, Off.Y >= 0.f ? 70.f : -70.f, 0.f)); }, 30.f);
	Do(Ci, [this, Job, m] { if (FSvcMan* X = SvcM(Job, m)) { X->bSeated = false; X->bOut = true; } });
	// (the last one out shuts it)
	if (m == 1) SvcDoor(Ci, Ti, 0, false);
}

// ------------------------------------------------------------------ there

void AGSE::SvcStart(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J) return;
	const int32 Ti = J->Ti; FTractor& T = Tractors[Ti];
	bWorked = true;
	T.Driver = J->Men[0].Ci; T.JobT = 0.f; T.StandX = J->StandX; T.Key = J->Flight.IsEmpty() ? J->Reg : J->Flight;
	ADP_LOG(TEXT("GSE: the %s to %s at stand x %.0f with %d people"), *T.Name, *T.Key, T.StandX, J->Men.Num());
	for (int32 m = 0; m < J->Men.Num(); ++m)
	{
		const int32 Ci = J->Men[m].Ci;
		HiredWay(Ci, T.At, 0.f);
		if (m > 0) { SvcBoard(Job, m); continue; }
		ClimbIn(Ci, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) X->bSeated = true; });
		WaitFor(Ci, [this, Job, Ti](float) { return SvcAll(Job, [](const FSvcMan& X) { return X.bSeated; }) && Tractors[Ti].SvcDoors[0].Now < 0.01f; });
		Do(Ci, [this, Job, Ti]
		{
			FTractor& V = Tractors[Ti]; Motor(V, true);
			if (!V.bUsed) { SvcApron(Job); return; }
			// (left somewhere else with full bags in it: to its box first, exchanged there, shut)
			V.Job = EJob::Out;
			SvcDriveHome(Job, [this, Job, Ti]
			{
				const double Go = GetWorld()->GetTimeSeconds() + 30.0;
				Tractors[Ti].Until = [this, Go] { return GetWorld()->GetTimeSeconds() > Go; };
				Tractors[Ti].AfterUntil = [this, Job, Ti] { SvcRestock(Tractors[Ti]); SvcApron(Job); };
			});
		});
	}
}

void AGSE::SvcApron(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J) return;
	const int32 Ti = J->Ti; FTractor& T = Tractors[Ti];
	const FServiceSpec& Sv = SvSpec(J->Svc);
	if (Sv.bDeicer) { DeicerApron(Job); return; }
	// (one at a time on the aircraft's right, between its wing and the next one's)
	if (Sv.Wet && T.Job != EJob::Out && !WetMay(Job)) return;   // (its turn under the tail; asked again)
	if (Sv.Lat < 0.f && !Sv.Wet && T.Job != EJob::Out && SvcSideBusy(Ti, J->StandX)) { RetryIn(Ti, 6.f, [this, Job] { SvcApron(Job); }); return; }
	// (the front right door: the caterers' or the ambulift's, whoever is first; the other one keeps out of the lane meanwhile)
	if (Sv.bCater && T.Job != EJob::Out && AmbuAtFront(J->StandX, true)) { RetryIn(Ti, 8.f, [this, Job] { SvcApron(Job); }); return; }
	if (Sv.bAmbu && T.Job != EJob::Out && (CaterAtFront(J->StandX) || CaterRear(J->StandX))) { RetryIn(Ti, 8.f, [this, Job] { SvcApron(Job); }); return; }
	T.Job = EJob::Out;
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(J->StandX, S, N, Sp);
	TArray<FVector> W;
	if (FVector::Dist2D(T.At, T.Home) < 200.f && FMath::Abs(FMath::FindDeltaAngleDegrees(T.Yaw, T.HomeYaw)) < 20.f) Leg(W, Sv.HomeX, Sv.HomeY, Sv.HomeHx, Sv.HomeHy, Sv.HomeX + Sv.HomeHx * 9.f, Sv.HomeY + Sv.HomeHy * 9.f, Sv.HomeHx, Sv.HomeHy, T.KTurnR);
	const float X = S + (Sv.InR > 0.f ? Sv.InLat : Sv.Wet ? Sv.Lat + 2.f * T.KTurnR / 100.f : Sv.Lat), Xp = S + Sv.Lat, Y1 = N + Sv.Far + 6.f, Y2 = N + Sv.Aft;
	const FVector From = W.Num() ? W.Last() : T.At;
	if (!Apron(W, From, Plan(X, Y1), &T)) { RetryIn(Ti, 8.f, [this, Job] { SvcApron(Job); }); return; }
	Go(T, MoveTemp(W), false, [this, Job, Ti, S, N, X, Xp, Y1, Y2]
	{
		const FVector Nose = Plan(S, N);
		if (SvSpec(Tractors[Ti].Svc).Wet)
		{
			// (nothing moving where it turns round; not longer than a minute for that)
			Tractors[Ti].Until = [this, Nose, Ti, Mid = Plan(S + 6.f, N + 42.f), T0 = GetWorld()->GetTimeSeconds()]
			{
				if (PlaneNear(Nose, 6000.f)) return false;
				if (GetWorld()->GetTimeSeconds() - T0 < 60.0) for (int32 o = 0; o < Tractors.Num(); ++o) if (o != Ti && Tractors[o].M.IsValid() && Tractors[o].Speed > 1.f && FVector::Dist2D(Tractors[o].At, Mid) < 1500.f) return false;
				return true;
			};
			Tractors[Ti].AfterUntil = [this, Job] { WetIn(Job); };
			return;
		}
		Tractors[Ti].Until = [this, Nose, Ti, Sx = Tractors[Ti].StandX, T0 = GetWorld()->GetTimeSeconds()] { if (SvSpec(Tractors[Ti].Svc).bStairs && GetWorld()->GetTimeSeconds() - T0 < 120.0 && !StairsLane(Ti, Sx)) return false; return !PlaneNear(Nose, 6000.f) && !((SvSpec(Tractors[Ti].Svc).bCater || SvSpec(Tractors[Ti].Svc).bAmbu) && SvcSideBusy(Ti, Sx)) && !(SvSpec(Tractors[Ti].Svc).bCater && AmbuAtFront(Sx, true)) && !(SvSpec(Tractors[Ti].Svc).bAmbu && CaterAtFront(Sx)); };
		Tractors[Ti].AfterUntil = [this, Job, Ti, S, N, X, Xp, Y1, Y2]
		{
			TArray<FVector> W2; const FServiceSpec& Sq = SvSpec(Tractors[Ti].Svc);
			if (Sq.bCater || Sq.bAmbu) { if (FSvcJob* Jw = SvcJ(Job)) { CaterIn(*Jw, Sq.bAmbu ? 0 : Jw->CtDoor, Y1, W2); Jw->InWay = W2; } }
			else if (Sq.bStairs) { FSvcJob* Jw = SvcJ(Job); if (Jw && !StairsIn(*Jw, Y1, W2)) { ADP_WARN(TEXT("GSE: the passenger stairs do not go to %s: no aircraft there"), *Jw->Reg); Jw->Phase = 3; Tractors[Ti].Job = EJob::Back; SvcHome(Job); return; } }
			else if (Sq.InR > 0.f)
			{
				const float Dl = FMath::Abs(Sq.Lat - Sq.InLat) * 100.f, La = 2.f * Sq.InR / 100.f * FMath::Sin(FMath::Acos(1.f - Dl / (2.f * Sq.InR))), Ye = N + Sq.InAft - La - 0.1f;
				Leg(W2, X, Y1, 0.f, -1.f, X, N + Sq.InAft, 0.f, -1.f, Sq.InR); Leg(W2, X, N + Sq.InAft, 0.f, -1.f, Xp, Ye, 0.f, -1.f, Sq.InR); Leg(W2, Xp, Ye, 0.f, -1.f, Xp, Y2, 0.f, -1.f, Sq.InR);
				if (FSvcJob* Jw = SvcJ(Job)) Jw->InWay = W2;
			}
			else Leg(W2, X, Y1, 0.f, -1.f, X, Y2, 0.f, -1.f, Tractors[Ti].KTurnR);
			Go(Tractors[Ti], MoveTemp(W2), false, [this, Job] { SvcWork(Job); });
		};
	});
}

void AGSE::SvcWork(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J) return;
	J->Phase = 2; FTractor& T = Tractors[J->Ti]; T.Job = EJob::AtStand; Motor(T, false);
	J->Place = T.At; J->PlaceYaw = T.Yaw;
	if (J->bFetch)
	{
		// (fetched back after you had it: its crew go on with it)
		J->bFetch = false;
		ClimbOut(J->Men[0].Ci, J->Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) { X->bSeated = false; X->bOut = true; } Later([this, Job] { SvcReturn(Job, 0); }); });
		Wait(J->Men[0].Ci, 0.5f);
		return;
	}
	ADP_LOG(TEXT("GSE: the %s parked beside %s"), *T.Name, *T.Key);
	if (SvSpec(J->Svc).bFuel) { FuelArrive(Job); SvcFuelWork(Job); return; }
	if (SvSpec(J->Svc).bCater) { CaterArrive(Job); CaterStage(Job); return; }
	if (SvSpec(J->Svc).bAmbu) { AmbuArrive(Job); AmbuStage(Job); return; }
	if (SvSpec(J->Svc).bStairs) { J->Stage = 0; StairsWork(Job); StairsHelper(Job); return; }
	if (SvSpec(J->Svc).Wet) { WetArrive(Job); WetWork(Job); return; }
	for (int32 m = 0; m < J->Men.Num(); ++m) if (!J->Men[m].bGone) SvcManAt(Job, m);
}

void AGSE::SvcTake(int32 Job, int32 m, bool bTake)
{
	// out of its place, or back into it: from the rear, or from inside (up over the rear step)
	FSvcMan* M = SvcM(Job, m); FSvcJob* J = SvcJ(Job); if (!M || !J) return;
	const int32 Ci = M->Ci, Ti = J->Ti; const FServiceSpec& Sv = SvSpec(J->Svc);
	const FVector Off(-420.f - 70.f * m, m % 2 ? 130.f : -130.f, 0.f);   // (clear of the back for the next one)
	if (!Sv.Take.IsValidIndex(m)) return;
	const int32 It = !bTake && Sv.Bring.IsValidIndex(m) && Sv.Bring[m] >= 0 ? Sv.Bring[m] : Sv.Take[m];
	const FServiceItem K = Sv.Items[It]; const FVector Place = Tractors[Ti].SvcItems[It].Piv, At = K.Stand, Rear(-375.f, 0.f, 0.f);
	WaitFor(Ci, [this, Job, m, bTake](float) { return SvcAll(Job, [m, bTake](const FSvcMan& X) { return X.Seat >= m || (bTake ? X.Item != INDEX_NONE : X.bStowed); }); });   // (one at a time at the back)
	auto Pass = [this, Job, m, Ci, Ti, It, bTake]
	{
		FTractor::FSvcItem& I = Tractors[Ti].SvcItems[It];
		I.Ci = bTake ? Ci : INDEX_NONE; I.bShow = true; I.bMoved = true;
		if (FSvcMan* X = SvcM(Job, m)) if (!bTake || !X->bStowed) X->Item = bTake ? It : INDEX_NONE;
	};
	auto Hand = [this, Ti, At, Place](float K2, float PH, FDriverPose& P) { const FTractor& T = Tractors[Ti]; P = ADPPose::Reach(TW(T, At), SvFace(TW(T, At), TW(T, Place)), 1, TW(T, Place), 0.8f, PH, 1.f); P.Lean = 16.f; P = ADPPose::Lerp(ADPPose::Standing(TW(T, At), SvFace(TW(T, At), TW(T, Place)), PH), P, ADPPose::Ramp(K2)); };
	if (!K.bDeep)
	{
		WalkTo(Ci, [this, Ti, At] { return TW(Tractors[Ti], At); }, 12.f);
		PoseAct(Ci, [Hand](float K2, float PH, FDriverPose& P) { Hand(K2 * 0.5f, PH, P); }, 0.9f); Do(Ci, Pass); PoseAct(Ci, [Hand](float K2, float PH, FDriverPose& P) { Hand(0.5f + K2 * 0.5f, PH, P); }, 0.8f);
	}
	else
	{
		const float Fl = Tractors[Ti].KFloor;
		auto Climb = [this, Ti, At, Rear, Fl](bool bIn)
		{
			return [this, Ti, At, Rear, Fl, bIn](float K0, float PH, FDriverPose& P)
			{
				// ground, the rear step, the load floor (bent under the door's head), along it
				const FTractor& T = Tractors[Ti]; const float X = (bIn ? K0 : 1.f - K0) * 4.f;
				const FVector Fw = FRotator(0.f, T.Yaw, 0.f).Vector();
				auto Key = [&](int32 i)
				{
					FDriverPose Q = ADPPose::Standing(TW(T, i == 0 ? Rear : i == 1 ? FVector(-345.f, 0.f, 0.f) : i == 2 ? FVector(-285.f, 0.f, Fl) : i == 3 ? FVector((At.X - 285.f) * 0.5f, At.Y * 0.5f, Fl) : At), Fw, PH);
					if (i == 1) { Q.Foot[0] = TW(T, FVector(-305.f, -10.f, 46.f)); Q.Pelvis = TW(T, FVector(-325.f, 0.f, PH + 14.f)); Q.Lean = 16.f; }
					if (i == 2) { Q.Pelvis.Z -= 14.f; Q.Lean = 22.f; Q.Foot[1] = TW(T, FVector(-305.f, 10.f, 46.f)); }
					return Q;
				};
				const int32 i = FMath::Clamp(FMath::FloorToInt(X), 0, 3);
				P = ADPPose::Lerp(Key(i), Key(i + 1), FMath::SmoothStep(0.f, 1.f, X - i));
			};
		};
		WalkTo(Ci, [this, Ti, Rear] { return TW(Tractors[Ti], Rear); }, 12.f);
		PoseAct(Ci, Climb(true), 4.f);
		PoseAct(Ci, [Hand](float K2, float PH, FDriverPose& P) { Hand(K2 * 0.5f, PH, P); }, 0.9f); Do(Ci, Pass); PoseAct(Ci, [Hand](float K2, float PH, FDriverPose& P) { Hand(0.5f + K2 * 0.5f, PH, P); }, 0.8f);
		PoseAct(Ci, Climb(false), 4.f);
	}
	if (!bTake) Do(Ci, [this, Job, m] { if (FSvcMan* X = SvcM(Job, m)) X->bStowed = true; });
	WalkTo(Ci, [this, Ti, Off] { return TW(Tractors[Ti], Off); }, 25.f);
}

void AGSE::SvcUp(int32 Job, int32 m, bool bUp)
{
	// the jetway's service stair: step by step, the left hand on the rail; its landing, the door, the tunnel, the aircraft's door
	FSvcMan* M = SvcM(Job, m); FSvcJob* J = SvcJ(Job); if (!M || !J) return;
	const int32 Ci = M->Ci, Jet = J->Jet;
	auto Rig = [this] { return AJetwayRigs::Get(GetWorld()); };
	auto StairPt = [this, Rig, Jet](bool bTop, float Out) { TArray<FVector> St; float Hw = 40.f; AJetwayRigs* R = Rig(); if (!R || !R->ServiceSteps(Jet, St, Hw)) return FVector(FVector::ZeroVector); const FVector Dn = SvFace(St.Last(), St[0]); return FVector(bTop ? St.Last() - Dn * Out : St[0] + Dn * Out); };
	auto DoorPt = [Rig, Jet](float Out) { FVector Md(0.f), O(0.f); AJetwayRigs* R = Rig(); if (R) R->ServiceDoorFrame(Jet, Md, O); return FVector(Md + O * Out); };
	auto CabPt = [Rig, Jet](float Fw) { FVector Md(0.f), F(0.f); AJetwayRigs* R = Rig(); if (R) R->CabView(Jet, Md, F); return FVector(Md + F * Fw); };
	auto Stair = [this, Ci, Jet, Rig](bool bUpward)
	{
		TArray<FVector> St0; float Hw0 = 40.f; AJetwayRigs* R0 = Rig(); const int32 Ns = R0 && R0->ServiceSteps(Jet, St0, Hw0) ? St0.Num() : 22;
		PoseAct(Ci, [this, Jet, Rig, bUpward](float K, float PH, FDriverPose& P)
		{
			TArray<FVector> St; float Hw = 40.f; AJetwayRigs* R = Rig(); if (!R || !R->ServiceSteps(Jet, St, Hw)) return;
			const int32 N = St.Num(); const FVector Dn = SvFace(St.Last(), St[0]), Face = bUpward ? -Dn : Dn;
			// (the place of step i; before the first: the ground, after the last: the landing)
			auto At = [&](int32 i) { if (i < 0) { FVector G = St[0] + Dn * (28.f * -i); G.Z = Plan(G.X / 100.f, -G.Y / 100.f).Z; return G; } return i >= N ? FVector(St[N - 1] - Dn * (28.f * (i - N + 1))) : St[i]; };
			// (one foot stands on step i, the other swings from i - 1 to i + 1; going down the same way backwards)
			const float U = (bUpward ? K : 1.f - K) * (N + 1) - 1.f; const int32 i = FMath::FloorToInt(U); const float F = U - i, Sm = FMath::SmoothStep(0.15f, 0.85f, F);
			const FVector Lt = ADPPose::LeftOf(Face); FVector Ft[2];
			for (int32 s = 0; s < 2; ++s)
			{
				const bool bStand = ((i & 1) == 0) == (s == 0);
				Ft[s] = (bStand ? At(i) : FMath::Lerp(At(i - 1), At(i + 1), Sm) + SvUp * (FMath::Sin(PI * Sm) * 9.f)) + Lt * (s == 0 ? 11.f : -11.f);
			}
			P = FDriverPose(); P.Facing = Face; P.Foot[0] = Ft[0]; P.Foot[1] = Ft[1]; P.Lean = bUpward ? 9.f : 2.f;
			const FVector Mid = FMath::Lerp(At(i), At(i + 1), F); P.Pelvis = FVector(Mid.X, Mid.Y, FMath::Min(Ft[0].Z, Ft[1].Z) + PH + 4.f);
			P.bHand[0] = true; P.Hand[0] = Mid + Lt * Hw + Face * 18.f + SvUp * 92.f; P.Grip[0] = 0.7f;   // (the right one carries)
		}, Ns * 0.62f);
	};
	// (one way at a time on it)
	auto Enter = [this, Ci, Jet](int8 Dir) { WaitFor(Ci, [this, Jet, Dir](float) { TPair<int8, int32>& U = SvcStairUse.FindOrAdd(Jet, TPair<int8, int32>(0, 0)); if (U.Value > 0 && U.Key != Dir) return false; U.Key = Dir; ++U.Value; return true; }); };
	auto Exit = [this, Ci, Jet] { Do(Ci, [this, Jet] { if (TPair<int8, int32>* U = SvcStairUse.Find(Jet)) U->Value = FMath::Max(0, U->Value - 1); }); };
	auto Level = [this, Ci](bool b) { Do(Ci, [this, Ci, b] { if (ATugs* G = Tg()) G->Crew[Ci].bUp = b; }); };
	auto Door = [this, Ci, Jet, Rig, DoorPt](bool bFromOut, bool bOpen)
	{
		PoseAct(Ci, [this, Ci, DoorPt, bFromOut](float K, float PH, FDriverPose& P)
		{
			// (standing where the walk ended, the left hand to the leaf's free side at handle height)
			const FVector H = DoorPt(bFromOut ? 6.f : -6.f), Out = (DoorPt(60.f) - DoorPt(0.f)).GetSafeNormal2D(); FVector St = DoorPt(bFromOut ? 62.f : -62.f);
			const ATugs* G = Tg(); St.Z = G && G->Crew[Ci].A.IsValid() ? G->Crew[Ci].A->GetActorLocation().Z : H.Z - 105.f;
			P = ADPPose::Reach(St, bFromOut ? -Out : Out, 0, FVector(H.X, H.Y, St.Z + 104.f), 0.7f, PH, FMath::Sin(PI * K));
		}, 0.9f);
		Do(Ci, [Rig, Jet, bOpen] { if (AJetwayRigs* R = Rig()) R->SetServiceDoor(Jet, bOpen); });
	};
	if (bUp)
	{
		Wait(Ci, 0.1f + 2.2f * m);   // (behind one another, not on one another's heels)
		Enter(1);
		WalkTo(Ci, [StairPt] { return StairPt(false, 130.f); }, 25.f); WalkTo(Ci, [StairPt] { return StairPt(false, 34.f); }, 12.f);
		Level(true); Stair(true);
		WalkTo(Ci, [StairPt] { return StairPt(true, 34.f); }, 20.f); WalkTo(Ci, [DoorPt] { return DoorPt(62.f); }, 15.f);
		Door(true, true); Wait(Ci, 1.4f);
		WalkTo(Ci, [DoorPt] { return DoorPt(-75.f); }, 20.f); Exit();
		WalkTo(Ci, [CabPt] { return CabPt(0.f); }, 40.f); WalkTo(Ci, [CabPt] { return CabPt(95.f); }, 25.f);
		// (no cabin yet, to-do 209: out of sight at the aircraft's door, as the passengers in the tunnel)
		Do(Ci, [this, Job, m, Ci] { if (ATugs* G = Tg()) if (AActor* A = G->Crew[Ci].A.Get()) A->SetActorHiddenInGame(true); if (FSvcMan* X = SvcM(Job, m)) X->bIn = true; });
		return;
	}
	Do(Ci, [this, Job, m, Ci] { if (FSvcMan* X = SvcM(Job, m)) X->bIn = false; if (ATugs* G = Tg()) if (AActor* A = G->Crew[Ci].A.Get()) A->SetActorHiddenInGame(false); });
	WalkTo(Ci, [CabPt] { return CabPt(0.f); }, 40.f); WalkTo(Ci, [DoorPt] { return DoorPt(-62.f); }, 15.f);
	Enter(2);
	Door(false, true); Wait(Ci, 1.4f);
	WalkTo(Ci, [DoorPt] { return DoorPt(62.f); }, 15.f); WalkTo(Ci, [StairPt] { return StairPt(true, 34.f); }, 12.f);
	Stair(false); Level(false); Exit();
	WalkTo(Ci, [StairPt] { return StairPt(false, 150.f); }, 25.f);
	Do(Ci, [this, Job, m, Jet, Rig] { if (FSvcMan* X = SvcM(Job, m)) X->bDown = true; if (SvcAll(Job, [](const FSvcMan& Y) { return Y.bDown; })) if (AJetwayRigs* R = Rig()) R->SetServiceDoor(Jet, false); });
}

void AGSE::SvcManAt(int32 Job, int32 m)
{
	FSvcJob* J = SvcJ(Job); FSvcMan* M = SvcM(Job, m); if (!J || !M) return;
	const int32 Ci = M->Ci, Ti = J->Ti, Jet = J->Jet; const FServiceSpec& Sv = SvSpec(J->Svc);
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(J->StandX, S, N, Sp);
	if (m == 0)
	{
		ClimbOut(Ci, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) { X->bSeated = false; X->bOut = true; } });
		WalkTo(Ci, [this, Ti] { return TW(Tractors[Ti], FVector(-345.f, -175.f, 0.f)); }, 30.f);   // (round the back's corner)
		for (int32 d = 1; d < Sv.Doors.Num(); ++d) SvcDoor(Ci, Ti, d, true);
	}
	else SvcAlight(Job, m);
	if (m > 0) WalkTo(Ci, [this, Ti, m] { return TW(Tractors[Ti], FVector(-430.f - 60.f * m, 130.f, 0.f)); }, 40.f);
	WaitFor(Ci, [this, Ti](float) { const FTractor& T = Tractors[Ti]; for (int32 d = 1; d < SvSpec(T.Svc).Doors.Num(); ++d) if (T.SvcDoors[d].Now < 0.99f) return false; return true; });
	SvcTake(Job, m, true);
	// (everyone off the aircraft, the jetway at it)
	WaitFor(Ci, [this, Job, Jet](float) { const FSvcJob* X = SvcJ(Job); AJetwayRigs* R = AJetwayRigs::Get(GetWorld()); return X && X->bGo && (!R || Jet == INDEX_NONE || R->State(Jet) == AJetwayRigs::EState::Docked); });
	const float Xw = S + Sp * 0.5f + 1.4f;   // (outside the wingtip, inside the stand's line)
	WalkTo(Ci, [this, Xw, N] { return Plan(Xw, N + 23.f); }, 50.f); WalkTo(Ci, [this, Xw, N] { return Plan(Xw, N + 6.f); }, 50.f);
	Do(Ci, [this, Job, m, Ci] { Later([this, Job, m] { SvcCabin(Job, m); }); });
	Wait(Ci, 0.5f);
}

void AGSE::SvcCabin(int32 Job, int32 m)
{
	// (planned here, at the stair's foot: the jetway stands now, so the stair has its steps)
	FSvcJob* J = SvcJ(Job); FSvcMan* M = SvcM(Job, m); if (!J || !M || M->bGone) return;
	const int32 Ci = M->Ci, Ti = J->Ti; const FServiceSpec& Sv = SvSpec(J->Svc);
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(J->StandX, S, N, Sp); const float Xw = S + Sp * 0.5f + 1.4f;
	if (Sv.bJetway) SvcUp(Job, m, true); else Do(Ci, [this, Job, m] { if (FSvcMan* X = SvcM(Job, m)) X->bIn = true; });
	WaitFor(Ci, [this, Job](float E) { const FSvcJob* X = SvcJ(Job); return !X || X->RowsDone >= X->Rows || E > 2400.f; });
	// the rubbish into bags: the full ones carried down
	Do(Ci, [this, Job, m, Ci, Ti]
	{
		const FServiceSpec& K = SvSpec(Tractors[Ti].Svc); FSvcMan* X = SvcM(Job, m);
		if (!X || !K.Bring.IsValidIndex(m) || K.Bring[m] < 0) return;
		if (X->Item != INDEX_NONE) { FTractor::FSvcItem& O = Tractors[Ti].SvcItems[X->Item]; O.Ci = INDEX_NONE; O.bShow = false; O.bMoved = true; }
		X->Item = K.Bring[m]; FTractor::FSvcItem& F = Tractors[Ti].SvcItems[X->Item]; F.Ci = Ci; F.bShow = true; F.bMoved = true; Tractors[Ti].bUsed = true;
	});
	Wait(Ci, 0.1f + 3.f * m);
	if (Sv.bJetway) SvcUp(Job, m, false); else Do(Ci, [this, Job, m] { if (FSvcMan* X = SvcM(Job, m)) { X->bIn = false; X->bDown = true; } });
	WalkTo(Ci, [this, Xw, N] { return Plan(Xw, N + 6.f); }, 50.f); WalkTo(Ci, [this, Xw, N] { return Plan(Xw, N + 23.f); }, 50.f);
	Do(Ci, [this, Job, m] { Later([this, Job, m] { SvcReturn(Job, m); }); });
	Wait(Ci, 0.5f);
}

bool AGSE::SvcHere(int32 Job) const
{
	const FSvcJob* J = SvcJobs.Find(Job); if (!J || !Tractors.IsValidIndex(J->Ti)) return false;
	const FTractor& T = Tractors[J->Ti];
	return T.Job == EJob::AtStand && !J->bFetch && !SvcYours(T) && T.Speed < 1.f && FVector::Dist2D(T.At, J->Place) < 1200.f;
}

void AGSE::SvcFetch(int32 Job)
{
	// you drove it off and left it: its driver walks to it and brings it back to its place
	FSvcJob* J = SvcJ(Job); if (!J || !J->Men.Num()) return;
	FTractor& T = Tractors[J->Ti]; const int32 Ci = J->Men[0].Ci, Ti = J->Ti;
	TArray<FVector> W;
	if (SvcHere(Job) || SvcYours(T) || T.Player.IsValid() || !Apron(W, T.At, SvSpec(J->Svc).InR > 0.f ? Plan(J->StandX + SvSpec(J->Svc).InLat, 171.f) : J->Place, &T))
	{
		// (with you again, or where it cannot be driven from: asked again)
		Wait(Ci, 8.f); Do(Ci, [this, Job] { Later([this, Job] { SvcReturn(Job, 0); }); }); Wait(Ci, 0.5f); return;
	}
	J->bFetch = true; T.Job = EJob::Boarding; T.Driver = Ci;
	ADP_LOG(TEXT("GSE: the %s's driver fetches it back to stand x %.0f"), *T.Name, J->StandX);
	HiredWay(Ci, T.At, 0.f);
	ClimbIn(Ci, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) X->bSeated = true; });
	Do(Ci, [this, Job, Ti] { for (FTractor::FSvcDoor& D : Tractors[Ti].SvcDoors) D.Want = 0.f; Motor(Tractors[Ti], true); SvcApron(Job); });
}

void AGSE::SvcReturn(int32 Job, int32 m)
{
	// back at the vehicle - if it is there (you may have taken it meanwhile: they wait, the driver fetches it when you have left it)
	FSvcJob* J = SvcJ(Job); FSvcMan* M = SvcM(Job, m); if (!J || !M || M->bGone) return;
	const int32 Ci = M->Ci, Ti = J->Ti; const FServiceSpec& Sv = SvSpec(J->Svc);
	if (!SvcHere(Job))
	{
		if (m == 0) { WaitFor(Ci, [this, Job, Ti](float) { return SvcHere(Job) || (!Tractors[Ti].Player.IsValid() && Tractors[Ti].Speed < 1.f); }); Do(Ci, [this, Job] { Later([this, Job] { if (SvcHere(Job)) SvcReturn(Job, 0); else SvcFetch(Job); }); }); }
		else { WaitFor(Ci, [this, Job](float) { return SvcHere(Job); }); Do(Ci, [this, Job, m] { Later([this, Job, m] { SvcReturn(Job, m); }); }); }
		Wait(Ci, 0.5f);
		return;
	}
	// (its load doors: shut by you, or for the way back)
	if (m == 0) for (int32 d = 1; d < Sv.Doors.Num(); ++d) if (Tractors[Ti].SvcDoors[d].Want < 0.5f) SvcDoor(Ci, Ti, d, true);
	WalkTo(Ci, [this, Ti, m] { return TW(Tractors[Ti], FVector(-430.f - 60.f * m, m % 2 ? 130.f : -130.f, 0.f)); }, 40.f);
	WaitFor(Ci, [this, Ti](float E) { const FTractor& T = Tractors[Ti]; for (int32 d = 1; d < SvSpec(T.Svc).Doors.Num(); ++d) if (T.SvcDoors[d].Now < 0.99f) return E > 60.f; return true; });
	SvcTake(Job, m, false);
	if (m == 0)
	{
		WaitFor(Ci, [this, Job](float) { return SvcAll(Job, [](const FSvcMan& X) { return X.bStowed; }); });
		for (int32 d = Sv.Doors.Num() - 1; d >= 1; --d) SvcDoor(Ci, Ti, d, false);
		Do(Ci, [this, Job] { Later([this, Job] { SvcLeave(Job); }); });
		Wait(Ci, 0.5f);
	}
	else { WaitFor(Ci, [this, Job](float) { const FSvcJob* X = SvcJ(Job); return !X || X->Phase == 3; }); SvcBoard(Job, m); }
}

// ------------------------------------------------------------------ and back

void AGSE::SvcLeave(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J || J->Phase == 3) return;
	J->Phase = 3; const int32 Ti = J->Ti, Ci = J->Men[0].Ci;
	if (!SvSpec(J->Svc).bFuel && !SvSpec(J->Svc).bCater && !SvSpec(J->Svc).bAmbu && !SvSpec(J->Svc).Wet && !SvSpec(J->Svc).bStairs) WalkTo(Ci, [this, Ti] { return TW(Tractors[Ti], FVector(-345.f, -175.f, 0.f)); }, 30.f);
	if (!J->Men[0].bSeated) ClimbIn(Ci, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) X->bSeated = true; });   // (seated: the catering truck that gave up the rear door)
	WaitFor(Ci, [this, Job, Ti](float) { return SvcAll(Job, [](const FSvcMan& X) { return X.bSeated; }) && Tractors[Ti].SvcDoors[0].Now < 0.01f; });
	if (SvSpec(J->Svc).Lat < 0.f && !SvSpec(J->Svc).Wet) WaitFor(Ci, [this, Ti, Sx = J->StandX](float E) { return E > 300.f || !SvcSideBusy(Ti, Sx); });   // (its turn on the aircraft's right)
	Do(Ci, [this, Job, Ti]
	{
		FSvcJob* X = SvcJ(Job); if (!X) return;
		FTractor& T = Tractors[Ti]; Motor(T, true);
		for (FTractor::FSvcDoor& D : T.SvcDoors) D.Want = 0.f;   // (a load door left open by somebody who is gone)
		// straight back behind the tail (the beeper), its back round to the side, then forward
		float S = 0.f, N = 0.f, Sp = 36.f, Yw = 0.f; StandGeo(X->StandX, S, N, Sp);
		const FServiceSpec& Sv = SvSpec(X->Svc); const float Ln = Sv.InR > 0.f ? Sv.InLat : Sv.Lat; const FVector A = Plan(S + Ln, N + Sv.Far), B = PlanPose(S + Ln + 8.f, N + Sv.Far + 8.f, -1.f, 0.f, Yw);
		// (you left it a little off its place: straight on from there)
		if (FVector::Dist2D(T.At, Sv.bCater || Sv.bAmbu || Sv.bStairs ? X->Place : Plan(S + Sv.Lat, N + Sv.Aft)) > 120.f || FMath::Abs(FMath::FindDeltaAngleDegrees(T.Yaw, X->PlaceYaw)) > 8.f) { T.Job = EJob::Back; SvcHome(Job); return; }
		TArray<FVector> R;
		if (Sv.Wet)
		{
			// (it stands with its nose away from the aircraft: straight out)
			const FVector F = T.At, E = Plan(S + Sv.Lat, N + Sv.Far + 4.f); for (int32 i = 1; i <= 30; ++i) R.Add(FMath::Lerp(F, E, i / 30.f));
			Go(T, MoveTemp(R), false, [this, Job, Ti] { Tractors[Ti].Job = EJob::Back; SvcHome(Job); });
			return;
		}
		if (Sv.InR > 0.f)
		{
			// (back out the way it came: under the wing and round to the lane beside the wingtip)
			for (int32 i = X->InWay.Num() - 1; i >= 0; --i) if (-X->InWay[i].Y / 100.f < N + Sv.Far - 0.5f) R.Add(X->InWay[i]);
			R.Add(A);
		}
		else for (int32 i = 1; i <= 30; ++i) R.Add(FMath::Lerp(T.At, A, i / 30.f));
		float Ya = T.Yaw; if (Sv.bCater || Sv.bAmbu || Sv.bStairs) PlanPose(S + Ln, N + Sv.Far, 0.f, -1.f, Ya);   // (its lane's way, not the door's)
		Dubins(R, A, Ya + 180.f, B, Yw + 180.f, T.KTurnR);
		Go(T, MoveTemp(R), true, [this, Job, Ti] { Tractors[Ti].Job = EJob::Back; SvcHome(Job); });
	});
}

void AGSE::SvcDriveHome(int32 Job, TFunction<void()> Then)
{
	FSvcJob* J = SvcJ(Job); if (!J) return;
	const int32 Ti = J->Ti; FTractor& T = Tractors[Ti]; const FServiceSpec& Sv = SvSpec(J->Svc);
	// down beside its box's way (two turning circles off it, clear of the next stand's line), round to point away, straight back in: nose out
	TArray<FVector> H; const FVector2f Ho(Sv.HomeX, Sv.HomeY), Hd(Sv.HomeHx, Sv.HomeHy), Sd(Sv.HomeHy, -Sv.HomeHx);
	if (Sv.bDeicer || Sv.bAmbu)
	{
		// (the apron's east edge: no room for a loop - from the west between the toilet truck's box and the stairs', round to face north, straight back in)
		const float Yl = Ho.Y + 21.f, R = T.KTurnR / 100.f;
		if (!Apron(H, T.At, Plan(Ho.X - R - 14.f, Yl), &T)) { RetryIn(Ti, 8.f, [this, Job, Then] { SvcDriveHome(Job, Then); }); return; }
		Leg(H, Ho.X - R - 14.f, Yl, 1.f, 0.f, Ho.X - R - 0.4f, Yl, 1.f, 0.f, T.KTurnR); Leg(H, Ho.X - R - 0.4f, Yl, 1.f, 0.f, Ho.X, Yl + R + 0.4f, 0.f, 1.f, T.KTurnR); Leg(H, Ho.X, Yl + R + 0.4f, 0.f, 1.f, Ho.X, Yl + R + 4.f, 0.f, 1.f, T.KTurnR);
		Go(T, MoveTemp(H), false, [this, Then, Ti]
		{
			FTractor& U = Tractors[Ti]; TArray<FVector> Rv; for (int32 i = 1; i <= 50; ++i) Rv.Add(FMath::Lerp(U.At, U.Home, i / 50.f));
			Go(U, MoveTemp(Rv), true, Then);
		});
		return;
	}
	if (Sv.HomeRoad > 0.f)
	{
		// along the road past the corner, then backed round it and straight back into the box
		const FVector2f Lf = -Sd, Cn = Ho + Hd * Sv.HomeRoad, P0 = Cn - Lf * 22.f, P1 = Cn + Lf * (T.KTurnR / 100.f);
		if (!Apron(H, T.At, Plan(P0.X, P0.Y), &T)) { RetryIn(Ti, 8.f, [this, Job, Then] { SvcDriveHome(Job, Then); }); return; }
		Leg(H, P0.X, P0.Y, Lf.X, Lf.Y, P1.X, P1.Y, Lf.X, Lf.Y, T.KTurnR);
		Go(T, MoveTemp(H), false, [this, Then, Ti, P1, Lf, Hd, Cn]
		{
			FTractor& U = Tractors[Ti]; TArray<FVector> R; float Y0 = 0.f, Y1 = 0.f; const FVector2f Q = Cn - Hd * (U.KTurnR / 100.f);
			const FVector Pa = PlanPose(P1.X, P1.Y, Lf.X, Lf.Y, Y0), Pb = PlanPose(Q.X, Q.Y, Hd.X, Hd.Y, Y1);
			Dubins(R, U.At, U.Yaw + 180.f, Pb, Y1 + 180.f, U.KTurnR * 0.98f);
			for (int32 i = 1; i <= 40; ++i) R.Add(FMath::Lerp(Pb, U.Home, i / 40.f));
			Go(U, MoveTemp(R), true, Then);
		});
		return;
	}
	const FVector2f A = Ho + Sd * (2.f * Sv.TurnR / 100.f) + Hd * 15.f, In = A + Hd * 34.f, B = Ho + Hd * 15.f, C = Ho + Hd * 19.f;
	if (!Apron(H, T.At, Plan(In.X, In.Y), &T)) { RetryIn(Ti, 8.f, [this, Job, Then] { SvcDriveHome(Job, Then); }); return; }
	Leg(H, In.X, In.Y, -Hd.X, -Hd.Y, A.X, A.Y, -Hd.X, -Hd.Y, T.KTurnR);
	Leg(H, A.X, A.Y, -Hd.X, -Hd.Y, B.X, B.Y, Hd.X, Hd.Y, T.KTurnR);
	Leg(H, B.X, B.Y, Hd.X, Hd.Y, C.X, C.Y, Hd.X, Hd.Y, T.KTurnR);
	Go(T, MoveTemp(H), false, [this, Then, Ti]
	{
		FTractor& U = Tractors[Ti]; TArray<FVector> R;
		for (int32 i = 1; i <= 40; ++i) R.Add(FMath::Lerp(U.At, U.Home, i / 40.f));
		Go(U, MoveTemp(R), true, Then);
	});
}

void AGSE::SvcHome(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J) return;
	const int32 Ti = J->Ti;
	auto End = [this, Job, Ti]
		{
			FTractor& X = Tractors[Ti]; X.Job = EJob::Home; Motor(X, false); ADP_LOG(TEXT("GSE: the %s parked, switched off"), *X.Name);
			FSvcJob* J2 = SvcJ(Job); if (!J2) return;
			for (int32 m = 0; m < J2->Men.Num(); ++m)
			{
				const int32 Ci = J2->Men[m].Ci; if (J2->Men[m].bGone) continue;
				if (m > 0) { SvcAlight(Job, m); Do(Ci, [this, Job, m] { if (FSvcMan* Y = SvcM(Job, m)) Y->bGone = true; }); GiveBack(Ci); continue; }
				Wait(Ci, 2.f);
				ClimbOut(Ci, Ti, [this, Job] { if (FSvcMan* Y = SvcM(Job, 0)) { Y->bSeated = false; Y->bOut = true; } });
				WaitFor(Ci, [this, Job](float E) { return E > 120.f || SvcAll(Job, [](const FSvcMan& Y) { return Y.Seat == 0; }); });
				Do(Ci, [this, Job, Ti] { FTractor& Y = Tractors[Ti]; Y.Job = EJob::None; Y.Key.Reset(); Y.StandX = 0.f; Y.Driver = INDEX_NONE; if (FSvcJob* Z = SvcJ(Job)) Z->Phase = 9; });
				GiveBack(Ci);
			}
		};
	// (it never left its box: nothing to drive)
	if (FVector::Dist2D(Tractors[Ti].At, Tractors[Ti].Home) < 200.f && FMath::Abs(FMath::FindDeltaAngleDegrees(Tractors[Ti].Yaw, Tractors[Ti].HomeYaw)) < 20.f) End(); else SvcDriveHome(Job, End);
}
