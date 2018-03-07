// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Xml;
using System.Text.RegularExpressions;
using System.Linq;
using System.Reflection;
using Microsoft.Win32;
using System.Text;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	class XGE : ActionExecutor
	{
		/// <summary>
		/// Whether to use the no_watchdog_thread option to prevent VS2015 toolchain stalls.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		bool bXGENoWatchdogThread = false;

		/// <summary>
		/// Whether to display the XGE build monitor.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		bool bShowXGEMonitor = false;

		/// <summary>
		/// When enabled, XGE will stop compiling targets after a compile error occurs.  Recommended, as it saves computing resources for others.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		bool bStopXGECompilationAfterErrors = true;

		private const string ProgressMarkupPrefix = "@action";

		public XGE()
		{
			XmlConfig.ApplyTo(this);
		}

		public override string Name
		{
			get { return "XGE"; }
		}

		public static bool IsAvailable()
		{
			// Get the name of the XgConsole executable.
			string XgConsole = "xgConsole";
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64)
			{
				XgConsole = "xgConsole.exe";
			}

			// Search the path for it
			string PathVariable = Environment.GetEnvironmentVariable("PATH");
			foreach (string SearchPath in PathVariable.Split(Path.PathSeparator))
			{
				try
				{
					string PotentialPath = Path.Combine(SearchPath, XgConsole);
					if(File.Exists(PotentialPath))
					{
						return true;
					}
				}
				catch(ArgumentException)
				{
					// PATH variable may contain illegal characters; just ignore them.
				}
			}
			return false;
		}

		// precompile the Regex needed to parse the XGE output (the ones we want are of the form "File (Duration at +time)"
		private static Regex XGEDurationRegex = new Regex(@"(?<Filename>.*) *\((?<Duration>[0-9:\.]+) at [0-9\+:\.]+\)", RegexOptions.ExplicitCapture);

		public static void ExportActions(List<Action> ActionsToExecute)
		{
			for(int FileNum = 0;;FileNum++)
			{
				string OutFile = Path.Combine(UnrealBuildTool.EngineDirectory.FullName, "Intermediate", "Build", String.Format("UBTExport.{0}.xge.xml", FileNum.ToString("D3")));
				if(!File.Exists(OutFile))
				{
					ExportActions(ActionsToExecute, OutFile);
					break;
				}
			}
		}

		public static void ExportActions(List<Action> ActionsToExecute, string OutFile)
		{
			WriteTaskFile(ActionsToExecute, OutFile, ProgressWriter.bWriteMarkup, bXGEExport: true);
			Log.TraceInformation("XGEEXPORT: Exported '{0}'", OutFile);
		}

		public override bool ExecuteActions(List<Action> ActionsToExecute, bool bLogDetailedActionStats)
		{
			bool XGEResult = true;

			// Batch up XGE execution by actions with the same output event handler.
			List<Action> ActionBatch = new List<Action>();
			ActionBatch.Add(ActionsToExecute[0]);
			for (int ActionIndex = 1; ActionIndex < ActionsToExecute.Count && XGEResult; ++ActionIndex)
			{
				Action CurrentAction = ActionsToExecute[ActionIndex];
				if (CurrentAction.OutputEventHandler == ActionBatch[0].OutputEventHandler)
				{
					ActionBatch.Add(CurrentAction);
				}
				else
				{
					XGEResult = ExecuteActionBatch(ActionBatch);
					ActionBatch.Clear();
					ActionBatch.Add(CurrentAction);
				}
			}
			if (ActionBatch.Count > 0 && XGEResult)
			{
				XGEResult = ExecuteActionBatch(ActionBatch);
				ActionBatch.Clear();
			}

			return XGEResult;
		}

		bool ExecuteActionBatch(List<Action> Actions)
		{
			bool XGEResult = true;
			if (Actions.Count > 0)
			{
				// Write the actions to execute to a XGE task file.
				string XGETaskFilePath = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", "XGETasks.xml").FullName;
				WriteTaskFile(Actions, XGETaskFilePath, ProgressWriter.bWriteMarkup, false);

				// Try to execute the XGE tasks, and if XGE is available, skip the local execution fallback.
				if (Telemetry.IsAvailable())
				{
					try
					{
						const string BuilderKey = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Xoreax\\IncrediBuild\\Builder";

						string CPUUtilization = Registry.GetValue(BuilderKey, "ForceCPUCount", "").ToString();
						string AvoidTaskExecutionOnLocalMachine = Registry.GetValue(BuilderKey, "AvoidLocalExec", "").ToString();
						string RestartRemoteProcessesOnLocalMachine = Registry.GetValue(BuilderKey, "AllowDoubleTargets", "").ToString();
						string LimitMaxNumberOfCores = Registry.GetValue(BuilderKey, "MaxHelpers", "").ToString();
						string WriteOutputToDiskInBackground = Registry.GetValue(BuilderKey, "LazyOutputWriter_Beta", "").ToString();
						string MaxConcurrentPDBs = Registry.GetValue(BuilderKey, "MaxConcurrentPDBs", "").ToString();
						string EnabledAsHelper = Registry.GetValue(BuilderKey, "LastEnabled", "").ToString();

						Telemetry.SendEvent("XGESettings.2",
							"CPUUtilization", CPUUtilization,
							"AvoidTaskExecutionOnLocalMachine", AvoidTaskExecutionOnLocalMachine,
							"RestartRemoteProcessesOnLocalMachine", RestartRemoteProcessesOnLocalMachine,
							"LimitMaxNumberOfCores", LimitMaxNumberOfCores,
							"WriteOutputToDiskInBackground", WriteOutputToDiskInBackground,
							"MaxConcurrentPDBs", MaxConcurrentPDBs,
							"EnabledAsHelper", EnabledAsHelper);
					}
					catch
					{
					}
				}
				XGEResult = ExecuteTaskFileWithProgressMarkup(XGETaskFilePath, Actions.Count, (Sender, Args) =>
					{
						if (Actions[0].OutputEventHandler != null)
						{
							Actions[0].OutputEventHandler(Sender, Args);
						}
						else
						{
							Console.WriteLine(Args.Data);
						}
					});
			}
			return XGEResult;
		}

		private static DataReceivedEventArgs ConstructDataReceivedEventArgs(string NewData)
		{
			// NOTE: This is a pretty nasty hack with C# reflection, however it saves us from having to replace all the
			// handlers in various Toolchains with a wrapper handler that takes a string - it is certainly doable, but 
			// touches code outside of this class that I don't want to touch right now

			// DataReceivedEventArgs is not normally constructable, so work around it with creating a scratch Args object
			DataReceivedEventArgs EventArgs = (DataReceivedEventArgs)System.Runtime.Serialization.FormatterServices.GetUninitializedObject(typeof(DataReceivedEventArgs));

			// now we need to set the Data field using reflection, since it is read only
			FieldInfo[] ArgFields = typeof(DataReceivedEventArgs).GetFields(BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.DeclaredOnly);
			ArgFields[0].SetValue(EventArgs, NewData);
			return EventArgs;
		}

		/// <summary>
		/// Writes a XGE task file containing the specified actions to the specified file path.
		/// </summary>
		static void WriteTaskFile(List<Action> InActions, string TaskFilePath, bool bProgressMarkup, bool bXGEExport)
		{
			Dictionary<string, string> ExportEnv = new Dictionary<string, string>();

			List<Action> Actions = InActions;
			if (bXGEExport)
			{
				IDictionary CurrentEnvironment = Environment.GetEnvironmentVariables();
				foreach (System.Collections.DictionaryEntry Pair in CurrentEnvironment)
				{
					if (!UnrealBuildTool.InitialEnvironment.Contains(Pair.Key) || (string)(UnrealBuildTool.InitialEnvironment[Pair.Key]) != (string)(Pair.Value))
					{
						ExportEnv.Add((string)(Pair.Key), (string)(Pair.Value));
					}
				}

				int NumSortErrors = 0;
				for (int ActionIndex = 0; ActionIndex < InActions.Count; ActionIndex++)
				{
					Action Action = InActions[ActionIndex];
					foreach (FileItem Item in Action.PrerequisiteItems)
					{
						if (Item.ProducingAction != null && InActions.Contains(Item.ProducingAction))
						{
							int DepIndex = InActions.IndexOf(Item.ProducingAction);
							if (DepIndex > ActionIndex)
							{
								//Console.WriteLine("Action is not topologically sorted.");
								//Console.WriteLine("  {0} {1}", Action.CommandPath, Action.CommandArguments);
								//Console.WriteLine("Dependency");
								//Console.WriteLine("  {0} {1}", Item.ProducingAction.CommandPath, Item.ProducingAction.CommandArguments);
								NumSortErrors++;
							}
						}
					}
				}
				if (NumSortErrors > 0)
				{
					//Console.WriteLine("The UBT action graph was not sorted. Sorting actions....");
					Actions = new List<Action>();
					HashSet<int> UsedActions = new HashSet<int>();
					for (int ActionIndex = 0; ActionIndex < InActions.Count; ActionIndex++)
					{
						if (UsedActions.Contains(ActionIndex))
						{
							continue;
						}
						Action Action = InActions[ActionIndex];
						foreach (FileItem Item in Action.PrerequisiteItems)
						{
							if (Item.ProducingAction != null && InActions.Contains(Item.ProducingAction))
							{
								int DepIndex = InActions.IndexOf(Item.ProducingAction);
								if (UsedActions.Contains(DepIndex))
								{
									continue;
								}
								Actions.Add(Item.ProducingAction);
								UsedActions.Add(DepIndex);
							}
						}
						Actions.Add(Action);
						UsedActions.Add(ActionIndex);
					}
					for (int ActionIndex = 0; ActionIndex < Actions.Count; ActionIndex++)
					{
						Action Action = Actions[ActionIndex];
						foreach (FileItem Item in Action.PrerequisiteItems)
						{
							if (Item.ProducingAction != null && Actions.Contains(Item.ProducingAction))
							{
								int DepIndex = Actions.IndexOf(Item.ProducingAction);
								if (DepIndex > ActionIndex)
								{
									Console.WriteLine("Action is not topologically sorted.");
									Console.WriteLine("  {0} {1}", Action.CommandPath, Action.CommandArguments);
									Console.WriteLine("Dependency");
									Console.WriteLine("  {0} {1}", Item.ProducingAction.CommandPath, Item.ProducingAction.CommandArguments);
									throw new BuildException("Cyclical Dependency in action graph.");
								}
							}
						}
					}
				}

			}


			XmlDocument XGETaskDocument = new XmlDocument();

			// <BuildSet FormatVersion="1">...</BuildSet>
			XmlElement BuildSetElement = XGETaskDocument.CreateElement("BuildSet");
			XGETaskDocument.AppendChild(BuildSetElement);
			BuildSetElement.SetAttribute("FormatVersion", "1");

			// <Environments>...</Environments>
			XmlElement EnvironmentsElement = XGETaskDocument.CreateElement("Environments");
			BuildSetElement.AppendChild(EnvironmentsElement);

			// <Environment Name="Default">...</CompileEnvironment>
			XmlElement EnvironmentElement = XGETaskDocument.CreateElement("Environment");
			EnvironmentsElement.AppendChild(EnvironmentElement);
			EnvironmentElement.SetAttribute("Name", "Default");

			// <Tools>...</Tools>
			XmlElement ToolsElement = XGETaskDocument.CreateElement("Tools");
			EnvironmentElement.AppendChild(ToolsElement);

			if (ExportEnv.Count > 0)
			{
				// <Variables>...</Variables>
				XmlElement VariablesElement = XGETaskDocument.CreateElement("Variables");
				EnvironmentElement.AppendChild(VariablesElement);

				foreach (KeyValuePair<string, string> Pair in ExportEnv)
				{
					// <Variable>...</Variable>
					XmlElement VariableElement = XGETaskDocument.CreateElement("Variable");
					VariablesElement.AppendChild(VariableElement);
					VariableElement.SetAttribute("Name", Pair.Key);
					VariableElement.SetAttribute("Value", Pair.Value);
				}
			}

			for (int ActionIndex = 0; ActionIndex < Actions.Count; ActionIndex++)
			{
				Action Action = Actions[ActionIndex];

				// <Tool ... />
				XmlElement ToolElement = XGETaskDocument.CreateElement("Tool");
				ToolsElement.AppendChild(ToolElement);
				ToolElement.SetAttribute("Name", string.Format("Tool{0}", ActionIndex));
				ToolElement.SetAttribute("AllowRemote", Action.bCanExecuteRemotely.ToString());

				// The XGE documentation says that 'AllowIntercept' must be set to 'true' for all tools where 'AllowRemote' is enabled
				ToolElement.SetAttribute("AllowIntercept", Action.bCanExecuteRemotely.ToString());

				string OutputPrefix = "";
				if (bProgressMarkup)
				{
					OutputPrefix += ProgressMarkupPrefix;
				}
				if (Action.bShouldOutputStatusDescription)
				{
					OutputPrefix += Action.StatusDescription;
				}
				if (OutputPrefix.Length > 0)
				{
					ToolElement.SetAttribute("OutputPrefix", OutputPrefix);
				}

				// When running on Windows, differentiate between .exe and batch files.
				// Those (.bat, .cmd) need to be run via cmd /c or shellexecute, 
				// the latter which we can't use because we want to redirect input/output

				bool bLaunchViaCmdExe = (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64) && (!Path.GetExtension(Action.CommandPath).ToLower().EndsWith(".exe"));

				string CommandPath = "";
				string CommandArguments = "";

				if (bLaunchViaCmdExe)
				{
					CommandPath = "cmd.exe";
					CommandArguments = string.Format
					(
						"/c \"{0} {1}\"",
						(Action.CommandPath),
						(Action.CommandArguments)
					);
				}
				else
				{
					CommandPath = Action.CommandPath;
					CommandArguments = Action.CommandArguments;
				}

				ToolElement.SetAttribute("Params", CommandArguments);
				ToolElement.SetAttribute("Path", CommandPath);
				ToolElement.SetAttribute("SkipIfProjectFailed", "true");
				if (Action.bIsGCCCompiler)
				{
					ToolElement.SetAttribute("AutoReserveMemory", "*.gch");
				}
				else
				{
					ToolElement.SetAttribute("AutoReserveMemory", "*.pch");
				}
				ToolElement.SetAttribute(
					"OutputFileMasks",
					string.Join(
						",",
						Action.ProducedItems.ConvertAll<string>(
							delegate(FileItem ProducedItem) { return ProducedItem.ToString(); }
							).ToArray()
						)
					);
			}

			// <Project Name="Default" Env="Default">...</Project>
			XmlElement ProjectElement = XGETaskDocument.CreateElement("Project");
			BuildSetElement.AppendChild(ProjectElement);
			ProjectElement.SetAttribute("Name", "Default");
			ProjectElement.SetAttribute("Env", "Default");

			for (int ActionIndex = 0; ActionIndex < Actions.Count; ActionIndex++)
			{
				Action Action = Actions[ActionIndex];

				// <Task ... />
				XmlElement TaskElement = XGETaskDocument.CreateElement("Task");
				ProjectElement.AppendChild(TaskElement);
				TaskElement.SetAttribute("SourceFile", "");
				if (!Action.bShouldOutputStatusDescription)
				{
					// If we were configured to not output a status description, then we'll instead
					// set 'caption' text for this task, so that the XGE coordinator has something
					// to display within the progress bars.  For tasks that are outputting a
					// description, XGE automatically displays that text in the progress bar, so we
					// only need to do this for tasks that output their own progress.
					TaskElement.SetAttribute("Caption", Action.StatusDescription);
				}
				TaskElement.SetAttribute("Name", string.Format("Action{0}", ActionIndex));
				TaskElement.SetAttribute("Tool", string.Format("Tool{0}", ActionIndex));
				TaskElement.SetAttribute("WorkingDir", Action.WorkingDirectory);
				TaskElement.SetAttribute("SkipIfProjectFailed", "true");

				// Create a semi-colon separated list of the other tasks this task depends on the results of.
				List<string> DependencyNames = new List<string>();
				foreach (FileItem Item in Action.PrerequisiteItems)
				{
					if (Item.ProducingAction != null && Actions.Contains(Item.ProducingAction))
					{
						DependencyNames.Add(string.Format("Action{0}", Actions.IndexOf(Item.ProducingAction)));
					}
				}

				if (DependencyNames.Count > 0)
				{
					TaskElement.SetAttribute("DependsOn", string.Join(";", DependencyNames.ToArray()));
				}
			}

			// Write the XGE task XML to a temporary file.
			using (FileStream OutputFileStream = new FileStream(TaskFilePath, FileMode.Create, FileAccess.Write))
			{
				XGETaskDocument.Save(OutputFileStream);
			}
		}

		/// <summary>
		/// The possible result of executing tasks with XGE.
		/// </summary>
		enum ExecutionResult
		{
			Unavailable,
			TasksFailed,
			TasksSucceeded,
		}

		/// <summary>
		/// Executes the tasks in the specified file.
		/// </summary>
		/// <param name="TaskFilePath">- The path to the file containing the tasks to execute in XGE XML format.</param>
		/// <param name="OutputEventHandler"></param>
		/// <param name="ActionCount"></param>
		/// <returns>Indicates whether the tasks were successfully executed.</returns>
		bool ExecuteTaskFile(string TaskFilePath, DataReceivedEventHandler OutputEventHandler, int ActionCount)
		{
			// A bug in the UCRT can cause XGE to hang on VS2015 builds. Figure out if this hang is likely to effect this build and workaround it if able.
			// @todo: There is a KB coming that will fix this. Once that KB is available, test if it is present. Stalls will not be a problem if it is.
			//
			// Stalls are possible. However there is a workaround in XGE build 1659 and newer that can avoid the issue.
			string XGEVersion = (string)Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Xoreax\IncrediBuild\Builder", "Version", null);
			if (XGEVersion != null)
			{
				int XGEBuildNumber;
				if (Int32.TryParse(XGEVersion, out XGEBuildNumber))
				{
					// Per Xoreax support, subtract 1001000 from the registry value to get the build number of the installed XGE.
					if (XGEBuildNumber - 1001000 >= 1659)
					{
						bXGENoWatchdogThread = true;
					}
					// @todo: Stalls are possible and we don't have a workaround. What should we do? Most people still won't encounter stalls, we don't really
					// want to disable XGE on them if it would have worked.
				}
			}

			bool bSilentCompileOutput = false;
			string SilentOption = bSilentCompileOutput ? "/Silent" : "";

			ProcessStartInfo XGEStartInfo = new ProcessStartInfo(
				"xgConsole",
				string.Format("\"{0}\" /Rebuild /NoWait {1} /NoLogo {2} /ShowTime {3}",
					TaskFilePath,
					bStopXGECompilationAfterErrors ? "/StopOnErrors" : "",
					SilentOption,
					bXGENoWatchdogThread ? "/no_watchdog_thread" : "")
				);
			XGEStartInfo.UseShellExecute = false;

			// Use the IDE-integrated Incredibuild monitor to display progress.
			XGEStartInfo.Arguments += " /UseIdeMonitor";

			// Optionally display the external XGE monitor.
			if (bShowXGEMonitor)
			{
				XGEStartInfo.Arguments += " /OpenMonitor";
			}

			try
			{
				// Start the process, redirecting stdout/stderr if requested.
				Process XGEProcess = new Process();
				XGEProcess.StartInfo = XGEStartInfo;
				bool bShouldRedirectOuput = OutputEventHandler != null;
				if (bShouldRedirectOuput)
				{
					XGEStartInfo.RedirectStandardError = true;
					XGEStartInfo.RedirectStandardOutput = true;
					XGEProcess.EnableRaisingEvents = true;
					XGEProcess.OutputDataReceived += OutputEventHandler;
					XGEProcess.ErrorDataReceived += OutputEventHandler;
				}
				XGEProcess.Start();
				if (bShouldRedirectOuput)
				{
					XGEProcess.BeginOutputReadLine();
					XGEProcess.BeginErrorReadLine();
				}

				Log.TraceInformation("Distributing {0} action{1} to XGE",
					ActionCount,
					ActionCount == 1 ? "" : "s");

				// Wait until the process is finished and return whether it all the tasks successfully executed.
				XGEProcess.WaitForExit();
				return XGEProcess.ExitCode == 0;
			}
			catch (Exception)
			{
				return false;
			}
		}

		/// <summary>
		/// Executes the tasks in the specified file, parsing progress markup as part of the output.
		/// </summary>
		bool ExecuteTaskFileWithProgressMarkup(string TaskFilePath, int NumActions, DataReceivedEventHandler OutputEventHandler)
		{
			using (ProgressWriter Writer = new ProgressWriter("Compiling C++ source files...", false))
			{
				int NumCompletedActions = 0;

				// Create a wrapper delegate that will parse the output actions
				DataReceivedEventHandler EventHandlerWrapper = (Sender, Args) =>
				{
					if (Args.Data != null && Args.Data.StartsWith(ProgressMarkupPrefix))
					{
						Writer.Write(++NumCompletedActions, NumActions);
						Args = ConstructDataReceivedEventArgs(Args.Data.Substring(ProgressMarkupPrefix.Length));
					}
					if (OutputEventHandler != null)
					{
						OutputEventHandler(Sender, Args);
					}
				};

				// Run through the standard XGE executor
				return ExecuteTaskFile(TaskFilePath, EventHandlerWrapper, NumActions);
			}
		}
	}
}
