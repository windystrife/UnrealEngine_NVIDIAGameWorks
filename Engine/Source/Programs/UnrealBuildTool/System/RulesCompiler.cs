// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Diagnostics;
using System.Runtime.Serialization;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Class which compiles (and caches) rules assemblies for different folders.
	/// </summary>
	public class RulesCompiler
	{
		/// <summary>
		/// Enum for types of rules files. Should match extensions in RulesFileExtensions.
		/// </summary>
		public enum RulesFileType
		{
			/// <summary>
			/// .build.cs files
			/// </summary>
			Module,

			/// <summary>
			/// .target.cs files
			/// </summary>
			Target,

			/// <summary>
			/// .automation.csproj files
			/// </summary>
			AutomationModule
		}

		/// <summary>
		/// Cached list of rules files in each directory of each type
		/// </summary>
		class RulesFileCache
		{
			public List<FileReference> ModuleRules = new List<FileReference>();
			public List<FileReference> TargetRules = new List<FileReference>();
			public List<FileReference> AutomationModules = new List<FileReference>();
		}

		/// Map of root folders to a cached list of all UBT-related source files in that folder or any of its sub-folders.
		/// We cache these file names so we can avoid searching for them later on.
		static Dictionary<DirectoryReference, RulesFileCache> RootFolderToRulesFileCache = new Dictionary<DirectoryReference, RulesFileCache>();

		/// <summary>
		/// 
		/// </summary>
#if NET_CORE
		const string FrameworkAssemblyExtension = "_NetCore.dll";
#else
		const string FrameworkAssemblyExtension = ".dll";
#endif

		/// <summary>
		/// 
		/// </summary>
		/// <param name="RulesFileType"></param>
		/// <param name="GameFolders"></param>
		/// <param name="ForeignPlugins"></param>
		/// <param name="AdditionalSearchPaths"></param>
		/// <param name="bIncludeEngine"></param>
		/// <param name="bIncludeEnterprise"></param>
		/// <returns></returns>
		public static List<FileReference> FindAllRulesSourceFiles(RulesFileType RulesFileType, List<DirectoryReference> GameFolders, List<FileReference> ForeignPlugins, List<DirectoryReference> AdditionalSearchPaths, bool bIncludeEngine = true, bool bIncludeEnterprise = true)
		{
			List<DirectoryReference> Folders = new List<DirectoryReference>();

			// Add all engine source (including third party source)
			if (bIncludeEngine)
			{
				Folders.Add(UnrealBuildTool.EngineSourceDirectory);
			}
			if(bIncludeEnterprise)
			{
				Folders.Add(UnrealBuildTool.EnterpriseSourceDirectory);
			}

			// @todo plugin: Disallow modules from including plugin modules as dependency modules? (except when the module is part of that plugin)

			// Get all the root folders for plugins
			List<DirectoryReference> RootFolders = new List<DirectoryReference>();
			if (bIncludeEngine)
			{
				RootFolders.Add(UnrealBuildTool.EngineDirectory);
			}
			if(bIncludeEnterprise)
			{
				RootFolders.Add(UnrealBuildTool.EnterpriseDirectory);
			}
			if (GameFolders != null)
			{
				RootFolders.AddRange(GameFolders);
			}

			// Find all the plugin source directories
			foreach (DirectoryReference RootFolder in RootFolders)
			{
				DirectoryReference PluginsFolder = DirectoryReference.Combine(RootFolder, "Plugins");
				foreach (FileReference PluginFile in Plugins.EnumeratePlugins(PluginsFolder))
				{
					Folders.Add(DirectoryReference.Combine(PluginFile.Directory, "Source"));
				}
			}

			// Add all the extra plugin folders
			if (ForeignPlugins != null)
			{
				foreach (FileReference ForeignPlugin in ForeignPlugins)
				{
					Folders.Add(DirectoryReference.Combine(ForeignPlugin.Directory, "Source"));
				}
			}

			// Add in the game folders to search
			if (GameFolders != null)
			{
				foreach (DirectoryReference GameFolder in GameFolders)
				{
					DirectoryReference GameSourceFolder = DirectoryReference.Combine(GameFolder, "Source");
					Folders.Add(GameSourceFolder);
					DirectoryReference GameIntermediateSourceFolder = DirectoryReference.Combine(GameFolder, "Intermediate", "Source");
					Folders.Add(GameIntermediateSourceFolder);
				}
			}

			// Process the additional search path, if sent in
			if (AdditionalSearchPaths != null)
			{
				foreach (DirectoryReference AdditionalSearchPath in AdditionalSearchPaths)
				{
					if (AdditionalSearchPath != null)
					{
						if (DirectoryReference.Exists(AdditionalSearchPath))
						{
							Folders.Add(AdditionalSearchPath);
						}
						else
						{
							throw new BuildException("Couldn't find AdditionalSearchPath for rules source files '{0}'", AdditionalSearchPath);
						}
					}
				}
			}

			// Iterate over all the folders to check
			List<FileReference> SourceFiles = new List<FileReference>();
			HashSet<FileReference> UniqueSourceFiles = new HashSet<FileReference>();
			foreach (DirectoryReference Folder in Folders)
			{
				IReadOnlyList<FileReference> SourceFilesForFolder = FindAllRulesFiles(Folder, RulesFileType);
				foreach (FileReference SourceFile in SourceFilesForFolder)
				{
					if (UniqueSourceFiles.Add(SourceFile))
					{
						SourceFiles.Add(SourceFile);
					}
				}
			}
			return SourceFiles;
		}

		/// <summary>
		/// Invalidate the cache for the givcen directory
		/// </summary>
		/// <param name="DirectoryPath">Directory to invalidate</param>
        public static void InvalidateRulesFileCache(string DirectoryPath)
        {
            DirectoryReference Directory = new DirectoryReference(DirectoryPath);
            RootFolderToRulesFileCache.Remove(Directory);
            DirectoryLookupCache.InvalidateCachedDirectory(Directory);
        }

		private static IReadOnlyList<FileReference> FindAllRulesFiles(DirectoryReference Directory, RulesFileType Type)
		{
			// Check to see if we've already cached source files for this folder
			RulesFileCache Cache;
			if (!RootFolderToRulesFileCache.TryGetValue(Directory, out Cache))
			{
				Cache = new RulesFileCache();
				FindAllRulesFilesRecursively(Directory, Cache);
					RootFolderToRulesFileCache[Directory] = Cache;
				}

			// Get the list of files of the type we're looking for
			if (Type == RulesCompiler.RulesFileType.Module)
			{
				return Cache.ModuleRules;
			}
			else if (Type == RulesCompiler.RulesFileType.Target)
			{
				return Cache.TargetRules;
			}
			else if (Type == RulesCompiler.RulesFileType.AutomationModule)
			{
				return Cache.AutomationModules;
			}
			else
			{
				throw new BuildException("Unhandled rules type: {0}", Type);
			}
		}

		private static void FindAllRulesFilesRecursively(DirectoryReference Directory, RulesFileCache Cache)
		{
			// Scan all the files in this directory
			bool bSearchSubFolders = true;
			foreach (FileReference File in DirectoryLookupCache.EnumerateFiles(Directory))
			{
				if (File.HasExtension(".build.cs"))
				{
					Cache.ModuleRules.Add(File);
					bSearchSubFolders = false;
				}
				else if (File.HasExtension(".target.cs"))
				{
					Cache.TargetRules.Add(File);
				}
				else if (File.HasExtension(".automation.csproj"))
				{
					Cache.AutomationModules.Add(File);
					bSearchSubFolders = false;
				}
			}

			// If we didn't find anything to stop the search, search all the subdirectories too
			if (bSearchSubFolders)
			{
				foreach (DirectoryReference SubDirectory in DirectoryLookupCache.EnumerateDirectories(Directory))
				{
					FindAllRulesFilesRecursively(SubDirectory, Cache);
				}
			}
		}

		/// <summary>
		/// The cached rules assembly for engine modules and targets.
		/// </summary>
		private static RulesAssembly EngineRulesAssembly;

		/// <summary>
		/// The cached rules assembly for enterprise modules and targets.
		/// </summary>
		private static RulesAssembly EnterpriseRulesAssembly;

		/// Map of assembly names we've already compiled and loaded to their Assembly and list of game folders.  This is used to prevent
		/// trying to recompile the same assembly when ping-ponging between different types of targets
		private static Dictionary<FileReference, RulesAssembly> LoadedAssemblyMap = new Dictionary<FileReference, RulesAssembly>();

		/// <summary>
		/// Creates the engine rules assembly
		/// </summary>
		/// <returns>New rules assembly</returns>
		public static RulesAssembly CreateEngineRulesAssembly()
		{
			if (EngineRulesAssembly == null)
			{
				IReadOnlyList<PluginInfo> IncludedPlugins = Plugins.ReadEnginePlugins(UnrealBuildTool.EngineDirectory);
				EngineRulesAssembly = CreateRulesAssembly(UnrealBuildTool.EngineDirectory, ProjectFileGenerator.EngineProjectFileNameBase, IncludedPlugins, null);
			}
			return EngineRulesAssembly;
		}

		/// <summary>
		/// Creates the enterprise rules assembly
		/// </summary>
		/// <returns>New rules assembly. Returns null if the enterprise directory is unavailable.</returns>
		public static RulesAssembly CreateEnterpriseRulesAssembly()
		{
			if (EnterpriseRulesAssembly == null && DirectoryReference.Exists(UnrealBuildTool.EnterpriseDirectory))
			{
				IReadOnlyList<PluginInfo> IncludedPlugins = Plugins.ReadEnterprisePlugins(UnrealBuildTool.EnterpriseDirectory);
				EnterpriseRulesAssembly = CreateRulesAssembly(UnrealBuildTool.EnterpriseDirectory, ProjectFileGenerator.EnterpriseProjectFileNameBase, IncludedPlugins, CreateEngineRulesAssembly());
			}
			return EnterpriseRulesAssembly;
		}

		/// <summary>
		/// Creates a rules assembly
		/// </summary>
		/// <param name="RootDirectory">The root directory to create rules for</param>
		/// <param name="AssemblyPrefix">A prefix for the assembly file name</param>
		/// <param name="Plugins">List of plugins to include in this assembly</param>
		/// <param name="Parent">The parent rules assembly</param>
		/// <returns>New rules assembly</returns>
		private static RulesAssembly CreateRulesAssembly(DirectoryReference RootDirectory, string AssemblyPrefix, IReadOnlyList<PluginInfo> Plugins, RulesAssembly Parent)
		{
			DirectoryReference SourceDirectory = DirectoryReference.Combine(RootDirectory, "Source");

			// Find all the rules files
			List<FileReference> ModuleFiles = new List<FileReference>(FindAllRulesFiles(SourceDirectory, RulesFileType.Module));
			List<FileReference> TargetFiles = new List<FileReference>(FindAllRulesFiles(SourceDirectory, RulesFileType.Target));

			// Add all the plugin modules too
			Dictionary<FileReference, PluginInfo> ModuleFileToPluginInfo = new Dictionary<FileReference, PluginInfo>();
			FindModuleRulesForPlugins(Plugins, ModuleFiles, ModuleFileToPluginInfo);

			// Create a path to the assembly that we'll either load or compile
			FileReference AssemblyFileName = FileReference.Combine(RootDirectory, "Intermediate", "Build", "BuildRules", AssemblyPrefix + "Rules" + FrameworkAssemblyExtension);
			return new RulesAssembly(Plugins, ModuleFiles, TargetFiles, ModuleFileToPluginInfo, AssemblyFileName, Parent);
		}

		/// <summary>
		/// Creates a rules assembly with the given parameters.
		/// </summary>
		/// <param name="ProjectFileName">The project file to create rules for. Null for the engine.</param>
		/// <returns>New rules assembly</returns>
		public static RulesAssembly CreateProjectRulesAssembly(FileReference ProjectFileName)
		{
			// Check if there's an existing assembly for this project
			RulesAssembly ProjectRulesAssembly;
			if (!LoadedAssemblyMap.TryGetValue(ProjectFileName, out ProjectRulesAssembly))
			{
				ProjectDescriptor Project = ProjectDescriptor.FromFile(ProjectFileName);

				// Create the parent assembly
				RulesAssembly Parent;
				if (Project.IsEnterpriseProject)
				{
					Parent = CreateEnterpriseRulesAssembly();
				}
				else
				{
					Parent = CreateEngineRulesAssembly();
				}

				// Find all the rules under the project source directory
				DirectoryReference ProjectDirectory = ProjectFileName.Directory;
				DirectoryReference ProjectSourceDirectory = DirectoryReference.Combine(ProjectDirectory, "Source");
				List<FileReference> ModuleFiles = new List<FileReference>(FindAllRulesFiles(ProjectSourceDirectory, RulesFileType.Module));
				List<FileReference> TargetFiles = new List<FileReference>(FindAllRulesFiles(ProjectSourceDirectory, RulesFileType.Target));

				// Find all the project plugins
				List<PluginInfo> ProjectPlugins = new List<PluginInfo>(Plugins.ReadProjectPlugins(ProjectFileName.Directory));

                // Add the project's additional plugin directories plugins too
				if(Project.AdditionalPluginDirectories != null)
				{
					foreach(string AdditionalPluginDirectory in Project.AdditionalPluginDirectories)
					{
						ProjectPlugins.AddRange(Plugins.ReadAdditionalPlugins(DirectoryReference.Combine(ProjectFileName.Directory, AdditionalPluginDirectory)));
					}
				}

				// Find all the plugin module rules
                Dictionary<FileReference, PluginInfo> ModuleFileToPluginInfo = new Dictionary<FileReference, PluginInfo>();
				FindModuleRulesForPlugins(ProjectPlugins, ModuleFiles, ModuleFileToPluginInfo);

				// Add the games project's intermediate source folder
				DirectoryReference ProjectIntermediateSourceDirectory = DirectoryReference.Combine(ProjectDirectory, "Intermediate", "Source");
				if (DirectoryReference.Exists(ProjectIntermediateSourceDirectory))
				{
					TargetFiles.AddRange(FindAllRulesFiles(ProjectIntermediateSourceDirectory, RulesFileType.Target));
				}

				// Compile the assembly
				FileReference AssemblyFileName = FileReference.Combine(ProjectDirectory, "Intermediate", "Build", "BuildRules", ProjectFileName.GetFileNameWithoutExtension() + "ModuleRules" + FrameworkAssemblyExtension);
				ProjectRulesAssembly = new RulesAssembly(ProjectPlugins, ModuleFiles, TargetFiles, ModuleFileToPluginInfo, AssemblyFileName, Parent);
				LoadedAssemblyMap.Add(ProjectFileName, ProjectRulesAssembly);
			}
			return ProjectRulesAssembly;
		}

		/// <summary>
		/// Creates a rules assembly with the given parameters.
		/// </summary>
		/// <param name="PluginFileName">The plugin file to create rules for</param>
		/// <param name="Parent">The parent rules assembly</param>
		/// <returns>The new rules assembly</returns>
		public static RulesAssembly CreatePluginRulesAssembly(FileReference PluginFileName, RulesAssembly Parent)
		{
			// Check if there's an existing assembly for this project
			RulesAssembly PluginRulesAssembly;
			if (!LoadedAssemblyMap.TryGetValue(PluginFileName, out PluginRulesAssembly))
			{
				// Find all the rules source files
				List<FileReference> ModuleFiles = new List<FileReference>();
				List<FileReference> TargetFiles = new List<FileReference>();

				// Create a list of plugins for this assembly. If it already exists in the parent assembly, just create an empty assembly.
				List<PluginInfo> ForeignPlugins = new List<PluginInfo>();
				if (Parent == null || !Parent.EnumeratePlugins().Any(x => x.File == PluginFileName))
				{
					ForeignPlugins.Add(new PluginInfo(PluginFileName, PluginType.External));
				}

				// Find all the modules
				Dictionary<FileReference, PluginInfo> ModuleFileToPluginInfo = new Dictionary<FileReference, PluginInfo>();
				FindModuleRulesForPlugins(ForeignPlugins, ModuleFiles, ModuleFileToPluginInfo);

				// Compile the assembly
				FileReference AssemblyFileName = FileReference.Combine(PluginFileName.Directory, "Intermediate", "Build", "BuildRules", Path.GetFileNameWithoutExtension(PluginFileName.FullName) + "ModuleRules" + FrameworkAssemblyExtension);
				PluginRulesAssembly = new RulesAssembly(ForeignPlugins, ModuleFiles, TargetFiles, ModuleFileToPluginInfo, AssemblyFileName, Parent);
				LoadedAssemblyMap.Add(PluginFileName, PluginRulesAssembly);
			}
			return PluginRulesAssembly;
		}

		/// <summary>
		/// Finds all the module rules for plugins under the given directory.
		/// </summary>
		/// <param name="Plugins">The directory to search</param>
		/// <param name="ModuleFiles">List of module files to be populated</param>
		/// <param name="ModuleFileToPluginInfo">Dictionary which is filled with mappings from the module file to its corresponding plugin file</param>
		private static void FindModuleRulesForPlugins(IReadOnlyList<PluginInfo> Plugins, List<FileReference> ModuleFiles, Dictionary<FileReference, PluginInfo> ModuleFileToPluginInfo)
		{
			foreach (PluginInfo Plugin in Plugins)
			{
				IReadOnlyList<FileReference> PluginModuleFiles = FindAllRulesFiles(DirectoryReference.Combine(Plugin.Directory, "Source"), RulesFileType.Module);
				foreach (FileReference ModuleFile in PluginModuleFiles)
				{
					ModuleFiles.Add(ModuleFile);
					ModuleFileToPluginInfo[ModuleFile] = Plugin;
				}
			}
		}

		/// <summary>
		/// Gets the filename that declares the given type.
		/// </summary>
		/// <param name="ExistingType">The type to search for.</param>
		/// <returns>The filename that declared the given type, or null</returns>
		public static string GetFileNameFromType(Type ExistingType)
		{
			FileReference FileName;
			if (EngineRulesAssembly != null && EngineRulesAssembly.TryGetFileNameFromType(ExistingType, out FileName))
			{
				return FileName.FullName;
			}
			else if (EnterpriseRulesAssembly != null && EnterpriseRulesAssembly.TryGetFileNameFromType(ExistingType, out FileName))
			{
				return FileName.FullName;
			}

			foreach (RulesAssembly RulesAssembly in LoadedAssemblyMap.Values)
			{
				if (RulesAssembly.TryGetFileNameFromType(ExistingType, out FileName))
				{
					return FileName.FullName;
				}
			}
			return null;
		}
    }
}
