// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	class HTML5ToolChain : VCToolChain
	{
		// ini configurations
		static bool targetWebGL2 = true; // Currently if this is set to true, UE4 can still fall back to WebGL 1 at runtime if browser does not support WebGL 2.
		static bool enableSIMD = false;
		static bool enableMultithreading = false;
		static bool bEnableTracing = false; // Debug option

		// verbose feedback
		delegate void VerbosePrint(CppConfiguration Configuration, bool bOptimizeForSize);	// proto
		static VerbosePrint PrintOnce = new VerbosePrint(PrintOnceOn);						// fn ptr
		static void PrintOnceOff(CppConfiguration Configuration, bool bOptimizeForSize) {}	// noop
		static void PrintOnceOn(CppConfiguration Configuration, bool bOptimizeForSize)
		{
			if (Configuration == CppConfiguration.Debug)
				Log.TraceInformation("HTML5ToolChain: " + Configuration + " -O0 faster compile time");
			else if (bOptimizeForSize)
				Log.TraceInformation("HTML5ToolChain: " + Configuration + " -Oz favor size over speed");
			else if (Configuration == CppConfiguration.Development)
				Log.TraceInformation("HTML5ToolChain: " + Configuration + " -O2 aggressive size and speed optimization");
			else if (Configuration == CppConfiguration.Shipping)
				Log.TraceInformation("HTML5ToolChain: " + Configuration + " -O3 favor speed over size");
			PrintOnce = new VerbosePrint(PrintOnceOff); // clear
		}

		public HTML5ToolChain(FileReference InProjectFile)
			: base(CppPlatform.HTML5, WindowsCompiler.VisualStudio2015, false, false, null)
		{
			if (!HTML5SDKInfo.IsSDKInstalled())
			{
				throw new BuildException("HTML5 SDK is not installed; cannot use toolchain.");
			}

			// ini configs
			// - normal ConfigCache w/ UnrealBuildTool.ProjectFile takes all game config ini files
			//   (including project config ini files)
			// - but, during packaging, if -remoteini is used -- need to use UnrealBuildTool.GetRemoteIniPath()
			//   (note: ConfigCache can take null ProjectFile)
			string EngineIniPath = UnrealBuildTool.GetRemoteIniPath();
			DirectoryReference ProjectDir = !String.IsNullOrEmpty(EngineIniPath) ? new DirectoryReference(EngineIniPath)
												: DirectoryReference.FromFile(InProjectFile);
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, ProjectDir, UnrealTargetPlatform.HTML5);

			// these will be going away...
			bool targetWebGL1 = false; // inverted check
			if ( Ini.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "TargetWebGL1", out targetWebGL1) )
			{
				targetWebGL2  = !targetWebGL1;
			}
//			Ini.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "EnableSIMD", out enableSIMD);
//			Ini.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "EnableMultithreading", out enableMultithreading);
			Ini.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "EnableTracing", out bEnableTracing);

			// TODO: remove this "fix" when emscripten supports (SIMD & pthreads) + WASM
				enableSIMD = false;
				// TODO: double check Engine/Source/Runtime/Core/Private/HTML5/HTML5PlatformProcess.cpp::SupportsMultithreading()
				enableMultithreading = false;

			Log.TraceInformation("HTML5ToolChain: TargetWebGL2 = "       + targetWebGL2         );
			Log.TraceInformation("HTML5ToolChain: EnableSIMD = "         + enableSIMD           );
			Log.TraceInformation("HTML5ToolChain: EnableMultithreading " + enableMultithreading );
			Log.TraceInformation("HTML5ToolChain: EnableTracing = "      + bEnableTracing       );

			PrintOnce = new VerbosePrint(PrintOnceOn); // reset
		}

		public static void PreBuildSync()
		{
			Log.TraceInformation("Setting Emscripten SDK: located in " + HTML5SDKInfo.EMSCRIPTEN_ROOT);
			HTML5SDKInfo.SetupEmscriptenTemp();
			HTML5SDKInfo.SetUpEmscriptenConfigFile();

			if (Environment.GetEnvironmentVariable("EMSDK") == null) // If env. var EMSDK is present, Emscripten is already configured by the developer
			{
				// If not using preset emsdk, configure our generated .emscripten config, instead of autogenerating one in the user's home directory.
				Environment.SetEnvironmentVariable("EM_CONFIG", HTML5SDKInfo.DOT_EMSCRIPTEN);
				Environment.SetEnvironmentVariable("EM_CACHE", HTML5SDKInfo.EMSCRIPTEN_CACHE);
			}
		}

		string GetSharedArguments_Global(CppConfiguration Configuration, bool bOptimizeForSize, string Architecture, bool bEnableShadowVariableWarnings, bool bShadowVariableWarningsAsErrors, bool bEnableUndefinedIdentifierWarnings, bool bUndefinedIdentifierWarningsAsErrors)
		{
			string Result = " ";
//			string Result = " -Werror";

			Result += " -fno-exceptions";

			Result += " -Wdelete-non-virtual-dtor";
			Result += " -Wno-switch"; // many unhandled cases
			Result += " -Wno-tautological-constant-out-of-range-compare"; // comparisons from TCHAR being a char
			Result += " -Wno-tautological-compare"; // comparison of unsigned expression < 0 is always false" (constant comparisons, which are possible with template arguments)
			Result += " -Wno-tautological-undefined-compare"; // pointer cannot be null in well-defined C++ code; comparison may be assumed to always evaluate
			Result += " -Wno-inconsistent-missing-override"; // as of 1.35.0, overriding a member function but not marked as 'override' triggers warnings
			Result += " -Wno-undefined-var-template"; // 1.36.11
			Result += " -Wno-invalid-offsetof"; // using offsetof on non-POD types
			Result += " -Wno-gnu-string-literal-operator-template"; // allow static FNames

// no longer needed as of UE4.18
//			Result += " -Wno-array-bounds"; // some VectorLoads go past the end of the array, but it's okay in that case

			if (bEnableShadowVariableWarnings)
			{
				Result += " -Wshadow" ;//+ (bShadowVariableWarningsAsErrors ? "" : " -Wno-error=shadow");
			}

			if (bEnableUndefinedIdentifierWarnings)
			{
				Result += " -Wundef" ;//+ (bUndefinedIdentifierWarningsAsErrors ? "" : " -Wno-error=undef");
			}

			// --------------------------------------------------------------------------------

			if (Configuration == CppConfiguration.Debug)
			{															// WARNING: UEBuildTarget.cs :: GetCppConfiguration()
				Result += " -O0"; // faster compile time				//          DebugGame is forced to Development
			}															// i.e. this will never get hit...

			else if (bOptimizeForSize)
			{															// Engine/Source/Programs/UnrealBuildTool/HTML5/UEBuildHTML5.cs
				Result += " -Oz"; // favor size over speed				// bCompileForSize=true; // set false, to build -O2 or -O3
			}															// SOURCE BUILD ONLY

			else if (Configuration == CppConfiguration.Development)
			{
				Result += " -O2"; // aggressive size and speed optimization
			}

			else if (Configuration == CppConfiguration.Shipping)
			{
				Result += " -O3"; // favor speed over size
			}

			PrintOnce(Configuration, bOptimizeForSize);

			// --------------------------------------------------------------------------------

			// JavaScript option overrides (see src/settings.js)
			if (enableSIMD)
			{
				Result += " -msse2 -s SIMD=1";
			}

			if (enableMultithreading)
			{
				Result += " -s USE_PTHREADS=1";
			}

			// --------------------------------------------------------------------------------
			// normally, these option are for linking -- but it using here to force recompile when
//			if (targetWebGL2) // flipping between webgl1 and webgl2
//			{
				Result += " -s USE_WEBGL2=1"; // see NOTE: UE-51094 UE-51267 futher below...
//			}
//			else
//			{
//				Result += " -s USE_WEBGL2=0";
//			}
			// --------------------------------------------------------------------------------

			// Expect that Emscripten SDK has been properly set up ahead in time (with emsdk and prebundled toolchains this is always the case)
			// This speeds up builds a tiny bit.
			Environment.SetEnvironmentVariable("EMCC_SKIP_SANITY_CHECK", "1");

			// THESE ARE TEST/DEBUGGING
//			Environment.SetEnvironmentVariable("EMCC_DEBUG", "1");
//			Environment.SetEnvironmentVariable("EMCC_CORES", "8");
//			Environment.SetEnvironmentVariable("EMCC_OPTIMIZE_NORMALLY", "1");

			// Linux builds needs this - or else system clang will be attempted to be picked up instead of UE4's
			// TODO: test on other platforms to remove this if() check
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
			{
				Environment.SetEnvironmentVariable(HTML5SDKInfo.PLATFORM_USER_HOME, HTML5SDKInfo.HTML5Intermediatory);
			}
			return Result;
		}

		string GetCLArguments_Global(CppCompileEnvironment CompileEnvironment)
		{
			string Result = GetSharedArguments_Global(CompileEnvironment.Configuration, CompileEnvironment.bOptimizeForSize, CompileEnvironment.Architecture, CompileEnvironment.bEnableShadowVariableWarnings, CompileEnvironment.bShadowVariableWarningsAsErrors, CompileEnvironment.bEnableUndefinedIdentifierWarnings, CompileEnvironment.bUndefinedIdentifierWarningsAsErrors);

// no longer needed as of UE4.18
//			Result += " -Wno-reorder"; // we disable constructor order warnings.

			return Result;
		}

		static string GetCLArguments_CPP(CppCompileEnvironment CompileEnvironment)
		{
			string Result = " -std=c++14";

			return Result;
		}

		static string GetCLArguments_C(string Architecture)
		{
			string Result = "";

			return Result;
		}

		string GetLinkArguments(LinkEnvironment LinkEnvironment)
		{
			string Result = GetSharedArguments_Global(LinkEnvironment.Configuration, LinkEnvironment.bOptimizeForSize, LinkEnvironment.Architecture, false, false, false, false);

			/* N.B. When editing link flags in this function, UnrealBuildTool does not seem to automatically pick them up and do an incremental
			 *	relink only of UE4Game.js (at least when building blueprints projects). Therefore after editing, delete the old build
			 *	outputs to force UE4 to relink:
			 *
			 *    > rm Engine/Binaries/HTML5/UE4Game.js*
			 */

			// suppress link time warnings
// no longer needed as of UE4.18
//			Result += " -Wno-parentheses"; // precedence order

			// enable verbose mode
			Result += " -v";

			// do we want debug info?
			if (LinkEnvironment.Configuration == CppConfiguration.Debug || LinkEnvironment.bCreateDebugInfo)
			{
				// TODO: Would like to have -g2 enabled here, but the UE4 manifest currently requires that UE4Game.js.symbols
				// is always generated to the build, but that file is redundant if -g2 is passed (i.e. --emit-symbol-map gets ignored)
				// so in order to enable -g2 builds, the UE4 packager should be made aware that .symbols file might not always exist.
//				Result += " -g2";

				// As a lightweight alternative, just retain function names in output.
				Result += " --profiling-funcs";

				// dump headers: http://stackoverflow.com/questions/42308/tool-to-track-include-dependencies
//				Result += " -H";
			}
			else if (LinkEnvironment.Configuration == CppConfiguration.Development)
			{
				// Development builds always have their function names intact.
				Result += " --profiling-funcs";
			}

			// Emit a .symbols map file of the minified function names. (on -g2 builds this has no effect)
			Result += " --emit-symbol-map";

//			if (LinkEnvironment.Configuration != CppConfiguration.Debug)
//			{
//				if (LinkEnvironment.bOptimizeForSize) Result += " -s OUTLINING_LIMIT=40000";
//				else Result += " -s OUTLINING_LIMIT=110000";
//			}

			if (LinkEnvironment.Configuration == CppConfiguration.Debug || LinkEnvironment.Configuration == CppConfiguration.Development)
			{
				// check for alignment/etc checking
//				Result += " -s SAFE_HEAP=1";
				//Result += " -s CHECK_HEAP_ALIGN=1";
				//Result += " -s SAFE_DYNCALLS=1";

				// enable assertions in non-Shipping/Release builds
				Result += " -s ASSERTIONS=1";
				Result += " -s GL_ASSERTIONS=1";
//				Result += " -s ASSERTIONS=2";
//				Result += " -s GL_ASSERTIONS=2";

				// In non-shipping builds, don't run ctol evaller, it can take a bit of extra time.
				Result += " -s EVAL_CTORS=0";

//				// add source map loading to code
//				string source_map = Path.Combine(HTML5SDKInfo.EMSCRIPTEN_ROOT, "src", "emscripten-source-map.min.js");
//				source_map = source_map.Replace("\\", "/").Replace(" ","\\ "); // use "unix path" and escape spaces
//				Result += " --pre-js " + source_map;

				// link in libcxxabi demangling
				Result += " -s DEMANGLE_SUPPORT=1";
			}

			Result += " -s BINARYEN=1 -s ALLOW_MEMORY_GROWTH=1";
//			Result += " -s BINARYEN_METHOD=\\'native-wasm\\'";
//			Result += " -s BINARYEN_MEM_MAX=-1";
//			Result += " -s BINARYEN_TRAP_MODE=\\'clamp\\'";

			// no need for exceptions
			Result += " -s DISABLE_EXCEPTION_CATCHING=1";

			// NOTE: UE-51094 UE-51267 -- always USE_WEBGL2, webgl1 only feature can be switched on the fly via url paramater "?webgl1"
//			if (targetWebGL2)
			{
				// WARNING - WARNING - WARNING - WARNING
				// ensure the following chunk of code is added near the end of "Parse args" at the end of "shared.Settings..." section
				// in file: .../Engine/Extras/ThirdPartyNotUE/emsdk/emscripten/XXX/emcc.py
				//		## *** UE4 EDIT start ***
				//		if shared.Settings.USE_WEBGL2:
				//		  newargs.append('-DUE4_HTML5_TARGET_WEBGL2=1')
				//		## *** UE4 EDIT end ***

				// Enable targeting WebGL 2 when available.
				Result += " -s USE_WEBGL2=1";


				// Also enable WebGL 1 emulation in WebGL 2 contexts. This adds backwards compatibility related features to WebGL 2,
				// such as:
				//  - keep supporting GL_EXT_shader_texture_lod extension in GLSLES 1.00 shaders
				//  - support for WebGL1 unsized internal texture formats
				//  - mask the GL_HALF_FLOAT_OES != GL_HALF_FLOAT mixup
				Result += " -s WEBGL2_BACKWARDS_COMPATIBILITY_EMULATION=1";
//				Result += " -s FULL_ES3=1";
			}
//			else
//			{
//				Result += " -s FULL_ES2=1";
//			}

			// The HTML page template precreates the WebGL context, so instruct the runtime to hook into that if available.
			Result += " -s GL_PREINITIALIZED_CONTEXT=1";

			// export console command handler. Export main func too because default exports ( e.g Main ) are overridden if we use custom exported functions.
			Result += " -s EXPORTED_FUNCTIONS=\"['_main', '_on_fatal']\"";

			Result += " -s NO_EXIT_RUNTIME=1";

			Result += " -s ERROR_ON_UNDEFINED_SYMBOLS=1";

			if (bEnableTracing)
			{
				Result += " --tracing";
			}

			Result += " -s CASE_INSENSITIVE_FS=1";

//			if (enableMultithreading)
//			{
//				Result += " -s ASYNCIFY=1"; // alllow BLOCKING calls (i.e. sleep)
//			}

			return Result;
		}

		static string GetLibArguments(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			return Result;
		}

		public static void CompileOutputReceivedDataEventHandler(Object Sender, DataReceivedEventArgs Line)
		{
			var Output = Line.Data;
			if (Output == null)
			{
				return;
			}

			Output = Output.Replace("\\", "/");
			// Need to match following for clickable links
			string RegexFilePath = @"^([\/A-Za-z0-9_\-\.]*)+\.(cpp|c|mm|m|hpp|h)";
			string RegexFilePath2 = @"^([A-Z]:[\/A-Za-z0-9_\-\.]*)+\.(cpp|c|mm|m|hpp|h)";
			string RegexLineNumber = @"\:\d+\:\d+\:";
			string RegexDescription = @"(\serror:\s|\swarning:\s).*";

			// Get Matches
			string MatchFilePath = Regex.Match(Output, RegexFilePath).Value;
			if (MatchFilePath.Length == 0)
			{
				MatchFilePath = Regex.Match(Output, RegexFilePath2).Value;
			}
			string MatchLineNumber = Regex.Match(Output, RegexLineNumber).Value;
			string MatchDescription = Regex.Match(Output, RegexDescription).Value;

			// If any of the above matches failed, do nothing
			if (MatchFilePath.Length == 0 ||
				MatchLineNumber.Length == 0 ||
				MatchDescription.Length == 0)
			{
				// emscripten output match
				string RegexEmscriptenInfo = @"^INFO:";
				Match match = Regex.Match(Output, RegexEmscriptenInfo);
				if ( match.Success ) {
					Log.TraceInformation(Output);
				} else {
					Log.TraceWarning(Output);
				}
				return;
			}

			// Convert Path
			string RegexStrippedPath = @"(Engine\/|[A-Za-z0-9_\-\.]*\/).*";
			string ConvertedFilePath = Regex.Match(MatchFilePath, RegexStrippedPath).Value;
			ConvertedFilePath = Path.GetFullPath(/*"..\\..\\" +*/ ConvertedFilePath);

			// Extract Line + Column Number
			string ConvertedLineNumber = Regex.Match(MatchLineNumber, @"\d+").Value;
			string ConvertedColumnNumber = Regex.Match(MatchLineNumber, @"(?<=:\d+:)\d+").Value;

			// Write output
			string ConvertedExpression = "  " + ConvertedFilePath + "(" + ConvertedLineNumber + "," + ConvertedColumnNumber + "):" + MatchDescription;
			Log.TraceInformation(ConvertedExpression); // To create clickable vs link
			Log.TraceInformation(Output);				// To preserve readable output log
		}

		public override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName, ActionGraph ActionGraph)
		{
			string Arguments = GetCLArguments_Global(CompileEnvironment);

			CPPOutput Result = new CPPOutput();

			// Add include paths to the argument list.
			foreach (string IncludePath in CompileEnvironment.IncludePaths.UserIncludePaths)
			{
				Arguments += string.Format(" -I\"{0}\"", IncludePath);
			}
			foreach (string IncludePath in CompileEnvironment.IncludePaths.SystemIncludePaths)
			{
				Arguments += string.Format(" -I\"{0}\"", IncludePath);
			}


			// Add preprocessor definitions to the argument list.
			foreach (string Definition in CompileEnvironment.Definitions)
			{
				Arguments += string.Format(" -D{0}", Definition);
			}

			if (bEnableTracing)
			{
				Arguments += string.Format(" -D__EMSCRIPTEN_TRACING__");
			}

			foreach (FileItem SourceFile in SourceFiles)
			{
				Action CompileAction = ActionGraph.Add(ActionType.Compile);
				CompileAction.CommandDescription = "Compile";
//				CompileAction.bPrintDebugInfo = true;

				bool bIsPlainCFile = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant() == ".C";

				// Add the C++ source file and its included files to the prerequisite item list.
				AddPrerequisiteSourceFile(CompileEnvironment, SourceFile, CompileAction.PrerequisiteItems);

				// Add the source file path to the command-line.
				string FileArguments = string.Format(" \"{0}\"", SourceFile.AbsolutePath);
				var ObjectFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.HTML5).GetBinaryExtension(UEBuildBinaryType.Object);
				// Add the object file to the produced item list.
				FileItem ObjectFile = FileItem.GetItemByFileReference(
					FileReference.Combine(
						CompileEnvironment.OutputDirectory,
						Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension
						)
					);
				CompileAction.ProducedItems.Add(ObjectFile);
				FileArguments += string.Format(" -o \"{0}\"", ObjectFile.AbsolutePath);

				// Add C or C++ specific compiler arguments.
				if (bIsPlainCFile)
				{
					FileArguments += GetCLArguments_C(CompileEnvironment.Architecture);
				}
				else
				{
					FileArguments += GetCLArguments_CPP(CompileEnvironment);
				}

				CompileAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
				CompileAction.CommandPath = HTML5SDKInfo.Python();

				CompileAction.CommandArguments = HTML5SDKInfo.EmscriptenCompiler() + " " + Arguments + FileArguments + CompileEnvironment.AdditionalArguments;

				//System.Console.WriteLine(CompileAction.CommandArguments);
				CompileAction.StatusDescription = Path.GetFileName(SourceFile.AbsolutePath);
				CompileAction.OutputEventHandler = new DataReceivedEventHandler(CompileOutputReceivedDataEventHandler);

				// Don't farm out creation of precomputed headers as it is the critical path task.
				CompileAction.bCanExecuteRemotely = CompileEnvironment.PrecompiledHeaderAction != PrecompiledHeaderAction.Create;

				// this is the final output of the compile step (a .abc file)
				Result.ObjectFiles.Add(ObjectFile);

				// VC++ always outputs the source file name being compiled, so we don't need to emit this ourselves
				CompileAction.bShouldOutputStatusDescription = true;

				// Don't farm out creation of precompiled headers as it is the critical path task.
				CompileAction.bCanExecuteRemotely =
					CompileEnvironment.PrecompiledHeaderAction != PrecompiledHeaderAction.Create ||
					CompileEnvironment.bAllowRemotelyCompiledPCHs;
			}

			return Result;
		}

		public override CPPOutput CompileRCFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> RCFiles, ActionGraph ActionGraph)
		{
			CPPOutput Result = new CPPOutput();

			return Result;
		}

		/// <summary>
		/// Translates clang output warning/error messages into vs-clickable messages
		/// </summary>
		/// <param name="sender"> Sending object</param>
		/// <param name="e"> Event arguments (In this case, the line of string output)</param>
		protected void RemoteOutputReceivedEventHandler(object sender, DataReceivedEventArgs e)
		{
			var Output = e.Data;
			if (Output == null)
			{
				return;
			}

			if (Utils.IsRunningOnMono)
			{
				Log.TraceInformation(Output);
			}
			else
			{
				// Need to match following for clickable links
				string RegexFilePath = @"^(\/[A-Za-z0-9_\-\.]*)+\.(cpp|c|mm|m|hpp|h)";
				string RegexLineNumber = @"\:\d+\:\d+\:";
				string RegexDescription = @"(\serror:\s|\swarning:\s).*";

				// Get Matches
				string MatchFilePath = Regex.Match(Output, RegexFilePath).Value.Replace("Engine/Source/../../", "");
				string MatchLineNumber = Regex.Match(Output, RegexLineNumber).Value;
				string MatchDescription = Regex.Match(Output, RegexDescription).Value;

				// If any of the above matches failed, do nothing
				if (MatchFilePath.Length == 0 ||
					MatchLineNumber.Length == 0 ||
					MatchDescription.Length == 0)
				{
					Log.TraceInformation(Output);
					return;
				}

				// Convert Path
				string RegexStrippedPath = @"\/Engine\/.*"; //@"(Engine\/|[A-Za-z0-9_\-\.]*\/).*";
				string ConvertedFilePath = Regex.Match(MatchFilePath, RegexStrippedPath).Value;
				ConvertedFilePath = Path.GetFullPath("..\\.." + ConvertedFilePath);

				// Extract Line + Column Number
				string ConvertedLineNumber = Regex.Match(MatchLineNumber, @"\d+").Value;
				string ConvertedColumnNumber = Regex.Match(MatchLineNumber, @"(?<=:\d+:)\d+").Value;

				// Write output
				string ConvertedExpression = "  " + ConvertedFilePath + "(" + ConvertedLineNumber + "," + ConvertedColumnNumber + "):" + MatchDescription;
				Log.TraceInformation(ConvertedExpression);	// To create clickable vs link
//				Log.TraceInformation(Output);				// To preserve readable output log
			}
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, ActionGraph ActionGraph)
		{
			FileItem OutputFile;

			// Make the final javascript file
			Action LinkAction = ActionGraph.Add(ActionType.Link);
			LinkAction.CommandDescription = "Link";
//			LinkAction.bPrintDebugInfo = true;

			// ResponseFile lines.
			List<string> ReponseLines = new List<string>();

			LinkAction.bCanExecuteRemotely = false;
			LinkAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
			LinkAction.CommandPath = HTML5SDKInfo.Python();
			LinkAction.CommandArguments = HTML5SDKInfo.EmscriptenCompiler();
//			bool bIsBuildingLibrary = LinkEnvironment.bIsBuildingLibrary || bBuildImportLibraryOnly;
//			ReponseLines.Add(
//					bIsBuildingLibrary ?
//					GetLibArguments(LinkEnvironment) :
//					GetLinkArguments(LinkEnvironment)
//				);
			ReponseLines.Add(GetLinkArguments(LinkEnvironment));

			// Add the input files to a response file, and pass the response file on the command-line.
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				//System.Console.WriteLine("File  {0} ", InputFile.AbsolutePath);
				ReponseLines.Add(string.Format(" \"{0}\"", InputFile.AbsolutePath));
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			if (!LinkEnvironment.bIsBuildingLibrary)
			{
				// Make sure ThirdParty libs are at the end.
				List<string> ThirdParty = (from Lib in LinkEnvironment.AdditionalLibraries
											where Lib.Contains("ThirdParty")
											select Lib).ToList();

				LinkEnvironment.AdditionalLibraries.RemoveAll(Element => Element.Contains("ThirdParty"));
				LinkEnvironment.AdditionalLibraries.AddRange(ThirdParty);

				foreach (string InputFile in LinkEnvironment.AdditionalLibraries)
				{
					FileItem Item = FileItem.GetItemByPath(InputFile);

					if (Item.AbsolutePath.Contains(".lib"))
						continue;

					if (Item.ToString().EndsWith(".js"))
						ReponseLines.Add(string.Format(" --js-library \"{0}\"", Item.AbsolutePath));


					// WARNING: With --pre-js and --post-js, the order in which these directives are passed to
					// the compiler is very critical, because that dictates the order in which they are appended.
					//
					// Set environment variable [ EMCC_DEBUG=1 ] to see the linker order used in packaging.
					//     See GetSharedArguments_Global() above to set this environment variable

					else if (Item.ToString().EndsWith(".jspre"))
						ReponseLines.Add(string.Format(" --pre-js \"{0}\"", Item.AbsolutePath));

					else if (Item.ToString().EndsWith(".jspost"))
						ReponseLines.Add(string.Format(" --post-js \"{0}\"", Item.AbsolutePath));


					else
						ReponseLines.Add(string.Format(" \"{0}\"", Item.AbsolutePath));

					LinkAction.PrerequisiteItems.Add(Item);
				}
			}
			// make the file we will create


			OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
			LinkAction.ProducedItems.Add(OutputFile);
			ReponseLines.Add(string.Format(" -o \"{0}\"", OutputFile.AbsolutePath));

			FileItem OutputBC = FileItem.GetItemByPath(LinkEnvironment.OutputFilePath.FullName.Replace(".js", ".bc").Replace(".html", ".bc"));
			LinkAction.ProducedItems.Add(OutputBC);
			ReponseLines.Add(string.Format(" --save-bc \"{0}\"", OutputBC.AbsolutePath));

			LinkAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);

			FileReference ResponseFileName = GetResponseFileName(LinkEnvironment, OutputFile);


			LinkAction.CommandArguments += string.Format(" @\"{0}\"", ResponseFile.Create(ResponseFileName, ReponseLines));

			LinkAction.OutputEventHandler = new DataReceivedEventHandler(RemoteOutputReceivedEventHandler);

			return OutputFile;
		}

		public override void ModifyBuildProducts(ReadOnlyTargetRules Target, UEBuildBinary Binary, List<string> Libraries, List<UEBuildBundleResource> BundleResources, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
			// we need to include the generated .mem and .symbols file.
			if (Binary.Config.Type != UEBuildBinaryType.StaticLibrary)
			{
				BuildProducts.Add(Binary.Config.OutputFilePath.ChangeExtension("wasm"), BuildProductType.RequiredResource);
				BuildProducts.Add(Binary.Config.OutputFilePath + ".symbols", BuildProductType.RequiredResource);
			}
		}
	};
}
