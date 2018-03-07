// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
    public class BlastMeshEditor : ModuleRules
    {
        public BlastMeshEditor(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicDependencyModuleNames.AddRange(
                new string[] {
                "Engine",
                "PhysX"
                }
            );

            PrivateDependencyModuleNames.AddRange(
                new string[] {
                "Blast",
                "BlastEditor",
                "Core",
                "CoreUObject",
                "InputCore",
                "RenderCore",
                "Slate",
                "SlateCore",
                "EditorStyle",
                "UnrealEd",
                "Projects",
                "DesktopPlatform",
                "AdvancedPreviewScene",
                "AssetTools",
                "LevelEditor",
                "MeshMergeUtilities",
                "RawMesh",
                "MeshUtilities",
                "RHI",
                }
            );

            DynamicallyLoadedModuleNames.Add("PropertyEditor");

            string[] BlastLibs =
            {
                 "NvBlastExtAuthoring",
            };

            Blast.SetupModuleBlastSupport(this, BlastLibs);

            AddEngineThirdPartyPrivateStaticDependencies(Target, "FBX");

            //SetupModulePhysXAPEXSupport(Target);
        }
    }
}