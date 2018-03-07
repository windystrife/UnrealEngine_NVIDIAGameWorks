// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DerivedDataCache : ModuleRules
{
	public DerivedDataCache(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("Core");

		// Internal (NotForLicensees) module
		var DDCUtilsModule = Path.Combine("Developer", "NotForLicensees", "DDCUtils", "DDCUtils.Build.cs");
		if (File.Exists(DDCUtilsModule))
		{
			DynamicallyLoadedModuleNames.Add("DDCUtils");
		}
	}
}
