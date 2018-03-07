// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnrealBuildTool;
using System.IO;
using System.Reflection;
using Tools.DotNETCommon;

namespace AutomationTool
{
	[Help("targetplatform=PlatformName", "target platform for building, cooking and deployment (also -Platform)")]
	[Help("servertargetplatform=PlatformName", "target platform for building, cooking and deployment of the dedicated server (also -ServerPlatform)")]
	public class ProjectParams
	{
		#region Constructors

		/// <summary>
		/// Gets a parameter from the command line if it hasn't been specified in the constructor. 
		/// If the command line is not available, default value will be used.
		/// </summary>
		/// <param name="Command">Command to parse the command line for. Can be null.</param>
		/// <param name="SpecifiedValue">Value specified in the constructor (or not)</param>
		/// <param name="Default">Default value.</param>
		/// <param name="ParamNames">Command line parameter names to parse.</param>
		/// <returns>Parameter value.</returns>
		bool GetParamValueIfNotSpecified(BuildCommand Command, bool? SpecifiedValue, bool Default, params string[] ParamNames)
		{
			if (SpecifiedValue.HasValue)
			{
				return SpecifiedValue.Value;
			}
			else if (Command != null)
			{
				foreach (var Param in ParamNames)
				{
					if (Command.ParseParam(Param))
					{
						return true;
					}
				}
			}
			return Default;
		}

		/// <summary>
		/// Gets optional parameter from the command line if it hasn't been specified in the constructor. 
		/// If the command line is not available or the command has not been specified in the command line, default value will be used.
		/// </summary>
		/// <param name="Command">Command to parse the command line for. Can be null.</param>
		/// <param name="SpecifiedValue">Value specified in the constructor (or not)</param>
		/// <param name="Default">Default value.</param>
		/// <param name="TrueParam">Name of a parameter that sets the value to 'true', for example: -clean</param>
		/// <param name="FalseParam">Name of a parameter that sets the value to 'false', for example: -noclean</param>
		/// <returns>Parameter value or default value if the paramater has not been specified</returns>
		bool? GetOptionalParamValueIfNotSpecified(BuildCommand Command, bool? SpecifiedValue, bool? Default, string TrueParam, string FalseParam)
		{
			if (SpecifiedValue.HasValue)
			{
				return SpecifiedValue.Value;
			}
			else if (Command != null)
			{
				bool? Value = null;
				if (!String.IsNullOrEmpty(TrueParam) && Command.ParseParam(TrueParam))
				{
					Value = true;
				}
				else if (!String.IsNullOrEmpty(FalseParam) && Command.ParseParam(FalseParam))
				{
					Value = false;
				}
				if (Value.HasValue)
				{
					return Value;
				}
			}
			return Default;
		}

		/// <summary>
		/// Gets a parameter value from the command line if it hasn't been specified in the constructor. 
		/// If the command line is not available, default value will be used.
		/// </summary>
		/// <param name="Command">Command to parse the command line for. Can be null.</param>
		/// <param name="SpecifiedValue">Value specified in the constructor (or not)</param>
		/// <param name="ParamName">Command line parameter name to parse.</param>
		/// <param name="Default">Default value</param>
		/// <param name="bTrimQuotes">If set, the leading and trailing quotes will be removed, e.g. instead of "/home/User Name" it will return /home/User Name</param>
		/// <returns>Parameter value.</returns>
		string ParseParamValueIfNotSpecified(BuildCommand Command, string SpecifiedValue, string ParamName, string Default = "", bool bTrimQuotes = false)
		{
			string Result = Default;

			if (SpecifiedValue != null)
			{
				Result = SpecifiedValue;
			}
			else if (Command != null)
			{
				Result = Command.ParseParamValue(ParamName, Default);
			}

			return bTrimQuotes ? Result.Trim( new char[]{'\"'} ) : Result;
		}

		/// <summary>
		/// Sets up platforms
		/// </summary>
        /// <param name="DependentPlatformMap">Set with a mapping from source->destination if specified on command line</param>
		/// <param name="Command">The command line to parse</param>
		/// <param name="OverrideTargetPlatforms">If passed use this always</param>
        /// <param name="DefaultTargetPlatforms">Use this if nothing is passed on command line</param>
		/// <param name="AllowPlatformParams">Allow raw -platform options</param>
		/// <param name="PlatformParamNames">Possible -parameters to check for</param>
		/// <returns>List of platforms parsed from the command line</returns>
		private List<TargetPlatformDescriptor> SetupTargetPlatforms(ref Dictionary<TargetPlatformDescriptor, TargetPlatformDescriptor> DependentPlatformMap, BuildCommand Command, List<TargetPlatformDescriptor> OverrideTargetPlatforms, List<TargetPlatformDescriptor> DefaultTargetPlatforms, bool AllowPlatformParams, params string[] PlatformParamNames)
		{
			List<TargetPlatformDescriptor> TargetPlatforms = null;
			if (CommandUtils.IsNullOrEmpty(OverrideTargetPlatforms))
			{
				if (Command != null)
				{
					// Parse the command line, we support the following params:
					// -'PlatformParamNames[n]'=Platform_1+Platform_2+...+Platform_k
					// or (if AllowPlatformParams is true)
					// -Platform_1 -Platform_2 ... -Platform_k
					string CmdLinePlatform = null;
					foreach (var ParamName in PlatformParamNames)
					{
						CmdLinePlatform = Command.ParseParamValue(ParamName);
						if (!String.IsNullOrEmpty(CmdLinePlatform))
						{
							break;
						}
					}

                    List<string> CookFlavors = null;
                    {
                        string CmdLineCookFlavor = Command.ParseParamValue("cookflavor");
                        if (!String.IsNullOrEmpty(CmdLineCookFlavor))
                        {
                            CookFlavors = new List<string>(CmdLineCookFlavor.Split('+'));
                        }
                    }

                    if (!String.IsNullOrEmpty(CmdLinePlatform))
					{
						// Get all platforms from the param value: Platform_1+Platform_2+...+Platform_k
						TargetPlatforms = new List<TargetPlatformDescriptor>();
						var PlatformNames = new List<string>(CmdLinePlatform.Split('+'));
						foreach (var PlatformName in PlatformNames)
						{
                            // Look for dependent platforms, Source_1.Dependent_1+Source_2.Dependent_2+Standalone_3
                            var SubPlatformNames = new List<string>(PlatformName.Split('.'));

                            foreach (var SubPlatformName in SubPlatformNames)
                            {
                                UnrealTargetPlatform NewPlatformType = (UnrealTargetPlatform)Enum.Parse(typeof(UnrealTargetPlatform), SubPlatformName, true);
                                // generate all valid platform descriptions for this platform type + cook flavors
                                List<TargetPlatformDescriptor> PlatformDescriptors = Platform.GetValidTargetPlatforms(NewPlatformType, CookFlavors);
                                TargetPlatforms.AddRange(PlatformDescriptors);
                                                              
                                if (SubPlatformName != SubPlatformNames[0])
                                {
                                    // This is not supported with cook flavors
                                    if (!CommandUtils.IsNullOrEmpty(CookFlavors))
                                    {
                                        throw new AutomationException("Cook flavors are not supported for dependent platforms!");
                                    }

                                    // We're a dependent platform so add ourselves to the map, pointing to the first element in the list
                                    UnrealTargetPlatform FirstPlatformType = (UnrealTargetPlatform)Enum.Parse(typeof(UnrealTargetPlatform), SubPlatformNames[0], true);
                                    DependentPlatformMap.Add(new TargetPlatformDescriptor(NewPlatformType), new TargetPlatformDescriptor(FirstPlatformType));
                                }
                            }
						}
					}
					else if (AllowPlatformParams)
					{
						// Look up platform names in the command line: -Platform_1 -Platform_2 ... -Platform_k
						TargetPlatforms = new List<TargetPlatformDescriptor>();
						foreach (UnrealTargetPlatform PlatType in Enum.GetValues(typeof(UnrealTargetPlatform)))
						{
							if (PlatType != UnrealTargetPlatform.Unknown)
							{
								if (Command.ParseParam(PlatType.ToString()))
								{
                                    List<TargetPlatformDescriptor> PlatformDescriptors = Platform.GetValidTargetPlatforms(PlatType, CookFlavors);
                                    TargetPlatforms.AddRange(PlatformDescriptors);
								}
							}
						}
					}
				}
			}
			else
			{
				TargetPlatforms = OverrideTargetPlatforms;
			}
			if (CommandUtils.IsNullOrEmpty(TargetPlatforms))
			{
				// Revert to single default platform - the current platform we're running
				TargetPlatforms = DefaultTargetPlatforms;
			}
			return TargetPlatforms;
		}

		public ProjectParams(ProjectParams InParams)
		{
			//
			//// Use this.Name with properties and fields!
			//

			this.RawProjectPath = InParams.RawProjectPath;
			this.MapsToCook = InParams.MapsToCook;
			this.MapIniSectionsToCook = InParams.MapIniSectionsToCook;
			this.DirectoriesToCook = InParams.DirectoriesToCook;
            this.InternationalizationPreset = InParams.InternationalizationPreset;
            this.CulturesToCook = InParams.CulturesToCook;
            this.BasedOnReleaseVersion = InParams.BasedOnReleaseVersion;
            this.CreateReleaseVersion = InParams.CreateReleaseVersion;
            this.GeneratePatch = InParams.GeneratePatch;
			this.AddPatchLevel = InParams.AddPatchLevel;
			this.StageBaseReleasePaks = InParams.StageBaseReleasePaks;
			this.DLCFile = InParams.DLCFile;
			this.GenerateRemaster = InParams.GenerateRemaster;
            this.DLCIncludeEngineContent = InParams.DLCIncludeEngineContent;
            this.DiffCookedContentPath = InParams.DiffCookedContentPath;
            this.AdditionalCookerOptions = InParams.AdditionalCookerOptions;
			this.ClientCookedTargets = InParams.ClientCookedTargets;
			this.ServerCookedTargets = InParams.ServerCookedTargets;
			this.EditorTargets = InParams.EditorTargets;
			this.ProgramTargets = InParams.ProgramTargets;
			this.ClientTargetPlatforms = InParams.ClientTargetPlatforms;
            this.ClientDependentPlatformMap = InParams.ClientDependentPlatformMap;
			this.ServerTargetPlatforms = InParams.ServerTargetPlatforms;
            this.ServerDependentPlatformMap = InParams.ServerDependentPlatformMap;
			this.Build = InParams.Build;
			this.SkipBuildClient = InParams.SkipBuildClient;
			this.SkipBuildEditor = InParams.SkipBuildEditor;
			this.Run = InParams.Run;
			this.Cook = InParams.Cook;
			this.IterativeCooking = InParams.IterativeCooking;
			this.IterateSharedCookedBuild = InParams.IterateSharedCookedBuild;
			this.IterateSharedBuildUsePrecompiledExe = InParams.IterateSharedBuildUsePrecompiledExe;
			this.CookAll = InParams.CookAll;
			this.CookPartialGC = InParams.CookPartialGC;
			this.CookInEditor = InParams.CookInEditor; 
			this.CookOutputDir = InParams.CookOutputDir;
			this.CookMapsOnly = InParams.CookMapsOnly;
			this.SkipCook = InParams.SkipCook;
			this.SkipCookOnTheFly = InParams.SkipCookOnTheFly;
            this.Prebuilt = InParams.Prebuilt;
            this.RunTimeoutSeconds = InParams.RunTimeoutSeconds;
			this.Clean = InParams.Clean;
			this.Pak = InParams.Pak;
			this.SignPak = InParams.SignPak;
			this.SignedPak = InParams.SignedPak;
			this.SkipPak = InParams.SkipPak;
			this.NoXGE = InParams.NoXGE;
			this.CookOnTheFly = InParams.CookOnTheFly;
            this.CookOnTheFlyStreaming = InParams.CookOnTheFlyStreaming;
            this.UnversionedCookedContent = InParams.UnversionedCookedContent;
			this.EncryptIniFiles = InParams.EncryptIniFiles;
            this.EncryptPakIndex = InParams.EncryptPakIndex;
            this.EncryptEverything = InParams.EncryptEverything;
			this.SkipCookingEditorContent = InParams.SkipCookingEditorContent;
            this.NumCookersToSpawn = InParams.NumCookersToSpawn;
			this.FileServer = InParams.FileServer;
			this.DedicatedServer = InParams.DedicatedServer;
			this.Client = InParams.Client;
			this.NoClient = InParams.NoClient;
			this.LogWindow = InParams.LogWindow;
			this.Stage = InParams.Stage;
			this.SkipStage = InParams.SkipStage;
            this.StageDirectoryParam = InParams.StageDirectoryParam;
			this.Manifests = InParams.Manifests;
            this.CreateChunkInstall = InParams.CreateChunkInstall;
			this.UE4Exe = InParams.UE4Exe;
			this.NoDebugInfo = InParams.NoDebugInfo;
			this.MapFile = InParams.MapFile;
			this.NoCleanStage = InParams.NoCleanStage;
			this.MapToRun = InParams.MapToRun;
			this.AdditionalServerMapParams = InParams.AdditionalServerMapParams;
			this.Foreign = InParams.Foreign;
			this.ForeignCode = InParams.ForeignCode;
			this.StageCommandline = InParams.StageCommandline;
            this.BundleName = InParams.BundleName;
			this.RunCommandline = InParams.RunCommandline;
			this.ServerCommandline = InParams.ServerCommandline;
            this.ClientCommandline = InParams.ClientCommandline;
            this.Package = InParams.Package;
			this.Deploy = InParams.Deploy;
			this.DeployFolder = InParams.DeployFolder;
			this.IterativeDeploy = InParams.IterativeDeploy;
			this.IgnoreCookErrors = InParams.IgnoreCookErrors;
			this.FastCook = InParams.FastCook;
			this.Devices = InParams.Devices;
			this.DeviceNames = InParams.DeviceNames;
			this.ServerDevice = InParams.ServerDevice;
            this.NullRHI = InParams.NullRHI;
            this.FakeClient = InParams.FakeClient;
            this.EditorTest = InParams.EditorTest;
            this.RunAutomationTests = InParams.RunAutomationTests;
            this.RunAutomationTest = InParams.RunAutomationTest;
            this.CrashIndex = InParams.CrashIndex;
            this.Port = InParams.Port;
			this.SkipServer = InParams.SkipServer;
			this.Unattended = InParams.Unattended;
            this.ServerDeviceAddress = InParams.ServerDeviceAddress;
            this.DeviceUsername = InParams.DeviceUsername;
            this.DevicePassword = InParams.DevicePassword;
            this.CrashReporter = InParams.CrashReporter;
			this.ClientConfigsToBuild = InParams.ClientConfigsToBuild;
			this.ServerConfigsToBuild = InParams.ServerConfigsToBuild;
			this.NumClients = InParams.NumClients;
            this.Compressed = InParams.Compressed;
            this.UseDebugParamForEditorExe = InParams.UseDebugParamForEditorExe;
			this.Archive = InParams.Archive;
			this.ArchiveDirectoryParam = InParams.ArchiveDirectoryParam;
			this.ArchiveMetaData = InParams.ArchiveMetaData;
			this.CreateAppBundle = InParams.CreateAppBundle;
			this.Distribution = InParams.Distribution;
			this.Prereqs = InParams.Prereqs;
			this.AppLocalDirectory = InParams.AppLocalDirectory;
			this.NoBootstrapExe = InParams.NoBootstrapExe;
            this.Prebuilt = InParams.Prebuilt;
            this.RunTimeoutSeconds = InParams.RunTimeoutSeconds;
			this.bIsCodeBasedProject = InParams.bIsCodeBasedProject;
			this.bCodeSign = InParams.bCodeSign;
			this.TitleID = InParams.TitleID;
			this.bTreatNonShippingBinariesAsDebugFiles = InParams.bTreatNonShippingBinariesAsDebugFiles;
			this.RunAssetNativization = InParams.RunAssetNativization;
		}

		/// <summary>
		/// Constructor. Be sure to use this.ParamName to set the actual property name as parameter names and property names
		/// overlap here.
		/// If a parameter value is not set, it will be parsed from the command line; if the command is null, the default value will be used.
		/// </summary>
		public ProjectParams(			
			FileReference RawProjectPath,

			BuildCommand Command = null,
			string Device = null,			
			string MapToRun = null,	
			string AdditionalServerMapParams = null,
			ParamList<string> Port = null,
			string RunCommandline = null,						
			string StageCommandline = null,
            string BundleName = null,
            string StageDirectoryParam = null,
			string UE4Exe = null,
			string SignPak = null,
			List<UnrealTargetConfiguration> ClientConfigsToBuild = null,
			List<UnrealTargetConfiguration> ServerConfigsToBuild = null,
			ParamList<string> MapsToCook = null,
			ParamList<string> MapIniSectionsToCook = null,
			ParamList<string> DirectoriesToCook = null,
            string InternationalizationPreset = null,
            ParamList<string> CulturesToCook = null,
			ParamList<string> ClientCookedTargets = null,
			ParamList<string> EditorTargets = null,
			ParamList<string> ServerCookedTargets = null,
			List<TargetPlatformDescriptor> ClientTargetPlatforms = null,
            Dictionary<TargetPlatformDescriptor, TargetPlatformDescriptor> ClientDependentPlatformMap = null,
			List<TargetPlatformDescriptor> ServerTargetPlatforms = null,
            Dictionary<TargetPlatformDescriptor, TargetPlatformDescriptor> ServerDependentPlatformMap = null,
			bool? Build = null,
			bool? SkipBuildClient = null,
			bool? SkipBuildEditor = null,
			bool? Cook = null,
			bool? Run = null,
			bool? SkipServer = null,
			bool? Clean = null,
            bool? Compressed = null,
            bool? UseDebugParamForEditorExe = null,
            bool? IterativeCooking = null,
			bool? IterateSharedCookedBuild = null,
			bool? IterateSharedBuildUsePrecompiledExe = null,
			bool? CookAll = null,
			bool? CookPartialGC = null,
			bool? CookInEditor = null,
			string CookOutputDir = null,
			bool? CookMapsOnly = null,
            bool? CookOnTheFly = null,
            bool? CookOnTheFlyStreaming = null,
            bool? UnversionedCookedContent = null,
			bool? EncryptIniFiles = null,
            bool? EncryptPakIndex = null,
            bool? EncryptEverything = null,
			bool? SkipCookingEditorContent = null,
            int? NumCookersToSpawn = null,
            string AdditionalCookerOptions = null,
            string BasedOnReleaseVersion = null,
            string CreateReleaseVersion = null,
			string CreateReleaseVersionBasePath = null,
			string BasedOnReleaseVersionBasePath = null,
            bool? GeneratePatch = null,
			bool? AddPatchLevel = null,
			bool? StageBaseReleasePaks = null,
			bool? GenerateRemaster = null,
			string DLCName = null,
            string DiffCookedContentPath = null,
            bool? DLCIncludeEngineContent = null,
			bool? CrashReporter = null,
			bool? DedicatedServer = null,
			bool? Client = null,
			bool? Deploy = null,
			string DeployFolder = null,
			bool? FileServer = null,
			bool? Foreign = null,
			bool? ForeignCode = null,
			bool? LogWindow = null,
			bool? NoCleanStage = null,
			bool? NoClient = null,
			bool? NoDebugInfo = null,
			bool? MapFile = null,
			bool? NoXGE = null,
			bool? Package = null,
			bool? Pak = null,
			bool? Prereqs = null,
			string AppLocalDirectory = null,
			bool? NoBootstrapExe = null,
            bool? SignedPak = null,
            bool? NullRHI = null,
            bool? FakeClient = null,
            bool? EditorTest = null,
            bool? RunAutomationTests = null,
            string RunAutomationTest = null,
            int? CrashIndex = null,
			bool? SkipCook = null,
			bool? SkipCookOnTheFly = null,
			bool? SkipPak = null,
			bool? SkipStage = null,
			bool? Stage = null,
			bool? Manifests = null,
            bool? CreateChunkInstall = null,
			bool? Unattended = null,
			int? NumClients = null,
			bool? Archive = null,
			string ArchiveDirectoryParam = null,
			bool? ArchiveMetaData = null,
			bool? CreateAppBundle = null,
			ParamList<string> ProgramTargets = null,
			bool? Distribution = null,
            bool? Prebuilt = null,
            int? RunTimeoutSeconds = null,
			string SpecifiedArchitecture = null,
            bool? IterativeDeploy = null,
			bool? FastCook = null,
			bool? IgnoreCookErrors = null,
            bool? RunAssetNativization = null,
			bool? CodeSign = null,
			bool? TreatNonShippingBinariesAsDebugFiles = null,
			string Provision = null,
			string Certificate = null,
		    string Team = null,
		    bool AutomaticSigning = false,
			ParamList<string> InMapsToRebuildLightMaps = null,
			ParamList<string> TitleID = null
			)
		{
			//
			//// Use this.Name with properties and fields!
			//

			this.RawProjectPath = RawProjectPath;
			if (DirectoriesToCook != null)
			{
				this.DirectoriesToCook = DirectoriesToCook;
			}
            this.InternationalizationPreset = ParseParamValueIfNotSpecified(Command, InternationalizationPreset, "i18npreset");

            // If not specified in parameters, check commandline.
            if (CulturesToCook == null)
            {
                if (Command != null)
                {
                    var CookCulturesString = Command.ParseParamValue("CookCultures");
                    if (CookCulturesString != null)
                    {
                        this.CulturesToCook = new ParamList<string>(CookCulturesString.Split(','));
                    }
                }
            }
            else
            {
                this.CulturesToCook = CulturesToCook;
            }

			if (ClientCookedTargets != null)
			{
				this.ClientCookedTargets = ClientCookedTargets;
			}
			if (ServerCookedTargets != null)
			{
				this.ServerCookedTargets = ServerCookedTargets;
			}
			if (EditorTargets != null)
			{
				this.EditorTargets = EditorTargets;
			}
			if (ProgramTargets != null)
			{
				this.ProgramTargets = ProgramTargets;
			}

			// Parse command line params for client platforms "-TargetPlatform=Win64+Mac", "-Platform=Win64+Mac" and also "-Win64", "-Mac" etc.
            if (ClientDependentPlatformMap != null)
            {
                this.ClientDependentPlatformMap = ClientDependentPlatformMap;
            }

            List<TargetPlatformDescriptor> DefaultTargetPlatforms = new ParamList<TargetPlatformDescriptor>(new TargetPlatformDescriptor(HostPlatform.Current.HostEditorPlatform));
            this.ClientTargetPlatforms = SetupTargetPlatforms(ref this.ClientDependentPlatformMap, Command, ClientTargetPlatforms, DefaultTargetPlatforms, true, "TargetPlatform", "Platform");

            // Parse command line params for server platforms "-ServerTargetPlatform=Win64+Mac", "-ServerPlatform=Win64+Mac". "-Win64" etc is not allowed here
            if (ServerDependentPlatformMap != null)
            {
                this.ServerDependentPlatformMap = ServerDependentPlatformMap;
            }
            this.ServerTargetPlatforms = SetupTargetPlatforms(ref this.ServerDependentPlatformMap, Command, ServerTargetPlatforms, this.ClientTargetPlatforms, false, "ServerTargetPlatform", "ServerPlatform");

			this.Build = GetParamValueIfNotSpecified(Command, Build, this.Build, "build");
			this.SkipBuildClient = GetParamValueIfNotSpecified(Command, SkipBuildClient, this.SkipBuildEditor, "skipbuildclient");
			this.SkipBuildEditor = GetParamValueIfNotSpecified(Command, SkipBuildEditor, this.SkipBuildEditor, "skipbuildeditor");
			this.Run = GetParamValueIfNotSpecified(Command, Run, this.Run, "run");
			this.Cook = GetParamValueIfNotSpecified(Command, Cook, this.Cook, "cook");
			this.CreateReleaseVersionBasePath = ParseParamValueIfNotSpecified(Command, CreateReleaseVersionBasePath, "createreleaseversionroot", String.Empty);
			this.BasedOnReleaseVersionBasePath = ParseParamValueIfNotSpecified(Command, BasedOnReleaseVersionBasePath, "basedonreleaseversionroot", String.Empty);
            this.CreateReleaseVersion = ParseParamValueIfNotSpecified(Command, CreateReleaseVersion, "createreleaseversion", String.Empty);
            this.BasedOnReleaseVersion = ParseParamValueIfNotSpecified(Command, BasedOnReleaseVersion, "basedonreleaseversion", String.Empty);
            this.GeneratePatch = GetParamValueIfNotSpecified(Command, GeneratePatch, this.GeneratePatch, "GeneratePatch");
            this.AddPatchLevel = GetParamValueIfNotSpecified(Command, AddPatchLevel, this.AddPatchLevel, "AddPatchLevel");
			this.StageBaseReleasePaks = GetParamValueIfNotSpecified(Command, StageBaseReleasePaks, this.StageBaseReleasePaks, "StageBaseReleasePaks");
			this.GenerateRemaster = GetParamValueIfNotSpecified(Command, GenerateRemaster, this.GenerateRemaster, "GenerateRemaster");
			this.AdditionalCookerOptions = ParseParamValueIfNotSpecified(Command, AdditionalCookerOptions, "AdditionalCookerOptions", String.Empty);

			DLCName = ParseParamValueIfNotSpecified(Command, DLCName, "DLCName", String.Empty);
			if(!String.IsNullOrEmpty(DLCName))
			{
				List<PluginInfo> CandidatePlugins = Plugins.ReadAvailablePlugins(CommandUtils.EngineDirectory, RawProjectPath, null);
				PluginInfo DLCPlugin = CandidatePlugins.FirstOrDefault(x => String.Equals(x.Name, DLCName, StringComparison.InvariantCultureIgnoreCase));
				if(DLCPlugin == null)
				{
					throw new AutomationException("Unable to find plugin '{0}' for building DLC", DLCName);
				}
				DLCFile = DLCPlugin.File;
			}

            //this.DLCName = 
            this.DiffCookedContentPath = ParseParamValueIfNotSpecified(Command, DiffCookedContentPath, "DiffCookedContentPath", String.Empty);
            this.DLCIncludeEngineContent = GetParamValueIfNotSpecified(Command, DLCIncludeEngineContent, this.DLCIncludeEngineContent, "DLCIncludeEngineContent");
			this.SkipCook = GetParamValueIfNotSpecified(Command, SkipCook, this.SkipCook, "skipcook");
			if (this.SkipCook)
			{
				this.Cook = true;
			}
			this.Clean = GetOptionalParamValueIfNotSpecified(Command, Clean, this.Clean, "clean", null);
			this.SignPak = ParseParamValueIfNotSpecified(Command, SignPak, "signpak", String.Empty);
			this.SignedPak = !String.IsNullOrEmpty(this.SignPak) || GetParamValueIfNotSpecified(Command, SignedPak, this.SignedPak, "signedpak");
			if (string.IsNullOrEmpty(this.SignPak))
			{
				this.SignPak = Path.Combine(RawProjectPath.Directory.FullName, @"Build\NoRedist\Keys.txt");
				if (!File.Exists(this.SignPak))
				{
					this.SignPak = null;
				}
			}
			this.Pak = GetParamValueIfNotSpecified(Command, Pak, this.Pak, "pak");
			this.SkipPak = GetParamValueIfNotSpecified(Command, SkipPak, this.SkipPak, "skippak");
			if (this.SkipPak)
			{
				this.Pak = true;
			}
			this.NoXGE = GetParamValueIfNotSpecified(Command, NoXGE, this.NoXGE, "noxge");
			this.CookOnTheFly = GetParamValueIfNotSpecified(Command, CookOnTheFly, this.CookOnTheFly, "cookonthefly");
            if (this.CookOnTheFly && this.SkipCook)
            {
                this.Cook = false;
            }
            this.CookOnTheFlyStreaming = GetParamValueIfNotSpecified(Command, CookOnTheFlyStreaming, this.CookOnTheFlyStreaming, "cookontheflystreaming");
            this.UnversionedCookedContent = GetParamValueIfNotSpecified(Command, UnversionedCookedContent, this.UnversionedCookedContent, "UnversionedCookedContent");
			this.EncryptIniFiles = GetParamValueIfNotSpecified(Command, EncryptIniFiles, this.EncryptIniFiles, "EncryptIniFiles");
            this.EncryptPakIndex = GetParamValueIfNotSpecified(Command, EncryptPakIndex, this.EncryptPakIndex, "EncryptPakIndex");
            this.EncryptEverything = GetParamValueIfNotSpecified(Command, EncryptEverything, this.EncryptEverything, "EncryptEverything");
			this.SkipCookingEditorContent = GetParamValueIfNotSpecified(Command, SkipCookingEditorContent, this.SkipCookingEditorContent, "SkipCookingEditorContent");
            if (NumCookersToSpawn.HasValue)
            {
                this.NumCookersToSpawn = NumCookersToSpawn.Value;
            }
            else if (Command != null)
            {
                this.NumCookersToSpawn = Command.ParseParamInt("NumCookersToSpawn");
            }
            this.Compressed = GetParamValueIfNotSpecified(Command, Compressed, this.Compressed, "compressed");
            this.UseDebugParamForEditorExe = GetParamValueIfNotSpecified(Command, UseDebugParamForEditorExe, this.UseDebugParamForEditorExe, "UseDebugParamForEditorExe");
			this.IterativeCooking = GetParamValueIfNotSpecified(Command, IterativeCooking, this.IterativeCooking, new string[] { "iterativecooking", "iterate" });
			this.IterateSharedCookedBuild = GetParamValueIfNotSpecified(Command, IterateSharedCookedBuild, this.IterateSharedCookedBuild, new string[] { "IterateSharedCookedBuild"});
			this.IterateSharedBuildUsePrecompiledExe = GetParamValueIfNotSpecified(Command, IterateSharedBuildUsePrecompiledExe, this.IterateSharedBuildUsePrecompiledExe, new string[] { "IterateSharedBuildUsePrecompiledExe" });

			this.SkipCookOnTheFly = GetParamValueIfNotSpecified(Command, SkipCookOnTheFly, this.SkipCookOnTheFly, "skipcookonthefly");
			this.CookAll = GetParamValueIfNotSpecified(Command, CookAll, this.CookAll, "CookAll");
			this.CookPartialGC = GetParamValueIfNotSpecified(Command, CookPartialGC, this.CookPartialGC, "CookPartialGC");
			this.CookInEditor = GetParamValueIfNotSpecified(Command, CookInEditor, this.CookInEditor, "CookInEditor");
			this.CookOutputDir = ParseParamValueIfNotSpecified(Command, CookOutputDir, "CookOutputDir", String.Empty, true);
			this.CookMapsOnly = GetParamValueIfNotSpecified(Command, CookMapsOnly, this.CookMapsOnly, "CookMapsOnly");
			this.FileServer = GetParamValueIfNotSpecified(Command, FileServer, this.FileServer, "fileserver");
			this.DedicatedServer = GetParamValueIfNotSpecified(Command, DedicatedServer, this.DedicatedServer, "dedicatedserver", "server");
			this.Client = GetParamValueIfNotSpecified(Command, Client, this.Client, "client");
			/*if( this.Client )
			{
				this.DedicatedServer = true;
			}*/
			this.NoClient = GetParamValueIfNotSpecified(Command, NoClient, this.NoClient, "noclient");
			this.LogWindow = GetParamValueIfNotSpecified(Command, LogWindow, this.LogWindow, "logwindow");
			this.Stage = GetParamValueIfNotSpecified(Command, Stage, this.Stage, "stage");
			this.SkipStage = GetParamValueIfNotSpecified(Command, SkipStage, this.SkipStage, "skipstage");
			if (this.SkipStage)
			{
				this.Stage = true;
			}
			this.StageDirectoryParam = ParseParamValueIfNotSpecified(Command, StageDirectoryParam, "stagingdirectory", String.Empty, true);
			this.bCodeSign = GetOptionalParamValueIfNotSpecified(Command, CodeSign, CommandUtils.IsBuildMachine, "CodeSign", "NoCodeSign").GetValueOrDefault();
			this.bTreatNonShippingBinariesAsDebugFiles = GetParamValueIfNotSpecified(Command, TreatNonShippingBinariesAsDebugFiles, false, "TreatNonShippingBinariesAsDebugFiles");
			this.Manifests = GetParamValueIfNotSpecified(Command, Manifests, this.Manifests, "manifests");
            this.CreateChunkInstall = GetParamValueIfNotSpecified(Command, CreateChunkInstall, this.CreateChunkInstall, "createchunkinstall");
			this.ChunkInstallDirectory = ParseParamValueIfNotSpecified(Command, ChunkInstallDirectory, "chunkinstalldirectory", String.Empty, true);
			this.ChunkInstallVersionString = ParseParamValueIfNotSpecified(Command, ChunkInstallVersionString, "chunkinstallversion", String.Empty, true);
            this.ChunkInstallReleaseString = ParseParamValueIfNotSpecified(Command, ChunkInstallReleaseString, "chunkinstallrelease", String.Empty, true);
            if (string.IsNullOrEmpty(this.ChunkInstallReleaseString))
            {
                this.ChunkInstallReleaseString = this.ChunkInstallVersionString;
            }
			this.Archive = GetParamValueIfNotSpecified(Command, Archive, this.Archive, "archive");
			this.ArchiveDirectoryParam = ParseParamValueIfNotSpecified(Command, ArchiveDirectoryParam, "archivedirectory", String.Empty, true);
			this.ArchiveMetaData = GetParamValueIfNotSpecified(Command, ArchiveMetaData, this.ArchiveMetaData, "archivemetadata");
			this.CreateAppBundle = GetParamValueIfNotSpecified(Command, CreateAppBundle, true, "createappbundle");
			this.Distribution = GetParamValueIfNotSpecified(Command, Distribution, this.Distribution, "distribution");
			this.Prereqs = GetParamValueIfNotSpecified(Command, Prereqs, this.Prereqs, "prereqs");
			this.AppLocalDirectory = ParseParamValueIfNotSpecified(Command, AppLocalDirectory, "applocaldirectory", String.Empty, true);
			this.NoBootstrapExe = GetParamValueIfNotSpecified(Command, NoBootstrapExe, this.NoBootstrapExe, "nobootstrapexe");
            this.Prebuilt = GetParamValueIfNotSpecified(Command, Prebuilt, this.Prebuilt, "prebuilt");
            if (this.Prebuilt)
            {
                this.SkipCook = true;
                /*this.SkipPak = true;
                this.SkipStage = true;
                this.Pak = true;
                this.Stage = true;*/
                this.Cook = true;
                this.Archive = true;
                
                this.Deploy = true;
                this.Run = true;
                //this.StageDirectoryParam = this.PrebuiltDir;
            }
            this.NoDebugInfo = GetParamValueIfNotSpecified(Command, NoDebugInfo, this.NoDebugInfo, "nodebuginfo");
			this.MapFile = GetParamValueIfNotSpecified(Command, MapFile, this.MapFile, "mapfile");
			this.NoCleanStage = GetParamValueIfNotSpecified(Command, NoCleanStage, this.NoCleanStage, "nocleanstage");
			this.MapToRun = ParseParamValueIfNotSpecified(Command, MapToRun, "map", String.Empty);
			this.AdditionalServerMapParams = ParseParamValueIfNotSpecified(Command, AdditionalServerMapParams, "AdditionalServerMapParams", String.Empty);
			this.Foreign = GetParamValueIfNotSpecified(Command, Foreign, this.Foreign, "foreign");
			this.ForeignCode = GetParamValueIfNotSpecified(Command, ForeignCode, this.ForeignCode, "foreigncode");
			this.StageCommandline = ParseParamValueIfNotSpecified(Command, StageCommandline, "cmdline");
			this.BundleName = ParseParamValueIfNotSpecified(Command, BundleName, "bundlename");
			this.RunCommandline = ParseParamValueIfNotSpecified(Command, RunCommandline, "addcmdline");
			this.RunCommandline = this.RunCommandline.Replace('\'', '\"'); // replace any single quotes with double quotes
			this.ServerCommandline = ParseParamValueIfNotSpecified(Command, ServerCommandline, "servercmdline");
			this.ServerCommandline = this.ServerCommandline.Replace('\'', '\"'); // replace any single quotes with double quotes
            this.ClientCommandline = ParseParamValueIfNotSpecified(Command, ClientCommandline, "clientcmdline");
            this.ClientCommandline = this.ClientCommandline.Replace('\'', '\"'); // replace any single quotes with double quotes
            this.Package = GetParamValueIfNotSpecified(Command, Package, this.Package, "package");

			this.Deploy = GetParamValueIfNotSpecified(Command, Deploy, this.Deploy, "deploy");
			this.DeployFolder = ParseParamValueIfNotSpecified(Command, DeployFolder, "deploy", null);

			// if the user specified -deploy but no folder, set the default
			if (this.Deploy && string.IsNullOrEmpty(this.DeployFolder))
			{
				this.DeployFolder = this.ShortProjectName;
			}
			else if (string.IsNullOrEmpty(this.DeployFolder) == false)
			{
				// if the user specified a folder set deploy to true.
				//@todo - remove 'deploy' var and check deployfolder != null?
				this.Deploy = true;
			}

			this.IterativeDeploy = GetParamValueIfNotSpecified(Command, IterativeDeploy, this.IterativeDeploy, new string[] {"iterativedeploy", "iterate" } );
			this.FastCook = GetParamValueIfNotSpecified(Command, FastCook, this.FastCook, "FastCook");
			this.IgnoreCookErrors = GetParamValueIfNotSpecified(Command, IgnoreCookErrors, this.IgnoreCookErrors, "IgnoreCookErrors");

            // Determine whether or not we're going to nativize Blueprint assets at cook time.
            this.RunAssetNativization = false;
            ConfigHierarchy GameIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Game, RawProjectPath.Directory, HostPlatform.Current.HostEditorPlatform);
            if (GameIni != null)
            {
                string BlueprintNativizationMethod;
                if (GameIni.TryGetValue("/Script/UnrealEd.ProjectPackagingSettings", "BlueprintNativizationMethod", out BlueprintNativizationMethod))
                {
                    this.RunAssetNativization = !string.IsNullOrEmpty(BlueprintNativizationMethod) && BlueprintNativizationMethod != "Disabled";
                }
            }

            string DeviceString = ParseParamValueIfNotSpecified(Command, Device, "device", String.Empty).Trim(new char[] { '\"' });
            if(DeviceString == "")
            {
                this.Devices = new ParamList<string>("");
                this.DeviceNames = new ParamList<string>("");
            }
            else
            {
                this.Devices = new ParamList<string>(DeviceString.Split('+'));
                this.DeviceNames = new ParamList<string>();
                foreach (var d in this.Devices)
                {
                    // strip the platform prefix the specified device.
                    if (d.Contains("@"))
                    {
                        this.DeviceNames.Add(d.Substring(d.IndexOf("@") + 1));
                    }
                    else
                    {
                        this.DeviceNames.Add(d);
                    }
                }
            }

			this.Provision = ParseParamValueIfNotSpecified(Command, Provision, "provision", String.Empty, true);
			this.Certificate = ParseParamValueIfNotSpecified(Command, Certificate, "certificate", String.Empty, true);
			this.Team = ParseParamValueIfNotSpecified(Command, Team, "team", String.Empty, true);
			this.AutomaticSigning = GetParamValueIfNotSpecified(Command, AutomaticSigning, this.AutomaticSigning, "AutomaticSigning");

			this.ServerDevice = ParseParamValueIfNotSpecified(Command, ServerDevice, "serverdevice", this.Devices.Count > 0 ? this.Devices[0] : "");
			this.NullRHI = GetParamValueIfNotSpecified(Command, NullRHI, this.NullRHI, "nullrhi");
			this.FakeClient = GetParamValueIfNotSpecified(Command, FakeClient, this.FakeClient, "fakeclient");
			this.EditorTest = GetParamValueIfNotSpecified(Command, EditorTest, this.EditorTest, "editortest");
            this.RunAutomationTest = ParseParamValueIfNotSpecified(Command, RunAutomationTest, "RunAutomationTest");
            this.RunAutomationTests = this.RunAutomationTest != "" || GetParamValueIfNotSpecified(Command, RunAutomationTests, this.RunAutomationTests, "RunAutomationTests");
            this.SkipServer = GetParamValueIfNotSpecified(Command, SkipServer, this.SkipServer, "skipserver");
			this.UE4Exe = ParseParamValueIfNotSpecified(Command, UE4Exe, "ue4exe", "UE4Editor-Cmd.exe");
			this.Unattended = GetParamValueIfNotSpecified(Command, Unattended, this.Unattended, "unattended");
			this.DeviceUsername = ParseParamValueIfNotSpecified(Command, DeviceUsername, "deviceuser", String.Empty);
			this.DevicePassword = ParseParamValueIfNotSpecified(Command, DevicePassword, "devicepass", String.Empty);
			this.CrashReporter = GetParamValueIfNotSpecified(Command, CrashReporter, this.CrashReporter, "crashreporter");
			this.SpecifiedArchitecture = ParseParamValueIfNotSpecified(Command, SpecifiedArchitecture, "specifiedarchitecture", String.Empty);

			if (ClientConfigsToBuild == null)
			{
				if (Command != null)
				{
					var ClientConfig = Command.ParseParamValue("clientconfig");

                    if (ClientConfig == null)
                        ClientConfig = Command.ParseParamValue("config");

                    if (ClientConfig != null)
					{
						this.ClientConfigsToBuild = new List<UnrealTargetConfiguration>();
						var Configs = new ParamList<string>(ClientConfig.Split('+'));
						foreach (var ConfigName in Configs)
						{
							this.ClientConfigsToBuild.Add((UnrealTargetConfiguration)Enum.Parse(typeof(UnrealTargetConfiguration), ConfigName, true));
						}
					}
				}
			}
			else
			{
				this.ClientConfigsToBuild = ClientConfigsToBuild;
			}

            if (Port == null)
            {
                if( Command != null )
                {
                    this.Port = new ParamList<string>();

                    var PortString = Command.ParseParamValue("port");
                    if (String.IsNullOrEmpty(PortString) == false)
                    {
                        var Ports = new ParamList<string>(PortString.Split('+'));
                        foreach (var P in Ports)
                        {
                            this.Port.Add(P);
                        }
                    }
                    
                }
            }
            else
            {
                this.Port = Port;
            }

            if (MapsToCook == null)
            {
                if (Command != null)
                {
                    this.MapsToCook = new ParamList<string>();

                    var MapsString = Command.ParseParamValue("MapsToCook");
                    if (String.IsNullOrEmpty(MapsString) == false)
                    {
                        var MapNames = new ParamList<string>(MapsString.Split('+'));
                        foreach ( var M in MapNames ) 
                        {
                            this.MapsToCook.Add( M );
                        }
                    }
                }
            }
            else
            {
                this.MapsToCook = MapsToCook;
            }

			if (MapIniSectionsToCook == null)
			{
				if (Command != null)
				{
					this.MapIniSectionsToCook = new ParamList<string>();

					var MapsString = Command.ParseParamValue("MapIniSectionsToCook");
					if (String.IsNullOrEmpty(MapsString) == false)
					{
						var MapNames = new ParamList<string>(MapsString.Split('+'));
						foreach (var M in MapNames)
						{
							this.MapIniSectionsToCook.Add(M);
						}
					}
				}
			}
			else
			{
				this.MapIniSectionsToCook = MapIniSectionsToCook;
			}

			if (String.IsNullOrEmpty(this.MapToRun) == false)
            {
                this.MapsToCook.Add(this.MapToRun);
            }

			if (InMapsToRebuildLightMaps == null)
			{
				if (Command != null)
				{
					this.MapsToRebuildLightMaps = new ParamList<string>();

					var MapsString = Command.ParseParamValue("MapsToRebuildLightMaps");
					if (String.IsNullOrEmpty(MapsString) == false)
					{
						var MapNames = new ParamList<string>(MapsString.Split('+'));
						foreach (var M in MapNames)
						{
							this.MapsToRebuildLightMaps.Add(M);
						}
					}
				}
			}
			else
			{
				this.MapsToRebuildLightMaps = InMapsToRebuildLightMaps;
			}

			if (TitleID == null)
			{
				if (Command != null)
				{
					this.TitleID = new ParamList<string>();

					var TitleString = Command.ParseParamValue("TitleID");
					if (String.IsNullOrEmpty(TitleString) == false)
					{
						var TitleIDs = new ParamList<string>(TitleString.Split('+'));
						foreach (var T in TitleIDs)
						{
							this.TitleID.Add(T);
						}
					}
				}
			}
			else
			{
				this.TitleID = TitleID;
			}

			if (ServerConfigsToBuild == null)
			{
				if (Command != null)
				{
					var ServerConfig = Command.ParseParamValue("serverconfig");

                    if (ServerConfig == null)
                        ServerConfig = Command.ParseParamValue("config");

                    if (ServerConfig != null)
					{
						this.ServerConfigsToBuild = new List<UnrealTargetConfiguration>();
						var Configs = new ParamList<string>(ServerConfig.Split('+'));
						foreach (var ConfigName in Configs)
						{
							this.ServerConfigsToBuild.Add((UnrealTargetConfiguration)Enum.Parse(typeof(UnrealTargetConfiguration), ConfigName, true));
						}
					}
				}
			}
			else
			{
				this.ServerConfigsToBuild = ServerConfigsToBuild;
			}
			if (NumClients.HasValue)
			{
				this.NumClients = NumClients.Value;
			}
			else if (Command != null)
			{
				this.NumClients = Command.ParseParamInt("numclients");
			}
            if (CrashIndex.HasValue)
            {
                this.CrashIndex = CrashIndex.Value;
            }
            else if (Command != null)
            {
                this.CrashIndex = Command.ParseParamInt("CrashIndex");
            }
            if (RunTimeoutSeconds.HasValue)
            {
                this.RunTimeoutSeconds = RunTimeoutSeconds.Value;
            }
            else if (Command != null)
            {
                this.RunTimeoutSeconds = Command.ParseParamInt("runtimeoutseconds");
            }

			// Gather up any '-ini:' arguments and save them. We'll pass these along to other tools that may be spawned in a new process as part of the command.
			foreach (string Param in Command.Params)
			{
				if (Param.StartsWith("ini:", StringComparison.InvariantCultureIgnoreCase))
				{
					this.ConfigOverrideParams.Add(Param);
				}
			}

			AutodetectSettings(false);
			ValidateAndLog();
		}

		#endregion

		#region Shared

		/// <summary>
		/// Shared: Full path to the .uproject file
		/// </summary>
		public FileReference RawProjectPath { private set; get; }

		/// <summary>
		/// Shared: The current project is a foreign project, commandline: -foreign
		/// </summary>
		[Help("foreign", "Generate a foreign uproject from blankproject and use that")]
		public bool Foreign { private set; get; }

		/// <summary>
		/// Shared: The current project is a foreign project, commandline: -foreign
		/// </summary>
		[Help("foreigncode", "Generate a foreign code uproject from platformergame and use that")]
		public bool ForeignCode { private set; get; }

		/// <summary>
		/// Shared: true if we should build crash reporter
		/// </summary>
		[Help("CrashReporter", "true if we should build crash reporter")]
		public bool CrashReporter { private set; get; }

		/// <summary>
		/// Shared: Determines if the build is going to use cooked data, commandline: -cook, -cookonthefly
		/// </summary>	
		[Help("cook, -cookonthefly", "Determines if the build is going to use cooked data")]
		public bool Cook { private set; get; }

		/// <summary>
		/// Shared: Determines if the build is going to use cooked data, commandline: -cook, -cookonthefly
		/// </summary>	
		[Help("skipcook", "use a cooked build, but we assume the cooked data is up to date and where it belongs, implies -cook")]
		public bool SkipCook { private set; get; }

		/// <summary>
		/// Shared: In a cookonthefly build, used solely to pass information to the package step. This is necessary because you can't set cookonthefly and cook at the same time, and skipcook sets cook.
		/// </summary>	
		[Help("skipcookonthefly", "in a cookonthefly build, used solely to pass information to the package step")]
		public bool SkipCookOnTheFly { private set; get; }

		/// <summary>
		/// Shared: Determines if the intermediate folders will be wiped before building, commandline: -clean
		/// </summary>
		[Help("clean", "wipe intermediate folders before building")]
		public bool? Clean { private set; get; }

		/// <summary>
		/// Shared: Assumes no user is sitting at the console, so for example kills clients automatically, commandline: -Unattended
		/// </summary>
		[Help("unattended", "assumes no operator is present, always terminates without waiting for something.")]
		public bool Unattended { private set; get; }

		/// <summary>
        /// Shared: Sets platforms to build for non-dedicated servers. commandline: -TargetPlatform
		/// </summary>
		public List<TargetPlatformDescriptor> ClientTargetPlatforms = new List<TargetPlatformDescriptor>();

        /// <summary>
        /// Shared: Dictionary that maps client dependent platforms to "source" platforms that it should copy data from. commandline: -TargetPlatform=source.dependent
        /// </summary>
        public Dictionary<TargetPlatformDescriptor, TargetPlatformDescriptor> ClientDependentPlatformMap = new Dictionary<TargetPlatformDescriptor, TargetPlatformDescriptor>();

		/// <summary>
        /// Shared: Sets platforms to build for dedicated servers. commandline: -ServerTargetPlatform
		/// </summary>
		public List<TargetPlatformDescriptor> ServerTargetPlatforms = new List<TargetPlatformDescriptor>();

        /// <summary>
        /// Shared: Dictionary that maps server dependent platforms to "source" platforms that it should copy data from: -ServerTargetPlatform=source.dependent
        /// </summary>
        public Dictionary<TargetPlatformDescriptor, TargetPlatformDescriptor> ServerDependentPlatformMap = new Dictionary<TargetPlatformDescriptor, TargetPlatformDescriptor>();

		/// <summary>
		/// Shared: True if pak file should be generated.
		/// </summary>
		[Help("pak", "generate a pak file")]
		public bool Pak { private set; get; }

		/// <summary>
		/// 
		/// </summary>
		public bool UsePak(Platform PlatformToCheck)
		{
			return Pak || PlatformToCheck.RequiresPak(this) == Platform.PakType.Always;
		}

		private string SignPakInternal { get; set; }

		/// <summary>
		/// Shared: Encryption keys used for signing the pak file.
		/// </summary>
		[Help("signpak=keys", "sign the generated pak file with the specified key, i.e. -signpak=C:\\Encryption.keys. Also implies -signedpak.")]
		public string SignPak 
		{ 
			private set
			{
				if (string.IsNullOrEmpty(value) || value.StartsWith("0x", StringComparison.InvariantCultureIgnoreCase))
				{
					SignPakInternal = value;
				}
				else
				{
					SignPakInternal = Path.GetFullPath(value);
				}
			}
			get
			{
				return SignPakInternal;
			}
		}

		/// <summary>
		/// Shared: the game will use only signed content.
		/// </summary>
		[Help("signed", "the game should expect to use a signed pak file.")]
		public bool SignedPak { private set; get; }

		/// <summary>
		/// Shared: true if this build is staged, command line: -stage
		/// </summary>
		[Help("skippak", "use a pak file, but assume it is already built, implies pak")]
		public bool SkipPak { private set; get; }

		/// <summary>
		/// Shared: true if this build is staged, command line: -stage
		/// </summary>
		[Help("stage", "put this build in a stage directory")]
		public bool Stage { private set; get; }

		/// <summary>
		/// Shared: true if this build is staged, command line: -stage
		/// </summary>
		[Help("skipstage", "uses a stage directory, but assumes everything is already there, implies -stage")]
		public bool SkipStage { private set; get; }

		/// <summary>
		/// Shared: true if this build is using streaming install manifests, command line: -manifests
		/// </summary>
		[Help("manifests", "generate streaming install manifests when cooking data")]
		public bool Manifests { private set; get; }

        /// <summary>
        /// Shared: true if this build chunk install streaming install data, command line: -createchunkinstalldata
        /// </summary>
        [Help("createchunkinstall", "generate streaming install data from manifest when cooking data, requires -stage & -manifests")]
        public bool CreateChunkInstall { private set; get; }

		/// <summary>
		/// Shared: Directory to use for built chunk install data, command line: -chunkinstalldirectory=
		/// </summary>
		public string ChunkInstallDirectory { set; get; }

		/// <summary>
		/// Shared: Version string to use for built chunk install data, command line: -chunkinstallversion=
		/// </summary>
		public string ChunkInstallVersionString { set; get; }

		/// <summary>
        /// Shared: Release string to use for built chunk install data, command line: -chunkinstallrelease=
        /// </summary>
        public string ChunkInstallReleaseString { set; get; }

        /// <summary>
		/// Shared: Directory to copy the client to, command line: -stagingdirectory=
		/// </summary>	
		public string BaseStageDirectory
		{
			get
			{
                if( !String.IsNullOrEmpty(StageDirectoryParam ) )
                {
                    return Path.GetFullPath( StageDirectoryParam );
                }
                if ( HasDLCName )
                {
                     return Path.GetFullPath( CommandUtils.CombinePaths( DLCFile.Directory.FullName, "Saved", "StagedBuilds" ) );
                }
                // default return the project saved\stagedbuilds directory
                return Path.GetFullPath( CommandUtils.CombinePaths(Path.GetDirectoryName(RawProjectPath.FullName), "Saved", "StagedBuilds") );
			}
		}

		[Help("stagingdirectory=Path", "Directory to copy the builds to, i.e. -stagingdirectory=C:\\Stage")]
		public string StageDirectoryParam;
        
		[Help("ue4exe=ExecutableName", "Name of the UE4 Editor executable, i.e. -ue4exe=UE4Editor.exe")]
		public string UE4Exe;

		/// <summary>
		/// Shared: true if this build is archived, command line: -archive
		/// </summary>
		[Help("archive", "put this build in an archive directory")]
		public bool Archive { private set; get; }

		/// <summary>
		/// Shared: Directory to archive the client to, command line: -archivedirectory=
		/// </summary>	
		public string BaseArchiveDirectory
		{
			get
			{
                return Path.GetFullPath(String.IsNullOrEmpty(ArchiveDirectoryParam) ? CommandUtils.CombinePaths(Path.GetDirectoryName(RawProjectPath.FullName), "ArchivedBuilds") : ArchiveDirectoryParam);
			}
		}

		[Help("archivedirectory=Path", "Directory to archive the builds to, i.e. -archivedirectory=C:\\Archive")]
		public string ArchiveDirectoryParam;

		/// <summary>
		/// Whether the project should use non monolithic staging
		/// </summary>
		[Help("archivemetadata", "Archive extra metadata files in addition to the build (e.g. build.properties)")]
		public bool ArchiveMetaData;

		/// <summary>
		/// When archiving for Mac, set this to true to package it in a .app bundle instead of normal loose files
		/// </summary>
		[Help("createappbundle", "When archiving for Mac, set this to true to package it in a .app bundle instead of normal loose files")]
		public bool CreateAppBundle;

        /// <summary>
        /// Determines if Blueprint assets should be substituted with auto-generated code.
        /// </summary>
        public bool RunAssetNativization;

		/// <summary>
		/// Keeps track of any '-ini:type:[section]:value' arguments on the command line. These will override cached config settings for the current process, and can be passed along to other tools.
		/// </summary>
		public List<string> ConfigOverrideParams = new List<string>();

        #endregion

        #region Build

        /// <summary>
        /// Build: True if build step should be executed, command: -build
        /// </summary>
        [Help("build", "True if build step should be executed")]
		public bool Build { private set; get; }

		/// <summary>
		/// SkipBuildClient if true then don't build the client exe
		/// </summary>
		public bool SkipBuildClient { private set; get; }

		/// <summary>
		/// SkipBuildEditor if true then don't build the editor exe
		/// </summary>
		public bool SkipBuildEditor { private set; get; }

		/// <summary>
		/// Build: True if XGE should NOT be used for building.
		/// </summary>
		[Help("noxge", "True if XGE should NOT be used for building")]
		public bool NoXGE { private set; get; }

		/// <summary>
		/// Build: List of maps to cook.
		/// </summary>	
		private ParamList<string> EditorTargetsList = null;
		public ParamList<string> EditorTargets
		{
			set { EditorTargetsList = value; }
			get
			{
				if (EditorTargetsList == null)
				{
					// Lazy auto-initialization
					AutodetectSettings(false);
				}
				return EditorTargetsList;
			}
		}

		/// <summary>
		/// Build: List of maps to cook.
		/// </summary>	
		private ParamList<string> ProgramTargetsList = null;
		public ParamList<string> ProgramTargets
		{
			set { ProgramTargetsList = value; }
			get
			{
				if (ProgramTargetsList == null)
				{
					// Lazy auto-initialization
					AutodetectSettings(false);
				}
				return ProgramTargetsList;
			}
		}

		/// <summary>
		/// Build: List of client configurations
		/// </summary>
		public List<UnrealTargetConfiguration> ClientConfigsToBuild = new List<UnrealTargetConfiguration>() { UnrealTargetConfiguration.Development };

		///<summary>
		/// Build: List of Server configurations
		/// </summary>
		public List<UnrealTargetConfiguration> ServerConfigsToBuild = new List<UnrealTargetConfiguration>() { UnrealTargetConfiguration.Development };

		/// <summary>
		/// Build: List of client cooked build targets.
		/// </summary>
		private ParamList<string> ClientCookedTargetsList = null;
		public ParamList<string> ClientCookedTargets
		{
			set { ClientCookedTargetsList = value; }
			get
			{
				if (ClientCookedTargetsList == null)
				{
					// Lazy auto-initialization
					AutodetectSettings(false);
				}
				return ClientCookedTargetsList;
			}
		}

		/// <summary>
		/// Build: List of Server cooked build targets.
		/// </summary>
		private ParamList<string> ServerCookedTargetsList = null;
		public ParamList<string> ServerCookedTargets
		{
			set { ServerCookedTargetsList = value; }
			get
			{
				if (ServerCookedTargetsList == null)
				{
					// Lazy auto-initialization
					AutodetectSettings(false);
				}
				return ServerCookedTargetsList;
			}
		}

		#endregion

		#region Cook

		/// <summary>
		/// Cook: List of maps to cook.
		/// </summary>
		public ParamList<string> MapsToCook = new ParamList<string>();


		/// <summary>
		/// Cook: List of map inisections to cook (see allmaps)
		/// </summary>
		public ParamList<string> MapIniSectionsToCook = new ParamList<string>();
		

		/// <summary>
		/// Cook: List of directories to cook.
		/// </summary>
		public ParamList<string> DirectoriesToCook = new ParamList<string>();

        /// <summary>
        /// Cook: Internationalization preset to cook.
        /// </summary>
        public string InternationalizationPreset;

        /// <summary>
        /// Cook: Create a cooked release version.  Also, the version. e.g. 1.0
        /// </summary>
        public string CreateReleaseVersion;
        
		/// <summary>
		/// Cook: While cooking clean up packages as we go along rather then cleaning everything (and potentially having to reload some of it) when we run out of space
		/// </summary>
		[Help("CookPartialgc", "while cooking clean up packages as we are done with them rather then cleaning everything up when we run out of space")]
		public bool CookPartialGC { private set; get; }

		/// <summary>
		/// Stage: Did we cook in the editor instead of from UAT (cook in editor uses a different staging directory)
		/// </summary>
		[Help("CookInEditor", "Did we cook in the editor instead of in UAT")]
		public bool CookInEditor { private set; get; }

		/// <summary>
		/// Cook: Output directory for cooked data
		/// </summary>
		public string CookOutputDir;

		/// <summary>
		/// Cook: Base this cook of a already released version of the cooked data
		/// </summary>
		public string BasedOnReleaseVersion;

		/// <summary>
        /// Cook: Path to the root of the directory where we store released versions of the game for a given version
        /// </summary>
		public string BasedOnReleaseVersionBasePath;

		/// <summary>
		/// Cook: Path to the root of the directory to create a new released version of the game.
		/// </summary>
		public string CreateReleaseVersionBasePath;

        /// <summary>
        /// Are we generating a patch, generate a patch from a previously released version of the game (use CreateReleaseVersion to create a release). 
        /// this requires BasedOnReleaseVersion
        /// see also CreateReleaseVersion, BasedOnReleaseVersion
        /// </summary>
        public bool GeneratePatch;
		
		/// <summary>
		/// Are we generating a remaster, generate a patch from a previously released version of the game (use CreateReleaseVersion to create a release). 
		/// this requires BasedOnReleaseVersion
		/// see also CreateReleaseVersion, BasedOnReleaseVersion
		/// </summary>
		public bool GenerateRemaster;

		/// <summary>
        /// </summary>
        public bool AddPatchLevel;
        /// <summary>
        /// Are we staging the unmodified pak files from the base release
        public bool StageBaseReleasePaks;

        /// Name of dlc to cook and package (if this paramter is supplied cooks the dlc and packages it into the dlc directory)
		/// </summary>
        public FileReference DLCFile;

        /// <summary>
        /// Enable cooking of engine content when cooking dlc 
        ///  not included in original release but is referenced by current cook
        /// </summary>
        public bool DLCIncludeEngineContent;

        /// <summary>
        /// After cook completes diff the cooked content against another cooked content directory.
        ///  report all errors to the log
        /// </summary>
        public string DiffCookedContentPath;

        /// <summary>
        /// Cook: Additional cooker options to include on the cooker commandline
        /// </summary>
        public string AdditionalCookerOptions;

        /// <summary>
        /// Cook: List of cultures to cook.
        /// </summary>
        public ParamList<string> CulturesToCook;

        /// <summary>
        /// Compress packages during cook.
        /// </summary>
        public bool Compressed;

		/// <summary>
		/// Encrypt ini files which are packaged into the pak file.  Only valid when encryption keys and building pak file specified. 
		/// </summary>
		public bool EncryptIniFiles;

		/// <summary>
		/// Encrypt all files which are packaged into the pak file.  Only valid when encryption keys and building pak file specified. 
		/// </summary>
		public bool EncryptEverything;

        /// <summary>
		/// Encrypt all files which are packaged into the pak file.  Only valid when encryption keys and building pak file specified. 
		/// </summary>
        public bool EncryptPakIndex;

        /// <summary>
        /// put -debug on the editorexe commandline
        /// </summary>
        public bool UseDebugParamForEditorExe;

        /// <summary>
        /// Cook: Do not include a version number in the cooked content
        /// </summary>
        public bool UnversionedCookedContent = true;


		/// <summary>
		/// Cook: Uses the iterative cooking, command line: -iterativecooking or -iterate
		/// </summary>
		[Help( "iterativecooking", "Uses the iterative cooking, command line: -iterativecooking or -iterate" )]
		public bool IterativeCooking;

		/// <summary>
		/// Cook: Iterate from a build on the network
		/// </summary>
		[Help("Iteratively cook from a shared cooked build")]
		public bool IterateSharedCookedBuild;

		/// <summary>
		/// Build: Don't build the game instead use the prebuild exe (requires iterate shared cooked build
		/// </summary>
		[Help("Iteratively cook from a shared cooked build")]
		public bool IterateSharedBuildUsePrecompiledExe;

		/// <summary>
		/// Cook: Only cook maps (and referenced content) instead of cooking everything only affects -cookall flag
		/// </summary>
		[Help("CookMapsOnly", "Cook only maps this only affects usage of -cookall the flag")]
        public bool CookMapsOnly;

        /// <summary>
        /// Cook: Only cook maps (and referenced content) instead of cooking everything only affects cookall flag
        /// </summary>
        [Help("CookAll", "Cook all the things in the content directory for this project")]
        public bool CookAll;


        /// <summary>
        /// Cook: Skip cooking editor content 
        /// </summary>
		[Help("SkipCookingEditorContent", "Skips content under /Engine/Editor when cooking")]
        public bool SkipCookingEditorContent;

        /// <summary>
        /// Cook: number of additional cookers to spawn while cooking
        /// </summary>
        public int NumCookersToSpawn;

		/// <summary>
		/// Cook: Uses the iterative deploy, command line: -iterativedeploy or -iterate
		/// </summary>
		[Help("iterativecooking", "Uses the iterative cooking, command line: -iterativedeploy or -iterate")]
		public bool IterativeDeploy;

		[Help("FastCook", "Uses fast cook path if supported by target")]
		public bool FastCook;

		/// <summary>
		/// Cook: Ignores cook errors and continues with packaging etc.
		/// </summary>
		[Help("IgnoreCookErrors", "Ignores cook errors and continues with packaging etc")]
		public bool IgnoreCookErrors { private set; get; }

		#endregion

		#region Stage

		/// <summary>
		/// Stage: Commandline: -nodebuginfo
		/// </summary>
		[Help("nodebuginfo", "do not copy debug files to the stage")]
		public bool NoDebugInfo { private set; get; }

		/// <summary>
		/// Stage: Commandline: -mapfile
		/// </summary>
		[Help("MapFile", "generates a *.map file")]
		public bool MapFile { private set; get; }

		/// <summary>
		/// true if the staging directory is to be cleaned: -cleanstage (also true if -clean is specified)
		/// </summary>
		[Help("nocleanstage", "skip cleaning the stage directory")]
		public bool NoCleanStage { set { bNoCleanStage = value; } get { return SkipStage || bNoCleanStage; } }
		private bool bNoCleanStage;

		/// <summary>
		/// Stage: If non-empty, the contents will be put into the stage
		/// </summary>
		[Help("cmdline", "command line to put into the stage in UE4CommandLine.txt")]
		public string StageCommandline;

        /// <summary>
		/// Stage: If non-empty, the contents will be used for the bundle name
		/// </summary>
		[Help("bundlename", "string to use as the bundle name when deploying to mobile device")]
        public string BundleName;

        /// <summary>
        /// On Windows, adds an executable to the root of the staging directory which checks for prerequisites being 
		/// installed and launches the game with a path to the .uproject file.
		/// </summary>
        public bool NoBootstrapExe { get; set; }

		/// <summary>
		/// By default we don't code sign unless it is required or requested
		/// </summary>
		public bool bCodeSign = false;

		/// <summary>
		/// Provision to use
		/// </summary>
		public string Provision = null;

		/// <summary>
		/// Certificate to use
		/// </summary>
		public string Certificate = null;

		/// <summary>
		/// Team ID to use
		/// </summary>
		public string Team = null;

		/// <summary>
		/// true if provisioning is automatically managed
		/// </summary>
		public bool AutomaticSigning = false;

		/// <summary>
		/// TitleID to package
		/// </summary>
		public ParamList<string> TitleID = new ParamList<string>();

		/// <summary>
		/// If true, non-shipping binaries will be considered DebugUFS files and will appear on the debugfiles manifest
		/// </summary>
		public bool bTreatNonShippingBinariesAsDebugFiles = false;

		#endregion

		#region Run

		/// <summary>
		/// Run: True if the Run step should be executed, command: -run
		/// </summary>
		[Help("run", "run the game after it is built (including server, if -server)")]
		public bool Run { private set; get; }

		/// <summary>
		/// Run: The client runs with cooked data provided by cook on the fly server, command line: -cookonthefly
		/// </summary>
		[Help("cookonthefly", "run the client with cooked data provided by cook on the fly server")]
		public bool CookOnTheFly { private set; get; }

        /// <summary>
        /// Run: The client should run in streaming mode when connecting to cook on the fly server
        /// </summary>
        [Help("Cookontheflystreaming", "run the client in streaming cook on the fly mode (don't cache files locally instead force reget from server each file load)")]
        public bool CookOnTheFlyStreaming { private set; get; }



		/// <summary>
		/// Run: The client runs with cooked data provided by UnrealFileServer, command line: -fileserver
		/// </summary>
		[Help("fileserver", "run the client with cooked data provided by UnrealFileServer")]
		public bool FileServer { private set; get; }

		/// <summary>
		/// Run: The client connects to dedicated server to get data, command line: -dedicatedserver
		/// </summary>
		[Help("dedicatedserver", "build, cook and run both a client and a server (also -server)")]
		public bool DedicatedServer { private set; get; }

		/// <summary>
		/// Run: Uses a client target configuration, also implies -dedicatedserver, command line: -client
		/// </summary>
		[Help( "client", "build, cook and run a client and a server, uses client target configuration" )]
		public bool Client { private set; get; }

		/// <summary>
		/// Run: Whether the client should start or not, command line (to disable): -noclient
		/// </summary>
		[Help("noclient", "do not run the client, just run the server")]
		public bool NoClient { private set; get; }

		/// <summary>
		/// Run: Client should create its own log window, command line: -logwindow
		/// </summary>
		[Help("logwindow", "create a log window for the client")]
		public bool LogWindow { private set; get; }

		/// <summary>
		/// Run: Map to run the game with.
		/// </summary>
		[Help("map", "map to run the game with")]
		public string MapToRun;

		/// <summary>
		/// Run: Additional server map params.
		/// </summary>
		[Help("AdditionalServerMapParams", "Additional server map params, i.e ?param=value")]
		public string AdditionalServerMapParams;

		/// <summary>
		/// Run: The target device to run the game on.  Comes in the form platform@devicename.
		/// </summary>
		[Help("device", "Devices to run the game on")]
		public ParamList<string> Devices;

		/// <summary>
		/// Run: The target device to run the game on.  No platform prefix.
		/// </summary>
		[Help("device", "Device names without the platform prefix to run the game on")]
		public ParamList<string> DeviceNames;

		/// <summary>
		/// Run: the target device to run the server on
		/// </summary>
		[Help("serverdevice", "Device to run the server on")]
		public string ServerDevice;

		/// <summary>
		/// Run: The indicated server has already been started
		/// </summary>
		[Help("skipserver", "Skip starting the server")]
		public bool SkipServer;

		/// <summary>
		/// Run: The indicated server has already been started
		/// </summary>
		[Help("numclients=n", "Start extra clients, n should be 2 or more")]
		public int NumClients;

		/// <summary>
		/// Run: Additional command line arguments to pass to the program
		/// </summary>
		[Help("addcmdline", "Additional command line arguments for the program")]
		public string RunCommandline;

        /// <summary>
		/// Run: Additional command line arguments to pass to the server
		/// </summary>
		[Help("servercmdline", "Additional command line arguments for the program")]
		public string ServerCommandline;

        /// <summary>
		/// Run: Override command line arguments to pass to the client, if set it will not try to guess at IPs or settings
		/// </summary>
		[Help("clientcmdline", "Override command line arguments to pass to the client")]
        public string ClientCommandline;

        /// <summary>
        /// Run:adds -nullrhi to the client commandline
        /// </summary>
        [Help("nullrhi", "add -nullrhi to the client commandlines")]
        public bool NullRHI;

        /// <summary>
        /// Run:adds ?fake to the server URL
        /// </summary>
        [Help("fakeclient", "adds ?fake to the server URL")]
        public bool FakeClient;

        /// <summary>
        /// Run:adds ?fake to the server URL
        /// </summary>
        [Help("editortest", "rather than running a client, run the editor instead")]
        public bool EditorTest;

        /// <summary>
        /// Run:when running -editortest or a client, run all automation tests, not compatible with -server
        /// </summary>
        [Help("RunAutomationTests", "when running -editortest or a client, run all automation tests, not compatible with -server")]
        public bool RunAutomationTests;

        /// <summary>
        /// Run:when running -editortest or a client, run all automation tests, not compatible with -server
        /// </summary>
        [Help("RunAutomationTests", "when running -editortest or a client, run a specific automation tests, not compatible with -server")]
        public string RunAutomationTest;

        /// <summary>
        /// Run: Adds commands like debug crash, debug rendercrash, etc based on index
        /// </summary>
        [Help("Crash=index", "when running -editortest or a client, adds commands like debug crash, debug rendercrash, etc based on index")]
        public int CrashIndex;

        public ParamList<string> Port;

        /// <summary>
        /// Run: Linux username for unattended key genereation
        /// </summary>
        [Help("deviceuser", "Linux username for unattended key genereation")]
        public string DeviceUsername;

        /// <summary>
        /// Run: Linux password for unattended key genereation
        /// </summary>
        [Help("devicepass", "Linux password")]
        public string DevicePassword;

        /// <summary>
        /// Run: Server device IP address
        /// </summary>
        public string ServerDeviceAddress;

        #endregion

		#region Package

		[Help("package", "package the project for the target platform")]
		public bool Package { get; set; }

		[Help("distribution", "package for distribution the project")]
		public bool Distribution { get; set; }

		[Help("prereqs", "stage prerequisites along with the project")]
		public bool Prereqs { get; set; }

		[Help("applocaldir", "location of prerequisites for applocal deployment")]
		public string AppLocalDirectory { get; set; }

		[Help("Prebuilt", "this is a prebuilt cooked and packaged build")]
        public bool Prebuilt { get; private set; }

        [Help("RunTimeoutSeconds", "timeout to wait after we lunch the game")]
        public int RunTimeoutSeconds;

		[Help("SpecifiedArchitecture", "Determine a specific Minimum OS")]
		public string SpecifiedArchitecture;

		#endregion

		#region Deploy

		[Help("deploy", "deploy the project for the target platform")]
		public bool Deploy { get; set; }

		[Help("deploy", "Location to deploy to on the target platform")]
		public string DeployFolder { get; set; }

		#endregion

		#region Misc

		[Help("MapsToRebuildLightMaps", "List of maps that need light maps rebuilding")]
		public ParamList<string> MapsToRebuildLightMaps = new ParamList<string>();

		[Help("IgnoreLightMapErrors", "Whether Light Map errors should be treated as critical")]
		public bool IgnoreLightMapErrors { get; set; }

		#endregion

		#region Initialization

		private Dictionary<TargetType, SingleTargetProperties> DetectedTargets;
		private Dictionary<UnrealTargetPlatform, ConfigHierarchy> LoadedEngineConfigs;
		private Dictionary<UnrealTargetPlatform, ConfigHierarchy> LoadedGameConfigs;

		private void AutodetectSettings(bool bReset)
		{
			if (bReset)
			{
				EditorTargetsList = null;
				ClientCookedTargetsList = null;
				ServerCookedTargetsList = null;
				ProgramTargetsList = null;
				ProjectBinariesPath = null;
				ProjectGameExePath = null;
			}

            List<UnrealTargetPlatform> ClientTargetPlatformTypes = ClientTargetPlatforms.ConvertAll(x => x.Type).Distinct().ToList();
            var Properties = ProjectUtils.GetProjectProperties(RawProjectPath, ClientTargetPlatformTypes, RunAssetNativization);

			bIsCodeBasedProject = Properties.bIsCodeBasedProject;			
			DetectedTargets = Properties.Targets;
			LoadedEngineConfigs = Properties.EngineConfigs;
			LoadedGameConfigs = Properties.GameConfigs;

			var GameTarget = String.Empty;
			var EditorTarget = String.Empty;
			var ServerTarget = String.Empty;
			var ProgramTarget = String.Empty;
			var ProjectType = TargetType.Game;

			if (!bIsCodeBasedProject)
			{
				GameTarget = "UE4Game";
				EditorTarget = "UE4Editor";
				ServerTarget = "UE4Server";
			}
			else if (!CommandUtils.CmdEnv.HasCapabilityToCompile)
			{
				var ShortName = ProjectUtils.GetShortProjectName(RawProjectPath);
				GameTarget = ShortName;
				EditorTarget = ShortName + "Editor";
				ServerTarget = ShortName + "Server";
			}
			else if (!CommandUtils.IsNullOrEmpty(Properties.Targets))
			{
				SingleTargetProperties TargetData;

				var GameTargetType = TargetType.Game;
				
				if( Client )
				{
					if( HasClientTargetDetected )
					{
						GameTargetType = TargetType.Client;
					}
					else
					{
						throw new AutomationException( "Client target not found!" );
					}
				}

				var ValidGameTargetTypes = new TargetType[]
				{
					GameTargetType,
					TargetType.Program		
				};

				foreach (var ValidTarget in ValidGameTargetTypes)
				{
					if (DetectedTargets.TryGetValue(ValidTarget, out TargetData))
					{
						GameTarget = TargetData.TargetName;
						ProjectType = ValidTarget;
						break;
					}
				}

				if (DetectedTargets.TryGetValue(TargetType.Editor, out TargetData))
				{
					EditorTarget = TargetData.TargetName;
				}
				if (DetectedTargets.TryGetValue(TargetType.Server, out TargetData))
				{
					ServerTarget = TargetData.TargetName;
				}
				if (DetectedTargets.TryGetValue(TargetType.Program, out TargetData))
				{
					ProgramTarget = TargetData.TargetName;
				}
			}
			else if (!CommandUtils.IsNullOrEmpty(Properties.Programs))
			{
				SingleTargetProperties TargetData = Properties.Programs[0];

				ProjectType = TargetType.Program;
				ProgramTarget = TargetData.TargetName;
				GameTarget = TargetData.TargetName;
			}
			else if (!this.Build)
			{
				var ShortName = ProjectUtils.GetShortProjectName(RawProjectPath);
				GameTarget = ShortName;
				EditorTarget = ShortName + "Editor";
				ServerTarget = ShortName + "Server";
			}
			else
			{
				throw new AutomationException("{0} does not look like uproject file but no targets have been found!", RawProjectPath);
			}

			IsProgramTarget = ProjectType == TargetType.Program;

			if (String.IsNullOrEmpty(EditorTarget) && ProjectType != TargetType.Program && CommandUtils.IsNullOrEmpty(EditorTargetsList))
			{
				if (Properties.bWasGenerated)
				{
					EditorTarget = "UE4Editor";
				}
				else
				{
					throw new AutomationException("Editor target not found!");
				}
			}
			if (String.IsNullOrEmpty(GameTarget) && Run && !NoClient && (Cook || CookOnTheFly) && CommandUtils.IsNullOrEmpty(ClientCookedTargetsList))
			{
				throw new AutomationException("Game target not found. Game target is required with -cook or -cookonthefly");
			}

			if (EditorTargetsList == null)
			{
				if (!GlobalCommandLine.NoCompileEditor && (ProjectType != TargetType.Program) && !String.IsNullOrEmpty(EditorTarget))
				{
					EditorTargetsList = new ParamList<string>(EditorTarget);
				}
				else
				{
					EditorTargetsList = new ParamList<string>();
				}
			}

			if (ProgramTargetsList == null)
			{
				if (ProjectType == TargetType.Program)
				{
					ProgramTargetsList = new ParamList<string>(ProgramTarget);
				}
				else
				{
					ProgramTargetsList = new ParamList<string>();
				}
			}

            if (ClientCookedTargetsList == null && !NoClient && (Cook || CookOnTheFly || Prebuilt))
			{
                if (String.IsNullOrEmpty(GameTarget))
				{
                    throw new AutomationException("Game target not found. Game target is required with -cook or -cookonthefly");
                }
				else
				{
                    ClientCookedTargetsList = new ParamList<string>(GameTarget);
                }
			}
            else if (ClientCookedTargetsList == null)
            {
                ClientCookedTargetsList = new ParamList<string>();
            }

            if (ServerCookedTargetsList == null && DedicatedServer && (Cook || CookOnTheFly))
			{
				if (String.IsNullOrEmpty(ServerTarget))
				{
                    throw new AutomationException("Server target not found. Server target is required with -server and -cook or -cookonthefly");
				}
				ServerCookedTargetsList = new ParamList<string>(ServerTarget);
			}
			else if (ServerCookedTargetsList == null)
			{
				ServerCookedTargetsList = new ParamList<string>();
			}

			if (String.IsNullOrEmpty(ProjectBinariesPath) || String.IsNullOrEmpty(ProjectGameExePath))
			{
				if ( ClientTargetPlatforms.Count > 0 )
				{
					var ProjectClientBinariesPath = ProjectUtils.GetClientProjectBinariesRootPath(RawProjectPath, ProjectType, Properties.bIsCodeBasedProject);
					ProjectBinariesPath = ProjectUtils.GetProjectClientBinariesFolder(ProjectClientBinariesPath, ClientTargetPlatforms[0].Type).FullName;
					ProjectGameExePath = CommandUtils.CombinePaths(ProjectBinariesPath, GameTarget + Platform.GetExeExtension(ClientTargetPlatforms[0].Type));
				}
			}
		}

		#endregion

		#region Utilities

		public bool HasEditorTargets
		{
			get { return !CommandUtils.IsNullOrEmpty(EditorTargets); }
		}

		public bool HasCookedTargets
		{
			get { return !CommandUtils.IsNullOrEmpty(ClientCookedTargets) || !CommandUtils.IsNullOrEmpty(ServerCookedTargets); }
		}

		public bool HasServerCookedTargets
		{
			get { return !CommandUtils.IsNullOrEmpty(ServerCookedTargets); }
		}

		public bool HasClientCookedTargets
		{
			get { return !CommandUtils.IsNullOrEmpty(ClientCookedTargets); }
		}

		public bool HasProgramTargets
		{
			get { return !CommandUtils.IsNullOrEmpty(ProgramTargets); }
		}

		public bool HasMapsToCook
		{
			get { return !CommandUtils.IsNullOrEmpty(MapsToCook); }
		}

		public bool HasMapIniSectionsToCook
		{
			get { return !CommandUtils.IsNullOrEmpty(MapIniSectionsToCook); }
		}

		public bool HasDirectoriesToCook
		{
			get { return !CommandUtils.IsNullOrEmpty(DirectoriesToCook); }
		}

        public bool HasInternationalizationPreset
        {
            get { return !String.IsNullOrEmpty(InternationalizationPreset); }
        }

        public bool HasBasedOnReleaseVersion
        {
            get { return !String.IsNullOrEmpty(BasedOnReleaseVersion); }
        }

        public bool HasAdditionalCookerOptions
        {
            get { return !String.IsNullOrEmpty(AdditionalCookerOptions); }
        }

        public bool HasDLCName
        {
            get { return DLCFile != null; }
        }

        public bool HasDiffCookedContentPath
        {
            get { return !String.IsNullOrEmpty(DiffCookedContentPath); }
        }

        public bool HasCreateReleaseVersion
        {
            get { return !String.IsNullOrEmpty(CreateReleaseVersion); }
        }

        public bool HasCulturesToCook
        {
            get { return CulturesToCook != null; }
        }

		public bool HasGameTargetDetected
		{
			get { return ProjectTargets.ContainsKey(TargetType.Game); }
		}

		public bool HasClientTargetDetected
		{
			get { return ProjectTargets.ContainsKey( TargetType.Client ); }
		}

		public bool HasDedicatedServerAndClient
		{
			get { return Client && DedicatedServer; }
		}

		/// <summary>
		/// Project name (name of the uproject file without extension or directory name where the project is localed)
		/// </summary>
		public string ShortProjectName
		{
			get { return ProjectUtils.GetShortProjectName(RawProjectPath); }
		}

  		/// <summary>
		/// True if this project contains source code.
		/// </summary>	
		public bool IsCodeBasedProject
		{
			get
			{
				return bIsCodeBasedProject;
			}
		}
		private bool bIsCodeBasedProject;

		public FileReference CodeBasedUprojectPath
		{
            get { return IsCodeBasedProject ? RawProjectPath : null; }
		}
		/// <summary>
		/// True if this project is a program.
		/// </summary>
		public bool IsProgramTarget { get; private set; }

		/// <summary>
		/// Path where the project's game (or program) binaries are built.
		/// </summary>
		public string ProjectBinariesFolder
		{
			get
			{
				if (String.IsNullOrEmpty(ProjectBinariesPath))
				{
					AutodetectSettings(false);
				}
				return ProjectBinariesPath;
			}
		}
		private string ProjectBinariesPath;


		/// <summary>
		/// Get the path to the directory of the version we are basing a diff or a patch on.  
		/// </summary>				
		public String GetBasedOnReleaseVersionPath(DeploymentContext SC, bool bIsClientOnly)
		{
			String BasePath = BasedOnReleaseVersionBasePath;
			String Platform = SC.StageTargetPlatform.GetCookPlatform(SC.DedicatedServer, bIsClientOnly);
			if (String.IsNullOrEmpty(BasePath))
			{
                BasePath = CommandUtils.CombinePaths(SC.ProjectRoot.FullName, "Releases", BasedOnReleaseVersion, Platform);
			}
			else
			{
				BasePath = CommandUtils.CombinePaths(BasePath, BasedOnReleaseVersion, Platform);
			}

            /*if ( TitleID != null && TitleID.Count == 1 )
            {
                BasePath = CommandUtils.CombinePaths( BasePath, TitleID[0]);
            }*/

			return BasePath;
		}

		/// <summary>
		/// Get the path to the target directory for creating a new release version
		/// </summary>
		/// <param name="SC"></param>
		/// <returns></returns>
		public String GetCreateReleaseVersionPath(DeploymentContext SC, bool bIsClientOnly)
		{
			String BasePath = CreateReleaseVersionBasePath;
			String Platform = SC.StageTargetPlatform.GetCookPlatform(SC.DedicatedServer, bIsClientOnly);
			if (String.IsNullOrEmpty(BasePath))
			{
				BasePath = CommandUtils.CombinePaths(SC.ProjectRoot.FullName, "Releases", CreateReleaseVersion, Platform);
			}
			else
			{
				BasePath = CommandUtils.CombinePaths(BasePath, CreateReleaseVersion, Platform);
			}

            /*if (TitleID != null && TitleID.Count == 1)
            {
                BasePath = CommandUtils.CombinePaths(BasePath, TitleID[0]);
            }*/

			return BasePath;
		}

        /// <summary>
        /// True if we are generating a patch
        /// </summary>
        public bool IsGeneratingPatch
        {
            get { return GeneratePatch; }
        }

        /// <summary>
        /// True if we are generating a new patch pak tier
        /// </summary>
        public bool ShouldAddPatchLevel
        {
            get { return AddPatchLevel; }
        }

        /// <summary>
        /// True if we should stage pak files from the base release
        /// </summary>
        public bool ShouldStageBaseReleasePaks
        {
            get { return StageBaseReleasePaks; }
        }

		/// <summary>
		/// True if we are generating a patch
		/// </summary>
		public bool IsGeneratingRemaster
		{
			get { return GenerateRemaster; }
		}

		/// <summary>
		/// Filename of the target game exe (or program exe).
		/// </summary>
		public string ProjectGameExeFilename
		{
			get
			{
				if (String.IsNullOrEmpty(ProjectGameExePath))
				{
					AutodetectSettings(false);
				}
				return ProjectGameExePath;
			}
		}
		private string ProjectGameExePath;

		public List<Platform> ClientTargetPlatformInstances
		{
			get
			{
				List<Platform> ClientPlatformInstances = new List<Platform>();
				foreach ( var ClientPlatform in ClientTargetPlatforms )
				{
					ClientPlatformInstances.Add(Platform.Platforms[ClientPlatform]);
				}
				return ClientPlatformInstances;
			}
		}

        public TargetPlatformDescriptor GetCookedDataPlatformForClientTarget(TargetPlatformDescriptor TargetPlatformDesc)
        {
            if (ClientDependentPlatformMap.ContainsKey(TargetPlatformDesc))
            {
                return ClientDependentPlatformMap[TargetPlatformDesc];
            }
            return TargetPlatformDesc;
        }

		public List<Platform> ServerTargetPlatformInstances
		{
			get
			{
				List<Platform> ServerPlatformInstances = new List<Platform>();
				foreach (var ServerPlatform in ServerTargetPlatforms)
				{
					ServerPlatformInstances.Add(Platform.Platforms[ServerPlatform]);
				}
				return ServerPlatformInstances;
			}
		}

        public TargetPlatformDescriptor GetCookedDataPlatformForServerTarget(TargetPlatformDescriptor TargetPlatformType)
        {
            if (ServerDependentPlatformMap.ContainsKey(TargetPlatformType))
            {
                return ServerDependentPlatformMap[TargetPlatformType];
            }
            return TargetPlatformType;
        }

		/// <summary>
		/// All auto-detected targets for this project
		/// </summary>
		public Dictionary<TargetType, SingleTargetProperties> ProjectTargets
		{
			get
			{
				if (DetectedTargets == null)
				{
					AutodetectSettings(false);
				}
				return DetectedTargets;
			}
		}

		/// <summary>
		/// List of all Engine ini files for this project
		/// </summary>
		public Dictionary<UnrealTargetPlatform, ConfigHierarchy> EngineConfigs
		{
			get
			{
				if (LoadedEngineConfigs == null)
				{
					AutodetectSettings(false);
				}
				return LoadedEngineConfigs;
			}
		}

		/// <summary>
		/// List of all Game ini files for this project
		/// </summary>
		public Dictionary<UnrealTargetPlatform, ConfigHierarchy> GameConfigs
		{
			get
			{
				if (LoadedGameConfigs == null)
				{
					AutodetectSettings(false);
				}
				return LoadedGameConfigs;
			}
		}

		public void Validate()
		{
			if (RawProjectPath == null)
			{
				throw new AutomationException("RawProjectPath can't be empty.");
			}
            if (!RawProjectPath.HasExtension(".uproject"))
            {
                throw new AutomationException("RawProjectPath {0} must end with .uproject", RawProjectPath);
            }
            if (!CommandUtils.FileExists(RawProjectPath.FullName))
            {
                throw new AutomationException("RawProjectPath {0} file must exist", RawProjectPath);
            }

			if (FileServer && !Cook)
			{
				throw new AutomationException("Only cooked builds can use a fileserver be staged, use -cook");
			}

			if (Stage && !Cook && !CookOnTheFly && !IsProgramTarget)
			{
				throw new AutomationException("Only cooked builds or programs can be staged, use -cook or -cookonthefly.");
			}

			if (Manifests && !Cook && !Stage && !Pak)
			{
				throw new AutomationException("Only staged pakd and cooked builds can generate streaming install manifests");
			}

			if (Pak && !Stage)
			{
				throw new AutomationException("Only staged builds can be paked, use -stage or -skipstage.");
			}

            if (Deploy && !Stage)
            {
                throw new AutomationException("Only staged builds can be deployed, use -stage or -skipstage.");
            }

            if ((Pak || Stage || Cook || CookOnTheFly || FileServer || DedicatedServer) && EditorTest)
            {
                throw new AutomationException("None of pak, stage, cook, CookOnTheFly or DedicatedServer can be used with EditorTest");
            }

            if (DedicatedServer && RunAutomationTests)
            {
                throw new AutomationException("DedicatedServer cannot be used with RunAutomationTests");
            }

			if ((CookOnTheFly || FileServer) && DedicatedServer)
			{
				throw new AutomationException("Don't use either -cookonthefly or -fileserver with -server.");
			}

			if (NoClient && !DedicatedServer && !CookOnTheFly)
			{
				throw new AutomationException("-noclient can only be used with -server or -cookonthefly.");
			}

			if (Build && !HasCookedTargets && !HasEditorTargets && !HasProgramTargets)
			{
				throw new AutomationException("-build is specified but there are no targets to build.");
			}

			if (Pak && FileServer)
			{
				throw new AutomationException("Can't use -pak and -fileserver at the same time.");
			}

			if (Cook && CookOnTheFly)
			{
				throw new AutomationException("Can't use both -cook and -cookonthefly.");
			}

            if (!HasDLCName && DLCIncludeEngineContent)
            {
                throw new AutomationException("DLCIncludeEngineContent flag is only valid when cooking dlc.");
            }

            if ((IsGeneratingPatch || HasDLCName || ShouldAddPatchLevel) && !HasBasedOnReleaseVersion)
            {
                throw new AutomationException("Require based on release version to build patches or dlc");
            }

            if (ShouldAddPatchLevel && !IsGeneratingPatch)
            {
                throw new AutomationException("Creating a new patch tier requires patch generation");
            }

			if (ShouldStageBaseReleasePaks && !HasBasedOnReleaseVersion)
			{
				throw new AutomationException("Staging pak files from the base release requires a base release version");
			}

			if (HasCreateReleaseVersion && HasDLCName)
            {
                throw new AutomationException("Can't create a release version at the same time as creating dlc.");
            }

            if (HasBasedOnReleaseVersion && (IterativeCooking || IterativeDeploy || IterateSharedCookedBuild))
            {
                throw new AutomationException("Can't use iterative cooking / deploy on dlc or patching or creating a release");
            }

            /*if (Compressed && !Pak)
            {
                throw new AutomationException("-compressed can only be used with -pak");
            }*/

            if (CreateChunkInstall && (!(Manifests || HasDLCName) || !Stage))
            {
                throw new AutomationException("-createchunkinstall can only be used with -manifests & -stage"); 
            }

			if (CreateChunkInstall && String.IsNullOrEmpty(ChunkInstallDirectory))
			{
				throw new AutomationException("-createchunkinstall must specify the chunk install data directory with -chunkinstalldirectory=");
			}

			if (CreateChunkInstall && String.IsNullOrEmpty(ChunkInstallVersionString))
			{
				throw new AutomationException("-createchunkinstall must specify the chunk install data version string with -chunkinstallversion=");
			}
		}

		protected bool bLogged = false;
		public virtual void ValidateAndLog()
		{
			// Avoid spamming, log only once
			if (!bLogged)
			{
				// In alphabetical order.
				CommandUtils.LogLog("Project Params **************");

				CommandUtils.LogLog("AdditionalServerMapParams={0}", AdditionalServerMapParams);
				CommandUtils.LogLog("Archive={0}", Archive);
				CommandUtils.LogLog("ArchiveMetaData={0}", ArchiveMetaData);
				CommandUtils.LogLog("CreateAppBundle={0}", CreateAppBundle);
				CommandUtils.LogLog("BaseArchiveDirectory={0}", BaseArchiveDirectory);
				CommandUtils.LogLog("BaseStageDirectory={0}", BaseStageDirectory);
				CommandUtils.LogLog("Build={0}", Build);
				CommandUtils.LogLog("SkipBuildClient={0}", SkipBuildClient);
				CommandUtils.LogLog("SkipBuildEditor={0}", SkipBuildEditor);
				CommandUtils.LogLog("Cook={0}", Cook);
				CommandUtils.LogLog("Clean={0}", Clean);
				CommandUtils.LogLog("Client={0}", Client);
				CommandUtils.LogLog("ClientConfigsToBuild={0}", string.Join(",", ClientConfigsToBuild));
				CommandUtils.LogLog("ClientCookedTargets={0}", ClientCookedTargets.ToString());
				CommandUtils.LogLog("ClientTargetPlatform={0}", string.Join(",", ClientTargetPlatforms));
				CommandUtils.LogLog("Compressed={0}", Compressed);
				CommandUtils.LogLog("UseDebugParamForEditorExe={0}", UseDebugParamForEditorExe);
				CommandUtils.LogLog("CookOnTheFly={0}", CookOnTheFly);
				CommandUtils.LogLog("CookOnTheFlyStreaming={0}", CookOnTheFlyStreaming);
				CommandUtils.LogLog("UnversionedCookedContent={0}", UnversionedCookedContent);
				CommandUtils.LogLog("EncryptIniFiles={0}", EncryptIniFiles);
                CommandUtils.LogLog("EncryptPakIndex={0}", EncryptPakIndex);
                CommandUtils.LogLog("EncryptEverything={0}", EncryptEverything);
				CommandUtils.LogLog("SkipCookingEditorContent={0}", SkipCookingEditorContent);
                CommandUtils.LogLog("NumCookersToSpawn={0}", NumCookersToSpawn);
				CommandUtils.LogLog("GeneratePatch={0}", GeneratePatch);
				CommandUtils.LogLog("AddPatchLevel={0}", AddPatchLevel);
				CommandUtils.LogLog("StageBaseReleasePaks={0}", StageBaseReleasePaks);
				CommandUtils.LogLog("GenerateRemaster={0}", GenerateRemaster);
				CommandUtils.LogLog("CreateReleaseVersion={0}", CreateReleaseVersion);
                CommandUtils.LogLog("BasedOnReleaseVersion={0}", BasedOnReleaseVersion);
                CommandUtils.LogLog("DLCFile={0}", DLCFile);
                CommandUtils.LogLog("DLCIncludeEngineContent={0}", DLCIncludeEngineContent);
                CommandUtils.LogLog("DiffCookedContentPath={0}", DiffCookedContentPath);
                CommandUtils.LogLog("AdditionalCookerOptions={0}", AdditionalCookerOptions);
				CommandUtils.LogLog("DedicatedServer={0}", DedicatedServer);
				CommandUtils.LogLog("DirectoriesToCook={0}", DirectoriesToCook.ToString());
                CommandUtils.LogLog("CulturesToCook={0}", CommandUtils.IsNullOrEmpty(CulturesToCook) ? "<Not Specified> (Use Defaults)" : CulturesToCook.ToString());
				CommandUtils.LogLog("EditorTargets={0}", EditorTargets.ToString());
				CommandUtils.LogLog("Foreign={0}", Foreign);
				CommandUtils.LogLog("IsCodeBasedProject={0}", IsCodeBasedProject.ToString());
				CommandUtils.LogLog("IsProgramTarget={0}", IsProgramTarget.ToString());
				CommandUtils.LogLog("IterativeCooking={0}", IterativeCooking);
				CommandUtils.LogLog("IterateSharedCookedBuild={0}", IterateSharedCookedBuild);
				CommandUtils.LogLog("IterateSharedBuildUsePrecompiledExe={0}", IterateSharedBuildUsePrecompiledExe);
				CommandUtils.LogLog("CookAll={0}", CookAll);
				CommandUtils.LogLog("CookPartialGC={0}", CookPartialGC);
				CommandUtils.LogLog("CookInEditor={0}", CookInEditor);
				CommandUtils.LogLog("CookMapsOnly={0}", CookMapsOnly);
                CommandUtils.LogLog("Deploy={0}", Deploy);
				CommandUtils.LogLog("IterativeDeploy={0}", IterativeDeploy);
				CommandUtils.LogLog("FastCook={0}", FastCook);
				CommandUtils.LogLog("LogWindow={0}", LogWindow);
				CommandUtils.LogLog("Manifests={0}", Manifests);
				CommandUtils.LogLog("MapToRun={0}", MapToRun);
				CommandUtils.LogLog("NoClient={0}", NoClient);
				CommandUtils.LogLog("NumClients={0}", NumClients);                
				CommandUtils.LogLog("NoDebugInfo={0}", NoDebugInfo);
				CommandUtils.LogLog("MapFile={0}", MapFile);
				CommandUtils.LogLog("NoCleanStage={0}", NoCleanStage);
				CommandUtils.LogLog("NoXGE={0}", NoXGE);
				CommandUtils.LogLog("MapsToCook={0}", MapsToCook.ToString());
				CommandUtils.LogLog("MapIniSectionsToCook={0}", MapIniSectionsToCook.ToString());
				CommandUtils.LogLog("Pak={0}", Pak);
				CommandUtils.LogLog("Package={0}", Package);
				CommandUtils.LogLog("NullRHI={0}", NullRHI);
				CommandUtils.LogLog("FakeClient={0}", FakeClient);
                CommandUtils.LogLog("EditorTest={0}", EditorTest);
                CommandUtils.LogLog("RunAutomationTests={0}", RunAutomationTests); 
                CommandUtils.LogLog("RunAutomationTest={0}", RunAutomationTest);
                CommandUtils.LogLog("RunTimeoutSeconds={0}", RunTimeoutSeconds);
                CommandUtils.LogLog("CrashIndex={0}", CrashIndex);
				CommandUtils.LogLog("ProgramTargets={0}", ProgramTargets.ToString());
                CommandUtils.LogLog("ProjectBinariesFolder={0}", ProjectBinariesFolder);
				CommandUtils.LogLog("ProjectBinariesPath={0}", ProjectBinariesPath);
				CommandUtils.LogLog("ProjectGameExeFilename={0}", ProjectGameExeFilename);
				CommandUtils.LogLog("ProjectGameExePath={0}", ProjectGameExePath);
				CommandUtils.LogLog("Distribution={0}", Distribution);
                CommandUtils.LogLog("Prebuilt={0}", Prebuilt);
				CommandUtils.LogLog("Prereqs={0}", Prereqs);
				CommandUtils.LogLog("AppLocalDirectory={0}", AppLocalDirectory);
				CommandUtils.LogLog("NoBootstrapExe={0}", NoBootstrapExe);
				CommandUtils.LogLog("RawProjectPath={0}", RawProjectPath);
				CommandUtils.LogLog("Run={0}", Run);
				CommandUtils.LogLog("ServerConfigsToBuild={0}", string.Join(",", ServerConfigsToBuild));
				CommandUtils.LogLog("ServerCookedTargets={0}", ServerCookedTargets.ToString());
				CommandUtils.LogLog("ServerTargetPlatform={0}", string.Join(",", ServerTargetPlatforms));
				CommandUtils.LogLog("ShortProjectName={0}", ShortProjectName.ToString());
				CommandUtils.LogLog("SignedPak={0}", SignedPak);
				CommandUtils.LogLog("SignPak={0}", SignPak);				
				CommandUtils.LogLog("SkipCook={0}", SkipCook);
				CommandUtils.LogLog("SkipCookOnTheFly={0}", SkipCookOnTheFly);
				CommandUtils.LogLog("SkipPak={0}", SkipPak);
				CommandUtils.LogLog("SkipStage={0}", SkipStage);
				CommandUtils.LogLog("Stage={0}", Stage);
				CommandUtils.LogLog("bTreatNonShippingBinariesAsDebugFiles={0}", bTreatNonShippingBinariesAsDebugFiles);
                CommandUtils.LogLog("NativizeAssets={0}", RunAssetNativization);
				CommandUtils.LogLog("Project Params **************");
			}
			bLogged = true;

			Validate();
		}

		#endregion
	}
}
