// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GeometryCacheEd : ModuleRules
{
	public GeometryCacheEd(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicIncludePaths.Add("Runtime/GeometryCacheEd/Classes");
        PublicIncludePaths.Add("Runtime/GeometryCacheEd/Public");
        PrivateIncludePaths.Add("Runtime/GeometryCacheEd/Private");
        
        PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "InputCore",
                "RenderCore",
                "ShaderCore",
                "RHI",		
                "UnrealEd",
				"AssetTools",
                "GeometryCache"
			}
		);
	}
}
