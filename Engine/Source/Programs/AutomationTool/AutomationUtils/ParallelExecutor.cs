// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Xml;

namespace AutomationTool
{
	static class ParallelExecutor
	{
		[DebuggerDisplay("{Caption}")]
		class BuildAction
		{
			public int SortIndex = -1;

			public string Caption;
			public string GroupPrefix;
			public string OutputPrefix;
			public string ToolPath;
			public string ToolArguments;
			public string WorkingDirectory;
			public bool bSkipIfProjectFailed;

			public List<BuildAction> Dependants = new List<BuildAction>();
			public List<BuildAction> Dependencies = new List<BuildAction>();

			public int TotalDependants;
			public int MissingDependencyCount;

			public IReadOnlyDictionary<string, string> Environment;
		}

		class BuildActionExecutor
		{
			public BuildAction Action;
			public List<string> LogLines = new List<string>();
			public int ExitCode = -1;

			private ManualResetEvent CompletedEvent;
			private List<BuildActionExecutor> CompletedActions;

			public BuildActionExecutor(BuildAction InAction, ManualResetEvent InCompletedEvent, List<BuildActionExecutor> InCompletedActions)
			{
				Action = InAction;
				CompletedEvent = InCompletedEvent;
				CompletedActions = InCompletedActions;
			}

			public void Run()
			{
				if(!String.IsNullOrEmpty(Action.OutputPrefix))
				{
					LogLines.Add(Action.OutputPrefix);
				}

				using(ManagedProcess Process = new ManagedProcess(Action.ToolPath, Action.ToolArguments, Action.WorkingDirectory, Action.Environment, null, ManagedProcessPriority.BelowNormal))
				{
					LogLines.AddRange(Process.ReadAllLines());
					ExitCode = Process.ExitCode;
				}

				lock(CompletedActions)
				{
					CompletedActions.Add(this);
				}

				CompletedEvent.Set();
			}
		}

		public static int Execute(string ActionsFileName, bool bStopOnErrors)
		{
			return Execute(ActionsFileName, Environment.ProcessorCount, bStopOnErrors);
		}

		public static int Execute(string ActionsFileName, int MaxProcesses, bool bStopOnErrors)
		{
			List<BuildAction> Actions = ReadActions(ActionsFileName);

			CommandUtils.Log("Building with {0} {1}...", MaxProcesses, (MaxProcesses == 1)? "process" : "processes");

			using (UnrealBuildTool.ScopedLogIndent Indent = new UnrealBuildTool.ScopedLogIndent("  "))
			{
				// Create the list of things to process
				List<BuildAction> QueuedActions = new List<BuildAction>();
				foreach(BuildAction Action in Actions)
				{
					if(Action.MissingDependencyCount == 0)
					{
						QueuedActions.Add(Action);
					}
				}

				// Create a job object for all the child processes
				int ExitCode = 0;
				string CurrentPrefix = "";
				Dictionary<BuildActionExecutor, Thread> ExecutingActions = new Dictionary<BuildActionExecutor,Thread>();
				List<BuildActionExecutor> CompletedActions = new List<BuildActionExecutor>();

				using(ManualResetEvent CompletedEvent = new ManualResetEvent(false))
				{
					while(QueuedActions.Count > 0 || ExecutingActions.Count > 0)
					{
						// Sort the actions by the number of things dependent on them
						QueuedActions.Sort((A, B) => (A.TotalDependants == B.TotalDependants)? (B.SortIndex - A.SortIndex) : (B.TotalDependants - A.TotalDependants));

						// Create threads up to the maximum number of actions
						while(ExecutingActions.Count < MaxProcesses && QueuedActions.Count > 0)
						{
							BuildAction Action = QueuedActions[QueuedActions.Count - 1];
							QueuedActions.RemoveAt(QueuedActions.Count - 1);

							BuildActionExecutor ExecutingAction = new BuildActionExecutor(Action, CompletedEvent, CompletedActions);

							Thread ExecutingThread = new Thread(() => { ExecutingAction.Run(); });
							ExecutingThread.Name = String.Format("Build:{0}", Action.Caption);
							ExecutingThread.Start();

							ExecutingActions.Add(ExecutingAction, ExecutingThread);
						}

						// Wait for something to finish
						CompletedEvent.WaitOne();

						// Wait for something to finish and flush it to the log
						lock(CompletedActions)
						{
							foreach(BuildActionExecutor CompletedAction in CompletedActions)
							{
								// Join the thread
								Thread CompletedThread = ExecutingActions[CompletedAction];
								CompletedThread.Join();
								ExecutingActions.Remove(CompletedAction);

								// Write it to the log
								if(CompletedAction.LogLines.Count > 0)
								{
									if(CurrentPrefix != CompletedAction.Action.GroupPrefix)
									{
										CurrentPrefix = CompletedAction.Action.GroupPrefix;
										CommandUtils.Log(CurrentPrefix);
									}
									foreach(string LogLine in CompletedAction.LogLines)
									{
										CommandUtils.Log(LogLine);
									}
								}

								// Check the exit code
								if(CompletedAction.ExitCode == 0)
								{
									// Mark all the dependents as done
									foreach(BuildAction DependantAction in CompletedAction.Action.Dependants)
									{
										if(--DependantAction.MissingDependencyCount == 0)
										{
											QueuedActions.Add(DependantAction);
										}
									}
								}
								else
								{
									// Update the exit code if it's not already set
									if(ExitCode == 0)
									{
										ExitCode = CompletedAction.ExitCode;
									}
								}
							}
							CompletedActions.Clear();
						}

						// If we've already got a non-zero exit code, clear out the list of queued actions so nothing else will run
						if(ExitCode != 0 && bStopOnErrors)
						{
							QueuedActions.Clear();
						}
					}
				}

				return ExitCode;
			}
		}

		private static List<BuildAction> ReadActions(string InputPath)
		{
			XmlDocument File = new XmlDocument();
			File.Load(InputPath);

			XmlNode RootNode = File.FirstChild;
			if(RootNode == null || RootNode.Name != "BuildSet")
			{
				throw new Exception("Incorrect node at root of graph; expected 'BuildSet'");
			}

			XmlNode EnvironmentsNode = RootNode.SelectSingleNode("Environments");
			if(EnvironmentsNode == null)
			{
				throw new Exception("Missing Environments node under root in XML document");
			}

			// Get the current environment variables
			Dictionary<string, string> CurrentEnvironment = new Dictionary<string,string>(StringComparer.InvariantCultureIgnoreCase);
			foreach(System.Collections.DictionaryEntry Entry in Environment.GetEnvironmentVariables())
			{
				CurrentEnvironment[Entry.Key.ToString()] = Entry.Value.ToString();
			}

			// Read the tool environment
			Dictionary<string, XmlNode> NameToTool = new Dictionary<string,XmlNode>();
			Dictionary<string, IReadOnlyDictionary<string, string>> NameToEnvironment = new Dictionary<string,IReadOnlyDictionary<string, string>>();
			for(XmlNode EnvironmentNode = EnvironmentsNode.FirstChild; EnvironmentNode != null; EnvironmentNode = EnvironmentNode.NextSibling)
			{
				if(EnvironmentNode.Name == "Environment")
				{
					// Read the tools nodes
					XmlNode ToolsNode = EnvironmentNode.SelectSingleNode("Tools");
					if(ToolsNode != null)
					{
						for(XmlNode ToolNode = ToolsNode.FirstChild; ToolNode != null; ToolNode = ToolNode.NextSibling)
						{
							if(ToolNode.Name == "Tool")
							{
								XmlNode Name = ToolNode.Attributes.GetNamedItem("Name");
								if(Name != null)
								{
									NameToTool.Add(Name.Value, ToolNode);
								}
							}
						}
					}

					// Read the environment variables for this environment. Each environment has its own set of variables 
					XmlNode VariablesNode = EnvironmentNode.SelectSingleNode("Variables");
					if(VariablesNode != null)
					{
						XmlNode Name = EnvironmentNode.Attributes.GetNamedItem("Name");
						if(Name != null)
						{
							Dictionary<string, string> NamedEnvironment = new Dictionary<string,string>(CurrentEnvironment, StringComparer.InvariantCultureIgnoreCase);
							for (XmlNode VariableNode = VariablesNode.FirstChild; VariableNode != null; VariableNode = VariableNode.NextSibling)
							{
								if (VariableNode.Name == "Variable")
								{
									XmlNode VariableName = VariableNode.Attributes.GetNamedItem("Name");
									if(VariableName != null)
									{
										NamedEnvironment[VariableName.Value] = VariableNode.Attributes.GetNamedItem("Value").Value;
									}
								}
							}
							NameToEnvironment[Name.Value] = NamedEnvironment;
						}
					}
				}
			}

			// Read all the tasks for each project, and convert them into actions
			List<BuildAction> Actions = new List<BuildAction>();
			for(XmlNode ProjectNode = RootNode.FirstChild; ProjectNode != null; ProjectNode = ProjectNode.NextSibling)
			{
				if(ProjectNode.Name == "Project")
				{
					int SortIndex = 0;
					Dictionary<string, BuildAction> NameToAction = new Dictionary<string,BuildAction>();
					for(XmlNode TaskNode = ProjectNode.FirstChild; TaskNode != null; TaskNode = TaskNode.NextSibling)
					{
						if(TaskNode.Name == "Task")
						{
							XmlNode ToolNode;
							if(NameToTool.TryGetValue(TaskNode.Attributes.GetNamedItem("Tool").Value, out ToolNode))
							{
								BuildAction Action = FindOrAddAction(NameToAction, TaskNode.Attributes.GetNamedItem("Name").Value, Actions);
								Action.SortIndex = ++SortIndex;
								TryGetAttribute(TaskNode, "Caption", out Action.Caption);
								
								TryGetAttribute(ToolNode, "GroupPrefix", out Action.GroupPrefix);
								TryGetAttribute(ToolNode, "OutputPrefix", out Action.OutputPrefix);
								TryGetAttribute(ToolNode, "Path", out Action.ToolPath);
								TryGetAttribute(ToolNode, "WorkingDir", out Action.WorkingDirectory);

								string EnvironmentName;
								if(!TryGetAttribute(ProjectNode, "Env", out EnvironmentName))
								{
									Action.Environment = CurrentEnvironment;
								}
								else if(!NameToEnvironment.TryGetValue(EnvironmentName, out Action.Environment))
								{
									throw new Exception(String.Format("Couldn't find environment '{0}'", EnvironmentName));
								}

								if(TryGetAttribute(ToolNode, "Params", out Action.ToolArguments))
								{
									Action.ToolArguments = ExpandEnvironmentVariables(Action.ToolArguments, Action.Environment);
								}

								string SkipIfProjectFailed;
								if(TryGetAttribute(TaskNode, "SkipIfProjectFailed", out SkipIfProjectFailed) && SkipIfProjectFailed == "True")
								{
									Action.bSkipIfProjectFailed = true;
								}

								string DependsOnList;
								if(TryGetAttribute(TaskNode, "DependsOn", out DependsOnList))
								{
									foreach (string DependsOn in DependsOnList.Split(';'))
									{
										BuildAction DependsOnAction = FindOrAddAction(NameToAction, DependsOn, Actions);
										if(!Action.Dependencies.Contains(DependsOnAction))
										{
											Action.Dependencies.Add(DependsOnAction);
										}
										if(!DependsOnAction.Dependants.Contains(Action))
										{
											DependsOnAction.Dependants.Add(Action);
										}
									}
								}

								HashSet<BuildAction> VisitedActions = new HashSet<BuildAction>();
								RecursiveIncDependents(Action, VisitedActions);

								Action.MissingDependencyCount = Action.Dependencies.Count;
							}
						}
					}

					// Make sure there have been no actions added which were referenced but never declared
					foreach(KeyValuePair<string, BuildAction> Pair in NameToAction)
					{
						if(Pair.Value.SortIndex == -1)
						{
							throw new Exception(String.Format("Action {0} was referenced but never declared", Pair.Key));
						}
					}
				}
			}
			return Actions;
		}

		static bool TryGetAttribute(XmlNode Node, string Name, out string Value)
		{
			XmlNode Attribute = Node.Attributes.GetNamedItem(Name);
			if(Attribute == null)
			{
				Value = null;
				return false;
			}
			else
			{
				Value = Attribute.Value;
				return true;
			}
		}

		static string ExpandEnvironmentVariables(string Input, IReadOnlyDictionary<string, string> Environment)
		{
			string Output = Input;
			for(int StartIndex = 0;;)
			{
				int Index = Output.IndexOf("$(", StartIndex);
				if(Index == -1) break;

				int EndIndex = Output.IndexOf(")", Index);
				if(EndIndex == -1) break;

				string VariableName = Output.Substring(Index + 2, EndIndex - Index - 2);

				string VariableValue;
				if(Environment.TryGetValue(VariableName, out VariableValue))
				{
					Output = Output.Substring(0, Index) + VariableValue + Output.Substring(EndIndex + 1);
					StartIndex = Index + VariableValue.Length;
				}
				else
				{
					CommandUtils.LogWarning("Unknown environment variable '{0}' in script", VariableName);
					StartIndex = EndIndex + 1;
				}
			}
			return Output;
		}

		private static BuildAction FindOrAddAction(Dictionary<string, BuildAction> NameToAction, string Name, List<BuildAction> Actions)
		{
			BuildAction Action;
			if(!NameToAction.TryGetValue(Name, out Action))
			{
				Action = new BuildAction();
				Actions.Add(Action);
				NameToAction.Add(Name, Action);
			}
			return Action;
		}

		private static void RecursiveIncDependents(BuildAction Action, HashSet<BuildAction> VisitedActions)
		{
			foreach(BuildAction Dependency in Action.Dependencies)
			{
				if(!VisitedActions.Contains(Action))
				{
					VisitedActions.Add(Action);
					Dependency.TotalDependants++;
					RecursiveIncDependents(Dependency, VisitedActions);
				}
			}
		}
	}
}
