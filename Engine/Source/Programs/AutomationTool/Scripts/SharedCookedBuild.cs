// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using System.Linq;
using System.Threading.Tasks;
using AutomationTool;
using UnrealBuildTool;
using Tools.DotNETCommon;

public class SharedCookedBuild
{
	private static Task CopySharedCookedBuildTask = null;

	private static bool CopySharedCookedBuildForTargetInternal(string CookedBuildPath, string CookPlatform, string LocalPath, bool bOnlyCopyAssetRegistry)
	{

		string BuildRoot = CommandUtils.P4Enabled ? CommandUtils.P4Env.Branch.Replace("/", "+") : "";
		string RecentCL = CommandUtils.P4Enabled ? CommandUtils.P4Env.Changelist.ToString() : "";

		BuildVersion Version;
		if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
		{
			RecentCL = Version.Changelist.ToString();
			BuildRoot = Version.BranchName;
		}
		System.GC.Collect();

		// check to see if we have already synced this build ;)
		var SyncedBuildFile = CommandUtils.CombinePaths(LocalPath, "SyncedBuild.txt");
		string BuildCL = "Invalid";
		if (File.Exists(SyncedBuildFile))
		{
			BuildCL = File.ReadAllText(SyncedBuildFile);
		}

		if (RecentCL == "" && CookedBuildPath.Contains("[CL]"))
		{
			CommandUtils.Log("Unable to copy shared cooked build: Unable to determine CL number from P4 or UGS, and is required by SharedCookedBuildPath");
			return false;
		}

		if (RecentCL == "" && CookedBuildPath.Contains("[BRANCHNAME]"))
		{
			CommandUtils.Log("Unable to copy shared cooked build: Unable to determine BRANCHNAME number from P4 or UGS, and is required by SharedCookedBuildPath");
			return false;
		}


		CookedBuildPath = CookedBuildPath.Replace("[CL]", RecentCL.ToString());
		CookedBuildPath = CookedBuildPath.Replace("[BRANCHNAME]", BuildRoot);
		CookedBuildPath = CookedBuildPath.Replace("[PLATFORM]", CookPlatform);

		if (Directory.Exists(CookedBuildPath) == false)
		{
			CommandUtils.Log("Unable to copy shared cooked build: Unable to find shared build at location {0} check SharedCookedBuildPath in Engine.ini SharedCookedBuildSettings is correct", CookedBuildPath);
			return false;
		}

		CommandUtils.Log("Attempting download of latest shared build CL {0} from location {1}", RecentCL, CookedBuildPath);

		if (BuildCL == RecentCL.ToString())
		{
			CommandUtils.Log("Already downloaded latest shared build at CL {0}", RecentCL);
			return false;
		}

		if (bOnlyCopyAssetRegistry)
		{
			RecentCL += "RegistryOnly";
		}

		if ( BuildCL == RecentCL )
		{
			CommandUtils.Log("Already downloaded latest shared build at CL {0}", RecentCL);
			return false;
		}

		// delete all the stuff
		CommandUtils.Log("Deleting previous shared build because it was out of date");
		CommandUtils.DeleteDirectory(LocalPath);
		Directory.CreateDirectory(LocalPath);



		string CookedBuildCookedDirectory = Path.Combine(CookedBuildPath, "Cooked");
		CookedBuildCookedDirectory = Path.GetFullPath(CookedBuildCookedDirectory);
		string LocalBuildCookedDirectory = Path.Combine(LocalPath, "Cooked");
		LocalBuildCookedDirectory = Path.GetFullPath(LocalBuildCookedDirectory);
		if (Directory.Exists(CookedBuildCookedDirectory))
		{
			foreach (string FileName in Directory.EnumerateFiles(CookedBuildCookedDirectory, "*.*", SearchOption.AllDirectories))
			{
				string SourceFileName = Path.GetFullPath(FileName);
				string DestFileName = SourceFileName.Replace(CookedBuildCookedDirectory, LocalBuildCookedDirectory);
				Directory.CreateDirectory(Path.GetDirectoryName(DestFileName));
				File.Copy(SourceFileName, DestFileName);
			}
		}

		if ( CopySharedCookedBuildTask != null )
		{
			WaitForCopy();
		}

		if (bOnlyCopyAssetRegistry == false)
		{
			CopySharedCookedBuildTask = Task.Run(() =>
				{
					// find all the files in the staged directory
					string CookedBuildStagedDirectory = Path.GetFullPath(Path.Combine(CookedBuildPath, "Staged"));
					string LocalBuildStagedDirectory = Path.GetFullPath(Path.Combine(LocalPath, "Staged"));
					if (Directory.Exists(CookedBuildStagedDirectory))
					{
						foreach (string FileName in Directory.EnumerateFiles(CookedBuildStagedDirectory, "*.*", SearchOption.AllDirectories))
						{
							string SourceFileName = Path.GetFullPath(FileName);
							string DestFileName = SourceFileName.Replace(CookedBuildStagedDirectory, LocalBuildStagedDirectory);
							Directory.CreateDirectory(Path.GetDirectoryName(DestFileName));
							File.Copy(SourceFileName, DestFileName);
						}
					}
					File.WriteAllText(SyncedBuildFile, RecentCL.ToString());
				}
				);
		}
		else
		{
			File.WriteAllText(SyncedBuildFile, RecentCL.ToString());
		}

		
		return true;
	}

	public static void CopySharedCookedBuildForTarget(string ProjectFullPath, UnrealTargetPlatform TargetPlatform, string CookPlatform, bool bOnlyCopyAssetRegistry = false)
	{
		var LocalPath = CommandUtils.CombinePaths(Path.GetDirectoryName(ProjectFullPath), "Saved", "SharedIterativeBuild", CookPlatform);

		FileReference ProjectFileRef = new FileReference(ProjectFullPath);
		// get network location 
		ConfigHierarchy Hierarchy = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFileRef), TargetPlatform);
		List<string> CookedBuildPaths;
		if (Hierarchy.GetArray("SharedCookedBuildSettings", "SharedCookedBuildPath", out CookedBuildPaths) == false)
		{
			CommandUtils.Log("Unable to copy shared cooked build: SharedCookedBuildPath not set in Engine.ini SharedCookedBuildSettings");
			return;
		}
		foreach (var CookedBuildPath in CookedBuildPaths)
		{
			if (CopySharedCookedBuildForTargetInternal(CookedBuildPath, CookPlatform, LocalPath, bOnlyCopyAssetRegistry) == true)
			{
				return;
			}
		}

		return;
	}

	public static void CopySharedCookedBuild(ProjectParams Params)
	{

		if (!Params.NoClient)
		{
			foreach (var ClientPlatform in Params.ClientTargetPlatforms)
			{
				// Use the data platform, sometimes we will copy another platform's data
				var DataPlatformDesc = Params.GetCookedDataPlatformForClientTarget(ClientPlatform);
				string PlatformToCook = Platform.Platforms[DataPlatformDesc].GetCookPlatform(false, Params.Client);
				CopySharedCookedBuildForTarget(Params.RawProjectPath.FullName, ClientPlatform.Type, PlatformToCook);
			}
		}
		if (Params.DedicatedServer)
		{
			foreach (var ServerPlatform in Params.ServerTargetPlatforms)
			{
				// Use the data platform, sometimes we will copy another platform's data
				var DataPlatformDesc = Params.GetCookedDataPlatformForServerTarget(ServerPlatform);
				string PlatformToCook = Platform.Platforms[DataPlatformDesc].GetCookPlatform(true, false);
				CopySharedCookedBuildForTarget(Params.RawProjectPath.FullName, ServerPlatform.Type, PlatformToCook);
			}
		}
	}

	public static void WaitForCopy()
	{
		if (CopySharedCookedBuildTask != null)
		{
			CopySharedCookedBuildTask.Wait();
		}
	}

}