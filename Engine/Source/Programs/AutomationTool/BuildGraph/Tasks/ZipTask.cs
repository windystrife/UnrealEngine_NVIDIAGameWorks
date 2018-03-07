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
	/// Parameters for a zip task
	/// </summary>
	public class ZipTaskParameters
	{
		/// <summary>
		/// The directory to read compressed files from
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.DirectoryName)]
		public string FromDir;

		/// <summary>
		/// List of file specifications separated by semicolons (eg. *.cpp;Engine/.../*.bat), or the name of a tag set. Relative paths are taken from FromDir.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files;

		/// <summary>
		/// The zip file to create
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileName)]
		public string ZipFile;

		/// <summary>
		/// Tag to be applied to the created zip file
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;
	}

	/// <summary>
	/// Compresses files into a zip archive.
	/// </summary>
	[TaskElement("Zip", typeof(ZipTaskParameters))]
	public class ZipTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		ZipTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public ZipTask(ZipTaskParameters InParameters)
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
			DirectoryReference FromDir = ResolveDirectory(Parameters.FromDir);

			// Find all the input files
			IEnumerable<FileReference> Files;
			if(Parameters.Files == null)
			{
				Files = DirectoryReference.EnumerateFiles(FromDir, "*", System.IO.SearchOption.AllDirectories);
			}
			else
			{
				Files = ResolveFilespec(FromDir, Parameters.Files, TagNameToFileSet);
			}

			// Create the zip file
			FileReference ArchiveFile = ResolveFile(Parameters.ZipFile);
			CommandUtils.ZipFiles(ArchiveFile, FromDir, Files);

			// Apply the optional tag to the produced archive
			foreach(string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).Add(ArchiveFile);
			}

			// Add the archive to the set of build products
			BuildProducts.Add(ArchiveFile);
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
			return FindTagNamesFromFilespec(Parameters.Files);
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
