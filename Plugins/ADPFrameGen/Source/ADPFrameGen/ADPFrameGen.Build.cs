using UnrealBuildTool;

public class ADPFrameGen : ModuleRules
{
	public ADPFrameGen(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "RHI" });
		PublicSystemLibraries.AddRange(new string[] { "d3d12.lib", "dxgi.lib", "d3dcompiler.lib" });
	}
}
