// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Landscape : ModuleRules
{
	public Landscape(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/Engine/Private", // for Engine/Private/Collision/PhysXCollision.h
				"Runtime/Landscape/Private"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"TargetPlatform",
				"DerivedDataCache",
				"Foliage",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"Engine",
				"RenderCore", 
				"RHI",
				"ShaderCore",
				"Renderer",
				"Foliage",
			}
		);

		SetupModulePhysXAPEXSupport(Target);
		if (Target.bCompilePhysX && Target.bBuildEditor)
		{
			DynamicallyLoadedModuleNames.Add("PhysXCooking");
		}

		if (Target.bBuildDeveloperTools && Target.Type != TargetType.Server)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"RawMesh"
				}
			);
		}

		if (Target.bBuildEditor == true)
		{
			// TODO: Remove all landscape editing code from the Landscape module!!!
			PrivateIncludePathModuleNames.Add("LandscapeEditor");

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"UnrealEd",
					"MaterialUtilities", 
					"SlateCore",
					"Slate",
				}
			);

			CircularlyReferencedDependentModules.AddRange(
				new string[] {
					"UnrealEd",
					"MaterialUtilities",
				}
			);
		}
	}
}
