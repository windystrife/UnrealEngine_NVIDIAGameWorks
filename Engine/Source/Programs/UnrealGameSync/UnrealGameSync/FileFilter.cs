// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	/// <summary>
	/// Indicates whether files which match a pattern should be included or excluded
	/// </summary>
	public enum FileFilterType
	{
		Include,
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
		/// <param name="Pattern">Pattern to match. See CreateRegex() for details.</param>
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
		/// <param name="Pattern">Pattern to match. See CreateRegex() for details.</param>
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
		/// <param name="Patterns">List of patterns to match.</param>
		public void AddRules(IEnumerable<string> Rules)
		{
			foreach(string Rule in Rules)
			{
				AddRule(Rule);
			}
		}

		/// <summary>
		/// Adds several rules in the given lines. Rules may be prefixed with conditions of the syntax {Key=Value, Key2=Value2}, which
		/// will be evaluated using variables in the given dictionary before being added.
		/// </summary>
		/// <param name="Lines"></param>
		/// <param name="Variables">Lookup for variables to test against</param>
		public void AddRules(IEnumerable<string> Rules, params string[] Tags)
		{
			foreach(string Rule in Rules)
			{
				AddRule(Rule, Tags);
			}
		}

		/// <summary>
		/// Reads a configuration file split into sections
		/// </summary>
		/// <param name="Filter"></param>
		/// <param name="RulesFileName"></param>
		/// <param name="Conditions"></param>
		public void ReadRulesFromFile(string FileName, string SectionName, params string[] AllowTags)
		{
			bool bInSection = false;
			foreach(string Line in File.ReadAllLines(FileName))
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
		/// <param name="bInclude">Whether to include or exclude files matching this rule</param>
		public void AddRule(string Pattern, FileFilterType Type)
		{
			string NormalizedPattern = Pattern.Replace('\\', '/');

			// We don't want a slash at the start, but if there was not one specified, it's not anchored to the root of the tree.
			if (NormalizedPattern.StartsWith("/"))
			{
				NormalizedPattern = NormalizedPattern.Substring(1);
			}
			else if(!NormalizedPattern.StartsWith("..."))
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
		/// Excludes all restricted platform folders from the filter
		/// </summary>
		public void ExcludeConfidentialPlatforms()
		{
			AddRule(".../PS4/...", FileFilterType.Exclude);
			AddRule(".../XboxOne/...", FileFilterType.Exclude);
		}

		/// <summary>
		/// Excludes all confidential folders from the filter
		/// </summary>
		public void ExcludeConfidentialFolders()
		{
			AddRule(".../EpicInternal/...", FileFilterType.Exclude);
			AddRule(".../CarefullyRedist/...", FileFilterType.Exclude);
			AddRule(".../NotForLicensees/...", FileFilterType.Exclude);
			AddRule(".../NoRedist/...", FileFilterType.Exclude);
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

	/// <summary>
	/// Node within a filter tree. Each node matches a single path fragment - a folder or file name, with an include or exclude rule consisting of a sequence of nodes. 
	/// </summary>
	class FileFilterNode
	{
		/// <summary>
		/// Node which this is parented to. Null for the root node.
		/// </summary>
		public readonly FileFilterNode Parent;

		/// <summary>
		/// Pattern to match for this node. May contain * or ? wildcards.
		/// </summary>
		public readonly string Pattern;

		/// <summary>
		/// Highest include rule number matched by this node or any child nodes.
		/// </summary>
		public int MaxIncludeRuleNumber = -1;

		/// <summary>
		/// Highest exclude rule number matched by this node or any child nodes.
		/// </summary>
		public int MaxExcludeRuleNumber = -1;

		/// <summary>
		/// Child branches of this node, distinct by the the pattern for each.
		/// </summary>
		public List<FileFilterNode> Branches = new List<FileFilterNode>();

		/// <summary>
		/// If a path matches the sequence of nodes ending in this node, the number of the rule which added it. -1 if this was not the last node in a rule.
		/// </summary>
		public int RuleNumber;

		/// <summary>
		/// Whether a rule terminating in this node should include files or exclude files.
		/// </summary>
		public FileFilterType Type;

		/// <summary>
		/// Default constructor.
		/// </summary>
		public FileFilterNode(FileFilterNode InParent, string InPattern)
		{
			Parent = InParent;
			Pattern = InPattern;
			RuleNumber = -1;
			Type = FileFilterType.Exclude;
		}

		/// <summary>
		/// Determine if the given token matches the pattern in this node
		/// </summary>
		public bool IsMatch(string Token)
		{
			if(Pattern.EndsWith("."))
			{
				return !Token.Contains('.') && IsMatch(Token, 0, Pattern.Substring(0, Pattern.Length - 1), 0);
			}
			else
			{
				return IsMatch(Token, 0, Pattern, 0);
			}
		}

		/// <summary>
		/// Determine if a given token matches a pattern, starting from the given positions within each string.
		/// </summary>
		static bool IsMatch(string Token, int TokenIdx, string Pattern, int PatternIdx)
		{
			for (; ; )
			{
				if (PatternIdx == Pattern.Length)
				{
					return (TokenIdx == Token.Length);
				}
				else if (Pattern[PatternIdx] == '*')
				{
					for (int NextTokenIdx = Token.Length; NextTokenIdx >= TokenIdx; NextTokenIdx--)
					{
						if (IsMatch(Token, NextTokenIdx, Pattern, PatternIdx + 1))
						{
							return true;
						}
					}
					return false;
				}
				else if (TokenIdx < Token.Length && (Char.ToLower(Token[TokenIdx]) == Char.ToLower(Pattern[PatternIdx]) || Pattern[PatternIdx] == '?'))
				{
					TokenIdx++;
					PatternIdx++;
				}
				else
				{
					return false;
				}
			}
		}

		/// <summary>
		/// Debugger visualization
		/// </summary>
		/// <returns>Path to this node</returns>
		public override string ToString()
		{
			return (Parent == null) ? "/" : Parent.ToString().TrimEnd('/') + "/" + Pattern;
		}
	}
}
