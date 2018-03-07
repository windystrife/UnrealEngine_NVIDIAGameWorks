using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
    public class BlastLoaderEditor : ModuleRules
    {
        public BlastLoaderEditor(ReadOnlyTargetRules Target) : base(Target)
        {
            OptimizeCode = CodeOptimization.InNonDebugBuilds;

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "BlastLoader",
                    "Projects",
                }
            );

            string[] BlastLibs =
            {
            };

            Blast.SetupModuleBlastSupport(this, BlastLibs);
        }
    }
}
