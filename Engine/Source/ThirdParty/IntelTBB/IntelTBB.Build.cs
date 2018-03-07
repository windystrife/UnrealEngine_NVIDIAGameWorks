// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IntelTBB : ModuleRules
{
	public IntelTBB(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
		{
			string IntelTBBPath = Target.UEThirdPartySourceDirectory + "IntelTBB/";
			switch (Target.WindowsPlatform.Compiler)
			{
				case WindowsCompiler.VisualStudio2017:
				case WindowsCompiler.VisualStudio2015: IntelTBBPath += "IntelTBB-4.4u3/"; break;
			}

			PublicSystemIncludePaths.Add(IntelTBBPath + "Include");

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				if (Target.WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 || Target.WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2017)
				{
					PublicLibraryPaths.Add(IntelTBBPath + "lib/Win64/vc14");
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Win32)
			{
				if (Target.WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 || Target.WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2017)
				{
					PublicLibraryPaths.Add(IntelTBBPath + "lib/Win32/vc14");
				}
			}

			// Disable the #pragma comment(lib, ...) used by default in MallocTBB...
			// We want to explicitly include the library.
			Definitions.Add("__TBBMALLOC_BUILD=1");

			string LibName = "tbbmalloc";
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				LibName += "_debug";
			}
			LibName += ".lib";
			PublicAdditionalLibraries.Add(LibName);
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicSystemIncludePaths.AddRange(
				new string[] {
					Target.UEThirdPartySourceDirectory + "IntelTBB/IntelTBB-4.0/include",
				}
			);

			PublicAdditionalLibraries.AddRange(
				new string[] {
					Target.UEThirdPartySourceDirectory + "IntelTBB/IntelTBB-4.0/lib/Mac/libtbb.a",
					Target.UEThirdPartySourceDirectory + "IntelTBB/IntelTBB-4.0/lib/Mac/libtbbmalloc.a",
				}
			);
		}
	}
}
