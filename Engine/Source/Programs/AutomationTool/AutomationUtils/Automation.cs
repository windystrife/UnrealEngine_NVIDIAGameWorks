// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Reflection;
using System.Diagnostics;
using UnrealBuildTool;

namespace AutomationTool
{
	#region Command info

	/// <summary>
	/// Command to execute info.
	/// </summary>
	class CommandInfo
	{
		public string CommandName;
		public List<string> Arguments = new List<string>();

		public override string ToString()
		{
			string Result = CommandName;
			Result += "(";
			for (int Index = 0; Index < Arguments.Count; ++Index)
			{
				if (Index > 0)
				{
					Result += ", ";
				}
				Result += Arguments[Index];
			}
			Result += ")";
			return Result;
		}
	}

	#endregion

	#region Command Line helpers

	/// <summary>
	/// Helper class for automatically registering command line params.
	/// </summary>
	public class CommandLineArg
	{
		public delegate void OnSetDelegate();
		public string Name;
		public OnSetDelegate SetDelegate;

		/// <summary>
		/// True of this arg was set in the command line.
		/// </summary>
		public bool IsSet { get; private set; }

		public CommandLineArg(string InName, OnSetDelegate InSet = null)
		{
			Name = InName;
			SetDelegate = InSet;
			GlobalCommandLine.RegisteredArgs.Add(Name, this);
		}

		public void Set()
		{
			IsSet = true;
			if (SetDelegate != null)
			{
				SetDelegate();
			}
		}

		public override string ToString()
		{
			return String.Format("{0}, {1}", Name, IsSet);
		}

		/// <summary>
		/// Returns true of this arg was set in the command line.
		/// </summary>
		public static implicit operator bool(CommandLineArg Arg)
		{
			return Arg.IsSet;
		}
	}

	/// <summary>
	/// Global command line parameters.
	/// </summary>
	public static class GlobalCommandLine
	{
		/// <summary>
		/// List of all registered command line parameters (global, not command-specific)
		/// </summary>
		public static Dictionary<string, CommandLineArg> RegisteredArgs = new Dictionary<string, CommandLineArg>(StringComparer.InvariantCultureIgnoreCase);

		public static CommandLineArg CompileOnly = new CommandLineArg("-CompileOnly");
		public static CommandLineArg Verbose = new CommandLineArg("-Verbose");
		public static CommandLineArg Submit = new CommandLineArg("-Submit");
		public static CommandLineArg NoSubmit = new CommandLineArg("-NoSubmit");
		public static CommandLineArg NoP4 = new CommandLineArg("-NoP4");
		public static CommandLineArg P4 = new CommandLineArg("-P4");
        public static CommandLineArg Preprocess = new CommandLineArg("-Preprocess");
        public static CommandLineArg Compile = new CommandLineArg("-Compile");
        /// <summary>
        /// This command is LEGACY because we used to run UAT.exe to compile scripts by default.
        /// Now we only compile by default when run via RunUAT.bat, which still understands -nocompile.
        /// However, the batch file simply passes on all arguments, so UAT will choke when encountering -nocompile.
        /// Keep this CommandLineArg around so that doesn't happen.
        /// </summary>
        public static CommandLineArg NoCompileLegacyDontUse = new CommandLineArg("-NoCompile");
        public static CommandLineArg NoCompileEditor = new CommandLineArg("-NoCompileEditor");
        public static CommandLineArg IncrementalBuildUBT = new CommandLineArg("-IncrementalBuildUBT");
		public static CommandLineArg Help = new CommandLineArg("-Help");
		public static CommandLineArg List = new CommandLineArg("-List");
		public static CommandLineArg VS2015 = new CommandLineArg("-2015");
		public static CommandLineArg NoKill = new CommandLineArg("-NoKill");
		public static CommandLineArg Installed = new CommandLineArg("-Installed");
		public static CommandLineArg UTF8Output = new CommandLineArg("-UTF8Output");
        public static CommandLineArg AllowStdOutLogVerbosity = new CommandLineArg("-AllowStdOutLogVerbosity");
        public static CommandLineArg NoAutoSDK = new CommandLineArg("-NoAutoSDK");
		public static CommandLineArg IgnoreJunk = new CommandLineArg("-ignorejunk");
        /// <summary>
        /// Allows you to use local storage for your root build storage dir (default of P:\Builds (on PC) is changed to Engine\Saved\LocalBuilds). Used for local testing.
        /// </summary>
        public static CommandLineArg UseLocalBuildStorage = new CommandLineArg("-UseLocalBuildStorage");
		public static CommandLineArg InstalledEngine = new CommandLineArg("-InstalledEngine");
		public static CommandLineArg NotInstalledEngine = new CommandLineArg("-NotInstalledEngine");

		/// <summary>
		/// Force initialize static members by calling this.
		/// </summary>
		public static void Init()
		{
			Log.TraceVerbose("Registered {0} command line parameters.", RegisteredArgs.Count);
		}
	}

	#endregion

	[Help(
@"Executes scripted commands

AutomationTool.exe [-verbose] [-compileonly] [-p4] Command0 [-Arg0 -Arg1 -Arg2 …] Command1 [-Arg0 -Arg1 …] Command2 [-Arg0 …] Commandn … [EnvVar0=MyValue0 … EnvVarn=MyValuen]"
)]
	[Help("verbose", "Enables verbose logging")]
	[Help("nop4", "Disables Perforce functionality (default if not run on a build machine)")]
	[Help("p4", "Enables Perforce functionality (default if run on a build machine)")]
	[Help("compileonly", "Does not run any commands, only compiles them")]
    [Help("compile", "Dynamically compiles all commands (otherwise assumes they are already built)")]
	[Help("help", "Displays help")]
	[Help("list", "Lists all available commands")]
	[Help("submit", "Allows UAT command to submit changes")]
	[Help("nosubmit", "Prevents any submit attempts")]
	[Help("nokill", "Does not kill any spawned processes on exit")]
	[Help("ignorejunk", "Prevents UBT from cleaning junk files")]
    [Help("UseLocalBuildStorage", @"Allows you to use local storage for your root build storage dir (default of P:\Builds (on PC) is changed to Engine\Saved\LocalBuilds). Used for local testing.")]
    public static class Automation
	{
		#region Command line parsing

		/// <summary>
		/// Parses command line parameter.
		/// </summary>
		/// <param name="ParamIndex">Parameter index</param>
		/// <param name="CommandLine">Command line</param>
		/// <param name="CurrentCommand">Recently parsed command</param>
		/// <param name="OutScriptsForProjectFileName">The only project to build scripts for</param>
		/// <param name="OutAdditionalScriptsFolders">Additional script locations</param>
		/// <returns>True if the parameter has been successfully parsed.</returns>
		private static void ParseParam(string CurrentParam, CommandInfo CurrentCommand, ref string OutScriptsForProjectFileName, List<string> OutAdditionalScriptsFolders)
		{
			bool bGlobalParam = false;
			foreach (var RegisteredParam in GlobalCommandLine.RegisteredArgs)
			{
				if (String.Compare(CurrentParam, RegisteredParam.Key, StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					// Register and exit, we're done here.
					RegisteredParam.Value.Set();
					bGlobalParam = true;
					break;
				}
			}

			// The parameter was not found in the list of global parameters, continue looking...
			if (CurrentParam.StartsWith("-ScriptsForProject=", StringComparison.InvariantCultureIgnoreCase))
			{
				if(OutScriptsForProjectFileName != null)
				{
					throw new AutomationException("The -ProjectScripts argument may only be specified once");
				}
				var ProjectFileName = CurrentParam.Substring(CurrentParam.IndexOf('=') + 1).Replace("\"", "");
				if(!File.Exists(ProjectFileName))
				{
					throw new AutomationException("Project '{0}' does not exist", ProjectFileName);
				}
				OutScriptsForProjectFileName = Path.GetFullPath(ProjectFileName);
			}
			else if (CurrentParam.StartsWith("-ScriptDir=", StringComparison.InvariantCultureIgnoreCase))
			{
				var ScriptDir = CurrentParam.Substring(CurrentParam.IndexOf('=') + 1);
				if (Directory.Exists(ScriptDir))
				{
					OutAdditionalScriptsFolders.Add(ScriptDir);
					Log.TraceVerbose("Found additional script dir: {0}", ScriptDir);
				}
				else
				{
					throw new AutomationException("Specified ScriptDir doesn't exist: {0}", ScriptDir);
				}
			}
			else if (CurrentParam.StartsWith("-"))
			{
				if (CurrentCommand != null)
				{
					CurrentCommand.Arguments.Add(CurrentParam.Substring(1));
				}
				else if (!bGlobalParam)
				{
					throw new AutomationException("Unknown parameter {0} in the command line that does not belong to any command.", CurrentParam);
				}
			}
			else if (CurrentParam.Contains("="))
			{
				// Environment variable
				int ValueStartIndex = CurrentParam.IndexOf('=') + 1;
				string EnvVarName = CurrentParam.Substring(0, ValueStartIndex - 1);
				if (String.IsNullOrEmpty(EnvVarName))
				{
					throw new AutomationException("Unable to parse environment variable that has no name. Error when parsing command line param {0}", CurrentParam);
				}
				string EnvVarValue = CurrentParam.Substring(ValueStartIndex);
				CommandUtils.SetEnvVar(EnvVarName, EnvVarValue);
			}
		}

		private static string ParseString(string Key, string Value)
		{
			if (!String.IsNullOrEmpty(Key))
			{
				if (Value == "true" || Value == "false")
				{
					return "-" + Key;
				}
				else
				{
					string param = "-" + Key + "=";
					if (Value.Contains(" "))
					{
						param += "\"" + Value + "\"";
					}
					else
					{
						param += Value;
					}
					return param;
				}
			}
			else
			{
				return Value;
			}
		}


		private static string ParseList(string Key, List<object> Value)
		{
			string param = "-" + Key + "=";
			bool bStart = true;
			foreach (var Val in Value)
			{
				if (!bStart)
				{
					param += "+";
				}
				param += Val as string;
				bStart = false;
			}
			return param;
		}

		private static void ParseDictionary(Dictionary<string, object> Value, List<string> Arguments)
		{
			foreach (var Pair in Value)
			{
				if ((Pair.Value as string) != null && !string.IsNullOrEmpty(Pair.Value as string))
				{
					Arguments.Add(ParseString(Pair.Key, Pair.Value as string));
				}
				else if (Pair.Value.GetType() == typeof(bool))
				{
					if ((bool)Pair.Value)
					{
						Arguments.Add("-" + Pair.Key);
					}
				}
				else if ((Pair.Value as List<object>) != null)
				{
					Arguments.Add(ParseList(Pair.Key, Pair.Value as List<object>));
				}
				else if ((Pair.Value as Dictionary<string, object>) != null)
				{
					string param = "-" + Pair.Key + "=\"";
					List<string> Args = new List<string>();
					ParseDictionary(Pair.Value as Dictionary<string, object>, Args);
					bool bStart = true;
					foreach (var Arg in Args)
					{
						if (!bStart)
						{
							param += " ";
						}
						param += Arg.Replace("\"", "\'");
						bStart = false;
					}
					param += "\"";
					Arguments.Add(param);
				}
			}
		}

		private static void ParseProfile(ref string[] CommandLine)
		{
			// find if there is a profile file to read
			string Profile = "";
			List<string> Arguments = new List<string>();
			for (int Index = 0; Index < CommandLine.Length; ++Index)
			{
				if (CommandLine[Index].StartsWith("-profile="))
				{
					Profile = CommandLine[Index].Substring(CommandLine[Index].IndexOf('=') + 1);
				}
				else
				{
					Arguments.Add(CommandLine[Index]);
				}
			}

			if (!string.IsNullOrEmpty(Profile))
			{
				if (File.Exists(Profile))
				{
					// find if the command has been specified
					var text = File.ReadAllText(Profile);
					var RawObject = fastJSON.JSON.Instance.Parse(text) as Dictionary<string, object>;
					var Params = RawObject["scripts"] as List<object>;
					foreach (var Script in Params)
					{
						string ScriptName = (Script as Dictionary<string, object>)["script"] as string;
						if (!string.IsNullOrEmpty(ScriptName) && !Arguments.Contains(ScriptName))
						{
							Arguments.Add(ScriptName);
						}
						(Script as Dictionary<string, object>).Remove("script");
						ParseDictionary((Script as Dictionary<string, object>), Arguments);
					}
				}
			}

			CommandLine = Arguments.ToArray();
		}

		/// <summary>
		/// Parse the command line and create a list of commands to execute.
		/// </summary>
		/// <param name="Arguments">Command line</param>
		/// <param name="OutCommandsToExecute">List containing the names of commands to execute</param>
		/// <param name="OutAdditionalScriptsFolders">Optional list of additional paths to look for scripts in</param>
		private static void ParseCommandLine(string[] Arguments, List<CommandInfo> OutCommandsToExecute, out string OutScriptsForProjectFileName, List<string> OutAdditionalScriptsFolders)
		{
			// Initialize global command line parameters
			GlobalCommandLine.Init();

			ParseProfile(ref Arguments);

			Log.TraceInformation("Parsing command line: {0}", CommandUtils.FormatCommandLine(Arguments));

			OutScriptsForProjectFileName = null;

			CommandInfo CurrentCommand = null;
			for (int Index = 0; Index < Arguments.Length; ++Index)
			{
				var Param = Arguments[Index];
				if (Param.StartsWith("-") || Param.Contains("="))
				{
					ParseParam(Arguments[Index], CurrentCommand, ref OutScriptsForProjectFileName, OutAdditionalScriptsFolders);
				}
				else
				{
					CurrentCommand = new CommandInfo();
					CurrentCommand.CommandName = Arguments[Index];
					OutCommandsToExecute.Add(CurrentCommand);
				}
			}

			// Validate
			var Result = OutCommandsToExecute.Count > 0 || GlobalCommandLine.Help || GlobalCommandLine.CompileOnly || GlobalCommandLine.List;
			if (OutCommandsToExecute.Count > 0)
			{
				Log.TraceVerbose("Found {0} scripts to execute:", OutCommandsToExecute.Count);
				foreach (var Command in OutCommandsToExecute)
				{
					Log.TraceVerbose("  " + Command.ToString());
				}
			}
			else if (!Result)
			{
				throw new AutomationException("Failed to find scripts to execute in the command line params.");
			}
			if (GlobalCommandLine.NoP4 && GlobalCommandLine.P4)
			{
				throw new AutomationException("{0} and {1} can't be set simultaneously.", GlobalCommandLine.NoP4.Name, GlobalCommandLine.P4.Name);
			}
			if (GlobalCommandLine.NoSubmit && GlobalCommandLine.Submit)
			{
				throw new AutomationException("{0} and {1} can't be set simultaneously.", GlobalCommandLine.NoSubmit.Name, GlobalCommandLine.Submit.Name);
			}
		}

		#endregion

		#region Main Program

		/// <summary>
		/// Main method.
		/// </summary>
		/// <param name="Arguments">Command line</param>
		public static ExitCode Process(string[] Arguments)
		{
			// Initial check for local or build machine runs BEFORE we parse the command line (We need this value set
			// in case something throws the exception while parsing the command line)
			IsBuildMachine = !String.IsNullOrEmpty(Environment.GetEnvironmentVariable("uebp_LOCAL_ROOT")) || Arguments.Any(x => x.Equals("-BuildMachine", StringComparison.InvariantCultureIgnoreCase));

			// Scan the command line for commands to execute.
			var CommandsToExecute = new List<CommandInfo>();
			string OutScriptsForProjectFileName;
			var AdditionalScriptsFolders = new List<string>();
			ParseCommandLine(Arguments, CommandsToExecute, out OutScriptsForProjectFileName, AdditionalScriptsFolders);

			// Get the path to the telemetry file, if present
			string TelemetryFile = CommandUtils.ParseParamValue(Arguments, "-Telemetry");
			
			Log.TraceVerbose("IsBuildMachine={0}", IsBuildMachine);
			Environment.SetEnvironmentVariable("IsBuildMachine", IsBuildMachine ? "1" : "0");

			// should we kill processes on exit
			ShouldKillProcesses = !GlobalCommandLine.NoKill;
			Log.TraceVerbose("ShouldKillProcesses={0}", ShouldKillProcesses);

			if (CommandsToExecute.Count == 0 && GlobalCommandLine.Help)
			{
				DisplayHelp();
				return ExitCode.Success;
			}

			// Disable AutoSDKs if specified on the command line
			if (GlobalCommandLine.NoAutoSDK)
			{
				PlatformExports.PreventAutoSDKSwitching();
			}

			// Setup environment
			Log.TraceLog("Setting up command environment.");
			CommandUtils.InitCommandEnvironment();

			// Determine if the engine is installed
			bIsEngineInstalled = GlobalCommandLine.Installed;
			string InstalledBuildFile = Path.Combine(CommandUtils.CmdEnv.LocalRoot, "Engine", "Build", "InstalledBuild.txt");
			bIsEngineInstalled |= File.Exists(InstalledBuildFile);
			if (bIsEngineInstalled.Value)
			{
				bIsEngineInstalled = !GlobalCommandLine.NotInstalledEngine;
			}
			else
			{
				bIsEngineInstalled = GlobalCommandLine.InstalledEngine;
			}

			// Initialize UBT
			if(!UnrealBuildTool.PlatformExports.Initialize(bIsEngineInstalled.Value))
			{
				Log.TraceInformation("Failed to initialize UBT");
				return ExitCode.Error_Unknown;
			}

			// Fill in the project info
			UnrealBuildTool.UProjectInfo.FillProjectInfo();

			// Clean rules folders up
			ProjectUtils.CleanupFolders();

			// Compile scripts.
			ScriptCompiler Compiler = new ScriptCompiler();
			using(TelemetryStopwatch ScriptCompileStopwatch = new TelemetryStopwatch("ScriptCompile"))
			{
				Compiler.FindAndCompileAllScripts(OutScriptsForProjectFileName, AdditionalScriptsFolders);
			}

			if (GlobalCommandLine.CompileOnly)
			{
				Log.TraceInformation("Compilation successful, exiting (CompileOnly)");
				return ExitCode.Success;
			}

			if (GlobalCommandLine.List)
			{
				ListAvailableCommands(Compiler.Commands);
				return ExitCode.Success;
			}

			if (GlobalCommandLine.Help)
			{
				DisplayHelp(CommandsToExecute, Compiler.Commands);
				return ExitCode.Success;
			}

			// Enable or disable P4 support
			CommandUtils.InitP4Support(CommandsToExecute, Compiler.Commands);
			if (CommandUtils.P4Enabled)
			{
				Log.TraceLog("Setting up Perforce environment.");
				CommandUtils.InitP4Environment();
				CommandUtils.InitDefaultP4Connection();
			}

			// Find and execute commands.
			ExitCode Result = Execute(CommandsToExecute, Compiler.Commands);
			if (TelemetryFile != null)
			{
				Directory.CreateDirectory(Path.GetDirectoryName(TelemetryFile));
				CommandUtils.Telemetry.Write(TelemetryFile);
			}
			return Result;
		}

		/// <summary>
		/// Execute commands specified in the command line.
		/// </summary>
		/// <param name="CommandsToExecute"></param>
		/// <param name="Commands"></param>
		private static ExitCode Execute(List<CommandInfo> CommandsToExecute, Dictionary<string, Type> Commands)
		{
			for (int CommandIndex = 0; CommandIndex < CommandsToExecute.Count; ++CommandIndex)
			{
				var CommandInfo = CommandsToExecute[CommandIndex];
				Log.TraceVerbose("Attempting to execute {0}", CommandInfo.ToString());
				Type CommandType;
				if (!Commands.TryGetValue(CommandInfo.CommandName, out CommandType))
				{
					throw new AutomationException("Failed to find command {0}", CommandInfo.CommandName);
				}

				BuildCommand Command = (BuildCommand)Activator.CreateInstance(CommandType);
				Command.Params = CommandInfo.Arguments.ToArray();
				try
				{
					ExitCode Result = Command.Execute();
					if(Result != ExitCode.Success)
					{
						return Result;
					}
					CommandUtils.Log("BUILD SUCCESSFUL");
				}
				finally
				{
					// dispose of the class if necessary
					var CommandDisposable = Command as IDisposable;
					if (CommandDisposable != null)
					{
						CommandDisposable.Dispose();
					}
				}

				// Make sure there's no directories on the stack.
				CommandUtils.ClearDirStack();
			}
			return ExitCode.Success;
		}

		#endregion

		#region Help

		/// <summary>
		/// Display help for the specified commands (to execute)
		/// </summary>
		/// <param name="CommandsToExecute">List of commands specified in the command line.</param>
		/// <param name="Commands">All discovered command objects.</param>
		private static void DisplayHelp(List<CommandInfo> CommandsToExecute, Dictionary<string, Type> Commands)
		{
			for (int CommandIndex = 0; CommandIndex < CommandsToExecute.Count; ++CommandIndex)
			{
				var CommandInfo = CommandsToExecute[CommandIndex];
				Type CommandType;
				if (Commands.TryGetValue(CommandInfo.CommandName, out CommandType) == false)
				{
					Log.TraceError("Help: Failed to find command {0}", CommandInfo.CommandName);
				}
				else
				{
					CommandUtils.Help(CommandType);
				}
			}
		}

		/// <summary>
		/// Display AutomationTool.exe help.
		/// </summary>
		private static void DisplayHelp()
		{
			CommandUtils.LogHelp(typeof(Automation));
		}

		/// <summary>
		/// List all available commands.
		/// </summary>
		/// <param name="Commands">All vailable commands.</param>
		private static void ListAvailableCommands(Dictionary<string, Type> Commands)
		{
			string Message = Environment.NewLine;
			Message += "Available commands:" + Environment.NewLine;
			foreach (var AvailableCommand in Commands)
			{
				Message += String.Format("  {0}{1}", AvailableCommand.Key, Environment.NewLine);
			}
			CommandUtils.Log(Message);
		}

		#endregion

		#region HelperFunctions

		/// <summary>
		/// Returns true if AutomationTool is running using installed Engine components
		/// </summary>
		/// <returns>True if running using installed Engine components</returns>
		static public bool IsEngineInstalled()
		{
			if (!bIsEngineInstalled.HasValue)
			{
				throw new AutomationException("IsEngineInstalled has not been initialized yet");
			}

			return bIsEngineInstalled.Value;
		}
		static private bool? bIsEngineInstalled;

		#endregion

		#region Properties

		/// <summary>
		/// True if this process is running on a build machine, false if locally.
		/// </summary>
		/// <remarks>
		/// The reason one this property exists in Automation class and not BuildEnvironment is that
		/// it's required long before BuildEnvironment is initialized.
		/// </remarks>
		public static bool IsBuildMachine
		{
			get
			{
				if (!bIsBuildMachine.HasValue)
				{
					throw new AutomationException("Trying to access IsBuildMachine property before it was initialized.");
				}
				return (bool)bIsBuildMachine;				
			}
			private set
			{
				bIsBuildMachine = value;
			}
		}
		private static bool? bIsBuildMachine;

		public static bool ShouldKillProcesses
		{
			get
			{
				if (!bShouldKillProcesses.HasValue)
				{
					throw new AutomationException("Trying to access ShouldKillProcesses property before it was initialized.");
				}
				return (bool)bShouldKillProcesses;
			}
			private set
			{
				bShouldKillProcesses = value;
			}
		}
		private static bool? bShouldKillProcesses;
		#endregion
	}

}
