// The game itself (Windows).
using UnrealBuildTool;

public class AirportDynastyProTarget : TargetRules
{
	public AirportDynastyProTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("AirportDynastyPro");
	}
}
