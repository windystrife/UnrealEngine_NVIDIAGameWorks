using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// ModuleRules is a data structure that contains the rules for defining a module
	/// </summary>
	public class ModuleRules
	{
		/// <summary>
		/// Type of module
		/// </summary>
		public enum ModuleType
		{
			/// <summary>
			/// C++
			/// </summary>
			CPlusPlus,

			/// <summary>
			/// External (third-party)
			/// </summary>
			External,
		}

		/// <summary>
		/// Code optimization settings
		/// </summary>
		public enum CodeOptimization
		{
			/// <summary>
			/// Code should never be optimized if possible.
			/// </summary>
			Never,

			/// <summary>
			/// Code should only be optimized in non-debug builds (not in Debug).
			/// </summary>
			InNonDebugBuilds,

			/// <summary>
			/// Code should only be optimized in shipping builds (not in Debug, DebugGame, Development)
			/// </summary>
			InShippingBuildsOnly,

			/// <summary>
			/// Code should always be optimized if possible.
			/// </summary>
			Always,

			/// <summary>
			/// Default: 'InNonDebugBuilds' for game modules, 'Always' otherwise.
			/// </summary>
			Default,
		}

		/// <summary>
		/// What type of PCH to use for this module.
		/// </summary>
		public enum PCHUsageMode
		{
			/// <summary>
			/// Default: Engine modules use shared PCHs, game modules do not
			/// </summary>
			Default,

			/// <summary>
			/// Never use shared PCHs.  Always generate a unique PCH for this module if appropriate
			/// </summary>
			NoSharedPCHs,

			/// <summary>
			/// Shared PCHs are OK!
			/// </summary>
			UseSharedPCHs,

			/// <summary>
			/// Shared PCHs may be used if an explicit private PCH is not set through PrivatePCHHeaderFile. In either case, none of the source files manually include a module PCH, and should include a matching header instead.
			/// </summary>
			UseExplicitOrSharedPCHs,
		}

		/// <summary>
		/// Which type of targets this module should be precompiled for
		/// </summary>
		public enum PrecompileTargetsType
		{
			/// <summary>
			/// Never precompile this module.
			/// </summary>
			None,

			/// <summary>
			/// Inferred from the module's directory. Engine modules under Engine/Source/Runtime will be compiled for games, those under Engine/Source/Editor will be compiled for the editor, etc...
			/// </summary>
			Default,

			/// <summary>
			/// Any game targets.
			/// </summary>
			Game,

			/// <summary>
			/// Any editor targets.
			/// </summary>
			Editor,

			/// <summary>
			/// Any targets.
			/// </summary>
			Any,
		}

		/// <summary>
		/// Information about a file which is required by the target at runtime, and must be moved around with it.
		/// </summary>
		[Serializable]
		public class RuntimeDependency
		{
			/// <summary>
			/// The file that should be staged. Should use $(EngineDir) and $(ProjectDir) variables as a root, so that the target can be relocated to different machines.
			/// </summary>
			public string Path;

			/// <summary>
			/// How to stage this file.
			/// </summary>
			public StagedFileType Type;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="InPath">Path to the runtime dependency</param>
			/// <param name="InType">How to stage the given path</param>
			public RuntimeDependency(string InPath, StagedFileType InType = StagedFileType.NonUFS)
			{
				Path = InPath;
				Type = InType;
			}
		}

		/// <summary>
		/// List of runtime dependencies, with convenience methods for adding new items
		/// </summary>
		[Serializable]
		public class RuntimeDependencyList : List<RuntimeDependency>
		{
			/// <summary>
			/// Default constructor
			/// </summary>
			public RuntimeDependencyList()
			{
			}

			/// <summary>
			/// Add a runtime dependency to the list
			/// </summary>
			/// <param name="InPath">Path to the runtime dependency. May include wildcards.</param>
			/// <param name="InType">How to stage this file</param>
			public void Add(string InPath, StagedFileType InType)
			{
				Add(new RuntimeDependency(InPath, InType));
			}
		}

		/// <summary>
		/// Rules for the target that this module belongs to
		/// </summary>
		public readonly ReadOnlyTargetRules Target;

		/// <summary>
		/// Type of module
		/// </summary>
		public ModuleType Type = ModuleType.CPlusPlus;

		/// <summary>
		/// Subfolder of Binaries/PLATFORM folder to put this module in when building DLLs. This should only be used by modules that are found via searching like the
		/// TargetPlatform or ShaderFormat modules. If FindModules is not used to track them down, the modules will not be found.
		/// </summary>
		public string BinariesSubFolder = "";

		/// <summary>
		/// When this module's code should be optimized.
		/// </summary>
		public CodeOptimization OptimizeCode = CodeOptimization.Default;

		/// <summary>
		/// Explicit private PCH for this module. Implies that this module will not use a shared PCH.
		/// </summary>
		public string PrivatePCHHeaderFile;

		/// <summary>
		/// Header file name for a shared PCH provided by this module.  Must be a valid relative path to a public C++ header file.
		/// This should only be set for header files that are included by a significant number of other C++ modules.
		/// </summary>
		public string SharedPCHHeaderFile;
		
		/// <summary>
		/// Specifies an alternate name for intermediate directories and files for intermediates of this module. Useful when hitting path length limitations.
		/// </summary>
		public string ShortName = null;

		/// <summary>
		/// Precompiled header usage for this module
		/// </summary>
		public PCHUsageMode PCHUsage = PCHUsageMode.Default;

		/// <summary>
		/// Use run time type information
		/// </summary>
		public bool bUseRTTI = false;

		/// <summary>
		/// Use AVX instructions
		/// </summary>
		public bool bUseAVX = false;

		/// <summary>
		/// Enable buffer security checks.  This should usually be enabled as it prevents severe security risks.
		/// </summary>
		public bool bEnableBufferSecurityChecks = true;

		/// <summary>
		/// Enable exception handling
		/// </summary>
		public bool bEnableExceptions = false;

		/// <summary>
		/// Enable objective C exception handling
		/// </summary>
		public bool bEnableObjCExceptions = false;

		/// <summary>
		/// Enable warnings for shadowed variables
		/// </summary>
		public bool bEnableShadowVariableWarnings = true;

		/// <summary>
		/// Enable warnings for using undefined identifiers in #if expressions
		/// </summary>
		public bool bEnableUndefinedIdentifierWarnings = true;

		/// <summary>
		/// If true and unity builds are enabled, this module will build without unity.
		/// </summary>
		public bool bFasterWithoutUnity = false;

		/// <summary>
		/// The number of source files in this module before unity build will be activated for that module.  If set to
		/// anything besides -1, will override the default setting which is controlled by MinGameModuleSourceFilesForUnityBuild
		/// </summary>
		public int MinSourceFilesForUnityBuildOverride = 0;

		/// <summary>
		/// Overrides BuildConfiguration.MinFilesUsingPrecompiledHeader if non-zero.
		/// </summary>
		public int MinFilesUsingPrecompiledHeaderOverride = 0;

		/// <summary>
		/// Module uses a #import so must be built locally when compiling with SN-DBS
		/// </summary>
		public bool bBuildLocallyWithSNDBS = false;

		/// <summary>
		/// Redistribution override flag for this module.
		/// </summary>
		public bool? IsRedistributableOverride = null;

		/// <summary>
		/// Whether the output from this module can be publicly distributed, even if it has code/
		/// dependencies on modules that are not (i.e. CarefullyRedist, NotForLicensees, NoRedist).
		/// This should be used when you plan to release binaries but not source.
		/// </summary>
		public bool bOutputPubliclyDistributable = false;

		/// <summary>
		/// List of folders which are whitelisted to be referenced when compiling this binary, without propagating restricted folder names
		/// </summary>
		public List<string> WhitelistRestrictedFolders = new List<string>();

		/// <summary>
		/// Enforce "include what you use" rules when PCHUsage is set to ExplicitOrSharedPCH; warns when monolithic headers (Engine.h, UnrealEd.h, etc...) 
		/// are used, and checks that source files include their matching header first.
		/// </summary>
		public bool bEnforceIWYU = true;

		/// <summary>
		/// Whether to add all the default include paths to the module (eg. the Source/Classes folder, subfolders under Source/Public).
		/// </summary>
		public bool bAddDefaultIncludePaths = true;

		/// <summary>
		/// List of modules names (no path needed) with header files that our module's public headers needs access to, but we don't need to "import" or link against.
		/// </summary>
		public List<string> PublicIncludePathModuleNames = new List<string>();

		/// <summary>
		/// List of public dependency module names (no path needed) (automatically does the private/public include). These are modules that are required by our public source files.
		/// </summary>
		public List<string> PublicDependencyModuleNames = new List<string>();

		/// <summary>
		/// List of modules name (no path needed) with header files that our module's private code files needs access to, but we don't need to "import" or link against.
		/// </summary>
		public List<string> PrivateIncludePathModuleNames = new List<string>();

		/// <summary>
		/// List of private dependency module names.  These are modules that our private code depends on but nothing in our public
		/// include files depend on.
		/// </summary>
		public List<string> PrivateDependencyModuleNames = new List<string>();

		/// <summary>
		/// Only for legacy reason, should not be used in new code. List of module dependencies that should be treated as circular references.  This modules must have already been added to
		/// either the public or private dependent module list.
		/// </summary>
		public List<string> CircularlyReferencedDependentModules = new List<string>();

		/// <summary>
		/// List of system/library include paths - typically used for External (third party) modules.  These are public stable header file directories that are not checked when resolving header dependencies.
		/// </summary>
		public List<string> PublicSystemIncludePaths = new List<string>();

		/// <summary>
		/// (This setting is currently not need as we discover all files from the 'Public' folder) List of all paths to include files that are exposed to other modules
		/// </summary>
		public List<string> PublicIncludePaths = new List<string>();

		/// <summary>
		/// List of all paths to this module's internal include files, not exposed to other modules (at least one include to the 'Private' path, more if we want to avoid relative paths)
		/// </summary>
		public List<string> PrivateIncludePaths = new List<string>();

		/// <summary>
		/// List of system/library paths (directory of .lib files) - typically used for External (third party) modules
		/// </summary>
		public List<string> PublicLibraryPaths = new List<string>();

		/// <summary>
		/// List of additional libraries (names of the .lib files including extension) - typically used for External (third party) modules
		/// </summary>
		public List<string> PublicAdditionalLibraries = new List<string>();

		/// <summary>
		/// List of XCode frameworks (iOS and MacOS)
		/// </summary>
		public List<string> PublicFrameworks = new List<string>();

		/// <summary>
		/// List of weak frameworks (for OS version transitions)
		/// </summary>
		public List<string> PublicWeakFrameworks = new List<string>();

		/// <summary>
		/// List of addition frameworks - typically used for External (third party) modules on Mac and iOS
		/// </summary>
		public List<UEBuildFramework> PublicAdditionalFrameworks = new List<UEBuildFramework>();

		/// <summary>
		/// List of addition resources that should be copied to the app bundle for Mac or iOS
		/// </summary>
		public List<UEBuildBundleResource> AdditionalBundleResources = new List<UEBuildBundleResource>();

		/// <summary>
		/// For builds that execute on a remote machine (e.g. iOS), this list contains additional files that
		/// need to be copied over in order for the app to link successfully.  Source/header files and PCHs are
		/// automatically copied.  Usually this is simply a list of precompiled third party library dependencies.
		/// </summary>
		public List<string> PublicAdditionalShadowFiles = new List<string>();

		/// <summary>
		/// List of delay load DLLs - typically used for External (third party) modules
		/// </summary>
		public List<string> PublicDelayLoadDLLs = new List<string>();

		/// <summary>
		/// Additional compiler definitions for this module
		/// </summary>
		public List<string> Definitions = new List<string>();

		/// <summary>
		/// Addition modules this module may require at run-time 
		/// </summary>
		public List<string> DynamicallyLoadedModuleNames = new List<string>();

		/// <summary>
		/// Extra modules this module may require at run time, that are on behalf of another platform (i.e. shader formats and the like)
		/// </summary>
		public List<string> PlatformSpecificDynamicallyLoadedModuleNames = new List<string>();

		/// <summary>
		/// List of files which this module depends on at runtime. These files will be staged along with the target.
		/// </summary>
		public RuntimeDependencyList RuntimeDependencies = new RuntimeDependencyList();

		/// <summary>
		/// List of additional properties to be added to the build receipt
		/// </summary>
		public List<ReceiptProperty> AdditionalPropertiesForReceipt = new List<ReceiptProperty>();

		/// <summary>
		/// Which targets this module should be precompiled for
		/// </summary>
		public PrecompileTargetsType PrecompileForTargets = PrecompileTargetsType.Default;

		/// <summary>
		/// External files which invalidate the makefile if modified. Relative paths are resolved relative to the .build.cs file.
		/// </summary>
		public List<string> ExternalDependencies = new List<string>();

		/// <summary>
		/// Property for the directory containing this module. Useful for adding paths to third party dependencies.
		/// </summary>
		public string ModuleDirectory
		{
			get
			{
				return Path.GetDirectoryName(RulesCompiler.GetFileNameFromType(GetType()));
			}
		}

		/// <summary>
		/// Default constructor. Deprecated in 4.15.
		/// </summary>
		[Obsolete("Please change your module constructor to take a ReadOnlyTargetRules parameter, and pass it to the base class constructor (eg. \"MyModuleRules(ReadOnlyTargetRules Target) : base(Target)\").")]
		public ModuleRules()
		{
		}

		/// <summary>
		/// Constructor. For backwards compatibility while the parameterless constructor is being phased out, initialization which would happen here is done by 
		/// RulesAssembly.CreateModulRules instead.
		/// </summary>
		/// <param name="Target">Rules for building this target</param>
		public ModuleRules(ReadOnlyTargetRules Target)
		{
		}

		/// <summary>
		/// Made Obsolete so that we can more clearly show that this should be used for third party modules within the Engine directory
		/// </summary>
		/// <param name="Target">The target this module belongs to</param>
		/// <param name="ModuleNames">The names of the modules to add</param>
		[Obsolete("Use AddEngineThirdPartyPrivateStaticDependencies to add dependencies on ThirdParty modules within the Engine Directory")]
		public void AddThirdPartyPrivateStaticDependencies(ReadOnlyTargetRules Target, params string[] ModuleNames)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, ModuleNames);
		}

		/// <summary>
		/// Add the given Engine ThirdParty modules as static private dependencies
		///	Statically linked to this module, meaning they utilize exports from the other module
		///	Private, meaning the include paths for the included modules will not be exposed when giving this modules include paths
		///	NOTE: There is no AddThirdPartyPublicStaticDependencies function.
		/// </summary>
		/// <param name="Target">The target this module belongs to</param>
		/// <param name="ModuleNames">The names of the modules to add</param>
		public void AddEngineThirdPartyPrivateStaticDependencies(ReadOnlyTargetRules Target, params string[] ModuleNames)
		{
			if (!UnrealBuildTool.IsEngineInstalled() || Target.LinkType == TargetLinkType.Monolithic)
			{
				PrivateDependencyModuleNames.AddRange(ModuleNames);
			}
		}

		/// <summary>
		/// Made Obsolete so that we can more clearly show that this should be used for third party modules within the Engine directory
		/// </summary>
		/// <param name="Target">Rules for the target being built</param>
		/// <param name="ModuleNames">The names of the modules to add</param>
		[Obsolete("Use AddEngineThirdPartyPrivateDynamicDependencies to add dependencies on ThirdParty modules within the Engine Directory")]
		public void AddThirdPartyPrivateDynamicDependencies(ReadOnlyTargetRules Target, params string[] ModuleNames)
		{
			AddEngineThirdPartyPrivateDynamicDependencies(Target, ModuleNames);
		}

		/// <summary>
		/// Add the given Engine ThirdParty modules as dynamic private dependencies
		///	Dynamically linked to this module, meaning they do not utilize exports from the other module
		///	Private, meaning the include paths for the included modules will not be exposed when giving this modules include paths
		///	NOTE: There is no AddThirdPartyPublicDynamicDependencies function.
		/// </summary>
		/// <param name="Target">Rules for the target being built</param>
		/// <param name="ModuleNames">The names of the modules to add</param>
		public void AddEngineThirdPartyPrivateDynamicDependencies(ReadOnlyTargetRules Target, params string[] ModuleNames)
		{
			if (!UnrealBuildTool.IsEngineInstalled() || Target.LinkType == TargetLinkType.Monolithic)
			{
				PrivateIncludePathModuleNames.AddRange(ModuleNames);
				DynamicallyLoadedModuleNames.AddRange(ModuleNames);
			}
		}

		/// <summary>
		/// Setup this module for PhysX/APEX support (based on the settings in UEBuildConfiguration)
		/// </summary>
		public void SetupModulePhysXAPEXSupport(ReadOnlyTargetRules Target)
		{
			// definitions used outside of PhysX/APEX need to be set here, not in PhysX.Build.cs or APEX.Build.cs, 
			// since we need to make sure we always set it, even to 0 (because these are Private dependencies, the
			// defines inside their Build.cs files won't leak out)
			if (Target.bCompilePhysX == true)
			{
				PrivateDependencyModuleNames.Add("PhysX");
				Definitions.Add("WITH_PHYSX=1");
			}
			else
			{
				Definitions.Add("WITH_PHYSX=0");
			}

			if (Target.bCompileAPEX == true)
			{
				if (!Target.bCompilePhysX)
				{
					throw new BuildException("APEX is enabled, without PhysX. This is not supported!");
				}

				PrivateDependencyModuleNames.Add("APEX");
				Definitions.Add("WITH_APEX=1");
				Definitions.Add("WITH_APEX_CLOTHING=1");
				Definitions.Add("WITH_CLOTH_COLLISION_DETECTION=1");
				Definitions.Add("WITH_PHYSX_COOKING=1");  // APEX currently relies on cooking even at runtime

			}
			else
			{
				Definitions.Add("WITH_APEX=0");
				Definitions.Add("WITH_APEX_CLOTHING=0");
				Definitions.Add("WITH_CLOTH_COLLISION_DETECTION=0");
				Definitions.Add(string.Format("WITH_PHYSX_COOKING={0}", Target.bBuildEditor ? 1 : 0));  // without APEX, we only need cooking in editor builds
			}

			if (Target.bCompileNvCloth == true)
			{
				if (!Target.bCompilePhysX)
				{
					throw new BuildException("NvCloth is enabled, without PhysX. This is not supported!");
				}

				PrivateDependencyModuleNames.Add("NvCloth");
                Definitions.Add("WITH_NVCLOTH=1");

			}
			else
			{
				Definitions.Add("WITH_NVCLOTH=0");
			}

            if (Target.bCompileNvFlexD3D == true || Target.bCompileNvFlexCUDA == true)
            {
                AddEngineThirdPartyPrivateStaticDependencies(Target, "FLEX");
                Definitions.Add("WITH_FLEX=1");

                if (Target.bCompileNvFlexD3D == true)
                {
                    Definitions.Add("WITH_FLEX_DX=1");
                    Definitions.Add("WITH_FLEX_CUDA=0");
                }

                if (Target.bCompileNvFlexCUDA == true)
                {
                    Definitions.Add("WITH_FLEX_CUDA=1");
                    Definitions.Add("WITH_FLEX_DX=0");
                }
            }
            else
            {
                Definitions.Add("WITH_FLEX=0");
            }
		}

		/// <summary>
		/// Hack to allow deprecating existing code which references the static BuildConfiguration object; redirect it to use properties on this object.
		/// </summary>
		[Obsolete("The BuildConfiguration alias is deprecated in 4.18. Set the same properties on the ReadOnlyTargetRules instance passed into the ModuleRules constructor instead.")]
		public ReadOnlyTargetRules BuildConfiguration
		{
			get { return Target; }
		}

		/// <summary>
		/// Hack to allow deprecating existing code which references the static UEBuildConfiguration object; redirect it to use properties on this object.
		/// </summary>
		[Obsolete("The UEBuildConfiguration alias is deprecated in 4.18. Set the same properties on the ReadOnlyTargetRules instance passed into the ModuleRules constructor instead.")]
		public ReadOnlyTargetRules UEBuildConfiguration
		{
			get { return Target; }
		}

		/// <summary>
		/// Hack to allow deprecating existing code which references the static WindowsPlatform object; redirect it to use properties on the target rules.
		/// </summary>
		[Obsolete("The WindowsPlatform alias is deprecated in 4.18. Set the same properties on the WindowsPlatform member of the ReadOnlyTargetRules instance passed into the ModuleRules constructor instead.")]
		public ReadOnlyWindowsTargetRules WindowsPlatform
		{
			get { return Target.WindowsPlatform; }
		}
	}
}
