// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/** Architecture as stored in the ini. */
	enum LinuxArchitecture
	{
		/** x86_64, most commonly used architecture.*/
		X86_64UnknownLinuxGnu,

		/** A.k.a. AArch32, ARM 32-bit with hardware floats */
		ArmUnknownLinuxGnueabihf,

		/** AArch64, ARM 64-bit */
		AArch64UnknownLinuxGnueabi,

		/** i686, Intel 32-bit */
		I686UnknownLinuxGnu
	}

	class LinuxPlatform : UEBuildPlatform
	{
		/// <summary>
		/// Linux architecture (compiler target triplet)
		/// </summary>
		// FIXME: for now switching between architectures is hard-coded
		public const string DefaultArchitecture = "x86_64-unknown-linux-gnu";
		//public const string DefaultArchitecture = "arm-unknown-linux-gnueabihf";
		//public const string DefaultArchitecture = "aarch64-unknown-linux-gnueabi";

		LinuxPlatformSDK SDK;

		/// <summary>
		/// Constructor
		/// </summary>
		public LinuxPlatform(LinuxPlatformSDK InSDK) : base(UnrealTargetPlatform.Linux, CppPlatform.Linux)
		{
			SDK = InSDK;
		}

		/// <summary>
		/// Whether the required external SDKs are installed for this platform. Could be either a manual install or an AutoSDK.
		/// </summary>
		public override SDKStatus HasRequiredSDKsInstalled()
		{
			return SDK.HasRequiredSDKsInstalled();
		}

		/// <summary>
		/// Find the default architecture for the given project
		/// </summary>
		public override string GetDefaultArchitecture(FileReference ProjectFile)
		{
			string ActiveArchitecture = DefaultArchitecture;

			// read settings from the config
			string EngineIniPath = ProjectFile != null ? ProjectFile.Directory.FullName : null;
			if (String.IsNullOrEmpty(EngineIniPath))
			{
				// If the project file hasn't been specified, try to get the path from -remoteini command line param
				EngineIniPath = UnrealBuildTool.GetRemoteIniPath();
			}
			DirectoryReference EngineIniDir = !String.IsNullOrEmpty(EngineIniPath) ? new DirectoryReference(EngineIniPath) : null;

			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, EngineIniDir, UnrealTargetPlatform.Linux);

			string LinuxArchitectureString;
			if (Ini.GetString("/Script/LinuxTargetPlatform.LinuxTargetSettings", "TargetArchitecture", out LinuxArchitectureString))
			{
				LinuxArchitecture Architecture;
				if (Enum.TryParse(LinuxArchitectureString, out Architecture))
				{
					switch (Architecture)
					{
						default:
							System.Console.WriteLine("Architecture enum value {0} does not map to a valid triplet.", Architecture);
							break;

						case LinuxArchitecture.X86_64UnknownLinuxGnu:
							ActiveArchitecture = "x86_64-unknown-linux-gnu";
							break;

						case LinuxArchitecture.ArmUnknownLinuxGnueabihf:
							ActiveArchitecture = "arm-unknown-linux-gnueabihf";
							break;

						case LinuxArchitecture.AArch64UnknownLinuxGnueabi:
							ActiveArchitecture = "aarch64-unknown-linux-gnueabi";
							break;

						case LinuxArchitecture.I686UnknownLinuxGnu:
							ActiveArchitecture = "i686-unknown-linux-gnu";
							break;
					}
				}
			}

			return ActiveArchitecture;
		}

		/// <summary>
		/// Get name for architecture-specific directories (can be shorter than architecture name itself)
		/// </summary>
		public override string GetFolderNameForArchitecture(string Architecture)
		{
			// shorten the string (heuristically)
			uint Sum = 0;
			int Len = Architecture.Length;
			for (int Index = 0; Index < Len; ++Index)
			{
				Sum += (uint)(Architecture[Index]);
				Sum <<= 1;	// allowed to overflow
			}
			return Sum.ToString("X");
		}

		public override void ResetTarget(TargetRules Target)
		{
			ValidateTarget(Target);
		}

		public override void ValidateTarget(TargetRules Target)
		{
			Target.bCompileSimplygon = false;
            Target.bCompileSimplygonSSF = false;
			// depends on arch, APEX cannot be as of November'16 compiled for AArch32/64
			Target.bCompileAPEX = Target.Architecture.StartsWith("x86_64");
			Target.bCompileNvCloth = Target.Architecture.StartsWith("x86_64");

			// Disable Simplygon support if compiling against the NULL RHI.
			if (Target.GlobalDefinitions.Contains("USE_NULL_RHI=1"))
			{
				Target.bCompileSimplygon = false;
				Target.bCompileSimplygonSSF = false;
				Target.bCompileCEF3 = false;
			}

			// check if OS update invalidated our build
			Target.bCheckSystemHeadersForModification = (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux);

			// At the moment ICU has not been compiled for AArch64 and i686. Also, localization isn't needed on servers by default, and ICU is pretty heavy
			if (Target.Architecture.StartsWith("aarch64") || Target.Architecture.StartsWith("i686") || Target.Type == TargetType.Server)
			{
				Target.bCompileICU = false;
			}

			if (ProjectFileGenerator.bGenerateProjectFiles)
			{
				// When generating project files we need intellisense generator to include info from all modules,
				// including editor-only third party libs
				Target.bCompileLeanAndMeanUE = false;
			}
		}

		/// <summary>
		/// Allows the platform to override whether the architecture name should be appended to the name of binaries.
		/// </summary>
		/// <returns>True if the architecture name should be appended to the binary</returns>
		public override bool RequiresArchitectureSuffix()
		{
			// Linux ignores architecture-specific names, although it might be worth it to prepend architecture
			return false;
		}

		public override bool CanUseXGE()
		{
			// XGE crashes with very high probability when v8_clang-3.9.0-centos cross-toolchain is used on Windows. Please make sure this is resolved before re-enabling it.
			return BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux;
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
			if (FileName.StartsWith("lib"))
			{
				return IsBuildProductName(FileName, 3, FileName.Length - 3, NamePrefixes, NameSuffixes, ".a")
					|| IsBuildProductName(FileName, 3, FileName.Length - 3, NamePrefixes, NameSuffixes, ".so");
			}
			else
			{
				return IsBuildProductName(FileName, NamePrefixes, NameSuffixes, "")
					|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".so")
					|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".a");
			}
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The binary extension (i.e. 'exe' or 'dll')</returns>
		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return ".so";
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
		/// <param name="InTarget">Rules for the target being built</param>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The debug info extension (i.e. 'pdb')</returns>
		public override string GetDebugInfoExtension(ReadOnlyTargetRules InTarget, UEBuildBinaryType InBinaryType)
		{
			return "";
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
			if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64))
			{
				if (!Target.bBuildRequiresCookedData)
				{
					if (ModuleName == "Engine")
					{
						if (Target.bBuildDeveloperTools)
						{
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("LinuxTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("LinuxNoEditorTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("LinuxClientTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("LinuxServerTargetPlatform");
						}
					}
				}

				// allow standalone tools to use targetplatform modules, without needing Engine
				if (Target.bForceBuildTargetPlatforms && ModuleName == "TargetPlatform")
				{
					Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("LinuxTargetPlatform");
					Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("LinuxNoEditorTargetPlatform");
					Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("LinuxClientTargetPlatform");
					Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("LinuxServerTargetPlatform");
				}
			}
		}

		/// <summary>
		/// Converts the passed in path from UBT host to compiler native format.
		/// </summary>
		public override string ConvertPath(string OriginalPath)
		{
			return LinuxToolChain.ConvertPath(OriginalPath);
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
					Rules.DynamicallyLoadedModuleNames.Add("LinuxTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("LinuxNoEditorTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("LinuxClientTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("LinuxServerTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("AllDesktopTargetPlatform");
				}

				if (bBuildShaderFormats)
				{
					Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatOpenGL");
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
			CompileEnvironment.Definitions.Add("PLATFORM_LINUX=1");
			CompileEnvironment.Definitions.Add("LINUX=1");

			CompileEnvironment.Definitions.Add("PLATFORM_SUPPORTS_JEMALLOC=1");	// this define does not set jemalloc as default, just indicates its support
			CompileEnvironment.Definitions.Add("WITH_DATABASE_SUPPORT=0");		//@todo linux: valid?

			// During the native builds, check the system includes as well (check toolchain when cross-compiling?)
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
			{
				CompileEnvironment.IncludePaths.SystemIncludePaths.Add("/usr/include");
			}

			if (Target.Architecture.StartsWith("arm"))	// AArch64 doesn't strictly need that - aligned access improves perf, but this will be likely offset by memcpys we're doing to guarantee it.
			{
				CompileEnvironment.Definitions.Add("REQUIRES_ALIGNED_INT_ACCESS");
			}

			// link with Linux libraries.
			LinkEnvironment.AdditionalLibraries.Add("pthread");
		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <returns>bool    true if debug info should be generated, false if not</returns>
		public override bool ShouldCreateDebugInfo(ReadOnlyTargetRules Target)
		{
			switch (Target.Configuration)
			{
				case UnrealTargetConfiguration.Development:
				case UnrealTargetConfiguration.Shipping:
				case UnrealTargetConfiguration.Test:
				case UnrealTargetConfiguration.Debug:
				default:
					return true;
			};
		}

		/// <summary>
		/// Creates a toolchain instance for the given platform.
		/// </summary>
		/// <param name="CppPlatform">The platform to create a toolchain for</param>
		/// <param name="Target">The target being built</param>
		/// <returns>New toolchain instance.</returns>
		public override UEToolChain CreateToolChain(CppPlatform CppPlatform, ReadOnlyTargetRules Target)
		{
			return new LinuxToolChain(Target.Architecture);
		}

		/// <summary>
		/// Deploys the given target
		/// </summary>
		/// <param name="Target">Information about the target being deployed</param>
		public override void Deploy(UEBuildDeployTarget Target)
		{
		}
	}

	class LinuxPlatformSDK : UEBuildPlatformSDK
	{
		/// <summary>
		/// This is the SDK version we support
		/// </summary>
		static string ExpectedSDKVersion = "v10_clang-5.0.0-centos7";	// now unified for all the architectures

		/// <summary>
		/// Platform name (embeds architecture for now)
		/// </summary>
		static private string TargetPlatformName = "Linux_x64";

		/// <summary>
		/// Whether platform supports switching SDKs during runtime
		/// </summary>
		/// <returns>true if supports</returns>
		protected override bool PlatformSupportsAutoSDKs()
		{
			return true;
		}

		/// <summary>
		/// Returns platform-specific name used in SDK repository
		/// </summary>
		/// <returns>path to SDK Repository</returns>
		public override string GetSDKTargetPlatformName()
		{
			return TargetPlatformName;
		}

		/// <summary>
		/// Returns SDK string as required by the platform
		/// </summary>
		/// <returns>Valid SDK string</returns>
		protected override string GetRequiredSDKString()
		{
			return ExpectedSDKVersion;
		}

		protected override String GetRequiredScriptVersionString()
		{
			return "3.0";
		}

		protected override bool PreferAutoSDK()
		{
			// having LINUX_ROOT set (for legacy reasons or for convenience of cross-compiling certain third party libs) should not make UBT skip AutoSDKs
			return true;
		}

		/// <summary>
		/// Whether the required external SDKs are installed for this platform
		/// </summary>
		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
			{
				return SDKStatus.Valid;
			}

			string MultiArchRoot = Environment.GetEnvironmentVariable("LINUX_MULTIARCH_ROOT");
			string BaseLinuxPath;

			if (MultiArchRoot != null)
			{
				// FIXME: UBT should loop across all the architectures and compile for all the selected ones.
				BaseLinuxPath = Path.Combine(MultiArchRoot, LinuxPlatform.DefaultArchitecture);
			}
			else
			{
				// support the existing, non-multiarch toolchains for continuity
				BaseLinuxPath = Environment.GetEnvironmentVariable("LINUX_ROOT");
			}

			// we don't have an LINUX_ROOT specified
			if (String.IsNullOrEmpty(BaseLinuxPath))
			{
				return SDKStatus.Invalid;
			}

			// paths to our toolchains
			BaseLinuxPath = BaseLinuxPath.Replace("\"", "");
			string ClangPath = Path.Combine(BaseLinuxPath, @"bin\clang++.exe");

			if (File.Exists(ClangPath))
			{
				return SDKStatus.Valid;
			}

			return SDKStatus.Invalid;
		}
	}

	class LinuxPlatformFactory : UEBuildPlatformFactory
	{
		protected override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.Linux; }
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		protected override void RegisterBuildPlatforms(SDKOutputLevel OutputLevel)
		{
			LinuxPlatformSDK SDK = new LinuxPlatformSDK();
			SDK.ManageAndValidateSDK(OutputLevel);

			if ((ProjectFileGenerator.bGenerateProjectFiles == true) || (SDK.HasRequiredSDKsInstalled() == SDKStatus.Valid))
			{
				FileReference LinuxTargetPlatformFile = FileReference.Combine(UnrealBuildTool.EngineSourceDirectory, "Developer", "Linux", "LinuxTargetPlatform", "LinuxTargetPlatform.Build.cs");
				if (FileReference.Exists(LinuxTargetPlatformFile))
				{
					// Register this build platform for Linux
					Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Linux.ToString());
					UEBuildPlatform.RegisterBuildPlatform(new LinuxPlatform(SDK));
					UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Linux, UnrealPlatformGroup.Unix);
				}
			}
		}
	}
}
