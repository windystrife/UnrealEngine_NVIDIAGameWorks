// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using Microsoft.Win32;

namespace UnrealBuildTool.Rules
{
	public class VisualStudioCodeSourceCodeAccess : ModuleRules
	{
        public VisualStudioCodeSourceCodeAccess(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"SourceCodeAccess",
					"DesktopPlatform",
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.Add("HotReload");
			}

			bool bHasVisualStudioDTE;
			try
			{
				// Interrogate the Win32 registry
				string DTEKey = null;
				switch (Target.WindowsPlatform.Compiler)
				{
					case WindowsCompiler.VisualStudio2017:
						DTEKey = "VisualStudio.DTE.15.0";
						break;
					case WindowsCompiler.VisualStudio2015:
						DTEKey = "VisualStudio.DTE.14.0";
						break;
				}
				bHasVisualStudioDTE = RegistryKey.OpenBaseKey(RegistryHive.ClassesRoot, RegistryView.Registry32).OpenSubKey(DTEKey) != null;
			}
			catch
			{
				bHasVisualStudioDTE = false;
			}

			if (bHasVisualStudioDTE)
			{
				Definitions.Add("VSACCESSOR_HAS_DTE=1");
			}
			else
			{
				Definitions.Add("VSACCESSOR_HAS_DTE=0");
			}

			bBuildLocallyWithSNDBS = true;
		}
	}
}
