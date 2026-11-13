// The clothes pipeline's editor helpers (2026-11-13), called from Tools/make_outfit.py: give a cloth or outfit asset
// its dataflow graph and terminal, set an outfit's sized sources (garment + the body it was made on), and regenerate.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ChaosOutfitAsset/SizedOutfitSource.h"
#include "ADPClothTools.generated.h"

UCLASS()
class ADPEDITORTOOLS_API UADPClothTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** The asset (a cloth asset or an outfit asset) made by this dataflow from this terminal node. */
	UFUNCTION(BlueprintCallable, Category = "ADP|Clothes")
	static bool SetDataflow(UObject* Asset, UObject* Dataflow, FName Terminal);

	/** An outfit's sizes: each a garment (cloth asset) and the body it was tailored on; then the outfit is made. */
	UFUNCTION(BlueprintCallable, Category = "ADP|Clothes")
	static bool SetSizedOutfitSources(UObject* Asset, FName Variable, const TArray<FChaosSizedOutfitSource>& Sources);

	/** Re-run the asset's dataflow (and the assets made from it). */
	UFUNCTION(BlueprintCallable, Category = "ADP|Clothes")
	static bool Regenerate(UObject* Asset);

	/** The asset's variables, as text (to check what the dataflow reads). */
	UFUNCTION(BlueprintCallable, Category = "ADP|Clothes")
	static FString DescribeDataflow(UObject* Asset);
};
