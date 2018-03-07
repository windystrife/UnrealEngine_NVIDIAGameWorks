// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class USDImporter : ModuleRules
	{
		public USDImporter(ReadOnlyTargetRules Target) : base(Target)
        {

			PublicIncludePaths.AddRange(
				new string[] {
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Editor/USDImporter/Private",
					ModuleDirectory + "/../UnrealUSDWrapper/Source/Public",
				}
				);

		
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"UnrealEd",
					"InputCore",
					"SlateCore",
                    "PropertyEditor",
					"Slate",
					"RawMesh",
                    "GeometryCache",
					"MeshUtilities",
                    "RenderCore",
                    "RHI",
                    "MessageLog",
					"JsonUtilities",
                }
				);

			string BaseLibDir = ModuleDirectory + "/../UnrealUSDWrapper/Lib/";

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				string LibraryPath = BaseLibDir + "x64/Release/";
				PublicAdditionalLibraries.Add(LibraryPath+"/UnrealUSDWrapper.lib");

                foreach (string FilePath in Directory.EnumerateFiles(Path.Combine(ModuleDirectory, "../../Binaries/Win64/"), "*.dll", SearchOption.AllDirectories))
                {
                    RuntimeDependencies.Add(new RuntimeDependency(FilePath));
                }
            }
            else if (Target.Platform == UnrealTargetPlatform.Linux && Target.Architecture.StartsWith("x86_64"))
			{
                // link directly to runtime libs on Linux, as this also puts them into rpath
				string RuntimeLibraryPath = Path.Combine(ModuleDirectory, "../../Binaries/", Target.Platform.ToString(), Target.Architecture.ToString());
				PublicAdditionalLibraries.Add(RuntimeLibraryPath +"/libUnrealUSDWrapper.so");

                foreach (string FilePath in Directory.EnumerateFiles(RuntimeLibraryPath, "*.so*", SearchOption.AllDirectories))
                {
                    RuntimeDependencies.Add(new RuntimeDependency(FilePath));
                }
            }
		}
	}
}
