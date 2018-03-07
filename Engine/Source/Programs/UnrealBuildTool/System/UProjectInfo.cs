// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Threading;
using System.Reflection;
using System.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Stores information about a project
	/// </summary>
	public static class UProjectInfo
	{
		/// <summary>
		/// Map of relative or complete project file names to the project info
		/// </summary>
		static HashSet<FileReference> ProjectFiles = new HashSet<FileReference>();

		/// <summary>
		/// Map of target names to the relative or complete project file name
		/// </summary>
		static Dictionary<string, FileReference> TargetToProjectDictionary = new Dictionary<string, FileReference>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Find all the target files under the given folder and add them to the TargetToProjectDictionary map
		/// </summary>
		/// <param name="InTargetFolder">Folder to search</param>
		/// <returns>True if any target files were found</returns>
		public static bool FindTargetFilesInFolder(DirectoryReference InTargetFolder)
		{
			bool bFoundTargetFiles = false;
			IEnumerable<string> Files;
			if (!Utils.IsRunningOnMono)
			{
				Files = Directory.EnumerateFiles(InTargetFolder.FullName, "*.target.cs", SearchOption.TopDirectoryOnly);
			}
			else
			{
				Files = Directory.GetFiles(InTargetFolder.FullName, "*.Target.cs", SearchOption.TopDirectoryOnly).AsEnumerable();
			}
			foreach (string TargetFilename in Files)
			{
				bFoundTargetFiles = true;
				foreach (FileReference ProjectFile in ProjectFiles)
				{
					FileInfo ProjectFileInfo = new FileInfo(ProjectFile.FullName);
					string ProjectDir = ProjectFileInfo.DirectoryName.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
					if (TargetFilename.StartsWith(ProjectDir, StringComparison.InvariantCultureIgnoreCase))
					{
						FileInfo TargetInfo = new FileInfo(TargetFilename);
						// Strip off the target.cs
						string TargetName = Utils.GetFilenameWithoutAnyExtensions(TargetInfo.Name);
						if (TargetToProjectDictionary.ContainsKey(TargetName) == false)
						{
							TargetToProjectDictionary.Add(TargetName, ProjectFile);
						}
					}
				}
			}
			return bFoundTargetFiles;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="CurrentTopDirectory"></param>
		/// <param name="bOutFoundTargetFiles"></param>
		/// <returns></returns>
		public static bool FindTargetFiles(DirectoryReference CurrentTopDirectory, ref bool bOutFoundTargetFiles)
		{
			// We will only search as deep as the first target file found
			List<DirectoryReference> SubFolderList = new List<DirectoryReference>();

			// Check the root directory
			bOutFoundTargetFiles |= FindTargetFilesInFolder(CurrentTopDirectory);
			if (bOutFoundTargetFiles == false)
			{
				foreach (DirectoryReference TargetFolder in Directory.EnumerateDirectories(CurrentTopDirectory.FullName, "*", SearchOption.TopDirectoryOnly).Select(x => new DirectoryReference(x)))
				{
					SubFolderList.Add(TargetFolder);
					bOutFoundTargetFiles |= FindTargetFilesInFolder(TargetFolder);
				}
			}

			if (bOutFoundTargetFiles == false)
			{
				// Recurse each folders folders
				foreach (DirectoryReference SubFolder in SubFolderList)
				{
					FindTargetFiles(SubFolder, ref bOutFoundTargetFiles);
				}
			}

			return bOutFoundTargetFiles;
		}

		static readonly string RootDirectory = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().GetOriginalLocation()), "..", "..", "..");
		static readonly string EngineSourceDirectory = Path.GetFullPath(Path.Combine(RootDirectory, "Engine", "Source"));

		/// <summary>
		/// Add a single project to the project info dictionary
		/// </summary>
		public static void AddProject(FileReference ProjectFile)
		{
			if (ProjectFiles.Add(ProjectFile))
			{
				DirectoryReference ProjectDirectory = ProjectFile.Directory;

				// Check if it's a code project
				DirectoryReference SourceFolder = DirectoryReference.Combine(ProjectDirectory, "Source");
				DirectoryReference IntermediateSourceFolder = DirectoryReference.Combine(ProjectDirectory, "Intermediate", "Source");
				bool bIsCodeProject = DirectoryReference.Exists(SourceFolder) || DirectoryReference.Exists(IntermediateSourceFolder);

				// Find all Target.cs files if it's a code project
				if (bIsCodeProject)
				{
					bool bFoundTargetFiles = false;
					if (DirectoryReference.Exists(SourceFolder) && !FindTargetFiles(SourceFolder, ref bFoundTargetFiles))
					{
						Log.TraceVerbose("No target files found under " + SourceFolder);
					}
					if (DirectoryReference.Exists(IntermediateSourceFolder) && !FindTargetFiles(IntermediateSourceFolder, ref bFoundTargetFiles))
					{
						Log.TraceVerbose("No target files found under " + IntermediateSourceFolder);
					}
				}
			}
		}

		/// <summary>
		/// Discover and fill in the project info
		/// </summary>
		public static void FillProjectInfo()
		{
			DateTime StartTime = DateTime.Now;

			List<DirectoryReference> DirectoriesToSearch = new List<DirectoryReference>();

			// Find all the .uprojectdirs files contained in the root folder and add their entries to the search array
			string EngineSourceDirectory = Path.GetFullPath(Path.Combine(RootDirectory, "Engine", "Source"));

			foreach (FileReference ProjectDirsFile in DirectoryReference.EnumerateFiles(UnrealBuildTool.RootDirectory, "*.uprojectdirs", SearchOption.TopDirectoryOnly))
			{
				Log.TraceVerbose("\tFound uprojectdirs file {0}", ProjectDirsFile.FullName);
				foreach(string Line in File.ReadAllLines(ProjectDirsFile.FullName))
				{
					string TrimLine = Line.Trim();
					if(!TrimLine.StartsWith(";"))
					{
						DirectoryReference BaseProjectDir = DirectoryReference.Combine(UnrealBuildTool.RootDirectory, TrimLine);
						if(BaseProjectDir.IsUnderDirectory(UnrealBuildTool.RootDirectory))
						{
							DirectoriesToSearch.Add(BaseProjectDir);
						}
						else
						{
							Log.TraceWarning("Project search path '{0}' is not under root directory, ignoring.", TrimLine);
						}
					}
				}
			}

			Log.TraceVerbose("\tFound {0} directories to search", DirectoriesToSearch.Count);

			foreach (DirectoryReference DirToSearch in DirectoriesToSearch)
			{
				Log.TraceVerbose("\t\tSearching {0}", DirToSearch.FullName);
				if (DirectoryReference.Exists(DirToSearch))
				{
					foreach (DirectoryReference SubDir in DirectoryReference.EnumerateDirectories(DirToSearch, "*", SearchOption.TopDirectoryOnly))
					{
						Log.TraceVerbose("\t\t\tFound subdir {0}", SubDir.FullName);
						foreach(FileReference UProjFile in DirectoryReference.EnumerateFiles(SubDir, "*.uproject", SearchOption.TopDirectoryOnly))
						{
							Log.TraceVerbose("\t\t\t\t{0}", UProjFile.FullName);
							AddProject(UProjFile);
						}
					}
				}
				else
				{
					Log.TraceVerbose("ProjectInfo: Skipping directory {0} from .uprojectdirs file as it doesn't exist.", DirToSearch);
				}
			}

			DateTime StopTime = DateTime.Now;

			if (UnrealBuildTool.bPrintPerformanceInfo)
			{
				TimeSpan TotalProjectInfoTime = StopTime - StartTime;
				Log.TraceInformation("FillProjectInfo took {0} milliseconds", TotalProjectInfoTime.TotalMilliseconds);
			}
		}

		/// <summary>
		/// Print out all the info for known projects
		/// </summary>
		public static void DumpProjectInfo()
		{
			Log.TraceInformation("Dumping project info...");
			Log.TraceInformation("\tProjectInfo");
			foreach (FileReference InfoEntry in ProjectFiles)
			{
				Log.TraceInformation("\t\t" + InfoEntry);
			}
			Log.TraceInformation("\tTarget to Project");
			foreach (KeyValuePair<string, FileReference> TargetEntry in TargetToProjectDictionary)
			{
				Log.TraceInformation("\t\tTarget     : " + TargetEntry.Key);
				Log.TraceInformation("\t\tProject    : " + TargetEntry.Value);
			}
		}

		/// <summary>
		/// Returns a list of all the projects
		/// </summary>
		/// <returns>List of projects</returns>
		public static IEnumerable<FileReference> AllProjectFiles
		{
			get { return ProjectFiles; }
		}

		/// <summary>
		/// Get the project folder for the given target name
		/// </summary>
		/// <param name="InTargetName">Name of the target of interest</param>
		/// <param name="OutProjectFileName">The project filename</param>
		/// <returns>True if the target was found</returns>
		public static bool TryGetProjectForTarget(string InTargetName, out FileReference OutProjectFileName)
		{
			return TargetToProjectDictionary.TryGetValue(InTargetName, out OutProjectFileName);
		}
	}
}
