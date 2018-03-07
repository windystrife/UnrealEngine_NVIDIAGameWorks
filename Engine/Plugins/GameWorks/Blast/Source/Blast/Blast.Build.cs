using System;
using System.IO;
using Tools.DotNETCommon;

namespace UnrealBuildTool.Rules
{
    public class Blast : ModuleRules
    {
        public static string GetBlastLibrarySuffix(ModuleRules Rules)
        {
            if ((Rules.Target.Configuration == UnrealTargetConfiguration.Debug || Rules.Target.Configuration == UnrealTargetConfiguration.DebugGame))// && Rules.Target.bDebugBuildsActuallyUseDebugCRT)
            {
                return "DEBUG";
            }
            else if (Rules.Target.Configuration == UnrealTargetConfiguration.Development && Rules.Target.Platform != UnrealTargetPlatform.Linux)
            {
                return "PROFILE"; //Linux only has debug and release
            }

            return "";
        }

        public static void SetupModuleBlastSupport(ModuleRules Rules, string[] BlastLibs)
        {
            string LibConfiguration = GetBlastLibrarySuffix(Rules);

            Rules.Definitions.Add(string.Format("BLAST_LIB_CONFIG_STRING=\"{0}\"", LibConfiguration));

            //Go up two from Source/Blast
            DirectoryReference ModuleRootFolder = (new DirectoryReference(Rules.ModuleDirectory)).ParentDirectory.ParentDirectory;
            DirectoryReference EngineDirectory = new DirectoryReference(Path.GetFullPath(Rules.Target.RelativeEnginePath));
            string BLASTLibDir = Path.Combine("$(EngineDir)", ModuleRootFolder.MakeRelativeTo(EngineDirectory), "Libraries", Rules.Target.Platform.ToString());

            Rules.PublicLibraryPaths.Add(Path.Combine(ModuleRootFolder.ToString(), "Libraries", Rules.Target.Platform.ToString()));

            string DLLSuffix = "";
            string DLLPrefix = "";
            string LibSuffix = "";

            // Libraries and DLLs for windows platform
            if (Rules.Target.Platform == UnrealTargetPlatform.Win64)
            {
                DLLSuffix = "_x64.dll";
                LibSuffix = "_x64.lib";
            }
            else if (Rules.Target.Platform == UnrealTargetPlatform.Win32)
            {
                DLLSuffix = "_x86.dll";
                LibSuffix = "_x86.lib";
            }
            else if (Rules.Target.Platform == UnrealTargetPlatform.Linux)
            {
                DLLPrefix = "lib";
                DLLSuffix = ".so";
            }

            Rules.Definitions.Add(string.Format("BLAST_LIB_DLL_SUFFIX=\"{0}\"", DLLSuffix));
            Rules.Definitions.Add(string.Format("BLAST_LIB_DLL_PREFIX=\"{0}\"", DLLPrefix));

            foreach (string Lib in BlastLibs)
            {
                Rules.PublicAdditionalLibraries.Add(String.Format("{0}{1}{2}", Lib, LibConfiguration, LibSuffix));
                var DllName = String.Format("{0}{1}{2}{3}", DLLPrefix, Lib, LibConfiguration, DLLSuffix);
                Rules.PublicDelayLoadDLLs.Add(DllName);
                Rules.RuntimeDependencies.Add(new RuntimeDependency(Path.Combine(BLASTLibDir, DllName)));
            }
            
            //It's useful to periodically turn this on since the order of appending files in unity build is random.
            //The use of types without the right header can creep in and cause random build failures

            //Rules.bFasterWithoutUnity = true;
        }

        public Blast(ReadOnlyTargetRules Target) : base(Target)
        {
            OptimizeCode = CodeOptimization.InNonDebugBuilds;

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Projects",
                    "RenderCore",
                    "Renderer",
                    "RHI",
                    "ShaderCore",
                    "BlastLoader"
                }
            );

            if (Target.bBuildEditor)
            {
                PrivateDependencyModuleNames.AddRange(
                  new string[]
                  {
                        "RawMesh",
                        "UnrealEd",
                        "BlastLoaderEditor",
                  }
                );
            }

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Engine",
                    "PhysX",
                }
            );

            string[] BlastLibs =
            {
                 "NvBlast",
                 "NvBlastGlobals",
                 "NvBlastExtSerialization",
                 "NvBlastExtShaders",
                 "NvBlastExtStress",
            };

            SetupModuleBlastSupport(this, BlastLibs);

            SetupModulePhysXAPEXSupport(Target);
        }
    }
}
