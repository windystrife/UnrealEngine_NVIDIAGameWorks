using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
    public class BlastLoader : ModuleRules
    {
        public BlastLoader(ReadOnlyTargetRules Target) : base(Target)
        {
            OptimizeCode = CodeOptimization.InNonDebugBuilds;

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
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
