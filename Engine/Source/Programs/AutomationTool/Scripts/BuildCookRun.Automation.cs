// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using AutomationTool;
using UnrealBuildTool;
using Tools.DotNETCommon;

[Help(@"Builds/Cooks/Runs a project.

For non-uprojects project targets are discovered by compiling target rule files found in the project folder.
If -map is not specified, the command looks for DefaultMap entry in the project's DefaultEngine.ini and if not found, in BaseEngine.ini.
If no DefaultMap can be found, the command falls back to /Engine/Maps/Entry.")]
[Help("project=Path", @"Project path (required), i.e: -project=QAGame, -project=Samples\BlackJack\BlackJack.uproject, -project=D:\Projects\MyProject.uproject")]
[Help("destsample", "Destination Sample name")]
[Help("foreigndest", "Foreign Destination")]
[Help(typeof(ProjectParams))]
[Help(typeof(UE4Build))]
[Help(typeof(CodeSign))]
public class BuildCookRun : BuildCommand
{
	#region BaseCommand interface

	public override void ExecuteBuild()
	{
		// these need to be done first
		var bForeign = ParseParam("foreign");
		var bForeignCode = ParseParam("foreigncode");
		if (bForeign)
		{
			MakeForeignSample();
		}
		else if (bForeignCode)
		{
			MakeForeignCodeSample();
		}
		var Params = SetupParams();

		DoBuildCookRun(Params);
	}

	#endregion

	#region Setup

	protected ProjectParams SetupParams()
	{
		Log("Setting up ProjectParams for {0}", ProjectPath);

		var Params = new ProjectParams
		(
			Command: this,
			// Shared
			RawProjectPath: ProjectPath
		);

		var DirectoriesToCook = ParseParamValue("cookdir");
		if (!String.IsNullOrEmpty(DirectoriesToCook))
		{
			Params.DirectoriesToCook = new ParamList<string>(DirectoriesToCook.Split('+'));
		}

        var InternationalizationPreset = ParseParamValue("i18npreset");
        if (!String.IsNullOrEmpty(InternationalizationPreset))
        {
            Params.InternationalizationPreset = InternationalizationPreset;
        }

        var CulturesToCook = ParseParamValue("cookcultures");
        if (!String.IsNullOrEmpty(CulturesToCook))
        {
            Params.CulturesToCook = new ParamList<string>(CulturesToCook.Split('+'));
        }

		if (Params.DedicatedServer)
		{
			foreach (var ServerPlatformInstance in Params.ServerTargetPlatformInstances)
			{
				ServerPlatformInstance.PlatformSetupParams(ref Params);
			}
		}
		else
		{
			foreach (var ClientPlatformInstance in Params.ClientTargetPlatformInstances)
			{
				ClientPlatformInstance.PlatformSetupParams(ref Params);
			}
		}

		Params.ValidateAndLog();
		return Params;
	}

	/// <summary>
	/// In case the command line specified multiple map names with a '+', selects the first map from the list.
	/// </summary>
	/// <param name="Maps">Map(s) specified in the commandline.</param>
	/// <returns>First map or an empty string.</returns>
	private static string GetFirstMap(string Maps)
	{
		string Map = String.Empty;
		if (!String.IsNullOrEmpty(Maps))
		{
			var AllMaps = Maps.Split(new char[] { '+' }, StringSplitOptions.RemoveEmptyEntries);
			if (!IsNullOrEmpty(AllMaps))
			{
				Map = AllMaps[0];
			}
		}
		return Map;
	}

	private string GetTargetName(Type TargetRulesType)
	{
		const string TargetPostfix = "Target";
		var Name = TargetRulesType.Name;
		if (Name.EndsWith(TargetPostfix, StringComparison.InvariantCultureIgnoreCase))
		{
			Name = Name.Substring(0, Name.Length - TargetPostfix.Length);
		}
		return Name;
	}

	private string GetDefaultMap(ProjectParams Params)
	{
		const string EngineEntryMap = "/Engine/Maps/Entry";
		Log("Trying to find DefaultMap in ini files");
		string DefaultMap = null;
		var ProjectFolder = GetDirectoryName(Params.RawProjectPath.FullName);
		var DefaultGameEngineConfig = CombinePaths(ProjectFolder, "Config", "DefaultEngine.ini");
		if (FileExists(DefaultGameEngineConfig))
		{
			Log("Looking for DefaultMap in {0}", DefaultGameEngineConfig);
			DefaultMap = GetDefaultMapFromIni(DefaultGameEngineConfig, Params.DedicatedServer);
			if (DefaultMap == null && Params.DedicatedServer)
			{
				DefaultMap = GetDefaultMapFromIni(DefaultGameEngineConfig, false);
			}
		}
		else
		{
			var BaseEngineConfig = CombinePaths(CmdEnv.LocalRoot, "Config", "BaseEngine.ini");
			if (FileExists(BaseEngineConfig))
			{
				Log("Looking for DefaultMap in {0}", BaseEngineConfig);
				DefaultMap = GetDefaultMapFromIni(BaseEngineConfig, Params.DedicatedServer);
				if (DefaultMap == null && Params.DedicatedServer)
				{
					DefaultMap = GetDefaultMapFromIni(BaseEngineConfig, false);
				}
			}
		}
		// We check for null here becase null == not found
		if (DefaultMap == null)
		{
			Log("No DefaultMap found, assuming: {0}", EngineEntryMap);
			DefaultMap = EngineEntryMap;
		}
		else
		{
			Log("Found DefaultMap={0}", DefaultMap);
		}
		return DefaultMap;
	}

	private string GetDefaultMapFromIni(string IniFilename, bool DedicatedServer)
	{
		var IniLines = ReadAllLines(IniFilename);
		string DefaultMap = null;

		string ConfigKeyStr = "GameDefaultMap";
		if (DedicatedServer)
		{
			ConfigKeyStr = "ServerDefaultMap";
		}

		foreach (var Line in IniLines)
		{
			if (Line.StartsWith(ConfigKeyStr, StringComparison.InvariantCultureIgnoreCase))
			{
				var DefaultMapPair = Line.Split('=');
				DefaultMap = DefaultMapPair[1].Trim();
			}
			if (DefaultMap != null)
			{
				break;
			}
		}
		return DefaultMap;
	}

	#endregion

	#region BuildCookRun

	protected void DoBuildCookRun(ProjectParams Params)
	{
		const ProjectBuildTargets ClientTargets = ProjectBuildTargets.ClientCooked | ProjectBuildTargets.ServerCooked;
        bool bGenerateNativeScripts = Params.RunAssetNativization;
		int WorkingCL = -1;
		if (P4Enabled && GlobalCommandLine.Submit && AllowSubmit)
		{
			WorkingCL = P4.CreateChange(P4Env.Client, String.Format("{0} build from changelist {1}", Params.ShortProjectName, P4Env.Changelist));
		}

        Project.Build(this, Params, WorkingCL, bGenerateNativeScripts ? (ProjectBuildTargets.All & ~ClientTargets) : ProjectBuildTargets.All);
		Project.Cook(Params);
        if (bGenerateNativeScripts)
        {
            // crash reporter is built along with client targets, so we need to 
            // include that target flag here as well - note: that its not folded
            // into ClientTargets because the editor needs its own CrashReporter 
            // as well (which would be built above)
            Project.Build(this, Params, WorkingCL, ClientTargets | ProjectBuildTargets.CrashReporter);
        }
        
		Project.CopyBuildToStagingDirectory(Params);
		Project.Package(Params, WorkingCL);
		Project.Archive(Params);
		Project.Deploy(Params);
		PrintRunTime();
		Project.Run(Params);

		// Check everything in!
		if (WorkingCL != -1)
		{
			int SubmittedCL;
			P4.Submit(WorkingCL, out SubmittedCL, true, true);
		}
	}

	private void MakeForeignSample()
	{
		string Sample = "BlankProject";
		var DestSample = ParseParamValue("DestSample", "CopiedBlankProject");
		var Src = CombinePaths(CmdEnv.LocalRoot, "Samples", "SampleGames", Sample);
		if (!DirectoryExists(Src))
		{
			throw new AutomationException("Can't find source directory to make foreign sample {0}.", Src);
		}

		var Dest = ParseParamValue("ForeignDest", CombinePaths(@"C:\testue4\foreign\", DestSample + "_ _Dir"));
		Log("Make a foreign sample {0} -> {1}", Src, Dest);

		CloneDirectory(Src, Dest);

		DeleteDirectory_NoExceptions(CombinePaths(Dest, "Intermediate"));
		DeleteDirectory_NoExceptions(CombinePaths(Dest, "Saved"));

		RenameFile(CombinePaths(Dest, Sample + ".uproject"), CombinePaths(Dest, DestSample + ".uproject"));

		var IniFile = CombinePaths(Dest, "Config", "DefaultEngine.ini");
		var Ini = new VersionFileUpdater(new FileReference(IniFile));
		Ini.ReplaceLine("GameName=", DestSample);
		Ini.Commit();
	}

	private void MakeForeignCodeSample()
	{
		string Sample = "PlatformerGame";
		string DestSample = "PlatformerGame";
		var Src = CombinePaths(CmdEnv.LocalRoot, Sample);
		if (!DirectoryExists(Src))
		{
			throw new AutomationException("Can't find source directory to make foreign sample {0}.", Src);
		}

		var Dest = ParseParamValue("ForeignDest", CombinePaths(@"C:\testue4\foreign\", DestSample + "_ _Dir"));
		Log("Make a foreign sample {0} -> {1}", Src, Dest);

		CloneDirectory(Src, Dest);
		DeleteDirectory_NoExceptions(CombinePaths(Dest, "Intermediate"));
		DeleteDirectory_NoExceptions(CombinePaths(Dest, "Saved"));
		DeleteDirectory_NoExceptions(CombinePaths(Dest, "Plugins", "FootIK", "Intermediate"));

		//RenameFile(CombinePaths(Dest, Sample + ".uproject"), CombinePaths(Dest, DestSample + ".uproject"));

		var IniFile = CombinePaths(Dest, "Config", "DefaultEngine.ini");
		var Ini = new VersionFileUpdater(new FileReference(IniFile));
		Ini.ReplaceLine("GameName=", DestSample);
		Ini.Commit();
	}

	private FileReference ProjectFullPath;
	public virtual FileReference ProjectPath
	{
		get
		{
			if (ProjectFullPath == null)
			{
				var bForeign = ParseParam("foreign");
				var bForeignCode = ParseParam("foreigncode");
				if (bForeign)
				{
					var DestSample = ParseParamValue("DestSample", "CopiedHoverShip");
					var Dest = ParseParamValue("ForeignDest", CombinePaths(@"C:\testue4\foreign\", DestSample + "_ _Dir"));
					ProjectFullPath = new FileReference(CombinePaths(Dest, DestSample + ".uproject"));
				}
				else if (bForeignCode)
				{
					var DestSample = ParseParamValue("DestSample", "PlatformerGame");
					var Dest = ParseParamValue("ForeignDest", CombinePaths(@"C:\testue4\foreign\", DestSample + "_ _Dir"));
					ProjectFullPath = new FileReference(CombinePaths(Dest, DestSample + ".uproject"));
				}
				else
				{
					var OriginalProjectName = ParseParamValue("project", "");

                    if (string.IsNullOrEmpty(OriginalProjectName))
                    {
                        throw new AutomationException("No project file specified. Use -project=<project>.");
                    }

					var ProjectName = OriginalProjectName;
					ProjectName = ProjectName.Trim(new char[] { '\"' });
					if (ProjectName.IndexOfAny(new char[] { '\\', '/' }) < 0)
					{
						ProjectName = CombinePaths(CmdEnv.LocalRoot, ProjectName, ProjectName + ".uproject");
					}
					else if (!FileExists_NoExceptions(ProjectName))
					{
						ProjectName = CombinePaths(CmdEnv.LocalRoot, ProjectName);
					}
					if(FileExists_NoExceptions(ProjectName))
					{
						ProjectFullPath = new FileReference(ProjectName);
					}
					else
					{
						var Branch = new BranchInfo(new List<UnrealTargetPlatform> { UnrealBuildTool.BuildHostPlatform.Current.Platform });
						var GameProj = Branch.FindGame(OriginalProjectName);
						if (GameProj != null)
						{
							ProjectFullPath = GameProj.FilePath;
						}
						if (ProjectFullPath == null || !FileExists_NoExceptions(ProjectFullPath.FullName))
						{
							throw new AutomationException("Could not find a project file {0}.", ProjectName);
						}
					}
				}
			}
			return ProjectFullPath;
		}
	}

	#endregion
}
