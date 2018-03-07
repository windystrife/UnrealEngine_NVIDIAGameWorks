// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml;
using UnrealBuildTool;

namespace AutomationScripts.Automation
{
	[Help("List labels valid for this branch.")]
	[Help("type", "Type of labels to list. Could be one of: all, promotable and promoted. Default: all.")]
	[Help("ticks", "Print ticks along with the label name.")]
	[Help("game=GameName", "Name of the game to filter labels. If not set then assuming shared promotable.")]
	[RequireP4]
	[DoesNotNeedP4CL]
	class UnrealSyncList : BuildCommand
	{
		enum QueryType
		{
			All,
			Promotable,
			Promoted
		}

		public override void ExecuteBuild()
		{
			var GameName = ParseParamValue("game");
			var QueryType = GetTypeParam();
			var BranchPath = CommandUtils.GetDirectoryName(P4Env.Branch);
			var Ticks = ParseParam("ticks");

			var BranchAndGameName = string.IsNullOrWhiteSpace(GameName)
				? string.Format("branch {0} shared-promotable", BranchPath)
				: string.Format("branch {0} and game {1}", BranchPath, GameName);

			if (string.IsNullOrWhiteSpace(BranchPath))
			{
				throw new AutomationException("The branch path is not set. Something went wrong.");
			}

			if (QueryType == QueryType.Promoted)
			{
				Log("Promoted labels for {0}.", BranchAndGameName);

				Print(GetPromotedLabels(BranchPath, GameName), Ticks);
			}
			else if(QueryType == QueryType.Promotable)
			{
				Log("Promotable labels for {0}.", BranchAndGameName);

				Print(GetPromotableLabels(BranchPath, GameName), Ticks);
			}
			else
			{
				Log("All labels for {0}.", BranchPath);

				Print(GetBranchLabels(BranchPath), Ticks);
			}
		}

		/// <summary>
		/// Gets promoted label list for given branch.
		/// </summary>
		/// <param name="BranchPath">A branch path.</param>
		/// <param name="GameName">The game name for which provide the label. If null or empty then provides shared-promotable label.</param>
		/// <returns>List of promoted labels for given branch path.</returns>
		public static P4Label[] GetPromotedLabels(string BranchPath, string GameName)
		{
			return P4.GetLabels(BranchPath + "/Promoted" + (GameName != null ? ("-" + GameName) : "") + "-CL-*");
		}

		/// <summary>
		/// Gets promoted label list for given branch.
		/// </summary>
		/// <param name="BranchPath">A branch path.</param>
		/// <param name="GameName">The game name for which provide the label. If null or empty then provides shared-promotable label.</param>
		/// <returns>List of promoted labels for given branch path.</returns>
		public static P4Label[] GetPromotableLabels(string BranchPath, string GameName)
		{
			var Combined = P4.GetLabels(BranchPath + "/Promot*" + (GameName != null ? ("-" + GameName) : "") + "-CL-*");

			IEnumerable<P4Label> CombinedOrdered = Combined.OrderByDescending((Label) => Label.Date);

			var Output = new List<P4Label>();

			foreach(var PossiblePromotable in CombinedOrdered)
			{
				if(PossiblePromotable.Name.StartsWith(BranchPath + "/Promoted"))
				{
					break;
				}

				if (PossiblePromotable.Name.StartsWith(BranchPath + "/Promotable"))
				{
					Output.Add(PossiblePromotable);
				}

				// else skip
			}

			return Output.ToArray();
		}

		/// <summary>
		/// Prints the labels.
		/// </summary>
		/// <param name="Labels">Labels to print.</param>
		/// <param name="Ticks">Print ticks along with label name?</param>
		public void Print(P4Label[] Labels, bool Ticks)
		{
			foreach(var Label in Labels.OrderByDescending((Label) => Label.Date))
			{
				Log(Label.Name + (Ticks ? (" " + Label.Date.Ticks.ToString()) : ""));
			}
		}

		/// <summary>
		/// Gets labels list for given branch.
		/// </summary>
		/// <param name="BranchPath">A branch path.</param>
		/// <returns>List of labels for given branch path.</returns>
		public P4Label[] GetBranchLabels(string BranchPath)
		{
			return P4.GetLabels(BranchPath + "/*");
		}

		/// <summary>
		/// Parses type param.
		/// </summary>
		/// <returns>Enum value of the query type param.</returns>
		private QueryType GetTypeParam()
		{
			var TypeString = ParseParamValue("type");

			if(string.IsNullOrWhiteSpace(TypeString))
			{
				return QueryType.All;
			}

			switch (TypeString.ToLower())
			{
				case "all":
					return QueryType.All;
				case "promotable":
					return QueryType.Promotable;
				case "promoted":
					return QueryType.Promoted;
				default:
					throw new AutomationException("Unsupported query type. Allowed are: all, promotable and promoted.");
			}
		}
	}

	[Help("Syncs promotable build. Use either -game or -label parameter.")]
	[Help("artist", "Artist sync i.e. sync content to head and all the rest to promoted label.")]
	[Help("preview", "This option makes that syncs are done in the preview mode (i.e. p4 sync -n).")]
	[Help("game=GameName", "Name of the game to sync. If not set then shared promotable will be synced.")]
	[Help("label=LabelName", "Promotable label name to sync to.")]
	[RequireP4]
	[DoesNotNeedP4CL]
	class UnrealSync : BuildCommand
	{
		public override void ExecuteBuild()
		{
			var Preview = ParseParam("preview");
			var ArtistSync = ParseParam("artist");
			var BranchPath = CommandUtils.GetDirectoryName(P4Env.Branch);
			var LabelParam = ParseParamValue("label");
			var GameName = ParseParamValue("game");

			if(GameName == null)
			{
				GameName = "";
			}

			string ProgramSyncLabelName = null;

			if (!string.IsNullOrWhiteSpace(LabelParam))
			{
				if (!LabelParam.StartsWith(BranchPath) || !P4.ValidateLabelContent(LabelParam))
				{
					throw new AutomationException("Label {0} either doesn't exist or is not valid for the current branch path {1}.", LabelParam, BranchPath);
				}

				ProgramSyncLabelName = LabelParam;
			}
			else
			{
				ProgramSyncLabelName = GetLatestPromotedLabel(BranchPath, GameName, true);
			}

			if (ProgramSyncLabelName == null)
			{
				throw new AutomationException("Label for {0} was not found.",
					string.IsNullOrWhiteSpace(GameName)
						? string.Format("branch {0} shared-promotable", BranchPath)
						: string.Format("branch {0} and game {1}", BranchPath, GameName));
			}

			SyncToLabel(BranchPath, ProgramSyncLabelName, ArtistSync, Preview);
		}

		/// <summary>
		/// Picks latest label from the provided group and returns its name.
		/// </summary>
		/// <param name="Labels">Labels to chose from.</param>
		/// <param name="bVerifyContent">If the method should skip labels that has no tagged files in it.</param>
		/// <returns>The name of the label. If none was valid returns null.</returns>
		public static string PickLatest(P4Label[] Labels, bool bVerifyContent)
		{
			if (Labels.Length == 0)
			{
				return null;
			}

			var OrderedLabels = Labels.OrderByDescending((Label) => Label.Date);

			if (bVerifyContent)
			{
				foreach (var Label in OrderedLabels)
				{
					if (P4.ValidateLabelContent(Label.Name))
					{
						return Label.Name;
					}
				}

				// Haven't found valid label.
				return null;
			}

			return OrderedLabels.First().Name;
		}

		/// <summary>
		/// Get latest promoted label given branch and game name.
		/// </summary>
		/// <param name="BranchPath">The branch path of the label.</param>
		/// <param name="GameName">The game name for which provide the label. If null or empty then provides shared-promotable label.</param>
		/// <param name="bVerifyContent">Verify if label tags at least one file.</param>
		/// <returns>Label name if it exists, null otherwise.</returns>
		public static string GetLatestPromotedLabel(string BranchPath, string GameName, bool bVerifyContent)
		{
			if (string.IsNullOrWhiteSpace(GameName))
			{
				GameName = null;
			}

			return PickLatest(UnrealSyncList.GetPromotedLabels(BranchPath, GameName), bVerifyContent);
		}

		/// <summary>
		/// Syncs to given label.
		/// </summary>
		/// <param name="BranchPath">Current branch path.</param>
		/// <param name="LabelName">Label name to sync.</param>
		/// <param name="bArtistSync">Perform artist sync? (binaries to label, content to latest)</param>
		/// <param name="bPreview">Perform preview sync? (p4 sync -n)</param>
		private void SyncToLabel(string BranchPath, string LabelName, bool bArtistSync = true, bool bPreview = false)
		{
			var ProgramRevisionSpec = "@" + LabelName;
			List<string> SyncSteps;

			if (bArtistSync)
			{
				// Get latest CL number to sync cause @head can change during
				// different syncs and it could create integrity problems in
				// workspace.
				var ContentRevisionSpec = "@" + P4.GetLatestCLNumber().ToString();

				var GameName = ParseGameNameFromLabel(LabelName);

				var ArtistSyncRulesPath = string.Format("{0}/{1}/Build/ArtistSyncRules.xml",
					BranchPath, string.IsNullOrWhiteSpace(GameName) ? "Samples" : GameName);

				var SyncRules = string.Join("\n", P4.P4Print(ArtistSyncRulesPath + "#head"));

				if (string.IsNullOrWhiteSpace(SyncRules))
				{
					throw new AutomationException("The path {0} is not valid or file is empty.", ArtistSyncRulesPath);
				}

				SyncSteps = GenerateSyncSteps(SyncRules, ContentRevisionSpec, ProgramRevisionSpec);
			}
			else
			{
				SyncSteps = new List<string>();
				SyncSteps.Add("/..." + ProgramRevisionSpec); // all files to label
			}

			foreach (var SyncStep in SyncSteps)
			{
				P4.Sync((bPreview ? "-n " : "") + BranchPath + SyncStep);
			}
		}

		/// <summary>
		/// Generates sync steps based on the sync rules xml content.
		/// </summary>
		/// <param name="SyncRules">ArtistSyncRules.xml content.</param>
		/// <param name="ContentRevisionSpec">Revision spec to which sync the content. Different for artist sync.</param>
		/// <param name="ProgramRevisionSpec">Revision spec to which sync everything except content.</param>
		/// <returns>An array of sync steps to perform.</returns>
		private List<string> GenerateSyncSteps(string SyncRules, string ContentRevisionSpec, string ProgramRevisionSpec)
		{
			var SyncRulesDocument = new XmlDocument();
			SyncRulesDocument.LoadXml(SyncRules);

			var RuleNodes = SyncRulesDocument.DocumentElement.SelectNodes("/ArtistSyncRules/Rules/string");

			var OutputList = new List<string>();

			foreach (XmlNode Node in RuleNodes)
			{
				var SyncStep = Node.InnerText.Replace("%LABEL_TO_SYNC_TO%", ProgramRevisionSpec);

				// If there was no label in sync step.
				// TODO: This is hack for messy ArtistSyncRules.xml format.
				// Needs to be changed soon.
				if (!SyncStep.Contains("@"))
				{
					SyncStep += ContentRevisionSpec;
				}

				OutputList.Add(SyncStep);
			}

			return OutputList;
		}

		/* Pattern used for parsing game name from label name. */
		private static readonly Regex GameNameFromLabelParsingPattern = new Regex(@"^.+/Promot(ed|able)-((?<game>\w+)-)?CL.+$", RegexOptions.Compiled);

		/// <summary>
		/// Tries to parse game name from label name.
		/// </summary>
		/// <param name="LabelName">The label name to parse from.</param>
		/// <returns>Game name if found. Null otherwise.</returns>
		private string ParseGameNameFromLabel(string LabelName)
		{
			return GameNameFromLabelParsingPattern.Match(LabelName).Groups["game"].Value;
		}

	}
}
