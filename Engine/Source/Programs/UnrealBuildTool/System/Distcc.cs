// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Xml;
using System.Text.RegularExpressions;
using System.Linq;
using System.Reflection;

namespace UnrealBuildTool
{
	class Distcc : ActionExecutor
	{
		/// <summary>
		/// When enabled allows DMUCS/Distcc to fallback to local compilation when remote compiling fails. Defaults to true as separation of pre-process and compile stages can introduce non-fatal errors.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		public bool bAllowDistccLocalFallback = true;

		/// <summary>
		/// When true enable verbose distcc output to aid debugging.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		public bool bVerboseDistccOutput = false;

		/// <summary>
		/// Path to the Distcc and DMUCS executables.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		public string DistccExecutablesPath = "/usr/local/bin";

		/// <summary>
		/// DMUCS coordinator hostname or IP address.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		public string DMUCSCoordinator = "localhost";

		/// <summary>
		/// The DMUCS distinguishing property value to request for compile hosts.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		public string DMUCSDistProp = "";

		public Distcc()
		{
			XmlConfig.ApplyTo(this);

			// The default for normal Mac users should be to use DistCode which installs as an Xcode plugin and provides dynamic host management
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
			{
				string UserDir = Environment.GetEnvironmentVariable("HOME");
				string MacDistccExecutablesPath = UserDir + "/Library/Application Support/Developer/Shared/Xcode/Plug-ins/Distcc 3.2.xcplugin/Contents/usr/bin";

				// But we should use the standard Posix directory when not installed - a build farm would use a static .dmucs hosts file not DistCode.
				if (System.IO.Directory.Exists(MacDistccExecutablesPath))
				{
					DistccExecutablesPath = MacDistccExecutablesPath;
				}

				if (System.IO.File.Exists(UserDir + "/Library/Preferences/com.marksatt.DistCode.plist"))
				{
					using (System.Diagnostics.Process DefaultsProcess = new System.Diagnostics.Process())
					{
						try
						{
							DefaultsProcess.StartInfo.FileName = "/usr/bin/defaults";
							DefaultsProcess.StartInfo.CreateNoWindow = true;
							DefaultsProcess.StartInfo.UseShellExecute = false;
							DefaultsProcess.StartInfo.RedirectStandardOutput = true;
							DefaultsProcess.StartInfo.RedirectStandardError = true;
							DefaultsProcess.StartInfo.Arguments = "read com.marksatt.DistCode DistProp";
							DefaultsProcess.Start();
							string Output = DefaultsProcess.StandardOutput.ReadToEnd();
							DefaultsProcess.WaitForExit();
							if (DefaultsProcess.ExitCode == 0)
							{
								DMUCSDistProp = Output;
							}
						}
						catch (Exception)
						{
						}
					}
                    using (System.Diagnostics.Process CoordModeProcess = new System.Diagnostics.Process())
                    {
                        using (System.Diagnostics.Process DefaultsProcess = new System.Diagnostics.Process())
                        {
                            try
                            {
                                CoordModeProcess.StartInfo.FileName = "/usr/bin/defaults";
                                CoordModeProcess.StartInfo.CreateNoWindow = true;
                                CoordModeProcess.StartInfo.UseShellExecute = false;
                                CoordModeProcess.StartInfo.RedirectStandardOutput = true;
								CoordModeProcess.StartInfo.RedirectStandardError = true;
                                CoordModeProcess.StartInfo.Arguments = "read com.marksatt.DistCode CoordinatorMode";
                                CoordModeProcess.Start();
                                string CoordModeProcessOutput = CoordModeProcess.StandardOutput.ReadToEnd();
                                CoordModeProcess.WaitForExit();
                                if (CoordModeProcess.ExitCode == 0 && CoordModeProcessOutput.StartsWith("1"))
                                {
                                    DefaultsProcess.StartInfo.FileName = "/usr/bin/defaults";
                                    DefaultsProcess.StartInfo.CreateNoWindow = true;
                                    DefaultsProcess.StartInfo.UseShellExecute = false;
                                    DefaultsProcess.StartInfo.RedirectStandardOutput = true;
									DefaultsProcess.StartInfo.RedirectStandardError = true;
                                    DefaultsProcess.StartInfo.Arguments = "read com.marksatt.DistCode CoordinatorIP";
                                    DefaultsProcess.Start();
                                    string Output = DefaultsProcess.StandardOutput.ReadToEnd();
                                    DefaultsProcess.WaitForExit();
                                    if (DefaultsProcess.ExitCode == 0)
                                    {
                                        DMUCSCoordinator = Output;
                                    }
                                }
                            }
                            catch (Exception)
                            {
                            }
                        }
                    }
				}
			}
		}

		public override string Name
		{
			get { return "Distcc"; }
		}

		public override bool ExecuteActions(List<Action> Actions, bool bLogDetailedActionStats)
		{
			bool bDistccResult = true;
			if (Actions.Count > 0)
			{
				// Time to sleep after each iteration of the loop in order to not busy wait.
				const float LoopSleepTime = 0.1f;

				int MaxActionsToExecuteInParallel = 0;

				string UserDir = Environment.GetEnvironmentVariable("HOME");
				string HostsInfo = UserDir + "/.dmucs/hosts-info";
				string NumUBTBuildTasks = Environment.GetEnvironmentVariable("NumUBTBuildTasks");
				Int32 MaxUBTBuildTasks = MaxActionsToExecuteInParallel;
				if (Int32.TryParse(NumUBTBuildTasks, out MaxUBTBuildTasks))
				{
					MaxActionsToExecuteInParallel = MaxUBTBuildTasks;
				}
				else if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				{
					using (System.Diagnostics.Process DefaultsProcess = new System.Diagnostics.Process())
					{
						try
						{
							DefaultsProcess.StartInfo.FileName = "/usr/bin/defaults";
							DefaultsProcess.StartInfo.CreateNoWindow = true;
							DefaultsProcess.StartInfo.UseShellExecute = false;
							DefaultsProcess.StartInfo.RedirectStandardOutput = true;
							DefaultsProcess.StartInfo.Arguments = "read com.apple.dt.Xcode IDEBuildOperationMaxNumberOfConcurrentCompileTasks";
							DefaultsProcess.Start();
							string Output = DefaultsProcess.StandardOutput.ReadToEnd();
							DefaultsProcess.WaitForExit();
							if (DefaultsProcess.ExitCode == 0 && Int32.TryParse(Output, out MaxUBTBuildTasks))
							{
								MaxActionsToExecuteInParallel = MaxUBTBuildTasks;
							}
						}
						catch (Exception)
						{
						}
					}
				}
				else if (System.IO.File.Exists(HostsInfo))
				{
					System.IO.StreamReader File = new System.IO.StreamReader(HostsInfo);
					string Line = null;
					while ((Line = File.ReadLine()) != null)
					{
						var HostInfo = Line.Split(' ');
						if (HostInfo.Count() == 3)
						{
							int NumCPUs = 0;
							if (System.Int32.TryParse(HostInfo[1], out NumCPUs))
							{
								MaxActionsToExecuteInParallel += NumCPUs;
							}
						}
					}
					File.Close();
				}
				else
				{
					MaxActionsToExecuteInParallel = System.Environment.ProcessorCount;
				}

				if (bAllowDistccLocalFallback == false)
				{
					Environment.SetEnvironmentVariable("DISTCC_FALLBACK", "0");
				}

				if (bVerboseDistccOutput == true)
				{
					Environment.SetEnvironmentVariable("DISTCC_VERBOSE", "1");
				}

				string DistccExecutable = DistccExecutablesPath + "/distcc";
				string GetHostExecutable = DistccExecutablesPath + "/gethost";

				Log.TraceInformation("Performing {0} actions ({1} in parallel)", Actions.Count, MaxActionsToExecuteInParallel, DistccExecutable, GetHostExecutable);

				Dictionary<Action, ActionThread> ActionThreadDictionary = new Dictionary<Action, ActionThread>();
				int JobNumber = 1;
				using (ProgressWriter ProgressWriter = new ProgressWriter("Compiling source code...", false))
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
										if (Action.ActionType == ActionType.Compile || Action.ActionType == ActionType.Link)
										{
											string TypeCommand = String.IsNullOrEmpty(DMUCSDistProp) ? "" : (" --type " + DMUCSDistProp);
											string NewCommandArguments = "--server " + DMUCSCoordinator + TypeCommand + " --wait -1 \"" + DistccExecutable + "\" " + Action.CommandPath + " " + Action.CommandArguments;
											
											if (Action.ActionType == ActionType.Compile)
											{
												// Distcc separates preprocessing and compilation which means we must silence these warnings
												NewCommandArguments += " -Wno-constant-logical-operand";
												NewCommandArguments += " -Wno-parentheses-equality";
											}
											
											Action.CommandPath = GetHostExecutable;
											Action.CommandArguments = NewCommandArguments;
										}

										ActionThread ActionThread = new ActionThread(Action, JobNumber, Actions.Count);
										JobNumber++;
										ActionThread.Run();

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
				foreach (KeyValuePair<Action, ActionThread> ActionProcess in ActionThreadDictionary)
				{
					Action Action = ActionProcess.Key;
					ActionThread ActionThread = ActionProcess.Value;

					// Check for pending actions, preemptive failure
					if (ActionThread == null)
					{
						bDistccResult = false;
						continue;
					}
					// Check for executed action but general failure
					if (ActionThread.ExitCode != 0)
					{
						bDistccResult = false;
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

					// Keep track of total thread seconds spent on tasks.
					TotalThreadSeconds += ThreadSeconds;
				}

				Log.WriteLineIf(bLogDetailedActionStats || UnrealBuildTool.bPrintDebugInfo,
					LogEventType.Console,
					"-------- End Detailed Actions Stats -----------------------------------------------------------");

				// Log total CPU seconds and numbers of processors involved in tasks.
				Log.WriteLineIf(bLogDetailedActionStats || UnrealBuildTool.bPrintDebugInfo,
					LogEventType.Console, "Cumulative thread seconds ({0} processors): {1:0.00}", System.Environment.ProcessorCount, TotalThreadSeconds);
			}
			return bDistccResult;
		}
	}
}
