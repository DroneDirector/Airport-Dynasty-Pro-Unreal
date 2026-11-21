// The real trees round the airport (to-do 200; owner, 2026-11-20 night: "place it where trees are actually supposed to be ...
// you can see a top-down view of the trees on the satellite photo ... and I also want it to be seasonal").
// Data/Sites/<site>/trees.bin (Tools/make_site_trees.py): every tree found in swisstopo's surface model and the 10 cm photo
// - its place, the ground under it, its real height and crown, conifer or broadleaf. Drawn as our own Blender trees
// (Art/Trees: Norway spruce, silver fir, beech), instanced and Nanite, each scaled to its real height:
//  - in 1 km blocks streamed round you (full trees out to TreeRadiusKm, the rest of the forests are the 10 cm photo);
//  - seasonal by the airport's own date: beech bare from December to March, fresh green in April-May, summer green,
//    yellow-orange-brown in autumn; spruce and fir always green (a little darker in winter); snow on the branches with
//    the real snow (the snow layer, to-do 193);
//  - never on our roads, car parks or the airport (the tool keeps them clear).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SiteTrees.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;

UCLASS()
class AIRPORTDYNASTYPRO_API ASiteTrees : public AActor
{
	GENERATED_BODY()
public:
	ASiteTrees();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	/** How far the full trees reach round you (km). */
	static float TreeRadiusKm;
	int32 NumTrees() const { return Count; }

private:
	struct FTree { float X, Y, Z, H, R, Kind; };
	TArray<FTree> Trees;
	struct FBlock { int32 First = 0, Num = 0; FVector2D Centre; bool bIn = false; TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Parts; };
	TMap<FIntPoint, FBlock> Blocks;
	UPROPERTY() TArray<TObjectPtr<UStaticMesh>> Meshes;   // Spruce_A, Spruce_B, Fir_A, Beech_A, BeechBare_A
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Mats;   // needles, conifer bark, leaves, beech bark
	int32 Count = 0;
	float Clock = 0.f;
	int32 SeasonDay = -1;
	bool Load();
	void Stream(bool bAll);
	void Show(const FIntPoint& Key, FBlock& B);
	void Hide(FBlock& B);
	void Season();
};
