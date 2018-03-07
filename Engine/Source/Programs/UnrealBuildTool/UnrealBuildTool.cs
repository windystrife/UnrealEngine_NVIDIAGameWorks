// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Threading;
using System.Reflection;
using System.Linq;
using System.Runtime.Serialization.Formatters.Binary;
using System.Runtime.Serialization;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	enum EHotReload
	{
		Disabled,
		FromIDE,
		FromEditor
	}

	static class UnrealBuildTool
	{
		/// <summary>
		/// How much time was spent scanning for include dependencies for outdated C++ files
		/// </summary>
		public static double TotalDeepIncludeScanTime = 0.0;

		/// <summary>
		/// The environment at boot time.
		/// </summary>
		static public System.Collections.IDictionary InitialEnvironment;

		/// <summary>
		/// Whether we're running with engine installed
		/// </summary>
		static private bool? bIsEngineInstalled;

		/// <summary>
		/// Whether we're running with an installed project
		/// </summary>
		static private bool? bIsProjectInstalled;

		/// <summary>
		/// The full name of the Root UE4 directory
		/// </summary>
		public static readonly DirectoryReference RootDirectory = new DirectoryReference(Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().GetOriginalLocation()), "..", "..", ".."));

		/// <summary>
		/// The full name of the Engine directory
		/// </summary>
		public static readonly DirectoryReference EngineDirectory = DirectoryReference.Combine(RootDirectory, "Engine");

		/// <summary>
		/// The full name of the Engine/Source directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceDirectory = DirectoryReference.Combine(EngineDirectory, "Source");

		/// <summary>
		/// Full path to the Engine/Source/Runtime directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceRuntimeDirectory = DirectoryReference.Combine(EngineSourceDirectory, "Runtime");

		/// <summary>
		/// Full path to the Engine/Source/Developer directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceDeveloperDirectory = DirectoryReference.Combine(EngineSourceDirectory, "Developer");

		/// <summary>
		/// Full path to the Engine/Source/Editor directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceEditorDirectory = DirectoryReference.Combine(EngineSourceDirectory, "Editor");

		/// <summary>
		/// Full path to the Engine/Source/Programs directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceProgramsDirectory = DirectoryReference.Combine(EngineSourceDirectory, "Programs");

		/// <summary>
		/// Full path to the Engine/Source/ThirdParty directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceThirdPartyDirectory = DirectoryReference.Combine(EngineSourceDirectory, "ThirdParty");

		/// <summary>
		/// The full name of the Enterprise directory
		/// </summary>
		public static readonly DirectoryReference EnterpriseDirectory = DirectoryReference.Combine(RootDirectory, "Enterprise");

		/// <summary>
		/// The full name of the Enterprise/Source directory
		/// </summary>
		public static readonly DirectoryReference EnterpriseSourceDirectory = DirectoryReference.Combine(EnterpriseDirectory, "Source");

		/// <summary>
		/// The Remote Ini directory.  This should always be valid when compiling using a remote server.
		/// </summary>
		static string RemoteIniPath = null;

		/// <summary>
		/// This is set to true during UBT startup, after it is safe to be accessing the IsGatheringBuild and IsAssemblingBuild properties.  Never access this directly.
		/// </summary>
		private static bool bIsSafeToCheckIfGatheringOrAssemblingBuild = false;

		/// <summary>
		/// True if this run of UBT should only invalidate Makefile.
		/// </summary>
		static public bool bIsInvalidatingMakefilesOnly = false;

		/// <summary>
		/// Cached array of all the supported target platforms
		/// </summary>
		static public UnrealTargetPlatform[] AllPlatforms = (UnrealTargetPlatform[])Enum.GetValues(typeof(UnrealTargetPlatform));

		/// <summary>
		/// Whether to print debug information out to the log
		/// </summary>
		static public bool bPrintDebugInfo
		{
			get;
			private set;
		}

		/// <summary>
		/// Whether to print performance information to the log
		/// </summary>
		static public bool bPrintPerformanceInfo
		{
			get;
			private set;
		}

		/// <summary>
		/// True if we should gather module dependencies for building in this run.  If this is false, then we'll expect to be loading information from disk about
		/// the target's modules before we'll be able to build anything.  One or both of IsGatheringBuild or IsAssemblingBuild must be true.
		/// </summary>
		static public bool IsGatheringBuild
		{
			get
			{
				if (bIsSafeToCheckIfGatheringOrAssemblingBuild)
				{
					return bIsGatheringBuild_Unsafe;
				}
				else
				{
					throw new BuildException("Accessed UnrealBuildTool.IsGatheringBuild before it was fully initialized at startup (bIsSafeToCheckIfGatheringOrAssemblingBuild)");
				}
			}
		}
		static private bool bIsGatheringBuild_Unsafe = true;

		/// <summary>
		/// True if we should go ahead and actually compile and link in this run.  If this is false, no actions will be executed and we'll stop after gathering
		/// and saving information about dependencies.  One or both of IsGatheringBuild or IsAssemblingBuild must be true.
		/// </summary>
		static public bool IsAssemblingBuild
		{
			get
			{
				if (bIsSafeToCheckIfGatheringOrAssemblingBuild)
				{
					return bIsAssemblingBuild_Unsafe;
				}
				else
				{
					throw new BuildException("Accessed UnrealBuildTool.IsGatheringBuild before it was fully initialized at startup (bIsSafeToCheckIfGatheringOrAssemblingBuild)");
				}
			}
		}
		static private bool bIsAssemblingBuild_Unsafe = true;

		/// <summary>
		/// Used when BuildConfiguration.bUseUBTMakefiles is enabled.  If true, it means that our cached includes may not longer be
		/// valid (or never were), and we need to recover by forcibly scanning included headers for all build prerequisites to make sure that our
		/// cached set of includes is actually correct, before determining which files are outdated.
		/// </summary>
		static public bool bNeedsFullCPPIncludeRescan = false;

		/// <summary>
		/// Sets the global flag indicating whether the engine is installed
		/// </summary>
		/// <param name="bInIsEngineInstalled">Whether the engine is installed or not</param>
		public static void SetIsEngineInstalled(bool bInIsEngineInstalled)
		{
			bIsEngineInstalled = bInIsEngineInstalled;
		}

		/// <summary>
		/// Returns true if UnrealBuildTool is running using installed Engine components
		/// </summary>
		/// <returns>True if running using installed Engine components</returns>
		static public bool IsEngineInstalled()
		{
			if (!bIsEngineInstalled.HasValue)
			{
				throw new BuildException("IsEngineInstalled() called before being initialized.");
			}
			return bIsEngineInstalled.Value;
		}

		/// <summary>
		/// Returns true if UnrealBuildTool is running using an installed project (ie. a mod kit)
		/// </summary>
		/// <returns>True if running using an installed project</returns>
		static public bool IsProjectInstalled()
		{
			if(!bIsProjectInstalled.HasValue)
			{
				bIsProjectInstalled = FileReference.Exists(FileReference.Combine(RootDirectory, "Engine", "Build", "InstalledProjectBuild.txt"));
			}
			return bIsProjectInstalled.Value;
		}

		/// <summary>
		/// Gets the absolute path to the UBT assembly.
		/// </summary>
		/// <returns>A string containing the path to the UBT assembly.</returns>
		static public string GetUBTPath()
		{
			string UnrealBuildToolPath = Assembly.GetExecutingAssembly().GetOriginalLocation();
			return UnrealBuildToolPath;
		}

		/// <summary>
		/// The Unreal remote tool ini directory.  This should be valid if compiling using a remote server
		/// </summary>
		/// <returns>The directory path</returns>
		static public string GetRemoteIniPath()
		{
			return RemoteIniPath;
		}

		static public void SetRemoteIniPath(string Path)
		{
			RemoteIniPath = Path;
		}

		/// <summary>
		/// Is this a valid configuration. Used primarily for Installed vs non-Installed.
		/// </summary>
		/// <param name="InConfiguration"></param>
		/// <returns>true if valid, false if not</returns>
		static public bool IsValidConfiguration(UnrealTargetConfiguration InConfiguration)
		{
			return InstalledPlatformInfo.IsValidConfiguration(InConfiguration, EProjectType.Code);
		}

		/// <summary>
		/// Is this a valid platform. Used primarily for Installed vs non-Installed.
		/// </summary>
		/// <param name="InPlatform"></param>
		/// <returns>true if valid, false if not</returns>
		static public bool IsValidPlatform(UnrealTargetPlatform InPlatform)
		{
			return InstalledPlatformInfo.IsValidPlatform(InPlatform, EProjectType.Code);
		}

		/// <summary>
		/// Determines whether a directory is part of the engine
		/// </summary>
		/// <param name="InDirectory"></param>
		/// <returns>true if the directory is under of the engine directories, false if not</returns>
		static public bool IsUnderAnEngineDirectory(DirectoryReference InDirectory)
		{
			// Enterprise modules are considered as engine modules
			return InDirectory.IsUnderDirectory( UnrealBuildTool.EngineDirectory ) || InDirectory.IsUnderDirectory( UnrealBuildTool.EnterpriseSourceDirectory ) ||
				InDirectory.IsUnderDirectory( DirectoryReference.Combine( UnrealBuildTool.EnterpriseDirectory, "Plugins" ) );
		}

		public static void RegisterAllUBTClasses(SDKOutputLevel OutputLevel, bool bValidatingPlatforms)
		{
			// Find and register all tool chains and build platforms that are present
			Assembly UBTAssembly = Assembly.GetExecutingAssembly();
			if (UBTAssembly != null)
			{
				Log.TraceVerbose("Searching for ToolChains, BuildPlatforms, BuildDeploys and ProjectGenerators...");

				List<System.Type> ProjectGeneratorList = new List<System.Type>();
				Type[] AllTypes = UBTAssembly.GetTypes();

					// register all build platforms first, since they implement SDK-switching logic that can set environment variables
					foreach (Type CheckType in AllTypes)
					{
						if (CheckType.IsClass && !CheckType.IsAbstract)
						{
							if (CheckType.IsSubclassOf(typeof(UEBuildPlatformFactory)))
							{
								Log.TraceVerbose("    Registering build platform: {0}", CheckType.ToString());
								UEBuildPlatformFactory TempInst = (UEBuildPlatformFactory)(UBTAssembly.CreateInstance(CheckType.FullName, true));
							TempInst.TryRegisterBuildPlatforms(OutputLevel, bValidatingPlatforms);
						}
					}
				}
				// register all the other classes
				foreach (Type CheckType in AllTypes)
				{
					if (CheckType.IsClass && !CheckType.IsAbstract)
					{
						if (CheckType.IsSubclassOf(typeof(UEPlatformProjectGenerator)))
						{
							ProjectGeneratorList.Add(CheckType);
						}
					}
				}

				// We need to process the ProjectGenerators AFTER all BuildPlatforms have been 
				// registered as they use the BuildPlatform to verify they are legitimate.
				foreach (Type ProjType in ProjectGeneratorList)
				{
					Log.TraceVerbose("    Registering project generator: {0}", ProjType.ToString());
					UEPlatformProjectGenerator TempInst = (UEPlatformProjectGenerator)(UBTAssembly.CreateInstance(ProjType.FullName, true));
					TempInst.RegisterPlatformProjectGenerator();
				}
			}
		}

		private static bool ParseRocketCommandlineArg(string InArg, ref string OutGameName, ref FileReference ProjectFile)
		{
			string LowercaseArg = InArg.ToLowerInvariant();

			string ProjectArg = null;
			if (LowercaseArg.StartsWith("-project="))
			{
				ProjectArg = InArg.Substring(9).Trim(new Char[] { ' ', '"', '\'' });
			}
			else if (LowercaseArg.EndsWith(".uproject"))
			{
				ProjectArg = InArg;
			}
			else if (LowercaseArg.StartsWith("-remoteini="))
			{
				RemoteIniPath = InArg.Substring(11);
			}
			else
            {
				return false;
			}

			if (ProjectArg != null && ProjectArg.Length > 0)
			{
				string ProjectPath = Path.GetDirectoryName(ProjectArg);
				OutGameName = Path.GetFileNameWithoutExtension(ProjectArg);

				if (Path.IsPathRooted(ProjectPath) == false)
				{
					if (Directory.Exists(ProjectPath) == false)
					{
						// If it is *not* rooted, then we potentially received a path that is 
						// relative to Engine/Binaries/[PLATFORM] rather than Engine/Source
						if (ProjectPath.StartsWith("../") || ProjectPath.StartsWith("..\\"))
						{
							string AlternativeProjectpath = ProjectPath.Substring(3);
							if (Directory.Exists(AlternativeProjectpath))
							{
								ProjectPath = AlternativeProjectpath;
								ProjectArg = ProjectArg.Substring(3);
								Debug.Assert(ProjectArg.Length > 0);
							}
						}
					}
				}

				ProjectFile = new FileReference(ProjectArg);
			}

			return true;
		}

		/// <summary>
		/// UBT startup order is fairly fragile, and relies on globals that may or may not be safe to use yet.
		/// This function is for super early startup stuff that should not access Configuration classes (anything loaded by XmlConfg).
		/// This should be very minimal startup code.
		/// </summary>
		/// <param name="Arguments">Cmdline arguments</param>
		private static int GuardedMain(string[] Arguments)
		{
			DateTime StartTime = DateTime.UtcNow;
			ECompilationResult Result = ECompilationResult.Succeeded;

			// Do super early log init as a safeguard. We'll re-init with proper config options later.
			Log.InitLogging(bLogTimestamps: false, InLogLevel: LogEventType.Log, bLogSeverity: true, bLogSources: false, bLogSourcesToConsole: false, bColorConsoleOutput: true, TraceListeners: new[] { new UEConsoleTraceListener() });

			// ensure we can resolve any external assemblies that are not in the same folder as our assembly.
			AssemblyUtils.InstallAssemblyResolver(Path.GetDirectoryName(Assembly.GetEntryAssembly().GetOriginalLocation()));

			// Grab the environment.
			InitialEnvironment = Environment.GetEnvironmentVariables();
			if (InitialEnvironment.Count < 1)
			{
				throw new BuildException("Environment could not be read");
			}

			// @todo: Ideally we never need to Mutex unless we are invoked with the same target project,
			// in the same branch/path!  This would allow two clientspecs to build at the same time (even though we need
			// to make sure that XGE is only used once at a time)
			bool bUseMutex = true;
			{
				int NoMutexArgumentIndex;
				if (Utils.ParseCommandLineFlag(Arguments, "-NoMutex", out NoMutexArgumentIndex))
				{
					bUseMutex = false;
				}
			}
			bool bWaitMutex = false;
			{
				int WaitMutexArgumentIndex;
				if (Utils.ParseCommandLineFlag(Arguments, "-WaitMutex", out WaitMutexArgumentIndex))
				{
					bWaitMutex = true;
				}
			}

			bool bAutoSDKOnly = false;
			{
				int AutoSDKOnlyArgumentIndex;
				if (Utils.ParseCommandLineFlag(Arguments, "-autosdkonly", out AutoSDKOnlyArgumentIndex))
				{
					bAutoSDKOnly = true;
				}
			}
			bool bValidatePlatforms = false;
			{
				int ValidatePlatformsArgumentIndex;
				if (Utils.ParseCommandLineFlag(Arguments, "-validateplatform", out ValidatePlatformsArgumentIndex))
				{
					bValidatePlatforms = true;
				}
			}


			// Don't allow simultaneous execution of Unreal Built Tool. Multi-selection in the UI e.g. causes this and you want serial
			// execution in this case to avoid contention issues with shared produced items.
			bool bCreatedMutex = false;
			{
				Mutex SingleInstanceMutex = null;
				if (bUseMutex)
				{
					int LocationHash = Assembly.GetEntryAssembly().GetOriginalLocation().GetHashCode();

					String MutexName;
					if (bAutoSDKOnly || bValidatePlatforms)
					{
						// this mutex has to be truly global because AutoSDK installs may change global state (like a console copying files into VS directories,
						// or installing Target Management services
						MutexName = "Global\\UnrealBuildTool_Mutex_AutoSDKS";
					}
					else
					{
						MutexName = "Global\\UnrealBuildTool_Mutex_" + LocationHash.ToString();
					}
					SingleInstanceMutex = new Mutex(true, MutexName, out bCreatedMutex);
					if (!bCreatedMutex)
					{
						if (bWaitMutex)
						{
							try
							{
								SingleInstanceMutex.WaitOne();
							}
							catch (AbandonedMutexException)
							{
							}
						}
						else if (bAutoSDKOnly || bValidatePlatforms)
						{
							throw new BuildException("A conflicting instance of UnrealBuildTool is already running. Either -autosdkonly or -validateplatform was passed, which allows only a single instance to be run globally. Therefore, the conflicting instance may be in another location or the current location: {0}. A process manager may be used to determine the conflicting process and what tool may have launched it.", Assembly.GetEntryAssembly().GetOriginalLocation());
						}
						else
						{
							throw new BuildException("A conflicting instance of UnrealBuildTool is already running at this location: {0}. A process manager may be used to determine the conflicting process and what tool may have launched it.", Assembly.GetEntryAssembly().GetOriginalLocation());
						}
					}
				}

				try
				{
					// Change the working directory to be the Engine/Source folder. We are likely running from Engine/Binaries/DotNET
					// This is critical to be done early so any code that relies on the current directory being Engine/Source will work.
					// UEBuildConfiguration.PostReset is susceptible to this, so we must do this before configs are loaded.
					string EngineSourceDirectory = Path.Combine(Path.GetDirectoryName(Assembly.GetEntryAssembly().GetOriginalLocation()), "..", "..", "..", "Engine", "Source");

					//@todo.Rocket: This is a workaround for recompiling game code in editor
					// The working directory when launching is *not* what we would expect
					if (Directory.Exists(EngineSourceDirectory) == false)
					{
						// We are assuming UBT always runs from <>/Engine/Binaries/DotNET/...
						EngineSourceDirectory = Assembly.GetExecutingAssembly().GetOriginalLocation();
						EngineSourceDirectory = EngineSourceDirectory.Replace("\\", "/");
						Int32 EngineIdx = EngineSourceDirectory.IndexOf("/Engine/Binaries/DotNET/", StringComparison.InvariantCultureIgnoreCase);
						if (EngineIdx > 0)
						{
							EngineSourceDirectory = Path.Combine(EngineSourceDirectory.Substring(0, EngineIdx), "Engine", "Source");
						}
					}
					if (Directory.Exists(EngineSourceDirectory)) // only set the directory if it exists, this should only happen if we are launching the editor from an artist sync
					{
						Directory.SetCurrentDirectory(EngineSourceDirectory);
					}

					// Read the XML configuration files
					if(!XmlConfig.ReadConfigFiles())
					{
						return 1;
					}

					// Create the build configuration object, and read the settings
					BuildConfiguration BuildConfiguration = new BuildConfiguration();
					XmlConfig.ApplyTo(BuildConfiguration);
					CommandLine.ParseArguments(Arguments, BuildConfiguration);

					// Copy some of the static settings that are being deprecated from BuildConfiguration
					bPrintDebugInfo = BuildConfiguration.bPrintDebugInfo;
					bPrintPerformanceInfo = BuildConfiguration.bPrintPerformanceInfo;

					// Don't run junk deleter if we're just setting up autosdks
					if(bAutoSDKOnly || bValidatePlatforms)
					{
						BuildConfiguration.bIgnoreJunk = true;
					}

					// Then let the command lines override any configs necessary.
					foreach (string Argument in Arguments)
					{
						string LowercaseArg = Argument.ToLowerInvariant();
						if (LowercaseArg == "-verbose")
						{
							bPrintDebugInfo = true;
							BuildConfiguration.LogLevel = "Verbose";
						}
						else if (LowercaseArg.StartsWith("-log="))
						{
							BuildConfiguration.LogFilename = LowercaseArg.Replace("-log=", "");
						}
						else if (LowercaseArg == "-xgeexport")
						{
							BuildConfiguration.bXGEExport = true;
							BuildConfiguration.bAllowXGE = true;
						}
						else if (LowercaseArg.StartsWith("-singlefile="))
						{
							BuildConfiguration.bUseUBTMakefiles = false;
							BuildConfiguration.SingleFileToCompile = LowercaseArg.Replace("-singlefile=", "");
						}
						else if (LowercaseArg == "-installed" || LowercaseArg == "-installedengine")
						{
							bIsEngineInstalled = true;
						}
						else if (LowercaseArg == "-notinstalledengine")
						{
							bIsEngineInstalled = false;
						}
						else if(LowercaseArg.StartsWith("-buildconfigurationdoc="))
						{
							XmlConfig.WriteDocumentation(new FileReference(Argument.Substring("-buildconfigurationdoc=".Length)));
							return 0;
						}
						else if(LowercaseArg.StartsWith("-modulerulesdoc="))
						{
							RulesDocumentation.WriteDocumentation(typeof(ModuleRules), new FileReference(Argument.Substring("-modulerulesdoc=".Length)));
							return 0;
						}
						else if(LowercaseArg.StartsWith("-targetrulesdoc="))
						{
							RulesDocumentation.WriteDocumentation(typeof(TargetRules), new FileReference(Argument.Substring("-targetrulesdoc=".Length)));
							return 0;
						}
					}

					// If it wasn't set explicitly by a command line option, check for the installed build marker file
					if (!bIsEngineInstalled.HasValue)
					{
						bIsEngineInstalled = FileReference.Exists(FileReference.Combine(RootDirectory, "Engine", "Build", "InstalledBuild.txt"));
					}

					// We can now do a full initialization of the logging system with proper configuration values.
					Log.InitLogging(
						bLogTimestamps: false,
						InLogLevel: (LogEventType)Enum.Parse(typeof(LogEventType), BuildConfiguration.LogLevel),
						bLogSeverity: true,
						bLogSources: false,
						bLogSourcesToConsole: false,
						bColorConsoleOutput: true,
						TraceListeners: new[] 
						{
							(TraceListener)(new UEConsoleTraceListener()),
							!string.IsNullOrEmpty(BuildConfiguration.LogFilename) ? new TextWriterTraceListener(new StreamWriter(new FileStream(BuildConfiguration.LogFilename, FileMode.Create, FileAccess.ReadWrite, FileShare.Read)) { AutoFlush = true }) : null,
						});

					// Parse rocket-specific arguments.
					FileReference ProjectFile = null;
					foreach (string Arg in Arguments)
					{
						string TempGameName = null;
						if (ParseRocketCommandlineArg(Arg, ref TempGameName, ref ProjectFile) == true)
						{
							// This is to allow relative paths for the project file
							Log.TraceVerbose("UBT Running for Rocket: " + ProjectFile);
							break;
						}
					}

					// Read the project file from the installed project text file
					if(ProjectFile == null)
					{
						FileReference InstalledProjectFile = FileReference.Combine(RootDirectory, "Engine", "Build", "InstalledProjectBuild.txt"); 
						if(FileReference.Exists(InstalledProjectFile))
						{
							ProjectFile = FileReference.Combine(UnrealBuildTool.RootDirectory, File.ReadAllText(InstalledProjectFile.FullName).Trim());
						}
					}

					// Build the list of game projects that we know about. When building from the editor (for hot-reload) or for projects from installed builds, we require the 
					// project file to be passed in. Otherwise we scan for projects in directories named in UE4Games.uprojectdirs.
					if (ProjectFile != null)
					{
						UProjectInfo.AddProject(ProjectFile);
					}
					else
					{
						UProjectInfo.FillProjectInfo();
					}

					// Read the project-specific build configuration settings
					ConfigCache.ReadSettings(DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.Unknown, BuildConfiguration);

					DateTime BasicInitStartTime = DateTime.UtcNow;

					Log.TraceVerbose("UnrealBuildTool (DEBUG OUTPUT MODE)");
					Log.TraceVerbose("Command-line: {0}", String.Join(" ", Arguments));

					Telemetry.Initialize();

					string GameName = null;
					bool bSpecificModulesOnly = false;

					// We need to be able to identify if one of the arguments is the platform...
					UnrealTargetPlatform CheckPlatform = UnrealTargetPlatform.Unknown;
					foreach (string Argument in Arguments)
					{
						UnrealTargetPlatform ParsedPlatform = UEBuildPlatform.ConvertStringToPlatform(Argument);
						if (ParsedPlatform != UnrealTargetPlatform.Unknown)
						{
							CheckPlatform = ParsedPlatform;
						}
					}

					// @todo ubtmake: remove this when building with RPCUtility works
					// @todo tvos merge: Check the change to this line, not clear why. Is TVOS needed here?
					if (CheckPlatform == UnrealTargetPlatform.Mac || CheckPlatform == UnrealTargetPlatform.IOS || CheckPlatform == UnrealTargetPlatform.TVOS)
					{
						BuildConfiguration.bUseUBTMakefiles = false;
					}

					// If we were asked to enable fast build iteration, we want the 'gather' phase to default to off (unless it is overridden below
					// using a command-line option.)
					if (BuildConfiguration.bUseUBTMakefiles)
					{
						bIsGatheringBuild_Unsafe = false;
					}

					bool bGenerateProjectFiles = false;
					WindowsCompiler OverrideWindowsCompiler = WindowsCompiler.Default;
					List<ProjectFileFormat> ProjectFileFormats = new List<ProjectFileFormat>();
					foreach (string Arg in Arguments)
					{
						string LowercaseArg = Arg.ToLowerInvariant();
						if (ProjectFile == null && ParseRocketCommandlineArg(Arg, ref GameName, ref ProjectFile))
						{
							// Already handled at startup. Calling now just to properly set the game name
							continue;
						}
						else if (LowercaseArg == "-projectfiles")
						{
							bGenerateProjectFiles = true;
						}
						else if(LowercaseArg.StartsWith("-projectfileformat="))
						{
							ProjectFileFormats.AddRange(ProjectFileGeneratorSettings.ParseFormatList(Arg.Substring("-projectfileformat=".Length)));
							bGenerateProjectFiles = true;
						}
						else if (LowercaseArg == "-2012unsupported")
						{
							// May be for compiling; don't set bGenerateProjectFiles by default 
							ProjectFileFormats.Add(ProjectFileFormat.VisualStudio2012);
						}
						else if (LowercaseArg == "-2013unsupported")
						{
							// May be for compiling; don't set bGenerateProjectFiles by default 
							ProjectFileFormats.Add(ProjectFileFormat.VisualStudio2013);
						}
						else if (LowercaseArg == "-2015")
						{
							// May be for compiling; don't set bGenerateProjectFiles by default, but do override the compiler if it is.
							ProjectFileFormats.Add(ProjectFileFormat.VisualStudio2015);
							OverrideWindowsCompiler = WindowsCompiler.VisualStudio2015;
						}
						else if (LowercaseArg == "-2017")
						{
							// May be for compiling; don't set bGenerateProjectFiles by default, but do override the compiler if it is. 
							ProjectFileFormats.Add(ProjectFileFormat.VisualStudio2017);
							OverrideWindowsCompiler = WindowsCompiler.VisualStudio2017;
						}
						else if (LowercaseArg.StartsWith("-makefile"))
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.Make);
						}
						else if (LowercaseArg.StartsWith("-cmakefile"))
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.CMake);
						}
						else if (LowercaseArg.StartsWith("-qmakefile"))
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.QMake);
						}
						else if (LowercaseArg.StartsWith("-kdevelopfile"))
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.KDevelop);
						}
						else if (LowercaseArg == "-codelitefiles")
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.CodeLite);
						}
						else if (LowercaseArg == "-xcodeprojectfiles")
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.XCode);
						}
						else if (LowercaseArg == "-eddieprojectfiles")
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.Eddie);
						}
						else if (LowercaseArg == "-vscode")
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.VisualStudioCode);
						}
						else if (LowercaseArg == "development" || LowercaseArg == "debug" || LowercaseArg == "shipping" || LowercaseArg == "test" || LowercaseArg == "debuggame")
						{
							//ConfigName = Arg;
						}
						else if (LowercaseArg == "-modulewithsuffix")
						{
							bSpecificModulesOnly = true;
							continue;
						}
						else if (LowercaseArg == "-invalidatemakefilesonly")
						{
							UnrealBuildTool.bIsInvalidatingMakefilesOnly = true;
						}
						else if (LowercaseArg == "-gather")
						{
							UnrealBuildTool.bIsGatheringBuild_Unsafe = true;
						}
						else if (LowercaseArg == "-nogather")
						{
							UnrealBuildTool.bIsGatheringBuild_Unsafe = false;
						}
						else if (LowercaseArg == "-gatheronly")
						{
							UnrealBuildTool.bIsGatheringBuild_Unsafe = true;
							UnrealBuildTool.bIsAssemblingBuild_Unsafe = false;
						}
						else if (LowercaseArg == "-assemble")
						{
							UnrealBuildTool.bIsAssemblingBuild_Unsafe = true;
						}
						else if (LowercaseArg == "-noassemble")
						{
							UnrealBuildTool.bIsAssemblingBuild_Unsafe = false;
						}
						else if (LowercaseArg == "-assembleonly")
						{
							UnrealBuildTool.bIsGatheringBuild_Unsafe = false;
							UnrealBuildTool.bIsAssemblingBuild_Unsafe = true;
						}
						else if (LowercaseArg == "-ignorejunk")
						{
							BuildConfiguration.bIgnoreJunk = true;
						}
						else if (LowercaseArg == "-define")
						{
							// Skip -define
						}
						else if (LowercaseArg == "-progress")
						{
							ProgressWriter.bWriteMarkup = true;
						}
						else if (CheckPlatform.ToString().ToLowerInvariant() == LowercaseArg)
						{
							// It's the platform set...
							//PlatformName = Arg;
						}
						else if (LowercaseArg == "-vsdebugandroid")
						{
							AndroidProjectGenerator.VSDebugCommandLineOptionPresent = true;
						}
						else
						{
							// This arg may be a game name. Check for the existence of a game folder with this name.
							// "Engine" is not a valid game name.
							if (LowercaseArg != "engine" && Arg.IndexOfAny(Path.GetInvalidPathChars()) == -1 && Arg.IndexOfAny(new char[]{ ':', Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar}) == -1 &&
								DirectoryReference.Exists(DirectoryReference.Combine(RootDirectory, Arg, "Config")))
							{
								GameName = Arg;
								Log.TraceVerbose("CommandLine: Found game name '{0}'", GameName);
							}
						}
					}

					if (!bIsGatheringBuild_Unsafe && !bIsAssemblingBuild_Unsafe)
					{
						throw new BuildException("UnrealBuildTool: At least one of either IsGatheringBuild or IsAssemblingBuild must be true.  Did you pass '-NoGather' with '-NoAssemble'?");
					}

					// Find and register all tool chains, build platforms, etc. that are present
					SDKOutputLevel OutputLevel;
					if(UnrealBuildTool.bPrintDebugInfo)
					{
						OutputLevel = SDKOutputLevel.Verbose;
					}
					else if(Environment.GetEnvironmentVariable("IsBuildMachine") == "1")
					{
						OutputLevel = SDKOutputLevel.Minimal;
					}
					else
					{
						OutputLevel = SDKOutputLevel.Quiet;
					}
					RegisterAllUBTClasses(OutputLevel, bValidatePlatforms);

					if (UnrealBuildTool.bPrintPerformanceInfo)
					{
						double BasicInitTime = (DateTime.UtcNow - BasicInitStartTime).TotalSeconds;
						Log.TraceInformation("Basic UBT initialization took " + BasicInitTime + "s");
					}

					// now that we know the available platforms, we can delete other platforms' junk. if we're only building specific modules from the editor, don't touch anything else (it may be in use).
					if (!bSpecificModulesOnly && !BuildConfiguration.bIgnoreJunk)
					{
						JunkDeleter.DeleteJunk();
					}

					if (bGenerateProjectFiles)
					{
						// If there aren't any formats set, read the default project file format from the config file
						if(ProjectFileFormats.Count == 0)
						{
							// Read from the XML config
							if(!String.IsNullOrEmpty(ProjectFileGeneratorSettings.Format))
							{
								ProjectFileFormats.AddRange(ProjectFileGeneratorSettings.ParseFormatList(ProjectFileGeneratorSettings.Format));
							}

							// Read from the editor config
							DirectoryReference EngineSavedDir = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Saved");
							if(IsEngineInstalled())
							{
								BuildVersion Version;
								if(BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
								{
									EngineSavedDir = DirectoryReference.Combine(Utils.GetUserSettingDirectory(), "UnrealEngine", String.Format("{0}.{1}", Version.MajorVersion, Version.MinorVersion), "Saved");
								}
							}
							ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.EditorSettings, DirectoryReference.FromFile(ProjectFile), BuildHostPlatform.Current.Platform, EngineSavedDir);

							string PreferredAccessor;
							if (Ini.GetString("/Script/SourceCodeAccess.SourceCodeAccessSettings", "PreferredAccessor", out PreferredAccessor))
							{
								ProjectFileFormat PreferredFormat;
								if (Enum.TryParse(PreferredAccessor, out PreferredFormat))
								{
									ProjectFileFormats.Add(PreferredFormat);
								}
							}

							// If there's still nothing set, get the default project file format for this platform
							if(ProjectFileFormats.Count == 0)
							{
								BuildHostPlatform.Current.GetDefaultProjectFileFormats(ProjectFileFormats);
							}
						}

						// Create each project generator and run it
						ProjectFileGenerator.bGenerateProjectFiles = true;
						foreach(ProjectFileFormat ProjectFileFormat in ProjectFileFormats.Distinct())
						{
							ProjectFileGenerator Generator;
							switch (ProjectFileFormat)
							{
								case ProjectFileFormat.Make:
									Generator = new MakefileGenerator(ProjectFile);
									break;
								case ProjectFileFormat.CMake:
									Generator = new CMakefileGenerator(ProjectFile);
									break;
								case ProjectFileFormat.QMake:
									Generator = new QMakefileGenerator(ProjectFile);
									break;
								case ProjectFileFormat.KDevelop:
									Generator = new KDevelopGenerator(ProjectFile);
									break;
								case ProjectFileFormat.CodeLite:
									Generator = new CodeLiteGenerator(ProjectFile);
									break;
								case ProjectFileFormat.VisualStudio:
									Generator = new VCProjectFileGenerator(ProjectFile, VCProjectFileFormat.Default, OverrideWindowsCompiler);
									break;
								case ProjectFileFormat.VisualStudio2012:
									Generator = new VCProjectFileGenerator(ProjectFile, VCProjectFileFormat.VisualStudio2012, OverrideWindowsCompiler);
									break;
								case ProjectFileFormat.VisualStudio2013:
									Generator = new VCProjectFileGenerator(ProjectFile, VCProjectFileFormat.VisualStudio2013, OverrideWindowsCompiler);
									break;
								case ProjectFileFormat.VisualStudio2015:
									Generator = new VCProjectFileGenerator(ProjectFile, VCProjectFileFormat.VisualStudio2015, OverrideWindowsCompiler);
									break;
								case ProjectFileFormat.VisualStudio2017:
									Generator = new VCProjectFileGenerator(ProjectFile, VCProjectFileFormat.VisualStudio2017, OverrideWindowsCompiler);
									break;
								case ProjectFileFormat.XCode:
									Generator = new XcodeProjectFileGenerator(ProjectFile);
									break;
								case ProjectFileFormat.Eddie:
									Generator = new EddieProjectFileGenerator(ProjectFile);
									break;
								case ProjectFileFormat.VisualStudioCode:
									Generator = new VSCodeProjectFileGenerator(ProjectFile);
									break;
								default:
									throw new BuildException("Unhandled project file type '{0}", ProjectFileFormat);
							}
							if(!Generator.GenerateProjectFiles(Arguments))
							{
								Result = ECompilationResult.OtherCompilationError;
							}
						}
					}
					else if (bAutoSDKOnly)
					{
						// RegisterAllUBTClasses has already done all the SDK setup.
						Result = ECompilationResult.Succeeded;
					}
					else if (bValidatePlatforms)
					{
						ValidatePlatforms(Arguments);
					}
					else if (BuildConfiguration.DeployTargetFile != null)
					{
						UEBuildDeployTarget DeployTarget = new UEBuildDeployTarget(BuildConfiguration.DeployTargetFile);
						Log.WriteLine(LogEventType.Console, "Deploying {0} {1} {2}...", DeployTarget.TargetName, DeployTarget.Platform, DeployTarget.Configuration);
						UEBuildPlatform.GetBuildPlatform(DeployTarget.Platform).Deploy(DeployTarget);
						Result = ECompilationResult.Succeeded;
					}
					else
					{
						// Build our project
						if (Result == ECompilationResult.Succeeded)
						{
							Result = RunUBT(BuildConfiguration, Arguments, ProjectFile);
						}
					}
					// Print some performance info
					double BuildDuration = (DateTime.UtcNow - StartTime).TotalSeconds;
					if (UnrealBuildTool.bPrintPerformanceInfo)
					{
						Log.TraceInformation("GetIncludes time: " + CPPHeaders.TotalTimeSpentGettingIncludes + "s (" + CPPHeaders.TotalIncludesRequested + " includes)");
						Log.TraceInformation("DirectIncludes cache miss time: " + CPPHeaders.DirectIncludeCacheMissesTotalTime + "s (" + CPPHeaders.TotalDirectIncludeCacheMisses + " misses)");
						Log.TraceInformation("FindIncludePaths calls: " + CPPHeaders.TotalFindIncludedFileCalls + " (" + CPPHeaders.IncludePathSearchAttempts + " searches)");
						Log.TraceInformation("PCH gen time: " + UEBuildModuleCPP.TotalPCHGenTime + "s");
						Log.TraceInformation("PCH cache time: " + UEBuildModuleCPP.TotalPCHCacheTime + "s");
						Log.TraceInformation("Deep C++ include scan time: " + UnrealBuildTool.TotalDeepIncludeScanTime + "s");
						Log.TraceInformation("Include Resolves: {0} ({1} misses, {2:0.00}%)", CPPHeaders.TotalDirectIncludeResolves, CPPHeaders.TotalDirectIncludeResolveCacheMisses, (float)CPPHeaders.TotalDirectIncludeResolveCacheMisses / (float)CPPHeaders.TotalDirectIncludeResolves * 100);
						Log.TraceInformation("Total FileItems: {0} ({1} missing)", FileItem.TotalFileItemCount, FileItem.MissingFileItemCount);

						Log.TraceInformation("Execution time: {0}s", BuildDuration);
					}
				}
				catch (Exception Exception)
				{
					Log.TraceError("UnrealBuildTool Exception: " + Exception);
					Result = ECompilationResult.OtherCompilationError;
				}

				if (bUseMutex)
				{
					// Release the mutex to avoid the abandoned mutex timeout.
					SingleInstanceMutex.ReleaseMutex();
					SingleInstanceMutex.Dispose();
					SingleInstanceMutex = null;
				}
			}

			Telemetry.Shutdown();

			// Print some performance info
			Log.TraceVerbose("Execution time: {0}", (DateTime.UtcNow - StartTime).TotalSeconds);

			if (ExtendedErrorCode != 0)
			{
				return ExtendedErrorCode;
			}
			return (int)Result;
		}

		public static int ExtendedErrorCode = 0;
		private static int Main(string[] Arguments)
		{
            // make sure we catch any exceptions and return an appropriate error code.
            // Some inner code already does this (to ensure the Mutex is released),
            // but we need something to cover all outer code as well.
			try
			{
				return GuardedMain(Arguments);
			}
			catch (Exception Exception)
			{
				if (Log.IsInitialized())
				{
					Log.TraceError("UnrealBuildTool Exception: " + Exception.ToString());
				}
				if (ExtendedErrorCode != 0)
				{
					return ExtendedErrorCode;
				}
				return (int)ECompilationResult.OtherCompilationError;
			}
		}

		/// <summary>
		/// Generates project files.  Can also be used to generate projects "in memory", to allow for building
		/// targets without even having project files at all.
		/// </summary>
		/// <param name="Generator">Project generator to use</param>
		/// <param name="Arguments">Command-line arguments</param>
		/// <returns>True if successful</returns>
		internal static bool GenerateProjectFiles(ProjectFileGenerator Generator, string[] Arguments)
		{
			ProjectFileGenerator.bGenerateProjectFiles = true;
			bool bSuccess = Generator.GenerateProjectFiles(Arguments);
			ProjectFileGenerator.bGenerateProjectFiles = false;
			return bSuccess;
		}



		/// <summary>
		/// Validates the various platforms to determine if they are ready for building
		/// </summary>
		public static void ValidatePlatforms(string[] Arguments)
		{
			List<UnrealTargetPlatform> Platforms = new List<UnrealTargetPlatform>();
			foreach (string CurArgument in Arguments)
			{
				if (CurArgument.StartsWith("-"))
				{
					if (CurArgument.StartsWith("-Platforms=", StringComparison.InvariantCultureIgnoreCase))
					{
						// Parse the list... will be in Foo+Bar+New format
						string PlatformList = CurArgument.Substring(11);
						while (PlatformList.Length > 0)
						{
							string PlatformString = PlatformList;
							Int32 PlusIdx = PlatformList.IndexOf("+");
							if (PlusIdx != -1)
							{
								PlatformString = PlatformList.Substring(0, PlusIdx);
								PlatformList = PlatformList.Substring(PlusIdx + 1);
							}
							else
							{
								// We are on the last platform... clear the list to exit the loop
								PlatformList = "";
							}

							// Is the string a valid platform? If so, add it to the list
							UnrealTargetPlatform SpecifiedPlatform = UnrealTargetPlatform.Unknown;
							foreach (UnrealTargetPlatform PlatformParam in Enum.GetValues(typeof(UnrealTargetPlatform)))
							{
								if (PlatformString.Equals(PlatformParam.ToString(), StringComparison.InvariantCultureIgnoreCase))
								{
									SpecifiedPlatform = PlatformParam;
									break;
								}
							}

							if (SpecifiedPlatform != UnrealTargetPlatform.Unknown)
							{
								if (Platforms.Contains(SpecifiedPlatform) == false)
								{
									Platforms.Add(SpecifiedPlatform);
								}
							}
							else
							{
								Log.TraceWarning("ValidatePlatforms invalid platform specified: {0}", PlatformString);
							}
						}
					}
					else switch (CurArgument.ToUpperInvariant())
						{
							case "-ALLPLATFORMS":
								foreach (UnrealTargetPlatform platform in Enum.GetValues(typeof(UnrealTargetPlatform)))
								{
									if (platform != UnrealTargetPlatform.Unknown)
									{
										if (Platforms.Contains(platform) == false)
										{
											Platforms.Add(platform);
										}
									}
								}
								break;
						}
				}
			}

			foreach (UnrealTargetPlatform platform in Platforms)
			{
				UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(platform, true);
				if (BuildPlatform != null && BuildPlatform.HasRequiredSDKsInstalled() == SDKStatus.Valid)
				{
					Console.WriteLine("##PlatformValidate: {0} VALID", BuildPlatform.GetPlatformValidationName());
				}
				else
				{
					Console.WriteLine("##PlatformValidate: {0} INVALID", BuildPlatform != null ? BuildPlatform.GetPlatformValidationName() : platform.ToString());
				}
			}
		}

		internal static ECompilationResult RunUBT(BuildConfiguration BuildConfiguration, string[] Arguments, FileReference ProjectFile)
		{
			bool bSuccess = true;

			DateTime RunUBTInitStartTime = DateTime.UtcNow;


			// Reset global configurations
			ActionGraph ActionGraph = new ActionGraph();

			string ExecutorName = "Unknown";
			ECompilationResult BuildResult = ECompilationResult.Succeeded;

			Thread CPPIncludesThread = null;

			List<UEBuildTarget> Targets = null;
			Dictionary<UEBuildTarget, CPPHeaders> TargetToHeaders = new Dictionary<UEBuildTarget, CPPHeaders>();

			double TotalExecutorTime = 0.0;

			try
			{
				List<string[]> TargetSettings = ParseCommandLineFlags(Arguments);

				int ArgumentIndex;
				// action graph implies using the dependency resolve cache
				bool GeneratingActionGraph = Utils.ParseCommandLineFlag(Arguments, "-graph", out ArgumentIndex);
				if (GeneratingActionGraph)
				{
					BuildConfiguration.bUseIncludeDependencyResolveCache = true;
				}

				if (UnrealBuildTool.bPrintPerformanceInfo)
				{
					double RunUBTInitTime = (DateTime.UtcNow - RunUBTInitStartTime).TotalSeconds;
					Log.TraceInformation("RunUBT initialization took " + RunUBTInitTime + "s");
				}

				List<TargetDescriptor> TargetDescs = new List<TargetDescriptor>();
				{
					DateTime TargetDescstStartTime = DateTime.UtcNow;

					foreach (string[] TargetSetting in TargetSettings)
					{
						TargetDescs.AddRange(UEBuildTarget.ParseTargetCommandLine(TargetSetting, ref ProjectFile));
					}

					if (UnrealBuildTool.bPrintPerformanceInfo)
					{
						double TargetDescsTime = (DateTime.UtcNow - TargetDescstStartTime).TotalSeconds;
						Log.TraceInformation("Target descriptors took " + TargetDescsTime + "s");
					}
				}

				if (UnrealBuildTool.bIsInvalidatingMakefilesOnly)
				{
					Log.TraceInformation("Invalidating makefiles only in this run.");
					if (TargetDescs.Count != 1)
					{
						Log.TraceError("You have to provide one target name for makefile invalidation.");
						return ECompilationResult.OtherCompilationError;
					}

					InvalidateMakefiles(TargetDescs[0]);
					return ECompilationResult.Succeeded;
				}

				EHotReload HotReload = EHotReload.Disabled;
				if (TargetDescs.Count == 1 && !Arguments.Any(x => x.Equals("-NoHotReload", StringComparison.InvariantCultureIgnoreCase)))
				{
					if (BuildConfiguration.bAllowHotReloadFromIDE && !TargetDescs[0].bIsEditorRecompile && ShouldDoHotReloadFromIDE(BuildConfiguration, Arguments, TargetDescs[0]))
					{
						HotReload = EHotReload.FromIDE;
					}
					else if (TargetDescs[0].OnlyModules.Count > 0 && TargetDescs[0].ForeignPlugins.Count == 0)
					{
						HotReload = EHotReload.FromEditor;
					}
				}
				TargetDescriptor HotReloadTargetDesc = (HotReload != EHotReload.Disabled) ? TargetDescs[0] : null;

				if (ProjectFileGenerator.bGenerateProjectFiles)
				{
					// Create empty timestamp file to record when was the last time we regenerated projects.
					Directory.CreateDirectory(Path.GetDirectoryName(ProjectFileGenerator.ProjectTimestampFile));
					File.Create(ProjectFileGenerator.ProjectTimestampFile).Dispose();
				}

				if (!ProjectFileGenerator.bGenerateProjectFiles)
				{
					if (BuildConfiguration.bUseUBTMakefiles)
					{
						// Only the modular editor and game targets will share build products.  Unfortunately, we can't determine at
						// at this point whether we're dealing with modular or monolithic game binaries, so we opt to always invalidate
						// cached includes if the target we're switching to is either a game target (has project file) or "UE4Editor".
						bool bMightHaveSharedBuildProducts = 
							ProjectFile != null ||	// Is this a game? (has a .uproject file for the target)
							TargetDescs[ 0 ].TargetName.Equals( "UE4Editor", StringComparison.InvariantCultureIgnoreCase );	// Is the engine?
						if( bMightHaveSharedBuildProducts )
						{
							bool bIsBuildingSameTargetsAsLastTime = false;

							string TargetCollectionName = MakeTargetCollectionName(TargetDescs);

							string LastBuiltTargetsFileName = (HotReload != EHotReload.Disabled) ? "HotReloadLastBuiltTargets.txt" : "LastBuiltTargets.txt";
							string LastBuiltTargetsFilePath = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", LastBuiltTargetsFileName).FullName;
							if (File.Exists(LastBuiltTargetsFilePath) && Utils.ReadAllText(LastBuiltTargetsFilePath) == TargetCollectionName)
							{
								// @todo ubtmake: Because we're using separate files for hot reload vs. full compiles, it's actually possible that includes will
								// become out of date without us knowing if the developer ping-pongs between hot reloading target A and building target B normally.
								// To fix this we can not use a different file name for last built targets, but the downside is slower performance when
								// performing the first hot reload after compiling normally (forces full include dependency scan)
								bIsBuildingSameTargetsAsLastTime = true;
							}

							// Save out the name of the targets we're building
							if (!bIsBuildingSameTargetsAsLastTime)
							{
								Directory.CreateDirectory(Path.GetDirectoryName(LastBuiltTargetsFilePath));
								File.WriteAllText(LastBuiltTargetsFilePath, TargetCollectionName, Encoding.UTF8);

								// Can't use super fast include checking unless we're building the same set of targets as last time, because
								// we might not know about all of the C++ include dependencies for already-up-to-date shared build products
								// between the targets
								bNeedsFullCPPIncludeRescan = true;
								Log.TraceInformation("Performing full C++ include scan ({0} a new target)", (HotReload != EHotReload.Disabled) ? "hot reloading" : "building");
							}
						}
					}
				}

				using (ISourceFileWorkingSet WorkingSet = SourceFileWorkingSet.Create(UnrealBuildTool.RootDirectory, DirectoryReference.FromFile(ProjectFile)))
				{
				UBTMakefile UBTMakefile = null;
				{
					// If we're generating project files, then go ahead and wipe out the existing UBTMakefile for every target, to make sure that
					// it gets a full dependency scan next time.
					// NOTE: This is just a safeguard and doesn't have to be perfect.  We also check for newer project file timestamps in LoadUBTMakefile()
					FileReference UBTMakefilePath = UnrealBuildTool.GetUBTMakefilePath(TargetDescs, HotReload);
					if (ProjectFileGenerator.bGenerateProjectFiles)	// @todo ubtmake: This is only hit when generating IntelliSense for project files.  Probably should be done right inside ProjectFileGenerator.bat
					{													// @todo ubtmake: Won't catch multi-target cases as GPF always builds one target at a time for Intellisense
						// Delete the UBTMakefile
						if (FileReference.Exists(UBTMakefilePath))
						{
								FileReference.Delete(UBTMakefilePath);
						}
					}

					// Make sure the gather phase is executed if we're not actually building anything
					if (ProjectFileGenerator.bGenerateProjectFiles || BuildConfiguration.bGenerateManifest || BuildConfiguration.bCleanProject || BuildConfiguration.bXGEExport || GeneratingActionGraph)
					{
						UnrealBuildTool.bIsGatheringBuild_Unsafe = true;
					}

					// Were we asked to run in 'assembler only' mode?  If so, let's check to see if that's even possible by seeing if
					// we have a valid UBTMakefile already saved to disk, ready for us to load.
					if (UnrealBuildTool.bIsAssemblingBuild_Unsafe && !UnrealBuildTool.bIsGatheringBuild_Unsafe)
					{
						// @todo ubtmake: Mildly terrified of BuildConfiguration/UEBuildConfiguration globals that were set during the Prepare phase but are not set now.  We may need to save/load all of these, otherwise
						//		we'll need to call SetupGlobalEnvironment on all of the targets (maybe other stuff, too.  See PreBuildStep())

						// Try to load the UBTMakefile.  It will only be loaded if it has valid content and is not determined to be out of date.    
						string ReasonNotLoaded;
							UBTMakefile = LoadUBTMakefile(UBTMakefilePath, ProjectFile, WorkingSet, out ReasonNotLoaded);

						// Invalid makefile if only modules have changed
						if (UBTMakefile != null && !TargetDescs.SelectMany(x => x.OnlyModules)
								.Select(x => new Tuple<string, bool>(x.OnlyModuleName.ToLower(), string.IsNullOrWhiteSpace(x.OnlyModuleSuffix)))
								.SequenceEqual(
									UBTMakefile.Targets.SelectMany(x => x.OnlyModules)
									.Select(x => new Tuple<string, bool>(x.OnlyModuleName.ToLower(), string.IsNullOrWhiteSpace(x.OnlyModuleSuffix)))
								)
							)
						{
							UBTMakefile = null;
							ReasonNotLoaded = "modules to compile have changed";
						}

						// Invalid makefile if foreign plugins have changed
						if (UBTMakefile != null && !TargetDescs.SelectMany(x => x.ForeignPlugins)
								.Select(x => new Tuple<string, bool>(x.FullName.ToLower(), string.IsNullOrWhiteSpace(x.FullName)))
								.SequenceEqual(
									UBTMakefile.Targets.SelectMany(x => x.ForeignPlugins)
									.Select(x => new Tuple<string, bool>(x.FullName.ToLower(), string.IsNullOrWhiteSpace(x.FullName)))
								)
							)
						{
							UBTMakefile = null;
							ReasonNotLoaded = "foreign plugins have changed";
						}

						if (UBTMakefile != null)
						{
							// Check if ini files are newer. Ini files contain build settings too.
							FileInfo UBTMakefileInfo = new FileInfo(UBTMakefilePath.FullName);
							foreach (TargetDescriptor Desc in TargetDescs)
							{
								DirectoryReference ProjectDirectory = DirectoryReference.FromFile(ProjectFile);
									foreach (ConfigHierarchyType IniType in (ConfigHierarchyType[])Enum.GetValues(typeof(ConfigHierarchyType)))
								{
									foreach (FileReference IniFilename in ConfigHierarchy.EnumerateConfigFileLocations(IniType, ProjectDirectory, Desc.Platform))
									{
										FileInfo IniFileInfo = new FileInfo(IniFilename.FullName);
										if (UBTMakefileInfo.LastWriteTime.CompareTo(IniFileInfo.LastWriteTime) < 0)
										{
											// Ini files are newer than UBTMakefile
											UBTMakefile = null;
											ReasonNotLoaded = "ini files are newer than UBTMakefile";
											break;
										}
									}

									if (UBTMakefile == null)
									{
										break;
									}
								}
								if (UBTMakefile == null)
								{
									break;
								}
							}
						}

						if (UBTMakefile == null)
						{
							// If the Makefile couldn't be loaded, then we're not going to be able to continue in "assembler only" mode.  We'll do both
							// a 'gather' and 'assemble' in the same run.  This will take a while longer, but subsequent runs will be fast!
							UnrealBuildTool.bIsGatheringBuild_Unsafe = true;

							FileItem.ClearCaches();

							Log.TraceInformation("Creating makefile for {0}{1}{2} ({3})",
								(HotReload != EHotReload.Disabled) ? "hot reloading " : "",
								TargetDescs[0].TargetName,
								TargetDescs.Count > 1 ? (" (and " + (TargetDescs.Count - 1).ToString() + " more)") : "",
								ReasonNotLoaded);
						}
					}

					// OK, after this point it is safe to access the UnrealBuildTool.IsGatheringBuild and UnrealBuildTool.IsAssemblingBuild properties.
					// These properties will not be changing again during this session/
					bIsSafeToCheckIfGatheringOrAssemblingBuild = true;
				}


				if (UBTMakefile != null && !IsGatheringBuild && IsAssemblingBuild)
				{
					// If we've loaded a makefile, then we can fill target information from this file!
					Targets = UBTMakefile.Targets;
				}
				else
				{
					DateTime TargetInitStartTime = DateTime.UtcNow;

					Targets = new List<UEBuildTarget>();
					foreach (TargetDescriptor TargetDesc in TargetDescs)
					{
						UEBuildTarget Target = UEBuildTarget.CreateTarget(TargetDesc, Arguments, BuildConfiguration.SingleFileToCompile != null);
						if ((Target == null) && (BuildConfiguration.bCleanProject))
						{
							continue;
						}
						Targets.Add(Target);
					}

					if (UnrealBuildTool.bPrintPerformanceInfo)
					{
						double TargetInitTime = (DateTime.UtcNow - TargetInitStartTime).TotalSeconds;
						Log.TraceInformation("Target init took " + TargetInitTime + "s");
					}
				}

				// Build action lists for all passed in targets.
				List<FileItem> OutputItemsForAllTargets = new List<FileItem>();
				Dictionary<string, List<UHTModuleInfo>> TargetNameToUObjectModules = new Dictionary<string, List<UHTModuleInfo>>(StringComparer.InvariantCultureIgnoreCase);
				foreach (UEBuildTarget Target in Targets)
				{
						if (HotReload != EHotReload.Disabled)
						{
						// Don't produce new DLLs if there's been no code changes
						BuildConfiguration.bSkipLinkingWhenNothingToCompile = true;
						Log.TraceInformation("Compiling game modules for hot reload");
					}

					// Create the header cache for this target
					FileReference DependencyCacheFile = DependencyCache.GetDependencyCachePathForTarget(ProjectFile, Target.Platform, Target.TargetName);
					bool bUseFlatCPPIncludeDependencyCache = BuildConfiguration.bUseUBTMakefiles && UnrealBuildTool.IsAssemblingBuild;
					CPPHeaders Headers = new CPPHeaders(ProjectFile, DependencyCacheFile, bUseFlatCPPIncludeDependencyCache, BuildConfiguration.bUseUBTMakefiles, BuildConfiguration.bUseIncludeDependencyResolveCache, BuildConfiguration.bTestIncludeDependencyResolveCache);
					TargetToHeaders[Target] = Headers;

					// Make sure the appropriate executor is selected
					UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Target.Platform);
					BuildConfiguration.bAllowXGE &= BuildPlatform.CanUseXGE();
					BuildConfiguration.bAllowDistcc &= BuildPlatform.CanUseDistcc();
					BuildConfiguration.bAllowSNDBS &= BuildPlatform.CanUseSNDBS();

					// When in 'assembler only' mode, we'll load this cache later on a worker thread.  It takes a long time to load!
					if (!(!UnrealBuildTool.IsGatheringBuild && UnrealBuildTool.IsAssemblingBuild))
					{
						// Load the direct include dependency cache.
						Headers.IncludeDependencyCache = DependencyCache.Create(DependencyCache.GetDependencyCachePathForTarget(Target.ProjectFile, Target.Platform, Target.GetTargetName()));
					}

					// We don't need this dependency cache in 'gather only' mode
					if (BuildConfiguration.bUseUBTMakefiles &&
						!(UnrealBuildTool.IsGatheringBuild && !UnrealBuildTool.IsAssemblingBuild))
					{
						// Load the cache that contains the list of flattened resolved includes for resolved source files
						// @todo ubtmake: Ideally load this asynchronously at startup and only block when it is first needed and not finished loading
						FileReference CacheFile = FlatCPPIncludeDependencyCache.GetDependencyCachePathForTarget(Target);
							if (!FlatCPPIncludeDependencyCache.TryRead(CacheFile, out Headers.FlatCPPIncludeDependencyCache))
						{
								if (!UnrealBuildTool.bNeedsFullCPPIncludeRescan)
							{
									if (!ProjectFileGenerator.bGenerateProjectFiles && !BuildConfiguration.bXGEExport && !BuildConfiguration.bGenerateManifest && !BuildConfiguration.bCleanProject)
								{
									UnrealBuildTool.bNeedsFullCPPIncludeRescan = true;
									Log.TraceInformation("Performing full C++ include scan (no include cache file)");
								}
							}
							Headers.FlatCPPIncludeDependencyCache = new FlatCPPIncludeDependencyCache(CacheFile);
						}
					}

					if (UnrealBuildTool.IsGatheringBuild)
					{
						List<FileItem> TargetOutputItems = new List<FileItem>();
						List<UHTModuleInfo> TargetUObjectModules = new List<UHTModuleInfo>();
						if (BuildConfiguration.bCleanProject)
						{
							BuildResult = Target.Clean(!BuildConfiguration.bDoNotBuildUHT && HotReload != EHotReload.FromIDE);
						}
						else
						{
							BuildResult = Target.Build(BuildConfiguration, TargetToHeaders[Target], TargetOutputItems, TargetUObjectModules, WorkingSet, ActionGraph, HotReload);
						}
						if (BuildResult != ECompilationResult.Succeeded)
						{
							break;
						}

						// Export a JSON dump of this target
						if (BuildConfiguration.JsonExportFile != null)
						{
							Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(BuildConfiguration.JsonExportFile)));
							Target.ExportJson(BuildConfiguration.JsonExportFile);
						}

						OutputItemsForAllTargets.AddRange(TargetOutputItems);

						// Update mapping of the target name to the list of UObject modules in this target
						TargetNameToUObjectModules[Target.GetTargetName()] = TargetUObjectModules;

						if ((BuildConfiguration.bXGEExport && BuildConfiguration.bGenerateManifest) || (!ProjectFileGenerator.bGenerateProjectFiles && !BuildConfiguration.bGenerateManifest && !BuildConfiguration.bCleanProject))
						{
							// Generate an action graph if we were asked to do that.  The graph generation needs access to the include dependency cache, so
							// we generate it before saving and cleaning that up.
							if (GeneratingActionGraph)
							{
								// The graph generation feature currently only works with a single target at a time.  This is because of how we need access
								// to include dependencies for the target, but those aren't kept around as we process multiple targets
								if (TargetSettings.Count != 1)
								{
									throw new BuildException("ERROR: The '-graph' option only works with a single target at a time");
								}
								ActionGraph.FinalizeActionGraph();
								List<Action> ActionsToExecute = ActionGraph.AllActions;

								ActionGraph.ActionGraphVisualizationType VisualizationType = ActionGraph.ActionGraphVisualizationType.OnlyCPlusPlusFilesAndHeaders;
								ActionGraph.SaveActionGraphVisualization(TargetToHeaders[Target], FileReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", Target.GetTargetName() + ".gexf").FullName, Target.GetTargetName(), VisualizationType, ActionsToExecute);
							}
						}
					}
				}

				if (BuildResult == ECompilationResult.Succeeded && !BuildConfiguration.bSkipBuild &&
					(
						(BuildConfiguration.bXGEExport && BuildConfiguration.bGenerateManifest) ||
						(!GeneratingActionGraph && !ProjectFileGenerator.bGenerateProjectFiles && !BuildConfiguration.bGenerateManifest && !BuildConfiguration.bCleanProject)
					))
				{
					if (UnrealBuildTool.IsGatheringBuild)
					{
						ActionGraph.FinalizeActionGraph();

						UBTMakefile = new UBTMakefile();
						UBTMakefile.AllActions = ActionGraph.AllActions;

						// For now simply treat all object files as the root target.
						{
							HashSet<Action> PrerequisiteActionsSet = new HashSet<Action>();
							foreach (FileItem OutputItem in OutputItemsForAllTargets)
							{
								ActionGraph.GatherPrerequisiteActions(OutputItem, ref PrerequisiteActionsSet);
							}
							UBTMakefile.PrerequisiteActions = PrerequisiteActionsSet.ToArray();
						}

						foreach (System.Collections.DictionaryEntry EnvironmentVariable in Environment.GetEnvironmentVariables())
						{
							UBTMakefile.EnvironmentVariables.Add(Tuple.Create((string)EnvironmentVariable.Key, (string)EnvironmentVariable.Value));
						}
						UBTMakefile.TargetNameToUObjectModules = TargetNameToUObjectModules;
						UBTMakefile.Targets = Targets;
						UBTMakefile.bUseAdaptiveUnityBuild = Targets.Any(x => x.Rules.bUseAdaptiveUnityBuild);
						UBTMakefile.SourceFileWorkingSet = Unity.SourceFileWorkingSet;
						UBTMakefile.CandidateSourceFilesForWorkingSet = Unity.CandidateSourceFilesForWorkingSet;

						if (BuildConfiguration.bUseUBTMakefiles && !UBTMakefile.PrerequisiteActions.Any(x => x.ActionHandler != null))
						{
							// We've been told to prepare to build, so let's go ahead and save out our action graph so that we can use in a later invocation 
							// to assemble the build.  Even if we are configured to assemble the build in this same invocation, we want to save out the
							// Makefile so that it can be used on subsequent 'assemble only' runs, for the fastest possible iteration times
							// @todo ubtmake: Optimization: We could make 'gather + assemble' mode slightly faster by saving this while busy compiling (on our worker thread)
							SaveUBTMakefile(TargetDescs, HotReload, UBTMakefile);
						}
					}

					if (UnrealBuildTool.IsAssemblingBuild)
					{
						// If we didn't build the graph in this session, then we'll need to load a cached one
						if (!UnrealBuildTool.IsGatheringBuild && BuildResult.Succeeded())
						{
							ActionGraph.AllActions = UBTMakefile.AllActions;

							// Patch action history for hot reload when running in assembler mode.  In assembler mode, the suffix on the output file will be
							// the same for every invocation on that makefile, but we need a new suffix each time.
							if (HotReload != EHotReload.Disabled)
							{
								PatchActionHistoryForHotReloadAssembling(ActionGraph, HotReloadTargetDesc.OnlyModules, Targets);
							}


							foreach (Tuple<string, string> EnvironmentVariable in UBTMakefile.EnvironmentVariables)
							{
								// @todo ubtmake: There may be some variables we do NOT want to clobber.
								Environment.SetEnvironmentVariable(EnvironmentVariable.Item1, EnvironmentVariable.Item2);
							}

							// Execute all the pre-build steps
							if(!ProjectFileGenerator.bGenerateProjectFiles && !BuildConfiguration.bXGEExport)
							{
									foreach (UEBuildTarget Target in Targets)
								{
										if (!Target.ExecuteCustomPreBuildSteps())
									{
										BuildResult = ECompilationResult.OtherCompilationError;
										break;
									}
								}
							}

							// If any of the targets need UHT to be run, we'll go ahead and do that now
							if (BuildResult.Succeeded())
							{
								foreach (UEBuildTarget Target in Targets)
								{
									List<UHTModuleInfo> TargetUObjectModules;
									if (UBTMakefile.TargetNameToUObjectModules.TryGetValue(Target.GetTargetName(), out TargetUObjectModules))
									{
										if (TargetUObjectModules.Count > 0)
										{
											// Execute the header tool
											FileReference ModuleInfoFileName = FileReference.Combine(Target.ProjectIntermediateDirectory, Target.GetTargetName() + ".uhtmanifest");
											ECompilationResult UHTResult = ECompilationResult.OtherCompilationError;
											if (!ExternalExecution.ExecuteHeaderToolIfNecessary(BuildConfiguration, Target, GlobalCompileEnvironment: null, UObjectModules: TargetUObjectModules, ModuleInfoFileName: ModuleInfoFileName, UHTResult: ref UHTResult, HotReload: HotReload))
											{
												Log.TraceInformation("UnrealHeaderTool failed for target '" + Target.GetTargetName() + "' (platform: " + Target.Platform.ToString() + ", module info: " + ModuleInfoFileName + ").");
												BuildResult = UHTResult;
												break;
											}
										}
									}
								}
							}
						}

						if (BuildResult.Succeeded())
						{
							// Make sure any old DLL files from in-engine recompiles aren't lying around.  Must be called after the action graph is finalized.
							ActionGraph.DeleteStaleHotReloadDLLs();

								foreach (UEBuildTarget Target in Targets)
								{
								UEBuildPlatform.GetBuildPlatform(Target.Platform).PreBuildSync();
							}

                            // Plan the actions to execute for the build.
                            Dictionary<UEBuildTarget, List<FileItem>> TargetToOutdatedPrerequisitesMap;
                            List<Action> ActionsToExecute = ActionGraph.GetActionsToExecute(BuildConfiguration, UBTMakefile.PrerequisiteActions, Targets, TargetToHeaders, out TargetToOutdatedPrerequisitesMap);

                            // Display some stats to the user.
                            Log.TraceVerbose(
                                    "{0} actions, {1} outdated and requested actions",
                                    ActionGraph.AllActions.Count,
                                    ActionsToExecute.Count
                                    );

							// Cache indirect includes for all outdated C++ files.  We kick this off as a background thread so that it can
							// perform the scan while we're compiling.  It usually only takes up to a few seconds, but we don't want to hurt
							// our best case UBT iteration times for this task which can easily be performed asynchronously
							if (BuildConfiguration.bUseUBTMakefiles && TargetToOutdatedPrerequisitesMap.Count > 0)
							{
								CPPIncludesThread = CreateThreadForCachingCPPIncludes(TargetToOutdatedPrerequisitesMap, TargetToHeaders);
								CPPIncludesThread.Start();
							}

							// If we're not touching any shared files (ie. anything under Engine), allow the build ids to be recycled between applications.
							HashSet<FileReference> OutputFiles = new HashSet<FileReference>(ActionsToExecute.SelectMany(x => x.ProducedItems.Select(y => y.Reference)));
							foreach (UEBuildTarget Target in Targets)
							{
									if (!Target.TryRecycleVersionManifests(OutputFiles))
								{
									Target.InvalidateVersionManifests();
								}
							}

							// Execute the actions.
							string TargetInfoForTelemetry = String.Join("|", Targets.Select(x => String.Format("{0} {1} {2}{3}", x.TargetName, x.Platform, x.Configuration, "")));
							DateTime ExecutorStartTime = DateTime.UtcNow;
								if (BuildConfiguration.bXGEExport)
							{
								XGE.ExportActions(ActionsToExecute);
								bSuccess = true;
							}
							else
							{
								bool bIsRemoteCompile = BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac && TargetDescs.Any(x => x.Platform == UnrealTargetPlatform.Mac || x.Platform == UnrealTargetPlatform.IOS);
								bSuccess = ActionGraph.ExecuteActions(BuildConfiguration, ActionsToExecute, bIsRemoteCompile, out ExecutorName, TargetInfoForTelemetry, HotReload);
							}
							TotalExecutorTime += (DateTime.UtcNow - ExecutorStartTime).TotalSeconds;

							// if the build succeeded, write the receipts and do any needed syncing
							if (bSuccess)
							{
								foreach (UEBuildTarget Target in Targets)
								{
									Target.WriteReceipts();
									UEBuildPlatform.GetBuildPlatform(Target.Platform).PostBuildSync(Target);
								}
								if (ActionsToExecute.Count == 0 && BuildConfiguration.bSkipLinkingWhenNothingToCompile)
								{
									BuildResult = ECompilationResult.UpToDate;
								}
							}
							else
							{
								BuildResult = ECompilationResult.OtherCompilationError;
							}
						}

						// Execute all the post-build steps
						if(BuildResult.Succeeded() && !ProjectFileGenerator.bGenerateProjectFiles && !BuildConfiguration.bXGEExport)
						{
								foreach (UEBuildTarget Target in Targets)
							{
									if (!Target.ExecuteCustomPostBuildSteps())
								{
									BuildResult = ECompilationResult.OtherCompilationError;
									break;
								}
							}
						}

						// Run the deployment steps
							if (BuildResult.Succeeded()
								&& String.IsNullOrEmpty(BuildConfiguration.SingleFileToCompile)
								&& !ProjectFileGenerator.bGenerateProjectFiles
								&& !BuildConfiguration.bGenerateManifest
								&& !BuildConfiguration.bCleanProject
							&& !BuildConfiguration.bXGEExport)
						{
							foreach (UEBuildTarget Target in Targets)
							{
									if (Target.DeployTargetFile != null && Target.OnlyModules.Count == 0)
								{
									Log.WriteLine(LogEventType.Console, "Deploying {0} {1} {2}...", Target.TargetName, Target.Platform, Target.Configuration);
									UEBuildDeployTarget DeployTarget = new UEBuildDeployTarget(Target.DeployTargetFile);
									UEBuildPlatform.GetBuildPlatform(Target.Platform).Deploy(DeployTarget);
								}
							}
						}
					}
				}
			}
			}
			catch (Exception Ex)
			{
				ExceptionUtils.PrintExceptionInfo(Ex, String.IsNullOrEmpty(BuildConfiguration.LogFilename)? null : BuildConfiguration.LogFilename);
				BuildResult = ECompilationResult.OtherCompilationError;
			}

			// Wait until our CPPIncludes dependency scanner thread has finished
			if (CPPIncludesThread != null)
			{
				CPPIncludesThread.Join();
			}

			// Save the include dependency cache.
			foreach(CPPHeaders Headers in TargetToHeaders.Values)
			{
				// NOTE: It's very important that we save the include cache, even if a build exception was thrown (compile error, etc), because we need to make sure that
				//    any C++ include dependencies that we computed for out of date source files are saved.  Remember, the build may fail *after* some build products
				//    are successfully built.  If we didn't save our dependency cache after build failures, source files for those build products that were successfully
				//    built before the failure would not be considered out of date on the next run, so this is our only chance to cache C++ includes for those files!

				if (Headers.IncludeDependencyCache != null)
				{
					Headers.IncludeDependencyCache.Save();
				}

				if (Headers.FlatCPPIncludeDependencyCache != null)
				{
					Headers.FlatCPPIncludeDependencyCache.Save();
				}
			}

			// Figure out how long we took to execute.
			if(ExecutorName != "Unknown")
			{
				double BuildDuration = (DateTime.UtcNow - RunUBTInitStartTime).TotalSeconds;
				Log.TraceInformation("Total build time: {0:0.00} seconds ({1} executor: {2:0.00} seconds)", BuildDuration, ExecutorName, TotalExecutorTime);
			}

			return BuildResult;
		}

		/// <summary>
		/// Invalidates makefiles for given target.
		/// </summary>
		/// <param name="Target">Target</param>
		private static void InvalidateMakefiles(TargetDescriptor Target)
		{
			string[] MakefileNames = new string[] { "HotReloadMakefile.ubt", "Makefile.ubt" };
			DirectoryReference BaseDir = GetUBTMakefileDirectoryPathForSingleTarget(Target);

			foreach (string MakefileName in MakefileNames)
			{
				FileReference MakefileRef = FileReference.Combine(BaseDir, MakefileName);
				if (FileReference.Exists(MakefileRef))
				{
					FileReference.Delete(MakefileRef);
				}
			}
		}
		/// <summary>
		/// Checks if the editor is currently running and this is a hot-reload
		/// </summary>
		private static bool ShouldDoHotReloadFromIDE(BuildConfiguration BuildConfiguration, string[] Arguments, TargetDescriptor TargetDesc)
		{
			// Check if Hot-reload is disabled globally for this project
			ConfigHierarchy Hierarchy = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(TargetDesc.ProjectFile), TargetDesc.Platform);
			bool bAllowHotReloadFromIDE;
			if(Hierarchy.TryGetValue("BuildConfiguration", "bAllowHotReloadFromIDE", out bAllowHotReloadFromIDE) && !bAllowHotReloadFromIDE)
			{
				return false;
			}

			if (Arguments.Any(x => x.Equals("-NoHotReloadFromIDE", StringComparison.InvariantCultureIgnoreCase)))
			{
				// Hot reload disabled through command line, possibly running from UAT
				return false;
			}

			bool bIsRunning = false;

			// @todo ubtmake: Kind of cheating here to figure out if an editor target.  At this point we don't have access to the actual target description, and
			// this code must be able to execute before we create or load module rules DLLs so that hot reload can work with bUseUBTMakefiles
			bool bIsEditorTarget = TargetDesc.TargetName.EndsWith("Editor", StringComparison.InvariantCultureIgnoreCase) || TargetDesc.bIsEditorRecompile;

			if (!ProjectFileGenerator.bGenerateProjectFiles && !BuildConfiguration.bGenerateManifest && bIsEditorTarget)
			{
				List<FileReference> EditorProcessFilenames = UEBuildTarget.MakeBinaryPaths(UnrealBuildTool.EngineDirectory, "UE4Editor", TargetDesc.Platform, TargetDesc.Configuration, UEBuildBinaryType.Executable, "", UnrealTargetConfiguration.Development, false, null, null, null, null);
				if (EditorProcessFilenames.Count != 1)
				{
					throw new BuildException("ShouldDoHotReload cannot handle multiple binaries returning from UEBuildTarget.MakeExecutablePaths");
				}

				string EditorProcessFilename = EditorProcessFilenames[0].CanonicalName;
				if (TargetDesc.Platform == UnrealTargetPlatform.Mac && !EditorProcessFilename.Contains(".app/contents/macos/"))
				{
					EditorProcessFilename += ".app/contents/macos/" + Path.GetFileNameWithoutExtension(EditorProcessFilename);
				}
					
				BuildHostPlatform.ProcessInfo[] Processes = BuildHostPlatform.Current.GetProcesses();
				string EditorRunsDir = Path.Combine(UnrealBuildTool.EngineDirectory.FullName, "Intermediate", "EditorRuns");

				if (!Directory.Exists(EditorRunsDir))
				{
					return false;
				}

				FileInfo[] EditorRunsFiles = new DirectoryInfo(EditorRunsDir).GetFiles();

				foreach(FileInfo File in EditorRunsFiles)
				{
					int PID;
					BuildHostPlatform.ProcessInfo Proc = null;
					if (!Int32.TryParse(File.Name, out PID) || (Proc = Processes.FirstOrDefault(P => P.PID == PID)) == default(BuildHostPlatform.ProcessInfo))
					{
						// Delete stale files (it may happen if editor crashes).
						File.Delete();
						continue;
					}

					// Don't break here to allow clean-up of other stale files.
					if (!bIsRunning)
					{
						// Otherwise check if the path matches.
						bIsRunning = new FileReference (Proc.Filename).CanonicalName == EditorProcessFilename;
					}
				}
			}
			return bIsRunning;
		}

		/// <summary>
		/// Parses the passed in command line for build configuration overrides.
		/// </summary>
		/// <param name="Arguments">List of arguments to parse</param>
		/// <returns>List of build target settings</returns>
		private static List<string[]> ParseCommandLineFlags(string[] Arguments)
		{
			List<string[]> TargetSettings = new List<string[]>();
			int ArgumentIndex = 0;

			if (Utils.ParseCommandLineFlag(Arguments, "-targets", out ArgumentIndex))
			{
				if (ArgumentIndex + 1 >= Arguments.Length)
				{
					throw new BuildException("Expected filename after -targets argument, but found nothing.");
				}
				// Parse lines from the referenced file into target settings.
				string[] Lines = File.ReadAllLines(Arguments[ArgumentIndex + 1]);
				foreach (string Line in Lines)
				{
					if (Line != "" && Line[0] != ';')
					{
						TargetSettings.Add(Line.Split(' '));
					}
				}
			}
			// Simply use full command line arguments as target setting if not otherwise overridden.
			else
			{
				TargetSettings.Add(Arguments);
			}

			return TargetSettings;
		}


		/// <summary>
		/// Returns a Thread object that can be kicked off to update C++ include dependency cache
		/// </summary>
		/// <param name="TargetToOutdatedPrerequisitesMap">Maps each target to a list of outdated C++ files that need indirect dependencies cached</param>
		/// <param name="TargetToHeaders">Map of target to cached header information</param>
		/// <returns>The thread object</returns>
		private static Thread CreateThreadForCachingCPPIncludes(Dictionary<UEBuildTarget, List<FileItem>> TargetToOutdatedPrerequisitesMap, Dictionary<UEBuildTarget, CPPHeaders> TargetToHeaders)
		{
			return new Thread(new ThreadStart(() =>
				{
					// @todo ubtmake: This thread will access data structures that are also used on the main UBT thread, but during this time UBT
					// is only invoking the build executor, so should not be touching this stuff.  However, we need to at some guards to make sure.
					foreach (KeyValuePair<UEBuildTarget, List<FileItem>> TargetAndOutdatedPrerequisites in TargetToOutdatedPrerequisitesMap)
					{
						UEBuildTarget Target = TargetAndOutdatedPrerequisites.Key;
						List<FileItem> OutdatedPrerequisites = TargetAndOutdatedPrerequisites.Value;
						CPPHeaders Headers = TargetToHeaders[Target];

						foreach (FileItem PrerequisiteItem in OutdatedPrerequisites)
						{
							// Invoke our deep include scanner to figure out whether any of the files included by this source file have
							// changed since the build product was built
							Headers.FindAndCacheAllIncludedFiles(PrerequisiteItem, PrerequisiteItem.CachedIncludePaths, bOnlyCachedDependencies: false);
						}
					}
				}));
		}

		/// <summary>
		/// Saves a UBTMakefile to disk
		/// </summary>
		/// <param name="TargetDescs">List of targets.  Order is not important</param>
		/// <param name="HotReload">The hot reload state</param>
		/// <param name="UBTMakefile">The UBT makefile</param>
		static void SaveUBTMakefile(List<TargetDescriptor> TargetDescs, EHotReload HotReload, UBTMakefile UBTMakefile)
		{
			if (!UBTMakefile.IsValidMakefile())
			{
				throw new BuildException("Can't save a makefile that has invalid contents.  See UBTMakefile.IsValidMakefile()");
			}

			DateTime TimerStartTime = DateTime.UtcNow;

			FileItem UBTMakefileItem = FileItem.GetItemByFileReference(GetUBTMakefilePath(TargetDescs, HotReload));

			// @todo ubtmake: Optimization: The UBTMakefile saved for game projects is upwards of 9 MB.  We should try to shrink its content if possible
			// @todo ubtmake: Optimization: C# Serialization may be too slow for these big Makefiles.  Loading these files often shows up as the slower part of the assembling phase.

			// Serialize the cache to disk.
			try
			{
				Directory.CreateDirectory(Path.GetDirectoryName(UBTMakefileItem.AbsolutePath));
				using (FileStream Stream = new FileStream(UBTMakefileItem.AbsolutePath, FileMode.Create, FileAccess.Write))
				{
					BinaryFormatter Formatter = new BinaryFormatter();
					Formatter.Serialize(Stream, UBTMakefile);
				}
			}
			catch (Exception Ex)
			{
				Console.Error.WriteLine("Failed to write makefile: {0}", Ex.Message);
			}

			if (UnrealBuildTool.bPrintPerformanceInfo)
			{
				TimeSpan TimerDuration = DateTime.UtcNow - TimerStartTime;
				Log.TraceInformation("Saving makefile took " + TimerDuration.TotalSeconds + "s");
			}
		}


		/// <summary>
		/// Loads a UBTMakefile from disk
		/// </summary>
		/// <param name="MakefilePath">Path to the makefile to load</param>
		/// <param name="ProjectFile">Path to the project file</param>
		/// <param name="ReasonNotLoaded">If the function returns null, this string will contain the reason why</param>
		/// <param name="WorkingSet">Interface to query which source files are in the working set</param>
		/// <returns>The loaded makefile, or null if it failed for some reason.  On failure, the 'ReasonNotLoaded' variable will contain information about why</returns>
		static UBTMakefile LoadUBTMakefile(FileReference MakefilePath, FileReference ProjectFile, ISourceFileWorkingSet WorkingSet, out string ReasonNotLoaded)
		{
			// Check the directory timestamp on the project files directory.  If the user has generated project files more
			// recently than the UBTMakefile, then we need to consider the file to be out of date
			FileInfo UBTMakefileInfo = new FileInfo(MakefilePath.FullName);
			if (!UBTMakefileInfo.Exists)
			{
				// UBTMakefile doesn't even exist, so we won't bother loading it
				ReasonNotLoaded = "no existing makefile";
				return null;
			}

			// Check the build version
			FileInfo BuildVersionFileInfo = new FileInfo(BuildVersion.GetDefaultFileName().FullName);
			if(BuildVersionFileInfo.Exists && UBTMakefileInfo.LastWriteTime.CompareTo(BuildVersionFileInfo.LastWriteTime) < 0)
			{
				Log.TraceVerbose("Existing makefile is older than Build.version, ignoring it");
				ReasonNotLoaded = "Build.version is newer";
				return null;
			}

			// @todo ubtmake: This will only work if the directory timestamp actually changes with every single GPF.  Force delete existing files before creating new ones?  Eh... really we probably just want to delete + create a file in that folder
			//			-> UPDATE: Seems to work OK right now though on Windows platform, maybe due to GUID changes
			// @todo ubtmake: Some platforms may not save any files into this folder.  We should delete + generate a "touch" file to force the directory timestamp to be updated (or just check the timestamp file itself.  We could put it ANYWHERE, actually)

			// Installed Build doesn't need to check engine projects for outdatedness
			if (!UnrealBuildTool.IsEngineInstalled())
			{
				if (DirectoryReference.Exists(ProjectFileGenerator.IntermediateProjectFilesPath))
				{
					DateTime EngineProjectFilesLastUpdateTime = new FileInfo(ProjectFileGenerator.ProjectTimestampFile).LastWriteTime;
					if (UBTMakefileInfo.LastWriteTime.CompareTo(EngineProjectFilesLastUpdateTime) < 0)
					{
						// Engine project files are newer than UBTMakefile
						Log.TraceVerbose("Existing makefile is older than generated engine project files, ignoring it");
						ReasonNotLoaded = "project files are newer";
						return null;
					}
				}
			}

			// Check the game project directory too
			if (ProjectFile != null)
			{
				string ProjectFilename = ProjectFile.FullName;
				FileInfo ProjectFileInfo = new FileInfo(ProjectFilename);
				if (!ProjectFileInfo.Exists || UBTMakefileInfo.LastWriteTime.CompareTo(ProjectFileInfo.LastWriteTime) < 0)
				{
					// .uproject file is newer than UBTMakefile
					Log.TraceVerbose("Makefile is older than .uproject file, ignoring it");
					ReasonNotLoaded = ".uproject file is newer";
					return null;
				}

				DirectoryReference MasterProjectRelativePath = ProjectFile.Directory;
				string GameIntermediateProjectFilesPath = Path.Combine(MasterProjectRelativePath.FullName, "Intermediate", "ProjectFiles");
				if (Directory.Exists(GameIntermediateProjectFilesPath))
				{
					DateTime GameProjectFilesLastUpdateTime = new DirectoryInfo(GameIntermediateProjectFilesPath).LastWriteTime;
					if (UBTMakefileInfo.LastWriteTime.CompareTo(GameProjectFilesLastUpdateTime) < 0)
					{
						// Game project files are newer than UBTMakefile
						Log.TraceVerbose("Makefile is older than generated game project files, ignoring it");
						ReasonNotLoaded = "game project files are newer";
						return null;
					}
				}
			}

			// Check to see if UnrealBuildTool.exe was compiled more recently than the UBTMakefile
			DateTime UnrealBuildToolTimestamp = new FileInfo(Assembly.GetExecutingAssembly().Location).LastWriteTime;
			if (UBTMakefileInfo.LastWriteTime.CompareTo(UnrealBuildToolTimestamp) < 0)
			{
				// UnrealBuildTool.exe was compiled more recently than the UBTMakefile
				Log.TraceVerbose("Makefile is older than UnrealBuildTool.exe, ignoring it");
				ReasonNotLoaded = "UnrealBuildTool.exe is newer";
				return null;
			}

			// Check to see if any BuildConfiguration files have changed since the last build
			List<XmlConfig.InputFile> InputFiles = XmlConfig.FindInputFiles();
			foreach(XmlConfig.InputFile InputFile in InputFiles)
			{
				FileInfo InputFileInfo = new FileInfo(InputFile.Location.FullName);
				if(InputFileInfo.LastWriteTime > UBTMakefileInfo.LastWriteTime)
			{
				Log.TraceVerbose("Makefile is older than BuildConfiguration.xml, ignoring it" );
				ReasonNotLoaded = "BuildConfiguration.xml is newer";
				return null;
			}
			}

			UBTMakefile LoadedUBTMakefile = null;

			try
			{
				DateTime LoadUBTMakefileStartTime = DateTime.UtcNow;

				using (FileStream Stream = new FileStream(UBTMakefileInfo.FullName, FileMode.Open, FileAccess.Read))
				{
					BinaryFormatter Formatter = new BinaryFormatter();
					LoadedUBTMakefile = Formatter.Deserialize(Stream) as UBTMakefile;
				}

				if (UnrealBuildTool.bPrintPerformanceInfo)
				{
					double LoadUBTMakefileTime = (DateTime.UtcNow - LoadUBTMakefileStartTime).TotalSeconds;
					Log.TraceInformation("LoadUBTMakefile took " + LoadUBTMakefileTime + "s");
				}
			}
			catch (Exception Ex)
			{
				Log.TraceWarning("Failed to read makefile: {0}", Ex.Message);
				ReasonNotLoaded = "couldn't read existing makefile";
				return null;
			}

			if (!LoadedUBTMakefile.IsValidMakefile())
			{
				Log.TraceWarning("Loaded makefile appears to have invalid contents, ignoring it ({0})", UBTMakefileInfo.FullName);
				ReasonNotLoaded = "existing makefile appears to be invalid";
				return null;
			}

			// Check if any of the target's Build.cs files are newer than the makefile
			foreach (UEBuildTarget Target in LoadedUBTMakefile.Targets)
			{
				string TargetCsFilename = Target.TargetRulesFile.FullName;
				if (TargetCsFilename != null)
				{
					FileInfo TargetCsFile = new FileInfo(TargetCsFilename);
					bool bTargetCsFileExists = TargetCsFile.Exists;
					if (!bTargetCsFileExists || TargetCsFile.LastWriteTime > UBTMakefileInfo.LastWriteTime)
					{
						Log.TraceVerbose("{0} has been {1} since makefile was built, ignoring it ({2})", TargetCsFilename, bTargetCsFileExists ? "changed" : "deleted", UBTMakefileInfo.FullName);
						ReasonNotLoaded = string.Format("changes to target files");
						return null;
					}
				}

				IEnumerable<string> BuildCsFilenames = Target.GetAllModuleBuildCsFilenames();
				foreach (string BuildCsFilename in BuildCsFilenames)
				{
					if (BuildCsFilename != null)
					{
						FileInfo BuildCsFile = new FileInfo(BuildCsFilename);
						bool bBuildCsFileExists = BuildCsFile.Exists;
						if (!bBuildCsFileExists || BuildCsFile.LastWriteTime > UBTMakefileInfo.LastWriteTime)
						{
							Log.TraceVerbose("{0} has been {1} since makefile was built, ignoring it ({2})", BuildCsFilename, bBuildCsFileExists ? "changed" : "deleted", UBTMakefileInfo.FullName);
							ReasonNotLoaded = string.Format("changes to module files");
							return null;
						}
					}
				}

				foreach (FlatModuleCsDataType FlatCsModuleData in Target.FlatModuleCsData.Values)
				{
					if (FlatCsModuleData.BuildCsFilename != null && FlatCsModuleData.ExternalDependencies.Count > 0)
					{
						string BaseDir = Path.GetDirectoryName(FlatCsModuleData.BuildCsFilename);
						foreach (string ExternalDependency in FlatCsModuleData.ExternalDependencies)
						{
							FileInfo DependencyFile = new FileInfo(Path.Combine(BaseDir, ExternalDependency));
							bool bDependencyFileExists = DependencyFile.Exists;
							if (!bDependencyFileExists || DependencyFile.LastWriteTime > UBTMakefileInfo.LastWriteTime)
							{
								Log.TraceVerbose("{0} has been {1} since makefile was built, ignoring it ({2})", DependencyFile.FullName, bDependencyFileExists ? "changed" : "deleted", UBTMakefileInfo.FullName);
								ReasonNotLoaded = string.Format("changes to external dependency");
								return null;
							}
						}
					}
				}
			}

			// We do a check to see if any modules' headers have changed which have
			// acquired or lost UHT types.  If so, which should be rare,
			// we'll just invalidate the entire makefile and force it to be rebuilt.
			foreach (UEBuildTarget Target in LoadedUBTMakefile.Targets)
			{
				// Get all H files in processed modules newer than the makefile itself
				HashSet<string> HFilesNewerThanMakefile =
					new HashSet<string>(
						Target.FlatModuleCsData
						.SelectMany(x => x.Value.ModuleSourceFolder != null ? Directory.EnumerateFiles(x.Value.ModuleSourceFolder.FullName, "*.h", SearchOption.AllDirectories) : Enumerable.Empty<string>())
						.Where(y => Directory.GetLastWriteTimeUtc(y) > UBTMakefileInfo.LastWriteTimeUtc)
						.OrderBy(z => z).Distinct()
					);

				// Get all H files in all modules processed in the last makefile build
				HashSet<string> AllUHTHeaders = new HashSet<string>(Target.FlatModuleCsData.Select(x => x.Value).SelectMany(x => x.UHTHeaderNames));

				// Check whether any headers have been deleted. If they have, we need to regenerate the makefile since the module might now be empty. If we don't,
				// and the file has been moved to a different module, we may include stale generated headers.
				foreach (string FileName in AllUHTHeaders)
				{
					if (!File.Exists(FileName))
					{
						Log.TraceVerbose("File processed by UHT was deleted ({0}); invalidating makefile", FileName);
						ReasonNotLoaded = string.Format("UHT file was deleted");
						return null;
					}
				}

				// Makefile is invalid if:
				// * There are any newer files which contain no UHT data, but were previously in the makefile
				// * There are any newer files contain data which needs processing by UHT, but weren't not previously in the makefile
				foreach (string Filename in HFilesNewerThanMakefile)
				{
					bool bContainsUHTData = CPPHeaders.DoesFileContainUObjects(Filename);
					bool bWasProcessed = AllUHTHeaders.Contains(Filename);
					if (bContainsUHTData != bWasProcessed)
					{
						Log.TraceVerbose("{0} {1} contain UHT types and now {2} , ignoring it ({3})", Filename, bWasProcessed ? "used to" : "didn't", bWasProcessed ? "doesn't" : "does", UBTMakefileInfo.FullName);
						ReasonNotLoaded = string.Format("new files with reflected types");
						return null;
					}
				}
			}

			// If adaptive unity build is enabled, do a check to see if there are any source files that became part of the
			// working set since the Makefile was created (or, source files were removed from the working set.)  If anything
			// changed, then we'll force a new Makefile to be created so that we have fresh unity build blobs.  We always
			// want to make sure that source files in the working set are excluded from those unity blobs (for fastest possible
			// iteration times.)
			if (LoadedUBTMakefile.bUseAdaptiveUnityBuild)
			{
				// Check if any source files in the working set no longer belong in it
				foreach(FileItem SourceFile in LoadedUBTMakefile.SourceFileWorkingSet)
				{
					if(!WorkingSet.Contains(SourceFile.Reference) && File.GetLastWriteTimeUtc(SourceFile.AbsolutePath) > UBTMakefileInfo.LastWriteTimeUtc)
					{
						Log.TraceVerbose("{0} was part of source working set and now is not; invalidating makefile ({1})", SourceFile.AbsolutePath, UBTMakefileInfo.FullName);
						ReasonNotLoaded = string.Format("working set of source files changed");
						return null;
					}
				}

				// Check if any source files that are eligible for being in the working set have been modified
				foreach (FileItem SourceFile in LoadedUBTMakefile.CandidateSourceFilesForWorkingSet)
				{
					if(WorkingSet.Contains(SourceFile.Reference) && File.GetLastWriteTimeUtc(SourceFile.AbsolutePath) > UBTMakefileInfo.LastWriteTimeUtc)
					{
						Log.TraceVerbose("{0} was part of source working set and now is not; invalidating makefile ({1})", SourceFile.AbsolutePath, UBTMakefileInfo.FullName);
						ReasonNotLoaded = string.Format("working set of source files changed");
						return null;
					}
				}
			}

			ReasonNotLoaded = null;
			return LoadedUBTMakefile;
		}


		/// <summary>
		/// Gets the file path for a UBTMakefile
		/// </summary>
		/// <param name="TargetDescs">List of targets.  Order is not important</param>
		/// <param name="HotReload">The hot reload state.</param>
		/// <returns>UBTMakefile path</returns>
		public static FileReference GetUBTMakefilePath(List<TargetDescriptor> TargetDescs, EHotReload HotReload)
		{
			FileReference UBTMakefilePath;

			if (TargetDescs.Count == 1)
			{
				TargetDescriptor TargetDesc = TargetDescs[0];

				string UBTMakefileName = (HotReload != EHotReload.Disabled) ? "HotReloadMakefile.ubt" : "Makefile.ubt";

				UBTMakefilePath = FileReference.Combine(GetUBTMakefileDirectoryPathForSingleTarget(TargetDesc), UBTMakefileName);
			}
			else
			{
				// For Makefiles that contain multiple targets, we'll make up a file name that contains all of the targets, their
				// configurations and platforms, and save it into the base intermediate folder
				string TargetCollectionName = MakeTargetCollectionName(TargetDescs);

				TargetDescriptor DescriptorWithProject = TargetDescs.FirstOrDefault(x => x.ProjectFile != null);

				DirectoryReference ProjectIntermediatePath;
				if (DescriptorWithProject != null)
				{
					ProjectIntermediatePath = DirectoryReference.Combine(DescriptorWithProject.ProjectFile.Directory, "Intermediate", "Build");
				}
				else
				{
					ProjectIntermediatePath = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build");
				}

				// @todo ubtmake: The TargetCollectionName string could be really long if there is more than one target!  Hash it?
				UBTMakefilePath = FileReference.Combine(ProjectIntermediatePath, TargetCollectionName + ".ubt");
			}

			return UBTMakefilePath;
		}

		/// <summary>
		/// Gets the file path for a UBTMakefile for single target.
		/// </summary>
		/// <param name="Target">The target.</param>
		/// <returns>UBTMakefile path</returns>
		private static DirectoryReference GetUBTMakefileDirectoryPathForSingleTarget(TargetDescriptor Target)
		{
			// @todo ubtmake: If this is a compile triggered from the editor it will have passed along the game's target name, not the editor target name.
			// At this point in Unreal Build Tool, we can't possibly know what the actual editor target name is, but we can take a guess.
			// Even if we get it wrong, this won't have any side effects aside from not being able to share the exact same cached UBT Makefile
			// between hot reloads invoked from the editor and hot reloads invoked from within the IDE.
			string TargetName = Target.TargetName;
			if (!TargetName.EndsWith("Editor", StringComparison.InvariantCultureIgnoreCase) && Target.bIsEditorRecompile)
			{
				TargetName += "Editor";
			}
			return GetUBTMakefileDirectory(Target.ProjectFile, Target.Platform, Target.Configuration, TargetName);
		}

		public static DirectoryReference GetUBTMakefileDirectory(FileReference ProjectFile, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string TargetName)
		{
			// If there's only one target, just save the UBTMakefile in the target's build intermediate directory
			// under a folder for that target (and platform/config combo.)
			if (ProjectFile != null)
			{
				return DirectoryReference.Combine(ProjectFile.Directory, "Intermediate", "Build", Platform.ToString(), TargetName, Configuration.ToString());
			}
			else
			{
				return DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", Platform.ToString(), TargetName, Configuration.ToString());
			}
		}

		public static FileReference GetUBTMakefilePath(FileReference ProjectFile, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string TargetName, bool bForHotReload)
		{
			DirectoryReference BaseDir = GetUBTMakefileDirectory(ProjectFile, Platform, Configuration, TargetName);
			return FileReference.Combine(BaseDir, bForHotReload? "HotReloadMakefile.ubt" : "Makefile.ubt");
		}

		/// <summary>
		/// Makes up a name for a set of targets that we can use for file or directory names
		/// </summary>
		/// <param name="TargetDescs">List of targets.  Order is not important</param>
		/// <returns>The name to use</returns>
		private static string MakeTargetCollectionName(List<TargetDescriptor> TargetDescs)
		{
			if (TargetDescs.Count == 0)
			{
				throw new BuildException("Expecting at least one Target to be passed to MakeTargetCollectionName");
			}

			List<TargetDescriptor> SortedTargets = new List<TargetDescriptor>();
			SortedTargets.AddRange(TargetDescs);
			SortedTargets.Sort((x, y) => { return x.TargetName.CompareTo(y.TargetName); });

			// Figure out what to call our action graph based on everything we're building
			StringBuilder TargetCollectionName = new StringBuilder();
			foreach (TargetDescriptor Target in SortedTargets)
			{
				if (TargetCollectionName.Length > 0)
				{
					TargetCollectionName.Append("_");
				}

				// @todo ubtmake: If this is a compile triggered from the editor it will have passed along the game's target name, not the editor target name.
				// At this point in Unreal Build Tool, we can't possibly know what the actual editor target name is, but we can take a guess.
				// Even if we get it wrong, this won't have any side effects aside from not being able to share the exact same cached UBT Makefile
				// between hot reloads invoked from the editor and hot reloads invoked from within the IDE.
				string TargetName = Target.TargetName;
				if (!TargetName.EndsWith("Editor", StringComparison.InvariantCultureIgnoreCase) && Target.bIsEditorRecompile)
				{
					TargetName += "Editor";
				}

				// @todo ubtmake: Should we also have the platform Architecture in this string?
				TargetCollectionName.Append(TargetName + "-" + Target.Platform.ToString() + "-" + Target.Configuration.ToString());
			}

			return TargetCollectionName.ToString();
		}

		/// <summary>
		/// Replaces a hot reload suffix in a filename.
		/// </summary>
		/// <param name="InOutFilename">The filename to replace the suffix in.</param>
		/// <param name="ModuleNameToSuffix">A function which returns a replacement suffix for a given module name.</param>
		/// <returns>true is a suffix replacement was made, false otherwise.</returns>
		public static bool ReplaceHotReloadFilenameSuffix(ref string InOutFilename, Func<string, string> ModuleNameToSuffix)
		{
			// Remove the extension
			string FilenameWithoutExtension = Path.GetFileNameWithoutExtension(InOutFilename);
			string Extension = Path.GetExtension(InOutFilename);

			// Remove the numbered suffix from the end of the filename
			int IndexOfLastHyphen = FilenameWithoutExtension.LastIndexOf('-');
			string OriginalNumberSuffix = FilenameWithoutExtension.Substring(IndexOfLastHyphen);
			int NumberSuffix;
			bool bHasNumberSuffix = int.TryParse(OriginalNumberSuffix, out NumberSuffix);

			string OriginalFileNameWithoutNumberSuffix = null;
			string PlatformConfigSuffix = "";
			if (bHasNumberSuffix)
			{
				// Remove "-####" suffix in Development configuration
				OriginalFileNameWithoutNumberSuffix = FilenameWithoutExtension.Substring(0, IndexOfLastHyphen);
			}
			else
			{
				OriginalFileNameWithoutNumberSuffix = FilenameWithoutExtension;

				// Remove "-####-Platform-Configuration" suffix in Debug configuration
				int IndexOfSecondLastHyphen = FilenameWithoutExtension.LastIndexOf('-', IndexOfLastHyphen - 1, IndexOfLastHyphen);
				if (IndexOfSecondLastHyphen > 0)
				{
					int IndexOfThirdLastHyphen = FilenameWithoutExtension.LastIndexOf('-', IndexOfSecondLastHyphen - 1, IndexOfSecondLastHyphen);
					if (IndexOfThirdLastHyphen > 0)
					{
						if (int.TryParse(FilenameWithoutExtension.Substring(IndexOfThirdLastHyphen + 1, IndexOfSecondLastHyphen - IndexOfThirdLastHyphen - 1), out NumberSuffix))
						{
							OriginalFileNameWithoutNumberSuffix = FilenameWithoutExtension.Substring(0, IndexOfThirdLastHyphen);
							PlatformConfigSuffix = FilenameWithoutExtension.Substring(IndexOfSecondLastHyphen);
							bHasNumberSuffix = true;
						}
					}
				}
			}

			if (!bHasNumberSuffix)
			{
				return false;
			}

			// Figure out which suffix to use
			int FirstHyphenIndex = OriginalFileNameWithoutNumberSuffix.IndexOf('-');
			if (FirstHyphenIndex == -1)
			{
				throw new BuildException("Expected produced item file name '{0}' to start with a prefix (such as 'UE4Editor-') when hot reloading", FilenameWithoutExtension);
			}
			string OutputFileNamePrefix = OriginalFileNameWithoutNumberSuffix.Substring(0, FirstHyphenIndex + 1);

			// Get the module name from the file name
			string ModuleName = OriginalFileNameWithoutNumberSuffix.Substring(OutputFileNamePrefix.Length);

			// Add a new suffix
			string UniqueSuffix = "-" + ModuleNameToSuffix(ModuleName);

			InOutFilename = OriginalFileNameWithoutNumberSuffix + UniqueSuffix + PlatformConfigSuffix + Extension;
			return true;
		}

		/// <summary>
		/// Returns a module suffix from a list of OnlyModules if one exists, otherwise generates a new one and adds it to the list.
		/// </summary>
		/// <param name="OnlyModules">A list of modules with suffixes to search and add to if necessary.</param>
		/// <param name="ModuleName">The module name to find in the list.</param>
		/// <returns>The existing or new suffix for the module name.</returns>
		public static string GetReplacementModuleSuffix(List<OnlyModule> OnlyModules, string ModuleName)
		{
			// Check the OnlyModules list for a prefix
			if (OnlyModules != null)
			{
				OnlyModule FoundModule = OnlyModules.Find((OnlyModule) => OnlyModule.OnlyModuleName.Equals(ModuleName, StringComparison.InvariantCultureIgnoreCase));
				if (FoundModule != null)
				{
					return FoundModule.OnlyModuleSuffix;
				}
			}

			// Generate a new random suffix
			string Result = (new Random((int)(DateTime.Now.Ticks % Int32.MaxValue)).Next(10000)).ToString();

			// Add it to the OnlyModules list
			OnlyModules.Add(new OnlyModule(ModuleName, Result));

			return Result;
		}

		/// <summary>
		/// Patch action history for hot reload when running in assembler mode.  In assembler mode, the suffix on the output file will be
		/// the same for every invocation on that makefile, but we need a new suffix each time.
		/// </summary>
		static void PatchActionHistoryForHotReloadAssembling(ActionGraph ActionGraph, List<OnlyModule> OnlyModules, List<UEBuildTarget> Targets)
		{
			// Take a copy of the array so that we don't modify the original in GetReplacementModuleSuffix
			List<OnlyModule> OnlyModulesLocal = new List<OnlyModule>(OnlyModules);

			// Gather all of the response files for link actions.  We're going to need to patch 'em up after we figure out new
			// names for all of the output files and import libraries
			List<string> ResponseFilePaths = new List<string>();

			// Keep a map of the original file names and their new file names, so we can fix up response files after
			List<Tuple<string, string>> OriginalFileNameAndNewFileNameList_NoExtensions = new List<Tuple<string, string>>();

			// Finally, we'll keep track of any file items that we had to create counterparts for change file names, so we can fix those up too
			Dictionary<FileItem, FileItem> AffectedOriginalFileItemAndNewFileItemMap = new Dictionary<FileItem, FileItem>();

			foreach (Action Action in ActionGraph.AllActions.Where((Action) => Action.ActionType == ActionType.Link))
			{
				// Assume that the first produced item (with no extension) is our output file name
				string OriginalFileNameWithoutExtension = Utils.GetFilenameWithoutAnyExtensions(Action.ProducedItems[0].AbsolutePath);

				string NewFileNameWithoutExtension = OriginalFileNameWithoutExtension;
				if (!ReplaceHotReloadFilenameSuffix(ref NewFileNameWithoutExtension, (ModuleName) => GetReplacementModuleSuffix(OnlyModulesLocal, ModuleName)))
				{
					continue;
				}

				// Find the response file in the command line.  We'll need to make a copy of it with our new file name.
				string ResponseFileExtension = ".response";
				int ResponseExtensionIndex = Action.CommandArguments.IndexOf(ResponseFileExtension, StringComparison.InvariantCultureIgnoreCase);
				if (ResponseExtensionIndex != -1)
				{
					int ResponseFilePathIndex = Action.CommandArguments.LastIndexOf("@\"", ResponseExtensionIndex);
					if (ResponseFilePathIndex == -1)
					{
						throw new BuildException("Couldn't find response file path in action's command arguments when hot reloading");
					}

					string OriginalResponseFilePathWithoutExtension = Action.CommandArguments.Substring(ResponseFilePathIndex + 2, (ResponseExtensionIndex - ResponseFilePathIndex) - 2);
					string OriginalResponseFilePath = OriginalResponseFilePathWithoutExtension + ResponseFileExtension;

					string NewResponseFilePath = OriginalResponseFilePath.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);

					// Copy the old response file to the new path
					File.Copy(OriginalResponseFilePath, NewResponseFilePath, overwrite: true);

					// Keep track of the new response file name.  We'll have to do some edits afterwards.
					ResponseFilePaths.Add(NewResponseFilePath);
				}


				// Go ahead and replace all occurrences of our file name in the command-line (ignoring extensions)
				Action.CommandArguments = Action.CommandArguments.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);


				// Update this action's list of produced items too
				for (int ItemIndex = 0; ItemIndex < Action.ProducedItems.Count; ++ItemIndex)
				{
					FileItem OriginalProducedItem = Action.ProducedItems[ItemIndex];

					string OriginalProducedItemFilePath = OriginalProducedItem.AbsolutePath;
					string NewProducedItemFilePath = OriginalProducedItemFilePath.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);

					if (OriginalProducedItemFilePath != NewProducedItemFilePath)
					{
						// OK, the produced item's file name changed so we'll update it to point to our new file
						FileItem NewProducedItem = FileItem.GetItemByPath(NewProducedItemFilePath);
						Action.ProducedItems[ItemIndex] = NewProducedItem;

						// Copy the other important settings from the original file item
						NewProducedItem.bNeedsHotReloadNumbersDLLCleanUp = OriginalProducedItem.bNeedsHotReloadNumbersDLLCleanUp;
						NewProducedItem.ProducingAction = OriginalProducedItem.ProducingAction;
						NewProducedItem.bIsRemoteFile = OriginalProducedItem.bIsRemoteFile;

						// Keep track of it so we can fix up dependencies in a second pass afterwards
						AffectedOriginalFileItemAndNewFileItemMap.Add(OriginalProducedItem, NewProducedItem);
					}
				}

				// The status description of the item has the file name, so we'll update it too
				Action.StatusDescription = Action.StatusDescription.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);


				// Keep track of the file names, so we can fix up response files afterwards.
				OriginalFileNameAndNewFileNameList_NoExtensions.Add(Tuple.Create(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension));
			}


			// Do another pass and update any actions that depended on the original file names that we changed
			foreach (Action Action in ActionGraph.AllActions)
			{
				for (int ItemIndex = 0; ItemIndex < Action.PrerequisiteItems.Count; ++ItemIndex)
				{
					FileItem OriginalFileItem = Action.PrerequisiteItems[ItemIndex];

					FileItem NewFileItem;
					if (AffectedOriginalFileItemAndNewFileItemMap.TryGetValue(OriginalFileItem, out NewFileItem))
					{
						// OK, looks like we need to replace this file item because we've renamed the file
						Action.PrerequisiteItems[ItemIndex] = NewFileItem;
					}
				}
			}


			if (OriginalFileNameAndNewFileNameList_NoExtensions.Count > 0)
			{
				foreach (string ResponseFilePath in ResponseFilePaths)
				{
					// Load the file up
					StringBuilder FileContents = new StringBuilder(Utils.ReadAllText(ResponseFilePath));

					// Replace all of the old file names with new ones
					foreach (Tuple<string, string> FileNameTuple in OriginalFileNameAndNewFileNameList_NoExtensions)
					{
						string OriginalFileNameWithoutExtension = FileNameTuple.Item1;
						string NewFileNameWithoutExtension = FileNameTuple.Item2;

						FileContents.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);
					}

					// Overwrite the original file
					string FileContentsString = FileContents.ToString();
					File.WriteAllText(ResponseFilePath, FileContentsString, new System.Text.UTF8Encoding(false));
				}
			}

			foreach (UEBuildTarget Target in Targets)
			{
				Target.PatchModuleManifestsForHotReloadAssembling(OnlyModulesLocal);
				Target.WriteReceipts();
			}
		}
	}
}
