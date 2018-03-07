// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Xml;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace BuildGraph.Tasks
{
	/// <summary>
	/// Parameters for a copy task
	/// </summary>
	public class RenameTaskParameters
	{
		/// <summary>
		/// The file or files to rename
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files;

		/// <summary>
		/// The current file name, or pattern to match (eg. *.txt). Should not include any path separators.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.DirectoryName)]
		public string From;

		/// <summary>
		/// The new name for the file(s). Should not include any path separators.
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.DirectoryName)]
		public string To;

		/// <summary>
		/// Tag to be applied to the renamed files
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;
	}

	/// <summary>
	/// Renames a file, or group of files.
	/// </summary>
	[TaskElement("Rename", typeof(RenameTaskParameters))]
	public class RenameTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		RenameTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public RenameTask(RenameTaskParameters InParameters)
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
			// Get the pattern to match against. If it's a simple pattern (eg. *.cpp, Engine/Build/...), automatically infer the source wildcard
			string FromPattern = Parameters.From;
			if (FromPattern == null)
			{
				List<string> Patterns = SplitDelimitedList(Parameters.Files);
				if (Patterns.Count != 1 || Patterns[0].StartsWith("#"))
				{
					throw new AutomationException("Missing 'From' attribute specifying pattern to match source files against");
				}

				FromPattern = Patterns[0];

				int SlashIdx = FromPattern.LastIndexOfAny(new char[] { Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar });
				if (SlashIdx != -1)
				{
					FromPattern = FromPattern.Substring(SlashIdx + 1);
				}
				if (FromPattern.StartsWith("..."))
				{
					FromPattern = "*" + FromPattern.Substring(3);
				}
			}

			// Convert the source pattern into a regex
			string EscapedFromPattern = "^" + Regex.Escape(FromPattern) + "$";
			EscapedFromPattern = EscapedFromPattern.Replace("\\*", "(.*)");
			EscapedFromPattern = EscapedFromPattern.Replace("\\?", "(.)");
			Regex FromRegex = new Regex(EscapedFromPattern, RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);

			// Split the output pattern into fragments that we can insert captures between
			string[] FromFragments = FromPattern.Split('*', '?');
			string[] ToFragments = Parameters.To.Split('*', '?');
			if(FromFragments.Length < ToFragments.Length)
			{
				throw new AutomationException("Too few capture groups in source pattern '{0}' to rename to '{1}'", FromPattern, Parameters.To);
			}

			// Find the input files
			HashSet<FileReference> InputFiles = ResolveFilespec(CommandUtils.RootDirectory, Parameters.Files, TagNameToFileSet);

			// Find all the corresponding output files
			Dictionary<FileReference, FileReference> RenameFiles = new Dictionary<FileReference, FileReference>();
			foreach (FileReference InputFile in InputFiles)
			{
				Match Match = FromRegex.Match(InputFile.GetFileName());
				if (Match.Success)
				{
					StringBuilder OutputName = new StringBuilder(ToFragments[0]);
					for (int Idx = 1; Idx < ToFragments.Length; Idx++)
					{
						OutputName.Append(Match.Groups[Idx].Value);
						OutputName.Append(ToFragments[Idx]);
					}
					RenameFiles[InputFile] = FileReference.Combine(InputFile.Directory, OutputName.ToString());
				}
			}

			// Print out everything we're going to do
			foreach(KeyValuePair<FileReference, FileReference> Pair in RenameFiles)
			{
				CommandUtils.RenameFile(Pair.Key.FullName, Pair.Value.FullName, true);
			}

			// Add the build product
			BuildProducts.UnionWith(RenameFiles.Values);

			// Apply the optional output tag to them
			foreach(string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(RenameFiles.Values);
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
			return FindTagNamesFromList(Parameters.Tag);
		}
	}
}
