// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MemoryProfiler2
{
	public class FAllocationTagHierarchy
	{
		public void InitializeHierarchy()
		{
			RootNode = new FNode(-1, null, null);

			var Untagged = new FAllocationTag("Untagged");
			UntaggedNode = new FNode(Untagged.TagNameIndex, RootNode, Untagged);
		}

		public void FinalizeHierarchy()
		{
			RootNode.SortChildrenRecursive();
		}

		public void RegisterTag(string TagStr, FAllocationTag Tag)
		{
			string CategoryName = "Uncategorized";
			string ActualTag = TagStr;

			// Split the tag into its category and remaining parts
			int CategorySeparatorIndex = TagStr.IndexOf(':');
			if (CategorySeparatorIndex != -1)
			{
				CategoryName = TagStr.Substring(0, CategorySeparatorIndex);
				ActualTag = TagStr.Substring(CategorySeparatorIndex + 1);
			}

			// Add the top-level category
			FNode CurrentNode = RootNode;
			{
				int CategoryNameIndex = FStreamInfo.GlobalInstance.GetNameIndex(CategoryName, true);
				CurrentNode = CurrentNode.EnsureChildNode(CategoryNameIndex);
			}

			// Split the remaining parts it hierarchical parts, add a name entry for each tag, and add to the hierarchy
			var HierarchicalTags = ActualTag.Split(TagPathSeparators, StringSplitOptions.RemoveEmptyEntries);
			var HierarchicalTagNameIndices = new int[HierarchicalTags.Length];
			for (int HierarchicalTagIndex = 0; HierarchicalTagIndex < HierarchicalTags.Length; ++HierarchicalTagIndex)
			{
				int HierarchicalTagNameIndex = FStreamInfo.GlobalInstance.GetNameIndex(HierarchicalTags[HierarchicalTagIndex], true);

				// Are we dealing with a child node, or a tag?
				if (HierarchicalTagIndex + 1 == HierarchicalTags.Length)
				{
					CurrentNode.EnsureChildTag(HierarchicalTagNameIndex, Tag);
				}
				else
				{
					CurrentNode = CurrentNode.EnsureChildNode(HierarchicalTagNameIndex);
				}
			}
		}

		public INode GetRootNode()
		{
			return RootNode;
		}

		public INode GetUntaggedNode()
		{
			return UntaggedNode;
		}

		private static char[] TagPathSeparators = { '/' };

		public interface INode
		{
			string GetNodeName();

			IEnumerable<INode> GetChildNodes();

			IEnumerable<INode> GetChildTags();

			FAllocationTag? GetTag();
		}

		private class FNode : INode
		{
			public FNode(int InNameIndex, FNode InParent, FAllocationTag? InTag)
			{
				Parent = InParent;
				if (Parent != null)
				{
					if (InTag.HasValue)
					{
						Parent.RegisterChildTag(InNameIndex, this);
					}
					else
					{
						Parent.RegisterChildNode(InNameIndex, this);
					}
				}

				NameIndex = InNameIndex;
				Tag = InTag;

				ChildNodes = new List<FNode>();
				ChildNodesByNameMap = new Dictionary<int, FNode>();

				ChildTags = new List<FNode>();
				ChildTagsByNameMap = new Dictionary<int, FNode>();
			}

			public string GetNodeName()
			{
				return FStreamInfo.GlobalInstance.NameArray[NameIndex];
			}

			public IEnumerable<INode> GetChildNodes()
			{
				return ChildNodes;
			}

			public IEnumerable<INode> GetChildTags()
			{
				return ChildTags;
			}

			public FAllocationTag? GetTag()
			{
				return Tag;
			}

			public FNode EnsureChildNode(int InChildNameIndex)
			{
				FNode ChildNode;
				if (!ChildNodesByNameMap.TryGetValue(InChildNameIndex, out ChildNode))
				{
					ChildNode = new FNode(InChildNameIndex, this, null);
				}
				return ChildNode;
			}

			public void EnsureChildTag(int InChildNameIndex, FAllocationTag InTag)
			{
				if (!ChildTagsByNameMap.ContainsKey(InChildNameIndex))
				{
					new FNode(InChildNameIndex, this, InTag);
				}
			}

			public void SortChildrenRecursive()
			{
				// We sort by display name
				ChildNodes.Sort((First, Second) => FStreamInfo.GlobalInstance.NameArray[First.NameIndex].CompareTo(FStreamInfo.GlobalInstance.NameArray[Second.NameIndex]));
				ChildTags.Sort((First, Second) => FStreamInfo.GlobalInstance.NameArray[First.NameIndex].CompareTo(FStreamInfo.GlobalInstance.NameArray[Second.NameIndex]));

				foreach (var ChildNode in ChildNodes)
				{
					ChildNode.SortChildrenRecursive();
				}
			}

			void RegisterChildNode(int InChildNameIndex, FNode InChild)
			{
				ChildNodes.Add(InChild);
				ChildNodesByNameMap.Add(InChildNameIndex, InChild);
			}

			void RegisterChildTag(int InChildNameIndex, FNode InChild)
			{
				ChildTags.Add(InChild);
				ChildTagsByNameMap.Add(InChildNameIndex, InChild);
			}

			FNode Parent;

			int NameIndex;
			FAllocationTag? Tag;

			List<FNode> ChildNodes;
			Dictionary<int, FNode> ChildNodesByNameMap;

			List<FNode> ChildTags;
			Dictionary<int, FNode> ChildTagsByNameMap;
		};

		FNode RootNode;

		FNode UntaggedNode;
	};

	public struct FAllocationTag : IEquatable<FAllocationTag>
	{
		public FAllocationTag(string TagStr)
		{
			// Add a name entry for this whole tag
			TagNameIndex = FStreamInfo.GlobalInstance.GetNameIndex(TagStr, true);
		}

		public string Name
		{
			get
			{
				return FStreamInfo.GlobalInstance.NameArray[TagNameIndex];
			}
		}

		public override int GetHashCode()
		{
			return TagNameIndex;
		}

		public override bool Equals(object Other)
		{
			return Other is FAllocationTag && ((FAllocationTag)Other).TagNameIndex == TagNameIndex;
		}

		public bool Equals(FAllocationTag Other)
		{
			return Other.TagNameIndex == TagNameIndex;
		}

		public static bool operator==(FAllocationTag LHS, FAllocationTag RHS)
		{
			return LHS.TagNameIndex == RHS.TagNameIndex;
		}

		public static bool operator!=(FAllocationTag LHS, FAllocationTag RHS)
		{
			return LHS.TagNameIndex != RHS.TagNameIndex;
		}

		public readonly int TagNameIndex;
	};

	public class FAllocationTags
	{
		public FAllocationTags(string TagsStr)
		{
			var SplitTags = TagsStr.Split(TagSeparators, StringSplitOptions.RemoveEmptyEntries);

			Tags = new FAllocationTag[SplitTags.Length];
			for (int TagIndex = 0; TagIndex < SplitTags.Length; ++TagIndex)
			{
				Tags[TagIndex] = new FAllocationTag(SplitTags[TagIndex]);
				FStreamInfo.GlobalInstance.TagHierarchy.RegisterTag(SplitTags[TagIndex], Tags[TagIndex]);
			}
		}

		public readonly FAllocationTag[] Tags;

		private static char[] TagSeparators = { ';' };
	}
}
