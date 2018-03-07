// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Where a plugin was loaded from
	/// </summary>
	public enum PluginLoadedFrom
	{
		/// <summary>
		/// Plugin is built-in to the engine
		/// </summary>
		Engine,

		/// <summary>
		/// Project-specific plugin, stored within a game project directory
		/// </summary>
		Project
	}

	/// <summary>
	/// Where a plugin was loaded from
	/// </summary>
	public enum PluginType
	{
		/// <summary>
		/// Plugin is built-in to the engine
		/// </summary>
		Engine,

		/// <summary>
		/// Enterprise plugin
		/// </summary>
		Enterprise,

		/// <summary>
		/// Project-specific plugin, stored within a game project directory
		/// </summary>
		Project,

		/// <summary>
		/// Plugin found in an external directory (found in an AdditionalPluginDirectory listed in the project file, or referenced on the command line)
		/// </summary>
		External,

		/// <summary>
		/// Project-specific mod plugin
		/// </summary>
		Mod,
	}

	/// <summary>
	/// Information about a single plugin
	/// </summary>
	[DebuggerDisplay("\\{{File}\\}")]
	public class PluginInfo
	{
		/// <summary>
		/// Plugin name
		/// </summary>
		public readonly string Name;

		/// <summary>
		/// Path to the plugin
		/// </summary>
		public readonly FileReference File;

		/// <summary>
		/// Path to the plugin's root directory
		/// </summary>
		public readonly DirectoryReference Directory;

		/// <summary>
		/// The plugin descriptor
		/// </summary>
		public PluginDescriptor Descriptor;

		/// <summary>
		/// The type of this plugin
		/// </summary>
		public PluginType Type;

		/// <summary>
		/// Constructs a PluginInfo object
		/// </summary>
		/// <param name="InFile">Path to the plugin descriptor</param>
		/// <param name="InType">The type of this plugin</param>
		public PluginInfo(FileReference InFile, PluginType InType)
		{
			Name = Path.GetFileNameWithoutExtension(InFile.FullName);
			File = InFile;
			Directory = File.Directory;
			Descriptor = PluginDescriptor.FromFile(File);
			Type = InType;
		}

		/// <summary>
		/// Determines whether the plugin should be enabled by default
		/// </summary>
		public bool EnabledByDefault
		{
			get
			{
				if(Descriptor.bEnabledByDefault.HasValue)
				{
					return Descriptor.bEnabledByDefault.Value;
				}
				else
				{
					return (LoadedFrom == PluginLoadedFrom.Project);
				}
			}
		}

		/// <summary>
		/// Determines where the plugin was loaded from
		/// </summary>
		public PluginLoadedFrom LoadedFrom
		{
			get
			{
				if(Type == PluginType.Engine || Type == PluginType.Enterprise)
				{
					return PluginLoadedFrom.Engine;
				}
				else
				{
					return PluginLoadedFrom.Project;
				}
			}
		}
	}

	/// <summary>
	/// Class for enumerating plugin metadata
	/// </summary>
	public static class Plugins
	{
		/// <summary>
		/// Cache of plugins under each directory
		/// </summary>
		static Dictionary<DirectoryReference, List<PluginInfo>> PluginInfoCache = new Dictionary<DirectoryReference, List<PluginInfo>>();

		/// <summary>
		/// Cache of plugin filenames under each directory
		/// </summary>
		static Dictionary<DirectoryReference, List<FileReference>> PluginFileCache = new Dictionary<DirectoryReference, List<FileReference>>();

		/// <summary>
		/// Filters the list of plugins to ensure that any game plugins override engine plugins with the same name, and otherwise that no two
		/// plugins with the same name exist. 
		/// </summary>
		/// <param name="Plugins">List of plugins to filter</param>
		/// <returns>Filtered list of plugins in the original order</returns>
		public static IEnumerable<PluginInfo> FilterPlugins(IEnumerable<PluginInfo> Plugins)
		{
			Dictionary<string, PluginInfo> NameToPluginInfo = new Dictionary<string, PluginInfo>(StringComparer.InvariantCultureIgnoreCase);
			foreach(PluginInfo Plugin in Plugins)
			{
				PluginInfo ExistingPluginInfo;
				if(!NameToPluginInfo.TryGetValue(Plugin.Name, out ExistingPluginInfo))
				{
					NameToPluginInfo.Add(Plugin.Name, Plugin);
				}
				else if(ExistingPluginInfo.Type == PluginType.Engine && Plugin.Type == PluginType.Project)
				{
					NameToPluginInfo[Plugin.Name] = Plugin;
				}
				else if(ExistingPluginInfo.Type != PluginType.Project || Plugin.Type != PluginType.Engine)
				{
					throw new BuildException(String.Format("Found '{0}' plugin in two locations ({1} and {2}). Plugin names must be unique.", Plugin.Name, ExistingPluginInfo.File, Plugin.File));
				}
			}
			return Plugins.Where(x => NameToPluginInfo[x.Name] == x);
		}

		/// <summary>
		/// Read all the plugins available to a given project
		/// </summary>
		/// <param name="EngineDirectoryName">Path to the engine directory</param>
		/// <param name="ProjectFileName">Path to the project file (or null)</param>
        /// <param name="AdditionalDirectories">List of additional directories to scan for available plugins</param>
		/// <returns>Sequence of PluginInfo objects, one for each discovered plugin</returns>
		public static List<PluginInfo> ReadAvailablePlugins(DirectoryReference EngineDirectoryName, FileReference ProjectFileName, string[] AdditionalDirectories)
		{
			List<PluginInfo> Plugins = new List<PluginInfo>();

			// Read all the engine plugins
			Plugins.AddRange(ReadEnginePlugins(EngineDirectoryName));

			// Read all the project plugins
			if (ProjectFileName != null)
			{
				Plugins.AddRange(ReadProjectPlugins(ProjectFileName.Directory));
			}

            // Scan for shared plugins in project specified additional directories
			if(AdditionalDirectories != null)
			{
				foreach (string AdditionalDirectory in AdditionalDirectories)
				{
					DirectoryReference DirRef = DirectoryReference.Combine(ProjectFileName.Directory, AdditionalDirectory);
					Plugins.AddRange(ReadPluginsFromDirectory(DirRef, PluginType.External));
				}
			}

            return Plugins;
		}

		/// <summary>
		/// Enumerates all the plugin files available to the given project
		/// </summary>
		/// <param name="ProjectFile">Path to the project file</param>
		/// <returns>List of project files</returns>
		public static IEnumerable<FileReference> EnumeratePlugins(FileReference ProjectFile)
		{
			DirectoryReference EnginePluginsDir = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Plugins");
			foreach(FileReference PluginFile in EnumeratePlugins(EnginePluginsDir))
			{
				yield return PluginFile;
			}

			DirectoryReference EnterprisePluginsDir = DirectoryReference.Combine(UnrealBuildTool.EnterpriseDirectory, "Plugins");
			foreach(FileReference PluginFile in EnumeratePlugins(EnterprisePluginsDir))
			{
				yield return PluginFile;
			}

			if(ProjectFile != null)
			{
				DirectoryReference ProjectPluginsDir = DirectoryReference.Combine(ProjectFile.Directory, "Plugins");
				foreach(FileReference PluginFile in EnumeratePlugins(ProjectPluginsDir))
				{
					yield return PluginFile;
				}

				DirectoryReference ProjectModsDir = DirectoryReference.Combine(ProjectFile.Directory, "Mods");
				foreach(FileReference PluginFile in EnumeratePlugins(ProjectModsDir))
				{
					yield return PluginFile;
				}
			}
		}

		/// <summary>
		/// Read all the plugin descriptors under the given engine directory
		/// </summary>
		/// <param name="EngineDirectory">The parent directory to look in.</param>
		/// <returns>Sequence of the found PluginInfo object.</returns>
		public static IReadOnlyList<PluginInfo> ReadEnginePlugins(DirectoryReference EngineDirectory)
		{
			DirectoryReference PluginsDir = DirectoryReference.Combine(EngineDirectory, "Plugins");
			return ReadPluginsFromDirectory(PluginsDir, PluginType.Engine);
		}

		/// <summary>
		/// Read all the plugin descriptors under the given enterprise directory
		/// </summary>
		/// <param name="EnterpriseDirectory">The parent directory to look in.</param>
		/// <returns>Sequence of the found PluginInfo object.</returns>
		public static IReadOnlyList<PluginInfo> ReadEnterprisePlugins(DirectoryReference EnterpriseDirectory)
		{
			DirectoryReference PluginsDir = DirectoryReference.Combine(EnterpriseDirectory, "Plugins");
			return ReadPluginsFromDirectory(PluginsDir, PluginType.Enterprise);
		}

		/// <summary>
		/// Read all the plugin descriptors under the given project directory
		/// </summary>
		/// <param name="ProjectDirectory">The parent directory to look in.</param>
		/// <returns>Sequence of the found PluginInfo object.</returns>
		public static IReadOnlyList<PluginInfo> ReadProjectPlugins(DirectoryReference ProjectDirectory)
		{
			List<PluginInfo> Plugins = new List<PluginInfo>();
			Plugins.AddRange(ReadPluginsFromDirectory(DirectoryReference.Combine(ProjectDirectory, "Plugins"), PluginType.Project));
			Plugins.AddRange(ReadPluginsFromDirectory(DirectoryReference.Combine(ProjectDirectory, "Mods"), PluginType.Mod));
			return Plugins.AsReadOnly();
		}

        /// <summary>
        /// Read all of the plugins found in the project specified additional plugin directories
        /// </summary>
        /// <param name="AdditionalDirectory">The list of additional directories to scan</param>
        /// <returns>List of the found PluginInfo objects</returns>
        public static IReadOnlyList<PluginInfo> ReadAdditionalPlugins(DirectoryReference AdditionalDirectory)
        {
			return ReadPluginsFromDirectory(AdditionalDirectory, PluginType.External);
        }

        /// <summary>
        /// Read all the plugin descriptors under the given directory
        /// </summary>
        /// <param name="ParentDirectory">The parent directory to look in.</param>
        /// <param name="Type">The plugin type</param>
        /// <returns>Sequence of the found PluginInfo object.</returns>
        public static IReadOnlyList<PluginInfo> ReadPluginsFromDirectory(DirectoryReference ParentDirectory, PluginType Type)
		{
			List<PluginInfo> Plugins;
			if (!PluginInfoCache.TryGetValue(ParentDirectory, out Plugins))
			{
				Plugins = new List<PluginInfo>();
				foreach (FileReference PluginFileName in EnumeratePlugins(ParentDirectory))
				{
					PluginInfo Plugin = new PluginInfo(PluginFileName, Type);
					Plugins.Add(Plugin);
				}
				PluginInfoCache.Add(ParentDirectory, Plugins);
			}
			return Plugins;
		}

		/// <summary>
		/// Find paths to all the plugins under a given parent directory (recursively)
		/// </summary>
		/// <param name="ParentDirectory">Parent directory to look in. Plugins will be found in any *subfolders* of this directory.</param>
		public static IEnumerable<FileReference> EnumeratePlugins(DirectoryReference ParentDirectory)
		{
			List<FileReference> FileNames;
			if (!PluginFileCache.TryGetValue(ParentDirectory, out FileNames))
			{
				FileNames = new List<FileReference>();
				if (DirectoryReference.Exists(ParentDirectory))
				{
					EnumeratePluginsInternal(ParentDirectory, FileNames);
				}
				PluginFileCache.Add(ParentDirectory, FileNames);
			}
			return FileNames;
		}

		/// <summary>
		/// Find paths to all the plugins under a given parent directory (recursively)
		/// </summary>
		/// <param name="ParentDirectory">Parent directory to look in. Plugins will be found in any *subfolders* of this directory.</param>
		/// <param name="FileNames">List of filenames. Will have all the discovered .uplugin files appended to it.</param>
		static void EnumeratePluginsInternal(DirectoryReference ParentDirectory, List<FileReference> FileNames)
		{
			foreach (DirectoryReference ChildDirectory in DirectoryReference.EnumerateDirectories(ParentDirectory))
			{
				int InitialFileNamesCount = FileNames.Count;
				foreach (FileReference PluginFile in DirectoryReference.EnumerateFiles(ChildDirectory, "*.uplugin"))
				{
					FileNames.Add(PluginFile);
				}
				if (FileNames.Count == InitialFileNamesCount)
				{
					EnumeratePluginsInternal(ChildDirectory, FileNames);
				}
			}
		}


		/// <summary>
		/// Determine if a plugin is enabled for a given project
		/// </summary>
		/// <param name="Project">The project to check</param>
		/// <param name="Plugin">Information about the plugin</param>
		/// <param name="Platform">The target platform</param>
		/// <param name="Target"></param>
		/// <returns>True if the plugin should be enabled for this project</returns>
		public static bool IsPluginEnabledForProject(PluginInfo Plugin, ProjectDescriptor Project, UnrealTargetPlatform Platform, TargetType Target)
		{
			bool bEnabled = Plugin.EnabledByDefault;
			if (Project != null && Project.Plugins != null)
			{
				foreach (PluginReferenceDescriptor PluginReference in Project.Plugins)
				{
					if (String.Compare(PluginReference.Name, Plugin.Name, true) == 0)
					{
						bEnabled = PluginReference.IsEnabledForPlatform(Platform) && PluginReference.IsEnabledForTarget(Target);
					}
				}
			}
			return bEnabled;
		}
	}
}
