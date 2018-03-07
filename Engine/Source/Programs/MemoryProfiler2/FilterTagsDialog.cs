// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace MemoryProfiler2
{
	public partial class FilterTagsDialog : Form
	{
		public FilterTagsDialog()
		{
			InitializeComponent();

			FilterTagsTreeView.TreeViewNodeSorter = null; // Data is pre-sorted
			FilterTagsTreeView.CheckBoxes = true;

			FilterTagsSearch.TextChanged += FilterTagsSearch_TextChanged;
			FilterTagsSearch.KeyDown += FilterTagsSearch_KeyDown;
			FilterTagsSearch.LostFocus += FilterTagsSearch_LostFocus;

			ResetFilterButton.Click += ResetFilterButton_Click;

			ActiveTags = new HashSet<FAllocationTag>();

			FilterTextTimer = new Timer();
			FilterTextTimer.Interval = 1000;
			FilterTextTimer.Tick += FilterTextTimer_Tick;
		}

		public HashSet<FAllocationTag> GetActiveTags()
		{
			return new HashSet<FAllocationTag>(ActiveTags);
		}

		public void PopulateTagFilterHierarchy()
		{
			FilterTagsSearch.Text = String.Empty;
			LastFilterText = String.Empty;
			PopulateTagFilterHierarchy(null);
		}

		private void PopulateTagFilterHierarchy(string[] InFilterTerms)
		{
			UseWaitCursor = true;

			// Build the internal tree
			{
				InternalTreeRoot = new FilterTagsTreeNode(null);

				var RootNode = FStreamInfo.GlobalInstance.TagHierarchy.GetRootNode();
				BuildInternalTree(InFilterTerms, RootNode, InternalTreeRoot);
			}

			// Build the WinForm tree
			if (FilterTagsTreeView.Handle != null) // This if statement exists to force the native tree view handle to be created
			{
				bool bIsFiltering = InFilterTerms != null && InFilterTerms.Length > 0;

				FilterTagsTreeView.BeginUpdate();

				FilterTagsTreeView.AfterCheck -= FilterTagsTreeView_AfterCheck;
				FilterTagsTreeView.BeforeExpand -= FilterTagsTreeView_BeforeExpand;

				FilterTagsTreeView.Nodes.Clear();
				BuildFilterTagsTree(InternalTreeRoot, FilterTagsTreeView.Nodes, bIsFiltering);
				UpdateCheckboxState(FilterTagsTreeView.Nodes);

				if (bIsFiltering)
				{
					FilterTagsTreeView.ExpandAll();
				}

				FilterTagsTreeView.AfterCheck += FilterTagsTreeView_AfterCheck;
				FilterTagsTreeView.BeforeExpand += FilterTagsTreeView_BeforeExpand;

				FilterTagsTreeView.EndUpdate();
			}

			UseWaitCursor = false;
		}

		private bool BuildInternalTree(string[] InFilterTerms, FAllocationTagHierarchy.INode InParentAllocTagNode, FilterTagsTreeNode InParentInternalNode)
		{
			bool bIsFiltering = InFilterTerms != null && InFilterTerms.Length > 0;

			foreach (var ChildAllocTagNode in InParentAllocTagNode.GetChildNodes())
			{
				var ChildInternalNode = new FilterTagsTreeNode(null, ChildAllocTagNode); // Don't set the parent yet as we might not add it to the tree
				if (BuildInternalTree(InFilterTerms, ChildAllocTagNode, ChildInternalNode))
				{
					// Link the node now
					ChildInternalNode.Parent = InParentInternalNode;
					InParentInternalNode.Children.Add(ChildInternalNode);
				}
			}

			foreach (var ChildAllocTag in InParentAllocTagNode.GetChildTags())
			{
				var AllocTag = ChildAllocTag.GetTag().Value;
				bool bChildTagNamePassesFilter = true;

				if (bIsFiltering)
				{
					var FullTagName = AllocTag.Name.ToLower();
					foreach (var FilterTerm in InFilterTerms)
					{
						if (!FullTagName.Contains(FilterTerm))
						{
							bChildTagNamePassesFilter = false;
							break;
						}
					}
				}

				if (bChildTagNamePassesFilter)
				{
					var ChildInternalNode = new FilterTagsTreeNode(InParentInternalNode, ChildAllocTag);
				}
			}

			return InParentInternalNode.Children.Count > 0;
		}

		private void BuildFilterTagsTree(FilterTagsTreeNode InParentInternalNode, TreeNodeCollection InTreeNodesList, bool bRecursive)
		{
			foreach (var ChildInternalNode in InParentInternalNode.Children)
			{
				var ChildTreeNode = new TreeNode(ChildInternalNode.AllocTagNode.GetNodeName());
				ChildTreeNode.Tag = ChildInternalNode;

				if (bRecursive)
				{
					BuildFilterTagsTree(ChildInternalNode, ChildTreeNode.Nodes, bRecursive);
				}
				else
				{
					// Add a dummy child node if needed so that we still get an expansion arrow
					if (ChildInternalNode.Children.Count > 0)
					{
						ChildTreeNode.Nodes.Add(new TreeNode());
					}
				}

				InTreeNodesList.Add(ChildTreeNode);
			}
		}

		private void UpdateCheckboxState(TreeNodeCollection InTreeNodesList)
		{
			foreach (TreeNode TreeNode in InTreeNodesList)
			{
				var InternalNode = TreeNode.Tag as FilterTagsTreeNode;
				if (InternalNode != null)
				{
					if (InternalNode.AllocTagNode.GetTag().HasValue)
					{
						TreeNode.Checked = ActiveTags.Contains(InternalNode.AllocTagNode.GetTag().Value);
					}
					else
					{
						// Hide the checkbox on non-tag nodes
						FilterTagsTreeView.SetCheckBoxImageState(TreeNode, 0);
					}
				}

				UpdateCheckboxState(TreeNode.Nodes);
			}
		}

		private void ApplyTextFilter()
		{
			FilterTextTimer.Stop();

			if (LastFilterText == null || LastFilterText != FilterTagsSearch.Text)
			{
				LastFilterText = FilterTagsSearch.Text;

				var FilterTerms = FilterTagsSearch.Text.ToLower().Split(FilterTermSeparatorCharacters, StringSplitOptions.RemoveEmptyEntries);
				PopulateTagFilterHierarchy(FilterTerms);
			}
		}

		private void FilterTagsSearch_TextChanged(object sender, System.EventArgs e)
		{
			FilterTextTimer.Stop();
			FilterTextTimer.Start();
		}

		private void FilterTagsSearch_KeyDown(object sender, KeyEventArgs e)
		{
			if (e.KeyCode == Keys.Enter)
			{
				ApplyTextFilter();
			}
		}

		private void FilterTagsSearch_LostFocus(object sender, EventArgs e)
		{
			ApplyTextFilter();
		}

		private void FilterTagsTreeView_AfterCheck(object sender, TreeViewEventArgs e)
		{
			var InternalNode = e.Node.Tag as FilterTagsTreeNode;
			if (InternalNode != null)
			{
				if (e.Node.Checked)
				{
					ActiveTags.Add(InternalNode.AllocTagNode.GetTag().Value);
				}
				else
				{
					ActiveTags.Remove(InternalNode.AllocTagNode.GetTag().Value);
				}
			}
		}

		private void FilterTagsTreeView_BeforeExpand(object sender, TreeViewCancelEventArgs e)
		{
			if (e.Node.Nodes.Count == 1 && e.Node.Nodes[0].Tag == null)
			{
				// Replace the dummy child with real children now that we're being expanded
				e.Node.TreeView.BeginUpdate();
				e.Node.Nodes.Clear();
				BuildFilterTagsTree((FilterTagsTreeNode)e.Node.Tag, e.Node.Nodes, false);
				UpdateCheckboxState(e.Node.Nodes);
				e.Node.TreeView.EndUpdate();
			}
		}

		private void ResetFilterButton_Click(object sender, EventArgs e)
		{
			ActiveTags.Clear();
			FilterTagsSearch.Text = String.Empty;
			PopulateTagFilterHierarchy(null);
		}

		private void FilterTextTimer_Tick(object sender, EventArgs e)
		{
			ApplyTextFilter();
		}

		private FilterTagsTreeNode InternalTreeRoot = null;

		private HashSet<FAllocationTag> ActiveTags;

		private Timer FilterTextTimer = null;

		private string LastFilterText = null;

		private static char[] FilterTermSeparatorCharacters = { ' ' };
	}

	class FilterTagsTreeNode
	{
		public FilterTagsTreeNode Parent = null;

		public List<FilterTagsTreeNode> Children = new List<FilterTagsTreeNode>();

		public FAllocationTagHierarchy.INode AllocTagNode;

		public FilterTagsTreeNode(FAllocationTagHierarchy.INode InAllocTagNode)
		{
			AllocTagNode = InAllocTagNode;
		}

		public FilterTagsTreeNode(FilterTagsTreeNode InParent, FAllocationTagHierarchy.INode InAllocTagNode)
		{
			Parent = InParent;
			if (Parent != null)
			{
				Parent.Children.Add(this);
			}

			AllocTagNode = InAllocTagNode;
		}
	}
}
