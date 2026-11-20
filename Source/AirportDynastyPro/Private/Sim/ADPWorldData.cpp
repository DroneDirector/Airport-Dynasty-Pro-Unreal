// The airports, the aircraft types and flight profiles - see Sim/ADPWorldData.h.
#include "Sim/ADPWorldData.h"
#include "ADPLog.h"
#include "Async/Async.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	TArray<FADPAirportInfo> GAirports;
	TMap<FString, int32> GByCode;
	TArray<FADPTypeInfo> GTypes;
	TAtomic<bool> GReady(false), GStarted(false);

	FString SimDataFile(const TCHAR* Name)
	{
		const FString A = FPaths::ProjectContentDir() / TEXT("Data") / Name;   // (packaged: Content/Data)
		return FPaths::FileExists(A) ? A : FPaths::ProjectDir() / TEXT("Data") / Name;
	}
	double Num(const TSharedPtr<FJsonValue>& V) { return V.IsValid() && V->Type == EJson::Number ? V->AsNumber() : 0.0; }
	FString Str(const TSharedPtr<FJsonValue>& V) { return V.IsValid() && V->Type == EJson::String ? V->AsString() : FString(); }

	void LoadNow()
	{
		const double T0 = FPlatformTime::Seconds();
		TArray<FADPAirportInfo> Aps; TMap<FString, int32> By; TArray<FADPTypeInfo> Tys;
		FString Text; TSharedPtr<FJsonObject> J;
		// the airports: plain lines (A = an airport, R = a runway of the airport above), tab-separated
		TArray<FString> Lines;
		if (FFileHelper::LoadFileToStringArray(Lines, *SimDataFile(TEXT("world-airports.tsv"))))
		{
			Aps.Reserve(Lines.Num() * 2 / 3);
			TArray<FString> C;
			auto D = [&](int32 i) { return C.IsValidIndex(i) && !C[i].IsEmpty() ? FCString::Atod(*C[i]) : 0.0; };
			for (const FString& Line : Lines)
			{
				C.Reset(); Line.ParseIntoArray(C, TEXT("	"), false);
				if (C.Num() >= 12 && C[0] == TEXT("A"))
				{
					FADPAirportInfo& P = Aps.AddDefaulted_GetRef();
					P.Ident = C[1]; P.Icao = C[2]; P.Iata = C[3]; P.Name = C[4]; P.Town = C[5]; P.Country = C[6]; P.Kind = C[7].Len() ? C[7][0] : 'S';
					P.Lat = D(8); P.Lon = D(9); P.ElevFt = int32(D(10)); P.bScheduled = C[11] == TEXT("1");
					const int32 I = Aps.Num() - 1;
					By.Add(P.Ident, I);
					if (!P.Icao.IsEmpty() && !By.Contains(P.Icao)) By.Add(P.Icao, I);
					if (!P.Iata.IsEmpty() && !By.Contains(P.Iata) && (P.Kind == 'L' || P.Kind == 'M')) By.Add(P.Iata, I);
				}
				else if (C.Num() >= 12 && C[0] == TEXT("R") && Aps.Num())
				{
					FADPRunwayInfo& W = Aps.Last().Runways.AddDefaulted_GetRef();
					W.Le = C[1]; W.He = C[2]; W.LengthFt = int32(D(3)); W.WidthFt = int32(D(4)); W.Surface = C[5].Len() ? C[5][0] : 'O'; W.bLit = C[6] == TEXT("1");
					W.bEnds = !C[7].IsEmpty() && !C[9].IsEmpty(); W.LeLat = D(7); W.LeLon = D(8); W.HeLat = D(9); W.HeLon = D(10); W.LeHdg = D(11);
				}
			}
		}
		if (FFileHelper::LoadFileToString(Text, *SimDataFile(TEXT("aircraft-types.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) && J)
			for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("types")))
			{
				const TSharedPtr<FJsonObject> O = V->AsObject(); FADPTypeInfo T;
				T.Id = O->GetStringField(TEXT("id")); T.Name = O->GetStringField(TEXT("name")); T.Class = O->GetStringField(TEXT("class")); T.Model = O->GetStringField(TEXT("model"));
				T.Seats = O->GetIntegerField(TEXT("seats")); T.Engines = O->GetIntegerField(TEXT("engines"));
				auto D = [&](const TCHAR* K) { double X = 0; O->TryGetNumberField(K, X); return X; };
				T.Mtow = D(TEXT("mtow")); T.Mlw = D(TEXT("mlw")); T.CruiseKt = D(TEXT("cruiseKt")); T.CruiseMach = D(TEXT("cruiseMach")); T.CeilingFt = D(TEXT("ceilingFt"));
				T.RangeNm = D(TEXT("rangeNm")); T.ClimbFpm = D(TEXT("climbFpm")); T.DescentFpm = D(TEXT("descentFpm")); T.V2Kt = D(TEXT("v2Kt")); T.VappKt = D(TEXT("vappKt"));
				T.BurnKgH = D(TEXT("burnKgH")); T.ToRunwayM = D(TEXT("toRunwayM")); T.LdRunwayM = D(TEXT("ldRunwayM")); T.TaxiKt = D(TEXT("taxiKt"));
				T.MaxBankDeg = D(TEXT("maxBankDeg")); T.ValueM = D(TEXT("valueM")); T.TurnDegS = D(TEXT("turnDegS"));
				Tys.Add(T);
			}
		AsyncTask(ENamedThreads::GameThread, [Aps = MoveTemp(Aps), By = MoveTemp(By), Tys = MoveTemp(Tys), T0]() mutable
		{
			GAirports = MoveTemp(Aps); GByCode = MoveTemp(By); GTypes = MoveTemp(Tys); GReady = true;
			ADP_LOG(TEXT("World data: %d airports, %d aircraft types (%.2f s on a background thread)"), GAirports.Num(), GTypes.Num(), FPlatformTime::Seconds() - T0);
		});
	}
}

void ADPWorldData::Load() { if (GStarted.Exchange(true)) return; Async(EAsyncExecution::Thread, [] { LoadNow(); }); }
bool ADPWorldData::IsReady() { return GReady; }
int32 ADPWorldData::NumAirports() { return GAirports.Num(); }
const FADPAirportInfo* ADPWorldData::Airport(const FString& Code) { const int32* I = GByCode.Find(Code.ToUpper()); return I ? &GAirports[*I] : nullptr; }
void ADPWorldData::ForEachAirport(TFunctionRef<void(const FADPAirportInfo&)> F) { for (const FADPAirportInfo& A : GAirports) F(A); }
const TArray<FADPTypeInfo>& ADPWorldData::Types() { return GTypes; }
const FADPTypeInfo* ADPWorldData::Type(const FString& Id) { return GTypes.FindByPredicate([&](const FADPTypeInfo& T) { return T.Id == Id; }); }

double ADPWorldData::DistanceNm(double Lat1, double Lon1, double Lat2, double Lon2)
{
	const double P1 = FMath::DegreesToRadians(Lat1), P2 = FMath::DegreesToRadians(Lat2), DP = P2 - P1, DL = FMath::DegreesToRadians(Lon2 - Lon1);
	const double A = FMath::Square(FMath::Sin(DP / 2)) + FMath::Cos(P1) * FMath::Cos(P2) * FMath::Square(FMath::Sin(DL / 2));
	return 2.0 * 3440.065 * FMath::Atan2(FMath::Sqrt(A), FMath::Sqrt(1 - A));
}

double ADPWorldData::BearingDeg(double Lat1, double Lon1, double Lat2, double Lon2)
{
	const double P1 = FMath::DegreesToRadians(Lat1), P2 = FMath::DegreesToRadians(Lat2), DL = FMath::DegreesToRadians(Lon2 - Lon1);
	const double B = FMath::RadiansToDegrees(FMath::Atan2(FMath::Sin(DL) * FMath::Cos(P2), FMath::Cos(P1) * FMath::Sin(P2) - FMath::Sin(P1) * FMath::Cos(P2) * FMath::Cos(DL)));
	return FMath::Fmod(B + 360.0, 360.0);
}

void ADPWorldData::Along(double Lat1, double Lon1, double Lat2, double Lon2, double F, double& Lat, double& Lon)
{
	const double P1 = FMath::DegreesToRadians(Lat1), L1 = FMath::DegreesToRadians(Lon1), P2 = FMath::DegreesToRadians(Lat2), L2 = FMath::DegreesToRadians(Lon2);
	const double D = DistanceNm(Lat1, Lon1, Lat2, Lon2) / 3440.065;
	if (D < 1e-9) { Lat = Lat1; Lon = Lon1; return; }
	const double A = FMath::Sin((1 - F) * D) / FMath::Sin(D), B = FMath::Sin(F * D) / FMath::Sin(D);
	const double X = A * FMath::Cos(P1) * FMath::Cos(L1) + B * FMath::Cos(P2) * FMath::Cos(L2), Y = A * FMath::Cos(P1) * FMath::Sin(L1) + B * FMath::Cos(P2) * FMath::Sin(L2), Z = A * FMath::Sin(P1) + B * FMath::Sin(P2);
	Lat = FMath::RadiansToDegrees(FMath::Atan2(Z, FMath::Sqrt(X * X + Y * Y))); Lon = FMath::RadiansToDegrees(FMath::Atan2(Y, X));
}

bool ADPWorldData::MakeProfile(const FString& TypeId, const FString& From, const FString& To, FADPProfile& P)
{
	const FADPTypeInfo* T = Type(TypeId); const FADPAirportInfo* A = Airport(From); const FADPAirportInfo* B = Airport(To);
	if (!T || !A || !B) return false;
	P = FADPProfile(); P.Type = *T;
	P.FromLat = A->Lat; P.FromLon = A->Lon; P.ToLat = B->Lat; P.ToLon = B->Lon; P.FromElevFt = A->ElevFt; P.ToElevFt = B->ElevFt;
	P.DistNm = DistanceNm(A->Lat, A->Lon, B->Lat, B->Lon) * 1.04;   // (airways and procedures: a little longer than the great circle)
	// taxi: longer at big airports
	const double TaxiOut = A->Kind == 'L' ? 14 : A->Kind == 'M' ? 9 : 5, TaxiIn = B->Kind == 'L' ? 9 : B->Kind == 'M' ? 6 : 3;
	// the cruise level: lower on short legs (no point climbing high for 15 minutes), the type's ceiling less 4,000 ft at most
	const double Top = FMath::Max(8000.0, T->CeilingFt - 4000.0);
	const bool bProp = T->Class == TEXT("turboprop") || T->Class == TEXT("bush") || T->Class == TEXT("helicopter");
	P.CruiseFt = FMath::Clamp(P.DistNm * (bProp ? 45.0 : 110.0), bProp ? 3000.0 : 8000.0, Top);   // (jets: about FL250 at 230 nm, FL350 at 320 nm+; props lower)
	P.CruiseFt = FMath::RoundToDouble(P.CruiseFt / 1000.0) * 1000.0;
	// climb and descent: the type's rates (slower with height: 70% on average), ground speeds below cruise
	auto Plan = [&]()
	{
		const double ClimbMin = (P.CruiseFt - A->ElevFt) / (T->ClimbFpm * (bProp ? 0.85 : 0.7)), DescMin = (P.CruiseFt - 3000.0 - B->ElevFt) / (T->DescentFpm * 0.85);
		P.ClimbNm = ClimbMin / 60.0 * T->CruiseKt * 0.68; P.DescentNm = FMath::Max(0.0, DescMin) / 60.0 * T->CruiseKt * 0.72;
		P.ApproachNm = 12.0;   // (from 3,000 ft above the field on the glide path to the runway)
		return std::make_pair(ClimbMin, FMath::Max(0.0, DescMin));
	};
	auto Mins = Plan();
	while (P.ClimbNm + P.DescentNm + P.ApproachNm > P.DistNm && P.CruiseFt > 4000.0) { P.CruiseFt -= 1000.0; Mins = Plan(); }   // (a short hop never reaches its level)
	const double CruiseNm = FMath::Max(0.0, P.DistNm - P.ClimbNm - P.DescentNm - P.ApproachNm);
	const double ApproachMin = P.ApproachNm / (T->VappKt * 1.15) * 60.0;
	double M = 0; P.PhaseStart[FADPProfile::TaxiOut] = M; M += TaxiOut;
	P.PhaseStart[FADPProfile::Takeoff] = M; M += 1.0;
	P.PhaseStart[FADPProfile::Climb] = M; M += Mins.first;
	P.PhaseStart[FADPProfile::Cruise] = M; M += CruiseNm / T->CruiseKt * 60.0;
	P.PhaseStart[FADPProfile::Descent] = M; M += Mins.second;
	P.PhaseStart[FADPProfile::Approach] = M; M += ApproachMin;
	P.PhaseStart[FADPProfile::Landing] = M; M += 1.0;
	P.PhaseStart[FADPProfile::TaxiIn] = M; M += TaxiIn;
	P.PhaseStart[FADPProfile::Done] = M;
	return true;
}

const TCHAR* FADPProfile::PhaseName(EPhase P)
{
	static const TCHAR* N[] = {TEXT("Taxi out"), TEXT("Takeoff"), TEXT("Climb"), TEXT("Cruise"), TEXT("Descent"), TEXT("Approach"), TEXT("Landing"), TEXT("Taxi in"), TEXT("Parked")};
	return N[FMath::Clamp(int32(P), 0, int32(Done))];
}

FADPProfile::FState FADPProfile::At(double Min) const
{
	FState S;
	int32 Ph = 0; while (Ph < Done && Min >= PhaseStart[Ph + 1]) ++Ph;
	S.Phase = EPhase(Ph);
	const double In = Min - PhaseStart[Ph], Len = FMath::Max(1e-6, PhaseStart[Ph + 1] - PhaseStart[Ph]), K = FMath::Clamp(In / Len, 0.0, 1.0);
	const double CruiseNm = FMath::Max(0.0, DistNm - ClimbNm - DescentNm - ApproachNm);
	switch (S.Phase)
	{
	case TaxiOut: S.AlongNm = 0; S.AltFt = FromElevFt; S.GsKt = Type.TaxiKt; break;
	case Takeoff: S.AlongNm = 0; S.AltFt = FromElevFt; S.GsKt = Type.V2Kt * K; break;
	case Climb: S.AlongNm = ClimbNm * K; S.AltFt = FMath::Lerp(FromElevFt, CruiseFt, FMath::Sqrt(K)); S.GsKt = FMath::Lerp(Type.V2Kt, Type.CruiseKt, K); S.VsFpm = Type.ClimbFpm * (1.0 - 0.6 * K); break;
	case Cruise: S.AlongNm = ClimbNm + CruiseNm * K; S.AltFt = CruiseFt; S.GsKt = Type.CruiseKt; break;
	case Descent: S.AlongNm = ClimbNm + CruiseNm + DescentNm * K; S.AltFt = FMath::Lerp(CruiseFt, ToElevFt + 3000.0, K); S.GsKt = FMath::Lerp(Type.CruiseKt, Type.VappKt * 1.3, K); S.VsFpm = -Type.DescentFpm * 0.85; break;
	case Approach: S.AlongNm = ClimbNm + CruiseNm + DescentNm + ApproachNm * K; S.AltFt = FMath::Lerp(ToElevFt + 3000.0, ToElevFt + 50.0, K); S.GsKt = Type.VappKt; S.VsFpm = -(Type.VappKt * 5.3); break;   // (3 degree glide path: 5.3 x ground speed)
	case Landing: S.AlongNm = DistNm; S.AltFt = ToElevFt; S.GsKt = FMath::Lerp(Type.VappKt, Type.TaxiKt, K); break;
	default: S.AlongNm = DistNm; S.AltFt = ToElevFt; S.GsKt = S.Phase == TaxiIn ? Type.TaxiKt : 0; break;
	}
	const double F = DistNm > 0 ? FMath::Clamp(S.AlongNm / DistNm, 0.0, 1.0) : 0.0;
	ADPWorldData::Along(FromLat, FromLon, ToLat, ToLon, F, S.Lat, S.Lon);
	double NLat, NLon; ADPWorldData::Along(FromLat, FromLon, ToLat, ToLon, FMath::Min(1.0, F + 0.001), NLat, NLon);
	S.HeadingDeg = ADPWorldData::BearingDeg(S.Lat, S.Lon, NLat, NLon);
	return S;
}
