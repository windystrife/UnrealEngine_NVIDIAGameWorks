// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using OneSky;
using EpicGames.OneSkyLocalization.Config;

class LauncherLocalization : BuildCommand
{

	public override void ExecuteBuild()
	{
		if (ParseParam("BuildEditor"))
		{
			UE4Build.BuildAgenda Agenda = new UE4Build.BuildAgenda();
			Agenda.AddTarget("UE4Editor", HostPlatform.Current.HostEditorPlatform, UnrealTargetConfiguration.Development);
			Agenda.AddTarget("ShaderCompileWorker", HostPlatform.Current.HostEditorPlatform, UnrealTargetConfiguration.Development);

			UE4Build Builder = new UE4Build(this);
			Builder.Build(Agenda, InDeleteBuildProducts: true, InUpdateVersionFiles: true, InForceNoXGE: true);
		}

		var EditorExe = CombinePaths(CmdEnv.LocalRoot, @"Engine/Binaries/Win64/UE4Editor-Cmd.exe");

		if (P4Enabled)
		{
			Log("Sync necessary content to head revision");
			P4.Sync(P4Env.Branch + "/Engine/Config/...");
			P4.Sync(P4Env.Branch + "/Engine/Content/...");
			P4.Sync(P4Env.Branch + "/Engine/Source/...");

            P4.Sync(P4Env.Branch + "/Portal/Config/...");
            P4.Sync(P4Env.Branch + "/Portal/Content/...");
            P4.Sync(P4Env.Branch + "/Portal/Source/...");
		}

		OneSkyConfigData OneSkyConfig = OneSkyConfigHelper.Find("OneSkyConfig_EpicGames");
		var oneSkyService = new OneSkyService(OneSkyConfig.ApiKey, OneSkyConfig.ApiSecret);

		// Export Launcher text from OneSky
		{
			var launcherGroup = GetLauncherGroup(oneSkyService);
			var appProject = GetAppProject(oneSkyService);
			var appFile = appProject.UploadedFiles.FirstOrDefault(f => f.Filename == "App.po");

			//Export
			if (appFile != null)
			{
				ExportFileToDirectory(appFile, new DirectoryInfo(CmdEnv.LocalRoot + "/Portal/Content/Localization/App"), launcherGroup.EnabledCultures);
			}
		}

		// Setup editor arguments for SCC.
		string EditorArguments = String.Empty;
		if (P4Enabled)
		{
			EditorArguments = String.Format("-SCCProvider={0} -P4Port={1} -P4User={2} -P4Client={3} -P4Passwd={4}", "Perforce", P4Env.ServerAndPort, P4Env.User, P4Env.Client, P4.GetAuthenticationToken());
		}
		else
		{
			EditorArguments = String.Format("-SCCProvider={0}", "None");
		}

		// Setup commandlet arguments for SCC.
		string CommandletSCCArguments = String.Empty;
		if (P4Enabled) { CommandletSCCArguments += "-EnableSCC"; }
		if (!AllowSubmit) { CommandletSCCArguments += (string.IsNullOrEmpty(CommandletSCCArguments) ? "" : " ") + "-DisableSCCSubmit"; }

		// Setup commandlet arguments with configurations.
		var CommandletArgumentSets = new string[] 
			{
				String.Format("-config={0}", @"../Portal/Config/Localization/App.ini") + (string.IsNullOrEmpty(CommandletSCCArguments) ? "" : " " + CommandletSCCArguments)
			};

		// Execute commandlet for each set of arguments.
		foreach (var CommandletArguments in CommandletArgumentSets)
		{
			Log("Localization for {0} {1}", EditorArguments, CommandletArguments);

			Log("Running UE4Editor to generate Localization data");

			string Arguments = String.Format("-run=GatherText {0} {1}", EditorArguments, CommandletArguments);
			var RunResult = Run(EditorExe, Arguments);

			if (RunResult.ExitCode != 0)
			{
				throw new AutomationException("Error while executing localization commandlet '{0}'", Arguments);
			}
		}

		// Upload Launcher text to OneSky
		UploadDirectoryToProject(GetAppProject(oneSkyService), new DirectoryInfo(CmdEnv.LocalRoot + "/Portal/Content/Localization/App"), "*.po");
	}

	private static ProjectGroup GetLauncherGroup(OneSkyService oneSkyService)
	{
		var launcherGroup = oneSkyService.ProjectGroups.FirstOrDefault(g => g.Name == "Launcher");

		if (launcherGroup == null)
		{
			launcherGroup = new ProjectGroup("Launcher", "en");
			oneSkyService.ProjectGroups.Add(launcherGroup);
		}

		return launcherGroup;
	}

	private static OneSky.Project GetAppProject(OneSkyService oneSkyService)
	{
		var launcherGroup = GetLauncherGroup(oneSkyService);

		OneSky.Project appProject = launcherGroup.Projects.FirstOrDefault(p => p.Name == "App");

		if (appProject == null)
		{
			ProjectType projectType = oneSkyService.ProjectTypes.First(pt => pt.Code == "website");

			appProject = new OneSky.Project("App", "The core application text that ships with the Launcher", projectType);
			launcherGroup.Projects.Add(appProject);
		}

		return appProject;
	}

	private static void ExportFileToDirectory(UploadedFile file, DirectoryInfo destination, IEnumerable<string> cultures)
	{
		foreach (var culture in cultures)
		{
			string finalCulture = culture;
			if (culture == "es")
			{
				// we no longer export es, instead replacing it with es-ES
				continue;
			}
			else if (culture == "es-ES")
			{
				finalCulture = "es";
			}

			var cultureDirectory = new DirectoryInfo(Path.Combine(destination.FullName, finalCulture));
			if (!cultureDirectory.Exists)
			{
				cultureDirectory.Create();
			}

			using (var memoryStream = new MemoryStream())
			{
				var exportFile = new FileInfo(Path.Combine(cultureDirectory.FullName, file.Filename));

				var exportTranslationState = file.ExportTranslation(culture, memoryStream).Result;
				if (exportTranslationState == UploadedFile.ExportTranslationState.Success)
				{
					memoryStream.Position = 0;
					using (Stream fileStream = File.Open(exportFile.FullName, FileMode.Create))
					{
						memoryStream.CopyTo(fileStream);
						Console.WriteLine("[SUCCESS] Exporting: " + exportFile.FullName + " Source Locale: " + culture + " Target Locale: " + finalCulture);
					}
				}
				else if (exportTranslationState == UploadedFile.ExportTranslationState.NoContent)
				{
						Console.WriteLine("[WARNING] Exporting: " + exportFile.FullName + " Source Locale: " + culture + " Target Locale: " + finalCulture + " has no translations!");
				}
				else
				{
						Console.WriteLine("[FAILED] Exporting: " + exportFile.FullName + " Source Locale: " + culture + " Target Locale: " + finalCulture);
				}
			}
		}
	}

	static void UploadDirectoryToProject(OneSky.Project project, DirectoryInfo directory, string fileExtension)
	{
		foreach (var file in Directory.GetFiles(directory.FullName, fileExtension, SearchOption.AllDirectories))
		{
			DirectoryInfo parentDirectory = Directory.GetParent(file);
			string localeName = parentDirectory.Name;

            if (localeName == "es")
            {
                localeName = "es-ES";
            }

			string currentFile = file;

			using (var fileStream = File.OpenRead(currentFile))
			{
				// Read the BOM
				var bom = new byte[3];
				fileStream.Read(bom, 0, 3);

				//We want to ignore the utf8 BOM
				if (bom[0] != 0xef || bom[1] != 0xbb || bom[2] != 0xbf)
				{
					fileStream.Position = 0;
				}

				Console.WriteLine("Uploading: " + currentFile + " Locale: " + localeName);
				var uploadedFile = project.Upload(Path.GetFileName(currentFile), fileStream, localeName).Result;

				if (uploadedFile == null)
				{
					Console.WriteLine("[FAILED] Uploading: " + currentFile + " Locale: " + localeName);
				}
				else
				{
					Console.WriteLine("[SUCCESS] Uploading: " + currentFile + " Locale: " + localeName);
				}
			}
		}
	}
}
