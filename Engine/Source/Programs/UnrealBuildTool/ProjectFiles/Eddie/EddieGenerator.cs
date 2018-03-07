// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{

	class EddieProjectFolder : MasterProjectFolder
	{
		public EddieProjectFolder(ProjectFileGenerator InitOwnerProjectFileGenerator, string InitFolderName)
			: base(InitOwnerProjectFileGenerator, InitFolderName)
		{
		}
	}

	class EddieProjectFileGenerator : ProjectFileGenerator
	{
		public EddieProjectFileGenerator(FileReference InOnlyGameProject)
			: base(InOnlyGameProject)
		{
		}
		
		override public string ProjectFileExtension
		{
			get
			{
				return ".wkst";
			}
		}
		
		public override void CleanProjectFiles(DirectoryReference InMasterProjectDirectory, string InMasterProjectName, DirectoryReference InIntermediateProjectFilesPath)
		{
			FileReference MasterProjDeleteFilename = FileReference.Combine(InMasterProjectDirectory, InMasterProjectName + ".wkst");
			if (FileReference.Exists(MasterProjDeleteFilename))
			{
				File.Delete(MasterProjDeleteFilename.FullName);
			}

			// Delete the project files folder
			if (DirectoryReference.Exists(InIntermediateProjectFilesPath))
			{
				try
				{
					Directory.Delete(InIntermediateProjectFilesPath.FullName, true);
				}
				catch (Exception Ex)
				{
					Log.TraceInformation("Error while trying to clean project files path {0}. Ignored.", InIntermediateProjectFilesPath);
					Log.TraceInformation("\t" + Ex.Message);
				}
			}
		}
		
		protected override ProjectFile AllocateProjectFile(FileReference InitFilePath)
		{
			return new EddieProjectFile(InitFilePath, OnlyGameProject);
		}
		
		public override MasterProjectFolder AllocateMasterProjectFolder(ProjectFileGenerator InitOwnerProjectFileGenerator, string InitFolderName)
		{
			return new EddieProjectFolder(InitOwnerProjectFileGenerator, InitFolderName);
		}
		
		private bool WriteEddieWorkset()
		{
			bool bSuccess = false;
			
			var WorksetDataContent = new StringBuilder();
			WorksetDataContent.Append("# @Eddie Workset@" + ProjectFileGenerator.NewLine);
			WorksetDataContent.Append("AddWorkset \"" + MasterProjectName + ".wkst\" \"" + MasterProjectPath + "\"" + ProjectFileGenerator.NewLine);
			
			System.Action< List<MasterProjectFolder> /* Folders */> AddProjectsFunction = null;
			AddProjectsFunction = (FolderList) =>
				{
					foreach (EddieProjectFolder CurFolder in FolderList)
					{
						WorksetDataContent.Append("AddFileGroup \"" + CurFolder.FolderName + "\" \"" + CurFolder.FolderName + "\"" + ProjectFileGenerator.NewLine);

						AddProjectsFunction(CurFolder.SubFolders);

						foreach (ProjectFile CurProject in CurFolder.ChildProjects)
						{
							EddieProjectFile EddieProject = CurProject as EddieProjectFile;
							if (EddieProject != null)
							{
								WorksetDataContent.Append("AddFile \"" + EddieProject.ToString() + "\" \"" + EddieProject.ProjectFilePath + "\"" + ProjectFileGenerator.NewLine);
							}
						}

						WorksetDataContent.Append("EndFileGroup \"" + CurFolder.FolderName + "\"" + ProjectFileGenerator.NewLine);
					}
				};
			AddProjectsFunction(RootFolder.SubFolders);
			
			string ProjectName = MasterProjectName;
			var FilePath = MasterProjectPath + "/" + ProjectName + ".wkst";
			bSuccess = WriteFileIfChanged(FilePath, WorksetDataContent.ToString(), new UTF8Encoding());
			
			return bSuccess;
		}
		
		protected override bool WriteMasterProjectFile(ProjectFile UBTProject)
		{
			return WriteEddieWorkset();
		}
		
		protected override void ConfigureProjectFileGeneration(string[] Arguments, ref bool IncludeAllPlatforms)
		{
			// Call parent implementation first
			base.ConfigureProjectFileGeneration(Arguments, ref IncludeAllPlatforms);

			if (bGeneratingGameProjectFiles)
			{
				bIncludeEngineSource = true;
			}
		}
	}
}