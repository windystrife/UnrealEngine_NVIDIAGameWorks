using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
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
