// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Security.AccessControl;
using System.Text;
using System.Linq;
using Ionic.Zip;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	class MacToolChainSettings : AppleToolChainSettings
	{
		/***********************************************************************
		 * NOTE:
		 *  Do NOT change the defaults to set your values, instead you should set the environment variables
		 *  properly in your system, as other tools make use of them to work properly!
		 *  The defaults are there simply for examples so you know what to put in your env vars...
		 ***********************************************************************/

		/// <summary>
		/// Which version of the Mac OS SDK to target at build time
		/// </summary>
		public string MacOSSDKVersion = "latest";
		public float MacOSSDKVersionFloat = 0.0f;

		/// <summary>
		/// Which version of the Mac OS X to allow at run time
		/// </summary>
		public string MacOSVersion = "10.11";

		/// <summary>
		/// Minimum version of Mac OS X to actually run on, running on earlier versions will display the system minimum version error dialog and exit.
		/// </summary>
		public string MinMacOSVersion = "10.12.6";

		/// <summary>
		/// Directory for the developer binaries
		/// </summary>
		public string ToolchainDir = "";

		/// <summary>
		/// Location of the SDKs
		/// </summary>
		public string BaseSDKDir;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="bVerbose">Whether to output verbose logging</param>
		public MacToolChainSettings(bool bVerbose) : base(bVerbose)
		{
			BaseSDKDir = XcodeDeveloperDir + "Platforms/MacOSX.platform/Developer/SDKs";
			ToolchainDir = XcodeDeveloperDir + "Toolchains/XcodeDefault.xctoolchain/usr/bin/";

			SelectSDK(BaseSDKDir, "MacOSX", ref MacOSSDKVersion, bVerbose);

			// convert to float for easy comparison
			MacOSSDKVersionFloat = float.Parse(MacOSSDKVersion, System.Globalization.CultureInfo.InvariantCulture);
		}
	}

	/// <summary>
	/// Option flags for the Mac toolchain
	/// </summary>
	[Flags]
	enum MacToolChainOptions
	{
		/// <summary>
		/// No custom options
		/// </summary>
		None = 0,

		/// <summary>
		/// Enable address sanitzier
		/// </summary>
		EnableAddressSanitizer = 0x1,

		/// <summary>
		/// Enable thread sanitizer
		/// </summary>
		EnableThreadSanitizer = 0x2,

		/// <summary>
		/// Enable undefined behavior sanitizer
		/// </summary>
		EnableUndefinedBehaviorSanitizer = 0x4,
	}

	/// <summary>
	/// Mac toolchain wrapper
	/// </summary>
	class MacToolChain : AppleToolChain
	{
		/// <summary>
		/// Whether to compile with ASan enabled
		/// </summary>
		MacToolChainOptions Options;

		public MacToolChain(FileReference InProjectFile, MacToolChainOptions InOptions)
			: base(CppPlatform.Mac, UnrealTargetPlatform.Mac, InProjectFile)
		{
			this.Options = InOptions;
		}

		public static Lazy<MacToolChainSettings> SettingsPrivate = new Lazy<MacToolChainSettings>(() => new MacToolChainSettings(false));

		public static MacToolChainSettings Settings
		{
			get { return SettingsPrivate.Value; }
		}

		/// <summary>
		/// Which compiler frontend to use
		/// </summary>
		private const string MacCompiler = "clang++";

		/// <summary>
		/// Which linker frontend to use
		/// </summary>
		private const string MacLinker = "clang++";

		/// <summary>
		/// Which archiver to use
		/// </summary>
		private const string MacArchiver = "libtool";

		/// <summary>
		/// Track which scripts need to be deleted before appending to
		/// </summary>
		private bool bHasWipedCopyDylibScript = false;
		private bool bHasWipedFixDylibScript = false;

		private static List<FileItem> BundleDependencies = new List<FileItem>();

		private static void SetupXcodePaths(bool bVerbose)
		{
		}

		public override void SetUpGlobalEnvironment(ReadOnlyTargetRules Target)
		{
			base.SetUpGlobalEnvironment(Target);

			SetupXcodePaths(true);
		}

		string GetCompileArguments_Global(CppCompileEnvironment CompileEnvironment)
		{
			string Result = "";

			Result += " -fmessage-length=0";
			Result += " -pipe";
			Result += " -fpascal-strings";

			Result += " -fexceptions";
			Result += " -fasm-blocks";

			if (Options.HasFlag(MacToolChainOptions.EnableAddressSanitizer))
			{
				Result += " -fsanitize=address";
			}
			if (Options.HasFlag(MacToolChainOptions.EnableThreadSanitizer))
			{
				Result += " -fsanitize=thread";
			}
			if (Options.HasFlag(MacToolChainOptions.EnableUndefinedBehaviorSanitizer))
			{
				Result += " -fsanitize=undefined";
			}

			Result += " -Wall -Werror";
			Result += " -Wdelete-non-virtual-dtor";
			//Result += " -Wsign-compare"; // fed up of not seeing the signed/unsigned warnings we get on Windows - lets enable them here too.

			Result += " -Wno-unused-variable";
			Result += " -Wno-unused-value";
			// This will hide the warnings about static functions in headers that aren't used in every single .cpp file
			Result += " -Wno-unused-function";
			// This hides the "enumeration value 'XXXXX' not handled in switch [-Wswitch]" warnings - we should maybe remove this at some point and add UE_LOG(, Fatal, ) to default cases
			Result += " -Wno-switch";
			// This hides the "warning : comparison of unsigned expression < 0 is always false" type warnings due to constant comparisons, which are possible with template arguments
			Result += " -Wno-tautological-compare";
			// This will prevent the issue of warnings for unused private variables.
			Result += " -Wno-unused-private-field";
			// needed to suppress warnings about using offsetof on non-POD types.
			Result += " -Wno-invalid-offsetof";
			// we use this feature to allow static FNames.
			Result += " -Wno-gnu-string-literal-operator-template";
			// Needed for Alembic third party lib
			Result += " -Wno-deprecated-register";

			if (Settings.MacOSSDKVersionFloat < 10.9f && Settings.MacOSSDKVersionFloat >= 10.11f)
			{
				Result += " -Wno-inconsistent-missing-override"; // too many missing overrides...
				Result += " -Wno-unused-local-typedef"; // PhysX has some, hard to remove
			}

			if (CompileEnvironment.bEnableShadowVariableWarnings)
			{
				Result += " -Wshadow" + (CompileEnvironment.bShadowVariableWarningsAsErrors ? "" : " -Wno-error=shadow");
			}

			if (CompileEnvironment.bEnableUndefinedIdentifierWarnings)
			{
				Result += " -Wundef" + (CompileEnvironment.bUndefinedIdentifierWarningsAsErrors ? "" : " -Wno-error=undef");
			}

			// @todo: Remove these two when the code is fixed and they're no longer needed
			Result += " -Wno-logical-op-parentheses";
			Result += " -Wno-unknown-pragmas";

			Result += " -c";

			Result += " -arch x86_64";
			Result += " -isysroot " + Settings.BaseSDKDir + "/MacOSX" + Settings.MacOSSDKVersion + ".sdk";
			Result += " -mmacosx-version-min=" + (CompileEnvironment.bEnableOSX109Support ? "10.9" : Settings.MacOSVersion);

			bool bStaticAnalysis = false;
			string StaticAnalysisMode = Environment.GetEnvironmentVariable("CLANG_STATIC_ANALYZER_MODE");
			if(StaticAnalysisMode != null && StaticAnalysisMode != "")
			{
				bStaticAnalysis = true;
			}

			// Optimize non- debug builds.
			if (CompileEnvironment.bOptimizeCode && !bStaticAnalysis)
			{
				// Don't over optimise if using AddressSanitizer or you'll get false positive errors due to erroneous optimisation of necessary AddressSanitizer instrumentation.
				if (Options.HasFlag(MacToolChainOptions.EnableAddressSanitizer))
				{
					Result += " -O1 -g -fno-optimize-sibling-calls -fno-omit-frame-pointer";
				}
				else if (Options.HasFlag(MacToolChainOptions.EnableThreadSanitizer))
				{
					Result += " -O1 -g";
				}
				else if (CompileEnvironment.bOptimizeForSize)
				{
					Result += " -Oz";
				}
				else
				{
					Result += " -O3";
				}
			}
			else
			{
				Result += " -O0";
			}

			// Create DWARF format debug info if wanted,
			if (CompileEnvironment.bCreateDebugInfo)
			{
				Result += " -gdwarf-2";
			}

			return Result;
		}

		static string GetCompileArguments_CPP()
		{
			string Result = "";
			Result += " -x objective-c++";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -std=c++14";
			Result += " -stdlib=libc++";
			return Result;
		}

		static string GetCompileArguments_MM()
		{
			string Result = "";
			Result += " -x objective-c++";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -std=c++14";
			Result += " -stdlib=libc++";
			return Result;
		}

		static string GetCompileArguments_M()
		{
			string Result = "";
			Result += " -x objective-c";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -std=c++14";
			Result += " -stdlib=libc++";
			return Result;
		}

		static string GetCompileArguments_C()
		{
			string Result = "";
			Result += " -x c";
			return Result;
		}

		static string GetCompileArguments_PCH()
		{
			string Result = "";
			Result += " -x objective-c++-header";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -std=c++14";
			Result += " -stdlib=libc++";
			return Result;
		}

		// Conditionally enable (default disabled) generation of information about every class with virtual functions for use by the C++ runtime type identification features 
		// (`dynamic_cast' and `typeid'). If you don't use those parts of the language, you can save some space by using -fno-rtti. 
		// Note that exception handling uses the same information, but it will generate it as needed. 
		static string GetRTTIFlag(CppCompileEnvironment CompileEnvironment)
		{
			string Result = "";

			if (CompileEnvironment.bUseRTTI)
			{
				Result = " -frtti";
			}
			else
			{
				Result = " -fno-rtti";
			}

			return Result;
		}
		
		string AddFrameworkToLinkCommand(string FrameworkName, string Arg = "-framework")
		{
			string Result = "";
			if (FrameworkName.EndsWith(".framework"))
			{
				Result += " -F \"" + ConvertPath(Path.GetDirectoryName(Path.GetFullPath(FrameworkName))) + "\"";
				FrameworkName = Path.GetFileNameWithoutExtension(FrameworkName);
			}
			Result += " " + Arg + " \"" + FrameworkName + "\"";
			return Result;
		}

		string GetLinkArguments_Global(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			Result += " -arch x86_64";
			Result += " -isysroot " + Settings.BaseSDKDir + "/MacOSX" + Settings.MacOSSDKVersion + ".sdk";
			Result += " -mmacosx-version-min=" + Settings.MacOSVersion;
			Result += " -dead_strip";

			if (Options.HasFlag(MacToolChainOptions.EnableAddressSanitizer) || Options.HasFlag(MacToolChainOptions.EnableThreadSanitizer) || Options.HasFlag(MacToolChainOptions.EnableUndefinedBehaviorSanitizer))
			{
				Result += " -g";
				if (Options.HasFlag(MacToolChainOptions.EnableAddressSanitizer))
				{
					Result += " -fsanitize=address";
				}
				else if (Options.HasFlag(MacToolChainOptions.EnableThreadSanitizer))
				{
					Result += " -fsanitize=thread";
				}
				else if (Options.HasFlag(MacToolChainOptions.EnableUndefinedBehaviorSanitizer))
				{
					Result += " -fsanitize=undefined";
				}
			}

			if (LinkEnvironment.bIsBuildingDLL)
			{
				Result += " -dynamiclib";
			}

			// Needed to make sure install_name_tool will be able to update paths in Mach-O headers
			Result += " -headerpad_max_install_names";

			Result += " -lc++";

			return Result;
		}

		static string GetArchiveArguments_Global(LinkEnvironment LinkEnvironment)
		{
			string Result = "";
			Result += " -static";
			return Result;
		}

		public override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName, ActionGraph ActionGraph)
		{
			var Arguments = new StringBuilder();
			var PCHArguments = new StringBuilder();

			Arguments.Append(GetCompileArguments_Global(CompileEnvironment));

			if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
			{
				// Add the precompiled header file's path to the include path so GCC can find it.
				// This needs to be before the other include paths to ensure GCC uses it instead of the source header file.
				var PrecompiledFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.Mac).GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
				PCHArguments.Append(" -include \"");
				PCHArguments.Append(CompileEnvironment.PrecompiledHeaderFile.AbsolutePath.Replace(PrecompiledFileExtension, ""));
				PCHArguments.Append("\"");
			}

			foreach(FileReference ForceIncludeFile in CompileEnvironment.ForceIncludeFiles)
			{
				PCHArguments.Append(String.Format(" -include \"{0}\"", ForceIncludeFile.FullName));
			}

			// Add include paths to the argument list.
			HashSet<string> AllIncludes = new HashSet<string>(CompileEnvironment.IncludePaths.UserIncludePaths);
			AllIncludes.UnionWith(CompileEnvironment.IncludePaths.SystemIncludePaths);
			foreach (string IncludePath in AllIncludes)
			{
				Arguments.Append(" -I\"");

				if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
				{
					Arguments.Append(ConvertPath(Path.GetFullPath(IncludePath)));

					// sync any third party headers we may need
					if (IncludePath.Contains("ThirdParty"))
					{
						string[] FileList = Directory.GetFiles(IncludePath, "*.h", SearchOption.AllDirectories);
						foreach (string File in FileList)
						{
							FileItem ExternalDependency = FileItem.GetItemByPath(File);
							LocalToRemoteFileItem(ExternalDependency, true);
						}

						FileList = Directory.GetFiles(IncludePath, "*.cpp", SearchOption.AllDirectories);
						foreach (string File in FileList)
						{
							FileItem ExternalDependency = FileItem.GetItemByPath(File);
							LocalToRemoteFileItem(ExternalDependency, true);
						}
					}
				}
				else
				{
					Arguments.Append(IncludePath);
				}

				Arguments.Append("\"");
			}

			foreach (string Definition in CompileEnvironment.Definitions)
			{
				string DefinitionArgument = Definition.Contains("\"") ? Definition.Replace("\"", "\\\"") : Definition;
				Arguments.Append(" -D\"");
				Arguments.Append(DefinitionArgument);
				Arguments.Append("\"");
			}

			CPPOutput Result = new CPPOutput();
			// Create a compile action for each source file.
			foreach (FileItem SourceFile in SourceFiles)
			{
				Action CompileAction = ActionGraph.Add(ActionType.Compile);
				string FileArguments = "";
				string Extension = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant();

				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					// Compile the file as a C++ PCH.
					FileArguments += GetCompileArguments_PCH();
					FileArguments += GetRTTIFlag(CompileEnvironment);
				}
				else if (Extension == ".C")
				{
					// Compile the file as C code.
					FileArguments += GetCompileArguments_C();
				}
				else if (Extension == ".CC")
				{
					// Compile the file as C++ code.
					FileArguments += GetCompileArguments_CPP();
					FileArguments += GetRTTIFlag(CompileEnvironment);
				}
				else if (Extension == ".MM")
				{
					// Compile the file as Objective-C++ code.
					FileArguments += GetCompileArguments_MM();
					FileArguments += GetRTTIFlag(CompileEnvironment);
				}
				else if (Extension == ".M")
				{
					// Compile the file as Objective-C++ code.
					FileArguments += GetCompileArguments_M();
				}
				else
				{
					// Compile the file as C++ code.
					FileArguments += GetCompileArguments_CPP();
					FileArguments += GetRTTIFlag(CompileEnvironment);

					// only use PCH for .cpp files
					FileArguments += PCHArguments.ToString();
				}

				// Add the C++ source file and its included files to the prerequisite item list.
				AddPrerequisiteSourceFile(CompileEnvironment, SourceFile, CompileAction.PrerequisiteItems);

				string OutputFilePath = null;
				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					var PrecompiledHeaderExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.Mac).GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
					// Add the precompiled header file to the produced item list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							CompileEnvironment.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + PrecompiledHeaderExtension
							)
						);

					FileItem RemotePrecompiledHeaderFile = LocalToRemoteFileItem(PrecompiledHeaderFile, false);
					CompileAction.ProducedItems.Add(RemotePrecompiledHeaderFile);
					Result.PrecompiledHeaderFile = RemotePrecompiledHeaderFile;

					// Add the parameters needed to compile the precompiled header file to the command-line.
					FileArguments += string.Format(" -o \"{0}\"", RemotePrecompiledHeaderFile.AbsolutePath);
				}
				else
				{
					if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						CompileAction.bIsUsingPCH = true;
						CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile);
					}
					var ObjectFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.Mac).GetBinaryExtension(UEBuildBinaryType.Object);
					// Add the object file to the produced item list.
					FileItem ObjectFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							CompileEnvironment.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension
							)
						);

					FileItem RemoteObjectFile = LocalToRemoteFileItem(ObjectFile, false);
					CompileAction.ProducedItems.Add(RemoteObjectFile);
					Result.ObjectFiles.Add(RemoteObjectFile);
					FileArguments += string.Format(" -o \"{0}\"", RemoteObjectFile.AbsolutePath);
					OutputFilePath = RemoteObjectFile.AbsolutePath;
				}

				// Add the source file path to the command-line.
				FileArguments += string.Format(" \"{0}\"", ConvertPath(SourceFile.AbsolutePath));

				if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
				{
					CompileAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
				}
				
				string AllArgs = Arguments + FileArguments + CompileEnvironment.AdditionalArguments;
				string CompilerPath = Settings.ToolchainDir + MacCompiler;
				
				// Analyze and then compile using the shell to perform the indirection
				string StaticAnalysisMode = Environment.GetEnvironmentVariable("CLANG_STATIC_ANALYZER_MODE");
				if(StaticAnalysisMode != null && StaticAnalysisMode != "" && OutputFilePath != null)
				{
					string TempArgs = "-c \"" + CompilerPath + " " + AllArgs + " --analyze -Wno-unused-command-line-argument -Xclang -analyzer-output=html -Xclang -analyzer-config -Xclang path-diagnostics-alternate=true -Xclang -analyzer-config -Xclang report-in-main-source-file=true -Xclang -analyzer-disable-checker -Xclang deadcode.DeadStores -o " + OutputFilePath.Replace(".o", ".html") + "; " + CompilerPath + " " + AllArgs + "\"";
					AllArgs = TempArgs;
					CompilerPath = "/bin/sh";
				}

				CompileAction.WorkingDirectory = GetMacDevSrcRoot();
				CompileAction.CommandPath = CompilerPath;
				CompileAction.CommandArguments = AllArgs;
				CompileAction.CommandDescription = "Compile";
				CompileAction.StatusDescription = Path.GetFileName(SourceFile.AbsolutePath);
				CompileAction.bIsGCCCompiler = true;
				// We're already distributing the command by execution on Mac.
				CompileAction.bCanExecuteRemotely = false;
				CompileAction.bShouldOutputStatusDescription = true;
				CompileAction.OutputEventHandler = new DataReceivedEventHandler(RemoteOutputReceivedEventHandler);
			}
			return Result;
		}

		private void AppendMacLine(StreamWriter Writer, string Format, params object[] Arg)
		{
			string PreLine = String.Format(Format, Arg);
			Writer.Write(PreLine + "\n");
		}

		private int LoadEngineCL()
		{
			BuildVersion Version;
			if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
			{
				return Version.Changelist;
			}
			else
			{
				return 0;
			}
		}

		public static string LoadEngineDisplayVersion(bool bIgnorePatchVersion = false)
		{
			BuildVersion Version;
			if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
			{
				return String.Format("{0}.{1}.{2}", Version.MajorVersion, Version.MinorVersion, bIgnorePatchVersion? 0 : Version.PatchVersion);
			}
			else
			{
				return "4.0.0";
			}
		}

		private int LoadBuiltFromChangelistValue()
		{
			return LoadEngineCL();
		}

		private int LoadIsLicenseeVersionValue()
		{
			BuildVersion Version;
			if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
			{
				return Version.IsLicenseeVersion;
			}
			else
			{
				return 0;
			}
		}

		private string LoadEngineAPIVersion()
		{
			int CL = 0;

			BuildVersion Version;
			if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
			{
				CL = Version.EffectiveCompatibleChangelist;
			}

			return String.Format("{0}.{1}.{2}", CL / (100 * 100), (CL / 100) % 100, CL % 100);
		}

		private void AddLibraryPathToRPaths(string Library, string ExeAbsolutePath, ref List<string> RPaths, ref string LinkCommand, bool bIsBuildingAppBundle)
		{
			string LibraryFullPath = Path.GetFullPath(Library);
 			string LibraryDir = Path.GetDirectoryName(LibraryFullPath);
			string ExeDir = Path.GetDirectoryName(ExeAbsolutePath);

			// Only dylibs and frameworks, and only those that are outside of Engine/Binaries/Mac and Engine/Source/ThirdParty, and outside of the folder where the executable is need an additional RPATH entry
			if ((Library.EndsWith("dylib") || Library.EndsWith(".framework")) && !LibraryFullPath.Contains("/Engine/Binaries/Mac/")
			    && !LibraryFullPath.Contains("/Engine/Source/ThirdParty/") && LibraryDir != ExeDir && !RPaths.Contains(LibraryDir))
			{
				// macOS gatekeeper erroneously complains about not seeing the CEF3 framework in the codesigned Launcher because it's only present in one of the folders specified in RPATHs.
				// To work around this we will only add a single RPATH entry for it, for the framework stored in .app/Contents/UE4/ subfolder of the packaged app bundle
				bool bCanUseMultipleRPATHs = !ExeAbsolutePath.Contains("EpicGamesLauncher-Mac-Shipping") || !Library.Contains("CEF3");

				// First, add a path relative to the executable.
				string RelativePath = Utils.MakePathRelativeTo(LibraryDir, ExeDir).Replace("\\", "/");
				if (bCanUseMultipleRPATHs)
				{
					LinkCommand += " -rpath \"@loader_path/" + RelativePath + "\"";
				}

				// If building an app bundle, we also need an RPATH for use in packaged game and a separate one for staged builds
				if (bIsBuildingAppBundle)
				{
					string EngineDir = UnrealBuildTool.RootDirectory.ToString();

					// In packaged games dylibs are stored in Contents/UE4 subfolders, for example in GameName.app/Contents/UE4/Engine/Binaries/ThirdParty/PhysX/Mac
					string BundleUE4Dir = Path.GetFullPath(ExeDir + "/../../Contents/UE4");
					string BundleLibraryDir = LibraryDir.Replace(EngineDir, BundleUE4Dir);
					string BundleRelativeDir = Utils.MakePathRelativeTo(BundleLibraryDir, ExeDir).Replace("\\", "/");
					LinkCommand += " -rpath \"@loader_path/" + BundleRelativeDir + "\"";

					// For staged code-based games we need additional entry if the game is not stored directly in the engine's root directory
					if (bCanUseMultipleRPATHs)
					{
						string StagedUE4Dir = Path.GetFullPath(ExeDir + "/../../../../../..");
						string StagedLibraryDir = LibraryDir.Replace(EngineDir, StagedUE4Dir);
						string StagedRelativeDir = Utils.MakePathRelativeTo(StagedLibraryDir, ExeDir).Replace("\\", "/");
						if (StagedRelativeDir != RelativePath)
						{
							LinkCommand += " -rpath \"@loader_path/" + StagedRelativeDir + "\"";
						}
					}
				}

				RPaths.Add(LibraryDir);
			}
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, ActionGraph ActionGraph)
		{
			bool bIsBuildingLibrary = LinkEnvironment.bIsBuildingLibrary || bBuildImportLibraryOnly;

			// Create an action that invokes the linker.
			Action LinkAction = ActionGraph.Add(ActionType.Link);

			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				LinkAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			}

			LinkAction.WorkingDirectory = GetMacDevSrcRoot();
			LinkAction.CommandPath = "/bin/sh";
			LinkAction.CommandDescription = "Link";

			string EngineAPIVersion = LoadEngineAPIVersion();
			string EngineDisplayVersion = LoadEngineDisplayVersion(true);
			string VersionArg = LinkEnvironment.bIsBuildingDLL ? " -current_version " + EngineAPIVersion + " -compatibility_version " + EngineDisplayVersion : "";

			string Linker = bIsBuildingLibrary ? MacArchiver : MacLinker;
			string LinkCommand = Settings.ToolchainDir + Linker + VersionArg + " " + (bIsBuildingLibrary ? GetArchiveArguments_Global(LinkEnvironment) : GetLinkArguments_Global(LinkEnvironment));

			// Tell the action that we're building an import library here and it should conditionally be
			// ignored as a prerequisite for other actions
			LinkAction.bProducesImportLibrary = !Utils.IsRunningOnMono && (bBuildImportLibraryOnly || LinkEnvironment.bIsBuildingDLL);

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
			OutputFile.bNeedsHotReloadNumbersDLLCleanUp = LinkEnvironment.bIsBuildingDLL;

			FileItem RemoteOutputFile = LocalToRemoteFileItem(OutputFile, false);

			// To solve the problem with cross dependencies, for now we create a broken dylib that does not link with other engine dylibs.
			// This is fixed in later step, FixDylibDependencies. For this and to know what libraries to copy whilst creating an app bundle,
			// we gather the list of engine dylibs.
			List<string> EngineAndGameLibraries = new List<string>();

			string DylibsPath = "@rpath";

			string AbsolutePath = OutputFile.AbsolutePath.Replace("\\", "/");
			if (!bIsBuildingLibrary)
			{
				LinkCommand += " -rpath @loader_path/ -rpath @executable_path/";
			}

			List<string> ThirdPartyLibraries = new List<string>();

			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				// Add any additional files that we'll need in order to link the app
				foreach (string AdditionalShadowFile in LinkEnvironment.AdditionalShadowFiles)
				{
					FileItem ShadowFile = FileItem.GetExistingItemByPath(AdditionalShadowFile);
					if (ShadowFile != null)
					{
						QueueFileForBatchUpload(ShadowFile);
						LinkAction.PrerequisiteItems.Add(ShadowFile);
					}
					else
					{
						throw new BuildException("Couldn't find required additional file to shadow: {0}", AdditionalShadowFile);
					}
				}

				// Add any frameworks to be shadowed to the remote
				foreach (string FrameworkPath in LinkEnvironment.Frameworks)
				{
					if (FrameworkPath.EndsWith(".framework"))
					{
						foreach (string FrameworkFile in Directory.EnumerateFiles(FrameworkPath, "*", SearchOption.AllDirectories))
						{
							FileItem FrameworkFileItem = FileItem.GetExistingItemByPath(FrameworkFile);
							QueueFileForBatchUpload(FrameworkFileItem);
							LinkAction.PrerequisiteItems.Add(FrameworkFileItem);
						}
					}
				}
			}

			bool bIsBuildingAppBundle = !LinkEnvironment.bIsBuildingDLL && !LinkEnvironment.bIsBuildingLibrary && !LinkEnvironment.bIsBuildingConsoleApplication;

			List<string> RPaths = new List<string>();

			if (!bIsBuildingLibrary || LinkEnvironment.bIncludeDependentLibrariesInLibrary)
			{
				// Add the additional libraries to the argument list.
				foreach (string AdditionalLibrary in LinkEnvironment.AdditionalLibraries)
				{
					// Can't link dynamic libraries when creating a static one
					if (bIsBuildingLibrary && (Path.GetExtension(AdditionalLibrary) == ".dylib" || AdditionalLibrary == "z"))
					{
						continue;
					}

					if (Path.GetDirectoryName(AdditionalLibrary) != "" &&
							 (Path.GetDirectoryName(AdditionalLibrary).Contains("Binaries/Mac") ||
							 Path.GetDirectoryName(AdditionalLibrary).Contains("Binaries\\Mac")))
					{
						// It's an engine or game dylib. Save it for later
						EngineAndGameLibraries.Add(ConvertPath(Path.GetFullPath(AdditionalLibrary)));

						if (!Utils.IsRunningOnMono)
						{
							FileItem EngineLibDependency = FileItem.GetItemByPath(AdditionalLibrary);
							LinkAction.PrerequisiteItems.Add(EngineLibDependency);
							FileItem RemoteEngineLibDependency = FileItem.GetRemoteItemByPath(ConvertPath(Path.GetFullPath(AdditionalLibrary)), UnrealTargetPlatform.Mac);
							LinkAction.PrerequisiteItems.Add(RemoteEngineLibDependency);
							//Log.TraceInformation("Adding {0} / {1} as a prereq to {2}", EngineLibDependency.AbsolutePath, RemoteEngineLibDependency.AbsolutePath, RemoteOutputFile.AbsolutePath);
						}
						else if (LinkEnvironment.bIsCrossReferenced == false)
						{
							FileItem EngineLibDependency = FileItem.GetItemByPath(AdditionalLibrary);
							LinkAction.PrerequisiteItems.Add(EngineLibDependency);
						}
					}
					else if (AdditionalLibrary.Contains(".framework/"))
					{
						LinkCommand += string.Format(" \'{0}\'", AdditionalLibrary);
					}
					else  if (string.IsNullOrEmpty(Path.GetDirectoryName(AdditionalLibrary)) && string.IsNullOrEmpty(Path.GetExtension(AdditionalLibrary)))
					{
						LinkCommand += string.Format(" -l{0}", AdditionalLibrary);
					}
					else
					{
						LinkCommand += string.Format(" \"{0}\"", ConvertPath(Path.GetFullPath(AdditionalLibrary)));
						if (Path.GetExtension(AdditionalLibrary) == ".dylib")
						{
							ThirdPartyLibraries.Add(AdditionalLibrary);
						}

						if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
						{
							// copy over libs we may need
							FileItem ShadowFile = FileItem.GetExistingItemByPath(AdditionalLibrary);
							if (ShadowFile != null)
							{
								QueueFileForBatchUpload(ShadowFile);
							}
						}
					}

					AddLibraryPathToRPaths(AdditionalLibrary, AbsolutePath, ref RPaths, ref LinkCommand, bIsBuildingAppBundle);
				}

				foreach (string AdditionalLibrary in LinkEnvironment.DelayLoadDLLs)
				{
					// Can't link dynamic libraries when creating a static one
					if (bIsBuildingLibrary && (Path.GetExtension(AdditionalLibrary) == ".dylib" || AdditionalLibrary == "z"))
					{
						continue;
					}

					LinkCommand += string.Format(" -weak_library \"{0}\"", ConvertPath(Path.GetFullPath(AdditionalLibrary)));

					AddLibraryPathToRPaths(AdditionalLibrary, AbsolutePath, ref RPaths, ref LinkCommand, bIsBuildingAppBundle);

                    if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
                    {
                        // copy over libs we may need
                        FileItem ShadowFile = FileItem.GetExistingItemByPath(AdditionalLibrary);
                        if (ShadowFile != null)
                        {
                            QueueFileForBatchUpload(ShadowFile);
                        }
                    }
                }
			}

			// Add frameworks
			Dictionary<string, bool> AllFrameworks = new Dictionary<string, bool>();
			foreach (string Framework in LinkEnvironment.Frameworks)
			{
				if (!AllFrameworks.ContainsKey(Framework))
				{
					AllFrameworks.Add(Framework, false);
				}
			}
			foreach (UEBuildFramework Framework in LinkEnvironment.AdditionalFrameworks)
			{
				if (!AllFrameworks.ContainsKey(Framework.FrameworkName))
				{
					AllFrameworks.Add(Framework.FrameworkName, false);
				}
			}
			foreach (string Framework in LinkEnvironment.WeakFrameworks)
			{
				if (!AllFrameworks.ContainsKey(Framework))
				{
					AllFrameworks.Add(Framework, true);
				}
			}

			if (!bIsBuildingLibrary)
			{
				foreach (var Framework in AllFrameworks)
				{
					LinkCommand += AddFrameworkToLinkCommand(Framework.Key, Framework.Value ? "-weak_framework" : "-framework");
					AddLibraryPathToRPaths(Framework.Key, AbsolutePath, ref RPaths, ref LinkCommand, bIsBuildingAppBundle);
				}
			}

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				if (bIsBuildingLibrary)
				{
					InputFileNames.Add(string.Format("\"{0}\"", InputFile.AbsolutePath));
				}
				else
				{
					string EnginePath = ConvertPath(Path.GetDirectoryName(Directory.GetCurrentDirectory()));
					string InputFileRelativePath = InputFile.AbsolutePath.Replace(EnginePath + "/", "../");
					InputFileNames.Add(string.Format("\"{0}\"", InputFileRelativePath));
				}
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			if (bIsBuildingLibrary)
			{
				foreach (string Filename in InputFileNames)
				{
					LinkCommand += " " + Filename;
				}
			}
			else
			{
				// Write the list of input files to a response file, with a tempfilename, on remote machine
				FileReference ResponsePath = FileReference.Combine(LinkEnvironment.IntermediateDirectory, Path.GetFileName(OutputFile.AbsolutePath) + ".response");

				// Never create response files when we are only generating IntelliSense data
				if (!ProjectFileGenerator.bGenerateProjectFiles)
				{
					ResponseFile.Create(ResponsePath, InputFileNames);

					if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
					{
						RPCUtilHelper.CopyFile(ResponsePath.FullName, ConvertPath(ResponsePath.FullName), true);
					}
				}

				LinkCommand += string.Format(" @\"{0}\"", ConvertPath(ResponsePath.FullName));
			}

			if (LinkEnvironment.bIsBuildingDLL)
			{
				// Add the output file to the command-line.
				string Filename = "";
				int Index = OutputFile.AbsolutePath.LastIndexOf(".app/Contents/MacOS/");
				if (Index > -1)
				{
					Index += ".app/Contents/MacOS/".Length;
					Filename = OutputFile.AbsolutePath.Substring(Index);
				}
				else
				{
					Filename = Path.GetFileName(OutputFile.AbsolutePath);
				}
				LinkCommand += string.Format(" -install_name {0}/{1}", DylibsPath, Filename);
			}

			if (!bIsBuildingLibrary)
			{
				if (UnrealBuildTool.IsEngineInstalled() || (Utils.IsRunningOnMono && LinkEnvironment.bIsCrossReferenced == false))
				{
					foreach (string Library in EngineAndGameLibraries)
					{
						string LibraryPath = Library;
						if (!File.Exists(Library))
						{
							string LibraryDir = Path.GetDirectoryName(Library);
							string LibraryName = Path.GetFileName(Library);
							string AppBundleName = "UE4Editor";
							if (LibraryName.Contains("UE4Editor-Mac-"))
							{
								string[] Parts = LibraryName.Split('-');
								AppBundleName += "-" + Parts[1] + "-" + Parts[2];
							}
							AppBundleName += ".app";
							LibraryPath = LibraryDir + "/" + AppBundleName + "/Contents/MacOS/" + LibraryName;
							if (!File.Exists(LibraryPath))
							{
								LibraryPath = Library;
							}
						}
						LinkCommand += " \"" + LibraryPath + "\"";
					}
				}
				else
				{
					// Tell linker to ignore unresolved symbols, so we don't have a problem with cross dependent dylibs that do not exist yet.
					// This is fixed in later step, FixDylibDependencies.
					LinkCommand += string.Format(" -undefined dynamic_lookup");
				}
			}

			// Add the output file to the command-line.
			LinkCommand += string.Format(" -o \"{0}\"", RemoteOutputFile.AbsolutePath);

			// Add the additional arguments specified by the environment.
			LinkCommand += LinkEnvironment.AdditionalArguments;

			if (!bIsBuildingLibrary)
			{
				// Fix the paths for third party libs
				foreach (string Library in ThirdPartyLibraries)
				{
					string LibraryFileName = Path.GetFileName(Library);
					LinkCommand += "; " + Settings.ToolchainDir + "install_name_tool -change " + LibraryFileName + " " + DylibsPath + "/" + LibraryFileName + " \"" + ConvertPath(OutputFile.AbsolutePath) + "\"";
				}
			}

			LinkAction.CommandArguments = "-c '" + LinkCommand + "'";

			// Only execute linking on the local Mac.
			LinkAction.bCanExecuteRemotely = false;

			LinkAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);
			LinkAction.OutputEventHandler = new DataReceivedEventHandler(RemoteOutputReceivedEventHandler);

			LinkAction.ProducedItems.Add(RemoteOutputFile);

			if (!DirectoryReference.Exists(LinkEnvironment.IntermediateDirectory))
			{
				return OutputFile;
			}

			if (!bIsBuildingLibrary)
			{
				// Prepare a script that will run later, once every dylibs and the executable are created. This script will be called by action created in FixDylibDependencies()
				FileReference FixDylibDepsScriptPath = FileReference.Combine(LinkEnvironment.LocalShadowDirectory, "FixDylibDependencies.sh");
				if (!bHasWipedFixDylibScript)
				{
					if (FileReference.Exists(FixDylibDepsScriptPath))
					{
						FileReference.Delete(FixDylibDepsScriptPath);
					}
					bHasWipedFixDylibScript = true;
				}

				if (!DirectoryReference.Exists(LinkEnvironment.LocalShadowDirectory))
				{
					DirectoryReference.CreateDirectory(LinkEnvironment.LocalShadowDirectory);
				}

				StreamWriter FixDylibDepsScript = File.AppendText(FixDylibDepsScriptPath.FullName);

				if (LinkEnvironment.bIsCrossReferenced || !Utils.IsRunningOnMono)
				{
					string EngineAndGameLibrariesString = "";
					foreach (string Library in EngineAndGameLibraries)
					{
						EngineAndGameLibrariesString += " \"" + Library + "\"";
					}
					string FixDylibLine = "pushd \"" + ConvertPath(Directory.GetCurrentDirectory()) + "\"  > /dev/null; ";
					FixDylibLine += string.Format("TIMESTAMP=`stat -n -f \"%Sm\" -t \"%Y%m%d%H%M.%S\" \"{0}\"`; ", RemoteOutputFile.AbsolutePath);
					FixDylibLine += LinkCommand.Replace("-undefined dynamic_lookup", EngineAndGameLibrariesString).Replace("$", "\\$");
					FixDylibLine += string.Format("; touch -t $TIMESTAMP \"{0}\"; if [[ $? -ne 0 ]]; then exit 1; fi; ", RemoteOutputFile.AbsolutePath);
					FixDylibLine += "popd > /dev/null";
					AppendMacLine(FixDylibDepsScript, FixDylibLine);
				}

				FixDylibDepsScript.Close();

				// Prepare a script that will be called by FinalizeAppBundle.sh to copy all necessary third party dylibs to the app bundle
				// This is done this way as FinalizeAppBundle.sh script can be created before all the libraries are processed, so
				// at the time of it's creation we don't have the full list of third party dylibs all modules need.
				if (DylibCopyScriptPath == null)
				{
					DylibCopyScriptPath = FileReference.Combine(LinkEnvironment.IntermediateDirectory, "DylibCopy.sh");
				}
				if (!bHasWipedCopyDylibScript)
				{
					if (FileReference.Exists(DylibCopyScriptPath))
					{
						FileReference.Delete(DylibCopyScriptPath);
					}
					bHasWipedCopyDylibScript = true;
				}
				string ExistingScript = FileReference.Exists(DylibCopyScriptPath) ? File.ReadAllText(DylibCopyScriptPath.FullName) : "";
				StreamWriter DylibCopyScript = File.AppendText(DylibCopyScriptPath.FullName);
				foreach (string Library in ThirdPartyLibraries)
				{
					string CopyCommandLineEntry = FormatCopyCommand(ConvertPath(Path.GetFullPath(Library)).Replace("$", "\\$"), "$1.app/Contents/MacOS");
					if (!ExistingScript.Contains(CopyCommandLineEntry))
					{
						AppendMacLine(DylibCopyScript, CopyCommandLineEntry);
					}
				}
				DylibCopyScript.Close();

				// For non-console application, prepare a script that will create the app bundle. It'll be run by FinalizeAppBundle action
				if (bIsBuildingAppBundle)
				{
					FileReference FinalizeAppBundleScriptPath = FileReference.Combine(LinkEnvironment.IntermediateDirectory, "FinalizeAppBundle.sh");
					StreamWriter FinalizeAppBundleScript = File.CreateText(FinalizeAppBundleScriptPath.FullName);
					AppendMacLine(FinalizeAppBundleScript, "#!/bin/sh");
					string BinariesPath = Path.GetDirectoryName(OutputFile.AbsolutePath);
					BinariesPath = Path.GetDirectoryName(BinariesPath.Substring(0, BinariesPath.IndexOf(".app")));
					AppendMacLine(FinalizeAppBundleScript, "cd \"{0}\"", ConvertPath(BinariesPath).Replace("$", "\\$"));

					string BundleVersion = LinkEnvironment.BundleVersion;
					if(BundleVersion == null)
					{
						BundleVersion = LoadEngineDisplayVersion();
					}

					string ExeName = Path.GetFileName(OutputFile.AbsolutePath);
					bool bIsLauncherProduct = ExeName.StartsWith("EpicGamesLauncher") || ExeName.StartsWith("EpicGamesBootstrapLauncher");
					string[] ExeNameParts = ExeName.Split('-');
					string GameName = ExeNameParts[0];

                    // bundle identifier
                    // plist replacements
                    DirectoryReference DirRef = (!string.IsNullOrEmpty(UnrealBuildTool.GetRemoteIniPath()) ? new DirectoryReference(UnrealBuildTool.GetRemoteIniPath()) : (ProjectFile != null ? ProjectFile.Directory : null));
                    ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirRef, UnrealTargetPlatform.IOS);

                    string BundleIdentifier;
                    Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "BundleIdentifier", out BundleIdentifier);

                    string ProjectName = GameName;
					FileReference UProjectFilePath = ProjectFile;

					if (UProjectFilePath != null)
					{
						ProjectName = UProjectFilePath.GetFileNameWithoutAnyExtensions();
					}

					if (GameName == "EpicGamesBootstrapLauncher")
					{
						GameName = "EpicGamesLauncher";
					}
					else if (GameName == "UE4" && UProjectFilePath != null)
					{
						GameName = UProjectFilePath.GetFileNameWithoutAnyExtensions();
					}

					AppendMacLine(FinalizeAppBundleScript, "mkdir -p \"{0}.app/Contents/MacOS\"", ExeName);
					AppendMacLine(FinalizeAppBundleScript, "mkdir -p \"{0}.app/Contents/Resources\"", ExeName);

					// Copy third party dylibs by calling additional script prepared earlier
					AppendMacLine(FinalizeAppBundleScript, "sh \"{0}\" \"{1}\"", ConvertPath(DylibCopyScriptPath.FullName).Replace("$", "\\$"), ExeName);

					string IconName = "UE4";
					string EngineSourcePath = ConvertPath(Directory.GetCurrentDirectory()).Replace("$", "\\$");
					string CustomResourcesPath = "";
					string CustomBuildPath = "";
					if (UProjectFilePath == null)
					{
						string[] TargetFiles = Directory.GetFiles(Directory.GetCurrentDirectory(), GameName + ".Target.cs", SearchOption.AllDirectories);
						if (TargetFiles.Length == 1)
						{
							CustomResourcesPath = Path.GetDirectoryName(TargetFiles[0]) + "/Resources/Mac";
							CustomBuildPath = Path.GetDirectoryName(TargetFiles[0]) + "../Build/Mac";
						}
						else
						{
							Log.TraceWarning("Found {0} Target.cs files for {1} in alldir search of directory {2}", TargetFiles.Length, GameName, Directory.GetCurrentDirectory());
						}
					}
					else
					{
						string ResourceParentFolderName = bIsLauncherProduct ? "Application" : GameName;
						CustomResourcesPath = Path.GetDirectoryName(UProjectFilePath.FullName) + "/Source/" + ResourceParentFolderName + "/Resources/Mac";
						if (!Directory.Exists(CustomResourcesPath))
						{
							CustomResourcesPath = Path.GetDirectoryName(UProjectFilePath.FullName) + "/Source/" + ProjectName + "/Resources/Mac";
						}
						CustomBuildPath = Path.GetDirectoryName(UProjectFilePath.FullName) + "/Build/Mac";
					}

					bool bBuildingEditor = GameName.EndsWith("Editor");

					// Copy resources
					string DefaultIcon = EngineSourcePath + "/Runtime/Launch/Resources/Mac/" + IconName + ".icns";
					string CustomIcon = "";
					if (bBuildingEditor)
					{
						CustomIcon = DefaultIcon;
					}
					else
					{
						CustomIcon = CustomBuildPath + "/Application.icns";
						if (!File.Exists(CustomIcon))
						{
							CustomIcon = CustomResourcesPath + "/" + GameName + ".icns";
							if (!File.Exists(CustomIcon))
							{
								CustomIcon = DefaultIcon;
							}
						}

						if (CustomIcon != DefaultIcon)
						{
							QueueFileForBatchUpload(FileItem.GetItemByFileReference(new FileReference(CustomIcon)));
							CustomIcon = ConvertPath(CustomIcon);
						}
					}
					AppendMacLine(FinalizeAppBundleScript, FormatCopyCommand(CustomIcon, String.Format("{0}.app/Contents/Resources/{1}.icns", ExeName, GameName)));

					if (ExeName.StartsWith("UE4Editor"))
					{
						AppendMacLine(FinalizeAppBundleScript, FormatCopyCommand(String.Format("{0}/Runtime/Launch/Resources/Mac/UProject.icns", EngineSourcePath), String.Format("{0}.app/Contents/Resources/UProject.icns", ExeName)));
					}

					string InfoPlistFile = CustomResourcesPath + (bBuildingEditor ? "Info-Editor.plist" : "Info.plist");
					if (!File.Exists(InfoPlistFile))
					{
						InfoPlistFile = EngineSourcePath + "/Runtime/Launch/Resources/Mac/" + (bBuildingEditor ? "Info-Editor.plist" : "Info.plist");
					}
					else
					{
						QueueFileForBatchUpload(FileItem.GetItemByFileReference(new FileReference(InfoPlistFile)));
						InfoPlistFile = ConvertPath(InfoPlistFile);
					}

					string TempInfoPlist = "$TMPDIR/TempInfo.plist";
					AppendMacLine(FinalizeAppBundleScript, FormatCopyCommand(InfoPlistFile, TempInfoPlist));

					// Fix contents of Info.plist
					AppendMacLine(FinalizeAppBundleScript, "sed -i \"\" \"s/\\${0}/{1}/g\" \"{2}\"", "{EXECUTABLE_NAME}", ExeName, TempInfoPlist);
					AppendMacLine(FinalizeAppBundleScript, "sed -i \"\" \"s/\\${0}/{1}/g\" \"{2}\"", "{APP_NAME}", bBuildingEditor ? ("com.epicgames." + GameName) : (BundleIdentifier.Replace("[PROJECT_NAME]", GameName).Replace("_", "")), TempInfoPlist);
					AppendMacLine(FinalizeAppBundleScript, "sed -i \"\" \"s/\\${0}/{1}/g\" \"{2}\"", "{MACOSX_DEPLOYMENT_TARGET}", Settings.MinMacOSVersion, TempInfoPlist);
					AppendMacLine(FinalizeAppBundleScript, "sed -i \"\" \"s/\\${0}/{1}/g\" \"{2}\"", "{ICON_NAME}", GameName, TempInfoPlist);
					AppendMacLine(FinalizeAppBundleScript, "sed -i \"\" \"s/\\${0}/{1}/g\" \"{2}\"", "{BUNDLE_VERSION}", BundleVersion, TempInfoPlist);

					// Copy it into place
					AppendMacLine(FinalizeAppBundleScript, FormatCopyCommand(TempInfoPlist, String.Format("{0}.app/Contents/Info.plist", ExeName)));
					AppendMacLine(FinalizeAppBundleScript, "chmod 644 \"{0}.app/Contents/Info.plist\"", ExeName);

					// Generate PkgInfo file
					string TempPkgInfo = "$TMPDIR/TempPkgInfo";
					AppendMacLine(FinalizeAppBundleScript, "echo 'echo -n \"APPL????\"' | bash > \"{0}\"", TempPkgInfo);
					AppendMacLine(FinalizeAppBundleScript, FormatCopyCommand(TempPkgInfo, String.Format("{0}.app/Contents/PkgInfo", ExeName)));

					// Make sure OS X knows the bundle was updated
					AppendMacLine(FinalizeAppBundleScript, "touch -c \"{0}.app\"", ExeName);

					FinalizeAppBundleScript.Close();

					// copy over some needed files
					// @todo mac: Make a QueueDirectoryForBatchUpload
					QueueFileForBatchUpload(FileItem.GetItemByFileReference(new FileReference("../../Engine/Source/Runtime/Launch/Resources/Mac/" + GameName + ".icns")));
					QueueFileForBatchUpload(FileItem.GetItemByFileReference(new FileReference("../../Engine/Source/Runtime/Launch/Resources/Mac/UProject.icns")));
					QueueFileForBatchUpload(FileItem.GetItemByFileReference(new FileReference("../../Engine/Source/Runtime/Launch/Resources/Mac/Info.plist")));
					QueueFileForBatchUpload(FileItem.GetItemByFileReference(FileReference.Combine(LinkEnvironment.IntermediateDirectory, "DylibCopy.sh")));
				}
			}

			return RemoteOutputFile;
		}

		static string FormatCopyCommand(string SourceFile, string TargetFile)
		{
			return String.Format("rsync --checksum \"{0}\" \"{1}\"", SourceFile, TargetFile);
		}

		FileItem FixDylibDependencies(LinkEnvironment LinkEnvironment, FileItem Executable, ActionGraph ActionGraph)
		{
			Action LinkAction = ActionGraph.Add(ActionType.Link);
			LinkAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
			LinkAction.CommandPath = "/bin/sh";
			LinkAction.CommandDescription = "";

			// Call the FixDylibDependencies.sh script which will link the dylibs and the main executable, this time proper ones, as it's called
			// once all are already created, so the cross dependency problem no longer prevents linking.
			// The script is deleted after it's executed so it's empty when we start appending link commands for the next executable.
			FileItem FixDylibDepsScript = FileItem.GetItemByFileReference(FileReference.Combine(LinkEnvironment.LocalShadowDirectory, "FixDylibDependencies.sh"));
			FileItem RemoteFixDylibDepsScript = LocalToRemoteFileItem(FixDylibDepsScript, true);

			LinkAction.CommandArguments = "-c 'chmod +x \"" + RemoteFixDylibDepsScript.AbsolutePath + "\"; \"" + RemoteFixDylibDepsScript.AbsolutePath + "\"; if [[ $? -ne 0 ]]; then exit 1; fi; ";

			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				LinkAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			}

			// Make sure this action is executed after all the dylibs and the main executable are created

			foreach (FileItem Dependency in BundleDependencies)
			{
				LinkAction.PrerequisiteItems.Add(Dependency);
			}

			BundleDependencies.Clear();

			LinkAction.StatusDescription = string.Format("Fixing dylib dependencies for {0}", Path.GetFileName(Executable.AbsolutePath));
			LinkAction.bCanExecuteRemotely = false;

			FileItem OutputFile = FileItem.GetItemByFileReference(FileReference.Combine(LinkEnvironment.LocalShadowDirectory, Path.GetFileNameWithoutExtension(Executable.AbsolutePath) + ".link"));
			FileItem RemoteOutputFile = LocalToRemoteFileItem(OutputFile, false);

			LinkAction.CommandArguments += "echo \"Dummy\" >> \"" + RemoteOutputFile.AbsolutePath + "\"";
			LinkAction.CommandArguments += "'";

			LinkAction.ProducedItems.Add(RemoteOutputFile);

			return RemoteOutputFile;
		}

		private static Dictionary<Action, string> DebugOutputMap = new Dictionary<Action, string>();
		static public void RPCDebugInfoActionHandler(Action Action, out int ExitCode, out string Output)
		{
			RPCUtilHelper.RPCActionHandler(Action, out ExitCode, out Output);
			if (DebugOutputMap.ContainsKey(Action))
			{
				if (ExitCode == 0)
				{
					RPCUtilHelper.CopyDirectory(Action.ProducedItems[0].AbsolutePath, DebugOutputMap[Action], RPCUtilHelper.ECopyOptions.None);
				}
				DebugOutputMap.Remove(Action);
			}
		}

		/// <summary>
		/// Generates debug info for a given executable
		/// </summary>
		/// <param name="MachOBinary">FileItem describing the executable or dylib to generate debug info for</param>
		/// <param name="LinkEnvironment"></param>
		/// <param name="ActionGraph"></param>
		public FileItem GenerateDebugInfo(FileItem MachOBinary, LinkEnvironment LinkEnvironment, ActionGraph ActionGraph)
		{
			string BinaryPath = MachOBinary.AbsolutePath;
			if (BinaryPath.Contains(".app"))
			{
				while (BinaryPath.Contains(".app"))
				{
					BinaryPath = Path.GetDirectoryName(BinaryPath);
				}
				BinaryPath = Path.Combine(BinaryPath, Path.GetFileName(Path.ChangeExtension(MachOBinary.AbsolutePath, ".dSYM")));
			}
			else
			{
				BinaryPath = Path.ChangeExtension(BinaryPath, ".dSYM");
			}

			FileItem OutputFile = FileItem.GetItemByPath(BinaryPath);
			FileItem DestFile = LocalToRemoteFileItem(OutputFile, false);
			FileItem InputFile = LocalToRemoteFileItem(MachOBinary, false);

			// Delete on the local machine
			if (Directory.Exists(OutputFile.AbsolutePath))
			{
				Directory.Delete(OutputFile.AbsolutePath, true);
			}

			// Make the compile action
			Action GenDebugAction = ActionGraph.Add(ActionType.GenerateDebugInfo);
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				DebugOutputMap.Add(GenDebugAction, OutputFile.AbsolutePath);
				GenDebugAction.ActionHandler = new Action.BlockingActionHandler(MacToolChain.RPCDebugInfoActionHandler);
			}
			GenDebugAction.WorkingDirectory = GetMacDevSrcRoot();
			GenDebugAction.CommandPath = "sh";

			// Deletes ay existing file on the building machine. Also, waits 30 seconds, if needed, for the input file to be created in an attempt to work around
			// a problem where dsymutil would exit with an error saying the input file did not exist.
			// Note that the source and dest are switched from a copy command
			GenDebugAction.CommandArguments = string.Format("-c 'rm -rf \"{2}\"; for i in {{1..30}}; do if [ -f \"{1}\" ] ; then break; else echo\"Waiting for {1} before generating dSYM file.\"; sleep 1; fi; done; \"{0}\"dsymutil -f \"{1}\" -o \"{2}\"'",
				Settings.ToolchainDir,
				InputFile.AbsolutePath,
				DestFile.AbsolutePath);
			GenDebugAction.PrerequisiteItems.Add(FixDylibOutputFile);
			GenDebugAction.PrerequisiteItems.Add(LocalToRemoteFileItem(InputFile, false));
			GenDebugAction.ProducedItems.Add(DestFile);
			GenDebugAction.CommandDescription = "";
			GenDebugAction.StatusDescription = "Generating " + Path.GetFileName(BinaryPath);
			GenDebugAction.bCanExecuteRemotely = false;

			return DestFile;
		}

		/// <summary>
		/// Creates app bundle for a given executable
		/// </summary>
		/// <param name="LinkEnvironment"></param>
		/// <param name="Executable">FileItem describing the executable to generate app bundle for</param>
		/// <param name="FixDylibOutputFile"></param>
		/// <param name="ActionGraph"></param>
		FileItem FinalizeAppBundle(LinkEnvironment LinkEnvironment, FileItem Executable, FileItem FixDylibOutputFile, ActionGraph ActionGraph)
		{
			// Make a file item for the source and destination files
			string FullDestPath = Executable.AbsolutePath.Substring(0, Executable.AbsolutePath.IndexOf(".app") + 4);
			FileItem DestFile = FileItem.GetItemByPath(FullDestPath);
			FileItem RemoteDestFile = LocalToRemoteFileItem(DestFile, false);

			// Make the compile action
			Action FinalizeAppBundleAction = ActionGraph.Add(ActionType.CreateAppBundle);
			FinalizeAppBundleAction.WorkingDirectory = GetMacDevSrcRoot(); // Path.GetFullPath(".");
			FinalizeAppBundleAction.CommandPath = "/bin/sh";
			FinalizeAppBundleAction.CommandDescription = "";

			// make path to the script
			FileItem BundleScript = FileItem.GetItemByFileReference(FileReference.Combine(LinkEnvironment.IntermediateDirectory, "FinalizeAppBundle.sh"));
			FileItem RemoteBundleScript = LocalToRemoteFileItem(BundleScript, true);

			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				FinalizeAppBundleAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			}

			FinalizeAppBundleAction.CommandArguments = "\"" + RemoteBundleScript.AbsolutePath + "\"";
			FinalizeAppBundleAction.PrerequisiteItems.Add(FixDylibOutputFile);
			FinalizeAppBundleAction.ProducedItems.Add(RemoteDestFile);
			FinalizeAppBundleAction.StatusDescription = string.Format("Finalizing app bundle: {0}.app", Path.GetFileName(Executable.AbsolutePath));
			FinalizeAppBundleAction.bCanExecuteRemotely = false;

			return RemoteDestFile;
		}

		FileItem CopyBundleResource(UEBuildBundleResource Resource, FileItem Executable, DirectoryReference BundleDirectory, ActionGraph ActionGraph)
		{
			Action CopyAction = ActionGraph.Add(ActionType.CreateAppBundle);
			CopyAction.WorkingDirectory = GetMacDevSrcRoot(); // Path.GetFullPath(".");
			CopyAction.CommandPath = "/bin/sh";
			CopyAction.CommandDescription = "";

			string BundlePath = BundleDirectory.FullName;
			string SourcePath = Path.Combine(Path.GetFullPath("."), Resource.ResourcePath);
			string TargetPath = Path.Combine(BundlePath, "Contents", Resource.BundleContentsSubdir, Path.GetFileName(Resource.ResourcePath));

			FileItem TargetItem;
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
			{
				TargetItem = FileItem.GetItemByPath(TargetPath);
			}
			else
			{
				TargetItem = FileItem.GetRemoteItemByPath(TargetPath, RemoteToolChainPlatform);
			}

			CopyAction.CommandArguments = string.Format("-c 'cp -f -R \"{0}\" \"{1}\"; touch -c \"{2}\"'", ConvertPath(SourcePath), Path.GetDirectoryName(TargetPath).Replace('\\', '/') + "/", TargetPath.Replace('\\', '/'));
			CopyAction.PrerequisiteItems.Add(Executable);
			CopyAction.ProducedItems.Add(TargetItem);
			CopyAction.bShouldOutputStatusDescription = Resource.bShouldLog;
			CopyAction.StatusDescription = string.Format("Copying {0} to app bundle", Path.GetFileName(Resource.ResourcePath));
			CopyAction.bCanExecuteRemotely = false;

			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				CopyAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			}

			if (Directory.Exists(Resource.ResourcePath))
			{
				foreach (string ResourceFile in Directory.GetFiles(Resource.ResourcePath, "*", SearchOption.AllDirectories))
				{
					QueueFileForBatchUpload(FileItem.GetItemByFileReference(new FileReference(ResourceFile)));
				}
			}
			else
			{
				QueueFileForBatchUpload(FileItem.GetItemByFileReference(new FileReference(SourcePath)));
			}

			return TargetItem;
		}

		public override void SetupBundleDependencies(List<UEBuildBinary> Binaries, string GameName)
		{
			base.SetupBundleDependencies(Binaries, GameName);

			foreach (UEBuildBinary Binary in Binaries)
			{
				BundleDependencies.Add(FileItem.GetItemByFileReference(Binary.Config.OutputFilePath));
			}
		}

		public override void FixBundleBinariesPaths(UEBuildTarget Target, List<UEBuildBinary> Binaries)
		{
			base.FixBundleBinariesPaths(Target, Binaries);

			string BundleContentsPath = Target.OutputPath.FullName + ".app/Contents/";
			foreach (UEBuildBinary Binary in Binaries)
			{
				string BinaryFileName = Path.GetFileName(Binary.Config.OutputFilePath.FullName);
				if (BinaryFileName.EndsWith(".dylib"))
				{
					// Only dylibs from the same folder as the executable should be moved to the bundle. UE4Editor-*Game* dylibs and plugins will be loaded
					// from their Binaries/Mac folders.
					string DylibDir = Path.GetDirectoryName(Path.GetFullPath(Binary.Config.OutputFilePath.FullName));
					string ExeDir = Path.GetDirectoryName(Path.GetFullPath(Target.OutputPath.FullName));
					if (DylibDir.StartsWith(ExeDir))
					{
						// get the subdir, which is the DylibDir - ExeDir
						string SubDir = DylibDir.Replace(ExeDir, "");
						Binary.Config.OutputFilePaths[0] = new FileReference(BundleContentsPath + "MacOS" + SubDir + "/" + BinaryFileName);
					}
				}
				else if (!BinaryFileName.EndsWith(".a") && !Binary.Config.OutputFilePath.FullName.Contains(".app/Contents/MacOS/")) // Binaries can contain duplicates
				{
					Binary.Config.OutputFilePaths[0] += ".app/Contents/MacOS/" + BinaryFileName;
				}
			}
		}

		static private DirectoryReference BundleContentsDirectory;

		public static Dictionary<UEBuildBinary, Dictionary<FileReference, BuildProductType>> AllBuildProducts = new Dictionary<UEBuildBinary, Dictionary<FileReference, BuildProductType>>();

		public override void ModifyBuildProducts(ReadOnlyTargetRules Target, UEBuildBinary Binary, List<string> Libraries, List<UEBuildBundleResource> BundleResources, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
			if (Target.bUsePDBFiles == true)
			{
				KeyValuePair<FileReference, BuildProductType>[] BuildProductsArray = BuildProducts.ToArray();

				foreach (KeyValuePair<FileReference, BuildProductType> BuildProductPair in BuildProductsArray)
				{
					string DebugExtension = "";
					switch (BuildProductPair.Value)
					{
						case BuildProductType.Executable:
							DebugExtension = UEBuildPlatform.GetBuildPlatform(Target.Platform).GetDebugInfoExtension(Target, UEBuildBinaryType.Executable);
							break;
						case BuildProductType.DynamicLibrary:
							DebugExtension = UEBuildPlatform.GetBuildPlatform(Target.Platform).GetDebugInfoExtension(Target, UEBuildBinaryType.DynamicLinkLibrary);
							break;
					}
					if (DebugExtension == ".dSYM")
					{
						string BinaryPath = BuildProductPair.Key.FullName;
						if(BinaryPath.Contains(".app"))
						{
							while(BinaryPath.Contains(".app"))
							{
								BinaryPath = Path.GetDirectoryName(BinaryPath);
							}
							BinaryPath = Path.Combine(BinaryPath, BuildProductPair.Key.GetFileName());
							BinaryPath = Path.ChangeExtension(BinaryPath, DebugExtension);
							FileReference Ref = new FileReference(BinaryPath);
							BuildProducts[Ref] = BuildProductType.SymbolFile;
						}
					}
					else if(BuildProductPair.Value == BuildProductType.SymbolFile && BuildProductPair.Key.FullName.Contains(".app"))
					{
						BuildProducts.Remove(BuildProductPair.Key);
					}
				}
			}

			if (Target.bIsBuildingConsoleApplication)
			{
				return;
			}

			if (BundleContentsDirectory == null && Binary.Config.Type == UEBuildBinaryType.Executable)
			{
				BundleContentsDirectory = Binary.Config.OutputFilePath.Directory.ParentDirectory;
			}

			// We need to know what third party dylibs would be copied to the bundle
			if (Binary.Config.Type != UEBuildBinaryType.StaticLibrary)
			{
			    foreach (string AdditionalLibrary in Libraries)
				{
					string LibName = Path.GetFileName(AdditionalLibrary);
					if (LibName.StartsWith("lib"))
					{
						if (Path.GetExtension(AdditionalLibrary) == ".dylib" && BundleContentsDirectory != null)
						{
							FileReference Entry = FileReference.Combine(BundleContentsDirectory, "MacOS", LibName);
							if (!BuildProducts.ContainsKey(Entry))
							{
								BuildProducts.Add(Entry, BuildProductType.DynamicLibrary);
							}
						}
					}
				}

			    foreach (UEBuildBundleResource Resource in BundleResources)
				{
					if (Directory.Exists(Resource.ResourcePath))
					{
						foreach (string ResourceFile in Directory.GetFiles(Resource.ResourcePath, "*", SearchOption.AllDirectories))
						{
							BuildProducts.Add(FileReference.Combine(BundleContentsDirectory, Resource.BundleContentsSubdir, ResourceFile.Substring(Path.GetDirectoryName(Resource.ResourcePath).Length + 1)), BuildProductType.RequiredResource);
						}
					}
					else
					{
						BuildProducts.Add(FileReference.Combine(BundleContentsDirectory, Resource.BundleContentsSubdir, Path.GetFileName(Resource.ResourcePath)), BuildProductType.RequiredResource);
					}
				}
			}

			if (Binary.Config.Type == UEBuildBinaryType.Executable)
			{
				// And we also need all the resources
				BuildProducts.Add(FileReference.Combine(BundleContentsDirectory, "Info.plist"), BuildProductType.RequiredResource);
				BuildProducts.Add(FileReference.Combine(BundleContentsDirectory, "PkgInfo"), BuildProductType.RequiredResource);

				if (Target.Type == TargetType.Editor)
				{
					BuildProducts.Add(FileReference.Combine(BundleContentsDirectory, "Resources/UE4Editor.icns"), BuildProductType.RequiredResource);
					BuildProducts.Add(FileReference.Combine(BundleContentsDirectory, "Resources/UProject.icns"), BuildProductType.RequiredResource);
				}
				else
				{
					string IconName = Target.Name;
					if (IconName == "EpicGamesBootstrapLauncher")
					{
						IconName = "EpicGamesLauncher";
					}
					BuildProducts.Add(FileReference.Combine(BundleContentsDirectory, "Resources/" + IconName + ".icns"), BuildProductType.RequiredResource);
				}
			}

			AllBuildProducts.Add(Binary, new Dictionary<FileReference, BuildProductType>(BuildProducts));
		}

		public static void PostBuildSync(UEBuildTarget InTarget)
		{
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				List<string> BuiltBinaries = new List<string>();
				foreach (UEBuildBinary Binary in InTarget.AppBinaries)
				{
					BuiltBinaries.Add(Path.GetFullPath(Binary.ToString()));

					string DebugExtension = UEBuildPlatform.GetBuildPlatform(InTarget.Platform).GetDebugInfoExtension(InTarget.Rules, Binary.Config.Type);
					if (DebugExtension == ".dSYM")
					{
						Dictionary<FileReference, BuildProductType> BuildProducts = AllBuildProducts[Binary];
						foreach (KeyValuePair<FileReference, BuildProductType> BuildProductPair in BuildProducts)
						{
							if (BuildProductPair.Value == BuildProductType.SymbolFile)
							{
								BuiltBinaries.Add(BuildProductPair.Key.FullName);
							}
						}
					}
				}

				string IntermediateDirectory = InTarget.EngineIntermediateDirectory.FullName;
				if (!Directory.Exists(IntermediateDirectory))
				{
					IntermediateDirectory = Path.GetFullPath("../../" + InTarget.AppName + "/Intermediate/Build/Mac/" + InTarget.AppName + "/" + InTarget.Configuration);
					if (!Directory.Exists(IntermediateDirectory))
					{
						IntermediateDirectory = Path.GetFullPath("../../Engine/Intermediate/Build/Mac/" + InTarget.AppName + "/" + InTarget.Configuration);
					}
				}

				string FixDylibDepsScript = Path.Combine(IntermediateDirectory, "FixDylibDependencies.sh");
				string FinalizeAppBundleScript = Path.Combine(IntermediateDirectory, "FinalizeAppBundle.sh");

				string RemoteWorkingDir = "";

				bool bIsStaticLibrary = InTarget.OutputPath.HasExtension(".a");

				if (!bIsStaticLibrary)
				{
					// Copy the command scripts to the intermediate on the target Mac.
					string RemoteFixDylibDepsScript = ConvertPath(Path.GetFullPath(FixDylibDepsScript));
					RemoteFixDylibDepsScript = RemoteFixDylibDepsScript.Replace("../../../../", "../../");
					RPCUtilHelper.CopyFile(Path.GetFullPath(FixDylibDepsScript), RemoteFixDylibDepsScript, true);

					if (!InTarget.Rules.bIsBuildingConsoleApplication)
					{
						string RemoteFinalizeAppBundleScript = ConvertPath(Path.GetFullPath(FinalizeAppBundleScript));
						RemoteFinalizeAppBundleScript = RemoteFinalizeAppBundleScript.Replace("../../../../", "../../");
						RPCUtilHelper.CopyFile(Path.GetFullPath(FinalizeAppBundleScript), RemoteFinalizeAppBundleScript, true);
					}


					// run it remotely
					RemoteWorkingDir = ConvertPath(Path.GetDirectoryName(Path.GetFullPath(FixDylibDepsScript)));

					Log.TraceInformation("Running FixDylibDependencies.sh...");
					Hashtable Results = RPCUtilHelper.Command(RemoteWorkingDir, "/bin/sh", "FixDylibDependencies.sh", null);
					if (Results != null)
					{
						string Result = (string)Results["CommandOutput"];
						if (Result != null)
						{
							Log.TraceInformation(Result);
						}
					}

					if (!InTarget.Rules.bIsBuildingConsoleApplication)
					{
						Log.TraceInformation("Running FinalizeAppBundle.sh...");
						Results = RPCUtilHelper.Command(RemoteWorkingDir, "/bin/sh", "FinalizeAppBundle.sh", null);
						if (Results != null)
						{
							string Result = (string)Results["CommandOutput"];
							if (Result != null)
							{
								Log.TraceInformation(Result);
							}
						}
					}
				}


				// If it is requested, send the app bundle back to the platform executing these commands.
				if (InTarget.Rules.bCopyAppBundleBackToDevice)
				{
					Log.TraceInformation("Copying binaries back to this device...");

					try
					{
						string BinaryDir = InTarget.OutputPath.Directory + "\\";
						if (BinaryDir.EndsWith(InTarget.AppName + "\\Binaries\\Mac\\") && InTarget.TargetType != TargetType.Game)
						{
							BinaryDir = BinaryDir.Replace(InTarget.TargetType.ToString(), "Game");
						}

						string RemoteBinariesDir = ConvertPath(BinaryDir);
						string LocalBinariesDir = BinaryDir;

						// Get the app bundle's name
						string AppFullName = InTarget.AppName;
						if (InTarget.Configuration != InTarget.Rules.UndecoratedConfiguration)
						{
							AppFullName += "-" + InTarget.Platform.ToString();
							AppFullName += "-" + InTarget.Configuration.ToString();
						}

						if (!InTarget.Rules.bIsBuildingConsoleApplication)
						{
							AppFullName += ".app";
						}

						List<string> NotBundledBinaries = new List<string>();
						foreach (string BinaryPath in BuiltBinaries)
						{
							if (InTarget.Rules.bIsBuildingConsoleApplication || bIsStaticLibrary || !BinaryPath.StartsWith(LocalBinariesDir + AppFullName))
							{
								NotBundledBinaries.Add(BinaryPath);
							}
						}

						// Zip the app bundle for transferring.
						if (!InTarget.Rules.bIsBuildingConsoleApplication && !bIsStaticLibrary)
						{
							string ZipCommand = "zip -0 -r -y -T \"" + AppFullName + ".zip\" \"" + AppFullName + "\"";
							RPCUtilHelper.Command(RemoteBinariesDir, ZipCommand, "", null);

							// Copy the AppBundle back to the source machine
							string LocalZipFileLocation = LocalBinariesDir + AppFullName + ".zip ";
							string RemoteZipFileLocation = RemoteBinariesDir + AppFullName + ".zip";

							RPCUtilHelper.CopyFile(RemoteZipFileLocation, LocalZipFileLocation, false);

							// Extract the copied app bundle (in zip format) to the local binaries directory
							using (ZipFile AppBundleZip = ZipFile.Read(LocalZipFileLocation))
							{
								foreach (ZipEntry Entry in AppBundleZip)
								{
									Entry.Extract(LocalBinariesDir, ExtractExistingFileAction.OverwriteSilently);
								}
							}

							// Delete the zip as we no longer need/want it.
							File.Delete(LocalZipFileLocation);
							RPCUtilHelper.Command(RemoteBinariesDir, "rm -f \"" + AppFullName + ".zip\"", "", null);
						}

						if (NotBundledBinaries.Count > 0)
						{

							foreach (string BinaryPath in NotBundledBinaries)
							{
								RPCUtilHelper.CopyFile(ConvertPath(BinaryPath), BinaryPath, false);
							}
						}

						Log.TraceInformation("Copied binaries successfully.");
					}
					catch (Exception)
					{
						Log.TraceInformation("Copying binaries back to this device failed.");
					}
				}
			}
		}

		public override ICollection<FileItem> PostBuild(FileItem Executable, LinkEnvironment BinaryLinkEnvironment, ActionGraph ActionGraph)
		{
			var OutputFiles = base.PostBuild(Executable, BinaryLinkEnvironment, ActionGraph);

			if (BinaryLinkEnvironment.bIsBuildingLibrary)
			{
				return OutputFiles;
			}

			if(BinaryLinkEnvironment.BundleDirectory != null)
			{
				foreach (UEBuildBundleResource Resource in BinaryLinkEnvironment.AdditionalBundleResources)
				{
					OutputFiles.Add(CopyBundleResource(Resource, Executable, BinaryLinkEnvironment.BundleDirectory, ActionGraph));
				}
			}

			// For Mac, generate the dSYM file if the config file is set to do so
			if (BinaryLinkEnvironment.bUsePDBFiles == true)
			{
				// We want dsyms to be created after all dylib dependencies are fixed. If FixDylibDependencies action was not created yet, save the info for later.
				if (FixDylibOutputFile != null)
				{
					OutputFiles.Add(GenerateDebugInfo(Executable, BinaryLinkEnvironment, ActionGraph));
				}
				else
				{
					ExecutablesThatNeedDsyms.Add(Executable);
				}
			}

			// If building for Mac on a Mac, use actions to finalize the builds (otherwise, we use Deploy)
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				return OutputFiles;
			}

			if (BinaryLinkEnvironment.bIsBuildingDLL)
			{
				return OutputFiles;
			}

			FixDylibOutputFile = FixDylibDependencies(BinaryLinkEnvironment, Executable, ActionGraph);
			OutputFiles.Add(FixDylibOutputFile);
			if (!BinaryLinkEnvironment.bIsBuildingConsoleApplication)
			{
				OutputFiles.Add(FinalizeAppBundle(BinaryLinkEnvironment, Executable, FixDylibOutputFile, ActionGraph));
			}

			// Add dsyms that we couldn't add before FixDylibDependencies action was created
			foreach (FileItem Exe in ExecutablesThatNeedDsyms)
			{
				OutputFiles.Add(GenerateDebugInfo(Exe, BinaryLinkEnvironment, ActionGraph));
			}
			ExecutablesThatNeedDsyms.Clear();

			return OutputFiles;
		}

		private FileItem FixDylibOutputFile = null;
		private List<FileItem> ExecutablesThatNeedDsyms = new List<FileItem>();
		private FileReference DylibCopyScriptPath = null;

		public void StripSymbols(FileReference SourceFile, FileReference TargetFile)
		{
			SetupXcodePaths(false);

			StripSymbolsWithXcode(SourceFile, TargetFile, Settings.ToolchainDir);
		}
	};
}
