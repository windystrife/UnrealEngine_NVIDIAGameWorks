namespace UnrealBuildTool.Rules
{
	public class NvFlow : ModuleRules
	{
		public NvFlow(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"NvFlow/Private",
					"../../../../Source/Runtime/Engine/Private",
 					"../../../../Source/Runtime/Renderer/Private",
                    "../Include/include",
					// ... add other private include paths required here ...
				}
				);

            PublicIncludePaths.AddRange(
                new string[] { 
                    "NvFlow/Public",
                }
            );

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"RHI",
					"RenderCore",
					"Renderer",
					"ShaderCore",
                    "XmlParser",
//     				"UnrealEd",
// 				    "AssetTools",
// 				    "ContentBrowser",
//                     "EditorStyle"
				}
				);

            PublicDependencyModuleNames.AddRange(
                new string[] 
                {
					"RenderCore",
					"Renderer"
                }
            );

// 		    PrivateIncludePathModuleNames.AddRange(
// 			    new string[] {
// 				    "AssetTools",
// 				    "AssetRegistry"
// 			    });
// 
// 		    DynamicallyLoadedModuleNames.AddRange(
// 			    new string[] {
// 				    "AssetTools",
// 				    "AssetRegistry"
// 			    });

            Definitions.Add("WITH_NVFLOW=1");

            string libbase = Target.UEThirdPartySourceDirectory + "../../Plugins/GameWorks/NvFlow/Include";

            if (Target.Platform == UnrealTargetPlatform.Win32)
            {
                PublicLibraryPaths.Add(libbase + "/win32/");
                PublicAdditionalLibraries.Add("NvFlowLibRelease_win32.lib");
                PublicDelayLoadDLLs.Add("NvFlowLibRelease_win32.dll");
                RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Plugins/GameWorks/NvFlow/Libraries/win32/NvFlowLibRelease_win32.dll"));
            }
            else if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                PublicLibraryPaths.Add(libbase + "/win64/");
                PublicAdditionalLibraries.Add("NvFlowLibRelease_win64.lib");
                PublicDelayLoadDLLs.Add("NvFlowLibRelease_win64.dll");
                RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Plugins/GameWorks/NvFlow/Libraries/win64/NvFlowLibRelease_win64.dll"));
            }

            // Add direct rendering dependencies on a per-platform basis
            if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
            {
                PrivateDependencyModuleNames.AddRange(new string[] { "DX11", "DX12" });
                PrivateIncludePaths.AddRange(
                    new string[] {
                        // ... add other private include paths required here ...
                        }
                    );
            }

            SetupModulePhysXAPEXSupport(Target);
        }
	}
}