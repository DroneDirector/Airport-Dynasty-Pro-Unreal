// The holographic airport map: drawn line by line in the Android edition's holographic blues (holomap.ts), with the
// route in warm orange.
#include "UI/SHoloMap.h"
#include "Site.h"
#include "UI/MapTrace.h"
#include "JetwayRigs.h"
#include "BabylonModel.h"
#include "TerminalPlan.h"
#include "ADPLog.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Misc/DateTime.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "TowerADC.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#pragma region Colours
namespace
{
	const FLinearColor Bg(0.012f, 0.05f, 0.1f, 0.84f);
	const FLinearColor Grid(0.35f, 0.7f, 1.f, 0.07f);
	const FLinearColor Wall(0.55f, 0.84f, 1.f, 0.95f);
	const FLinearColor WallGlow(0.2f, 0.63f, 1.f, 0.3f);
	const FLinearColor GlassC(0.55f, 0.84f, 1.f, 0.45f);
	const FLinearColor Ghost(0.35f, 0.59f, 0.9f, 0.16f);
	const FLinearColor Mark(0.63f, 0.92f, 1.f, 0.85f);
	const FLinearColor RouteC(1.f, 0.67f, 0.24f, 0.95f);
	const FLinearColor RouteGlow(1.f, 0.6f, 0.2f, 0.3f);
	const FLinearColor Label(0.81f, 0.94f, 1.f, 1.f);
	constexpr float MiniMetres = 60.f; // the small map shows this much across
}
#pragma endregion

#pragma region Setup

void SHoloMap::Construct(const FArguments& Args)
{
	// nothing of the map ever shows outside its own frame (small or full screen)
	SetClipping(EWidgetClipping::ClipToBounds);
}

void SHoloMap::LoadPeaks()
{
	Peaks.Reset(); PeakBox = FBox2f(ForceInit);
	FString T; TSharedPtr<FJsonObject> J;
	if (!FFileHelper::LoadFileToString(T, *ADPSite::File(TEXT("peaks.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(T), J) || !J) return;
	const TArray<TSharedPtr<FJsonValue>>* L;
	if (!J->TryGetArrayField(TEXT("peaks"), L)) return;
	for (const TSharedPtr<FJsonValue>& V : *L)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject(); FPeak P;
		P.Name = O->GetStringField(TEXT("name")); P.P = FVector2f(float(O->GetNumberField(TEXT("x"))), float(O->GetNumberField(TEXT("y"))));
		P.Elev = int32(O->GetNumberField(TEXT("elev"))); O->TryGetBoolField(TEXT("main"), P.bMain);
		Peaks.Add(P); PeakBox += P.P;
	}
}

void SHoloMap::SetPlan(const FTerminalPlan& InPlan)
{
	LoadPeaks();
	Plan = InPlan;
	Map.Build(Plan);
	for (int32 f = 0; f < 3; ++f) { Walls[f].Reset(); StairBoxes[f].Reset(); LiftBoxes[f].Reset(); }
	Bounds = FBox2f(ForceInit);
	// each room's sides as lines, with the doors' openings left out
	for (const FPlanRoom& R : Plan.Rooms)
	{
		Bounds += FVector2f(R.X0, R.Z0);
		Bounds += FVector2f(R.X1, R.Z1);
		const int32 F = FMath::Clamp(R.Floor, 1, 2);
		auto Side = [&](bool bAlongX, float Fixed, float From, float To, bool bGlass)
		{
			TArray<FVector2f> Gaps;
			for (const FPlanDoor& D : Plan.Doors)
			{
				if (D.Floor != R.Floor || D.bAlongX != bAlongX) continue;
				const float Line = bAlongX ? D.Z : D.X, At = bAlongX ? D.X : D.Z;
				if (FMath::Abs(Line - Fixed) > 0.35f || At < From || At > To) continue;
				Gaps.Add(FVector2f(At - D.Width / 2, At + D.Width / 2));
			}
			Gaps.Sort([](const FVector2f& A, const FVector2f& B) { return A.X < B.X; });
			float C = From;
			auto Add = [&](float A, float B)
			{
				if (B - A < 0.05f) return;
				Walls[F].Add(bAlongX ? FSeg{FVector2f(A, Fixed), FVector2f(B, Fixed), bGlass} : FSeg{FVector2f(Fixed, A), FVector2f(Fixed, B), bGlass});
			};
			for (const FVector2f& G : Gaps) { Add(C, G.X); C = FMath::Max(C, G.Y); }
			Add(C, To);
		};
		Side(true, R.Z0, R.X0, R.X1, R.Glass.Contains(TEXT("s")));
		Side(true, R.Z1, R.X0, R.X1, R.Glass.Contains(TEXT("n")));
		Side(false, R.X0, R.Z0, R.Z1, R.Glass.Contains(TEXT("w")));
		Side(false, R.X1, R.Z0, R.Z1, R.Glass.Contains(TEXT("e")));
	}
	// the walls as they really stand (Data/holomap-walls.json, traced from the terminal itself - ADPMapTrace, run before
	// every release): they take the plan rooms' place, so every part built since the plan (the 16 gates, the claim and
	// arrivals wing, the tax free, the escalator rooms) is on the map; the plan keeps the names, the routes and the stairs
	{
		FString Text; TSharedPtr<FJsonObject> O;
		for (const FString& F : {FPaths::ProjectContentDir() / TEXT("Data/holomap-walls.json"), FPaths::ProjectDir() / TEXT("Data/holomap-walls.json")})
			if (FFileHelper::LoadFileToString(Text, *F) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O) && O.IsValid()) break;
		const TSharedPtr<FJsonObject>* Fl = nullptr;
		if (O.IsValid() && O->TryGetObjectField(TEXT("floors"), Fl))
		{
			int32 N = 0;
			for (int32 f = 1; f <= 2; ++f)
			{
				const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
				if (!(*Fl)->TryGetArrayField(FString::FromInt(f), L) || !L->Num()) continue;
				Walls[f].Reset();
				for (const TSharedPtr<FJsonValue>& V : *L)
				{
					const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray(); if (A.Num() < 5) continue;
					FSeg S{FVector2f(A[0]->AsNumber(), A[1]->AsNumber()), FVector2f(A[2]->AsNumber(), A[3]->AsNumber()), A[4]->AsNumber() > 0};
					Walls[f].Add(S); Bounds += S.A; Bounds += S.B; ++N;
				}
			}
			UE_LOG(LogTemp, Log, TEXT("Holographic map: %d wall lines traced from the terminal"), N);
		}
	}
	for (const FPlanStair& S : Plan.Stairs)
		for (int32 f = 1; f <= 2; ++f) StairBoxes[f].Add(FBox2f(FVector2f(S.X0, S.Z0), FVector2f(S.X1, S.Z1)));
	for (const FPlanPoint& L : Plan.Lifts)
		for (int32 f = 1; f <= 2; ++f) LiftBoxes[f].Add(FBox2f(FVector2f(L.X - 1.06f, L.Z - 1.06f), FVector2f(L.X + 1.06f, L.Z + 1.06f)));
	Columns.Reset();
	for (const FPlanPoint& C : Plan.Columns) Columns.Add(FVector2f(C.X, C.Z));
	LoadBuildings();
	for (const FBuilding& B : Buildings) { Bounds += B.Box.Min; Bounds += B.Box.Max; }
	Bounds = Bounds.ExpandBy(6.f);
	All = Bounds;
	ResetView();
}

void SHoloMap::LoadBuildings()
{
	Buildings.Reset();
	// the jetways and the car rental centre, traced from the Babylon model (Tools/make_holomap_buildings.py)
	FString Text;
	TArray<TSharedPtr<FJsonValue>> List;
	for (const FString& F : {FPaths::ProjectContentDir() / TEXT("Data/holomap-buildings.json"), FPaths::ProjectDir() / TEXT("Data/holomap-buildings.json")})
		if (FFileHelper::LoadFileToString(Text, *F) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), List)) break;
	for (const TSharedPtr<FJsonValue>& V : List)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O.IsValid()) continue;
		FBuilding& B = Buildings.AddDefaulted_GetRef();
		B.Label = O->GetStringField(TEXT("label"));
		B.Box = FBox2f(ForceInit);
		for (const TSharedPtr<FJsonValue>& FV : O->GetArrayField(TEXT("floors")))
		{
			const TSharedPtr<FJsonObject> FO = FV->AsObject();
			FBFloor& Fl = B.Floors.AddDefaulted_GetRef();
			Fl.Label = FO->GetStringField(TEXT("label"));
			Fl.Y0 = FO->GetNumberField(TEXT("y0")); Fl.Y1 = FO->GetNumberField(TEXT("y1"));
			for (const TSharedPtr<FJsonValue>& SV : FO->GetArrayField(TEXT("segs")))
			{
				const TArray<TSharedPtr<FJsonValue>>& A = SV->AsArray();
				if (A.Num() < 5) continue;
				FSeg S{FVector2f(A[0]->AsNumber(), A[1]->AsNumber()), FVector2f(A[2]->AsNumber(), A[3]->AsNumber()), A[4]->AsNumber() > 0};
				Fl.Segs.Add(S); B.Box += S.A; B.Box += S.B;
			}
		}
	}
	// the control tower, from its own plan (TowerADC): shaft and entrance hall, the lift lobby, the cab
	{
		FBuilding& T = Buildings.AddDefaulted_GetRef();
		T.Label = TEXT("ATC TOWER");
		T.Box = FBox2f(ForceInit);
		const FVector2f C(ATowerADC::AtX, ATowerADC::AtZ);
		auto Ring = [&C](FBFloor& F, float R, bool bGlass, int32 N)
		{
			for (int32 i = 0; i < N; ++i)
			{
				const float A0 = 2 * PI * i / N, A1 = 2 * PI * (i + 1) / N;
				F.Segs.Add(FSeg{C + FVector2f(FMath::Cos(A0), FMath::Sin(A0)) * R, C + FVector2f(FMath::Cos(A1), FMath::Sin(A1)) * R, bGlass});
			}
		};
		auto Box = [&C](FBFloor& F, float X0, float Z0, float X1, float Z1, bool bGlass)
		{
			const FVector2f P[4] = {C + FVector2f(X0, Z0), C + FVector2f(X1, Z0), C + FVector2f(X1, Z1), C + FVector2f(X0, Z1)};
			for (int32 i = 0; i < 4; ++i) F.Segs.Add(FSeg{P[i], P[(i + 1) % 4], bGlass});
		};
		FBFloor& G = T.Floors.AddDefaulted_GetRef(); G.Label = TEXT("GROUND FLOOR"); G.Y0 = -1.f; G.Y1 = ATowerADC::LobbyY - 1.f;
		Ring(G, 3.6f, false, 48); Box(G, -6.6f, -1.75f, -3.0f, 1.75f, true); Box(G, -0.55f, 1.9f, 0.55f, 3.0f, false);
		FBFloor& L = T.Floors.AddDefaulted_GetRef(); L.Label = TEXT("LIFT LOBBY"); L.Y0 = ATowerADC::LobbyY - 1.f; L.Y1 = ATowerADC::CabY - 0.5f;
		Ring(L, 4.4f, true, 48); Ring(L, 1.35f, false, 24); Box(L, -0.55f, 1.9f, 0.55f, 3.0f, false);
		FBFloor& K = T.Floors.AddDefaulted_GetRef(); K.Label = TEXT("CAB"); K.Y0 = ATowerADC::CabY - 0.5f; K.Y1 = 99.f;
		Ring(K, 6.0f, true, 64); Ring(K, 5.65f, false, 64); Ring(K, 4.75f, false, 64); Ring(K, 7.38f, true, 64); Ring(K, 1.45f, false, 24);
		for (const FBFloor& F : T.Floors) for (const FSeg& S : F.Segs) { T.Box += S.A; T.Box += S.B; }
	}
}

int32 SHoloMap::BuildingAt(const FVector& P, int32& OutFloor) const
{
	OutFloor = 0;
	for (int32 b = 0; b < Buildings.Num(); ++b)
	{
		if (!Buildings[b].Box.ExpandBy(0.6f).IsInsideOrOn(FVector2f(P.X, P.Z))) continue;
		for (int32 f = 0; f < Buildings[b].Floors.Num(); ++f)
			if (P.Y >= Buildings[b].Floors[f].Y0 && P.Y < Buildings[b].Floors[f].Y1) { OutFloor = f; return b; }
	}
	return INDEX_NONE;
}

void SHoloMap::SetPlayer(const FVector& PlanPos, float YawDeg)
{
	Player = PlanPos;
	Yaw = YawDeg;
	// the route follows you; a waypoint you reach is done, and the route clears when you reach the last
	if (Waypoints.Num() && FVector::Dist2D(FVector(Waypoints[0].X, Waypoints[0].Z, 0), FVector(Player.X, Player.Z, 0)) < 2.f && FloorOf(Waypoints[0].Y) == FloorOf(Player.Y))
	{
		Waypoints.RemoveAt(0);
		Rebuild();
	}
	if (Route.Num())
		while (Route.Num() > 1 && FVector::Dist2D(FVector(Route[0].X, Route[0].Z, 0), FVector(Player.X, Player.Z, 0)) < 1.5f) Route.RemoveAt(0);
}

void SHoloMap::SetFull(bool bOn) { bFull = bOn; if (bOn) ResetView(); }

void SHoloMap::ResetView() { View = Bounds; }

void SHoloMap::AddHangar(const FString& Name, const TArray<FVector4f>& Outline, const TArray<FVector4f>& Door)
{
	FBuilding& B = Buildings.AddDefaulted_GetRef();
	B.Label = Name; B.bNamed = true; B.Box = FBox2f(ForceInit);
	for (const TCHAR* F : { TEXT("HALL / GROUND FLOOR"), TEXT("CREW ANNEX UPSTAIRS") })
	{
		FBFloor& Fl = B.Floors.AddDefaulted_GetRef(); Fl.Label = F;
		Fl.Y0 = B.Floors.Num() == 1 ? -1.f : 4.5f; Fl.Y1 = B.Floors.Num() == 1 ? 4.5f : 99.f;
		for (const FVector4f& W : Outline) Fl.Segs.Add(FSeg{FVector2f(W.X, W.Y), FVector2f(W.Z, W.W), false});
		for (const FVector4f& W : Door) Fl.Segs.Add(FSeg{FVector2f(W.X, W.Y), FVector2f(W.Z, W.W), true});
	}
	for (const FSeg& S : B.Floors[0].Segs) { B.Box += S.A; B.Box += S.B; }
	All += B.Box.ExpandBy(20.f);
}

FReply SHoloMap::OnMouseWheel(const FGeometry& Geo, const FPointerEvent& Event)
{
	if (!bFull) return FReply::Unhandled();
	// zoom round the pointer: what is under it stays under it; out as far as everything on the map, in to a room
	const FVector2f Size(Geo.GetLocalSize()), L(Geo.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	const FVector2f Under = FromLocal(L, Size);
	const float K = FMath::Pow(1.25f, -Event.GetWheelDelta());
	FVector2f NewHalf = View.GetExtent() * K;
	// out: no further than the whole map in view; in: no closer than 20 m across
	const FVector2f MaxHalf = Zoomable().GetExtent() * 1.1f;   // (the mountains' layer on: out as far as them)
	NewHalf *= FMath::Min(1.f, FMath::Max(MaxHalf.X / NewHalf.X, MaxHalf.Y / NewHalf.Y));
	NewHalf *= FMath::Max(1.f, 10.f / FMath::Min(NewHalf.X, NewHalf.Y));
	// keep the point under the pointer where it is
	const FVector2f Mid = View.GetCenter(), Rel = (Under - Mid) / View.GetExtent();
	const FVector2f NewMid = Under - Rel * NewHalf;
	View = FBox2f(NewMid - NewHalf, NewMid + NewHalf);
	return FReply::Handled();
}

#pragma endregion

#pragma region Placing

float SHoloMap::Scale(const FVector2f& Size) const
{
	if (!bFull)
	{
		// the small map's reach: the terminal's rooms close up; a hangar's whole hall and annex; out on the airfield, wide
		// enough to see the next building coming
		int32 Fl; const int32 B = BuildingAt(Player, Fl);
		const bool bInHangar = B != INDEX_NONE && Buildings[B].bNamed;
		// (out on the forecourt or the apron too, not only past everything drawn: 35 m in front of the entrance no wall was in the
		//  small map's reach and it was empty)
		const bool bOut = B == INDEX_NONE && (!Bounds.IsInside(FVector2f(Player.X, Player.Z)) || Player.Z < -5.f || Player.Z > 86.f);
		return Size.X / (bInHangar ? 120.f : bOut ? 220.f : MiniMetres);
	}
	const FVector2f E = View.GetSize();
	return FMath::Min((Size.X - 40) / E.X, (Size.Y - 90) / E.Y);
}

FVector2f SHoloMap::ToLocal(const FVector2f& P, const FVector2f& Size) const
{
	if (!bPFresh || PSize != Size)
	{
		bPFresh = true; PSize = Size; PScale = Scale(Size);
		const float T0 = FMath::DegreesToRadians(Yaw);
		PF = FVector2f(FMath::Cos(T0), -FMath::Sin(T0)); PRt = FVector2f(-FMath::Sin(T0), -FMath::Cos(T0));
	}
	const float S = PScale;
	if (bFull)
	{
		const FVector2f E = View.GetSize() * S;
		const FVector2f O((Size.X - E.X) / 2, 60 + (Size.Y - 70 - E.Y) / 2);
		return FVector2f(O.X + (P.X - View.Min.X) * S, O.Y + (View.Max.Y - P.Y) * S);
	}
	// turned so the way you face is up: Unreal yaw θ faces the plan's (cos θ, -sin θ)
	const FVector2f &F = PF, &Rt = PRt;
	const FVector2f D(P.X - Player.X, P.Y - Player.Z);
	return Size * 0.5f + FVector2f(FVector2f::DotProduct(D, Rt), -FVector2f::DotProduct(D, F)) * S;
}

FVector2f SHoloMap::FromLocal(const FVector2f& L, const FVector2f& Size) const
{
	const float S = Scale(Size);
	const FVector2f E = View.GetSize() * S;
	const FVector2f O((Size.X - E.X) / 2, 60 + (Size.Y - 70 - E.Y) / 2);
	return FVector2f(View.Min.X + (L.X - O.X) / S, View.Max.Y - (L.Y - O.Y) / S);
}

#pragma endregion

#pragma region Painting

int32 SHoloMap::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2f Size(Geo.GetLocalSize());
	bPFresh = false;
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FPaintGeometry PG = Geo.ToPaintGeometry();
	FSlateDrawElement::MakeBox(Out, Layer, PG, White, ESlateDrawEffect::None, Bg);

	// the faint grid
	const float GridStep = bFull ? 40.f : 24.f;
	for (float X = 0; X < Size.X; X += GridStep) FSlateDrawElement::MakeLines(Out, Layer + 1, PG, TArray<FVector2f>{FVector2f(X, 0), FVector2f(X, Size.Y)}, ESlateDrawEffect::None, Grid, false, 1);
	for (float Y = 0; Y < Size.Y; Y += GridStep) FSlateDrawElement::MakeLines(Out, Layer + 1, PG, TArray<FVector2f>{FVector2f(0, Y), FVector2f(Size.X, Y)}, ESlateDrawEffect::None, Grid, false, 1);

	const int32 F = FloorOf(Player.Y), Other = F == 1 ? 2 : 1;
	auto OnScreen = [&](const FVector2f& L) { return L.X > -40 && L.Y > -40 && L.X < Size.X + 40 && L.Y < Size.Y + 40; };
	auto Line = [&](const FVector2f& A, const FVector2f& B, const FLinearColor& C, float T, const FVector2f& Off = FVector2f::ZeroVector)
	{
		const FVector2f LA = ToLocal(A, Size) + Off, LB = ToLocal(B, Size) + Off;
		if (!OnScreen(LA) && !OnScreen(LB)) return;
		FSlateDrawElement::MakeLines(Out, Layer + 2, PG, TArray<FVector2f>{LA, LB}, ESlateDrawEffect::None, C, true, T);
	};
	auto Rect = [&](const FBox2f& B, const FLinearColor& C, float T, const FVector2f& Off = FVector2f::ZeroVector)
	{
		Line(B.Min, FVector2f(B.Max.X, B.Min.Y), C, T, Off); Line(FVector2f(B.Max.X, B.Min.Y), B.Max, C, T, Off);
		Line(B.Max, FVector2f(B.Min.X, B.Max.Y), C, T, Off); Line(FVector2f(B.Min.X, B.Max.Y), B.Min, C, T, Off);
	};

	int32 BF = 0;
	const int32 InB = BuildingAt(Player, BF);
	// the other floor, a faint ghost a little below
	for (const FSeg& S : Walls[Other]) Line(S.A, S.B, Ghost, 1, FVector2f(0, (F - Other) * 7.f));
	// this floor: a soft glow under bright walls, glass lighter (dimmer while you are in another building)
	if (InB == INDEX_NONE) for (const FSeg& S : Walls[F]) Line(S.A, S.B, WallGlow, 4.f);
	for (const FSeg& S : Walls[F]) Line(S.A, S.B, InB == INDEX_NONE ? (S.bGlass ? GlassC : Wall) : FLinearColor(0.55f, 0.84f, 1.f, 0.4f), InB == INDEX_NONE ? 1.6f : 1.f);
	// the other buildings, whole: the one you are in bright on its floor; the rest on the floor nearest your height,
	// their other floors as ghosts
	// the jetways, live: where each one is right now (the old traced jetways 1-4 step aside for them)
	if (LiveJetways)
	{
		TArray<FVector4f> J; LiveJetways(J);
		for (const FVector4f& S : J) Line(FVector2f(S.X, S.Y), FVector2f(S.Z, S.W), FLinearColor(0.62f, 0.88f, 1.f, 0.85f), 1.2f);
	}
	for (int32 b = 0; b < Buildings.Num(); ++b)
	{
		const FBuilding& Bd = Buildings[b];
		if (LiveJetways && Bd.Label.StartsWith(TEXT("JETWAY"))) continue;
		int32 Show = 0;
		if (b == InB) Show = BF;
		else for (int32 f = 0; f < Bd.Floors.Num(); ++f) if (Player.Y >= Bd.Floors[f].Y0 && Player.Y < Bd.Floors[f].Y1) Show = f;
		for (int32 f = 0; f < Bd.Floors.Num(); ++f)
		{
			if (f == Show) continue;
			for (const FSeg& S : Bd.Floors[f].Segs) Line(S.A, S.B, Ghost, 1);
		}
		if (!Bd.Floors.IsValidIndex(Show)) continue;
		const bool bHere = b == InB;
		if (bHere) for (const FSeg& S : Bd.Floors[Show].Segs) Line(S.A, S.B, WallGlow, 4.f);
		for (const FSeg& S : Bd.Floors[Show].Segs) Line(S.A, S.B, bHere ? (S.bGlass ? GlassC : Wall) : FLinearColor(0.55f, 0.84f, 1.f, S.bGlass ? 0.3f : 0.6f), bHere ? 1.6f : 1.1f);
	}
	for (const FBox2f& B : StairBoxes[F]) Rect(B, Mark, 1);
	for (const FBox2f& B : LiftBoxes[F]) Rect(B, Mark, 1);
	if (F == 1) for (const FVector2f& C : Columns) Rect(FBox2f(C - FVector2f(0.4f), C + FVector2f(0.4f)), Mark, 1);

	// the rooms' names, full screen only
	if (bFull)
	{
		const FSlateFontInfo Small = FCoreStyle::GetDefaultFontStyle("Regular", 8);
		// the hangars' names, over their doors
		const FSlateFontInfo Bold = FCoreStyle::GetDefaultFontStyle("Bold", 9);
		for (const FBuilding& Bd : Buildings)
			if (Bd.bNamed)
			{
				const FVector2f C = ToLocal(FVector2f(Bd.Box.GetCenter().X, Bd.Box.Min.Y), Size);
				FSlateDrawElement::MakeText(Out, Layer + 3, Geo.ToPaintGeometry(FVector2f(200, 16), FSlateLayoutTransform(FVector2f(C.X - Bd.Label.Len() * 3.2f, C.Y + 4))), Bd.Label, Bold, ESlateDrawEffect::None, FLinearColor(0.75f, 0.93f, 1.f, 0.95f));
			}
		for (const FPlanRoom& R : Plan.Rooms)
		{
			if (R.Floor != F || (R.X1 - R.X0) * (R.Z1 - R.Z0) < 120) continue;
			FString Name = R.Name;
			int32 Paren;
			if (Name.FindChar(TEXT('('), Paren)) Name = Name.Left(Paren).TrimEnd();
			const FVector2f C = ToLocal(FVector2f((R.X0 + R.X1) / 2, (R.Z0 + R.Z1) / 2), Size);
			FSlateDrawElement::MakeText(Out, Layer + 3, Geo.ToPaintGeometry(FVector2f(160, 14), FSlateLayoutTransform(FVector2f(C.X - Name.Len() * 2.6f, C.Y - 6))), Name.ToUpper(), Small, ESlateDrawEffect::None, FLinearColor(0.63f, 0.84f, 1.f, 0.7f));
		}
	}

	// the route: bright on this floor, faint on the other
	if (Route.Num())
	{
		FVector Prev = Player;
		for (const FVector& P : Route)
		{
			const bool bMine = FloorOf(P.Y) == F && FloorOf(Prev.Y) == F;
			Line(FVector2f(Prev.X, Prev.Z), FVector2f(P.X, P.Z), bMine ? RouteGlow : FLinearColor(1, 0.75f, 0.4f, 0.12f), bMine ? 6.f : 3.f);
			if (bMine) Line(FVector2f(Prev.X, Prev.Z), FVector2f(P.X, P.Z), RouteC, 2.f);
			Prev = P;
		}
	}

	// your marks: numbered rings
	for (int32 i = 0; i < Waypoints.Num(); ++i)
	{
		const FVector2f C = ToLocal(FVector2f(Waypoints[i].X, Waypoints[i].Z), Size);
		const bool bHere = FloorOf(Waypoints[i].Y) == F;
		const FLinearColor Col = bHere ? RouteC : FLinearColor(1, 0.75f, 0.4f, 0.35f);
		TArray<FVector2f> Ring;
		for (int32 k = 0; k <= 20; ++k) { const float A = k / 20.f * 2 * PI; Ring.Add(C + FVector2f(FMath::Cos(A), FMath::Sin(A)) * 7.f); }
		FSlateDrawElement::MakeLines(Out, Layer + 4, Geo.ToPaintGeometry(), Ring, ESlateDrawEffect::None, Col, true, 2.f);
		FSlateDrawElement::MakeText(Out, Layer + 5, Geo.ToPaintGeometry(FVector2f(16, 14), FSlateLayoutTransform(C + FVector2f(-4, -7))), FString::FromInt(i + 1), FCoreStyle::GetDefaultFontStyle("Bold", 8), ESlateDrawEffect::None, Col);
	}

	// you: an arrow with your view cone
	{
		const FVector2f C = ToLocal(FVector2f(Player.X, Player.Z), Size);
		const float T = FMath::DegreesToRadians(Yaw);
		// in the small map you always face up; full screen the arrow turns
		const float A = bFull ? FMath::Atan2(FMath::Sin(T), FMath::Cos(T)) : -HALF_PI;
		auto Dir = [&](float Ang, float Len) { return C + FVector2f(FMath::Cos(Ang), FMath::Sin(Ang)) * Len; };
		for (const float S : {-0.6f, 0.6f})
			FSlateDrawElement::MakeLines(Out, Layer + 4, PG, TArray<FVector2f>{C, Dir(A + S, 34)}, ESlateDrawEffect::None, FLinearColor(0.6f, 0.9f, 1.f, 0.35f), true, 1);
		FSlateDrawElement::MakeLines(Out, Layer + 4, PG, TArray<FVector2f>{Dir(A, 9), Dir(A + 2.5f, 6), Dir(A + PI, 2), Dir(A - 2.5f, 6), Dir(A, 9)}, ESlateDrawEffect::None, FLinearColor::White, true, 2.f);
	}

	// the mountains' layer (to-do 171): a small summit mark, the name and the height; and the button that switches it
	if (bFull && Peaks.Num())
	{
		if (bPeaks)
			for (const FPeak& Pk : Peaks)
			{
				const FVector2f C = ToLocal(Pk.P, Size);
				if (C.X < 0 || C.Y < 0 || C.X > Size.X || C.Y > Size.Y) continue;
				const FLinearColor Ice(0.85f, 0.95f, 1.f, Pk.bMain ? 0.95f : 0.75f);
				FSlateDrawElement::MakeLines(Out, Layer + 4, Geo.ToPaintGeometry(), TArray<FVector2f>{C + FVector2f(-6, 4), C + FVector2f(0, -6), C + FVector2f(6, 4), C + FVector2f(-6, 4)}, ESlateDrawEffect::None, Ice, true, 1.5f);
				const FString Lb = FString::Printf(TEXT("%s  %d m"), *Pk.Name, Pk.Elev);
				FSlateDrawElement::MakeText(Out, Layer + 4, Geo.ToPaintGeometry(FVector2f(260, 14), FSlateLayoutTransform(C + FVector2f(9, -8))), Lb, FCoreStyle::GetDefaultFontStyle(Pk.bMain ? "Bold" : "Regular", 9), ESlateDrawEffect::None, Ice);
			}
		const FVector2f O(Size.X - 150, 8);
		FSlateDrawElement::MakeBox(Out, Layer + 6, Geo.ToPaintGeometry(FVector2f(140, 24), FSlateLayoutTransform(O)), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, bPeaks ? FLinearColor(0.10f, 0.45f, 0.75f, 0.85f) : FLinearColor(0.02f, 0.12f, 0.22f, 0.8f));
		FSlateDrawElement::MakeText(Out, Layer + 7, Geo.ToPaintGeometry(FVector2f(130, 18), FSlateLayoutTransform(O + FVector2f(10, 4))), bPeaks ? TEXT("MOUNTAINS  ON") : TEXT("MOUNTAINS  OFF"), FCoreStyle::GetDefaultFontStyle("Bold", 10), ESlateDrawEffect::None, FLinearColor::White);
	}

	// the legend, full screen only, in the map's lower right corner
	if (bFull && Legend.Num() && !bLegendOpen)
	{
		// minimized: a small tab, LEGEND and a badge with the key that opens it (as the Buttons tab)
		const FVector2f O(Size.X - 190, Size.Y - 44);
		FSlateDrawElement::MakeBox(Out, Layer + 5, Geo.ToPaintGeometry(FVector2f(170, 28), FSlateLayoutTransform(O)), White, ESlateDrawEffect::None, FLinearColor(0.012f, 0.05f, 0.1f, 0.9f));
		FSlateDrawElement::MakeText(Out, Layer + 6, Geo.ToPaintGeometry(FVector2f(70, 18), FSlateLayoutTransform(O + FVector2f(10, 6))), TEXT("LEGEND"), FCoreStyle::GetDefaultFontStyle("Bold", 9), ESlateDrawEffect::None, Label);
		const float KW = 16.f + LegendKey.Len() * 7.5f;
		FSlateDrawElement::MakeBox(Out, Layer + 6, Geo.ToPaintGeometry(FVector2f(KW, 20), FSlateLayoutTransform(O + FVector2f(170 - KW - 6, 4))), White, ESlateDrawEffect::None, FLinearColor(0.08f, 0.24f, 0.4f, 1.f));
		FSlateDrawElement::MakeText(Out, Layer + 7, Geo.ToPaintGeometry(FVector2f(KW, 18), FSlateLayoutTransform(O + FVector2f(170 - KW - 6 + 7, 5))), LegendKey, FCoreStyle::GetDefaultFontStyle("Bold", 10), ESlateDrawEffect::None, FLinearColor::White);
	}
	if (bFull && Legend.Num() && bLegendOpen)
	{
		const float LH = 15.f, W = 300.f, H = 16.f + Legend.Num() * LH;
		const FVector2f O(Size.X - W - 20, Size.Y - H - 20);
		FSlateDrawElement::MakeBox(Out, Layer + 5, Geo.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(O)), White, ESlateDrawEffect::None, FLinearColor(0.012f, 0.05f, 0.1f, 0.9f));
		for (int32 i = 0; i < Legend.Num(); ++i)
			FSlateDrawElement::MakeText(Out, Layer + 6, Geo.ToPaintGeometry(FVector2f(W - 16, LH), FSlateLayoutTransform(O + FVector2f(8, 8 + i * LH))), Legend[i],
				FCoreStyle::GetDefaultFontStyle(i == 0 ? "Bold" : "Regular", 9), ESlateDrawEffect::None, i == 0 ? Label : FLinearColor(0.63f, 0.84f, 1.f, 0.9f));
	}

	// the floor shown, and the corner brackets
	FString FloorName = F == 2 ? TEXT("FLOOR 2") : TEXT("FLOOR 1");
	{ int32 BF2 = 0; const int32 B2 = BuildingAt(Player, BF2); if (B2 != INDEX_NONE) FloorName = Buildings[B2].Label + TEXT("  ·  ") + Buildings[B2].Floors[BF2].Label; }
	FSlateDrawElement::MakeText(Out, Layer + 5, Geo.ToPaintGeometry(FVector2f(200, 18), FSlateLayoutTransform(FVector2f(10, 8))), bFull ? FloorName + TEXT("  ·  CLICK WHERE YOU WANT TO GO  ·  M CLOSES") : FloorName, FCoreStyle::GetDefaultFontStyle("Bold", bFull ? 12 : 9), ESlateDrawEffect::None, Label);
	const float B = bFull ? 26.f : 16.f;
	for (const FVector2f& Corner : {FVector2f(0, 0), FVector2f(Size.X, 0), FVector2f(0, Size.Y), FVector2f(Size.X, Size.Y)})
	{
		const float SX = Corner.X > 0 ? -1.f : 1.f, SY = Corner.Y > 0 ? -1.f : 1.f;
		const FVector2f P = Corner + FVector2f(SX * 3, SY * 3);
		FSlateDrawElement::MakeLines(Out, Layer + 5, PG, TArray<FVector2f>{P + FVector2f(SX * B, 0), P, P + FVector2f(0, SY * B)}, ESlateDrawEffect::None, Wall, true, 2);
	}
	return Layer + 5;
}

#pragma endregion

#pragma region Clicking

FReply SHoloMap::OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Event)
{
	if (!bFull)
	{
		if (OnOpen) OnOpen();
		return FReply::Handled();
	}
	// full screen: a click adds a waypoint (the route goes through your marks in order); a right-click on one removes it
	const FVector2f Size(Geo.GetLocalSize());
	const FVector2f L(Geo.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	// the MOUNTAINS button: the names on or off (on: zoomed out so they are in view)
	if (Peaks.Num() && L.X >= Size.X - 150 && L.X <= Size.X - 10 && L.Y >= 8 && L.Y <= 32)
	{
		bPeaks = !bPeaks;
		if (bPeaks) View = Zoomable().ExpandBy(500.f); else ResetView();
		return FReply::Handled();
	}
	const FVector2f P = FromLocal(L, Size);
	const int32 F = FloorOf(Player.Y);
	if (Event.GetEffectingButton() == EKeys::RightMouseButton)
	{
		for (int32 i = 0; i < Waypoints.Num(); ++i)
			if (FVector2f::Distance(FVector2f(Waypoints[i].X, Waypoints[i].Z), P) < 3.f) { Waypoints.RemoveAt(i); Rebuild(); break; }
		return FReply::Handled();
	}
	const int32 To = PlaceAt(P.X, P.Y, F);
	if (To != INDEX_NONE) { Waypoints.Add(FVector(P.X, Map.Places[To].Y, P.Y)); Rebuild(); }
	return FReply::Handled();
}

int32 SHoloMap::PlaceAt(float X, float Z, int32 Floor) const
{
	for (int32 i = 0; i < Map.Places.Num(); ++i)
	{
		const FWalkMap::FPlace& Q = Map.Places[i];
		if (!Q.bOutside && FloorOf(Q.Y) == Floor && X >= Q.X0 && X <= Q.X1 && Z >= Q.Z0 && Z <= Q.Z1) return i;
	}
	return INDEX_NONE;
}

void SHoloMap::Rebuild()
{
	// from you to the first mark, then mark to mark (through the doors, up or down the stairs)
	Route.Reset();
	FVector From = Player;
	for (const FVector& W : Waypoints)
	{
		const int32 A = PlaceAt(From.X, From.Z, FloorOf(From.Y)), B = PlaceAt(W.X, W.Z, FloorOf(W.Y));
		TArray<FVector> Part;
		if (A != INDEX_NONE && B != INDEX_NONE && Map.Way(A, From, B, W, Part)) Route.Append(Part);
		else Route.Add(W);
		From = W;
	}
}

#pragma endregion

// ---- the compass strip, its own section right above the small map (owner, 2026-11-18: not on top of the map) ----
int32 SCompassStrip::OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const
{
	// N, E, S, W, the points between and tick marks, sliding left and right as you turn; true directions - the plan's x
	// runs the runway's true heading (Data/airfield.json, 068 deg), so the way you face on the real globe is the runway
	// heading plus your turn from it
	const TSharedPtr<SHoloMap> M = Map.Pin(); if (!M.IsValid()) return Layer;
	const FVector2f Size(Geo.GetLocalSize());
	const FPaintGeometry PG = Geo.ToPaintGeometry();
	const float Bearing = M->Bearing();
	const float H = Size.Y, Y0 = 0.f, X0 = 0.f, W = Size.X, Mid = X0 + W * 0.5f, PxPerDeg = W / 120.f;   // (120 degrees across)
	static const FSlateBrush* Box = FCoreStyle::Get().GetBrush("WhiteBrush");
	FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X0, Y0))), Box, ESlateDrawEffect::None, FLinearColor(0.02f, 0.10f, 0.20f, 0.82f));
	FSlateDrawElement::MakeLines(Out, Layer + 1, PG, TArray<FVector2f>{FVector2f(0, H - 1), FVector2f(W, H - 1)}, ESlateDrawEffect::None, FLinearColor(0.55f, 0.84f, 1.f, 0.5f), false, 1);
	const FSlateFontInfo Big = FCoreStyle::GetDefaultFontStyle("Bold", 10), Sm = FCoreStyle::GetDefaultFontStyle("Bold", 7), Num = FCoreStyle::GetDefaultFontStyle("Bold", 8);
	const FLinearColor TickC(0.55f, 0.84f, 1.f, 0.85f), North(1.f, 0.42f, 0.25f, 1.f), Letter(0.85f, 0.95f, 1.f, 1.f);
	for (int32 d = 0; d < 360; d += 5)
	{
		const float Off = FMath::Fmod(d - Bearing + 540.f, 360.f) - 180.f;   // (-180..180: left of you negative)
		if (FMath::Abs(Off) > 56.f) continue;
		const float X = Mid + Off * PxPerDeg;
		const bool bCard = d % 90 == 0, bInter = d % 45 == 0, bTen = d % 15 == 0;
		const float L = bCard ? 9.f : bInter ? 7.f : bTen ? 5.f : 3.f;
		FSlateDrawElement::MakeLines(Out, Layer + 2, PG, TArray<FVector2f>{FVector2f(X, H - 2), FVector2f(X, H - 2 - L)}, ESlateDrawEffect::None, d == 0 ? North : TickC, true, bCard ? 1.6f : 1.f);
		if (bInter)
		{
			static const TCHAR* Pt[] = { TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW") };
			const FString T = Pt[d / 45];
			const FSlateFontInfo& Fo = bCard ? Big : Sm;
			const float Tw = T.Len() * (bCard ? 7.f : 5.2f);
			FSlateDrawElement::MakeText(Out, Layer + 2, Geo.ToPaintGeometry(FVector2f(30, 14), FSlateLayoutTransform(FVector2f(X - Tw * 0.5f, bCard ? 3.f : 5.f))), T, Fo, ESlateDrawEffect::None, d == 0 ? North : Letter);
		}
	}
	// the way you face: its degrees in a small box at the middle, a mark under it
	const FString Deg = FString::Printf(TEXT("%03d"), FMath::RoundToInt(Bearing) % 360);
	FSlateDrawElement::MakeBox(Out, Layer + 3, Geo.ToPaintGeometry(FVector2f(30, 15), FSlateLayoutTransform(FVector2f(Mid - 15, 2))), Box, ESlateDrawEffect::None, FLinearColor(0.02f, 0.08f, 0.16f, 0.95f));
	FSlateDrawElement::MakeText(Out, Layer + 4, Geo.ToPaintGeometry(FVector2f(30, 14), FSlateLayoutTransform(FVector2f(Mid - 10, 3))), Deg, Num, ESlateDrawEffect::None, FLinearColor(1.f, 0.85f, 0.4f, 1.f));
	FSlateDrawElement::MakeLines(Out, Layer + 4, PG, TArray<FVector2f>{FVector2f(Mid - 4, H - 1), FVector2f(Mid, H - 6), FVector2f(Mid + 4, H - 1)}, ESlateDrawEffect::None, FLinearColor(1.f, 0.8f, 0.3f, 1.f), true, 2);
	return Layer + 4;
}

// ---- the map traced from the real terminal ----
#if WITH_EDITORONLY_DATA
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#endif
// The holographic map traced from the real terminal (owner, 2026-11-19: "the holographic map is missing some sections
// because the terminal has been extended ... from now on, always add new parts ... automatically"). Run in the editor's
// game (-UIMapTrace, also by Tools/release_pc.sh before every release): every wall of the terminal model - its parts as
// they stand in the game, the annex pieces and the 16 gates included - is cut at walking height on each floor (1.3 m
// above Floor 1 and Floor 2); each cut through an upright face becomes a line on the map (glass kept as glass). Doors,
// lamps, signs, seats, counters, plants, cars, the stands' aircraft, escalators and stairs are left out by their names;
// anything shorter than a wall is left out by its height. Written to Data/holomap-walls.json (plan metres); SHoloMap
bool ADPMapTrace::Run(UWorld* W, float F2Y)
{
#if WITH_EDITORONLY_DATA
	if (!W) return false;
	ABabylonModel* Term = nullptr;
	for (TActorIterator<ABabylonModel> It(W); It; ++It) if (It->ModelName() == TEXT("Terminal")) { Term = *It; break; }
	if (!Term) { ADP_WARN(TEXT("Map trace: no terminal in the world")); return false; }
	static const TCHAR* Skip[] = { TEXT("STAND-"), TEXT("/LAMP"), TEXT("LIGHT"), TEXT("SIGN"), TEXT("SEAT"), TEXT("KIOSK"), TEXT("COUNTER"), TEXT("PLANT"),
		TEXT("TREE"), TEXT("CAR-"), TEXT("BIN"), TEXT("DESK"), TEXT("SCREEN"), TEXT("TABLE"), TEXT("CHAIR"), TEXT("BELT"), TEXT("ESC"), TEXT("STAIR"),
		TEXT("TRIM"), TEXT("HANDRAIL"), TEXT("BENCH"), TEXT("SHELF"), TEXT("CROWN"), TEXT("ADVERT"), TEXT("FIDS"), TEXT("MONITOR"), TEXT("TROLLEY"),
		TEXT("LIFT-CAB"), TEXT("CANOPY"), TEXT("ROOF"), TEXT("CEIL"), TEXT("FLOOR"), TEXT("KERB"), TEXT("DOWNLIGHT"), TEXT("box#lamp") };
	const float Cut[3] = { 0.f, 130.f, (F2Y + 1.3f) * 100.f };   // (world height of each floor's cut, cm)
	TArray<TArray<FVector4f>> Segs; Segs.SetNum(3);
	TSet<FIntVector4> Seen[3]; TArray<int32> Glass[3];
	int32 Parts = 0, Kept = 0;
	TArray<UInstancedStaticMeshComponent*> Lists; Term->GetComponents<UInstancedStaticMeshComponent>(Lists);
	for (UInstancedStaticMeshComponent* K : Lists)
	{
		UStaticMesh* SM = K ? K->GetStaticMesh() : nullptr; if (!SM || !K->IsVisible()) continue;
		const FMeshDescription* MD = SM->GetMeshDescription(0); if (!MD) continue;
		FStaticMeshConstAttributes Attr(*MD);
		const TVertexAttributesConstRef<FVector3f> Pos = Attr.GetVertexPositions();
		const FString Mat = K->GetMaterial(0) ? K->GetMaterial(0)->GetName() : FString();
		const bool bGlassMat = Mat.Contains(TEXT("glass"), ESearchCase::IgnoreCase);
		const FBox MB = SM->GetBoundingBox();
		for (int32 i = 0; i < K->GetInstanceCount(); ++i)
		{
			++Parts;
			FTransform T; if (!K->GetInstanceTransform(i, T, true)) continue;
			if (T.GetScale3D().GetAbsMin() < 0.001f) continue;   // (hidden copies)
			const FString Nm = Term->NodeAt(K, i);
			bool bSkip = Nm.StartsWith(TEXT("D-")) || Nm.Contains(TEXT("/D-"));
			for (const TCHAR* S : Skip) if (!bSkip && Nm.Contains(S, ESearchCase::IgnoreCase)) bSkip = true;
			if (bSkip) continue;
			const FBox WB = MB.TransformBy(T);
			if (WB.Max.Z - WB.Min.Z < 220.f) continue;   // (shorter than a wall)
			bool bAny = false;
			for (int32 f = 1; f <= 2; ++f) if (WB.Min.Z < Cut[f] && WB.Max.Z > Cut[f]) bAny = true;
			if (!bAny) continue;
			++Kept;
			TArray<FVector4f> Curve[3];   // (curved glass - the gates' windows - is cut into pieces of a few cm: joined into lines of about 1 m below)
			for (const FTriangleID Tri : MD->Triangles().GetElementIDs())
			{
				const TArrayView<const FVertexID> V = MD->GetTriangleVertices(Tri);
				const FVector P0 = T.TransformPosition(FVector(Pos[V[0]])), P1 = T.TransformPosition(FVector(Pos[V[1]])), P2 = T.TransformPosition(FVector(Pos[V[2]]));
				const FVector Nn = FVector::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();
				if (Nn.IsNearlyZero() || FMath::Abs(Nn.Z) > 0.35f) continue;   // (upright faces only)
				for (int32 f = 1; f <= 2; ++f)
				{
					const float H = Cut[f];
					TArray<FVector, TInlineAllocator<3>> X;
					const FVector Pt[3] = { P0, P1, P2 };
					for (int32 e = 0; e < 3; ++e)
					{
						const FVector& A = Pt[e]; const FVector& B = Pt[(e + 1) % 3];
						if ((A.Z - H) * (B.Z - H) < 0.f) { const float U = (H - A.Z) / (B.Z - A.Z); X.Add(A + (B - A) * U); }
					}
					if (X.Num() < 2) continue;
					const FVector2f A2(X[0].X / 100.f, -X[0].Y / 100.f), B2(X[1].X / 100.f, -X[1].Y / 100.f);   // (plan metres: x, z)
					if (FVector2f::Distance(A2, B2) < 0.12f) { if (bGlassMat && FVector2f::Distance(A2, B2) > 0.004f) Curve[f].Add(FVector4f(A2.X, A2.Y, B2.X, B2.Y)); continue; }
					// (the same line once: its ends on a 5 cm grid, either way round)
					FIntPoint QA(FMath::RoundToInt(A2.X * 20), FMath::RoundToInt(A2.Y * 20)), QB(FMath::RoundToInt(B2.X * 20), FMath::RoundToInt(B2.Y * 20));
					if (QB.X < QA.X || (QB.X == QA.X && QB.Y < QA.Y)) Swap(QA, QB);
					const FIntVector4 Key(QA.X, QA.Y, QB.X, QB.Y);
					if (Seen[f].Contains(Key)) continue;
					Seen[f].Add(Key);
					Segs[f].Add(FVector4f(A2.X, A2.Y, B2.X, B2.Y));
					if (bGlassMat) Glass[f].Add(Segs[f].Num() - 1);
				}
			}
			for (int32 f = 1; f <= 2; ++f)
			{
				TArray<FVector4f>& C = Curve[f]; TBitArray<> Used(false, C.Num());
				for (int32 a = 0; a < C.Num(); ++a)
				{
					if (Used[a]) continue;
					Used[a] = true;
					FVector2f Start(C[a].X, C[a].Y), End(C[a].Z, C[a].W);
					for (bool bMore = true; bMore && FVector2f::Distance(Start, End) < 1.f;)
					{
						bMore = false;
						for (int32 b = 0; b < C.Num() && !bMore; ++b)
						{
							if (Used[b]) continue;
							const FVector2f P(C[b].X, C[b].Y), Q(C[b].Z, C[b].W); constexpr float Nr = 0.002f;
							if (FVector2f::Distance(P, End) < Nr) { End = Q; bMore = true; }
							else if (FVector2f::Distance(Q, End) < Nr) { End = P; bMore = true; }
							else if (FVector2f::Distance(P, Start) < Nr) { Start = Q; bMore = true; }
							else if (FVector2f::Distance(Q, Start) < Nr) { Start = P; bMore = true; }
							if (bMore) Used[b] = true;
						}
					}
					if (FVector2f::Distance(Start, End) < 0.12f) continue;
					Segs[f].Add(FVector4f(Start.X, Start.Y, End.X, End.Y)); Glass[f].Add(Segs[f].Num() - 1);
				}
			}
		}
	}
	// written as plan metres per floor: [x0, z0, x1, z1, glass]
	FString J = FString::Printf(TEXT("{\"made\": \"%s\", \"parts\": %d, \"floors\": {"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M")), Kept);
	for (int32 f = 1; f <= 2; ++f)
	{
		TSet<int32> G(Glass[f]);
		J += FString::Printf(TEXT("%s\"%d\": ["), f > 1 ? TEXT(", ") : TEXT(""), f);
		for (int32 i = 0; i < Segs[f].Num(); ++i)
		{
			const FVector4f& S = Segs[f][i];
			J += FString::Printf(TEXT("%s[%.2f,%.2f,%.2f,%.2f,%d]"), i ? TEXT(",") : TEXT(""), S.X, S.Y, S.Z, S.W, G.Contains(i) ? 1 : 0);
		}
		J += TEXT("]");
	}
	J += TEXT("}");
	// (the jetways as they stand at the trace, for checking pictures only - the map draws them live)
	{
		TArray<FVector4f> Jw; for (TActorIterator<AJetwayRigs> It(W); It; ++It) { It->MapShapes(Jw); break; }
		J += TEXT(", \"jetways\": [");
		for (int32 k = 0; k < Jw.Num(); ++k) J += FString::Printf(TEXT("%s[%.2f,%.2f,%.2f,%.2f]"), k ? TEXT(",") : TEXT(""), Jw[k].X, Jw[k].Y, Jw[k].Z, Jw[k].W);
		J += TEXT("]}\n");
	}
	const FString Out = FPaths::ProjectDir() / TEXT("Data/holomap-walls.json");
	FFileHelper::SaveStringToFile(J, *Out);
	FFileHelper::SaveStringToFile(J, *(FPaths::ProjectContentDir() / TEXT("Data/holomap-walls.json")));
	ADP_LOG(TEXT("Map trace: %d of %d terminal parts are walls; Floor 1 %d lines, Floor 2 %d lines -> Data/holomap-walls.json"), Kept, Parts, Segs[1].Num(), Segs[2].Num());
	return true;
#else
	return false;
#endif
}
