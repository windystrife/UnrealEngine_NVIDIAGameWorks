// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for the submit task
	/// </summary>
	public class SubmitTaskParameters
	{
		/// <summary>
		/// The description for the submitted changelist
		/// </summary>
		[TaskParameter]
		public string Description;

		/// <summary>
		/// The files to submit
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files;

		/// <summary>
		/// The Perforce file type for the submitted files (eg. binary+FS32)
		/// </summary>
		[TaskParameter(Optional = true)]
		public string FileType;

		/// <summary>
		/// The workspace name. If specified, a new workspace will be created using the given stream and root directory to submit the files. If not, the current workspace will be used.
		/// </summary>
		[TaskParameter(Optional=true)]
		public string Workspace;

		/// <summary>
		/// The stream for the workspace; defaults to the current stream. Ignored unless If the Workspace attribute is also specified.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Stream;

		/// <summary>
		/// Root directory for the stream. If not specified, defaults to the current root directory.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.DirectoryName)]
		public string RootDir;

		/// <summary>
		/// Whether to revert unchanged files before attempting to submit
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool RevertUnchanged;
	}

	/// <summary>
	/// Creates a new changelist and submits a set of files to a Perforce stream.
	/// </summary>
	[TaskElement("Submit", typeof(SubmitTaskParameters))]
	public class SubmitTask : CustomTask
	{
		/// <summary>
		/// Parameters for the task
		/// </summary>
		SubmitTaskParameters Parameters;

		/// <summary>
		/// Construct a version task
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public SubmitTask(SubmitTaskParameters InParameters)
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
			HashSet<FileReference> Files = ResolveFilespec(CommandUtils.RootDirectory, Parameters.Files, TagNameToFileSet);
			if (CommandUtils.AllowSubmit && Files.Count > 0)
			{
				// Get the connection that we're going to submit with
				P4Connection SubmitP4 = CommandUtils.P4;
				if (Parameters.Workspace != null)
				{
					// Create a brand new workspace
					P4ClientInfo Client = new P4ClientInfo();
					Client.Owner = CommandUtils.P4Env.User;
					Client.Host = Environment.MachineName;
					Client.Stream = Parameters.Stream ?? CommandUtils.P4Env.Branch;
					Client.RootPath = Parameters.RootDir ?? CommandUtils.RootDirectory.FullName;
					Client.Name = Parameters.Workspace;
					Client.Options = P4ClientOption.NoAllWrite | P4ClientOption.Clobber | P4ClientOption.NoCompress | P4ClientOption.Unlocked | P4ClientOption.NoModTime | P4ClientOption.RmDir;
					Client.LineEnd = P4LineEnd.Local;
					CommandUtils.P4.CreateClient(Client, AllowSpew: false);

					// Create a new connection for it
					SubmitP4 = new P4Connection(Client.Owner, Client.Name);
				}

				// Get the latest version of it
				int NewCL = SubmitP4.CreateChange(Description: Parameters.Description);
				foreach(FileReference File in Files)
				{
					SubmitP4.Revert(String.Format("-k \"{0}\"", File.FullName));
					SubmitP4.Sync(String.Format("-k \"{0}\"", File.FullName), AllowSpew: false);
					SubmitP4.Add(NewCL, String.Format("\"{0}\"", File.FullName));
					SubmitP4.Edit(NewCL, String.Format("\"{0}\"", File.FullName));
					if (Parameters.FileType != null)
					{
						SubmitP4.P4(String.Format("reopen -t \"{0}\" \"{1}\"", Parameters.FileType, File.FullName), AllowSpew: false);
					}
				}

				// Revert any unchanged files
				if(Parameters.RevertUnchanged)
				{
					SubmitP4.RevertUnchanged(NewCL);
					if(SubmitP4.TryDeleteEmptyChange(NewCL))
					{
						CommandUtils.Log("No files to submit; ignored.");
						return;
					}
				}

				// Submit it
				int SubmittedCL;
				SubmitP4.Submit(NewCL, out SubmittedCL);
				if (SubmittedCL <= 0)
				{
					throw new AutomationException("Submit failed.");
				}
				CommandUtils.Log("Submitted in changelist {0}", SubmittedCL);
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
