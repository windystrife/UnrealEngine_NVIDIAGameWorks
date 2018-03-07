// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;
using Tools.DotNETCommon;

public class TVOSPlatform : IOSPlatform
{
	public TVOSPlatform()
		:base(UnrealTargetPlatform.TVOS)
	{
		TargetIniPlatformType = UnrealTargetPlatform.IOS;
	}

	public override bool PrepForUATPackageOrDeploy(UnrealTargetConfiguration Config, FileReference ProjectFile, string InProjectName, DirectoryReference InProjectDirectory, string InExecutablePath, DirectoryReference InEngineDir, bool bForDistribution, string CookFlavor, bool bIsDataDeploy, bool bCreateStubIPA)
	{
		return TVOSExports.PrepForUATPackageOrDeploy(Config, ProjectFile, InProjectName, InProjectDirectory, InExecutablePath, InEngineDir, bForDistribution, CookFlavor, bIsDataDeploy, bCreateStubIPA);
	}

    public override void GetProvisioningData(FileReference InProject, bool bDistribution, out string MobileProvision, out string SigningCertificate, out string Team, out bool bAutomaticSigning)
    {
		TVOSExports.GetProvisioningData(InProject, bDistribution, out MobileProvision, out SigningCertificate, out Team, out bAutomaticSigning);
    }

	public override bool DeployGeneratePList(FileReference ProjectFile, UnrealTargetConfiguration Config, DirectoryReference ProjectDirectory, bool bIsUE4Game, string GameName, string ProjectName, DirectoryReference InEngineDir, DirectoryReference AppDirectory, out bool bSupportsPortrait, out bool bSupportsLandscape, out bool bSkipIcons)
	{
		return TVOSExports.GeneratePList(ProjectFile, Config, ProjectDirectory, bIsUE4Game, GameName, ProjectName, InEngineDir, AppDirectory, out bSupportsPortrait, out bSupportsLandscape, out bSkipIcons);
	}

    public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		return "TVOS";
	}

    public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
    {
        //		if (UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
        {
            // copy the icons/launch screens from the engine
            {
				FileReference SourcePath = FileReference.Combine(SC.LocalRoot, "Engine", "Binaries", "TVOS", "AssetCatalog", "Assets.car");
				if(FileReference.Exists(SourcePath))
				{
					SC.StageFile(StagedFileType.SystemNonUFS, SourcePath, new StagedFileReference("Assets.car"));
				}
            }

            // copy any additional framework assets that will be needed at runtime
            {
                DirectoryReference SourcePath = DirectoryReference.Combine((SC.IsCodeBasedProject ? SC.ProjectRoot : SC.EngineRoot), "Intermediate", "TVOS", "FrameworkAssets");
                if (DirectoryReference.Exists(SourcePath))
                {
					SC.StageFiles(StagedFileType.SystemNonUFS, SourcePath, StageFilesSearch.AllDirectories, StagedDirectoryReference.Root);
                }
            }

            // copy the icons/launch screens from the game (may stomp the engine copies)
            {
                FileReference SourcePath = FileReference.Combine(SC.ProjectRoot, "Binaries", "TVOS", "AssetCatalog", "Assets.car");
				if(FileReference.Exists(SourcePath))
				{
	                SC.StageFile(StagedFileType.SystemNonUFS, SourcePath, new StagedFileReference("Assets.car"));
				}
            }

            // copy the plist (only if code signing, as it's protected by the code sign blob in the executable and can't be modified independently)
            if (GetCodeSignDesirability(Params))
            {
                DirectoryReference SourcePath = DirectoryReference.Combine((SC.IsCodeBasedProject ? SC.ProjectRoot : SC.EngineRoot), "Intermediate", "TVOS");
                FileReference TargetPListFile = FileReference.Combine(SourcePath, (SC.IsCodeBasedProject ? SC.ShortProjectName : "UE4Game") + "-Info.plist");
                //				if (!File.Exists(TargetPListFile))
                {
                    // ensure the plist, entitlements, and provision files are properly copied
                    Console.WriteLine("CookPlat {0}, this {1}", GetCookPlatform(false, false), ToString());
                    if (!SC.IsCodeBasedProject)
                    {
                        UnrealBuildTool.PlatformExports.SetRemoteIniPath(SC.ProjectRoot.FullName);
                    }

                    if (SC.StageTargetConfigurations.Count != 1)
                    {
                        throw new AutomationException("iOS is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
                    }

                    var TargetConfiguration = SC.StageTargetConfigurations[0];

                    bool bSupportsPortrait = false;
                    bool bSupportsLandscape = false;
                    bool bSkipIcons = false;
                    DeployGeneratePList(SC.RawProjectPath, TargetConfiguration, (SC.IsCodeBasedProject ? SC.ProjectRoot : SC.EngineRoot), !SC.IsCodeBasedProject, (SC.IsCodeBasedProject ? SC.ShortProjectName : "UE4Game"), SC.ShortProjectName, SC.EngineRoot, DirectoryReference.Combine((SC.IsCodeBasedProject ? SC.ProjectRoot : SC.EngineRoot), "Binaries", "TVOS", "Payload", (SC.IsCodeBasedProject ? SC.ShortProjectName : "UE4Game") + ".app"), out bSupportsPortrait, out bSupportsLandscape, out bSkipIcons);
                }

                SC.StageFile(StagedFileType.SystemNonUFS, TargetPListFile, new StagedFileReference("Info.plist"));
            }
        }

        // copy the movies from the project
        {
			DirectoryReference PlatformMovies = DirectoryReference.Combine(SC.ProjectRoot, "Build", "TVOS", "Resources", "Movies");
			if(DirectoryReference.Exists(PlatformMovies))
			{
				SC.StageFiles(StagedFileType.SystemNonUFS, PlatformMovies, StageFilesSearch.TopDirectoryOnly, StagedDirectoryReference.Root);
			}

			DirectoryReference AllMovies = DirectoryReference.Combine(SC.ProjectRoot, "Content", "Movies");
			if(DirectoryReference.Exists(AllMovies))
			{
				SC.StageFiles(StagedFileType.SystemNonUFS, AllMovies, StageFilesSearch.AllDirectories, StagedDirectoryReference.Root);
			}
        }
    }
}
