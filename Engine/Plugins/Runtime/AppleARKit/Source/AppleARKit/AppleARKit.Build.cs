using UnrealBuildTool;

public class AppleARKit : ModuleRules
{
	public AppleARKit(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
				"AppleARKit/Public"
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				"AppleARKit/Private",
				"../../../../Source/Runtime/Renderer/Private",
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Slate",
				"SlateCore",
				"RHI",
                "Renderer",
                "RenderCore",
                "ShaderCore",
                "HeadMountedDisplay",
                "IOSRuntimeSettings",
                "AugmentedReality"
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			Definitions.Add("ARKIT_SUPPORT=1");
			PublicFrameworks.Add( "ARKit" );
		}
		else
		{
			Definitions.Add("ARKIT_SUPPORT=0");
		}
	}
}
