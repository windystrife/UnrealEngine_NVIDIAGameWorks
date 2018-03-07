// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Xml;
using System.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Android-specific target settings
	/// </summary>
	public class AndroidTargetRules
	{
		/// <summary>
		/// Lists Architectures that you want to build
		/// </summary>
		[CommandLine("-Architectures=", ListSeparator = '+')]
		public List<string> Architectures = new List<string>();

		/// <summary>
		/// Lists GPU Architectures that you want to build (mostly used for mobile etc.)
		/// </summary>
		[CommandLine("-GPUArchitectures=", ListSeparator = '+')]
		public List<string> GPUArchitectures = new List<string>();
	}

	/// <summary>
	/// Read-only wrapper for Android-specific target settings
	/// </summary>
	public class ReadOnlyAndroidTargetRules
	{
		/// <summary>
		/// The private mutable settings object
		/// </summary>
		private AndroidTargetRules Inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The settings object to wrap</param>
		public ReadOnlyAndroidTargetRules(AndroidTargetRules Inner)
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

		public IReadOnlyList<string> Architectures
		{
			get { return Inner.Architectures.AsReadOnly(); }
		}

		public IReadOnlyList<string> GPUArchitectures
		{
			get { return Inner.GPUArchitectures.AsReadOnly(); }
		}

		#if !__MonoCS__
		#pragma warning restore CS1591
		#endif
		#endregion
	}

	class AndroidPlatform : UEBuildPlatform
	{
		AndroidPlatformSDK SDK;

		public AndroidPlatform(AndroidPlatformSDK InSDK) : base(UnrealTargetPlatform.Android, CppPlatform.Android)
		{
			SDK = InSDK;
		}

		public override SDKStatus HasRequiredSDKsInstalled()
		{
			return SDK.HasRequiredSDKsInstalled();
		}

		public override void ResetTarget(TargetRules Target)
		{
			ValidateTarget(Target);
		}

		public override void ValidateTarget(TargetRules Target)
		{
			Target.bCompileLeanAndMeanUE = true;
			Target.bCompilePhysX = true;
			Target.bCompileAPEX = false;
			Target.bCompileNvCloth = false;

			Target.bBuildEditor = false;
			Target.bBuildDeveloperTools = false;
			Target.bCompileSimplygon = false;
			Target.bCompileSimplygonSSF = false;

			Target.bCompileRecast = true;
			Target.bDeployAfterCompile = true;
		}

		public override bool CanUseXGE()
		{
			return !(Environment.GetEnvironmentVariable("IsBuildMachine") == "1");
		}

		public override bool IsBuildProduct(string FileName, string[] NamePrefixes, string[] NameSuffixes)
		{
			return IsBuildProductWithArch(FileName, NamePrefixes, NameSuffixes, ".so")
				|| IsBuildProductWithArch(FileName, NamePrefixes, NameSuffixes, ".apk")
				|| IsBuildProductWithArch(FileName, NamePrefixes, NameSuffixes, ".a");
		}

		static bool IsBuildProductWithArch(string Name, string[] NamePrefixes, string[] NameSuffixes, string Extension)
		{
			// Strip off the extension, then a GPU suffix, then a CPU suffix, before testing whether it matches a build product name.
			if (Name.EndsWith(Extension, StringComparison.InvariantCultureIgnoreCase))
			{
				int ExtensionEndIdx = Name.Length - Extension.Length;
				foreach (string GpuSuffix in AndroidToolChain.AllGpuSuffixes)
				{
					int GpuIdx = ExtensionEndIdx - GpuSuffix.Length;
					if (GpuIdx > 0 && String.Compare(Name, GpuIdx, GpuSuffix, 0, GpuSuffix.Length, StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						foreach (string CpuSuffix in AndroidToolChain.AllCpuSuffixes)
						{
							int CpuIdx = GpuIdx - CpuSuffix.Length;
							if (CpuIdx > 0 && String.Compare(Name, CpuIdx, CpuSuffix, 0, CpuSuffix.Length, StringComparison.InvariantCultureIgnoreCase) == 0)
							{
								return IsBuildProductName(Name, 0, CpuIdx, NamePrefixes, NameSuffixes);
							}
						}
					}
				}
			}
			return false;
		}

		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return ".so";
				case UEBuildBinaryType.Executable:
					return ".so";
				case UEBuildBinaryType.StaticLibrary:
					return ".a";
				case UEBuildBinaryType.Object:
					return ".o";
				case UEBuildBinaryType.PrecompiledHeader:
					return ".gch";
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		public override string GetDebugInfoExtension(ReadOnlyTargetRules InTarget, UEBuildBinaryType InBinaryType)
		{
			return "";
		}

		public override bool HasDefaultBuildConfig(UnrealTargetPlatform Platform, DirectoryReference ProjectPath)
		{
			string[] BoolKeys = new string[] {
				"bBuildForArmV7", "bBuildForArm64", "bBuildForX86", "bBuildForX8664", 
				"bBuildForES2", "bBuildForES31", "bBuildWithHiddenSymbolVisibility"
			};

			// look up Android specific settings
			if (!DoProjectSettingsMatchDefault(Platform, ProjectPath, "/Script/AndroidRuntimeSettings.AndroidRuntimeSettings",
				BoolKeys, null, null))
			{
				return false;
			}

			// check the base settings
			return base.HasDefaultBuildConfig(Platform, ProjectPath);
		}

		public override bool ShouldCompileMonolithicBinary(UnrealTargetPlatform InPlatform)
		{
			// This platform currently always compiles monolithic
			return true;
		}

		public override bool ShouldNotBuildEditor(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true;
		}

		public override bool BuildRequiresCookedData(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true;
		}

		public override bool RequiresDeployPrepAfterCompile()
		{
			return true;
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
			if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Mac) || (Target.Platform == UnrealTargetPlatform.Linux))
			{
				bool bBuildShaderFormats = Target.bForceBuildShaderFormats;
				if (!Target.bBuildRequiresCookedData)
				{
					if (ModuleName == "Engine")
					{
						if (Target.bBuildDeveloperTools)
						{
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("AndroidTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_PVRTCTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_ATCTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_DXTTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_ETC1TargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_ETC2TargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_ASTCTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_MultiTargetPlatform");
						}
					}
					else if (ModuleName == "TargetPlatform")
					{
						bBuildShaderFormats = true;
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatPVR");
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatDXT");
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatASTC");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("TextureFormatAndroid");    // ATITC, ETC1 and ETC2
						if (Target.bBuildDeveloperTools)
						{
							//Rules.DynamicallyLoadedModuleNames.Add("AudioFormatADPCM");	//@todo android: android audio
						}
					}
				}

				// allow standalone tools to use targetplatform modules, without needing Engine
				if (ModuleName == "TargetPlatform")
				{
					if (Target.bForceBuildTargetPlatforms)
					{
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("AndroidTargetPlatform");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_PVRTCTargetPlatform");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_ATCTargetPlatform");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_DXTTargetPlatform");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_ETC1TargetPlatform");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_ETC2TargetPlatform");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_ASTCTargetPlatform");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("Android_MultiTargetPlatform");
					}

					if (bBuildShaderFormats)
					{
						//Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatAndroid");		//@todo android: ShaderFormatAndroid
					}
				}
			}
		}

		public override List<FileReference> FinalizeBinaryPaths(FileReference BinaryName, FileReference ProjectFile, ReadOnlyTargetRules Target)
		{
			AndroidToolChain ToolChain = new AndroidToolChain(ProjectFile, false, Target.AndroidPlatform.Architectures, Target.AndroidPlatform.GPUArchitectures);

			var Architectures = ToolChain.GetAllArchitectures();
			var GPUArchitectures = ToolChain.GetAllGPUArchitectures();

			// make multiple output binaries
			List<FileReference> AllBinaries = new List<FileReference>();
			foreach (string Architecture in Architectures)
			{
				foreach (string GPUArchitecture in GPUArchitectures)
				{
					AllBinaries.Add(new FileReference(AndroidToolChain.InlineArchName(BinaryName.FullName, Architecture, GPUArchitecture)));
				}
			}

			return AllBinaries;
		}

		private bool IsVulkanSDKAvailable()
		{
			bool bHaveVulkan = false;

			// First look for VulkanSDK (two possible env variables)
			string VulkanSDKPath = Environment.GetEnvironmentVariable("VULKAN_SDK");
			if (String.IsNullOrEmpty(VulkanSDKPath))
			{
				VulkanSDKPath = Environment.GetEnvironmentVariable("VK_SDK_PATH");
			}

			// Note: header is the same for all architectures so just use arch-arm
			string NDKPath = Environment.GetEnvironmentVariable("NDKROOT");
			string NDKVulkanIncludePath = NDKPath + "/platforms/android-24/arch-arm/usr/include/vulkan";

			// Use NDK Vulkan header if discovered, or VulkanSDK if available
			if (File.Exists(NDKVulkanIncludePath + "/vulkan.h"))
			{
				bHaveVulkan = true;
			}
			else
			if (!String.IsNullOrEmpty(VulkanSDKPath))
			{
				bHaveVulkan = true;
			}
			else
			if (File.Exists("ThirdParty/Vulkan/Windows/Include/vulkan/vulkan.h"))
			{
				bHaveVulkan = true;
			}

			return bHaveVulkan;
		}

		public override void AddExtraModules(ReadOnlyTargetRules Target, List<string> PlatformExtraModules)
		{
			bool bVulkanExists = IsVulkanSDKAvailable();
			if (bVulkanExists)
			{
				PlatformExtraModules.Add("VulkanRHI");
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
		}

		public override void SetUpEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{
			// we want gcc toolchain 4.9, but fall back to 4.8 or 4.6 for now if it doesn't exist
			string NDKPath = Environment.GetEnvironmentVariable("NDKROOT");
			NDKPath = NDKPath.Replace("\"", "");

			AndroidToolChain ToolChain = new AndroidToolChain(Target.ProjectFile, false, Target.AndroidPlatform.Architectures, Target.AndroidPlatform.GPUArchitectures);

			// figure out the NDK version
			string NDKToolchainVersion = "unknown";
			string NDKDefine = "100500";	// assume r10e
			string SourcePropFilename = Path.Combine(NDKPath, "source.properties");
			if (File.Exists(SourcePropFilename))
			{
				string RevisionString = "";
				string[] PropertyContents = File.ReadAllLines(SourcePropFilename);
				foreach (string PropertyLine in PropertyContents)
				{
					if (PropertyLine.StartsWith("Pkg.Revision"))
					{
						RevisionString = PropertyLine;
						break;
					}
				}

				int EqualsIndex = RevisionString.IndexOf('=');
				if (EqualsIndex > 0)
				{
					string[] RevisionParts = RevisionString.Substring(EqualsIndex + 1).Trim().Split('.');
					int RevisionMinor = int.Parse(RevisionParts.Length > 1 ? RevisionParts[1] : "0");
					char RevisionLetter = Convert.ToChar('a' + RevisionMinor);
					int RevisionBeta = 0;  // @TODO
					NDKToolchainVersion = "r" + RevisionParts[0] + (RevisionMinor > 0 ? Char.ToString(RevisionLetter) : "");
					NDKDefine = RevisionParts[0] + string.Format("{0:00}", RevisionMinor + 1) + string.Format("{0:00}", RevisionBeta);
				}
			}
			else {
				string ReleaseFilename = Path.Combine(NDKPath, "RELEASE.TXT");
				if (File.Exists(ReleaseFilename))
				{
					string[] PropertyContents = File.ReadAllLines(SourcePropFilename);
					NDKToolchainVersion = PropertyContents[0];
				}
			}
			
			// PLATFORM_ANDROID_NDK_VERSION is in the form 150100, where 15 is major version, 01 is the letter (1 is 'a'), 00 indicates beta revision if letter is 00
			Log.TraceInformation("PLATFORM_ANDROID_NDK_VERSION = {0}", NDKDefine);
			CompileEnvironment.Definitions.Add("PLATFORM_ANDROID_NDK_VERSION=" + NDKDefine);

			string GccVersion = "4.6";
			int NDKVersionInt = ToolChain.GetNdkApiLevelInt();
			if (Directory.Exists(Path.Combine(NDKPath, @"sources/cxx-stl/gnu-libstdc++/4.9")))
			{
				GccVersion = "4.9";
			} else
			if (Directory.Exists(Path.Combine(NDKPath, @"sources/cxx-stl/gnu-libstdc++/4.8")))
			{
				GccVersion = "4.8";
			}

			Log.TraceInformation("NDK toolchain: {0}, NDK version: {1}, GccVersion: {2}, ClangVersion: {3}", NDKToolchainVersion, NDKVersionInt.ToString(), GccVersion, ToolChain.GetClangVersionString());

			CompileEnvironment.Definitions.Add("PLATFORM_DESKTOP=0");
			CompileEnvironment.Definitions.Add("PLATFORM_CAN_SUPPORT_EDITORONLY_DATA=0");

			CompileEnvironment.Definitions.Add("WITH_OGGVORBIS=1");

			CompileEnvironment.Definitions.Add("UNICODE");
			CompileEnvironment.Definitions.Add("_UNICODE");

			CompileEnvironment.Definitions.Add("PLATFORM_ANDROID=1");
			CompileEnvironment.Definitions.Add("ANDROID=1");

			CompileEnvironment.Definitions.Add("WITH_DATABASE_SUPPORT=0");
			CompileEnvironment.Definitions.Add("WITH_EDITOR=0");
			CompileEnvironment.Definitions.Add("USE_NULL_RHI=0");
			CompileEnvironment.Definitions.Add("REQUIRES_ALIGNED_INT_ACCESS");

			CompileEnvironment.IncludePaths.SystemIncludePaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/include");

			// the toolchain will actually filter these out
			CompileEnvironment.IncludePaths.SystemIncludePaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/armeabi-v7a/include");
			CompileEnvironment.IncludePaths.SystemIncludePaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/arm64-v8a/include");
			CompileEnvironment.IncludePaths.SystemIncludePaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/x86/include");
			CompileEnvironment.IncludePaths.SystemIncludePaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/x86_64/include");

			LinkEnvironment.LibraryPaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/armeabi-v7a");
			LinkEnvironment.LibraryPaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/arm64-v8a");
			LinkEnvironment.LibraryPaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/x86");
			LinkEnvironment.LibraryPaths.Add("$(NDKROOT)/sources/cxx-stl/gnu-libstdc++/" + GccVersion + "/libs/x86_64");

			CompileEnvironment.IncludePaths.SystemIncludePaths.Add("$(NDKROOT)/sources/android/native_app_glue");
			CompileEnvironment.IncludePaths.SystemIncludePaths.Add("$(NDKROOT)/sources/android/cpufeatures");

			//@TODO: Tegra Gfx Debugger - standardize locations - for now, change the hardcoded paths and force this to return true to test
			if (UseTegraGraphicsDebugger(Target))
			{
				//LinkEnvironment.LibraryPaths.Add("ThirdParty/NVIDIA/TegraGfxDebugger");
				//LinkEnvironment.LibraryPaths.Add("F:/NVPACK/android-kk-egl-t124-a32/stub");
				//LinkEnvironment.AdditionalLibraries.Add("Nvidia_gfx_debugger_stub");
			}

			SetupGraphicsDebugger(Target, CompileEnvironment, LinkEnvironment);

			LinkEnvironment.AdditionalLibraries.Add("gnustl_shared");
			LinkEnvironment.AdditionalLibraries.Add("gcc");
			LinkEnvironment.AdditionalLibraries.Add("z");
			LinkEnvironment.AdditionalLibraries.Add("c");
			LinkEnvironment.AdditionalLibraries.Add("m");
			LinkEnvironment.AdditionalLibraries.Add("log");
			LinkEnvironment.AdditionalLibraries.Add("dl");
			if (!UseTegraGraphicsDebugger(Target))
			{
				LinkEnvironment.AdditionalLibraries.Add("GLESv2");
				LinkEnvironment.AdditionalLibraries.Add("EGL");
			}
			LinkEnvironment.AdditionalLibraries.Add("OpenSLES");
			LinkEnvironment.AdditionalLibraries.Add("android");
		}

		private bool UseTegraGraphicsDebugger(ReadOnlyTargetRules Target)
		{
			// Disable for now
			return false;
		}

		private bool SetupGraphicsDebugger(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{
			string AndroidGraphicsDebugger;
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Target.ProjectFile), UnrealTargetPlatform.Android);
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "AndroidGraphicsDebugger", out AndroidGraphicsDebugger);

			if (AndroidGraphicsDebugger.ToLower() == "renderdoc")
			{
				string RenderDocPath;
				AndroidPlatformSDK.GetPath(Ini, "/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "RenderDocPath", out RenderDocPath);
				string RenderDocLibPath = Path.Combine(RenderDocPath, @"android\lib\armeabi-v7a");
				if (Directory.Exists(RenderDocLibPath))
				{
					LinkEnvironment.LibraryPaths.Add(RenderDocLibPath);
					LinkEnvironment.AdditionalLibraries.Add("VkLayer_GLES_RenderDoc");
					return true;
				}
			}

			return false;
		}

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

		public override UEToolChain CreateToolChain(CppPlatform CppPlatform, ReadOnlyTargetRules Target)
		{
			bool bUseLdGold = Target.bUseUnityBuild;
			return new AndroidToolChain(Target.ProjectFile, bUseLdGold, Target.AndroidPlatform.Architectures, Target.AndroidPlatform.GPUArchitectures);
		}

		/// <summary>
		/// Deploys the given target
		/// </summary>
		/// <param name="Target">Information about the target being deployed</param>
		public override void Deploy(UEBuildDeployTarget Target)
		{
			new UEDeployAndroid(Target.ProjectFile).PrepTargetForDeployment(Target);
		}
	}

	class AndroidPlatformSDK : UEBuildPlatformSDK
	{
		protected override bool PlatformSupportsAutoSDKs()
		{
			return true;
		}

		public override string GetSDKTargetPlatformName()
		{
			return "Android";
		}

		protected override string GetRequiredSDKString()
		{
			return "-21";
		}

		protected override String GetRequiredScriptVersionString()
		{
			return "3.2";
		}

		// prefer auto sdk on android as correct 'manual' sdk detection isn't great at the moment.
		protected override bool PreferAutoSDK()
		{
			return true;
		}

		private static bool ExtractPath(string Source, out string Path)
		{
			int start = Source.IndexOf('"');
			int end = Source.LastIndexOf('"');
			if (start != 1 && end != -1 && start < end)
			{
				++start;
				Path = Source.Substring(start, end - start);
				return true;
			}
			else
			{
				Path = "";
			}

			return false;
		}

		public static bool GetPath(ConfigHierarchy Ini, string SectionName, string Key, out string Value)
		{
			string temp;
			if (Ini.TryGetValue(SectionName, Key, out temp))
			{
				return ExtractPath(temp, out Value);
			}
			else
			{
				Value = "";
			}

			return false;
		}

		/// <summary>
		/// checks if the sdk is installed or has been synced
		/// </summary>
		/// <returns></returns>
		private bool HasAnySDK()
		{
			string NDKPath = Environment.GetEnvironmentVariable("NDKROOT");
			{
				var configCacheIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, (DirectoryReference)null, UnrealTargetPlatform.Unknown);
				var AndroidEnv = new Dictionary<string, string>();

				Dictionary<string, string> EnvVarNames = new Dictionary<string, string> { 
                                                         {"ANDROID_HOME", "SDKPath"}, 
                                                         {"NDKROOT", "NDKPath"}, 
                                                         {"ANT_HOME", "ANTPath"},
                                                         {"JAVA_HOME", "JavaPath"}
                                                         };

				string path;
				foreach (var kvp in EnvVarNames)
				{
					if (GetPath(configCacheIni, "/Script/AndroidPlatformEditor.AndroidSDKSettings", kvp.Value, out path) && !string.IsNullOrEmpty(path))
					{
						AndroidEnv.Add(kvp.Key, path);
					}
					else
					{
						var envValue = Environment.GetEnvironmentVariable(kvp.Key);
						if (!String.IsNullOrEmpty(envValue))
						{
							AndroidEnv.Add(kvp.Key, envValue);
						}
					}
				}

				// If we are on Mono and we are still missing a key then go and find it from the .bash_profile
				if (Utils.IsRunningOnMono && !EnvVarNames.All(s => AndroidEnv.ContainsKey(s.Key)))
				{
					string BashProfilePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), ".bash_profile");
					if (!File.Exists(BashProfilePath))
					{
						// Try .bashrc if didn't fine .bash_profile
						BashProfilePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), ".bashrc");
					}
					if (File.Exists(BashProfilePath))
					{
						string[] BashProfileContents = File.ReadAllLines(BashProfilePath);

						// Walk backwards so we keep the last export setting instead of the first
						for (int LineIndex = BashProfileContents.Length - 1; LineIndex >= 0; --LineIndex)
						{
							foreach (var kvp in EnvVarNames)
							{
								if (AndroidEnv.ContainsKey(kvp.Key))
								{
									continue;
								}

								if (BashProfileContents[LineIndex].StartsWith("export " + kvp.Key + "="))
								{
									string PathVar = BashProfileContents[LineIndex].Split('=')[1].Replace("\"", "");
									AndroidEnv.Add(kvp.Key, PathVar);
								}
							}
						}
					}
				}

				// Set for the process
				foreach (var kvp in AndroidEnv)
				{
					Environment.SetEnvironmentVariable(kvp.Key, kvp.Value);
				}

				// See if we have an NDK path now...
				AndroidEnv.TryGetValue("NDKROOT", out NDKPath);
			}

			// we don't have an NDKROOT specified
			if (String.IsNullOrEmpty(NDKPath))
			{
				return false;
			}

			NDKPath = NDKPath.Replace("\"", "");

			// need a supported llvm
			if (!Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm")) &&
				!Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm-3.6")) &&
				!Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm-3.5")) &&
				!Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm-3.3")) &&
				!Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm-3.1")))
			{
				return false;
			}
			return true;
		}

		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			// if any autosdk setup has been done then the local process environment is suspect
			if (HasSetupAutoSDK())
			{
				return SDKStatus.Invalid;
			}

			if (HasAnySDK())
			{
				return SDKStatus.Valid;
			}

			return SDKStatus.Invalid;
		}
	}

	class AndroidPlatformFactory : UEBuildPlatformFactory
	{
		protected override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.Android; }
		}

		protected override void RegisterBuildPlatforms(SDKOutputLevel OutputLevel)
		{
			AndroidPlatformSDK SDK = new AndroidPlatformSDK();
			SDK.ManageAndValidateSDK(OutputLevel);

			if ((ProjectFileGenerator.bGenerateProjectFiles == true) || (SDK.HasRequiredSDKsInstalled() == SDKStatus.Valid) || Environment.GetEnvironmentVariable("IsBuildMachine") == "1")
			{
				bool bRegisterBuildPlatform = true;

				FileReference AndroidTargetPlatformFile = FileReference.Combine(UnrealBuildTool.EngineSourceDirectory, "Developer", "Android", "AndroidTargetPlatform", "AndroidTargetPlatform.Build.cs");
				if (FileReference.Exists(AndroidTargetPlatformFile) == false)
				{
					bRegisterBuildPlatform = false;
				}

				if (bRegisterBuildPlatform == true)
				{
					// Register this build platform
					Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Android.ToString());
					UEBuildPlatform.RegisterBuildPlatform(new AndroidPlatform(SDK));
					UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Android, UnrealPlatformGroup.Android);
				}
			}
		}
	}
}
