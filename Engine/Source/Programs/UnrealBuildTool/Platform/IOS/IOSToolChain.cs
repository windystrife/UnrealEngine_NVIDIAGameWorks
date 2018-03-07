// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Diagnostics;
using System.Security.AccessControl;
using System.Xml;
using System.Text;
using Ionic.Zip;
using Ionic.Zlib;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	class IOSToolChainSettings : AppleToolChainSettings
	{
		/// <summary>
		/// Which version of the iOS SDK to target at build time
		/// </summary>
		[XmlConfigFile(Category = "IOSToolChain")]
		public string IOSSDKVersion = "latest";
		public readonly float IOSSDKVersionFloat = 0.0f;

		/// <summary>
		/// Which version of the iOS to allow at build time
		/// </summary>
		[XmlConfigFile(Category = "IOSToolChain")]
		public string BuildIOSVersion = "7.0";

		/// <summary>
		/// Directory for the developer binaries
		/// </summary>
		public string ToolchainDir = "";

		/// <summary>
		/// Location of the SDKs
		/// </summary>
		public readonly string BaseSDKDir;
		public readonly string BaseSDKDirSim;

		public readonly string DevicePlatformName;
		public readonly string SimulatorPlatformName;

		public IOSToolChainSettings() : this("iPhoneOS", "iPhoneSimulator")
		{
		}

		protected IOSToolChainSettings(string DevicePlatformName, string SimulatorPlatformName) : base(true)
		{
			XmlConfig.ApplyTo(this);

			this.DevicePlatformName = DevicePlatformName;
			this.SimulatorPlatformName = SimulatorPlatformName;

			// update cached paths
			BaseSDKDir = XcodeDeveloperDir + "Platforms/" + DevicePlatformName + ".platform/Developer/SDKs";
			BaseSDKDirSim = XcodeDeveloperDir + "Platforms/" + SimulatorPlatformName + ".platform/Developer/SDKs";
			ToolchainDir = XcodeDeveloperDir + "Toolchains/XcodeDefault.xctoolchain/usr/bin/";

			// make sure SDK is selected
			SelectSDK(BaseSDKDir, DevicePlatformName, ref IOSSDKVersion, true);

			// convert to float for easy comparison
			IOSSDKVersionFloat = float.Parse(IOSSDKVersion, System.Globalization.CultureInfo.InvariantCulture);
		}
	}

	class IOSToolChain : AppleToolChain
	{
		protected IOSProjectSettings ProjectSettings;

		public IOSToolChain(FileReference InProjectFile, IOSProjectSettings InProjectSettings)
			: this(CppPlatform.IOS, InProjectFile, InProjectSettings, () => new IOSToolChainSettings())
		{
		}

		protected IOSToolChain(CppPlatform TargetPlatform, FileReference InProjectFile, IOSProjectSettings InProjectSettings, Func<IOSToolChainSettings> InCreateSettings)
			: base(TargetPlatform, UnrealTargetPlatform.Mac, InProjectFile)
		{
			ProjectSettings = InProjectSettings;
			Settings = new Lazy<IOSToolChainSettings>(InCreateSettings);
		}

		// ***********************************************************************
		// * NOTE:
		// *  Do NOT change the defaults to set your values, instead you should set the environment variables
		// *  properly in your system, as other tools make use of them to work properly!
		// *  The defaults are there simply for examples so you know what to put in your env vars...
		// ***********************************************************************

		// If you are looking for where to change the remote compile server name, look in RemoteToolChain.cs

		/// <summary>
		/// If this is set, then we don't do any post-compile steps except moving the executable into the proper spot on the Mac
		/// </summary>
		[XmlConfigFile]
		public static bool bUseDangerouslyFastMode = false;

		/// <summary>
		/// The lazily constructed settings for the toolchain
		/// </summary>
		private Lazy<IOSToolChainSettings> Settings;

		/// <summary>
		/// Which compiler frontend to use
		/// </summary>
		private const string IOSCompiler = "clang++";

		/// <summary>
		/// Which linker frontend to use
		/// </summary>
		private const string IOSLinker = "clang++";

		/// <summary>
		/// Which library archiver to use
		/// </summary>
		private const string IOSArchiver = "libtool";

		public static List<FileReference> BuiltBinaries = new List<FileReference>();

		/// <summary>
		/// Additional frameworks stored locally so we have access without LinkEnvironment
		/// </summary>
		public static List<UEBuildFramework> RememberedAdditionalFrameworks = new List<UEBuildFramework>();

        public override string GetSDKVersion()
        {
            return Settings.Value.IOSSDKVersionFloat.ToString();
        }

		public override void ModifyBuildProducts(ReadOnlyTargetRules Target, UEBuildBinary Binary, List<string> Libraries, List<UEBuildBundleResource> BundleResources, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
			if (Target.bCreateStubIPA && Binary.Config.Type != UEBuildBinaryType.StaticLibrary)
			{
				FileReference StubFile = FileReference.Combine(Binary.Config.OutputFilePath.Directory, Binary.Config.OutputFilePath.GetFileNameWithoutExtension() + ".stub");
				BuildProducts.Add(StubFile, BuildProductType.Package);
                if (CppPlatform == CppPlatform.TVOS)
                {
                    FileReference AssetFile = FileReference.Combine(Binary.Config.OutputFilePath.Directory, "AssetCatalog", "Assets.car");
                    BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
                }
			}
            if ((ProjectSettings.bGeneratedSYMFile == true || ProjectSettings.bGeneratedSYMBundle == true) && ProjectSettings.bGenerateCrashReportSymbols && Binary.Config.Type == UEBuildBinaryType.Executable)
            {
                FileReference DebugFile = FileReference.Combine(Binary.Config.OutputFilePath.Directory, Binary.Config.OutputFilePath.GetFileNameWithoutExtension() + ".udebugsymbols");
                BuildProducts.Add(DebugFile, BuildProductType.SymbolFile);
            }
        }

		/// <summary>
		/// Adds a build product and its associated debug file to a receipt.
		/// </summary>
		/// <param name="OutputFile">Build product to add</param>
		/// <param name="OutputType">Type of build product</param>
		public override bool ShouldAddDebugFileToReceipt(FileReference OutputFile, BuildProductType OutputType)
		{
			return OutputType == BuildProductType.Executable;
		}

		string GetCompileArguments_Global(CppCompileEnvironment CompileEnvironment)
		{
            string Result = "";

			Result += " -fmessage-length=0";
			Result += " -pipe";
			Result += " -fpascal-strings";


			// Optionally enable exception handling (off by default since it generates extra code needed to propagate exceptions)
			if (CompileEnvironment.bEnableExceptions)
			{
				Result += " -fexceptions";
			}
			else
			{
				Result += " -fno-exceptions";
			}

			if (CompileEnvironment.bEnableObjCExceptions)
			{
				Result += " -fobjc-exceptions";
			}
			else
			{
				Result += " -fno-objc-exceptions";
			}

			string SanitizerMode = Environment.GetEnvironmentVariable("ENABLE_ADDRESS_SANITIZER");
			if(SanitizerMode != null && SanitizerMode == "YES")
			{
				Result += " -fsanitize=address";
			}
			
			string UndefSanitizerMode = Environment.GetEnvironmentVariable("ENABLE_UNDEFINED_BEHAVIOR_SANITIZER");
			if(UndefSanitizerMode != null && UndefSanitizerMode == "YES")
			{
				Result += " -fsanitize=undefined -fno-sanitize=bounds,enum,return,float-divide-by-zero";
			}

			Result += GetRTTIFlag(CompileEnvironment);
			Result += " -fvisibility=hidden"; // hides the linker warnings with PhysX

			// 			if (CompileEnvironment.TargetConfiguration == CPPTargetConfiguration.Shipping)
			// 			{
			// 				Result += " -flto";
			// 			}

			Result += " -Wall -Werror";
			Result += " -Wdelete-non-virtual-dtor";

			if (CompileEnvironment.bEnableShadowVariableWarnings)
			{
				Result += " -Wshadow" + (CompileEnvironment.bShadowVariableWarningsAsErrors? "" : " -Wno-error=shadow");
			}

			if (CompileEnvironment.bEnableUndefinedIdentifierWarnings)
			{
				Result += " -Wundef" + (CompileEnvironment.bUndefinedIdentifierWarningsAsErrors ? "" : " -Wno-error=undef");
			}

			Result += " -Wno-unused-variable";
			Result += " -Wno-unused-value";
			// this will hide the warnings about static functions in headers that aren't used in every single .cpp file
			Result += " -Wno-unused-function";
			// this hides the "enumeration value 'XXXXX' not handled in switch [-Wswitch]" warnings - we should maybe remove this at some point and add UE_LOG(, Fatal, ) to default cases
			Result += " -Wno-switch";
			// this hides the "warning : comparison of unsigned expression < 0 is always false" type warnings due to constant comparisons, which are possible with template arguments
			Result += " -Wno-tautological-compare";
			//This will prevent the issue of warnings for unused private variables.
			Result += " -Wno-unused-private-field";
			// needed to suppress warnings about using offsetof on non-POD types.
			Result += " -Wno-invalid-offsetof";
			// we use this feature to allow static FNames.
			Result += " -Wno-gnu-string-literal-operator-template";

			if (Settings.Value.IOSSDKVersionFloat >= 9.0)
			{
				Result += " -Wno-inconsistent-missing-override"; // too many missing overrides...
				Result += " -Wno-unused-local-typedef"; // PhysX has some, hard to remove
			}
			
			// fix for Xcode 8.3 enabling nonportable include checks, but p4 has some invalid cases in it
			if (Settings.Value.IOSSDKVersionFloat >= 10.3)
			{
				Result += " -Wno-nonportable-include-path";
			}

			// fix for Xcode 8.3 enabling nonportable include checks, but p4 has some invalid cases in it
			if (Settings.Value.IOSSDKVersionFloat >= 10.3)
			{
				Result += " -Wno-nonportable-include-path";
			}

			if (IsBitcodeCompilingEnabled(CompileEnvironment.Configuration))
			{
				Result += " -fembed-bitcode";
			}

			Result += " -c";

			// What architecture(s) to build for
			Result += GetArchitectureArgument(CompileEnvironment.Configuration, CompileEnvironment.Architecture);

			if (CompileEnvironment.Architecture == "-simulator")
			{
				Result += " -isysroot " + Settings.Value.BaseSDKDirSim + "/" + Settings.Value.SimulatorPlatformName + Settings.Value.IOSSDKVersion + ".sdk";
			}
			else
			{
				Result += " -isysroot " + Settings.Value.BaseSDKDir + "/" +  Settings.Value.DevicePlatformName + Settings.Value.IOSSDKVersion + ".sdk";
			}

			Result += " -m" +  GetXcodeMinVersionParam() + "=" + ProjectSettings.RuntimeVersion;
			
			bool bStaticAnalysis = false;
			string StaticAnalysisMode = Environment.GetEnvironmentVariable("CLANG_STATIC_ANALYZER_MODE");
			if(StaticAnalysisMode != null && StaticAnalysisMode != "")
			{
				bStaticAnalysis = true;
			}

			// Optimize non- debug builds.
			if (CompileEnvironment.bOptimizeCode && !bStaticAnalysis)
			{
				if (CompileEnvironment.bOptimizeForSize)
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

			// Add additional frameworks so that their headers can be found
			foreach (UEBuildFramework Framework in CompileEnvironment.AdditionalFrameworks)
			{
				if (Framework.OwningModule != null && Framework.FrameworkZipPath != null && Framework.FrameworkZipPath != "")
				{
					Result += " -F\"" + GetRemoteIntermediateFrameworkZipPath(Framework) + "\"";
				}
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

		static string GetLocalFrameworkZipPath(UEBuildFramework Framework)
		{
			if (Framework.OwningModule == null)
			{
				throw new BuildException("GetLocalFrameworkZipPath: No owning module for framework {0}", Framework.FrameworkName);
			}

			return Path.GetFullPath(Framework.OwningModule.ModuleDirectory + "/" + Framework.FrameworkZipPath);
		}

		static string GetRemoteFrameworkZipPath(UEBuildFramework Framework)
		{
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				return ConvertPath(GetLocalFrameworkZipPath(Framework));
			}

			return GetLocalFrameworkZipPath(Framework);
		}

		static string GetRemoteIntermediateFrameworkZipPath(UEBuildFramework Framework)
		{
			if (Framework.OwningModule == null)
			{
				throw new BuildException("GetRemoteIntermediateFrameworkZipPath: No owning module for framework {0}", Framework.FrameworkName);
			}

			string IntermediatePath = UnrealBuildTool.EngineDirectory + "/Intermediate/UnzippedFrameworks/" + Framework.OwningModule.Name;
			IntermediatePath = Path.GetFullPath((IntermediatePath + Framework.FrameworkZipPath).Replace(".zip", ""));

			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				return ConvertPath(IntermediatePath);
			}

			return IntermediatePath;
		}

		static void CleanIntermediateDirectory(string Path)
		{
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				// Delete the intermediate directory on the mac
				RPCUtilHelper.Command("/", String.Format("rm -rf \"{0}\"", Path), "", null);

				// Create a fresh intermediate after we delete it
				RPCUtilHelper.Command("/", String.Format("mkdir -p \"{0}\"", Path), "", null);
			}
			else
			{
				// Delete the local dest directory if it exists
				if (Directory.Exists(Path))
				{
					Directory.Delete(Path, true);
				}

				// Create the intermediate local directory
				string ResultsText;
				RunExecutableAndWait("mkdir", String.Format("-p \"{0}\"", Path), out ResultsText);
			}
		}

		bool IsBitcodeCompilingEnabled(CppConfiguration Configuration)
		{
			return Configuration == CppConfiguration.Shipping && ProjectSettings.bShipForBitcode;
		}

		public virtual string GetXcodeMinVersionParam()
		{
			return "iphoneos-version-min";
		}

		public virtual string GetArchitectureArgument(CppConfiguration Configuration, string UBTArchitecture)
		{
            // get the list of architectures to compile
            string Archs =
				UBTArchitecture == "-simulator" ? "i386" :
				String.Join(",", (Configuration == CppConfiguration.Shipping) ? ProjectSettings.ShippingArchitectures : ProjectSettings.NonShippingArchitectures);

			Log.TraceLogOnce("Compiling with these architectures: " + Archs);

			// parse the string
			string[] Tokens = Archs.Split(",".ToCharArray());


			string Result = "";

			foreach (string Token in Tokens)
			{
				Result += " -arch " + Token;
			}

            //  Remove this in 4.16 
            //  Commented this out, for now. @Pete let's conditionally check this when we re-implement this fix. 
            //  Result += " -mcpu=cortex-a9";

			return Result;
		}

		public string GetAdditionalLinkerFlags(CppConfiguration InConfiguration)
		{
			if (InConfiguration != CppConfiguration.Shipping)
			{
				return ProjectSettings.AdditionalLinkerFlags;
			}
			else
			{
				return ProjectSettings.AdditionalShippingLinkerFlags;
			}
		}

		string GetLinkArguments_Global(LinkEnvironment LinkEnvironment)
		{
            string Result = "";

			Result += GetArchitectureArgument(LinkEnvironment.Configuration, LinkEnvironment.Architecture);

			bool bIsDevice = (LinkEnvironment.Architecture != "-simulator");
			Result += String.Format(" -isysroot {0}Platforms/{1}.platform/Developer/SDKs/{1}{2}.sdk",
				Settings.Value.XcodeDeveloperDir, bIsDevice? Settings.Value.DevicePlatformName : Settings.Value.SimulatorPlatformName, Settings.Value.IOSSDKVersion);

			if(IsBitcodeCompilingEnabled(LinkEnvironment.Configuration))
			{
				FileItem OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
				FileItem RemoteOutputFile = LocalToRemoteFileItem(OutputFile, false);

				Result += " -fembed-bitcode -Xlinker -bitcode_verify -Xlinker -bitcode_hide_symbols -Xlinker -bitcode_symbol_map ";
				Result += " -Xlinker " + Path.GetDirectoryName(RemoteOutputFile.AbsolutePath);
			}

			Result += " -dead_strip";
			Result += " -m" + GetXcodeMinVersionParam() + "=" + ProjectSettings.RuntimeVersion;
			Result += " -Wl";
			if(!IsBitcodeCompilingEnabled(LinkEnvironment.Configuration))
			{
				Result += "-no_pie";
			}
			Result += " -stdlib=libc++";
			Result += " -ObjC";
			//			Result += " -v";
			
			string SanitizerMode = Environment.GetEnvironmentVariable("ENABLE_ADDRESS_SANITIZER");
			if(SanitizerMode != null && SanitizerMode == "YES")
			{
				Result += " -rpath \"@executable_path/Frameworks/libclang_rt.asan_ios_dynamic.dylib\"";
				Result += " -fsanitize=address";
			}
			
			string UndefSanitizerMode = Environment.GetEnvironmentVariable("ENABLE_UNDEFINED_BEHAVIOR_SANITIZER");
			if(UndefSanitizerMode != null && UndefSanitizerMode == "YES")
			{
				Result += " -rpath \"@executable_path/libclang_rt.ubsan_ios_dynamic.dylib\"";
				Result += " -fsanitize=undefined";
			}

			Result += " " + GetAdditionalLinkerFlags(LinkEnvironment.Configuration);

			// link in the frameworks
			foreach (string Framework in LinkEnvironment.Frameworks)
			{
                if (Framework != "ARKit" || Settings.Value.IOSSDKVersionFloat >= 11.0f)
                {
                    Result += " -framework " + Framework;
                }
			}
			foreach (UEBuildFramework Framework in LinkEnvironment.AdditionalFrameworks)
			{
				if (Framework.OwningModule != null && Framework.FrameworkZipPath != null && Framework.FrameworkZipPath != "")
				{
					// If this framework has a zip specified, we'll need to setup the path as well
					Result += " -F\"" + GetRemoteIntermediateFrameworkZipPath(Framework) + "\"";
				}

				Result += " -framework " + Framework.FrameworkName;
			}
			foreach (string Framework in LinkEnvironment.WeakFrameworks)
			{
				Result += " -weak_framework " + Framework;
			}

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
			string Arguments = GetCompileArguments_Global(CompileEnvironment);
			string PCHArguments = "";

			if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
			{
				// Add the precompiled header file's path to the include path so GCC can find it.
				// This needs to be before the other include paths to ensure GCC uses it instead of the source header file.
				var PrecompiledFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS).GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
				PCHArguments += string.Format(" -include \"{0}\"", ConvertPath(CompileEnvironment.PrecompiledHeaderFile.AbsolutePath.Replace(PrecompiledFileExtension, "")));
			}

			foreach(FileReference ForceIncludeFile in CompileEnvironment.ForceIncludeFiles)
			{
				PCHArguments += String.Format(" -include \"{0}\"", ConvertPath(ForceIncludeFile.FullName));
			}

			// Add include paths to the argument list.
			HashSet<string> AllIncludes = new HashSet<string>(CompileEnvironment.IncludePaths.UserIncludePaths);
			AllIncludes.UnionWith(CompileEnvironment.IncludePaths.SystemIncludePaths);
			foreach (string IncludePath in AllIncludes)
			{
				Arguments += string.Format(" -I\"{0}\"", ConvertPath(Path.GetFullPath(IncludePath)));

				if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
				{
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
			}

			foreach (string Definition in CompileEnvironment.Definitions)
			{
				Arguments += string.Format(" -D\"{0}\"", Definition);
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
					FileArguments += PCHArguments;
				}

				// Add the C++ source file and its included files to the prerequisite item list.
				AddPrerequisiteSourceFile(CompileEnvironment, SourceFile, CompileAction.PrerequisiteItems);

				string OutputFilePath = null;
				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					var PrecompiledFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS).GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
					// Add the precompiled header file to the produced item list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							CompileEnvironment.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + PrecompiledFileExtension
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
					var ObjectFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS).GetBinaryExtension(UEBuildBinaryType.Object);
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

				string CompilerPath = Settings.Value.ToolchainDir + IOSCompiler;
				if (!Utils.IsRunningOnMono && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
				{
					CompileAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
				}

				string AllArgs = Arguments + FileArguments + CompileEnvironment.AdditionalArguments;
/*				string SourceText = System.IO.File.ReadAllText(SourceFile.AbsolutePath);
				if (CompileEnvironment.bOptimizeForSize && (SourceFile.AbsolutePath.Contains("ElementBatcher.cpp") || SourceText.Contains("ElementBatcher.cpp") || SourceFile.AbsolutePath.Contains("AnimationRuntime.cpp") || SourceText.Contains("AnimationRuntime.cpp")
					|| SourceFile.AbsolutePath.Contains("AnimEncoding.cpp") || SourceText.Contains("AnimEncoding.cpp") || SourceFile.AbsolutePath.Contains("TextRenderComponent.cpp") || SourceText.Contains("TextRenderComponent.cpp")
					|| SourceFile.AbsolutePath.Contains("SWidget.cpp") || SourceText.Contains("SWidget.cpp") || SourceFile.AbsolutePath.Contains("SCanvas.cpp") || SourceText.Contains("SCanvas.cpp") || SourceFile.AbsolutePath.Contains("ShaderCore.cpp") || SourceText.Contains("ShaderCore.cpp")
                    || SourceFile.AbsolutePath.Contains("ParticleSystemRender.cpp") || SourceText.Contains("ParticleSystemRender.cpp")))
				{
					Console.WriteLine("Forcing {0} to --O3!", SourceFile.AbsolutePath);

					AllArgs = AllArgs.Replace("-Oz", "-O3");
				}*/
				
				// Analyze and then compile using the shell to perform the indirection
				string StaticAnalysisMode = Environment.GetEnvironmentVariable("CLANG_STATIC_ANALYZER_MODE");
				if(StaticAnalysisMode != null && StaticAnalysisMode != "" && OutputFilePath != null)
				{
					string TempArgs = "-c \"" + CompilerPath + " " + AllArgs + " --analyze -Wno-unused-command-line-argument -Xclang -analyzer-output=html -Xclang -analyzer-config -Xclang path-diagnostics-alternate=true -Xclang -analyzer-config -Xclang report-in-main-source-file=true -Xclang -analyzer-disable-checker -Xclang deadcode.DeadStores -o " + OutputFilePath.Replace(".o", ".html") + "; " + CompilerPath + " " + AllArgs + "\"";
					AllArgs = TempArgs;
					CompilerPath = "/bin/sh";
				}

				// RPC utility parameters are in terms of the Mac side
				CompileAction.WorkingDirectory = GetMacDevSrcRoot();
				CompileAction.CommandPath = CompilerPath;
				CompileAction.CommandArguments = AllArgs; // Arguments + FileArguments + CompileEnvironment.AdditionalArguments;
				CompileAction.CommandDescription = "Compile";
				CompileAction.StatusDescription = string.Format("{0}", Path.GetFileName(SourceFile.AbsolutePath));
				CompileAction.bIsGCCCompiler = true;
				// We're already distributing the command by execution on Mac.
				CompileAction.bCanExecuteRemotely = false;
				CompileAction.bShouldOutputStatusDescription = true;
				CompileAction.OutputEventHandler = new DataReceivedEventHandler(RemoteOutputReceivedEventHandler);
			}
			return Result;
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, ActionGraph ActionGraph)
		{
			string LinkerPath = Settings.Value.ToolchainDir +
				(LinkEnvironment.bIsBuildingLibrary ? IOSArchiver : IOSLinker);

			// Create an action that invokes the linker.
			Action LinkAction = ActionGraph.Add(ActionType.Link);

			if (!Utils.IsRunningOnMono && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				LinkAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			}

			// RPC utility parameters are in terms of the Mac side
			LinkAction.WorkingDirectory = GetMacDevSrcRoot();

			// build this up over the rest of the function
			string LinkCommandArguments = LinkEnvironment.bIsBuildingLibrary ? GetArchiveArguments_Global(LinkEnvironment) : GetLinkArguments_Global(LinkEnvironment);

			if (!LinkEnvironment.bIsBuildingLibrary)
			{
				// Add the library paths to the argument list.
				foreach (string LibraryPath in LinkEnvironment.LibraryPaths)
				{
					LinkCommandArguments += string.Format(" -L\"{0}\"", LibraryPath);
				}

				// Add the additional libraries to the argument list.
				foreach (string AdditionalLibrary in LinkEnvironment.AdditionalLibraries)
				{
					// for absolute library paths, convert to remote filename
					if (!String.IsNullOrEmpty(Path.GetDirectoryName(AdditionalLibrary)))
					{
						// add it to the prerequisites to make sure it's built first (this should be the case of non-system libraries)
						FileItem LibFile = FileItem.GetItemByPath(AdditionalLibrary);
						FileItem RemoteLibFile = LocalToRemoteFileItem(LibFile, true);
						LinkAction.PrerequisiteItems.Add(RemoteLibFile);

						// and add to the commandline
						LinkCommandArguments += string.Format(" \"{0}\"", ConvertPath(Path.GetFullPath(AdditionalLibrary)));
					}
					else
					{
						LinkCommandArguments += string.Format(" -l\"{0}\"", AdditionalLibrary);
					}
				}
			}

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
			}

			// Handle additional framework assets that might need to be shadowed
			foreach (UEBuildFramework Framework in LinkEnvironment.AdditionalFrameworks)
			{
				if (Framework.OwningModule == null || Framework.FrameworkZipPath == null || Framework.FrameworkZipPath == "")
				{
					continue;	// Only care about frameworks that have a zip specified
				}

				// If we've already remembered this framework, skip
				if (RememberedAdditionalFrameworks.Contains(Framework))
				{
					continue;
				}

				// Remember any files we need to unzip
				RememberedAdditionalFrameworks.Add(Framework);

				// Copy them to remote mac if needed
				if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
				{
					FileItem ShadowFile = FileItem.GetExistingItemByPath(GetLocalFrameworkZipPath(Framework));

					if (ShadowFile != null)
					{
						QueueFileForBatchUpload(ShadowFile);
						LinkAction.PrerequisiteItems.Add(ShadowFile);
					}
					else
					{
						throw new BuildException("Couldn't find required additional file to shadow: {0}", Framework.FrameworkZipPath);
					}
				}
			}

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
			FileItem RemoteOutputFile = LocalToRemoteFileItem(OutputFile, false);
			LinkAction.ProducedItems.Add(RemoteOutputFile);

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				InputFileNames.Add(string.Format("\"{0}\"", InputFile.AbsolutePath));
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			// Write the list of input files to a response file, with a tempfilename, on remote machine
			if (LinkEnvironment.bIsBuildingLibrary)
			{
				foreach (string Filename in InputFileNames)
				{
					LinkCommandArguments += " " + Filename;
				}
				// @todo rocket lib: the -filelist command should take a response file (see else condition), except that it just says it can't
				// find the file that's in there. Rocket.lib may overflow the commandline by putting all files on the commandline, so this 
				// may be needed:
				// LinkCommandArguments += string.Format(" -filelist \"{0}\"", ConvertPath(ResponsePath));
			}
			else
			{
				bool bIsUE4Game = LinkEnvironment.OutputFilePath.FullName.Contains("UE4Game");
				FileReference ResponsePath = FileReference.Combine(((!bIsUE4Game && ProjectFile != null) ? ProjectFile.Directory : UnrealBuildTool.EngineDirectory), "Intermediate", "Build", LinkEnvironment.Platform.ToString(), "LinkFileList_" + LinkEnvironment.OutputFilePath.GetFileNameWithoutExtension() + ".tmp");
				if (!Utils.IsRunningOnMono && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
				{
					ResponseFile.Create(ResponsePath, InputFileNames);
					RPCUtilHelper.CopyFile(ResponsePath.FullName, ConvertPath(ResponsePath.FullName), true);
				}
				else
				{
					ResponseFile.Create(new FileReference(ConvertPath(ResponsePath.FullName)), InputFileNames);
				}
				LinkCommandArguments += string.Format(" @\"{0}\"", ConvertPath(ResponsePath.FullName));
			}

			// Add the output file to the command-line.
			LinkCommandArguments += string.Format(" -o \"{0}\"", RemoteOutputFile.AbsolutePath);

			// Add the additional arguments specified by the environment.
			LinkCommandArguments += LinkEnvironment.AdditionalArguments;

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			LinkAction.StatusDescription = string.Format("{0}", OutputFile.AbsolutePath);
			LinkAction.OutputEventHandler = new DataReceivedEventHandler(RemoteOutputReceivedEventHandler);

			LinkAction.CommandPath = "sh";
			if(LinkEnvironment.Configuration == CppConfiguration.Shipping && Path.GetExtension(RemoteOutputFile.AbsolutePath) != ".a")
			{
				// When building a shipping package, symbols are stripped from the exe as the last build step. This is a problem
				// when re-packaging and no source files change because the linker skips symbol generation and dsymutil will 
				// recreate a new .dsym file from a symboless exe file. It's just sad. To make things happy we need to delete 
				// the output file to force the linker to recreate it with symbols again.
				string	linkCommandArguments = "-c '";

				linkCommandArguments += string.Format("rm -f \"{0}\";", RemoteOutputFile.AbsolutePath);
				linkCommandArguments += string.Format("rm -f \"{0}\\*.bcsymbolmap\";", Path.GetDirectoryName(RemoteOutputFile.AbsolutePath));
				linkCommandArguments += LinkerPath + " " + LinkCommandArguments + ";";

				linkCommandArguments += "'";

				LinkAction.CommandArguments = linkCommandArguments;
			}
			else
			{
				// This is not a shipping build so no need to delete the output file since symbols will not have been stripped from it.
				LinkAction.CommandArguments = string.Format("-c '{0} {1}'", LinkerPath, LinkCommandArguments);
			}

			return RemoteOutputFile;
		}

        public FileItem CompileAssetCatalog(FileItem Executable, string EngineDir, string BuildDir, string IntermediateDir, ActionGraph ActionGraph, CppPlatform Platform, List<FileItem> Icons)
        {
            // Make a file item for the source and destination files
            FileItem LocalExecutable = RemoteToLocalFileItem(Executable);
            string FullDestPathRoot = Path.Combine(Path.GetDirectoryName(LocalExecutable.AbsolutePath), "AssetCatalog", "Assets.car");

            FileItem OutputFile;
            if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac && Platform == CppPlatform.IOS)
            {
                FullDestPathRoot = Path.Combine(Path.GetDirectoryName(LocalExecutable.AbsolutePath), "Payload", Path.GetFileNameWithoutExtension(LocalExecutable.AbsolutePath) + ".app", "Assets.car");
            }
            OutputFile = FileItem.GetItemByPath(FullDestPathRoot);

            // Make the compile action
            Action CompileAssetAction = ActionGraph.Add(ActionType.CreateAppBundle);
            if (!Utils.IsRunningOnMono)
            {
                CompileAssetAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
            }

            CompileAssetAction.WorkingDirectory = GetMacDevSrcRoot();
            CompileAssetAction.CommandPath = "/usr/bin/xcrun";

            FileItem AssetCat = FileItem.GetItemByPath(Path.Combine(IntermediateDir, "Resources", "Assets.xcassets"));
            FileItem DestFile = LocalToRemoteFileItem(OutputFile, false);
            FileItem InputFile = LocalToRemoteFileItem(AssetCat, BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac);

            string Arguments = "";
            Arguments += " actool --output-format human-readable-text --notices --warnings";
            Arguments += " --output-partial-info-plist \"";
            Arguments += Path.GetDirectoryName(InputFile.AbsolutePath).Replace("\\", "/") + "/assetcatalog_generated_info.plist\"";
            Arguments += " --app-icon AppIcon";
            Arguments += Platform == CppPlatform.TVOS ? " --launch-image LaunchImage" : "";
//            Arguments += " --compress-pngs";
            Arguments += " --enable-on-demand-resources YES";
            Arguments += Platform == CppPlatform.TVOS ? " --filter-for-device-model AppleTV5,3 --filter-for-device-os-version 9.2" : "";
            Arguments += Platform == CppPlatform.TVOS ? " --target-device tv --minimum-deployment-target 9.2 --platform appletvos" : " --target-device iphone --target-device ipad --minimum-deployment-target 9.0 --platform iphoneos --product-type com.apple.product-type.application";
            Arguments += " --compile \"";
            Arguments += Path.GetDirectoryName(DestFile.AbsolutePath).Replace("\\", "/") + "\" \"" + InputFile.AbsolutePath + "\"";

            CompileAssetAction.CommandArguments = Arguments;
            CompileAssetAction.PrerequisiteItems.Add(Executable);
			CompileAssetAction.PrerequisiteItems.AddRange(Icons);
            CompileAssetAction.ProducedItems.Add(DestFile);
            CompileAssetAction.StatusDescription = CompileAssetAction.CommandArguments;// string.Format("Generating debug info for {0}", Path.GetFileName(Executable.AbsolutePath));
            CompileAssetAction.bCanExecuteRemotely = false;

            return DestFile;
        }

        /// <summary>
        /// Generates debug info for a given executable
        /// </summary>
        /// <param name="Executable">FileItem describing the executable to generate debug info for</param>
		/// <param name="ActionGraph"></param>
        public FileItem GenerateDebugInfo(FileItem Executable, ActionGraph ActionGraph)
		{
            // Make a file item for the source and destination files
            FileItem LocalExecutable = RemoteToLocalFileItem(Executable);
			string FullDestPathRoot = Path.Combine(Path.GetDirectoryName(LocalExecutable.AbsolutePath), Path.GetFileName(LocalExecutable.AbsolutePath) + ".dSYM");

            FileItem OutputFile;
            OutputFile = FileItem.GetItemByPath(FullDestPathRoot);
            FileItem ZipOutputFile;
            ZipOutputFile = FileItem.GetItemByPath(FullDestPathRoot + ".zip");
            FileItem DestFile = LocalToRemoteFileItem(OutputFile, false);
            FileItem ZipDestFile = LocalToRemoteFileItem(ZipOutputFile, false);

            // Make the compile action
            Action GenDebugAction = ActionGraph.Add(ActionType.GenerateDebugInfo);
			if (!Utils.IsRunningOnMono)
			{
				GenDebugAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			}

			GenDebugAction.WorkingDirectory = GetMacDevSrcRoot();
			GenDebugAction.CommandPath = "sh";
			if(ProjectSettings.bGeneratedSYMBundle)
			{
				GenDebugAction.CommandArguments = string.Format("-c 'rm -rf \"{1}\"; /usr/bin/dsymutil \"{0}\" -o \"{1}\"; cd \"{1}/..\"; zip -r -y -1 {2}.zip {2}'",
					Executable.AbsolutePath,
                    DestFile.AbsolutePath,
					Path.GetFileName(FullDestPathRoot));
                GenDebugAction.ProducedItems.Add(ZipDestFile);
                Log.TraceInformation("Zip file: " + ZipDestFile.AbsolutePath);
            }
            else
			{
				GenDebugAction.CommandArguments = string.Format("-c 'rm -rf \"{1}\"; /usr/bin/dsymutil \"{0}\" -f -o \"{1}\"'",
						Executable.AbsolutePath,
						DestFile.AbsolutePath);
			}
			GenDebugAction.PrerequisiteItems.Add(Executable);
            GenDebugAction.ProducedItems.Add(DestFile);
            GenDebugAction.StatusDescription = GenDebugAction.CommandArguments;// string.Format("Generating debug info for {0}", Path.GetFileName(Executable.AbsolutePath));
			GenDebugAction.bCanExecuteRemotely = false;

			return (ProjectSettings.bGeneratedSYMBundle ? ZipDestFile : DestFile);
		}

        /// <summary>
        /// Generates pseudo pdb info for a given executable
        /// </summary>
        /// <param name="Executable">FileItem describing the executable to generate debug info for</param>
		/// <param name="ActionGraph"></param>
        public FileItem GeneratePseudoPDB(FileItem Executable, ActionGraph ActionGraph)
        {
            // Make a file item for the source and destination files
            FileItem LocalExecutable = RemoteToLocalFileItem(Executable);
            string FullDestPathRoot = Path.Combine(Path.GetDirectoryName(LocalExecutable.AbsolutePath), Path.GetFileName(LocalExecutable.AbsolutePath) + ".udebugsymbols");
            string FulldSYMPathRoot = Path.Combine(Path.GetDirectoryName(LocalExecutable.AbsolutePath), Path.GetFileName(LocalExecutable.AbsolutePath) + ".dSYM");
            string PathToDWARF = Path.Combine(FulldSYMPathRoot, "Contents", "Resources", "DWARF", Path.GetFileName(LocalExecutable.AbsolutePath));

            FileItem dSYMFile;
            dSYMFile = FileItem.GetItemByPath(FulldSYMPathRoot);
            FileItem dSYMOutFile = LocalToRemoteFileItem(dSYMFile, false);

            FileItem DWARFFile;
            DWARFFile = FileItem.GetItemByPath(PathToDWARF);
            FileItem DWARFOutFile = LocalToRemoteFileItem(DWARFFile, false);

            FileItem OutputFile;
            OutputFile = FileItem.GetItemByPath(FullDestPathRoot);
            FileItem DestFile = LocalToRemoteFileItem(OutputFile, false);

            // Make the compile action
            Action GenDebugAction = ActionGraph.Add(ActionType.GenerateDebugInfo);
            GenDebugAction.WorkingDirectory = GetMacDevEngineRoot() + "/Binaries/Mac/";
            if (!Utils.IsRunningOnMono)
            {
                GenDebugAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
                FileItem DsymExporter = FileItem.GetItemByPath(Path.GetFullPath(Path.Combine(BranchDirectory, "Engine/")) + "/Binaries/Mac/" + "DsymExporter");
                QueueFileForBatchUpload(DsymExporter);
                QueueDirectoryForBatchUpload(DirectoryReference.MakeFromNormalizedFullPath(Path.GetFullPath(Path.Combine(BranchDirectory, "Engine/")) + "Binaries/ThirdParty/ICU/icu4c-53_1/Mac/"));
                QueueDirectoryForBatchUpload(DirectoryReference.MakeFromNormalizedFullPath(Path.GetFullPath(Path.Combine(BranchDirectory, "Engine/")) + "Content/Internationalization/"));
            }

            GenDebugAction.CommandPath = "sh";
            GenDebugAction.CommandArguments = string.Format("-c 'rm -rf \"{1}\"; dwarfdump --uuid {3} | cut -d\" \" -f2; chmod 777 ./DsymExporter; ./DsymExporter -UUID=$(dwarfdump --uuid {3} | cut -d\" \" -f2) \"{0}\" \"{2}\"'",
                    DWARFOutFile.AbsolutePath,
                    DestFile.AbsolutePath,
                    Path.GetDirectoryName(DestFile.AbsolutePath),
                    dSYMOutFile.AbsolutePath);
            GenDebugAction.PrerequisiteItems.Add(dSYMOutFile);
            GenDebugAction.ProducedItems.Add(DestFile);
            GenDebugAction.StatusDescription = GenDebugAction.CommandArguments;// string.Format("Generating debug info for {0}", Path.GetFileName(Executable.AbsolutePath));
            GenDebugAction.bCanExecuteRemotely = false;

            return DestFile;
        }

        private static void PackageStub(string BinaryPath, string GameName, string ExeName)
		{
			// create the ipa
			string IPAName = BinaryPath + "/" + ExeName + ".stub";
			// delete the old one
			if (File.Exists(IPAName))
			{
				File.Delete(IPAName);
			}

			// make the subdirectory if needed
			string DestSubdir = Path.GetDirectoryName(IPAName);
			if (!Directory.Exists(DestSubdir))
			{
				Directory.CreateDirectory(DestSubdir);
			}

			// set up the directories
			string ZipWorkingDir = String.Format("Payload/{0}.app/", GameName);
			string ZipSourceDir = string.Format("{0}/Payload/{1}.app", BinaryPath, GameName);

			// create the file
			using (ZipFile Zip = new ZipFile())
			{
				// add the entire directory
				Zip.AddDirectory(ZipSourceDir, ZipWorkingDir);

				// Update permissions to be UNIX-style
				// Modify the file attributes of any added file to unix format
				foreach (ZipEntry E in Zip.Entries)
				{
					const byte FileAttributePlatform_NTFS = 0x0A;
					const byte FileAttributePlatform_UNIX = 0x03;
					const byte FileAttributePlatform_FAT = 0x00;

					const int UNIX_FILETYPE_NORMAL_FILE = 0x8000;
					//const int UNIX_FILETYPE_SOCKET = 0xC000;
					//const int UNIX_FILETYPE_SYMLINK = 0xA000;
					//const int UNIX_FILETYPE_BLOCKSPECIAL = 0x6000;
					const int UNIX_FILETYPE_DIRECTORY = 0x4000;
					//const int UNIX_FILETYPE_CHARSPECIAL = 0x2000;
					//const int UNIX_FILETYPE_FIFO = 0x1000;

					const int UNIX_EXEC = 1;
					const int UNIX_WRITE = 2;
					const int UNIX_READ = 4;


					int MyPermissions = UNIX_READ | UNIX_WRITE;
					int OtherPermissions = UNIX_READ;

					int PlatformEncodedBy = (E.VersionMadeBy >> 8) & 0xFF;
					int LowerBits = 0;

					// Try to preserve read-only if it was set
					bool bIsDirectory = E.IsDirectory;

					// Check to see if this 
					bool bIsExecutable = false;
					if (Path.GetFileNameWithoutExtension(E.FileName).Equals(GameName, StringComparison.InvariantCultureIgnoreCase))
					{
						bIsExecutable = true;
					}

					if (bIsExecutable)
					{
						// The executable will be encrypted in the final distribution IPA and will compress very poorly, so keeping it
						// uncompressed gives a better indicator of IPA size for our distro builds
						E.CompressionLevel = CompressionLevel.None;
					}

					if ((PlatformEncodedBy == FileAttributePlatform_NTFS) || (PlatformEncodedBy == FileAttributePlatform_FAT))
					{
						FileAttributes OldAttributes = E.Attributes;
						//LowerBits = ((int)E.Attributes) & 0xFFFF;

						if ((OldAttributes & FileAttributes.Directory) != 0)
						{
							bIsDirectory = true;
						}

						// Permissions
						if ((OldAttributes & FileAttributes.ReadOnly) != 0)
						{
							MyPermissions &= ~UNIX_WRITE;
							OtherPermissions &= ~UNIX_WRITE;
						}
					}

					if (bIsDirectory || bIsExecutable)
					{
						MyPermissions |= UNIX_EXEC;
						OtherPermissions |= UNIX_EXEC;
					}

					// Re-jigger the external file attributes to UNIX style if they're not already that way
					if (PlatformEncodedBy != FileAttributePlatform_UNIX)
					{
						int NewAttributes = bIsDirectory ? UNIX_FILETYPE_DIRECTORY : UNIX_FILETYPE_NORMAL_FILE;

						NewAttributes |= (MyPermissions << 6);
						NewAttributes |= (OtherPermissions << 3);
						NewAttributes |= (OtherPermissions << 0);

						// Now modify the properties
						E.AdjustExternalFileAttributes(FileAttributePlatform_UNIX, (NewAttributes << 16) | LowerBits);
					}
				}

				// Save it out
				Zip.Save(IPAName);
			}
		}

		public static new void PreBuildSync()
		{
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				BuiltBinaries = new List<FileReference>();
			}

			RemoteToolChain.PreBuildSync();

			// Unzip any third party frameworks that are stored as zips
			foreach (UEBuildFramework Framework in RememberedAdditionalFrameworks)
			{
				string ZipSrcPath = GetRemoteFrameworkZipPath(Framework);
				string ZipDstPath = GetRemoteIntermediateFrameworkZipPath(Framework);

				Log.TraceInformation("Unzipping: {0} -> {1}", ZipSrcPath, ZipDstPath);

				CleanIntermediateDirectory(ZipDstPath);

				// Assume that there is another directory inside the zip with the same name as the zip
				ZipDstPath = ZipDstPath.Substring(0, ZipDstPath.LastIndexOf('/'));

				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				{
					// If we're on the mac, just unzip using the shell
					string ResultsText;
					RunExecutableAndWait("unzip", String.Format("-o \"{0}\" -d \"{1}\"", ZipSrcPath, ZipDstPath), out ResultsText);
					continue;
				}
				else
				{
					// Use RPC utility if the zip is on remote mac
					Hashtable Result = RPCUtilHelper.Command("/", String.Format("unzip -o \"{0}\" -d \"{1}\"", ZipSrcPath, ZipDstPath), "", null);

					foreach (DictionaryEntry Entry in Result)
					{
						Log.TraceInformation("{0}", Entry.Value);
					}
				}
			}
        }

        public void GenerateAssetCatalog(string EngineDir, string BuildDir, string IntermediateDir, CppPlatform Platform, ref List<FileItem> Icons)
        {
            if (Platform == CppPlatform.TVOS)
            {
                string[] Directories = { "Assets.xcassets",
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconLarge.imagestack"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconLarge.imagestack", "Back.imagestacklayer"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconLarge.imagestack", "Back.imagestacklayer", "Content.imageset"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconLarge.imagestack", "Front.imagestacklayer"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconLarge.imagestack", "Front.imagestacklayer", "Content.imageset"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconLarge.imagestack", "Middle.imagestacklayer"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconLarge.imagestack", "Middle.imagestacklayer", "Content.imageset"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconSmall.imagestack"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconSmall.imagestack", "Back.imagestacklayer"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconSmall.imagestack", "Back.imagestacklayer", "Content.imageset"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconSmall.imagestack", "Front.imagestacklayer"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconSmall.imagestack", "Front.imagestacklayer", "Content.imageset"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconSmall.imagestack", "Middle.imagestacklayer"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconSmall.imagestack", "Middle.imagestacklayer", "Content.imageset"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "TopShelf.imageset"),
                                            Path.Combine("Assets.xcassets", "LaunchImage.launchimage"),
                };
                string[] Contents = { "{\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"assets\" : [\n\t\t{\n\t\t\t\"size\" : \"1280x768\",\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"AppIconLarge.imagestack\",\n\t\t\t\"role\" : \"primary-app-icon\"\n\t\t},\n\t\t{\n\t\t\t\"size\" : \"400x240\",\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"AppIconSmall.imagestack\",\n\t\t\t\"role\" : \"primary-app-icon\"\n\t\t},\n\t\t{\n\t\t\t\"size\" : \"1920x720\",\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"TopShelf.imageset\",\n\t\t\t\"role\" : \"top-shelf-image\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"layers\" : [\n\t\t{\n\t\t\t\"filename\" : \"Front.imagestacklayer\"\n\t\t},\n\t\t{\n\t\t\t\"filename\" : \"Middle.imagestacklayer\"\n\t\t},\n\t\t{\n\t\t\t\"filename\" : \"Back.imagestacklayer\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"images\" : [\n\t\t{\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"Icon_Large_Back.png\",\n\t\t\t\"scale\" : \"1x\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"images\" : [\n\t\t{\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"Icon_Large_Front.png\",\n\t\t\t\"scale\" : \"1x\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"images\" : [\n\t\t{\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"Icon_Large_Middle.png\",\n\t\t\t\"scale\" : \"1x\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"layers\" : [\n\t\t{\n\t\t\t\"filename\" : \"Front.imagestacklayer\"\n\t\t},\n\t\t{\n\t\t\t\"filename\" : \"Middle.imagestacklayer\"\n\t\t},\n\t\t{\n\t\t\t\"filename\" : \"Back.imagestacklayer\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"images\" : [\n\t\t{\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"Icon_Small_Back.png\",\n\t\t\t\"scale\" : \"1x\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"images\" : [\n\t\t{\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"Icon_Small_Front.png\",\n\t\t\t\"scale\" : \"1x\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"images\" : [\n\t\t{\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"Icon_Small_Middle.png\",\n\t\t\t\"scale\" : \"1x\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"images\" : [\n\t\t{\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"TopShelf.png\",\n\t\t\t\"scale\" : \"1x\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"images\" : [\n\t\t{\n\t\t\t\"orientation\" : \"landscape\",\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"Launch.png\",\n\t\t\t\"extent\" : \"full-screen\",\n\t\t\t\"minimum-system-version\" : \"9.0\",\n\t\t\t\"scale\" : \"1x\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                };
                string[] Images = { null,
                                null,
                                null,
                                null,
                                "Icon_Large_Back.png",
                                null,
                                "Icon_Large_Front.png",
                                null,
                                "Icon_Large_Middle.png",
                                null,
                                null,
                                "Icon_Small_Back.png",
                                null,
                                "Icon_Small_Front.png",
                                null,
                                "Icon_Small_Middle.png",
                                "TopShelf.png",
                                "Launch.png"
                };

                // create asset catalog for images
                for (int i = 0; i < Directories.Length; ++i)
                {
                    string Dir = Path.Combine(IntermediateDir, "Resources", Directories[i]);
                    if (!Directory.Exists(Dir))
                    {
                        Directory.CreateDirectory(Dir);
                    }
                    File.WriteAllText(Path.Combine(Dir, "Contents.json"), Contents[i]);
                    LocalToRemoteFileItem(FileItem.GetItemByPath(Path.Combine(Dir, "Contents.json")), BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac);
                    if (Images[i] != null)
                    {
                        string Image = Path.Combine((Directory.Exists(Path.Combine(BuildDir, "Resource", "Graphics")) ? (BuildDir) : (Path.Combine(EngineDir, "Build", "TVOS"))), "Resources", "Graphics", Images[i]);
                        if (File.Exists(Image))
                        {
                            File.Copy(Image, Path.Combine(Dir, Images[i]), true);
                            LocalToRemoteFileItem(FileItem.GetItemByPath(Path.Combine(Dir, Images[i])), BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac);
                            FileInfo DestFileInfo = new FileInfo(Path.Combine(Dir, Images[i]));
                            DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
                        }
                    }
                }
            }
            else
            {
                // copy the template asset catalog to the appropriate directory
                string Dir = Path.Combine(IntermediateDir, "Resources", "Assets.xcassets");
                if (!Directory.Exists(Dir))
                {
                    Directory.CreateDirectory(Dir);
                }
                // create the directories
                foreach (string directory in Directory.EnumerateDirectories(Path.Combine(EngineDir, "Build", "IOS", "Resources", "Assets.xcassets"), "*", SearchOption.AllDirectories))
                {
                    Dir = directory.Replace(Path.Combine(EngineDir, "Build", "IOS"), IntermediateDir);
                    if (!Directory.Exists(Dir))
                    {
                        Directory.CreateDirectory(Dir);
                    }
                }
                // copy the default files
                foreach (string file in Directory.EnumerateFiles(Path.Combine(EngineDir, "Build", "IOS", "Resources", "Assets.xcassets"), "*", SearchOption.AllDirectories))
                {
                    Dir = file.Replace(Path.Combine(EngineDir, "Build", "IOS"), IntermediateDir);
                    File.Copy(file, Dir, true);
                    LocalToRemoteFileItem(FileItem.GetItemByPath(Dir), BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac);
                    FileInfo DestFileInfo = new FileInfo(Dir);
                    DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
                }
                // copy the icons from the game directory if it has any
                string[][] Images = {
                    new string []{ "IPhoneIcon20@2x.png", "Icon20@2x.png", "Icon40.png" },
                    new string []{ "IPhoneIcon20@3x.png", "Icon20@3x.png", "Icon60.png" },
                    new string []{ "IPhoneIcon29@2x.png", "Icon29@2x.png", "Icon58.png" },
                    new string []{ "IPhoneIcon29@3x.png", "Icon29@3x.png", "Icon87.png" },
                    new string []{ "IPhoneIcon40@2x.png", "Icon40@2x.png", "Icon80.png" },
                    new string []{ "IPhoneIcon40@3x.png", "Icon40@3x.png", "Icon60@2x.png", "Icon120.png" },
                    new string []{ "IPhoneIcon60@2x.png", "Icon60@2x.png", "Icon40@3x.png", "Icon120.png" },
                    new string []{ "IPhoneIcon60@3x.png", "Icon60@3x.png", "Icon180.png" },
                    new string []{ "IPadIcon20.png", "Icon20.png" },
                    new string []{ "IPadIcon20@2x.png", "Icon20@2x.png", "Icon40.png" },
                    new string []{ "IPadIcon29.png", "Icon29.png" },
                    new string []{ "IPadIcon29@2x.png", "Icon29@2x.png", "Icon58.png" },
                    new string []{ "IPadIcon40.png", "Icon40.png", "Icon20@2x.png" },
                    new string []{ "IPadIcon76.png", "Icon76.png" },
                    new string []{ "IPadIcon76@2x.png", "Icon76@2x.png", "Icon152.png" },
                    new string []{ "IPadIcon83.5@2x.png", "Icon83.5@2x.png", "Icon167.png" },
                    new string []{ "Icon1024.png", "Icon1024.png" },
                };
                Dir = Path.Combine(IntermediateDir, "Resources", "Assets.xcassets", "AppIcon.appiconset");
                for (int Index = 0; Index < Images.Length; ++Index)
                {
                    string Image = Path.Combine((Directory.Exists(Path.Combine(BuildDir, "Resources", "Graphics")) ? (BuildDir) : (Path.Combine(EngineDir, "Build", "IOS"))), "Resources", "Graphics", Images[Index][1]);
                    if (!File.Exists(Image) && Images[Index].Length > 2)
                    {
                        Image = Path.Combine((Directory.Exists(Path.Combine(BuildDir, "Resources", "Graphics")) ? (BuildDir) : (Path.Combine(EngineDir, "Build", "IOS"))), "Resources", "Graphics", Images[Index][2]);
                    }
                    if (!File.Exists(Image) && Images[Index].Length > 3)
                    {
                        Image = Path.Combine((Directory.Exists(Path.Combine(BuildDir, "Resources", "Graphics")) ? (BuildDir) : (Path.Combine(EngineDir, "Build", "IOS"))), "Resources", "Graphics", Images[Index][3]);
                    }
                    if (File.Exists(Image))
                    {
                        File.Copy(Image, Path.Combine(Dir, Images[Index][0]), true);
						Icons.Add(FileItem.GetItemByPath(Image));
                        LocalToRemoteFileItem(FileItem.GetItemByPath(Path.Combine(Dir, Images[Index][0])), BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac);
                        FileInfo DestFileInfo = new FileInfo(Path.Combine(Dir, Images[Index][0]));
                        DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
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

            // For IOS/tvOS, generate the dSYM file if the config file is set to do so
			if (ProjectSettings.bGeneratedSYMFile == true || ProjectSettings.bGeneratedSYMBundle == true || BinaryLinkEnvironment.bUsePDBFiles == true)
            {
                OutputFiles.Add(GenerateDebugInfo(Executable, ActionGraph));
                if (ProjectSettings.bGenerateCrashReportSymbols)
                {
                    OutputFiles.Add(GeneratePseudoPDB(Executable, ActionGraph));
                }
            }

            // generate the asset catalog
            if (CppPlatform == CppPlatform.TVOS || (CppPlatform == CppPlatform.IOS && Settings.Value.IOSSDKVersionFloat >= 11.0f))
            {
                string EngineDir = UnrealBuildTool.EngineDirectory.ToString();
                string BuildDir = (((ProjectFile != null) ? ProjectFile.Directory.ToString() : (string.IsNullOrEmpty(UnrealBuildTool.GetRemoteIniPath()) ? UnrealBuildTool.EngineDirectory.ToString() : UnrealBuildTool.GetRemoteIniPath()))) + "/Build/" + (BinaryLinkEnvironment.Platform == CppPlatform.IOS ? "IOS" : "TVOS");
                string IntermediateDir = (((ProjectFile != null) ? ProjectFile.Directory.ToString() : UnrealBuildTool.EngineDirectory.ToString())) + "/Intermediate/" + (BinaryLinkEnvironment.Platform == CppPlatform.IOS ? "IOS" : "TVOS");
				List<FileItem> Icons = new List<FileItem>();
                GenerateAssetCatalog(EngineDir, BuildDir, IntermediateDir, BinaryLinkEnvironment.Platform, ref Icons);
                OutputFiles.Add(CompileAssetCatalog(Executable, EngineDir, BuildDir, IntermediateDir, ActionGraph, BinaryLinkEnvironment.Platform, Icons));
            }

            return OutputFiles;
        }

		public static string GetCodesignPlatformName(UnrealTargetPlatform Platform)
		{
			switch(Platform)
			{
				case UnrealTargetPlatform.TVOS:
					return "appletvos";
				case UnrealTargetPlatform.IOS:
					return "iphoneos";
				default:
					throw new BuildException("Invalid platform for GetCodesignPlatformName()");
			}
		}

		private static void GenerateCrashlyticsData(string ExecutableDirectory, string ExecutableName, string ProjectDir, string ProjectName)
        {
            string FabricPath = UnrealBuildTool.EngineDirectory + "/Intermediate/UnzippedFrameworks/ThirdPartyFrameworks/Fabric.embeddedframework";
            if (Directory.Exists(FabricPath))
            {
				Log.TraceInformation("Generating and uploading Crashlytics Data");
				string PlistFile = ProjectDir + "/Intermediate/IOS/" + ProjectName + "-Info.plist";
                Process FabricProcess = new Process();
                FabricProcess.StartInfo.WorkingDirectory = ExecutableDirectory;
                FabricProcess.StartInfo.FileName = "/bin/sh";
				FabricProcess.StartInfo.Arguments = string.Format("-c 'chmod 777 \"{0}/Fabric.framework/upload-symbols\"; \"{0}/Fabric.framework/upload-symbols\" -a 7a4cebd0324af21696e5e321802c5e26ba541cad -p ios {1}'",
                    FabricPath,
					ExecutableDirectory + "/" + ExecutableName + ".dSYM.zip");
                FabricProcess.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);
                FabricProcess.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);

                OutputReceivedDataEventHandlerEncounteredError = false;
                OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
                Utils.RunLocalProcess(FabricProcess);
                if (OutputReceivedDataEventHandlerEncounteredError)
                {
                    throw new Exception(OutputReceivedDataEventHandlerEncounteredErrorMessage);
                }
            }
        }

        public static void PostBuildSync(UEBuildTarget Target)
		{
			IOSProjectSettings ProjectSettings = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS)).ReadProjectSettings(Target.ProjectFile);

			string AppName = Target.TargetType == TargetType.Game ? Target.TargetName : Target.AppName;

			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
			{
				string RemoteShadowDirectoryMac = Path.GetDirectoryName(Target.OutputPath.FullName);
				string FinalRemoteExecutablePath = String.Format("{0}/Payload/{1}.app/{1}", RemoteShadowDirectoryMac, AppName);

				// strip the debug info from the executable if needed
				if (Target.Rules.bStripSymbolsOnIOS || (Target.Configuration == UnrealTargetConfiguration.Shipping))
				{
					Process StripProcess = new Process();
					StripProcess.StartInfo.WorkingDirectory = RemoteShadowDirectoryMac;
					StripProcess.StartInfo.FileName = new IOSToolChainSettings().ToolchainDir + "strip";
					StripProcess.StartInfo.Arguments = "\"" + Target.OutputPath + "\"";
					StripProcess.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);
					StripProcess.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);

					OutputReceivedDataEventHandlerEncounteredError = false;
					OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
					Utils.RunLocalProcess(StripProcess);
					if (OutputReceivedDataEventHandlerEncounteredError)
					{
						throw new Exception(OutputReceivedDataEventHandlerEncounteredErrorMessage);
					}
				}

                // ensure the plist, entitlements, and provision files are properly copied
                var DeployHandler = (Target.Platform == UnrealTargetPlatform.IOS ? new UEDeployIOS() : new UEDeployTVOS());
                DeployHandler.PrepTargetForDeployment(new UEBuildDeployTarget(Target));

				// copy the executable
				if (!File.Exists(FinalRemoteExecutablePath))
				{
					Directory.CreateDirectory(String.Format("{0}/Payload/{1}.app", RemoteShadowDirectoryMac, AppName));
				}
				File.Copy(Target.OutputPath.FullName, FinalRemoteExecutablePath, true);

				GenerateCrashlyticsData(RemoteShadowDirectoryMac, Path.GetFileName(Target.OutputPath.FullName), Target.ProjectDirectory.FullName, AppName);

				if (Target.Rules.bCreateStubIPA)
				{
					string Project = Target.ProjectDirectory + "/" + AppName + ".uproject";

					string SchemeName = AppName;

                    // generate the dummy project so signing works
                    if (AppName == "UE4Game" || AppName == "UE4Client" || Utils.IsFileUnderDirectory(Target.ProjectDirectory + "/" + AppName + ".uproject", Path.GetFullPath("../..")))
					{
						UnrealBuildTool.GenerateProjectFiles(new XcodeProjectFileGenerator(Target.ProjectFile), new string[] { "-platforms=" + (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS"), "-NoIntellIsense", (Target.Platform == UnrealTargetPlatform.IOS ? "-iosdeployonly" : "-tvosdeployonly"), "-ignorejunk" });
						Project = Path.GetFullPath("../..") + "/UE4_" + (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS") + ".xcworkspace";
                        if (AppName == "UE4Game" || AppName == "UE4Client")
                        {
                            SchemeName = "UE4";
                        }
					}
					else
					{
						UnrealBuildTool.GenerateProjectFiles(new XcodeProjectFileGenerator(Target.ProjectFile), new string[] { "-platforms" + (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS"), "-NoIntellIsense", (Target.Platform == UnrealTargetPlatform.IOS ? "-iosdeployonly" : "-tvosdeployonly"), "-ignorejunk", "-project=\"" + Target.ProjectDirectory + "/" + AppName + ".uproject\"", "-game" });
						Project = Target.ProjectDirectory + "/" + AppName + "_" + (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS") + ".xcworkspace";
					}

					if (Directory.Exists(Project))
					{
                        // ensure the plist, entitlements, and provision files are properly copied
                        DeployHandler = (Target.Platform == UnrealTargetPlatform.IOS ? new UEDeployIOS() : new UEDeployTVOS());
                        DeployHandler.PrepTargetForDeployment(new UEBuildDeployTarget(Target));

						var ConfigName = Target.Configuration.ToString();
						if (Target.Rules.Type != TargetType.Game && Target.Rules.Type != TargetType.Program)
						{
							ConfigName += " " + Target.Rules.Type.ToString();
						}

						// code sign the project
						IOSProvisioningData ProvisioningData = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(Target.Platform)).ReadProvisioningData(ProjectSettings);
						string CmdLine = new IOSToolChainSettings().XcodeDeveloperDir + "usr/bin/xcodebuild" +
										" -workspace \"" + Project + "\"" +
										" -configuration \"" + ConfigName + "\"" +
										" -scheme '" + SchemeName + "'" +
										" -sdk " + GetCodesignPlatformName(Target.Platform) +
										" -destination generic/platform=" + (Target.Platform == UnrealTargetPlatform.IOS ? "iOS" : "tvOS") + 
						                (!string.IsNullOrEmpty(ProvisioningData.TeamUUID) ? " DEVELOPMENT_TEAM=" + ProvisioningData.TeamUUID : "");
						if (!ProjectSettings.bAutomaticSigning)
						{
							CmdLine += " CODE_SIGN_IDENTITY=\"" + (!string.IsNullOrEmpty(ProvisioningData.SigningCertificate) ? ProvisioningData.SigningCertificate : "IPhoneDeveloper") + "\"" +
							(!string.IsNullOrEmpty(ProvisioningData.MobileProvisionUUID) ? (" PROVISIONING_PROFILE_SPECIFIER=" + ProvisioningData.MobileProvisionUUID) : "");
						}
						else
						{
							CmdLine += " CODE_SIGN_IDENTITY=\"iPhone Developer\"";
						}

                        Console.WriteLine("Code signing with command line: " + CmdLine);

						Process SignProcess = new Process();
						SignProcess.StartInfo.WorkingDirectory = RemoteShadowDirectoryMac;
						SignProcess.StartInfo.FileName = "/usr/bin/xcrun";
						SignProcess.StartInfo.Arguments = CmdLine;
						SignProcess.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);
						SignProcess.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);

						OutputReceivedDataEventHandlerEncounteredError = false;
						OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
						Utils.RunLocalProcess(SignProcess);

						// delete the temp project
						if (Project.Contains("_" + (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS") + ".xcodeproj"))
						{
							Directory.Delete(Project, true);
						}

						if (OutputReceivedDataEventHandlerEncounteredError)
						{
							throw new Exception(OutputReceivedDataEventHandlerEncounteredErrorMessage);
						}

						// Package the stub
						PackageStub(RemoteShadowDirectoryMac, AppName, Target.OutputPath.GetFileNameWithoutExtension());
					}
				}

				{
					// Copy bundled assets from additional frameworks to the intermediate assets directory (so they can get picked up during staging)
					String LocalFrameworkAssets = Path.GetFullPath(Target.ProjectDirectory + "/Intermediate/" + (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS") + "/FrameworkAssets");

					// Clean the local dest directory if it exists
					CleanIntermediateDirectory(LocalFrameworkAssets);

					foreach (UEBuildFramework Framework in RememberedAdditionalFrameworks)
					{
						if (Framework.OwningModule == null || Framework.CopyBundledAssets == null || Framework.CopyBundledAssets == "")
						{
							continue;		// Only care if we need to copy bundle assets
						}

						string UnpackedZipPath = GetRemoteIntermediateFrameworkZipPath(Framework);

						// For now, this is hard coded, but we need to loop over all modules, and copy bundled assets that need it
						string LocalSource = UnpackedZipPath + "/" + Framework.CopyBundledAssets;
						string BundleName = Framework.CopyBundledAssets.Substring(Framework.CopyBundledAssets.LastIndexOf('/') + 1);
						string LocalDest = LocalFrameworkAssets + "/" + BundleName;

						Log.TraceInformation("Copying bundled asset... LocalSource: {0}, LocalDest: {1}", LocalSource, LocalDest);

						string ResultsText;
                        RunExecutableAndWait("cp", String.Format("-R -L \"{0}\" \"{1}\"", LocalSource, LocalDest), out ResultsText);
                    }
                }
			}
			else
			{
				// store off the binaries
				foreach (UEBuildBinary Binary in Target.AppBinaries)
				{
					BuiltBinaries.Add(Binary.Config.OutputFilePath);
				}

				// check to see if the DangerouslyFast mode is valid (in other words, a build has gone through since a Rebuild/Clean operation)
				FileReference DangerouslyFastValidFile = FileReference.Combine(Target.ProjectIntermediateDirectory, "DangerouslyFastIsNotDangerous");
				bool bUseDangerouslyFastModeWasRequested = bUseDangerouslyFastMode;
				if (bUseDangerouslyFastMode)
				{
					if (!FileReference.Exists(DangerouslyFastValidFile))
					{
						Log.TraceInformation("Dangeroulsy Fast mode was requested, but a slow mode hasn't completed. Performing slow now...");
						bUseDangerouslyFastMode = false;
					}
				}

				foreach (FileReference FilePath in BuiltBinaries)
				{
					string RemoteExecutablePath = ConvertPath(FilePath.FullName);

					// when going super fast, just copy the executable to the final resting spot
					if (bUseDangerouslyFastMode)
					{
						Log.TraceInformation("==============================================================================");
						Log.TraceInformation("USING DANGEROUSLY FAST MODE! IF YOU HAVE ANY PROBLEMS, TRY A REBUILD/CLEAN!!!!");
						Log.TraceInformation("==============================================================================");

						// copy the executable
						string RemoteShadowDirectoryMac = ConvertPath(Path.GetDirectoryName(Target.OutputPath.FullName));
						string FinalRemoteExecutablePath = String.Format("{0}/Payload/{1}.app/{1}", RemoteShadowDirectoryMac, Target.TargetName);
						RPCUtilHelper.Command("/", String.Format("cp -f {0} {1}", RemoteExecutablePath, FinalRemoteExecutablePath), "", null);
					}
					else if (!Utils.IsRunningOnMono && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
					{
						RPCUtilHelper.CopyFile(RemoteExecutablePath, FilePath.FullName, false);

						if ((ProjectSettings.bGeneratedSYMFile == true || ProjectSettings.bGeneratedSYMBundle == true) && Path.GetExtension(FilePath.FullName) != ".a")
						{
							string DSYMExt;
							if(ProjectSettings.bGeneratedSYMBundle)
							{
								DSYMExt = ".dSYM.zip";
							}
							else
							{
								DSYMExt = ".dSYM";
							}
							RPCUtilHelper.CopyFile(RemoteExecutablePath + DSYMExt, FilePath.FullName + DSYMExt, false);
                            if (ProjectSettings.bGenerateCrashReportSymbols)
                            {
                                RPCUtilHelper.CopyFile(RemoteExecutablePath + ".udebugsymbols", FilePath.FullName + ".udebugsymbols", false);
                            }

                        }
                    }
				}

				// Generate the stub
				if (Target.Rules.bCreateStubIPA || bUseDangerouslyFastMode)
				{
                    // ensure the plist, entitlements, and provision files are properly copied
                    var DeployHandler = (Target.Platform == UnrealTargetPlatform.IOS ? new UEDeployIOS() : new UEDeployTVOS());
                    DeployHandler.PrepTargetForDeployment(new UEBuildDeployTarget(Target));

					if (!bUseDangerouslyFastMode)
					{
						// generate the dummy project so signing works
						UnrealBuildTool.GenerateProjectFiles(new XcodeProjectFileGenerator(Target.ProjectFile), new string[] { "-NoIntellisense", (Target.Platform == UnrealTargetPlatform.IOS ? "-iosdeployonly" : "-tvosdeployonly"), ((Target.ProjectFile != null) ? "-game" : "") });
					}

					// now that 
					Process StubGenerateProcess = new Process();
					StubGenerateProcess.StartInfo.WorkingDirectory = Path.GetFullPath("..\\Binaries\\DotNET\\IOS");
					StubGenerateProcess.StartInfo.FileName = Path.Combine(StubGenerateProcess.StartInfo.WorkingDirectory, "iPhonePackager.exe");

					string Arguments = "";
					string PathToApp = Target.RulesAssembly.GetTargetFileName(AppName).FullName;
					string SchemeName = AppName;

					// right now, no programs have a Source subdirectory, so assume the PathToApp is directly in the root
					if (Path.GetDirectoryName(PathToApp).Contains(@"\Engine\Source\Programs"))
					{
						PathToApp = Path.GetDirectoryName(PathToApp);
					}
					else
					{
						Int32 SourceIndex = PathToApp.LastIndexOf(@"\Intermediate\Source");
						if (SourceIndex != -1)
						{
							PathToApp = PathToApp.Substring(0, SourceIndex);
						}
						else
						{
							SourceIndex = PathToApp.LastIndexOf(@"\Source");
							if (SourceIndex != -1)
							{
								PathToApp = PathToApp.Substring(0, SourceIndex);
							}
							else
							{
								throw new BuildException("The target was not in a /Source subdirectory");
							}
						}
						if (AppName != "UE4Game" && AppName != "UE4Client")
						{
							PathToApp += "\\" + AppName + ".uproject";
						}
						else
						{
							SchemeName = "UE4";
						}
					}

					var SchemeConfiguration = Target.Configuration.ToString();
					if (Target.Rules.Type != TargetType.Game && Target.Rules.Type != TargetType.Program)
					{
						SchemeConfiguration += " " + Target.Rules.Type.ToString();
					}

					if (bUseDangerouslyFastMode)
					{
						// the quickness!
						Arguments += "DangerouslyFast " + PathToApp;
					}
					else
					{
						Arguments += "PackageIPA \"" + PathToApp + "\" -createstub";
                        IOSProvisioningData ProvisioningData = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(Target.Platform)).ReadProvisioningData(ProjectSettings);
                        if (ProvisioningData != null && ProvisioningData.MobileProvisionUUID != "")
                        {
                            Arguments += " -provisioningUUID \"" + ProvisioningData.MobileProvisionUUID + "\"";
                        }
						// if we are making the dsym, then we can strip the debug info from the executable
						if (Target.Rules.bStripSymbolsOnIOS || (Target.Configuration == UnrealTargetConfiguration.Shipping))
						{
							Arguments += " -strip";
						}
					}
					Arguments += " -config " + Target.Configuration + " -mac " + RemoteServerName + " -schemename " + SchemeName + " -schemeconfig \"" + SchemeConfiguration + "\"";

                    string Architecture = Target.Architecture;
                    if (Architecture != "")
					{
						// pass along the architecture if we need, skipping the initial -, so we have "-architecture simulator"
						Arguments += " -architecture " + Architecture.Substring(1);
					}

					if (Target.Platform == UnrealTargetPlatform.TVOS)
					{
						Arguments += " -tvos";
					}

					if (!bUseRPCUtil)
					{
						Arguments += " -usessh";
						Arguments += " -sshexe \"" + ResolvedSSHExe + "\"";
						Arguments += " -sshauth \"" + ResolvedSSHAuthentication + "\"";
						Arguments += " -sshuser \"" + ResolvedRSyncUsername + "\"";
						Arguments += " -rsyncexe \"" + ResolvedRSyncExe + "\"";
						Arguments += " -overridedevroot \"" + UserDevRootMac + "\"";
					}

					// programmers that use Xcode packaging mode should use the following commandline instead, as it will package for Xcode on each compile
					//				Arguments = "PackageApp " + GameName + " " + Configuration;
					StubGenerateProcess.StartInfo.Arguments = Arguments;

					StubGenerateProcess.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);
					StubGenerateProcess.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);

					OutputReceivedDataEventHandlerEncounteredError = false;
					OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
					int ExitCode = Utils.RunLocalProcess(StubGenerateProcess);
					if (OutputReceivedDataEventHandlerEncounteredError)
					{
						UnrealBuildTool.ExtendedErrorCode = ExitCode;
						throw new Exception(OutputReceivedDataEventHandlerEncounteredErrorMessage);
					}

					// now that a slow mode sync has finished, we can now do DangerouslyFast mode again (if requested)
					if (bUseDangerouslyFastModeWasRequested)
					{
						File.Create(DangerouslyFastValidFile.FullName);
					}
					else
					{
						// if we didn't want dangerously fast, then delete the file so that setting/unsetting the flag will do the right thing without a Rebuild
						File.Delete(DangerouslyFastValidFile.FullName);
					}
				}

				{
					// Copy bundled assets from additional frameworks to the intermediate assets directory (so they can get picked up during staging)
					String LocalFrameworkAssets = Path.GetFullPath(Target.ProjectDirectory + "/Intermediate/" + (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS") + "/FrameworkAssets");
					String RemoteFrameworkAssets = ConvertPath(LocalFrameworkAssets);

					CleanIntermediateDirectory(RemoteFrameworkAssets);

					// Delete the local dest directory if it exists
					if (Directory.Exists(LocalFrameworkAssets))
					{
						Directory.Delete(LocalFrameworkAssets, true);
					}

					foreach (UEBuildFramework Framework in RememberedAdditionalFrameworks)
					{
						if (Framework.OwningModule == null || Framework.CopyBundledAssets == null || Framework.CopyBundledAssets == "")
						{
							continue;		// Only care if we need to copy bundle assets
						}

						string RemoteZipPath = GetRemoteIntermediateFrameworkZipPath(Framework);

						RemoteZipPath = RemoteZipPath.Replace(".zip", "");

						// For now, this is hard coded, but we need to loop over all modules, and copy bundled assets that need it
						string RemoteSource = RemoteZipPath + "/" + Framework.CopyBundledAssets;
						string BundleName = Framework.CopyBundledAssets.Substring(Framework.CopyBundledAssets.LastIndexOf('/') + 1);

						String RemoteDest = RemoteFrameworkAssets + "/" + BundleName;
						String LocalDest = LocalFrameworkAssets + "\\" + BundleName;

						Log.TraceInformation("Copying bundled asset... RemoteSource: {0}, RemoteDest: {1}, LocalDest: {2}", RemoteSource, RemoteDest, LocalDest);

                        Hashtable Results = RPCUtilHelper.Command("/", String.Format("cp -R -L \"{0}\" \"{1}\"", RemoteSource, RemoteDest), "", null);

                        foreach (DictionaryEntry Entry in Results)
						{
							Log.TraceInformation("{0}", Entry.Value);
						}

						// Copy the bundled resource from the remote mac to the local dest
						RPCUtilHelper.CopyDirectory(RemoteDest, LocalDest, RPCUtilHelper.ECopyOptions.None);
					}
				}

                if (Target.Platform == UnrealTargetPlatform.TVOS)
                {
                    // copy back the built asset catalog
                    string CatPath = Path.GetDirectoryName(Target.OutputPath.FullName) + "\\AssetCatalog\\Assets.car";
                    RPCUtilHelper.CopyFile(ConvertPath(CatPath), CatPath, false);
                }

                // If it is requested, send the app bundle back to the platform executing these commands.
                if (Target.Rules.bCopyAppBundleBackToDevice)
				{
					Log.TraceInformation("Copying binaries back to this device...");

					try
					{
						string BinaryDir = Path.GetDirectoryName(Target.OutputPath.FullName) + "\\";
						if (BinaryDir.EndsWith(Target.AppName + "\\Binaries\\" + (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS") + "\\") && Target.TargetType != TargetType.Game)
						{
							BinaryDir = BinaryDir.Replace(Target.TargetType.ToString(), "Game");
						}

						// Get the app bundle's name
						string AppFullName = Target.AppName;
						if (Target.Configuration != UnrealTargetConfiguration.Development)
						{
							AppFullName += "-" + Target.Platform.ToString();
							AppFullName += "-" + Target.Configuration.ToString();
						}

						foreach (string BinaryPath in BuiltBinaries.Select(x => x.FullName))
						{
							if (!BinaryPath.Contains("Dummy"))
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

		public static int RunExecutableAndWait(string ExeName, string ArgumentList, out string StdOutResults)
		{
			// Create the process
			ProcessStartInfo PSI = new ProcessStartInfo(ExeName, ArgumentList);
			PSI.RedirectStandardOutput = true;
			PSI.UseShellExecute = false;
			PSI.CreateNoWindow = true;
			Process NewProcess = Process.Start(PSI);

			// Wait for the process to exit and grab it's output
			StdOutResults = NewProcess.StandardOutput.ReadToEnd();
			NewProcess.WaitForExit();
			return NewProcess.ExitCode;
		}

		public void StripSymbols(FileReference SourceFile, FileReference TargetFile)
		{
			StripSymbolsWithXcode(SourceFile, TargetFile, Settings.Value.ToolchainDir);
		}
	};
}
