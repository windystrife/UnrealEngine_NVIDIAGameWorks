// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	///  Base class to handle deploy of a target for a given platform
	/// </summary>
	class BaseWindowsDeploy : UEBuildDeploy
	{

        public bool PrepForUATPackageOrDeploy(FileReference ProjectFile, string ProjectName, string ProjectDirectory, List<UnrealTargetConfiguration> TargetConfigurations, List<string> ExecutablePaths, string EngineDirectory)
        {
            string ApplicationIconPath = Path.Combine(ProjectDirectory, "Build/Windows/Application.ico");
            // Does a Project icon exist?
            if (!File.Exists(ApplicationIconPath))
            {
                // Also check for legacy location
                ApplicationIconPath = Path.Combine(ProjectDirectory, "Source", ProjectName, "Resources", "Windows", ProjectName + ".ico");
                if (!File.Exists(ApplicationIconPath))
                {
                    // point to the default UE4 icon instead
                    ApplicationIconPath = Path.Combine(EngineDirectory, "Source/Runtime/Launch/Resources/Windows/UE4.ico");
                }
            }
            // sets the icon on the original exe this will be used in the task bar when the bootstrap exe runs
            if (File.Exists(ApplicationIconPath))
            {
                GroupIconResource GroupIcon = null;
                GroupIcon = GroupIconResource.FromIco(ApplicationIconPath);

                foreach (string ExecutablePath in ExecutablePaths)
                {
                    // Update the icon on the original exe because this will be used when the game is running in the task bar
                    using (ModuleResourceUpdate Update = new ModuleResourceUpdate(ExecutablePath, false))
                    {
                        const int IconResourceId = 123; // As defined in Engine\Source\Runtime\Launch\Resources\Windows\resource.h
                        if (GroupIcon != null)
                        {
                            Update.SetIcons(IconResourceId, GroupIcon);
                        }
                    }
                }
            }
            return true;
        }




		public override bool PrepTargetForDeployment(UEBuildDeployTarget InTarget)
		{
			if ((InTarget.TargetType != TargetType.Editor && InTarget.TargetType != TargetType.Program) && (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win32 || BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64))
			{
				string InAppName = InTarget.AppName;
				Log.TraceInformation("Prepping {0} for deployment to {1}", InAppName, InTarget.Platform.ToString());
				System.DateTime PrepDeployStartTime = DateTime.UtcNow;

                List<UnrealTargetConfiguration> TargetConfigs = new List<UnrealTargetConfiguration> { InTarget.Configuration };
                List<string> ExePaths = new List<string> { InTarget.OutputPath.FullName };
				string RelativeEnginePath = UnrealBuildTool.EngineDirectory.MakeRelativeTo(DirectoryReference.GetCurrentDirectory());
                PrepForUATPackageOrDeploy(InTarget.ProjectFile, InAppName, InTarget.ProjectDirectory.FullName, TargetConfigs, ExePaths, RelativeEnginePath);
			}
			return true;
		}
	}
}
