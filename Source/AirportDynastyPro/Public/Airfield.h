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

#pragma region Airfield data

struct AIRPORTDYNASTYPRO_API FAirfieldData
{
	struct FTwy { FString Id, Kind; TArray<FVector2f> Pts; float W = 25; };
	struct FStand { FString Id; float X = 0, Nose = 0, Dir = -1, Span = 36, Len = 44.5f; bool bJetway = false, bCargo = false; };
	struct FRect { float X0, Y0, X1, Y1; FString Id; };
	/** A hangar's place: its centre line X (on its lead-in line), the apron edge Y its threshold meets, its size, door. */
	struct FHangarSite { FString Id, Size; float X = 0, Y = 0; bool bOpen = false; };

	float RwyX0 = -1919, RwyX1 = 2081, RwyY = 646, RwyW = 60;
	float AY = 386, BY = 906, TY = 206, KY = 1086;
	TArray<FTwy> Taxiways;
	TArray<FStand> Stands;
	TArray<FVector2f> Holds;
	TArray<FRect> Aprons;
	TArray<FHangarSite> Hangars;
	/** The service roads for the ground vehicles (Data/airfield.json roads): centre line, width, a road-holding position at its end. */
	struct FRoad { FString Id; float W = 7; TArray<FVector2f> Pts; bool bHold = false; };
	TArray<FRoad> Roads;
	/** Paved lots (the tower's, the fire station's forecourt): a rectangle and a row of parking spaces along one side. */
	struct FLot { FString Id; float X0, Y0, X1, Y1, BayX0 = 0, BayY = 0, BayW = 2.6f, BayD = 5.2f, BayDir = 1; int32 Bays = 0; bool bPaved = true, bOnModel = false; };
	TArray<FLot> Lots;
	FRect Boundary{-2000, -170, 2240, 1320}, Cargo{200, 1231, 590, 1301};
	double Lat = 63.457988, Lon = -18.713703;
	float ElevFt = 130, Heading = 68;

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
	void EdgePoly(const TArray<FVector2f>& P, const TArray<FVector2f>& Out, const TArray<FVector2f>& Flags, const TArray<float>& InnerLift, bool bFadeEnds);
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
