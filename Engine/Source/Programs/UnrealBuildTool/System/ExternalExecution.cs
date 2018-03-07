// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Text.RegularExpressions;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	// This enum has to be compatible with the one defined in the
	// UE4\Engine\Source\Runtime\Core\Public\Misc\ComplilationResult.h 
	// to keep communication between UHT, UBT and Editor compiling
	// processes valid.
	enum ECompilationResult
	{
		/// <summary>
		/// Compilation succeeded
		/// </summary>
		Succeeded = 0,

		/// <summary>
		/// Build was canceled, this is used on the engine side only
		/// </summary>
		Canceled = 1,

		/// <summary>
		/// All targets were up to date, used only with -canskiplink
		/// </summary>
		UpToDate = 2,

		/// <summary>
		/// The process has most likely crashed. This is what UE returns in case of an assert
		/// </summary>
		CrashOrAssert = 3,

		/// <summary>
		/// Compilation failed because generated code changed which was not supported
		/// </summary>
		FailedDueToHeaderChange = 4,

		/// <summary>
		/// Compilation failed due to compilation errors
		/// </summary>
		OtherCompilationError = 5,

		/// <summary>
		/// Compilation is not supported in the current build
		/// </summary>
		Unsupported,

		/// <summary>
		/// Unknown error
		/// </summary>
		Unknown
	}
	static class CompilationResultExtensions
	{
		public static bool Succeeded(this ECompilationResult Result)
		{
			return Result == ECompilationResult.Succeeded || Result == ECompilationResult.UpToDate;
		}
	}

	/// <summary>
	/// Type of module. Mirrored in UHT as EBuildModuleType.
	/// This should be sorted by the order in which we expect modules to be built.
	/// </summary>
	enum UHTModuleType
	{
		Program,
		EngineRuntime,
		EngineDeveloper,
		EngineEditor,
		EngineThirdParty,
		GameRuntime,
		GameDeveloper,
		GameEditor,
		GameThirdParty,
	}
	static class UHTModuleTypeExtensions
	{
		public static bool IsProgramModule(this UHTModuleType ModuleType)
		{
			return ModuleType == UHTModuleType.Program;
		}
		public static bool IsEngineModule(this UHTModuleType ModuleType)
		{
			return ModuleType == UHTModuleType.EngineRuntime || ModuleType == UHTModuleType.EngineDeveloper || ModuleType == UHTModuleType.EngineEditor || ModuleType == UHTModuleType.EngineThirdParty;
		}
		public static bool IsGameModule(this UHTModuleType ModuleType)
		{
			return ModuleType == UHTModuleType.GameRuntime || ModuleType == UHTModuleType.GameDeveloper || ModuleType == UHTModuleType.GameEditor || ModuleType == UHTModuleType.GameThirdParty;
		}
		public static UHTModuleType? EngineModuleTypeFromHostType(ModuleHostType ModuleType)
		{
			switch (ModuleType)
			{
				case ModuleHostType.Runtime:
				case ModuleHostType.RuntimeNoCommandlet:
                case ModuleHostType.RuntimeAndProgram:
                case ModuleHostType.CookedOnly:
                case ModuleHostType.ServerOnly:
                case ModuleHostType.ClientOnly:
                    return UHTModuleType.EngineRuntime;
				case ModuleHostType.Developer:
					return UHTModuleType.EngineDeveloper;
				case ModuleHostType.Editor:
				case ModuleHostType.EditorNoCommandlet:
					return UHTModuleType.EngineEditor;
				default:
					return null;
			}
		}
		public static UHTModuleType? GameModuleTypeFromHostType(ModuleHostType ModuleType)
		{
			switch (ModuleType)
			{
				case ModuleHostType.Runtime:
				case ModuleHostType.RuntimeNoCommandlet:
                case ModuleHostType.RuntimeAndProgram:
                case ModuleHostType.CookedOnly:
                case ModuleHostType.ServerOnly:
                case ModuleHostType.ClientOnly:
                    return UHTModuleType.GameRuntime;
				case ModuleHostType.Developer:
					return UHTModuleType.GameDeveloper;
				case ModuleHostType.Editor:
				case ModuleHostType.EditorNoCommandlet:
					return UHTModuleType.GameEditor;
				default:
					return null;
			}
		}
	}

	/// <summary>
	/// Information about a module that needs to be passed to UnrealHeaderTool for code generation
	/// </summary>
	[Serializable]
	class UHTModuleInfo : ISerializable
	{
		/// <summary>
		/// Module name
		/// </summary>
		public string ModuleName;

		/// <summary>
		/// Path to the module rules file
		/// </summary>
		public FileReference ModuleRulesFile;

		/// <summary>
		/// Module base directory
		/// </summary>
		public DirectoryReference ModuleDirectory;

		/// <summary>
		/// Module type
		/// </summary>
		public string ModuleType;

		/// <summary>
		/// Public UObject headers found in the Classes directory (legacy)
		/// </summary>
		public List<FileItem> PublicUObjectClassesHeaders;

		/// <summary>
		/// Public headers with UObjects
		/// </summary>
		public List<FileItem> PublicUObjectHeaders;

		/// <summary>
		/// Private headers with UObjects
		/// </summary>
		public List<FileItem> PrivateUObjectHeaders;

		/// <summary>
		/// Module PCH absolute path
		/// </summary>
		public string PCH;

		/// <summary>
		/// Base (i.e. extensionless) path+filename of the .gen files
		/// </summary>
		public string GeneratedCPPFilenameBase;

		/// <summary>
		/// Version of code generated by UHT
		/// </summary>
		public EGeneratedCodeVersion GeneratedCodeVersion;

		public UHTModuleInfo()
		{
		}

		public UHTModuleInfo(SerializationInfo Info, StreamingContext Context)
		{
			ModuleName = Info.GetString("mn");
			ModuleRulesFile = (FileReference)Info.GetValue("mr", typeof(FileReference));
			ModuleDirectory = (DirectoryReference)Info.GetValue("md", typeof(DirectoryReference));
			ModuleType = Info.GetString("mt");
			PublicUObjectClassesHeaders = (List<FileItem>)Info.GetValue("cl", typeof(List<FileItem>));
			PublicUObjectHeaders = (List<FileItem>)Info.GetValue("pu", typeof(List<FileItem>));
			PrivateUObjectHeaders = (List<FileItem>)Info.GetValue("pr", typeof(List<FileItem>));
			PCH = Info.GetString("pc");
			GeneratedCPPFilenameBase = Info.GetString("ge");
			GeneratedCodeVersion = (EGeneratedCodeVersion)Info.GetInt32("gv");
		}

		public void GetObjectData(SerializationInfo Info, StreamingContext Context)
		{
			Info.AddValue("mn", ModuleName);
			Info.AddValue("mr", ModuleRulesFile);
			Info.AddValue("md", ModuleDirectory);
			Info.AddValue("mt", ModuleType);
			Info.AddValue("cl", PublicUObjectClassesHeaders);
			Info.AddValue("pu", PublicUObjectHeaders);
			Info.AddValue("pr", PrivateUObjectHeaders);
			Info.AddValue("pc", PCH);
			Info.AddValue("ge", GeneratedCPPFilenameBase);
			Info.AddValue("gv", (int)GeneratedCodeVersion);
		}

		public override string ToString()
		{
			return ModuleName;
		}
	}

	/// <summary>
	/// This MUST be kept in sync with EGeneratedBodyVersion enum and 
	/// ToGeneratedBodyVersion function in UHT defined in GeneratedCodeVersion.h.
	/// </summary>
	public enum EGeneratedCodeVersion
	{
		/// <summary>
		/// 
		/// </summary>
		None,

		/// <summary>
		/// 
		/// </summary>
		V1,

		/// <summary>
		/// 
		/// </summary>
		V2,

		/// <summary>
		/// 
		/// </summary>
		VLatest = V2
	};

	struct UHTManifest
	{
		public struct Module
		{
			public string Name;
			public string ModuleType;
			public string BaseDirectory;
			public string IncludeBase;     // The include path which all UHT-generated includes should be relative to
			public string OutputDirectory;
			public List<string> ClassesHeaders;
			public List<string> PublicHeaders;
			public List<string> PrivateHeaders;
			public string PCH;
			public string GeneratedCPPFilenameBase;
			public bool SaveExportedHeaders;
			public EGeneratedCodeVersion UHTGeneratedCodeVersion;

			public override string ToString()
			{
				return Name;
			}
		}

		public UHTManifest(UEBuildTarget Target, string InRootLocalPath, string InRootBuildPath, string InExternalDependenciesFile, IEnumerable<UHTModuleInfo> ModuleInfo, bool bUsePrecompiled)
		{
			IsGameTarget = (Target.TargetType != TargetType.Program);
			RootLocalPath = InRootLocalPath;
			RootBuildPath = InRootBuildPath;
			TargetName = Target.GetTargetName();
			ExternalDependenciesFile = InExternalDependenciesFile;

			Modules = ModuleInfo.Select(Info => new Module
			{
				Name = Info.ModuleName,
				ModuleType = Info.ModuleType,
				BaseDirectory = Info.ModuleDirectory.FullName,
				IncludeBase = Info.ModuleDirectory.FullName,
				OutputDirectory = Path.GetDirectoryName(Info.GeneratedCPPFilenameBase),
				ClassesHeaders = Info.PublicUObjectClassesHeaders.Select((Header) => Header.AbsolutePath).ToList(),
				PublicHeaders = Info.PublicUObjectHeaders.Select((Header) => Header.AbsolutePath).ToList(),
				PrivateHeaders = Info.PrivateUObjectHeaders.Select((Header) => Header.AbsolutePath).ToList(),
				PCH = Info.PCH,
				GeneratedCPPFilenameBase = Info.GeneratedCPPFilenameBase,
				SaveExportedHeaders = !bUsePrecompiled || !Info.ModuleDirectory.IsUnderDirectory(UnrealBuildTool.EngineDirectory),
				UHTGeneratedCodeVersion = Info.GeneratedCodeVersion,
			}).ToList();
		}

		public bool IsGameTarget;     // True if the current target is a game target
		public string RootLocalPath;    // The engine path on the local machine
		public string RootBuildPath;    // The engine path on the build machine, if different (e.g. Mac/iOS builds)
		public string TargetName;       // Name of the target currently being compiled
		public string ExternalDependenciesFile; // File to contain additional dependencies that the generated code depends on
		public List<Module> Modules;
	}


	/// <summary>
	/// This handles all running of the UnrealHeaderTool
	/// </summary>
	class ExternalExecution
	{
		/// <summary>
		/// Generates a UHTModuleInfo for a particular named module under a directory.
		/// </summary>
		/// <returns></returns>
		public static UHTModuleInfo CreateUHTModuleInfo(IEnumerable<FileReference> HeaderFilenames, string ModuleName, FileReference ModuleRulesFile, DirectoryReference ModuleDirectory, UHTModuleType ModuleType, EGeneratedCodeVersion GeneratedCodeVersion)
		{
			DirectoryReference ClassesFolder = DirectoryReference.Combine(ModuleDirectory, "Classes");
			DirectoryReference PublicFolder = DirectoryReference.Combine(ModuleDirectory, "Public");

			List<FileItem> AllClassesHeaders = new List<FileItem>();
			List<FileItem> PublicUObjectHeaders = new List<FileItem>();
			List<FileItem> PrivateUObjectHeaders = new List<FileItem>();

			foreach (FileReference Header in HeaderFilenames)
			{
				// Check to see if we know anything about this file.  If we have up-to-date cached information about whether it has
				// UObjects or not, we can skip doing a test here.
				FileItem UObjectHeaderFileItem = FileItem.GetExistingItemByFileReference(Header);

				if (CPPHeaders.DoesFileContainUObjects(UObjectHeaderFileItem.AbsolutePath))
				{
					if (UObjectHeaderFileItem.Reference.IsUnderDirectory(ClassesFolder))
					{
						AllClassesHeaders.Add(UObjectHeaderFileItem);
					}
					else if (UObjectHeaderFileItem.Reference.IsUnderDirectory(PublicFolder))
					{
						PublicUObjectHeaders.Add(UObjectHeaderFileItem);
					}
					else
					{
						PrivateUObjectHeaders.Add(UObjectHeaderFileItem);
					}
				}
			}

			UHTModuleInfo Result = new UHTModuleInfo
			{
				ModuleName = ModuleName,
				ModuleRulesFile = ModuleRulesFile,
				ModuleDirectory = ModuleDirectory,
				ModuleType = ModuleType.ToString(),
				PublicUObjectClassesHeaders = AllClassesHeaders,
				PublicUObjectHeaders = PublicUObjectHeaders,
				PrivateUObjectHeaders = PrivateUObjectHeaders,
				GeneratedCodeVersion = GeneratedCodeVersion
			};

			return Result;
		}

		static ExternalExecution()
		{
		}

		public static UHTModuleType GetEngineModuleTypeFromDescriptor(ModuleDescriptor Module)
		{
            UHTModuleType? Type = UHTModuleTypeExtensions.EngineModuleTypeFromHostType(Module.Type);
            if (Type == null)
            {
                throw new BuildException("Unhandled engine module type {0}", Module.Type.ToString());
            }
            return Type.GetValueOrDefault();
        }

		public static UHTModuleType GetGameModuleTypeFromDescriptor(ModuleDescriptor Module)
		{
            UHTModuleType? Type = UHTModuleTypeExtensions.GameModuleTypeFromHostType(Module.Type);
            if (Type == null)
            {
                throw new BuildException("Unhandled game module type {0}", Module.Type.ToString());
            }
            return Type.GetValueOrDefault();
        }

		public static UHTModuleType? GetEngineModuleTypeBasedOnLocation(DirectoryReference SourceDirectory, FileReference ModuleFileName)
		{
			if (ModuleFileName.IsUnderDirectory(DirectoryReference.Combine(SourceDirectory, "Runtime")))
			{
				return UHTModuleType.EngineRuntime;
			}

			if (ModuleFileName.IsUnderDirectory(DirectoryReference.Combine(SourceDirectory, "Developer")))
			{
				return UHTModuleType.EngineDeveloper;
			}

			if (ModuleFileName.IsUnderDirectory(DirectoryReference.Combine(SourceDirectory, "Editor")))
			{
				return UHTModuleType.EngineEditor;
			}

			if (ModuleFileName.IsUnderDirectory(DirectoryReference.Combine(SourceDirectory, "Programs")))
			{
				return UHTModuleType.Program;
			}

			if (ModuleFileName.IsUnderDirectory(DirectoryReference.Combine(SourceDirectory, "ThirdParty")))
			{
				return UHTModuleType.EngineThirdParty;
			}

			return null;
		}

		/// <summary>
		/// Returns a copy of Nodes sorted by dependency.  Independent or circularly-dependent nodes should
		/// remain in their same relative order within the original Nodes sequence.
		/// </summary>
		/// <param name="NodeList">The list of nodes to sort.</param>
		public static void StableTopologicalSort(List<UEBuildModuleCPP> NodeList)
		{
			int            NodeCount = NodeList.Count;

			Dictionary<UEBuildModule, HashSet<UEBuildModule>> Cache = new Dictionary<UEBuildModule, HashSet<UEBuildModule>>();

			for (int Index1 = 0; Index1 != NodeCount; ++Index1)
			{
				UEBuildModuleCPP Node1 = NodeList[Index1];

				for (int Index2 = 0; Index2 != Index1; ++Index2)
				{
					UEBuildModuleCPP Node2 = NodeList[Index2];

					if (IsDependency(Node2, Node1, Cache) && !IsDependency(Node1, Node2, Cache))
					{
						// Rotate element at Index1 into position at Index2
						for (int Index3 = Index1; Index3 != Index2; )
						{
							--Index3;
							NodeList[Index3 + 1] = NodeList[Index3];
						}
						NodeList[Index2] = Node1;

						// Break out of this loop, because this iteration must have covered all existing cases
						// involving the node formerly at position Index1
						break;
					}
				}
			}
		}

		/// <summary>
		/// Tests whether one module has a dependency on another
		/// </summary>
		/// <param name="FromModule">The module to test</param>
		/// <param name="ToModule">The module to look for a dependency</param>
		/// <param name="Cache">Cache mapping module to all its dependencies</param>
		/// <returns>True if ToModule is a dependency of FromModule, false otherwise</returns>
		static bool IsDependency(UEBuildModuleCPP FromModule, UEBuildModuleCPP ToModule, Dictionary<UEBuildModule, HashSet<UEBuildModule>> Cache)
		{
			HashSet<UEBuildModule> Dependencies;
			if(!Cache.TryGetValue(FromModule, out Dependencies))
			{
				Dependencies = new HashSet<UEBuildModule>();
				FromModule.GetAllDependencyModules(new List<UEBuildModule>(), Dependencies, true, true, false);
				Cache.Add(FromModule, Dependencies);
			}
			return Dependencies.Contains(ToModule);
		}

		public static void SetupUObjectModules(IEnumerable<UEBuildModuleCPP> ModulesToGenerateHeadersFor, ReadOnlyTargetRules Target, CppCompileEnvironment GlobalCompileEnvironment, List<UHTModuleInfo> UObjectModules, Dictionary<string, FlatModuleCsDataType> FlatModuleCsData, EGeneratedCodeVersion GeneratedCodeVersion)
		{
			DateTime UObjectDiscoveryStartTime = DateTime.UtcNow;

			// Sort modules by type, then by dependency
			List<UEBuildModuleCPP> ModulesSortedByType = ModulesToGenerateHeadersFor.OrderBy(c => c.Type).ToList();
			StableTopologicalSort(ModulesSortedByType);

			foreach (UEBuildModuleCPP Module in ModulesSortedByType)
			{
				UEBuildModuleCPP.UHTModuleInfoCacheType UHTModuleInfo = Module.GetCachedUHTModuleInfo(GeneratedCodeVersion);
				if (UHTModuleInfo.Info.PublicUObjectClassesHeaders.Count > 0 || UHTModuleInfo.Info.PrivateUObjectHeaders.Count > 0 || UHTModuleInfo.Info.PublicUObjectHeaders.Count > 0)
				{
					// If we've got this far and there are no source files then it's likely we're installed and ignoring
					// engine files, so we don't need a .gen.cpp either
					UEBuildModuleCPP.AutoGenerateCppInfoClass.BuildInfoClass BuildInfo = null;
					UHTModuleInfo.Info.GeneratedCPPFilenameBase = Path.Combine(Module.GeneratedCodeDirectory.FullName, UHTModuleInfo.Info.ModuleName) + ".gen";
					if (Module.SourceFilesToBuild.Count != 0)
					{
						BuildInfo = new UEBuildModuleCPP.AutoGenerateCppInfoClass.BuildInfoClass(Path.Combine(Module.GeneratedCodeDirectory.FullName, "*.gen.cpp"));
					}

					Module.AutoGenerateCppInfo = new UEBuildModuleCPP.AutoGenerateCppInfoClass(BuildInfo);

					// If we're running in "gather" mode only, we'll go ahead and cache PCH information for each module right now, so that we don't
					// have to do it in the assembling phase.  It's OK for gathering to take a bit longer, even if UObject headers are not out of
					// date in order to save a lot of time in the assembling runs.
					UHTModuleInfo.Info.PCH = "";
					if (UnrealBuildTool.IsGatheringBuild && !UnrealBuildTool.IsAssemblingBuild)
					{
						// We need to figure out which PCH header this module is including, so that UHT can inject an include statement for it into any .cpp files it is synthesizing
						CppCompileEnvironment ModuleCompileEnvironment = Module.CreateModuleCompileEnvironment(Target, GlobalCompileEnvironment);
						Module.CachePCHUsageForModuleSourceFiles(Target, ModuleCompileEnvironment);
						if (Module.ProcessedDependencies.UniquePCHHeaderFile != null)
						{
							UHTModuleInfo.Info.PCH = Module.ProcessedDependencies.UniquePCHHeaderFile.AbsolutePath;
						}
					}

					UObjectModules.Add(UHTModuleInfo.Info);
					FlatModuleCsData[Module.Name].ModuleSourceFolder = Module.ModuleDirectory;
					FlatModuleCsData[Module.Name].UHTHeaderNames = UHTModuleInfo.HeaderFilenames.ToList();
					Log.TraceVerbose("Detected UObject module: " + UHTModuleInfo.Info.ModuleName);
				}
				else
				{
					// Remove any stale generated code directory
					if(Module.GeneratedCodeDirectory != null && (!UnrealBuildTool.IsEngineInstalled() || !Module.GeneratedCodeDirectory.IsUnderDirectory(UnrealBuildTool.EngineDirectory)))
					{
						if (DirectoryReference.Exists(Module.GeneratedCodeDirectory))
						{
							Log.TraceVerbose("Deleting stale generated code directory: " + Module.GeneratedCodeDirectory.ToString());
							Directory.Delete(Module.GeneratedCodeDirectory.FullName, true);
						}
					}
				}
			}

			if (UnrealBuildTool.bPrintPerformanceInfo)
			{
				double UObjectDiscoveryTime = (DateTime.UtcNow - UObjectDiscoveryStartTime).TotalSeconds;
				Console.WriteLine("UObject discovery time: " + UObjectDiscoveryTime + "s");
			}
		}

		/// <summary>
		/// Gets the path to the receipt for UHT
		/// </summary>
		/// <returns>Path to the UHT receipt</returns>
		static FileReference GetHeaderToolReceiptFile()
		{
			UnrealTargetConfiguration Config = BuildConfiguration.bForceDebugUnrealHeaderTool ? UnrealTargetConfiguration.Debug : UnrealTargetConfiguration.Development;
			return TargetReceipt.GetDefaultPath(UnrealBuildTool.EngineDirectory, "UnrealHeaderTool", BuildHostPlatform.Current.Platform, Config, "");
		}

		/// <summary>
		/// Gets UnrealHeaderTool.exe path. Does not care if UnrealheaderTool was build as a monolithic exe or not.
		/// </summary>
		static string GetHeaderToolPath()
		{
			FileReference ReceiptFileName = GetHeaderToolReceiptFile();
			TargetReceipt Receipt = TargetReceipt.Read(ReceiptFileName, UnrealBuildTool.EngineDirectory, null);

			string HeaderToolPath = Receipt.BuildProducts[0].Path.FullName;
			return HeaderToolPath;
		}

		/// <summary>
		/// Gets the latest write time of any of the UnrealHeaderTool binaries (including DLLs and Plugins) or DateTime.MaxValue if UnrealHeaderTool does not exist
		/// </summary>
		/// <returns>
		/// Latest timestamp of UHT binaries or DateTime.MaxValue if UnrealHeaderTool is out of date and needs to be rebuilt.
		/// </returns>
		static bool GetHeaderToolTimestamp(out DateTime Timestamp)
		{
			using (ScopedTimer TimestampTimer = new ScopedTimer("GetHeaderToolTimestamp"))
			{
				// Try to read the receipt for UHT.


				FileReference ReceiptPath = GetHeaderToolReceiptFile();
				if (!FileReference.Exists(ReceiptPath))
				{
					Timestamp = DateTime.MaxValue;
					return false;
				}

				TargetReceipt Receipt;
				if (!TargetReceipt.TryRead(ReceiptPath, UnrealBuildTool.EngineDirectory, null, out Receipt))
				{
					Timestamp = DateTime.MaxValue;
					return false;
				}

				// Check all the binaries exist, and that all the DLLs are built against the right version
				if (!CheckBinariesExist(Receipt) || !CheckDynamicLibaryVersionsMatch(Receipt))
				{
					Timestamp = DateTime.MaxValue;
					return false;
				}

				// Return the timestamp for all the binaries
				Timestamp = GetTimestampFromBinaries(Receipt);
				return true;
			}
		}

		/// <summary>
		/// Checks if all the files in a receipt are present and that all the DLLs are at the same version
		/// </summary>
		/// <returns>
		/// True if all the files are valid.
		/// </returns>
		static bool CheckBinariesExist(TargetReceipt Receipt)
		{
			bool bExist = true;
			foreach (BuildProduct BuildProduct in Receipt.BuildProducts)
			{
				if (BuildProduct.Type == BuildProductType.Executable || BuildProduct.Type == BuildProductType.DynamicLibrary)
				{
					if (!FileReference.Exists(BuildProduct.Path))
					{
						Log.TraceWarning("Missing binary: {0}", BuildProduct.Path);
						bExist = false;
					}
				}
			}
			return bExist;
		}

		/// <summary>
		/// Checks if all the files in a receipt have the same version
		/// </summary>
		/// <returns>
		/// True if all the files are valid.
		/// </returns>
		static bool CheckDynamicLibaryVersionsMatch(TargetReceipt Receipt)
		{
			List<Tuple<FileReference, int>> BinaryVersions = new List<Tuple<FileReference, int>>();
			foreach (BuildProduct BuildProduct in Receipt.BuildProducts)
			{
				if (BuildProduct.Type == BuildProductType.DynamicLibrary)
				{
					int Version = BuildHostPlatform.Current.GetDllApiVersion(BuildProduct.Path.FullName);
					BinaryVersions.Add(new Tuple<FileReference, int>(BuildProduct.Path, Version));
				}
			}

			bool bMatch = true;
			if (BinaryVersions.Count > 0 && !BinaryVersions.All(x => x.Item2 == BinaryVersions[0].Item2))
			{
				Log.TraceWarning("Detected mismatch in binary versions:");
				foreach (Tuple<FileReference, int> BinaryVersion in BinaryVersions)
				{
					Log.TraceWarning("  {0} has API version {1}", BinaryVersion.Item1, BinaryVersion.Item2);
					FileReference.Delete(BinaryVersion.Item1);
				}
				bMatch = false;
			}
			return bMatch;
		}

		/// <summary>
		/// Checks if all the files in a receipt are present and that all the DLLs are at the same version
		/// </summary>
		/// <returns>
		/// True if all the files are valid.
		/// </returns>
		static DateTime GetTimestampFromBinaries(TargetReceipt Receipt)
		{
			DateTime LatestWriteTime = DateTime.MinValue;
			foreach (BuildProduct BuildProduct in Receipt.BuildProducts)
			{
				if (BuildProduct.Type == BuildProductType.Executable || BuildProduct.Type == BuildProductType.DynamicLibrary)
				{
					DateTime WriteTime = FileReference.GetLastWriteTime(BuildProduct.Path);
					if (WriteTime > LatestWriteTime)
					{
						LatestWriteTime = WriteTime;
					}
				}
			}
			return LatestWriteTime;
		}

		/// <summary>
		/// Gets the timestamp of CoreUObject.gen.cpp file.
		/// </summary>
		/// <returns>Last write time of CoreUObject.gen.cpp or DateTime.MaxValue if it doesn't exist.</returns>
		private static DateTime GetCoreGeneratedTimestamp(string ModuleName, string ModuleGeneratedCodeDirectory)
		{
			// In Installed Builds, we don't check the timestamps on engine headers.  Default to a very old date.
			if (UnrealBuildTool.IsEngineInstalled())
			{
				return DateTime.MinValue;
			}

			// Otherwise look for CoreUObject.init.gen.cpp
			FileInfo CoreGeneratedFileInfo = new FileInfo(Path.Combine(ModuleGeneratedCodeDirectory, ModuleName + ".init.gen.cpp"));
			if (CoreGeneratedFileInfo.Exists)
			{
				return CoreGeneratedFileInfo.LastWriteTime;
			}

			// Doesn't exist, so use a 'newer that everything' date to force rebuild headers.
			return DateTime.MaxValue;
		}

		/// <summary>
		/// Checks the class header files and determines if generated UObject code files are out of date in comparison.
		/// </summary>
		/// <param name="BuildConfiguration">Build configuration</param>
		/// <param name="UObjectModules">Modules that we generate headers for</param>
		/// <param name="HeaderToolTimestamp">Timestamp for UHT</param>
		/// <param name="bUsePrecompiled">Whether to use precompiled engine modules</param>
		/// <param name="HotReload">The hot reload state</param>
		/// <returns>    True if the code files are out of date</returns>
		private static bool AreGeneratedCodeFilesOutOfDate(BuildConfiguration BuildConfiguration, List<UHTModuleInfo> UObjectModules, DateTime HeaderToolTimestamp, bool bUsePrecompiled, EHotReload HotReload)
		{
			// Get CoreUObject.init.gen.cpp timestamp.  If the source files are older than the CoreUObject generated code, we'll
			// need to regenerate code for the module
			DateTime? CoreGeneratedTimestamp = null;
			{
				// Find the CoreUObject module
				foreach (UHTModuleInfo Module in UObjectModules)
				{
					if (Module.ModuleName.Equals("CoreUObject", StringComparison.InvariantCultureIgnoreCase))
					{
						CoreGeneratedTimestamp = GetCoreGeneratedTimestamp(Module.ModuleName, Path.GetDirectoryName(Module.GeneratedCPPFilenameBase));
						break;
					}
				}
				if (CoreGeneratedTimestamp == null)
				{
					throw new BuildException("Could not find CoreUObject in list of all UObjectModules");
				}
			}


			foreach (UHTModuleInfo Module in UObjectModules)
			{
				// If we're using a precompiled engine, skip skip checking timestamps for modules that are under the engine directory
				if (bUsePrecompiled && UnrealBuildTool.IsUnderAnEngineDirectory(Module.ModuleDirectory))
				{
					continue;
				}

				// Make sure we have an existing folder for generated code.  If not, then we definitely need to generate code!
				string GeneratedCodeDirectory = Path.GetDirectoryName(Module.GeneratedCPPFilenameBase);
				FileSystemInfo TestDirectory = (FileSystemInfo)new DirectoryInfo(GeneratedCodeDirectory);
				if (!TestDirectory.Exists)
				{
					// Generated code directory is missing entirely!
					Log.TraceVerbose("UnrealHeaderTool needs to run because no generated code directory was found for module {0}", Module.ModuleName);
					return true;
				}

				// Grab our special "Timestamp" file that we saved after the last set of headers were generated.  This file
				// actually contains the list of source files which contained UObjects, so that we can compare to see if any
				// UObject source files were deleted (or no longer contain UObjects), which means we need to run UHT even
				// if no other source files were outdated
				string TimestampFile = Path.Combine(GeneratedCodeDirectory, @"Timestamp");
				FileSystemInfo SavedTimestampFileInfo = (FileSystemInfo)new FileInfo(TimestampFile);
				if (!SavedTimestampFileInfo.Exists)
				{
					// Timestamp file was missing (possibly deleted/cleaned), so headers are out of date
					Log.TraceVerbose("UnrealHeaderTool needs to run because UHT Timestamp file did not exist for module {0}", Module.ModuleName);
					return true;
				}

				// Make sure the last UHT run completed after UnrealHeaderTool.exe was compiled last, and after the CoreUObject headers were touched last.
				DateTime SavedTimestamp = SavedTimestampFileInfo.LastWriteTime;
				if (HeaderToolTimestamp > SavedTimestamp || CoreGeneratedTimestamp > SavedTimestamp)
				{
					// Generated code is older than UnrealHeaderTool.exe or CoreUObject headers.  Out of date!
					Log.TraceVerbose("UnrealHeaderTool needs to run because UnrealHeaderTool.exe or CoreUObject headers are newer than SavedTimestamp for module {0}", Module.ModuleName);
					return true;
				}

				// Has the .build.cs file changed since we last generated headers successfully?
				FileInfo ModuleRulesFile = new FileInfo(Module.ModuleRulesFile.FullName);
				if (!ModuleRulesFile.Exists || ModuleRulesFile.LastWriteTime > SavedTimestamp)
				{
					Log.TraceVerbose("UnrealHeaderTool needs to run because SavedTimestamp is older than the rules file ({0}) for module {1}", Module.ModuleRulesFile, Module.ModuleName);
					return true;
				}

				// Iterate over our UObjects headers and figure out if any of them have changed
				List<FileItem> AllUObjectHeaders = new List<FileItem>();
				AllUObjectHeaders.AddRange(Module.PublicUObjectClassesHeaders);
				AllUObjectHeaders.AddRange(Module.PublicUObjectHeaders);
				AllUObjectHeaders.AddRange(Module.PrivateUObjectHeaders);

				// Load up the old timestamp file and check to see if anything has changed
				{
					string[] UObjectFilesFromPreviousRun = File.ReadAllLines(TimestampFile);
					if (AllUObjectHeaders.Count != UObjectFilesFromPreviousRun.Length)
					{
						Log.TraceVerbose("UnrealHeaderTool needs to run because there are a different number of UObject source files in module {0}", Module.ModuleName);
						return true;
					}
					for (int FileIndex = 0; FileIndex < AllUObjectHeaders.Count; ++FileIndex)
					{
						if (!UObjectFilesFromPreviousRun[FileIndex].Equals(AllUObjectHeaders[FileIndex].AbsolutePath, StringComparison.InvariantCultureIgnoreCase))
						{
							Log.TraceVerbose("UnrealHeaderTool needs to run because the set of UObject source files in module {0} has changed", Module.ModuleName);
							return true;
						}
					}
				}

				foreach (FileItem HeaderFile in AllUObjectHeaders)
				{
					DateTime HeaderFileTimestamp = HeaderFile.Info.LastWriteTime;

					// Has the source header changed since we last generated headers successfully?
					if (HeaderFileTimestamp > SavedTimestamp)
					{
						Log.TraceVerbose("UnrealHeaderTool needs to run because SavedTimestamp is older than HeaderFileTimestamp ({0}) for module {1}", HeaderFile.AbsolutePath, Module.ModuleName);
						return true;
					}

					// When we're running in assembler mode, outdatedness cannot be inferred by checking the directory timestamp
					// of the source headers.  We don't care if source files were added or removed in this mode, because we're only
					// able to process the known UObject headers that are in the Makefile.  If UObject header files are added/removed,
					// we expect the user to re-run GenerateProjectFiles which will force UBTMakefile outdatedness.
					// @todo ubtmake: Possibly, we should never be doing this check these days.
					//
					// We don't need to do this check if using hot reload makefiles, since makefile out-of-date checks already handle it.
					if ((HotReload == EHotReload.Disabled || !BuildConfiguration.bUseUBTMakefiles) && (UnrealBuildTool.IsGatheringBuild || !UnrealBuildTool.IsAssemblingBuild))
					{
						// Also check the timestamp on the directory the source file is in.  If the directory timestamp has
						// changed, new source files may have been added or deleted.  We don't know whether the new/deleted
						// files were actually UObject headers, but because we don't know all of the files we processed
						// in the previous run, we need to assume our generated code is out of date if the directory timestamp
						// is newer.
						DateTime HeaderDirectoryTimestamp = new DirectoryInfo(Path.GetDirectoryName(HeaderFile.AbsolutePath)).LastWriteTime;
						if (HeaderDirectoryTimestamp > SavedTimestamp)
						{
							Log.TraceVerbose("UnrealHeaderTool needs to run because the directory containing an existing header ({0}) has changed, and headers may have been added to or deleted from module {1}", HeaderFile.AbsolutePath, Module.ModuleName);
							return true;
						}
					}
				}
			}

			return false;
		}

		/// <summary>
		/// Determines if any external dependencies for generated code is out of date
		/// </summary>
		/// <param name="ExternalDependenciesFile">Path to the external dependencies file</param>
		/// <returns>True if any external dependencies are out of date</returns>
		private static bool AreExternalDependenciesOutOfDate(FileReference ExternalDependenciesFile)
		{
			if (!FileReference.Exists(ExternalDependenciesFile))
			{
				return true;
			}

			DateTime LastWriteTime = File.GetLastWriteTimeUtc(ExternalDependenciesFile.FullName);

			string[] Lines = File.ReadAllLines(ExternalDependenciesFile.FullName);
			foreach (string Line in Lines)
			{
				string ExternalDependencyFile = Line.Trim();
				if (ExternalDependencyFile.Length > 0)
				{
					if (!File.Exists(ExternalDependencyFile) || File.GetLastWriteTimeUtc(ExternalDependencyFile) > LastWriteTime)
					{
						return true;
					}
				}
			}

			return false;
		}

		/// <summary>
		/// Updates the intermediate include directory timestamps of all the passed in UObject modules
		/// </summary>
		private static void UpdateDirectoryTimestamps(List<UHTModuleInfo> UObjectModules, bool bUsePrecompiled)
		{
			foreach (UHTModuleInfo Module in UObjectModules)
			{
				string GeneratedCodeDirectory = Path.GetDirectoryName(Module.GeneratedCPPFilenameBase);
				DirectoryInfo GeneratedCodeDirectoryInfo = new DirectoryInfo(GeneratedCodeDirectory);

				try
				{
					if (GeneratedCodeDirectoryInfo.Exists)
					{
						// Don't write anything to the engine directory if we're running an installed build
						if (bUsePrecompiled && UnrealBuildTool.IsUnderAnEngineDirectory(Module.ModuleDirectory))
						{
							continue;
						}

						// Touch the include directory since we have technically 'generated' the headers
						// However, the headers might not be touched at all since that would cause the compiler to recompile everything
						// We can't alter the directory timestamp directly, because this may throw exceptions when the directory is
						// open in visual studio or windows explorer, so instead we create a blank file that will change the timestamp for us
						FileReference TimestampFile = FileReference.Combine(new DirectoryReference(GeneratedCodeDirectoryInfo.FullName), "Timestamp");

						// Save all of the UObject files to a timestamp file.  We'll load these on the next run to see if any new
						// files with UObject classes were deleted, so that we'll know to run UHT even if the timestamps of all
						// of the other source files were unchanged
						{
							List<string> AllUObjectFiles = new List<string>();
							AllUObjectFiles.AddRange(Module.PublicUObjectClassesHeaders.ConvertAll(Item => Item.AbsolutePath));
							AllUObjectFiles.AddRange(Module.PublicUObjectHeaders.ConvertAll(Item => Item.AbsolutePath));
							AllUObjectFiles.AddRange(Module.PrivateUObjectHeaders.ConvertAll(Item => Item.AbsolutePath));
							ResponseFile.Create(TimestampFile, AllUObjectFiles, ResponseFile.CreateOptions.WriteEvenIfUnchanged);
						}
					}
				}
				catch (Exception Exception)
				{
					throw new BuildException(Exception, "Couldn't touch header directories: " + Exception.Message);
				}
			}
		}

		/// <summary>
		/// Run an external exe (and capture the output), given the exe path and the commandline.
		/// </summary>
		public static int RunExternalDotNETExecutable(string ExePath, string Commandline)
		{
#if NET_CORE
			ProcessStartInfo ExeInfo = new ProcessStartInfo("dotnet", ExePath + " " + Commandline);
#else
			ProcessStartInfo ExeInfo = new ProcessStartInfo(ExePath, Commandline);
#endif
			Log.TraceVerbose("RunExternalExecutable {0} {1}", ExePath, Commandline);
			ExeInfo.UseShellExecute = false;
			ExeInfo.RedirectStandardOutput = true;
			using (Process GameProcess = Process.Start(ExeInfo))
			{
				GameProcess.BeginOutputReadLine();
				GameProcess.OutputDataReceived += PrintProcessOutputAsync;
				GameProcess.WaitForExit();

				return GameProcess.ExitCode;
			}
		}

		/// <summary>
		/// Run an external native executable (and capture the output), given the executable path and the commandline.
		/// </summary>
		public static int RunExternalNativeExecutable(string ExePath, string Commandline)
		{
			ProcessStartInfo ExeInfo = new ProcessStartInfo(ExePath, Commandline);
			Log.TraceVerbose("RunExternalExecutable {0} {1}", ExePath, Commandline);
			ExeInfo.UseShellExecute = false;
			ExeInfo.RedirectStandardOutput = true;
			using (Process GameProcess = Process.Start(ExeInfo))
			{
				GameProcess.BeginOutputReadLine();
				GameProcess.OutputDataReceived += PrintProcessOutputAsync;
				GameProcess.WaitForExit();

				return GameProcess.ExitCode;
			}
		}

		/// <summary>
		/// Simple function to pipe output asynchronously
		/// </summary>
		private static void PrintProcessOutputAsync(object Sender, DataReceivedEventArgs Event)
		{
			// DataReceivedEventHandler is fired with a null string when the output stream is closed.  We don't want to
			// print anything for that event.
			if (!String.IsNullOrEmpty(Event.Data))
			{
				Log.TraceInformation(Event.Data);
			}
		}

		/// <summary>
		/// Builds and runs the header tool and touches the header directories.
		/// Performs any early outs if headers need no changes, given the UObject modules, tool path, game name, and configuration
		/// </summary>
		public static bool ExecuteHeaderToolIfNecessary(BuildConfiguration BuildConfiguration, UEBuildTarget Target, CppCompileEnvironment GlobalCompileEnvironment, List<UHTModuleInfo> UObjectModules, FileReference ModuleInfoFileName, ref ECompilationResult UHTResult, EHotReload HotReload)
		{
			if (ProgressWriter.bWriteMarkup)
			{
				Log.WriteLine(LogEventType.Console, "@progress push 5%");
			}
			using (ProgressWriter Progress = new ProgressWriter("Generating code...", false))
			{
				// We never want to try to execute the header tool when we're already trying to build it!
				bool bIsBuildingUHT = Target.GetTargetName().Equals("UnrealHeaderTool", StringComparison.InvariantCultureIgnoreCase);

				string RootLocalPath = UnrealBuildTool.RootDirectory.FullName;

				// check if UHT is out of date
				DateTime HeaderToolTimestamp = DateTime.MaxValue;
				bool bHaveHeaderTool = !bIsBuildingUHT && GetHeaderToolTimestamp(out HeaderToolTimestamp);

				// ensure the headers are up to date
				bool bUHTNeedsToRun = (BuildConfiguration.bForceHeaderGeneration || !bHaveHeaderTool || AreGeneratedCodeFilesOutOfDate(BuildConfiguration, UObjectModules, HeaderToolTimestamp, Target.bUsePrecompiled, HotReload));
				if (bUHTNeedsToRun || UnrealBuildTool.IsGatheringBuild)
				{
					// Since code files are definitely out of date, we'll now finish computing information about the UObject modules for UHT.  We
					// want to save this work until we know that UHT actually needs to be run to speed up best-case iteration times.
					if (UnrealBuildTool.IsGatheringBuild)		// In assembler-only mode, PCH info is loaded from our UBTMakefile!
					{
						foreach (UHTModuleInfo UHTModuleInfo in UObjectModules)
						{
							// Only cache the PCH name if we don't already have one.  When running in 'gather only' mode, this will have already been cached
							if (string.IsNullOrEmpty(UHTModuleInfo.PCH))
							{
								UHTModuleInfo.PCH = "";

								// We need to figure out which PCH header this module is including, so that UHT can inject an include statement for it into any .cpp files it is synthesizing
								UEBuildModuleCPP DependencyModuleCPP = (UEBuildModuleCPP)Target.GetModuleByName(UHTModuleInfo.ModuleName);
								CppCompileEnvironment ModuleCompileEnvironment = DependencyModuleCPP.CreateModuleCompileEnvironment(Target.Rules, GlobalCompileEnvironment);
								DependencyModuleCPP.CachePCHUsageForModuleSourceFiles(Target.Rules, ModuleCompileEnvironment);
								if (DependencyModuleCPP.ProcessedDependencies != null && DependencyModuleCPP.ProcessedDependencies.UniquePCHHeaderFile != null)
								{
									UHTModuleInfo.PCH = DependencyModuleCPP.ProcessedDependencies.UniquePCHHeaderFile.AbsolutePath;
								}
							}
						}
					}
				}

				// Get the file containing dependencies for the generated code
				FileReference ExternalDependenciesFile = ModuleInfoFileName.ChangeExtension(".deps");
				if (AreExternalDependenciesOutOfDate(ExternalDependenciesFile))
				{
					bUHTNeedsToRun = true;
					bHaveHeaderTool = false; // Force UHT to build until dependency checking is fast enough to run all the time
				}

				// @todo ubtmake: Optimization: Ideally we could avoid having to generate this data in the case where UHT doesn't even need to run!  Can't we use the existing copy?  (see below use of Manifest)
				UHTManifest Manifest = new UHTManifest(Target, RootLocalPath, UEBuildPlatform.GetBuildPlatform(Target.Platform).ConvertPath(RootLocalPath + '\\'), ExternalDependenciesFile.FullName, UObjectModules, Target.bUsePrecompiled);

				if (!bIsBuildingUHT && bUHTNeedsToRun)
				{
					// Always build UnrealHeaderTool if header regeneration is required, unless we're running within an installed ecosystem or hot-reloading
					if ((!UnrealBuildTool.IsEngineInstalled() || Target.bHasProjectScriptPlugin) &&
						!BuildConfiguration.bDoNotBuildUHT &&
						HotReload != EHotReload.FromIDE &&
						!(bHaveHeaderTool && !UnrealBuildTool.IsGatheringBuild && UnrealBuildTool.IsAssemblingBuild))	// If running in "assembler only" mode, we assume UHT is already up to date for much faster iteration!
					{
						// If it is out of date or not there it will be built.
						// If it is there and up to date, it will add 0.8 seconds to the build time.
						Log.TraceInformation("Building UnrealHeaderTool...");

						StringBuilder UBTArguments = new StringBuilder();

						UBTArguments.Append("UnrealHeaderTool");

						// Which desktop platform do we need to compile UHT for?
						UBTArguments.Append(" " + BuildHostPlatform.Current.Platform.ToString());

						// NOTE: We force Development configuration for UHT so that it runs quickly, even when compiling debug, unless we say so explicitly
						if (BuildConfiguration.bForceDebugUnrealHeaderTool)
						{
							UBTArguments.Append(" " + UnrealTargetConfiguration.Debug.ToString());
						}
						else
						{
							UBTArguments.Append(" " + UnrealTargetConfiguration.Development.ToString());
						}

						// NOTE: We disable mutex when launching UBT from within UBT to compile UHT
						UBTArguments.Append(" -NoMutex");

						if (!BuildConfiguration.bAllowXGE)
						{
							UBTArguments.Append(" -noxge");
						}

						// Always ignore the junk manifest on recursive invocations; it will have been run by this process if necessary
						UBTArguments.Append(" -ignorejunk");

						// Add UHT plugins to UBT command line as external plugins
						foreach(UEBuildPlugin EnabledPlugin in Target.EnabledPlugins)
						{
							if (EnabledPlugin.Descriptor.bCanBeUsedWithUnrealHeaderTool)
							{
								UBTArguments.Append(" -PLUGIN \"" + EnabledPlugin.Info.File + "\"");
							}
						}						

						if (RunExternalDotNETExecutable(UnrealBuildTool.GetUBTPath(), UBTArguments.ToString()) != 0)
						{
							return false;
						}
					}

					Progress.Write(1, 3);

					string ActualTargetName = String.IsNullOrEmpty(Target.GetTargetName()) ? "UE4" : Target.GetTargetName();
					Log.TraceInformation("Parsing headers for {0}", ActualTargetName);

					string HeaderToolPath = GetHeaderToolPath();
					if (!File.Exists(HeaderToolPath))
					{
						throw new BuildException("Unable to generate headers because UnrealHeaderTool binary was not found ({0}).", Path.GetFullPath(HeaderToolPath));
					}

					// Disable extensions when serializing to remove the $type fields
					Directory.CreateDirectory(ModuleInfoFileName.Directory.FullName);
					System.IO.File.WriteAllText(ModuleInfoFileName.FullName, fastJSON.JSON.Instance.ToJSON(Manifest, new fastJSON.JSONParameters { UseExtensions = false }));

					string CmdLine = (Target.ProjectFile != null) ? "\"" + Target.ProjectFile.FullName + "\"" : Target.GetTargetName();
					CmdLine += " \"" + ModuleInfoFileName + "\" -LogCmds=\"loginit warning, logexit warning, logdatabase error\" -Unattended -WarningsAsErrors";
					if (UnrealBuildTool.IsEngineInstalled())
					{
						CmdLine += " -installed";
					}

					if (BuildConfiguration.bFailIfGeneratedCodeChanges)
					{
						CmdLine += " -FailIfGeneratedCodeChanges";
					}

					if (Target.Rules != null && !Target.Rules.bCompileAgainstEngine)
					{
						CmdLine += " -NoEnginePlugins";
					}

					Log.TraceInformation("  Running UnrealHeaderTool {0}", CmdLine);

					Stopwatch s = new Stopwatch();
					s.Start();
					UHTResult = (ECompilationResult)RunExternalNativeExecutable(ExternalExecution.GetHeaderToolPath(), CmdLine);
					s.Stop();

					if (UHTResult != ECompilationResult.Succeeded)
					{
						// On Linux and Mac, the shell will return 128+signal number exit codes if UHT gets a signal (e.g. crashes or is interrupted)
						if ((BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux ||
							BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac) &&
							(int)(UHTResult) >= 128
							)
						{
							// SIGINT is 2, so 128 + SIGINT is 130
							UHTResult = ((int)(UHTResult) == 130) ? ECompilationResult.Canceled : ECompilationResult.CrashOrAssert;
						}

						if ((BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win32 || 
							BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64) && 
							(int)(UHTResult) < 0)
						{
							Log.TraceInformation(String.Format("UnrealHeaderTool failed with exit code 0x{0:X} - check that UE4 prerequisites are installed.", (int)UHTResult));
						}
						return false;
					}

					Log.TraceInformation("Reflection code generated for {0} in {1} seconds", ActualTargetName, s.Elapsed.TotalSeconds);
					if (UnrealBuildTool.bPrintPerformanceInfo)
					{
						Log.TraceInformation("UnrealHeaderTool took {1}", ActualTargetName, (double)s.ElapsedMilliseconds / 1000.0);
					}

					// Now that UHT has successfully finished generating code, we need to update all cached FileItems in case their last write time has changed.
					// Otherwise UBT might not detect changes UHT made.
					DateTime StartTime = DateTime.UtcNow;
					FileItem.ResetInfos();
					double ResetDuration = (DateTime.UtcNow - StartTime).TotalSeconds;
					Log.TraceVerbose("FileItem.ResetInfos() duration: {0}s", ResetDuration);
				}
				else
				{
					Log.TraceVerbose("Generated code is up to date.");
				}

				Progress.Write(2, 3);

				// There will never be generated code if we're building UHT, so this should never be called.
				if (!bIsBuildingUHT)
				{
					// Allow generated code to be sync'd to remote machines if needed. This needs to be done even if UHT did not run because
					// generated headers include other generated headers using absolute paths which in case of building remotely are already
					// the remote machine absolute paths. Because of that parsing headers will not result in finding all includes properly.
					// @todo ubtmake: Need to figure out what this does in the assembler case, and whether we need to run it
					UEBuildPlatform.GetBuildPlatform(Target.Platform).PostCodeGeneration(Manifest);
				}

				// touch the directories
				UpdateDirectoryTimestamps(UObjectModules, Target.bUsePrecompiled);

				Progress.Write(3, 3);
			}
			if (ProgressWriter.bWriteMarkup)
			{
				Log.WriteLine(LogEventType.Console, "@progress pop");
			}
			return true;
		}
	}
}
