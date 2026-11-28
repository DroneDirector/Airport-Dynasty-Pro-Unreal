// The runway crossing on the tower frequency (RunwayRadio.h): hangar step 6.
#include "RunwayRadio.h"
#include "CrewVoice.h"
#include "People.h"
#include "SimTraffic.h"
#include "Airfield.h"
#include "Site.h"
#include "ADPLog.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	const TCHAR* RrTowerVoice = TEXT("bf_emma");   // (the one controller: no crew member has this voice - CrewVoice.cpp VoiceFor)
	TWeakObjectPtr<UWorld> RrWorld; double RrFreqUntil = 0.0;   // (the frequency: busy until then)

	const TCHAR* RrPhon(TCHAR C)
	{
		static const TCHAR* Ph[] = {TEXT("Alpha"), TEXT("Bravo"), TEXT("Charlie"), TEXT("Delta"), TEXT("Echo"), TEXT("Foxtrot"), TEXT("Golf"), TEXT("Hotel"), TEXT("India"), TEXT("Juliett"), TEXT("Kilo"), TEXT("Lima"), TEXT("Mike"), TEXT("November"), TEXT("Oscar"), TEXT("Papa"), TEXT("Quebec"), TEXT("Romeo"), TEXT("Sierra"), TEXT("Tango"), TEXT("Uniform"), TEXT("Victor"), TEXT("Whiskey"), TEXT("X-ray"), TEXT("Yankee"), TEXT("Zulu")};
		static const TCHAR* Dg[] = {TEXT("zero"), TEXT("one"), TEXT("two"), TEXT("three"), TEXT("four"), TEXT("five"), TEXT("six"), TEXT("seven"), TEXT("eight"), TEXT("niner")};
		C = FChar::ToUpper(C);
		if (C >= 'A' && C <= 'Z') return Ph[C - 'A'];
		if (C >= '0' && C <= '9') return Dg[C - '0'];
		return nullptr;
	}

	double RrNow(UWorld* W) { return W ? W->GetTimeSeconds() : 0.0; }

	/** One line on the frequency: logged, heard (the driver where he sits, the tower from the radio), shown when you are near. */
	void RrLine(UWorld* W, ADPRunwayRadio::FCall& C, const ADPRunwayRadio::FWho& Who, bool bTower, const FString& Text)
	{
		const double Now = RrNow(W);
		ADP_LOG(TEXT("Radio: [%s] %s"), bTower ? TEXT("Tower") : *Who.Callsign, *Text);
		AActor* From = bTower ? Who.Radio : (Who.Driver ? Who.Driver : Who.Radio);
		if (From && !(Who.bYou && !bTower)) ADPCrewVoice::Say(From, Text, bTower ? FString(RrTowerVoice) : Who.Voice);   // (your own words are yours: shown, not voiced)
		bool bNear = Who.bYou;
		if (!bNear && W && Who.Radio)
			if (const APlayerController* PC = W->GetFirstPlayerController())
				if (PC->PlayerCameraManager) bNear = FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), Who.Radio->GetActorLocation()) < 2500.f;
		if (bNear)
			if (AADPPeople* Pp = AADPPeople::Get(W))
			{
				Pp->SubWho = bTower ? ADPRunwayRadio::Tower() : (Who.bYou ? FString(TEXT("You (")) + Who.Callsign + TEXT(")") : Who.Callsign.Left(1).ToUpper() + Who.Callsign.Mid(1));
				Pp->SubLine = Text; Pp->SubTone = AADPPeople::ETone::Calm; Pp->SubT = FMath::Clamp(1.5f + Text.Len() * 0.06f, 2.5f, 8.f);
			}
		int32 Words = 1; for (const TCHAR Ch : Text) Words += Ch == TEXT(' ');
		const double Dur = 0.8 + Words * 0.36;
		RrWorld = W; RrFreqUntil = Now + Dur;
		C.At = Now + Dur + FMath::FRandRange(0.6f, bTower ? 1.4f : 2.5f);   // (a moment before the answer)
	}
}

namespace ADPRunwayRadio
{
	bool FreqFree(UWorld* W) { return RrWorld.Get() != W || RrNow(W) >= RrFreqUntil; }

	FString Tower()
	{
		FString N = ADPSite::Name(ADPSite::Id());
		if (N.IsEmpty()) if (TSharedPtr<FJsonObject> Ab = ADPSite::AirportBlock()) Ab->TryGetStringField(TEXT("name"), N);
		N = N.Replace(TEXT(" Airport"), TEXT("")).Replace(TEXT("Airport "), TEXT(""));
		return (N.IsEmpty() ? FString(TEXT("Airport")) : N) + TEXT(" Tower");
	}

	FString Spell(const FString& Reg)
	{
		FString S;
		for (const TCHAR C : Reg) if (const TCHAR* P = RrPhon(C)) { if (!S.IsEmpty()) S += TEXT(" "); S += P; }
		return S;
	}

	FString TwyAt(const FVector2f& P)
	{
		const FAirfieldData& D = AAirfield::Shared();
		const FAirfieldData::FTwy* Best = nullptr; float Bd = 1e9f;
		for (const FAirfieldData::FTwy& T : D.Taxiways)
		{
			if (T.Id.IsEmpty() || T.Id.StartsWith(TEXT("CURVE"))) continue;
			for (int32 i = 1; i < T.Pts.Num(); ++i)
			{
				const FVector2f A = T.Pts[i - 1], Ab = T.Pts[i] - A; const float L2 = Ab.SizeSquared();
				const float K = L2 > 1e-4f ? FMath::Clamp(FVector2f::DotProduct(P - A, Ab) / L2, 0.f, 1.f) : 0.f;
				const float Dd = FVector2f::Distance(A + Ab * K, P);
				if (Dd < Bd) { Bd = Dd; Best = &T; }
			}
		}
		if (!Best) return FString();
		const TCHAR* L = RrPhon(Best->Id[0]);
		return (L ? FString(L) : Best->Id.Left(1)) + (Best->Id.Len() > 1 ? TEXT(" ") + Best->Id.Mid(1) : FString());
	}

	bool Hold(UWorld* W, FCall& C, const FWho& Who, bool bCall, bool bBusy, const FString& Traffic)
	{
		const double Now = RrNow(W);
		if (bBusy) C.FreeSince = -1.0; else if (C.FreeSince < 0.0) C.FreeSince = Now;
		if (C.Step == 5) return true;
		if (Now < C.At || !FreqFree(W)) return false;   // (a line is being said: the next one after it)
		const FString Rw = ASimTraffic::RunwaySpoken(W), Why = Traffic.IsEmpty() ? FString(TEXT("traffic on the runway")) : Traffic;
		const bool bFree = !bBusy && C.FreeSince >= 0.0 && Now - C.FreeSince >= 10.0;   // (free for 10 s: as the crews' rule before)
		switch (C.Step)
		{
		case 0:   // the request
			if (!bCall) return false;
			RrLine(W, C, Who, false, FString::Printf(TEXT("%s, %s%s, holding short runway %s%s, request crossing %s."), *Tower(), *Who.Callsign,
				Who.Towing.IsEmpty() ? TEXT("") : *(TEXT(" ") + Who.Towing), *Rw, Who.Twy.IsEmpty() ? TEXT("") : *(TEXT(" at ") + Who.Twy), *Who.To));
			C.Step = 1; return false;
		case 1:   // the tower's answer
		case 3:   // holding: the tower clears it when the runway has been free 10 s
			if (bFree)
			{
				RrLine(W, C, Who, true, FString::Printf(TEXT("%s, cross runway %s, report vacated."), *Who.Callsign, *Rw));
				C.Step = 4; return false;
			}
			if (C.Step == 1 && bBusy)   // (free but not yet 10 s: the clearance comes a moment later)
			{
				RrLine(W, C, Who, true, FString::Printf(TEXT("%s, hold short runway %s, %s."), *Who.Callsign, *Rw, *Why));
				C.Step = 2;
			}
			return false;
		case 2:   // the readback of the hold
			RrLine(W, C, Who, false, FString::Printf(TEXT("Holding short runway %s, %s."), *Rw, *Who.Callsign));
			C.Step = 3; return false;
		case 4:   // the readback of the crossing - unless the runway got busy in between: the clearance is cancelled
			if (bBusy)
			{
				RrLine(W, C, Who, true, FString::Printf(TEXT("%s, hold position, cancel crossing clearance, %s."), *Who.Callsign, *Why));
				ADP_WARN(TEXT("Radio: %s - the crossing clearance cancelled before it moved (%s)"), *Who.Callsign, *Why);
				C.Step = 2; return false;
			}
			RrLine(W, C, Who, false, FString::Printf(TEXT("Crossing runway %s, wilco, %s."), *Rw, *Who.Callsign));
			C.Step = 5; return false;   // (it moves after its own words: the next frame)
		default: return C.Step == 5;
		}
	}

	bool Exchange(UWorld* W, FCall& C, const FWho& Who, const FString& Ask, bool bGive, const FString& Give, const FString& HoldLine, const FString& HoldBack, const FString& Back)
	{
		if (C.Step == 5) return true;
		if (RrNow(W) < C.At || !FreqFree(W)) return false;   // (one line at a time on the frequency)
		switch (C.Step)
		{
		case 0:
			if (Ask.IsEmpty()) { C.Step = 1; return false; }
			RrLine(W, C, Who, false, Ask); C.Step = 1; return false;
		case 1:
		case 3:
			if (bGive) { RrLine(W, C, Who, true, Give); C.Step = 4; return false; }
			if (C.Step == 1 && !HoldLine.IsEmpty())
			{
				RrLine(W, C, Who, true, HoldLine); C.Step = HoldBack.IsEmpty() ? 3 : 2;
				ADP_LOG(TEXT("Radio: %s told to hold"), *Who.Callsign);
			}
			return false;
		case 2: RrLine(W, C, Who, false, HoldBack); C.Step = 3; return false;
		case 4: RrLine(W, C, Who, false, Back); C.Step = 5; return false;
		default: return C.Step == 5;
		}
	}

	void Vacated(UWorld* W, FCall& C, const FWho& Who)
	{
		if (C.Step != 5 && C.Step != 7) return;
		const double Now = RrNow(W);
		if (Now < C.At || !FreqFree(W)) return;
		if (C.Step == 5) { RrLine(W, C, Who, false, FString::Printf(TEXT("%s, %s, runway %s vacated."), *Tower(), *Who.Callsign, *ASimTraffic::RunwaySpoken(W))); C.Step = 7; return; }
		if (C.Step == 7) { RrLine(W, C, Who, true, FString::Printf(TEXT("%s, roger."), *Who.Callsign)); C.Step = 6; }
	}

	void Incursion(UWorld* W, FCall& C, const FWho& Who)
	{
		if (C.bIncursion || C.Step >= 5) return;
		C.bIncursion = true;
		ADP_WARN(TEXT("Radio: %s entered runway %s without a crossing clearance (runway incursion)"), *Who.Callsign, *ASimTraffic::RunwaySpoken(W));
		RrLine(W, C, Who, true, FString::Printf(TEXT("%s, you are on runway %s without clearance, vacate the runway immediately, report vacated."), *Who.Callsign, *ASimTraffic::RunwaySpoken(W)));
		C.Step = 5;   // (from here: off it, then "vacated")
	}
}
