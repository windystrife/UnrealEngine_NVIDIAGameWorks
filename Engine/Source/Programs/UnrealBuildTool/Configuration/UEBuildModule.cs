// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Xml;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// A unit of code compilation and linking.
	/// </summary>
	abstract class UEBuildModule
	{
		/// <summary>
		/// The name that uniquely identifies the module.
		/// </summary>
		public readonly string Name;

		/// <summary>
		/// The type of module being built. Used to switch between debug/development and precompiled/source configurations.
		/// </summary>
		public UHTModuleType Type;

		/// <summary>
		/// The rules for this module
		/// </summary>
		public ModuleRules Rules;

		/// <summary>
		/// Path to the module directory
		/// </summary>
		public readonly DirectoryReference ModuleDirectory;

		/// <summary>
		/// Is this module allowed to be redistributed.
		/// </summary>
		private readonly bool? IsRedistributableOverride;

		/// <summary>
		/// The name of the .Build.cs file this module was created from, if any
		/// </summary>
		public FileReference RulesFile;

		/// <summary>
		/// The binary the module will be linked into for the current target.  Only set after UEBuildBinary.BindModules is called.
		/// </summary>
		public UEBuildBinary Binary = null;

		/// <summary>
		/// Include path for this module's base directory, relative to the Engine/Source directory
		/// </summary>
		protected string NormalizedModuleIncludePath;

		/// <summary>
		/// The name of the _API define for this module
		/// </summary>
		protected readonly string ModuleApiDefine;

		/// <summary>
		/// Set of all the public definitions
		/// </summary>
		protected readonly HashSet<string> PublicDefinitions;

		/// <summary>
		/// Set of all public include paths
		/// </summary>
		protected readonly HashSet<string> PublicIncludePaths;

		/// <summary>
		/// Set of all private include paths
		/// </summary>
		protected readonly HashSet<string> PrivateIncludePaths;

		/// <summary>
		/// Set of all system include paths
		/// </summary>
		protected readonly HashSet<string> PublicSystemIncludePaths;

		/// <summary>
		/// Set of all public library paths
		/// </summary>
		protected readonly HashSet<string> PublicLibraryPaths;

		/// <summary>
		/// Set of all additional libraries
		/// </summary>
		protected readonly HashSet<string> PublicAdditionalLibraries;

		/// <summary>
		/// Set of additional frameworks
		/// </summary>
		protected readonly HashSet<string> PublicFrameworks;

		/// <summary>
		/// 
		/// </summary>
		protected readonly HashSet<string> PublicWeakFrameworks;

		/// <summary>
		/// 
		/// </summary>
		protected readonly HashSet<UEBuildFramework> PublicAdditionalFrameworks;

		/// <summary>
		/// 
		/// </summary>
		protected readonly HashSet<string> PublicAdditionalShadowFiles;

		/// <summary>
		/// 
		/// </summary>
		protected readonly HashSet<UEBuildBundleResource> PublicAdditionalBundleResources;

		/// <summary>
		/// Names of modules with header files that this module's public interface needs access to.
		/// </summary>
		protected List<UEBuildModule> PublicIncludePathModules;

		/// <summary>
		/// Names of modules that this module's public interface depends on.
		/// </summary>
		protected List<UEBuildModule> PublicDependencyModules;

		/// <summary>
		/// Names of DLLs that this module should delay load
		/// </summary>
		protected HashSet<string> PublicDelayLoadDLLs;

		/// <summary>
		/// Names of modules with header files that this module's private implementation needs access to.
		/// </summary>
		protected List<UEBuildModule> PrivateIncludePathModules;

		/// <summary>
		/// Names of modules that this module's private implementation depends on.
		/// </summary>
		protected List<UEBuildModule> PrivateDependencyModules;

		/// <summary>
		/// Extra modules this module may require at run time
		/// </summary>
		protected List<UEBuildModule> DynamicallyLoadedModules;

		/// <summary>
		/// Extra modules this module may require at run time, that are on behalf of another platform (i.e. shader formats and the like)
		/// </summary>
		protected List<UEBuildModule> PlatformSpecificDynamicallyLoadedModules;

		/// <summary>
		/// Files which this module depends on at runtime.
		/// </summary>
		public List<RuntimeDependency> RuntimeDependencies;

		/// <summary>
		/// Set of all whitelisted restricted folder references
		/// </summary>
		private readonly HashSet<DirectoryReference> WhitelistRestrictedFolders;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InName">Name of the module</param>
		/// <param name="InType">Type of the module, for UHT</param>
		/// <param name="InModuleDirectory">Base directory for the module</param>
		/// <param name="InRules">Rules for this module</param>
		/// <param name="InRulesFile">Path to the rules file</param>
		/// <param name="InRuntimeDependencies">List of runtime dependencies</param>
		public UEBuildModule(string InName, UHTModuleType InType, DirectoryReference InModuleDirectory, ModuleRules InRules, FileReference InRulesFile, List<RuntimeDependency> InRuntimeDependencies)
		{
			Name = InName;
			Type = InType;
			ModuleDirectory = InModuleDirectory;
			Rules = InRules;
			RulesFile = InRulesFile;

			NormalizedModuleIncludePath = Utils.CleanDirectorySeparators(ModuleDirectory.MakeRelativeTo(UnrealBuildTool.EngineSourceDirectory), '/');
			ModuleApiDefine = Name.ToUpperInvariant() + "_API";

			PublicDefinitions = HashSetFromOptionalEnumerableStringParameter(InRules.Definitions);
			PublicIncludePaths = HashSetFromOptionalEnumerableStringParameter(InRules.PublicIncludePaths);
			PublicSystemIncludePaths = HashSetFromOptionalEnumerableStringParameter(InRules.PublicSystemIncludePaths);
			PublicLibraryPaths = HashSetFromOptionalEnumerableStringParameter(InRules.PublicLibraryPaths);
			PublicAdditionalLibraries = HashSetFromOptionalEnumerableStringParameter(InRules.PublicAdditionalLibraries);
			PublicFrameworks = HashSetFromOptionalEnumerableStringParameter(InRules.PublicFrameworks);
			PublicWeakFrameworks = HashSetFromOptionalEnumerableStringParameter(InRules.PublicWeakFrameworks);
			PublicAdditionalFrameworks = InRules.PublicAdditionalFrameworks == null ? new HashSet<UEBuildFramework>() : new HashSet<UEBuildFramework>(InRules.PublicAdditionalFrameworks);
			PublicAdditionalShadowFiles = HashSetFromOptionalEnumerableStringParameter(InRules.PublicAdditionalShadowFiles);
			PublicAdditionalBundleResources = InRules.AdditionalBundleResources == null ? new HashSet<UEBuildBundleResource>() : new HashSet<UEBuildBundleResource>(InRules.AdditionalBundleResources);
			PublicDelayLoadDLLs = HashSetFromOptionalEnumerableStringParameter(InRules.PublicDelayLoadDLLs);
			PrivateIncludePaths = HashSetFromOptionalEnumerableStringParameter(InRules.PrivateIncludePaths);
			RuntimeDependencies = InRuntimeDependencies;
			IsRedistributableOverride = InRules.IsRedistributableOverride;

			WhitelistRestrictedFolders = new HashSet<DirectoryReference>(InRules.WhitelistRestrictedFolders.Select(x => DirectoryReference.Combine(ModuleDirectory, x)));
		}

		/// <summary>
		/// Returns a list of this module's dependencies.
		/// </summary>
		/// <returns>An enumerable containing the dependencies of the module.</returns>
		public HashSet<UEBuildModule> GetDependencies(bool bWithIncludePathModules, bool bWithDynamicallyLoadedModules)
		{
			HashSet<UEBuildModule> Modules = new HashSet<UEBuildModule>();
			Modules.UnionWith(PublicDependencyModules);
			Modules.UnionWith(PrivateDependencyModules);
			if(bWithIncludePathModules)
			{
				Modules.UnionWith(PublicIncludePathModules);
				Modules.UnionWith(PrivateIncludePathModules);
			}
			if(bWithDynamicallyLoadedModules)
			{
				Modules.UnionWith(DynamicallyLoadedModules);
				Modules.UnionWith(PlatformSpecificDynamicallyLoadedModules);
			}
			return Modules;
		}

		/// <summary>
		/// Returns a list of this module's immediate dependencies.
		/// </summary>
		/// <returns>An enumerable containing the dependencies of the module.</returns>
		public IEnumerable<UEBuildModule> GetDirectDependencyModules()
		{
			return PublicDependencyModules.Concat(PrivateDependencyModules).Concat(DynamicallyLoadedModules).Concat(PlatformSpecificDynamicallyLoadedModules);
		}

		/// <summary>
		/// Converts an optional string list parameter to a well-defined hash set.
		/// </summary>
		protected static HashSet<string> HashSetFromOptionalEnumerableStringParameter(IEnumerable<string> InEnumerableStrings)
		{
			return InEnumerableStrings == null ? new HashSet<string>() : new HashSet<string>(InEnumerableStrings);
		}

		/// <summary>
		/// Determines whether this module has a circular dependency on the given module
		/// </summary>
		public bool HasCircularDependencyOn(string ModuleName)
		{
			return Rules.CircularlyReferencedDependentModules.Contains(ModuleName);
		}

		/// <summary>
		/// Enumerates additional build products which may be produced by this module. Some platforms (eg. Mac, Linux) can link directly against .so/.dylibs, but they 
		/// are also copied to the output folder by the toolchain.
		/// </summary>
		/// <param name="Libraries">List to which libraries required by this module are added</param>
		/// <param name="BundleResources">List of bundle resources required by this module</param>
		public void GatherAdditionalResources(List<string> Libraries, List<UEBuildBundleResource> BundleResources)
		{
			Libraries.AddRange(PublicAdditionalLibraries);
			BundleResources.AddRange(PublicAdditionalBundleResources);
		}

		/// <summary>
		/// Determines the distribution level of a module based on its directory and includes.
		/// </summary>
		/// <param name="ProjectDir">The project directory, if available</param>
		/// <returns>Map of the restricted folder types to the first found instance</returns>
		public Dictionary<RestrictedFolder, DirectoryReference> FindRestrictedFolderReferences(DirectoryReference ProjectDir)
		{
			Dictionary<RestrictedFolder, DirectoryReference> References = new Dictionary<RestrictedFolder, DirectoryReference>();
			if (!Rules.bOutputPubliclyDistributable)
			{
				// Find all the directories that this module references
				HashSet<DirectoryReference> ReferencedDirs = new HashSet<DirectoryReference>();
				GetReferencedDirectories(ReferencedDirs);

				// Remove all the whitelisted folders
				ReferencedDirs.ExceptWith(WhitelistRestrictedFolders);
				ReferencedDirs.ExceptWith(PublicDependencyModules.SelectMany(x => x.WhitelistRestrictedFolders));
				ReferencedDirs.ExceptWith(PrivateDependencyModules.SelectMany(x => x.WhitelistRestrictedFolders));

				// Add flags for each of them
				foreach(DirectoryReference ReferencedDir in ReferencedDirs)
				{
					// Find the base directory containing this reference
					DirectoryReference BaseDir;
					if(ReferencedDir.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
					{
						BaseDir = UnrealBuildTool.EngineDirectory;
					}
					else if(ProjectDir != null && ReferencedDir.IsUnderDirectory(ProjectDir))
					{
						BaseDir = ProjectDir;
					}
					else
					{
						continue;
					}

					// Add references to each of the restricted folders
					List<RestrictedFolder> Folders = RestrictedFolders.FindRestrictedFolders(BaseDir, ReferencedDir);
					foreach(RestrictedFolder Folder in Folders)
					{
						if(!References.ContainsKey(Folder))
						{
							References.Add(Folder, ReferencedDir);
						}
					}
				}
			}
			return References;
		}

		/// <summary>
		/// Finds all the directories that this folder references when building
		/// </summary>
		/// <param name="Directories">Set of directories to add to</param>
		protected virtual void GetReferencedDirectories(HashSet<DirectoryReference> Directories)
		{
			Directories.Add(ModuleDirectory);

			foreach(string PublicIncludePath in PublicIncludePaths)
			{
				Directories.Add(new DirectoryReference(PublicIncludePath));
			}
			foreach(string PrivateIncludePath in PrivateIncludePaths)
			{
				Directories.Add(new DirectoryReference(PrivateIncludePath));
			}
			foreach(string PublicSystemIncludePath in PublicSystemIncludePaths)
			{
				Directories.Add(new DirectoryReference(PublicSystemIncludePath));
			}
			foreach(string PublicLibraryPath in PublicLibraryPaths)
			{
				Directories.Add(new DirectoryReference(PublicLibraryPath));
			}
		}

		/// <summary>
		/// Find all the modules which affect the public compile environment. Searches through 
		/// </summary>
		/// <param name="Modules"></param>
		/// <param name="ModuleToIncludePathsOnlyFlag"></param>
		protected void FindModulesInPublicCompileEnvironment(List<UEBuildModule> Modules, Dictionary<UEBuildModule, bool> ModuleToIncludePathsOnlyFlag)
		{
			//
			bool bModuleIncludePathsOnly;
			if (!ModuleToIncludePathsOnlyFlag.TryGetValue(this, out bModuleIncludePathsOnly))
			{
				Modules.Add(this);
			}
			else if (!bModuleIncludePathsOnly)
			{
				return;
			}

			ModuleToIncludePathsOnlyFlag[this] = false;

			foreach (UEBuildModule DependencyModule in PublicDependencyModules)
			{
				DependencyModule.FindModulesInPublicCompileEnvironment(Modules, ModuleToIncludePathsOnlyFlag);
			}

			// Now add an include paths from modules with header files that we need access to, but won't necessarily be importing
			foreach (UEBuildModule IncludePathModule in PublicIncludePathModules)
			{
				IncludePathModule.FindIncludePathModulesInPublicCompileEnvironment(Modules, ModuleToIncludePathsOnlyFlag);
			}
		}

		/// <summary>
		/// Find all the modules which affect the public compile environment. Searches through 
		/// </summary>
		/// <param name="Modules"></param>
		/// <param name="ModuleToIncludePathsOnlyFlag"></param>
		protected void FindIncludePathModulesInPublicCompileEnvironment(List<UEBuildModule> Modules, Dictionary<UEBuildModule, bool> ModuleToIncludePathsOnlyFlag)
		{
			if (!ModuleToIncludePathsOnlyFlag.ContainsKey(this))
			{
				// Add this module to the list
				Modules.Add(this);
				ModuleToIncludePathsOnlyFlag.Add(this, true);

				// Include any of its public include path modules in the compile environment too
				foreach (UEBuildModule IncludePathModule in PublicIncludePathModules)
				{
					IncludePathModule.FindIncludePathModulesInPublicCompileEnvironment(Modules, ModuleToIncludePathsOnlyFlag);
				}
			}
		}

		/// <summary>
		/// Sets up the environment for compiling any module that includes the public interface of this module.
		/// </summary>
		public void AddModuleToCompileEnvironment(
			UEBuildBinary SourceBinary,
			bool bIncludePathsOnly,
			HashSet<string> IncludePaths,
			HashSet<string> SystemIncludePaths,
			List<string> Definitions,
			List<UEBuildFramework> AdditionalFrameworks
			)
		{
			// Add this module's public include paths and definitions.
			AddIncludePathsWithChecks(IncludePaths, PublicIncludePaths);
			AddIncludePathsWithChecks(SystemIncludePaths, PublicSystemIncludePaths);
			Definitions.AddRange(PublicDefinitions);

			// If this module is being built into a DLL or EXE, set up an IMPORTS or EXPORTS definition for it.
			if(SourceBinary == null)
			{
				// No source binary means a shared PCH, so always import all symbols. It's possible that an include path module now may be a imported module for the shared PCH consumer.
				if(!bIncludePathsOnly)
				{
					if(Binary == null || !Binary.Config.bAllowExports)
					{
						Definitions.Add(ModuleApiDefine + "=");
					}
					else
					{
						Definitions.Add(ModuleApiDefine + "=DLLIMPORT");
					}
				}
			}
			else if (Binary == null)
			{
				// If we're referencing include paths for a module that's not being built, we don't actually need to import anything from it, but we need to avoid barfing when
				// the compiler encounters an _API define. We also want to avoid changing the compile environment in cases where the module happens to be compiled because it's a dependency
				// of something else, which cause a fall-through to the code below and set up an empty _API define.
				if (bIncludePathsOnly)
				{
					Log.TraceVerbose("{0}: Include paths only for {1} (no binary)", SourceBinary.Config.OutputFilePaths[0].GetFileNameWithoutExtension(), Name);
					Definitions.Add(ModuleApiDefine + "=");
				}
			}
			else
			{
				FileReference BinaryPath = Binary.Config.OutputFilePaths[0];
				FileReference SourceBinaryPath = SourceBinary.Config.OutputFilePaths[0];

				if (ProjectFileGenerator.bGenerateProjectFiles || (Binary.Config.Type == UEBuildBinaryType.StaticLibrary))
				{
					// When generating IntelliSense files, never add dllimport/dllexport specifiers as it
					// simply confuses the compiler
					Definitions.Add(ModuleApiDefine + "=");
				}
				else if (Binary == SourceBinary)
				{
					if (Binary.Config.bAllowExports)
					{
						Log.TraceVerbose("{0}: Exporting {1} from {2}", SourceBinaryPath.GetFileNameWithoutExtension(), Name, BinaryPath.GetFileNameWithoutExtension());
						Definitions.Add(ModuleApiDefine + "=DLLEXPORT");
					}
					else
					{
						Log.TraceVerbose("{0}: Not importing/exporting {1} (binary: {2})", SourceBinaryPath.GetFileNameWithoutExtension(), Name, BinaryPath.GetFileNameWithoutExtension());
						Definitions.Add(ModuleApiDefine + "=");
					}
				}
				else
				{
					// @todo SharedPCH: Public headers included from modules that are not importing the module of that public header, seems invalid.  
					//		Those public headers have no business having APIs in them.  OnlineSubsystem has some public headers like this.  Without changing
					//		this, we need to suppress warnings at compile time.
					if (bIncludePathsOnly)
					{
						Log.TraceVerbose("{0}: Include paths only for {1} (binary: {2})", SourceBinaryPath.GetFileNameWithoutExtension(), Name, BinaryPath.GetFileNameWithoutExtension());
						Definitions.Add(ModuleApiDefine + "=");
					}
					else if (Binary.Config.bAllowExports)
					{
						Log.TraceVerbose("{0}: Importing {1} from {2}", SourceBinaryPath.GetFileNameWithoutExtension(), Name, BinaryPath.GetFileNameWithoutExtension());
						Definitions.Add(ModuleApiDefine + "=DLLIMPORT");
					}
					else
					{
						Log.TraceVerbose("{0}: Not importing/exporting {1} (binary: {2})", SourceBinaryPath.GetFileNameWithoutExtension(), Name, BinaryPath.GetFileNameWithoutExtension());
						Definitions.Add(ModuleApiDefine + "=");
					}
				}
			}

			// Add the module's directory to the include path, so we can root #includes to it
			IncludePaths.Add(NormalizedModuleIncludePath);

			// Add the additional frameworks so that the compiler can know about their #include paths
			AdditionalFrameworks.AddRange(PublicAdditionalFrameworks);

			// Remember the module so we can refer to it when needed
			foreach (UEBuildFramework Framework in PublicAdditionalFrameworks)
			{
				Framework.OwningModule = this;
			}
		}

		static Regex VCMacroRegex = new Regex(@"\$\([A-Za-z0-9_]+\)");

		/// <summary>
		/// Checks if path contains a VC macro
		/// </summary>
		protected bool DoesPathContainVCMacro(string Path)
		{
			return VCMacroRegex.IsMatch(Path);
		}

		/// <summary>
		/// Adds PathsToAdd to IncludePaths, performing path normalization and ignoring duplicates.
		/// </summary>
		protected void AddIncludePathsWithChecks(HashSet<string> IncludePaths, HashSet<string> PathsToAdd)
		{
			if (ProjectFileGenerator.bGenerateProjectFiles)
			{
				// Extra checks are switched off for IntelliSense generation as they provide
				// no additional value and cause performance impact.
				IncludePaths.UnionWith(PathsToAdd);
			}
			else
			{
				foreach (string Path in PathsToAdd)
				{
					string NormalizedPath = Path.TrimEnd('/');
					// If path doesn't exist, it may contain VC macro (which is passed directly to and expanded by compiler).
					if (Directory.Exists(NormalizedPath) || DoesPathContainVCMacro(NormalizedPath))
					{
						IncludePaths.Add(NormalizedPath);
					}
				}
			}
		}

		/// <summary>
		/// Sets up the environment for compiling this module.
		/// </summary>
		protected virtual void SetupPrivateCompileEnvironment(
			HashSet<string> IncludePaths,
			HashSet<string> SystemIncludePaths,
			List<string> Definitions,
			List<UEBuildFramework> AdditionalFrameworks
			)
		{
			HashSet<UEBuildModule> VisitedModules = new HashSet<UEBuildModule>();

			if (this.Type.IsGameModule())
			{
				Definitions.Add("DEPRECATED_FORGAME=DEPRECATED");
			}

			// Add this module's private include paths and definitions.
			AddIncludePathsWithChecks(IncludePaths, PrivateIncludePaths);

			// Find all the modules that are part of the public compile environment for this module.
			List<UEBuildModule> Modules = new List<UEBuildModule>();
			Dictionary<UEBuildModule, bool> ModuleToIncludePathsOnlyFlag = new Dictionary<UEBuildModule, bool>();
			FindModulesInPublicCompileEnvironment(Modules, ModuleToIncludePathsOnlyFlag);

			// Add in all the modules that are private dependencies
			foreach (UEBuildModule DependencyModule in PrivateDependencyModules)
			{
				DependencyModule.FindModulesInPublicCompileEnvironment(Modules, ModuleToIncludePathsOnlyFlag);
			}

			// And finally add in all the modules that are include path only dependencies
			foreach (UEBuildModule IncludePathModule in PrivateIncludePathModules)
			{
				IncludePathModule.FindIncludePathModulesInPublicCompileEnvironment(Modules, ModuleToIncludePathsOnlyFlag);
			}

			// Now set up the compile environment for the modules in the original order that we encountered them
			foreach (UEBuildModule Module in Modules)
			{
				Module.AddModuleToCompileEnvironment(Binary, ModuleToIncludePathsOnlyFlag[Module], IncludePaths, SystemIncludePaths, Definitions, AdditionalFrameworks);
			}
		}

		/// <summary>
		/// Sets up the environment for linking any module that includes the public interface of this module.
		/// </summary>
		protected virtual void SetupPublicLinkEnvironment(
			UEBuildBinary SourceBinary,
			List<string> LibraryPaths,
			List<string> AdditionalLibraries,
			List<string> Frameworks,
			List<string> WeakFrameworks,
			List<UEBuildFramework> AdditionalFrameworks,
			List<string> AdditionalShadowFiles,
			List<UEBuildBundleResource> AdditionalBundleResources,
			List<string> DelayLoadDLLs,
			List<UEBuildBinary> BinaryDependencies,
			HashSet<UEBuildModule> VisitedModules
			)
		{
			// There may be circular dependencies in compile dependencies, so we need to avoid reentrance.
			if (VisitedModules.Add(this))
			{
				// Add this module's binary to the binary dependencies.
				if (Binary != null
					&& Binary != SourceBinary
					&& !BinaryDependencies.Contains(Binary))
				{
					BinaryDependencies.Add(Binary);
				}

				// If this module belongs to a static library that we are not currently building, recursively add the link environment settings for all of its dependencies too.
				// Keep doing this until we reach a module that is not part of a static library (or external module, since they have no associated binary).
				// Static libraries do not contain the symbols for their dependencies, so we need to recursively gather them to be linked into other binary types.
				bool bIsBuildingAStaticLibrary = (SourceBinary != null && SourceBinary.Config.Type == UEBuildBinaryType.StaticLibrary);
				bool bIsModuleBinaryAStaticLibrary = (Binary != null && Binary.Config.Type == UEBuildBinaryType.StaticLibrary);
				if (!bIsBuildingAStaticLibrary && bIsModuleBinaryAStaticLibrary)
				{
					// Gather all dependencies and recursively call SetupPublicLinkEnvironmnet
					List<UEBuildModule> AllDependencyModules = new List<UEBuildModule>();
					AllDependencyModules.AddRange(PrivateDependencyModules);
					AllDependencyModules.AddRange(PublicDependencyModules);

					foreach (UEBuildModule DependencyModule in AllDependencyModules)
					{
						bool bIsExternalModule = (DependencyModule as UEBuildModuleExternal != null);
						bool bIsInStaticLibrary = (DependencyModule.Binary != null && DependencyModule.Binary.Config.Type == UEBuildBinaryType.StaticLibrary);
						if (bIsExternalModule || bIsInStaticLibrary)
						{
							DependencyModule.SetupPublicLinkEnvironment(SourceBinary, LibraryPaths, AdditionalLibraries, Frameworks, WeakFrameworks,
								AdditionalFrameworks, AdditionalShadowFiles, AdditionalBundleResources, DelayLoadDLLs, BinaryDependencies, VisitedModules);
						}
					}
				}

				// Add this module's public include library paths and additional libraries.
				LibraryPaths.AddRange(PublicLibraryPaths);
				AdditionalLibraries.AddRange(PublicAdditionalLibraries);
				Frameworks.AddRange(PublicFrameworks);
				WeakFrameworks.AddRange(PublicWeakFrameworks);
				AdditionalBundleResources.AddRange(PublicAdditionalBundleResources);
				// Remember the module so we can refer to it when needed
				foreach (UEBuildFramework Framework in PublicAdditionalFrameworks)
				{
					Framework.OwningModule = this;
				}
				AdditionalFrameworks.AddRange(PublicAdditionalFrameworks);
				AdditionalShadowFiles.AddRange(PublicAdditionalShadowFiles);
				DelayLoadDLLs.AddRange(PublicDelayLoadDLLs);
			}
		}

		/// <summary>
		/// Sets up the environment for linking this module.
		/// </summary>
		public virtual void SetupPrivateLinkEnvironment(
			UEBuildBinary SourceBinary,
			LinkEnvironment LinkEnvironment,
			List<UEBuildBinary> BinaryDependencies,
			HashSet<UEBuildModule> VisitedModules
			)
		{
			// Allow the module's public dependencies to add library paths and additional libraries to the link environment.
			SetupPublicLinkEnvironment(SourceBinary, LinkEnvironment.LibraryPaths, LinkEnvironment.AdditionalLibraries, LinkEnvironment.Frameworks, LinkEnvironment.WeakFrameworks,
				LinkEnvironment.AdditionalFrameworks, LinkEnvironment.AdditionalShadowFiles, LinkEnvironment.AdditionalBundleResources, LinkEnvironment.DelayLoadDLLs, BinaryDependencies, VisitedModules);

			// Also allow the module's public and private dependencies to modify the link environment.
			List<UEBuildModule> AllDependencyModules = new List<UEBuildModule>();
			AllDependencyModules.AddRange(PrivateDependencyModules);
			AllDependencyModules.AddRange(PublicDependencyModules);

			foreach (UEBuildModule DependencyModule in AllDependencyModules)
			{
				DependencyModule.SetupPublicLinkEnvironment(SourceBinary, LinkEnvironment.LibraryPaths, LinkEnvironment.AdditionalLibraries, LinkEnvironment.Frameworks, LinkEnvironment.WeakFrameworks,
					LinkEnvironment.AdditionalFrameworks, LinkEnvironment.AdditionalShadowFiles, LinkEnvironment.AdditionalBundleResources, LinkEnvironment.DelayLoadDLLs, BinaryDependencies, VisitedModules);
			}
		}

		/// <summary>
		/// Compiles the module, and returns a list of files output by the compiler.
		/// </summary>
		public abstract List<FileItem> Compile(ReadOnlyTargetRules Target, UEToolChain ToolChain, CppCompileEnvironment CompileEnvironment, List<PrecompiledHeaderTemplate> SharedPCHModules, ISourceFileWorkingSet WorkingSet, ActionGraph ActionGraph);

		// Object interface.
		public override string ToString()
		{
			return Name;
		}

		/// <summary>
		/// Finds the modules referenced by this module which have not yet been bound to a binary
		/// </summary>
		/// <returns>List of unbound modules</returns>
		public List<UEBuildModule> GetUnboundReferences()
		{
			List<UEBuildModule> Modules = new List<UEBuildModule>();
			Modules.AddRange(PrivateDependencyModules.Where(x => x.Binary == null));
			Modules.AddRange(PublicDependencyModules.Where(x => x.Binary == null));
			return Modules;
		}

		/// <summary>
		/// Gets all of the modules referenced by this module
		/// </summary>
		/// <param name="ReferencedModules">Hash of all referenced modules with their addition index.</param>
		/// <param name="IgnoreReferencedModules">Hashset used to ignore modules which are already added to the list</param>
		/// <param name="bIncludeDynamicallyLoaded">True if dynamically loaded modules (and all of their dependent modules) should be included.</param>
		/// <param name="bForceCircular">True if circular dependencies should be processed</param>
		/// <param name="bOnlyDirectDependencies">True to return only this module's direct dependencies</param>
		public virtual void GetAllDependencyModules(List<UEBuildModule> ReferencedModules, HashSet<UEBuildModule> IgnoreReferencedModules, bool bIncludeDynamicallyLoaded, bool bForceCircular, bool bOnlyDirectDependencies)
		{
		}

		/// <summary>
		/// Gets all of the modules precompiled along with this module
		/// </summary>
		/// <param name="Modules">Set of all the precompiled modules</param>
		public virtual void RecursivelyAddPrecompiledModules(List<UEBuildModule> Modules)
		{
		}

		public delegate UEBuildModule CreateModuleDelegate(string Name, string ReferenceChain);

		/// <summary>
		/// Creates all the modules required for this target
		/// </summary>
		/// <param name="CreateModule">Delegate to create a module with a given name</param>
		/// <param name="ReferenceChain">Chain of references before reaching this module</param>
		public void RecursivelyCreateModules(CreateModuleDelegate CreateModule, string ReferenceChain)
		{
			// Get the reference chain for anything referenced by this module
			string NextReferenceChain = String.Format("{0} -> {1}", ReferenceChain, (RulesFile == null)? Name : RulesFile.GetFileName());

			// Recursively create all the public include path modules. These modules may not be added to the target (and we don't process their referenced 
			// dependencies), but they need to be created to set up their include paths.
			RecursivelyCreateIncludePathModulesByName(Rules.PublicIncludePathModuleNames, ref PublicIncludePathModules, CreateModule, NextReferenceChain);

			// Create all the referenced modules. This path can be recursive, so we check against PrivateIncludePathModules to ensure we don't recurse through the 
			// same module twice (it produces better errors if something fails).
			if(PrivateIncludePathModules == null)
			{
				// Create the private include path modules
				RecursivelyCreateIncludePathModulesByName(Rules.PrivateIncludePathModuleNames, ref PrivateIncludePathModules, CreateModule, NextReferenceChain);

				// Create all the dependency modules
				RecursivelyCreateModulesByName(Rules.PublicDependencyModuleNames, ref PublicDependencyModules, CreateModule, NextReferenceChain);
				RecursivelyCreateModulesByName(Rules.PrivateDependencyModuleNames, ref PrivateDependencyModules, CreateModule, NextReferenceChain);
				RecursivelyCreateModulesByName(Rules.DynamicallyLoadedModuleNames, ref DynamicallyLoadedModules, CreateModule, NextReferenceChain);
				RecursivelyCreateModulesByName(Rules.PlatformSpecificDynamicallyLoadedModuleNames, ref PlatformSpecificDynamicallyLoadedModules, CreateModule, NextReferenceChain);
			}
		}

		private static void RecursivelyCreateModulesByName(List<string> ModuleNames, ref List<UEBuildModule> Modules, CreateModuleDelegate CreateModule, string ReferenceChain)
		{
			// Check whether the module list is already set. We set this immediately (via the ref) to avoid infinite recursion.
			if (Modules == null)
			{
				Modules = new List<UEBuildModule>();
				foreach (string ModuleName in ModuleNames)
				{
					UEBuildModule Module = CreateModule(ModuleName, ReferenceChain);
					if (!Modules.Contains(Module))
					{
						Module.RecursivelyCreateModules(CreateModule, ReferenceChain);
						Modules.Add(Module);
					}
				}
			}
		}

		private static void RecursivelyCreateIncludePathModulesByName(List<string> ModuleNames, ref List<UEBuildModule> Modules, CreateModuleDelegate CreateModule, string ReferenceChain)
		{
			// Check whether the module list is already set. We set this immediately (via the ref) to avoid infinite recursion.
			if (Modules == null)
			{
				Modules = new List<UEBuildModule>();
				foreach (string ModuleName in ModuleNames)
				{
					UEBuildModule Module = CreateModule(ModuleName, ReferenceChain);
					RecursivelyCreateIncludePathModulesByName(Module.Rules.PublicIncludePathModuleNames, ref Module.PublicIncludePathModules, CreateModule, ReferenceChain);
					Modules.Add(Module);
				}
			}
		}

		/// <summary>
		/// Write information about this binary to a JSON file
		/// </summary>
		/// <param name="Writer">Writer for this binary's data</param>
		public virtual void ExportJson(JsonWriter Writer)
		{
			Writer.WriteValue("Name", Name);
			Writer.WriteValue("Type", Type.ToString());
			Writer.WriteValue("Directory", ModuleDirectory.FullName);
			Writer.WriteValue("Rules", RulesFile.FullName);
			Writer.WriteValue("PCHUsage", Rules.PCHUsage.ToString());

			if (Rules.PrivatePCHHeaderFile != null)
			{
				Writer.WriteValue("PrivatePCH", FileReference.Combine(ModuleDirectory, Rules.PrivatePCHHeaderFile).FullName);
			}

			if (Rules.SharedPCHHeaderFile != null)
			{
				Writer.WriteValue("SharedPCH", FileReference.Combine(ModuleDirectory, Rules.SharedPCHHeaderFile).FullName);
			}

			ExportJsonModuleArray(Writer, "PublicDependencyModules", PublicDependencyModules);
			ExportJsonModuleArray(Writer, "PublicIncludePathModules", PublicIncludePathModules);
			ExportJsonModuleArray(Writer, "PrivateDependencyModules", PrivateDependencyModules);
			ExportJsonModuleArray(Writer, "PrivateIncludePathModules", PrivateIncludePathModules);
			ExportJsonModuleArray(Writer, "DynamicallyLoadedModules", DynamicallyLoadedModules);

			Writer.WriteArrayStart("CircularlyReferencedModules");
			foreach(string ModuleName in Rules.CircularlyReferencedDependentModules)
			{
				Writer.WriteValue(ModuleName);
			}
			Writer.WriteArrayEnd();

			Writer.WriteArrayStart("RuntimeDependencies");
			foreach(RuntimeDependency RuntimeDependency in RuntimeDependencies)
			{
				Writer.WriteObjectStart();
				Writer.WriteValue("Path", RuntimeDependency.Path.FullName);
				Writer.WriteValue("Type", RuntimeDependency.Type.ToString());
				Writer.WriteObjectEnd();
			}
			Writer.WriteArrayEnd();
		}

		/// <summary>
		/// Write an array of module names to a JSON writer
		/// </summary>
		/// <param name="Writer">Writer for the array data</param>
		/// <param name="ArrayName">Name of the array property</param>
		/// <param name="Modules">Sequence of modules to write. May be null.</param>
		void ExportJsonModuleArray(JsonWriter Writer, string ArrayName, IEnumerable<UEBuildModule> Modules)
		{
			Writer.WriteArrayStart(ArrayName);
			if (Modules != null)
			{
				foreach (UEBuildModule Module in Modules)
				{
					Writer.WriteValue(Module.Name);
				}
			}
			Writer.WriteArrayEnd();
		}
	};
}
