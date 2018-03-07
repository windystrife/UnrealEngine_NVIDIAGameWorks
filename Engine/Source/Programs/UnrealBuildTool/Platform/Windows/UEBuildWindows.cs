// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;
using System.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Available compiler toolchains on Windows platform
	/// </summary>
	public enum WindowsCompiler
	{
		/// <summary>
		/// Use the default compiler. A specific value will always be used outside of configuration classes.
		/// </summary>
		Default,

		/// <summary>
		/// Visual Studio 2013 (Visual C++ 12.0)
		/// </summary>
		[Obsolete("UE4 does not support building Visual Studio 2013 targets from the 4.16 release onwards.")]
		VisualStudio2013,

		/// <summary>
		/// Visual Studio 2015 (Visual C++ 14.0)
		/// </summary>
		VisualStudio2015,

		/// <summary>
		/// Visual Studio 2017 (Visual C++ 15.0)
		/// </summary>
		VisualStudio2017,
	}

	/// <summary>
	/// Which static analyzer to use
	/// </summary>
	public enum WindowsStaticAnalyzer
	{
		/// <summary>
		/// Do not perform static analysis
		/// </summary>
		None,

		/// <summary>
		/// Use the built-in Visual C++ static analyzer
		/// </summary>
		VisualCpp,

		/// <summary>
		/// Use PVS-Studio for static analysis
		/// </summary>
		PVSStudio,
	}

	/// <summary>
	/// Windows-specific target settings
	/// </summary>
	public class WindowsTargetRules
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public WindowsTargetRules()
		{
			XmlConfig.ApplyTo(this);
		}

		/// <summary>
		/// Version of the compiler toolchain to use on Windows platform. A value of "default" will be changed to a specific version at UBT startup.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "CompilerVersion")]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-2015", Value = "VisualStudio2015")]
		[CommandLine("-2017", Value = "VisualStudio2017")]
		public WindowsCompiler Compiler = WindowsCompiler.Default;

		/// <summary>
		/// Enable PIX debugging (automatically disabled in Shipping and Test configs)
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "bEnablePIXProfiling")]
		public bool bPixProfilingEnabled = true;

		/// <summary>
		/// The name of the company (author, provider) that created the project.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Game, "/Script/EngineSettings.GeneralProjectSettings", "CompanyName")]
		public string CompanyName;

		/// <summary>
		/// The project's copyright and/or trademark notices.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Game, "/Script/EngineSettings.GeneralProjectSettings", "CopyrightNotice")]
		public string CopyrightNotice;

		/// <summary>
		/// The project's name.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Game, "/Script/EngineSettings.GeneralProjectSettings", "ProjectName")]
		public string ProductName;

		/// <summary>
		/// The static analyzer to use
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-StaticAnalyzer")]
		public WindowsStaticAnalyzer StaticAnalyzer = WindowsStaticAnalyzer.None;

		/// <summary>
		/// Whether we should export a file containing .obj->source file mappings.
		/// </summary>
		[XmlConfigFile]
		[CommandLine("-ObjSrcMap")]
		public string ObjSrcMapFile = null;

		/// <summary>
		/// Provides a Module Definition File (.def) to the linker to describe various attributes of a DLL.
		/// Necessary when exporting functions by ordinal values instead of by name.
		/// </summary>
		public string ModuleDefinitionFile;

		/// <summary>
		/// Enables strict standard conformance mode (/permissive-) in VS2017+.
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-Strict")]
		public bool bStrictConformanceMode = false; 

		/// VS2015 updated some of the CRT definitions but not all of the Windows SDK has been updated to match.
		/// Microsoft provides legacy_stdio_definitions library to enable building with VS2015 until they fix everything up.
		public bool bNeedsLegacyStdioDefinitionsLib
		{
			get { return Compiler == WindowsCompiler.VisualStudio2015 || Compiler == WindowsCompiler.VisualStudio2017; }
		}

		/// <summary>
		/// When using a Visual Studio compiler, returns the version name as a string
		/// </summary>
		/// <returns>The Visual Studio compiler version name (e.g. "2015")</returns>
		public string GetVisualStudioCompilerVersionName()
		{
			switch (Compiler)
			{
				case WindowsCompiler.VisualStudio2015:
					return "2015";
				case WindowsCompiler.VisualStudio2017:
					return "2015"; // VS2017 is backwards compatible with VS2015 compiler

				default:
					throw new BuildException("Unexpected WindowsCompiler version for GetVisualStudioCompilerVersionName().  Either not using a Visual Studio compiler or switch block needs to be updated");
			}
		}
	}

	/// <summary>
	/// Read-only wrapper for Windows-specific target settings
	/// </summary>
	public class ReadOnlyWindowsTargetRules
	{
		/// <summary>
		/// The private mutable settings object
		/// </summary>
		private WindowsTargetRules Inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The settings object to wrap</param>
		public ReadOnlyWindowsTargetRules(WindowsTargetRules Inner)
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

		public WindowsCompiler Compiler
		{
			get { return Inner.Compiler; }
		}

		public bool bPixProfilingEnabled
		{
			get { return Inner.bPixProfilingEnabled; }
		}

		public string CompanyName
		{
			get { return Inner.CompanyName; }
		}

		public string CopyrightNotice
		{
			get { return Inner.CopyrightNotice; }
		}

		public string ProductName
		{
			get { return Inner.ProductName; }
		}

		public WindowsStaticAnalyzer StaticAnalyzer
		{
			get { return Inner.StaticAnalyzer; }
		}

		public string ObjSrcMapFile
		{
			get { return Inner.ObjSrcMapFile; }
		}

		public string ModuleDefinitionFile
		{
			get { return Inner.ModuleDefinitionFile; }
		}

		public bool bNeedsLegacyStdioDefinitionsLib
		{
			get { return Inner.bNeedsLegacyStdioDefinitionsLib; }
		}

		public bool bStrictConformanceMode
		{
			get { return Inner.bStrictConformanceMode; }
		}

		public string GetVisualStudioCompilerVersionName()
		{
			return Inner.GetVisualStudioCompilerVersionName();
		}

		#if !__MonoCS__
		#pragma warning restore CS1591
		#endif
		#endregion
	}

	class WindowsPlatform : UEBuildPlatform
	{
		/// <summary>
		/// True if we should use Clang/LLVM instead of MSVC to compile code on Windows platform
		/// </summary>
		public static readonly bool bCompileWithClang = false;

		/// <summary>
		/// When using Clang, enabling enables the MSVC-like "clang-cl" wrapper, otherwise we pass arguments to Clang directly
		/// </summary>
		public static readonly bool bUseVCCompilerArgs = true;

		/// <summary>
		/// True if we should use the Clang linker (LLD) when bCompileWithClang is enabled, otherwise we use the MSVC linker
		/// </summary>
		public static readonly bool bAllowClangLinker = bCompileWithClang && false;

		/// <summary>
		/// True if we should use the Intel Compiler instead of MSVC to compile code on Windows platform
		/// </summary>
		public static readonly bool bCompileWithICL = false;

		/// <summary>
		/// True if we should use the Intel linker (xilink) when bCompileWithICL is enabled, otherwise we use the MSVC linker
		/// </summary>
		public static readonly bool bAllowICLLinker = bCompileWithICL && true;

		/// <summary>
		/// Whether to compile against the Windows 10 SDK, instead of the Windows 8.1 SDK.  This requires the Visual Studio 2015
		/// compiler or later, and the Windows 10 SDK must be installed.  The application will require at least Windows 8.x to run.
		/// @todo UWP: Expose this to be enabled more easily for building Windows 10 desktop apps
		/// </summary>
		public static readonly bool bUseWindowsSDK10 = false;

		/// <summary>
		/// True if we allow using addresses larger than 2GB on 32 bit builds
		/// </summary>
		public static readonly bool bBuildLargeAddressAwareBinary = true;

		WindowsPlatformSDK SDK;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InPlatform">Creates a windows platform with the given enum value</param>
		/// <param name="InDefaultCppPlatform">The default C++ platform to compile for</param>
		/// <param name="InSDK">The installed Windows SDK</param>
		public WindowsPlatform(UnrealTargetPlatform InPlatform, CppPlatform InDefaultCppPlatform, WindowsPlatformSDK InSDK) : base(InPlatform, InDefaultCppPlatform)
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
		/// Validate a target's settings
		/// </summary>
		public override void ValidateTarget(TargetRules Target)
		{
			Target.bCompileNvCloth = true;
			// Disable Simplygon support if compiling against the NULL RHI.
			if (Target.GlobalDefinitions.Contains("USE_NULL_RHI=1"))
			{
				Target.bCompileSimplygon = false;
				Target.bCompileSimplygonSSF = false;
				Target.bCompileCEF3 = false;
			}

			// Set the compiler version if necessary
			if (Target.WindowsPlatform.Compiler == WindowsCompiler.Default)
			{
				Target.WindowsPlatform.Compiler = GetDefaultCompiler();
			}

			// Disable linking if we're using a static analyzer
			if(Target.WindowsPlatform.StaticAnalyzer != WindowsStaticAnalyzer.None)
			{
				Target.bDisableLinking = true;
			}

			// Disable PCHs for PVS studio
			if(Target.WindowsPlatform.StaticAnalyzer == WindowsStaticAnalyzer.PVSStudio)
			{
				Target.bUsePCHFiles = false;
			}

			// Override PCH settings
			if (bCompileWithClang)
			{
				// @todo clang: Shared PCHs don't work on clang yet because the PCH will have definitions assigned to different values
				// than the consuming translation unit.  Unlike the warning in MSVC, this is a compile in Clang error which cannot be suppressed
				Target.bUseSharedPCHs = false;

				// @todo clang: PCH files aren't supported by "clang-cl" yet (no /Yc support, and "-x c++-header" cannot be specified)
				if (WindowsPlatform.bUseVCCompilerArgs)
				{
					Target.bUsePCHFiles = false;
				}
			}
			if (bCompileWithICL)
			{
				Target.NumIncludedBytesPerUnityCPP = Math.Min(Target.NumIncludedBytesPerUnityCPP, 256 * 1024);

				Target.bUseSharedPCHs = false;

				if (WindowsPlatform.bUseVCCompilerArgs)
				{
					Target.bUsePCHFiles = false;
				}
			}

			// E&C support.
			if (Target.bSupportEditAndContinue && Target.Configuration == UnrealTargetConfiguration.Debug)
			{
				Target.bUseIncrementalLinking = true;
			}

			// Incremental linking.
			if (Target.bUseIncrementalLinking && !Target.bDisableDebugInfo)
			{
				Target.bUsePDBFiles = true;
			}
		}

		/// <summary>
		/// Gets the default compiler which should be used, if it's not set explicitly by the target, command line, or config file.
		/// </summary>
		/// <returns>The default compiler version</returns>
		internal static WindowsCompiler GetDefaultCompiler()
		{
			// If there's no specific compiler set, try to pick the matching compiler for the selected IDE
			object ProjectFormatObject;
			if (XmlConfig.TryGetValue(typeof(VCProjectFileGenerator), "Version", out ProjectFormatObject))
			{
				VCProjectFileFormat ProjectFormat = (VCProjectFileFormat)ProjectFormatObject;
				if (ProjectFormat == VCProjectFileFormat.VisualStudio2017)
				{
					return WindowsCompiler.VisualStudio2017;
				}
				else if (ProjectFormat == VCProjectFileFormat.VisualStudio2015)
				{
					return WindowsCompiler.VisualStudio2015;
				}
			}

			// Second, default based on what's installed, test for 2015 first
			DirectoryReference VCInstallDir;
			if (TryGetVCInstallDir(WindowsCompiler.VisualStudio2015, out VCInstallDir))
			{
				return WindowsCompiler.VisualStudio2015;
			}
			if (TryGetVCInstallDir(WindowsCompiler.VisualStudio2017, out VCInstallDir))
			{
				return WindowsCompiler.VisualStudio2017;
			}

			// If we do have a Visual Studio installation, but we're missing just the C++ parts, warn about that.
			DirectoryReference VSInstallDir;
			if (TryGetVSInstallDir(WindowsCompiler.VisualStudio2015, out VSInstallDir))
			{
				Log.TraceWarning("Visual Studio 2015 is installed, but is missing the C++ toolchain. Please verify that \"Common Tools for Visual C++ 2015\" are selected from the Visual Studio 2015 installation options.");
			}
			else if (TryGetVSInstallDir(WindowsCompiler.VisualStudio2017, out VSInstallDir))
			{
				Log.TraceWarning("Visual Studio 2017 is installed, but is missing the C++ toolchain. Please verify that the \"VC++ 2017 toolset\" component is selected in the Visual Studio 2017 installation options.");
			}
			else
			{
				Log.TraceWarning("No Visual C++ installation was found. Please download and install Visual Studio 2015 with C++ components.");
			}

			// Finally, default to VS2015 anyway
			return WindowsCompiler.VisualStudio2015;
		}

		/// <summary>
		/// Returns the human-readable name of the given compiler
		/// </summary>
		/// <param name="Compiler">The compiler value</param>
		/// <returns>Name of the compiler</returns>
		public static string GetCompilerName(WindowsCompiler Compiler)
		{
			switch (Compiler)
			{
				case WindowsCompiler.VisualStudio2015:
					return "Visual Studio 2015";
				case WindowsCompiler.VisualStudio2017:
					return "Visual Studio 2017";
				default:
					return Compiler.ToString();
			}
		}

		/// <summary>
		/// Read the Visual Studio install directory for the given compiler version. Note that it is possible for the compiler toolchain to be installed without
		/// Visual Studio.
		/// </summary>
		/// <param name="Compiler">Version of the toolchain to look for.</param>
		/// <param name="InstallDir">On success, the directory that Visual Studio is installed to.</param>
		/// <returns>True if the directory was found, false otherwise.</returns>
		public static bool TryGetVSInstallDir(WindowsCompiler Compiler, out DirectoryReference InstallDir)
		{
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Win64 && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Win32)
			{
				InstallDir = null;
				return false;
			}

			switch (Compiler)
			{
				case WindowsCompiler.VisualStudio2015:
					return TryReadInstallDirRegistryKey32("Microsoft\\VisualStudio\\SxS\\VS7", "14.0", out InstallDir);
				case WindowsCompiler.VisualStudio2017:
					return TryReadInstallDirRegistryKey32("Microsoft\\VisualStudio\\SxS\\VS7", "15.0", out InstallDir);
				default:
					throw new BuildException("Invalid compiler version ({0})", Compiler);
			}
		}

		/// <summary>
		/// Read the Visual C++ install directory for the given version.
		/// </summary>
		/// <param name="Compiler">Version of the toolchain to look for.</param>
		/// <param name="InstallDir">On success, the directory that Visual C++ is installed to.</param>
		/// <returns>True if the directory was found, false otherwise.</returns>
		public static bool TryGetVCInstallDir(WindowsCompiler Compiler, out DirectoryReference InstallDir)
		{
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Win64 && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Win32)
			{
				InstallDir = null;
				return false;
			}

			if (Compiler == WindowsCompiler.VisualStudio2015)
			{
				return TryReadInstallDirRegistryKey32("Microsoft\\VisualStudio\\SxS\\VC7", "14.0", out InstallDir);
			}
			else if (Compiler == WindowsCompiler.VisualStudio2017)
			{
				// VS15Preview installs the compiler under the IDE directory, and does not register it as a standalone component. Check just in case this changes 
				// for compat in future since it's more specific.
				if (TryReadInstallDirRegistryKey32("Microsoft\\VisualStudio\\SxS\\VC7", "15.0", out InstallDir))
				{
					return true;
				}

				// Otherwise just check under the VS folder...
				DirectoryReference VSInstallDir;
				if (TryGetVSInstallDir(Compiler, out VSInstallDir))
				{
					FileReference VersionPath = FileReference.Combine(VSInstallDir, "VC", "Auxiliary", "Build", "Microsoft.VCToolsVersion.default.txt");
					if (FileReference.Exists(VersionPath))
					{
						InstallDir = VersionPath.Directory.ParentDirectory.ParentDirectory;
						return true;
					}
				}
				return false;
			}
			else
			{
				throw new BuildException("Invalid compiler version ({0})", Compiler);
			}
		}

		/// <summary>
		/// Reads an install directory for a 32-bit program from a registry key. This checks for per-user and machine wide settings, and under the Wow64 virtual keys (HKCU\SOFTWARE, HKLM\SOFTWARE, HKCU\SOFTWARE\Wow6432Node, HKLM\SOFTWARE\Wow6432Node).
		/// </summary>
		/// <param name="KeySuffix">Path to the key to read, under one of the roots listed above.</param>
		/// <param name="ValueName">Value to be read.</param>
		/// <param name="InstallDir">On success, the directory corresponding to the value read.</param>
		/// <returns>True if the key was read, false otherwise.</returns>
		static bool TryReadInstallDirRegistryKey32(string KeySuffix, string ValueName, out DirectoryReference InstallDir)
		{
			if (TryReadDirRegistryKey("HKEY_CURRENT_USER\\SOFTWARE\\" + KeySuffix, ValueName, out InstallDir))
			{
				return true;
			}
			if (TryReadDirRegistryKey("HKEY_LOCAL_MACHINE\\SOFTWARE\\" + KeySuffix, ValueName, out InstallDir))
			{
				return true;
			}
			if (TryReadDirRegistryKey("HKEY_CURRENT_USER\\SOFTWARE\\Wow6432Node\\" + KeySuffix, ValueName, out InstallDir))
			{
				return true;
			}
			if (TryReadDirRegistryKey("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\" + KeySuffix, ValueName, out InstallDir))
			{
				return true;
			}
			return false;
		}

		/// <summary>
		/// Attempts to reads a directory name stored in a registry key
		/// </summary>
		/// <param name="KeyName">Key to read from</param>
		/// <param name="ValueName">Value within the key to read</param>
		/// <param name="Value">The directory read from the registry key</param>
		/// <returns>True if the key was read, false if it was missing or empty</returns>
		static bool TryReadDirRegistryKey(string KeyName, string ValueName, out DirectoryReference Value)
		{
			string StringValue = Registry.GetValue(KeyName, ValueName, null) as string;
			if (String.IsNullOrEmpty(StringValue))
			{
				Value = null;
				return false;
			}
			else
			{
				Value = new DirectoryReference(StringValue);
				return true;
			}
		}

		/// <summary>
		/// If this platform can be compiled with SN-DBS
		/// </summary>
		public override bool CanUseSNDBS()
		{
			// Check that SN-DBS is available
			string SCERootPath = Environment.GetEnvironmentVariable("SCE_ROOT_DIR");
			if (!String.IsNullOrEmpty(SCERootPath))
			{
				string SNDBSPath = Path.Combine(SCERootPath, "common", "sn-dbs", "bin", "dbsbuild.exe");
				bool bIsSNDBSAvailable = File.Exists(SNDBSPath);
				return bIsSNDBSAvailable;
			}
			else
			{
				return false;
			}
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
			return IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".exe")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".dll")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".dll.response")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".lib")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".pdb")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".exp")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".obj")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".map")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".objpaths");
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
					return ".dll";
				case UEBuildBinaryType.Executable:
					return ".exe";
				case UEBuildBinaryType.StaticLibrary:
					return ".lib";
				case UEBuildBinaryType.Object:
					return ".obj";
				case UEBuildBinaryType.PrecompiledHeader:
					return ".pch";
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		/// <summary>
		/// Get the extension to use for debug info for the given binary type
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The debug info extension (i.e. 'pdb')</returns>
		public override string GetDebugInfoExtension(ReadOnlyTargetRules Target, UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
				case UEBuildBinaryType.Executable:
					return ".pdb";
			}
			return "";
		}

		/// <summary>
		/// Whether the editor should be built for this platform or not
		/// </summary>
		/// <param name="InPlatform"> The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration">The UnrealTargetConfiguration being built</param>
		/// <returns>bool   true if the editor should be built, false if not</returns>
		public override bool ShouldNotBuildEditor(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return false;
		}

		public override bool BuildRequiresCookedData(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return false;
		}

		public override bool HasDefaultBuildConfig(UnrealTargetPlatform Platform, DirectoryReference ProjectPath)
		{
			if (Platform == UnrealTargetPlatform.Win32)
			{
				string[] StringKeys = new string[] {
					"MinimumOSVersion"
				};

				// look up OS specific settings
				if (!DoProjectSettingsMatchDefault(Platform, ProjectPath, "/Script/WindowsTargetPlatform.WindowsTargetSettings",
					null, null, StringKeys))
				{
					return false;
				}
			}

			// check the base settings
			return base.HasDefaultBuildConfig(Platform, ProjectPath);
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
		/// Return whether this platform has uniquely named binaries across multiple games
		/// </summary>
		public override bool HasUniqueBinaries()
		{
			// Windows applications have many shared binaries between games
			return false;
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
					Rules.DynamicallyLoadedModuleNames.Add("WindowsTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("WindowsNoEditorTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("WindowsServerTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("WindowsClientTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("AllDesktopTargetPlatform");
				}

				if (bBuildShaderFormats)
				{
					Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatD3D");
					Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatOpenGL");

					Rules.DynamicallyLoadedModuleNames.Remove("VulkanRHI");
					Rules.DynamicallyLoadedModuleNames.Add("VulkanShaderFormat");
				}
			}

			if (ModuleName == "D3D11RHI")
			{
				// To enable platform specific D3D11 RHI Types
				Rules.PrivateIncludePaths.Add("Runtime/Windows/D3D11RHI/Private/Windows");
			}

			if (ModuleName == "D3D12RHI")
			{
				if (Target.WindowsPlatform.bPixProfilingEnabled && Target.Platform == UnrealTargetPlatform.Win64 && Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Configuration != UnrealTargetConfiguration.Test)
				{
					// Define to indicate profiling enabled (64-bit only)
					Rules.Definitions.Add("D3D12_PROFILING_ENABLED=1");
					Rules.Definitions.Add("PROFILE");
				}
				else
				{
					Rules.Definitions.Add("D3D12_PROFILING_ENABLED=0");
				}

				// To enable platform specific D3D12 RHI Types
				Rules.PrivateIncludePaths.Add("Runtime/Windows/D3D12RHI/Private/Windows");
			}

			// Delay-load D3D12 so we can use the latest features and still run on downlevel versions of the OS
			Rules.PublicDelayLoadDLLs.Add("d3d12.dll");
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="Target">Settings for the target being compiled</param>
		/// <param name="CompileEnvironment">The compile environment for this target</param>
		/// <param name="LinkEnvironment">The link environment for this target</param>
		public override void SetUpEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{
			CompileEnvironment.Definitions.Add("WIN32=1");

			if (WindowsPlatform.bUseWindowsSDK10)
			{
				// Windows 8 or higher required
				CompileEnvironment.Definitions.Add("_WIN32_WINNT=0x0602");
				CompileEnvironment.Definitions.Add("WINVER=0x0602");
			}
			else
			{
				// Windows 7 or higher required
				CompileEnvironment.Definitions.Add("_WIN32_WINNT=0x0601");
				CompileEnvironment.Definitions.Add("WINVER=0x0601");
			}
			CompileEnvironment.Definitions.Add("PLATFORM_WINDOWS=1");

			CompileEnvironment.Definitions.Add("DEPTH_32_BIT_CONVERSION=0");

			FileReference MorpheusShaderPath = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Shaders", "Private", "PS4", "PostProcessHMDMorpheus.usf");
			if (FileReference.Exists(MorpheusShaderPath))
			{
				CompileEnvironment.Definitions.Add("HAS_MORPHEUS=1");

				//on PS4 the SDK now handles distortion correction.  On PC we will still have to handle it manually,				
				CompileEnvironment.Definitions.Add("MORPHEUS_ENGINE_DISTORTION=1");
			}

			// Add path to Intel math libraries when using ICL based on target platform
			if (WindowsPlatform.bCompileWithICL)
			{
				var Result = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "IntelSWTools", "compilers_and_libraries", "windows", "compiler", "lib", Target.Platform == UnrealTargetPlatform.Win32 ? "ia32" : "intel64");
				if (!Directory.Exists(Result))
				{
					throw new BuildException("ICL was selected but the required math libraries were not found.  Could not find: " + Result);
				}

				LinkEnvironment.LibraryPaths.Add(Result);
			}

			// Explicitly exclude the MS C++ runtime libraries we're not using, to ensure other libraries we link with use the same
			// runtime library as the engine.
			bool bUseDebugCRT = Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT;
			if (!Target.bUseStaticCRT || bUseDebugCRT)
			{
				LinkEnvironment.ExcludedLibraries.Add("LIBCMT");
				LinkEnvironment.ExcludedLibraries.Add("LIBCPMT");
			}
			if (!Target.bUseStaticCRT || !bUseDebugCRT)
			{
				LinkEnvironment.ExcludedLibraries.Add("LIBCMTD");
				LinkEnvironment.ExcludedLibraries.Add("LIBCPMTD");
			}
			if (Target.bUseStaticCRT || bUseDebugCRT)
			{
				LinkEnvironment.ExcludedLibraries.Add("MSVCRT");
				LinkEnvironment.ExcludedLibraries.Add("MSVCPRT");
			}
			if (Target.bUseStaticCRT || !bUseDebugCRT)
			{
				LinkEnvironment.ExcludedLibraries.Add("MSVCRTD");
				LinkEnvironment.ExcludedLibraries.Add("MSVCPRTD");
			}
			LinkEnvironment.ExcludedLibraries.Add("LIBC");
			LinkEnvironment.ExcludedLibraries.Add("LIBCP");
			LinkEnvironment.ExcludedLibraries.Add("LIBCD");
			LinkEnvironment.ExcludedLibraries.Add("LIBCPD");

			//@todo ATL: Currently, only VSAccessor requires ATL (which is only used in editor builds)
			// When compiling games, we do not want to include ATL - and we can't when compiling games
			// made with Launcher build due to VS 2012 Express not including ATL.
			// If more modules end up requiring ATL, this should be refactored into a BuildTarget flag (bNeedsATL)
			// that is set by the modules the target includes to allow for easier tracking.
			// Alternatively, if VSAccessor is modified to not require ATL than we should always exclude the libraries.
			if (Target.LinkType == TargetLinkType.Monolithic &&
				(Target.Type == TargetType.Game || Target.Type == TargetType.Client || Target.Type == TargetType.Server))
			{
				LinkEnvironment.ExcludedLibraries.Add("atl");
				LinkEnvironment.ExcludedLibraries.Add("atls");
				LinkEnvironment.ExcludedLibraries.Add("atlsd");
				LinkEnvironment.ExcludedLibraries.Add("atlsn");
				LinkEnvironment.ExcludedLibraries.Add("atlsnd");
			}

			// Add the library used for the delayed loading of DLLs.
			LinkEnvironment.AdditionalLibraries.Add("delayimp.lib");

			//@todo - remove once FB implementation uses Http module
			if (Target.bCompileAgainstEngine)
			{
				// link against wininet (used by FBX and Facebook)
				LinkEnvironment.AdditionalLibraries.Add("wininet.lib");
			}

			// Compile and link with Win32 API libraries.
			LinkEnvironment.AdditionalLibraries.Add("rpcrt4.lib");
			//LinkEnvironment.AdditionalLibraries.Add("wsock32.lib");
			LinkEnvironment.AdditionalLibraries.Add("ws2_32.lib");
			LinkEnvironment.AdditionalLibraries.Add("dbghelp.lib");
			LinkEnvironment.AdditionalLibraries.Add("comctl32.lib");
			LinkEnvironment.AdditionalLibraries.Add("Winmm.lib");
			LinkEnvironment.AdditionalLibraries.Add("kernel32.lib");
			LinkEnvironment.AdditionalLibraries.Add("user32.lib");
			LinkEnvironment.AdditionalLibraries.Add("gdi32.lib");
			LinkEnvironment.AdditionalLibraries.Add("winspool.lib");
			LinkEnvironment.AdditionalLibraries.Add("comdlg32.lib");
			LinkEnvironment.AdditionalLibraries.Add("advapi32.lib");
			LinkEnvironment.AdditionalLibraries.Add("shell32.lib");
			LinkEnvironment.AdditionalLibraries.Add("ole32.lib");
			LinkEnvironment.AdditionalLibraries.Add("oleaut32.lib");
			LinkEnvironment.AdditionalLibraries.Add("uuid.lib");
			LinkEnvironment.AdditionalLibraries.Add("odbc32.lib");
			LinkEnvironment.AdditionalLibraries.Add("odbccp32.lib");
			LinkEnvironment.AdditionalLibraries.Add("netapi32.lib");
			LinkEnvironment.AdditionalLibraries.Add("iphlpapi.lib");
			LinkEnvironment.AdditionalLibraries.Add("setupapi.lib"); //  Required for access monitor device enumeration

			// Windows Vista/7 Desktop Windows Manager API for Slate Windows Compliance
			LinkEnvironment.AdditionalLibraries.Add("dwmapi.lib");

			// IME
			LinkEnvironment.AdditionalLibraries.Add("imm32.lib");

			// For 64-bit builds, we'll forcibly ignore a linker warning with DirectInput.  This is
			// Microsoft's recommended solution as they don't have a fixed .lib for us.
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				LinkEnvironment.AdditionalArguments += " /ignore:4078";
			}

			if (Target.Type != TargetType.Editor)
			{
				if (!string.IsNullOrEmpty(Target.WindowsPlatform.CompanyName))
				{
					CompileEnvironment.Definitions.Add(String.Format("PROJECT_COMPANY_NAME={0}", SanitizeMacroValue(Target.WindowsPlatform.CompanyName)));
				}

				if (!string.IsNullOrEmpty(Target.WindowsPlatform.CopyrightNotice))
				{
					CompileEnvironment.Definitions.Add(String.Format("PROJECT_COPYRIGHT_STRING={0}", SanitizeMacroValue(Target.WindowsPlatform.CopyrightNotice)));
				}

				if (!string.IsNullOrEmpty(Target.WindowsPlatform.ProductName))
				{
					CompileEnvironment.Definitions.Add(String.Format("PROJECT_PRODUCT_NAME={0}", SanitizeMacroValue(Target.WindowsPlatform.ProductName)));
				}

				if (Target.ProjectFile != null)
				{
					CompileEnvironment.Definitions.Add(String.Format("PROJECT_PRODUCT_IDENTIFIER={0}", SanitizeMacroValue(Target.ProjectFile.GetFileNameWithoutExtension())));
				}
			}

			// Set up default stack size
			ConfigHierarchy EngineIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Target.ProjectFile), UnrealTargetPlatform.Win64);
			String TargetSettingsIniPath = "/Script/WindowsTargetPlatform.WindowsTargetSettings";
			int IniDefaultStackSize = 0;
			String StackSizeName = Target.Type == TargetType.Editor ? "DefaultStackSizeEditor" : "DefaultStackSize";
			if (EngineIni.GetInt32(TargetSettingsIniPath, StackSizeName, out IniDefaultStackSize))
			{
				LinkEnvironment.DefaultStackSize = IniDefaultStackSize;
			}

			int IniDefaultStackSizeCommit = 0;
			String StackSizeCommitName = Target.Type == TargetType.Editor ? "DefaultStackSizeCommitEditor" : "DefaultStackSizeCommit";
			if (EngineIni.GetInt32(TargetSettingsIniPath, StackSizeCommitName, out IniDefaultStackSizeCommit))
			{
				LinkEnvironment.DefaultStackSizeCommit = IniDefaultStackSizeCommit;
			}

			LinkEnvironment.ModuleDefinitionFile = Target.WindowsPlatform.ModuleDefinitionFile;
		}

		/// <summary>
		/// Macros passed via the command line have their quotes stripped, and are tokenized before being re-stringized by the compiler. This conversion
		/// back and forth is normally ok, but certain characters such as single quotes must always be paired. Remove any such characters here.
		/// </summary>
		/// <param name="Value">The macro value</param>
		/// <returns>The sanitized value</returns>
		static string SanitizeMacroValue(string Value)
		{
			StringBuilder Result = new StringBuilder(Value.Length);
			for(int Idx = 0; Idx < Value.Length; Idx++)
			{
				if(Value[Idx] != '\'' && Value[Idx] != '\"')
				{
					Result.Append(Value[Idx]);
				}
			}
			return Result.ToString();
		}

		/// <summary>
		/// Setup the configuration environment for building
		/// </summary>
		/// <param name="Target"> The target being built</param>
		/// <param name="GlobalCompileEnvironment">The global compile environment</param>
		/// <param name="GlobalLinkEnvironment">The global link environment</param>
		public override void SetUpConfigurationEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment GlobalCompileEnvironment, LinkEnvironment GlobalLinkEnvironment)
		{
			if (GlobalCompileEnvironment.bUseDebugCRT)
			{
				GlobalCompileEnvironment.Definitions.Add("_DEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("NDEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
			}

			UnrealTargetConfiguration CheckConfig = Target.Configuration;
			switch (CheckConfig)
			{
				default:
				case UnrealTargetConfiguration.Debug:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_DEBUG=1");
					break;
				case UnrealTargetConfiguration.DebugGame:
				// Default to Development; can be overridden by individual modules.
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

			// NOTE: Even when debug info is turned off, we currently force the linker to generate debug info
			//       anyway on Visual C++ platforms.  This will cause a PDB file to be generated with symbols
			//       for most of the classes and function/method names, so that crashes still yield somewhat
			//       useful call stacks, even though compiler-generate debug info may be disabled.  This gives
			//       us much of the build-time savings of fully-disabled debug info, without giving up call
			//       data completely.
			GlobalLinkEnvironment.bCreateDebugInfo = true;
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
					return !Target.bOmitPCDebugInfoInDevelopment;
				case UnrealTargetConfiguration.DebugGame:
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
			if (Target.WindowsPlatform.StaticAnalyzer == WindowsStaticAnalyzer.PVSStudio)
			{
				return new PVSToolChain(CppPlatform, Target.WindowsPlatform.Compiler);
			}
			else if(Target.WindowsPlatform.StaticAnalyzer == WindowsStaticAnalyzer.VisualCpp)
			{
				return new VCToolChain(CppPlatform, Target.WindowsPlatform.Compiler, true, Target.WindowsPlatform.bStrictConformanceMode, Target.WindowsPlatform.ObjSrcMapFile);
			}
			else
			{
				return new VCToolChain(CppPlatform, Target.WindowsPlatform.Compiler, false, Target.WindowsPlatform.bStrictConformanceMode, Target.WindowsPlatform.ObjSrcMapFile);
			}
		}

		/// <summary>
		/// Deploys the given target
		/// </summary>
		/// <param name="Target">Information about the target being deployed</param>
		public override void Deploy(UEBuildDeployTarget Target)
		{
			new BaseWindowsDeploy().PrepTargetForDeployment(Target);
		}
	}

	class WindowsPlatformSDK : UEBuildPlatformSDK
	{
		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			return SDKStatus.Valid;
		}
	}

	class WindowsPlatformFactory : UEBuildPlatformFactory
	{
		protected override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.Win64; }
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		protected override void RegisterBuildPlatforms(SDKOutputLevel OutputLevel)
		{
			WindowsPlatformSDK SDK = new WindowsPlatformSDK();
			SDK.ManageAndValidateSDK(OutputLevel);

			// Register this build platform for both Win64 and Win32
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Win64.ToString());
			UEBuildPlatform.RegisterBuildPlatform(new WindowsPlatform(UnrealTargetPlatform.Win64, CppPlatform.Win64, SDK));
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Win64, UnrealPlatformGroup.Windows);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Win64, UnrealPlatformGroup.Microsoft);

			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Win32.ToString());
			UEBuildPlatform.RegisterBuildPlatform(new WindowsPlatform(UnrealTargetPlatform.Win32, CppPlatform.Win32, SDK));
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Win32, UnrealPlatformGroup.Windows);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Win32, UnrealPlatformGroup.Microsoft);
		}
	}
}
