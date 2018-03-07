// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Windows.Forms;
using System.Diagnostics;
using System.Drawing;
using System.Text.RegularExpressions;

namespace MemoryProfiler2 
{
	/// <summary> Class parsing snapshot into a tree from a given root node. </summary>
	public static class FCallGraphTreeViewParser
	{
		/// <summary> Parent address to use when creating call graph. </summary>
		public static int ParentFunctionIndex;

		/// <summary> Reference to the main memory profiler window. </summary>
		private static MainWindow OwnerWindow;

		public static void SetProfilerWindow( MainWindow InMainWindow )
		{
			OwnerWindow = InMainWindow;
		}

		public static void ParseSnapshot( TreeView CallGraphTreeView, List<FCallStackAllocationInfo> CallStackList, bool bShouldSortBySize, string FilterText, bool bInvertCallStacks )
		{
			// Progress bar.
			OwnerWindow.ToolStripProgressBar.Value = 0;
			OwnerWindow.ToolStripProgressBar.Visible = true;
			long ProgressInterval = CallStackList.Count / 20;
			long NextProgressUpdate = ProgressInterval;
			int CallStackCurrent = 0;
			
			OwnerWindow.UpdateStatus( "Updating call graph for " + OwnerWindow.CurrentFilename );

			CallGraphTreeView.BeginUpdate();

			CallGraphTreeView.TreeViewNodeSorter = null; // clear this to avoid a Sort for each call to Add

			Debug.WriteLine("FCallGraphTreeViewParser.ParseSnapshot - Building call graph tree for " + OwnerWindow.CurrentFilename);

			var TruncatedNode = new FCallGraphNode("Truncated Callstacks");
			var RegularNode = new FCallGraphNode("Full Callstacks");

			bool bFilterIn = OwnerWindow.IsFilteringIn();

			using ( FScopedLogTimer ParseTiming = new FScopedLogTimer( "FCallGraphTreeViewParser.ParseSnapshot" ) )
			{
				var FilteredCallstackList = new List<FCallStackAllocationInfo>(CallStackList.Count);
				foreach (var AllocationInfo in CallStackList)
				{
					var FilteredAllocationInfo = AllocationInfo.GetAllocationInfoForTags(OwnerWindow.GetTagsFilter(), bFilterIn);
					if (FilteredAllocationInfo.TotalCount != 0)
					{
						FilteredCallstackList.Add(FilteredAllocationInfo);
					}
				}

				// Iterate over all call graph paths and add them to the graph.
				foreach ( FCallStackAllocationInfo AllocationInfo in FilteredCallstackList )
				{
					// Update progress bar.
					if( CallStackCurrent >= NextProgressUpdate )
					{
						OwnerWindow.ToolStripProgressBar.PerformStep();
						NextProgressUpdate += ProgressInterval;
						Debug.WriteLine( "FCallGraphTreeViewParser.ParseSnapshot " + OwnerWindow.ToolStripProgressBar.Value + "/20" );
					}
					CallStackCurrent++;

					// Add this call graph to the tree view.
					FCallStack CallStack = FStreamInfo.GlobalInstance.CallStackArray[ AllocationInfo.CallStackIndex ];
					// Split the tree into full and truncated callstacks.
					var RootNode = CallStack.bIsTruncated ? TruncatedNode : RegularNode;
					// Apply filter based on text representation of address.
					if( CallStack.RunFilters( FilterText, OwnerWindow.Options.ClassGroups, bFilterIn, OwnerWindow.SelectedMemoryPool ) )
					{
						// Add call stack to proper part of graph.
						AddCallStackToGraph( RootNode, CallStack, AllocationInfo, ParentFunctionIndex, bInvertCallStacks );
					}
				}
			}

			Debug.WriteLine("FCallGraphTreeViewParser.ParseSnapshot - Sorting call graph tree for " + OwnerWindow.CurrentFilename);

			// Sort the nodes before adding them to the tree (sorting when in the tree is slow!).
			SortNodes(TruncatedNode, bShouldSortBySize);
			SortNodes(RegularNode, bShouldSortBySize);

			Debug.WriteLine("FCallGraphTreeViewParser.ParseSnapshot - Populating call graph tree for " + OwnerWindow.CurrentFilename);

			// Clear out existing nodes and add two root nodes. One for regular call stacks and one for truncated ones.
			CallGraphTreeView.Nodes.Clear();
			CallGraphTreeView.Tag = new FCallGraphTreeViewTag();
			AddRootTreeNode(TruncatedNode, CallGraphTreeView);
			AddRootTreeNode(RegularNode, CallGraphTreeView);

			CallGraphTreeView.BeforeExpand -= HandlePreExpandTreeNode;
			CallGraphTreeView.BeforeExpand += HandlePreExpandTreeNode;

			CallGraphTreeView.EndUpdate();

			OwnerWindow.ToolStripProgressBar.Visible = false;
		}

		private static void AddCallStackToGraph( FCallGraphNode RootNode, FCallStack CallStack, FCallStackAllocationInfo AllocationInfo, int ParentFunctionIndex, bool bInvertCallStacks )
		{
			// Used to determine whether it is okay to add address to the graph. An index of -1 means we don't care about parenting.
			bool bAllowNodeUpdate = ParentFunctionIndex == -1;
			// Iterate over each address and add it to the tree view.
			FCallGraphNode CurrentNode = RootNode;
			for( int AdressIndex = 0; AdressIndex < CallStack.AddressIndices.Count; AdressIndex++ )
			{
				int AddressIndex;
				if( bInvertCallStacks )
				{
					AddressIndex = CallStack.AddressIndices[ CallStack.AddressIndices.Count - 1 - AdressIndex ];
				}
				else
				{
					AddressIndex = CallStack.AddressIndices[ AdressIndex ];
				}

				// Filter based on function if wanted. This means we only include callstacks that contain the function
				// and we also reparent the callstack to start at the first occurence of the function.
				if( ParentFunctionIndex != -1 && !bAllowNodeUpdate )
				{
					bAllowNodeUpdate = FStreamInfo.GlobalInstance.CallStackAddressArray[ AddressIndex ].FunctionIndex == ParentFunctionIndex;
				}

				if( bAllowNodeUpdate )
				{
					// Update the node for this address. The return value of the function will be the new parent node.
					CurrentNode = UpdateNodeAndPayload( CurrentNode, AddressIndex, AllocationInfo );
				}
			}
		}

		private static FCallGraphNode UpdateNodeAndPayload( FCallGraphNode ParentNode, int AddressIndex, FCallStackAllocationInfo AllocationInfo )
		{
			int FunctionIndex = FStreamInfo.GlobalInstance.CallStackAddressArray[AddressIndex].FunctionIndex;
			FCallStack CallStack = FStreamInfo.GlobalInstance.CallStackArray[AllocationInfo.CallStackIndex];

			var TotalAllocationSize = AllocationInfo.TotalSize;
			var TotalAllocationCount = AllocationInfo.TotalCount;

			// Iterate over existing nodes to see whether there is a match.
			foreach (FCallGraphNode Node in ParentNode.Children)
			{
				// If there is a match, update the allocation size and return the current node.
				if (FStreamInfo.GlobalInstance.CallStackAddressArray[Node.AddressIndex].FunctionIndex == FunctionIndex)
				{
					Node.AllocationSize += TotalAllocationSize;
					Node.AllocationCount += TotalAllocationCount;
					Node.CallStacks.Add(CallStack);

					// Return current node as parent for next iteration.
					return Node;
				}
			}

			// If we made it here it means that we need to add a new node.
			string FunctionName = FStreamInfo.GlobalInstance.NameArray[FunctionIndex];
			FCallGraphNode NewNode = new FCallGraphNode(ParentNode, AddressIndex, TotalAllocationSize, TotalAllocationCount, FunctionName, CallStack);
			return NewNode;
		}

		private static void AddRootTreeNode(FCallGraphNode RootNode, TreeView TreeView)
		{
			TreeNode RootTreeNode = new TreeNode(RootNode.FunctionName);
			RootTreeNode.Tag = RootNode;
			RootNode.TreeNode = RootTreeNode;
			TreeView.Nodes.Add(RootTreeNode);

			var CallGraphTreeViewTag = (FCallGraphTreeViewTag)TreeView.Tag;
			CallGraphTreeViewTag.RootNodes.Add(RootNode);

			if (RootNode.Children.Count > 0)
			{
				RootTreeNode.Nodes.Add(new TreeNode());
			}
		}

		private static void EnsureTreeNode(FCallGraphNode Node)
		{
			var NodesToCreate = new List<FCallGraphNode>();
			{
				FCallGraphNode WorkingNode = Node;
				while (WorkingNode.TreeNode == null)
				{
					NodesToCreate.Add(WorkingNode);
					WorkingNode = WorkingNode.Parent;
				}
				NodesToCreate.Add(WorkingNode);
			}

			NodesToCreate.Reverse();
			foreach (FCallGraphNode NodeToCreate in NodesToCreate)
			{
				EnsureTreeNodeHasValidChildren(NodeToCreate, NodeToCreate.TreeNode);
			}
		}

		private static void EnsureTreeNodeHasValidChildren(FCallGraphNode ParentNode, TreeNode ParentTreeNode)
		{
			// If this entry has a single dummy node, then we need to insert its real children
			if (ParentTreeNode.Nodes.Count == 1 && ParentTreeNode.Nodes[0].Tag == null)
			{
				ParentTreeNode.Nodes.Clear();

				foreach (FCallGraphNode Node in ParentNode.Children)
				{
					string SizeString = MainWindow.FormatSizeString(Node.AllocationSize);
					string NodeName = String.Format("{0}  {1} {2}  {3}", SizeString, Node.AllocationCount.ToString("N0"), Node.AllocationCount == 1 ? "allocation" : "allocations", Node.FunctionName);

					TreeNode TreeNode = new TreeNode(NodeName);
					TreeNode.Tag = Node;
					Node.TreeNode = TreeNode;
					ParentTreeNode.Nodes.Add(TreeNode);

					if (Node.Children.Count > 0)
					{
						TreeNode.Nodes.Add(new TreeNode());
					}
				}
			}
		}

		private static void SortNodes(FCallGraphNode ParentNode, bool bShouldSortBySize)
		{
			if (bShouldSortBySize)
			{
				ParentNode.SortChildrenByAllocationSize();
			}
			else
			{
				ParentNode.SortChildrenByAllocationCount();
			}

			foreach (FCallGraphNode Node in ParentNode.Children)
			{
				SortNodes(Node, bShouldSortBySize);
			}
		}

		private static void HandlePreExpandTreeNode(object sender, TreeViewCancelEventArgs e)
		{
			e.Node.TreeView.BeginUpdate();
			EnsureTreeNodeHasValidChildren((FCallGraphNode)e.Node.Tag, e.Node);
			e.Node.TreeView.EndUpdate();
		}

		public static TreeNode FindNode( TreeView CallGraphTreeView, string InText, bool bTextIsRegex, bool bInMatchCase )
		{
			var SearchParams = new FNodeSearchParams(InText, bTextIsRegex, bInMatchCase);

			TreeNode SelectedTreeNode = CallGraphTreeView.SelectedNode;

			FCallGraphNode FoundNode = FindRecursiveChildNode((FCallGraphNode)SelectedTreeNode.Tag, SearchParams);
			while (SelectedTreeNode != null && FoundNode == null)
			{
				var Siblings = SelectedTreeNode.Parent != null ? ((FCallGraphNode)SelectedTreeNode.Parent.Tag).Children : ((FCallGraphTreeViewTag)CallGraphTreeView.Tag).RootNodes;
				FoundNode = FindRecursiveSiblingNode(Siblings, (FCallGraphNode)SelectedTreeNode.Tag, SearchParams);
				SelectedTreeNode = SelectedTreeNode.Parent;
			}

			if (FoundNode != null)
			{
				CallGraphTreeView.BeginUpdate();
				EnsureTreeNode(FoundNode);
				CallGraphTreeView.EndUpdate();

				return FoundNode.TreeNode;
			}

			return null;
		}

		private static FCallGraphNode FindRecursiveSiblingNode(List<FCallGraphNode> Siblings, FCallGraphNode SelectedNode, FNodeSearchParams SearchParams)
		{
			for (int SiblingIndex = Siblings.IndexOf(SelectedNode) + 1; SiblingIndex < Siblings.Count; ++SiblingIndex)
			{
				FCallGraphNode SiblingNode = Siblings[SiblingIndex];

				if (SearchParams.DoesNodeMatchSearch(SiblingNode))
				{
					return SiblingNode;
				}

				FCallGraphNode FoundNode = FindRecursiveChildNode(SiblingNode, SearchParams);
				if (FoundNode != null)
				{
					return FoundNode;
				}
			}

			return null;
		}

		private static FCallGraphNode FindRecursiveChildNode(FCallGraphNode SelectedNode, FNodeSearchParams SearchParams)
		{
			foreach (FCallGraphNode ChildNode in SelectedNode.Children)
			{
				if (SearchParams.DoesNodeMatchSearch(ChildNode))
				{
					return ChildNode;
				}

				FCallGraphNode FoundNode = FindRecursiveChildNode(ChildNode, SearchParams);
				if (FoundNode != null)
				{
					return FoundNode;
				}
			}

			return null;
		}

		public static int HighlightAllNodes( TreeView CallGraphTreeView, string InText, bool bTextIsRegex, bool bInMatchCase, out TreeNode FirstResult, out long AllocationSize, out int AllocationCount )
		{
			var SearchParams = new FNodeSearchParams(InText, bTextIsRegex, bInMatchCase);

			var MatchingNodes = new List<FCallGraphNode>();
			var NodeSearchState = new FNodeSearchState();
			foreach (FCallGraphNode RootNode in ((FCallGraphTreeViewTag)CallGraphTreeView.Tag).RootNodes)
			{
				GetAllMatchingNodes(RootNode, MatchingNodes, NodeSearchState, SearchParams);
			}

			CallGraphTreeView.BeginUpdate();
			foreach (FCallGraphNode MatchingNode in MatchingNodes)
			{
				EnsureTreeNode(MatchingNode);

				MatchingNode.TreeNode.BackColor = Color.CornflowerBlue;
				MatchingNode.TreeNode.EnsureVisible();
			}
			CallGraphTreeView.EndUpdate();

			FirstResult = null;
			if (MatchingNodes.Count > 0)
			{
				FirstResult = MatchingNodes[0].TreeNode;
			}

			AllocationSize = NodeSearchState.AllocationSize;
			AllocationCount = NodeSearchState.AllocationCount;

			return MatchingNodes.Count;
		}

		private static void GetAllMatchingNodes(FCallGraphNode ParentNode, List<FCallGraphNode> MatchingNodes, FNodeSearchState SearchState, FNodeSearchParams SearchParams)
		{
			foreach (FCallGraphNode ChildNode in ParentNode.Children)
			{
				if (SearchParams.DoesNodeMatchSearch(ChildNode))
				{
					MatchingNodes.Add(ChildNode);

					if (!SearchState.CallStacks.Contains(ChildNode.CallStacks[0]))
					{
						// If one callstack from this node is new, then all must be, due to the way the graph is arranged and the order in which it is searched
						foreach (FCallStack CallStack in ChildNode.CallStacks)
						{
							SearchState.CallStacks.Add(CallStack);
						}
						SearchState.AllocationSize += ChildNode.AllocationSize;
						SearchState.AllocationCount += ChildNode.AllocationCount;
					}
				}

				GetAllMatchingNodes(ChildNode, MatchingNodes, SearchState, SearchParams);
			}
		}

		public static void UnhighlightAll( TreeView CallGraphTreeView )
		{
			var Worklist = new List<TreeNode>();
			foreach( TreeNode node in CallGraphTreeView.Nodes )
			{
				Worklist.Add( node );
			}

			while( Worklist.Count > 0 )
			{
				TreeNode node = Worklist[ Worklist.Count - 1 ];
				Worklist.RemoveAt( Worklist.Count - 1 );

				node.BackColor = Color.Transparent;

				foreach( TreeNode childNode in node.Nodes )
				{
					Worklist.Add( childNode );
				}
			}
		}

		public static List<FCallStack> GetHighlightedCallStacks( TreeView CallGraphTreeView )
		{
			List<FCallStack> Results = new List<FCallStack>();

			var Worklist = new List<TreeNode>();
			foreach( TreeNode node in CallGraphTreeView.Nodes )
			{
				Worklist.Add( node );
			}

			while( Worklist.Count > 0 )
			{
				TreeNode node = Worklist[ Worklist.Count - 1 ];
				Worklist.RemoveAt( Worklist.Count - 1 );

				if( node.BackColor != Color.Transparent )
				{
					var payload = (FCallGraphNode)node.Tag;
					Results.AddRange( payload.CallStacks );
				}
				else
				{
					// Only search children if this node isn't highlighted.
					// This makes the search go faster, and means that we don't
					// have to worry about duplicate callstacks in the list.
					foreach( TreeNode childNode in node.Nodes )
					{
						Worklist.Add( childNode );
					}
				}
			}

			return Results;
		}

		public static List<string> GetHighlightedNodesAsStrings( TreeView CallGraphTreeView )
		{
			List<string> Results = new List<string>();

			var Worklist = new List<TreeNode>();
			foreach( TreeNode node in CallGraphTreeView.Nodes )
			{
				Worklist.Add( node );
			}

			while( Worklist.Count > 0 )
			{
				TreeNode Node = Worklist[ Worklist.Count - 1 ];
				Worklist.RemoveAt( Worklist.Count - 1 );

				if( Node.BackColor != Color.Transparent )
				{
					var Payload = (FCallGraphNode)Node.Tag;
					if( Payload != null )
					{
						string FunctionName = FStreamInfo.GlobalInstance.NameArray[ FStreamInfo.GlobalInstance.CallStackAddressArray[ Payload.AddressIndex ].FunctionIndex ];
						int AllocationCount = Payload.AllocationCount;
						long AllocationSize = Payload.AllocationSize;
						Results.Add( FunctionName + "," + AllocationCount + "," + AllocationSize );
					}
				}
				else
				{
					// Only search children if this node isn't highlighted.
					// This makes the search go faster, and means that we don't
					// have to worry about duplicate callstacks in the list.
					foreach( TreeNode childNode in Node.Nodes )
					{
						Worklist.Add( childNode );
					}
				}
			}

			return Results;
		}
    }

	/// <summary> TreeNode payload containing raw node stats about address and allocation size. </summary>
	public class FCallGraphNode
	{
		/// <summary> Index into stream info addresses array associated with node. </summary>
		public int AddressIndex;

		/// <summary> Cummulative size for this point in the graph. </summary>
		public long AllocationSize;

		/// <summary> Number of allocations at this point in the graph. </summary>
		public int AllocationCount;

		public string FunctionName;

		public List<FCallStack> CallStacks = new List<FCallStack>();

		public FCallGraphNode Parent;

		public List<FCallGraphNode> Children = new List<FCallGraphNode>();

		public TreeNode TreeNode = null;

		public FCallGraphNode(string InFunctionName)
		{
			Parent = null;
			AddressIndex = 0;
			AllocationSize = 0;
			AllocationCount = 0;
			FunctionName = InFunctionName;
		}

		/// <summary> Constructor, initializing all members with passed in values. </summary>
		public FCallGraphNode(FCallGraphNode InParent, int InAddressIndex, long InAllocationSize, int InAllocationCount, string InFunctionName, FCallStack CallStack)
		{
			Parent = InParent;
			if (Parent != null)
			{
				Parent.Children.Add(this);
			}

			AddressIndex = InAddressIndex;
			AllocationSize = InAllocationSize;
			AllocationCount = InAllocationCount;
			FunctionName = InFunctionName;
			CallStacks.Add(CallStack);
		}

		public void SortChildrenByAllocationSize()
		{
			Children.Sort((First, Second) => Math.Sign(Second.AllocationSize - First.AllocationSize));
		}

		public void SortChildrenByAllocationCount()
		{
			Children.Sort((First, Second) => Math.Sign(Second.AllocationCount - First.AllocationCount));
		}
	}

	public class FCallGraphTreeViewTag
	{
		public List<FCallGraphNode> RootNodes = new List<FCallGraphNode>();
	}

	class FNodeSearchParams
	{
		private static string SearchText;
		private static bool bMatchCase;
		private static Regex SearchRegex;

		public FNodeSearchParams(string InText, bool bTextIsRegex, bool bInMatchCase)
		{
			bMatchCase = bInMatchCase;
			SearchText = InText;

			SearchRegex = null;
			if (bTextIsRegex)
			{
				RegexOptions Options = RegexOptions.Compiled | RegexOptions.CultureInvariant;
				if (!bMatchCase)
				{
					Options |= RegexOptions.IgnoreCase;
				}

				try
				{
					SearchRegex = new Regex(SearchText, Options);
				}
				catch (ArgumentException e)
				{
					MessageBox.Show("Invalid regular expression: " + e.Message);
					throw;
				}
			}
		}

		public bool DoesNodeMatchSearch(FCallGraphNode Node)
		{
			if (SearchRegex != null)
			{
				return SearchRegex.Match(Node.FunctionName).Success;
			}
			else if (bMatchCase)
			{
				return Node.FunctionName.IndexOf(SearchText, StringComparison.InvariantCulture) != -1;
			}
			else
			{
				return Node.FunctionName.IndexOf(SearchText, StringComparison.InvariantCultureIgnoreCase) != -1;
			}
		}
	}

	class FNodeSearchState
    {
        public HashSet<FCallStack> CallStacks = new HashSet<FCallStack>();
        public long AllocationSize;
        public int AllocationCount;

        public FNodeSearchState()
        {
        }
    }
}