// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System;
using System.IO;

public class ICU : ModuleRules
{
	enum EICULinkType
	{
		None,
		Static,
		Dynamic
	}

	public ICU(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bNeedsDlls = false;

		string ICUVersion = "icu4c-53_1";
		string ICURootPath = Target.UEThirdPartySourceDirectory + "ICU/" + ICUVersion + "/";

		// Includes
		PublicSystemIncludePaths.Add(ICURootPath + "include" + "/");

		string PlatformFolderName = Target.Platform.ToString();

		string TargetSpecificPath = ICURootPath + PlatformFolderName + "/";

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			string VSVersionFolderName = "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			TargetSpecificPath += VSVersionFolderName + "/";

			string[] LibraryNameStems =
			{
				"dt",   // Data
				"uc",   // Unicode Common
				"in",   // Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};
			string LibraryNamePostfix = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ?
				"d" : string.Empty;

			// Library Paths
			PublicLibraryPaths.Add(TargetSpecificPath + "lib" + "/");

			EICULinkType ICULinkType = (Target.LinkType == TargetLinkType.Monolithic)? EICULinkType.Static : EICULinkType.Dynamic;
			switch(ICULinkType)
			{
			case EICULinkType.Static:
				foreach (string Stem in LibraryNameStems)
				{
					string LibraryName = "sicu" + Stem + LibraryNamePostfix + "." + "lib";
					PublicAdditionalLibraries.Add(LibraryName);
				}
				break;
			case EICULinkType.Dynamic:
				foreach (string Stem in LibraryNameStems)
				{
					string LibraryName = "icu" + Stem + LibraryNamePostfix + "." + "lib";
					PublicAdditionalLibraries.Add(LibraryName);
				}

				foreach (string Stem in LibraryNameStems)
				{
					string LibraryName = "icu" + Stem + LibraryNamePostfix + "53" + "." + "dll";
					PublicDelayLoadDLLs.Add(LibraryName);
				}

				if(Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
				{
					string BinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/ICU/{0}/{1}/VS{2}/", ICUVersion, Target.Platform.ToString(), Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
					foreach(string Stem in LibraryNameStems)
					{
						string LibraryName = BinariesDir + String.Format("icu{0}{1}53.dll", Stem, LibraryNamePostfix);
						RuntimeDependencies.Add(new RuntimeDependency(LibraryName));
					}
				}

				bNeedsDlls = true;

				break;
			}
		}
		else if	(Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.Android)
		{
			string StaticLibraryExtension = "a";

			switch (Target.Platform)
			{
				case UnrealTargetPlatform.Linux:
					TargetSpecificPath += Target.Architecture + "/";
					break;
				case UnrealTargetPlatform.Android:
					PublicLibraryPaths.Add(TargetSpecificPath + "ARMv7/lib");
					PublicLibraryPaths.Add(TargetSpecificPath + "ARM64/lib");
					PublicLibraryPaths.Add(TargetSpecificPath + "x86/lib");
					PublicLibraryPaths.Add(TargetSpecificPath + "x64/lib");
					break;
			}

			string[] LibraryNameStems =
			{
				"data", // Data
				"uc",   // Unicode Common
				"i18n", // Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};
			string LibraryNamePostfix = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ?
				"d" : string.Empty;

			// Library Paths
			// Temporarily? only link statically on Linux too
			//EICULinkType ICULinkType = (Target.Platform == UnrealTargetPlatform.Android || Target.IsMonolithic) ? EICULinkType.Static : EICULinkType.Dynamic;
			EICULinkType ICULinkType = EICULinkType.Static;
			switch (ICULinkType)
			{
				case EICULinkType.Static:
					foreach (string Stem in LibraryNameStems)
					{
						string LibraryName = "icu" + Stem + LibraryNamePostfix;
						if (Target.Platform == UnrealTargetPlatform.Android)
						{
							// we will filter out in the toolchain
							PublicAdditionalLibraries.Add(LibraryName); // Android requires only the filename.
						}
						else
						{
							PublicAdditionalLibraries.Add(TargetSpecificPath + "lib/" + "lib" + LibraryName + "." + StaticLibraryExtension); // Linux seems to need the path, not just the filename.
						}
					}
					break;
				case EICULinkType.Dynamic:
					if (Target.Platform == UnrealTargetPlatform.Linux)
					{
						string PathToBinary = String.Format("$(EngineDir)/Binaries/ThirdParty/ICU/{0}/{1}/{2}/", ICUVersion, Target.Platform.ToString(),
							Target.Architecture);

						foreach (string Stem in LibraryNameStems)
						{
							string LibraryName = "icu" + Stem + LibraryNamePostfix;
							string LibraryPath = Target.UEThirdPartyBinariesDirectory + "ICU/icu4c-53_1/Linux/" + Target.Architecture + "/";

							PublicLibraryPaths.Add(LibraryPath);
							PublicAdditionalLibraries.Add(LibraryName);

							// add runtime dependencies (for staging)
							RuntimeDependencies.Add(new RuntimeDependency(PathToBinary + "lib" + LibraryName + ".so"));
							RuntimeDependencies.Add(new RuntimeDependency(PathToBinary + "lib" + LibraryName + ".so.53"));  // version-dependent
						}
					}
					break;
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS)
		{
			string StaticLibraryExtension = "a";
			string DynamicLibraryExtension = "dylib";

			string[] LibraryNameStems =
			{
				"data", // Data
				"uc",   // Unicode Common
				"i18n", // Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};
			string LibraryNamePostfix = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ?
				"d" : string.Empty;

			EICULinkType ICULinkType = (Target.Platform == UnrealTargetPlatform.IOS || (Target.LinkType == TargetLinkType.Monolithic)) ? EICULinkType.Static : EICULinkType.Dynamic;
			// Library Paths
			switch (ICULinkType)
			{
				case EICULinkType.Static:
					foreach (string Stem in LibraryNameStems)
					{
						string LibraryName = "libicu" + Stem + LibraryNamePostfix + "." + StaticLibraryExtension;
						PublicAdditionalLibraries.Add(TargetSpecificPath + "lib/" + LibraryName);
						PublicAdditionalShadowFiles.Add(TargetSpecificPath + "lib/" + LibraryName);
					}
					break;
				case EICULinkType.Dynamic:
					foreach (string Stem in LibraryNameStems)
					{
						if (Target.Platform == UnrealTargetPlatform.Mac)
						{
							string LibraryName = "libicu" + Stem + ".53.1" + LibraryNamePostfix + "." + DynamicLibraryExtension;
							string LibraryPath = Target.UEThirdPartyBinariesDirectory + "ICU/icu4c-53_1/Mac/" + LibraryName;

							PublicDelayLoadDLLs.Add(LibraryPath);
							PublicAdditionalShadowFiles.Add(LibraryPath);
							RuntimeDependencies.Add(new RuntimeDependency(LibraryPath));
						}
					}

					bNeedsDlls = true;

					break;
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			// we don't bother with debug libraries on HTML5. Mainly because debugging isn't viable on html5 currently
			string StaticLibraryExtension = "bc";

			string[] LibraryNameStems =
			{
				"data", // Data
				"uc",   // Unicode Common
				"i18n", // Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};

			string OpimizationSuffix = "";
			if (Target.bCompileForSize)
			{
				OpimizationSuffix = "_Oz";
			}
			else
			{
				if (Target.Configuration == UnrealTargetConfiguration.Development)
				{
					OpimizationSuffix = "_O2";
				}
				else if (Target.Configuration == UnrealTargetConfiguration.Shipping)
				{
					OpimizationSuffix = "_O3";
				}
			}

			foreach (string Stem in LibraryNameStems)
			{
				string LibraryName = "libicu" + Stem + OpimizationSuffix + "." + StaticLibraryExtension;
				PublicAdditionalLibraries.Add(TargetSpecificPath + LibraryName);
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			string LibraryNamePrefix = "sicu";
			string[] LibraryNameStems =
			{
				"dt",	// Data
				"uc",   // Unicode Common
				"in",	// Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};
			string LibraryNamePostfix = (Target.Configuration == UnrealTargetConfiguration.Debug) ?
				"d" : string.Empty;
			string LibraryExtension = "lib";
			foreach (string Stem in LibraryNameStems)
			{
				string LibraryName = ICURootPath + "PS4/lib/" + LibraryNamePrefix + Stem + LibraryNamePostfix + "." + LibraryExtension;
				PublicAdditionalLibraries.Add(LibraryName);
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Switch)
		{
			string LibraryNamePrefix = "sicu";
			string[] LibraryNameStems =
			{
				"dt",	// Data
				"uc",   // Unicode Common
				"in",	// Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};
			string LibraryNamePostfix = ""; //(Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "d" : string.Empty;
			string LibraryExtension = "a";
			foreach (string Stem in LibraryNameStems)
			{
				string LibraryName = ICURootPath + "Switch/lib/" + LibraryNamePrefix + Stem + LibraryNamePostfix + "." + LibraryExtension;
				PublicAdditionalLibraries.Add(LibraryName);
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			// Use reflection to allow type not to exist if console code is not present
			System.Type XboxOnePlatformType = System.Type.GetType("UnrealBuildTool.XboxOnePlatform,UnrealBuildTool");
			if (XboxOnePlatformType != null)
			{
				string LibraryNamePrefix = "sicu";
				string[] LibraryNameStems =
				{
					"dt",	// Data
					"uc",   // Unicode Common
					"in",	// Internationalization
					"le",   // Layout Engine
					"lx",   // Layout Extensions
					"io"	// Input/Output
				};
				string LibraryNamePostfix = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ?
					"d" : string.Empty;
				string LibraryExtension = "lib";
				foreach (string Stem in LibraryNameStems)
				{
					System.Object VersionName = XboxOnePlatformType.GetMethod("GetVisualStudioCompilerVersionName").Invoke(null, null);
					string LibraryName = ICURootPath + "XboxOne/VS" + VersionName.ToString() + "/lib/" + LibraryNamePrefix + Stem + LibraryNamePostfix + "." + LibraryExtension;
					PublicAdditionalLibraries.Add(LibraryName);
				}
			}
		}

		// common defines
		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32) ||
			(Target.Platform == UnrealTargetPlatform.Linux) ||
			(Target.Platform == UnrealTargetPlatform.Android) ||
			(Target.Platform == UnrealTargetPlatform.Mac) ||
			(Target.Platform == UnrealTargetPlatform.IOS) ||
			(Target.Platform == UnrealTargetPlatform.TVOS) ||
			(Target.Platform == UnrealTargetPlatform.PS4) ||
			(Target.Platform == UnrealTargetPlatform.XboxOne) ||
			(Target.Platform == UnrealTargetPlatform.HTML5) ||
			(Target.Platform == UnrealTargetPlatform.Switch)
			)
		{
			// Definitions
			Definitions.Add("U_USING_ICU_NAMESPACE=0"); // Disables a using declaration for namespace "icu".
			Definitions.Add("U_STATIC_IMPLEMENTATION"); // Necessary for linking to ICU statically.
			Definitions.Add("U_NO_DEFAULT_INCLUDE_UTF_HEADERS=1"); // Disables unnecessary inclusion of headers - inclusions are for ease of use.
			Definitions.Add("UNISTR_FROM_CHAR_EXPLICIT=explicit"); // Makes UnicodeString constructors for ICU character types explicit.
			Definitions.Add("UNISTR_FROM_STRING_EXPLICIT=explicit"); // Makes UnicodeString constructors for "char"/ICU string types explicit.
			Definitions.Add("UCONFIG_NO_TRANSLITERATION=1"); // Disables declarations and compilation of unused ICU transliteration functionality.
		}

		if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			// Definitions
			Definitions.Add("ICU_NO_USER_DATA_OVERRIDE=1");
			Definitions.Add("U_PLATFORM=U_PF_ORBIS");
		}

		if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			// Definitions
			Definitions.Add("ICU_NO_USER_DATA_OVERRIDE=1");
			Definitions.Add("U_PLATFORM=U_PF_DURANGO");
		}

		Definitions.Add("NEEDS_ICU_DLLS=" + (bNeedsDlls ? "1" : "0"));
	}
}
