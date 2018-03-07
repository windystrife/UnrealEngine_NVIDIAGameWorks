// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	abstract class UEBuildPlatform
	{
		private static Dictionary<UnrealTargetPlatform, UEBuildPlatform> BuildPlatformDictionary = new Dictionary<UnrealTargetPlatform, UEBuildPlatform>();

		// a mapping of a group to the platforms in the group (ie, Microsoft contains Win32 and Win64)
		static Dictionary<UnrealPlatformGroup, List<UnrealTargetPlatform>> PlatformGroupDictionary = new Dictionary<UnrealPlatformGroup, List<UnrealTargetPlatform>>();

		/// <summary>
		/// The corresponding target platform enum
		/// </summary>
		public readonly UnrealTargetPlatform Platform;

		/// <summary>
		/// The default C++ target platform to use
		/// </summary>
		public readonly CppPlatform DefaultCppPlatform;

		/// <summary>
		/// Cached copy of the list of folders to exclude for this platform
		/// </summary>
		private FileSystemName[] CachedExcludedFolderNames;

		/// <summary>
		/// List of all confidential folder names
		/// </summary>
		public static readonly FileSystemName[] RestrictedFolderNames =
		{
			new FileSystemName("EpicInternal"),
			new FileSystemName("CarefullyRedist"),
			new FileSystemName("NotForLicensees"),
			new FileSystemName("NoRedist"),
			new FileSystemName("PS4"),
			new FileSystemName("XboxOne"),
			new FileSystemName("Switch")
		};

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InPlatform">The enum value for this platform</param>
		/// <param name="InDefaultCPPPlatform">The default C++ platform for this platform</param>
		public UEBuildPlatform(UnrealTargetPlatform InPlatform, CppPlatform InDefaultCPPPlatform)
		{
			Platform = InPlatform;
			DefaultCppPlatform = InDefaultCPPPlatform;
		}

		/// <summary>
		/// Finds a list of folder names to exclude when building for this platform
		/// </summary>
		public FileSystemName[] GetExcludedFolderNames()
		{
			if(CachedExcludedFolderNames == null)
			{
				// Find all the platform folders to exclude from the list of precompiled modules
				List<FileSystemName> Names = new List<FileSystemName>();
				foreach (UnrealTargetPlatform TargetPlatform in Enum.GetValues(typeof(UnrealTargetPlatform)))
				{
					if (TargetPlatform != UnrealTargetPlatform.Unknown && TargetPlatform != Platform)
					{
						Names.Add(new FileSystemName(TargetPlatform.ToString()));
					}
				}

				// Also exclude all the platform groups that this platform is not a part of
				List<UnrealPlatformGroup> IncludePlatformGroups = new List<UnrealPlatformGroup>(UEBuildPlatform.GetPlatformGroups(Platform));
				foreach (UnrealPlatformGroup PlatformGroup in Enum.GetValues(typeof(UnrealPlatformGroup)))
				{
					if (!IncludePlatformGroups.Contains(PlatformGroup))
					{
						Names.Add(new FileSystemName(PlatformGroup.ToString()));
					}
				}

				// Save off the list as an array
				CachedExcludedFolderNames = Names.ToArray();
			}
			return CachedExcludedFolderNames;
		}

		/// <summary>
		/// Whether the required external SDKs are installed for this platform. Could be either a manual install or an AutoSDK.
		/// </summary>
		public abstract SDKStatus HasRequiredSDKsInstalled();

		/// <summary>
		/// Gets all the registered platforms
		/// </summary>
		/// <returns>Sequence of registered platforms</returns>
		public static IEnumerable<UnrealTargetPlatform> GetRegisteredPlatforms()
		{
			return BuildPlatformDictionary.Keys;
		}

		/// <summary>
		/// Get the default architecture for a project. This may be overriden on the command line to UBT.
		/// </summary>
		/// <param name="ProjectFile">Optional project to read settings from </param>
		public virtual string GetDefaultArchitecture(FileReference ProjectFile)
		{
			// by default, use an empty architecture (which is really just a modifer to the platform for some paths/names)
			return "";
		}

		/// <summary>
		/// Get name for architecture-specific directories (can be shorter than architecture name itself)
		/// </summary>
		public virtual string GetFolderNameForArchitecture(string Architecture)
		{
			// by default, use the architecture name
			return Architecture;
		}

		/// <summary>
		/// Searches a directory tree for build products to be cleaned.
		/// </summary>
		/// <param name="BaseDir">The directory to search</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UE4Editor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <param name="FilesToClean">List to receive a list of files to be cleaned</param>
		/// <param name="DirectoriesToClean">List to receive a list of directories to be cleaned</param>
		public void FindBuildProductsToClean(DirectoryReference BaseDir, string[] NamePrefixes, string[] NameSuffixes, List<FileReference> FilesToClean, List<DirectoryReference> DirectoriesToClean)
		{
			foreach (FileReference File in DirectoryReference.EnumerateFiles(BaseDir))
			{
				string FileName = File.GetFileName();
				if (IsDefaultBuildProduct(FileName, NamePrefixes, NameSuffixes) || IsBuildProduct(FileName, NamePrefixes, NameSuffixes))
				{
					FilesToClean.Add(File);
				}
			}
			foreach (DirectoryReference SubDir in DirectoryReference.EnumerateDirectories(BaseDir))
			{
				string SubDirName = SubDir.GetDirectoryName();
				if (IsBuildProduct(SubDirName, NamePrefixes, NameSuffixes))
				{
					DirectoriesToClean.Add(SubDir);
				}
				else
				{
					FindBuildProductsToClean(SubDir, NamePrefixes, NameSuffixes, FilesToClean, DirectoriesToClean);
				}
			}
		}

		/// <summary>
		/// Determines if a filename is a default UBT build product
		/// </summary>
		/// <param name="FileName">The name to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UE4Editor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <returns>True if the substring matches the name of a build product, false otherwise</returns>
		public static bool IsDefaultBuildProduct(string FileName, string[] NamePrefixes, string[] NameSuffixes)
		{
			return UEBuildPlatform.IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".target")
				|| UEBuildPlatform.IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".modules")
				|| UEBuildPlatform.IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".version");
		}

		/// <summary>
		/// Determines if the given name is a build product for a target.
		/// </summary>
		/// <param name="FileName">The name to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UE4Editor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <returns>True if the string matches the name of a build product, false otherwise</returns>
		public abstract bool IsBuildProduct(string FileName, string[] NamePrefixes, string[] NameSuffixes);

		/// <summary>
		/// Determines if a string is in the canonical name of a UE build product, with a specific extension (eg. "UE4Editor-Win64-Debug.exe" or "UE4Editor-ModuleName-Win64-Debug.dll"). 
		/// </summary>
		/// <param name="FileName">The file name to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UE4Editor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <param name="Extension">The extension to check for</param>
		/// <returns>True if the string matches the name of a build product, false otherwise</returns>
		public static bool IsBuildProductName(string FileName, string[] NamePrefixes, string[] NameSuffixes, string Extension)
		{
			return IsBuildProductName(FileName, 0, FileName.Length, NamePrefixes, NameSuffixes, Extension);
		}

		/// <summary>
		/// Determines if a substring is in the canonical name of a UE build product, with a specific extension (eg. "UE4Editor-Win64-Debug.exe" or "UE4Editor-ModuleName-Win64-Debug.dll"). 
		/// </summary>
		/// <param name="FileName">The name to check</param>
		/// <param name="Index">Index of the first character to be checked</param>
		/// <param name="Count">Number of characters of the substring to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UE4Editor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <param name="Extension">The extension to check for</param>
		/// <returns>True if the substring matches the name of a build product, false otherwise</returns>
		public static bool IsBuildProductName(string FileName, int Index, int Count, string[] NamePrefixes, string[] NameSuffixes, string Extension)
		{
			// Check if the extension matches, and forward on to the next IsBuildProductName() overload without it if it does.
			if (Count > Extension.Length && String.Compare(FileName, Index + Count - Extension.Length, Extension, 0, Extension.Length, StringComparison.InvariantCultureIgnoreCase) == 0)
			{
				return IsBuildProductName(FileName, Index, Count - Extension.Length, NamePrefixes, NameSuffixes);
			}
			return false;
		}

		/// <summary>
		/// Determines if a substring is in the canonical name of a UE build product, excluding extension or other decoration (eg. "UE4Editor-Win64-Debug" or "UE4Editor-ModuleName-Win64-Debug"). 
		/// </summary>
		/// <param name="FileName">The name to check</param>
		/// <param name="Index">Index of the first character to be checked</param>
		/// <param name="Count">Number of characters of the substring to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UE4Editor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <returns>True if the substring matches the name of a build product, false otherwise</returns>
		public static bool IsBuildProductName(string FileName, int Index, int Count, string[] NamePrefixes, string[] NameSuffixes)
		{
			foreach (string NamePrefix in NamePrefixes)
			{
				if (Count >= NamePrefix.Length && String.Compare(FileName, Index, NamePrefix, 0, NamePrefix.Length, StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					int MinIdx = Index + NamePrefix.Length;
					foreach (string NameSuffix in NameSuffixes)
					{
						int MaxIdx = Index + Count - NameSuffix.Length;
						if (MaxIdx >= MinIdx && String.Compare(FileName, MaxIdx, NameSuffix, 0, NameSuffix.Length, StringComparison.InvariantCultureIgnoreCase) == 0)
						{
							if (MinIdx < MaxIdx && FileName[MinIdx] == '-')
							{
								MinIdx++;
								while (MinIdx < MaxIdx && FileName[MinIdx] != '-' && FileName[MinIdx] != '.')
								{
									MinIdx++;
								}
							}
							if (MinIdx == MaxIdx)
							{
								return true;
							}
						}
					}
				}
			}
			return false;
		}

		public virtual void PreBuildSync()
		{
		}

		public virtual void PostBuildSync(UEBuildTarget Target)
		{
		}

		/// <summary>
		/// Called immediately after UnrealHeaderTool is executed to generated code for all UObjects modules.  Only is called if UnrealHeaderTool was actually run in this session.
		/// </summary>
		/// <param name="Manifest">List of UObject modules we generated code for.</param>
		public virtual void PostCodeGeneration(UHTManifest Manifest)
		{
		}

		/// <summary>
		/// Converts the passed in path from UBT host to compiler native format.
		/// </summary>
		/// <param name="OriginalPath">The path to convert</param>
		/// <returns>The path in native format for the toolchain</returns>
		public virtual string ConvertPath(string OriginalPath)
		{
			return OriginalPath;
		}

		/// <summary>
		/// Get the bundle directory for the shared link environment
		/// </summary>
		/// <param name="Rules">The target rules</param>
		/// <param name="OutputFiles">List of executable output files</param>
		/// <returns>Path to the bundle directory</returns>
		public virtual DirectoryReference GetBundleDirectory(ReadOnlyTargetRules Rules, List<FileReference> OutputFiles)
		{
			return null;
		}

		/// <summary>
		/// Attempt to convert a string to an UnrealTargetPlatform enum entry
		/// </summary>
		/// <returns>UnrealTargetPlatform.Unknown on failure (the platform didn't match the enum)</returns>
		public static UnrealTargetPlatform ConvertStringToPlatform(string InPlatformName)
		{
			// special case x64, to not break anything
			// @todo: Is it possible to remove this hack?
			if (InPlatformName.Equals("X64", StringComparison.InvariantCultureIgnoreCase))
			{
				return UnrealTargetPlatform.Win64;
			}

			// we can't parse the string into an enum because Enum.Parse is case sensitive, so we loop over the enum
			// looking for matches
			foreach (string PlatformName in Enum.GetNames(typeof(UnrealTargetPlatform)))
			{
				if (InPlatformName.Equals(PlatformName, StringComparison.InvariantCultureIgnoreCase))
				{
					// convert the known good enum string back to the enum value
					return (UnrealTargetPlatform)Enum.Parse(typeof(UnrealTargetPlatform), PlatformName);
				}
			}
			return UnrealTargetPlatform.Unknown;
		}

		/// <summary>
		/// Determines whether a given platform is available
		/// </summary>
		/// <param name="Platform">The platform to check for</param>
		/// <returns>True if it's available, false otherwise</returns>
		public static bool IsPlatformAvailable(UnrealTargetPlatform Platform)
		{
			return BuildPlatformDictionary.ContainsKey(Platform);
		}

		/// <summary>
		/// Register the given platforms UEBuildPlatform instance
		/// </summary>
		/// <param name="InBuildPlatform"> The UEBuildPlatform instance to use for the InPlatform</param>
		public static void RegisterBuildPlatform(UEBuildPlatform InBuildPlatform)
		{
			if (BuildPlatformDictionary.ContainsKey(InBuildPlatform.Platform) == true)
			{
				Log.TraceWarning("RegisterBuildPlatform Warning: Registering build platform {0} for {1} when it is already set to {2}",
					InBuildPlatform.ToString(), InBuildPlatform.Platform.ToString(), BuildPlatformDictionary[InBuildPlatform.Platform].ToString());
				BuildPlatformDictionary[InBuildPlatform.Platform] = InBuildPlatform;
			}
			else
			{
				BuildPlatformDictionary.Add(InBuildPlatform.Platform, InBuildPlatform);
			}
		}

		/// <summary>
		/// Assign a platform as a member of the given group
		/// </summary>
		public static void RegisterPlatformWithGroup(UnrealTargetPlatform InPlatform, UnrealPlatformGroup InGroup)
		{
			// find or add the list of groups for this platform
			List<UnrealTargetPlatform> Platforms;
			if(!PlatformGroupDictionary.TryGetValue(InGroup, out Platforms))
			{
				Platforms = new List<UnrealTargetPlatform>();
				PlatformGroupDictionary.Add(InGroup, Platforms);
			}
			Platforms.Add(InPlatform);
		}

		/// <summary>
		/// Retrieve the list of platforms in this group (if any)
		/// </summary>
		public static List<UnrealTargetPlatform> GetPlatformsInGroup(UnrealPlatformGroup InGroup)
		{
			List<UnrealTargetPlatform> PlatformList;
			PlatformGroupDictionary.TryGetValue(InGroup, out PlatformList);
			return PlatformList;
		}

		/// <summary>
		/// Enumerates all the platform groups for a given platform
		/// </summary>
		/// <param name="Platform">The platform to look for</param>
		/// <returns>List of platform groups that this platform is a member of</returns>
		public static IEnumerable<UnrealPlatformGroup> GetPlatformGroups(UnrealTargetPlatform Platform)
		{
			return PlatformGroupDictionary.Where(x => x.Value.Contains(Platform)).Select(x => x.Key);
		}

		/// <summary>
		/// Retrieve the IUEBuildPlatform instance for the given TargetPlatform
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="bInAllowFailure"> If true, do not throw an exception and return null</param>
		/// <returns>UEBuildPlatform  The instance of the build platform</returns>
		public static UEBuildPlatform GetBuildPlatform(UnrealTargetPlatform InPlatform, bool bInAllowFailure = false)
		{
			if (BuildPlatformDictionary.ContainsKey(InPlatform) == true)
			{
				return BuildPlatformDictionary[InPlatform];
			}
			if (bInAllowFailure == true)
			{
				return null;
			}
			throw new BuildException("GetBuildPlatform: No BuildPlatform found for {0}", InPlatform.ToString());
		}

		/// <summary>
		/// Gets the UnrealTargetPlatform matching a given CPPTargetPlatform
		/// </summary>
		/// <param name="InCPPPlatform">The compile platform</param>
		/// <returns>The target platform</returns>
		public static UnrealTargetPlatform CPPTargetPlatformToUnrealTargetPlatform(CppPlatform InCPPPlatform)
		{
			switch (InCPPPlatform)
			{
				case CppPlatform.Win32:			return UnrealTargetPlatform.Win32;
				case CppPlatform.Win64:			return UnrealTargetPlatform.Win64;
				case CppPlatform.Mac:				return UnrealTargetPlatform.Mac;
				case CppPlatform.XboxOne:			return UnrealTargetPlatform.XboxOne;
				case CppPlatform.PS4:				return UnrealTargetPlatform.PS4;
				case CppPlatform.Android:			return UnrealTargetPlatform.Android;
				case CppPlatform.IOS:				return UnrealTargetPlatform.IOS;
				case CppPlatform.HTML5:			return UnrealTargetPlatform.HTML5;
				case CppPlatform.Linux:			return UnrealTargetPlatform.Linux;
				case CppPlatform.TVOS:			return UnrealTargetPlatform.TVOS;
				case CppPlatform.Switch: 			return UnrealTargetPlatform.Switch;
			}
			throw new BuildException("CPPTargetPlatformToUnrealTargetPlatform: Unknown CPPTargetPlatform {0}", InCPPPlatform.ToString());
		}

		/// <summary>
		/// Retrieve the IUEBuildPlatform instance for the given CPPTargetPlatform
		/// </summary>
		/// <param name="InPlatform">  The CPPTargetPlatform being built</param>
		/// <param name="bInAllowFailure"> If true, do not throw an exception and return null</param>
		/// <returns>UEBuildPlatform  The instance of the build platform</returns>
		public static UEBuildPlatform GetBuildPlatformForCPPTargetPlatform(CppPlatform InPlatform, bool bInAllowFailure = false)
		{
			UnrealTargetPlatform UTPlatform = CPPTargetPlatformToUnrealTargetPlatform(InPlatform);
			if (BuildPlatformDictionary.ContainsKey(UTPlatform) == true)
			{
				return BuildPlatformDictionary[UTPlatform];
			}
			if (bInAllowFailure == true)
			{
				return null;
			}
			throw new BuildException("UEBuildPlatform::GetBuildPlatformForCPPTargetPlatform: No BuildPlatform found for {0}", InPlatform.ToString());
		}

		/// <summary>
		/// Allow all registered build platforms to modify the newly created module
		/// passed in for the given platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public static void PlatformModifyHostModuleRules(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			foreach (KeyValuePair<UnrealTargetPlatform, UEBuildPlatform> PlatformEntry in BuildPlatformDictionary)
			{
				PlatformEntry.Value.ModifyModuleRulesForOtherPlatform(ModuleName, Rules, Target);
			}
		}

		/// <summary>
		/// Returns the delimiter used to separate paths in the PATH environment variable for the platform we are executing on.
		/// </summary>
		public static String GetPathVarDelimiter()
		{
			switch (BuildHostPlatform.Current.Platform)
			{
				case UnrealTargetPlatform.Linux:
				case UnrealTargetPlatform.Mac:
					return ":";
				case UnrealTargetPlatform.Win32:
				case UnrealTargetPlatform.Win64:
					return ";";
				default:
					Log.TraceWarning("PATH var delimiter unknown for platform " + BuildHostPlatform.Current.Platform.ToString() + " using ';'");
					return ";";
			}
		}

		/// <summary>
		/// Returns the name that should be returned in the output when doing -validateplatforms
		/// </summary>
		public virtual string GetPlatformValidationName()
		{
			return Platform.ToString();
		}

		/// <summary>
		/// If this platform can be compiled with XGE
		/// </summary>
		public virtual bool CanUseXGE()
		{
			return true;
		}

		/// <summary>
		/// If this platform can be compiled with DMUCS/Distcc
		/// </summary>
		public virtual bool CanUseDistcc()
		{
			return false;
		}

		/// <summary>
		/// If this platform can be compiled with SN-DBS
		/// </summary>
		public virtual bool CanUseSNDBS()
		{
			return false;
		}

		/// <summary>
		/// Set all the platform-specific defaults for a new target
		/// </summary>
		public virtual void ResetTarget(TargetRules Target)
		{
		}

		/// <summary>
		/// Validate a target's settings
		/// </summary>
		public virtual void ValidateTarget(TargetRules Target)
		{
		}

		/// <summary>
		/// Return whether the given platform requires a monolithic build
		/// </summary>
		/// <param name="InPlatform">The platform of interest</param>
		/// <param name="InConfiguration">The configuration of interest</param>
		/// <returns></returns>
		public static bool PlatformRequiresMonolithicBuilds(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			// Some platforms require monolithic builds...
			UEBuildPlatform BuildPlatform = GetBuildPlatform(InPlatform, true);
			if (BuildPlatform != null)
			{
				return BuildPlatform.ShouldCompileMonolithicBinary(InPlatform);
			}

			// We assume it does not
			return false;
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The binary extension (i.e. 'exe' or 'dll')</returns>
		public virtual string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			throw new BuildException("GetBinaryExtensiton for {0} not handled in {1}", InBinaryType.ToString(), this.ToString());
		}

		/// <summary>
		/// Get the extension to use for debug info for the given binary type
		/// </summary>
		/// <param name="InTarget">Options for the target being built</param>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The debug info extension (i.e. 'pdb')</returns>
		public virtual string GetDebugInfoExtension(ReadOnlyTargetRules InTarget, UEBuildBinaryType InBinaryType)
		{
			throw new BuildException("GetDebugInfoExtension for {0} not handled in {1}", InBinaryType.ToString(), this.ToString());
		}

		/// <summary>
		/// Whether the editor should be built for this platform or not
		/// </summary>
		/// <param name="InPlatform"> The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration">The UnrealTargetConfiguration being built</param>
		/// <returns>bool   true if the editor should be built, false if not</returns>
		public virtual bool ShouldNotBuildEditor(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return false;
		}

		/// <summary>
		/// Whether this build should support ONLY cooked data or not
		/// </summary>
		/// <param name="InPlatform"> The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration">The UnrealTargetConfiguration being built</param>
		/// <returns>bool   true if the editor should be built, false if not</returns>
		public virtual bool BuildRequiresCookedData(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return false;
		}

		/// <summary>
		/// Whether this platform requires the use of absolute paths in Unity files. The compiler will try to combine paths in
		/// each #include directive with the standard include paths, and unity files in intermediate directories can result in the
		/// maximum path length being exceeded on Windows. On the other hand, remote compilation requires relative paths so
		/// dependency checking works correctly on the local machine as well as on the remote machine.
		/// </summary>
		/// <returns>bool true if it is required, false if not</returns>
		public virtual bool UseAbsolutePathsInUnityFiles()
		{
			return true;
		}

		/// <summary>
		/// Whether this platform should build a monolithic binary
		/// </summary>
		public virtual bool ShouldCompileMonolithicBinary(UnrealTargetPlatform InPlatform)
		{
			return false;
		}

		/// <summary>
		/// Modify the rules for a newly created module, where the target is a different host platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public virtual void ModifyModuleRulesForOtherPlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
		}

		/// <summary>
		/// Allows the platform to override whether the architecture name should be appended to the name of binaries.
		/// </summary>
		/// <returns>True if the architecture name should be appended to the binary</returns>
		public virtual bool RequiresArchitectureSuffix()
		{
			return true;
		}

		/// <summary>
		/// For platforms that need to output multiple files per binary (ie Android "fat" binaries)
		/// this will emit multiple paths. By default, it simply makes an array from the input
		/// </summary>
		public virtual List<FileReference> FinalizeBinaryPaths(FileReference BinaryName, FileReference ProjectFile, ReadOnlyTargetRules Target)
		{
			List<FileReference> TempList = new List<FileReference>() { BinaryName };
			return TempList;
		}

		/// <summary>
		/// Return whether this platform has uniquely named binaries across multiple games
		/// </summary>
		public virtual bool HasUniqueBinaries()
		{
			return true;
		}

		/// <summary>
		/// Return whether we wish to have this platform's binaries in our builds
		/// </summary>
		public virtual bool IsBuildRequired()
		{
			return true;
		}

		/// <summary>
		/// Return whether we wish to have this platform's binaries in our CIS tests
		/// </summary>
		public virtual bool IsCISRequired()
		{
			return true;
		}

		/// <summary>
		/// Whether the build platform requires deployment prep
		/// </summary>
		/// <returns></returns>
		public virtual bool RequiresDeployPrepAfterCompile()
		{
			return false;
		}

		/// <summary>
		/// Return all valid configurations for this platform
		/// Typically, this is always Debug, Development, and Shipping - but Test is a likely future addition for some platforms
		/// </summary>
		public virtual List<UnrealTargetConfiguration> GetConfigurations(UnrealTargetPlatform InUnrealTargetPlatform, bool bIncludeDebug)
		{
			List<UnrealTargetConfiguration> Configurations = new List<UnrealTargetConfiguration>()
			{
				UnrealTargetConfiguration.Development,
			};

			if (bIncludeDebug)
			{
				Configurations.Insert(0, UnrealTargetConfiguration.Debug);
			}

			return Configurations;
		}

		protected static bool DoProjectSettingsMatchDefault(UnrealTargetPlatform Platform, DirectoryReference ProjectDirectoryName, string Section, string[] BoolKeys, string[] IntKeys, string[] StringKeys)
		{
			ConfigHierarchy ProjIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, ProjectDirectoryName, Platform);
			ConfigHierarchy DefaultIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, (DirectoryReference)null, Platform);

			// look at all bool values
			if (BoolKeys != null) foreach (string Key in BoolKeys)
				{
					bool Default = false, Project = false;
					DefaultIni.GetBool(Section, Key, out Default);
					ProjIni.GetBool(Section, Key, out Project);
					if (Default != Project)
					{
						Log.TraceInformationOnce(Key + " is not set to default. (" + Default + " vs. " + Project + ")");
						return false;
					}
				}

			// look at all int values
			if (IntKeys != null) foreach (string Key in IntKeys)
				{
					int Default = 0, Project = 0;
					DefaultIni.GetInt32(Section, Key, out Default);
					ProjIni.GetInt32(Section, Key, out Project);
					if (Default != Project)
					{
						Log.TraceInformationOnce(Key + " is not set to default. (" + Default + " vs. " + Project + ")");
						return false;
					}
				}

			// look for all string values
			if (StringKeys != null) foreach (string Key in StringKeys)
				{
					string Default = "", Project = "";
					DefaultIni.GetString(Section, Key, out Default);
					ProjIni.GetString(Section, Key, out Project);
					if (Default != Project)
					{
						Log.TraceInformationOnce(Key + " is not set to default. (" + Default + " vs. " + Project + ")");
						return false;
					}
				}

			// if we get here, we match all important settings
			return true;
		}

		/// <summary>
		/// Check for the default configuration
		/// return true if the project uses the default build config
		/// </summary>
		public virtual bool HasDefaultBuildConfig(UnrealTargetPlatform Platform, DirectoryReference ProjectDirectoryName)
		{
			string[] BoolKeys = new string[] {
				"bCompileApex", "bCompileICU", "bCompileSimplygon", "bCompileSimplygonSSF",
				"bCompileLeanAndMeanUE", "bIncludeADO", "bCompileRecast", "bCompileSpeedTree",
				"bCompileWithPluginSupport", "bCompilePhysXVehicle", "bCompileFreeType",
				"bCompileForSize", "bCompileCEF3"
			};

			return DoProjectSettingsMatchDefault(Platform, ProjectDirectoryName, "/Script/BuildSettings.BuildSettings",
				BoolKeys, null, null);
		}

		/// <summary>
		/// Get a list of extra modules the platform requires.
		/// This is to allow undisclosed platforms to add modules they need without exposing information about the platform.
		/// </summary>
		/// <param name="Target">The target being build</param>
		/// <param name="ExtraModuleNames">List of extra modules the platform needs to add to the target</param>
		public virtual void AddExtraModules(ReadOnlyTargetRules Target, List<string> ExtraModuleNames)
		{
		}

		/// <summary>
		/// Modify the rules for a newly created module, in a target that's being built for this platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public virtual void ModifyModuleRulesForActivePlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="Target">Settings for the target being compiled</param>
		/// <param name="CompileEnvironment">The compile environment for this target</param>
		/// <param name="LinkEnvironment">The link environment for this target</param>
		public abstract void SetUpEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment);

		/// <summary>
		/// Setup the configuration environment for building
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <param name="GlobalCompileEnvironment">The global compile environment</param>
		/// <param name="GlobalLinkEnvironment">The global link environment</param>
		public virtual void SetUpConfigurationEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment GlobalCompileEnvironment, LinkEnvironment GlobalLinkEnvironment)
		{
			if (GlobalCompileEnvironment.bUseDebugCRT)
			{
				GlobalCompileEnvironment.Definitions.Add("_DEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("NDEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
			}

			switch (Target.Configuration)
			{
				default:
				case UnrealTargetConfiguration.Debug:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_DEBUG=1");
					break;
				case UnrealTargetConfiguration.DebugGame:
				// Individual game modules can be switched to be compiled in debug as necessary. By default, everything is compiled in development.
				case UnrealTargetConfiguration.Development:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_DEVELOPMENT=1");
					break;
				case UnrealTargetConfiguration.Shipping:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_SHIPPING=1");
					break;
				case UnrealTargetConfiguration.Test:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_TEST=1");
					break;
			}

			// Create debug info based on the heuristics specified by the user.
			GlobalCompileEnvironment.bCreateDebugInfo =
				!Target.bDisableDebugInfo && ShouldCreateDebugInfo(Target);
			GlobalLinkEnvironment.bCreateDebugInfo = GlobalCompileEnvironment.bCreateDebugInfo;
		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <returns>bool    true if debug info should be generated, false if not</returns>
		public abstract bool ShouldCreateDebugInfo(ReadOnlyTargetRules Target);

		/// <summary>
		/// Creates a toolchain instance for the given platform. There should be a single toolchain instance per-target, as their may be
		/// state data and configuration cached between calls.
		/// </summary>
		/// <param name="CppPlatform">The platform to create a toolchain for</param>
		/// <param name="Target">The target being built</param>
		/// <returns>New toolchain instance.</returns>
		public abstract UEToolChain CreateToolChain(CppPlatform CppPlatform, ReadOnlyTargetRules Target);

		/// <summary>
		/// Deploys the given target
		/// </summary>
		/// <param name="Target">Information about the target being deployed</param>
		public abstract void Deploy(UEBuildDeployTarget Target);
	}
}
