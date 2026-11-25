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
#include "Misc/ScopeLock.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"

namespace
{
	TMap<FString, FADPWx> GWxReports;            // by station
	TArray<TPair<FVector2D, FString>> GWxWhere;  // (lat, lon) of each station, for the nearest
	FDateTime GWxFetched; bool GWxStarted = false, GWxBusy = false;
	const TCHAR* MetarUrl = TEXT("https://aviationweather.gov/data/cache/metars.cache.csv.gz");

	const TCHAR* TafUrl = TEXT("https://aviationweather.gov/data/cache/tafs.cache.xml.gz");
	// for the menus (to-do 282; asked for on the browser's thread): each station's whole row, and its forecast
	FCriticalSection GWxLock; TMap<FString, FString> GWxRows, GWxTafs; bool GWxTafBusy = false;

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
		TMap<FString, FADPWx> R; TArray<TPair<FVector2D, FString>> Wh; TArray<FString> C; TMap<FString, FString> Rows;
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
			R.Add(W.Station, W); Rows.Add(W.Station, Lines[i]);
			Wh.Add(TPair<FVector2D, FString>(FVector2D(D(3, 0), D(4, 0)), W.Station));
		}
		{ FScopeLock Lk(&GWxLock); GWxRows = MoveTemp(Rows); }
		AsyncTask(ENamedThreads::GameThread, [R = MoveTemp(R), Wh = MoveTemp(Wh)]() mutable
		{ GWxReports = MoveTemp(R); GWxWhere = MoveTemp(Wh); GWxFetched = FDateTime::UtcNow(); ADP_LOG(TEXT("Weather: %d live airport reports"), GWxReports.Num()); });
	}

	// (owner, 2026-11-20: when the connection drops, the weather stays as it last was - so the last reports are kept, on disk
	// too, and a game started offline uses them; the built-in climate only before the first ever fetch)
	FString CacheFile() { return FPaths::ProjectSavedDir() / TEXT("Weather") / TEXT("last-metars.csv.gz"); }

	// the forecasts (TAF): the raw text and the station of each
	void ParseTafs(const FString& T)
	{
		TMap<FString, FString> R; int32 At = 0;
		const FString A0 = TEXT("<raw_text>"), A1 = TEXT("</raw_text>"), B0 = TEXT("<station_id>"), B1 = TEXT("</station_id>");
		while ((At = T.Find(A0, ESearchCase::CaseSensitive, ESearchDir::FromStart, At)) != INDEX_NONE)
		{
			const int32 E = T.Find(A1, ESearchCase::CaseSensitive, ESearchDir::FromStart, At), S = E < 0 ? -1 : T.Find(B0, ESearchCase::CaseSensitive, ESearchDir::FromStart, E);
			const int32 Se = S < 0 ? -1 : T.Find(B1, ESearchCase::CaseSensitive, ESearchDir::FromStart, S);
			if (Se < 0) break;
			FString Raw = T.Mid(At + A0.Len(), E - At - A0.Len()); Raw.RemoveFromStart(TEXT("<![CDATA[")); Raw.RemoveFromEnd(TEXT("]]>"));
			const FString Id = T.Mid(S + B0.Len(), Se - S - B0.Len());
			if (!R.Contains(Id)) R.Add(Id, Raw);
			At = Se;
		}
		ADP_LOG(TEXT("Weather: %d forecasts"), R.Num());
		FScopeLock Lk(&GWxLock); GWxTafs = MoveTemp(R);
	}

	void Unpack(TArray<uint8> Gz, bool bFromDisk, bool bTafs = false)
	{
		Async(EAsyncExecution::ThreadPool, [Gz = MoveTemp(Gz), bFromDisk, bTafs]()
		{
			if (Gz.Num() < 18) return;
			const int32 Size = int32(Gz[Gz.Num() - 4] | (Gz[Gz.Num() - 3] << 8) | (Gz[Gz.Num() - 2] << 16) | (Gz[Gz.Num() - 1] << 24));
			TArray<uint8> Raw; Raw.SetNumUninitialized(FMath::Clamp(Size, 1, 64 * 1024 * 1024));
			if (!FCompression::UncompressMemory(NAME_Gzip, Raw.GetData(), Raw.Num(), Gz.GetData(), Gz.Num())) { ADP_WARN(TEXT("Weather: the reports couldn't be unpacked")); return; }
			Raw.Add(0);
			if (bTafs) { ParseTafs(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Raw.GetData()))); return; }
			Parse(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Raw.GetData())));
			if (bFromDisk) ADP_LOG(TEXT("Weather: the last known reports read from disk (until the live ones come)"));
		});
	}

	void Fetch()
	{
		if (GWxBusy) return; GWxBusy = true;
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Q = FHttpModule::Get().CreateRequest();
		Q->SetURL(MetarUrl); Q->SetVerb(TEXT("GET")); Q->SetHeader(TEXT("User-Agent"), TEXT("game-weather"));
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

	// (the pages' forecasts; without them the pages show the report alone)
	void FetchTafs()
	{
		if (GWxTafBusy) return; GWxTafBusy = true;
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Q = FHttpModule::Get().CreateRequest();
		Q->SetURL(TafUrl); Q->SetVerb(TEXT("GET")); Q->SetHeader(TEXT("User-Agent"), TEXT("game-weather"));
		Q->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
		{
			GWxTafBusy = false;
			if (!bOk || !Res.IsValid() || Res->GetResponseCode() != 200) { ADP_LOG(TEXT("Weather: no forecasts this time - those there stay")); return; }
			TArray<uint8> Gz = Res->GetContent(); Unpack(MoveTemp(Gz), false, true);
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
	Fetch(); FetchTafs();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { Fetch(); FetchTafs(); return true; }), 900.0f);   // (every 15 minutes, as the owner asked)
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

bool ADPWeather::PageJson(const FString& Url, FString& Out)
{
	const bool bTaf = Url.Contains(TEXT("/api/data/taf?")), bMetar = Url.Contains(TEXT("/api/data/metar?"));
	if (!bTaf && !bMetar) return false;
	auto Arg = [&Url](const TCHAR* K)
	{
		const FString Key = FString(K) + TEXT("="); int32 At = Url.Find(TEXT("?") + Key); if (At < 0) At = Url.Find(TEXT("&") + Key);
		if (At < 0) return FString();
		FString V = Url.Mid(At + 1 + Key.Len()); int32 E; if (V.FindChar(TEXT('&'), E)) V.LeftInline(E);
		return FGenericPlatformHttp::UrlDecode(V);
	};
	TArray<FString> Ids, Bx; Arg(TEXT("ids")).ParseIntoArray(Ids, TEXT(",")); Arg(TEXT("bbox")).ParseIntoArray(Bx, TEXT(","));
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> J = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FScopeLock Lk(&GWxLock);
	if (bMetar && GWxRows.Num() == 0) return false;   // (nothing yet: the page asks again by itself)
	TArray<FString> C;
	auto Num = [&](const TCHAR* K, const FString& V, double Mul = 1.0) { if (!V.IsEmpty()) J->WriteValue(K, FCString::Atod(*V) * Mul); };
	auto Row = [&](const FString& L)
	{
		Split(L, C); if (C.Num() < 31) return;
		J->WriteObjectStart();
		J->WriteValue(TEXT("icaoId"), C[1]); J->WriteValue(TEXT("rawOb"), C[0]); J->WriteValue(TEXT("reportTime"), C[2]);
		Num(TEXT("lat"), C[3]); Num(TEXT("lon"), C[4]); Num(TEXT("temp"), C[5]); Num(TEXT("dewp"), C[6]);
		if (C[7].IsNumeric()) Num(TEXT("wdir"), C[7]); else J->WriteValue(TEXT("wdir"), FString(TEXT("VRB")));
		Num(TEXT("wspd"), C[8]); Num(TEXT("wgst"), C[9]);
		if (!C[10].IsEmpty()) J->WriteValue(TEXT("visib"), C[10]);
		Num(TEXT("altim"), C[11], 33.8639);   // (inches of mercury -> hPa)
		if (!C[21].IsEmpty()) J->WriteValue(TEXT("wxString"), C[21]);
		if (!C[30].IsEmpty()) J->WriteValue(TEXT("fltCat"), C[30]);
		if (C.Num() > 43) Num(TEXT("elev"), C[43]);
		J->WriteArrayStart(TEXT("clouds"));
		for (int32 k = 22; k + 1 < 30; k += 2)
		{
			if (C[k].IsEmpty()) continue;
			const bool bVv = C[k] == TEXT("OVX");   // (the sky hidden: how far one sees up)
			J->WriteObjectStart(); J->WriteValue(TEXT("cover"), bVv ? FString(TEXT("VV")) : C[k]);
			Num(TEXT("base"), bVv && C[k + 1].IsEmpty() && C.Num() > 41 ? C[41] : C[k + 1]); J->WriteObjectEnd();
		}
		J->WriteArrayEnd(); J->WriteObjectEnd();
	};
	J->WriteArrayStart();
	if (bTaf)
	{
		for (const FString& I : Ids)
			if (const FString* T = GWxTafs.Find(I)) { J->WriteObjectStart(); J->WriteValue(TEXT("icaoId"), I); J->WriteValue(TEXT("rawTAF"), *T); J->WriteObjectEnd(); }
	}
	else if (Bx.Num() == 4)
	{
		const double S = FCString::Atod(*Bx[0]), W = FCString::Atod(*Bx[1]), N = FCString::Atod(*Bx[2]), E = FCString::Atod(*Bx[3]); int32 n = 0;
		for (const auto& P : GWxRows)
		{
			Split(P.Value, C); if (C.Num() < 31 || C[3].IsEmpty()) continue;
			const double La = FCString::Atod(*C[3]), Lo = FCString::Atod(*C[4]);
			if (La >= S && La <= N && Lo >= W && Lo <= E && n++ < 400) Row(P.Value);
		}
	}
	else for (const FString& I : Ids) if (const FString* L = GWxRows.Find(I)) Row(*L);
	J->WriteArrayEnd(); J->Close();
	return true;
}
