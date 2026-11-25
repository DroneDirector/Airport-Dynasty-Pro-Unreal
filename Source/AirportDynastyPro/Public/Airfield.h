// Your airport's airfield (Data/airfield.json, from the Android edition's airfield plan, the same axes and origin as
// the terminal): the grass inside the boundary, the aprons, runway 08/26 with its markings, the taxiways with their
// yellow centre lines, the holding positions and the cargo building, the hangar apron with each hangar's lead-in line
// (the hangars themselves: Hangar.h). The data is shared with the traffic.
// The ground (owner, 2026-11-12): one smooth grade the whole airfield is built on. The runway rolls gently along its
// length as real runways do (both ends a little low, a crest past the middle, slopes about 0.3%) with a slight crown
// across it for the rain, the taxiways and the grass beside it follow, and it all blends to the level aprons by the
// buildings and to the globe's ground at the boundary. Every paved surface, the grass and every marking is a mesh
// draped on that grade (never a flat slab that could float or cut in), the aircraft and vehicles ride on it, and
// anything built later on the airfield asks GroundAt for its height.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Airfield.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UProceduralMeshComponent;
class FJsonObject;

#pragma region Airfield data

struct AIRPORTDYNASTYPRO_API FAirfieldData
{
	struct FTwy { FString Id, Kind; TArray<FVector2f> Pts; float W = 25; };
	/** A stand's own frame (to-do 277): the aircraft's nose wheel on its stop, and which way it faces. Everything placed or driven
	 *  relative to a stand goes through it, so one facing +y (the remote and the cargo stands) is the terminal stands' turned half
	 *  round. Side and Aft are metres as on a TERMINAL stand (nose to -y): Side along +x there, Aft along +y, towards the tail. */
	struct FStandFrame
	{
		float X = 0, Y = 0, K = 1;   // (plan m: the nose wheel; K 1 nose to -y, -1 nose to +y)
		FVector2f P(float Side, float Aft) const { return FVector2f(X + K * Side, Y + K * Aft); }
		/** Terminal-way plan metres (as if the stand were a terminal one standing here) to the real ones, and back: a half turn round the nose wheel. */
		FVector2f Turn(const FVector2f& At) const { return K > 0.f ? At : FVector2f(2.f * X - At.X, 2.f * Y - At.Y); }
		FVector2f Way(float Hx, float Hy) const { return FVector2f(K * Hx, K * Hy); }                       // (a direction, as on a terminal stand)
		FVector2f Local(const FVector2f& At) const { return FVector2f(K * (At.X - X), K * (At.Y - Y)); }   // (a plan point's Side and Aft)
		FVector2f Face() const { return FVector2f(0.f, -K); }                                               // (the way the nose points)
	};
	/** The size every aircraft is drawn and measured as today (the A320): where it stops on a stand with a stop for each size. */
	static constexpr TCHAR DrawnSize = 'C';
	struct FStand
	{
		FString Id; float X = 0, Nose = 0, Dir = -1, Span = 36, Len = 44.5f; bool bJetway = false, bCargo = false, bRemote = false; TCHAR Code = 'C';   // (Code: the largest ICAO code letter it takes)
		/** A remote stand's own stop for each size of aircraft (Nose: the one for the size the game draws), where its paint ends, and the sizes whose stop leaves its tug room (airfield.json: stops, tug). */
		TMap<TCHAR, float> Stops; float NoseEnd = 0; FString TugSizes;
		bool TugRoom(TCHAR Size) const { int32 At; return !bRemote || TugSizes.FindChar(Size, At); }
		/** Its own pushback tug, GPU and chocks: every stand - but a remote one where the drawn size's own stop leaves no room for a tug before the bus road. */
		bool HasTug() const { return TugRoom(DrawnSize); }
		/** Where its tug stands on the centre line in front of the nose (plan y), and its loop's radius (a big stand's: its bar's tip swings 2.7 m out, clear of the jetway's red zone 3 m off); 0: in the box beside it (a C gate with a jetway). */
		float TugHead(float& Radius) const { Radius = bRemote || (bJetway && Span <= 40.f) ? 6.f : bCargo ? 9.5f : Span > 40.f ? 12.f : 8.f; return bRemote ? Frame().Y - 17.f : bCargo ? Frame().Y - 15.1f : bJetway && Span <= 40.f ? 0.f : Span > 40.f ? Nose - 8.4f : Nose - 18.f; }
		FStandFrame Frame() const { FStandFrame Fr; Fr.X = X; Fr.K = Dir < 0 ? 1.f : -1.f; Fr.Y = Nose + Fr.K * 5.1f; return Fr; }   // (the nose wheel stops 5.1 m behind the stop line)
	};
	/** The stand the simulation's StandX means (no two stands share an x), nullptr when none. */
	const FStand* StandAt(float StandX) const { return Stands.FindByPredicate([&](const FStand& S) { return FMath::Abs(S.X - StandX) < 1.f; }); }
	/** The red boundary lines along the stands' sides, and the jetways' red zones, as drawn. */
	struct FStandLine { const FStand* Stand; FVector2f A, B; };
	void StandLines(bool bCargo, TArray<FStandLine>& Lines, TArray<TPair<const FStand*, FBox2f>>& Zones) const;
	/** -RemoteStands: the remote stands count as built (testing, to-do 277). */
	static bool RemoteOn();
	struct FRect { float X0, Y0, X1, Y1; FString Id; };
	/** A hangar's place: its centre line X (on its lead-in line), the apron edge Y its threshold meets, its size, door. */
	struct FHangarSite { FString Id, Size; float X = 0, Y = 0; bool bOpen = false; };

	float RwyX0 = -1919, RwyX1 = 2081, RwyY = 646, RwyW = 60;
	float AY = 386, BY = 906, TY = 206, KY = 1086;
	TArray<FTwy> Taxiways;
	TArray<FStand> Stands;   // (only the built ones: "built": false in the layout is left out here, so nothing that uses stands sees it)
	TArray<FVector2f> Holds;
	TArray<FRect> Aprons;
	TArray<FHangarSite> Hangars;
	/** The service roads for the ground vehicles (Data/airfield.json roads): centre line, width, a road-holding position at its end. */
	struct FRoad { FString Id; float W = 7; TArray<FVector2f> Pts; bool bHold = false; };
	TArray<FRoad> Roads;
	/** Paved lots (the tower's, the fire station's forecourt): a rectangle and a row of parking spaces along one side. */
	struct FLot { FString Id; float X0, Y0, X1, Y1, BayX0 = 0, BayY = 0, BayW = 2.6f, BayD = 5.2f, BayDir = 1; int32 Bays = 0; bool bPaved = true, bOnModel = false; float Lift = -0.03f; FString Surface = TEXT("asphalt"); };
	TArray<FLot> Lots;
	/** Level building pads (the fire station's): the ground flat at Z inside, back to the natural grade over Blend metres. */
	struct FPad { FString Id; float X0, Y0, X1, Y1, Z = 0, Blend = 30; };
	TArray<FPad> Pads;
	/** The remote stands R0-R7 as drawn (to-do 277, AirfieldRemote.cpp): pieces of paving, paint, texts, signs, blue lights, masts. */
	struct FRemote
	{
		struct FPoly { FName Mat; bool bRing = false; TArray<FVector2f> Outer, Pts; TArray<int32> Tri; TArray<TArray<FVector2f>> Runs; FVector2f Min = FVector2f::ZeroVector, Max = FVector2f::ZeroVector; };
		struct FPaint { FName Mat; float W, Up; TArray<FVector2f> Pts; };
		struct FText { FString T; float X, Y, H, Yaw; bool bWhite; };
		struct FLight { FVector2f At, Dir; };
		TArray<FPoly> Polys; TArray<FPaint> Paint; TArray<FText> Texts, Signs; TArray<FLight> Lights; TArray<FVector2f> Masts; TArray<float> LinkX;
		float LinkHalf = 20, RampTo = 30;
	};
	FRemote Remote;
	void LoadRemote(const TSharedPtr<FJsonObject>& Json);
	/** On the remote stands' paving (or within Margin of it). */
	bool OnRemote(const FVector2f& P, float Margin = 0.f) const;
	FRect Boundary{-2000, -170, 2240, 1320}, Cargo{200, 1231, 590, 1301};
	double Lat = 63.457988, Lon = -18.713703;
	float ElevFt = 130, Heading = 68;
	FString RwyEnd0 = TEXT("08"), RwyEnd1 = TEXT("26");   // the runway's names at its x0 and x1 ends (the site's own: Site.h)

	bool Load();
	const FTwy* Find(const FString& Id) const { return Taxiways.FindByPredicate([&](const FTwy& T) { return T.Id == Id; }); }
};

#pragma endregion

UCLASS()
class AIRPORTDYNASTYPRO_API AAirfield : public AActor
{
	GENERATED_BODY()

public:
	AAirfield();
	/** Lays out the airfield; with bModelSite the Babylon model already has the site round the terminal (aprons, kerb). */
	bool Build(bool bModelSite = false);
	const FAirfieldData& Data() const { return D; }

	#pragma region The grade
	/** The airfield's data, loaded once for everything that needs it (some of it before the airfield is built). */
	static const FAirfieldData& Shared();
	/** How high the ground stands at a plan point, metres above the airport's level (the aprons' level). */
	static float GroundAt(float X, float Y);
	/** The ground's rise per metre there, along +x and along +y. */
	static FVector2f SlopeAt(float X, float Y);
	#pragma endregion

private:
	#pragma region Pieces
	UInstancedStaticMeshComponent* Kit(const FName Mat);
	/** A flat slab between two corners (plan x, y on the airfield) from Bottom to Top metres. */
	void Slab(const FName Mat, float X0, float Y0, float X1, float Y1, float Bottom, float Top);
	/** A strip along a line (a taxiway, a marking), W wide, its top at Top metres. */
	void Strip(const FName Mat, const FVector2f& A, const FVector2f& B, float W, float Top, float Thick = 0.02f);
	// the draped surfaces: a sheet per material, every quad or band cut into cells of Step metres whose corners sit on
	// the grade, Lift metres over it (pavements a little below zero, markings a little above their pavement)
	struct FSheet { TArray<FVector> V; TArray<int32> I; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> C; };
	TMap<FName, FSheet> Sheets;
	void Quad(const FName Mat, float X0, float Y0, float X1, float Y1, float Lift, float Step = 10.f, const FLinearColor& Col = FLinearColor::White);
	void Band(const FName Mat, const FVector2f& A, const FVector2f& B, float W, float Lift, float Step = 10.f, const FLinearColor& Col = FLinearColor::White);
	void Vertex(FSheet& S, float X, float Y, float Lift, const FLinearColor& Col = FLinearColor::White, const FVector2D* UVOverride = nullptr);
	// the soft edge where a pavement meets the ground (owner, 2026-11-12, Art/Edges, Tools/make_edge_textures.py): a
	// see-through band 2.6 m wide laid along the edge, 1.2 m onto the pavement and 1.4 m onto the ground, its picture
	// chosen by the pavement (concrete: a soil seam and grime creeping in; asphalt: crumbs and cracks; the runway's
	// ends: bigger cracks), cross-fading over its last metres so bands that meet merge instead of cutting. A and B
	// run along the edge, Out points off the pavement; InnerLift is the pavement's top, OuterLift the ground's.
	void EdgeBand(const FVector2f& A, const FVector2f& B, const FVector2f& Out, float InnerLift, float OuterLift, float Asphalt, float End, bool bFadeA = true, bool bFadeB = true);
	/** The four edges of a rectangle (x0..x1, y0..y1) that meet the ground; Sides: bits 1 south (y0), 2 north (y1), 4 west, 8 east. */
	void EdgeRect(float X0, float Y0, float X1, float Y1, float InnerLift, float Asphalt, int32 Sides = 15, float EndsAtX = 0.f);
	/** Every pavement edge of the airfield, laid after the pavements. */
	void EdgeBands(bool bModelSite);
	/** The taxiways as one smooth pavement (owner, 2026-11-13: "how the corners of a taxiway are supposed to be"): turn
	 *  curves at every junction, fillets where they meet the runway and the aprons, shoulders, the edge stripes and the
	 *  soft grass edge all round the airfield's paving, all from one distance field so nothing meets in square blocks. */
	void PaveTaxiways(bool bModelSite);
	/** The aprons laid from the layout (the terminal's too, with the Babylon model's own surface picture, so they can be
	 *  resized), with every stand's markings from the stand list (ICAO Annex 14 5.2.13 / FAA): lead-in lines and their
	 *  turns, stop bars, stand numbers, red stand boundaries and equipment restraint lines, the service road behind the
	 *  stands, the red jetway zones, and each stand's own wear (engine soot, drips where the wheels stop, faint tyre marks
	 *  on some). */
	void Aprons(bool bModelSite);
	/** The service roads (owner, 2026-11-13: gate 1 to the tower, the fire station to the apron and onto taxiway A): asphalt
	 *  on the grade, rounded bends, white edge lines and a dashed middle, soft edges into the grass, a road-holding position
	 *  (stop line and STOP) where one meets a taxiway. */
	void ServiceRoads(bool bModelSite);
	/** A soft edge band along a curved edge: P the points along it, Out their outward normals, Flags (asphalt, end). */
	void EdgePoly(const TArray<FVector2f>& P, const TArray<FVector2f>& Out, const TArray<FVector2f>& Flags, const TArray<float>& InnerLift, bool bFadeEnds, const TArray<float>* EdgeLift = nullptr);
	/** The remote stands, their bus road, depot and bus bays (AirfieldRemote.cpp, to-do 277). */
	void RemoteApron(bool bModelSite);
	void Tri(FSheet& S, int32 A, int32 B, int32 C);
	void FlushSheets();
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> SheetMeshes;
	#pragma endregion

	FAirfieldData D;
	float Sink = 0; // with the Babylon model: the whole airfield 25 cm lower, so the model's own ground always wins
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> Kits;
};
