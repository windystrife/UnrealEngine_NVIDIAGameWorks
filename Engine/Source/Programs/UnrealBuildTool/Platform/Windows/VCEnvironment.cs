// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Microsoft.Win32;
using System.Text;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Stores information about a Visual C++ installation and compile environment
	/// </summary>
	class VCEnvironment
	{
		/// <summary>
		/// The platform the envvars have been initialized for
		/// </summary>
		public readonly CppPlatform Platform;

		/// <summary>
		/// The compiler version we're using
		/// </summary>
		public readonly WindowsCompiler Compiler;

		/// <summary>
		/// The path to the base Visual Studio install directory (may be null for standalone toolchain)
		/// </summary>
		public readonly DirectoryReference VSInstallDir;
		
		/// <summary>
		/// The path to the base Visual C++ install directory
		/// </summary>
		public readonly DirectoryReference VCInstallDir;

		/// <summary>
		/// The path to the 32bit platform tool binaries.
		/// </summary>
		public readonly DirectoryReference VCToolPath32;

		/// <summary>
		/// The path to the 64bit platform tool binaries.
		/// </summary>
		public readonly DirectoryReference VCToolPath64;

		/// <summary>
		/// Installation folder of the Windows SDK, e.g. C:\Program Files\Microsoft SDKs\Windows\v6.0A\
		/// </summary>
		public readonly string WindowsSDKDir;
		
		/// <summary>
		/// 
		/// </summary>
		public readonly string WindowsSDKExtensionDir;

		/// <summary>
		/// Installation folder of the Windows SDK Extensions, e.g. C:\Program Files (x86)\Windows SDKs\10
		/// </summary>
		public readonly string WindowsSDKLibVersion;

		/// <summary>
		/// Installation folder of the NetFx SDK, since that is split out from platform SDKs >= v10
		/// </summary>
		public readonly string NetFxSDKExtensionDir;

		/// <summary>
		/// // 10.0.9910.0 for instance...
		/// </summary>
		public readonly Version WindowsSDKExtensionHeaderLibVersion;

		/// <summary>
		/// The path to the linker for linking executables
		/// </summary>
		public readonly FileReference CompilerPath;

		/// <summary>
		/// The version of cl.exe we're running
		/// </summary>
		public readonly Version CLExeVersion;

		/// <summary>
		/// The path to the linker for linking executables
		/// </summary>
		public readonly FileReference LinkerPath;

		/// <summary>
		/// The path to the linker for linking libraries
		/// </summary>
		public readonly FileReference LibraryManagerPath;

		/// <summary>
		/// The path to the resource compiler
		/// </summary>
		public readonly FileReference ResourceCompilerPath;

		/// <summary>
		/// For Visual Studio 2015; the path to the universal CRT.
		/// </summary>
		public readonly string UniversalCRTDir;

		/// <summary>
		/// For Visual Studio 2015; the universal CRT version to use.
		/// </summary>
		public readonly string UniversalCRTVersion;

		static readonly string InitialIncludePaths = Environment.GetEnvironmentVariable("INCLUDE");
		static readonly string InitialLibraryPaths = Environment.GetEnvironmentVariable("LIB");

		private string _MSBuildPath = null;

		/// <summary>
		/// 
		/// </summary>
		public string MSBuildPath // The path to MSBuild
		{
			get
			{
				if (_MSBuildPath == null)
				{
					_MSBuildPath = GetMSBuildToolPath();
				}
				return _MSBuildPath;
			}
		}

		/// <summary>
		/// Initializes environment variables required by toolchain. Different for 32 and 64 bit.
		/// </summary>
		public static VCEnvironment SetEnvironment(CppPlatform Platform, WindowsCompiler Compiler)
		{
			if (EnvVars != null && EnvVars.Platform == Platform && EnvVars.Compiler == Compiler)
			{
				return EnvVars;
			}

			EnvVars = new VCEnvironment(Platform, Compiler);
			return EnvVars;
		}

		private VCEnvironment(CppPlatform InPlatform, WindowsCompiler InCompiler)
		{
			Platform = InPlatform;
			Compiler = InCompiler;

			// Get the Visual Studio install directory
			WindowsPlatform.TryGetVSInstallDir(Compiler, out VSInstallDir);

			// Get the Visual C++ compiler install directory. 
			if(!WindowsPlatform.TryGetVCInstallDir(Compiler, out VCInstallDir))
			{
				throw new BuildException(WindowsPlatform.GetCompilerName(Compiler) + " must be installed in order to build this target.");
			}

			// Figure out the default toolchain directory. VS15 separates this out into separate directories, with platforms as subdirectories under that.
			DirectoryReference VCToolChainDir = null;
			if(Compiler == WindowsCompiler.VisualStudio2017)
			{
				string Version = File.ReadAllText(FileReference.Combine(VCInstallDir, "Auxiliary", "Build", "Microsoft.VCToolsVersion.default.txt").FullName).Trim();
				VCToolChainDir = DirectoryReference.Combine(VCInstallDir, "Tools", "MSVC", Version);
            }

            WindowsSDKDir = FindWindowsSDKInstallationFolder(Platform, Compiler);
			WindowsSDKLibVersion = FindWindowsSDKLibVersion(WindowsSDKDir);
			WindowsSDKExtensionDir = FindWindowsSDKExtensionInstallationFolder(Compiler);
			NetFxSDKExtensionDir = FindNetFxSDKExtensionInstallationFolder(Compiler);
			WindowsSDKExtensionHeaderLibVersion = FindWindowsSDKExtensionLatestVersion(WindowsSDKExtensionDir);
			FindUniversalCRT(Compiler, out UniversalCRTDir, out UniversalCRTVersion);

			VCToolPath32 = GetVCToolPath32(Compiler, VCInstallDir, VCToolChainDir);
			VCToolPath64 = GetVCToolPath64(Compiler, VCInstallDir, VCToolChainDir);

            // Compile using 64 bit tools for 64 bit targets, and 32 for 32.
            DirectoryReference CompilerDir = (Platform == CppPlatform.Win64) ? VCToolPath64 : VCToolPath32;

			// Regardless of the target, if we're linking on a 64 bit machine, we want to use the 64 bit linker (it's faster than the 32 bit linker and can handle large linking jobs)
			DirectoryReference LinkerDir = VCToolPath64;

			CompilerPath = GetCompilerToolPath(InPlatform, CompilerDir);
			CLExeVersion = FindCLExeVersion(CompilerPath.FullName);
			LinkerPath = GetLinkerToolPath(InPlatform, LinkerDir);
			LibraryManagerPath = GetLibraryLinkerToolPath(InPlatform, LinkerDir);
			ResourceCompilerPath = new FileReference(GetResourceCompilerToolPath(Platform));

            // Make sure the base 32-bit VS tool path is in the PATH, regardless of which configuration we're using. The toolchain may need to reference support DLLs from this directory (eg. mspdb120.dll).
            string PathEnvironmentVariable = Environment.GetEnvironmentVariable("PATH") ?? "";
            if (!PathEnvironmentVariable.Split(';').Any(x => String.Compare(x, VCToolPath32.FullName, true) == 0))
            {
                PathEnvironmentVariable = VCToolPath32.FullName + ";" + PathEnvironmentVariable;
                Environment.SetEnvironmentVariable("PATH", PathEnvironmentVariable);
            }

			// Setup the INCLUDE environment variable
			List<string> IncludePaths = GetVisualCppIncludePaths(Compiler, VCInstallDir, VCToolChainDir, UniversalCRTDir, UniversalCRTVersion, NetFxSDKExtensionDir, WindowsSDKDir, WindowsSDKLibVersion);
			if(InitialIncludePaths != null)
			{
				IncludePaths.Add(InitialIncludePaths);
			}
            Environment.SetEnvironmentVariable("INCLUDE", String.Join(";", IncludePaths));
			
			// Setup the LIB environment variable
            List<string> LibraryPaths = GetVisualCppLibraryPaths(Compiler, VCInstallDir, VCToolChainDir, UniversalCRTDir, UniversalCRTVersion, NetFxSDKExtensionDir, WindowsSDKDir, WindowsSDKLibVersion, Platform);
			if(InitialLibraryPaths != null)
			{
				LibraryPaths.Add(InitialLibraryPaths);
			}
            Environment.SetEnvironmentVariable("LIB", String.Join(";", LibraryPaths));
		}

		/// <returns>The path to Windows SDK directory for the specified version.</returns>
		private static string FindWindowsSDKInstallationFolder(CppPlatform InPlatform, WindowsCompiler InCompiler)
		{
			// When targeting Windows XP on Visual Studio 2012+, we need to point at the older Windows SDK 7.1A that comes
			// installed with Visual Studio 2012 Update 1. (http://blogs.msdn.com/b/vcblog/archive/2012/10/08/10357555.aspx)
			string Version;
			switch (InCompiler)
			{
				case WindowsCompiler.VisualStudio2017:
				case WindowsCompiler.VisualStudio2015:
					if (WindowsPlatform.bUseWindowsSDK10)
					{
						Version = "v10.0";
					}
					else
					{
						Version = "v8.1";
					}
					break;

				default:
					throw new BuildException("Unexpected compiler setting when trying to determine Windows SDK folder");
			}

			// Based on VCVarsQueryRegistry
			string FinalResult = null;
			foreach (string IndividualVersion in Version.Split('|'))
			{
				object Result = Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Microsoft\Microsoft SDKs\Windows\" + IndividualVersion, "InstallationFolder", null)
					?? Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\" + IndividualVersion, "InstallationFolder", null)
					?? Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\" + IndividualVersion, "InstallationFolder", null);

				if (Result != null)
				{
					FinalResult = (string)Result;
					break;
				}
			}
			if (FinalResult == null)
			{
				throw new BuildException("Windows SDK {0} must be installed in order to build this target.", Version);
			}

			return FinalResult;
		}

		/// <summary>
		/// Gets the version of the Windows SDK libraries to use. As per VCVarsQueryRegistry.bat, this is the directory name that sorts last.
		/// </summary>
		static string FindWindowsSDKLibVersion(string WindowsSDKDir)
		{
			string WindowsSDKLibVersion;
			if (WindowsPlatform.bUseWindowsSDK10)
			{
				DirectoryInfo IncludeDir = new DirectoryInfo(Path.Combine(WindowsSDKDir, "include"));
				if (!IncludeDir.Exists)
				{
					throw new BuildException("Couldn't find Windows 10 SDK include directory ({0})", IncludeDir.FullName);
				}

				DirectoryInfo LatestIncludeDir = IncludeDir.EnumerateDirectories().OrderBy(x => x.Name).LastOrDefault();
				if (LatestIncludeDir == null)
				{
					throw new BuildException("No Windows 10 SDK versions found under ({0})", IncludeDir.FullName);
				}

				WindowsSDKLibVersion = LatestIncludeDir.Name;
			}
			else
			{
				WindowsSDKLibVersion = "winv6.3";
			}
			return WindowsSDKLibVersion;
		}

		private static string FindNetFxSDKExtensionInstallationFolder(WindowsCompiler Compiler)
		{
			string[] Versions;
			switch (Compiler)
			{
				case WindowsCompiler.VisualStudio2017:
				case WindowsCompiler.VisualStudio2015:
					Versions = new string[] { "4.6", "4.6.1", "4.6.2" };
					break;

				default:
					return string.Empty;
			}

			foreach (string Version in Versions)
			{
				string Result = Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Microsoft SDKs\NETFXSDK\" + Version, "KitsInstallationFolder", null) as string
							?? Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\NETFXSDK\" + Version, "KitsInstallationFolder", null) as string;
				if (Result != null)
				{
					return Result.TrimEnd('\\');
				}
			}

			return string.Empty;
		}

		private static string FindWindowsSDKExtensionInstallationFolder(WindowsCompiler Compiler)
		{
			string Version;
			switch (Compiler)
			{
				case WindowsCompiler.VisualStudio2017:
				case WindowsCompiler.VisualStudio2015:
					if (WindowsPlatform.bUseWindowsSDK10)
					{
						Version = "v10.0";
					}
					else
					{
						return string.Empty;
					}
					break;

				default:
					return string.Empty;
			}

			// Based on VCVarsQueryRegistry
			string FinalResult = null;
			{
				object Result = Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows SDKs\" + Version, "InstallationFolder", null)
						  ?? Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows SDKs\" + Version, "InstallationFolder", null);
				if (Result == null)
				{
					Result = Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\" + Version, "InstallationFolder", null)
						  ?? Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\" + Version, "InstallationFolder", null);
				}
				if (Result != null)
				{
					FinalResult = ((string)Result).TrimEnd('\\');
				}

			}
			if (FinalResult == null)
			{
				FinalResult = string.Empty;
			}

			return FinalResult;
		}

		static Version FindWindowsSDKExtensionLatestVersion(string WindowsSDKExtensionDir)
		{
			Version LatestVersion = new Version(0, 0, 0, 0);

			if (WindowsPlatform.bUseWindowsSDK10 &&
				!string.IsNullOrEmpty(WindowsSDKExtensionDir) &&
				Directory.Exists(WindowsSDKExtensionDir))
			{
				string IncludesBaseDirectory = Path.Combine(WindowsSDKExtensionDir, "include");
				if (Directory.Exists(IncludesBaseDirectory))
				{
					string[] IncludeVersions = Directory.GetDirectories(IncludesBaseDirectory);
					foreach (string IncludeVersion in IncludeVersions)
					{
						string VersionString = Path.GetFileName(IncludeVersion);
						Version FoundVersion;
						if (Version.TryParse(VersionString, out FoundVersion) && FoundVersion > LatestVersion)
						{
							LatestVersion = FoundVersion;
						}
					}
				}
			}
			return LatestVersion;
		}

		/// <summary>
		/// Gets the path to the 32bit tool binaries.
		/// </summary>
		/// <param name="Compiler">The compiler version</param>
		/// <param name="VCInstallDir">Base install directory for the VC toolchain</param>
		/// <param name="VCToolChainDir">Base directory for the VC toolchain</param>
		/// <returns>Directory containing the 32-bit toolchain binaries</returns>
		static DirectoryReference GetVCToolPath32(WindowsCompiler Compiler, DirectoryReference VCInstallDir, DirectoryReference VCToolChainDir)
		{
			if (Compiler == WindowsCompiler.VisualStudio2017)
			{
				FileReference NativeCompilerPath = FileReference.Combine(VCToolChainDir, "bin", "HostX64", "x86", "cl.exe");
				if (FileReference.Exists(NativeCompilerPath))
				{
					return NativeCompilerPath.Directory;
				}

				FileReference CrossCompilerPath = FileReference.Combine(VCToolChainDir, "bin", "HostX86", "x86", "cl.exe");
				if (FileReference.Exists(CrossCompilerPath))
				{
					return CrossCompilerPath.Directory;
				}

				throw new BuildException("No 32-bit compiler toolchain found in {0} or {1}", NativeCompilerPath, CrossCompilerPath);
			}
			else
			{
				FileReference CompilerPath = FileReference.Combine(VCInstallDir, "bin", "cl.exe");
				if (FileReference.Exists(CompilerPath))
				{
					return CompilerPath.Directory;
				}
				throw new BuildException("No 32-bit compiler toolchain found in {0}", CompilerPath);
			}
		}

		/// <summary>
		/// Gets the path to the 64bit tool binaries.
		/// </summary>
		/// <param name="Compiler">The version of the compiler being used</param>
		/// <param name="VCInstallDir">Base install directory for the VC toolchain</param>
		/// <param name="VCToolChainDir">Base directory for the VC toolchain</param>
		/// <returns>Directory containing the 64-bit toolchain binaries</returns>
		static DirectoryReference GetVCToolPath64(WindowsCompiler Compiler, DirectoryReference VCInstallDir, DirectoryReference VCToolChainDir)
		{
			if (Compiler == WindowsCompiler.VisualStudio2017)
			{
				// Use the native 64-bit compiler if present
				FileReference NativeCompilerPath = FileReference.Combine(VCToolChainDir, "bin", "HostX64", "x64", "cl.exe");
				if (FileReference.Exists(NativeCompilerPath))
				{
					return NativeCompilerPath.Directory;
				}

				// Otherwise try the x64-on-x86 compiler. VS Express only includes the latter.
				FileReference CrossCompilerPath = FileReference.Combine(VCToolChainDir, "bin", "HostX86", "x64", "cl.exe");
				if (FileReference.Exists(CrossCompilerPath))
				{
					return CrossCompilerPath.Directory;
				}

				throw new BuildException("No 64-bit compiler toolchain found in {0} or {1}", NativeCompilerPath, CrossCompilerPath);
			}
			else
			{
				// Use the native 64-bit compiler if present
				FileReference NativeCompilerPath = FileReference.Combine(VCInstallDir, "bin", "amd64", "cl.exe");
				if (FileReference.Exists(NativeCompilerPath))
				{
					return NativeCompilerPath.Directory;
				}

				// Otherwise use the amd64-on-x86 compiler. VS2012 Express only includes the latter.
				FileReference CrossCompilerPath = FileReference.Combine(VCInstallDir, "bin", "x86_amd64", "cl.exe");
				if (FileReference.Exists(CrossCompilerPath))
				{
					return CrossCompilerPath.Directory;
				}

				throw new BuildException("No 64-bit compiler toolchain found in {0} or {1}", NativeCompilerPath, CrossCompilerPath);
			}
		}

		/// <summary>
		/// Gets the path to the compiler.
		/// </summary>
		static FileReference GetCompilerToolPath(CppPlatform Platform, DirectoryReference PlatformVSToolPath)
		{
			// If we were asked to use Clang, then we'll redirect the path to the compiler to the LLVM installation directory
			if (WindowsPlatform.bCompileWithClang)
			{
				string CompilerDriverName;
				string Result;
				if (WindowsPlatform.bUseVCCompilerArgs)
				{
					// Use 'clang-cl', a wrapper around Clang that supports Visual C++ compiler command-line arguments
					CompilerDriverName = "clang-cl.exe";
				}
				else
				{
					// Use regular Clang compiler on Windows
					CompilerDriverName = "clang.exe";
				}

				Result = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "LLVM", "bin", CompilerDriverName);
				if (!File.Exists(Result))
				{
					Result = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "LLVM", "bin", CompilerDriverName);
				}

				if (!File.Exists(Result))
				{
					throw new BuildException("Clang was selected as the Windows compiler, but LLVM/Clang does not appear to be installed.  Could not find: " + Result);
				}

				return new FileReference(Result);
			}

			if (WindowsPlatform.bCompileWithICL)
			{
				var Result = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "IntelSWTools", "compilers_and_libraries", "windows", "bin", Platform == CppPlatform.Win32 ? "ia32" : "intel64", "icl.exe");
				if (!File.Exists(Result))
				{
					throw new BuildException("ICL was selected as the Windows compiler, but does not appear to be installed.  Could not find: " + Result);
				}

				return new FileReference(Result);
			}

			return FileReference.Combine(PlatformVSToolPath, "cl.exe");
		}

		/// <returns>The version of the compiler.</returns>
		private static Version FindCLExeVersion(string CompilerExe)
		{
			// Check that the cl.exe exists (GetVersionInfo doesn't handle this well).
			if (!File.Exists(CompilerExe))
			{
				// By default VS2015 doesn't install the C++ toolchain. Help developers out with a special message.
				throw new BuildException("Failed to find cl.exe in the default toolchain directory " + CompilerExe + ". Please verify that \"Common Tools for Visual C++ 2015\" was selected when installing Visual Studio 2015.");
			}

			FileVersionInfo ExeVersionInfo = FileVersionInfo.GetVersionInfo(CompilerExe);
			if (ExeVersionInfo == null)
			{
				throw new BuildException("Failed to read the version number of: " + CompilerExe);
			}

			return new Version(ExeVersionInfo.FileMajorPart, ExeVersionInfo.FileMinorPart, ExeVersionInfo.FileBuildPart, ExeVersionInfo.FilePrivatePart);
		}

		/// <summary>
		/// Gets the path to the linker.
		/// </summary>
		static FileReference GetLinkerToolPath(CppPlatform Platform, DirectoryReference PlatformVSToolPath)
		{
			// If we were asked to use Clang, then we'll redirect the path to the compiler to the LLVM installation directory
			if (WindowsPlatform.bCompileWithClang && WindowsPlatform.bAllowClangLinker)
			{
				string Result = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "LLVM", "bin", "lld.exe");
				if (!File.Exists(Result))
				{
					Result = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "LLVM", "bin", "lld.exe");
				}
				if (!File.Exists(Result))
				{
					throw new BuildException("Clang was selected as the Windows compiler, but LLVM/Clang does not appear to be installed.  Could not find: " + Result);
				}

				return new FileReference(Result);
			}

			if (WindowsPlatform.bCompileWithICL && WindowsPlatform.bAllowICLLinker)
			{
				var Result = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "IntelSWTools", "compilers_and_libraries", "windows", "bin", Platform == CppPlatform.Win32 ? "ia32" : "intel64", "xilink.exe");
				if (!File.Exists(Result))
				{
					throw new BuildException("ICL was selected as the Windows compiler, but does not appear to be installed.  Could not find: " + Result);
				}

				return new FileReference(Result);
			}

			return FileReference.Combine(PlatformVSToolPath, "link.exe");
		}

		/// <summary>
		/// Gets the path to the library linker.
		/// </summary>
		static FileReference GetLibraryLinkerToolPath(CppPlatform Platform, DirectoryReference PlatformVSToolPath)
		{
			// Regardless of the target, if we're linking on a 64 bit machine, we want to use the 64 bit linker (it's faster than the 32 bit linker)
			//@todo.WIN32: Using the 64-bit linker appears to be broken at the moment.

			if (WindowsPlatform.bCompileWithICL && WindowsPlatform.bAllowICLLinker)
			{
				var Result = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "IntelSWTools", "compilers_and_libraries", "windows", "bin", Platform == CppPlatform.Win32 ? "ia32" : "intel64", "xilib.exe");
				if (!File.Exists(Result))
				{
					throw new BuildException("ICL was selected as the Windows compiler, but does not appear to be installed.  Could not find: " + Result);
				}

				return new FileReference(Result);
			}

			return FileReference.Combine(PlatformVSToolPath, "lib.exe");
		}

		/// <summary>
		/// Gets the path to the resource compiler's rc.exe for the specified platform.
		/// </summary>
		string GetResourceCompilerToolPath(CppPlatform Platform)
		{
			// 64 bit -- we can use the 32 bit version to target 64 bit on 32 bit OS.
			if (Platform == CppPlatform.Win64)
			{
				if (WindowsPlatform.bUseWindowsSDK10)
				{
					return Path.Combine(WindowsSDKExtensionDir, "bin/x64/rc.exe");
				}
				else
				{
					return Path.Combine(WindowsSDKDir, "bin/x64/rc.exe");
				}
			}
			else
			{
				if (WindowsPlatform.bUseWindowsSDK10)
				{
					return Path.Combine(WindowsSDKExtensionDir, "bin/x86/rc.exe");
				}
				else
				{
					return Path.Combine(WindowsSDKDir, "bin/x86/rc.exe");
				}
			}
		}

		/// <summary>
		/// Gets the path to MSBuild.
		/// This mirrors the logic in GetMSBuildPath.bat.
		/// </summary>
		public static string GetMSBuildToolPath()
		{
			// Try to get the MSBuild 14.0 path directly (see https://msdn.microsoft.com/en-us/library/hh162058(v=vs.120).aspx)
			string ToolPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "MSBuild", "14.0", "bin", "MSBuild.exe");
			if(File.Exists(ToolPath))
			{
				return ToolPath;
			} 

			// Try to get the MSBuild 14.0 path from the registry
			if (TryReadMsBuildInstallPath("Microsoft\\MSBuild\\ToolsVersions\\14.0", "MSBuildToolsPath", "MSBuild.exe", out ToolPath))
			{
				return ToolPath;
			}

			// Check for MSBuild 15. This is installed alongside Visual Studio 2017, so we get the path relative to that.
			if (TryReadMsBuildInstallPath("Microsoft\\VisualStudio\\SxS\\VS7", "15.0", "MSBuild\\15.0\\bin\\MSBuild.exe", out ToolPath))
			{
				return ToolPath;
			}

			// Check for older versions of MSBuild. These are registered as separate versions in the registry.
			if (TryReadMsBuildInstallPath("Microsoft\\MSBuild\\ToolsVersions\\12.0", "MSBuildToolsPath", "MSBuild.exe", out ToolPath))
			{
				return ToolPath;
			}
			if (TryReadMsBuildInstallPath("Microsoft\\MSBuild\\ToolsVersions\\4.0", "MSBuildToolsPath", "MSBuild.exe", out ToolPath))
			{
				return ToolPath;
			}

			throw new BuildException("NOTE: Please ensure that 64bit Tools are installed with DevStudio - there is usually an option to install these during install");
		}

		/// <summary>
		/// Function to query the registry under HKCU/HKLM Win32/Wow64 software registry keys for a certain install directory.
		/// This mirrors the logic in GetMSBuildPath.bat.
		/// </summary>
		/// <returns></returns>
		static bool TryReadMsBuildInstallPath(string KeyRelativePath, string KeyName, string MsBuildRelativePath, out string OutMsBuildPath)
		{
			string[] KeyBasePaths =
			{
				@"HKEY_CURRENT_USER\SOFTWARE\",
				@"HKEY_LOCAL_MACHINE\SOFTWARE\",
				@"HKEY_CURRENT_USER\SOFTWARE\Wow6432Node\",
				@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\"
			};

			foreach (string KeyBasePath in KeyBasePaths)
			{
				string Value = Registry.GetValue(KeyBasePath + KeyRelativePath, KeyName, null) as string;
				if (Value != null)
				{
					string MsBuildPath = Path.Combine(Value, MsBuildRelativePath);
					if (File.Exists(MsBuildPath))
					{
						OutMsBuildPath = MsBuildPath;
						return true;
					}
				}
			}

			OutMsBuildPath = null;
			return false;
		}

		/// <summary>
		/// Gets the version of the Universal CRT to use. As per VCVarsQueryRegistry.bat, this is the directory name that sorts last.
		/// </summary>
		static void FindUniversalCRT(WindowsCompiler Compiler, out string UniversalCRTDir, out string UniversalCRTVersion)
		{
			if (Compiler < WindowsCompiler.VisualStudio2015)
			{
				UniversalCRTDir = null;
				UniversalCRTVersion = null;
				return;
			}

			string[] RootKeys =
			{
				"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots",
				"HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots",
				"HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows Kits\\Installed Roots",
				"HKEY_CURRENT_USER\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows Kits\\Installed Roots",
			};

			List<DirectoryInfo> IncludeDirs = new List<DirectoryInfo>();
			foreach(string RootKey in RootKeys)
			{
				string IncludeDirString = Registry.GetValue(RootKey, "KitsRoot10", null) as string;
				if(IncludeDirString != null)
				{
					DirectoryInfo IncludeDir = new DirectoryInfo(Path.Combine(IncludeDirString, "include"));
					if (IncludeDir.Exists)
					{
						IncludeDirs.AddRange(IncludeDir.EnumerateDirectories());
					}
				}
			}

			DirectoryInfo LatestIncludeDir = IncludeDirs.OrderBy(x => x.Name).LastOrDefault(n => n.Name.All(s => (s >= '0' && s <= '9') || s == '.') && Directory.Exists(n.FullName + "\\ucrt"));
			if(LatestIncludeDir == null)
			{
				UniversalCRTDir = null;
				UniversalCRTVersion = null;
			}
			else
			{
				UniversalCRTDir = LatestIncludeDir.Parent.Parent.FullName;
				UniversalCRTVersion = LatestIncludeDir.Name;
			}
		}

		/// <summary>
		/// Sets the Visual C++ INCLUDE environment variable
		/// </summary>
		static List<string> GetVisualCppIncludePaths(WindowsCompiler Compiler, DirectoryReference VisualCppDir, DirectoryReference VisualCppToolchainDir, string UniversalCRTDir, string UniversalCRTVersion, string NetFXSDKDir, string WindowsSDKDir, string WindowsSDKLibVersion)
		{
			List<string> IncludePaths = new List<string>();

			// Add the standard Visual C++ include paths
			if (Compiler == WindowsCompiler.VisualStudio2017)
			{
				DirectoryReference StdIncludeDir = DirectoryReference.Combine(VisualCppToolchainDir, "INCLUDE");
				if (DirectoryReference.Exists(StdIncludeDir))
				{
					IncludePaths.Add(StdIncludeDir.FullName);
				}
				DirectoryReference AtlMfcIncludeDir = DirectoryReference.Combine(VisualCppToolchainDir, "ATLMFC", "INCLUDE");
				if (DirectoryReference.Exists(AtlMfcIncludeDir))
				{
					IncludePaths.Add(AtlMfcIncludeDir.FullName);
				}
			}
			else
			{
			    DirectoryReference StdIncludeDir = DirectoryReference.Combine(VisualCppDir, "INCLUDE");
			    if (DirectoryReference.Exists(StdIncludeDir))
			    {
				    IncludePaths.Add(StdIncludeDir.FullName);
			    }
				DirectoryReference AtlMfcIncludeDir = DirectoryReference.Combine(VisualCppDir, "ATLMFC", "INCLUDE");
			    if (DirectoryReference.Exists(AtlMfcIncludeDir))
			    {
				    IncludePaths.Add(AtlMfcIncludeDir.FullName);
			    }
			}

			// Add the universal CRT paths
			if (!String.IsNullOrEmpty(UniversalCRTDir) && !String.IsNullOrEmpty(UniversalCRTVersion))
			{
				IncludePaths.Add(Path.Combine(UniversalCRTDir, "include", UniversalCRTVersion, "ucrt"));
			}

			// Add the NETFXSDK include path
			if (!String.IsNullOrEmpty(NetFXSDKDir))
			{
				IncludePaths.Add(Path.Combine(NetFXSDKDir, "include", "um")); // 2015
			}

			// Add the Windows SDK paths
			if (Compiler >= WindowsCompiler.VisualStudio2015 && WindowsPlatform.bUseWindowsSDK10)
			{
				IncludePaths.Add(Path.Combine(WindowsSDKDir, "include", WindowsSDKLibVersion, "shared"));
				IncludePaths.Add(Path.Combine(WindowsSDKDir, "include", WindowsSDKLibVersion, "um"));
				IncludePaths.Add(Path.Combine(WindowsSDKDir, "include", WindowsSDKLibVersion, "winrt"));
			}
			else
			{
				IncludePaths.Add(Path.Combine(WindowsSDKDir, "include", "shared"));
				IncludePaths.Add(Path.Combine(WindowsSDKDir, "include", "um"));
				IncludePaths.Add(Path.Combine(WindowsSDKDir, "include", "winrt"));
			}

			// Add the existing include paths
			string ExistingIncludePaths = Environment.GetEnvironmentVariable("INCLUDE");
			if (ExistingIncludePaths != null)
			{
				IncludePaths.AddRange(ExistingIncludePaths.Split(new char[] { ';' }, StringSplitOptions.RemoveEmptyEntries));
			}

			// Set the environment variable
			return IncludePaths;
		}

		/// <summary>
		/// Sets the Visual C++ LIB environment variable
		/// </summary>
		static List<string> GetVisualCppLibraryPaths(WindowsCompiler Compiler, DirectoryReference VisualCppDir, DirectoryReference VisualCppToolchainDir, string UniversalCRTDir, string UniversalCRTVersion, string NetFXSDKDir, string WindowsSDKDir, string WindowsSDKLibVersion, CppPlatform Platform)
		{
			List<string> LibraryPaths = new List<string>();

			// Add the standard Visual C++ library paths
			if (Compiler == WindowsCompiler.VisualStudio2017)
			{
				if (Platform == CppPlatform.Win32)
				{
					DirectoryReference StdLibraryDir = DirectoryReference.Combine(VisualCppToolchainDir, "lib", "x86");
					if (DirectoryReference.Exists(StdLibraryDir))
					{
						LibraryPaths.Add(StdLibraryDir.FullName);
					}
				}
				else
				{
					DirectoryReference StdLibraryDir = DirectoryReference.Combine(VisualCppToolchainDir, "lib", "x64");
					if (DirectoryReference.Exists(StdLibraryDir))
					{
						LibraryPaths.Add(StdLibraryDir.FullName);
					}
				}
			}
			else
			{
				if (Platform == CppPlatform.Win32)
				{
					DirectoryReference StdLibraryDir = DirectoryReference.Combine(VisualCppDir, "LIB");
					if (DirectoryReference.Exists(StdLibraryDir))
					{
						LibraryPaths.Add(StdLibraryDir.FullName);
					}
					DirectoryReference AtlMfcLibraryDir = DirectoryReference.Combine(VisualCppDir, "ATLMFC", "LIB");
					if (DirectoryReference.Exists(AtlMfcLibraryDir))
					{
						LibraryPaths.Add(AtlMfcLibraryDir.FullName);
					}
				}
				else
				{
					DirectoryReference StdLibraryDir = DirectoryReference.Combine(VisualCppDir, "LIB", "amd64");
					if (DirectoryReference.Exists(StdLibraryDir))
					{
						LibraryPaths.Add(StdLibraryDir.FullName);
					}
					DirectoryReference AtlMfcLibraryDir = DirectoryReference.Combine(VisualCppDir, "ATLMFC", "LIB", "amd64");
					if (DirectoryReference.Exists(AtlMfcLibraryDir))
					{
						LibraryPaths.Add(AtlMfcLibraryDir.FullName);
					}
				}
			}

			// Add the Universal CRT
			if (!String.IsNullOrEmpty(UniversalCRTDir) && !String.IsNullOrEmpty(UniversalCRTVersion))
			{
				if (Platform == CppPlatform.Win32)
				{
					LibraryPaths.Add(Path.Combine(UniversalCRTDir, "lib", UniversalCRTVersion, "ucrt", "x86"));
				}
				else
				{
					LibraryPaths.Add(Path.Combine(UniversalCRTDir, "lib", UniversalCRTVersion, "ucrt", "x64"));
				}
			}

			// Add the NETFXSDK include path
			if (!String.IsNullOrEmpty(NetFXSDKDir))
			{
				if (Platform == CppPlatform.Win32)
				{
					LibraryPaths.Add(Path.Combine(NetFXSDKDir, "lib", "um", "x86"));
				}
				else
				{
					LibraryPaths.Add(Path.Combine(NetFXSDKDir, "lib", "um", "x64"));
				}
			}

			// Add the standard Windows SDK paths
			if (Platform == CppPlatform.Win32)
			{
				LibraryPaths.Add(Path.Combine(WindowsSDKDir, "lib", WindowsSDKLibVersion, "um", "x86"));
			}
			else
			{
				LibraryPaths.Add(Path.Combine(WindowsSDKDir, "lib", WindowsSDKLibVersion, "um", "x64"));
			}

			// Add the existing library paths
			string ExistingLibraryPaths = Environment.GetEnvironmentVariable("LIB");
			if (ExistingLibraryPaths != null)
			{
				LibraryPaths.AddRange(ExistingLibraryPaths.Split(new char[] { ';' }, StringSplitOptions.RemoveEmptyEntries));
			}

			return LibraryPaths;
		}

		static VCEnvironment EnvVars = null;
	}
}
