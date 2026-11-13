// Editor-only helpers for the content scripts (Tools/*.py): what Unreal's Python can't reach yet, e.g. a dataflow
// asset's struct-array variables (the clothes' resizable outfits). Never part of the game.
using UnrealBuildTool;

public class ADPEditorTools : ModuleRules
{
	public ADPEditorTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DataflowEngine", "DataflowCore", "ChaosClothAssetEngine", "ChaosOutfitAssetEngine" });
	}
}
