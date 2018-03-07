// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	static class SourceFileSearch
	{
		// Certain file types should never be added to project files. These extensions must all be lowercase.
		static readonly string[] DefaultExcludedFileSuffixes = new string[] 
		{
			".vcxproj",				// Visual Studio project files
			".vcxproj.filters",		// Visual Studio filter file
			".vcxproj.user",		// Visual Studio project user file
			".vcxproj.vspscc",		// Visual Studio source control state
			".sln",					// Visual Studio solution file
			".sdf",					// Visual Studio embedded symbol database file
			".suo",					// Visual Studio solution option files
			".sln.docstates.suo",	// Visual Studio temp file
			".tmp",					// Temp files used by P4 diffing (e.g. t6884t87698.tmp)
			".csproj",				// Visual Studio C# project files
			".userprefs",			// MonoDevelop project settings
			".ds_store",			// Mac Desktop Services Store hidden files
			Path.DirectorySeparatorChar + "do_not_delete.txt",		// Perforce placeholder file
		};

		// Default directory names to exclude. Must be lowercase.
		static readonly string[] DefaultExcludedDirectorySuffixes = new string[] 
		{
			Path.DirectorySeparatorChar + "intermediate"
		};

		/// Finds mouse source files
		public static List<FileReference> FindModuleSourceFiles(FileReference ModuleRulesFile, bool SearchSubdirectories = true)
		{
			// The module's "base directory" is simply the directory where its xxx.Build.cs file is stored.  We'll always
			// harvest source files for this module in this base directory directory and all of its sub-directories.
			return SourceFileSearch.FindFiles(ModuleRulesFile.Directory, SubdirectoryNamesToExclude: null, SearchSubdirectories: SearchSubdirectories);
		}

		/// <summary>
		/// Fills in a project file with source files discovered in the specified list of paths
		/// </summary>
		/// <param name="DirectoryToSearch">Directory to search</param>
		/// <param name="SubdirectoryNamesToExclude">Directory base names to ignore when searching subdirectories.  Can be null.</param>
		/// <param name="SearchSubdirectories">True to include subdirectories, otherwise we only search the list of base directories</param>
		public static List<FileReference> FindFiles(DirectoryReference DirectoryToSearch, List<string> SubdirectoryNamesToExclude = null, bool SearchSubdirectories = true)
		{
			// Build a list of directory names that we don't want to search under. We always ignore intermediate directories.
			string[] ExcludedDirectorySuffixes;
			if (SubdirectoryNamesToExclude == null)
			{
				ExcludedDirectorySuffixes = DefaultExcludedDirectorySuffixes;
			}
			else
			{
				ExcludedDirectorySuffixes = SubdirectoryNamesToExclude.Select(x => Path.DirectorySeparatorChar + x.ToLowerInvariant()).Union(DefaultExcludedDirectorySuffixes).ToArray();
			}

			// Find all the files
			List<FileReference> FoundFiles = new List<FileReference>();
			if (SearchSubdirectories)
			{
				FindFilesInternalRecursive(DirectoryToSearch, ExcludedDirectorySuffixes, FoundFiles);
			}
			else
			{
				FindFilesInternal(DirectoryToSearch, ExcludedDirectorySuffixes, FoundFiles);
			}
			return FoundFiles;
		}

		static void FindFilesInternal(DirectoryReference Directory, string[] ExcludedDirectorySuffixes, List<FileReference> FoundFiles)
		{
			foreach (FileReference File in DirectoryReference.EnumerateFiles(Directory))
			{
				if (ShouldInclude(File, DefaultExcludedFileSuffixes))
				{
					FoundFiles.Add(File);
				}
			}
		}

		static void FindFilesInternalRecursive(DirectoryReference Directory, string[] ExcludedDirectorySuffixes, List<FileReference> FoundFiles)
		{
			FindFilesInternal(Directory, ExcludedDirectorySuffixes, FoundFiles);

			foreach (DirectoryReference SubDirectory in DirectoryReference.EnumerateDirectories(Directory))
			{
				if (ShouldInclude(SubDirectory, ExcludedDirectorySuffixes))
				{
					FindFilesInternalRecursive(SubDirectory, ExcludedDirectorySuffixes, FoundFiles);
				}
			}
		}

		static bool ShouldInclude(FileSystemReference Reference, string[] InvariantExcludedSuffixes)
		{
			// Ignore Mac resource fork files on non-HFS partitions
			if (Path.GetFileName(Reference.CanonicalName).StartsWith("._"))
			{
				return false;
			}
			
			foreach (string InvariantExcludedSuffix in InvariantExcludedSuffixes)
			{
				if (Reference.CanonicalName.EndsWith(InvariantExcludedSuffix))
				{
					return false;
				}
			}
			return true;
		}
	}
}
