// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;
using System.Runtime.Serialization;
using System.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// For C++ source file items, this structure is used to cache data that will be used for include dependency scanning
	/// </summary>
	[Serializable]
	class CppIncludePaths : ISerializable
	{
		/// <summary>
		/// Ordered list of include paths for the module
		/// </summary>
		public HashSet<string> UserIncludePaths;

		/// <summary>
		/// The include paths where changes to contained files won't cause dependent C++ source files to
		/// be recompiled, unless BuildConfiguration.bCheckSystemHeadersForModification==true.
		/// </summary>
		public HashSet<string> SystemIncludePaths;

		/// <summary>
		/// Whether headers in system paths should be checked for modification when determining outdated actions.
		/// </summary>
		public bool bCheckSystemHeadersForModification;

		/// <summary>
		/// Contains a mapping from filename to the full path of the header in this environment.  This is used to optimized include path lookups at runtime for any given single module.
		/// </summary>
		public Dictionary<string, FileItem> IncludeFileSearchDictionary = new Dictionary<string, FileItem>();

		/// <summary>
		/// Construct an empty set of include paths
		/// </summary>
		public CppIncludePaths()
		{
			UserIncludePaths = new HashSet<string>();
			SystemIncludePaths = new HashSet<string>();
		}

		/// <summary>
		/// Copy constructor
		/// </summary>
		/// <param name="Other">Duplicate another instance's settings</param>
		public CppIncludePaths(CppIncludePaths Other)
		{
			UserIncludePaths = new HashSet<string>(Other.UserIncludePaths);
			SystemIncludePaths = new HashSet<string>(Other.SystemIncludePaths);
			bCheckSystemHeadersForModification = Other.bCheckSystemHeadersForModification;
		}

		/// <summary>
		/// Deserialize the include paths from the given context
		/// </summary>
		/// <param name="Info">Serialization info</param>
		/// <param name="Context">Serialization context</param>
		public CppIncludePaths(SerializationInfo Info, StreamingContext Context)
		{
			UserIncludePaths = new HashSet<string>((string[])Info.GetValue("ip", typeof(string[])));
			SystemIncludePaths = new HashSet<string>((string[])Info.GetValue("sp", typeof(string[])));
			bCheckSystemHeadersForModification = Info.GetBoolean("cs");
		}

		/// <summary>
		/// Serialize this instance
		/// </summary>
		/// <param name="Info">Serialization info</param>
		/// <param name="Context">Serialization context</param>
		public void GetObjectData(SerializationInfo Info, StreamingContext Context)
		{
			Info.AddValue("ip", UserIncludePaths.ToArray());
			Info.AddValue("sp", SystemIncludePaths.ToArray());
			Info.AddValue("cs", bCheckSystemHeadersForModification);
		}

		/// <summary>
		/// Given a C++ source file, returns a list of include paths we should search to resolve #includes for this path
		/// </summary>
		/// <param name="SourceFile">C++ source file we're going to check #includes for.</param>
		/// <returns>Ordered list of paths to search</returns>
		public List<string> GetPathsToSearch(FileReference SourceFile)
		{
			List<string> IncludePathsToSearch = new List<string>();
			IncludePathsToSearch.Add(SourceFile.Directory.FullName);
			IncludePathsToSearch.AddRange(UserIncludePaths);
			if (bCheckSystemHeadersForModification)
			{
				IncludePathsToSearch.AddRange(SystemIncludePaths);
			}
			return IncludePathsToSearch;
		}
	}

	/// <summary>
	/// List of all files included in a file and helper class for handling circular dependencies.
	/// </summary>
	class IncludedFilesSet : HashSet<FileItem>
	{
		/// <summary>
		/// Whether this file list has been fully initialized or not.
		/// </summary>
		public bool bIsInitialized;

		/// <summary>
		/// List of files which include this file in one of its includes.
		/// </summary>
		public List<FileItem> CircularDependencies = new List<FileItem>();
	}

	class CPPHeaders
	{
		/// <summary>
		/// The project that we're caching headers for
		/// </summary>
		public FileReference ProjectFile;

		/// <summary>
		/// Path to the dependency cache for this target
		/// </summary>
		public FileReference DependencyCacheFile;

		/// <summary>
		/// Contains a cache of include dependencies (direct and indirect), one for each target we're building.
		/// </summary>
		public DependencyCache IncludeDependencyCache = null;

		/// <summary>
		/// Contains a cache of include dependencies (direct and indirect), one for each target we're building.
		/// </summary>
		public FlatCPPIncludeDependencyCache FlatCPPIncludeDependencyCache = null;

		/// <summary>
		/// 
		/// </summary>
		public static int TotalFindIncludedFileCalls = 0;

		/// <summary>
		/// 
		/// </summary>
		public static int IncludePathSearchAttempts = 0;

		/// <summary>
		/// A cache of the list of other files that are directly or indirectly included by a C++ file.
		/// </summary>
		Dictionary<FileItem, IncludedFilesSet> ExhaustiveIncludedFilesMap = new Dictionary<FileItem, IncludedFilesSet>();

		/// <summary>
		/// A cache of all files included by a C++ file, but only has files that we knew about from a previous session, loaded from a cache at startup
		/// </summary>
		Dictionary<FileItem, IncludedFilesSet> OnlyCachedIncludedFilesMap = new Dictionary<FileItem, IncludedFilesSet>();

		/// <summary>
		/// 
		/// </summary>
		public bool bUseUBTMakefiles;

		/// <summary>
		/// 
		/// </summary>
		public bool bUseFlatCPPIncludeDependencyCache;
		
		/// <summary>
		/// 
		/// </summary>
		public bool bUseIncludeDependencyResolveCache;

		/// <summary>
		/// 
		/// </summary>
		public bool bTestIncludeDependencyResolveCache;

		public CPPHeaders(FileReference ProjectFile, FileReference DependencyCacheFile, bool bUseUBTMakefiles, bool bUseFlatCPPIncludeDependencyCache, bool bUseIncludeDependencyResolveCache, bool bTestIncludeDependencyResolveCache)
		{
			this.ProjectFile = ProjectFile;
			this.DependencyCacheFile = DependencyCacheFile;
			this.bUseUBTMakefiles = bUseUBTMakefiles;
			this.bUseFlatCPPIncludeDependencyCache = bUseFlatCPPIncludeDependencyCache;
			this.bUseIncludeDependencyResolveCache = bUseIncludeDependencyResolveCache;
			this.bTestIncludeDependencyResolveCache = bTestIncludeDependencyResolveCache;
		}

		/// <summary>
		/// Finds the header file that is referred to by a partial include filename.
		/// </summary>
		/// <param name="FromFile">The file containing the include directory</param>
		/// <param name="RelativeIncludePath">path relative to the project</param>
		/// <param name="IncludePaths">Include paths to search</param>
		public static FileItem FindIncludedFile(FileReference FromFile, string RelativeIncludePath, CppIncludePaths IncludePaths)
		{
			FileItem Result = null;

			++TotalFindIncludedFileCalls;

			// Only search for the include file if the result hasn't been cached.
			string InvariantPath = RelativeIncludePath.ToLowerInvariant();
			if (!IncludePaths.IncludeFileSearchDictionary.TryGetValue(InvariantPath, out Result))
			{
				int SearchAttempts = 0;
				if (Path.IsPathRooted(RelativeIncludePath))
				{
					FileReference Reference = FileReference.Combine(UnrealBuildTool.EngineSourceDirectory, RelativeIncludePath);
					if (DirectoryLookupCache.FileExists(Reference))
					{
						Result = FileItem.GetItemByFileReference(Reference);
					}
					++SearchAttempts;
				}
				else
				{
					// Find the first include path that the included file exists in.
					List<string> IncludePathsToSearch = IncludePaths.GetPathsToSearch(FromFile);
					foreach (string IncludePath in IncludePathsToSearch)
					{
						++SearchAttempts;
						string RelativeFilePath = "";
						try
						{
							RelativeFilePath = Path.Combine(IncludePath, RelativeIncludePath);
						}
						catch (ArgumentException Exception)
						{
							throw new BuildException(Exception, "Failed to combine null or invalid include paths.");
						}
						FileReference FullFilePath = null;
						try
						{
							FullFilePath = FileReference.Combine(UnrealBuildTool.EngineSourceDirectory, RelativeFilePath);
						}
						catch (Exception)
						{
						}
						if (FullFilePath != null && DirectoryLookupCache.FileExists(FullFilePath))
						{
							Result = FileItem.GetItemByFileReference(FullFilePath);
							break;
						}
					}
				}

				IncludePathSearchAttempts += SearchAttempts;

				if (UnrealBuildTool.bPrintPerformanceInfo)
				{
					// More than two search attempts indicates:
					//		- Include path was not relative to the directory that the including file was in
					//		- Include path was not relative to the project's base
					if (SearchAttempts > 2)
					{
						Log.TraceVerbose("   Cache miss: " + RelativeIncludePath + " found after " + SearchAttempts.ToString() + " attempts: " + (Result != null ? Result.AbsolutePath : "NOT FOUND!"));
					}
				}

				// Cache the result of the include path search.
				IncludePaths.IncludeFileSearchDictionary.Add(InvariantPath, Result);
			}

			// @todo ubtmake: The old UBT tried to skip 'external' (STABLE) headers here.  But it didn't work.  We might want to do this though!  Skip system headers and source/thirdparty headers!

			if (Result != null)
			{
				Log.TraceVerbose("Resolved included file \"{0}\" to: {1}", RelativeIncludePath, Result.AbsolutePath);
			}
			else
			{
				Log.TraceVerbose("Couldn't resolve included file \"{0}\"", RelativeIncludePath);
			}

			return Result;
		}

		public List<FileItem> FindAndCacheAllIncludedFiles(FileItem SourceFile, CppIncludePaths IncludePaths, bool bOnlyCachedDependencies)
		{
			List<FileItem> Result = null;

			if (IncludePaths.IncludeFileSearchDictionary == null)
			{
				IncludePaths.IncludeFileSearchDictionary = new Dictionary<string, FileItem>();
			}

			if (bOnlyCachedDependencies && bUseFlatCPPIncludeDependencyCache)
			{
				Result = FlatCPPIncludeDependencyCache.GetDependenciesForFile(SourceFile.Reference);
				if (Result == null)
				{
					// Nothing cached for this file!  It is new to us.  This is the expected flow when our CPPIncludeDepencencyCache is missing.
				}
			}
			else
			{
				// @todo ubtmake: HeaderParser.h is missing from the include set for Module.UnrealHeaderTool.cpp (failed to find include using:  FileItem DirectIncludeResolvedFile = CPPEnvironment.FindIncludedFile(DirectInclude.IncludeName, !BuildConfiguration.bCheckExternalHeadersForModification, IncludePathsToSearch, IncludeFileSearchDictionary );)

				// If we're doing an exhaustive include scan, make sure that we have our include dependency cache loaded and ready
				if (!bOnlyCachedDependencies)
				{
					if (IncludeDependencyCache == null)
					{
						IncludeDependencyCache = DependencyCache.Create(DependencyCacheFile);
					}
				}

				Result = new List<FileItem>();

				IncludedFilesSet IncludedFileList = new IncludedFilesSet();
				FindAndCacheAllIncludedFiles(SourceFile, IncludePaths, ref IncludedFileList, bOnlyCachedDependencies: bOnlyCachedDependencies);
				foreach (FileItem IncludedFile in IncludedFileList)
				{
					Result.Add(IncludedFile);
				}

				// Update cache
				if (bUseFlatCPPIncludeDependencyCache && !bOnlyCachedDependencies)
				{
					List<FileReference> Dependencies = new List<FileReference>();
					foreach (FileItem IncludedFile in Result)
					{
						Dependencies.Add(IncludedFile.Reference);
					}
					FileReference PCHName = SourceFile.PrecompiledHeaderIncludeFilename;
					FlatCPPIncludeDependencyCache.SetDependenciesForFile(SourceFile.Reference, PCHName, Dependencies);
				}
			}

			return Result;
		}


		/// <summary>
		/// Finds the files directly or indirectly included by the given C++ file.
		/// </summary>
		/// <param name="CPPFile">C++ file to get the dependencies for.</param>
		/// <param name="IncludePaths"></param>
		/// <param name="bOnlyCachedDependencies"></param>
		/// <param name="Result">List of CPPFile dependencies.</param>
		/// <returns>false if CPPFile is still being processed further down the callstack, true otherwise.</returns>
		public bool FindAndCacheAllIncludedFiles(FileItem CPPFile, CppIncludePaths IncludePaths, ref IncludedFilesSet Result, bool bOnlyCachedDependencies)
		{
			IncludedFilesSet IncludedFileList;
			Dictionary<FileItem, IncludedFilesSet> IncludedFilesMap = bOnlyCachedDependencies ? OnlyCachedIncludedFilesMap : ExhaustiveIncludedFilesMap;
			if (!IncludedFilesMap.TryGetValue(CPPFile, out IncludedFileList))
			{
				DateTime TimerStartTime = DateTime.UtcNow;

				IncludedFileList = new IncludedFilesSet();

				// Add an uninitialized entry for the include file to avoid infinitely recursing on include file loops.
				IncludedFilesMap.Add(CPPFile, IncludedFileList);

				// Gather a list of names of files directly included by this C++ file.
				List<DependencyInclude> DirectIncludes = GetDirectIncludeDependencies(CPPFile, bOnlyCachedDependencies: bOnlyCachedDependencies);

				// Build a list of the unique set of files that are included by this file.
				HashSet<FileItem> DirectlyIncludedFiles = new HashSet<FileItem>();
				// require a for loop here because we need to keep track of the index in the list.
				for (int DirectlyIncludedFileNameIndex = 0; DirectlyIncludedFileNameIndex < DirectIncludes.Count; ++DirectlyIncludedFileNameIndex)
				{
					// Resolve the included file name to an actual file.
					DependencyInclude DirectInclude = DirectIncludes[DirectlyIncludedFileNameIndex];
					if (!DirectInclude.HasAttemptedResolve ||
						// ignore any preexisting resolve cache if we are not configured to use it.
						!bUseIncludeDependencyResolveCache ||
						// if we are testing the resolve cache, we force UBT to resolve every time to look for conflicts
						bTestIncludeDependencyResolveCache
						)
					{
						++TotalDirectIncludeResolveCacheMisses;

						// search the include paths to resolve the file
						FileItem DirectIncludeResolvedFile = CPPHeaders.FindIncludedFile(CPPFile.Reference, DirectInclude.IncludeName, IncludePaths);
						if (DirectIncludeResolvedFile != null)
						{
							DirectlyIncludedFiles.Add(DirectIncludeResolvedFile);
						}
						IncludeDependencyCache.CacheResolvedIncludeFullPath(CPPFile, DirectlyIncludedFileNameIndex, DirectIncludeResolvedFile != null ? DirectIncludeResolvedFile.Reference : null, bUseIncludeDependencyResolveCache, bTestIncludeDependencyResolveCache);
					}
					else
					{
						// we might have cached an attempt to resolve the file, but couldn't actually find the file (system headers, etc).
						if (DirectInclude.IncludeResolvedNameIfSuccessful != null)
						{
							DirectlyIncludedFiles.Add(FileItem.GetItemByFileReference(DirectInclude.IncludeResolvedNameIfSuccessful));
						}
					}
				}
				TotalDirectIncludeResolves += DirectIncludes.Count;

				// Convert the dictionary of files included by this file into a list.
				foreach (FileItem DirectlyIncludedFile in DirectlyIncludedFiles)
				{
					// Add the file we're directly including
					IncludedFileList.Add(DirectlyIncludedFile);

					// Also add all of the indirectly included files!
					if (FindAndCacheAllIncludedFiles(DirectlyIncludedFile, IncludePaths, ref IncludedFileList, bOnlyCachedDependencies: bOnlyCachedDependencies) == false)
					{
						// DirectlyIncludedFile is a circular dependency which is still being processed
						// further down the callstack. Add this file to its circular dependencies list 
						// so that it can update its dependencies later.
						IncludedFilesSet DirectlyIncludedFileIncludedFileList;
						if (IncludedFilesMap.TryGetValue(DirectlyIncludedFile, out DirectlyIncludedFileIncludedFileList))
						{
							DirectlyIncludedFileIncludedFileList.CircularDependencies.Add(CPPFile);
						}
					}
				}

				// All dependencies have been processed by now so update all circular dependencies
				// with the full list.
				foreach (FileItem CircularDependency in IncludedFileList.CircularDependencies)
				{
					IncludedFilesSet CircularDependencyIncludedFiles = IncludedFilesMap[CircularDependency];
					foreach (FileItem IncludedFile in IncludedFileList)
					{
						CircularDependencyIncludedFiles.Add(IncludedFile);
					}
				}
				// No need to keep this around anymore.
				IncludedFileList.CircularDependencies.Clear();

				// Done collecting files.
				IncludedFileList.bIsInitialized = true;

				TimeSpan TimerDuration = DateTime.UtcNow - TimerStartTime;
				TotalTimeSpentGettingIncludes += TimerDuration.TotalSeconds;
			}

			if (IncludedFileList.bIsInitialized)
			{
				// Copy the list of files included by this file into the result list.
				foreach (FileItem IncludedFile in IncludedFileList)
				{
					// If the result list doesn't contain this file yet, add the file and the files it includes.
					// NOTE: For some reason in .NET 4, Add() is over twice as fast as calling UnionWith() on the set
					Result.Add(IncludedFile);
				}

				return true;
			}
			else
			{
				// The IncludedFileList.bIsInitialized was false because we added a dummy entry further down the call stack.  We're already processing
				// the include list for this header elsewhere in the stack frame, so we don't need to add anything here.
				return false;
			}
		}

		public FileItem CachePCHUsageForCPPFile(FileItem CPPFile, CppIncludePaths IncludePaths, CppPlatform Platform)
		{
			// @todo ubtmake: We don't really need to scan every file looking for PCH headers, just need one.  The rest is just for error checking.
			// @todo ubtmake: We don't need all of the direct includes either.  We just need the first, unless we want to check for errors.
			List<DependencyInclude> DirectIncludeFilenames = GetDirectIncludeDependencies(CPPFile, bOnlyCachedDependencies: false);
			if (UnrealBuildTool.bPrintDebugInfo)
			{
				Log.TraceVerbose("Found direct includes for {0}: {1}", Path.GetFileName(CPPFile.AbsolutePath), string.Join(", ", DirectIncludeFilenames.Select(F => F.IncludeName)));
			}

			if (DirectIncludeFilenames.Count == 0)
			{
				return null;
			}

			DependencyInclude FirstInclude = DirectIncludeFilenames[0];

			// Resolve the PCH header to an absolute path.
			// Check NullOrEmpty here because if the file could not be resolved we need to throw an exception
			if (FirstInclude.IncludeResolvedNameIfSuccessful != null &&
				// ignore any preexisting resolve cache if we are not configured to use it.
				bUseIncludeDependencyResolveCache &&
				// if we are testing the resolve cache, we force UBT to resolve every time to look for conflicts
				!bTestIncludeDependencyResolveCache)
			{
				CPPFile.PrecompiledHeaderIncludeFilename = FirstInclude.IncludeResolvedNameIfSuccessful;
				return FileItem.GetItemByFileReference(CPPFile.PrecompiledHeaderIncludeFilename);
			}

			// search the include paths to resolve the file.
			string FirstIncludeName = FirstInclude.IncludeName;
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(Platform);
			// convert back from relative to host path if needed
			if (!BuildPlatform.UseAbsolutePathsInUnityFiles())
			{
				FirstIncludeName = RemoteExports.UnconvertPath(FirstIncludeName);
			}

			FileItem PrecompiledHeaderIncludeFile = CPPHeaders.FindIncludedFile(CPPFile.Reference, FirstIncludeName, IncludePaths);
			if (PrecompiledHeaderIncludeFile == null)
			{
				FirstIncludeName = RemoteExports.UnconvertPath(FirstInclude.IncludeName);
				throw new BuildException("The first include statement in source file '{0}' is trying to include the file '{1}' as the precompiled header, but that file could not be located in any of the module's include search paths.", CPPFile.AbsolutePath, FirstIncludeName);
			}

			IncludeDependencyCache.CacheResolvedIncludeFullPath(CPPFile, 0, PrecompiledHeaderIncludeFile.Reference, bUseIncludeDependencyResolveCache, bTestIncludeDependencyResolveCache);
			CPPFile.PrecompiledHeaderIncludeFilename = PrecompiledHeaderIncludeFile.Reference;

			return PrecompiledHeaderIncludeFile;
		}

		public static double TotalTimeSpentGettingIncludes = 0.0;
		public static int TotalIncludesRequested = 0;
		public static double DirectIncludeCacheMissesTotalTime = 0.0;
		public static int TotalDirectIncludeCacheMisses = 0;
		public static int TotalDirectIncludeResolveCacheMisses = 0;
		public static int TotalDirectIncludeResolves = 0;


		/// <summary>
		/// Regex that matches #include statements.
		/// </summary>
		static readonly Regex CPPHeaderRegex = new Regex("(([ \t]*#[ \t]*include[ \t]*[<\"](?<HeaderFile>[^\">]*)[\">][^\n]*\n*)|([^\n]*\n*))*",
													RegexOptions.Compiled | RegexOptions.Singleline | RegexOptions.ExplicitCapture);

		static readonly Regex MMHeaderRegex = new Regex("(([ \t]*#[ \t]*import[ \t]*[<\"](?<HeaderFile>[^\">]*)[\">][^\n]*\n*)|([^\n]*\n*))*",
													RegexOptions.Compiled | RegexOptions.Singleline | RegexOptions.ExplicitCapture);

		/// <summary>
		/// Regex that matches C++ code with UObject declarations which we will need to generated code for.
		/// </summary>
		static readonly Regex UObjectRegex = new Regex("^\\s*U(CLASS|STRUCT|ENUM|INTERFACE|DELEGATE)\\b", RegexOptions.Compiled | RegexOptions.Multiline);

		// Maintains a cache of file contents
		private static Dictionary<string, string> FileContentsCache = new Dictionary<string, string>();

		private static string GetFileContents(string Filename)
		{
			string Contents;
			if (FileContentsCache.TryGetValue(Filename, out Contents))
			{
				return Contents;
			}

			using (var Reader = new StreamReader(Filename, System.Text.Encoding.UTF8))
			{
				Contents = Reader.ReadToEnd();
				FileContentsCache.Add(Filename, Contents);
			}

			return Contents;
		}

		// Checks if a file contains UObjects
		public static bool DoesFileContainUObjects(string Filename)
		{
			string Contents = GetFileContents(Filename);
			return UObjectRegex.IsMatch(Contents);
		}

		/// <summary>
		/// Finds the names of files directly included by the given C++ file, and also whether the file contains any UObjects
		/// </summary>
		public List<DependencyInclude> GetDirectIncludeDependencies(FileItem CPPFile, bool bOnlyCachedDependencies)
		{
			// Try to fulfill request from cache first.
			List<DependencyInclude> Info = IncludeDependencyCache.GetCachedDependencyInfo(CPPFile);
			if (Info != null)
			{
				return Info;
			}

			List<DependencyInclude> Result = new List<DependencyInclude>();
			if (bOnlyCachedDependencies)
			{
				return Result;
			}

			DateTime TimerStartTime = DateTime.UtcNow;
			++CPPHeaders.TotalDirectIncludeCacheMisses;

			Result = GetUncachedDirectIncludeDependencies(CPPFile, ProjectFile);

			// Populate cache with results.
			IncludeDependencyCache.SetDependencyInfo(CPPFile, Result);

			CPPHeaders.DirectIncludeCacheMissesTotalTime += (DateTime.UtcNow - TimerStartTime).TotalSeconds;

			return Result;
		}

		public static List<DependencyInclude> GetUncachedDirectIncludeDependencies(FileItem CPPFile, FileReference ProjectFile)
		{
			List<DependencyInclude> Result = new List<DependencyInclude>();

			// Get the adjusted filename
			string FileToRead = CPPFile.AbsolutePath;

			// Read lines from the C++ file.
			string FileContents = GetFileContents(FileToRead);
			if (string.IsNullOrEmpty(FileContents))
			{
				return Result;
			}

			// Note: This depends on UBT executing w/ a working directory of the Engine/Source folder!
			string EngineSourceFolder = Directory.GetCurrentDirectory();
			string InstalledFolder = EngineSourceFolder;
			Int32 EngineSourceIdx = EngineSourceFolder.IndexOf("\\Engine\\Source");
			if (EngineSourceIdx != -1)
			{
				InstalledFolder = EngineSourceFolder.Substring(0, EngineSourceIdx);
			}

			if (Utils.IsRunningOnMono)
			{
				// Mono crashes when running a regex on a string longer than about 5000 characters, so we parse the file in chunks
				int StartIndex = 0;
				const int SafeTextLength = 4000;
				while (StartIndex < FileContents.Length)
				{
					int EndIndex = StartIndex + SafeTextLength < FileContents.Length ? FileContents.IndexOf("\n", StartIndex + SafeTextLength) : FileContents.Length;
					if (EndIndex == -1)
					{
						EndIndex = FileContents.Length;
					}

					Result.AddRange(CollectHeaders(ProjectFile, CPPFile, FileToRead, FileContents, InstalledFolder, StartIndex, EndIndex));

					StartIndex = EndIndex + 1;
				}
			}
			else
			{
				Result = CollectHeaders(ProjectFile, CPPFile, FileToRead, FileContents, InstalledFolder, 0, FileContents.Length);
			}

			return Result;
		}

		/// <summary>
		/// Collects all header files included in a CPPFile
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="CPPFile"></param>
		/// <param name="FileToRead"></param>
		/// <param name="FileContents"></param>
		/// <param name="InstalledFolder"></param>
		/// <param name="StartIndex"></param>
		/// <param name="EndIndex"></param>
		private static List<DependencyInclude> CollectHeaders(FileReference ProjectFile, FileItem CPPFile, string FileToRead, string FileContents, string InstalledFolder, int StartIndex, int EndIndex)
		{
			List<DependencyInclude> Result = new List<DependencyInclude>();

			Match M = CPPHeaderRegex.Match(FileContents, StartIndex, EndIndex - StartIndex);
			CaptureCollection Captures = M.Groups["HeaderFile"].Captures;
			Result.Capacity = Result.Count;
			foreach (Capture C in Captures)
			{
				string HeaderValue = C.Value;

				if (HeaderValue.IndexOfAny(Path.GetInvalidPathChars()) != -1)
				{
					throw new BuildException("In {0}: An #include statement contains invalid characters.  You might be missing a double-quote character. (\"{1}\")", FileToRead, C.Value);
				}

				//@TODO: The intermediate exclusion is to work around autogenerated absolute paths in Module.SomeGame.cpp style files
				bool bCheckForBackwardSlashes = FileToRead.StartsWith(InstalledFolder) || ((ProjectFile != null) && new FileReference(FileToRead).IsUnderDirectory(ProjectFile.Directory));
				if (bCheckForBackwardSlashes && !FileToRead.Contains("Intermediate") && !FileToRead.Contains("ThirdParty") && HeaderValue.IndexOf('\\', 0) >= 0)
				{
					throw new BuildException("In {0}: #include \"{1}\" contains backslashes ('\\'), please use forward slashes ('/') instead.", FileToRead, C.Value);
				}
				HeaderValue = Utils.CleanDirectorySeparators(HeaderValue);
				Result.Add(new DependencyInclude(HeaderValue));
			}

			// also look for #import in objective C files
			string Ext = Path.GetExtension(CPPFile.AbsolutePath).ToUpperInvariant();
			if (Ext == ".MM" || Ext == ".M")
			{
				M = MMHeaderRegex.Match(FileContents, StartIndex, EndIndex - StartIndex);
				Captures = M.Groups["HeaderFile"].Captures;
				Result.Capacity += Captures.Count;
				foreach (Capture C in Captures)
				{
					Result.Add(new DependencyInclude(C.Value));
				}
			}

			return Result;
		}
	}
}
