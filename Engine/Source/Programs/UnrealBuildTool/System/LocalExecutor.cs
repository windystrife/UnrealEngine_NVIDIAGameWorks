// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Linq;

namespace UnrealBuildTool
{
	class ActionThread
	{
		/// <summary>
		/// Cache the exit code from the command so that the executor can report errors
		/// </summary>
		public int ExitCode = 0;

		/// <summary>
		/// Set to true only when the local or RPC action is complete
		/// </summary>
		public bool bComplete = false;

		/// <summary>
		/// Cache the action that this thread is managing
		/// </summary>
		Action Action;

		/// <summary>
		/// For reporting status to the user
		/// </summary>
		int JobNumber;
		int TotalJobs;

		/// <summary>
		/// Regex that matches environment variables in $(Variable) format.
		/// </summary>
		static Regex EnvironmentVariableRegex = new Regex("\\$\\(([\\d\\w]+)\\)");

		/// <summary>
		/// Replaces the environment variables references in a string with their values.
		/// </summary>
		public static string ExpandEnvironmentVariables(string Text)
		{
			foreach (Match EnvironmentVariableMatch in EnvironmentVariableRegex.Matches(Text))
			{
				string VariableValue = Environment.GetEnvironmentVariable(EnvironmentVariableMatch.Groups[1].Value);
				Text = Text.Replace(EnvironmentVariableMatch.Value, VariableValue);
			}
			return Text;
		}

		/// <summary>
		/// Constructor, takes the action to process
		/// </summary>
		public ActionThread(Action InAction, int InJobNumber, int InTotalJobs)
		{
			Action = InAction;
			JobNumber = InJobNumber;
			TotalJobs = InTotalJobs;
		}

		/// <summary>
		/// Sends a string to an action's OutputEventHandler
		/// </summary>
		private static void SendOutputToEventHandler(Action Action, string Output)
		{
			// pass the output to any handler requested
			if (Action.OutputEventHandler != null)
			{
				// NOTE: This is a pretty nasty hack with C# reflection, however it saves us from having to replace all the
				// handlers in various Toolchains with a wrapper handler that takes a string - it is certainly doable, but 
				// touches code outside of this class that I don't want to touch right now

				// DataReceivedEventArgs is not normally constructable, so work around it with creating a scratch Args object
				DataReceivedEventArgs EventArgs = (DataReceivedEventArgs)System.Runtime.Serialization.FormatterServices.GetUninitializedObject(typeof(DataReceivedEventArgs));

				// now we need to set the Data field using reflection, since it is read only
				FieldInfo[] ArgFields = typeof(DataReceivedEventArgs).GetFields(BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.DeclaredOnly);

				// call the handler once per line
				string[] Lines = Output.Split("\r\n".ToCharArray());
				foreach (string Line in Lines)
				{
					// set the Data field
					ArgFields[0].SetValue(EventArgs, Line);

					// finally call the handler with this faked object
					Action.OutputEventHandler(Action, EventArgs);
				}
			}
			else
			{
				// if no handler, print it out!
				Log.TraceInformation(Output);
			}
		}


		/// <summary>
		/// Used when debuging Actions outputs all action return values to debug out
		/// </summary>
		/// <param name="sender"> Sending object</param>
		/// <param name="e">  Event arguments (In this case, the line of string output)</param>
		protected void ActionDebugOutput(object sender, DataReceivedEventArgs e)
		{
			string Output = e.Data;
			if (Output == null)
			{
				return;
			}

			Log.TraceInformation(Output);
		}


		/// <summary>
		/// The actual function to run in a thread. This is potentially long and blocking
		/// </summary>
		private void ThreadFunc()
		{
			// thread start time
			Action.StartTime = DateTimeOffset.Now;

			if (Action.ActionHandler != null)
			{
				// call the function and get the ExitCode and an output string
				string Output;
				Action.ActionHandler(Action, out ExitCode, out Output);

				// Output status description (file name) when no output is returned
				if (string.IsNullOrEmpty(Output))
				{
					if (Action.bShouldOutputStatusDescription)
					{
						Output = string.Format("[{0}/{1}] {2} {3}", JobNumber, TotalJobs, Action.CommandDescription, Action.StatusDescription);
					}
					else
					{
						Output = Action.StatusDescription;
					}
				}

				SendOutputToEventHandler(Action, Output);
			}
			else
			{
				// batch files (.bat, .cmd) need to be run via or cmd /c or shellexecute, 
				// the latter which we can't use because we want to redirect input/output
				bool bLaunchViaCmdExe = !Utils.IsRunningOnMono && !Path.GetExtension(Action.CommandPath).ToLower().EndsWith("exe");

				// Create the action's process.
				ProcessStartInfo ActionStartInfo = new ProcessStartInfo();
				ActionStartInfo.WorkingDirectory = ExpandEnvironmentVariables(Action.WorkingDirectory);


				string ExpandedCommandPath = ExpandEnvironmentVariables(Action.CommandPath);
				if (bLaunchViaCmdExe)
				{
					ActionStartInfo.FileName = "cmd.exe";
					ActionStartInfo.Arguments = string.Format
					(
						"/c \"{0} {1}\"",
						ExpandEnvironmentVariables(ExpandedCommandPath),
						ExpandEnvironmentVariables(Action.CommandArguments)
					);
				}
				else
				{
					ActionStartInfo.FileName = ExpandedCommandPath;
					ActionStartInfo.Arguments = ExpandEnvironmentVariables(Action.CommandArguments);
				}

				ActionStartInfo.UseShellExecute = false;
				ActionStartInfo.RedirectStandardInput = false;
				ActionStartInfo.RedirectStandardOutput = false;
				ActionStartInfo.RedirectStandardError = false;

				// Log command-line used to execute task if debug info printing is enabled.
				if (UnrealBuildTool.bPrintDebugInfo)
				{
					Log.TraceVerbose("Executing: {0} {1}", ExpandedCommandPath, ActionStartInfo.Arguments);
				}
				// Log summary if wanted.
				else if (Action.bShouldOutputStatusDescription)
				{
					string CommandDescription = Action.CommandDescription != null ? Action.CommandDescription : Path.GetFileName(ExpandedCommandPath);
					if (string.IsNullOrEmpty(CommandDescription))
					{
						Log.TraceInformation(Action.StatusDescription);
					}
					else
					{
						Log.TraceInformation("[{0}/{1}] {2} {3}", JobNumber, TotalJobs, CommandDescription, Action.StatusDescription);
					}
				}

				// Try to launch the action's process, and produce a friendly error message if it fails.
				Process ActionProcess = null;
				try
				{
					try
					{
						ActionProcess = new Process();
						ActionProcess.StartInfo = ActionStartInfo;
						bool bShouldRedirectOuput = Action.OutputEventHandler != null || Action.bPrintDebugInfo;
						if (bShouldRedirectOuput)
						{
							ActionStartInfo.RedirectStandardOutput = true;
							ActionStartInfo.RedirectStandardError = true;
							ActionProcess.EnableRaisingEvents = true;

							if (Action.OutputEventHandler != null)
							{
								ActionProcess.OutputDataReceived += Action.OutputEventHandler;
								ActionProcess.ErrorDataReceived += Action.OutputEventHandler;
							}

							if (Action.bPrintDebugInfo)
							{
								ActionProcess.OutputDataReceived += new DataReceivedEventHandler(ActionDebugOutput);
								ActionProcess.ErrorDataReceived += new DataReceivedEventHandler(ActionDebugOutput);
							}
						}
						ActionProcess.Start();
						if (bShouldRedirectOuput)
						{
							ActionProcess.BeginOutputReadLine();
							ActionProcess.BeginErrorReadLine();
						}
					}
					catch (Exception ex)
					{
						Log.TraceError("Failed to start local process for action: {0} {1}", Action.CommandPath, Action.CommandArguments);
						ExceptionUtils.PrintExceptionInfo(ex, null);
						ExitCode = 1;
						bComplete = true;
						return;
					}

					// wait for process to start
					// NOTE: this may or may not be necessary; seems to depend on whether the system UBT is running on start the process in a timely manner.
					int checkIterations = 0;
					bool haveConfiguredProcess = false;
					do
					{
						if (ActionProcess.HasExited)
						{
							if (haveConfiguredProcess == false)
								Debug.WriteLine("Process for action exited before able to configure!");
							break;
						}

						if (!haveConfiguredProcess)
						{
							try
							{
								ActionProcess.PriorityClass = ProcessPriorityClass.BelowNormal;
								haveConfiguredProcess = true;
							}
							catch (Exception)
							{
							}
							break;
						}

						Thread.Sleep(10);

						checkIterations++;
					} while (checkIterations < 100);
					if (checkIterations == 100)
					{
						throw new BuildException("Failed to configure local process for action: {0} {1}", Action.CommandPath, Action.CommandArguments);
					}

					// block until it's complete
					// @todo iosmerge: UBT had started looking at:	if (Utils.IsValidProcess(Process))
					//    do we need to check that in the thread model?
					ActionProcess.WaitForExit();

					// capture exit code
					ExitCode = ActionProcess.ExitCode;
				}
				finally
				{
					// As the process has finished now, free its resources. On non-Windows platforms, processes depend 
					// on POSIX/BSD threading and these are limited per application. Disposing the Process releases 
					// these thread resources.
					if (ActionProcess != null)
						ActionProcess.Close();
				}
			}

			// track how long it took
			Action.EndTime = DateTimeOffset.Now;

			if (!Utils.IsRunningOnMono)
			{
				// let RPCUtilHelper clean up anything thread related
				RPCUtilHelper.OnThreadComplete();
			}

			// we are done!!
			bComplete = true;
		}

		/// <summary>
		/// Starts a thread and runs the action in that thread
		/// </summary>
		public void Run()
		{
			Thread T = new Thread(ThreadFunc);
			T.Start();
		}
	};

	class LocalExecutor : ActionExecutor
	{
		/// <summary>
		/// Processor count multiplier for local execution. Can be below 1 to reserve CPU for other tasks.
		/// When using the local executor (not XGE), run a single action on each CPU core.  Note that you can set this to a larger value
		/// to get slightly faster build times in many cases, but your computer's responsiveness during compiling may be much worse.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		double ProcessorCountMultiplier = 1.0;

		/// <summary>
		/// Maximum processor count for local execution. 
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		int MaxProcessorCount = int.MaxValue;

		public LocalExecutor()
		{
			XmlConfig.ApplyTo(this);
		}

		public override string Name
		{
			get { return "Local"; }
		}

		public virtual double AdjustedProcessorCountMultiplier
		{
			get { return ProcessorCountMultiplier; }
		}

		/// <summary>
		/// Executes the specified actions locally.
		/// </summary>
		/// <returns>True if all the tasks successfully executed, or false if any of them failed.</returns>
		public override bool ExecuteActions(List<Action> Actions, bool bLogDetailedActionStats)
		{
			// Time to sleep after each iteration of the loop in order to not busy wait.
			const float LoopSleepTime = 0.1f;

			// Use WMI to figure out physical cores, excluding hyper threading.
			int NumCores = Utils.GetPhysicalProcessorCount();
			if (NumCores == -1)
			{
				NumCores = System.Environment.ProcessorCount;
			}
			// The number of actions to execute in parallel is trying to keep the CPU busy enough in presence of I/O stalls.
			int MaxActionsToExecuteInParallel = 0;
			if (NumCores < System.Environment.ProcessorCount && ProcessorCountMultiplier != 1.0)
			{
				// The CPU has more logical cores than physical ones, aka uses hyper-threading. 
				// Use multiplier if provided
				MaxActionsToExecuteInParallel = (int)(NumCores * ProcessorCountMultiplier);
			}
			else if (NumCores < System.Environment.ProcessorCount && NumCores > 4)
			{
				// The CPU has more logical cores than physical ones, aka uses hyper-threading. 
				// Use average of logical and physical if we have "lots of cores"
				MaxActionsToExecuteInParallel = (int)(NumCores + System.Environment.ProcessorCount) / 2;
			}
			// No hyper-threading. Only kicking off a task per CPU to keep machine responsive.
			else
			{
				MaxActionsToExecuteInParallel = NumCores;
			}

#if !NET_CORE
			if (Utils.IsRunningOnMono)
			{
				long PhysicalRAMAvailableMB = (new PerformanceCounter("Mono Memory", "Total Physical Memory").RawValue) / (1024 * 1024);
				// heuristic: give each action at least 1.5GB of RAM (some clang instances will need more) if the total RAM is low, or 1GB on 16+GB machines
				long MinMemoryPerActionMB = (PhysicalRAMAvailableMB < 16384) ? 3 * 1024 / 2 : 1024;
				int MaxActionsAffordedByMemory = (int)(Math.Max(1, (PhysicalRAMAvailableMB) / MinMemoryPerActionMB));

				MaxActionsToExecuteInParallel = Math.Min(MaxActionsToExecuteInParallel, MaxActionsAffordedByMemory);
			}
#endif

			MaxActionsToExecuteInParallel = Math.Max(1, Math.Min(MaxActionsToExecuteInParallel, MaxProcessorCount));

			Log.TraceInformation("Performing {0} actions ({1} in parallel)", Actions.Count, MaxActionsToExecuteInParallel);

			Dictionary<Action, ActionThread> ActionThreadDictionary = new Dictionary<Action, ActionThread>();
			int JobNumber = 1;
			using (ProgressWriter ProgressWriter = new ProgressWriter("Compiling C++ source code...", false))
			{
				int ProgressValue = 0;
				while (true)
				{
					// Count the number of pending and still executing actions.
					int NumUnexecutedActions = 0;
					int NumExecutingActions = 0;
					foreach (Action Action in Actions)
					{
						ActionThread ActionThread = null;
						bool bFoundActionProcess = ActionThreadDictionary.TryGetValue(Action, out ActionThread);
						if (bFoundActionProcess == false)
						{
							NumUnexecutedActions++;
						}
						else if (ActionThread != null)
						{
							if (ActionThread.bComplete == false)
							{
								NumUnexecutedActions++;
								NumExecutingActions++;
							}
						}
					}

					// Update the current progress
					int NewProgressValue = Actions.Count + 1 - NumUnexecutedActions;
					if (ProgressValue != NewProgressValue)
					{
						ProgressWriter.Write(ProgressValue, Actions.Count + 1);
						ProgressValue = NewProgressValue;
					}

					// If there aren't any pending actions left, we're done executing.
					if (NumUnexecutedActions == 0)
					{
						break;
					}

					// If there are fewer actions executing than the maximum, look for pending actions that don't have any outdated
					// prerequisites.
					foreach (Action Action in Actions)
					{
						ActionThread ActionProcess = null;
						bool bFoundActionProcess = ActionThreadDictionary.TryGetValue(Action, out ActionProcess);
						if (bFoundActionProcess == false)
						{
							if (NumExecutingActions < Math.Max(1, MaxActionsToExecuteInParallel))
							{
								// Determine whether there are any prerequisites of the action that are outdated.
								bool bHasOutdatedPrerequisites = false;
								bool bHasFailedPrerequisites = false;
								foreach (FileItem PrerequisiteItem in Action.PrerequisiteItems)
								{
									if (PrerequisiteItem.ProducingAction != null && Actions.Contains(PrerequisiteItem.ProducingAction))
									{
										ActionThread PrerequisiteProcess = null;
										bool bFoundPrerequisiteProcess = ActionThreadDictionary.TryGetValue(PrerequisiteItem.ProducingAction, out PrerequisiteProcess);
										if (bFoundPrerequisiteProcess == true)
										{
											if (PrerequisiteProcess == null)
											{
												bHasFailedPrerequisites = true;
											}
											else if (PrerequisiteProcess.bComplete == false)
											{
												bHasOutdatedPrerequisites = true;
											}
											else if (PrerequisiteProcess.ExitCode != 0)
											{
												bHasFailedPrerequisites = true;
											}
										}
										else
										{
											bHasOutdatedPrerequisites = true;
										}
									}
								}

								// If there are any failed prerequisites of this action, don't execute it.
								if (bHasFailedPrerequisites)
								{
									// Add a null entry in the dictionary for this action.
									ActionThreadDictionary.Add(Action, null);
								}
								// If there aren't any outdated prerequisites of this action, execute it.
								else if (!bHasOutdatedPrerequisites)
								{
									ActionThread ActionThread = new ActionThread(Action, JobNumber, Actions.Count);
									JobNumber++;

									try
									{
										ActionThread.Run();
									}
									catch (Exception ex)
									{
										throw new BuildException(ex, "Failed to start thread for action: {0} {1}\r\n{2}", Action.CommandPath, Action.CommandArguments, ex.ToString());
									}

									ActionThreadDictionary.Add(Action, ActionThread);

									NumExecutingActions++;
								}
							}
						}
					}

					System.Threading.Thread.Sleep(TimeSpan.FromSeconds(LoopSleepTime));
				}
			}

			Log.WriteLineIf(bLogDetailedActionStats, LogEventType.Console, "-------- Begin Detailed Action Stats ----------------------------------------------------------");
			Log.WriteLineIf(bLogDetailedActionStats, LogEventType.Console, "^Action Type^Duration (seconds)^Tool^Task^Using PCH");

			double TotalThreadSeconds = 0;

			// Check whether any of the tasks failed and log action stats if wanted.
			bool bSuccess = true;
			double TotalBuildProjectTime = 0;
			double TotalCompileTime = 0;
			double TotalCreateAppBundleTime = 0;
			double TotalGenerateDebugInfoTime = 0;
			double TotalLinkTime = 0;
			double TotalOtherActionsTime = 0;
			foreach (KeyValuePair<Action, ActionThread> ActionProcess in ActionThreadDictionary)
			{
				Action Action = ActionProcess.Key;
				ActionThread ActionThread = ActionProcess.Value;

				// Check for pending actions, preemptive failure
				if (ActionThread == null)
				{
					bSuccess = false;
					continue;
				}
				// Check for executed action but general failure
				if (ActionThread.ExitCode != 0)
				{
					bSuccess = false;
				}
				// Log CPU time, tool and task.
				double ThreadSeconds = Action.Duration.TotalSeconds;

				Log.WriteLineIf(bLogDetailedActionStats,
					LogEventType.Console,
					"^{0}^{1:0.00}^{2}^{3}^{4}",
					Action.ActionType.ToString(),
					ThreadSeconds,
					Path.GetFileName(Action.CommandPath),
					  Action.StatusDescription,
					Action.bIsUsingPCH);

				// Update statistics
				switch (Action.ActionType)
				{
					case ActionType.BuildProject:
						TotalBuildProjectTime += ThreadSeconds;
						break;

					case ActionType.Compile:
						TotalCompileTime += ThreadSeconds;
						break;

					case ActionType.CreateAppBundle:
						TotalCreateAppBundleTime += ThreadSeconds;
						break;

					case ActionType.GenerateDebugInfo:
						TotalGenerateDebugInfoTime += ThreadSeconds;
						break;

					case ActionType.Link:
						TotalLinkTime += ThreadSeconds;
						break;

					default:
						TotalOtherActionsTime += ThreadSeconds;
						break;
				}

				// Keep track of total thread seconds spent on tasks.
				TotalThreadSeconds += ThreadSeconds;
			}

			Log.WriteLineIf(bLogDetailedActionStats, LogEventType.Console, "-------- End Detailed Actions Stats -----------------------------------------------------------");

			// Log total CPU seconds and numbers of processors involved in tasks.
			Log.WriteLineIf(bLogDetailedActionStats || UnrealBuildTool.bPrintDebugInfo,
				LogEventType.Console, "Cumulative thread seconds ({0} processors): {1:0.00}", System.Environment.ProcessorCount, TotalThreadSeconds);

			// Log detailed stats
			Log.WriteLineIf(bLogDetailedActionStats || UnrealBuildTool.bPrintDebugInfo,
				LogEventType.Console,
				"Cumulative action seconds ({0} processors): {1:0.00} building projects, {2:0.00} compiling, {3:0.00} creating app bundles, {4:0.00} generating debug info, {5:0.00} linking, {6:0.00} other",
				System.Environment.ProcessorCount,
				TotalBuildProjectTime,
				TotalCompileTime,
				TotalCreateAppBundleTime,
				TotalGenerateDebugInfoTime,
				TotalLinkTime,
				TotalOtherActionsTime
			);

			return bSuccess;
		}
	};
}
