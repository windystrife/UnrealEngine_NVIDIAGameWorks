// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace BuildGraph.Tasks
{
	/// <summary>
	/// Parameters for a copy task
	/// </summary>
	public class DeleteTaskParameters
	{
		/// <summary>
		/// List of file specifications separated by semicolons (eg. *.cpp;Engine/.../*.bat), or the name of a tag set
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files;

		/// <summary>
		/// Whether to delete empty directories after deleting the files. Defaults to true.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool DeleteEmptyDirectories = true;
	}

	/// <summary>
	/// Delete a set of files.
	/// </summary>
	[TaskElement("Delete", typeof(DeleteTaskParameters))]
	public class DeleteTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		DeleteTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public DeleteTask(DeleteTaskParameters InParameters)
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
			// Find all the referenced files and delete them
			HashSet<FileReference> Files = ResolveFilespec(CommandUtils.RootDirectory, Parameters.Files, TagNameToFileSet);
			foreach(FileReference File in Files)
			{
				if (!InternalUtils.SafeDeleteFile(File.FullName))
				{
					CommandUtils.LogWarning("Couldn't delete file {0}", File.FullName);
				}
			}

			// Try to delete all the parent directories. Keep track of the directories we've already deleted to avoid hitting the disk.
			if(Parameters.DeleteEmptyDirectories)
			{
				// Find all the directories that we're touching
				HashSet<DirectoryReference> ParentDirectories = new HashSet<DirectoryReference>();
				foreach(FileReference File in Files)
				{
					ParentDirectories.Add(File.Directory);
				}

				// Recurse back up from each of those directories to the root folder
				foreach(DirectoryReference ParentDirectory in ParentDirectories)
				{
					for(DirectoryReference CurrentDirectory = ParentDirectory; CurrentDirectory != CommandUtils.RootDirectory; CurrentDirectory = CurrentDirectory.ParentDirectory)
					{
						if(!TryDeleteEmptyDirectory(CurrentDirectory))
						{
							break;
						}
					}
				}
			}
		}

		/// <summary>
		/// Deletes a directory, if it's empty
		/// </summary>
		/// <param name="CandidateDirectory">The directory to check</param>
		/// <returns>True if the directory was deleted, false if not</returns>
		static bool TryDeleteEmptyDirectory(DirectoryReference CandidateDirectory)
		{
			// Make sure the directory exists
			if(!DirectoryReference.Exists(CandidateDirectory))
			{
				return false;
			}

			// Check if there are any files in it. If there are, don't bother trying to delete it.
			if(Directory.EnumerateFiles(CandidateDirectory.FullName).Any() || Directory.EnumerateDirectories(CandidateDirectory.FullName).Any())
			{
				return false;
			}

			// Try to delete the directory.
			try
			{
				Directory.Delete(CandidateDirectory.FullName);
				return true;
			}
			catch(Exception Ex)
			{
				CommandUtils.LogWarning("Couldn't delete directory {0} ({1})", CandidateDirectory.FullName, Ex.Message);
				return false;
			}
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
			yield break;
		}
	}
}
