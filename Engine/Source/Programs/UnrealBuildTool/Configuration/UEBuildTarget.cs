// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Xml;
using System.Runtime.Serialization;
using System.Text.RegularExpressions;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// The platform we're building for
	/// </summary>
	public enum UnrealTargetPlatform
	{
		/// <summary>
		/// Unknown target platform
		/// </summary>
		Unknown,

		/// <summary>
		/// 32-bit Windows
		/// </summary>
		Win32,

		/// <summary>
		/// 64-bit Windows
		/// </summary>
		Win64,

		/// <summary>
		/// Mac
		/// </summary>
		Mac,

		/// <summary>
		/// XboxOne
		/// </summary>
		XboxOne,

		/// <summary>
		/// Playstation 4
		/// </summary>
		PS4,

		/// <summary>
		/// iOS
		/// </summary>
		IOS,

		/// <summary>
		/// Android
		/// </summary>
		Android,

		/// <summary>
		/// HTML5
		/// </summary>
		HTML5,

		/// <summary>
		/// Linux
		/// </summary>
		Linux,

		/// <summary>
		/// All desktop platforms
		/// </summary>
		AllDesktop,

		/// <summary>
		/// TVOS
		/// </summary>
		TVOS,

		/// <summary>
		/// Nintendo Switch
		/// </summary>
		Switch,
	}

	/// <summary>
	/// Platform groups
	/// </summary>
	public enum UnrealPlatformGroup
	{
		/// <summary>
		/// this group is just to lump Win32 and Win64 into Windows directories, removing the special Windows logic in MakeListOfUnsupportedPlatforms
		/// </summary>
		Windows,

		/// <summary>
		/// Microsoft platforms
		/// </summary>
		Microsoft,

		/// <summary>
		/// Apple platforms
		/// </summary>
		Apple,

		/// <summary>
		/// making IOS a group allows TVOS to compile IOS code
		/// </summary>
		IOS,

		/// <summary>
		/// Unix platforms
		/// </summary>
		Unix,

		/// <summary>
		/// Android platforms
		/// </summary>
		Android,

		/// <summary>
		/// Sony platforms
		/// </summary>
		Sony,

		/// <summary>
		/// These two groups can be further used to conditionally compile files for a given platform. e.g
		/// Core/Private/HTML5/Simulator/{VC tool chain files}
		/// Core/Private/HTML5/Device/{emscripten toolchain files}.  
		/// Note: There's no default group - if the platform is not registered as device or simulator - both are rejected. 
		/// </summary>
		Device,

		/// <summary>
		/// These two groups can be further used to conditionally compile files for a given platform. e.g
		/// Core/Private/HTML5/Simulator/{VC tool chain files}
		/// Core/Private/HTML5/Device/{emscripten toolchain files}.  
		/// Note: There's no default group - if the platform is not registered as device or simulator - both are rejected. 
		/// </summary>
		Simulator,

		/// <summary>
		/// Target all desktop platforms (Win64, Mac, Linux) simultaneously
		/// </summary>
		AllDesktop,
	}

	/// <summary>
	/// The class of platform. See Utils.GetPlatformsInClass().
	/// </summary>
	public enum UnrealPlatformClass
	{
		/// <summary>
		/// All platforms
		/// </summary>
		All,

		/// <summary>
		/// All desktop platforms (Win32, Win64, Mac, Linux)
		/// </summary>
		Desktop,

		/// <summary>
		/// All platforms which support the editor (Win64, Mac, Linux)
		/// </summary>
		Editor,

		/// <summary>
		/// Platforms which support running servers (Win32, Win64, Mac, Linux)
		/// </summary>
		Server,
	}

	/// <summary>
	/// The type of configuration a target can be built for
	/// </summary>
	public enum UnrealTargetConfiguration
	{
		/// <summary>
		/// Unknown
		/// </summary>
		Unknown,

		/// <summary>
		/// Debug configuration
		/// </summary>
		Debug,

		/// <summary>
		/// DebugGame configuration; equivalent to development, but with optimization disabled for game modules
		/// </summary>
		DebugGame,

		/// <summary>
		/// Development configuration
		/// </summary>
		Development,

		/// <summary>
		/// Shipping configuration
		/// </summary>
		Shipping,

		/// <summary>
		/// Test configuration
		/// </summary>
		Test,
	}

	/// <summary>
	/// A container for a binary files (dll, exe) with its associated debug info.
	/// </summary>
	public class BuildManifest
	{
		/// <summary>
		/// 
		/// </summary>
		public readonly List<string> BuildProducts = new List<string>();

		/// <summary>
		/// 
		/// </summary>
		public readonly List<string> LibraryBuildProducts = new List<string>();

		/// <summary>
		/// 
		/// </summary>
		public readonly List<string> DeployTargetFiles = new List<string>();

		/// <summary>
		/// 
		/// </summary>
		public readonly List<string> PreBuildScripts = new List<string>();

		/// <summary>
		/// 
		/// </summary>
		public readonly List<string> PostBuildScripts = new List<string>();

		/// <summary>
		/// 
		/// </summary>
		public BuildManifest()
		{
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="FileName"></param>
		public void AddBuildProduct(string FileName)
		{
			string FullFileName = Path.GetFullPath(FileName);
			if (!BuildProducts.Contains(FullFileName))
			{
				BuildProducts.Add(FullFileName);
			}
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="FileName"></param>
		/// <param name="DebugInfoExtension"></param>
		public void AddBuildProduct(string FileName, string DebugInfoExtension)
		{
			AddBuildProduct(FileName);
			if (!String.IsNullOrEmpty(DebugInfoExtension))
			{
				AddBuildProduct(Path.ChangeExtension(FileName, DebugInfoExtension));
			}
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="FileName"></param>
		public void AddLibraryBuildProduct(string FileName)
		{
			string FullFileName = Path.GetFullPath(FileName);
			if (!LibraryBuildProducts.Contains(FullFileName))
			{
				LibraryBuildProducts.Add(FullFileName);
			}
		}
	}

	[Serializable]
	class FlatModuleCsDataType : ISerializable
	{
		public FlatModuleCsDataType(SerializationInfo Info, StreamingContext Context)
		{
			BuildCsFilename = Info.GetString("bf");
			ModuleSourceFolder = (DirectoryReference)Info.GetValue("mf", typeof(DirectoryReference));
			ExternalDependencies = (List<string>)Info.GetValue("ed", typeof(List<string>));
			UHTHeaderNames = (List<string>)Info.GetValue("hn", typeof(List<string>));
		}

		public void GetObjectData(SerializationInfo Info, StreamingContext Context)
		{
			Info.AddValue("bf", BuildCsFilename);
			Info.AddValue("mf", ModuleSourceFolder);
			Info.AddValue("ed", ExternalDependencies);
			Info.AddValue("hn", UHTHeaderNames);
		}

		public FlatModuleCsDataType(string InBuildCsFilename, IEnumerable<string> InExternalDependencies)
		{
			BuildCsFilename = InBuildCsFilename;
			ExternalDependencies = new List<string>(InExternalDependencies);
		}

		public string BuildCsFilename;
		public DirectoryReference ModuleSourceFolder;
		public List<string> ExternalDependencies;
		public List<string> UHTHeaderNames = new List<string>();
	}

	[Serializable]
	class OnlyModule : ISerializable
	{
		public OnlyModule(SerializationInfo Info, StreamingContext Context)
		{
			OnlyModuleName = Info.GetString("mn");
			OnlyModuleSuffix = Info.GetString("ms");
		}

		public void GetObjectData(SerializationInfo Info, StreamingContext Context)
		{
			Info.AddValue("mn", OnlyModuleName);
			Info.AddValue("ms", OnlyModuleSuffix);
		}

		public OnlyModule(string InitOnlyModuleName)
		{
			OnlyModuleName = InitOnlyModuleName;
			OnlyModuleSuffix = String.Empty;
		}

		public OnlyModule(string InitOnlyModuleName, string InitOnlyModuleSuffix)
		{
			OnlyModuleName = InitOnlyModuleName;
			OnlyModuleSuffix = InitOnlyModuleSuffix;
		}

		/// <summary>
		/// If building only a single module, this is the module name to build
		/// </summary>
		public readonly string OnlyModuleName;

		/// <summary>
		/// When building only a single module, the optional suffix for the module file name
		/// </summary>
		public readonly string OnlyModuleSuffix;
	}


	/// <summary>
	/// Describes all of the information needed to initialize a UEBuildTarget object
	/// </summary>
	class TargetDescriptor
	{
		public FileReference ProjectFile;
		public string TargetName;
		public UnrealTargetPlatform Platform;
		public UnrealTargetConfiguration Configuration;
		public string Architecture;
		public bool bIsEditorRecompile;
		public string RemoteRoot;
		public List<OnlyModule> OnlyModules;
		public List<FileReference> ForeignPlugins;
		public string ForceReceiptFileName;
	}

	/// <summary>
	/// A target that can be built
	/// </summary>
	[Serializable]
	class UEBuildTarget : ISerializable
	{
		public string GetAppName()
		{
			return AppName;
		}

		public string GetTargetName()
		{
			return TargetName;
		}

		public static List<TargetDescriptor> ParseTargetCommandLine(string[] Arguments, ref FileReference ProjectFile)
		{
			UnrealTargetPlatform Platform = UnrealTargetPlatform.Unknown;
			UnrealTargetConfiguration Configuration = UnrealTargetConfiguration.Unknown;
			List<string> TargetNames = new List<string>();
			string Architecture = null;
			string RemoteRoot = null;
			List<OnlyModule> OnlyModules = new List<OnlyModule>();
			List<FileReference> ForeignPlugins = new List<FileReference>();
			string ForceReceiptFileName = null;

			// If true, the recompile was launched by the editor.
			bool bIsEditorRecompile = false;

			// Settings for creating/using static libraries for the engine
			List<string> PossibleTargetNames = new List<string>();
			for (int ArgumentIndex = 0; ArgumentIndex < Arguments.Length; ArgumentIndex++)
			{
				string Argument = Arguments[ArgumentIndex];
				if(!Argument.StartsWith("-"))
				{
					UnrealTargetPlatform ParsedPlatform;
					if(Enum.TryParse(Argument, true, out ParsedPlatform) && ParsedPlatform != UnrealTargetPlatform.Unknown)
					{
						if(Platform != UnrealTargetPlatform.Unknown)
						{
							throw new BuildException("Multiple platforms specified on command line (first {0}, then {1})", Platform, ParsedPlatform);
						}
						Platform = ParsedPlatform;
						continue;
					}

					UnrealTargetConfiguration ParsedConfiguration;
					if(Enum.TryParse(Argument, true, out ParsedConfiguration) && ParsedConfiguration != UnrealTargetConfiguration.Unknown)
					{
						if(Configuration != UnrealTargetConfiguration.Unknown)
						{
							throw new BuildException("Multiple configurations specified on command line (first {0}, then {1})", Configuration, ParsedConfiguration);
						}
						Configuration = ParsedConfiguration;
						continue;
					}

					PossibleTargetNames.Add(Argument);
				}
				else
				{
					switch (Arguments[ArgumentIndex].ToUpperInvariant())
					{
						case "-MODULE":
							// Specifies a module to recompile.  Can be specified more than once on the command-line to compile multiple specific modules.
							{
								if (ArgumentIndex + 1 >= Arguments.Length)
								{
									throw new BuildException("Expected module name after -Module argument, but found nothing.");
								}
								string OnlyModuleName = Arguments[++ArgumentIndex];

								OnlyModules.Add(new OnlyModule(OnlyModuleName));
							}
							break;

						case "-MODULEWITHSUFFIX":
							{
								// Specifies a module name to compile along with a suffix to append to the DLL file name.  Can be specified more than once on the command-line to compile multiple specific modules.
								if (ArgumentIndex + 2 >= Arguments.Length)
								{
									throw new BuildException("Expected module name and module suffix -ModuleWithSuffix argument");
								}

								string OnlyModuleName = Arguments[++ArgumentIndex];
								string OnlyModuleSuffix = Arguments[++ArgumentIndex];

								OnlyModules.Add(new OnlyModule(OnlyModuleName, OnlyModuleSuffix));
							}
							break;

						case "-PLUGIN":
							{
								if (ArgumentIndex + 1 >= Arguments.Length)
								{
									throw new BuildException("Expected plugin filename after -Plugin argument, but found nothing.");
								}

								ForeignPlugins.Add(new FileReference(Arguments[++ArgumentIndex]));
							}
							break;

						case "-RECEIPT":
							{
								if (ArgumentIndex + 1 >= Arguments.Length)
								{
									throw new BuildException("Expected path to the generated receipt after -Receipt argument, but found nothing.");
								}

								ForceReceiptFileName = Arguments[++ArgumentIndex];
							}
							break;

						// -RemoteRoot <RemoteRoot> sets where the generated binaries are CookerSynced.
						case "-REMOTEROOT":
							if (ArgumentIndex + 1 >= Arguments.Length)
							{
								throw new BuildException("Expected path after -RemoteRoot argument, but found nothing.");
							}
							ArgumentIndex++;
							if (Arguments[ArgumentIndex].StartsWith("xe:\\") == true)
							{
								RemoteRoot = Arguments[ArgumentIndex].Substring("xe:\\".Length);
							}
							else if (Arguments[ArgumentIndex].StartsWith("devkit:\\") == true)
							{
								RemoteRoot = Arguments[ArgumentIndex].Substring("devkit:\\".Length);
							}
							break;

						case "-DEPLOY":
							// Does nothing at the moment...
							break;

						case "-EDITORRECOMPILE":
							{
								bIsEditorRecompile = true;
							}
							break;

						default:
							break;
					}
				}
			}

			if (Platform == UnrealTargetPlatform.Unknown)
			{
				throw new BuildException("Couldn't find platform name.");
			}
			if (Configuration == UnrealTargetConfiguration.Unknown)
			{
				throw new BuildException("Couldn't determine configuration name.");
			}

			List<TargetDescriptor> Targets = new List<TargetDescriptor>();
			if (PossibleTargetNames.Count > 0)
			{
				// We have possible targets!
				string PossibleTargetName = PossibleTargetNames[0];

				// If Engine is installed, the PossibleTargetName could contain a path
				string TargetName = PossibleTargetName;

				// If a project file was not specified see if we can find one
				if (ProjectFile == null && UProjectInfo.TryGetProjectForTarget(TargetName, out ProjectFile))
				{
					Log.TraceVerbose("Found project file for {0} - {1}", TargetName, ProjectFile);
				}

				UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);

				if(Architecture == null)
				{
					Architecture = BuildPlatform.GetDefaultArchitecture(ProjectFile);
				}

				Targets.Add(new TargetDescriptor()
					{
						ProjectFile = ProjectFile,
						TargetName = TargetName,
						Platform = Platform,
						Configuration = Configuration,
						Architecture = Architecture,
						bIsEditorRecompile = bIsEditorRecompile,
						RemoteRoot = RemoteRoot,
						OnlyModules = OnlyModules,
						ForeignPlugins = ForeignPlugins,
						ForceReceiptFileName = ForceReceiptFileName
					});
			}
			if (Targets.Count == 0)
			{
				throw new BuildException("No target name was specified on the command-line.");
			}
			return Targets;
		}

		public static UnrealTargetPlatform[] GetSupportedPlatforms(TargetRules Rules)
		{
			// Check if the rules object implements the legacy GetSupportedPlatforms() function. If it does, we'll call it for backwards compatibility.
			if(Rules.GetType().GetMethod("GetSupportedPlatforms").DeclaringType != typeof(TargetRules))
			{
				List<UnrealTargetPlatform> PlatformsList = new List<UnrealTargetPlatform>();
#pragma warning disable 0612
				if (Rules.GetSupportedPlatforms(ref PlatformsList))
				{
					return PlatformsList.ToArray();
				}
#pragma warning restore 0612
			}

			// Otherwise take the SupportedPlatformsAttribute from the first type in the inheritance chain that supports it
			for (Type CurrentType = Rules.GetType(); CurrentType != null; CurrentType = CurrentType.BaseType)
			{
				object[] Attributes = Rules.GetType().GetCustomAttributes(typeof(SupportedPlatformsAttribute), false);
				if (Attributes.Length > 0)
				{
					return Attributes.OfType<SupportedPlatformsAttribute>().SelectMany(x => x.Platforms).Distinct().ToArray();
				}
			}

			// Otherwise, get the default for the target type
			if (Rules.Type == TargetType.Program)
			{
				return Utils.GetPlatformsInClass(UnrealPlatformClass.Desktop);
			}
			else if (Rules.Type == TargetType.Editor)
			{
				return Utils.GetPlatformsInClass(UnrealPlatformClass.Editor);
			}
			else
			{
				return Utils.GetPlatformsInClass(UnrealPlatformClass.All);
			}
		}

		/// <summary>
		/// Creates a target object for the specified target name.
		/// </summary>
		/// <param name="Desc">Information about the target</param>
		/// <param name="Arguments">Command line arguments</param>
		/// <param name="bCompilingSingleFile">Whether we're compiling a single file</param>
		/// <returns>The build target object for the specified build rules source file</returns>
		public static UEBuildTarget CreateTarget(TargetDescriptor Desc, string[] Arguments, bool bCompilingSingleFile)
		{
			DateTime CreateTargetStartTime = DateTime.UtcNow;

			RulesAssembly RulesAssembly;
			if (Desc.ProjectFile != null)
			{
				RulesAssembly = RulesCompiler.CreateProjectRulesAssembly(Desc.ProjectFile);
			}
			else
			{
				RulesAssembly = RulesCompiler.CreateEngineRulesAssembly();

				if (RulesAssembly.GetTargetFileName(Desc.TargetName) == null)
				{
					// Target isn't part of the engine assembly, try the enterprise assembly
					RulesAssembly EnterpriseRulesAssembly = RulesCompiler.CreateEnterpriseRulesAssembly();

					if (EnterpriseRulesAssembly != null)
					{
						RulesAssembly = EnterpriseRulesAssembly;
					}
				}
			}
			if (Desc.ForeignPlugins != null)
			{
				foreach (FileReference ForeignPlugin in Desc.ForeignPlugins)
				{
					RulesAssembly = RulesCompiler.CreatePluginRulesAssembly(ForeignPlugin, RulesAssembly);
				}
			}

			FileReference TargetFileName;
			TargetRules RulesObject = RulesAssembly.CreateTargetRules(Desc.TargetName, Desc.Platform, Desc.Configuration, Desc.Architecture, Desc.ProjectFile, Desc.bIsEditorRecompile, out TargetFileName);
			if ((ProjectFileGenerator.bGenerateProjectFiles == false) && !GetSupportedPlatforms(RulesObject).Contains(Desc.Platform))
			{
				throw new BuildException("{0} does not support the {1} platform.", Desc.TargetName, Desc.Platform.ToString());
			}

			// Now that we found the actual Editor target, make sure we're no longer using the old TargetName (which is the Game target)
			Desc.TargetName = RulesObject.Name;

			// Parse any additional command-line arguments. These override default settings specified in config files or the .target.cs files.
			foreach(object ConfigurableObject in RulesObject.GetConfigurableObjects())
			{
				CommandLine.ParseArguments(Arguments, ConfigurableObject);
			}

			// Set the final value for the link type in the target rules
			if(RulesObject.LinkType == TargetLinkType.Default)
			{
				RulesObject.LinkType = RulesObject.GetLegacyLinkType(Desc.Platform, Desc.Configuration);
			}

			// Set the default value for whether to use the shared build environment
			if(RulesObject.BuildEnvironment == TargetBuildEnvironment.Default)
			{
				if(RulesObject.ShouldUseSharedBuildEnvironment(new TargetInfo(new ReadOnlyTargetRules(RulesObject))))
				{
					RulesObject.BuildEnvironment = TargetBuildEnvironment.Shared;
				}
				else
				{
					RulesObject.BuildEnvironment = TargetBuildEnvironment.Unique;
				}
			}

			// Invoke the legacy callback for configuring the global environment
			if(RulesObject.BuildEnvironment == TargetBuildEnvironment.Unique)
			{
				TargetRules.CPPEnvironmentConfiguration CppEnvironment = new TargetRules.CPPEnvironmentConfiguration(RulesObject);
				TargetRules.LinkEnvironmentConfiguration LinkEnvironment = new TargetRules.LinkEnvironmentConfiguration(RulesObject);
				RulesObject.SetupGlobalEnvironment(new TargetInfo(new ReadOnlyTargetRules(RulesObject)), ref LinkEnvironment, ref CppEnvironment);
			}

			// Check if the rules object implements the legacy GetGeneratedCodeVersion() method. If it does, we'll call it for backwards compatibility.
			if(RulesObject.GetType().GetMethod("GetGeneratedCodeVersion").DeclaringType != typeof(TargetRules))
			{
				RulesObject.GeneratedCodeVersion = RulesObject.GetGeneratedCodeVersion();
			}

			// Invoke the ConfigureToolchain() callback. 
			RulesObject.ConfigureToolchain(new TargetInfo(new ReadOnlyTargetRules(RulesObject)));

			// Setup the malloc profiler
			if (RulesObject.bUseMallocProfiler)
			{
				RulesObject.bOmitFramePointers = false;
				RulesObject.GlobalDefinitions.Add("USE_MALLOC_PROFILER=1");
			}

			// handle some special case defines (so build system can pass -DEFINE as normal instead of needing
			// to know about special parameters)
			foreach (string Define in RulesObject.GlobalDefinitions)
			{
				switch (Define)
				{
					case "WITH_EDITOR=0":
						RulesObject.bBuildEditor = false;
						break;

					case "WITH_EDITORONLY_DATA=0":
						RulesObject.bBuildWithEditorOnlyData = false;
						break;

					// Memory profiler doesn't work if frame pointers are omitted
					case "USE_MALLOC_PROFILER=1":
						RulesObject.bOmitFramePointers = false;
						break;

					case "WITH_LEAN_AND_MEAN_UE=1":
						RulesObject.bCompileLeanAndMeanUE = true;
						break;
				}
			}

			// If we're compiling just a single file, we need to prevent unity builds from running
			if(bCompilingSingleFile)
			{
				RulesObject.bUseUnityBuild = false;
				RulesObject.bForceUnityBuild = false;
				RulesObject.bUsePCHFiles = false;
				RulesObject.bDisableLinking = true;
			}

			// Lean and mean means no Editor and other frills.
			if (RulesObject.bCompileLeanAndMeanUE)
			{
				RulesObject.bBuildEditor = false;
				RulesObject.bBuildDeveloperTools = false;
				RulesObject.bCompileSimplygon = false;
                RulesObject.bCompileSimplygonSSF = false;
				RulesObject.bCompileSpeedTree = false;
			}

			// Automatically include CoreUObject
			if (RulesObject.bCompileAgainstEngine)
			{
				RulesObject.bCompileAgainstCoreUObject = true;
			}

			// Disable editor when its not needed
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(RulesObject.Platform);
			if (BuildPlatform.ShouldNotBuildEditor(Desc.Platform, Desc.Configuration) == true)
			{
				RulesObject.bBuildEditor = false;
			}

			// Disable the DDC and a few other things related to preparing assets
			if (BuildPlatform.BuildRequiresCookedData(Desc.Platform, Desc.Configuration) == true)
			{
				RulesObject.bBuildRequiresCookedData = true;
			}

			// Must have editor only data if building the editor.
			if (RulesObject.bBuildEditor)
			{
				RulesObject.bBuildWithEditorOnlyData = true;
			}

			// Apply the override to force debug info to be enabled
			if (RulesObject.bForceDebugInfo)
			{
				RulesObject.bDisableDebugInfo = false;
				RulesObject.bOmitPCDebugInfoInDevelopment = false;
			}

			// Allow the platform to finalize the settings
			UEBuildPlatform Platform = UEBuildPlatform.GetBuildPlatform(RulesObject.Platform);
			Platform.ValidateTarget(RulesObject);

			// Skip deploy step in UBT if UAT is going to do deploy step
			if (Arguments.Contains("-skipdeploy") 
				&& RulesObject.Platform == UnrealTargetPlatform.Android) // TODO: if safe on other platforms
			{
				RulesObject.bDeployAfterCompile = false;
            }

            // Include generated code plugin if not building an editor target and project is configured for nativization
            if (RulesObject.ProjectFile != null && RulesObject.Type != TargetType.Editor && ShouldIncludeNativizedAssets(RulesObject.ProjectFile.Directory))
            {
                string PlatformName;
                if (RulesObject.Platform == UnrealTargetPlatform.Win32 || RulesObject.Platform == UnrealTargetPlatform.Win64)
                {
                    PlatformName = "Windows";
                }
                else
                {
                    PlatformName = RulesObject.Platform.ToString();
                }

                FileReference PluginFile = FileReference.Combine(RulesObject.ProjectFile.Directory, "Intermediate", "Plugins", "NativizedAssets", PlatformName, RulesObject.Type.ToString(), "NativizedAssets.uplugin");
                if (FileReference.Exists(PluginFile))
                {
                    Desc.ForeignPlugins.Add(PluginFile);
                    RulesAssembly = RulesCompiler.CreatePluginRulesAssembly(PluginFile, RulesAssembly);
                }
                else
                {
                    Log.TraceWarning("{0} is configured for nativization, but is missing the generated code plugin at \"{1}\". Make sure to cook {2} data before attempting to build the {3} target. If data was cooked with nativization enabled, this can also mean there were no Blueprint assets that required conversion, in which case this warning can be safely ignored.", RulesObject.Name, PluginFile.FullName, RulesObject.Type.ToString(), RulesObject.Platform.ToString());
                }
            }

            // Generate a build target from this rules module
            UEBuildTarget BuildTarget = new UEBuildTarget(Desc, new ReadOnlyTargetRules(RulesObject), RulesAssembly, TargetFileName);

			if (UnrealBuildTool.bPrintPerformanceInfo)
			{
				double CreateTargetTime = (DateTime.UtcNow - CreateTargetStartTime).TotalSeconds;
				Log.TraceInformation("CreateTarget for " + Desc.TargetName + " took " + CreateTargetTime + "s");
			}

			return BuildTarget;
		}

		/// <summary>
		/// The target rules
		/// </summary>
		[NonSerialized]
		public ReadOnlyTargetRules Rules;

		/// <summary>
		/// The rules assembly to use when searching for modules
		/// </summary>
		[NonSerialized]
		public RulesAssembly RulesAssembly;

		/// <summary>
		/// The project file for this target
		/// </summary>
		public FileReference ProjectFile;

		/// <summary>
		/// The project descriptor for this target
		/// </summary>
		[NonSerialized]
		public ProjectDescriptor ProjectDescriptor;

		/// <summary>
		/// Type of target
		/// </summary>
		public TargetType TargetType;

		/// <summary>
		/// The name of the application the target is part of. For targets with bUseSharedBuildEnvironment = true, this is typically the name of the base application, eg. UE4Editor for any game editor.
		/// </summary>
		public string AppName;

		/// <summary>
		/// The name of the target
		/// </summary>
		public string TargetName;

		/// <summary>
		/// Whether the target uses the shared build environment. If false, AppName==TargetName and all binaries should be written to the project directory.
		/// </summary>
		public bool bUseSharedBuildEnvironment;

		/// <summary>
		/// Platform as defined by the VCProject and passed via the command line. Not the same as internal config names.
		/// </summary>
		public UnrealTargetPlatform Platform;

		/// <summary>
		/// Target as defined by the VCProject and passed via the command line. Not necessarily the same as internal name.
		/// </summary>
		public UnrealTargetConfiguration Configuration;

		/// <summary>
		/// The architecture this target is being built for
		/// </summary>
		public string Architecture;

		/// <summary>
		/// Relative path for platform-specific intermediates (eg. Intermediate/Build/Win64)
		/// </summary>
		public string PlatformIntermediateFolder;

		/// <summary>
		/// TargetInfo object which can be passed to RulesCompiler
		/// </summary>
		public TargetInfo TargetInfo;

		/// <summary>
		/// Root directory for the active project. Typically contains the .uproject file, or the engine root.
		/// </summary>
		public DirectoryReference ProjectDirectory;

		/// <summary>
		/// Default directory for intermediate files. Typically underneath ProjectDirectory.
		/// </summary>
		public DirectoryReference ProjectIntermediateDirectory;

		/// <summary>
		/// Directory for engine intermediates. For an agnostic editor/game executable, this will be under the engine directory. For monolithic executables this will be the same as the project intermediate directory.
		/// </summary>
		public DirectoryReference EngineIntermediateDirectory;

		/// <summary>
		/// Output paths of final executable.
		/// </summary>
		public List<FileReference> OutputPaths;

		/// <summary>
		/// Returns the OutputPath is there is only one entry in OutputPaths
		/// </summary>
		public FileReference OutputPath
		{
			get
			{
				if (OutputPaths.Count != 1)
				{
					throw new BuildException("Attempted to use UEBuildTarget.OutputPath property, but there are multiple (or no) OutputPaths. You need to handle multiple in the code that called this (size = {0})", OutputPaths.Count);
				}
				return OutputPaths[0];
			}
		}

		/// <summary>
		/// Path to the file that contains the version for this target. Writing this file allows a target to read its version information at runtime.
		/// </summary>
		public FileReference VersionFile;

		/// <summary>
		/// Remote path of the binary if it is to be synced with CookerSync
		/// </summary>
		public string RemoteRoot;

		/// <summary>
		/// Whether to build target modules that can be reused for future builds
		/// </summary>
		public bool bPrecompile;

		/// <summary>
		/// Whether to use precompiled engine modules
		/// </summary>
		public bool bUsePrecompiled;

		/// <summary>
		/// Identifies whether the project contains a script plugin. This will cause UHT to be rebuilt, even in installed builds.
		/// </summary>
		public bool bHasProjectScriptPlugin;

		/// <summary>
		/// All plugins which are built for this target
		/// </summary>
		[NonSerialized]
		public List<UEBuildPlugin> BuildPlugins;

		/// <summary>
		/// All plugin dependencies for this target. This differs from the list of plugins that is built for Launcher, where we build everything, but link in only the enabled plugins.
		/// </summary>
		[NonSerialized]
		public List<UEBuildPlugin> EnabledPlugins;

		/// <summary>
		/// All plugins which should be precompiled for this target
		/// </summary>
		[NonSerialized]
		public List<UEBuildPlugin> PrecompilePlugins;

		/// <summary>
		/// Additional plugin filenames to include when building UnrealHeaderTool for the current target
		/// </summary>
		public List<FileReference> ForeignPlugins = new List<FileReference>();

		/// <summary>
		/// All application binaries; may include binaries not built by this target.
		/// </summary>
		[NonSerialized]
		public List<UEBuildBinary> AppBinaries = new List<UEBuildBinary>();

		/// <summary>
		/// Extra engine module names to either include in the binary (monolithic) or create side-by-side DLLs for (modular)
		/// </summary>
		[NonSerialized]
		public List<string> ExtraModuleNames = new List<string>();

		/// <summary>
		/// True if re-compiling this target from the editor
		/// </summary>
		public bool bEditorRecompile;

		/// <summary>
		/// If building only a specific set of modules, these are the modules to build
		/// </summary>
		public List<OnlyModule> OnlyModules = new List<OnlyModule>();

		/// <summary>
		/// Kept to determine the correct module parsing order when filtering modules.
		/// </summary>
		[NonSerialized]
		protected List<UEBuildBinary> NonFilteredModules = new List<UEBuildBinary>();

		/// <summary>
		/// true if target should be compiled in monolithic mode, false if not
		/// </summary>
		protected bool bCompileMonolithic = false;

		/// <summary>
		/// Used to keep track of all modules by name.
		/// </summary>
		[NonSerialized]
		private Dictionary<string, UEBuildModule> Modules = new Dictionary<string, UEBuildModule>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Used to map names of modules to their .Build.cs filename
		/// </summary>
		public Dictionary<string, FlatModuleCsDataType> FlatModuleCsData = new Dictionary<string, FlatModuleCsDataType>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// The receipt for this target, which contains a record of this build.
		/// </summary>
		public TargetReceipt Receipt
		{
			get;
			private set;
		}

		/// <summary>
		/// Filename for the receipt for this target.
		/// </summary>
		public FileReference ReceiptFileName
		{
			get;
			private set;
		}

		/// <summary>
		/// Module manifests to be written to each output folder
		/// </summary>
		private KeyValuePair<FileReference, ModuleManifest>[] FileReferenceToModuleManifestPairs;

		/// <summary>
		/// Force output of the receipt to an additional filename
		/// </summary>
		[NonSerialized]
		private string ForceReceiptFileName;

		/// <summary>
		/// The name of the .Target.cs file, if the target was created with one
		/// </summary>
		public readonly FileReference TargetRulesFile;

		/// <summary>
		/// List of scripts to run before building
		/// </summary>
		FileReference[] PreBuildStepScripts;

		/// <summary>
		/// List of scripts to run after building
		/// </summary>
		FileReference[] PostBuildStepScripts;

		/// <summary>
		/// File containing information needed to deploy this target
		/// </summary>
		public FileReference DeployTargetFile;

		/// <summary>
		/// A list of the module filenames which were used to build this target.
		/// </summary>
		/// <returns></returns>
		public IEnumerable<string> GetAllModuleBuildCsFilenames()
		{
			return FlatModuleCsData.Values.Select(Data => Data.BuildCsFilename);
		}

		/// <summary>
		/// A list of the module filenames which were used to build this target.
		/// </summary>
		/// <returns></returns>
		public IEnumerable<string> GetAllModuleFolders()
		{
			return FlatModuleCsData.Values.SelectMany(Data => Data.UHTHeaderNames);
		}

		/// <summary>
		/// Whether this target should be compiled in monolithic mode
		/// </summary>
		/// <returns>true if it should, false if it shouldn't</returns>
		public bool ShouldCompileMonolithic()
		{
			return bCompileMonolithic;	// @todo ubtmake: We need to make sure this function and similar things aren't called in assembler mode
		}

		public UEBuildTarget(SerializationInfo Info, StreamingContext Context)
		{
			TargetType = (TargetType)Info.GetInt32("tt");
			ProjectFile = (FileReference)Info.GetValue("pf", typeof(FileReference));
			AppName = Info.GetString("an");
			TargetName = Info.GetString("tn");
			bUseSharedBuildEnvironment = Info.GetBoolean("sb");
			Platform = (UnrealTargetPlatform)Info.GetInt32("pl");
			Configuration = (UnrealTargetConfiguration)Info.GetInt32("co");
			Architecture = Info.GetString("ar");
			PlatformIntermediateFolder = Info.GetString("if");
			TargetInfo = (TargetInfo)Info.GetValue("ti", typeof(TargetInfo));
			ProjectDirectory = (DirectoryReference)Info.GetValue("pd", typeof(DirectoryReference));
			ProjectIntermediateDirectory = (DirectoryReference)Info.GetValue("pi", typeof(DirectoryReference));
			EngineIntermediateDirectory = (DirectoryReference)Info.GetValue("ed", typeof(DirectoryReference));
			OutputPaths = (List<FileReference>)Info.GetValue("op", typeof(List<FileReference>));
			VersionFile = (FileReference)Info.GetValue("vf", typeof(FileReference));
			RemoteRoot = Info.GetString("rr");
			bPrecompile = Info.GetBoolean("pc");
			bUsePrecompiled = Info.GetBoolean("up");
			bEditorRecompile = Info.GetBoolean("er");
			OnlyModules = (List<OnlyModule>)Info.GetValue("om", typeof(List<OnlyModule>));
			bCompileMonolithic = Info.GetBoolean("cm");
			string[] FlatModuleCsDataKeys = (string[])Info.GetValue("fk", typeof(string[]));
			FlatModuleCsDataType[] FlatModuleCsDataValues = (FlatModuleCsDataType[])Info.GetValue("fv", typeof(FlatModuleCsDataType[]));
			for (int Index = 0; Index != FlatModuleCsDataKeys.Length; ++Index)
			{
				FlatModuleCsData.Add(FlatModuleCsDataKeys[Index], FlatModuleCsDataValues[Index]);
			}
			Receipt = (TargetReceipt)Info.GetValue("re", typeof(TargetReceipt));
			ReceiptFileName = (FileReference)Info.GetValue("rf", typeof(FileReference));
			FileReferenceToModuleManifestPairs = (KeyValuePair<FileReference, ModuleManifest>[])Info.GetValue("vm", typeof(KeyValuePair<FileReference, ModuleManifest>[]));
			TargetRulesFile = (FileReference)Info.GetValue("tc", typeof(FileReference));
			PreBuildStepScripts = (FileReference[])Info.GetValue("pr", typeof(FileReference[]));
			PostBuildStepScripts = (FileReference[])Info.GetValue("po", typeof(FileReference[]));
			DeployTargetFile = (FileReference)Info.GetValue("dt", typeof(FileReference));
			bHasProjectScriptPlugin = Info.GetBoolean("sp");
		}

		public void GetObjectData(SerializationInfo Info, StreamingContext Context)
		{
			Info.AddValue("tt", (int)TargetType);
			Info.AddValue("pf", ProjectFile);
			Info.AddValue("an", AppName);
			Info.AddValue("tn", TargetName);
			Info.AddValue("sb", bUseSharedBuildEnvironment);
			Info.AddValue("pl", (int)Platform);
			Info.AddValue("co", (int)Configuration);
			Info.AddValue("ar", Architecture);
			Info.AddValue("if", PlatformIntermediateFolder);
			Info.AddValue("ti", TargetInfo);
			Info.AddValue("pd", ProjectDirectory);
			Info.AddValue("pi", ProjectIntermediateDirectory);
			Info.AddValue("ed", EngineIntermediateDirectory);
			Info.AddValue("op", OutputPaths);
			Info.AddValue("vf", VersionFile);
			Info.AddValue("rr", RemoteRoot);
			Info.AddValue("pc", bPrecompile);
			Info.AddValue("up", bUsePrecompiled);
			Info.AddValue("er", bEditorRecompile);
			Info.AddValue("om", OnlyModules);
			Info.AddValue("cm", bCompileMonolithic);
			Info.AddValue("fk", FlatModuleCsData.Keys.ToArray());
			Info.AddValue("fv", FlatModuleCsData.Values.ToArray());
			Info.AddValue("re", Receipt);
			Info.AddValue("rf", ReceiptFileName);
			Info.AddValue("vm", FileReferenceToModuleManifestPairs);
			Info.AddValue("tc", TargetRulesFile);
			Info.AddValue("pr", PreBuildStepScripts);
			Info.AddValue("po", PostBuildStepScripts);
			Info.AddValue("dt", DeployTargetFile);
			Info.AddValue("sp", bHasProjectScriptPlugin);
		}

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InDesc">Target descriptor</param>
		/// <param name="InRules">The target rules, as created by RulesCompiler.</param>
		/// <param name="InRulesAssembly">The chain of rules assemblies that this target was created with</param>
		/// <param name="InTargetCsFilename">The name of the target </param>
		public UEBuildTarget(TargetDescriptor InDesc, ReadOnlyTargetRules InRules, RulesAssembly InRulesAssembly, FileReference InTargetCsFilename)
		{
			ProjectFile = InDesc.ProjectFile;
			AppName = InDesc.TargetName;
			TargetName = InDesc.TargetName;
			Platform = InDesc.Platform;
			Configuration = InDesc.Configuration;
			Architecture = InDesc.Architecture;
			Rules = InRules;
			RulesAssembly = InRulesAssembly;
			TargetType = Rules.Type;
			bEditorRecompile = InDesc.bIsEditorRecompile;
			bPrecompile = InRules.bPrecompile;
			bUsePrecompiled = InRules.bUsePrecompiled;
			ForeignPlugins = InDesc.ForeignPlugins;
			ForceReceiptFileName = InDesc.ForceReceiptFileName;

			// now that we have the platform, we can set the intermediate path to include the platform/architecture name
			PlatformIntermediateFolder = Path.Combine("Intermediate", "Build", Platform.ToString(), UEBuildPlatform.GetBuildPlatform(Platform).GetFolderNameForArchitecture(Architecture));

			Debug.Assert(InTargetCsFilename == null || InTargetCsFilename.HasExtension(".Target.cs"));
			TargetRulesFile = InTargetCsFilename;

			bCompileMonolithic = (Rules.LinkType == TargetLinkType.Monolithic);

			// Some platforms may *require* monolithic compilation...
			if (!bCompileMonolithic && UEBuildPlatform.PlatformRequiresMonolithicBuilds(InDesc.Platform, InDesc.Configuration))
			{
				throw new BuildException(String.Format("{0} does not support modular builds", InDesc.Platform));
			}

			TargetInfo = new TargetInfo(Rules);

			// Set the build environment
			bUseSharedBuildEnvironment = (Rules.BuildEnvironment == TargetBuildEnvironment.Shared);

			if (bUseSharedBuildEnvironment)
			{
				AppName = GetAppNameForTargetType(TargetInfo.Type.Value);
			}

			// Figure out what the project directory is. If we have a uproject file, use that. Otherwise use the engine directory.
			if (ProjectFile != null)
			{
				ProjectDirectory = ProjectFile.Directory;
			}
			else
			{
				if (InTargetCsFilename.IsUnderDirectory(UnrealBuildTool.EnterpriseDirectory))
				{
					ProjectDirectory = UnrealBuildTool.EnterpriseDirectory;
				}
				else
				{
					ProjectDirectory = UnrealBuildTool.EngineDirectory;
				}
			}

			// Build the project intermediate directory
			ProjectIntermediateDirectory = DirectoryReference.Combine(ProjectDirectory, PlatformIntermediateFolder, GetTargetName(), Configuration.ToString());

			// Build the engine intermediate directory. If we're building agnostic engine binaries, we can use the engine intermediates folder. Otherwise we need to use the project intermediates directory.
			if (!bUseSharedBuildEnvironment)
			{
				EngineIntermediateDirectory = ProjectIntermediateDirectory;
			}
			else if (Configuration == UnrealTargetConfiguration.DebugGame)
			{
				EngineIntermediateDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, PlatformIntermediateFolder, AppName, UnrealTargetConfiguration.Development.ToString());
			}
			else
			{
				EngineIntermediateDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, PlatformIntermediateFolder, AppName, Configuration.ToString());
			}

			// Get the receipt path for this target
			ReceiptFileName = TargetReceipt.GetDefaultPath(ProjectDirectory, TargetName, Platform, Configuration, Architecture);

			// Read the project descriptor
			if (ProjectFile != null)
			{
				ProjectDescriptor = ProjectDescriptor.FromFile(ProjectFile);
			}

			RemoteRoot = InDesc.RemoteRoot;

			OnlyModules = InDesc.OnlyModules;

			// Construct the output paths for this target's executable
			DirectoryReference OutputDirectory;
			if ((bCompileMonolithic || TargetType == TargetType.Program || !bUseSharedBuildEnvironment) && !Rules.bOutputToEngineBinaries)
			{
				OutputDirectory = ProjectDirectory;
			}
			else
			{
				OutputDirectory = UnrealBuildTool.EngineDirectory;
			}

            bool bCompileAsDLL = Rules.bShouldCompileAsDLL && bCompileMonolithic;
            OutputPaths = MakeBinaryPaths(OutputDirectory, bCompileMonolithic ? TargetName : AppName, Platform, Configuration, bCompileAsDLL ? UEBuildBinaryType.DynamicLinkLibrary : UEBuildBinaryType.Executable, TargetInfo.Architecture, Rules.UndecoratedConfiguration, bCompileMonolithic && ProjectFile != null, Rules.ExeBinariesSubFolder, Rules.OverrideExecutableFileExtension, ProjectFile, Rules);

			// Get the path to the version file unless this is a formal build (where it will be compiled in)
			UnrealTargetConfiguration VersionConfig = Configuration;
			if(VersionConfig == UnrealTargetConfiguration.DebugGame && !bCompileMonolithic && TargetType != TargetType.Program && bUseSharedBuildEnvironment && ProjectFile != null)
			{
				VersionConfig = UnrealTargetConfiguration.Development;
			}
			VersionFile = BuildVersion.GetFileNameForTarget(OutputDirectory, bCompileMonolithic? TargetName : AppName, Platform, VersionConfig, Architecture);
		}

		/// <summary>
		/// Gets the app name for a given target type
		/// </summary>
		/// <param name="Type">The target type</param>
		/// <returns>The app name for this target type</returns>
		static string GetAppNameForTargetType(TargetType Type)
		{
			switch(Type)
			{
				case TargetType.Game:
					return "UE4";
				case TargetType.Client:
					return "UE4Client";
				case TargetType.Server:
					return "UE4Server";
				case TargetType.Editor:
					return "UE4Editor";
				default:
					throw new BuildException("Invalid target type ({0})", (int)Type);
			}
		}

		/// <summary>
		/// Cleans build products and intermediates for the target. This deletes files which are named consistently with the target being built
		/// (e.g. UE4Editor-Foo-Win64-Debug.dll) rather than an actual record of previous build products.
		/// </summary>
		/// <param name="bIncludeUnrealHeaderTool">Whether to clean UnrealHeaderTool as well</param>
		/// <returns>Whether the clean succeeded</returns>
		public ECompilationResult Clean(bool bIncludeUnrealHeaderTool)
		{
			// Find the base folders that can contain binaries
			List<DirectoryReference> BaseDirs = new List<DirectoryReference>();
			BaseDirs.Add(UnrealBuildTool.EngineDirectory);
			foreach (FileReference Plugin in Plugins.EnumeratePlugins(ProjectFile))
			{
				BaseDirs.Add(Plugin.Directory);
			}
			if (ProjectFile != null)
			{
				BaseDirs.Add(ProjectFile.Directory);
			}

			// If we're running a precompiled build, remove anything under the engine folder
			if (bUsePrecompiled)
			{
				BaseDirs.RemoveAll(x => x.IsUnderDirectory(UnrealBuildTool.EngineDirectory));
			}

			// If we're in an installed project build, only allow cleaning stuff that's under the mod directories
			if (UnrealBuildTool.IsProjectInstalled())
			{
				List<DirectoryReference> ModDirs = EnabledPlugins.Where(x => x.Type == PluginType.Mod).Select(x => x.Directory).ToList();
				BaseDirs.RemoveAll(x => !ModDirs.Any(y => x.IsUnderDirectory(y)));
			}

			// Get all the names which can prefix build products
			List<string> NamePrefixes = new List<string>();
			if (Rules.Type != TargetType.Program)
			{
				NamePrefixes.Add(GetAppNameForTargetType(Rules.Type));
			}
			NamePrefixes.Add(TargetName);

			// Get the suffixes for this configuration
			List<string> NameSuffixes = new List<string>();
			if (Configuration == Rules.UndecoratedConfiguration)
			{
				NameSuffixes.Add("");
			}
			NameSuffixes.Add(String.Format("-{0}-{1}", Platform.ToString(), Configuration.ToString()));
			if (!String.IsNullOrEmpty(Architecture))
			{
				NameSuffixes.AddRange(NameSuffixes.ToArray().Select(x => x + Architecture));
			}

			// Add all the makefiles and caches to be deleted
			List<FileReference> FilesToDelete = new List<FileReference>();
			FilesToDelete.Add(FlatCPPIncludeDependencyCache.GetDependencyCachePathForTarget(this));
			FilesToDelete.Add(DependencyCache.GetDependencyCachePathForTarget(ProjectFile, Platform, TargetName));
			FilesToDelete.Add(UnrealBuildTool.GetUBTMakefilePath(ProjectFile, Platform, Configuration, TargetName, false));
			FilesToDelete.Add(UnrealBuildTool.GetUBTMakefilePath(ProjectFile, Platform, Configuration, TargetName, true));
			FilesToDelete.Add(ActionHistory.GeneratePathForTarget(this));

			// Add all the intermediate folders to be deleted
			List<DirectoryReference> DirectoriesToDelete = new List<DirectoryReference>();
			foreach (DirectoryReference BaseDir in BaseDirs)
			{
				foreach (string NamePrefix in NamePrefixes)
				{
					DirectoryReference GeneratedCodeDir = DirectoryReference.Combine(BaseDir, "Intermediate", "Build", Platform.ToString(), NamePrefix, "Inc");
					if (DirectoryReference.Exists(GeneratedCodeDir))
					{
						DirectoriesToDelete.Add(GeneratedCodeDir);
					}

					DirectoryReference IntermediateDir = DirectoryReference.Combine(BaseDir, "Intermediate", "Build", Platform.ToString(), NamePrefix, Configuration.ToString());
					if (DirectoryReference.Exists(IntermediateDir))
					{
						DirectoriesToDelete.Add(IntermediateDir);
					}
				}
			}

			// Add all the build products from this target
			string[] NamePrefixesArray = NamePrefixes.Distinct().ToArray();
			string[] NameSuffixesArray = NameSuffixes.Distinct().ToArray();
			foreach (DirectoryReference BaseDir in BaseDirs)
			{
				DirectoryReference BinariesDir = DirectoryReference.Combine(BaseDir, "Binaries", Platform.ToString());
				if(DirectoryReference.Exists(BinariesDir))
				{
					UEBuildPlatform.GetBuildPlatform(Platform).FindBuildProductsToClean(BinariesDir, NamePrefixesArray, NameSuffixesArray, FilesToDelete, DirectoriesToDelete);
				}
			}

			// Delete all the directories, then all the files. By sorting the list of directories before we delete them, we avoid spamming the log if a parent directory is deleted first.
			foreach (DirectoryReference DirectoryToDelete in DirectoriesToDelete.OrderBy(x => x.FullName))
			{
				if (DirectoryReference.Exists(DirectoryToDelete))
				{
					Log.TraceVerbose("    Deleting {0}{1}...", DirectoryToDelete, Path.DirectorySeparatorChar);
					try
					{
						DirectoryReference.Delete(DirectoryToDelete, true);
					}
					catch (Exception Ex)
					{
						Log.TraceError("Unable to delete {0} ({1})", DirectoryToDelete, Ex.Message);
					}
				}
			}

			foreach (FileReference FileToDelete in FilesToDelete.OrderBy(x => x.FullName))
			{
				if (FileReference.Exists(FileToDelete))
				{
					Log.TraceVerbose("    Deleting " + FileToDelete);
					try
					{
						FileReference.Delete(FileToDelete);
					}
					catch (Exception Ex)
					{
						Log.TraceError("Unable to delete {0} ({1})", FileToDelete, Ex.Message);
					}
				}
			}

			// Finally clean UnrealHeaderTool if this target uses CoreUObject modules and we're not cleaning UHT already and we want UHT to be cleaned.
			if (bIncludeUnrealHeaderTool && !bUsePrecompiled && TargetName != "UnrealHeaderTool")
			{
				ExternalExecution.RunExternalDotNETExecutable(UnrealBuildTool.GetUBTPath(), String.Format("UnrealHeaderTool {0} {1} -NoMutex -Clean -IgnoreJunk", BuildHostPlatform.Current.Platform, UnrealTargetConfiguration.Development));
			}

			return ECompilationResult.Succeeded;
		}

		/// <summary>
		/// Create a list of all the externally referenced files
		/// </summary>
		/// <param name="Files">Set of referenced files</param>
		void GetExternalFileList(HashSet<FileReference> Files)
		{
			// Find all the modules we depend on
			HashSet<UEBuildModule> Modules = new HashSet<UEBuildModule>();
			foreach (UEBuildBinary Binary in AppBinaries)
			{
				foreach (UEBuildModule Module in Binary.GetAllDependencyModules(bIncludeDynamicallyLoaded: false, bForceCircular: false))
				{
					Modules.Add(Module);
				}
			}

			// Get the platform we're building for
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);

			foreach (UEBuildModule Module in Modules)
			{
				// Create the module rules
				FileReference ModuleRulesFileName;
				ModuleRules Rules = CreateModuleRulesAndSetDefaults(Module.Name, "external file list option", out ModuleRulesFileName);

				// Add Additional Bundle Resources for all modules
				foreach (UEBuildBundleResource Resource in Rules.AdditionalBundleResources)
				{
					if (Directory.Exists(Resource.ResourcePath))
					{
						Files.UnionWith(DirectoryReference.EnumerateFiles(new DirectoryReference(Resource.ResourcePath), "*", SearchOption.AllDirectories));
					}
					else
					{
						Files.Add(new FileReference(Resource.ResourcePath));
					}
				}

				// Add any zip files from Additional Frameworks
				foreach (UEBuildFramework Framework in Rules.PublicAdditionalFrameworks)
				{
					if (!String.IsNullOrEmpty(Framework.FrameworkZipPath))
					{
						Files.Add(FileReference.Combine(Module.ModuleDirectory, Framework.FrameworkZipPath));
					}
				}

				// Add the rules file itself
				Files.Add(ModuleRulesFileName);

				// Get a list of all the library paths
				List<string> LibraryPaths = new List<string>();
				LibraryPaths.Add(Directory.GetCurrentDirectory());
				LibraryPaths.AddRange(Rules.PublicLibraryPaths.Where(x => !x.StartsWith("$(")).Select(x => Path.GetFullPath(x.Replace('/', Path.DirectorySeparatorChar))));

				// Get all the extensions to look for
				List<string> LibraryExtensions = new List<string>();
				LibraryExtensions.Add(BuildPlatform.GetBinaryExtension(UEBuildBinaryType.StaticLibrary));
				LibraryExtensions.Add(BuildPlatform.GetBinaryExtension(UEBuildBinaryType.DynamicLinkLibrary));

				// Add all the libraries
				foreach (string LibraryExtension in LibraryExtensions)
				{
					foreach (string LibraryName in Rules.PublicAdditionalLibraries)
					{
						foreach (string LibraryPath in LibraryPaths)
						{
							string LibraryFileName = Path.Combine(LibraryPath, LibraryName);
							if (File.Exists(LibraryFileName))
							{
								Files.Add(new FileReference(LibraryFileName));
							}

							if(LibraryName.IndexOfAny(new char[] { Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar }) == -1)
							{
								string UnixLibraryFileName = Path.Combine(LibraryPath, "lib" + LibraryName + LibraryExtension);
								if (File.Exists(UnixLibraryFileName))
								{
									Files.Add(new FileReference(UnixLibraryFileName));
								}
							}
						}
					}
				}

				// Add all the additional shadow files
				foreach (string AdditionalShadowFile in Rules.PublicAdditionalShadowFiles)
				{
					string ShadowFileName = Path.GetFullPath(AdditionalShadowFile);
					if (File.Exists(ShadowFileName))
					{
						Files.Add(new FileReference(ShadowFileName));
					}
				}

				// Find all the include paths
				List<string> AllIncludePaths = new List<string>();
				AllIncludePaths.AddRange(Rules.PublicIncludePaths);
				AllIncludePaths.AddRange(Rules.PublicSystemIncludePaths);

				// Add all the include paths
				foreach (string IncludePath in AllIncludePaths.Where(x => !x.StartsWith("$(")))
				{
					if (Directory.Exists(IncludePath))
					{
						foreach (string IncludeFileName in Directory.EnumerateFiles(IncludePath, "*", SearchOption.AllDirectories))
						{
							string Extension = Path.GetExtension(IncludeFileName).ToLower();
							if (Extension == ".h" || Extension == ".inl")
							{
								Files.Add(new FileReference(IncludeFileName));
							}
						}
					}
				}
			}
		}

		/// <summary>
		/// Generates a public manifest file for writing out
		/// </summary>
		public void GenerateManifest()
		{
			FileReference ManifestPath;
			if (UnrealBuildTool.IsEngineInstalled() && ProjectFile != null)
			{
				ManifestPath = FileReference.Combine(ProjectFile.Directory, "Intermediate", "Build", "Manifest.xml");
			}
			else
			{
				ManifestPath = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", "Manifest.xml");
			}

			BuildManifest Manifest = new BuildManifest();

			if (!Rules.bDisableLinking)
			{
				// Expand all the paths in the receipt; they'll currently use variables for the engine and project directories
				foreach (BuildProduct BuildProduct in Receipt.BuildProducts)
				{
					// Don't add static libraries into the manifest unless we're explicitly building them; we don't submit them to Perforce.
					if (!bPrecompile && (BuildProduct.Type == BuildProductType.StaticLibrary || BuildProduct.Type == BuildProductType.ImportLibrary))
					{
						Manifest.LibraryBuildProducts.Add(BuildProduct.Path.FullName);
					}
					else
					{
						Manifest.AddBuildProduct(BuildProduct.Path.FullName);
					}
				}

				UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
				if (OnlyModules.Count == 0)
				{
					Manifest.AddBuildProduct(ReceiptFileName.FullName);
				}

				if (DeployTargetFile != null)
				{
					Manifest.DeployTargetFiles.Add(DeployTargetFile.FullName);
				}

				if (PreBuildStepScripts != null)
				{
					Manifest.PreBuildScripts.AddRange(PreBuildStepScripts.Select(x => x.FullName));
				}

				if(PostBuildStepScripts != null)
				{
					Manifest.PostBuildScripts.AddRange(PostBuildStepScripts.Select(x => x.FullName));
				}
			}

			Utils.WriteClass<BuildManifest>(Manifest, ManifestPath.FullName, "");
		}

		/// <summary>
		/// Prepare all the receipts this target (all the .target and .modules files). See the VersionManifest class for an explanation of what these files are.
		/// </summary>
		void PrepareReceipts(UEToolChain ToolChain, bool bCreateDebugInfo)
		{
			// If linking is disabled, don't generate any receipt
			if(Rules.bDisableLinking)
			{
				return;
			}

			// Read the version file
			BuildVersion Version;
			if (!BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
			{
				Version = new BuildVersion();
			}

			// Create a unique identifier for this build which can be used to identify modules which are compatible. It's fine to share this between runs with the same makefile.
			// By default we leave it blank when compiling a subset of modules (for hot reload, etc...), otherwise it won't match anything else. When writing to a directory
			// that already contains a manifest, we'll reuse the build id that's already in there (see below).
			if(String.IsNullOrEmpty(Version.BuildId))
			{
				if(OnlyModules.Count > 0 || bEditorRecompile)
				{
					// If we're hot reloading, just use the last version number.
					BuildVersion LastVersion;
					if(BuildVersion.TryRead(VersionFile, out LastVersion))
					{
						Version = LastVersion;
					}
					else
					{
						Version.BuildId = "";
					}
				}
				else
				{
					// If this is a formal build, we can just the compatible changelist as the unique id. Otherwise generate something randomly.
					if(Rules.bFormalBuild)
					{
						Version.BuildId = String.Format("{0}", Version.EffectiveCompatibleChangelist);
					}
					else
					{
						Version.BuildId = Guid.NewGuid().ToString();
					}
				}
			}
			
			// Find all the build products and modules from this binary
			Receipt = new TargetReceipt(TargetName, Platform, Configuration, Version);
			foreach (UEBuildBinary Binary in AppBinaries)
			{
				// Get all the build products for this binary
				Dictionary<FileReference, BuildProductType> BuildProducts = new Dictionary<FileReference, BuildProductType>();
				Binary.GetBuildProducts(Rules, ToolChain, BuildProducts, bCreateDebugInfo);

				// Add them to the receipt
				foreach (KeyValuePair<FileReference, BuildProductType> BuildProductPair in BuildProducts)
				{
					BuildProduct BuildProduct = Receipt.AddBuildProduct(BuildProductPair.Key, BuildProductPair.Value);
					BuildProduct.IsPrecompiled = !Binary.Config.bAllowCompilation;
				}
			}

			// Add the project file
			if(ProjectFile != null)
			{
				Receipt.RuntimeDependencies.Add(ProjectFile, StagedFileType.UFS);
			}

			// Add the descriptors for all enabled plugins
			foreach(UEBuildPlugin EnabledPlugin in EnabledPlugins)
			{
				if(EnabledPlugin.bDescriptorNeededAtRuntime || EnabledPlugin.bDescriptorReferencedExplicitly)
				{
					Receipt.RuntimeDependencies.Add(EnabledPlugin.File, StagedFileType.UFS);
				}
			}

			// Add slate runtime dependencies
            if (Rules.bUsesSlate)
            {
				AddRuntimeDependenciesFromDir(DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Content", "Slate"), StagedFileType.UFS);
				if (Configuration != UnrealTargetConfiguration.Shipping)
				{
					AddRuntimeDependenciesFromDir(DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Content", "SlateDebug"), StagedFileType.UFS);
				}
				if (ProjectFile != null)
				{
					AddRuntimeDependenciesFromDir(DirectoryReference.Combine(ProjectDirectory, "Content", "Slate"), StagedFileType.UFS);
					if (Configuration != UnrealTargetConfiguration.Shipping)
					{
						AddRuntimeDependenciesFromDir(DirectoryReference.Combine(ProjectDirectory, "Content", "SlateDebug"), StagedFileType.UFS);
					}
				}
			}

			// Find all the modules which are part of this target
			HashSet<UEBuildModule> UniqueLinkedModules = new HashSet<UEBuildModule>();
			foreach (UEBuildBinary Binary in AppBinaries)
			{
				if (!Binary.Config.bPrecompileOnly)
				{
					foreach (UEBuildModule Module in Binary.Modules)
					{
						if (UniqueLinkedModules.Add(Module))
						{
							foreach (RuntimeDependency RuntimeDependency in Module.RuntimeDependencies)
							{
								Receipt.RuntimeDependencies.Add(RuntimeDependency.Path, RuntimeDependency.Type);
							}
							Receipt.AdditionalProperties.AddRange(Module.Rules.AdditionalPropertiesForReceipt);
						}
					}
				}
			}

			// Add any dependencies of precompiled modules into the receipt
			if(bPrecompile)
			{
				// Add the runtime dependencies of precompiled modules that are not directly part of this target
				foreach (UEBuildBinary Binary in AppBinaries)
				{
					if(Binary.Config.bPrecompileOnly)
					{
						foreach (UEBuildModule Module in Binary.Modules)
						{
							if (UniqueLinkedModules.Add(Module))
							{
								foreach (RuntimeDependency RuntimeDependency in Module.RuntimeDependencies)
								{
									Receipt.PrecompiledRuntimeDependencies.Add(RuntimeDependency.Path);
								}
							}
						}
					}
				}

				// Add all the files which are required to use the precompiled modules
				HashSet<FileReference> ExternalFiles = new HashSet<FileReference>();
				GetExternalFileList(ExternalFiles);

				// Convert them into relative to the target receipt
				foreach(FileReference ExternalFile in ExternalFiles)
				{
					if(ExternalFile.IsUnderDirectory(UnrealBuildTool.EngineDirectory) || ExternalFile.IsUnderDirectory(ProjectDirectory))
					{
						Receipt.PrecompiledBuildDependencies.Add(ExternalFile);
					}
				}

				// Also add the version file if it's been specified
				if (VersionFile != null)
				{
					Receipt.BuildProducts.Add(new BuildProduct(VersionFile, BuildProductType.BuildResource));
				}
			}

			// Prepare all the version manifests
			Dictionary<FileReference, ModuleManifest> FileNameToModuleManifest = new Dictionary<FileReference, ModuleManifest>();
			if (!bCompileMonolithic)
			{
				// Create the receipts for each folder
				foreach (UEBuildBinary Binary in AppBinaries)
				{
					if(Binary.Config.Type == UEBuildBinaryType.DynamicLinkLibrary && Binary.Config.bAllowCompilation)
					{
						DirectoryReference DirectoryName = Binary.Config.OutputFilePath.Directory;
						bool bIsGameDirectory = !DirectoryName.IsUnderDirectory(UnrealBuildTool.EngineDirectory);
						FileReference ManifestFileName = FileReference.Combine(DirectoryName, ModuleManifest.GetStandardFileName(AppName, Platform, Configuration, Architecture, bIsGameDirectory));

						ModuleManifest Manifest;
						if (!FileNameToModuleManifest.TryGetValue(ManifestFileName, out Manifest))
						{
							Manifest = new ModuleManifest(Version.BuildId);

							ModuleManifest ExistingManifest;
							if (ModuleManifest.TryRead(ManifestFileName, out ExistingManifest) && Version.BuildId == ExistingManifest.BuildId)
							{
								if (OnlyModules.Count > 0)
								{
									// We're just building an existing module; reuse the existing manifest AND build id.
									Manifest = ExistingManifest;
								}
								else if (Version.Changelist != 0)
								{
									// We're rebuilding at the same changelist. Keep all the existing binaries.
									Manifest.ModuleNameToFileName = Manifest.ModuleNameToFileName.Union(ExistingManifest.ModuleNameToFileName).ToDictionary(x => x.Key, x => x.Value);
								}
							}

							FileNameToModuleManifest.Add(ManifestFileName, Manifest);
						}

						foreach (string ModuleName in Binary.Config.ModuleNames)
						{
							Manifest.ModuleNameToFileName[ModuleName] = Binary.Config.OutputFilePath.GetFileName();
						}
					}
				}
			}
			FileReferenceToModuleManifestPairs = FileNameToModuleManifest.ToArray();

			// Add all the version manifests to the receipt
			foreach(FileReference VersionManifestFile in FileNameToModuleManifest.Keys)
			{
				Receipt.AddBuildProduct(VersionManifestFile, BuildProductType.RequiredResource);
			}

            // add the SDK used by the tool chain
            Receipt.AdditionalProperties.Add(new ReceiptProperty("SDK", ToolChain.GetSDKVersion()));
		}

		/// <summary>
		/// Add the contents of a directory as runtime dependencies
		/// </summary>
		/// <param name="BaseDir">The base directory to enumerate files from</param>
		/// <param name="Type">How the file should be staged</param>
		void AddRuntimeDependenciesFromDir(DirectoryReference BaseDir, StagedFileType Type)
		{
			if (DirectoryReference.Exists(BaseDir))
			{
				foreach (FileReference File in DirectoryReference.EnumerateFiles(BaseDir, "*", SearchOption.AllDirectories))
				{
					Receipt.RuntimeDependencies.Add(File, Type);
				}
			}
		}

		/// <summary>
		/// Try to recycle the build id from existing version manifests in the engine directory rather than generating a new one, if no engine binaries are being modified.
		/// This allows sharing engine binaries when switching between projects and switching between UE4 and a game-specific project. Note that different targets may require
		/// additional engine modules to be built, so we don't prohibit files being added or removed.
		/// </summary>
		/// <param name="OutputFiles">List of files being modified by this build</param>
		/// <returns>True if the existing version manifests will remain valid during this build, false if they are invalidated</returns>
		public bool TryRecycleVersionManifests(HashSet<FileReference> OutputFiles)
		{
			// Make sure we've got a list of version manifests to check against
			if(FileReferenceToModuleManifestPairs == null)
			{
				return false;
			}

			// Make sure we've got a file containing the last build id
			BuildVersion CurrentVersion;
			if(VersionFile == null || !BuildVersion.TryRead(VersionFile, out CurrentVersion))
			{
				return false;
			}

			// Read any the existing module manifests under the engine directory
			Dictionary<FileReference, ModuleManifest> ExistingFileToManifest = new Dictionary<FileReference, ModuleManifest>();
			foreach(FileReference ExistingFile in FileReferenceToModuleManifestPairs.Select(x => x.Key))
			{
				ModuleManifest ExistingManifest;
				if(ExistingFile.IsUnderDirectory(UnrealBuildTool.EngineDirectory) && ModuleManifest.TryRead(ExistingFile, out ExistingManifest))
				{
					ExistingFileToManifest.Add(ExistingFile, ExistingManifest);
				}
			}

			// Check if we're modifying any files in an existing valid manifest. If the build id for a manifest doesn't match, we can behave as if it doesn't exist.
			foreach(KeyValuePair<FileReference, ModuleManifest> ExistingPair in ExistingFileToManifest)
			{
				if(ExistingPair.Value.BuildId == CurrentVersion.BuildId)
				{
					DirectoryReference ExistingManifestDir = ExistingPair.Key.Directory;
					foreach(FileReference ExistingFile in ExistingPair.Value.ModuleNameToFileName.Values.Select(x => FileReference.Combine(ExistingManifestDir, x)))
					{
						if(OutputFiles.Contains(ExistingFile))
						{
							return false;
						}
					}
				}
			}

			// Allow the existing build id to be reused. Update the receipt.
			Receipt.Version.BuildId = CurrentVersion.BuildId;

			// Merge the existing manifests with the manifests in memory.
			foreach(KeyValuePair<FileReference, ModuleManifest> NewPair in FileReferenceToModuleManifestPairs)
			{
				// Reuse the existing build id
				ModuleManifest NewManifest = NewPair.Value;
				NewManifest.BuildId = CurrentVersion.BuildId;

				// Merge in the files from the existing manifest
				ModuleManifest ExistingManifest;
				if(ExistingFileToManifest.TryGetValue(NewPair.Key, out ExistingManifest) && ExistingManifest.BuildId == CurrentVersion.BuildId)
				{
					foreach(KeyValuePair<string, string> ModulePair in ExistingManifest.ModuleNameToFileName)
					{
						if(!NewManifest.ModuleNameToFileName.ContainsKey(ModulePair.Key))
						{
							NewManifest.ModuleNameToFileName.Add(ModulePair.Key, ModulePair.Value);
						}
					}
				}
			}
			return true;
		}

		/// <summary>
		/// Delete all the existing version manifests
		/// </summary>
		public void InvalidateVersionManifests()
		{
			// Delete all the existing manifests, so we don't try to recycle partial builds in future (the current build may fail after modifying engine files, 
			// causing bModifyingEngineFiles to be incorrect on the next invocation).
			if(FileReferenceToModuleManifestPairs != null)
			{
				foreach (FileReference VersionManifestFile in FileReferenceToModuleManifestPairs.Select(x => x.Key))
				{
					// Make sure the file (and directory) exists before trying to delete it
					if(FileReference.Exists(VersionManifestFile))
					{
						FileReference.Delete(VersionManifestFile);
					}
				}
			}
		}

		/// <summary>
		/// Patches the manifests with the new module suffixes from the OnlyModules list.
		/// </summary>
		public void PatchModuleManifestsForHotReloadAssembling(List<OnlyModule> OnlyModules)
		{
			if (FileReferenceToModuleManifestPairs == null)
			{
				return;
			}

			foreach (KeyValuePair<FileReference, ModuleManifest> FileNameToVersionManifest in FileReferenceToModuleManifestPairs)
			{
				foreach (KeyValuePair<string, string> Manifest in FileNameToVersionManifest.Value.ModuleNameToFileName)
				{
					string ModuleFilename = Manifest.Value;
					if (UnrealBuildTool.ReplaceHotReloadFilenameSuffix(ref ModuleFilename, (ModuleName) => UnrealBuildTool.GetReplacementModuleSuffix(OnlyModules, ModuleName)))
					{
						FileNameToVersionManifest.Value.ModuleNameToFileName[Manifest.Key] = ModuleFilename;
						break;
					}
				}
			}
		}

		/// <summary>
		/// Writes out the version manifest
		/// </summary>
		public void WriteReceipts()
		{
			if (Receipt != null)
			{
				UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
				if (OnlyModules == null || OnlyModules.Count == 0)
				{
					DirectoryReference.CreateDirectory(ReceiptFileName.Directory);
					Receipt.Write(ReceiptFileName, UnrealBuildTool.EngineDirectory, ProjectDirectory);
				}
				if (ForceReceiptFileName != null)
				{
					Directory.CreateDirectory(Path.GetDirectoryName(ForceReceiptFileName));
					Receipt.Write(new FileReference(ForceReceiptFileName), UnrealBuildTool.EngineDirectory, ProjectDirectory);
				}
				if(VersionFile != null)
				{
					DirectoryReference.CreateDirectory(VersionFile.Directory);

					StringWriter Writer = new StringWriter();
					Receipt.Version.Write(Writer);

					string Text = Writer.ToString();
					if(!FileReference.Exists(VersionFile) || File.ReadAllText(VersionFile.FullName) != Text)
					{
						File.WriteAllText(VersionFile.FullName, Text);
					}
				}
			}
			if (FileReferenceToModuleManifestPairs != null)
			{
				foreach (KeyValuePair<FileReference, ModuleManifest> FileNameToVersionManifest in FileReferenceToModuleManifestPairs)
				{
					// Write the manifest out to a string buffer, then only write it to disk if it's changed.
					string OutputText;
					using (StringWriter Writer = new StringWriter())
					{
						FileNameToVersionManifest.Value.Write(Writer);
						OutputText = Writer.ToString();
					}
					if(!FileReference.Exists(FileNameToVersionManifest.Key) || File.ReadAllText(FileNameToVersionManifest.Key.FullName) != OutputText)
					{
						Directory.CreateDirectory(Path.GetDirectoryName(FileNameToVersionManifest.Key.FullName));
						FileNameToVersionManifest.Value.Write(FileNameToVersionManifest.Key.FullName);
					}
				}
			}
		}

		/// <summary>
		/// Gathers dependency modules for given binaries list.
		/// </summary>
		/// <param name="Binaries">Binaries list.</param>
		/// <returns>Dependency modules set.</returns>
		static HashSet<UEBuildModuleCPP> GatherDependencyModules(List<UEBuildBinary> Binaries)
		{
			HashSet<UEBuildModuleCPP> Output = new HashSet<UEBuildModuleCPP>();

			foreach (UEBuildBinary Binary in Binaries)
			{
				List<UEBuildModule> DependencyModules = Binary.GetAllDependencyModules(bIncludeDynamicallyLoaded: false, bForceCircular: false);
				foreach (UEBuildModuleCPP Module in DependencyModules.OfType<UEBuildModuleCPP>())
				{
					if (Module.Binary != null)
					{
						Output.Add(Module);
					}
				}
			}

			return Output;
		}

		/// <summary>
		/// Builds the target, appending list of output files and returns building result.
		/// </summary>
		public ECompilationResult Build(BuildConfiguration BuildConfiguration, CPPHeaders Headers, List<FileItem> OutputItems, List<UHTModuleInfo> UObjectModules, ISourceFileWorkingSet WorkingSet, ActionGraph ActionGraph, EHotReload HotReload)
		{
			CppPlatform CppPlatform = UEBuildPlatform.GetBuildPlatform(Platform).DefaultCppPlatform;
			CppConfiguration CppConfiguration = GetCppConfiguration(Configuration);

			CppCompileEnvironment GlobalCompileEnvironment = new CppCompileEnvironment(CppPlatform, CppConfiguration, Architecture, Headers);
			UEToolChain TargetToolChain = UEBuildPlatform.GetBuildPlatform(Platform).CreateToolChain(GlobalCompileEnvironment.Platform, Rules);
			LinkEnvironment GlobalLinkEnvironment = new LinkEnvironment(GlobalCompileEnvironment.Platform, GlobalCompileEnvironment.Configuration, GlobalCompileEnvironment.Architecture);

			PreBuildSetup(TargetToolChain, GlobalCompileEnvironment, GlobalLinkEnvironment);

			// Save off the original list of binaries. We'll use this to figure out which PCHs to create later, to avoid switching PCHs when compiling single modules.
			List<UEBuildBinary> OriginalBinaries = AppBinaries;

			// If we're building a single module, then find the binary for that module and add it to our target
			if (OnlyModules.Count > 0)
			{
				NonFilteredModules = AppBinaries;
				AppBinaries = GetFilteredOnlyModules(AppBinaries, OnlyModules);
				if (AppBinaries.Count == 0)
				{
					throw new BuildException("One or more of the modules specified using the '-module' argument could not be found.");
				}
			}
			else if (HotReload == EHotReload.FromIDE)
			{
				AppBinaries = GetFilteredGameModules(AppBinaries);
				if (AppBinaries.Count == 0)
				{
					throw new BuildException("One or more of the modules specified using the '-module' argument could not be found.");
				}
			}

			// For installed builds, filter out all the binaries that aren't in mods
			if (!ProjectFileGenerator.bGenerateProjectFiles && UnrealBuildTool.IsProjectInstalled())
			{
				List<DirectoryReference> ModDirectories = EnabledPlugins.Where(x => x.Type == PluginType.Mod).Select(x => x.Directory).ToList();

				List<UEBuildBinary> FilteredBinaries = new List<UEBuildBinary>();
				foreach (UEBuildBinary DLLBinary in AppBinaries)
				{
					if(ModDirectories.Any(x => DLLBinary.Config.OutputFilePath.IsUnderDirectory(x)))
					{
						FilteredBinaries.Add(DLLBinary);
					}
				}
				AppBinaries = FilteredBinaries;

				if (AppBinaries.Count == 0)
				{
					throw new BuildException("No modules found to build. All requested binaries were already part of the installed data.");
				}
			}

			// If we're just compiling a single file, filter the list of binaries to only include the file we're interested in.
			if (!String.IsNullOrEmpty(BuildConfiguration.SingleFileToCompile))
			{
				FileItem SingleFileItem = FileItem.GetItemByPath(BuildConfiguration.SingleFileToCompile);

				HashSet<UEBuildModuleCPP> Dependencies = GatherDependencyModules(AppBinaries);

				// We only want to build the binaries for this single file
				List<UEBuildBinary> FilteredBinaries = new List<UEBuildBinary>();
				foreach (UEBuildModuleCPP Dependency in Dependencies)
				{
					bool bFileExistsInDependency = Dependency.SourceFilesFound.CPPFiles.Exists(x => x.AbsolutePath == SingleFileItem.AbsolutePath);
					if (bFileExistsInDependency)
					{
						FilteredBinaries.Add(Dependency.Binary);

						UEBuildModuleCPP.SourceFilesClass EmptySourceFileList = new UEBuildModuleCPP.SourceFilesClass();
						Dependency.SourceFilesToBuild.CopyFrom(EmptySourceFileList);
						Dependency.SourceFilesToBuild.CPPFiles.Add(SingleFileItem);
					}
				}
				AppBinaries = FilteredBinaries;

				// Check we have at least one match
				if(AppBinaries.Count == 0)
				{
					throw new BuildException("Couldn't find any module containing {0} in {1}.", SingleFileItem.Reference, TargetName);
				}
			}

			if(!ProjectFileGenerator.bGenerateProjectFiles)
			{
				// Check the distribution level of all binaries based on the dependencies they have
				if(ProjectFile == null && !Rules.bOutputPubliclyDistributable)
				{
					Dictionary<UEBuildModule, Dictionary<RestrictedFolder, DirectoryReference>> ModuleRestrictedFolderCache = new Dictionary<UEBuildModule, Dictionary<RestrictedFolder, DirectoryReference>>();

					bool bResult = true;
					foreach (UEBuildBinary Binary in AppBinaries)
					{
						bResult &= Binary.CheckRestrictedFolders(DirectoryReference.FromFile(ProjectFile), ModuleRestrictedFolderCache);
					}

					if(!bResult)
					{
						throw new BuildException("Unable to create binaries in less restricted locations than their input files.");
					}
				}

				// Check for linking against modules prohibited by the EULA
				CheckForEULAViolation();

				// Build a mapping from module to its plugin
				Dictionary<UEBuildModule, UEBuildPlugin> ModuleToPlugin = new Dictionary<UEBuildModule, UEBuildPlugin>();
				foreach(UEBuildPlugin Plugin in BuildPlugins)
				{
					foreach(UEBuildModule Module in Plugin.Modules)
					{
                        if (!ModuleToPlugin.ContainsKey(Module))
                        {
                            ModuleToPlugin.Add(Module, Plugin);
                        }
					}
				}

				// Check there aren't any engine binaries with dependencies on game modules. This can happen when game-specific plugins override engine plugins.
				foreach(UEBuildModule Module in Modules.Values)
				{
					if(Module.Binary != null && UnrealBuildTool.IsUnderAnEngineDirectory(Module.RulesFile.Directory))
					{
						DirectoryReference RootDirectory = UnrealBuildTool.EngineDirectory;

						if (Module.RulesFile.IsUnderDirectory(UnrealBuildTool.EnterpriseDirectory))
						{
							RootDirectory = UnrealBuildTool.EnterpriseDirectory;
						}

						HashSet<UEBuildModule> ReferencedModules = Module.GetDependencies(bWithIncludePathModules: true, bWithDynamicallyLoadedModules: true);

						// Make sure engine modules don't depend on enterprise or game modules and that enterprise modules don't depend on game modules
						foreach(UEBuildModule ReferencedModule in ReferencedModules)
						{
							if(ReferencedModule.RulesFile != null && !ReferencedModule.RulesFile.IsUnderDirectory(UnrealBuildTool.EngineDirectory) && !ReferencedModule.RulesFile.IsUnderDirectory(RootDirectory))
							{
								string EngineModuleRelativePath = Module.RulesFile.MakeRelativeTo(UnrealBuildTool.EngineDirectory.ParentDirectory);
								string ReferencedModuleRelativePath = (ProjectFile != null && ReferencedModule.RulesFile.IsUnderDirectory(ProjectFile.Directory)) ? ReferencedModule.RulesFile.MakeRelativeTo(ProjectFile.Directory.ParentDirectory) : ReferencedModule.RulesFile.FullName;
								throw new BuildException("Engine module '{0}' should not depend on game module '{1}'", EngineModuleRelativePath, ReferencedModuleRelativePath);
							}
						}

						// Make sure engine modules don't directly reference engine plugins
						if(Module.RulesFile.IsUnderDirectory(UnrealBuildTool.EngineSourceDirectory) && !Module.RulesFile.IsUnderDirectory(TargetRulesFile.Directory))
						{
							foreach(UEBuildModule ReferencedModule in ReferencedModules)
							{
								if(ReferencedModule.RulesFile != null && ModuleToPlugin.ContainsKey(ReferencedModule) && !IsWhitelistedEnginePluginReference(Module.Name, ReferencedModule.Name))
								{
									string EngineModuleRelativePath = Module.RulesFile.MakeRelativeTo(UnrealBuildTool.EngineDirectory.ParentDirectory);
									string ReferencedModuleRelativePath = ReferencedModule.RulesFile.MakeRelativeTo(UnrealBuildTool.EngineDirectory.ParentDirectory);
									Log.TraceWarning("Warning: Engine module '{0}' should not depend on plugin module '{1}'", EngineModuleRelativePath, ReferencedModuleRelativePath);
								}
							}
						}
					}
				}

				// Check that each plugin declares its dependencies explicitly
				foreach(UEBuildPlugin Plugin in BuildPlugins)
				{
					foreach(UEBuildModule Module in Plugin.Modules)
					{
						HashSet<UEBuildModule> DependencyModules = Module.GetDependencies(bWithIncludePathModules: true, bWithDynamicallyLoadedModules: true);
						foreach(UEBuildModule DependencyModule in DependencyModules)
						{
							UEBuildPlugin DependencyPlugin;
							if(ModuleToPlugin.TryGetValue(DependencyModule, out DependencyPlugin) && DependencyPlugin != Plugin && !Plugin.Dependencies.Contains(DependencyPlugin))
							{
								Log.TraceWarning("Warning: Plugin '{0}' does not list plugin '{1}' as a dependency, but module '{2}' depends on '{3}'.", Plugin.Name, DependencyPlugin.Name, Module.Name, DependencyModule.Name);
							}
						}
					}
				}
			}

			// Execute the pre-build steps
			if(!ProjectFileGenerator.bGenerateProjectFiles)
			{
				if(!ExecuteCustomPreBuildSteps())
				{
					return ECompilationResult.OtherCompilationError;
				}
			}

			// If we're compiling monolithic, make sure the executable knows about all referenced modules
			if (ShouldCompileMonolithic())
			{
				UEBuildBinary ExecutableBinary = AppBinaries[0];

				// Add all the modules that the executable depends on. Plugins will be already included in this list.
				List<UEBuildModule> AllReferencedModules = ExecutableBinary.GetAllDependencyModules(bIncludeDynamicallyLoaded: true, bForceCircular: true);
				foreach (UEBuildModule CurModule in AllReferencedModules)
				{
					if (CurModule.Binary == null || CurModule.Binary == ExecutableBinary || CurModule.Binary.Config.Type == UEBuildBinaryType.StaticLibrary)
					{
						ExecutableBinary.AddModule(CurModule);
					}
				}
			}

			// Add global definitions for project-specific binaries. HACK: Also defining for monolithic builds in binary releases. Might be better to set this via command line instead?
			if(!bUseSharedBuildEnvironment || bCompileMonolithic)
			{
				UEBuildBinary ExecutableBinary = AppBinaries[0];

				bool IsCurrentPlatform;
				if (Utils.IsRunningOnMono)
				{
					IsCurrentPlatform = Platform == UnrealTargetPlatform.Mac;
				}
				else
				{
					IsCurrentPlatform = Platform == UnrealTargetPlatform.Win64 || Platform == UnrealTargetPlatform.Win32;
				}

				if ((TargetType == TargetType.Game || TargetType == TargetType.Client || TargetType == TargetType.Server)
					&& IsCurrentPlatform)
				{
					// The hardcoded engine directory needs to be a relative path to match the normal EngineDir format. Not doing so breaks the network file system (TTP#315861).
					string OutputFilePath = ExecutableBinary.Config.OutputFilePath.FullName;
					if (Platform == UnrealTargetPlatform.Mac && OutputFilePath.Contains(".app/Contents/MacOS"))
					{
						OutputFilePath = OutputFilePath.Substring(0, OutputFilePath.LastIndexOf(".app/Contents/MacOS") + 4);
					}
					string EnginePath = Utils.CleanDirectorySeparators(UnrealBuildTool.EngineDirectory.MakeRelativeTo(ExecutableBinary.Config.OutputFilePath.Directory), '/');
					if (EnginePath.EndsWith("/") == false)
					{
						EnginePath += "/";
					}
					GlobalCompileEnvironment.Definitions.Add("UE_ENGINE_DIRECTORY=" + EnginePath);
				}
			}

			// On Mac and Linux we have actions that should be executed after all the binaries are created
			TargetToolChain.SetupBundleDependencies(AppBinaries, TargetName);

			// Create a receipt for the target
			if (!ProjectFileGenerator.bGenerateProjectFiles)
			{
				PrepareReceipts(TargetToolChain, GlobalLinkEnvironment.bCreateDebugInfo);
			}

			// Write out the deployment context, if necessary
			if(Rules.bDeployAfterCompile && !Rules.bDisableLinking)
			{
				UEBuildDeployTarget DeployTarget = new UEBuildDeployTarget(this);
				DeployTargetFile = FileReference.Combine(ProjectIntermediateDirectory, "Deploy.dat");
				DeployTarget.Write(DeployTargetFile);
			}

			// If we're only generating the manifest, return now
			if (BuildConfiguration.bGenerateManifest)
			{
				GenerateManifest();
				if (!BuildConfiguration.bXGEExport)
				{
					return ECompilationResult.Succeeded;
				}
			}

			if ((BuildConfiguration.bXGEExport && BuildConfiguration.bGenerateManifest) || (!BuildConfiguration.bGenerateManifest && !ProjectFileGenerator.bGenerateProjectFiles))
			{
				HashSet<UEBuildModuleCPP> ModulesToGenerateHeadersFor = GatherDependencyModules(OriginalBinaries);

				if (OnlyModules.Count > 0)
				{
					HashSet<UEBuildModuleCPP> CorrectlyOrderedModules = GatherDependencyModules(NonFilteredModules);

					CorrectlyOrderedModules.RemoveWhere((Module) => !ModulesToGenerateHeadersFor.Contains(Module));
					ModulesToGenerateHeadersFor = CorrectlyOrderedModules;
				}

				ExternalExecution.SetupUObjectModules(ModulesToGenerateHeadersFor, Rules, GlobalCompileEnvironment, UObjectModules, FlatModuleCsData, Rules.GeneratedCodeVersion);

				// NOTE: Even in Gather mode, we need to run UHT to make sure the files exist for the static action graph to be setup correctly.  This is because UHT generates .cpp
				// files that are injected as top level prerequisites.  If UHT only emitted included header files, we wouldn't need to run it during the Gather phase at all.
				if (UObjectModules.Count > 0)
				{
					// Execute the header tool
					FileReference ModuleInfoFileName = FileReference.Combine(ProjectIntermediateDirectory, GetTargetName() + ".uhtmanifest");
					ECompilationResult UHTResult = ECompilationResult.OtherCompilationError;
					if (!ExternalExecution.ExecuteHeaderToolIfNecessary(BuildConfiguration, this, GlobalCompileEnvironment, UObjectModules, ModuleInfoFileName, ref UHTResult, HotReload))
					{
						Log.TraceInformation(String.Format("Error: UnrealHeaderTool failed for target '{0}' (platform: {1}, module info: {2}, exit code: {3} ({4})).", GetTargetName(), Platform.ToString(), ModuleInfoFileName, UHTResult.ToString(), (int)UHTResult));
						return UHTResult;
					}
				}
			}

			if (ShouldCompileMonolithic() && !ProjectFileGenerator.bGenerateProjectFiles && Rules != null && TargetType != TargetType.Program)
			{
				// All non-program monolithic binaries implicitly depend on all static plugin libraries so they are always linked appropriately
				// In order to do this, we create a new module here with a cpp file we emit that invokes an empty function in each library.
				// If we do not do this, there will be no static initialization for libs if no symbols are referenced in them.
				CreateLinkerFixupsCPPFile(GlobalCompileEnvironment);
			}

			GlobalLinkEnvironment.bShouldCompileMonolithic = ShouldCompileMonolithic();

			// Find all the shared PCHs.
			List<PrecompiledHeaderTemplate> SharedPCHs = new List<PrecompiledHeaderTemplate>();
			if (!ProjectFileGenerator.bGenerateProjectFiles && Rules.bUseSharedPCHs)
			{
				SharedPCHs = FindSharedPCHs(OriginalBinaries, GlobalCompileEnvironment);
			}

			// Compile the resource files common to all DLLs on Windows
			if (!ShouldCompileMonolithic())
			{
				if (Platform == UnrealTargetPlatform.Win32 || Platform == UnrealTargetPlatform.Win64)
				{
					FileItem VersionFile = FileItem.GetExistingItemByFileReference(FileReference.Combine(UnrealBuildTool.EngineSourceDirectory, "Runtime", "Core", "Resources", "Windows", "ModuleVersionResource.rc.inl"));
					VersionFile.CachedIncludePaths = GlobalCompileEnvironment.IncludePaths;
					CPPOutput VersionOutput = TargetToolChain.CompileRCFiles(GlobalCompileEnvironment, new List<FileItem> { VersionFile }, ActionGraph);
					GlobalLinkEnvironment.CommonResourceFiles.AddRange(VersionOutput.ObjectFiles);

					if(!Rules.bFormalBuild)
					{
						CppCompileEnvironment DefaultResourceCompileEnvironment = new CppCompileEnvironment(GlobalCompileEnvironment);
						DefaultResourceCompileEnvironment.Definitions.Add("ORIGINAL_FILE_NAME=\"UE4\"");

						FileItem DefaultResourceFile = FileItem.GetExistingItemByFileReference(FileReference.Combine(UnrealBuildTool.EngineSourceDirectory, "Runtime", "Launch", "Resources", "Windows", "PCLaunch.rc"));
						DefaultResourceFile.CachedIncludePaths = DefaultResourceCompileEnvironment.IncludePaths;
						CPPOutput DefaultResourceOutput = TargetToolChain.CompileRCFiles(DefaultResourceCompileEnvironment, new List<FileItem> { DefaultResourceFile }, ActionGraph);

						GlobalLinkEnvironment.DefaultResourceFiles.AddRange(DefaultResourceOutput.ObjectFiles);
					}
				}
			}

			// Build the target's binaries.
			foreach (UEBuildBinary Binary in AppBinaries)
			{
				if(Binary.Config.bAllowCompilation)
				{
					OutputItems.AddRange(Binary.Build(Rules, TargetToolChain, GlobalCompileEnvironment, GlobalLinkEnvironment, SharedPCHs, WorkingSet, ActionGraph));
				}
			}

			// Allow the toolchain to modify the final output items
			TargetToolChain.FinalizeOutput(Rules, OutputItems, ActionGraph);

			// Make sure all the checked headers were valid
			List<string> InvalidIncludeDirectiveMessages = Modules.Values.OfType<UEBuildModuleCPP>().Where(x => x.InvalidIncludeDirectiveMessages != null).SelectMany(x => x.InvalidIncludeDirectiveMessages).ToList();
			if (InvalidIncludeDirectiveMessages.Count > 0)
			{
				foreach (string InvalidIncludeDirectiveMessage in InvalidIncludeDirectiveMessages)
				{
					Log.WriteLine(0, LogEventType.Error, LogFormatOptions.NoSeverityPrefix, "{0}", InvalidIncludeDirectiveMessage);
				}
				Log.TraceError("Build canceled.");
				return ECompilationResult.Canceled;
			}

			// Clean any stale modules which exist in multiple output directories. This can lead to the wrong DLL being loaded on Windows.
			CleanStaleModules();
			return ECompilationResult.Succeeded;
		}

		/// <summary>
		/// Cleans any stale modules that have changed moved output folder.
		/// 
		/// On Windows, the loader reads imported DLLs from the first location it finds them. If modules are moved from one place to another, we have to be sure to clean up the old versions 
		/// so that they're not loaded accidentally causing unintuitive import errors.
		/// </summary>
		void CleanStaleModules()
		{
			// Find all the output files
			HashSet<FileReference> OutputFiles = new HashSet<FileReference>();
			foreach(UEBuildBinary Binary in AppBinaries)
			{
				OutputFiles.UnionWith(Binary.Config.OutputFilePaths);
			}

			// Build a map of base filenames to their full path
			Dictionary<string, FileReference> OutputNameToLocation = new Dictionary<string, FileReference>(StringComparer.InvariantCultureIgnoreCase);
			foreach(FileReference OutputFile in OutputFiles)
			{
				OutputNameToLocation[OutputFile.GetFileName()] = OutputFile;
			}

			// Search all the output directories for files with a name matching one of our output files
			foreach(DirectoryReference OutputDirectory in OutputFiles.Select(x => x.Directory).Distinct())
			{
                if (DirectoryReference.Exists(OutputDirectory))
                {
                    foreach (FileReference ExistingFile in DirectoryReference.EnumerateFiles(OutputDirectory))
                    {
                        FileReference OutputFile;
                        if (OutputNameToLocation.TryGetValue(ExistingFile.GetFileName(), out OutputFile) && !OutputFiles.Contains(ExistingFile))
                        {
                            Log.TraceInformation("Deleting '{0}' to avoid ambiguity with '{1}'", ExistingFile, OutputFile);
							try
							{
								FileReference.Delete(ExistingFile);
							}
							catch (Exception Ex)
							{
								Log.TraceError("Unable to delete {0} ({1})", ExistingFile, Ex.Message);
							}
                        }
                    }
                }
			}
		}

		/// <summary>
		/// Check whether a reference from an engine module to a plugin module is allowed. Temporary hack until these can be fixed up propertly.
		/// </summary>
		/// <param name="EngineModuleName">Name of the engine module.</param>
		/// <param name="PluginModuleName">Name of the plugin module.</param>
		/// <returns>True if the reference is whitelisted.</returns>
		static bool IsWhitelistedEnginePluginReference(string EngineModuleName, string PluginModuleName)
		{
			if(EngineModuleName == "AndroidDeviceDetection" && PluginModuleName == "TcpMessaging")
			{
				return true;
			}
			if(EngineModuleName == "Voice" && PluginModuleName == "AndroidPermission")
			{
				return true;
			}
			return false;
		}

		/// <summary>
		/// Export the definition of this target to a JSON file
		/// </summary>
		/// <param name="FileName">File to write to</param>
		public void ExportJson(string FileName)
		{
			using (JsonWriter Writer = new JsonWriter(FileName))
			{
				Writer.WriteObjectStart();

				Writer.WriteValue("Name", TargetName);
				Writer.WriteValue("Configuration", Configuration.ToString());
				Writer.WriteValue("Platform", Platform.ToString());
				if (ProjectFile != null)
				{
					Writer.WriteValue("ProjectFile", ProjectFile.FullName);
				}

				Writer.WriteArrayStart("Binaries");
				foreach (UEBuildBinary Binary in AppBinaries)
				{
					Writer.WriteObjectStart();
					Binary.ExportJson(Writer);
					Writer.WriteObjectEnd();
				}
				Writer.WriteArrayEnd();

				Writer.WriteObjectStart("Modules");
				foreach(UEBuildModule Module in Modules.Values)
				{
					Writer.WriteObjectStart(Module.Name);
					Module.ExportJson(Writer);
					Writer.WriteObjectEnd();
				}
				Writer.WriteObjectEnd();

				Writer.WriteObjectEnd();
			}
		}

		/// <summary>
		/// Check for EULA violation dependency issues.
		/// </summary>
		private void CheckForEULAViolation()
		{
			if (TargetType != TargetType.Editor && TargetType != TargetType.Program && Configuration == UnrealTargetConfiguration.Shipping &&
				Rules.bCheckLicenseViolations)
			{
				bool bLicenseViolation = false;
				foreach (UEBuildBinary Binary in AppBinaries)
				{
					List<UEBuildModule> AllDependencies = Binary.GetAllDependencyModules(true, false);
					IEnumerable<UEBuildModule> NonRedistModules = AllDependencies.Where((DependencyModule) =>
							!IsRedistributable(DependencyModule) && DependencyModule.Name != AppName
						);

					if (NonRedistModules.Count() != 0)
					{
						IEnumerable<UEBuildModule> NonRedistDeps = AllDependencies.Where((DependantModule) =>
							DependantModule.GetDirectDependencyModules().Intersect(NonRedistModules).Any()
						);
						string Message = string.Format("Non-editor build cannot depend on non-redistributable modules. {0} depends on '{1}'.", Binary.ToString(), string.Join("', '", NonRedistModules));
						if (NonRedistDeps.Any())
						{
							Message = string.Format("{0}\nDependant modules '{1}'", Message, string.Join("', '", NonRedistDeps));
						}
						if(Rules.bBreakBuildOnLicenseViolation)
						{
							Log.TraceError("ERROR: {0}", Message);
						}
						else
						{
							Log.TraceWarning("WARNING: {0}", Message);
						}
						bLicenseViolation = true;
					}
				}
				if (Rules.bBreakBuildOnLicenseViolation && bLicenseViolation)
				{
					throw new BuildException("Non-editor build cannot depend on non-redistributable modules.");
				}
			}
		}

		/// <summary>
		/// Tells if this module can be redistributed.
		/// </summary>
		public static bool IsRedistributable(UEBuildModule Module)
		{
			if(Module.Rules != null && Module.Rules.IsRedistributableOverride.HasValue)
			{
				return Module.Rules.IsRedistributableOverride.Value;
			}

			if(Module.RulesFile != null)
			{
				return !Module.RulesFile.IsUnderDirectory(UnrealBuildTool.EngineSourceDeveloperDirectory) && !Module.RulesFile.IsUnderDirectory(UnrealBuildTool.EngineSourceEditorDirectory);
			}

			return true;
		}

		/// <summary>
		/// Setup target before build. This method finds dependencies, sets up global environment etc.
		/// </summary>
		public void PreBuildSetup(UEToolChain TargetToolChain, CppCompileEnvironment GlobalCompileEnvironment, LinkEnvironment GlobalLinkEnvironment)
		{
			// Set up the global compile and link environment in GlobalCompileEnvironment and GlobalLinkEnvironment.
			SetupGlobalEnvironment(TargetToolChain, GlobalCompileEnvironment, GlobalLinkEnvironment);

			// Setup the target's modules.
			SetupModules();

			// Setup the target's binaries.
			SetupBinaries();

			// Setup the target's plugins
			SetupPlugins();

			// Setup the custom build steps for this target
			SetupCustomBuildSteps();

			// Create all the modules for each binary
			foreach (UEBuildBinary Binary in AppBinaries)
			{
				foreach (string ModuleName in Binary.Config.ModuleNames)
				{
					UEBuildModule Module = FindOrCreateModuleByName(ModuleName, TargetRulesFile.GetFileName());
					Module.Binary = Binary;
					Binary.AddModule(Module);
				}
			}

			// Add the plugin binaries to the build
			UEBuildBinaryType BinaryType = ShouldCompileMonolithic() ? UEBuildBinaryType.StaticLibrary : UEBuildBinaryType.DynamicLinkLibrary;
			foreach (UEBuildPlugin Plugin in BuildPlugins)
			{
				foreach(UEBuildModule Module in Plugin.Modules)
				{
					// Add the corresponding binary for it
					bool bAllowCompilation = RulesAssembly.DoesModuleHaveSource(Module.Name);
					bool bPrecompileOnly = !EnabledPlugins.Contains(Plugin);
					Module.Binary = CreateBinaryForModule(Module, BinaryType, bAllowCompilation: bAllowCompilation, bIsCrossTarget: false, bPrecompileOnly: bPrecompileOnly);

					// Add it to the binary if we're compiling monolithic (and it's enabled)
					if (ShouldCompileMonolithic() && EnabledPlugins.Contains(Plugin))
					{
						AppBinaries[0].AddModule(Module);
					}
				}
			}

			// Describe what's being built.
			Log.TraceVerbose("Building {0} - {1} - {2} - {3}", AppName, TargetName, Platform, Configuration);

			// Put the non-executable output files (PDB, import library, etc) in the intermediate directory.
			GlobalLinkEnvironment.IntermediateDirectory = GlobalCompileEnvironment.OutputDirectory;
			GlobalLinkEnvironment.OutputDirectory = GlobalLinkEnvironment.IntermediateDirectory;

			// By default, shadow source files for this target in the root OutputDirectory
			GlobalLinkEnvironment.LocalShadowDirectory = GlobalLinkEnvironment.OutputDirectory;

			// Add all of the extra modules, including game modules, that need to be compiled along
			// with this app.  These modules are always statically linked in monolithic targets, but not necessarily linked to anything in modular targets,
			// and may still be required at runtime in order for the application to load and function properly!
			AddExtraModules();

			// Create all the modules referenced by the existing binaries
			foreach(UEBuildBinary Binary in AppBinaries)
			{
				Binary.CreateAllDependentModules(FindOrCreateModuleByName);
			}

			// Bind every referenced C++ module to a binary
			for (int Idx = 0; Idx < AppBinaries.Count; Idx++)
			{
				List<UEBuildModule> DependencyModules = AppBinaries[Idx].GetAllDependencyModules(true, true);
				foreach (UEBuildModuleCPP DependencyModule in DependencyModules.OfType<UEBuildModuleCPP>())
				{
					if(DependencyModule.Binary == null)
					{
						AddModuleToBinary(DependencyModule, false);
					}
				}
			}

			// Add all the precompiled modules to the target. In contrast to "Extra Modules", these modules are not compiled into monolithic targets by default.
			AddPrecompiledModules();

			// Add the external and non-C++ referenced modules to the binaries that reference them.
			foreach (UEBuildModuleCPP Module in Modules.Values.OfType<UEBuildModuleCPP>())
			{
				if(Module.Binary != null)
				{
					foreach (UEBuildModule ReferencedModule in Module.GetUnboundReferences())
					{
						Module.Binary.AddModule(ReferencedModule);
					}
				}
			}

			if (!bCompileMonolithic)
			{
				if (GlobalLinkEnvironment.Platform == CppPlatform.Win64 || GlobalLinkEnvironment.Platform == CppPlatform.Win32)
				{
					// On Windows create import libraries for all binaries ahead of time, since linking binaries often causes bottlenecks
					foreach (UEBuildBinary Binary in AppBinaries)
					{
						Binary.SetCreateImportLibrarySeparately(true);
					}
				}
				else
				{
					// On other platforms markup all the binaries containing modules with circular references
					foreach (UEBuildModule Module in Modules.Values.Where(x => x.Binary != null))
					{
						foreach (string CircularlyReferencedModuleName in Module.Rules.CircularlyReferencedDependentModules)
						{
							UEBuildModule CircularlyReferencedModule;
							if (Modules.TryGetValue(CircularlyReferencedModuleName, out CircularlyReferencedModule) && CircularlyReferencedModule.Binary != null)
							{
								CircularlyReferencedModule.Binary.SetCreateImportLibrarySeparately(true);
							}
						}
					}
				}
			}

			// On Mac AppBinaries paths for non-console targets need to be adjusted to be inside the app bundle
			if (GlobalLinkEnvironment.Platform == CppPlatform.Mac && !GlobalLinkEnvironment.bIsBuildingConsoleApplication)
			{
				TargetToolChain.FixBundleBinariesPaths(this, AppBinaries);
			}
		}

		/// <summary>
		/// Writes scripts for all the custom build steps
		/// </summary>
		private void SetupCustomBuildSteps()
		{
			// Make sure the intermediate directory exists
			DirectoryReference ScriptDirectory = ProjectIntermediateDirectory;
			if(!DirectoryReference.Exists(ScriptDirectory))
			{
				DirectoryReference.CreateDirectory(ScriptDirectory);
			}

			// Find all the pre-build steps
			List<Tuple<string[], UEBuildPlugin>> PreBuildCommandBatches = new List<Tuple<string[], UEBuildPlugin>>();
			if(ProjectDescriptor != null && ProjectDescriptor.PreBuildSteps != null)
			{
				AddCustomBuildSteps(ProjectDescriptor.PreBuildSteps, null, PreBuildCommandBatches);
			}
			if(Rules.PreBuildSteps.Count > 0)
			{
				PreBuildCommandBatches.Add(new Tuple<string[], UEBuildPlugin>(Rules.PreBuildSteps.ToArray(), null));
			}
			foreach(UEBuildPlugin BuildPlugin in BuildPlugins.Where(x => x.Descriptor.PreBuildSteps != null))
			{
				AddCustomBuildSteps(BuildPlugin.Descriptor.PreBuildSteps, BuildPlugin, PreBuildCommandBatches);
			}
			PreBuildStepScripts = WriteCustomBuildStepScripts(BuildHostPlatform.Current.Platform, ScriptDirectory, "PreBuild", PreBuildCommandBatches);

			// Find all the post-build steps
			List<Tuple<string[], UEBuildPlugin>> PostBuildCommandBatches = new List<Tuple<string[], UEBuildPlugin>>();
			if(ProjectDescriptor != null && ProjectDescriptor.PostBuildSteps != null)
			{
				AddCustomBuildSteps(ProjectDescriptor.PostBuildSteps, null, PostBuildCommandBatches);
			}
			if(Rules.PostBuildSteps.Count > 0)
			{
				PostBuildCommandBatches.Add(new Tuple<string[], UEBuildPlugin>(Rules.PostBuildSteps.ToArray(), null));
			}
			foreach(UEBuildPlugin BuildPlugin in BuildPlugins.Where(x => x.Descriptor.PostBuildSteps != null))
			{
				AddCustomBuildSteps(BuildPlugin.Descriptor.PostBuildSteps, BuildPlugin, PostBuildCommandBatches);
			}
			PostBuildStepScripts = WriteCustomBuildStepScripts(BuildHostPlatform.Current.Platform, ScriptDirectory, "PostBuild", PostBuildCommandBatches);
		}

		/// <summary>
		/// Adds custom build steps from the given JSON object to the list of command batches
		/// </summary>
		/// <param name="BuildSteps">The custom build steps</param>
		/// <param name="Plugin">The plugin to associate with these commands</param>
		/// <param name="CommandBatches">List to receive the command batches</param>
		private void AddCustomBuildSteps(CustomBuildSteps BuildSteps, UEBuildPlugin Plugin, List<Tuple<string[], UEBuildPlugin>> CommandBatches)
		{
			string[] Commands;
			if(BuildSteps.TryGetCommands(BuildHostPlatform.Current.Platform, out Commands))
			{
				CommandBatches.Add(Tuple.Create(Commands, Plugin));
			}
		}

		/// <summary>
		/// Write scripts containing the custom build steps for the given host platform
		/// </summary>
		/// <param name="HostPlatform">The current host platform</param>
		/// <param name="Directory">The output directory for the scripts</param>
		/// <param name="FilePrefix">Bare prefix for all the created script files</param>
		/// <param name="CommandBatches">List of custom build steps, and their matching PluginInfo (if appropriate)</param>
		/// <returns>List of created script files</returns>
		private FileReference[] WriteCustomBuildStepScripts(UnrealTargetPlatform HostPlatform, DirectoryReference Directory, string FilePrefix, List<Tuple<string[], UEBuildPlugin>> CommandBatches)
		{
			List<FileReference> ScriptFiles = new List<FileReference>();
			foreach(Tuple<string[], UEBuildPlugin> CommandBatch in CommandBatches)
			{
				// Find all the standard variables
				Dictionary<string, string> Variables = new Dictionary<string,string>();
				Variables.Add("EngineDir", UnrealBuildTool.EngineDirectory.FullName);
				Variables.Add("ProjectDir", ProjectDirectory.FullName);
				Variables.Add("TargetName", TargetName);
				Variables.Add("TargetPlatform", Platform.ToString());
				Variables.Add("TargetConfiguration", Configuration.ToString());
				Variables.Add("TargetType", TargetType.ToString());
				if(ProjectFile != null)
				{
					Variables.Add("ProjectFile", ProjectFile.FullName);
				}
				if(CommandBatch.Item2 != null)
				{
					Variables.Add("PluginDir", CommandBatch.Item2.Directory.FullName);
				}

				// Get the output path to the script
				string ScriptExtension = (HostPlatform == UnrealTargetPlatform.Win64)? ".bat" : ".sh";
				FileReference ScriptFile = FileReference.Combine(Directory, String.Format("{0}-{1}{2}", FilePrefix, ScriptFiles.Count + 1, ScriptExtension));

				// Write it to disk
				List<string> Contents = new List<string>();
				if(HostPlatform == UnrealTargetPlatform.Win64)
				{
					Contents.Insert(0, "@echo off");
				}
				foreach(string Command in CommandBatch.Item1)
				{
					Contents.Add(Utils.ExpandVariables(Command, Variables));
				}
				File.WriteAllLines(ScriptFile.FullName, Contents);

				// Add the output file to the list of generated scripts
				ScriptFiles.Add(ScriptFile);
			}
			return ScriptFiles.ToArray();
		}

		/// <summary>
		/// Executes the custom pre-build steps
		/// </summary>
		public bool ExecuteCustomPreBuildSteps()
		{
			return Utils.ExecuteCustomBuildSteps(PreBuildStepScripts);
		}

		/// <summary>
		/// Executes the custom post-build steps
		/// </summary>
		public bool ExecuteCustomPostBuildSteps()
		{
			return Utils.ExecuteCustomBuildSteps(PostBuildStepScripts);
		}

		private static FileReference AddModuleFilenameSuffix(string ModuleName, FileReference FilePath, string Suffix)
		{
			int MatchPos = FilePath.FullName.LastIndexOf(ModuleName, StringComparison.InvariantCultureIgnoreCase);
			if (MatchPos < 0)
			{
				throw new BuildException("Failed to find module name \"{0}\" specified on the command line inside of the output filename \"{1}\" to add appendage.", ModuleName, FilePath);
			}
			string Appendage = "-" + Suffix;
			return new FileReference(FilePath.FullName.Insert(MatchPos + ModuleName.Length, Appendage));
		}

		private static List<UEBuildBinary> GetFilteredOnlyModules(List<UEBuildBinary> Binaries, List<OnlyModule> OnlyModules)
		{
			List<UEBuildBinary> Result = new List<UEBuildBinary>();

			foreach (UEBuildBinary Binary in Binaries)
			{
				// If we're doing an OnlyModule compile, we never want the executable that static libraries are linked into for monolithic builds
				if(Binary.Config.Type != UEBuildBinaryType.Executable)
				{
					OnlyModule FoundOnlyModule = Binary.FindOnlyModule(OnlyModules);
					if (FoundOnlyModule != null)
					{
						Result.Add(Binary);

						if (!String.IsNullOrEmpty(FoundOnlyModule.OnlyModuleSuffix))
						{
							Binary.Config.OriginalOutputFilePaths = Binary.Config.OutputFilePaths;
							Binary.Config.OutputFilePaths = Binary.Config.OutputFilePaths.Select(Path => AddModuleFilenameSuffix(FoundOnlyModule.OnlyModuleName, Path, FoundOnlyModule.OnlyModuleSuffix)).ToList();
						}
					}
				}
			}

			return Result;
		}

		private List<UEBuildBinary> GetFilteredGameModules(List<UEBuildBinary> Binaries)
		{
			List<UEBuildBinary> Result = new List<UEBuildBinary>();

			foreach (UEBuildBinary DLLBinary in Binaries)
			{
				List<UEBuildModule> GameModules = DLLBinary.FindGameModules();
				if (GameModules != null && GameModules.Count > 0)
				{
					if(!UnrealBuildTool.IsProjectInstalled() || EnabledPlugins.Where(x => x.Type == PluginType.Mod).Any(x => DLLBinary.Config.OutputFilePaths[0].IsUnderDirectory(x.Directory)))
					{
						Result.Add(DLLBinary);

						string UniqueSuffix = (new Random((int)(DateTime.Now.Ticks % Int32.MaxValue)).Next(10000)).ToString();

						DLLBinary.Config.OriginalOutputFilePaths = DLLBinary.Config.OutputFilePaths;
						DLLBinary.Config.OutputFilePaths = DLLBinary.Config.OutputFilePaths.Select(Path => AddModuleFilenameSuffix(GameModules[0].Name, Path, UniqueSuffix)).ToList();
					}
				}
			}

			return Result;
		}

		/// <summary>
		/// All non-program monolithic binaries implicitly depend on all static plugin libraries so they are always linked appropriately
		/// In order to do this, we create a new module here with a cpp file we emit that invokes an empty function in each library.
		/// If we do not do this, there will be no static initialization for libs if no symbols are referenced in them.
		/// </summary>
		private void CreateLinkerFixupsCPPFile(CppCompileEnvironment GlobalCompileEnvironment)
		{
			UEBuildBinary ExecutableBinary = AppBinaries[0];

			List<string> PrivateDependencyModuleNames = new List<string>();

			foreach (UEBuildModule TargetModule in ExecutableBinary.Modules)
			{
				ModuleRules CheckRules = TargetModule.Rules;
				if (CheckRules.Type != ModuleRules.ModuleType.External)
				{
					PrivateDependencyModuleNames.Add(TargetModule.Name);
				}
			}

			// We ALWAYS have to write this file as the IMPLEMENT_PRIMARY_GAME_MODULE function depends on the UELinkerFixups function existing.
			{
				string LinkerFixupsName = "UELinkerFixups";

				// Include an empty header so UEBuildModule.Compile does not complain about a lack of PCH
				string HeaderFilename = LinkerFixupsName + "Name.h";
				FileReference LinkerFixupHeaderFilenameWithPath = FileReference.Combine(GlobalCompileEnvironment.OutputDirectory, HeaderFilename);

				// Create the cpp filename
				FileReference LinkerFixupCPPFilename = FileReference.Combine(GlobalCompileEnvironment.OutputDirectory, LinkerFixupsName + ".cpp");
				if (!FileReference.Exists(LinkerFixupCPPFilename))
				{
					// Create a dummy file in case it doesn't exist yet so that the module does not complain it's not there
					ResponseFile.Create(LinkerFixupHeaderFilenameWithPath, new List<string>());
					ResponseFile.Create(LinkerFixupCPPFilename, new List<string>(new string[] { String.Format("#include \"{0}\"", LinkerFixupHeaderFilenameWithPath) }));
				}

				// Create the source file list (just the one cpp file)
				List<FileItem> SourceFiles = new List<FileItem>();
				FileItem LinkerFixupCPPFileItem = FileItem.GetItemByFileReference(LinkerFixupCPPFilename);
				SourceFiles.Add(LinkerFixupCPPFileItem);

				// Create the CPP module
				DirectoryReference FakeModuleDirectory = LinkerFixupCPPFilename.Directory;
				UEBuildModuleCPP NewModule = CreateArtificialModule(LinkerFixupsName, FakeModuleDirectory, SourceFiles, PrivateDependencyModuleNames);

				// Now bind this new module to the executable binary so it will link the plugin libs correctly
				NewModule.bSkipDefinitionsForCompileEnvironment = true;
				NewModule.Rules.PCHUsage = ModuleRules.PCHUsageMode.NoSharedPCHs;
				NewModule.RecursivelyCreateModules(FindOrCreateModuleByName, LinkerFixupsName);
				BindArtificialModuleToBinary(NewModule, ExecutableBinary, GlobalCompileEnvironment);

				// Create the cpp file
				NewModule.bSkipDefinitionsForCompileEnvironment = false;
				List<string> LinkerFixupsFileContents = GenerateLinkerFixupsContents(ExecutableBinary, NewModule.CreateModuleCompileEnvironment(Rules, GlobalCompileEnvironment), HeaderFilename, LinkerFixupsName, PrivateDependencyModuleNames);
				NewModule.bSkipDefinitionsForCompileEnvironment = true;

				// Determine if the file changed. Write it if it either doesn't exist or the contents are different.
				bool bShouldWriteFile = true;
				if (FileReference.Exists(LinkerFixupCPPFilename))
				{
					string[] ExistingFixupText = File.ReadAllLines(LinkerFixupCPPFilename.FullName);
					string JoinedNewContents = string.Join("", LinkerFixupsFileContents.ToArray());
					string JoinedOldContents = string.Join("", ExistingFixupText);
					bShouldWriteFile = (JoinedNewContents != JoinedOldContents);
				}

				// If we determined that we should write the file, write it now.
				if (bShouldWriteFile)
				{
					ResponseFile.Create(LinkerFixupHeaderFilenameWithPath, new List<string>());
					ResponseFile.Create(LinkerFixupCPPFilename, LinkerFixupsFileContents);

					// Update the cached file states so that the linker fixups definitely get rebuilt
					FileItem.GetItemByFileReference(LinkerFixupHeaderFilenameWithPath).ResetFileInfo();
					LinkerFixupCPPFileItem.ResetFileInfo();
				}
			}
		}

		private List<string> GenerateLinkerFixupsContents(UEBuildBinary ExecutableBinary, CppCompileEnvironment CompileEnvironment, string HeaderFilename, string LinkerFixupsName, List<string> PrivateDependencyModuleNames)
		{
			List<string> Result = new List<string>();

			Result.Add("#include \"" + HeaderFilename + "\"");

			// To reduce the size of the command line for the compiler, we're going to put all definitions inside of the cpp file.
			foreach (string Definition in CompileEnvironment.Definitions)
			{
				string MacroName;
				string MacroValue = String.Empty;
				int EqualsIndex = Definition.IndexOf('=');
				if (EqualsIndex >= 0)
				{
					MacroName = Definition.Substring(0, EqualsIndex);
					MacroValue = Definition.Substring(EqualsIndex + 1);
				}
				else
				{
					MacroName = Definition;
				}
				Result.Add("#ifndef " + MacroName);
				Result.Add(String.Format("\t#define {0} {1}", MacroName, MacroValue));
				Result.Add("#endif");
			}

			// Write functions for accessing embedded pak signing keys
			String EncryptionKey;
			String[] PakSigningKeys;
			GetEncryptionAndSigningKeys(out EncryptionKey, out PakSigningKeys);
			bool bRegisterEncryptionKey = false;
			bool bRegisterPakSigningKeys = false;

			if (!string.IsNullOrEmpty(EncryptionKey))
			{
				Result.Add("extern void RegisterEncryptionKey(const char*);");
				bRegisterEncryptionKey = true;
			}

			if (PakSigningKeys != null && PakSigningKeys.Length == 3 && !string.IsNullOrEmpty(PakSigningKeys[1]) && !string.IsNullOrEmpty(PakSigningKeys[2]))
			{
				Result.Add("extern void RegisterPakSigningKeys(const char*, const char*);");
				bRegisterPakSigningKeys = true;
			}

			if (bRegisterEncryptionKey || bRegisterPakSigningKeys)
			{
				Result.Add("struct FEncryptionAndSigningKeyRegistration");
				Result.Add("{");
				Result.Add("\tFEncryptionAndSigningKeyRegistration()");
				Result.Add("\t{");
				if (bRegisterEncryptionKey)
				{
					Result.Add(string.Format("\t\tRegisterEncryptionKey(\"{0}\");", EncryptionKey));
				}
				if (bRegisterPakSigningKeys)
				{
					Result.Add(string.Format("\t\tRegisterPakSigningKeys(\"{0}\", \"{1}\");", PakSigningKeys[2], PakSigningKeys[1]));
				}
				Result.Add("\t}");
				Result.Add("};");
				Result.Add("FEncryptionAndSigningKeyRegistration GEncryptionAndSigningKeyRegistration;");
			}

			// Add a function that is not referenced by anything that invokes all the empty functions in the different static libraries
			Result.Add("void " + LinkerFixupsName + "()");
			Result.Add("{");

			// Fill out the body of the function with the empty function calls. This is what causes the static libraries to be considered relevant
			List<UEBuildModule> DependencyModules = ExecutableBinary.GetAllDependencyModules(bIncludeDynamicallyLoaded: false, bForceCircular: false);
			HashSet<string> AlreadyAddedEmptyLinkFunctions = new HashSet<string>();
			foreach (UEBuildModuleCPP BuildModuleCPP in DependencyModules.OfType<UEBuildModuleCPP>().Where(CPPModule => CPPModule.AutoGenerateCppInfo != null))
			{
				IEnumerable<string> GeneratedCppFilesWithTheFunction = BuildModuleCPP.FindGeneratedCppFiles();

				bool bFound = false;
				foreach (string CppFile in GeneratedCppFilesWithTheFunction)
				{
					bFound = true;

					string FunctionName = "EmptyLinkFunctionForGeneratedCode" + Path.GetFileName(CppFile).Replace(".gen.cpp", "").Replace(".", "_");
					if (AlreadyAddedEmptyLinkFunctions.Add(FunctionName))
					{
						Result.Add("    extern void " + FunctionName + "();");
						Result.Add("    " + FunctionName + "();");
					}
				}

				if (!bFound)
				{
					Result.Add("    //" + BuildModuleCPP.Name + " has no generated files, path: " + BuildModuleCPP.GeneratedCodeDirectory.ToString());
				}
			}
			foreach (string DependencyModuleName in PrivateDependencyModuleNames)
			{
				Result.Add("    extern void EmptyLinkFunctionForStaticInitialization" + DependencyModuleName + "();");
				Result.Add("    EmptyLinkFunctionForStaticInitialization" + DependencyModuleName + "();");
			}

			// End the function body that was started above
			Result.Add("}");

			return Result;
		}

		/// <summary>
		/// Binds artificial module to given binary.
		/// </summary>
		/// <param name="Module">Module to bind.</param>
		/// <param name="Binary">Binary to bind.</param>
		/// <param name="GlobalCompileEnvironment">The global C++ compile environment</param>
		private void BindArtificialModuleToBinary(UEBuildModuleCPP Module, UEBuildBinary Binary, CppCompileEnvironment GlobalCompileEnvironment)
		{
			Module.Binary = Binary;

			// Process dependencies for this new module
			Module.CachePCHUsageForModuleSourceFiles(Rules, Module.CreateModuleCompileEnvironment(Rules, GlobalCompileEnvironment));

			// Add module to binary
			Binary.AddModule(Module);
		}

		/// <summary>
		/// Creates artificial module.
		/// </summary>
		/// <param name="Name">Name of the module.</param>
		/// <param name="Directory">Directory of the module.</param>
		/// <param name="SourceFiles">Source files.</param>
		/// <param name="PrivateDependencyModuleNames">Private dependency list.</param>
		/// <returns>Created module.</returns>
		private UEBuildModuleCPP CreateArtificialModule(string Name, DirectoryReference Directory, IEnumerable<FileItem> SourceFiles, IEnumerable<string> PrivateDependencyModuleNames)
		{
			ModuleRules Rules = new ModuleRules(this.Rules);
			Rules.PrivateDependencyModuleNames.AddRange(PrivateDependencyModuleNames);

			return new UEBuildModuleCPP(
				InName: Name,
				InType: UHTModuleType.GameRuntime,
				InModuleDirectory: Directory,
				InGeneratedCodeDirectory: null,
				InIntelliSenseGatherer: null,
				InSourceFiles: SourceFiles.ToList(),
				InRules: Rules,
				bInBuildSourceFiles: true,
				InRulesFile: null,
				InRuntimeDependencies: new List<RuntimeDependency>());
		}

		/// <summary>
		/// Determines which modules can be used to create shared PCHs
		/// </summary>
		/// <returns>List of shared PCH modules, in order of preference</returns>
		List<PrecompiledHeaderTemplate> FindSharedPCHs(List<UEBuildBinary> OriginalBinaries, CppCompileEnvironment GlobalCompileEnvironment)
		{
			// Find how many other shared PCH modules each module depends on, and use that to sort the shared PCHs by reverse order of size.
			HashSet<UEBuildModuleCPP> SharedPCHModules = new HashSet<UEBuildModuleCPP>();
			foreach(UEBuildBinary Binary in OriginalBinaries)
			{
				foreach(UEBuildModuleCPP Module in Binary.Modules.OfType<UEBuildModuleCPP>())
				{
					if(Module.Rules.SharedPCHHeaderFile != null)
					{
						SharedPCHModules.Add(Module);
					}
				}
			}

			// Shared PCHs are only supported for engine modules at the moment. Check there are no game modules in the list.
			List<UEBuildModuleCPP> NonEngineSharedPCHs = SharedPCHModules.Where(x => !x.RulesFile.IsUnderDirectory(UnrealBuildTool.EngineDirectory)).ToList();
			if(NonEngineSharedPCHs.Count > 0)
			{
				throw new BuildException("Shared PCHs are only supported for engine modules (found {0}).", String.Join(", ", NonEngineSharedPCHs.Select(x => x.Name)));
			}

			// Find a priority for each shared PCH, determined as the number of other shared PCHs it includes.
			Dictionary<UEBuildModuleCPP, int> SharedPCHModuleToPriority = new Dictionary<UEBuildModuleCPP, int>();
			foreach(UEBuildModuleCPP SharedPCHModule in SharedPCHModules)
			{
				List<UEBuildModule> Dependencies = new List<UEBuildModule>();
				SharedPCHModule.GetAllDependencyModules(Dependencies, new HashSet<UEBuildModule>(), false, false, false);
				SharedPCHModuleToPriority.Add(SharedPCHModule, Dependencies.Count(x => SharedPCHModules.Contains(x)));
			}

			// Create the shared PCH modules, in order
			List<PrecompiledHeaderTemplate> OrderedSharedPCHModules = new List<PrecompiledHeaderTemplate>();
			foreach(UEBuildModuleCPP Module in SharedPCHModuleToPriority.OrderByDescending(x => x.Value).Select(x => x.Key))
			{
				OrderedSharedPCHModules.Add(Module.CreateSharedPCHTemplate(this, GlobalCompileEnvironment));
			}

			// Print the ordered list of shared PCHs
			if(OrderedSharedPCHModules.Count > 0)
			{
				Log.TraceVerbose("Found {0} shared PCH headers (listed in order of preference):", SharedPCHModules.Count);
				foreach (PrecompiledHeaderTemplate SharedPCHModule in OrderedSharedPCHModules)
				{
					Log.TraceVerbose("	" + SharedPCHModule.Module.Name);
				}
			}
			return OrderedSharedPCHModules;
		}

		/// When building a target, this is called to add any additional modules that should be compiled along
		/// with the main target.  If you override this in a derived class, remember to call the base implementation!
		protected virtual void AddExtraModules()
		{
			// Add extra modules that will either link into the main binary (monolithic), or be linked into separate DLL files (modular)
			foreach (string ModuleName in ExtraModuleNames)
			{
				UEBuildModule Module = FindOrCreateModuleByName(ModuleName, TargetRulesFile.GetFileName());
				if (Module.Binary == null)
				{
					AddModuleToBinary(Module, false);
				}
			}
		}

		/// <summary>
		/// Adds all the precompiled modules into the target. Precompiled modules are compiled alongside the target, but not linked into it unless directly referenced.
		/// </summary>
		protected void AddPrecompiledModules()
		{
			if (bPrecompile || bUsePrecompiled)
			{
				// Find all the modules that are part of the target
				List<UEBuildModule> PrecompiledModules = new List<UEBuildModule>();
				foreach (UEBuildModuleCPP Module in Modules.Values.OfType<UEBuildModuleCPP>())
				{
					if(Module.Binary != null && Module.RulesFile.IsUnderDirectory(UnrealBuildTool.EngineDirectory) && !PrecompiledModules.Contains(Module))
					{
						PrecompiledModules.Add(Module);
					}
				}

				// If we're precompiling a base engine target, create binaries for all the engine modules that are compatible with it.
				if (bPrecompile && ProjectFile == null && TargetType != TargetType.Program)
				{
					// Find all the known module names in this assembly
					List<string> ModuleNames = new List<string>();
					RulesAssembly.GetAllModuleNames(ModuleNames);

					// Find all the platform folders to exclude from the list of precompiled modules
					List<string> ExcludeFolders = new List<string>();
					foreach (UnrealTargetPlatform TargetPlatform in Enum.GetValues(typeof(UnrealTargetPlatform)))
					{
						if (TargetPlatform != Platform)
						{
							string DirectoryFragment = Path.DirectorySeparatorChar + TargetPlatform.ToString() + Path.DirectorySeparatorChar;
							ExcludeFolders.Add(DirectoryFragment);
						}
					}

					// Also exclude all the platform groups that this platform is not a part of
					List<UnrealPlatformGroup> IncludePlatformGroups = new List<UnrealPlatformGroup>(UEBuildPlatform.GetPlatformGroups(Platform));
					foreach (UnrealPlatformGroup PlatformGroup in Enum.GetValues(typeof(UnrealPlatformGroup)))
					{
						if (!IncludePlatformGroups.Contains(PlatformGroup))
						{
							string DirectoryFragment = Path.DirectorySeparatorChar + PlatformGroup.ToString() + Path.DirectorySeparatorChar;
							ExcludeFolders.Add(DirectoryFragment);
						}
					}

					// Find all the directories containing engine modules that may be compatible with this target
					List<DirectoryReference> Directories = new List<DirectoryReference>();
					if (TargetType == TargetType.Editor)
					{
						Directories.Add(UnrealBuildTool.EngineSourceEditorDirectory);
					}
					Directories.Add(UnrealBuildTool.EngineSourceRuntimeDirectory);

					// Also allow anything in the developer directory in non-shipping configurations (though we blacklist by default unless the PrecompileForTargets
					// setting indicates that it's actually useful at runtime).
					bool bAllowDeveloperModules = false;
					if(Configuration != UnrealTargetConfiguration.Shipping)
					{
						Directories.Add(UnrealBuildTool.EngineSourceDeveloperDirectory);
						bAllowDeveloperModules = true;
					}

					// Find all the modules that are not part of the standard set
					HashSet<string> FilteredModuleNames = new HashSet<string>();
					foreach (string ModuleName in ModuleNames)
					{
						FileReference ModuleFileName = RulesAssembly.GetModuleFileName(ModuleName);
						if (Directories.Any(x => ModuleFileName.IsUnderDirectory(x)))
						{
							string RelativeFileName = ModuleFileName.MakeRelativeTo(UnrealBuildTool.EngineSourceDirectory);
							if (ExcludeFolders.All(x => RelativeFileName.IndexOf(x, StringComparison.InvariantCultureIgnoreCase) == -1) && !PrecompiledModules.Any(x => x.Name == ModuleName))
							{
								FilteredModuleNames.Add(ModuleName);
							}
						}
					}

					// Add all the plugins which aren't already being built
					foreach(UEBuildPlugin PrecompilePlugin in PrecompilePlugins)
					{
						foreach (ModuleDescriptor ModuleDescriptor in PrecompilePlugin.Descriptor.Modules)
						{
							if (ModuleDescriptor.IsCompiledInConfiguration(Platform, TargetType, bAllowDeveloperModules && Rules.bBuildDeveloperTools, Rules.bBuildEditor, Rules.bBuildRequiresCookedData))
							{
								string RelativeFileName = RulesAssembly.GetModuleFileName(ModuleDescriptor.Name).MakeRelativeTo(UnrealBuildTool.EngineDirectory);
								if (!ExcludeFolders.Any(x => RelativeFileName.Contains(x)) && !PrecompiledModules.Any(x => x.Name == ModuleDescriptor.Name))
								{
									FilteredModuleNames.Add(ModuleDescriptor.Name);
								}
							}
						}
					}

					// Create rules for each remaining module, and check that it's set to be precompiled
					const string PrecompileReferenceChain = "precompile option";
					foreach(string FilteredModuleName in FilteredModuleNames)
					{
						FileReference ModuleFileName = null;

						// Try to create the rules object, but catch any exceptions if it fails. Some modules (eg. SQLite) may determine that they are unavailable in the constructor.
						ModuleRules ModuleRules;
						try
						{
							ModuleRules = RulesAssembly.CreateModuleRules(FilteredModuleName, this.Rules, PrecompileReferenceChain, out ModuleFileName);
						}
						catch (BuildException)
						{
							ModuleRules = null;
						}

						// Figure out if it can be precompiled
						bool bCanPrecompile = false;
						if (ModuleRules != null && ModuleRules.Type == ModuleRules.ModuleType.CPlusPlus)
						{
							switch (ModuleRules.PrecompileForTargets)
							{
								case ModuleRules.PrecompileTargetsType.None:
									bCanPrecompile = false;
									break;
								case ModuleRules.PrecompileTargetsType.Default:
									bCanPrecompile = !ModuleFileName.IsUnderDirectory(UnrealBuildTool.EngineSourceDeveloperDirectory) || TargetType == TargetType.Editor;
									break;
								case ModuleRules.PrecompileTargetsType.Game:
									bCanPrecompile = (TargetType == TargetType.Client || TargetType == TargetType.Server || TargetType == TargetType.Game);
									break;
								case ModuleRules.PrecompileTargetsType.Editor:
									bCanPrecompile = (TargetType == TargetType.Editor);
									break;
								case ModuleRules.PrecompileTargetsType.Any:
									bCanPrecompile = true;
									break;
							}
						}

						// Create the module
						if (bCanPrecompile)
						{
							UEBuildModule Module = FindOrCreateModuleByName(FilteredModuleName, PrecompileReferenceChain);
							Module.RecursivelyCreateModules(FindOrCreateModuleByName, PrecompileReferenceChain);
							PrecompiledModules.Add(Module);
						}
					}
				}

				// In monolithic, make sure every module is compiled into a static library first. Even if it's linked into an existing executable, we can add it into a static library and link it into
				// the executable afterwards. In modular builds, just compile every module into a DLL.
				UEBuildBinaryType PrecompiledBinaryType = bCompileMonolithic ? UEBuildBinaryType.StaticLibrary : UEBuildBinaryType.DynamicLinkLibrary;
				foreach (UEBuildModule PrecompiledModule in PrecompiledModules)
				{
					if (PrecompiledModule.Binary == null || (bCompileMonolithic && PrecompiledModule.Binary.Config.Type != UEBuildBinaryType.StaticLibrary))
					{
						PrecompiledModule.Binary = CreateBinaryForModule(PrecompiledModule, PrecompiledBinaryType, bAllowCompilation: !bUsePrecompiled, bIsCrossTarget: false, bPrecompileOnly: bPrecompile);
					}
					else if (bUsePrecompiled && !ProjectFileGenerator.bGenerateProjectFiles)
					{
						// Even if there's already a binary for this module, we never want to compile it.
						PrecompiledModule.Binary.Config.bAllowCompilation = false;
					}
				}
			}
		}

		public void AddModuleToBinary(UEBuildModule Module, bool bIsCrossTarget)
		{
			if (ShouldCompileMonolithic())
			{
				// When linking monolithically, any unbound modules will be linked into the main executable
				Module.Binary = AppBinaries[0];
				Module.Binary.AddModule(Module);
			}
			else
			{
				// Otherwise create a new module for it
				Module.Binary = CreateBinaryForModule(Module, UEBuildBinaryType.DynamicLinkLibrary, bAllowCompilation: true, bIsCrossTarget: bIsCrossTarget, bPrecompileOnly: false);
			}

            if (Module.Binary == null)
            {
                throw new BuildException("Failed to set up binary for module {0}", Module.Name);
            }
		}

		/// <summary>
		/// Adds a binary for the given module. Does not check whether a binary already exists, or whether a binary should be created for this build configuration.
		/// </summary>
		/// <param name="Module">The module to create a binary for</param>
		/// <param name="BinaryType">Type of binary to be created</param>
		/// <param name="bAllowCompilation">Whether this binary can be compiled. The function will check whether plugin binaries can be compiled.</param>
		/// <param name="bIsCrossTarget"></param>
		/// <param name="bPrecompileOnly">This module is not part of the target, but is being built due to -precompile</param>
		/// <returns>The new binary</returns>
		private UEBuildBinary CreateBinaryForModule(UEBuildModule Module, UEBuildBinaryType BinaryType, bool bAllowCompilation, bool bIsCrossTarget, bool bPrecompileOnly)
		{
			// Get the plugin info for this module
			PluginInfo Plugin = null;
			if (Module.RulesFile != null)
			{
				RulesAssembly.TryGetPluginForModule(Module.RulesFile, out Plugin);
			}

			// Get the root output directory and base name (target name/app name) for this binary
			DirectoryReference BaseOutputDirectory;
			if (Plugin != null)
			{
				BaseOutputDirectory = Plugin.Directory;
			}
			else if (RulesAssembly.IsGameModule(Module.Name) || !bUseSharedBuildEnvironment)
			{
				BaseOutputDirectory = ProjectDirectory;
			}
			else
			{
				if (Module.ModuleDirectory.IsUnderDirectory(UnrealBuildTool.EnterpriseDirectory))
				{
					BaseOutputDirectory = UnrealBuildTool.EnterpriseDirectory;
				}
				else
				{
					BaseOutputDirectory = UnrealBuildTool.EngineDirectory;
				}
			}

			// Get the configuration that this module will be built in. Engine modules compiled in DebugGame will use Development.
			UnrealTargetConfiguration ModuleConfiguration = Configuration;
			if (Configuration == UnrealTargetConfiguration.DebugGame && !RulesAssembly.IsGameModule(Module.Name))
			{
				ModuleConfiguration = UnrealTargetConfiguration.Development;
			}

			// Get the output and intermediate directories for this module
			DirectoryReference OutputDirectory = DirectoryReference.Combine(BaseOutputDirectory, "Binaries", Platform.ToString());
			DirectoryReference IntermediateDirectory = DirectoryReference.Combine(BaseOutputDirectory, PlatformIntermediateFolder, AppName, ModuleConfiguration.ToString());

			// Append a subdirectory if the module rules specifies one
			if (Module.Rules != null && !String.IsNullOrEmpty(Module.Rules.BinariesSubFolder))
			{
				OutputDirectory = DirectoryReference.Combine(OutputDirectory, Module.Rules.BinariesSubFolder);
				IntermediateDirectory = DirectoryReference.Combine(IntermediateDirectory, Module.Rules.BinariesSubFolder);
			}

            // Get the output filenames
            FileReference BaseBinaryPath = FileReference.Combine(OutputDirectory, MakeBinaryFileName(AppName + "-" + Module.Name, Platform, ModuleConfiguration, Architecture, Rules.UndecoratedConfiguration, BinaryType, Rules.OverrideExecutableFileExtension));
			List<FileReference> OutputFilePaths = UEBuildPlatform.GetBuildPlatform(Platform).FinalizeBinaryPaths(BaseBinaryPath, ProjectFile, Rules);

			// Prepare the configuration object
			UEBuildBinaryConfiguration Config = new UEBuildBinaryConfiguration(
				InType: BinaryType,
				InOutputFilePaths: OutputFilePaths,
				InIntermediateDirectory: IntermediateDirectory,
				bInHasModuleRules: Module.Rules != null,
				bInAllowExports: BinaryType == UEBuildBinaryType.DynamicLinkLibrary,
				bInAllowCompilation: bAllowCompilation,
				bInIsCrossTarget: bIsCrossTarget,
				bInPrecompileOnly: bPrecompileOnly,
				InModuleNames: new string[] { Module.Name }
			);

			// Create the new binary
			UEBuildBinary Binary = new UEBuildBinary(Config);
			Binary.AddModule(Module);
			AppBinaries.Add(Binary);
			return Binary;
		}

        /// <summary>
        /// Makes a filename (without path) for a compiled binary (e.g. "Core-Win64-Debug.lib") */
        /// </summary>
        /// <param name="BinaryName">The name of this binary</param>
        /// <param name="Platform">The platform being built for</param>
        /// <param name="Configuration">The configuration being built</param>
		/// <param name="Architecture">The target architecture being built</param>
        /// <param name="UndecoratedConfiguration">The target configuration which doesn't require a platform and configuration suffix. Development by default.</param>
        /// <param name="BinaryType">Type of binary</param>
        /// /// <param name="OverrideExecutableFileExtension">If not empty, will override the file extension of the executable</param>
        /// <returns>Name of the binary</returns>
        public static string MakeBinaryFileName(string BinaryName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string Architecture, UnrealTargetConfiguration UndecoratedConfiguration, UEBuildBinaryType BinaryType, string OverrideExecutableFileExtension)
		{
			StringBuilder Result = new StringBuilder();

			if (Platform == UnrealTargetPlatform.Linux && (BinaryType == UEBuildBinaryType.DynamicLinkLibrary || BinaryType == UEBuildBinaryType.StaticLibrary))
			{
				Result.Append("lib");
			}

			Result.Append(BinaryName);

			if (Configuration != UndecoratedConfiguration)
			{
				Result.AppendFormat("-{0}-{1}", Platform.ToString(), Configuration.ToString());
			}

			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
			if(BuildPlatform.RequiresArchitectureSuffix())
			{
				Result.Append(Architecture);
			}

			if (!String.IsNullOrEmpty(OverrideExecutableFileExtension))
			{
				Result.Append(OverrideExecutableFileExtension);
			}
			else
			{
				Result.Append(BuildPlatform.GetBinaryExtension(BinaryType));
			}

            return Result.ToString();
		}

        /// <summary>
        /// Determine the output path for a target's executable
        /// </summary>
        /// <param name="BaseDirectory">The base directory for the executable; typically either the engine directory or project directory.</param>
        /// <param name="BinaryName">Name of the binary</param>
        /// <param name="Platform">Target platform to build for</param>
        /// <param name="Configuration">Target configuration being built</param>
		/// <param name="Architecture">Architecture being built</param>
        /// <param name="BinaryType">The type of binary we're compiling</param>
        /// <param name="UndecoratedConfiguration">The configuration which doesn't have a "-{Platform}-{Configuration}" suffix added to the binary</param>
        /// <param name="bIncludesGameModules">Whether this executable contains game modules</param>
        /// <param name="ExeSubFolder">Subfolder for executables. May be null.</param>
		/// <param name="OverrideExecutableFileExtension">Override for the executable file extension</param>
		/// <param name="ProjectFile">The project file containing the target being built</param>
		/// <param name="Rules">Rules for the target being built</param>
        /// <returns>List of executable paths for this target</returns>
        public static List<FileReference> MakeBinaryPaths(DirectoryReference BaseDirectory, string BinaryName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, UEBuildBinaryType BinaryType, string Architecture, UnrealTargetConfiguration UndecoratedConfiguration, bool bIncludesGameModules, string ExeSubFolder, string OverrideExecutableFileExtension, FileReference ProjectFile, ReadOnlyTargetRules Rules)
		{
			// Get the configuration for the executable. If we're building DebugGame, and this executable only contains engine modules, use the same name as development.
			UnrealTargetConfiguration ExeConfiguration = Configuration;
			if (Configuration == UnrealTargetConfiguration.DebugGame && !bIncludesGameModules)
			{
				ExeConfiguration = UnrealTargetConfiguration.Development;
			}

			// Build the binary path
			DirectoryReference BinaryDirectory = DirectoryReference.Combine(BaseDirectory, "Binaries", Platform.ToString());
			if (!String.IsNullOrEmpty(ExeSubFolder))
			{
				BinaryDirectory = DirectoryReference.Combine(BinaryDirectory, ExeSubFolder);
			}
			FileReference BinaryFile = FileReference.Combine(BinaryDirectory, MakeBinaryFileName(BinaryName, Platform, ExeConfiguration, Architecture, UndecoratedConfiguration, BinaryType, OverrideExecutableFileExtension));

			// Allow the platform to customize the output path (and output several executables at once if necessary)
			return UEBuildPlatform.GetBuildPlatform(Platform).FinalizeBinaryPaths(BinaryFile, ProjectFile, Rules);
		}

		/// <summary>
		/// Sets up the modules for the target.
		/// </summary>
		protected void SetupModules()
		{
			List<string> PlatformExtraModules = new List<string>();
			UEBuildPlatform.GetBuildPlatform(Platform).AddExtraModules(Rules, PlatformExtraModules);
			ExtraModuleNames.AddRange(PlatformExtraModules);
		}

		/// <summary>
		/// Sets up the plugins for this target
		/// </summary>
		protected virtual void SetupPlugins()
		{
			// Find all the valid plugins
			Dictionary<string, PluginInfo> NameToInfo = RulesAssembly.EnumeratePlugins().ToDictionary(x => x.Name, x => x, StringComparer.InvariantCultureIgnoreCase);

			// Remove any plugins for platforms we don't have
			List<UnrealTargetPlatform> MissingPlatforms = new List<UnrealTargetPlatform>();
			foreach (UnrealTargetPlatform TargetPlatform in Enum.GetValues(typeof(UnrealTargetPlatform)))
			{
				if (UEBuildPlatform.GetBuildPlatform(TargetPlatform, true) == null)
				{
					MissingPlatforms.Add(TargetPlatform);
				}
			}

			// Get an array of folders to filter out
			FileSystemName[] ExcludeFolders = MissingPlatforms.Select(x => new FileSystemName(x.ToString())).ToArray();

			// Set of all the plugins that have been referenced
			HashSet<string> ReferencedNames = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);

			// Map of plugin names to instances of that plugin
			Dictionary<string, UEBuildPlugin> NameToInstance = new Dictionary<string, UEBuildPlugin>(StringComparer.InvariantCultureIgnoreCase);

			// Find a map of plugins which are explicitly referenced in the project file
			if(ProjectDescriptor != null && ProjectDescriptor.Plugins != null)
			{
				string ProjectReferenceChain = ProjectFile.GetFileName();
				foreach(PluginReferenceDescriptor PluginReference in ProjectDescriptor.Plugins)
				{
					// Make sure we don't have multiple references to the same plugin
					if(!ReferencedNames.Add(PluginReference.Name))
					{
						Log.TraceWarning("Plugin '{0}' is listed multiple times in project file '{1}'.", PluginReference.Name, ProjectFile);
					}
					else
					{
						AddPlugin(PluginReference, ProjectReferenceChain, ExcludeFolders, NameToInstance, NameToInfo);
					}
				}
			}

			// Also synthesize references for plugins which are enabled by default
			if (Rules.bCompileAgainstEngine)
			{
				foreach(PluginInfo Plugin in NameToInfo.Values)
				{
					if(Plugin.EnabledByDefault && !ReferencedNames.Contains(Plugin.Name))
					{
						if (Plugin.Descriptor.bCanContainContent || Plugin.Descriptor.Modules.Any(x => x.IsCompiledInConfiguration(Platform, TargetType, Rules.bBuildDeveloperTools, Rules.bBuildEditor, Rules.bBuildRequiresCookedData)))
						{
							ReferencedNames.Add(Plugin.Name);

							PluginReferenceDescriptor PluginReference = new PluginReferenceDescriptor(Plugin.Name, null, true);
							PluginReference.bOptional = true;

							AddPlugin(PluginReference, "default plugins", ExcludeFolders, NameToInstance, NameToInfo);
						}
					}
				}
			}

			// If this is a program, synthesize references for plugins which are enabled via the config file
			if(TargetType == TargetType.Program)
			{
				ConfigHierarchy EngineConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Programs", TargetName), Platform);

				List<string> PluginNames;
				if(EngineConfig.GetArray("Plugins", "ProgramEnabledPlugins", out PluginNames))
				{
					foreach(string PluginName in PluginNames)
					{
						if(ReferencedNames.Add(PluginName))
						{
							PluginReferenceDescriptor PluginReference = new PluginReferenceDescriptor(PluginName, null, true);
							AddPlugin(PluginReference, "DefaultEngine.ini", ExcludeFolders, NameToInstance, NameToInfo);
						}
					}
				}
			}

			// Create the list of enabled plugins
			EnabledPlugins = new List<UEBuildPlugin>(NameToInstance.Values);

			// Try to add any the other plugins that are valid
			if(Rules.bBuildAllPlugins)
			{
				foreach(PluginInfo Plugin in NameToInfo.Values)
				{
					if(!NameToInstance.ContainsKey(Plugin.Name))
					{
						PluginReferenceDescriptor Reference = new PluginReferenceDescriptor(Plugin.Name, null, true);
						AddPlugin(Reference, TargetRulesFile.GetFileName(), ExcludeFolders, NameToInstance, NameToInfo);
					}
				}
			}

			// Add any foreign plugins to the list
			if (ForeignPlugins != null)
			{
				foreach (FileReference ForeignPlugin in ForeignPlugins)
				{
					PluginInfo ForeignPluginInfo = NameToInfo.Values.FirstOrDefault(x => x.File == ForeignPlugin);
					if (!NameToInstance.ContainsKey(ForeignPluginInfo.Name))
					{
						PluginReferenceDescriptor Reference = new PluginReferenceDescriptor(ForeignPluginInfo.Name, null, true);
						AddPlugin(Reference, "foreign plugin argument", ExcludeFolders, NameToInstance, NameToInfo);
					}
				}
			}

			// Set the list of plugins that should be built
			BuildPlugins = new List<UEBuildPlugin>(NameToInstance.Values);

			// Create the list of plugins to precompile. This is separate to the list of binaries being built, because
			// they and their dependencies won't be linked into the application binary by default on monolithic builds.
			if (bPrecompile && ProjectFile == null && TargetType != TargetType.Program)
			{
				foreach(PluginInfo Plugin in NameToInfo.Values)
				{
					if (!NameToInstance.ContainsKey(Plugin.Name) && Plugin.Descriptor.Modules != null)
					{
						PluginReferenceDescriptor Reference = new PluginReferenceDescriptor(Plugin.Name, null, true);
						AddPlugin(Reference, "precompile argument", ExcludeFolders, NameToInstance, NameToInfo);
					}
				}
				PrecompilePlugins = new List<UEBuildPlugin>(NameToInstance.Values.Except(BuildPlugins));
			}

			// Determine if the project has a script plugin. We will always build UHT if there is a script plugin in the game folder.
			bHasProjectScriptPlugin = EnabledPlugins.Any(x => x.Descriptor.bCanBeUsedWithUnrealHeaderTool && !x.File.IsUnderDirectory(UnrealBuildTool.EngineDirectory));
		}

		/// <summary>
		/// Creates a plugin instance from a reference to it
		/// </summary>
		/// <param name="Reference">Reference to the plugin</param>
		/// <param name="ReferenceChain">Textual representation of the chain of references, for error reporting</param>
		/// <param name="ExcludeFolders">Array of folder names to be excluded</param>
		/// <param name="NameToInstance">Map from plugin name to instance of it</param>
		/// <param name="NameToInfo">Map from plugin name to information</param>
		/// <returns>Instance of the plugin, or null if it should not be used</returns>
		private UEBuildPlugin AddPlugin(PluginReferenceDescriptor Reference, string ReferenceChain, FileSystemName[] ExcludeFolders, Dictionary<string, UEBuildPlugin> NameToInstance, Dictionary<string, PluginInfo> NameToInfo)
		{
			// Ignore disabled references
			if(!Reference.bEnabled)
			{
				return null;
			}

			// Try to get an existing reference to this plugin
			UEBuildPlugin Instance;
			if(NameToInstance.TryGetValue(Reference.Name, out Instance))
			{
				// If this is a non-optional reference, make sure that and every referenced dependency is staged
				if(!Reference.bOptional && !Instance.bDescriptorReferencedExplicitly)
				{
					Instance.bDescriptorReferencedExplicitly = true;
					if(Instance.Descriptor.Plugins != null)
					{
						foreach(PluginReferenceDescriptor NextReference in Instance.Descriptor.Plugins)
						{
							string NextReferenceChain = String.Format("{0} -> {1}", ReferenceChain, Instance.File.GetFileName());
							AddPlugin(NextReference, NextReferenceChain, ExcludeFolders, NameToInstance, NameToInfo);
						}
					}
				}
			}
			else
			{
				// Check if the plugin is required for this platform
				if(!Reference.IsEnabledForPlatform(Platform) || !Reference.IsEnabledForTarget(TargetType))
				{
					Log.TraceVerbose("Ignoring plugin '{0}' (referenced via {1}) for platform/configuration", Reference.Name, ReferenceChain);
					return null;
				}

				// Disable any plugin reference which does not support the target platform
				if (!Rules.bIncludePluginsForTargetPlatforms && !Reference.IsSupportedTargetPlatform(Platform))
				{
					Log.TraceVerbose("Ignoring plugin '{0}' (referenced via {1}) due to unsupported target platform.", Reference.Name, ReferenceChain);
					return null;
				}

				// Find the plugin being enabled
				PluginInfo Info;
				if(!NameToInfo.TryGetValue(Reference.Name, out Info))
				{
					if (Reference.bOptional)
					{
						return null;
					}
					else
					{
						throw new BuildException("Unable to find plugin '{0}' (referenced via {1}). Install it and try again, or remove it from the required plugin list.", Reference.Name, ReferenceChain);
					}
				}

				// Disable any plugin which does not support the target platform. The editor should update such references in the .uproject file on load.
				if (!Rules.bIncludePluginsForTargetPlatforms && !Info.Descriptor.SupportsTargetPlatform(Platform))
				{
					Log.TraceVerbose("Ignoring plugin '{0}' (referenced via {1}) due to target platform not supported by descriptor.", Reference.Name, ReferenceChain);
					return null;
				}

				// Disable any plugin that requires the build platform
				if(Info.Descriptor.bRequiresBuildPlatform && ShouldExcludePlugin(Info, ExcludeFolders))
				{
					Log.TraceVerbose("Ignoring plugin '{0}' (referenced via {1}) due to missing build platform", Reference.Name, ReferenceChain);
					return null;
				}

				// Create the new instance and add it to the cache
				Instance = new UEBuildPlugin(Info);
				Instance.bDescriptorReferencedExplicitly = !Reference.bOptional;
				NameToInstance.Add(Info.Name, Instance);

				// Get the reference chain for this plugin
				string PluginReferenceChain = String.Format("{0} -> {1}", ReferenceChain, Info.File.GetFileName());

				// Create modules for this plugin
				UEBuildBinaryType BinaryType = ShouldCompileMonolithic() ? UEBuildBinaryType.StaticLibrary : UEBuildBinaryType.DynamicLinkLibrary;
				if (Info.Descriptor.Modules != null)
				{
					foreach (ModuleDescriptor ModuleInfo in Info.Descriptor.Modules)
					{
						if (ModuleInfo.IsCompiledInConfiguration(Platform, TargetType, Rules.bBuildDeveloperTools, Rules.bBuildEditor, Rules.bBuildRequiresCookedData))
						{
							UEBuildModule Module = FindOrCreateModuleByName(ModuleInfo.Name, PluginReferenceChain);
							if(!Instance.Modules.Contains(Module))
							{
								if (!Module.RulesFile.IsUnderDirectory(Info.Directory))
								{
									throw new BuildException("Plugin '{0}' (referenced via {1}) does not contain the '{2}' module, but lists it in '{3}'.", Info.Name, ReferenceChain, ModuleInfo.Name, Info.File);
								}
								Instance.bDescriptorNeededAtRuntime = true;
								Instance.Modules.Add(Module);
							}
						}
					}
				}

				// Create the dependencies set
				HashSet<UEBuildPlugin> Dependencies = new HashSet<UEBuildPlugin>();
				if(Info.Descriptor.Plugins != null)
				{
					foreach(PluginReferenceDescriptor NextReference in Info.Descriptor.Plugins)
					{
						UEBuildPlugin NextInstance = AddPlugin(NextReference, PluginReferenceChain, ExcludeFolders, NameToInstance, NameToInfo);
						if(NextInstance != null)
						{
							Dependencies.Add(NextInstance);
							if(NextInstance.Dependencies == null)
							{
								throw new BuildException("Found circular dependency from plugin '{0}' onto itself.", NextReference.Name);
							}
							Dependencies.UnionWith(NextInstance.Dependencies);
						}
					}
				}
				Instance.Dependencies = Dependencies;

				// Stage the descriptor if the plugin contains content
				if (Info.Descriptor.bCanContainContent || Dependencies.Any(x => x.bDescriptorNeededAtRuntime))
				{
					Instance.bDescriptorNeededAtRuntime = true;
				}
			}
			return Instance;
		}

		/// <summary>
		/// Checks whether a plugin path contains a platform directory fragment
		/// </summary>
		private bool ShouldExcludePlugin(PluginInfo Plugin, FileSystemName[] ExcludeFolders)
		{
			if (Plugin.LoadedFrom == PluginLoadedFrom.Engine)
			{
				return Plugin.File.ContainsAnyNames(ExcludeFolders, UnrealBuildTool.EngineDirectory);
			}
			else if(ProjectFile != null)
			{
				return Plugin.File.ContainsAnyNames(ExcludeFolders, ProjectFile.Directory);
			}
			else
			{
				return false;
			}
		}

		/// <summary>
		/// Sets up the binaries for the target.
		/// </summary>
		protected void SetupBinaries()
		{
			if (Rules != null)
			{
				if(TargetType == TargetType.Program)
				{
					// If we're using the new method for specifying binaries, fill in the binary configurations now
					if(Rules.LaunchModuleName == null)
					{
						throw new BuildException("LaunchModuleName must be set for program targets.");
					}

					AppBinaries.Add(
						new UEBuildBinary(
							new UEBuildBinaryConfiguration(	
								InType: Rules.bShouldCompileAsDLL? UEBuildBinaryType.DynamicLinkLibrary : UEBuildBinaryType.Executable,
								InOutputFilePaths: OutputPaths,
								InIntermediateDirectory: ProjectIntermediateDirectory,
								InModuleNames: new List<string>() { Rules.LaunchModuleName } )
							)
						);
				}
				else
				{
					// Don't write to the Engine intermediate directory for Monolithic builds
					DirectoryReference IntermediateDirectory = ShouldCompileMonolithic() ? ProjectIntermediateDirectory : EngineIntermediateDirectory;

					// Editor
					UEBuildBinaryConfiguration Config = new UEBuildBinaryConfiguration(InType: UEBuildBinaryType.Executable,
																						InOutputFilePaths: OutputPaths,
																						InIntermediateDirectory: IntermediateDirectory,
																						bInCreateImportLibrarySeparately: (ShouldCompileMonolithic() ? false : true),
																						bInAllowExports: !ShouldCompileMonolithic(),
																						InModuleNames: new List<string>() { Rules.LaunchModuleName });

					if (Platform == UnrealTargetPlatform.Win64 && Configuration != UnrealTargetConfiguration.Shipping && TargetType == TargetType.Editor)
					{
						Config.bBuildAdditionalConsoleApp = true;
					}

					AppBinaries.Add(new UEBuildBinary(Config));
				}

				ExtraModuleNames.AddRange(Rules.ExtraModuleNames);
			}
		}

		/// <summary>
		/// Sets up the global compile and link environment for the target.
		/// </summary>
		public virtual void SetupGlobalEnvironment(UEToolChain ToolChain, CppCompileEnvironment GlobalCompileEnvironment, LinkEnvironment GlobalLinkEnvironment)
		{
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);

			ToolChain.SetUpGlobalEnvironment(Rules);

			// @Hack: This to prevent UHT from listing CoreUObject.init.gen.cpp as its dependency.
			// We flag the compile environment when we build UHT so that we don't need to check
			// this for each file when generating their dependencies.
			GlobalCompileEnvironment.bHackHeaderGenerator = (GetAppName() == "UnrealHeaderTool");

			GlobalCompileEnvironment.bUseDebugCRT = GlobalCompileEnvironment.Configuration == CppConfiguration.Debug && Rules.bDebugBuildsActuallyUseDebugCRT;
			GlobalCompileEnvironment.bEnableOSX109Support = Rules.bEnableOSX109Support;
			GlobalCompileEnvironment.Definitions.Add(String.Format("IS_PROGRAM={0}", TargetType == TargetType.Program ? "1" : "0"));
			GlobalCompileEnvironment.Definitions.AddRange(Rules.GlobalDefinitions);
			GlobalCompileEnvironment.bEnableExceptions = Rules.bForceEnableExceptions || Rules.bBuildEditor;
			GlobalCompileEnvironment.bEnableObjCExceptions = Rules.bForceEnableObjCExceptions || Rules.bBuildEditor;
			GlobalCompileEnvironment.bShadowVariableWarningsAsErrors = Rules.bShadowVariableErrors;
			GlobalCompileEnvironment.bUndefinedIdentifierWarningsAsErrors = Rules.bUndefinedIdentifierErrors;
			GlobalCompileEnvironment.bOptimizeForSize = Rules.bCompileForSize;
			GlobalCompileEnvironment.bOmitFramePointers = Rules.bOmitFramePointers;
			GlobalCompileEnvironment.bUsePDBFiles = Rules.bUsePDBFiles;
			GlobalCompileEnvironment.bSupportEditAndContinue = Rules.bSupportEditAndContinue;
			GlobalCompileEnvironment.bUseIncrementalLinking = Rules.bUseIncrementalLinking;
			GlobalCompileEnvironment.bAllowLTCG = Rules.bAllowLTCG;
			GlobalCompileEnvironment.bAllowRemotelyCompiledPCHs = Rules.bAllowRemotelyCompiledPCHs;
			GlobalCompileEnvironment.IncludePaths.bCheckSystemHeadersForModification = Rules.bCheckSystemHeadersForModification;
			GlobalCompileEnvironment.bPrintTimingInfo = Rules.bPrintToolChainTimingInfo;
			GlobalCompileEnvironment.bUseRTTI = Rules.bForceEnableRTTI;

			GlobalLinkEnvironment.bIsBuildingConsoleApplication = Rules.bIsBuildingConsoleApplication;
			GlobalLinkEnvironment.bOptimizeForSize = Rules.bCompileForSize;
			GlobalLinkEnvironment.bOmitFramePointers = Rules.bOmitFramePointers;
			GlobalLinkEnvironment.bSupportEditAndContinue = Rules.bSupportEditAndContinue;
			GlobalLinkEnvironment.bCreateMapFile = Rules.bCreateMapFile;
			GlobalLinkEnvironment.bHasExports = Rules.bHasExports;
			GlobalLinkEnvironment.bAllowALSR = (GlobalCompileEnvironment.Configuration != CppConfiguration.Shipping || Rules.bAllowASLRInShipping == false);
			GlobalLinkEnvironment.bUsePDBFiles = Rules.bUsePDBFiles;
			GlobalLinkEnvironment.BundleDirectory = BuildPlatform.GetBundleDirectory(Rules, OutputPaths);
			GlobalLinkEnvironment.BundleVersion = Rules.BundleVersion;
			GlobalLinkEnvironment.bAllowLTCG = Rules.bAllowLTCG;
			GlobalLinkEnvironment.bUseIncrementalLinking = Rules.bUseIncrementalLinking;
			GlobalLinkEnvironment.bUseFastPDBLinking = Rules.bUseFastPDBLinking;
			GlobalLinkEnvironment.bPrintTimingInfo = Rules.bPrintToolChainTimingInfo;
		
			// Add the 'Engine/Source' path as a global include path for all modules
			string EngineSourceDirectory = Path.GetFullPath(Path.Combine("..", "..", "Engine", "Source"));
			if (!Directory.Exists(EngineSourceDirectory))
			{
				throw new BuildException("Couldn't find Engine/Source directory using relative path");
			}
			GlobalCompileEnvironment.IncludePaths.UserIncludePaths.Add(EngineSourceDirectory);

			//@todo.PLATFORM: Do any platform specific tool chain initialization here if required

			string OutputAppName = GetAppName();

			UnrealTargetConfiguration EngineTargetConfiguration = Configuration == UnrealTargetConfiguration.DebugGame ? UnrealTargetConfiguration.Development : Configuration;
			GlobalCompileEnvironment.OutputDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, PlatformIntermediateFolder, OutputAppName, EngineTargetConfiguration.ToString());

			// Installed Engine intermediates go to the project's intermediate folder. Installed Engine never writes to the engine intermediate folder. (Those files are immutable)
			// Also, when compiling in monolithic, all intermediates go to the project's folder.  This is because a project can change definitions that affects all engine translation
			// units too, so they can't be shared between different targets.  They are effectively project-specific engine intermediates.
			if (UnrealBuildTool.IsEngineInstalled() || (ProjectFile != null && ShouldCompileMonolithic()))
			{
				if (ShouldCompileMonolithic())
				{
					if (ProjectFile != null)
					{
						GlobalCompileEnvironment.OutputDirectory = DirectoryReference.Combine(ProjectFile.Directory, PlatformIntermediateFolder, OutputAppName, Configuration.ToString());
					}
					else if (ForeignPlugins.Count > 0)
					{
						GlobalCompileEnvironment.OutputDirectory = DirectoryReference.Combine(ForeignPlugins[0].Directory, PlatformIntermediateFolder, OutputAppName, Configuration.ToString());
					}
				}
			}

			if(Rules.PCHOutputDirectory != null)
			{
				GlobalCompileEnvironment.PCHOutputDirectory = DirectoryReference.Combine(new DirectoryReference(Rules.PCHOutputDirectory), PlatformIntermediateFolder, OutputAppName, Configuration.ToString());
			}

			if (Rules.bForceCompileDevelopmentAutomationTests)
            {
                GlobalCompileEnvironment.Definitions.Add("WITH_DEV_AUTOMATION_TESTS=1");
            }
            else
            {
                switch(Configuration)
                {
                    case UnrealTargetConfiguration.Test:
                    case UnrealTargetConfiguration.Shipping:
                        GlobalCompileEnvironment.Definitions.Add("WITH_DEV_AUTOMATION_TESTS=0");
                        break;
                    default:
                        GlobalCompileEnvironment.Definitions.Add("WITH_DEV_AUTOMATION_TESTS=1");
                        break;
                }
            }

            if (Rules.bForceCompilePerformanceAutomationTests)
            {
                GlobalCompileEnvironment.Definitions.Add("WITH_PERF_AUTOMATION_TESTS=1");
            }
            else
            {
                switch (Configuration)
                {
                    case UnrealTargetConfiguration.Shipping:
                        GlobalCompileEnvironment.Definitions.Add("WITH_PERF_AUTOMATION_TESTS=0");
                        break;
                    default:
                        GlobalCompileEnvironment.Definitions.Add("WITH_PERF_AUTOMATION_TESTS=1");
                        break;
                }
            }

			// By default, shadow source files for this target in the root OutputDirectory
			GlobalCompileEnvironment.LocalShadowDirectory = GlobalCompileEnvironment.OutputDirectory;

			GlobalCompileEnvironment.Definitions.Add("UNICODE");
			GlobalCompileEnvironment.Definitions.Add("_UNICODE");
			GlobalCompileEnvironment.Definitions.Add("__UNREAL__");

			GlobalCompileEnvironment.Definitions.Add(String.Format("IS_MONOLITHIC={0}", ShouldCompileMonolithic() ? "1" : "0"));

			if (Rules.bCompileAgainstEngine)
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_ENGINE=1");
				GlobalCompileEnvironment.Definitions.Add(
					String.Format("WITH_UNREAL_DEVELOPER_TOOLS={0}", Rules.bBuildDeveloperTools ? "1" : "0"));
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_ENGINE=0");
				// Can't have developer tools w/out engine
				GlobalCompileEnvironment.Definitions.Add("WITH_UNREAL_DEVELOPER_TOOLS=0");
			}

			if (Rules.bCompileAgainstCoreUObject)
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_COREUOBJECT=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_COREUOBJECT=0");
			}

			if (Rules.bCompileWithStatsWithoutEngine)
			{
				GlobalCompileEnvironment.Definitions.Add("USE_STATS_WITHOUT_ENGINE=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("USE_STATS_WITHOUT_ENGINE=0");
			}

			if (Rules.bCompileWithPluginSupport)
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_PLUGIN_SUPPORT=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_PLUGIN_SUPPORT=0");
			}

            if (Rules.bWithPerfCounters)
            {
                GlobalCompileEnvironment.Definitions.Add("WITH_PERFCOUNTERS=1");
            }
            else
            {
                GlobalCompileEnvironment.Definitions.Add("WITH_PERFCOUNTERS=0");
            }

			if (Rules.bUseLoggingInShipping)
			{
				GlobalCompileEnvironment.Definitions.Add("USE_LOGGING_IN_SHIPPING=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("USE_LOGGING_IN_SHIPPING=0");
			}

			if (Rules.bLoggingToMemoryEnabled)
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_LOGGING_TO_MEMORY=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_LOGGING_TO_MEMORY=0");
			}

            if (Rules.bUseCacheFreedOSAllocs)
            {
                GlobalCompileEnvironment.Definitions.Add("USE_CACHE_FREED_OS_ALLOCS=1");
            }
            else
            {
                GlobalCompileEnvironment.Definitions.Add("USE_CACHE_FREED_OS_ALLOCS=0");
            }

			if (Rules.bUseChecksInShipping)
			{
				GlobalCompileEnvironment.Definitions.Add("USE_CHECKS_IN_SHIPPING=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("USE_CHECKS_IN_SHIPPING=0");
			}

			// Propagate whether we want a lean and mean build to the C++ code.
			if (Rules.bCompileLeanAndMeanUE)
			{
				GlobalCompileEnvironment.Definitions.Add("UE_BUILD_MINIMAL=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("UE_BUILD_MINIMAL=0");
			}

			// bBuildEditor has now been set appropriately for all platforms, so this is here to make sure the #define 
			if (Rules.bBuildEditor)
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_EDITOR=1");
			}
			else if (!GlobalCompileEnvironment.Definitions.Contains("WITH_EDITOR=0"))
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_EDITOR=0");
			}

			if (Rules.bBuildWithEditorOnlyData == false)
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_EDITORONLY_DATA=0");
			}

			// Check if server-only code should be compiled out.
			if (Rules.bWithServerCode == true)
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_SERVER_CODE=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_SERVER_CODE=0");
			}

			// Set the define for whether we're compiling with CEF3
			if (Rules.bCompileCEF3 && (Platform == UnrealTargetPlatform.Win32 || Platform == UnrealTargetPlatform.Win64 || Platform == UnrealTargetPlatform.Mac || Platform == UnrealTargetPlatform.Linux))
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_CEF3=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_CEF3=0");
			}

			if (Rules.bUseXGEController &&
				Rules.Type == TargetType.Editor &&
				(Platform == UnrealTargetPlatform.Win32 || Platform == UnrealTargetPlatform.Win64))
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_XGE_CONTROLLER=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_XGE_CONTROLLER=0");
			}

			// tell the compiled code the name of the UBT platform (this affects folder on disk, etc that the game may need to know)
			GlobalCompileEnvironment.Definitions.Add("UBT_COMPILED_PLATFORM=" + Platform.ToString());
			GlobalCompileEnvironment.Definitions.Add("UBT_COMPILED_TARGET=" + TargetType.ToString());

			// Initialize the compile and link environments for the platform, configuration, and project.
			BuildPlatform.SetUpEnvironment(Rules, GlobalCompileEnvironment, GlobalLinkEnvironment);
			BuildPlatform.SetUpConfigurationEnvironment(Rules, GlobalCompileEnvironment, GlobalLinkEnvironment);
		}

		private void GetEncryptionAndSigningKeys(out String AESKey, out String[] PakSigningKeys)
		{
			EncryptionAndSigning.ParseEncryptionIni(ProjectDirectory, Platform, out PakSigningKeys, out AESKey);

			if (!String.IsNullOrEmpty(AESKey))
			{
				if (AESKey.Length < 32)
				{
					Log.TraceError("AES key specified in configs must be at least 32 characters long!");
					AESKey = String.Empty;
				}
			}

			// If we didn't extract any keys from the new ini file setup, try looking for the old keys text file
			if (PakSigningKeys == null && !string.IsNullOrEmpty(Rules.PakSigningKeysFile))
			{
				string FullFilename = Path.Combine(ProjectDirectory.FullName, Rules.PakSigningKeysFile);

				Log.TraceVerbose("Adding signing keys to executable from '{0}'", FullFilename);

				if (File.Exists(FullFilename))
				{
					string[] Lines = File.ReadAllLines(FullFilename);
					List<string> Keys = new List<string>();
					foreach (string Line in Lines)
					{
						if (!string.IsNullOrEmpty(Line))
						{
							if (Line.StartsWith("0x"))
							{
								Keys.Add(Line.Trim());
							}
						}
					}

					if (Keys.Count == 3)
					{
						PakSigningKeys = new String[2];
						PakSigningKeys[0] = Keys[1];
						PakSigningKeys[1] = Keys[2];
					}
					else
					{
						Log.TraceWarning("Contents of signing key file are invalid so will be ignored");
					}
				}
				else
				{
					Log.TraceVerbose("Signing key file is missing! Executable will not include signing keys");
				}
			}
		}

		static CppConfiguration GetCppConfiguration(UnrealTargetConfiguration Configuration)
		{
			switch (Configuration)
			{
				case UnrealTargetConfiguration.Debug:
					return CppConfiguration.Debug;
				case UnrealTargetConfiguration.DebugGame:
				case UnrealTargetConfiguration.Development:
					return CppConfiguration.Development;
				case UnrealTargetConfiguration.Shipping:
					return CppConfiguration.Shipping;
				case UnrealTargetConfiguration.Test:
					return CppConfiguration.Shipping;
				default:
					throw new BuildException("Unhandled target configuration");
			}
		}

        /// <summary>
        /// Create a rules object for the given module, and set any default values for this target
        /// </summary>
        private ModuleRules CreateModuleRulesAndSetDefaults(string ModuleName, string ReferenceChain, out FileReference ModuleFileName)
		{
			// Create the rules from the assembly
			ModuleRules RulesObject = RulesAssembly.CreateModuleRules(ModuleName, Rules, ReferenceChain, out ModuleFileName);

			// Reads additional dependencies array for project module from project file and fills PrivateDependencyModuleNames. 
			if (ProjectDescriptor != null && ProjectDescriptor.Modules != null)
			{
				ModuleDescriptor Module = ProjectDescriptor.Modules.FirstOrDefault(x => x.Name.Equals(ModuleName, StringComparison.InvariantCultureIgnoreCase));
				if (Module != null && Module.AdditionalDependencies != null)
				{
					RulesObject.PrivateDependencyModuleNames.AddRange(Module.AdditionalDependencies);
				}
			}

			// Validate rules object
			if (RulesObject.Type == ModuleRules.ModuleType.CPlusPlus)
			{
				List<string> InvalidDependencies = RulesObject.DynamicallyLoadedModuleNames.Intersect(RulesObject.PublicDependencyModuleNames.Concat(RulesObject.PrivateDependencyModuleNames)).ToList();
				if (InvalidDependencies.Count != 0)
				{
					throw new BuildException("Module rules for '{0}' should not be dependent on modules which are also dynamically loaded: {1}", ModuleName, String.Join(", ", InvalidDependencies));
				}

				// Make sure that engine modules use shared PCHs or have an explicit private PCH
				if(RulesObject.PCHUsage == ModuleRules.PCHUsageMode.NoSharedPCHs && RulesObject.PrivatePCHHeaderFile == null)
				{
					if(ProjectFile == null || !ModuleFileName.IsUnderDirectory(ProjectFile.Directory))
					{
						Log.TraceWarning("{0} module has shared PCHs disabled, but does not have a private PCH set", ModuleName);
					}
				}

				// Disable shared PCHs for game modules by default (but not game plugins, since they won't depend on the game's PCH!)
				if (RulesObject.PCHUsage == ModuleRules.PCHUsageMode.Default)
				{
					if (ProjectFile == null || !ModuleFileName.IsUnderDirectory(ProjectFile.Directory) || Rules.bIWYU)
					{
						// Engine module or plugin module -- allow shared PCHs
						RulesObject.PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
					}
					else
					{
						PluginInfo Plugin;
						if(RulesAssembly.TryGetPluginForModule(ModuleFileName, out Plugin))
						{
							// Game plugin.  Enable shared PCHs by default, since they aren't typically large enough to warrant their own PCH.
							RulesObject.PCHUsage = ModuleRules.PCHUsageMode.UseSharedPCHs;
						}
						else
						{
							// Game module.  Do not enable shared PCHs by default, because games usually have a large precompiled header of their own and compile times would suffer.
							RulesObject.PCHUsage = ModuleRules.PCHUsageMode.NoSharedPCHs;
						}
					}
				}
			}
			return RulesObject;
		}

		/// <summary>
		/// Finds a module given its name.  Throws an exception if the module couldn't be found.
		/// </summary>
		public UEBuildModule FindOrCreateModuleByName(string ModuleName, string ReferenceChain)
		{
			UEBuildModule Module;
			if (!Modules.TryGetValue(ModuleName, out Module))
			{
				// @todo projectfiles: Cross-platform modules can appear here during project generation, but they may have already
				//   been filtered out by the project generator.  This causes the projects to not be added to directories properly.
				FileReference ModuleFileName;
				ModuleRules RulesObject = CreateModuleRulesAndSetDefaults(ModuleName, ReferenceChain, out ModuleFileName);
				DirectoryReference ModuleDirectory = ModuleFileName.Directory;

				// Get the type of module we're creating
				UHTModuleType? ModuleType = null;

				// Get the plugin for this module
				PluginInfo Plugin;
				RulesAssembly.TryGetPluginForModule(ModuleFileName, out Plugin);

				// Get the module descriptor for this module if it's a plugin
				ModuleDescriptor PluginModuleDesc = null;
				if (Plugin != null)
				{
					PluginModuleDesc = Plugin.Descriptor.Modules.FirstOrDefault(x => x.Name == ModuleName);
					if (PluginModuleDesc != null && PluginModuleDesc.Type == ModuleHostType.Program)
					{
						ModuleType = UHTModuleType.Program;
					}
				}

				if (UnrealBuildTool.IsUnderAnEngineDirectory(ModuleFileName.Directory))
				{
					if (RulesObject.Type == ModuleRules.ModuleType.External)
					{
						ModuleType = UHTModuleType.EngineThirdParty;
					}
					else
					{
						if (!ModuleType.HasValue && PluginModuleDesc != null)
						{
							ModuleType = ExternalExecution.GetEngineModuleTypeFromDescriptor(PluginModuleDesc);
						}

						if (!ModuleType.HasValue)
						{
							if (ModuleFileName.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
							{
								ModuleType = ExternalExecution.GetEngineModuleTypeBasedOnLocation(UnrealBuildTool.EngineSourceDirectory, ModuleFileName);
							}
							else if (ModuleFileName.IsUnderDirectory(UnrealBuildTool.EnterpriseSourceDirectory))
							{
								ModuleType = ExternalExecution.GetEngineModuleTypeBasedOnLocation(UnrealBuildTool.EnterpriseSourceDirectory, ModuleFileName);
							}
						}
					}
				}
				else
				{
					if (RulesObject.Type == ModuleRules.ModuleType.External)
					{
						ModuleType = UHTModuleType.GameThirdParty;
					}
					else
					{
						if (!ModuleType.HasValue && PluginModuleDesc != null)
						{
							ModuleType = ExternalExecution.GetGameModuleTypeFromDescriptor(PluginModuleDesc);
						}

						if (!ModuleType.HasValue)
						{
							if (ProjectDescriptor != null && ProjectDescriptor.Modules != null)
							{
								ModuleDescriptor ProjectModule = ProjectDescriptor.Modules.FirstOrDefault(x => x.Name == ModuleName);
								if (ProjectModule != null)
								{
									ModuleType = UHTModuleTypeExtensions.GameModuleTypeFromHostType(ProjectModule.Type);
								}
								else
								{
									// No descriptor file or module was not on the list
									ModuleType = UHTModuleType.GameRuntime;
								}
							}
						}
					}
				}

				if (!ModuleType.HasValue)
				{
					throw new BuildException("Unable to determine module type for {0}\n(referenced via {1})", ModuleFileName, ReferenceChain);
				}

				// Get the base directory for paths referenced by the module. If the module's under the UProject source directory use that, otherwise leave it relative to the Engine source directory.
				if (ProjectFile != null)
				{
					DirectoryReference ProjectSourceDirectoryName = DirectoryReference.Combine(ProjectFile.Directory, "Source");
					if (ModuleFileName.IsUnderDirectory(ProjectSourceDirectoryName))
					{
						RulesObject.PublicIncludePaths = CombinePathList(ProjectSourceDirectoryName, RulesObject.PublicIncludePaths);
						RulesObject.PrivateIncludePaths = CombinePathList(ProjectSourceDirectoryName, RulesObject.PrivateIncludePaths);
						RulesObject.PublicLibraryPaths = CombinePathList(ProjectSourceDirectoryName, RulesObject.PublicLibraryPaths);
						RulesObject.PublicAdditionalShadowFiles = CombinePathList(ProjectSourceDirectoryName, RulesObject.PublicAdditionalShadowFiles);
					}
				}

				// Get the generated code directory. Plugins always write to their own intermediate directory so they can be copied between projects, shared engine 
				// intermediates go in the engine intermediate folder, and anything else goes in the project folder.
				DirectoryReference GeneratedCodeDirectory = null;
				if (RulesObject.Type != ModuleRules.ModuleType.External)
				{
					if (Plugin != null)
					{
						GeneratedCodeDirectory = Plugin.Directory;
					}
					else if (bUseSharedBuildEnvironment && ModuleFileName.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
					{
						GeneratedCodeDirectory = UnrealBuildTool.EngineDirectory;
					}
					else
					{
						GeneratedCodeDirectory = ProjectDirectory;
					}
					GeneratedCodeDirectory = DirectoryReference.Combine(GeneratedCodeDirectory, PlatformIntermediateFolder, AppName, "Inc", ModuleName);
				}

				// Don't generate include paths for third party modules; they don't follow our conventions. Core is a special-case... leave it alone
				if (RulesObject.Type != ModuleRules.ModuleType.External && RulesObject.bAddDefaultIncludePaths)
				{
					// Add the default include paths to the module rules, if they exist. Would be nice not to include game plugins here, but it would be a regression to change now.
					bool bIsGameModuleOrProgram = ModuleFileName.IsUnderDirectory(TargetRulesFile.Directory) || (Plugin != null && Plugin.LoadedFrom == PluginLoadedFrom.Project);
					AddDefaultIncludePathsToModuleRules(ModuleFileName, bIsGameModuleOrProgram, Plugin, RulesObject);

					// Add the path to the generated headers 
					if (GeneratedCodeDirectory != null)
					{
						string RelativeGeneratedCodeDirectory = Utils.CleanDirectorySeparators(GeneratedCodeDirectory.MakeRelativeTo(UnrealBuildTool.EngineSourceDirectory), '/');
						RulesObject.PublicIncludePaths.Add(RelativeGeneratedCodeDirectory);
					}
				}

				// Figure out whether we need to build this module
				// We don't care about actual source files when generating projects, as these are discovered separately
				bool bDiscoverFiles = !ProjectFileGenerator.bGenerateProjectFiles;
				bool bBuildFiles = bDiscoverFiles && (OnlyModules.Count == 0 || OnlyModules.Any(x => string.Equals(x.OnlyModuleName, ModuleName, StringComparison.InvariantCultureIgnoreCase)));

				IntelliSenseGatherer IntelliSenseGatherer = null;
				List<FileItem> FoundSourceFiles = new List<FileItem>();
				if (RulesObject.Type == ModuleRules.ModuleType.CPlusPlus)
				{
					ProjectFile ProjectFileForIDE = null;
					if (ProjectFileGenerator.bGenerateProjectFiles && ProjectFileGenerator.ModuleToProjectFileMap.TryGetValue(ModuleName, out ProjectFileForIDE))
					{
						IntelliSenseGatherer = ProjectFileForIDE;
					}

					// So all we care about are the game module and/or plugins.
					if (bDiscoverFiles && (!UnrealBuildTool.IsEngineInstalled() || !UnrealBuildTool.IsUnderAnEngineDirectory(ModuleFileName.Directory)))
					{
						List<FileReference> SourceFilePaths = new List<FileReference>();

						if (ProjectFileForIDE != null)
						{
							foreach (ProjectFile.SourceFile SourceFile in ProjectFileForIDE.SourceFiles)
							{
								SourceFilePaths.Add(SourceFile.Reference);
							}
						}
						else
						{
							// Don't have a project file for this module with the source file names cached already, so find the source files ourselves
							SourceFilePaths = SourceFileSearch.FindModuleSourceFiles(ModuleRulesFile: ModuleFileName);
						}
						FoundSourceFiles = GetCPlusPlusFilesToBuild(SourceFilePaths, ModuleDirectory, Platform);
					}
				}

				// Allow the current platform to modify the module rules
				UEBuildPlatform.GetBuildPlatform(Platform).ModifyModuleRulesForActivePlatform(ModuleName, RulesObject, Rules);

				// Allow all build platforms to 'adjust' the module setting. 
				// This will allow undisclosed platforms to make changes without 
				// exposing information about the platform in publicly accessible 
				// locations.
				UEBuildPlatform.PlatformModifyHostModuleRules(ModuleName, RulesObject, Rules);

				// Expand the list of runtime dependencies, and update the run-time dependencies path to remove $(PluginDir) and replace with a full path. When the 
				// receipt is saved it'll be converted to a $(ProjectDir) or $(EngineDir) equivalent.
				List<RuntimeDependency> RuntimeDependencies = new List<RuntimeDependency>();
				if(RulesObject.RuntimeDependencies.Count > 0)
				{
					// Get all the valid variables which can be expanded for this module
					Dictionary<string, string> Variables = new Dictionary<string, string>();
					Variables["EngineDir"] = UnrealBuildTool.EngineDirectory.FullName;
					if (ProjectFile != null)
					{
						Variables["ProjectDir"] = ProjectDirectory.FullName;
					}
					if (Plugin != null)
					{
						Variables["PluginDir"] = Plugin.Directory.FullName;
					}

					// Convert them into concrete file lists. Ignore anything that still hasn't been resolved.
					foreach (ModuleRules.RuntimeDependency Dependency in RulesObject.RuntimeDependencies)
					{
						string ExpandedPath = Utils.ExpandVariables(Dependency.Path, Variables);
						if (!ExpandedPath.StartsWith("$("))
						{
							int WildcardIdx = FileFilter.FindWildcardIndex(ExpandedPath);
							if (WildcardIdx == -1)
							{
								RuntimeDependencies.Add(new RuntimeDependency(new FileReference(ExpandedPath), Dependency.Type));
							}
							else
							{
								RuntimeDependencies.AddRange(FileFilter.ResolveWildcard(ExpandedPath).Select(x => new RuntimeDependency(x, Dependency.Type)));
							}
						}
					}
				}

				// Now, go ahead and create the module builder instance
				Module = InstantiateModule(RulesObject, ModuleName, ModuleType.Value, ModuleDirectory, GeneratedCodeDirectory, IntelliSenseGatherer, FoundSourceFiles, bBuildFiles, ModuleFileName, RuntimeDependencies);
				Modules.Add(Module.Name, Module);
				FlatModuleCsData.Add(Module.Name, new FlatModuleCsDataType((Module.RulesFile == null) ? null : Module.RulesFile.FullName, RulesObject.ExternalDependencies));
			}
			return Module;
		}

		protected virtual UEBuildModule InstantiateModule(
			ModuleRules RulesObject,
			string ModuleName,
			UHTModuleType ModuleType,
			DirectoryReference ModuleDirectory,
			DirectoryReference GeneratedCodeDirectory,
			IntelliSenseGatherer IntelliSenseGatherer,
			List<FileItem> ModuleSourceFiles,
			bool bBuildSourceFiles,
			FileReference InRulesFile,
			List<RuntimeDependency> InRuntimeDependencies)
		{
			switch (RulesObject.Type)
			{
				case ModuleRules.ModuleType.CPlusPlus:
					return new UEBuildModuleCPP(
							InName: ModuleName,
							InType: ModuleType,
							InModuleDirectory: ModuleDirectory,
							InGeneratedCodeDirectory: GeneratedCodeDirectory,
							InIntelliSenseGatherer: IntelliSenseGatherer,
							InSourceFiles: ModuleSourceFiles,
							InRules: RulesObject,
							bInBuildSourceFiles: bBuildSourceFiles,
							InRulesFile: InRulesFile,
							InRuntimeDependencies: InRuntimeDependencies
						);

				case ModuleRules.ModuleType.External:
					return new UEBuildModuleExternal(
							InName: ModuleName,
							InType: ModuleType,
							InModuleDirectory: ModuleDirectory,
							InRules: RulesObject,
							InRulesFile: InRulesFile,
							InRuntimeDependencies: InRuntimeDependencies
						);

				default:
					throw new BuildException("Unrecognized module type specified by 'Rules' object {0}", RulesObject.ToString());
			}
		}

		/// <summary>
		/// Add the standard default include paths to the given modulerules object
		/// </summary>
		/// <param name="ModuleFile">The filename to the module rules file (Build.cs)</param>
		/// <param name="IsGameModule">true if it is a game module, false if not</param>
		/// <param name="Plugin">The plugin that this module belongs to</param>
		/// <param name="RulesObject">The module rules object itself</param>
		public void AddDefaultIncludePathsToModuleRules(FileReference ModuleFile, bool IsGameModule, PluginInfo Plugin, ModuleRules RulesObject)
		{
			// Get the base source directory for this module. This may be the project source directory, engine source directory, or plugin source directory.
			if (!ModuleFile.IsUnderDirectory(UnrealBuildTool.EngineSourceDirectory) && !ModuleFile.IsUnderDirectory(UnrealBuildTool.EnterpriseSourceDirectory))
			{
				// Add the module source directory 
				DirectoryReference BaseSourceDirectory;
				if (Plugin != null)
				{
					BaseSourceDirectory = DirectoryReference.Combine(Plugin.Directory, "Source");
				}
				else
				{
					BaseSourceDirectory = DirectoryReference.Combine(ProjectFile.Directory, "Source");
				}

				// If it's a game module (plugin or otherwise), add the root source directory to the include paths.
				if (IsGameModule)
				{
					RulesObject.PublicIncludePaths.Add(NormalizeIncludePath(BaseSourceDirectory));
				}

				// Resolve private include paths against the project source root
				for (int Idx = 0; Idx < RulesObject.PrivateIncludePaths.Count; Idx++)
				{
					string PrivateIncludePath = RulesObject.PrivateIncludePaths[Idx];
					if (!Path.IsPathRooted(PrivateIncludePath))
					{
						PrivateIncludePath = DirectoryReference.Combine(BaseSourceDirectory, PrivateIncludePath).FullName;
					}
					RulesObject.PrivateIncludePaths[Idx] = PrivateIncludePath;
				}
			}

			// Add the 'classes' directory, if it exists
			DirectoryReference ClassesDirectory = DirectoryReference.Combine(ModuleFile.Directory, "Classes");
			if (DirectoryLookupCache.DirectoryExists(ClassesDirectory))
			{
				RulesObject.PublicIncludePaths.Add(NormalizeIncludePath(ClassesDirectory));
			}

			// Get the list of folders to exclude for this platform
			FileSystemName[] ExcludedFolderNames = UEBuildPlatform.GetBuildPlatform(Platform).GetExcludedFolderNames();

			// Add all the public directories
			DirectoryReference PublicDirectory = DirectoryReference.Combine(ModuleFile.Directory, "Public");
			if (DirectoryLookupCache.DirectoryExists(PublicDirectory))
			{
				RulesObject.PublicIncludePaths.Add(NormalizeIncludePath(PublicDirectory));

				foreach (DirectoryReference PublicSubDirectory in DirectoryLookupCache.EnumerateDirectoriesRecursively(PublicDirectory))
				{
					if(!PublicSubDirectory.ContainsAnyNames(ExcludedFolderNames, PublicDirectory))
					{
						RulesObject.PublicIncludePaths.Add(NormalizeIncludePath(PublicSubDirectory));
					}
				}
			}
		}

		/// <summary>
		/// Normalize an include path to be relative to the engine source directory
		/// </summary>
		public static string NormalizeIncludePath(DirectoryReference Directory)
		{
			return Utils.CleanDirectorySeparators(Directory.MakeRelativeTo(UnrealBuildTool.EngineSourceDirectory), '/');
		}

		/// <summary>
		/// Finds a module given its name.  Throws an exception if the module couldn't be found.
		/// </summary>
		public UEBuildModule GetModuleByName(string Name)
		{
			UEBuildModule Result;
			if (Modules.TryGetValue(Name, out Result))
			{
				return Result;
			}
			else
			{
				throw new BuildException("Couldn't find referenced module '{0}'.", Name);
			}
		}


		/// <summary>
		/// Combines a list of paths with a base path.
		/// </summary>
		/// <param name="BasePath">Base path to combine with. May be null or empty.</param>
		/// <param name="PathList">List of input paths to combine with. May be null.</param>
		/// <returns>List of paths relative The build module object for the specified build rules source file</returns>
		private static List<string> CombinePathList(DirectoryReference BasePath, List<string> PathList)
		{
			List<string> NewPathList = new List<string>();
			foreach (string Path in PathList)
			{
				NewPathList.Add(System.IO.Path.Combine(BasePath.FullName, Path));
			}
			return NewPathList;
		}


		/// <summary>
		/// Given a list of source files for a module, filters them into a list of files that should actually be included in a build
		/// </summary>
		/// <param name="SourceFiles">Original list of files, which may contain non-source</param>
		/// <param name="SourceFilesBaseDirectory">Directory that the source files are in</param>
		/// <param name="TargetPlatform">The platform we're going to compile for</param>
		/// <returns>The list of source files to actually compile</returns>
		static List<FileItem> GetCPlusPlusFilesToBuild(List<FileReference> SourceFiles, DirectoryReference SourceFilesBaseDirectory, UnrealTargetPlatform TargetPlatform)
		{
			// Make a list of all platform name strings that we're *not* currently compiling, to speed
			// up file path comparisons later on
			List<UnrealTargetPlatform> SupportedPlatforms = new List<UnrealTargetPlatform>();
			SupportedPlatforms.Add(TargetPlatform);
			List<string> OtherPlatformNameStrings = Utils.MakeListOfUnsupportedPlatforms(SupportedPlatforms);


			// @todo projectfiles: Consider saving out cached list of source files for modules so we don't need to harvest these each time

			List<FileItem> FilteredFileItems = new List<FileItem>();
			FilteredFileItems.Capacity = SourceFiles.Count;

			// @todo projectfiles: hard-coded source file set.  Should be made extensible by platform tool chains.
			string[] CompilableSourceFileTypes = new string[]
				{
					".cpp",
					".c",
					".cc",
					".mm",
					".m",
					".rc",
					".manifest"
				};

			// When generating project files, we have no file to extract source from, so we'll locate the code files manually
			foreach (FileReference SourceFilePath in SourceFiles)
			{
				// We're only able to compile certain types of files
				bool IsCompilableSourceFile = false;
				foreach (string CurExtension in CompilableSourceFileTypes)
				{
					if (SourceFilePath.HasExtension(CurExtension))
					{
						IsCompilableSourceFile = true;
						break;
					}
				}

				if (IsCompilableSourceFile)
				{
					if (SourceFilePath.IsUnderDirectory(SourceFilesBaseDirectory))
					{
						// Store the path as relative to the project file
						string RelativeFilePath = SourceFilePath.MakeRelativeTo(SourceFilesBaseDirectory);

						// All compiled files should always be in a sub-directory under the project file directory.  We enforce this here.
						if (Path.IsPathRooted(RelativeFilePath) || RelativeFilePath.StartsWith(".."))
						{
							throw new BuildException("Error: Found source file {0} in project whose path was not relative to the base directory of the source files", RelativeFilePath);
						}

						// Check for source files that don't belong to the platform we're currently compiling.  We'll filter
						// those source files out
						bool IncludeThisFile = true;
						foreach (string CurPlatformName in OtherPlatformNameStrings)
						{
							if (RelativeFilePath.IndexOf(Path.DirectorySeparatorChar + CurPlatformName + Path.DirectorySeparatorChar, StringComparison.InvariantCultureIgnoreCase) != -1
								|| RelativeFilePath.StartsWith(CurPlatformName + Path.DirectorySeparatorChar))
							{
								IncludeThisFile = false;
								break;
							}
						}

						if (IncludeThisFile)
						{
							FilteredFileItems.Add(FileItem.GetItemByFileReference(SourceFilePath));
						}
					}
				}
			}

			// @todo projectfiles: Consider enabling this error but changing it to a warning instead.  It can fire for
			//    projects that are being digested for IntelliSense (because the module was set as a cross-
			//	  platform dependency), but all of their source files were filtered out due to platform masking
			//    in the project generator
			bool AllowEmptyProjects = true;
			if (!AllowEmptyProjects)
			{
				if (FilteredFileItems.Count == 0)
				{
					throw new BuildException("Could not find any valid source files for base directory {0}.  Project has {1} files in it", SourceFilesBaseDirectory, SourceFiles.Count);
				}
			}

			return FilteredFileItems;
		}

        static bool ShouldIncludeNativizedAssets(DirectoryReference GameProjectDirectory)
        {
            ConfigHierarchy Config = ConfigCache.ReadHierarchy(ConfigHierarchyType.Game, GameProjectDirectory, BuildHostPlatform.Current.Platform);
            if (Config != null)
            {
                // Determine whether or not the user has enabled nativization of Blueprint assets at cook time (default is 'Disabled')
                string BlueprintNativizationMethod;
                if (!Config.TryGetValue("/Script/UnrealEd.ProjectPackagingSettings", "BlueprintNativizationMethod", out BlueprintNativizationMethod))
                {
                    BlueprintNativizationMethod = "Disabled";
                }

                return BlueprintNativizationMethod != "Disabled";
            }

            return false;
        }
	}
}
