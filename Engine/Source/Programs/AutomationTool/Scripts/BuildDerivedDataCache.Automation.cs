// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using UnrealBuildTool;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;
using Tools.DotNETCommon;

public class BuildDerivedDataCache : BuildCommand
{
	public override void ExecuteBuild()
	{
		// Get the list of platform names
		string[] FeaturePacks = ParseParamValue("FeaturePacks").Split(';');
		string TempDir = ParseParamValue("TempDir");
		UnrealTargetPlatform HostPlatform = (UnrealTargetPlatform)Enum.Parse(typeof(UnrealTargetPlatform), ParseParamValue("HostPlatform"));
		string TargetPlatforms = ParseParamValue("TargetPlatforms");
		string SavedDir = ParseParamValue("SavedDir");


		// Get paths to everything within the temporary directory
		string EditorExe = CommandUtils.GetEditorCommandletExe(TempDir, HostPlatform);
		string RelativePakPath = "Engine/DerivedDataCache/Compressed.ddp";
		string OutputPakFile = CommandUtils.CombinePaths(TempDir, RelativePakPath);
		string OutputCsvFile = Path.ChangeExtension(OutputPakFile, ".csv");


		List<string> ProjectPakFiles = new List<string>();
		List<string> FeaturePackPaths = new List<string>();
		// loop through all the projects first and bail out if one of them doesn't exist.
		foreach (string FeaturePack in FeaturePacks)
		{
			if (!String.IsNullOrWhiteSpace(FeaturePack))
			{
				string FeaturePackPath = CommandUtils.CombinePaths(CommandUtils.RootDirectory.FullName, FeaturePack);
				if (!CommandUtils.FileExists(FeaturePackPath))
				{
					throw new AutomationException("Could not find project: " + FeaturePack);
				}
				FeaturePackPaths.Add(FeaturePackPath);
			}
		}

		// loop through all the paths and generate ddc data for them
		foreach (string FeaturePackPath in FeaturePackPaths)
		{
			string ProjectSpecificPlatforms = TargetPlatforms;
			FileReference FileRef = new FileReference(FeaturePackPath);
			string GameName = FileRef.GetFileNameWithoutAnyExtensions();
			ProjectDescriptor Project = ProjectDescriptor.FromFile(FileRef);

			if (Project.TargetPlatforms != null && Project.TargetPlatforms.Length > 0)
			{
				// Restrict target platforms used to those specified in project file
				List<string> FilteredPlatforms = new List<string>();

				// Always include the editor platform for cooking
				string EditorCookPlatform = Platform.GetPlatform(HostPlatform).GetEditorCookPlatform();
				if (TargetPlatforms.Contains(EditorCookPlatform))
				{
					FilteredPlatforms.Add(EditorCookPlatform);
				}

				foreach (string TargetPlatform in Project.TargetPlatforms)
				{
					if (TargetPlatforms.Contains(TargetPlatform))
					{
						FilteredPlatforms.Add(TargetPlatform);
					}
				}

				ProjectSpecificPlatforms = CommandUtils.CombineCommandletParams(FilteredPlatforms.Distinct().ToArray());
			}

			CommandUtils.Log("Generating DDC data for {0} on {1}", GameName, ProjectSpecificPlatforms);
			CommandUtils.DDCCommandlet(FileRef, EditorExe, null, ProjectSpecificPlatforms, "-fill -DDC=CreateInstalledEnginePak -ProjectOnly");

			string ProjectPakFile = CommandUtils.CombinePaths(Path.GetDirectoryName(OutputPakFile), String.Format("Compressed-{0}.ddp", GameName));
			CommandUtils.DeleteFile(ProjectPakFile);
			CommandUtils.RenameFile(OutputPakFile, ProjectPakFile);

			string ProjectCsvFile = Path.ChangeExtension(ProjectPakFile, ".csv");
			CommandUtils.DeleteFile(ProjectCsvFile);
			CommandUtils.RenameFile(OutputCsvFile, ProjectCsvFile);

			ProjectPakFiles.Add(Path.GetFileName(ProjectPakFile));

		}

		// Generate DDC for the editor, and merge all the other PAK files in
		CommandUtils.Log("Generating DDC data for engine content on {0}", TargetPlatforms);
		CommandUtils.DDCCommandlet(null, EditorExe, null, TargetPlatforms, "-fill -DDC=CreateInstalledEnginePak " + CommandUtils.MakePathSafeToUseWithCommandLine("-MergePaks=" + String.Join("+", ProjectPakFiles)));

		string SavedPakFile = CommandUtils.CombinePaths(SavedDir, RelativePakPath);
		CommandUtils.CopyFile(OutputPakFile, SavedPakFile);
	}
}

