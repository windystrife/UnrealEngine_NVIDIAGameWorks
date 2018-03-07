// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AppFramework : ModuleRules
{
	public AppFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
                "CoreUObject",
				"ApplicationCore",
				"Slate",
				"SlateCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"InputCore",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/AppFramework/Private",
                "Runtime/AppFramework/Private/Framework",
                "Runtime/AppFramework/Private/Framework/Testing",
				"Runtime/AppFramework/Private/Widgets",
                "Runtime/AppFramework/Private/Widgets/Colors",
                "Runtime/AppFramework/Private/Widgets/Input",
                "Runtime/AppFramework/Private/Widgets/Testing",
                "Runtime/AppFramework/Private/Widgets/Workflow",
			}
		);

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"SlateReflector",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"SlateReflector",
				}
			);
		}
	}
}
