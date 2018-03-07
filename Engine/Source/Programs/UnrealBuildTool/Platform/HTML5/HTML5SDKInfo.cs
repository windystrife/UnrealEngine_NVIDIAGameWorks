// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using System.Text;
using UnrealBuildTool;
using System.Diagnostics;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	///
	/// </summary>
	public class HTML5SDKInfo
	{
		static string SDKVersion = "1.37.19";
		static string LLVM_VER = "e1.37.19_64bit";
//		static string BINARYEN_VER = "e1.37.9_64bit";
//		static string SDKVersion = "incoming";
//		static string LLVM_VER = "incoming";
//		static string BINARYEN_VER = "incoming";
		static string NODE_VER = "4.1.1_64bit";
		static string PYTHON_VER = "2.7.5.3_64bit";

		// --------------------------------------------------
		// --------------------------------------------------
		static string SDKBase
		{
			get
			{
				// If user has configured a custom Emscripten toolchain, use that automatically.
				if (Environment.GetEnvironmentVariable("EMSDK") != null) return Environment.GetEnvironmentVariable("EMSDK");
				// Otherwise, use the one embedded in this repository.
				return FileReference.Combine(UnrealBuildTool.EngineDirectory, "Extras", "ThirdPartyNotUE", "emsdk").FullName;
			}
		}
		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		static public string EMSCRIPTEN_ROOT
		{
			get
			{
				// If user has configured a custom Emscripten toolchain, use that automatically.
				if (Environment.GetEnvironmentVariable("EMSCRIPTEN") != null) return Environment.GetEnvironmentVariable("EMSCRIPTEN");

				// Otherwise, use the one embedded in this repository.
				return Path.Combine(SDKBase, "emscripten", SDKVersion);
			}
		}
		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		static public string EmscriptenCMakeToolChainFile { get { return Path.Combine(EMSCRIPTEN_ROOT,  "cmake", "Modules", "Platform", "Emscripten.cmake"); } }
		// --------------------------------------------------
		// --------------------------------------------------
		static string CURRENT_PLATFORM
		{
			get
			{
				switch (BuildHostPlatform.Current.Platform)
				{
					case UnrealTargetPlatform.Win64:
						return "Win64";
					case UnrealTargetPlatform.Mac:
						return "Mac";
					case UnrealTargetPlatform.Linux:
						return "Linux";
					default:
						return "error_unknown_platform";
				}
			}
		}
		static string PLATFORM_EXE
		{
			get
			{
				switch (BuildHostPlatform.Current.Platform)
				{
					case UnrealTargetPlatform.Win64:
						return ".exe";
					case UnrealTargetPlatform.Mac:
					case UnrealTargetPlatform.Linux:
						return "";
					default:
						return "error_unknown_platform";
				}
			}
		}

		// Reads the contents of the ".emscripten" config file.
		static string ReadEmscriptenConfigFile()
		{
			string config = Environment.GetEnvironmentVariable("EM_CONFIG"); // This is either a string containing the config directly, or points to a file
			if (config != null && File.Exists(config))
			{
				Log.TraceInformation( "NOTE[ReadEmscriptenConfigFile]: using EM_CONFIG" );
				config = File.ReadAllText(config);
			}
			return config;
		}

		// Pulls the value of the given option key in .emscripten config file.
		static string GetEmscriptenConfigVar(string variable)
		{
			string config = ReadEmscriptenConfigFile();
			if (config == null) return null;
			string[] tokens = config.Split('\n');

			// Parse lines of type "KEY='value'"
			Regex regex = new Regex(variable + "\\s*=\\s*['\\\"](.*)['\\\"]");
			foreach(string line in tokens)
			{
				Match m = regex.Match(line);
				if (m.Success)
				{
					return m.Groups[1].ToString();
				}
			}
			return null;
		}

		static string LLVM_ROOT
		{
			get
			{
				// If user has configured a custom Emscripten toolchain, use that automatically.
				string llvm_root = GetEmscriptenConfigVar("LLVM_ROOT");
				if (llvm_root != null) return llvm_root;

				// Otherwise, use the one embedded in this repository.
				return Path.Combine(SDKBase, CURRENT_PLATFORM, "clang", LLVM_VER);
			}
		}
		static string BINARYEN_ROOT
		{
			get
			{
				// If user has configured a custom Emscripten toolchain, use that automatically.
				string binaryen_root = GetEmscriptenConfigVar("BINARYEN_ROOT");
				if (binaryen_root != null) return binaryen_root;

				// Otherwise, use the one embedded in this repository.
//				return Path.Combine(SDKBase, CURRENT_PLATFORM, "binaryen", BINARYEN_VER);
				return Path.Combine(SDKBase, CURRENT_PLATFORM, "clang", LLVM_VER, "binaryen");
			}
		}
		static string NODE_JS
		{
			get
			{
				// If user has configured a custom Emscripten toolchain, use that automatically.
				string node_js = GetEmscriptenConfigVar("NODE_JS");
				if (node_js != null) return node_js;

				// Otherwise, use the one embedded in this repository.
				return Path.Combine(SDKBase, CURRENT_PLATFORM, "node", NODE_VER, "bin", "node" + PLATFORM_EXE);
			}
		}
		static string PYTHON
		{
			get
			{
				// If user has configured a custom Emscripten toolchain, use that automatically.
				string python = GetEmscriptenConfigVar("PYTHON");
				if (python != null) return python;

				switch (BuildHostPlatform.Current.Platform)
				{
					case UnrealTargetPlatform.Win64:
						return Path.Combine(SDKBase, "Win64", "python", PYTHON_VER, "python.exe");

					case UnrealTargetPlatform.Mac:
					case UnrealTargetPlatform.Linux:
						return "/usr/bin/python";

					default:
						return "error_unknown_platform";
				}
			}
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		public static string HTML5Intermediatory
		{
			get
			{
				string HTML5IntermediatoryPath = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", "HTML5").FullName;
				if (!Directory.Exists(HTML5IntermediatoryPath))
				{
					Directory.CreateDirectory(HTML5IntermediatoryPath);
				}
				return HTML5IntermediatoryPath;
			}
		}
		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		static public string DOT_EMSCRIPTEN
		{
			get
			{
				// If user has configured a custom Emscripten toolchain, use that automatically.
				if (Environment.GetEnvironmentVariable("EMSDK") != null && Environment.GetEnvironmentVariable("EM_CONFIG") != null && File.Exists(Environment.GetEnvironmentVariable("EM_CONFIG")))
				{
					Log.TraceInformation( "NOTE[DOT_EMSCRIPTEN]: using EM_CONFIG" );
					return Environment.GetEnvironmentVariable("EM_CONFIG");
				}

				// Otherwise, use the one embedded in this repository.
				return Path.Combine(HTML5Intermediatory, ".emscripten");
			}
		}
		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		static public string EMSCRIPTEN_CACHE { get { return Path.Combine(HTML5Intermediatory, "EmscriptenCache"); ; } }

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		public static string SetupEmscriptenTemp()
		{
			// If user has configured a custom Emscripten toolchain, use that automatically.
			if (Environment.GetEnvironmentVariable("EMSDK") != null)
			{
				string emscripten_temp = GetEmscriptenConfigVar("TEMP_DIR");
				if (emscripten_temp != null) return emscripten_temp;
			}

			// Otherwise, use the one embedded in this repository.
			string TempPath = Path.Combine(HTML5Intermediatory, "EmscriptenTemp");
			try
			{
				if (Directory.Exists(TempPath))
				{
					Directory.Delete(TempPath, true);
				}

				Directory.CreateDirectory(TempPath);
			}
			catch (Exception Ex)
			{
				Log.TraceErrorOnce(" Recreation of Emscripten Temp folder failed because of " + Ex.ToString());
			}

			return TempPath;
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		public static string PLATFORM_USER_HOME
		{
			get
			{
				switch (BuildHostPlatform.Current.Platform)
				{
					case UnrealTargetPlatform.Win64:
						return "USERPROFILE";
					case UnrealTargetPlatform.Mac:
					case UnrealTargetPlatform.Linux:
						return "HOME";
					default:
						return "error_unknown_platform";
				}
			}
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		public static string SetUpEmscriptenConfigFile()
		{
			// If user has configured a custom Emscripten toolchain, use that automatically.
			if (Environment.GetEnvironmentVariable("EMSDK") != null && Environment.GetEnvironmentVariable("EM_CONFIG") != null && File.Exists(Environment.GetEnvironmentVariable("EM_CONFIG")))
			{
				Log.TraceInformation( "NOTE[SetUpEmscriptenConfigFile]: using EM_CONFIG" );
				return Environment.GetEnvironmentVariable("EM_CONFIG");
			}

			// Otherwise, use the one embedded in this repository.

			// the following are for diff checks for file timestamps
			string SaveDotEmscripten = DOT_EMSCRIPTEN + ".save";
			if (File.Exists(SaveDotEmscripten))
			{
				File.Delete(SaveDotEmscripten);
			}
			string config_old = "";

			// make a fresh .emscripten resource file
			if (File.Exists(DOT_EMSCRIPTEN))
			{
				config_old = File.ReadAllText(DOT_EMSCRIPTEN);
				File.Move(DOT_EMSCRIPTEN, SaveDotEmscripten);
			}

			// the best way to generate .emscripten resource file,
			// is to run "emcc -v" (show version info) without an existing one
			// --------------------------------------------------
			// save a few things
			string PATH_SAVE = Environment.GetEnvironmentVariable("PATH");
			string HOME_SAVE = Environment.GetEnvironmentVariable(PLATFORM_USER_HOME);
			// warm up the .emscripten resource file
			string NODE_ROOT = Path.GetDirectoryName(NODE_JS);
			string PYTHON_ROOT = Path.GetDirectoryName(PYTHON);
			Environment.SetEnvironmentVariable("PATH",
					EMSCRIPTEN_ROOT + Path.PathSeparator +
					LLVM_ROOT + Path.PathSeparator +
					BINARYEN_ROOT + Path.PathSeparator +
					NODE_ROOT + Path.PathSeparator +
					PYTHON_ROOT + Path.PathSeparator +
					PATH_SAVE);
			Environment.SetEnvironmentVariable(PLATFORM_USER_HOME, HTML5Intermediatory);
			// --------------------------------------------------
				string cmd = "\"" + Path.Combine(EMSCRIPTEN_ROOT, "emcc") + "\"";
				ProcessStartInfo processInfo = new ProcessStartInfo(PYTHON, cmd + " -v");
				processInfo.CreateNoWindow = true;
				processInfo.UseShellExecute = false;
// jic output dump is needed...
//				processInfo.RedirectStandardError = true;
//				processInfo.RedirectStandardOutput = true;
				Process process = Process.Start(processInfo);
//				process.OutputDataReceived += (object sender, DataReceivedEventArgs e) => Console.WriteLine("output>>" + e.Data);
//				process.BeginOutputReadLine();
//				process.ErrorDataReceived += (object sender, DataReceivedEventArgs e) => Console.WriteLine("error>>" + e.Data);
//				process.BeginErrorReadLine();
				process.WaitForExit();
				Console.WriteLine("emcc ExitCode: {0}", process.ExitCode);
				process.Close();
				// uncomment OPTIMIZER (GUBP on build machines needs this)
				// and PYTHON (reduce warnings on EMCC_DEBUG=1)
				string pyth = Regex.Replace(PYTHON, @"\\", @"\\");
				string optz = Regex.Replace(Path.Combine(LLVM_ROOT, "optimizer") + PLATFORM_EXE, @"\\", @"\\");
				string byn = Regex.Replace(BINARYEN_ROOT, @"\\", @"\\");
				string txt = Regex.Replace(
					Regex.Replace(
							Regex.Replace(File.ReadAllText(DOT_EMSCRIPTEN), "#(PYTHON).*", "$1 = '" + pyth + "'"),
							"# (EMSCRIPTEN_NATIVE_OPTIMIZER).*", "$1 = '" + optz + "'"),
						"(BINARYEN_ROOT).*", "$1 = '" + byn + "'");
				File.WriteAllText(DOT_EMSCRIPTEN, txt);
				Log.TraceInformation( txt );

			// --------------------------------------------------
			if (File.Exists(SaveDotEmscripten))
			{
				if ( config_old.Equals(txt, StringComparison.Ordinal) )
				{	// preserve file timestamp -- otherwise, emscripten "system libs" will always be recompiled
					File.Delete(DOT_EMSCRIPTEN);
					File.Move(SaveDotEmscripten, DOT_EMSCRIPTEN);
				}
				else
				{
					File.Delete(SaveDotEmscripten);
				}
			}
			// --------------------------------------------------
			// --------------------------------------------------
			// restore a few things
			Environment.SetEnvironmentVariable(PLATFORM_USER_HOME, HOME_SAVE);
			Environment.SetEnvironmentVariable("PATH", PATH_SAVE);

			// --------------------------------------------------
			// the following are needed when CMake is used
			Environment.SetEnvironmentVariable("EMSCRIPTEN", EMSCRIPTEN_ROOT);
			Environment.SetEnvironmentVariable("NODEPATH", Path.GetDirectoryName(NODE_JS));
			Environment.SetEnvironmentVariable("NODE", NODE_JS);
			Environment.SetEnvironmentVariable("LLVM", LLVM_ROOT);

			return DOT_EMSCRIPTEN;
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		public static string EmscriptenVersion()
		{
			return SDKVersion;
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		public static string EmscriptenPackager()
		{
			return Path.Combine(EMSCRIPTEN_ROOT, "tools", "file_packager.py");
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		public static string EmscriptenCompiler()
		{
			return "\"" + Path.Combine(EMSCRIPTEN_ROOT, "emcc") + "\"";
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		public static string Python()
		{
			return PYTHON;
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		public static bool IsSDKInstalled()
		{
			bool sdkInstalled = Directory.Exists(EMSCRIPTEN_ROOT) && File.Exists(NODE_JS) && Directory.Exists(LLVM_ROOT) && File.Exists(PYTHON);

			// For developers: if custom EMSDK is specified but it is detected to not be properly installed,
			// issue diagnostics about the relevant paths.
			if (Environment.GetEnvironmentVariable("EMSDK") != null && !sdkInstalled)
			{
				Log.TraceInformation("EMSDK enviroment variable is set to point to " + Environment.GetEnvironmentVariable("EMSDK") + " but needed tools were not found:");
				Log.TraceInformation("EMSCRIPTEN_ROOT: " + EMSCRIPTEN_ROOT + ", exists: " + Directory.Exists(EMSCRIPTEN_ROOT).ToString());
				Log.TraceInformation("LLVM_ROOT: " + LLVM_ROOT + ", exists: " + Directory.Exists(LLVM_ROOT).ToString());
				Log.TraceInformation("NODE_JS: " + NODE_JS + ", exists: " + File.Exists(NODE_JS).ToString());
				Log.TraceInformation("PYTHON: " + PYTHON + ", exists: " + File.Exists(PYTHON).ToString());
			}

			return sdkInstalled;
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		public static int HeapSize(ConfigHierarchy ConfigCache, string BuildType)
		{
			// defaults for this script
			int ConfigHeapSize = BuildType == "Development" ? 1024 : 512;

			// values set by editor
			var bGotHeapSize = ConfigCache.GetInt32("/Script/HTML5PlatformEditor.HTML5TargetSettings", "HeapSize" + BuildType, out ConfigHeapSize);

			Log.TraceInformation("Setting Heap size to {0} Mb ", ConfigHeapSize);
			return ConfigHeapSize;
		}

		// this script is used at:
		// HTML5ToolChain.cs
		// UEBuildHTML5.cs
		// HTML5Platform.[PakFiles.]Automation.cs
		// BuildPhysX.Automation.cs
	}
}
