// The airport's site (Site.h).
#include "Site.h"
#include "ADPLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString GSiteId;
	TMap<FString, TSharedPtr<FJsonObject>> GSiteInfo;

	FString SiteDir(const FString& Id) { return FPaths::ProjectContentDir() / TEXT("Data/Sites") / Id; }
	TSharedPtr<FJsonObject> ReadJson(const FString& Path)
	{
		FString T; TSharedPtr<FJsonObject> J;
		if (FFileHelper::LoadFileToString(T, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(T), J) && J.IsValid()) return J;
		return nullptr;
	}
	bool Known(const FString& Id) { return Id == ADPSite::DefaultId() || FPaths::FileExists(SiteDir(Id) / TEXT("site.json")); }
}

void ADPSite::Begin(const FString& MapOptions)
{
	FString Id;
	if (!FParse::Value(FCommandLine::Get(), TEXT("Site="), Id)) Id = UGameplayStatics::ParseOption(MapOptions, TEXT("Site"));
	if (Id.IsEmpty()) GConfig->GetString(TEXT("ADP.Site"), TEXT("Site"), Id, GGameUserSettingsIni);
	Id = Id.ToLower();
	if (!Known(Id)) Id = DefaultId();
	if (Id != GSiteId) ADP_LOG(TEXT("Site: %s (%s)"), *Id, *Name(Id));
	GSiteId = Id;
}

const FString& ADPSite::Id()
{
	if (GSiteId.IsEmpty()) Begin(FString());
	return GSiteId;
}

bool ADPSite::IsDefault() { return Id() == DefaultId(); }

TArray<FString> ADPSite::All()
{
	TArray<FString> Out = {DefaultId()};
	TArray<FString> Dirs;
	IFileManager::Get().FindFiles(Dirs, *(FPaths::ProjectContentDir() / TEXT("Data/Sites/*")), false, true);
	Dirs.Sort();
	for (const FString& D : Dirs) if (D != DefaultId() && Known(D.ToLower())) Out.Add(D.ToLower());
	return Out;
}

TSharedPtr<FJsonObject> ADPSite::Info(const FString& Id)
{
	if (Id == DefaultId()) return nullptr;
	if (TSharedPtr<FJsonObject>* J = GSiteInfo.Find(Id)) return *J;
	return GSiteInfo.Add(Id, ReadJson(SiteDir(Id) / TEXT("site.json")));
}

FString ADPSite::Name(const FString& Id)
{
	FString N;
	if (TSharedPtr<FJsonObject> I = Info(Id)) { const TSharedPtr<FJsonObject>* Ap; if (I->TryGetObjectField(TEXT("airport"), Ap)) (*Ap)->TryGetStringField(TEXT("name"), N); }
	else if (Id == DefaultId())
	{
		if (TSharedPtr<FJsonObject> A = ReadJson(FPaths::ProjectContentDir() / TEXT("Data/airfield.json"))) { const TSharedPtr<FJsonObject>* Ap; if (A->TryGetObjectField(TEXT("airport"), Ap)) (*Ap)->TryGetStringField(TEXT("name"), N); }
	}
	return N.IsEmpty() ? Id : N;
}

FString ADPSite::Place(const FString& Id)
{
	FString P = TEXT("Iceland");
	if (TSharedPtr<FJsonObject> I = Info(Id)) I->TryGetStringField(TEXT("place"), P);
	return P;
}

void ADPSite::Choose(const FString& Id)
{
	if (!Known(Id.ToLower())) return;
	GConfig->SetString(TEXT("ADP.Site"), TEXT("Site"), *Id.ToLower(), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

FString ADPSite::File(const FString& Name)
{
	if (!IsDefault()) return SiteDir(Id()) / Name;
	const FString C = FPaths::ProjectContentDir() / TEXT("Data") / Name;
	if (FPaths::FileExists(C) || FPaths::DirectoryExists(C)) return C;
	return FPaths::ProjectDir() / TEXT("Data") / Name;
}

TSharedPtr<FJsonObject> ADPSite::AirportBlock()
{
	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	for (const FString& P : {FPaths::ProjectContentDir() / TEXT("Data/airfield.json"), FPaths::ProjectDir() / TEXT("Data/airfield.json")})
		if (TSharedPtr<FJsonObject> A = ReadJson(P))
		{
			const TSharedPtr<FJsonObject>* Ap;
			if (A->TryGetObjectField(TEXT("airport"), Ap)) for (const auto& KV : (*Ap)->Values) Out->SetField(KV.Key, KV.Value);
			break;
		}
	if (TSharedPtr<FJsonObject> I = Info(Id()))
	{
		const TSharedPtr<FJsonObject>* Ap;
		if (I->TryGetObjectField(TEXT("airport"), Ap)) for (const auto& KV : (*Ap)->Values) Out->SetField(KV.Key, KV.Value);
	}
	return Out;
}

double ADPSite::UtcOffsetHours(const FDateTime& Utc)
{
	TSharedPtr<FJsonObject> I = Info(Id());
	if (!I) return 0.0;   // (Iceland: UTC all year)
	double Std = 0.0; I->TryGetNumberField(TEXT("utcOffset"), Std);
	bool bEuSummer = false; I->TryGetBoolField(TEXT("euSummerTime"), bEuSummer);
	if (!bEuSummer) return Std;
	// EU summer time: from 01:00 UTC on the last Sunday of March to 01:00 UTC on the last Sunday of October
	auto LastSunday = [&](int32 Month)
	{
		FDateTime D(Utc.GetYear(), Month, FDateTime::DaysInMonth(Utc.GetYear(), Month), 1, 0, 0);
		while (D.GetDayOfWeek() != EDayOfWeek::Sunday) D -= FTimespan::FromDays(1.0);
		return D;
	};
	return (Utc >= LastSunday(3) && Utc < LastSunday(10)) ? Std + 1.0 : Std;
}

void ADPSite::Destinations(TArray<FString>& Names, TArray<FString>& Codes)
{
	Names = {TEXT("Reykjavík"), TEXT("Copenhagen"), TEXT("Oslo"), TEXT("London"), TEXT("Vágar"), TEXT("Amsterdam"), TEXT("Frankfurt"), TEXT("Stockholm"), TEXT("Bergen"), TEXT("Edinburgh"), TEXT("Paris"), TEXT("Helsinki")};
	Codes = {TEXT("KEF"), TEXT("CPH"), TEXT("OSL"), TEXT("LHR"), TEXT("FAE"), TEXT("AMS"), TEXT("FRA"), TEXT("ARN"), TEXT("BGO"), TEXT("EDI"), TEXT("CDG"), TEXT("HEL")};
	const TArray<TSharedPtr<FJsonValue>>* D;
	if (TSharedPtr<FJsonObject> I = Info(Id())) if (I->TryGetArrayField(TEXT("destinations"), D) && D->Num() >= 8)
	{
		Names.Reset(); Codes.Reset();
		for (const TSharedPtr<FJsonValue>& V : *D) { const TArray<TSharedPtr<FJsonValue>>& P = V->AsArray(); if (P.Num() == 2) { Names.Add(P[0]->AsString()); Codes.Add(P[1]->AsString()); } }
	}
}

FDateTime ADPSite::LocalNow()
{
	const FDateTime U = FDateTime::UtcNow();
	return U + FTimespan::FromHours(UtcOffsetHours(U));
}
