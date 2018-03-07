// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Mac-specific target settings
	/// </summary>
	public class MacTargetRules
	{
		/// <summary>
		/// Whether to generate dSYM files
		/// Lists Architectures that you want to build
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bGeneratedSYMFile")]
		public bool bGenerateDsymFile = true;

		/// <summary>
		/// Enables address sanitizer (ASan)
		/// </summary>
		[CommandLine("-EnableASan")]
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bEnableAddressSanitizer")]
		public bool bEnableAddressSanitizer = false;

		/// <summary>
		/// Enables thread sanitizer (TSan)
		/// </summary>
		[CommandLine("-EnableTSan")]
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bEnableThreadSanitizer")]
		public bool bEnableThreadSanitizer = false;

		/// <summary>
		/// Enables undefined behavior sanitizer (UBSan)
		/// </summary>
		[CommandLine("-EnableUBSan")]
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bEnableUndefinedBehaviorSanitizer")]
		public bool bEnableUndefinedBehaviorSanitizer = false;

		/// <summary>
		/// Constructor
		/// </summary>
		public MacTargetRules()
		{
			XmlConfig.ApplyTo(this);
		}
	}

	/// <summary>
	/// Read-only wrapper for Mac-specific target settings
	/// </summary>
	public class ReadOnlyMacTargetRules
	{
		/// <summary>
		/// The private mutable settings object
		/// </summary>
		private MacTargetRules Inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The settings object to wrap</param>
		public ReadOnlyMacTargetRules(MacTargetRules Inner)
		{
			this.Inner = Inner;
		}

		/// <summary>
		/// Accessors for fields on the inner TargetRules instance
		/// </summary>
		#region Read-only accessor properties 
		#if !__MonoCS__
		#pragma warning disable CS1591
		#endif

		public bool bGenerateDsymFile
		{
			get { return Inner.bGenerateDsymFile; }
		}

		public bool bEnableAddressSanitizer
		{
			get { return Inner.bEnableAddressSanitizer; }
		}

		public bool bEnableThreadSanitizer
		{
			get { return Inner.bEnableThreadSanitizer; }
		}

		public bool bEnableUndefinedBehaviorSanitizer
		{
			get { return Inner.bEnableUndefinedBehaviorSanitizer; }
		}

		#if !__MonoCS__
		#pragma warning restore CS1591
		#endif
		#endregion
	}

	class MacPlatform : UEBuildPlatform
	{
		MacPlatformSDK SDK;

		public MacPlatform(MacPlatformSDK InSDK) : base(UnrealTargetPlatform.Mac, CppPlatform.Mac)
		{
			SDK = InSDK;
		}

		public override SDKStatus HasRequiredSDKsInstalled()
		{
			return SDK.HasRequiredSDKsInstalled();
		}

		public override bool CanUseXGE()
		{
			return false;
		}

		public override bool CanUseDistcc()
		{
			return true;
		}

		public override void ResetTarget(TargetRules Target)
		{
			Target.bCompileSimplygon = false;
            Target.bCompileSimplygonSSF = false;
		}

		public override void ValidateTarget(TargetRules Target)
		{
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				// @todo: Temporarily disable precompiled header files when building remotely due to errors
				Target.bUsePCHFiles = false;
			}

			// Needs OS X 10.11 for Metal. The remote toolchain has not been initialized yet, so just assume it's a recent SDK.
			if ((BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac || MacToolChain.Settings.MacOSSDKVersionFloat >= 10.11f) && Target.bCompileAgainstEngine)
			{
				Target.GlobalDefinitions.Add("HAS_METAL=1");
				Target.ExtraModuleNames.Add("MetalRHI");
			}
			else
			{
				Target.GlobalDefinitions.Add("HAS_METAL=0");
			}

			// Force using the ANSI allocator if ASan is enabled
			if(Target.MacPlatform.bEnableAddressSanitizer)
			{
				Target.GlobalDefinitions.Add("FORCE_ANSI_ALLOCATOR=1");
			}

			if (ProjectFileGenerator.bGenerateProjectFiles)
			{
				// When generating project files we need intellisense generator to include info from all modules, including editor-only third party libs
				Target.bCompileLeanAndMeanUE = false;
			}

			Target.bCompileNvCloth = true;

			Target.bUsePDBFiles = !Target.bDisableDebugInfo && Target.Configuration != UnrealTargetConfiguration.Debug && Platform == UnrealTargetPlatform.Mac && Target.MacPlatform.bGenerateDsymFile;

			// we always deploy - the build machines need to be able to copy the files back, which needs the full bundle
			Target.bDeployAfterCompile = true;

			Target.bCheckSystemHeadersForModification = BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac;
		}

		/// <summary>
		/// Determines if the given name is a build product for a target.
		/// </summary>
		/// <param name="FileName">The name to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UE4Editor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <returns>True if the string matches the name of a build product, false otherwise</returns>
		public override bool IsBuildProduct(string FileName, string[] NamePrefixes, string[] NameSuffixes)
		{
			return IsBuildProductName(FileName, NamePrefixes, NameSuffixes, "")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".dsym")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".dylib")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".a")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".app");
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binrary type being built</param>
		/// <returns>string    The binary extenstion (ie 'exe' or 'dll')</returns>
		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return ".dylib";
				case UEBuildBinaryType.Executable:
					return "";
				case UEBuildBinaryType.StaticLibrary:
					return ".a";
				case UEBuildBinaryType.Object:
					return ".o";
				case UEBuildBinaryType.PrecompiledHeader:
					return ".gch";
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		/// <summary>
		/// Get the extension to use for debug info for the given binary type
		/// </summary>
		/// <param name="Target">Rules for the target being built</param>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The debug info extension (i.e. 'pdb')</returns>
		public override string GetDebugInfoExtension(ReadOnlyTargetRules Target, UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return Target.bUsePDBFiles ? ".dSYM" : "";
				case UEBuildBinaryType.Executable:
					return Target.bUsePDBFiles ? ".dSYM" : "";
				case UEBuildBinaryType.StaticLibrary:
					return "";
				case UEBuildBinaryType.Object:
					return "";
				case UEBuildBinaryType.PrecompiledHeader:
					return "";
				default:
					return "";
			}
		}

		/// <summary>
		/// Modify the rules for a newly created module, where the target is a different host platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForOtherPlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
		}

		/// <summary>
		/// Don't use absolute paths in unity files; we may be remote compiling.
		/// </summary>
		/// <returns>bool true if it is required, false if not</returns>
		public override bool UseAbsolutePathsInUnityFiles()
		{
			return false;
		}


		/// <summary>
		/// Return whether we wish to have this platform's binaries in our builds
		/// </summary>
		public override bool IsBuildRequired()
		{
			return false;
		}

		/// <summary>
		/// Return whether we wish to have this platform's binaries in our CIS tests
		/// </summary>
		public override bool IsCISRequired()
		{
			return false;
		}

		/// <summary>
		/// Converts the passed in path from UBT host to compiler native format.
		/// </summary>
		/// <param name="OriginalPath">The path to convert</param>
		/// <returns>The path in native format for the toolchain</returns>
		public override string ConvertPath(string OriginalPath)
		{
			return MacToolChain.ConvertPath(OriginalPath);
		}

		public override void PreBuildSync()
		{
			MacToolChain.PreBuildSync();
		}

		public override void PostBuildSync(UEBuildTarget Target)
		{
			MacToolChain.PostBuildSync(Target);
		}

		public override void PostCodeGeneration(UHTManifest Manifest)
		{
			MacToolChain.PostCodeGeneration(Manifest);
		}

		public override DirectoryReference GetBundleDirectory(ReadOnlyTargetRules Rules, List<FileReference> OutputFiles)
		{
			if(Rules.bIsBuildingConsoleApplication)
			{
				return null;
			}
			else
			{
				return new DirectoryReference(OutputFiles[0].FullName + ".app");
			}
		}

		/// <summary>
		/// Modify the rules for a newly created module, in a target that's being built for this platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForActivePlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			bool bBuildShaderFormats = Target.bForceBuildShaderFormats;

			if (!Target.bBuildRequiresCookedData)
			{
				if (ModuleName == "TargetPlatform")
				{
					bBuildShaderFormats = true;
				}
			}

			// allow standalone tools to use target platform modules, without needing Engine
			if (ModuleName == "TargetPlatform")
			{
				if (Target.bForceBuildTargetPlatforms)
				{
					Rules.DynamicallyLoadedModuleNames.Add("MacTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("MacNoEditorTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("MacClientTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("MacServerTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("AllDesktopTargetPlatform");
				}

				if (bBuildShaderFormats)
				{
					// Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatD3D");
					Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatOpenGL");
					Rules.DynamicallyLoadedModuleNames.Add("MetalShaderFormat");

					Rules.DynamicallyLoadedModuleNames.Remove("VulkanRHI");
					Rules.DynamicallyLoadedModuleNames.Add("VulkanShaderFormat");
				}
			}
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="Target">Settings for the target being compiled</param>
		/// <param name="CompileEnvironment">The compile environment for this target</param>
		/// <param name="LinkEnvironment">The link environment for this target</param>
		public override void SetUpEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{
			CompileEnvironment.Definitions.Add("PLATFORM_MAC=1");
			CompileEnvironment.Definitions.Add("PLATFORM_APPLE=1");

			CompileEnvironment.Definitions.Add("WITH_TTS=0");
			CompileEnvironment.Definitions.Add("WITH_SPEECH_RECOGNITION=0");
			if (!CompileEnvironment.Definitions.Contains("WITH_DATABASE_SUPPORT=0") && !CompileEnvironment.Definitions.Contains("WITH_DATABASE_SUPPORT=1"))
			{
				CompileEnvironment.Definitions.Add("WITH_DATABASE_SUPPORT=0");
			}
		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <returns>true if debug info should be generated, false if not</returns>
		public override bool ShouldCreateDebugInfo(ReadOnlyTargetRules Target)
		{
			return true;
		}

		/// <summary>
		/// Creates a toolchain instance for the given platform.
		/// </summary>
		/// <param name="CppPlatform">The platform to create a toolchain for</param>
		/// <param name="Target">The target being built</param>
		/// <returns>New toolchain instance.</returns>
		public override UEToolChain CreateToolChain(CppPlatform CppPlatform, ReadOnlyTargetRules Target)
		{
			MacToolChainOptions Options = MacToolChainOptions.None;
			if(Target.MacPlatform.bEnableAddressSanitizer)
			{
				Options |= MacToolChainOptions.EnableAddressSanitizer;
			}
			if(Target.MacPlatform.bEnableThreadSanitizer)
			{
				Options |= MacToolChainOptions.EnableThreadSanitizer;
			}
			if(Target.MacPlatform.bEnableUndefinedBehaviorSanitizer)
			{
				Options |= MacToolChainOptions.EnableUndefinedBehaviorSanitizer;
			}
			return new MacToolChain(Target.ProjectFile, Options);
		}

		/// <summary>
		/// Deploys the given target
		/// </summary>
		/// <param name="Target">Information about the target being deployed</param>
		public override void Deploy(UEBuildDeployTarget Target)
		{
			new UEDeployMac().PrepTargetForDeployment(Target);
		}
	}

	class MacPlatformSDK : UEBuildPlatformSDK
	{
		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			return SDKStatus.Valid;
		}
	}

	class MacPlatformFactory : UEBuildPlatformFactory
	{
		protected override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.Mac; }
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		protected override void RegisterBuildPlatforms(SDKOutputLevel OutputLevel)
		{
			MacPlatformSDK SDK = new MacPlatformSDK();
			SDK.ManageAndValidateSDK(OutputLevel);

			// Register this build platform for Mac
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Mac.ToString());
			UEBuildPlatform.RegisterBuildPlatform(new MacPlatform(SDK));
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Mac, UnrealPlatformGroup.Unix);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Mac, UnrealPlatformGroup.Apple);
		}
	}
}
