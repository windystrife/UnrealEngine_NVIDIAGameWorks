// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class WebBrowser : ModuleRules
{
	public WebBrowser(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/WebBrowser/Public");
		PrivateIncludePaths.Add("Runtime/WebBrowser/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"RHI",
				"InputCore",
				"Slate",
				"SlateCore",
				"Serialization",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// We need this one on Android for URL decoding
			PrivateDependencyModuleNames.Add("HTTP");
		}

		if (Target.Platform == UnrealTargetPlatform.Win64
		||  Target.Platform == UnrealTargetPlatform.Win32
		||  Target.Platform == UnrealTargetPlatform.Mac
		||  Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateDependencyModuleNames.Add("CEF3Utils");
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"CEF3"
				);

			if (Target.Type != TargetType.Server)
			{
				if (Target.Platform == UnrealTargetPlatform.Mac)
				{
					// Add contents of UnrealCefSubProcess.app directory as runtime dependencies
					foreach (string FilePath in Directory.EnumerateFiles(Target.RelativeEnginePath + "/Binaries/Mac/UnrealCEFSubProcess.app", "*", SearchOption.AllDirectories))
					{
						RuntimeDependencies.Add(new RuntimeDependency(FilePath));
					}
				}
				else if (Target.Platform == UnrealTargetPlatform.Linux)
				{
					RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/" + Target.Platform.ToString() + "/UnrealCEFSubProcess"));
				}
				else
				{
					RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/" + Target.Platform.ToString() + "/UnrealCEFSubProcess.exe"));
				}
			}
		}

		if (Target.Platform == UnrealTargetPlatform.PS4 &&
			Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
		}

		bEnableShadowVariableWarnings = false;
	}
}
