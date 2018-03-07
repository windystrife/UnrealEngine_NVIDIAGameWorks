// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class libstrophe : ModuleRules
{
	public libstrophe(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string StrophePackagePath = Target.UEThirdPartySourceDirectory + "libstrophe";

		if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			Definitions.Add("WITH_XMPP_STROPHE=1");
			Definitions.Add("XML_STATIC");

			string IncludePath = Path.Combine(StrophePackagePath, "libstrophe-0.9.1");
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
			string LibraryPath = Path.Combine(StrophePackagePath, "libstrophe-0.9.1", "XboxOne", ToolchainName, ConfigPath);

			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "strophe.lib"));

			AddEngineThirdPartyPrivateStaticDependencies(Target, "Expat");
		}
		else
		{
			Definitions.Add("WITH_XMPP_STROPHE=0");
		}
    }
}
