namespace UnrealBuildTool.Rules
{
	public class OverlayEditor : ModuleRules
	{
		public OverlayEditor(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicDependencyModuleNames.AddRange(
                new string[] {
				    "Core",
                    "CoreUObject",
                    "Overlay",
                }
            );

            PrivateDependencyModuleNames.AddRange(
                new string[] {
					"UnrealEd",
                    "Engine",
                }
            );
			
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"AssetTools",
				}
			);
			
            PublicIncludePaths.AddRange(
                new string[]
                {
                    "Editor/OverlayEditor/Public",
                }
            );

			PrivateIncludePaths.AddRange(
				new string[] {
					"Editor/OverlayEditor/Private",
					"Editor/OverlayEditor/Private/Factories",
				}
			);

            PrivateIncludePathModuleNames.AddRange(
                new string[] {
                    "AssetTools",
                }
            );

            if (Target.bBuildEditor)
            {
                PrivateIncludePathModuleNames.Add("TargetPlatform");
            }
        }
	}
}