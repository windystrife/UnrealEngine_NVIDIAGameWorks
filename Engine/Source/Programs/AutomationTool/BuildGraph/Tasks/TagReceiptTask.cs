// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for the Tag Receipt task.
	/// </summary>
	public class TagReceiptTaskParameters
	{
		/// <summary>
		/// Set of receipt files (*.target) to read, including wildcards and tag names, separated by semicolons.
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files;

		/// <summary>
		/// Path to the Engine folder, used to expand $(EngineDir) properties in receipt files. Defaults to the Engine directory for the current workspace.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.DirectoryName)]
		public string EngineDir;

		/// <summary>
		/// Path to the project folder, used to expand $(ProjectDir) properties in receipt files. Defaults to the Engine directory for the current workspace.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.DirectoryName)]
		public string ProjectDir;

		/// <summary>
		/// Whether to tag the Build Products listed in receipts
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool BuildProducts;

		/// <summary>
		/// Which type of Build Products to tag (See TargetReceipt.cs - UnrealBuildTool.BuildProductType for valid values)
		/// </summary>
		[TaskParameter(Optional = true)]
		public string BuildProductType;

		/// <summary>
		/// Whether to tag the Runtime Dependencies listed in receipts
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool RuntimeDependencies;

		/// <summary>
		/// Which type of Runtime Dependencies to tag (See TargetReceipt.cs - UnrealBuildTool.StagedFileType for valid values)
		/// </summary>
		[TaskParameter(Optional = true)]
		public string StagedFileType;

		/// <summary>
		/// Whether to tag the Precompiled Build Dependencies listed in receipts
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool PrecompiledBuildDependencies;

		/// <summary>
		/// Whether to tag the Precompiled Runtime Dependencies listed in receipts
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool PrecompiledRuntimeDependencies;

		/// <summary>
		/// Name of the tag to apply
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.TagList)]
		public string With;
	}

	/// <summary>
	/// Task which tags build products and/or runtime dependencies by reading from *.target files.
	/// </summary>
	[TaskElement("TagReceipt", typeof(TagReceiptTaskParameters))]
	class TagReceiptTask : CustomTask
	{
		/// <summary>
		/// Parameters to this task
		/// </summary>
		TagReceiptTaskParameters Parameters;

		BuildProductType BuildProductType;

		StagedFileType StagedFileType;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters to select which files to search</param>
		public TagReceiptTask(TagReceiptTaskParameters InParameters)
		{
			Parameters = InParameters;

			if (!String.IsNullOrEmpty(Parameters.BuildProductType))
			{
				BuildProductType = (BuildProductType)Enum.Parse(typeof(BuildProductType), Parameters.BuildProductType);
			}
			if (!String.IsNullOrEmpty(Parameters.StagedFileType))
			{
				StagedFileType = (StagedFileType)Enum.Parse(typeof(StagedFileType), Parameters.StagedFileType);
			}
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		public override void Execute(JobContext Job, HashSet<FileReference
			> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Set the Engine directory
			DirectoryReference EngineDir = DirectoryReference.Combine(CommandUtils.RootDirectory, "Engine");
			if (!String.IsNullOrEmpty(Parameters.EngineDir))
			{
				EngineDir = DirectoryReference.Combine(CommandUtils.RootDirectory, Parameters.EngineDir);
			}

			// Set the Project directory
			DirectoryReference ProjectDir = DirectoryReference.Combine(CommandUtils.RootDirectory, "Engine");
			if (!String.IsNullOrEmpty(Parameters.ProjectDir))
			{
				ProjectDir = DirectoryReference.Combine(CommandUtils.RootDirectory, Parameters.ProjectDir);
			}

			// Resolve the input list
			IEnumerable<FileReference> TargetFiles = ResolveFilespec(CommandUtils.RootDirectory, Parameters.Files, TagNameToFileSet);
			HashSet<FileReference> Files = new HashSet<FileReference>();

			foreach (FileReference TargetFile in TargetFiles)
			{
				// check all files are .target files
				if (TargetFile.GetExtension() != ".target")
				{
					throw new AutomationException("Invalid file passed to TagReceipt task ({0})", TargetFile.FullName);
				}

				// Read the receipt
				TargetReceipt Receipt;
				if (!TargetReceipt.TryRead(TargetFile, EngineDir, ProjectDir, out Receipt))
				{
					CommandUtils.LogWarning("Unable to load file using TagReceipt task ({0})", TargetFile.FullName);
					continue;
				}

				if (Parameters.BuildProducts)
				{
					foreach (BuildProduct BuildProduct in Receipt.BuildProducts)
					{
						if (String.IsNullOrEmpty(Parameters.BuildProductType) || BuildProduct.Type == BuildProductType)
						{
							Files.Add(BuildProduct.Path);
						}
					}
				}

				if (Parameters.RuntimeDependencies)
				{
					foreach (RuntimeDependency RuntimeDependency in Receipt.RuntimeDependencies)
					{
						if (String.IsNullOrEmpty(Parameters.StagedFileType) || RuntimeDependency.Type == StagedFileType)
						{
							// Only add files that exist as dependencies are assumed to always exist
							FileReference DependencyPath = RuntimeDependency.Path;
							if (FileReference.Exists(DependencyPath))
							{
								Files.Add(DependencyPath);
							}
							else
							{
								CommandUtils.LogWarning("File listed as RuntimeDependency in {0} does not exist ({1})", TargetFile.FullName, DependencyPath.FullName);
							}
						}
					}
				}

				if (Parameters.PrecompiledBuildDependencies)
				{
					foreach(FileReference PrecompiledBuildDependency in Receipt.PrecompiledBuildDependencies)
					{
						// Only add files that exist as dependencies are assumed to always exist
						FileReference DependencyPath = PrecompiledBuildDependency;
						if (FileReference.Exists(DependencyPath))
						{
							Files.Add(DependencyPath);
						}
						else
						{
							CommandUtils.LogWarning("File listed as PrecompiledBuildDependency in {0} does not exist ({1})", TargetFile.FullName, DependencyPath.FullName);
						}
					}
				}

				if (Parameters.PrecompiledRuntimeDependencies)
				{
					foreach (FileReference PrecompiledRuntimeDependency in Receipt.PrecompiledRuntimeDependencies)
					{
						// Only add files that exist as dependencies are assumed to always exist
						FileReference DependencyPath = PrecompiledRuntimeDependency;
						if (FileReference.Exists(DependencyPath))
						{
							Files.Add(DependencyPath);
						}
						else
						{
							CommandUtils.LogWarning("File listed as PrecompiledRuntimeDependency in {0} does not exist ({1})", TargetFile.FullName, DependencyPath.FullName);
						}
					}
				}
			}

			// Apply the tag to all the matching files
			FindOrAddTagSet(TagNameToFileSet, Parameters.With).UnionWith(Files);
		}

		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}

		/// <summary>
		/// Find all the tags which are required by this task
		/// </summary>
		/// <returns>The tag names which are required by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			return FindTagNamesFromFilespec(Parameters.Files);
		}

		/// <summary>
		/// Find all the referenced tags from tasks in this task
		/// </summary>
		/// <returns>The tag names which are produced/modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			return FindTagNamesFromList(Parameters.With);
		}
	}
}
