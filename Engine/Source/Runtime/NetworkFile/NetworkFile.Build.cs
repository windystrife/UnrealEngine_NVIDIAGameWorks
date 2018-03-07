// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NetworkFile : ModuleRules
{
	public NetworkFile(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("DerivedDataCache");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Sockets"
			});

		PublicIncludePaths.Add("Runtime/CoreUObject/Public/Interfaces");
		PublicIncludePaths.Add("Runtime/CoreUObject/Public/UObject");
		PublicIncludePaths.Add("Runtime/CoreUObject/Public");

		if (!Target.bBuildRequiresCookedData)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					"DerivedDataCache",
				});
		}

		if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			Definitions.Add("ENABLE_HTTP_FOR_NF=1");
			PrivateDependencyModuleNames.Add("HTML5JS");
		}
		else
		{
			Definitions.Add("ENABLE_HTTP_FOR_NF=0");
		}
	}
}
