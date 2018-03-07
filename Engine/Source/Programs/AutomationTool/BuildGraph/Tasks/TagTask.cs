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
	/// Parameters for the Tag task.
	/// </summary>
	public class TagTaskParameters
	{
		/// <summary>
		/// Set the base directory to resolve relative paths and patterns against. If set, any absolute patterns (eg. /Engine/Build/...) are taken to be relative to this path. If not, they are taken to be truly absolute.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.DirectoryName)]
		public string BaseDir;

		/// <summary>
		/// Set of files to work from, including wildcards and tag names, separated by semicolons. Resolved relative to BaseDir if set, otherwise to the branch root directory.
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files;

		/// <summary>
		/// Patterns to filter the list of files by, including tag names or wildcards. May include patterns that apply to the base directory if set. Defaults to all files if not specified.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.FileSpec)]
		public string Filter;

		/// <summary>
		/// Set of patterns to exclude from the matched list. May include tag names of patterns that apply to the base directory.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.FileSpec)]
		public string Except;

		/// <summary>
		/// Name of the tag to apply
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.TagList)]
		public string With;
	}

	/// <summary>
	/// Applies a tag to a given set of files. The list of files is found by enumerating the tags and file specifications given by the 'Files' 
	/// parameter. From this list, any files not matched by the 'Filter' parameter are removed, followed by any files matched by the 'Except' parameter.
	/// </summary>
	[TaskElement("Tag", typeof(TagTaskParameters))]
	class TagTask : CustomTask
	{
		/// <summary>
		/// Parameters to this task
		/// </summary>
		TagTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters to select which files to match</param>
		public TagTask(TagTaskParameters InParameters)
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
			// Get the base directory
			DirectoryReference BaseDir = ResolveDirectory(Parameters.BaseDir);

			// Parse all the exclude rules
			List<string> ExcludeRules = ParseRules(BaseDir, Parameters.Except ?? "", TagNameToFileSet);

			// Resolve the input list
			HashSet<FileReference> Files = ResolveFilespecWithExcludePatterns(BaseDir, Parameters.Files, ExcludeRules, TagNameToFileSet);

			// Limit to matches against the 'Filter' parameter, if set
			if(Parameters.Filter != null)
			{
				FileFilter Filter = new FileFilter();
				Filter.AddRules(ParseRules(BaseDir, Parameters.Filter, TagNameToFileSet));
				Files.RemoveWhere(x => !Filter.Matches(x.FullName));
			}

			// Apply the tag to all the matching files
			foreach(string TagName in FindTagNamesFromList(Parameters.With))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(Files);
			}
		}

		/// <summary>
		/// Add rules matching a given set of patterns to a file filter. Patterns are added as absolute paths from the root.
		/// </summary>
		/// <param name="BaseDir">The base directory for relative paths.</param>
		/// <param name="DelimitedPatterns">List of patterns to add, separated by semicolons.</param>
		/// <param name="TagNameToFileSet">Mapping of tag name to a set of files.</param>
		/// <returns>List of rules, suitable for adding to a FileFilter object</returns>
		List<string> ParseRules(DirectoryReference BaseDir, string DelimitedPatterns, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Split up the list of patterns
			List<string> Patterns = SplitDelimitedList(DelimitedPatterns);

			// Parse them into a list of rules
			List<string> Rules = new List<string>();
			foreach(string Pattern in Patterns)
			{
				if(Pattern.StartsWith("#"))
				{
					// Add the files in a specific set to the filter
					HashSet<FileReference> Files = FindOrAddTagSet(TagNameToFileSet, Pattern);
					foreach(FileReference File in Files)
					{
						Rules.Add(File.FullName);
					}
				}
				else
				{
					// Parse a wildcard filter
					if(Pattern.StartsWith("..."))
					{
						Rules.Add(Pattern);
					}
					else if(!Pattern.Contains("/"))
					{
						Rules.Add(".../" + Pattern);
					}
					else if(!Pattern.StartsWith("/"))
					{
						Rules.Add(BaseDir.FullName + "/" + Pattern);
					}
					else
					{
						Rules.Add(BaseDir.FullName + Pattern);
					}
				}
			}
			return Rules;
		}

		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are read by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			foreach(string TagName in FindTagNamesFromFilespec(Parameters.Files))
			{
				yield return TagName;
			}

			if(!String.IsNullOrEmpty(Parameters.Filter))
			{
				foreach(string TagName in FindTagNamesFromFilespec(Parameters.Filter))
				{
					yield return TagName;
				}
			}

			if(!String.IsNullOrEmpty(Parameters.Except))
			{
				foreach(string TagName in FindTagNamesFromFilespec(Parameters.Except))
				{
					yield return TagName;
				}
			}
		}

		/// <summary>
		/// Find all the referenced tags from tasks in this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			return FindTagNamesFromList(Parameters.With);
		}
	}
}
