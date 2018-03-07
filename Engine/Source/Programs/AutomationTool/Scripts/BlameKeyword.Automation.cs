// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Security;
using System.Security.Principal;
using System.Security.Permissions;
using System.Runtime.InteropServices;
using System.Runtime.ConstrainedExecution;
using Microsoft.Win32.SafeHandles;
using AutomationTool;
using System.Threading;
using UnrealBuildTool;

[Help("BlameKeyword command. Looks for the specified keywords in all files at the specified path and finds who added them based on P4 history")]
[Help("Path", "Local path to search (including subfolders)")]
[Help("Keywords", "Comma separated list of keywords to search for")]
[Help("Timelapse", "(Optional) If specified, uses full revision history (slow)")]
[RequireP4]
class BlameKeyword : BuildCommand
{
	static string[] KnownExtensions = new string[]
	{
		".cpp",
		".c",
		".mm",
		".inl",
		".h",
		".hpp",
		".cs"
	};

	private int ContainsAnyKeyword(string Text, string[] Keywords)
	{
		for (int KeywordIndex = 0; KeywordIndex < Keywords.Length; ++KeywordIndex)
		{
			if (Text.IndexOf(Keywords[KeywordIndex], 0, StringComparison.InvariantCultureIgnoreCase) >= 0)
			{
				return KeywordIndex;
			}
		}
		return -1;
	}

	public override void ExecuteBuild()
	{
		Log("************************** BlameKeyword");

		var Extensions = new HashSet<string>(KnownExtensions, StringComparer.InvariantCultureIgnoreCase);
		var PathToSearch = ParseParamValue("Path", CombinePaths(CmdEnv.LocalRoot, "Engine", "Source", "Runtime"));
		var Keywords = ParseParamValue("Keywords", "@todo,@fixme").Split(',');
		var bUseTimelapse = ParseParam("Timelapse");
		var OutFilename = ParseParamValue("Out", CombinePaths(CmdEnv.LogFolder, "BlameKeywordsOut.csv"));

		if (IsNullOrEmpty(Keywords))
		{
			throw new AutomationException("No keywords to look for");
		}

		Log("Path={0}", PathToSearch);
		Log("Keywords={0}", String.Join(",", Keywords));
		Log("Timelapse={0}", bUseTimelapse);
		Log("Out={0}", OutFilename);

		// Find files to search
		var FilenamesToSearch = new List<string>();
		var DirInfo = new DirectoryInfo(PathToSearch);
		if (DirInfo.Exists)
		{
			var FoundFiles = Directory.GetFiles(DirInfo.FullName, "*", SearchOption.AllDirectories);
			foreach (var Filename in FoundFiles)
			{
				var Ext = Path.GetExtension(Filename);
				if (Extensions.Contains(Ext))
				{
					FilenamesToSearch.Add(Filename);
				}
			}
		}
		else
		{
			var FilenameInfo = new FileInfo(PathToSearch);
			if (FilenameInfo.Exists)
			{
				FilenamesToSearch.Add(FilenameInfo.FullName);
			}
		}

		if (IsNullOrEmpty(FilenamesToSearch))
		{
			throw new AutomationException("No files found in {0}", PathToSearch);
		}

		// Crate a csv based on search results.
		var BlameCSV = new StringBuilder();
		BlameCSV.AppendLine("Keyword;Filename;Line No;User;Revision;Changelist;Date;Line Contents");
		foreach (var Filename in FilenamesToSearch)
		{
			var FileContents = ReadAllLines(Filename);
			P4Connection.BlameLineInfo[] BlameInfo = null;
			for (int LineIndex = 0; LineIndex < FileContents.Length; ++LineIndex)
			{
				var Line = FileContents[LineIndex];
				var KeywordIndex = ContainsAnyKeyword(Line, Keywords);
				if (KeywordIndex >= 0)
				{
					if (BlameInfo == null)
					{
						BlameInfo = bUseTimelapse ? P4.Timelapse(Filename) : P4.Blame(Filename);
					}
					if (IsNullOrEmpty(BlameInfo) == false && LineIndex < BlameInfo.Length)
					{
						var LineInfo = BlameInfo[LineIndex];
						// In case this isn't the first revision of this line, look at previous
						// revisions to find the first revision where the keyword was added.
						foreach (var PreviousLineRevision in LineInfo.PreviousRevisions)
						{
							if (ContainsAnyKeyword(PreviousLineRevision.Line, Keywords) >= 0)
							{
								LineInfo = PreviousLineRevision;
								break;
							}
						}
						var Blame = String.Format("\"{0}\";\"{1}\";{2};{3};{4};{5};{6};\"{7}\"",
							Keywords[KeywordIndex], Filename, LineIndex + 1, LineInfo.Revision.User, LineInfo.Revision.Revision, LineInfo.Revision.Changelist, LineInfo.Revision.Date, Line);
						BlameCSV.AppendLine(Blame);
					}
				}
			}
		}

		// Write out the csv
		Log("Creating blame csv: {0}", OutFilename);
		WriteAllText(OutFilename, BlameCSV.ToString());
	}
}
