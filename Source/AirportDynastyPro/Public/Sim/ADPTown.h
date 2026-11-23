// Your airport and its neighbours (to-do 197 / 199; owner, 2026-11-20 night - real-world rules).
//  197 - fire cover: an aerodrome's rescue and fire-fighting category (ICAO Annex 14, chapter 9) must match the largest
//    aircraft using it: category 6 an A320 (28-39 m long), 7 an A321, 8 a 787 / A330, 9 a 777, 10 an A380 - each with its
//    number of trucks (2, 2, 3, 3, 3), water (7,900 - 32,300 litres) and fire-fighters on duty. Your fire station's crew are
//    staff like any other (the 'fire' job, on 24-hour watches: about a quarter on duty at any time); with too few on duty,
//    aircraft above the category the station can cover may not take off or land here - they wait (or divert);
//  199 - the night curfew: Chablais starts closed 00:00-06:00 local for regular flights (as Geneva); night movements and loud
//    aircraft bring complaints from the towns round it; the town's mood goes up with a community fund, insulation of nearby
//    homes and compensation, and a council that is lobbied (time and money) may shorten the curfew if the mood is good;
//    or the SYNDIC is bribed: the curfew loosened at once - but every week a journalist or an audit may find it: a scandal,
//    fines, a court case, reputation lost and a longer curfew than before. Getting away with it is possible, never certain.
#pragma once

#include "CoreMinimal.h"
#include "Sim/ADPSim.h"

namespace ADPTown
{
	/** The rescue and fire-fighting category a type needs (ICAO). */
	int32 FireCategoryOf(const FString& TypeId);
	/** Firefighters to employ for your biggest aircraft (0: no fleet). */
	int32 FireCrewNeeded(const FADPWorld& W);
	/** The category your station covers now (crew on duty and trucks); 10 everywhere else. */
	int32 FireCategoryAt(const FADPWorld& W, const FString& Icao, const FDateTime& At);
	/** Our airport's curfew now (local hours closed from .. to; From == To: none) - read by the flights module. */
	void HereCurfew(double& From, double& To);
	/** Our airport's curfew taken from the career's town (called before every departure check). */
	void UseWorld(const FADPWorld& W);
	// the player's ways (the Overview, ue.adp.sim)
	bool CommunityFund(FADPWorld& W, double Usd, const FDateTime& At);
	bool Insulate(FADPWorld& W, int32 Homes, const FDateTime& At);
	bool Compensate(FADPWorld& W, double Usd, const FDateTime& At);
	bool Lobby(FADPWorld& W, const FDateTime& At, FString& Why);
	bool Bribe(FADPWorld& W, double Usd, const FDateTime& At, FString& Why);
	bool BuyFireTruck(FADPWorld& W, const FDateTime& At);
	/** The syndic's name (the town's mayor) - a person you meet. */
	FString SyndicName(const FADPWorld& W);
}

class AIRPORTDYNASTYPRO_API FADPTownModule : public IADPSimModule
{
public:
	virtual FName Name() const override { return TEXT("Town"); }
	virtual double Interval() const override { return 3600.0; }
	virtual FTimespan MaxCatchUp() const override { return FTimespan::FromDays(30.0); }
	virtual void Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp) override;
	FRandomStream Rng = FRandomStream(1999);
};
