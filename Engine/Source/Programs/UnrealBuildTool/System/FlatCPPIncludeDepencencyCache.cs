// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.Serialization.Formatters.Binary;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Stores an ordered list of header files
	/// </summary>
	class FlatCPPIncludeDependencyInfo
	{
		/// The PCH header this file is dependent on.
		public FileReference PCHName;

		/// List of files this file includes, excluding headers that were included from a PCH.
		public List<FileReference> Includes;

		/// Transient cached list of FileItems for all of the includes for a specific files.  This just saves a bunch of string
		/// hash lookups as we locate FileItems for files that we've already requested dependencies for
		public List<FileItem> IncludeFileItems;
	}


	/// <summary>
	/// For a given target, caches all of the C++ source files and the files they are including
	/// </summary>
	class FlatCPPIncludeDependencyCache
	{
		/// <summary>
		/// The version number for binary serialization
		/// </summary>
		const int FileVersion = 2;

		/// <summary>
		/// The file signature for binary serialization
		/// </summary>
		const int FileSignature = ('F' << 24) | ('C' << 16) | FileVersion;

		/// File name of this cache, should be unique for every includes context (e.g. target)
		private FileReference BackingFile;

		/// True if the cache needs to be saved
		private bool bIsDirty = false;

		/// Dependency lists, keyed (case-insensitively) on file's absolute path.
		private Dictionary<FileReference, FlatCPPIncludeDependencyInfo> DependencyMap;

		/// <summary>
		/// Constructs a fresh cache, storing the file name that it should be saved as later on
		/// </summary>
		/// <param name="BackingFile">File name for this cache, usually unique per context (e.g. target)</param>
		public FlatCPPIncludeDependencyCache(FileReference BackingFile)
		{
			this.BackingFile = BackingFile;
			DependencyMap = new Dictionary<FileReference, FlatCPPIncludeDependencyInfo>();
		}

		/// <summary>
		/// Loads the cache from disk
		/// </summary>
		/// <param name="BackingFile">The file to read from</param>
		/// <param name="Cache">The loaded cache</param>
		/// <returns>True if successful</returns>
		public static bool TryRead(FileReference BackingFile, out FlatCPPIncludeDependencyCache Cache)
		{
			if(!FileReference.Exists(BackingFile))
			{
				Cache = null;
				return false;
			}

			if (UnrealBuildTool.bPrintPerformanceInfo)
			{
				Log.TraceInformation("Loading existing FlatCPPIncludeDependencyCache: " + BackingFile.FullName);
			}

			DateTime TimerStartTime = DateTime.UtcNow;

			try
			{
				// If the .buildmutex file for the cache is present, it means that something went wrong between loading
				// and saving the cache last time (most likely the UBT process being terminated), so we don't want to load
				// it.
				string CacheBuildMutexPath = BackingFile.FullName + ".buildmutex";
				if (File.Exists(CacheBuildMutexPath))
				{
					Cache = null;
					return false;
				}
				File.Create(CacheBuildMutexPath).Close();

				using (BinaryReader Reader = new BinaryReader(new FileStream(BackingFile.FullName, FileMode.Open, FileAccess.Read)))
				{
					// @todo ubtmake: We can store the cache in a cheaper/smaller way using hash file names and indices into included headers, but it might actually slow down load times
					// @todo ubtmake: If we can index PCHs here, we can avoid storing all of the PCH's included headers (PCH's action should have been invalidated, so we shouldn't even have to report the PCH's includes as our indirect includes)
					if (Reader.ReadInt32() != FileSignature)
					{
						Cache = null;
						return false;
					}

					Cache = Deserialize(Reader);

					if (UnrealBuildTool.bPrintPerformanceInfo)
					{
						TimeSpan TimerDuration = DateTime.UtcNow - TimerStartTime;
						Log.TraceInformation("Loading FlatCPPIncludeDependencyCache took " + TimerDuration.TotalSeconds + "s");
					}

					return true;
				}
			}
			catch (Exception Ex)
			{
				Console.Error.WriteLine("Failed to read FlatCPPIncludeDependencyCache: {0}", Ex.Message);
				FileReference.Delete(BackingFile);
				Cache = null;
				return false;
			}
		}

		/// <summary>
		/// Saves out the cache
		/// </summary>
		public void Save()
		{
			// Only save if we've made changes to it since load.
			if (bIsDirty)
			{
				DateTime TimerStartTime = DateTime.UtcNow;

				// Serialize the cache to disk.
				try
				{
					DirectoryReference.CreateDirectory(BackingFile.Directory);
					using (BinaryWriter Writer = new BinaryWriter(new FileStream(BackingFile.FullName, FileMode.Create, FileAccess.Write)))
					{
						Writer.Write(FileSignature);
						Serialize(Writer);
					}
				}
				catch (Exception Ex)
				{
					Console.Error.WriteLine("Failed to write FlatCPPIncludeDependencyCache: {0}", Ex.Message);
				}

				if (UnrealBuildTool.bPrintPerformanceInfo)
				{
					TimeSpan TimerDuration = DateTime.UtcNow - TimerStartTime;
					Log.TraceInformation("Saving FlatCPPIncludeDependencyCache took " + TimerDuration.TotalSeconds + "s");
				}
			}
			else
			{
				if (UnrealBuildTool.bPrintPerformanceInfo)
				{
					Log.TraceInformation("FlatCPPIncludeDependencyCache did not need to be saved (bIsDirty=false)");
				}
			}

			if (File.Exists(BackingFile.FullName + ".buildmutex"))
			{
				try
				{
					File.Delete(BackingFile.FullName + ".buildmutex");
				}
				catch
				{
					// We don't care if we couldn't delete this file, as maybe it couldn't have been created in the first place.
				}
			}
		}

		/// <summary>
		/// Serializes the dependency cache to a binary writer
		/// </summary>
		/// <param name="Writer">Writer to output to</param>
		void Serialize(BinaryWriter Writer)
		{
			Dictionary<FileReference, int> FileToUniqueId = new Dictionary<FileReference, int>();
			Writer.Write(BackingFile);

			// Write out all the dependency info objects
			Writer.Write(DependencyMap.Count);
			foreach (KeyValuePair<FileReference, FlatCPPIncludeDependencyInfo> Pair in DependencyMap)
			{
				Writer.Write(Pair.Key);

				FlatCPPIncludeDependencyInfo Info = Pair.Value;
				Writer.Write(Info.PCHName, FileToUniqueId);

				Writer.Write(Info.Includes.Count);
				foreach (FileReference Include in Info.Includes)
				{
					Writer.Write(Include, FileToUniqueId);
				}
			}
		}

		/// <summary>
		/// Deserialize the dependency cache from a binary reader
		/// </summary>
		/// <param name="Reader">Reader for the cache data</param>
		/// <returns>New dependency cache object</returns>
		static FlatCPPIncludeDependencyCache Deserialize(BinaryReader Reader)
		{
			FlatCPPIncludeDependencyCache Cache = new FlatCPPIncludeDependencyCache(Reader.ReadFileReference());

			// Create the dependency map
			int NumDependencyMapEntries = Reader.ReadInt32();
			Cache.DependencyMap = new Dictionary<FileReference, FlatCPPIncludeDependencyInfo>(NumDependencyMapEntries);

			// Read all the dependency map entries
			List<FileReference> UniqueFiles = new List<FileReference>(NumDependencyMapEntries * 2);
			for (int Idx = 0; Idx < NumDependencyMapEntries; Idx++)
			{
				FileReference File = Reader.ReadFileReference();

				FlatCPPIncludeDependencyInfo Info = new FlatCPPIncludeDependencyInfo();

				// Read the PCH name
				Info.PCHName = Reader.ReadFileReference(UniqueFiles);

				// Read the includes 
				int NumIncludes = Reader.ReadInt32();
				Info.Includes = new List<FileReference>(NumIncludes);

				for (int IncludeIdx = 0; IncludeIdx < NumIncludes; IncludeIdx++)
				{
					Info.Includes.Add(Reader.ReadFileReference(UniqueFiles));
				}

				// Add it to the map
				Cache.DependencyMap.Add(File, Info);
			}

			return Cache;
		}

		/// <summary>
		/// Sets the new dependencies for the specified file
		/// </summary>
		/// <param name="AbsoluteFilePath">File to set</param>
		/// <param name="PCHName">The PCH for this file</param>
		/// <param name="Dependencies">List of source dependencies</param>
		public void SetDependenciesForFile(FileReference AbsoluteFilePath, FileReference PCHName, List<FileReference> Dependencies)
		{
			FlatCPPIncludeDependencyInfo DependencyInfo = new FlatCPPIncludeDependencyInfo();
			DependencyInfo.PCHName = PCHName;	// @todo ubtmake: Not actually used yet.  The idea is to use this to reduce the number of indirect includes we need to store in the cache.
			DependencyInfo.Includes = Dependencies;
			DependencyInfo.IncludeFileItems = null;

			// @todo ubtmake: We could shrink this file by not storing absolute paths (project and engine relative paths only, except for system headers.)  May affect load times.
			DependencyMap[AbsoluteFilePath] = DependencyInfo;
			bIsDirty = true;
		}


		/// <summary>
		/// Gets everything that this file includes from our cache (direct and indirect!)
		/// </summary>
		/// <param name="AbsoluteFilePath">Path to the file</param>
		/// <returns>The list of includes</returns>
		public List<FileItem> GetDependenciesForFile(FileReference AbsoluteFilePath)
		{
			FlatCPPIncludeDependencyInfo DependencyInfo;
			if (DependencyMap.TryGetValue(AbsoluteFilePath, out DependencyInfo))
			{
				// Update our transient cache of FileItems for each of the included files
				if (DependencyInfo.IncludeFileItems == null)
				{
					DependencyInfo.IncludeFileItems = new List<FileItem>(DependencyInfo.Includes.Count);
					foreach (FileReference Dependency in DependencyInfo.Includes)
					{
						DependencyInfo.IncludeFileItems.Add(FileItem.GetItemByFileReference(Dependency));
					}
				}
				return DependencyInfo.IncludeFileItems;
			}

			return null;
		}

		/// <summary>
		/// Gets the dependency cache path and filename for the specified target.
		/// </summary>
		/// <param name="Target">Current build target</param>
		/// <returns>Cache Path</returns>
		public static FileReference GetDependencyCachePathForTarget(UEBuildTarget Target)
		{
			DirectoryReference PlatformIntermediatePath;
			if (Target.ProjectFile != null)
			{
				PlatformIntermediatePath = DirectoryReference.Combine(Target.ProjectFile.Directory, Target.PlatformIntermediateFolder);
			}
			else
			{
				PlatformIntermediatePath = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, Target.PlatformIntermediateFolder);
			}
			return FileReference.Combine(PlatformIntermediatePath, Target.GetTargetName(), "FlatCPPIncludes.bin");
		}
	}
}
