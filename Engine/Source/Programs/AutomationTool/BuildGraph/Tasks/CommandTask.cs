// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using AutomationTool;
using UnrealBuildTool;
using System.Xml;
using System.IO;
using Tools.DotNETCommon;

namespace BuildGraph.Tasks
{
	static class StringExtensions
	{
		public static bool CaseInsensitiveContains(this string Text, string Value)
		{
			return System.Globalization.CultureInfo.InvariantCulture.CompareInfo.IndexOf(Text, Value, System.Globalization.CompareOptions.IgnoreCase) >= 0;
		}
	}

	/// <summary>
	/// Parameters for a task which calls another UAT command
	/// </summary>
	public class CommandTaskParameters
	{
		/// <summary>
		/// The command name to execute
		/// </summary>
		[TaskParameter]
		public string Name;

		/// <summary>
		/// Arguments to be passed to the command
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;

		/// <summary>
		/// If non-null, instructs telemetry from the command to be merged into the telemetry for this UAT instance with the given prefix. May be an empty (non-null) string.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string MergeTelemetryWithPrefix;
	}

	/// <summary>
	/// Invokes an AutomationTool child process to run the given command.
	/// </summary>
	[TaskElement("Command", typeof(CommandTaskParameters))]
	public class CommandTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		CommandTaskParameters Parameters;

		/// <summary>
		/// Construct a new CommandTask.
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public CommandTask(CommandTaskParameters InParameters)
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
			// If we're merging telemetry from the child process, get a temp filename for it
			FileReference TelemetryFile = null;
			if (Parameters.MergeTelemetryWithPrefix != null)
			{
				TelemetryFile = FileReference.Combine(CommandUtils.RootDirectory, "Engine", "Intermediate", "UAT", "Telemetry.json");
				DirectoryReference.CreateDirectory(TelemetryFile.Directory);
			}

			// Run the command
			StringBuilder CommandLine = new StringBuilder();
			if (Parameters.Arguments == null || (!Parameters.Arguments.CaseInsensitiveContains("-p4") && !Parameters.Arguments.CaseInsensitiveContains("-nop4")))
			{
				CommandLine.AppendFormat("{0} ", CommandUtils.P4Enabled ? "-p4" : "-nop4");
			}
			if (Parameters.Arguments == null || (!Parameters.Arguments.CaseInsensitiveContains("-submit") && !Parameters.Arguments.CaseInsensitiveContains("-nosubmit")))
			{
				if(GlobalCommandLine.Submit.IsSet)
				{
					CommandLine.Append("-submit ");
				}
				if(GlobalCommandLine.NoSubmit.IsSet)
				{
					CommandLine.Append("-nosubmit ");
				}
			}
			CommandLine.Append(Parameters.Name);
			if (!String.IsNullOrEmpty(Parameters.Arguments))
			{
				CommandLine.AppendFormat(" {0}", Parameters.Arguments);
			}
			if (TelemetryFile != null)
			{
				CommandLine.AppendFormat(" -Telemetry={0}", CommandUtils.MakePathSafeToUseWithCommandLine(TelemetryFile.FullName));
			}
			CommandUtils.RunUAT(CommandUtils.CmdEnv, CommandLine.ToString(), Identifier: Parameters.Name);

			// Merge in any new telemetry data that was produced
			if (Parameters.MergeTelemetryWithPrefix != null)
			{
				TelemetryData NewTelemetry;
				if (TelemetryData.TryRead(TelemetryFile, out NewTelemetry))
				{
					CommandUtils.Telemetry.Merge(Parameters.MergeTelemetryWithPrefix, NewTelemetry);
				}
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
			yield break;
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
