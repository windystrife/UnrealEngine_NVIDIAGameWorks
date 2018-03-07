// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Indicates whether files which match a pattern should be included or excluded
	/// </summary>
	public enum FileFilterType
	{
		/// <summary>
		/// Include files matching this pattern
		/// </summary>
		Include,

		/// <summary>
		/// Exclude files matching this pattern
		/// </summary>
		Exclude,
	}

	/// <summary>
	/// Stores a set of rules, similar to p4 path specifications, which can be used to efficiently filter files in or out of a set.
	/// The rules are merged into a tree with nodes for each path fragments and rule numbers in each leaf, allowing tree traversal in an arbitrary order 
	/// to how they are applied.
	/// </summary>
	public class FileFilter
	{
		/// <summary>
		/// Root node for the tree.
		/// </summary>
		FileFilterNode RootNode;

		/// <summary>
		/// The default node, which will match any path
		/// </summary>
		FileFilterNode DefaultNode;

		/// <summary>
		/// Terminating nodes for each rule added to the filter
		/// </summary>
		List<FileFilterNode> Rules = new List<FileFilterNode>();

		/// <summary>
		/// Default constructor
		/// </summary>
		public FileFilter(FileFilterType DefaultType = FileFilterType.Exclude)
		{
			RootNode = new FileFilterNode(null, "");

			DefaultNode = new FileFilterNode(RootNode, "...");
			DefaultNode.Type = DefaultType;
		}

		/// <summary>
		/// Copy constructor
		/// </summary>
		/// <param name="Other">Filter to copy from</param>
		public FileFilter(FileFilter Other)
			: this(Other.DefaultNode.Type)
		{
			foreach (FileFilterNode OtherRule in Other.Rules)
			{
				AddRule(OtherRule.ToString(), OtherRule.Type);
			}
		}

		/// <summary>
		/// Constructs a file filter from a p4-style filespec. Exclude lines are prefixed with a - character.
		/// </summary>
		/// <param name="Lines">Lines to parse rules from</param>
		public FileFilter(IEnumerable<string> Lines)
			: this()
		{
			foreach (string Line in Lines)
			{
				AddRule(Line);
			}
		}

		/// <summary>
		/// Adds an include or exclude rule to the filter
		/// </summary>
		/// <param name="Rule">Pattern to match. See CreateRegex() for details.</param>
		public void AddRule(string Rule)
		{
			if (Rule.StartsWith("-"))
			{
				Exclude(Rule.Substring(1).TrimStart());
			}
			else
			{
				Include(Rule);
			}
		}

		/// <summary>
		/// Adds an include or exclude rule to the filter. The rule may be 
		/// </summary>
		/// <param name="Rule">Pattern to match. See CreateRegex() for details.</param>
		/// <param name="AllowTags">If the rule starts with a list of tags {a,b,c}, specifies a list of tags to match against</param>
		public void AddRule(string Rule, params string[] AllowTags)
		{
			string CleanRule = Rule.Trim();
			if(CleanRule.StartsWith("{"))
			{
				// Find the end of the condition
				int ConditionEnd = CleanRule.IndexOf('}');
				if(ConditionEnd == -1)
				{
					throw new Exception(String.Format("Missing closing parenthesis in rule: {0}", CleanRule));
				}

				// Check there's a matching tag
				string[] RuleTags = CleanRule.Substring(1, ConditionEnd - 1).Split(',').Select(x => x.Trim()).ToArray();
				if(!RuleTags.Any(x => AllowTags.Contains(x)))
				{
					return;
				}

				// Strip the condition from the rule
				CleanRule = CleanRule.Substring(ConditionEnd + 1).TrimStart(); 
			}
			AddRule(CleanRule);
		}

		/// <summary>
		/// Adds several rules to the filter
		/// </summary>
		/// <param name="Rules">List of patterns to match.</param>
		public void AddRules(IEnumerable<string> Rules)
		{
			foreach(string Rule in Rules)
			{
				AddRule(Rule);
			}
		}

		/// <summary>
		/// Adds several rules to the filter
		/// </summary>
		/// <param name="Rules">List of patterns to match.</param>
		/// <param name="Type">Whether the rules are include or exclude rules</param>
		public void AddRules(IEnumerable<string> Rules, FileFilterType Type)
		{
			foreach(string Rule in Rules)
			{
				AddRule(Rule, Type);
			}
		}

		/// <summary>
		/// Adds several rules in the given lines. Rules may be prefixed with conditions of the syntax {Key=Value, Key2=Value2}, which
		/// will be evaluated using variables in the given dictionary before being added.
		/// </summary>
		/// <param name="Rules"></param>
		/// <param name="Tags">Lookup for variables to test against</param>
		public void AddRules(IEnumerable<string> Rules, params string[] Tags)
		{
			foreach(string Rule in Rules)
			{
				AddRule(Rule, Tags);
			}
		}

		/// <summary>
		/// Adds a rule which matches a filename relative to a given base directory.
		/// </summary>
		/// <param name="File">The filename to add a rule for</param>
		/// <param name="BaseDirectory">Base directory for the rule</param>
		/// <param name="Type">Whether to add an include or exclude rule</param>
		public void AddRuleForFile(FileReference File, DirectoryReference BaseDirectory, FileFilterType Type)
		{
			AddRule("/" + File.MakeRelativeTo(BaseDirectory), Type);
		}

		/// <summary>
		/// Adds rules for files which match the given names
		/// </summary>
		/// <param name="Files">The filenames to rules for</param>
		/// <param name="BaseDirectory">Base directory for the rules</param>
		/// <param name="Type">Whether to add an include or exclude rule</param>
		public void AddRuleForFiles(IEnumerable<FileReference> Files, DirectoryReference BaseDirectory, FileFilterType Type)
		{
			foreach (FileReference File in Files)
			{
				AddRuleForFile(File, BaseDirectory, Type);
			}
		}

		/// <summary>
		/// Reads a configuration file split into sections
		/// </summary>
		/// <param name="FileName"></param>
		/// <param name="SectionName"></param>
		/// <param name="AllowTags"></param>
		public void ReadRulesFromFile(FileReference FileName, string SectionName, params string[] AllowTags)
		{
			bool bInSection = false;
			foreach(string Line in File.ReadAllLines(FileName.FullName))
			{
				string TrimLine = Line.Trim();
				if(!TrimLine.StartsWith(";") && TrimLine.Length > 0)
				{
					if(TrimLine.StartsWith("["))
					{
						bInSection = (TrimLine == "[" + SectionName + "]");
					}
					else if(bInSection)
					{
						AddRule(Line, AllowTags);
					}
				}
			}
		}

		/// <summary>
		/// Adds an include rule to the filter
		/// </summary>
		/// <param name="Pattern">Pattern to match. See CreateRegex() for details.</param>
		public void Include(string Pattern)
		{
			AddRule(Pattern, FileFilterType.Include);
		}

		/// <summary>
		/// Adds several exclude rules to the filter
		/// </summary>
		/// <param name="Patterns">Patterns to match. See CreateRegex() for details.</param>
		public void Include(IEnumerable<string> Patterns)
		{
			foreach (string Pattern in Patterns)
			{
				Include(Pattern);
			}
		}

		/// <summary>
		/// Adds an exclude rule to the filter
		/// </summary>
		/// <param name="Pattern">Mask to match. See CreateRegex() for details.</param>
		public void Exclude(string Pattern)
		{
			AddRule(Pattern, FileFilterType.Exclude);
		}

		/// <summary>
		/// Adds several exclude rules to the filter
		/// </summary>
		/// <param name="Patterns">Patterns to match. See CreateRegex() for details.</param>
		public void Exclude(IEnumerable<string> Patterns)
		{
			foreach (string Pattern in Patterns)
			{
				Exclude(Pattern);
			}
		}

		/// <summary>
		/// Adds an include or exclude rule to the filter
		/// </summary>
		/// <param name="Pattern">The pattern which the rule should match</param>
		/// <param name="Type">Whether to include or exclude files matching this rule</param>
		public void AddRule(string Pattern, FileFilterType Type)
		{
			string NormalizedPattern = Pattern.Replace('\\', '/');

			// Remove the slash from the start of the pattern. Any exclude pattern that doesn't contain a directory separator is assumed to apply to any directory (eg. *.cpp), otherwise it's 
			// taken relative to the root.
			if (NormalizedPattern.StartsWith("/"))
			{
				NormalizedPattern = NormalizedPattern.Substring(1);
			}
			else if(!NormalizedPattern.Contains("/") && !NormalizedPattern.StartsWith("..."))
			{
				NormalizedPattern = ".../" + NormalizedPattern;
			}

			// All directories indicate a wildcard match
			if (NormalizedPattern.EndsWith("/"))
			{
				NormalizedPattern += "...";
			}

			// Replace any directory wildcards mid-string
			for (int Idx = NormalizedPattern.IndexOf("..."); Idx != -1; Idx = NormalizedPattern.IndexOf("...", Idx))
			{
				if (Idx > 0 && NormalizedPattern[Idx - 1] != '/')
				{
					NormalizedPattern = NormalizedPattern.Insert(Idx, "*/");
					Idx++;
				}

				Idx += 3;

				if (Idx < NormalizedPattern.Length && NormalizedPattern[Idx] != '/')
				{
					NormalizedPattern = NormalizedPattern.Insert(Idx, "/*");
					Idx += 2;
				}
			}

			// Split the pattern into fragments
			string[] BranchPatterns = NormalizedPattern.Split('/');

			// Add it into the tree
			FileFilterNode LastNode = RootNode;
			foreach (string BranchPattern in BranchPatterns)
			{
				FileFilterNode NextNode = LastNode.Branches.FirstOrDefault(x => x.Pattern == BranchPattern);
				if (NextNode == null)
				{
					NextNode = new FileFilterNode(LastNode, BranchPattern);
					LastNode.Branches.Add(NextNode);
				}
				LastNode = NextNode;
			}

			// We've reached the end of the pattern, so mark it as a leaf node
			Rules.Add(LastNode);
			LastNode.RuleNumber = Rules.Count - 1;
			LastNode.Type = Type;

			// Update the maximums along that path
			for (FileFilterNode UpdateNode = LastNode; UpdateNode != null; UpdateNode = UpdateNode.Parent)
			{
				if (Type == FileFilterType.Include)
				{
					UpdateNode.MaxIncludeRuleNumber = LastNode.RuleNumber;
				}
				else
				{
					UpdateNode.MaxExcludeRuleNumber = LastNode.RuleNumber;
				}
			}
		}

		/// <summary>
		/// Excludes all confidential folders from the filter
		/// </summary>
		public void ExcludeRestrictedFolders()
		{
			foreach(FileSystemName RestrictedFolderName in PlatformExports.RestrictedFolderNames)
			{
				AddRule(String.Format(".../{0}/...", RestrictedFolderName), FileFilterType.Exclude);
			}
		}

		/// <summary>
		/// Determines whether the given file matches the filter
		/// </summary>
		/// <param name="FileName">File to match</param>
		/// <returns>True if the file passes the filter</returns>
		public bool Matches(string FileName)
		{
			string[] Tokens = FileName.TrimStart('/', '\\').Split('/', '\\');

			FileFilterNode MatchingNode = FindMatchingNode(RootNode, Tokens, 0, DefaultNode);

			return MatchingNode.Type == FileFilterType.Include;
		}

		/// <summary>
		/// Determines whether it's possible for anything within the given folder name to match the filter. Useful to early out of recursive file searches.
		/// </summary>
		/// <param name="FolderName">File to match</param>
		/// <returns>True if the file passes the filter</returns>
		public bool PossiblyMatches(string FolderName)
		{
			string[] Tokens = FolderName.Trim('/', '\\').Split('/', '\\');

			FileFilterNode MatchingNode = FindMatchingNode(RootNode, Tokens.Union(new string[]{ "" }).ToArray(), 0, DefaultNode);

			return MatchingNode.Type == FileFilterType.Include || HighestPossibleIncludeMatch(RootNode, Tokens, 0, MatchingNode.RuleNumber) > MatchingNode.RuleNumber;
		}

		/// <summary>
		/// Applies the filter to each element in a sequence, and returns the list of files that match
		/// </summary>
		/// <param name="FileNames">List of filenames</param>
		/// <returns>List of filenames which match the filter</returns>
		public IEnumerable<string> ApplyTo(IEnumerable<string> FileNames)
		{
			return FileNames.Where(x => Matches(x));
		}

		/// <summary>
		/// Applies the filter to each element in a sequence, and returns the list of files that match
		/// </summary>
		/// <param name="BaseDirectory">The base directory to match files against</param>
		/// <param name="FileNames">List of filenames</param>
		/// <returns>List of filenames which match the filter</returns>
		public IEnumerable<FileReference> ApplyTo(DirectoryReference BaseDirectory, IEnumerable<FileReference> FileNames)
		{
			return FileNames.Where(x => Matches(x.MakeRelativeTo(BaseDirectory)));
		}

		/// <summary>
		/// Finds a list of files within a given directory which match the filter.
		/// </summary>
		/// <param name="DirectoryName">File to match</param>
		/// <param name="bIgnoreSymlinks">Whether to ignore symlinks in the output</param>
		/// <returns>List of files that pass the filter</returns>
		public List<FileReference> ApplyToDirectory(DirectoryReference DirectoryName, bool bIgnoreSymlinks)
		{
			List<FileReference> MatchingFileNames = new List<FileReference>();
			if (DirectoryReference.Exists(DirectoryName))
			{
				FindMatchesFromDirectory(new DirectoryInfo(DirectoryName.FullName), "", bIgnoreSymlinks, MatchingFileNames);
			}
			return MatchingFileNames;
		}

		/// <summary>
		/// Finds a list of files within a given directory which match the filter.
		/// </summary>
		/// <param name="DirectoryName">File to match</param>
		/// <param name="PrefixPath"></param>
		/// <param name="bIgnoreSymlinks">Whether to ignore symlinks in the output</param>
		/// <returns>List of files that pass the filter</returns>
		public List<FileReference> ApplyToDirectory(DirectoryReference DirectoryName, string PrefixPath, bool bIgnoreSymlinks)
		{
			List<FileReference> MatchingFileNames = new List<FileReference>();
			if (DirectoryReference.Exists(DirectoryName))
			{
				FindMatchesFromDirectory(new DirectoryInfo(DirectoryName.FullName), PrefixPath.Replace('\\', '/').TrimEnd('/') + "/", bIgnoreSymlinks, MatchingFileNames);
			}
			return MatchingFileNames;
		}

		/// <summary>
		/// Checks whether the given pattern contains a supported wildcard. Useful for distinguishing explicit file references from opportunistic file references.
		/// </summary>
		/// <param name="Pattern">The pattern to check</param>
		/// <returns>True if the pattern contains a wildcard (?, *, ...), false otherwise.</returns>
		public static int FindWildcardIndex(string Pattern)
		{
			int Result = -1;
			for(int Idx = 0; Idx < Pattern.Length; Idx++)
			{
				if(Pattern[Idx] == '?' || Pattern[Idx] == '*' || (Pattern[Idx] == '.' && Idx + 2 < Pattern.Length && Pattern[Idx + 1] == '.' && Pattern[Idx + 2] == '.'))
				{
					Result = Idx;
					break;
				}
			}
			return Result;
		}

		/// <summary>
		/// Resolve an individual wildcard to a set of files
		/// </summary>
		/// <param name="Pattern">The pattern to resolve</param>
		/// <returns>List of files matching the wildcard</returns>
		public static List<FileReference> ResolveWildcard(string Pattern)
		{
			for(int Idx = FindWildcardIndex(Pattern); Idx > 0; Idx--)
			{
				if(Pattern[Idx] == '/' || Pattern[Idx] == '\\')
				{
					return ResolveWildcard(new DirectoryReference(Pattern.Substring(0, Idx)), Pattern.Substring(Idx + 1));
				}
			}
			return ResolveWildcard(DirectoryReference.GetCurrentDirectory(), Pattern);
		}

		/// <summary>
		/// Resolve an individual wildcard to a set of files
		/// </summary>
		/// <param name="BaseDir">Base directory for wildcards</param>
		/// <param name="Pattern">The pattern to resolve</param>
		/// <returns>List of files matching the wildcard</returns>
		public static List<FileReference> ResolveWildcard(DirectoryReference BaseDir, string Pattern)
		{
			FileFilter Filter = new FileFilter(FileFilterType.Exclude);
			Filter.AddRule(Pattern);
			return Filter.ApplyToDirectory(BaseDir, true);
		}

		/// <summary>
		/// Finds a list of files within a given directory which match the filter.
		/// </summary>
		/// <param name="CurrentDirectory">The current directory</param>
		/// <param name="NamePrefix">Current relative path prefix in the traversed directory tree</param>
		/// <param name="bIgnoreSymlinks">Whether to ignore symlinks in the output</param>
		/// <param name="MatchingFileNames">Receives a list of matching files</param>
		/// <returns>True if the file passes the filter</returns>
		void FindMatchesFromDirectory(DirectoryInfo CurrentDirectory, string NamePrefix, bool bIgnoreSymlinks, List<FileReference> MatchingFileNames)
		{
			foreach (FileInfo NextFile in CurrentDirectory.EnumerateFiles())
			{
				string FileName = NamePrefix + NextFile.Name;
				if (Matches(FileName) && (!bIgnoreSymlinks || !NextFile.Attributes.HasFlag(FileAttributes.ReparsePoint)))
				{
					MatchingFileNames.Add(new FileReference(NextFile));
				}
			}
			foreach (DirectoryInfo NextDirectory in CurrentDirectory.EnumerateDirectories())
			{
				string NextNamePrefix = NamePrefix + NextDirectory.Name;
				if (PossiblyMatches(NextNamePrefix))
				{
					FindMatchesFromDirectory(NextDirectory, NextNamePrefix + "/", bIgnoreSymlinks, MatchingFileNames);
				}
			}
		}

		/// <summary>
		/// Finds the node which matches a given list of tokens.
		/// </summary>
		/// <param name="CurrentNode"></param>
		/// <param name="Tokens"></param>
		/// <param name="TokenIdx"></param>
		/// <param name="CurrentBestNode"></param>
		/// <returns></returns>
		FileFilterNode FindMatchingNode(FileFilterNode CurrentNode, string[] Tokens, int TokenIdx, FileFilterNode CurrentBestNode)
		{
			// If we've matched all the input tokens, check if this rule is better than any other we've seen
			if (TokenIdx == Tokens.Length)
			{
				return (CurrentNode.RuleNumber > CurrentBestNode.RuleNumber) ? CurrentNode : CurrentBestNode;
			}

			// If there is no rule under the current node which is better than the current best node, early out
			if (CurrentNode.MaxIncludeRuleNumber <= CurrentBestNode.RuleNumber && CurrentNode.MaxExcludeRuleNumber <= CurrentBestNode.RuleNumber)
			{
				return CurrentBestNode;
			}

			// Test all the branches for one that matches
			FileFilterNode BestNode = CurrentBestNode;
			foreach (FileFilterNode Branch in CurrentNode.Branches)
			{
				if (Branch.Pattern == "...")
				{
					for (int NextTokenIdx = Tokens.Length; NextTokenIdx >= TokenIdx; NextTokenIdx--)
					{
						BestNode = FindMatchingNode(Branch, Tokens, NextTokenIdx, BestNode);
					}
				}
				else
				{
					if (Branch.IsMatch(Tokens[TokenIdx]))
					{
						BestNode = FindMatchingNode(Branch, Tokens, TokenIdx + 1, BestNode);
					}
				}
			}
			return BestNode;
		}

		/// <summary>
		/// Returns the highest possible rule number which can match the given list of input tokens, assuming that the list of input tokens is incomplete.
		/// </summary>
		/// <param name="CurrentNode">The current node being checked</param>
		/// <param name="Tokens">The tokens to match</param>
		/// <param name="TokenIdx">Current token index</param>
		/// <param name="CurrentBestRuleNumber">The highest rule number seen so far. Used to optimize tree traversals.</param>
		/// <returns>New highest rule number</returns>
		int HighestPossibleIncludeMatch(FileFilterNode CurrentNode, string[] Tokens, int TokenIdx, int CurrentBestRuleNumber)
		{
			// If we've matched all the input tokens, check if this rule is better than any other we've seen
			if (TokenIdx == Tokens.Length)
			{
				return Math.Max(CurrentBestRuleNumber, CurrentNode.MaxIncludeRuleNumber);
			}

			// Test all the branches for one that matches
			int BestRuleNumber = CurrentBestRuleNumber;
			if (CurrentNode.MaxIncludeRuleNumber > BestRuleNumber)
			{
				foreach (FileFilterNode Branch in CurrentNode.Branches)
				{
					if (Branch.Pattern == "...")
					{
						if (Branch.MaxIncludeRuleNumber > BestRuleNumber)
						{
							BestRuleNumber = Branch.MaxIncludeRuleNumber;
						}
					}
					else
					{
						if (Branch.IsMatch(Tokens[TokenIdx]))
						{
							BestRuleNumber = HighestPossibleIncludeMatch(Branch, Tokens, TokenIdx + 1, BestRuleNumber);
						}
					}
				}
			}
			return BestRuleNumber;
		}
	}
}
