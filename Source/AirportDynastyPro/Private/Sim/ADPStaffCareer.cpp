// People's careers, candidates and contracts (to-do 178-180; owner, 2026-11-20 night) - see Sim/ADPStaff.h.
//  - 178: first officers become captains (3,000 flight hours and a good rating: the usual airline command minimum, with a
//    10-day command course), cabin crew senior cabin crew after 2 years and pursers after 4; pay rises with rank; people
//    paid under the going rate ask for a raise, and those passed over for promotion lose heart (and may leave);
//  - 179: every candidate has flight hours, type ratings, languages, a home town and asks for a wage; an interview shows
//    their strengths and weak points; a paid job advert brings three better candidates;
//  - 180: full or part time (part time: 55% of the pay, at most 3 duty days in a row), the notice period by contract,
//    severance by years served, holidays asked for and approved or refused (unanswered after 7 days: approved).
#include "Sim/ADPStaff.h"
#include "Sim/ADPCrew.h"
#include "Sim/ADPEconomy.h"
#include "Sim/ADPWorldData.h"
#include "Site.h"

namespace
{
	const TCHAR* SCPick(FRandomStream& R, const TArray<const TCHAR*>& L) { return L[R.RandRange(0, L.Num() - 1)]; }
	void SCSay(FADPWorld& W, const FDateTime& At, const FString& Title, const FString& Text, int32 Prio = 1)
	{ FADPNotice N; N.At = At; N.Kind = TEXT("staff"); N.Title = Title; N.Text = Text; N.Priority = Prio; W.Notices.Add(N); }
	FADPPerson* SCFind(FADPWorld& W, const FString& Id) { return W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == Id; }); }
}

void ADPStaff::Describe(FADPPerson& P, const FString& Country, FRandomStream& Rng)
{
	const int32 Yrs = FMath::Max(0, P.Age - (P.Role == TEXT("captain") || P.Role == TEXT("fo") ? 21 : 19));
	if (P.Role == TEXT("captain")) P.HoursTotal = FMath::RoundToDouble(FMath::Clamp(3000.0 + Yrs * Rng.FRandRange(350.0, 650.0), 3000.0, 22000.0));
	else if (P.Role == TEXT("fo")) P.HoursTotal = FMath::RoundToDouble(FMath::Clamp(250.0 + Yrs * Rng.FRandRange(150.0, 420.0), 250.0, 6000.0));
	P.Rank = P.Role == TEXT("captain") ? TEXT("Captain") : P.Role == TEXT("fo") ? (P.HoursTotal >= 2500 ? TEXT("Senior first officer") : TEXT("First officer"))
		: P.Role == TEXT("fa") ? (Yrs >= 6 && Rng.FRand() < 0.4 ? TEXT("Purser") : Yrs >= 3 && Rng.FRand() < 0.5 ? TEXT("Senior cabin crew") : TEXT("Cabin crew"))
		: P.Role == TEXT("mech") ? (Rng.FRand() < 0.4 ? TEXT("B1 licensed engineer") : TEXT("Mechanic")) : FString();
	// languages: the country's own and English (flight crew always English - ICAO level 4 or better), now and then more
	P.Languages.Reset();
	if (Country == TEXT("CH")) { P.Languages.Add(TEXT("French")); if (Rng.FRand() < 0.55) P.Languages.Add(TEXT("German")); if (Rng.FRand() < 0.2) P.Languages.Add(TEXT("Italian")); }
	else if (Country == TEXT("IS")) P.Languages.Add(TEXT("Icelandic"));
	else if (Country == TEXT("FR")) P.Languages.Add(TEXT("French"));
	else if (Country == TEXT("DE") || Country == TEXT("AT")) P.Languages.Add(TEXT("German"));
	if (P.Role == TEXT("captain") || P.Role == TEXT("fo") || P.Role == TEXT("fa") || Rng.FRand() < 0.7) P.Languages.AddUnique(TEXT("English"));
	if (Rng.FRand() < 0.25) P.Languages.AddUnique(SCPick(Rng, {TEXT("Spanish"), TEXT("Italian"), TEXT("Portuguese"), TEXT("Danish"), TEXT("Dutch"), TEXT("Polish"), TEXT("Arabic"), TEXT("Mandarin")}));
	// home: a town round the airport (they live there and commute - to-do 116)
	if (Country == TEXT("CH")) P.Home = SCPick(Rng, {TEXT("Villeneuve"), TEXT("Montreux"), TEXT("Aigle"), TEXT("Monthey"), TEXT("Vevey"), TEXT("Roche"), TEXT("Noville"), TEXT("Rennaz"), TEXT("Bex"), TEXT("Saint-Maurice")});
	else if (Country == TEXT("IS")) P.Home = SCPick(Rng, {TEXT("Vík"), TEXT("Hvolsvöllur"), TEXT("Hella"), TEXT("Selfoss"), TEXT("Kirkjubæjarklaustur"), TEXT("Skógar")});
	// strengths and weak points (seen after an interview), from their rating
	static const TArray<const TCHAR*> Good = {TEXT("calm under pressure"), TEXT("excellent with passengers"), TEXT("precise with procedures"), TEXT("a natural leader"), TEXT("great team player"),
		TEXT("smooth hands on the controls"), TEXT("never late"), TEXT("quick learner"), TEXT("strong technical knowledge"), TEXT("speaks up about safety")};
	static const TArray<const TCHAR*> Weak = {TEXT("sometimes late"), TEXT("hard to manage"), TEXT("nervous in bad weather"), TEXT("careless with paperwork"), TEXT("short with passengers"),
		TEXT("slow to learn new types"), TEXT("often asks for leave"), TEXT("gossips about the company"), TEXT("a little overconfident"), TEXT("tires on long days")};
	P.Strengths = FString::Printf(TEXT("%s, %s"), SCPick(Rng, Good), SCPick(Rng, Good));
	P.Weaknesses = P.Rating >= 4.6 && Rng.FRand() < 0.6 ? FString(TEXT("none worth noting")) : FString(SCPick(Rng, Weak));
	// what they ask for: the going rate, more for better ratings and experience
	P.Asks = FMath::RoundToDouble(P.Salary * (0.96 + (P.Rating - 4.0) * 0.08) / 10.0) * 10.0;
	P.Salary = P.Asks;
	P.Contract = Rng.FRand() < 0.15 && P.Role != TEXT("captain") ? TEXT("part") : TEXT("full");
	if (P.Contract == TEXT("part")) { P.Salary = FMath::RoundToDouble(P.Salary * 0.55); P.Asks = P.Salary; }
	P.NoticeDays = P.Role == TEXT("captain") || P.Role == TEXT("fo") ? 90 : P.Contract == TEXT("part") ? 14 : 30;
}

bool ADPStaff::Interview(FADPWorld& W, const FString& PersonId)
{
	FADPPerson* P = SCFind(W, PersonId);
	if (!P) return false;
	P->bInterviewed = true;
	return true;
}

double ADPStaff::AdvertCost(const FString& Role) { return Role == TEXT("captain") ? 6000.0 : Role == TEXT("fo") ? 4000.0 : Role == TEXT("mech") ? 2500.0 : 1500.0; }

bool ADPStaff::Advert(FADPWorld& W, const FString& AirlineId, const FString& Role, const FDateTime& At, FRandomStream& Rng)
{
	const double Cost = AdvertCost(Role);
	const FADPAirline* A = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == AirlineId; });
	if (!A || A->Balance < Cost) return false;
	ADPEconomy::Pay(W, AirlineId, At, TEXT("marketing"), -Cost, FString::Printf(TEXT("Job advert: %s"), *ADPCrew::RoleName(Role)), FString());
	const FString Country = CountryOf(W, AirlineId);
	for (int32 i = 0; i < 3; ++i)
	{
		FADPPerson P = MakePerson(Role, Country, Rng);
		P.Rating = FMath::RoundToDouble(Rng.FRandRange(4.3, 5.0) * 10.0) / 10.0;
		P.HoursTotal *= 1.25;
		Describe(P, Country, Rng);   // (asks a little more, as better people do)
		W.People.Add(P);
	}
	SCSay(W, At, TEXT("Job advert"), FString::Printf(TEXT("Your advert for %s brought three strong candidates - see Crew."), *ADPCrew::RoleName(Role)), 0);
	return true;
}

bool ADPStaff::SetContract(FADPWorld& W, const FString& PersonId, const FString& Contract)
{
	FADPPerson* P = SCFind(W, PersonId);
	if (!P || P->AirlineId.IsEmpty() || (Contract != TEXT("full") && Contract != TEXT("part")) || P->Contract == Contract) return false;
	P->Salary = FMath::RoundToDouble(Contract == TEXT("part") ? P->Salary * 0.55 : P->Salary / 0.55);
	P->Contract = Contract; P->NoticeDays = P->Role == TEXT("captain") || P->Role == TEXT("fo") ? 90 : Contract == TEXT("part") ? 14 : 30;
	return true;
}

bool ADPStaff::AnswerLeave(FADPWorld& W, int32 Index, bool bYes, const FDateTime& At)
{
	if (!W.LeaveRequests.IsValidIndex(Index) || W.LeaveRequests[Index].State != TEXT("asked")) return false;
	FADPLeaveRequest& L = W.LeaveRequests[Index];
	L.State = bYes ? TEXT("approved") : TEXT("refused");
	if (FADPPerson* P = SCFind(W, L.PersonId)) P->Mood = FMath::Clamp(P->Mood + (bYes ? 6.0 : -10.0), 0.0, 100.0);
	return true;
}

void ADPStaff::CareerDay(FADPWorld& W, const FDateTime& To, FRandomStream& Rng)
{
	// per airline: how many captains and first officers it has (a promotion needs a place for a captain)
	TMap<FString, int32> Caps, Fos;
	for (const FADPPerson& P : W.People) if (!P.AirlineId.IsEmpty()) { if (P.Role == TEXT("captain")) ++Caps.FindOrAdd(P.AirlineId); else if (P.Role == TEXT("fo")) ++Fos.FindOrAdd(P.AirlineId); }
	for (FADPPerson& P : W.People)
	{
		if (P.AirlineId.IsEmpty() || P.Status == TEXT("dismissed")) continue;
		const FString Country = CountryOf(W, P.AirlineId);
		const double Going = GoingRate(P.Role, Country) * (P.Contract == TEXT("part") ? 0.55 : 1.0);
		const double Years = P.HiredAt != FDateTime() ? (To - P.HiredAt).GetTotalDays() / 365.0 : 0.0;
		// 178: first officer -> captain
		if (P.Role == TEXT("fo") && P.HoursTotal >= 3000.0 && P.Rating >= 3.8 && P.Training.IsEmpty())
		{
			if (Caps.FindRef(P.AirlineId) < Fos.FindRef(P.AirlineId))
			{
				P.Role = TEXT("captain"); P.Rank = TEXT("Captain"); --Fos.FindOrAdd(P.AirlineId); ++Caps.FindOrAdd(P.AirlineId);
				P.Salary = FMath::RoundToDouble(FMath::Max(P.Salary * 1.25, GoingRate(TEXT("captain"), Country) * (P.Contract == TEXT("part") ? 0.55 : 1.0)));
				P.Training = TEXT("Command upgrade"); P.TrainingUntil = To + FTimespan::FromDays(10.0); P.Status = TEXT("training");
				P.Mood = FMath::Min(100.0, P.Mood + 15.0); P.PassedOverSince = FDateTime();
				SCSay(W, To, TEXT("Promotion"), FString::Printf(TEXT("%s is promoted to captain (%.0f hours) - a 10-day command course first, pay now $%.0f a week."), *P.Name, P.HoursTotal, P.Salary));
			}
			else
			{
				if (P.PassedOverSince == FDateTime()) P.PassedOverSince = To;
				if ((To - P.PassedOverSince).GetTotalDays() > 60.0) P.Mood = FMath::Max(0.0, P.Mood - 0.6);
				if (FMath::IsNearlyEqual((To - P.PassedOverSince).GetTotalDays(), 60.0, 0.5))
					SCSay(W, To, TEXT("Passed over"), FString::Printf(TEXT("%s has been ready for command for two months with no captain's place - they are losing heart."), *P.Name));
			}
		}
		else if (P.Role == TEXT("fo") && P.HoursTotal >= 2500.0 && P.Rank == TEXT("First officer")) { P.Rank = TEXT("Senior first officer"); P.Salary = FMath::RoundToDouble(P.Salary * 1.06); }
		// 178: cabin crew -> senior cabin crew -> purser
		if (P.Role == TEXT("fa") && P.Rating >= 4.0)
		{
			if (Years >= 4.0 && P.Rank != TEXT("Purser")) { P.Rank = TEXT("Purser"); P.Salary = FMath::RoundToDouble(P.Salary * 1.08); SCSay(W, To, TEXT("Promotion"), FString::Printf(TEXT("%s is now a purser."), *P.Name), 0); }
			else if (Years >= 2.0 && P.Rank == TEXT("Cabin crew")) { P.Rank = TEXT("Senior cabin crew"); P.Salary = FMath::RoundToDouble(P.Salary * 1.07); SCSay(W, To, TEXT("Promotion"), FString::Printf(TEXT("%s is now senior cabin crew."), *P.Name), 0); }
		}
		// 178: a raise asked for (paid under the going rate and not happy; at most every two months)
		if (P.Salary < Going * 0.97 && P.Mood < 55.0 && (P.RaiseAskedAt == FDateTime() || (To - P.RaiseAskedAt).GetTotalDays() > 60.0) && Rng.FRand() < 0.08)
		{
			P.RaiseAskedAt = To;
			SCSay(W, To, TEXT("A raise asked for"), FString::Printf(TEXT("%s (%s) asks for a raise to $%.0f a week, the going rate. Set their pay under Crew, or they may hand in their notice."), *P.Name, *P.Role, FMath::RoundToDouble(Going / 10.0) * 10.0));
		}
		// 180: holidays asked for (about twice a year each), 5-14 days, two to six weeks ahead
		if (P.Status == TEXT("on") && Rng.FRand() < 0.0055)
		{
			FADPLeaveRequest L; L.PersonId = P.Id; L.Days = Rng.RandRange(5, 14); L.From = To + FTimespan::FromDays(Rng.RandRange(14, 42)); L.AskedAt = To;
			W.LeaveRequests.Add(L);
			SCSay(W, To, TEXT("Holiday request"), FString::Printf(TEXT("%s (%s) asks for %d days off from %s. Approve or refuse it under Crew (approved by itself after 7 days)."), *P.Name, *P.Role, L.Days, *L.From.ToString(TEXT("%d.%m.%Y"))));
		}
	}
	// 180: holidays - unanswered for 7 days: approved; approved ones begin and end
	for (FADPLeaveRequest& L : W.LeaveRequests)
	{
		FADPPerson* P = SCFind(W, L.PersonId);
		if (!P) { L.State = TEXT("taken"); continue; }
		if (L.State == TEXT("asked") && (To - L.AskedAt).GetTotalDays() >= 7.0) L.State = TEXT("approved");
		if (L.State == TEXT("approved") && L.From <= To && P->Status == TEXT("on"))
		{ P->Status = TEXT("leave"); P->RestUntil = L.From + FTimespan::FromDays(L.Days); L.State = TEXT("taken"); }
	}
	for (FADPPerson& P : W.People) if (P.Status == TEXT("leave") && P.RestUntil <= To) P.Status = TEXT("on");
	W.LeaveRequests.RemoveAll([&](const FADPLeaveRequest& L) { return L.State == TEXT("taken") || (L.State == TEXT("refused") && (To - L.AskedAt).GetTotalDays() > 30.0); });
}
