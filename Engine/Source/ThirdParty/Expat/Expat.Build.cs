// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Expat : ModuleRules
{
	public Expat(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string ExpatPackagePath = Target.UEThirdPartySourceDirectory + "Expat";

		if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			string IncludePath = Path.Combine(ExpatPackagePath, "expat-2.2.0/lib");
			PublicSystemIncludePaths.Add(IncludePath);

			// Use reflection to allow type not to exist if console code is not present
			string ToolchainName = "VS";
			System.Type XboxOnePlatformType = System.Type.GetType("UnrealBuildTool.XboxOnePlatform,UnrealBuildTool");
			if (XboxOnePlatformType != null)
			{
				System.Object VersionName = XboxOnePlatformType.GetMethod("GetVisualStudioCompilerVersionName").Invoke(null, null);
				ToolchainName += VersionName.ToString();
			}

			string ConfigPath = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release";
			string LibraryPath = Path.Combine(ExpatPackagePath, "expat-2.2.0", "XboxOne", ToolchainName, ConfigPath);

			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "expat.lib"));
		}
    }
}
