// The refueller's own moving parts (to-do 212, Private/GSEFuel.cpp): what a FTractor of AGSE needs besides the shared ones.
#pragma once

#include "CoreMinimal.h"

class UCableComponent;
class UBoxComponent;
class UTextRenderComponent;

/** What you pull off a reel by hand and wind back by hand: how much is off it (cm), at least what it needs now. The crew's follows by itself. */
struct FSvcPay
{
	float Pay = 0.f, Want = 0.f, Need = 0.f; bool bHand = false;
	float Slack() const { return bHand ? FMath::Max(0.f, Pay - Need) : 0.f; }
	void Pull(float To) { if (!bHand) { bHand = true; Pay = Need; } Want = Need + To; }
	float Tick(float Dt, bool bCarried, bool bHome)
	{
		if (!bHand) return 0.f;
		Pay = FMath::FInterpConstantTo(Pay, Want, Dt, 80.f); if (Pay < Need) { Pay = Need; Want = FMath::Max(Want, Need); } if (bCarried) Want = Pay;
		if (bHome && Pay <= Need + 1.f && Want <= Need + 1.f) bHand = false;
		return Pay;
	}
};

/** One hose: its reel, its nozzle and where that is. */
struct FSvcHose
{
	enum : uint8 { InPlace, RightHand, LeftHand, Put, YourHand };
	int32 GReel = INDEX_NONE, GTail = INDEX_NONE, GNozzle = INDEX_NONE; TArray<int32> GTurn; UCableComponent* Hose = nullptr; FSvcPay Line;
	uint8 Noz = InPlace; FVector NozAt = FVector::ZeroVector, NozNow = FVector::ZeroVector; float NozYaw = 0.f, NozYawNow = 0.f, Out = 0.f, OutWas = 0.f, Reel = 0.f; bool bConn = false;
};

/** (itself the left hose, its crew's; Rt: the right one) */
struct FSvcRefuel : FSvcHose
{
	FSvcHose Rt;
	int32 GWheel[2] = {INDEX_NONE, INDEX_NONE}, GSteer[2] = {INDEX_NONE, INDEX_NONE};   // the third axle
	int32 GPlat = INDEX_NONE, GGate = INDEX_NONE, GPLever = INDEX_NONE, GScissor[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};
	int32 GBondReel = INDEX_NONE, GClamp = INDEX_NONE, GBondTail = INDEX_NONE, GChock[2] = {INDEX_NONE, INDEX_NONE}, GDead = INDEX_NONE, GDeadLead = INDEX_NONE;
	int32 GLever[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE}, GNeedle[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};   // (the pump's and the left hose's lever)
	int32 GExt[2] = {INDEX_NONE, INDEX_NONE}, GTicket = INDEX_NONE;
	// (to-do 235: the manhole lids, the fill flap and its valve lever - opened by hand; the ticket button sinks in when pressed)
	int32 GLid[2] = {INDEX_NONE, INDEX_NONE}, GFlap = INDEX_NONE, GFill = INDEX_NONE, GTicketBtn = INDEX_NONE;
	float LidNow[2] = {0.f, 0.f}, LidWant[2] = {0.f, 0.f}, FlapNow = 0.f, FlapWant = 0.f, FillNow = 0.f, FillWant = 0.f, PrintT = 0.f;
	UCableComponent *Bond = nullptr, *Lead = nullptr; UBoxComponent* Floor = nullptr; UTextRenderComponent* Meter = nullptr;
	// where the carried things are: in their place, in a hand of Ci, or put somewhere (At, Yaw: root frame)
	uint8 Clamp = InPlace, Chock[2] = {InPlace, InPlace}, Dead = InPlace, Ext[2] = {InPlace, InPlace}, Ticket = InPlace;   // (Ticket: InPlace none, Put hanging from the printer)
	FVector ClampAt = FVector::ZeroVector, Couple = FVector(-430.f, -118.f, 380.f); FSvcPay BondLine;
	int32 Ci = INDEX_NONE;
	float Lift = 0.f, LiftWant = 0.f, Gate = 0.f, GateWant = 0.f, BondOut = 0.f, BondSpin = 0.f;
	float LeverNow[4] = {0.f, 0.f, 0.f, 0.f}, LeverWant[4] = {0.f, 0.f, 0.f, 0.f}, Flow = 0.f, Litres = 0.f, Shown = -1.f;
	// the meter: started, stopped by its button, the preset or the dead man let go of; Given: what you pumped into this aircraft
	float Tank = 20000.f, Preset = 0.f, PresetT = 0.f, Given = 0.f, DeadT = 0.f, TicketT = 0.f, Pocket = 0.f; int32 PresetI = 0;
	bool bRun = false, bBack = false, bPress = false, bTimeOut = false, bPocket = false;   // (bBack: it counts fuel taken out)
	void Start() { bRun = true; Litres = 0.f; bBack = LeverNow[3] > 0.5f; if (Ticket == Put) Ticket = InPlace; }
	void Stop() { if (bRun && Litres >= 1.f && Ticket == InPlace) Print(); bRun = false; }
	void Print() { Ticket = Put; TicketT = 0.f; }
	bool bLive = true, bPump = false, bStop = false, bSeen = false; float YouStandX = -1e6f; double SeenAt = -10.;   // (you: a stop pressed, the aircraft you serve;  (bLive: its parts have moved or may move; bPump: the dead man pressed)
};
