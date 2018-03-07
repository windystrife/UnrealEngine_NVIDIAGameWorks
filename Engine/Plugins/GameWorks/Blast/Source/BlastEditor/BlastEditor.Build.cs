using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
    public class BlastEditor : ModuleRules
    {
        public BlastEditor(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicDependencyModuleNames.AddRange(
                new string[] {
                    "Blast",
                    "UnrealEd"
                }
            );

            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "Blast",
                    "Core",
                    "CoreUObject",
                    "AssetTools",
                    "Engine",
                    "RenderCore",
                    "Renderer",
                    "PropertyEditor",
                    "XmlParser",
                    "Slate",
                    "ContentBrowser",
                    "Projects",
                    "Slate",
                    "SlateCore",
                    "MainFrame",
                    "InputCore",
                    "EditorStyle",
                    "LevelEditor",
                    "JsonUtilities",
                    "Json",
                    "RHI",
                }
            );

            //Don't add to PrivateDependencyModuleNames since we only use IBlastMeshEditorModule and we can't have a circular depdency on Linux
            PrivateIncludePathModuleNames.AddRange(
                new string[] {
                    "BlastMeshEditor",
                }
            );


            string[] BlastLibs =
            {
                 "NvBlastExtAssetUtils",
                 "NvBlastExtAuthoring",
            };

            Blast.SetupModuleBlastSupport(this, BlastLibs);

            AddEngineThirdPartyPrivateStaticDependencies(Target, "FBX");

        }
    }
}