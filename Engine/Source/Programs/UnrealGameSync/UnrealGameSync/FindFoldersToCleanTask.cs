// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	class FolderToClean
	{
		public DirectoryInfo Directory;
		public List<FolderToClean> SubFolders = new List<FolderToClean>();
		public List<FileInfo> FilesToClean = new List<FileInfo>();
		public bool bEmptyLeaf = false;
		public bool bEmptyAfterDelete = true;

		public FolderToClean(DirectoryInfo InDirectory)
		{
			Directory = InDirectory;
		}

		public string Name
		{
			get { return Directory.Name; }
		}

		public override string ToString()
		{
			return Directory.FullName;
		}
	}

	class FindFoldersToCleanTask : IModalTask, IDisposable
	{
		class PerforceHaveFolder
		{
			public Dictionary<string, PerforceHaveFolder> SubFolders = new Dictionary<string,PerforceHaveFolder>(StringComparer.InvariantCultureIgnoreCase);
			public HashSet<string> Files = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);
		}

		PerforceConnection PerforceClient;
		string ClientRootPath;
		IReadOnlyList<string> SyncPaths;
		TextWriter Log;
		FolderToClean RootFolderToClean;

		int RemainingFoldersToScan;
		ManualResetEvent FinishedScan = new ManualResetEvent(false);
		bool bAbortScan;

		public List<string> FileNames = new List<string>();

		public FindFoldersToCleanTask(PerforceConnection InPerforceClient, FolderToClean InRootFolderToClean, string InClientRootPath, IReadOnlyList<string> InSyncPaths, TextWriter InLog)
		{
			PerforceClient = InPerforceClient;
			ClientRootPath = InClientRootPath.TrimEnd('/') + "/";
			SyncPaths = new List<string>(InSyncPaths);
			Log = InLog;
			RootFolderToClean = InRootFolderToClean;
			FinishedScan = new ManualResetEvent(true);

			foreach(string SyncPath in SyncPaths)
			{
				Debug.Assert(SyncPath.StartsWith(ClientRootPath));
				if(SyncPath.StartsWith(ClientRootPath, StringComparison.InvariantCultureIgnoreCase))
				{
					string[] Fragments = SyncPath.Substring(ClientRootPath.Length).Split('/');

					FolderToClean SyncFolder = RootFolderToClean;
					for(int Idx = 0; Idx < Fragments.Length - 1; Idx++)
					{
						FolderToClean NextSyncFolder = SyncFolder.SubFolders.FirstOrDefault(x => x.Name.Equals(Fragments[Idx], StringComparison.InvariantCultureIgnoreCase));
						if(NextSyncFolder == null)
						{
							NextSyncFolder = new FolderToClean(new DirectoryInfo(Path.Combine(SyncFolder.Directory.FullName, Fragments[Idx])));
							SyncFolder.SubFolders.Add(NextSyncFolder);
						}
						SyncFolder = NextSyncFolder;
					}

					string Wildcard = Fragments[Fragments.Length - 1];
					if(Wildcard == "...")
					{
						QueueFolderToPopulate(SyncFolder);
					}
					else if(SyncFolder.Directory.Exists)
					{
						SyncFolder.FilesToClean = SyncFolder.FilesToClean.Union(SyncFolder.Directory.GetFiles(Wildcard)).GroupBy(x => x.Name).Select(x => x.First()).ToList();
					}
				}
			}
		}

		void QueueFolderToPopulate(FolderToClean Folder)
		{
			if(Interlocked.Increment(ref RemainingFoldersToScan) == 1)
			{
				FinishedScan.Reset();
			}
			ThreadPool.QueueUserWorkItem(x => PopulateFolder(Folder));
		}

		void PopulateFolder(FolderToClean Folder)
		{
			if(!bAbortScan)
			{
				if((Folder.Directory.Attributes & FileAttributes.ReparsePoint) == 0)
				{
					foreach(DirectoryInfo SubDirectory in Folder.Directory.EnumerateDirectories())
					{
						FolderToClean SubFolder = new FolderToClean(SubDirectory);
						Folder.SubFolders.Add(SubFolder);
						QueueFolderToPopulate(SubFolder);
					}
					Folder.FilesToClean = Folder.Directory.EnumerateFiles().ToList();
				}
				Folder.bEmptyLeaf = Folder.SubFolders.Count == 0 && Folder.FilesToClean.Count == 0;
			}

			if(Interlocked.Decrement(ref RemainingFoldersToScan) == 0)
			{
				FinishedScan.Set();
			}
		}

		void RemoveFilesToKeep(FolderToClean Folder, PerforceHaveFolder KeepFolder)
		{
			foreach(FolderToClean SubFolder in Folder.SubFolders)
			{
				PerforceHaveFolder KeepSubFolder;
				if(KeepFolder.SubFolders.TryGetValue(SubFolder.Directory.Name, out KeepSubFolder))
				{
					RemoveFilesToKeep(SubFolder, KeepSubFolder);
					Folder.bEmptyAfterDelete &= SubFolder.bEmptyAfterDelete;
				}
			}
			Folder.bEmptyAfterDelete &= (Folder.FilesToClean.RemoveAll(x => KeepFolder.Files.Contains(x.Name)) == 0);
		}

		void RemoveEmptyFolders(FolderToClean Folder)
		{
			foreach(FolderToClean SubFolder in Folder.SubFolders)
			{
				RemoveEmptyFolders(SubFolder);
			}
			Folder.SubFolders.RemoveAll(x => x.SubFolders.Count == 0 && x.FilesToClean.Count == 0 && !x.bEmptyLeaf);
		}

		public void Dispose()
		{
			bAbortScan = true;

			if(FinishedScan != null)
			{
				FinishedScan.WaitOne();
				FinishedScan.Dispose();
				FinishedScan = null;
			}
		}

		public bool Run(out string ErrorMessage)
		{
			Log.WriteLine("Finding files in workspace...");
			Log.WriteLine();

			// Query the have table
			PerforceHaveFolder HaveFolder = new PerforceHaveFolder();
			foreach(string SyncPath in SyncPaths)
			{
				List<PerforceFileRecord> FileRecords;
				if(!PerforceClient.Have(SyncPath, out FileRecords, Log))
				{
					ErrorMessage = "Couldn't query have table from Perforce.";
					return false;
				}
				foreach(PerforceFileRecord FileRecord in FileRecords)
				{
					if(!FileRecord.ClientPath.StartsWith(ClientRootPath, StringComparison.InvariantCultureIgnoreCase))
					{
						ErrorMessage = String.Format("Failed to get have table; file '{0}' doesn't start with client root path ('{1}')", FileRecord.ClientPath, ClientRootPath);
						return false;
					}
					
					string[] Tokens = FileRecord.ClientPath.Substring(ClientRootPath.Length).Split('/', '\\');

					PerforceHaveFolder FileFolder = HaveFolder;
					for(int Idx = 0; Idx < Tokens.Length - 1; Idx++)
					{
						PerforceHaveFolder NextFileFolder;
						if(!FileFolder.SubFolders.TryGetValue(Tokens[Idx], out NextFileFolder))
						{
							NextFileFolder = new PerforceHaveFolder();
							FileFolder.SubFolders.Add(Tokens[Idx], NextFileFolder);
						}
						FileFolder = NextFileFolder;
					}
					FileFolder.Files.Add(Tokens[Tokens.Length - 1]);
				}
			}

			// Wait to finish scanning the directory
			FinishedScan.WaitOne();

			// Remove all the files in the have table from the list of files to delete
			RemoveFilesToKeep(RootFolderToClean, HaveFolder);

			// Remove all the empty folders
			RemoveEmptyFolders(RootFolderToClean);
			ErrorMessage = null;
			return true;
		}
	}
}
