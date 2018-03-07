using UnrealBuildTool;
using System.IO;

public class OSVR : ModuleRules
{
    public OSVR(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePathModuleNames.Add("TargetPlatform");

        PrivateIncludePaths.AddRange(
            new string[] {
				"OSVR/Private",
				// ... add other private include paths required here ...
			}
            );

        PrivateDependencyModuleNames.AddRange(
            new string[]
			{
				"OSVRClientKit",
                "Core",
				"CoreUObject",      // Provides Actors and Structs
				"Engine",           // Used by Actor
				"Slate",            // Used by InputDevice to fire bespoke FKey events
				"InputCore",        // Provides LOCTEXT and other Input features
				"InputDevice",      // Provides IInputInterface
				"RHI",
				"RenderCore",
				"Renderer",
				"ShaderCore",
				"HeadMountedDisplay",
				"Json"
				// ... add private dependencies that you statically link with here ...
			}
            );
        if(Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }

        if(Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.AddRange(new string[] { "D3D11RHI" });

            // Required for some private headers needed for the rendering support.
            var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
            PrivateIncludePaths.AddRange(
                new string[] {
                            Path.Combine(EngineDir, @"Source\Runtime\Windows\D3D11RHI\Private"),
                            Path.Combine(EngineDir, @"Source\Runtime\Windows\D3D11RHI\Private\Windows")
    				        });
        }
    }
}
