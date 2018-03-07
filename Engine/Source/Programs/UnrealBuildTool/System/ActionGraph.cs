// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Linq;
using System.Runtime.Serialization;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Enumerates build action types.
	/// </summary>
	enum ActionType
	{
		BuildProject,

		Compile,

		CreateAppBundle,

		GenerateDebugInfo,

		Link,
	}

	/// <summary>
	/// A build action.
	/// </summary>
	[Serializable]
	class Action : ISerializable
	{
		///
		/// Preparation and Assembly (serialized)
		/// 

		/// <summary>
		/// The type of this action (for debugging purposes).
		/// </summary>
		public readonly ActionType ActionType;

		/// <summary>
		/// Every file this action depends on.  These files need to exist and be up to date in order for this action to even be considered
		/// </summary>
		public List<FileItem> PrerequisiteItems = new List<FileItem>();

		/// <summary>
		/// The files that this action produces after completing
		/// </summary>
		public List<FileItem> ProducedItems = new List<FileItem>();

		/// <summary>
		/// Directory from which to execute the program to create produced items
		/// </summary>
		public string WorkingDirectory = null;

		/// <summary>
		/// True if we should log extra information when we run a program to create produced items
		/// </summary>
		public bool bPrintDebugInfo = false;

		/// <summary>
		/// The command to run to create produced items
		/// </summary>
		public string CommandPath = null;

		/// <summary>
		/// Command-line parameters to pass to the program
		/// </summary>
		public string CommandArguments = null;

		/// <summary>
		/// Optional friendly description of the type of command being performed, for example "Compile" or "Link".  Displayed by some executors.
		/// </summary>
		public string CommandDescription = null;

		/// <summary>
		/// Human-readable description of this action that may be displayed as status while invoking the action.  This is often the name of the file being compiled, or an executable file name being linked.  Displayed by some executors.
		/// </summary>
		public string StatusDescription = "...";

		/// <summary>
		/// True if this action is allowed to be run on a remote machine when a distributed build system is being used, such as XGE
		/// </summary>
		public bool bCanExecuteRemotely = false;

		/// <summary>
		/// True if this action is allowed to be run on a remote machine with SNDBS. Files with #import directives must be compiled locally. Also requires bCanExecuteRemotely = true.
		/// </summary>
		public bool bCanExecuteRemotelyWithSNDBS = true;

		/// <summary>
		/// True if this action is using the GCC compiler.  Some build systems may be able to optimize for this case.
		/// </summary>
		public bool bIsGCCCompiler = false;

		/// <summary>
		/// Whether the action is using a pre-compiled header to speed it up.
		/// </summary>
		public bool bIsUsingPCH = false;

		/// <summary>
		/// Whether the files in ProducedItems should be deleted before executing this action, when the action is outdated
		/// </summary>
		public bool bShouldDeleteProducedItems = false;

		/// <summary>
		/// Whether we should log this action, whether executed locally or remotely.  This is useful for actions that take time
		/// but invoke tools without any console output.
		/// </summary>
		public bool bShouldOutputStatusDescription = true;

		/// <summary>
		/// Whether this action includes incremental linking.
		/// </summary>
		public bool bUseIncrementalLinking = false;

		/// <summary>
		/// True if any libraries produced by this action should be considered 'import libraries'
		/// </summary>
		public bool bProducesImportLibrary = false;

		/// <summary>
		/// Optional custom event handler for standard output.
		/// </summary>
		public DataReceivedEventHandler OutputEventHandler = null;	// @todo ubtmake urgent: Delegate variables are not saved, but we are comparing against this in ExecuteActions() for XGE!

		/// <summary>
		/// Callback used to perform a special action instead of a generic command line
		/// </summary>
		public delegate void BlockingActionHandler(Action Action, out int ExitCode, out string Output);
		public BlockingActionHandler ActionHandler = null;		// @todo ubtmake urgent: Delegate variables are not saved, but we are comparing against this in ExecuteActions() for XGE!



		///
		/// Preparation only (not serialized)
		///

		/// <summary>
		/// Unique action identifier.  Used for displaying helpful info about detected cycles in the graph.
		/// </summary>
		public readonly int UniqueId;

		/// <summary>
		/// Total number of actions depending on this one.
		/// </summary>
		public int NumTotalDependentActions = 0;

		/// <summary>
		/// Relative cost of producing items for this action.
		/// </summary>
		public long RelativeCost = 0;


		///
		/// Assembly only (not serialized)
		///

		/// <summary>
		/// Start time of action, optionally set by executor.
		/// </summary>
		public DateTimeOffset StartTime = DateTimeOffset.MinValue;

		/// <summary>
		/// End time of action, optionally set by executor.
		/// </summary>
		public DateTimeOffset EndTime = DateTimeOffset.MinValue;


		public Action(ActionType InActionType, int InUniqueId)
		{
			ActionType = InActionType;
			UniqueId = InUniqueId;
		}

		public Action(SerializationInfo SerializationInfo, StreamingContext StreamingContext)
		{
			ActionType = (ActionType)SerializationInfo.GetByte("at");
			WorkingDirectory = SerializationInfo.GetString("wd");
			bPrintDebugInfo = SerializationInfo.GetBoolean("di");
			CommandPath = SerializationInfo.GetString("cp");
			CommandArguments = SerializationInfo.GetString("ca");
			CommandDescription = SerializationInfo.GetString("cd");
			StatusDescription = SerializationInfo.GetString("sd");
			bCanExecuteRemotely = SerializationInfo.GetBoolean("ce");
			bCanExecuteRemotelyWithSNDBS = SerializationInfo.GetBoolean("cs");
			bIsGCCCompiler = SerializationInfo.GetBoolean("ig");
			bIsUsingPCH = SerializationInfo.GetBoolean("iu");
			bShouldDeleteProducedItems = SerializationInfo.GetBoolean("dp");
			bShouldOutputStatusDescription = SerializationInfo.GetBoolean("os");
			bUseIncrementalLinking = SerializationInfo.GetBoolean("in");
			bProducesImportLibrary = SerializationInfo.GetBoolean("il");
			PrerequisiteItems = (List<FileItem>)SerializationInfo.GetValue("pr", typeof(List<FileItem>));
			ProducedItems = (List<FileItem>)SerializationInfo.GetValue("pd", typeof(List<FileItem>));
		}

		/// <summary>
		/// ISerializable: Called when serialized to report additional properties that should be saved
		/// </summary>
		public void GetObjectData(SerializationInfo SerializationInfo, StreamingContext StreamingContext)
		{
			SerializationInfo.AddValue("at", (byte)ActionType);
			SerializationInfo.AddValue("wd", WorkingDirectory);
			SerializationInfo.AddValue("di", bPrintDebugInfo);
			SerializationInfo.AddValue("cp", CommandPath);
			SerializationInfo.AddValue("ca", CommandArguments);
			SerializationInfo.AddValue("cd", CommandDescription);
			SerializationInfo.AddValue("sd", StatusDescription);
			SerializationInfo.AddValue("ce", bCanExecuteRemotely);
			SerializationInfo.AddValue("cs", bCanExecuteRemotelyWithSNDBS);
			SerializationInfo.AddValue("ig", bIsGCCCompiler);
			SerializationInfo.AddValue("iu", bIsUsingPCH);
			SerializationInfo.AddValue("dp", bShouldDeleteProducedItems);
			SerializationInfo.AddValue("os", bShouldOutputStatusDescription);
			SerializationInfo.AddValue("in", bUseIncrementalLinking);
			SerializationInfo.AddValue("il", bProducesImportLibrary);
			SerializationInfo.AddValue("pr", PrerequisiteItems);
			SerializationInfo.AddValue("pd", ProducedItems);
		}

		/// <summary>
		/// Compares two actions based on total number of dependent items, descending.
		/// </summary>
		/// <param name="A">Action to compare</param>
		/// <param name="B">Action to compare</param>
		public static int Compare(Action A, Action B)
		{
			// Primary sort criteria is total number of dependent files, up to max depth.
			if (B.NumTotalDependentActions != A.NumTotalDependentActions)
			{
				return Math.Sign(B.NumTotalDependentActions - A.NumTotalDependentActions);
			}
			// Secondary sort criteria is relative cost.
			if (B.RelativeCost != A.RelativeCost)
			{
				return Math.Sign(B.RelativeCost - A.RelativeCost);
			}
			// Tertiary sort criteria is number of pre-requisites.
			else
			{
				return Math.Sign(B.PrerequisiteItems.Count - A.PrerequisiteItems.Count);
			}
		}

		public override string ToString()
		{
			string ReturnString = "";
			if (CommandPath != null)
			{
				ReturnString += CommandPath + " - ";
			}
			if (CommandArguments != null)
			{
				ReturnString += CommandArguments;
			}
			return ReturnString;
		}

		/// <summary>
		/// Returns the amount of time that this action is or has been executing in.
		/// </summary>
		public TimeSpan Duration
		{
			get
			{
				if (EndTime == DateTimeOffset.MinValue)
				{
					return DateTimeOffset.Now - StartTime;
				}

				return EndTime - StartTime;
			}
		}
	};

	abstract class ActionExecutor
	{
		public abstract string Name
		{
			get;
		}

		public abstract bool ExecuteActions(List<Action> ActionsToExecute, bool bLogDetailedActionStats);
	}

	class ActionGraph
	{
		/// <summary>
		/// Unique Id given to all actions added to this graph
		/// </summary>
		public int NextUniqueId;

		/// <summary>
		/// List of all the actions
		/// </summary>
		public List<Action> AllActions = new List<Action>();

		public ActionGraph()
		{
			XmlConfig.ApplyTo(this);
			NextUniqueId = 0;
		}

		public Action Add(ActionType Type)
		{
			int UniqueId = ++NextUniqueId;
			Action NewAction = new Action(Type, UniqueId);
			AllActions.Add(NewAction);
			return NewAction;
		}

		public void FinalizeActionGraph()
		{
			// Link producing actions to the items they produce.
			LinkActionsAndItems();

			// Detect cycles in the action graph.
			DetectActionGraphCycles();

			// Sort action list by "cost" in descending order to improve parallelism.
			SortActionList();
		}

		/// <summary>
		/// Builds a list of actions that need to be executed to produce the specified output items.
		/// </summary>
		public List<Action> GetActionsToExecute(BuildConfiguration BuildConfiguration, Action[] PrerequisiteActions, List<UEBuildTarget> Targets, Dictionary<UEBuildTarget, CPPHeaders> TargetToHeaders, out Dictionary<UEBuildTarget, List<FileItem>> TargetToOutdatedPrerequisitesMap)
		{
			DateTime CheckOutdatednessStartTime = DateTime.UtcNow;

			// Build a set of all actions needed for this target.
			Dictionary<Action, bool> IsActionOutdatedMap = new Dictionary<Action, bool>();
			foreach (Action Action in PrerequisiteActions)
			{
				IsActionOutdatedMap.Add(Action, true);
			}

			// For all targets, build a set of all actions that are outdated.
			Dictionary<Action, bool> OutdatedActionDictionary = new Dictionary<Action, bool>();
			List<ActionHistory> HistoryList = new List<ActionHistory>();
			HashSet<FileReference> OpenHistoryFiles = new HashSet<FileReference>();
			TargetToOutdatedPrerequisitesMap = new Dictionary<UEBuildTarget, List<FileItem>>();
			foreach (UEBuildTarget BuildTarget in Targets)	// @todo ubtmake: Optimization: Ideally we don't even need to know about targets for ubtmake -- everything would come from the files
			{
				FileReference HistoryFilename = ActionHistory.GeneratePathForTarget(BuildTarget);
				if (!OpenHistoryFiles.Contains(HistoryFilename))		// @todo ubtmake: Optimization: We should be able to move the command-line outdatedness and build product deletion over to the 'gather' phase, as the command-lines won't change between assembler runs
				{
					ActionHistory History = new ActionHistory(HistoryFilename.FullName);
					HistoryList.Add(History);
					OpenHistoryFiles.Add(HistoryFilename);
					GatherAllOutdatedActions(BuildConfiguration, BuildTarget, TargetToHeaders[BuildTarget], History, ref OutdatedActionDictionary, TargetToOutdatedPrerequisitesMap);
				}
			}

			// If we're only compiling a single file, we should always compile and should never link.
			if (!string.IsNullOrEmpty(BuildConfiguration.SingleFileToCompile))
			{
				// Never do anything but compile the target file
				AllActions.RemoveAll(x => x.ActionType != ActionType.Compile);

				// Check all of the leftover compilation actions for the one we want... that one is always outdated.
				FileItem SingleFileToCompile = FileItem.GetExistingItemByPath(BuildConfiguration.SingleFileToCompile);
				foreach (Action Action in AllActions)
				{
					bool bIsSingleFileAction = Action.PrerequisiteItems.Contains(SingleFileToCompile);
					OutdatedActionDictionary[Action] = bIsSingleFileAction;
				}

				// Don't save the history of a single file compilation.
				HistoryList.Clear();
			}

			// Delete produced items that are outdated.
			DeleteOutdatedProducedItems(OutdatedActionDictionary, BuildConfiguration.bShouldDeleteAllOutdatedProducedItems);

			// Save the action history.
			// This must happen after deleting outdated produced items to ensure that the action history on disk doesn't have
			// command-lines that don't match the produced items on disk.
			foreach (ActionHistory TargetHistory in HistoryList)
			{
				TargetHistory.Save();
			}

			// Create directories for the outdated produced items.
			CreateDirectoriesForProducedItems(OutdatedActionDictionary);

			// Build a list of actions that are both needed for this target and outdated.
			HashSet<Action> ActionsToExecute = AllActions.Where(Action => Action.CommandPath != null && IsActionOutdatedMap.ContainsKey(Action) && OutdatedActionDictionary[Action]).ToHashSet();

			// Delete PDB files for all produced items, since incremental updates are slower than full ones.
			foreach (Action ActionToExecute in ActionsToExecute)
			{
				foreach (FileItem ProducedItem in ActionToExecute.ProducedItems)
				{
					if(ProducedItem.bExists && !ActionToExecute.bUseIncrementalLinking && ProducedItem.Reference.HasExtension(".pdb"))
					{
						Log.TraceVerbose("Deleting outdated pdb: {0}", ProducedItem.AbsolutePath);
						ProducedItem.Delete();
					}
				}
			}

			// Remove link actions if asked to
			if (BuildConfiguration.bSkipLinkingWhenNothingToCompile)
			{
				// Get all items produced by a compile action
				HashSet<FileItem> ProducedItems = ActionsToExecute.Where(Action => Action.ActionType == ActionType.Compile).SelectMany(x => x.ProducedItems).ToHashSet();

				// Get all link actions which have no out-of-date prerequisites
				HashSet<Action> UnlinkedActions = ActionsToExecute.Where(Action => Action.ActionType == ActionType.Link && !ProducedItems.Overlaps(Action.PrerequisiteItems)).ToHashSet();

				// Don't regard an action as unlinked if there is an associated 'failed.hotreload' file.
				UnlinkedActions.RemoveWhere(Action => Action.ProducedItems.Any(Item => File.Exists(Path.Combine(Path.GetDirectoryName(Item.AbsolutePath), "failed.hotreload"))));

				HashSet<Action> UnlinkedActionsWithFailedHotreload = ActionsToExecute.Where(Action => Action.ActionType == ActionType.Link && !ProducedItems.Overlaps(Action.PrerequisiteItems)).ToHashSet();

				// Remove unlinked items
				ActionsToExecute.ExceptWith(UnlinkedActions);

				// Re-add unlinked items which produce things which are dependencies of other actions
				for (;;)
				{
					// Get all prerequisite items of a link action
					HashSet<Action> PrerequisiteLinkActions = ActionsToExecute.Where(Action => Action.ActionType == ActionType.Link).SelectMany(x => x.PrerequisiteItems).Select(Item => Item.ProducingAction).ToHashSet();

					// Find all unlinked actions that need readding
					HashSet<Action> UnlinkedActionsToReadd = UnlinkedActions.Where(Action => PrerequisiteLinkActions.Contains(Action)).ToHashSet();

					// Also re-add any DLL whose import library is being rebuilt. These may be separate actions, and the import library will reference the new DLL even if it isn't being compiled itself, so it must exist.
					HashSet<FileReference> ProducedItemsToReAdd = ActionsToExecute.SelectMany(x => x.ProducedItems).Select(x => x.Reference).Where(x => x.HasExtension(".lib")).Select(x => x.ChangeExtension(".suppressed.lib")).ToHashSet();
					UnlinkedActionsToReadd.UnionWith(UnlinkedActions.Where(x => x.ProducedItems.Any(y => ProducedItemsToReAdd.Contains(y.Reference))));

					// Bail if we didn't find anything
					if (UnlinkedActionsToReadd.Count == 0)
					{
						break;
					}

					ActionsToExecute.UnionWith(UnlinkedActionsToReadd);
					UnlinkedActions.ExceptWith(UnlinkedActionsToReadd);

					// Break early if there are no more unlinked actions to readd
					if (UnlinkedActions.Count == 0)
					{
						break;
					}
				}

				// Remove actions that are wholly dependent on unlinked actions
				ActionsToExecute = ActionsToExecute.Where(Action => Action.PrerequisiteItems.Count == 0 || !Action.PrerequisiteItems.Select(Item => Item.ProducingAction).ToHashSet().IsSubsetOf(UnlinkedActions)).ToHashSet();
			}

			if (UnrealBuildTool.bPrintPerformanceInfo)
			{
				double CheckOutdatednessTime = (DateTime.UtcNow - CheckOutdatednessStartTime).TotalSeconds;
				Log.TraceInformation("Checking outdatedness took " + CheckOutdatednessTime + "s");
			}

			return ActionsToExecute.ToList();
		}

		/// <summary>
		/// Executes a list of actions.
		/// </summary>
		public static bool ExecuteActions(BuildConfiguration BuildConfiguration, List<Action> ActionsToExecute, bool bIsRemoteCompile, out string ExecutorName, string TargetInfoForTelemetry, EHotReload HotReload)
		{
			bool Result = true;
			ExecutorName = "";
			if (ActionsToExecute.Count > 0)
			{
				DateTime StartTime = DateTime.UtcNow;

				ActionExecutor Executor;
				if(bIsRemoteCompile)
				{
					Executor = new RemoteExecutor();
				}
				else if(ActionsToExecute.Any(x => x.ActionHandler != null))
				{
					Executor = new LocalExecutor();
				}
				else if ((XGE.IsAvailable() && BuildConfiguration.bAllowXGE) || BuildConfiguration.bXGEExport)
				{
					Executor = new XGE();
				}
				else if(BuildConfiguration.bAllowDistcc)
				{
					Executor = new Distcc();
				}
				else if(SNDBS.IsAvailable() && BuildConfiguration.bAllowSNDBS)
				{
					Executor = new SNDBS();
				}
				else
				{
					Executor = new LocalExecutor();
				}

				ExecutorName = Executor.Name;
				Result = Executor.ExecuteActions(ActionsToExecute, BuildConfiguration.bLogDetailedActionStats);

				if(Telemetry.IsAvailable() && !BuildConfiguration.bXGEExport)
				{
					Telemetry.SendEvent("BuildTime",
						"ExecutorType", ExecutorName,
						"OS", Environment.OSVersion.ToString(),
						"MachineName", Environment.MachineName,
						"NumLogicalCores", Environment.ProcessorCount.ToString(),
						"Targets", TargetInfoForTelemetry,
						"NumActions", ActionsToExecute.Count.ToString(),
						"NumCompileActions", ActionsToExecute.Count(x => x.ActionType == ActionType.Compile).ToString(),
						"NumPchCompileActions", ActionsToExecute.Count(x => x.ActionType == ActionType.Compile && x.ProducedItems.Any(y => y.Reference.HasExtension(".pch") || y.Reference.HasExtension(".gch"))).ToString(),
						"NumLinkActions", ActionsToExecute.Count(x => x.ActionType == ActionType.Link).ToString(),
						"ElapsedTime", (DateTime.UtcNow - StartTime).TotalSeconds.ToString());
				}

				if (!BuildConfiguration.bXGEExport)
				{
					// Verify the link outputs were created (seems to happen with Win64 compiles)
					foreach (Action BuildAction in ActionsToExecute)
					{
						if (BuildAction.ActionType == ActionType.Link)
						{
							foreach (FileItem Item in BuildAction.ProducedItems)
							{
								bool bExists;
								if (Item.bIsRemoteFile)
								{
									DateTime UnusedTime;
									long UnusedLength;
									bExists = RPCUtilHelper.GetRemoteFileInfo(Item.AbsolutePath, out UnusedTime, out UnusedLength);
								}
								else
								{
									// allow output to be a directory
									bExists = File.Exists(Item.AbsolutePath) || Directory.Exists(Item.AbsolutePath);
								}

								if (HotReload != EHotReload.Disabled)
								{
									string FailedFilename = Path.Combine(Path.GetDirectoryName(Item.AbsolutePath), "failed.hotreload");
									if (!bExists)
									{
										// Create a failed.hotreload file here to indicate that we need to attempt another hotreload link
										// step in future, even though no source files have changed.
										// This is necessary because we also don't want to link a brand new instance of a module every time
										// a user hits the Compile button when nothing has changed.
										FileItem.CreateIntermediateTextFile(new FileReference(FailedFilename), "");
									}
									else
									{
										try
										{
											File.Delete(FailedFilename);
										}
										catch
										{
											// Ignore but log failed deletions
											Log.TraceVerbose("Failed to delete failed.hotreload file \"{0}\" - this may cause redundant hotreloads", FailedFilename);
										}
									}
								}

								if (!bExists)
								{
									throw new BuildException("UBT ERROR: Failed to produce item: " + Item.AbsolutePath);
								}
							}
						}
					}
				}
			}
			// Nothing to execute.
			else
			{
				ExecutorName = "NoActionsToExecute";
				Log.TraceInformation("Target is up to date");
			}

			return Result;
		}

		/// <summary>
		/// Links actions with their prerequisite and produced items into an action graph.
		/// </summary>
		void LinkActionsAndItems()
		{
			foreach (Action Action in AllActions)
			{
				foreach (FileItem ProducedItem in Action.ProducedItems)
				{
					ProducedItem.ProducingAction = Action;
					Action.RelativeCost += ProducedItem.RelativeCost;
				}
			}
		}
		static string SplitFilename(string Filename, out string PlatformSuffix, out string ConfigSuffix, out string ProducedItemExtension)
		{
			string WorkingString = Filename;
			ProducedItemExtension = Path.GetExtension(WorkingString);
			if (!WorkingString.EndsWith(ProducedItemExtension))
			{
				throw new BuildException("Bogus extension");
			}
			WorkingString = WorkingString.Substring(0, WorkingString.Length - ProducedItemExtension.Length);

			ConfigSuffix = "";
			foreach (UnrealTargetConfiguration CurConfig in Enum.GetValues(typeof(UnrealTargetConfiguration)))
			{
				if (CurConfig != UnrealTargetConfiguration.Unknown)
				{
					string Test = "-" + CurConfig;
					if (WorkingString.EndsWith(Test))
					{
						WorkingString = WorkingString.Substring(0, WorkingString.Length - Test.Length);
						ConfigSuffix = Test;
						break;
					}
				}
			}
			PlatformSuffix = "";
			foreach (object CurPlatform in Enum.GetValues(typeof(UnrealTargetPlatform)))
			{
				string Test = "-" + CurPlatform;
				if (WorkingString.EndsWith(Test))
				{
					WorkingString = WorkingString.Substring(0, WorkingString.Length - Test.Length);
					PlatformSuffix = Test;
					break;
				}
			}
			return WorkingString;
		}


		/// <summary>
		/// Finds and deletes stale hot reload DLLs.
		/// </summary>
		public void DeleteStaleHotReloadDLLs()
		{
			DateTime DeleteStartTime = DateTime.UtcNow;

			foreach (Action BuildAction in AllActions)
			{
				if (BuildAction.ActionType == ActionType.Link)
				{
					foreach (FileItem Item in BuildAction.ProducedItems)
					{
						if (Item.bNeedsHotReloadNumbersDLLCleanUp)
						{
							string PlatformSuffix, ConfigSuffix, ProducedItemExtension;
							string Base = SplitFilename(Item.AbsolutePath, out PlatformSuffix, out ConfigSuffix, out ProducedItemExtension);
							String WildCard = Base + "-*" + PlatformSuffix + ConfigSuffix + ProducedItemExtension;
							// Log.TraceInformation("Deleting old hot reload wildcard: \"{0}\".", WildCard);
							// Wildcard search and delete
							string DirectoryToLookIn = Path.GetDirectoryName(WildCard);
							string FileName = Path.GetFileName(WildCard);
							if (Directory.Exists(DirectoryToLookIn))
							{
								// Delete all files within the specified folder
								string[] FilesToDelete = Directory.GetFiles(DirectoryToLookIn, FileName, SearchOption.TopDirectoryOnly);
								foreach (string JunkFile in FilesToDelete)
								{

									string JunkPlatformSuffix, JunkConfigSuffix, JunkProducedItemExtension;
									SplitFilename(JunkFile, out JunkPlatformSuffix, out JunkConfigSuffix, out JunkProducedItemExtension);
									// now make sure that this file has the same config and platform
									if (JunkPlatformSuffix == PlatformSuffix && JunkConfigSuffix == ConfigSuffix)
									{
										try
										{
											Log.TraceInformation("Deleting old hot reload file: \"{0}\".", JunkFile);
											File.Delete(JunkFile);
										}
										catch (Exception Ex)
										{
											// Ignore all exceptions
											Log.TraceInformation("Unable to delete old hot reload file: \"{0}\". Error: {1}", JunkFile, Ex.Message);
										}

										// Delete the PDB file.
										string JunkPDBFile = JunkFile.Replace(ProducedItemExtension, ".pdb");
										if (System.IO.File.Exists(JunkPDBFile))
										{
											try
											{
												Log.TraceInformation("Deleting old hot reload file: \"{0}\".", JunkPDBFile);
												File.Delete(JunkPDBFile);
											}
											catch (Exception Ex)
											{
												// Ignore all exceptions
												Log.TraceInformation("Unable to delete old hot reload file: \"{0}\". Error: {1}", JunkPDBFile, Ex.Message);
											}
										}
									}
								}
							}
						}
					}
				}
			}

			if (UnrealBuildTool.bPrintPerformanceInfo)
			{
				double DeleteTime = (DateTime.UtcNow - DeleteStartTime).TotalSeconds;
				Log.TraceInformation("Deleting stale hot reload DLLs took " + DeleteTime + "s");
			}
		}

		/// <summary>
		/// Sorts the action list for improved parallelism with local execution.
		/// </summary>
		public void SortActionList()
		{
			// Mapping from action to a list of actions that directly or indirectly depend on it (up to a certain depth).
			Dictionary<Action, HashSet<Action>> ActionToDependentActionsMap = new Dictionary<Action, HashSet<Action>>();
			// Perform multiple passes over all actions to propagate dependencies.
			const int MaxDepth = 5;
			for (int Pass = 0; Pass < MaxDepth; Pass++)
			{
				foreach (Action DependendAction in AllActions)
				{
					foreach (FileItem PrerequisiteItem in DependendAction.PrerequisiteItems)
					{
						Action PrerequisiteAction = PrerequisiteItem.ProducingAction;
						if (PrerequisiteAction != null)
						{
							HashSet<Action> DependentActions = null;
							if (ActionToDependentActionsMap.ContainsKey(PrerequisiteAction))
							{
								DependentActions = ActionToDependentActionsMap[PrerequisiteAction];
							}
							else
							{
								DependentActions = new HashSet<Action>();
								ActionToDependentActionsMap[PrerequisiteAction] = DependentActions;
							}
							// Add dependent action...
							DependentActions.Add(DependendAction);
							// ... and all actions depending on it.
							if (ActionToDependentActionsMap.ContainsKey(DependendAction))
							{
								DependentActions.UnionWith(ActionToDependentActionsMap[DependendAction]);
							}
						}
					}
				}

			}
			// At this point we have a list of dependent actions for each action, up to MaxDepth layers deep.
			foreach (KeyValuePair<Action, HashSet<Action>> ActionMap in ActionToDependentActionsMap)
			{
				ActionMap.Key.NumTotalDependentActions = ActionMap.Value.Count;
			}
			// Sort actions by number of actions depending on them, descending. Secondary sort criteria is file size.
			AllActions.Sort(Action.Compare);
		}

		/// <summary>
		/// Checks for cycles in the action graph.
		/// </summary>
		void DetectActionGraphCycles()
		{
			// Starting with actions that only depend on non-produced items, iteratively expand a set of actions that are only dependent on
			// non-cyclical actions.
			Dictionary<Action, bool> ActionIsNonCyclical = new Dictionary<Action, bool>();
			Dictionary<Action, List<Action>> CyclicActions = new Dictionary<Action, List<Action>>();
			while (true)
			{
				bool bFoundNewNonCyclicalAction = false;

				foreach (Action Action in AllActions)
				{
					if (!ActionIsNonCyclical.ContainsKey(Action))
					{
						// Determine if the action depends on only actions that are already known to be non-cyclical.
						bool bActionOnlyDependsOnNonCyclicalActions = true;
						foreach (FileItem PrerequisiteItem in Action.PrerequisiteItems)
						{
							if (PrerequisiteItem.ProducingAction != null)
							{
								if (!ActionIsNonCyclical.ContainsKey(PrerequisiteItem.ProducingAction))
								{
									bActionOnlyDependsOnNonCyclicalActions = false;
									if (!CyclicActions.ContainsKey(Action))
									{
										CyclicActions.Add(Action, new List<Action>());
									}

									List<Action> CyclicPrereq = CyclicActions[Action];
									if (!CyclicPrereq.Contains(PrerequisiteItem.ProducingAction))
									{
										CyclicPrereq.Add(PrerequisiteItem.ProducingAction);
									}
								}
							}
						}

						// If the action only depends on known non-cyclical actions, then add it to the set of known non-cyclical actions.
						if (bActionOnlyDependsOnNonCyclicalActions)
						{
							ActionIsNonCyclical.Add(Action, true);
							bFoundNewNonCyclicalAction = true;
							if (CyclicActions.ContainsKey(Action))
							{
								CyclicActions.Remove(Action);
							}
						}
					}
				}

				// If this iteration has visited all actions without finding a new non-cyclical action, then all non-cyclical actions have
				// been found.
				if (!bFoundNewNonCyclicalAction)
				{
					break;
				}
			}

			// If there are any cyclical actions, throw an exception.
			if (ActionIsNonCyclical.Count < AllActions.Count)
			{
				// Describe the cyclical actions.
				string CycleDescription = "";
				foreach (Action Action in AllActions)
				{
					if (!ActionIsNonCyclical.ContainsKey(Action))
					{
						CycleDescription += string.Format("Action #{0}: {1}\n", Action.UniqueId, Action.CommandPath);
						CycleDescription += string.Format("\twith arguments: {0}\n", Action.CommandArguments);
						foreach (FileItem PrerequisiteItem in Action.PrerequisiteItems)
						{
							CycleDescription += string.Format("\tdepends on: {0}\n", PrerequisiteItem.AbsolutePath);
						}
						foreach (FileItem ProducedItem in Action.ProducedItems)
						{
							CycleDescription += string.Format("\tproduces:   {0}\n", ProducedItem.AbsolutePath);
						}
						CycleDescription += string.Format("\tDepends on cyclic actions:\n");
						if (CyclicActions.ContainsKey(Action))
						{
							foreach (Action CyclicPrerequisiteAction in CyclicActions[Action])
							{
								if (CyclicPrerequisiteAction.ProducedItems.Count == 1)
								{
									CycleDescription += string.Format("\t\t{0} (produces: {1})\n", CyclicPrerequisiteAction.UniqueId, CyclicPrerequisiteAction.ProducedItems[0].AbsolutePath);
								}
								else
								{
									CycleDescription += string.Format("\t\t{0}\n", CyclicPrerequisiteAction.UniqueId);
									foreach (FileItem CyclicProducedItem in CyclicPrerequisiteAction.ProducedItems)
									{
										CycleDescription += string.Format("\t\t\tproduces:   {0}\n", CyclicProducedItem.AbsolutePath);
									}
								}
							}
							CycleDescription += "\n";
						}
						else
						{
							CycleDescription += string.Format("\t\tNone?? Coding error!\n");
						}
						CycleDescription += "\n\n";
					}
				}

				throw new BuildException("Action graph contains cycle!\n\n{0}", CycleDescription);
			}
		}

		/// <summary>
		/// Determines the full set of actions that must be built to produce an item.
		/// </summary>
		/// <param name="OutputItem">- The item to be built.</param>
		/// <param name="PrerequisiteActions">- The actions that must be built and the root action are</param>
		public void GatherPrerequisiteActions(
			FileItem OutputItem,
			ref HashSet<Action> PrerequisiteActions
			)
		{
			if (OutputItem != null && OutputItem.ProducingAction != null)
			{
				if (!PrerequisiteActions.Contains(OutputItem.ProducingAction))
				{
					PrerequisiteActions.Add(OutputItem.ProducingAction);
					foreach (FileItem PrerequisiteItem in OutputItem.ProducingAction.PrerequisiteItems)
					{
						GatherPrerequisiteActions(PrerequisiteItem, ref PrerequisiteActions);
					}
				}
			}
		}

		/// <summary>
		/// Determines whether an action is outdated based on the modification times for its prerequisite
		/// and produced items.
		/// </summary>
		/// <param name="BuildConfiguration">Build configuration options</param>
		/// <param name="Target"></param>
		/// <param name="Headers"></param>
		/// <param name="RootAction">- The action being considered.</param>
		/// <param name="OutdatedActionDictionary">-</param>
		/// <param name="ActionHistory"></param>
		/// <param name="TargetToOutdatedPrerequisitesMap"></param>
		/// <returns>true if outdated</returns>
		public bool IsActionOutdated(BuildConfiguration BuildConfiguration, UEBuildTarget Target, CPPHeaders Headers, Action RootAction, Dictionary<Action, bool> OutdatedActionDictionary, ActionHistory ActionHistory, Dictionary<UEBuildTarget, List<FileItem>> TargetToOutdatedPrerequisitesMap)
		{
			// Only compute the outdated-ness for actions that don't aren't cached in the outdated action dictionary.
			bool bIsOutdated = false;
			if (!OutdatedActionDictionary.TryGetValue(RootAction, out bIsOutdated))
			{
				// Determine the last time the action was run based on the write times of its produced files.
				string LatestUpdatedProducedItemName = null;
				DateTimeOffset LastExecutionTime = DateTimeOffset.MaxValue;
				foreach (FileItem ProducedItem in RootAction.ProducedItems)
				{
					// Optionally skip the action history check, as this only works for local builds
					if (BuildConfiguration.bUseActionHistory)
					{
						// Check if the command-line of the action previously used to produce the item is outdated.
						string OldProducingCommandLine = "";
						string NewProducingCommandLine = RootAction.CommandPath + " " + RootAction.CommandArguments;
						if (!ActionHistory.GetProducingCommandLine(ProducedItem, out OldProducingCommandLine)
						|| !String.Equals(OldProducingCommandLine, NewProducingCommandLine, StringComparison.InvariantCultureIgnoreCase))
						{
							Log.TraceVerbose(
								"{0}: Produced item \"{1}\" was produced by outdated command-line.\nOld command-line: {2}\nNew command-line: {3}",
								RootAction.StatusDescription,
								Path.GetFileName(ProducedItem.AbsolutePath),
								OldProducingCommandLine,
								NewProducingCommandLine
								);

							bIsOutdated = true;

							// Update the command-line used to produce this item in the action history.
							ActionHistory.SetProducingCommandLine(ProducedItem, NewProducingCommandLine);
						}
					}

					// If the produced file doesn't exist or has zero size, consider it outdated.  The zero size check is to detect cases
					// where aborting an earlier compile produced invalid zero-sized obj files, but that may cause actions where that's
					// legitimate output to always be considered outdated.
					if (ProducedItem.bExists && (ProducedItem.bIsRemoteFile || ProducedItem.Length > 0 || ProducedItem.IsDirectory))
					{
						// VS 15.3+ does not touch lib files if they do not contain any modifications, so ignore any timestamps on them.
						String ProducedItemExtension = Path.GetExtension(ProducedItem.AbsolutePath).ToUpperInvariant();
						if (ProducedItemExtension == ".LIB" || ProducedItemExtension == ".EXP" || ProducedItemExtension == ".PDB")
						{
							continue;
						}

						// Use the oldest produced item's time as the last execution time.
						if (ProducedItem.LastWriteTime < LastExecutionTime)
						{
							LastExecutionTime = ProducedItem.LastWriteTime;
							LatestUpdatedProducedItemName = ProducedItem.AbsolutePath;
						}
					}
					else
					{
						// If any of the produced items doesn't exist, the action is outdated.
						Log.TraceVerbose(
							"{0}: Produced item \"{1}\" doesn't exist.",
							RootAction.StatusDescription,
							Path.GetFileName(ProducedItem.AbsolutePath)
							);
						bIsOutdated = true;
					}
				}

				Log.WriteLineIf(BuildConfiguration.bLogDetailedActionStats && !String.IsNullOrEmpty(LatestUpdatedProducedItemName),
					LogEventType.Verbose, "{0}: Oldest produced item is {1}", RootAction.StatusDescription, LatestUpdatedProducedItemName);

				bool bCheckIfIncludedFilesAreNewer = false;
				bool bPerformExhaustiveIncludeSearchAndUpdateCache = false;
				if (RootAction.ActionType == ActionType.Compile)
				{
					// Outdated targets don't need their headers scanned yet, because presumably they would already be out of dated based on already-cached
					// includes before getting this far.  However, if we find them to be outdated after processing includes, we'll do a deep scan later
					// on and cache all of the includes so that we have them for a quick outdatedness check the next run.
					if (!bIsOutdated &&
						BuildConfiguration.bUseUBTMakefiles &&
						UnrealBuildTool.IsAssemblingBuild)
					{
						bCheckIfIncludedFilesAreNewer = true;
					}

					// Were we asked to force an update of our cached includes BEFORE we try to build?  This may be needed if our cache can no longer
					// be trusted and we need to fill it with perfectly valid data (even if we're in assembler only mode)
					if (BuildConfiguration.bUseUBTMakefiles &&
						UnrealBuildTool.bNeedsFullCPPIncludeRescan)
					{
						// This will be slow!
						bPerformExhaustiveIncludeSearchAndUpdateCache = true;
					}
				}


				if (bCheckIfIncludedFilesAreNewer || bPerformExhaustiveIncludeSearchAndUpdateCache)
				{
					// Scan this file for included headers that may be out of date.  Note that it's OK if we break out early because we found
					// the action to be outdated.  For outdated actions, we kick off a separate include scan in a background thread later on to
					// catch all of the other includes and form an exhaustive set.
					foreach (FileItem PrerequisiteItem in RootAction.PrerequisiteItems)
					{
						// @todo ubtmake: Make sure we are catching RC files here too.  Anything that the toolchain would have tried it on.  Logic should match the CACHING stuff below
						if (PrerequisiteItem.CachedIncludePaths != null)
						{
							List<FileItem> IncludedFileList = Headers.FindAndCacheAllIncludedFiles(PrerequisiteItem, PrerequisiteItem.CachedIncludePaths, bOnlyCachedDependencies: !bPerformExhaustiveIncludeSearchAndUpdateCache);
							if (IncludedFileList != null)
							{
								foreach (FileItem IncludedFile in IncludedFileList)
								{
									if (IncludedFile.bExists)
									{
										// allow a 1 second slop for network copies
										TimeSpan TimeDifference = IncludedFile.LastWriteTime - LastExecutionTime;
										bool bPrerequisiteItemIsNewerThanLastExecution = TimeDifference.TotalSeconds > 1;
										if (bPrerequisiteItemIsNewerThanLastExecution)
										{
											Log.TraceVerbose(
												"{0}: Included file {1} is newer than the last execution of the action: {2} vs {3}",
												RootAction.StatusDescription,
												Path.GetFileName(IncludedFile.AbsolutePath),
												IncludedFile.LastWriteTime.LocalDateTime,
												LastExecutionTime.LocalDateTime
												);
											bIsOutdated = true;

											// Don't bother checking every single include if we've found one that is out of date
											break;
										}
									}
								}
							}
						}

						if (bIsOutdated)
						{
							break;
						}
					}
				}

				if (!bIsOutdated)
				{
					// Check if any of the prerequisite items are produced by outdated actions, or have changed more recently than
					// the oldest produced item.
					foreach (FileItem PrerequisiteItem in RootAction.PrerequisiteItems)
					{
						// Only check for outdated import libraries if we were configured to do so.  Often, a changed import library
						// won't affect a dependency unless a public header file was also changed, in which case we would be forced
						// to recompile anyway.  This just allows for faster iteration when working on a subsystem in a DLL, as we
						// won't have to wait for dependent targets to be relinked after each change.
						bool bIsImportLibraryFile = false;
						if (PrerequisiteItem.ProducingAction != null && PrerequisiteItem.ProducingAction.bProducesImportLibrary)
						{
							bIsImportLibraryFile = PrerequisiteItem.AbsolutePath.EndsWith(".LIB", StringComparison.InvariantCultureIgnoreCase);
						}
						if (!bIsImportLibraryFile || !BuildConfiguration.bIgnoreOutdatedImportLibraries)
						{
							// If the prerequisite is produced by an outdated action, then this action is outdated too.
							if (PrerequisiteItem.ProducingAction != null)
							{
								if (IsActionOutdated(BuildConfiguration, Target, Headers, PrerequisiteItem.ProducingAction, OutdatedActionDictionary, ActionHistory, TargetToOutdatedPrerequisitesMap))
								{
									Log.TraceVerbose(
										"{0}: Prerequisite {1} is produced by outdated action.",
										RootAction.StatusDescription,
										Path.GetFileName(PrerequisiteItem.AbsolutePath)
										);
									bIsOutdated = true;
								}
							}

							if (PrerequisiteItem.bExists)
							{
								// allow a 1 second slop for network copies
								TimeSpan TimeDifference = PrerequisiteItem.LastWriteTime - LastExecutionTime;
								bool bPrerequisiteItemIsNewerThanLastExecution = TimeDifference.TotalSeconds > 1;
								if (bPrerequisiteItemIsNewerThanLastExecution)
								{
									Log.TraceVerbose(
										"{0}: Prerequisite {1} is newer than the last execution of the action: {2} vs {3}",
										RootAction.StatusDescription,
										Path.GetFileName(PrerequisiteItem.AbsolutePath),
										PrerequisiteItem.LastWriteTime.LocalDateTime,
										LastExecutionTime.LocalDateTime
										);
									bIsOutdated = true;
								}
							}

							// GatherAllOutdatedActions will ensure all actions are checked for outdated-ness, so we don't need to recurse with
							// all this action's prerequisites once we've determined it's outdated.
							if (bIsOutdated)
							{
								break;
							}
						}
					}
				}

				// For compile actions, we have C++ files that are actually dependent on header files that could have been changed.  We only need to
				// know about the set of header files that are included for files that are already determined to be out of date (such as if the file
				// is missing or was modified.)  In the case that the file is out of date, we'll perform a deep scan to update our cached set of
				// includes for this file, so that we'll be able to determine whether it is out of date next time very quickly.
				if (BuildConfiguration.bUseUBTMakefiles)
				{
					DateTime DeepIncludeScanStartTime = DateTime.UtcNow;

					// @todo ubtmake: we may be scanning more files than we need to here -- indirectly outdated files are bIsOutdated=true by this point (for example basemost includes when deeper includes are dirty)
					if (bIsOutdated && RootAction.ActionType == ActionType.Compile)	// @todo ubtmake: Does this work with RC files?  See above too.
					{
						Log.TraceVerbose("Outdated action: {0}", RootAction.StatusDescription);
						foreach (FileItem PrerequisiteItem in RootAction.PrerequisiteItems)
						{
							if (PrerequisiteItem.CachedIncludePaths != null)
							{
								if (!IsCPPFile(PrerequisiteItem))
								{
									throw new BuildException("Was only expecting C++ files to have CachedCPPEnvironments!");
								}
								Log.TraceVerbose("  -> DEEP include scan: {0}", PrerequisiteItem.AbsolutePath);

								List<FileItem> OutdatedPrerequisites;
								if (!TargetToOutdatedPrerequisitesMap.TryGetValue(Target, out OutdatedPrerequisites))
								{
									OutdatedPrerequisites = new List<FileItem>();
									TargetToOutdatedPrerequisitesMap.Add(Target, OutdatedPrerequisites);
								}

								OutdatedPrerequisites.Add(PrerequisiteItem);
							}
							else if (IsCPPImplementationFile(PrerequisiteItem) || IsCPPResourceFile(PrerequisiteItem))
							{
								Log.TraceVerbose("  -> WARNING: No CachedCPPEnvironment: {0}", PrerequisiteItem.AbsolutePath);
							}
						}
					}

					if (UnrealBuildTool.bPrintPerformanceInfo)
					{
						double DeepIncludeScanTime = (DateTime.UtcNow - DeepIncludeScanStartTime).TotalSeconds;
						UnrealBuildTool.TotalDeepIncludeScanTime += DeepIncludeScanTime;
					}
				}

				// Cache the outdated-ness of this action.
				OutdatedActionDictionary.Add(RootAction, bIsOutdated);
			}

			return bIsOutdated;
		}


		/// <summary>
		/// Builds a dictionary containing the actions from AllActions that are outdated by calling
		/// IsActionOutdated.
		/// </summary>
		void GatherAllOutdatedActions(BuildConfiguration BuildConfiguration, UEBuildTarget Target, CPPHeaders Headers, ActionHistory ActionHistory, ref Dictionary<Action, bool> OutdatedActions, Dictionary<UEBuildTarget, List<FileItem>> TargetToOutdatedPrerequisitesMap)
		{
			DateTime CheckOutdatednessStartTime = DateTime.UtcNow;

			foreach (Action Action in AllActions)
			{
				IsActionOutdated(BuildConfiguration, Target, Headers, Action, OutdatedActions, ActionHistory, TargetToOutdatedPrerequisitesMap);
			}

			if (UnrealBuildTool.bPrintPerformanceInfo)
			{
				double CheckOutdatednessTime = (DateTime.UtcNow - CheckOutdatednessStartTime).TotalSeconds;
				Log.TraceInformation("Checking actions for " + Target.GetTargetName() + " took " + CheckOutdatednessTime + "s");
			}
		}

		/// <summary>
		/// Deletes all the items produced by actions in the provided outdated action dictionary.
		/// </summary>
		/// <param name="OutdatedActionDictionary">Dictionary of outdated actions</param>
		/// <param name="bShouldDeleteAllFiles"> Whether to delete all files associated with outdated items or just ones required</param>
		static void DeleteOutdatedProducedItems(Dictionary<Action, bool> OutdatedActionDictionary, bool bShouldDeleteAllFiles)
		{
			foreach (KeyValuePair<Action, bool> OutdatedActionInfo in OutdatedActionDictionary)
			{
				if (OutdatedActionInfo.Value)
				{
					Action OutdatedAction = OutdatedActionInfo.Key;
					foreach (FileItem ProducedItem in OutdatedActionInfo.Key.ProducedItems)
					{
						if (ProducedItem.bExists && (bShouldDeleteAllFiles || OutdatedAction.bShouldDeleteProducedItems))
						{
							Log.TraceVerbose("Deleting outdated item: {0}", ProducedItem.AbsolutePath);
							ProducedItem.Delete();
						}
					}
				}
			}
		}

		/// <summary>
		/// Creates directories for all the items produced by actions in the provided outdated action
		/// dictionary.
		/// </summary>
		static void CreateDirectoriesForProducedItems(Dictionary<Action, bool> OutdatedActionDictionary)
		{
			foreach (KeyValuePair<Action, bool> OutdatedActionInfo in OutdatedActionDictionary)
			{
				if (OutdatedActionInfo.Value)
				{
					foreach (FileItem ProducedItem in OutdatedActionInfo.Key.ProducedItems)
					{
						if (ProducedItem.bIsRemoteFile)
						{
							// we don't need to do this in the SSH mode, the action will have an output file, and it will use that to make the directory while executing the command
							if (RemoteToolChain.bUseRPCUtil)
							{
								try
								{
									RPCUtilHelper.MakeDirectory(Path.GetDirectoryName(ProducedItem.AbsolutePath).Replace("\\", "/"));
								}
								catch (System.Exception Ex)
								{
									throw new BuildException(Ex, "Error while creating remote directory for '{0}'.  (Exception: {1})", ProducedItem.AbsolutePath, Ex.Message);
								}
							}
						}
						else
						{
							string DirectoryPath = Path.GetDirectoryName(ProducedItem.AbsolutePath);
							if (!Directory.Exists(DirectoryPath))
							{
								Log.TraceVerbose("Creating directory for produced item: {0}", DirectoryPath);
								Directory.CreateDirectory(DirectoryPath);
							}
						}
					}
				}
			}
		}



		/// <summary>
		/// Checks if the specified file is a C++ source implementation file (e.g., .cpp)
		/// </summary>
		/// <param name="FileItem">The file to check</param>
		/// <returns>True if this is a C++ source file</returns>
		private static bool IsCPPImplementationFile(FileItem FileItem)
		{
			return (FileItem.AbsolutePath.EndsWith(".cpp", StringComparison.InvariantCultureIgnoreCase) ||
					FileItem.AbsolutePath.EndsWith(".c", StringComparison.InvariantCultureIgnoreCase) ||
					FileItem.AbsolutePath.EndsWith(".mm", StringComparison.InvariantCultureIgnoreCase));
		}


		/// <summary>
		/// Checks if the specified file is a C++ source header file (e.g., .h or .inl)
		/// </summary>
		/// <param name="FileItem">The file to check</param>
		/// <returns>True if this is a C++ source file</returns>
		private static bool IsCPPIncludeFile(FileItem FileItem)
		{
			return (FileItem.AbsolutePath.EndsWith(".h", StringComparison.InvariantCultureIgnoreCase) ||
					FileItem.AbsolutePath.EndsWith(".hpp", StringComparison.InvariantCultureIgnoreCase) ||
					FileItem.AbsolutePath.EndsWith(".inl", StringComparison.InvariantCultureIgnoreCase));
		}


		/// <summary>
		/// Checks if the specified file is a C++ resource file (e.g., .rc)
		/// </summary>
		/// <param name="FileItem">The file to check</param>
		/// <returns>True if this is a C++ source file</returns>
		private static bool IsCPPResourceFile(FileItem FileItem)
		{
			return (FileItem.AbsolutePath.EndsWith(".rc", StringComparison.InvariantCultureIgnoreCase));
		}


		/// <summary>
		/// Checks if the specified file is a C++ source file
		/// </summary>
		/// <param name="FileItem">The file to check</param>
		/// <returns>True if this is a C++ source file</returns>
		private static bool IsCPPFile(FileItem FileItem)
		{
			return IsCPPImplementationFile(FileItem) || IsCPPIncludeFile(FileItem) || IsCPPResourceFile(FileItem);
		}



		/// <summary>
		/// Types of action graph visualizations that we can emit
		/// </summary>
		public enum ActionGraphVisualizationType
		{
			OnlyActions,
			ActionsWithFiles,
			ActionsWithFilesAndHeaders,
			OnlyFilesAndHeaders,
			OnlyCPlusPlusFilesAndHeaders
		}



		/// <summary>
		/// Saves the action graph (and include dependency network) to a graph gile
		/// </summary>
		/// <param name="Headers">Cached header data for this target</param>
		/// <param name="Filename">File name to emit</param>
		/// <param name="Description">Description to be stored in graph metadata</param>
		/// <param name="VisualizationType">Type of graph to create</param>
		/// <param name="Actions">All actions</param>
		/// <param name="IncludeCompileActions">True if we should include compile actions.  If disabled, only the static link actions will be shown, which is useful to see module relationships</param>
		public static void SaveActionGraphVisualization(CPPHeaders Headers, string Filename, string Description, ActionGraphVisualizationType VisualizationType, List<Action> Actions, bool IncludeCompileActions = true)
		{
			// True if we should include individual files in the graph network, or false to include only the build actions
			bool IncludeFiles = VisualizationType != ActionGraphVisualizationType.OnlyActions;
			bool OnlyIncludeCPlusPlusFiles = VisualizationType == ActionGraphVisualizationType.OnlyCPlusPlusFilesAndHeaders;

			// True if want to show actions in the graph, otherwise we're only showing files
			bool IncludeActions = VisualizationType != ActionGraphVisualizationType.OnlyFilesAndHeaders && VisualizationType != ActionGraphVisualizationType.OnlyCPlusPlusFilesAndHeaders;

			// True if C++ header dependencies should be expanded into the graph, or false to only have .cpp files
			bool ExpandCPPHeaderDependencies = IncludeFiles && (VisualizationType == ActionGraphVisualizationType.ActionsWithFilesAndHeaders || VisualizationType == ActionGraphVisualizationType.OnlyFilesAndHeaders || VisualizationType == ActionGraphVisualizationType.OnlyCPlusPlusFilesAndHeaders);

			DateTime TimerStartTime = DateTime.UtcNow;

			List<GraphNode> GraphNodes = new List<GraphNode>();

			Dictionary<FileItem, GraphNode> FileToGraphNodeMap = new Dictionary<FileItem, GraphNode>();

			// Filter our list of actions
			List<Action> FilteredActions = new List<Action>();
			{
				for (int ActionIndex = 0; ActionIndex < Actions.Count; ++ActionIndex)
				{
					Action Action = Actions[ActionIndex];

					if (!IncludeActions || IncludeCompileActions || (Action.ActionType != ActionType.Compile))
					{
						FilteredActions.Add(Action);
					}
				}
			}


			HashSet<FileItem> FilesToCreateNodesFor = new HashSet<FileItem>();
			for (int ActionIndex = 0; ActionIndex < FilteredActions.Count; ++ActionIndex)
			{
				Action Action = FilteredActions[ActionIndex];

				if (IncludeActions)
				{
					GraphNode GraphNode = new GraphNode()
					{
						Id = GraphNodes.Count,

						// Don't bother including "Link" text if we're excluding compile actions
						Label = IncludeCompileActions ? (Action.ActionType.ToString() + " " + Action.StatusDescription) : Action.StatusDescription
					};

					switch (Action.ActionType)
					{
						case ActionType.BuildProject:
							GraphNode.Color = new GraphColor() { R = 0.3f, G = 1.0f, B = 1.0f, A = 1.0f };
							GraphNode.Size = 1.1f;
							break;

						case ActionType.Compile:
							GraphNode.Color = new GraphColor() { R = 0.3f, G = 1.0f, B = 0.3f, A = 1.0f };
							break;

						case ActionType.Link:
							GraphNode.Color = new GraphColor() { R = 0.3f, G = 0.3f, B = 1.0f, A = 1.0f };
							GraphNode.Size = 1.2f;
							break;
					}

					GraphNodes.Add(GraphNode);
				}

				if (IncludeFiles)
				{
					foreach (FileItem ProducedFileItem in Action.ProducedItems)
					{
						if (!OnlyIncludeCPlusPlusFiles || IsCPPFile(ProducedFileItem))
						{
							FilesToCreateNodesFor.Add(ProducedFileItem);
						}
					}

					foreach (FileItem PrerequisiteFileItem in Action.PrerequisiteItems)
					{
						if (!OnlyIncludeCPlusPlusFiles || IsCPPFile(PrerequisiteFileItem))
						{
							FilesToCreateNodesFor.Add(PrerequisiteFileItem);
						}
					}
				}
			}


			Dictionary<FileItem, List<FileItem>> OverriddenPrerequisites = new Dictionary<FileItem, List<FileItem>>();

			// Determine the average size of all of the C++ source files
			Int64 AverageCPPFileSize;
			{
				Int64 TotalFileSize = 0;
				int CPPFileCount = 0;
				foreach (FileItem FileItem in FilesToCreateNodesFor)
				{
					if (IsCPPFile(FileItem))
					{
						++CPPFileCount;
						TotalFileSize += new FileInfo(FileItem.AbsolutePath).Length;
					}
				}

				if (CPPFileCount > 0)
				{
					AverageCPPFileSize = TotalFileSize / CPPFileCount;
				}
				else
				{
					AverageCPPFileSize = 1;
				}
			}

			foreach (FileItem FileItem in FilesToCreateNodesFor)
			{
				GraphNode FileGraphNode = new GraphNode()
				{
					Id = GraphNodes.Count,
					Label = Path.GetFileName(FileItem.AbsolutePath)
				};

				if (FileItem.AbsolutePath.EndsWith(".h", StringComparison.InvariantCultureIgnoreCase) ||
					FileItem.AbsolutePath.EndsWith(".inl", StringComparison.InvariantCultureIgnoreCase))
				{
					// Header file
					FileGraphNode.Color = new GraphColor() { R = 0.9f, G = 0.2f, B = 0.9f, A = 1.0f };
				}
				else if (FileItem.AbsolutePath.EndsWith(".cpp", StringComparison.InvariantCultureIgnoreCase) ||
						 FileItem.AbsolutePath.EndsWith(".c", StringComparison.InvariantCultureIgnoreCase) ||
						 FileItem.AbsolutePath.EndsWith(".mm", StringComparison.InvariantCultureIgnoreCase))
				{
					// C++ file
					FileGraphNode.Color = new GraphColor() { R = 1.0f, G = 1.0f, B = 0.3f, A = 1.0f };
				}
				else
				{
					// Other file
					FileGraphNode.Color = new GraphColor() { R = 0.4f, G = 0.4f, B = 0.1f, A = 1.0f };
				}

				// Set the size of the file node based on the size of the file on disk
				bool bIsCPPFile = IsCPPFile(FileItem);
				if (bIsCPPFile)
				{
					float MinNodeSize = 0.25f;
					float MaxNodeSize = 2.0f;
					long FileSize = new FileInfo(FileItem.AbsolutePath).Length;
					float FileSizeScale = (float)((double)FileSize / (double)AverageCPPFileSize);

					float SourceFileSizeScaleFactor = 0.1f;		// How much to make nodes for files bigger or larger based on their difference from the average file's size
					FileGraphNode.Size = Math.Min(Math.Max(1.0f + SourceFileSizeScaleFactor * FileSizeScale, MinNodeSize), MaxNodeSize);
				}

				//@todo: Testing out attribute support.  Replace with an attribute that is actually useful!
				//if( FileItem.PrecompiledHeaderIncludeFilename != null )
				//{ 
				//FileGraphNode.Attributes[ "PCHFile" ] = Path.GetFileNameWithoutExtension( FileItem.PrecompiledHeaderIncludeFilename );
				//}

				FileToGraphNodeMap[FileItem] = FileGraphNode;
				GraphNodes.Add(FileGraphNode);

				if (ExpandCPPHeaderDependencies && bIsCPPFile)
				{
					List<DependencyInclude> DirectlyIncludedFilenames = Headers.GetDirectIncludeDependencies(FileItem, bOnlyCachedDependencies: false);

					// Resolve the included file name to an actual file.
					List<FileItem> DirectlyIncludedFiles =
						DirectlyIncludedFilenames
						.Where(DirectlyIncludedFilename => (DirectlyIncludedFilename.IncludeResolvedNameIfSuccessful != null))
						.Select(DirectlyIncludedFilename => DirectlyIncludedFilename.IncludeResolvedNameIfSuccessful)
						// Skip same include over and over (.inl files)
						.Distinct()
						.Select(FileItem.GetItemByFileReference)
						.ToList();

					OverriddenPrerequisites[FileItem] = DirectlyIncludedFiles;
				}
			}


			// Connect everything together
			List<GraphEdge> GraphEdges = new List<GraphEdge>();

			if (IncludeActions)
			{
				for (int ActionIndex = 0; ActionIndex < FilteredActions.Count; ++ActionIndex)
				{
					Action Action = FilteredActions[ActionIndex];
					GraphNode ActionGraphNode = GraphNodes[ActionIndex];

					List<FileItem> ActualPrerequisiteItems = Action.PrerequisiteItems;
					if (IncludeFiles && ExpandCPPHeaderDependencies && Action.ActionType == ActionType.Compile)
					{
						// The first prerequisite is always the .cpp file to compile
						FileItem CPPFile = Action.PrerequisiteItems[0];
						if (!IsCPPFile(CPPFile))
						{
							throw new BuildException("Was expecting a C++ file as the first prerequisite for a Compile action");
						}

						ActualPrerequisiteItems = new List<FileItem>();
						ActualPrerequisiteItems.Add(CPPFile);
					}


					foreach (FileItem PrerequisiteFileItem in ActualPrerequisiteItems)
					{
						if (IncludeFiles)
						{
							GraphNode PrerequisiteFileGraphNode;
							if (FileToGraphNodeMap.TryGetValue(PrerequisiteFileItem, out PrerequisiteFileGraphNode))
							{
								// Connect a file our action is dependent on, to our action itself
								GraphEdge GraphEdge = new GraphEdge()
								{
									Id = GraphEdges.Count,
									Source = PrerequisiteFileGraphNode,
									Target = ActionGraphNode,
								};

								GraphEdges.Add(GraphEdge);
							}
							else
							{
								// Not a file we were tracking
								// Console.WriteLine( "Unknown file: " + PrerequisiteFileItem.AbsolutePath );
							}
						}
						else if (PrerequisiteFileItem.ProducingAction != null)
						{
							// Not showing files, so connect the actions together
							int ProducingActionIndex = FilteredActions.IndexOf(PrerequisiteFileItem.ProducingAction);
							if (ProducingActionIndex != -1)
							{
								GraphNode SourceGraphNode = GraphNodes[ProducingActionIndex];

								GraphEdge GraphEdge = new GraphEdge()
								{
									Id = GraphEdges.Count,
									Source = SourceGraphNode,
									Target = ActionGraphNode,
								};

								GraphEdges.Add(GraphEdge);
							}
							else
							{
								// Our producer action was filtered out
							}
						}
					}

					foreach (FileItem ProducedFileItem in Action.ProducedItems)
					{
						if (IncludeFiles)
						{
							if (!OnlyIncludeCPlusPlusFiles || IsCPPFile(ProducedFileItem))
							{
								GraphNode ProducedFileGraphNode = FileToGraphNodeMap[ProducedFileItem];

								GraphEdge GraphEdge = new GraphEdge()
								{
									Id = GraphEdges.Count,
									Source = ActionGraphNode,
									Target = ProducedFileGraphNode,
								};

								GraphEdges.Add(GraphEdge);
							}
						}
					}
				}
			}

			if (IncludeFiles && ExpandCPPHeaderDependencies)
			{
				// Fill in overridden prerequisites
				foreach (KeyValuePair<FileItem, List<FileItem>> FileAndPrerequisites in OverriddenPrerequisites)
				{
					FileItem FileItem = FileAndPrerequisites.Key;
					List<FileItem> FilePrerequisites = FileAndPrerequisites.Value;

					GraphNode FileGraphNode = FileToGraphNodeMap[FileItem];
					foreach (FileItem PrerequisiteFileItem in FilePrerequisites)
					{
						GraphNode PrerequisiteFileGraphNode;
						if (FileToGraphNodeMap.TryGetValue(PrerequisiteFileItem, out PrerequisiteFileGraphNode))
						{
							GraphEdge GraphEdge = new GraphEdge()
							{
								Id = GraphEdges.Count,
								Source = PrerequisiteFileGraphNode,
								Target = FileGraphNode,
							};

							GraphEdges.Add(GraphEdge);
						}
						else
						{
							// Some other header that we don't track directly
							//Console.WriteLine( "File not known: " + PrerequisiteFileItem.AbsolutePath );
						}
					}
				}
			}

			GraphVisualization.WriteGraphFile(Filename, Description, GraphNodes, GraphEdges);

			if (UnrealBuildTool.bPrintPerformanceInfo)
			{
				TimeSpan TimerDuration = DateTime.UtcNow - TimerStartTime;
				Log.TraceInformation("Generating and saving ActionGraph took " + TimerDuration.TotalSeconds + "s");
			}
		}
	};
}
