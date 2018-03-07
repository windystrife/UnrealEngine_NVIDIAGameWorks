// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Windows.Forms.VisualStyles;

namespace UnrealGameSync
{
	partial class CleanWorkspaceWindow : Form
	{
		class TreeNodeData
		{
			public FileInfo File;
			public FolderToClean Folder;
			public int NumFiles;
			public int NumSelectedFiles;
			public int NumEmptySelectedFiles;
			public int NumDefaultSelectedFiles;
		}

		enum SelectionType
		{
			All,
			SafeToDelete,
			Empty,
			None,
		}

		static readonly CheckBoxState[] CheckBoxStates = 
		{ 
			CheckBoxState.UncheckedNormal, 
			CheckBoxState.MixedNormal, 
			CheckBoxState.CheckedNormal 
		};

		FolderToClean RootFolderToClean;

		static readonly string[] SafeToDeleteFolders =
		{
			"/binaries/",
			"/intermediate/",
			"/build/receipts/",
			"/.vs/",
			"/automationtool/saved/rules/",
			"/saved/logs/",
	
			"/bin/debug/",
			"/bin/development/",
			"/bin/release/",
			"/obj/debug/",
			"/obj/development/",
			"/obj/release/",

			"/bin/x86/debug/",
			"/bin/x86/development/",
			"/bin/x86/release/",
			"/obj/x86/debug/",
			"/obj/x86/development/",
			"/obj/x86/release/",

			"/bin/x64/debug/",
			"/bin/x64/development/",
			"/bin/x64/release/",
			"/obj/x64/debug/",
			"/obj/x64/development/",
			"/obj/x64/release/",
		};

		static readonly string[] SafeToDeleteExtensions =
		{
			".pdb",
			".obj",
			".sdf",
			".suo",
			".sln",
			".csproj.user",
			".csproj.references",
		};

		[DllImport("Shell32.dll", EntryPoint = "ExtractIconExW", CharSet = CharSet.Unicode, ExactSpelling = true, CallingConvention = CallingConvention.StdCall)]
		private static extern int ExtractIconEx(string sFile, int iIndex, IntPtr piLargeVersion, out IntPtr piSmallVersion, int amountIcons);

		private CleanWorkspaceWindow(FolderToClean InRootFolderToClean)
		{
			RootFolderToClean = InRootFolderToClean;
			InitializeComponent();
		}

		public static void DoClean(IWin32Window Owner, PerforceConnection PerforceClient, string LocalRootPath, string ClientRootPath, IReadOnlyList<string> SyncPaths, TextWriter Log)
		{
			// Figure out which folders to clean
			FolderToClean RootFolderToClean = new FolderToClean(new DirectoryInfo(LocalRootPath));
			using(FindFoldersToCleanTask QueryWorkspace = new FindFoldersToCleanTask(PerforceClient, RootFolderToClean, ClientRootPath, SyncPaths, Log))
			{
				string ErrorMessage;
				if(!ModalTaskWindow.Execute(Owner, QueryWorkspace, "Clean Workspace", "Querying files in Perforce, please wait...", out ErrorMessage))
				{
					if(!String.IsNullOrEmpty(ErrorMessage))
					{
						MessageBox.Show(ErrorMessage);
					}
					return;
				}
			}

			// If there's nothing to delete, don't bother displaying the dialog at all
			if(RootFolderToClean.FilesToClean.Count == 0 && RootFolderToClean.SubFolders.Count == 0)
			{
				MessageBox.Show("You have no local files which are not in Perforce.", "Workspace Clean", MessageBoxButtons.OK);
				return;
			}

			// Populate the tree
			CleanWorkspaceWindow CleanWorkspace = new CleanWorkspaceWindow(RootFolderToClean);
			CleanWorkspace.ShowDialog();
		}

		private void CleanWorkspaceWindow_Load(object sender, EventArgs e)
		{
			IntPtr FolderIconPtr;
			ExtractIconEx("imageres.dll", 3, IntPtr.Zero, out FolderIconPtr, 1);

			IntPtr FileIconPtr;
			ExtractIconEx("imageres.dll", 2, IntPtr.Zero, out FileIconPtr, 1);

			Icon[] Icons = new Icon[]{ Icon.FromHandle(FolderIconPtr), Icon.FromHandle(FileIconPtr) };

			Bitmap TypeImageListBitmap = new Bitmap(Icons.Length * 3 * 32, 16, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
			using(Graphics Graphics = Graphics.FromImage(TypeImageListBitmap))
			{
				int MinX = 0;
				for(int IconIdx = 0; IconIdx < Icons.Length; IconIdx++)
				{
					for(int StateIdx = 0; StateIdx < 3; StateIdx++)
					{
						Size CheckBoxSize = CheckBoxRenderer.GetGlyphSize(Graphics, CheckBoxStates[StateIdx]);
						CheckBoxRenderer.DrawCheckBox(Graphics, new Point(MinX + (16 - CheckBoxSize.Width) / 2, (16 - CheckBoxSize.Height) / 2), CheckBoxStates[StateIdx]);
						Graphics.DrawIcon(Icons[IconIdx], MinX + 16, 0);
						MinX += 32;
					}
				}
			}

			ImageList TypeImageList = new ImageList();
			TypeImageList.ImageSize = new Size(32, 16);
			TypeImageList.ColorDepth = ColorDepth.Depth32Bit;
			TypeImageList.Images.AddStrip(TypeImageListBitmap);
			TreeView.ImageList = TypeImageList;

			TreeNode Node = BuildTreeViewStructure(RootFolderToClean, "/", false, 0);
			Node.Text = RootFolderToClean.Directory.FullName;
			TreeView.Nodes.Add(Node);
		}

		private TreeNode BuildTreeViewStructure(FolderToClean Folder, string FolderPath, bool bParentFolderSelected, int Depth)
		{
			bool bSelectFolder = bParentFolderSelected || IsSafeToDeleteFolder(FolderPath) || Folder.bEmptyLeaf;

			TreeNodeData FolderNodeData = new TreeNodeData();
			FolderNodeData.Folder = Folder;

			TreeNode FolderNode = new TreeNode();
			FolderNode.Text = Folder.Name;
			FolderNode.Tag = FolderNodeData;

			foreach(FolderToClean SubFolder in Folder.SubFolders.OrderBy(x => x.Name))
			{
				TreeNode ChildNode = BuildTreeViewStructure(SubFolder, FolderPath + SubFolder.Name.ToLowerInvariant() + "/", bSelectFolder, Depth + 1);
				FolderNode.Nodes.Add(ChildNode);

				TreeNodeData ChildNodeData = (TreeNodeData)ChildNode.Tag;
				FolderNodeData.NumFiles += ChildNodeData.NumFiles;
				FolderNodeData.NumSelectedFiles += ChildNodeData.NumSelectedFiles;
				FolderNodeData.NumEmptySelectedFiles += ChildNodeData.NumEmptySelectedFiles;
				FolderNodeData.NumDefaultSelectedFiles += ChildNodeData.NumDefaultSelectedFiles;
			}

			foreach(FileInfo File in Folder.FilesToClean.OrderBy(x => x.Name))
			{
				string Name = File.Name.ToLowerInvariant();

				bool bSelectFile = bSelectFolder || IsSafeToDeleteFile(FolderPath, File.Name.ToLowerInvariant());

				TreeNodeData FileNodeData = new TreeNodeData();
				FileNodeData.File = File;
				FileNodeData.NumFiles = 1;
				FileNodeData.NumSelectedFiles = bSelectFile? 1 : 0;
				FileNodeData.NumEmptySelectedFiles = 0;
				FileNodeData.NumDefaultSelectedFiles = FileNodeData.NumSelectedFiles;

				TreeNode FileNode = new TreeNode();
				FileNode.Text = File.Name;
				FileNode.Tag = FileNodeData;
				FolderNode.Nodes.Add(FileNode);

				UpdateImage(FileNode);

				FolderNodeData.NumFiles++;
				FolderNodeData.NumSelectedFiles += FileNodeData.NumSelectedFiles;
				FolderNodeData.NumEmptySelectedFiles += FileNodeData.NumEmptySelectedFiles;
				FolderNodeData.NumDefaultSelectedFiles += FileNodeData.NumDefaultSelectedFiles;
			}

			if(FolderNodeData.Folder.bEmptyLeaf)
			{
				FolderNodeData.NumFiles++;
				FolderNodeData.NumSelectedFiles++;
				FolderNodeData.NumEmptySelectedFiles++;
				FolderNodeData.NumDefaultSelectedFiles++;
			}

			if(FolderNodeData.NumSelectedFiles > 0 && !FolderNodeData.Folder.bEmptyAfterDelete && Depth < 2)
			{
				FolderNode.Expand();
			}
			else
			{
				FolderNode.Collapse();
			}

			UpdateImage(FolderNode);
			return FolderNode;
		}

		private bool IsSafeToDeleteFolder(string FolderPath)
		{
			return SafeToDeleteFolders.Any(x => FolderPath.EndsWith(x));
		}

		private bool IsSafeToDeleteFile(string FolderPath, string Name)
		{
			return SafeToDeleteExtensions.Any(x => Name.EndsWith(x));
		}

		private void TreeView_DrawNode(object sender, DrawTreeNodeEventArgs e)
		{
			e.Graphics.DrawLine(Pens.Black, new Point(e.Bounds.Left, e.Bounds.Top), new Point(e.Bounds.Right, e.Bounds.Bottom));
		}

		private void TreeView_NodeMouseClick(object sender, TreeNodeMouseClickEventArgs e)
		{
			TreeNode Node = e.Node;
			if(e.Button == System.Windows.Forms.MouseButtons.Right)
			{
				TreeView.SelectedNode = e.Node;
				FolderContextMenu.Tag = e.Node;
				FolderContextMenu.Show(TreeView.PointToScreen(e.Location));
			}
			else if(e.X >= e.Node.Bounds.Left - 32 && e.X < e.Node.Bounds.Left - 16)
			{
				TreeNodeData NodeData = (TreeNodeData)Node.Tag;
				SetSelected(Node, (NodeData.NumSelectedFiles == 0)? SelectionType.All : SelectionType.None);
			}
		}

		private void SetSelected(TreeNode ParentNode, SelectionType Type)
		{
			TreeNodeData ParentNodeData = (TreeNodeData)ParentNode.Tag;

			int PrevNumSelectedFiles = ParentNodeData.NumSelectedFiles;
			SetSelectedOnChildren(ParentNode, Type);

			int DeltaNumSelectedFiles = ParentNodeData.NumSelectedFiles - PrevNumSelectedFiles;
			if(DeltaNumSelectedFiles != 0)
			{
				for(TreeNode NextParentNode = ParentNode.Parent; NextParentNode != null; NextParentNode = NextParentNode.Parent)
				{
					TreeNodeData NextParentNodeData = (TreeNodeData)NextParentNode.Tag;
					NextParentNodeData.NumSelectedFiles += DeltaNumSelectedFiles;
					UpdateImage(NextParentNode);
				}
			}
		}

		private void SetSelectedOnChildren(TreeNode ParentNode, SelectionType Type)
		{
			TreeNodeData ParentNodeData = (TreeNodeData)ParentNode.Tag;

			int NewNumSelectedFiles = 0;
			switch(Type)
			{
				case SelectionType.All:
					NewNumSelectedFiles = ParentNodeData.NumFiles;
					break;
				case SelectionType.Empty:
					NewNumSelectedFiles = ParentNodeData.NumEmptySelectedFiles;
					break;
				case SelectionType.SafeToDelete:
					NewNumSelectedFiles = ParentNodeData.NumDefaultSelectedFiles;
					break;
				case SelectionType.None:
					NewNumSelectedFiles = 0;
					break;
			}

			if(NewNumSelectedFiles != ParentNodeData.NumSelectedFiles)
			{
				foreach(TreeNode ChildNode in ParentNode.Nodes)
				{
					SetSelectedOnChildren(ChildNode, Type);
				}
				ParentNodeData.NumSelectedFiles = NewNumSelectedFiles;
				UpdateImage(ParentNode);
			}
		}

		private void UpdateImage(TreeNode Node)
		{
			TreeNodeData NodeData = (TreeNodeData)Node.Tag;
			int ImageIndex = (NodeData.Folder != null)? 0 : 3;
			ImageIndex += (NodeData.NumSelectedFiles == 0)? 0 : (NodeData.NumSelectedFiles < NodeData.NumFiles || (NodeData.Folder != null && !NodeData.Folder.bEmptyAfterDelete))? 1 : 2;
			Node.ImageIndex = ImageIndex;
			Node.SelectedImageIndex = ImageIndex;
		}

		private void DeleteBtn_Click(object sender, EventArgs e)
		{
			List<FileInfo> Files = new List<FileInfo>();
			List<DirectoryInfo> Directories = new List<DirectoryInfo>();
			foreach(TreeNode RootNode in TreeView.Nodes)
			{
				FindSelection(RootNode, Files, Directories);
			}

			string ErrorMessage;
			if(!ModalTaskWindow.Execute(this, new DeleteFilesTask(Files, Directories), "Clean Workspace", "Deleting files, please wait...", out ErrorMessage) && !String.IsNullOrEmpty(ErrorMessage))
			{
				FailedToDeleteWindow FailedToDelete = new FailedToDeleteWindow();
				FailedToDelete.FileList.Text = ErrorMessage;
				FailedToDelete.FileList.SelectionStart = 0;
				FailedToDelete.FileList.SelectionLength = 0;
				FailedToDelete.ShowDialog();
			}
		}

		private void FindSelection(TreeNode Node, List<FileInfo> Files, List<DirectoryInfo> Directories)
		{
			TreeNodeData NodeData = (TreeNodeData)Node.Tag;
			if(NodeData.File != null)
			{
				if(NodeData.NumSelectedFiles > 0)
				{
					Files.Add(NodeData.File);
				}
			}
			else
			{
				foreach(TreeNode ChildNode in Node.Nodes)
				{
					FindSelection(ChildNode, Files, Directories);
				}
				if(NodeData.Folder != null && NodeData.Folder.bEmptyAfterDelete && NodeData.NumSelectedFiles == NodeData.NumFiles)
				{
					Directories.Add(NodeData.Folder.Directory);
				}
			}
		}

		private void SelectAllBtn_Click(object sender, EventArgs e)
		{
			foreach(TreeNode Node in TreeView.Nodes)
			{
				SetSelected(Node, SelectionType.All);
			}
		}

		private void FolderContextMenu_SelectAll_Click(object sender, EventArgs e)
		{
			TreeNode Node = (TreeNode)FolderContextMenu.Tag;
			SetSelected(Node, SelectionType.All);
		}

		private void FolderContextMenu_SelectSafeToDelete_Click(object sender, EventArgs e)
		{
			TreeNode Node = (TreeNode)FolderContextMenu.Tag;
			SetSelected(Node, SelectionType.SafeToDelete);
		}

		private void FolderContextMenu_SelectEmptyFolder_Click(object sender, EventArgs e)
		{
			TreeNode Node = (TreeNode)FolderContextMenu.Tag;
			SetSelected(Node, SelectionType.Empty);
		}

		private void FolderContextMenu_SelectNone_Click(object sender, EventArgs e)
		{
			TreeNode Node = (TreeNode)FolderContextMenu.Tag;
			SetSelected(Node, SelectionType.None);
		}

		private void FolderContextMenu_OpenWithExplorer_Click(object sender, EventArgs e)
		{
			TreeNode Node = (TreeNode)FolderContextMenu.Tag;
			TreeNodeData NodeData = (TreeNodeData)Node.Tag;
			Process.Start("explorer.exe", String.Format("\"{0}\"", NodeData.Folder.Directory.FullName));
		}
	}
}
