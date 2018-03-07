// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.Linq;
using System.IO;
using System.ComponentModel;
using System.Diagnostics;
using System.Text.RegularExpressions;

namespace AutomationTool
{

	public partial class P4Connection
	{
		/// <summary>
		/// Information about file revision.
		/// </summary>
		public class RevisionInfo
		{
			public int Revision;
			public int Changelist;
			public string User;
			public P4Action Action;
			public DateTime Date;

			protected RevisionInfo()
			{
			}
			public override string ToString()
			{
				return String.Format("#{0} {1} change {2} by {3} on {4}", Revision, Action, Changelist, User, Date);
			}
			/// <summary>
			/// Create a new RevisionInfo instance from Filelog command output
			/// </summary>
			/// <param name="Line">Single line from Filelog command output</param>
			/// <returns></returns>
			public static RevisionInfo FromFilelogOutput(string Line)
			{
				var Info = new RevisionInfo();
				var ChangelistString = GetValueFromFileLog(Line, " change ");
				Info.Revision = Int32.Parse(GetValueFromFileLog(Line, "... #"));
				Info.Changelist = Int32.Parse(ChangelistString);
				Info.User = GetValueFromFileLog(Line, " by ");
				Info.Action = ParseAction(GetValueFromFileLog(Line, String.Format("{0} ", ChangelistString)));
				Info.Date = DateTime.Parse(GetValueFromFileLog(Line, " on "));
				return Info;
			}
			/// <summary>
			/// Given a prefix, gets a value following the prefix from filelog output.
			/// </summary>
			/// <param name="LogLine">Filelog output line</param>
			/// <param name="Key">Prefix</param>
			/// <returns>Value</returns>
			private static string GetValueFromFileLog(string LogLine, string Key)
			{
				int ValueStartIndex = LogLine.IndexOf(Key) + Key.Length;
				int ValueEndIndex = LogLine.IndexOf(" ", ValueStartIndex + 1);
				return LogLine.Substring(ValueStartIndex, ValueEndIndex - ValueStartIndex);
			}
		}

		/// <summary>
		/// Class representing a single line in Blame method result.
		/// </summary>
		public class BlameLineInfo
		{
			public List<BlameLineInfo> PreviousRevisions;
			public RevisionInfo Revision;
			public string Line;
			public BlameLineInfo(string InLine, RevisionInfo InRevision)
			{
				PreviousRevisions = new List<BlameLineInfo>();
				Revision = InRevision;
				Line = InLine;
			}
			public override string ToString()
			{
				return Line;
			}
			public RevisionInfo FirstRevision
			{
				get { return CommandUtils.IsNullOrEmpty(PreviousRevisions) ? Revision : PreviousRevisions[0].Revision; }
			}
		}

		/// <summary>
		/// Diff change type
		/// </summary>
		public enum DiffChangeType
		{
			Add = 0,
			Change = 1,
			Delete
		}

		/// <summary>
		/// Class representing a single change in Diff result.
		/// </summary>
		public class DiffChange
		{
			public int SourceStart;
			public int SourceEnd;
			public int DestStart;
			public int DestEnd;
			public DiffChangeType Type;
			public List<string> OldContents = new List<string>();
			public List<string> NewContents = new List<string>();

			public DiffChange(string[] Diff, int DiffIndex)
			{
				// Determine what kind of change this is (add/change/delete)
				var Change = Diff[DiffIndex];
				var ChangeTypes = new string[] { "a", "c", "d" };
				var ChangeTypeIndex = -1;
				for (int ChangeIndex = 0; ChangeIndex < ChangeTypes.Length; ++ChangeIndex)
				{
					ChangeTypeIndex = Change.IndexOf(ChangeTypes[ChangeIndex]);
					if (ChangeTypeIndex != -1)
					{
						Type = (DiffChangeType)ChangeIndex;
						break;
					}
				}
				// Get range of changes
				var SourceRange = Change.Substring(0, ChangeTypeIndex);
				ParseRange(SourceRange, out SourceStart, out SourceEnd);
				var DestRange = Change.Substring(ChangeTypeIndex + 1);
				ParseRange(DestRange, out DestStart, out DestEnd);	
				// Get the actual change contents
				for (++DiffIndex; DiffIndex < Diff.Length; ++DiffIndex)
				{
					var ContentsLine = Diff[DiffIndex];
					if (ContentsLine.StartsWith("<"))
					{
						OldContents.Add(ContentsLine.Substring(2));
					}
					else if (ContentsLine.StartsWith(">"))
					{
						NewContents.Add(ContentsLine.Substring(2));
					}
					else if (!ContentsLine.StartsWith("---"))
					{
						break;
					}
				}
			}

			/// <summary>
			/// Parses a text to get a range of changes.
			/// </summary>
			/// <param name="RangeText"></param>
			/// <param name="Start"></param>
			/// <param name="End"></param>
			private void ParseRange(string RangeText, out int Start, out int End)
			{
				var Range = RangeText.Split(',');
				Start = Int32.Parse(Range[0]) - 1;
				if (Range.Length > 1)
				{
					End = Int32.Parse(Range[1]) - 1;
				}
				else
				{
					End = Start;
				}
			}

			public override string ToString()
			{
				return String.Format("{0}: {1},{2}->{3},{4}", Type, SourceStart, SourceEnd, DestStart, DestEnd);
			}

			/// <summary>
			/// Number of lines that have changed in the original file
			/// </summary>
			public int SourceCount
			{
				get { return SourceEnd - SourceStart + 1; }
			}
			/// <summary>
			/// Number of lines that have changed in the resulting file
			/// </summary>
			public int DestCount
			{
				get { return DestEnd - DestStart + 1; }
			}
		}

		/// <summary>
		/// Goes through all changes from Diff result and applies them to the blame file.
		/// </summary>
		/// <param name="Source"></param>
		/// <param name="Diff"></param>
		/// <param name="RevisionInfo"></param>
		private static void ParseDiff(List<BlameLineInfo> Source, DiffChange[] Diff, RevisionInfo RevisionInfo)
		{
			int DestOffset = 0;
			foreach (var Change in Diff)
			{
				DestOffset = ApplyDiff(RevisionInfo, Source, Change, DestOffset);
			}
		}

		/// <summary>
		/// Applies a single Diff change to the Blame file.
		/// </summary>
		/// <param name="RevisionInfo"></param>
		/// <param name="Result"></param>
		/// <param name="ChangeInfo"></param>
		/// <param name="DestOffset"></param>
		/// <returns></returns>
		private static int ApplyDiff(RevisionInfo RevisionInfo, List<BlameLineInfo> Result, DiffChange ChangeInfo, int DestOffset)
		{
			switch (ChangeInfo.Type)
			{
			case DiffChangeType.Add:
				DestOffset = ApplyDiffAdd(RevisionInfo, Result, ChangeInfo, DestOffset);
				break;
			case DiffChangeType.Delete:
				DestOffset = ApplyDiffDelete(Result, ChangeInfo, DestOffset);
				break;
			case DiffChangeType.Change:
				DestOffset = ApplyDiffChange(RevisionInfo, Result, ChangeInfo, DestOffset);
				break;
			}
			return DestOffset;
		}

		/// <summary>
		/// Applies 'change' to the blame file.
		/// </summary>
		private static int ApplyDiffChange(RevisionInfo RevisionInfo, List<BlameLineInfo> Result, DiffChange ChangeInfo, int DestOffset)
		{
			// Remember the first revision for lines that are about to change
			var PreviousRevisions = new BlameLineInfo[ChangeInfo.SourceCount];
			for (int PrevIndex = 0; PrevIndex < ChangeInfo.SourceCount; ++PrevIndex)
			{
				PreviousRevisions[PrevIndex] = Result[PrevIndex + ChangeInfo.SourceStart + DestOffset];
			}

			// Apply Change as Delete+Add operation
			DestOffset = ApplyDiffDelete(Result, ChangeInfo, DestOffset);
			DestOffset = ApplyDiffAdd(RevisionInfo, Result, ChangeInfo, DestOffset);

			// Keep previous revisions
			for (int PrevIndex = 0; PrevIndex < PreviousRevisions.Length && PrevIndex < ChangeInfo.NewContents.Count; ++PrevIndex)
			{
				Result[PrevIndex + ChangeInfo.DestStart].PreviousRevisions.Add(PreviousRevisions[PrevIndex]);
			}
			return DestOffset;
		}

		/// <summary>
		/// Applies 'delete' to the blame file
		/// </summary>
		private static int ApplyDiffDelete(List<BlameLineInfo> Result, DiffChange ChangeInfo, int DestOffset)
		{
			Result.RemoveRange(ChangeInfo.SourceStart + DestOffset, ChangeInfo.SourceCount);
			DestOffset -= ChangeInfo.SourceCount;
			return DestOffset;
		}

		/// <summary>
		/// Applies 'add' to the diff file.
		/// </summary>
		private static int ApplyDiffAdd(RevisionInfo RevisionInfo, List<BlameLineInfo> Result, DiffChange ChangeInfo, int DestOffset)
		{
			int DestInsert = ChangeInfo.DestStart;
			for (int ContentIndex = 0; ContentIndex < ChangeInfo.NewContents.Count; ++ContentIndex, ++DestInsert, ++DestOffset)
			{
				var AddBlame = new BlameLineInfo(ChangeInfo.NewContents[ContentIndex], RevisionInfo);
				Result.Insert(DestInsert, AddBlame);
			}
			return DestOffset;
		}

		/// <summary>
		/// Converts Print command output to a blame file.
		/// </summary>
		/// <param name="Output">Print command output</param>
		/// <param name="RevisionInfo">Revision of the printed file.</param>
		/// <returns>List of lines in the blame file.</returns>
		private static List<BlameLineInfo> PrintToBlame(string[] Lines, RevisionInfo RevisionInfo)
		{
			var InitialBlame = new List<BlameLineInfo>();
			for (int LineIndex = 0; LineIndex < Lines.Length; ++LineIndex)
			{
				var LineInfo = new BlameLineInfo(Lines[LineIndex], RevisionInfo);
				InitialBlame.Add(LineInfo);
			}
			return InitialBlame;
		}

		/// <summary>
		/// Returns a list of revisions for the given file. Does not include rename/move changes.
		/// </summary>
		/// <param name="DepotFilename"></param>
		/// <returns></returns>
		public RevisionInfo[] Filelog(string DepotFilename, bool bQuiet = false)
		{
			if (!bQuiet)
			{
				CommandUtils.Log("Filelog({0})", DepotFilename);
			}
			var Result = new List<RevisionInfo>();
			var FilelogResult = P4("filelog " + DepotFilename, AllowSpew: false);
			var LogLines = FilelogResult.Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.None);
			foreach (var Line in LogLines)
			{
				if (Line.StartsWith("... #"))
				{
					var RevInfo = RevisionInfo.FromFilelogOutput(Line);
					Result.Add(RevInfo);
					if (RevInfo.Revision == 1)
					{
						// There may be other revisions but they are move/rename changes.
						break;
					}
				}
			}
			// Sort ascending.
			Result.Sort((A, B) => { return A.Revision - B.Revision; });
			return Result.ToArray();
		}

		/// <summary>
		/// Prints the contents of a file to an array of lines.
		/// </summary>
		/// <param name="CommandLine"></param>
		/// <param name="bQuiet"></param>
		/// <returns></returns>
		public string[] P4Print(string CommandLine, bool bQuiet = false)
		{
			if (!bQuiet)
			{
				CommandUtils.Log("P4Print({0})", CommandLine);
			}
			var PrintResult = P4("print " + CommandLine, AllowSpew: false);
			var OutputLines = new List<string>(PrintResult.Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.None));
			// The first line is just the filename
			OutputLines.RemoveAt(0);
			return OutputLines.ToArray();
		}

		/// <summary>
		/// Prints the contents of a file to an array of lines.
		/// </summary>
		/// <param name="CommandLine"></param>
		/// <param name="bQuiet"></param>
		/// <returns></returns>
		public string[] Annotate(string CommandLine, bool bQuiet = false)
		{
			if (!bQuiet)
			{
				CommandUtils.Log("Annotate({0})", CommandLine);
			}
			var PrintResult = P4("annotate " + CommandLine, AllowSpew: false);
			var OutputLines = new List<string>(PrintResult.Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.None));
			// The first line is just the filename
			OutputLines.RemoveAt(0);
			return OutputLines.ToArray();
		}

		/// <summary>
		/// Performs a diff on a file at two revisions.
		/// </summary>
		/// <param name="DepotFilename"></param>
		/// <param name="RevA"></param>
		/// <param name="RevB"></param>
		/// <returns>List of changes between revisions.</returns>
		public DiffChange[] Diff2(string DepotFilename, int RevA, int RevB, bool bQuiet = false)
		{
			if (!bQuiet)
			{
				CommandUtils.Log("Diff2({0}, {1}, {2})", DepotFilename, RevA, RevB);
			}
			var Changes = new List<DiffChange>();
			var Diff2Result = P4(String.Format("diff2 {0}#{1} {0}#{2}", DepotFilename, RevA, RevB), AllowSpew: false);
			var Diff2Output = Diff2Result.Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.None);
			// Skip the first line - it contains only the diff description
			for (int DiffLineIndex = 1; DiffLineIndex < Diff2Output.Length; ++DiffLineIndex)
			{
				var DiffLine = Diff2Output[DiffLineIndex];
				if (!String.IsNullOrEmpty(DiffLine) && Char.IsNumber(DiffLine[0]))
				{
					var NewChange = new DiffChange(Diff2Output, DiffLineIndex);
					Changes.Add(NewChange);
				}
			}
			return Changes.ToArray();
		}

		/// <summary>
		/// Generates a Blame file for the specified depot file up to the specified revision (default = all revisions). Each line contains a complete revision history.
		/// </summary>
		/// <param name="DepotFilename"></param>
		/// <param name="MaxRevision"></param>
		/// <returns>An array of lines with blame information.</returns>
		public BlameLineInfo[] Timelapse(string DepotFilename, int MaxRevision = Int32.MaxValue)
		{
			CommandUtils.Log("Timelapse({0}#{1})", DepotFilename, (MaxRevision == Int32.MaxValue) ? "head" : MaxRevision.ToString());
			// Get the file history.
			var FileHistory = Filelog(DepotFilename, bQuiet: true);
			if (!CommandUtils.IsNullOrEmpty(FileHistory))
			{
				// Construct the initial blame file from the first revision
				var PrintResult = P4Print(String.Format("{0}#{1}", DepotFilename, FileHistory[0].Revision), bQuiet: true);
				var Blame = PrintToBlame(PrintResult, FileHistory[0]);
				var CurrentRevision = FileHistory[0].Revision;

				// Apply all diff changes up to the specified revision.
				for (int RevisionIndex = 1; RevisionIndex < FileHistory.Length && CurrentRevision <= MaxRevision; ++RevisionIndex)
				{
					var RevisionInfo = FileHistory[RevisionIndex];
					switch (RevisionInfo.Action)
					{
						case P4Action.Delete:
						case P4Action.MoveDelete:
							Blame.Clear();
							break;
						case P4Action.Branch:
						case P4Action.Add:
						case P4Action.MoveAdd:
							Blame = PrintToBlame(P4Print(String.Format("{0}#{1}", DepotFilename, RevisionInfo.Revision), bQuiet: true), RevisionInfo);
							break;
						default:
						{
							int PreviousRev = FileHistory[RevisionIndex - 1].Revision;
							CurrentRevision = RevisionInfo.Revision;
							var Diff2Output = Diff2(DepotFilename, PreviousRev, CurrentRevision, bQuiet: true);

							ParseDiff(Blame, Diff2Output, FileHistory[RevisionIndex]);
						}
						break;
					}

				}
				return Blame.ToArray();
			}
			else
			{
				return new BlameLineInfo[0];
			}
		}

		/// <summary>
		/// Generates a Blame file for the specified depot file up to the specified revision (default = all revisions). Only the last revision is available for each line.
		/// </summary>
		/// <param name="DepotFilename"></param>
		/// <param name="MaxRevision"></param>
		/// <returns>An array of lines with blame information.</returns>
		public BlameLineInfo[] Blame(string DepotFilename)
		{
			CommandUtils.Log("Blame({0})", DepotFilename);
			// Get the file history.
			var FileHistory = Filelog(DepotFilename, bQuiet: true);
			
			// Construct the initial blame file from the first revision
			var AnnotateResult = Annotate(DepotFilename, bQuiet: true);
			var Blame = new List<BlameLineInfo>(AnnotateResult.Length);
			// Get the revision at each line and put it in the resulting Blame file
			for (int LineIndex = 0; LineIndex < AnnotateResult.Length; ++LineIndex)
			{				
				var AnnotatedLine = AnnotateResult[LineIndex];
				// The last line is empty, we could maybe just not iterate it but I'm not sure it's 100% of the time.
				if (!String.IsNullOrEmpty(AnnotatedLine))
				{
					var RevisionEndIndex = AnnotatedLine.IndexOf(':');
					var RevisionIndex = Int32.Parse(AnnotatedLine.Substring(0, RevisionEndIndex)) - 1;
					var LineContentsStart = RevisionEndIndex + 2;
					var LineContents = LineContentsStart >= AnnotatedLine.Length ? String.Empty : AnnotatedLine.Substring(LineContentsStart);
					var BlameInfo = new BlameLineInfo(LineContents, FileHistory[RevisionIndex]);
					Blame.Add(BlameInfo);
				}
			}
			return Blame.ToArray();
		}
	}
}
