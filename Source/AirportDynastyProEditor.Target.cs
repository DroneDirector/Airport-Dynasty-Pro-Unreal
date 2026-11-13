// The game inside the Unreal editor.
using UnrealBuildTool;

public class AirportDynastyProEditorTarget : TargetRules
{
	public AirportDynastyProEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("AirportDynastyPro");
		ExtraModuleNames.Add("ADPEditorTools");   // editor-only helpers for the content scripts
	}
}
