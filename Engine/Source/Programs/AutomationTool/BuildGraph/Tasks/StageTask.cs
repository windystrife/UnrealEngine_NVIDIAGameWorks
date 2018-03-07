// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace BuildGraph.Tasks
{
	/// <summary>
	/// Parameters for the staging task
	/// </summary>
	public class StageTaskParameters
	{
		/// <summary>
		/// The project that this target belongs to
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.FileName)]
		public string Project;

		/// <summary>
		/// Name of the target to stage
		/// </summary>
		[TaskParameter]
		public string Target;

		/// <summary>
		/// Platform to stage
		/// </summary>
		[TaskParameter]
		public UnrealTargetPlatform Platform;

		/// <summary>
		/// Configuration to be staged
		/// </summary>
		[TaskParameter]
		public UnrealTargetConfiguration Configuration;

		/// <summary>
		/// Architecture to be staged
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Architecture;

		/// <summary>
		/// Directory the receipt files should be staged to
		/// </summary>
		[TaskParameter]
		public string ToDir;

		/// <summary>
		/// Whether to overwrite existing files
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool Overwrite;

		/// <summary>
		/// Tag to be applied to build products of this task
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;
	}

	/// <summary>
	/// Stages files listed in a build receipt to an output directory.
	/// </summary>
	[TaskElement("Stage", typeof(StageTaskParameters))]
	public class StageTask : CustomTask
	{
		/// <summary>
		/// Parameters for the task
		/// </summary>
		StageTaskParameters Parameters;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public StageTask(StageTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		public override void Execute(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Get the project path, and check it exists
			FileReference ProjectFile = null;
			if(Parameters.Project != null)
			{
				ProjectFile = ResolveFile(Parameters.Project);
				if(!FileReference.Exists(ProjectFile))
				{
					throw new AutomationException("Couldn't find project '{0}'", ProjectFile.FullName);
				}
			}

			// Get the directories used for staging this project
			DirectoryReference SourceEngineDir = CommandUtils.EngineDirectory;
			DirectoryReference SourceProjectDir = (ProjectFile == null)? SourceEngineDir : ProjectFile.Directory;

			// Get the output directories. We flatten the directory structure on output.
			DirectoryReference TargetDir = ResolveDirectory(Parameters.ToDir);
			DirectoryReference TargetEngineDir = DirectoryReference.Combine(TargetDir, "Engine");
			DirectoryReference TargetProjectDir = DirectoryReference.Combine(TargetDir, ProjectFile.GetFileNameWithoutExtension());

			// Get the path to the receipt
			FileReference ReceiptFileName = TargetReceipt.GetDefaultPath(SourceProjectDir, Parameters.Target, Parameters.Platform, Parameters.Configuration, Parameters.Architecture);

			// Try to load it
			TargetReceipt Receipt;
			if(!TargetReceipt.TryRead(ReceiptFileName, SourceEngineDir, SourceProjectDir, out Receipt))
			{
				throw new AutomationException("Couldn't read receipt '{0}'", ReceiptFileName);
			}

			// Stage all the build products needed at runtime
			HashSet<FileReference> SourceFiles = new HashSet<FileReference>();
			foreach(BuildProduct BuildProduct in Receipt.BuildProducts.Where(x => x.Type != BuildProductType.StaticLibrary && x.Type != BuildProductType.ImportLibrary))
			{
				SourceFiles.Add(BuildProduct.Path);
			}
			foreach(RuntimeDependency RuntimeDependency in Receipt.RuntimeDependencies.Where(x => x.Type != StagedFileType.UFS))
			{
				SourceFiles.Add(RuntimeDependency.Path);
			}

			// Get all the target files
			List<FileReference> TargetFiles = new List<FileReference>();
			foreach(FileReference SourceFile in SourceFiles)
			{
				// Get the destination file to copy to, mapping to the new engine and project directories as appropriate
				FileReference TargetFile;
				if(SourceFile.IsUnderDirectory(SourceEngineDir))
				{
					TargetFile = FileReference.Combine(TargetEngineDir, SourceFile.MakeRelativeTo(SourceEngineDir));
				}
				else
				{
					TargetFile = FileReference.Combine(TargetProjectDir, SourceFile.MakeRelativeTo(SourceProjectDir));
				}

				// Fixup the case of the output file. Would expect Platform.DeployLowerCaseFilenames() to return true here, but seems not to be the case.
				if(Parameters.Platform == UnrealTargetPlatform.PS4)
				{
					TargetFile = FileReference.Combine(TargetDir, TargetFile.MakeRelativeTo(TargetDir).ToLowerInvariant());
				}

				// Only copy the output file if it doesn't already exist. We can stage multiple targets to the same output directory.
				if(Parameters.Overwrite || !FileReference.Exists(TargetFile))
				{
					DirectoryReference.CreateDirectory(TargetFile.Directory);
					CommandUtils.CopyFile(SourceFile.FullName, TargetFile.FullName);
				}

				// Add it to the list of target files
				TargetFiles.Add(TargetFile);
			}

			// Apply the optional tag to the build products
			foreach(string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(TargetFiles);
			}

			// Add the target file to the list of build products
			BuildProducts.UnionWith(TargetFiles);
		}

		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}

		/// <summary>
		/// Find all the tags which are used as inputs to this task
		/// </summary>
		/// <returns>The tag names which are read by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			yield break;
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			return FindTagNamesFromList(Parameters.Tag);
		}
	}
}
