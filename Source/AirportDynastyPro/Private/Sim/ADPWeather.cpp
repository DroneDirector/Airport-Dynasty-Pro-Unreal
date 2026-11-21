// The real world's weather - see Sim/ADPWeather.h.
#include "Sim/ADPWeather.h"
#include "ADPLog.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Compression.h"
#include "Containers/Ticker.h"
#include "Async/Async.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	TMap<FString, FADPWx> GWxReports;            // by station
	TArray<TPair<FVector2D, FString>> GWxWhere;  // (lat, lon) of each station, for the nearest
	FDateTime GWxFetched; bool GWxStarted = false, GWxBusy = false;
	const TCHAR* MetarUrl = TEXT("https://aviationweather.gov/data/cache/metars.cache.csv.gz");

	double Dist(double La1, double Lo1, double La2, double Lo2)
	{
		const double P1 = FMath::DegreesToRadians(La1), P2 = FMath::DegreesToRadians(La2), DL = FMath::DegreesToRadians(Lo2 - Lo1);
		const double A = FMath::Square(FMath::Sin((P2 - P1) / 2)) + FMath::Cos(P1) * FMath::Cos(P2) * FMath::Square(FMath::Sin(DL / 2));
		return 2.0 * 3440.065 * FMath::Atan2(FMath::Sqrt(A), FMath::Sqrt(1 - A));
	}

	// one CSV row (only the first field, the raw report, is quoted)
	void Split(const FString& L, TArray<FString>& Out)
	{
		Out.Reset(); FString Cur; bool bQ = false;
		for (TCHAR Ch : L) { if (Ch == '"') bQ = !bQ; else if (Ch == ',' && !bQ) { Out.Add(Cur); Cur.Reset(); } else Cur.AppendChar(Ch); }
		Out.Add(Cur);
	}

	void Parse(const FString& Text)
	{
		TArray<FString> Lines; Text.ParseIntoArrayLines(Lines);
		TMap<FString, FADPWx> R; TArray<TPair<FVector2D, FString>> Wh; TArray<FString> C;
		for (int32 i = 1; i < Lines.Num(); ++i)
		{
			Split(Lines[i], C); if (C.Num() < 31 || C[1].IsEmpty()) continue;
			FADPWx W; W.bLive = true; W.Station = C[1]; W.Raw = C[0];
			auto D = [&](int32 k, double Def) { return C[k].IsEmpty() ? Def : FCString::Atod(*C[k].Replace(TEXT("+"), TEXT(""))); };
			W.TempC = D(5, 15); W.WindDir = D(7, 0); W.WindKt = D(8, 0); W.GustKt = D(9, 0);
			W.VisM = D(10, 10) * 1609.34;
			for (int32 k = 22; k + 1 < 30; k += 2)
				if ((C[k] == TEXT("BKN") || C[k] == TEXT("OVC") || C[k] == TEXT("OVX")) && !C[k + 1].IsEmpty()) { W.CeilingFt = FMath::Min(W.CeilingFt, FCString::Atod(*C[k + 1])); }
			if (C.Num() > 41 && !C[41].IsEmpty()) W.CeilingFt = FMath::Min(W.CeilingFt, FCString::Atod(*C[41]));   // (vertical visibility: fog)
			const FString& Wx = C[21];
			W.bStorm = Wx.Contains(TEXT("TS")); W.bSnow = Wx.Contains(TEXT("SN")) || Wx.Contains(TEXT("SG")) || Wx.Contains(TEXT("PL"));
			W.bFreezing = Wx.Contains(TEXT("FZ")); W.bFog = Wx.Contains(TEXT("FG")); W.bRain = Wx.Contains(TEXT("RA")) || Wx.Contains(TEXT("DZ"));
			R.Add(W.Station, W);
			Wh.Add(TPair<FVector2D, FString>(FVector2D(D(3, 0), D(4, 0)), W.Station));
		}
		AsyncTask(ENamedThreads::GameThread, [R = MoveTemp(R), Wh = MoveTemp(Wh)]() mutable
		{ GWxReports = MoveTemp(R); GWxWhere = MoveTemp(Wh); GWxFetched = FDateTime::UtcNow(); ADP_LOG(TEXT("Weather: %d live airport reports"), GWxReports.Num()); });
	}

	// (owner, 2026-11-20: when the connection drops, the weather stays as it last was - so the last reports are kept, on disk
	// too, and a game started offline uses them; the built-in climate only before the first ever fetch)
	FString CacheFile() { return FPaths::ProjectSavedDir() / TEXT("Weather") / TEXT("last-metars.csv.gz"); }

	void Unpack(TArray<uint8> Gz, bool bFromDisk)
	{
		Async(EAsyncExecution::ThreadPool, [Gz = MoveTemp(Gz), bFromDisk]()
		{
			if (Gz.Num() < 18) return;
			const int32 Size = int32(Gz[Gz.Num() - 4] | (Gz[Gz.Num() - 3] << 8) | (Gz[Gz.Num() - 2] << 16) | (Gz[Gz.Num() - 1] << 24));
			TArray<uint8> Raw; Raw.SetNumUninitialized(FMath::Clamp(Size, 1, 64 * 1024 * 1024));
			if (!FCompression::UncompressMemory(NAME_Gzip, Raw.GetData(), Raw.Num(), Gz.GetData(), Gz.Num())) { ADP_WARN(TEXT("Weather: the reports couldn't be unpacked")); return; }
			Raw.Add(0);
			Parse(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Raw.GetData())));
			if (bFromDisk) ADP_LOG(TEXT("Weather: the last known reports read from disk (until the live ones come)"));
		});
	}

	void Fetch()
	{
		if (GWxBusy) return; GWxBusy = true;
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Q = FHttpModule::Get().CreateRequest();
		Q->SetURL(MetarUrl); Q->SetVerb(TEXT("GET")); Q->SetHeader(TEXT("User-Agent"), TEXT("AirportDynastyPro-Weather"));
		Q->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
		{
			GWxBusy = false;
			if (!bOk || !Res.IsValid() || Res->GetResponseCode() != 200) { ADP_WARN(TEXT("Weather: no connection - the weather stays as it last was (%d reports)"), GWxReports.Num()); return; }
			TArray<uint8> Gz = Res->GetContent();
			FFileHelper::SaveArrayToFile(Gz, *CacheFile());   // (kept for when the connection drops or the game starts offline)
			Unpack(MoveTemp(Gz), false);
		});
		Q->ProcessRequest();
	}

	// the built-in climate: typical weather for the latitude, season and hour, the same for every player at the same time
	FADPWx Climate(const FString& Icao, double Lat, double Lon, const FDateTime& When)
	{
		FADPWx W; W.Station = Icao;
		const int32 Day = When.GetDayOfYear(); const double AbsLat = FMath::Abs(Lat);
		const double Winter = FMath::Cos((Day - (Lat >= 0 ? 15 : 196)) / 365.25 * 2 * PI);   // 1 midwinter .. -1 midsummer
		FRandomStream R(int32(GetTypeHash(Icao) ^ (When.GetYear() * 1000 + Day) * 31 ^ When.GetHour()));
		W.TempC = 28.0 - AbsLat * 0.45 - Winter * AbsLat * 0.25 + R.FRandRange(-4, 4) + 4.0 * FMath::Sin((When.GetHour() - 9) / 24.0 * 2 * PI);
		W.WindDir = R.FRandRange(0, 360); W.WindKt = FMath::Max(0.0, 6.0 + AbsLat * 0.12 + Winter * 3.0 + R.FRandRange(-5, 8));
		W.GustKt = R.FRand() < 0.25 ? W.WindKt + R.FRandRange(6, 14) : 0.0;
		const double Wet = 0.15 + AbsLat / 300.0 + FMath::Max(0.0, Winter) * 0.1;
		if (R.FRand() < Wet) { W.bRain = W.TempC > 1; W.bSnow = W.TempC <= 1; W.CeilingFt = R.FRandRange(600, 3000); W.VisM = R.FRandRange(2000, 8000); }
		if (R.FRand() < 0.04) { W.bFog = true; W.VisM = R.FRandRange(150, 800); W.CeilingFt = R.FRandRange(100, 300); }
		if (R.FRand() < (AbsLat < 30 ? 0.06 : 0.015) * (1.0 - Winter * 0.5)) W.bStorm = true;
		W.bFreezing = W.TempC < 0 && W.bRain;
		return W;
	}
}

void ADPWeather::Start()
{
	if (GWxStarted) return; GWxStarted = true;
	{ TArray<uint8> Gz; if (FFileHelper::LoadFileToArray(Gz, *CacheFile(), FILEREAD_Silent)) Unpack(MoveTemp(Gz), true); }
	Fetch();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { Fetch(); return true; }), 900.0f);   // (every 15 minutes, as the owner asked)
}

FADPWx ADPWeather::At(const FString& Icao, double Lat, double Lon, const FDateTime& When)
{
	// the real reports for "now" - the latest we have, however old if the connection dropped (the weather as it last was);
	// times far from now (the catch-up of days the game was closed) use the climate
	const bool bNow = FMath::Abs((FDateTime::UtcNow() - When).GetTotalHours()) < 3.0 && GWxReports.Num() > 0;
	if (bNow)
	{
		if (const FADPWx* W = GWxReports.Find(Icao)) return *W;
		double Best = 60.0; const FString* Near = nullptr;
		for (const auto& P : GWxWhere) { const double D = Dist(Lat, Lon, P.Key.X, P.Key.Y); if (D < Best) { Best = D; Near = &P.Value; } }
		if (Near) if (const FADPWx* W = GWxReports.Find(*Near)) return *W;
	}
	return Climate(Icao, Lat, Lon, When);
}

int32 ADPWeather::NumReports() { return GWxReports.Num(); }
FDateTime ADPWeather::FetchedAt() { return GWxFetched; }

void ADPWeather::Components(const FADPWx& W, double RunwayHdg, double& Cross, double& Tail)
{
	const double Wind = FMath::Max(W.WindKt, W.GustKt), A = FMath::DegreesToRadians(W.WindDir - RunwayHdg);
	Cross = FMath::Abs(Wind * FMath::Sin(A)); Tail = -Wind * FMath::Cos(A);
}
