// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;

[Help("custom code to restructure C++ source code for the new stats system.")]
class CodeSurgery : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("************************* Reworking stats code");

		var Wildcards = new List<string>() { "*.h", "*.cpp", "*.inl" };

		// @todo: Add support for STATCAT_
		var DeclareStrings = new List<string>() { "DECLARE_CYCLE_STAT", "DECLARE_FLOAT_COUNTER_STAT", "DECLARE_DWORD_COUNTER_STAT", "DECLARE_FLOAT_ACCUMULATOR_STAT", "DECLARE_DWORD_ACCUMULATOR_STAT", "DECLARE_MEMORY_STAT", "DECLARE_MEMORY_STAT_POOL", "DECLARE_STATS_GROUP" };

		DirectoryInfo DirInfo = new DirectoryInfo(CmdEnv.LocalRoot);
		var TopLevelDirs = DirInfo.GetDirectories();

		var Dirs = new List<string>();

		foreach (var TopLevelDir in TopLevelDirs)
		{
			if (DirectoryExists_NoExceptions(CombinePaths(TopLevelDir.FullName, "Source")))
			{
				Dirs.Add(CombinePaths(TopLevelDir.FullName, "Source"));
			}
		}

		var AllFiles = new List<string>();

		foreach (var Dir in Dirs)
		{
			foreach (var Wildcard in Wildcards)
			{
				foreach (var ThisFile in CommandUtils.FindFiles_NoExceptions(Wildcard, true, Dir))
				{
					if (
						!ThisFile.Contains(@"Runtime\Core\Public\Stats\Stats2") &&
						!ThisFile.Contains(@"\ThirdParty\")
						)
					{
						Log("Source File: {0}", ThisFile);
						AllFiles.Add(ThisFile);
					}
				}
			}
		}

		var DeclareFiles = new Dictionary<string, string>();
		var DeclareLines = new Dictionary<string, string>();
		var EnumFiles = new Dictionary<string, string>();
		var EnumLines = new Dictionary<string, string>();
		var Broken = new Dictionary<string, string>();


		foreach (var ThisFile in AllFiles)
		{
			var FileText = ReadAllText(ThisFile);
			if (FileText.Contains("STAT_") || FileText.Contains("STATGROUP_"))
			{
				var Lines = ReadAllLines(ThisFile);
				foreach (var LineWithWS in Lines)
				{
					var Line = LineWithWS.Trim();
					if (Line.Contains("STAT_") || Line.Contains("STATGROUP_"))
					{
						string TypeString = "STAT_";
						if (!Line.Contains(TypeString) || Line.Contains("DECLARE_STATS_GROUP"))
						{
							TypeString = "STATGROUP_";
						}
						bool bDeclareLine = false;
						foreach (var DeclareString in DeclareStrings)
						{
							if (Line.Contains(DeclareString))
							{
								bDeclareLine = true;
								break;
							}
						}
						var Cut = Line;
						string Exception = "DECLARE_MEMORY_STAT_POOL";
						if (Line.StartsWith(Exception))
						{
							Cut = Line.Substring(Exception.Length);
						}
						Cut = Cut.Substring(Cut.IndexOf(TypeString));
						int End = Cut.IndexOf(",");
						if (End < 0)
						{
							End = Cut.Length;
						}
						int EndEq = Cut.IndexOf("=");
						int EndParen = Cut.IndexOf(")");
						int EndParen2 = Cut.IndexOf("(");

						if (EndEq > 0 && EndEq < End)
						{
							End = EndEq;
						}
						if (EndParen > 0 && EndParen < End)
						{
							End = EndParen;
						}
						if (EndParen2 > 0 && EndParen2 < End)
						{
							End = EndParen2;
						}
						string StatName = Cut.Substring(0, End).Trim();

						bool bEnumLine = false;
						if (!bDeclareLine)
						{
							if ((Line.EndsWith(",") || Line == StatName) && Line.StartsWith(TypeString))
							{
								bEnumLine = true;
							}
						}

						if (bEnumLine || bDeclareLine)
						{
							Log("{0} {1} Line: {2} : {3}", bEnumLine ? "Enum" : "Declare", TypeString, StatName, Line);

							if (bEnumLine)
							{
								if (EnumFiles.ContainsKey(StatName))
								{
									if (!Broken.ContainsKey(StatName))
									{
										Broken.Add(StatName, Line);
									}
								}
								else
								{
									EnumFiles.Add(StatName, ThisFile);
									EnumLines.Add(StatName, Line);
								}
							}
							else
							{
								if (DeclareFiles.ContainsKey(StatName))
								{
									if (!Broken.ContainsKey(StatName))
									{
										Broken.Add(StatName, Line);
									}
								}
								else
								{
									DeclareFiles.Add(StatName, ThisFile);
									DeclareLines.Add(StatName, Line);
								}
							}
						}
					}
				}
			}
		}

		var AllGoodStats = new List<string>();
		var AllGoodGroups = new List<string>();


		foreach (var DeclareLine in DeclareLines)
		{
			if (!Broken.ContainsKey(DeclareLine.Key))
			{
				if (EnumFiles.ContainsKey(DeclareLine.Key))
				{
					if (DeclareLine.Key.StartsWith("STATGROUP_"))
					{
						AllGoodGroups.Add(DeclareLine.Key);
					}
					else
					{
						AllGoodStats.Add(DeclareLine.Key);
					}
				}
				else
				{
					Broken.Add(DeclareLine.Key, DeclareLine.Value);
				}
			}
		}

		var ToCheckOuts = new HashSet<string>();
		Log("Stats *************************");
		foreach (var AllGoodStat in AllGoodStats)
		{
			Log("{0}", AllGoodStat);
			ToCheckOuts.Add(DeclareFiles[AllGoodStat]);
			ToCheckOuts.Add(EnumFiles[AllGoodStat]);
		}
		Log("Groups *************************");
		foreach (var AllGoodGroup in AllGoodGroups)
		{
			Log("{0}", AllGoodGroup);
			ToCheckOuts.Add(DeclareFiles[AllGoodGroup]);
			ToCheckOuts.Add(EnumFiles[AllGoodGroup]);
		}
		Log("Broken *************************");
		foreach (var BrokenItem in Broken)
		{
			Log("{0}", BrokenItem.Key);
		}
		Log("*************************");

		int WorkingCL = -1;
		if (P4Enabled)
		{
			WorkingCL = P4.CreateChange(P4Env.Client, "Stat code surgery");
			Log("Working in {0}", WorkingCL);
		}
		else
		{
			throw new AutomationException("this command needs to run with P4.");
		}

		var CheckedOuts = new HashSet<string>();
		foreach (var ToCheckOut in ToCheckOuts)
		{
			if (P4.Edit_NoExceptions(WorkingCL, ToCheckOut))
			{
				CheckedOuts.Add(ToCheckOut);
			}
		}
		Log("Checked Out *************************");
		foreach (var CheckedOut in CheckedOuts)
		{
			Log("{0}", CheckedOut);
		}
		Log("Failed to check out *************************");
		foreach (var ToCheckOut in ToCheckOuts)
		{
			if (!CheckedOuts.Contains(ToCheckOut))
			{
				Log("{0}", ToCheckOut);
			}
		}
		Log("*************************");
		foreach (var AllGoodStat in AllGoodStats)
		{
			if (EnumFiles[AllGoodStat].EndsWith(".cpp", StringComparison.InvariantCultureIgnoreCase))
			{
				var DeclareFileText = ReadAllText(DeclareFiles[AllGoodStat]);
				DeclareFileText = DeclareFileText.Replace(DeclareLines[AllGoodStat], "");
				WriteAllText(DeclareFiles[AllGoodStat], DeclareFileText);

				var EnumFileText = ReadAllText(EnumFiles[AllGoodStat]);
				EnumFileText = EnumFileText.Replace(EnumLines[AllGoodStat], DeclareLines[AllGoodStat]);
				WriteAllText(EnumFiles[AllGoodStat], EnumFileText);
			}
			else
			{
				var DeclareFileText = ReadAllText(DeclareFiles[AllGoodStat]);
				DeclareFileText = DeclareFileText.Replace(DeclareLines[AllGoodStat], "DEFINE_STAT(" + AllGoodStat + ");");
				WriteAllText(DeclareFiles[AllGoodStat], DeclareFileText);

				var EnumFileText = ReadAllText(EnumFiles[AllGoodStat]);
				var ExternDeclare = DeclareLines[AllGoodStat];

				int Paren = ExternDeclare.IndexOf("(");
				ExternDeclare = ExternDeclare.Substring(0, Paren) + "_EXTERN" + ExternDeclare.Substring(Paren);

				Paren = ExternDeclare.LastIndexOf(")");
				ExternDeclare = ExternDeclare.Substring(0, Paren) + ", " + ExternDeclare.Substring(Paren);

				EnumFileText = EnumFileText.Replace(EnumLines[AllGoodStat], ExternDeclare);
				WriteAllText(EnumFiles[AllGoodStat], EnumFileText);

			}
		}
		foreach (var AllGoodGroup in AllGoodGroups)
		{
			var DeclareFileText = ReadAllText(DeclareFiles[AllGoodGroup]);
			DeclareFileText = DeclareFileText.Replace(DeclareLines[AllGoodGroup], "");
			WriteAllText(DeclareFiles[AllGoodGroup], DeclareFileText);

			var EnumFileText = ReadAllText(EnumFiles[AllGoodGroup]);
			EnumFileText = EnumFileText.Replace(EnumLines[AllGoodGroup], DeclareLines[AllGoodGroup]);
			WriteAllText(EnumFiles[AllGoodGroup], EnumFileText);
		}

		Log("*************************");

	}
}
