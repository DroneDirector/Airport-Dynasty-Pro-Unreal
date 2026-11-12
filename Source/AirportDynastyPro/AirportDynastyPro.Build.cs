// The game's code module and what it uses from the engine.
using System.IO;
using System.Text.RegularExpressions;
using UnrealBuildTool;

public class AirportDynastyPro : ModuleRules
{
	public AirportDynastyPro(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// the PC version's number (pc-version.json), shown on the title screen
		string Version = "0.0.0";
		string VersionFile = Path.Combine(ModuleDirectory, "..", "..", "pc-version.json");
		if (File.Exists(VersionFile)) { Match M = Regex.Match(File.ReadAllText(VersionFile), "\"version\"\\s*:\\s*\"([^\"]+)\""); if (M.Success) Version = M.Groups[1].Value; }
		ExternalDependencies.Add(VersionFile);   // a new number rebuilds
		PublicDefinitions.Add("ADP_PC_VERSION=\"" + Version + "\"");
		PublicSystemLibraries.AddRange(new string[] { "sapi.lib", "ole32.lib", "shlwapi.lib", "shell32.lib" });   // Windows' voices (announcer fallback)
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Json", "JsonUtilities", "MassEntity", "Slate", "SlateCore", "RHI", "RenderCore", "WebBrowser", "CesiumRuntime", "ImageWrapper", "HTTP", "NNE", "ProceduralMeshComponent", "ADPFrameGen", "HairStrandsCore" });
	}
}
