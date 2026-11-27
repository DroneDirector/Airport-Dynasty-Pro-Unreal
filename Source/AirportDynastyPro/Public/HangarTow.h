// A hangar tow (to-do 311 step A, docs/Hired-Crew-Plan.md): an aircraft of ours at our airport in a C or D check
// (Sim/ADPMaint) is towed by the stand's tug and its hired crew (Tugs.h TowPlan / Push / Release: the same calls your own
// tow will use) from its stand along the taxiways to a free hangar, pushed tail first onto the bay's stop mark, the door
// opened before and shut after; cold and dark inside. After the check the same tow takes it back to a free stand.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RunwayRadio.h"
#include "HangarTow.generated.h"

class ABabylonModel;
class AHangar;
class UADPAircraftLights;
class UInstancedStaticMeshComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API AHangarTow : public AActor
{
	GENERATED_BODY()

public:
	AHangarTow();
	static AHangarTow* Get(UWorld* W);
	virtual void Tick(float Dt) override;
	/** Reg is in or on its way to or from a hangar: it cannot be flown. */
	static bool Busy(UWorld* W, const FString& Reg);
	/** Reg's tow now (the Maintenance page): its hall, its stage (0 the tug coming, 1 on the move, 2 unhooked, 3 in the hall)
	 *  and whether it is the tow back out. False: no tow for it. */
	static bool TowOf(UWorld* W, const FString& Reg, FString& Hall, int32& Stage, bool& bBack);
	/** A hall another tow holds now (in it, or on the way in or out): not free for one more. */
	static bool HallTaken(UWorld* W, const FString& HallId);
	/** A tow is on the runway strip now (between the holding points): no take-off starts (SimTraffic). */
	static bool OnRunway(UWorld* W, bool bVans = true);   // (bVans: the maintenance vans crossing count too)
	/** Where the tows cross the runway (plan m): the holding points on the stands' side and on the hangars' side - the
	 *  maintenance vans cross only there (GSEMaint.cpp). The last tow's crossing is kept; false before any tow was planned. */
	static bool Crossing(UWorld* W, FVector2f& South, FVector2f& North);
	/** Your own tow (to-do 311 step C): the aircraft the hangar tug at At (world) may couple to - Key's if set, else the nearest
	 *  waiting one within 40 m (Key set then); its nose tyre and main gear (plan m). */
	static bool YouFit(UWorld* W, const FVector& At, FString& Key, FVector2f& Nose, FVector2f& Main);
	/** E at the tow bar: the tow ends here if the nose tyre is on its stop mark (0.5 m, 3 deg); else Hint says how far off. */
	static bool YouEnd(UWorld* W, const FString& Key, FString& Hint);
	/** E at the hangar tug's wheel, holding short of the runway on your own tow (hangar step 6): your call to the tower.
	 *  False: no call of yours is due (E does what it did). */
	static bool YouCall(UWorld* W);

	/** One leg of the tow: the line the leading gear follows (plan m, corners rounded) - the nose tyre's pulled, the main
	 *  gear's pushed tail first; the other gear follows 12.8 m behind on the same line. */
	struct FLeg { TArray<FVector2f> P; TArray<float> S; bool bPush = false; float HoldS = -1.f; };   // (HoldS: where its leading gear reaches the runway's holding point)

private:
	struct FJob
	{
		FString Reg, Key, Stand, Hangar; float StandX = 0, Hx = 0; bool bBack = false, bPlanned = false;
		int32 Stage = 0, Leg = 0; float S = 0, V = 0, WhyT = 0, Clock = 0;
		TArray<FLeg> Legs; TArray<FVector2f> Way, Home; FString Via;   // (Via: the taxiways it follows, as said on the radio)
		bool bYou = false; float DoorT = 0.f;             // (towed by you at the hangar tug's wheel: TugYou.cpp; the crew take it over from where it stands - Rejoin)
		float BlockT = 0.f;                               // (stopped for another aircraft's wingtip: when it was last logged)
		FVector2f Nose = FVector2f::ZeroVector, Main = FVector2f::ZeroVector;
		bool bCleared = false; float HoldT = 0.f;        // (holding short of the runway: cleared to cross, how long held)
		ADPRunwayRadio::FCall Radio;                     // (the crossing on the tower frequency: RunwayRadio.h, hangar step 6)
		bool bCallNow = false, bAtHold = false, bWasOn = false, bVacate = false;   // (you: E made the call; at the holding point now; on the strip last frame; "vacated" due)
		bool bRider = false; int32 Rider = 0;             // (the brake rider in the cockpit; AGSE::MaintRider's answer)
		int32 Chocks = 0, CkI = INDEX_NONE;              // (the hangar's chocks: 0 in the store, 1 being set, 2 set, 3 being taken away; its first of 4 instances)
		TWeakObjectPtr<ABabylonModel> Model; TWeakObjectPtr<UADPAircraftLights> Lamps; TWeakObjectPtr<AHangar> Hall;
	};
	TArray<FJob> Jobs;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Ck;
	float Scan = 0;
	TArray<TWeakObjectPtr<AActor>> Planes;   // (the other aircraft, parked or moving - SIM/ and MOVE/ tags - gathered every 2 s for the wingtip check)
	/** No other aircraft within 3 m of the towed one's wings, body or tailplane were it at Nose / Main (plan m); Who: the one in the way. */
	bool Clear(const FJob& J, const FVector2f& Nose, const FVector2f& Main, FString& Who) const;
	void Look();
	/** After loading (to-do 311): an aircraft the save has in a hall (FADPAircraft::HangarIn) is there again, chocked, its check
	 *  going on or its tow back next; one on its way out stands at its stand again and its tow begins anew from there. */
	void Restore();
	bool bRestored = false;
	bool Start(FJob& J, bool bBack);
	void Run(FJob& J, float Dt);
	void Pose(FJob& J);
	bool Chock(FJob& J, bool bOn, bool bNow = false);   // (bNow: set at once, as they were when the game was saved - Restore)
	void YouRun(FJob& J, float Dt, bool bGo, bool bChocked, float Lag);
	bool Rejoin(FJob& J);
	void Finish(FJob& J);
	void Mark(const FJob& J, FVector2f& Nose, FVector2f& Dir) const;
	ADPRunwayRadio::FWho RadioWho(const FJob& J, const FVector2f& At) const;   // (who talks for this tow, holding short at At)
};
