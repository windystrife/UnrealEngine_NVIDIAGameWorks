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

/// <summary>
/// Helper command used for cooking.
/// </summary>
/// <remarks>
/// Command line parameters used by this command:
/// -clean
/// </remarks>
public partial class Project : CommandUtils
{
    #region Cook Command

    public static void Cook(ProjectParams Params)
	{
		if ((!Params.Cook && !(Params.CookOnTheFly && !Params.SkipServer)) || Params.SkipCook)
		{
			return;
		}
		Params.ValidateAndLog();

		Log("********** COOK COMMAND STARTED **********");

		string UE4EditorExe = HostPlatform.Current.GetUE4ExePath(Params.UE4Exe);
		if (!FileExists(UE4EditorExe))
		{
			throw new AutomationException("Missing " + UE4EditorExe + " executable. Needs to be built first.");
		}

		if (Params.CookOnTheFly && !Params.SkipServer)
		{
            if (Params.HasDLCName)
            {
                throw new AutomationException("Cook on the fly doesn't support cooking dlc");
            }
			if (Params.ClientTargetPlatforms.Count > 0)
			{
				var LogFolderOutsideOfSandbox = GetLogFolderOutsideOfSandbox();
				if (!GlobalCommandLine.Installed)
				{
					// In the installed runs, this is the same folder as CmdEnv.LogFolder so delete only in not-installed
					DeleteDirectory(LogFolderOutsideOfSandbox);
					CreateDirectory(LogFolderOutsideOfSandbox);
				}

				String COTFCommandLine = Params.RunCommandline;
				if (Params.IterativeCooking)
				{
					COTFCommandLine += " -iterate -iteratehash";
				}
				if (Params.UseDebugParamForEditorExe)
				{
					COTFCommandLine += " -debug";
				}

				var ServerLogFile = CombinePaths(LogFolderOutsideOfSandbox, "Server.log");
				Platform ClientPlatformInst = Params.ClientTargetPlatformInstances[0];
				string TargetCook = ClientPlatformInst.GetCookPlatform(false, false); // cook on he fly doesn't support server cook platform... 
				ServerProcess = RunCookOnTheFlyServer(Params.RawProjectPath, Params.NoClient ? "" : ServerLogFile, TargetCook, COTFCommandLine);

				if (ServerProcess != null)
				{
					Log("Waiting a few seconds for the server to start...");
					Thread.Sleep(5000);
				}
			}
			else
			{
				throw new AutomationException("Failed to run, client target platform not specified");
			}
		}
		else
		{
            var PlatformsToCook = new HashSet<string>();
            if (!Params.NoClient)
			{
				foreach (var ClientPlatform in Params.ClientTargetPlatforms)
				{
					// Use the data platform, sometimes we will copy another platform's data
					var DataPlatformDesc = Params.GetCookedDataPlatformForClientTarget(ClientPlatform);
                    string PlatformToCook = Platform.Platforms[DataPlatformDesc].GetCookPlatform(false, Params.Client);
                    PlatformsToCook.Add(PlatformToCook);
                }
			}
			if (Params.DedicatedServer)
			{
				foreach (var ServerPlatform in Params.ServerTargetPlatforms)
				{
					// Use the data platform, sometimes we will copy another platform's data
					var DataPlatformDesc = Params.GetCookedDataPlatformForServerTarget(ServerPlatform);
                    string PlatformToCook = Platform.Platforms[DataPlatformDesc].GetCookPlatform(true, false);
                    PlatformsToCook.Add(PlatformToCook);
                }
			}

			if (Params.Clean.HasValue && Params.Clean.Value && !Params.IterativeCooking)
			{
				Log("Cleaning cooked data.");
				CleanupCookedData(PlatformsToCook.ToList(), Params);
			}

			// cook the set of maps, or the run map, or nothing
			string[] Maps = null;
			if (Params.HasMapsToCook)
			{
				Maps = Params.MapsToCook.ToArray();
                foreach (var M in Maps)
                {
					Log("HasMapsToCook " + M.ToString());
                }
                foreach (var M in Params.MapsToCook)
                {
					Log("Params.HasMapsToCook " + M.ToString());
                }
			}
			
			string[] Dirs = null;
			if (Params.HasDirectoriesToCook)
			{
				Dirs = Params.DirectoriesToCook.ToArray();
			}

            string InternationalizationPreset = null;
            if (Params.HasInternationalizationPreset)
            {
                InternationalizationPreset = Params.InternationalizationPreset;
            }

			string[] CulturesToCook = null;
			if (Params.HasCulturesToCook)
			{
				CulturesToCook = Params.CulturesToCook.ToArray();
			}

            try
            {
                var CommandletParams = IsBuildMachine ? "-buildmachine -fileopenlog" : "-fileopenlog";
                if (Params.UnversionedCookedContent)
                {
                    CommandletParams += " -unversioned";
                }
				if (Params.FastCook)
				{
					CommandletParams += " -FastCook";
				}
                if (Params.UseDebugParamForEditorExe)
                {
                    CommandletParams += " -debug";
                }
                if (Params.Manifests)
                {
                    CommandletParams += " -manifests";
                }
                if (Params.IterativeCooking)
                {
                    CommandletParams += " -iterate -iterateshash";
                }
				if ( Params.IterateSharedCookedBuild)
				{
					SharedCookedBuild.CopySharedCookedBuild(Params);
					CommandletParams += " -iteratesharedcookedbuild";					
				}

				if (Params.CookMapsOnly)
                {
                    CommandletParams += " -mapsonly";
                }
                if (Params.CookAll)
                {
                    CommandletParams += " -cookall";
                }
                if (Params.HasCreateReleaseVersion)
                {
                    CommandletParams += " -createreleaseversion=" + Params.CreateReleaseVersion;
                }
                if ( Params.SkipCookingEditorContent)
                {
                    CommandletParams += " -skipeditorcontent";
                }
                if ( Params.NumCookersToSpawn != 0)
                {
                    CommandletParams += " -numcookerstospawn=" + Params.NumCookersToSpawn;
                }
				if ( Params.CookPartialGC)
				{
					CommandletParams += " -partialgc";
				}
				if (Params.HasMapIniSectionsToCook)
				{
					string MapIniSections = CombineCommandletParams(Params.MapIniSectionsToCook.ToArray());

					CommandletParams += " -MapIniSection=" + MapIniSections;
				}
				if (Params.HasDLCName)
                {
                    CommandletParams += " -dlcname=" + Params.DLCFile.GetFileNameWithoutExtension();
                    if ( !Params.DLCIncludeEngineContent )
                    {
                        CommandletParams += " -errorOnEngineContentUse";
                    }
                }
				if(!String.IsNullOrEmpty(Params.CookOutputDir))
				{
					CommandletParams += " -outputdir=" + CommandUtils.MakePathSafeToUseWithCommandLine(Params.CookOutputDir);
				}
                // don't include the based on release version unless we are cooking dlc or creating a new release version
                // in this case the based on release version is used in packaging
                if (Params.HasBasedOnReleaseVersion && (Params.HasDLCName || Params.HasCreateReleaseVersion))
                {
                    CommandletParams += " -basedonreleaseversion=" + Params.BasedOnReleaseVersion;
                }
                // if we are not going to pak but we specified compressed then compress in the cooker ;)
                // otherwise compress the pak files
                if (!Params.Pak && !Params.SkipPak && Params.Compressed)
                {
                    CommandletParams += " -compressed";
                }
                
                if (Params.HasAdditionalCookerOptions)
                {
                    string FormatedAdditionalCookerParams = Params.AdditionalCookerOptions.TrimStart(new char[] { '\"', ' ' }).TrimEnd(new char[] { '\"', ' ' });
                    CommandletParams += " ";
                    CommandletParams += FormatedAdditionalCookerParams;
                }

                if (!Params.NoClient)
                {
                    var MapsList = Maps == null ? new List<string>() :  Maps.ToList(); 
                    foreach (var ClientPlatform in Params.ClientTargetPlatforms)
                    {
                        var DataPlatformDesc = Params.GetCookedDataPlatformForClientTarget(ClientPlatform);
                        CommandletParams += (Platform.Platforms[DataPlatformDesc].GetCookExtraCommandLine(Params));
                        MapsList.AddRange((Platform.Platforms[ClientPlatform].GetCookExtraMaps()));
                    }
                    Maps = MapsList.ToArray();
                }

				// Config overrides (-ini)
				foreach(string ConfigOverrideParam in Params.ConfigOverrideParams)
				{
					CommandletParams += " -";
					CommandletParams += ConfigOverrideParam;
				}

                CookCommandlet(Params.RawProjectPath, Params.UE4Exe, Maps, Dirs, InternationalizationPreset, CulturesToCook, CombineCommandletParams(PlatformsToCook.ToArray()), CommandletParams);

				SharedCookedBuild.WaitForCopy();
            }
			catch (Exception Ex)
			{
				if (Params.IgnoreCookErrors)
				{
					LogWarning("Ignoring cook failure.");
				}
				else
				{
					// Delete cooked data (if any) as it may be incomplete / corrupted.
					Log("Cook failed. Deleting cooked data.");
					CleanupCookedData(PlatformsToCook.ToList(), Params);
					throw new AutomationException(ExitCode.Error_UnknownCookFailure, Ex, "Cook failed.");
				}
			}

            if (Params.HasDiffCookedContentPath)
            {
                try
                {
                    DiffCookedContent(Params);
                }
                catch ( Exception Ex )
                {
                    // Delete cooked data (if any) as it may be incomplete / corrupted.
                    Log("Cook failed. Deleting cooked data.");
                    CleanupCookedData(PlatformsToCook.ToList(), Params);
                    throw new AutomationException(ExitCode.Error_UnknownCookFailure, Ex, "Cook failed.");
                }
            }
            
		}


		Log("********** COOK COMMAND COMPLETED **********");
	}

    public struct FileInfo
    {
        public FileInfo(string InFilename)
        {
            Filename = InFilename;
            FirstByteFailed = -1;
            BytesMismatch = 0;
            File1Size = 0;
            File2Size = 0;
        }
        public FileInfo(string InFilename, long InFirstByteFailed, long InBytesMismatch, long InFile1Size, long InFile2Size)
        {
            Filename = InFilename;
            FirstByteFailed = InFirstByteFailed;
            BytesMismatch = InBytesMismatch;
            File1Size = InFile1Size;
            File2Size = InFile2Size;
        }
        public string Filename;
        public long FirstByteFailed;
        public long BytesMismatch;
        public long File1Size;
        public long File2Size;
    };

    private static void DiffCookedContent( ProjectParams Params)
    {
        List<TargetPlatformDescriptor> PlatformsToCook = Params.ClientTargetPlatforms;
        string ProjectPath = Params.RawProjectPath.FullName;

        var CookedSandboxesPath = CombinePaths(GetDirectoryName(ProjectPath), "Saved", "Cooked");

        for (int CookPlatformIndex = 0; CookPlatformIndex < PlatformsToCook.Count; ++CookPlatformIndex)
        {
            // temporary directory to save the pak file to (pak file is usually not local and on network drive)
            var TemporaryPakPath = CombinePaths(GetDirectoryName(ProjectPath), "Saved", "Temp", "LocalPKG");
            // extracted files from pak file
            var TemporaryFilesPath = CombinePaths(GetDirectoryName(ProjectPath), "Saved", "Temp", "LocalFiles");

            

            try
            {
                Directory.Delete(TemporaryPakPath, true);
            }
            catch(Exception Ex)
            {
                if (!(Ex is System.IO.DirectoryNotFoundException))
                {
                    Log("Failed deleting temporary directories " + TemporaryPakPath + " continuing. " + Ex.GetType().ToString());
                }
            }
            try
            {
                Directory.Delete(TemporaryFilesPath, true);
            }
            catch (Exception Ex)
            {
                if (!(Ex is System.IO.DirectoryNotFoundException))
                {
                    Log("Failed deleting temporary directories " + TemporaryFilesPath + " continuing. " + Ex.GetType().ToString());
                }
            }

            try
            {

                Directory.CreateDirectory(TemporaryPakPath);
                Directory.CreateDirectory(TemporaryFilesPath);

                Platform CurrentPlatform = Platform.Platforms[PlatformsToCook[CookPlatformIndex]];

                string SourceCookedContentPath = Params.DiffCookedContentPath;

                List<string> PakFiles = new List<string>();

                string CookPlatformString = CurrentPlatform.GetCookPlatform(false, Params.Client);

                if (Path.HasExtension(SourceCookedContentPath) && (!SourceCookedContentPath.EndsWith(".pak")))
                {
                    // must be a per platform pkg file try this
                    CurrentPlatform.ExtractPackage(Params, Params.DiffCookedContentPath, TemporaryPakPath);

                    // find the pak file
                    PakFiles.AddRange( Directory.EnumerateFiles(TemporaryPakPath, Params.ShortProjectName+"*.pak", SearchOption.AllDirectories));
                    PakFiles.AddRange( Directory.EnumerateFiles(TemporaryPakPath, "pakchunk*.pak", SearchOption.AllDirectories));
                }
                else if (!Path.HasExtension(SourceCookedContentPath))
                {
                    // try find the pak or pkg file
                    string SourceCookedContentPlatformPath = CombinePaths(SourceCookedContentPath, CookPlatformString);

                    foreach (var PakName in Directory.EnumerateFiles(SourceCookedContentPlatformPath, Params.ShortProjectName + "*.pak", SearchOption.AllDirectories))
                    {
                        string TemporaryPakFilename = CombinePaths(TemporaryPakPath, Path.GetFileName(PakName));
                        File.Copy(PakName, TemporaryPakFilename);
                        PakFiles.Add(TemporaryPakFilename);
                    }

                    foreach (var PakName in Directory.EnumerateFiles(SourceCookedContentPlatformPath, "pakchunk*.pak", SearchOption.AllDirectories))
                    {
                        string TemporaryPakFilename = CombinePaths(TemporaryPakPath, Path.GetFileName(PakName));
                        File.Copy(PakName, TemporaryPakFilename);
                        PakFiles.Add(TemporaryPakFilename);
                    }

                    if ( PakFiles.Count <= 0 )
                    {
                        Log("No Pak files found in " + SourceCookedContentPlatformPath +" :(");
                    }
                }
                else if (SourceCookedContentPath.EndsWith(".pak"))
                {
                    string TemporaryPakFilename = CombinePaths(TemporaryPakPath, Path.GetFileName(SourceCookedContentPath));
                    File.Copy(SourceCookedContentPath, TemporaryPakFilename);
                    PakFiles.Add(TemporaryPakFilename);
                }


                string FullCookPath = CombinePaths(CookedSandboxesPath, CookPlatformString);

                var UnrealPakExe = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/Win64/UnrealPak.exe");


                foreach (var Name in PakFiles)
                {
                    Log("Extracting pak " + Name + " for comparision to location " + TemporaryFilesPath);

                    string UnrealPakParams = Name + " -Extract " + " " + TemporaryFilesPath;
                    try
                    {
                        RunAndLog(CmdEnv, UnrealPakExe, UnrealPakParams, Options: ERunOptions.Default | ERunOptions.UTF8Output | ERunOptions.LoggingOfRunDuration);
                    }
                    catch(Exception Ex)
                    {
                        Log("Pak failed to extract because of " + Ex.GetType().ToString());
                    }
                }

                const string RootFailedContentDirectory = "\\\\epicgames.net\\root\\Developers\\Daniel.Lamb";

                string FailedContentDirectory = CombinePaths(RootFailedContentDirectory, CommandUtils.P4Env.Branch + CommandUtils.P4Env.Changelist.ToString(), Params.ShortProjectName, CookPlatformString);

                Directory.CreateDirectory(FailedContentDirectory);

                // diff the content
                List<FileInfo> FileReport = new List<FileInfo>();

                List<string> AllFiles = Directory.EnumerateFiles(FullCookPath, "*.uasset", System.IO.SearchOption.AllDirectories).ToList();
                AllFiles.AddRange(Directory.EnumerateFiles(FullCookPath, "*.umap", System.IO.SearchOption.AllDirectories).ToList());
                foreach (string SourceFilename in AllFiles)
                {
                    // Filename.StartsWith( CookedSandboxesPath );
                    string RelativeFilename = SourceFilename.Remove(0, FullCookPath.Length);

                    string DestFilename = TemporaryFilesPath + RelativeFilename;

                    Log("Comparing file "+ RelativeFilename);

                    byte[] SourceFile = null;
                    try
                    {
                        SourceFile = File.ReadAllBytes(SourceFilename);
                    }
                    catch (Exception)
                    {
                        Log("Diff cooked content failed to load file " + SourceFilename);
                    }

                    byte[] DestFile = null;
                    try
                    {
                        DestFile = File.ReadAllBytes(DestFilename);
                    }
                    catch (Exception)
                    {
                        Log("Diff cooked content failed to load file " + DestFilename );
                    }

                    if (SourceFile == null || DestFile == null)
                    {
                        Log("Diff cooked content failed on file " + SourceFilename + " when comparing against " + DestFilename + " " + (SourceFile==null?SourceFilename:DestFilename) + " file is missing");
                    }
                    else if (SourceFile.LongLength == DestFile.LongLength)
                    {
                        /*long FirstByteFailed = -1;
                        long BytesFailed = 0;*/

                        FileInfo DiffFileInfo = new FileInfo(SourceFilename);
                        DiffFileInfo.File1Size = DiffFileInfo.File2Size = SourceFile.LongLength;

                        bool bFailedDiff = false;
                        for (long Index = 0; Index < SourceFile.LongLength; ++Index) 
                        {
                            if (SourceFile[Index] != DestFile[Index])
                            {
                                if ( DiffFileInfo.FirstByteFailed == -1)
                                {
                                    DiffFileInfo.FirstByteFailed = Index;
                                }
                                DiffFileInfo.BytesMismatch += 1;

                                if (bFailedDiff == false)
                                {
                                    bFailedDiff = true;

                                    Log("Diff cooked content failed on file " + SourceFilename + " when comparing against " + DestFilename + " at offset " + Index.ToString());
                                    string SavedSourceFilename = CombinePaths(FailedContentDirectory, Path.GetFileName(SourceFilename) + "Source");
                                    string SavedDestFilename = CombinePaths(FailedContentDirectory, Path.GetFileName(DestFilename) + "Dest");

                                    Log("Creating directory " + Path.GetDirectoryName(SavedSourceFilename));

                                    try
                                    {
                                        Directory.CreateDirectory(Path.GetDirectoryName(SavedSourceFilename));
                                    }
                                    catch (Exception E)
                                    {
                                        Log("Failed to create directory " + Path.GetDirectoryName(SavedSourceFilename) + " Exception " + E.ToString());
                                    }
                                    Log("Creating directory " + Path.GetDirectoryName(SavedDestFilename));
                                    try
                                    {
                                        Directory.CreateDirectory(Path.GetDirectoryName(SavedDestFilename));
                                    }
                                    catch (Exception E)
                                    {
                                        Log("Failed to create directory " + Path.GetDirectoryName(SavedDestFilename) + " Exception " + E.ToString());
                                    }
                                    File.Copy(SourceFilename, SavedSourceFilename, true);
                                    File.Copy(DestFilename, SavedDestFilename, true);
                                    Log("Content temporarily saved to " + SavedSourceFilename + " and " + SavedDestFilename + " at offset " + Index.ToString());
                                }
                                // break;
                            }
                        }
                        if (!bFailedDiff)
                        {
                            Log("Content matches for " + SourceFilename + " and " + DestFilename);
                        }
                        else 
                        {
                            FileReport.Add(DiffFileInfo);
                        }
                    }
                    else
                    {
                        Log("Diff cooked content failed on file " + SourceFilename + " when comparing against " + DestFilename + " files are different sizes " + SourceFile.LongLength.ToString() + " " + DestFile.LongLength.ToString());

                        FileInfo DiffFileInfo = new FileInfo(SourceFilename);

                        DiffFileInfo.File1Size = SourceFile.LongLength;
                        DiffFileInfo.File2Size = DestFile.LongLength;

                        FileReport.Add(DiffFileInfo);
                    }
                }

                Log("Mismatching files:");
                foreach (var Report in FileReport)
                {
                    if ( Report.FirstByteFailed == -1)
                    {
                        Log("File " + Report.Filename + " size mismatch: " + Report.File1Size + " VS " +Report.File2Size);
                    }
                    else
                    {
                        Log("File " + Report.Filename + " bytes mismatch: " + Report.BytesMismatch + " first byte failed at: " + Report.FirstByteFailed + " file size: " + Report.File1Size);
                    }
                }

            }
            catch ( Exception Ex )
            {
                Log("Exception " + Ex.ToString());
                continue;
            }
        }
    }

	private static void CleanupCookedData(List<string> PlatformsToCook, ProjectParams Params)
	{
		var ProjectPath = Params.RawProjectPath.FullName;
		var CookedSandboxesPath = CombinePaths(GetDirectoryName(ProjectPath), "Saved", "Cooked");
		var CleanDirs = new string[PlatformsToCook.Count];
		for (int DirIndex = 0; DirIndex < CleanDirs.Length; ++DirIndex)
		{
			CleanDirs[DirIndex] = CombinePaths(CookedSandboxesPath, PlatformsToCook[DirIndex]);
		}

		const bool bQuiet = true;
		foreach(string CleanDir in CleanDirs)
		{
			DeleteDirectory(bQuiet, CleanDir);
		}
	}

	#endregion
}
