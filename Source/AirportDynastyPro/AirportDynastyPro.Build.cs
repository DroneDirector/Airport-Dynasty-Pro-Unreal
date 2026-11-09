// The game's code module and what it uses from the engine.
using UnrealBuildTool;

public class AirportDynastyPro : ModuleRules
{
	public AirportDynastyPro(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Json", "JsonUtilities", "MassEntity" });
	}
}
