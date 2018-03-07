// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class SQLiteSupport : ModuleRules
	{
		public SQLiteSupport(ReadOnlyTargetRules Target) : base(Target)
		{
			string PlatformName = "";
			string ConfigurationName = "";

			switch (Target.Platform)
			{
				case UnrealTargetPlatform.Win32:
					PlatformName = "Win32/";
					break;
				case UnrealTargetPlatform.Win64:
					PlatformName = "x64/";
					break;
			
			    case UnrealTargetPlatform.IOS:
				case UnrealTargetPlatform.TVOS:
					PlatformName = "IOS/";
                    break;
                case UnrealTargetPlatform.Mac:
                    PlatformName = "Mac/";
                    break;
                case UnrealTargetPlatform.Linux:
                    PlatformName = "Linux/";
                    break;
			}

			switch (Target.Configuration)
			{
				case UnrealTargetConfiguration.Debug:
					ConfigurationName = "Debug/";
					break;
				case UnrealTargetConfiguration.DebugGame:
					ConfigurationName = "Debug/";
					break;
				default:
					ConfigurationName = "Release/";
					break;
			}

			string LibraryPath = "" + Target.UEThirdPartySourceDirectory + "sqlite/lib/" + PlatformName + ConfigurationName;

			string LibraryFilename;
			if(Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.XboxOne)
			{
				LibraryFilename = Path.Combine(LibraryPath, "sqlite.lib");
			}
			else
			{
				LibraryFilename = Path.Combine(LibraryPath, "sqlite.a");
			}
		
			if (!File.Exists(LibraryFilename) && !Target.bPrecompile)
			{
				throw new BuildException("Please refer to the Engine/Source/ThirdParty/sqlite/README.txt file prior to enabling this module.");
			}

			PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "sqlite/sqlite/");

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"DatabaseSupport",
				}
			);

			// Lib file
			PublicLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(LibraryFilename);

			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}
