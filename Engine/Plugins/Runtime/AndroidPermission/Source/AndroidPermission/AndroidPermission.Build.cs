// Copyright 2017 Google Inc.

using UnrealBuildTool;
using System.IO;

public class AndroidPermission : ModuleRules
{
	public AndroidPermission(ReadOnlyTargetRules Target) : base(Target)
	{
		
		PublicIncludePaths.AddRange(
			new string[] {
				"AndroidPermission/Public",
				"Runtime/Launch/Public"
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				"AndroidPermission/Private",
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
            AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "AndroidPermission_APL.xml")));
        }
    }
}
