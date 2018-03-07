// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace AutomationTool
{
	/// <summary>
	/// Specifies validation that should be performed on a task parameter.
	/// </summary>
	public enum TaskParameterValidationType
	{
		/// <summary>
		/// Allow any valid values for the field type.
		/// </summary>
		Default,

		/// <summary>
		/// A standard name; alphanumeric characters, plus underscore and space. Spaces at the start or end, or more than one in a row are prohibited.
		/// </summary>
		Name,

		/// <summary>
		/// A list of names separated by semicolons
		/// </summary>
		NameList,

		/// <summary>
		/// A tag name (a regular name with '#' prefix)
		/// </summary>
		Tag,

		/// <summary>
		/// A list of tag names separated by semicolons
		/// </summary>
		TagList,

		/// <summary>
		/// A standard node/aggregate/agent name or tag name
		/// </summary>
		Target,

		/// <summary>
		/// A list of standard name or tag names separated by semicolons
		/// </summary>
		TargetList,

		/// <summary>
		/// A file specification, which may contain tags and wildcards.
		/// </summary>
		FileSpec,

		/// <summary>
		/// A single file name
		/// </summary>
		FileName,

		/// <summary>
		/// A single directory name
		/// </summary>
		DirectoryName,
	}

	/// <summary>
	/// Attribute to mark parameters to a task, which should be read as XML attributes from the script file.
	/// </summary>
	[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
	public class TaskParameterAttribute : Attribute
	{
		/// <summary>
		/// Whether the parameter can be omitted
		/// </summary>
		public bool Optional
		{
			get;
			set;
		}

		/// <summary>
		/// Sets additional restrictions on how this field is validated in the schema. Default is to allow any valid field type.
		/// </summary>
		public TaskParameterValidationType ValidationType
		{
			get;
			set;
		}
	}

	/// <summary>
	/// Attribute used to associate an XML element name with a parameter block that can be used to construct tasks
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public class TaskElementAttribute : Attribute
	{
		/// <summary>
		/// Name of the XML element that can be used to denote this class
		/// </summary>
		public string Name;

		/// <summary>
		/// Type to be constructed from the deserialized element
		/// </summary>
		public Type ParametersType;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InName">Name of the XML element used to denote this object</param>
		/// <param name="InParametersType">Type to be constructed from this object</param>
		public TaskElementAttribute(string InName, Type InParametersType)
		{
			Name = InName;
			ParametersType = InParametersType;
		}
	}

	/// <summary>
	/// Proxy to handle executing multiple tasks simultaneously (such as compile tasks). If a task supports simultaneous execution, it can return a separate
	/// executor an executor instance from GetExecutor() callback. If not, it must implement Execute().
	/// </summary>
	public interface ITaskExecutor
	{
		/// <summary>
		/// Adds another task to this executor
		/// </summary>
		/// <param name="Task">Task to add</param>
		/// <returns>True if the task could be added, false otherwise</returns>
		bool Add(CustomTask Task);

		/// <summary>
		/// Execute all the tasks added to this executor.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		/// <returns>Whether the task succeeded or not. Exiting with an exception will be caught and treated as a failure.</returns>
		void Execute(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet);
	}

	/// <summary>
	/// Base class for all custom build tasks
	/// </summary>
	public abstract class CustomTask
	{
		/// <summary>
		/// Line number in a source file that this task was declared. Optional; used for log messages.
		/// </summary>
		public Tuple<FileReference, int> SourceLocation
		{
			get;
			set;
		}

		/// <summary>
		/// Execute this node.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		/// <returns>Whether the task succeeded or not. Exiting with an exception will be caught and treated as a failure.</returns>
		public abstract void Execute(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet);

		/// <summary>
		/// Creates a proxy to execute this node.
		/// </summary>
		/// <returns>New proxy instance if one is available to execute this task, otherwise null.</returns>
		public virtual ITaskExecutor GetExecutor()
		{
			return null;
		}

		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public abstract void Write(XmlWriter Writer);

		/// <summary>
		/// Writes this task to an XML writer, using the given parameters object.
		/// </summary>
		/// <param name="Writer">Writer for the XML schema</param>
		/// <param name="Parameters">Parameters object that this task is constructed with</param>
		protected void Write(XmlWriter Writer, object Parameters)
		{
			TaskElementAttribute Element = GetType().GetCustomAttribute<TaskElementAttribute>();
			Writer.WriteStartElement(Element.Name);

			foreach (FieldInfo Field in Parameters.GetType().GetFields())
			{
				if (Field.MemberType == MemberTypes.Field)
				{
					TaskParameterAttribute ParameterAttribute = Field.GetCustomAttribute<TaskParameterAttribute>();
					if (ParameterAttribute != null)
					{
						object Value = Field.GetValue(Parameters);
						if (Value != null && Field.FieldType == typeof(bool) && (bool)Value == false)
						{
							Value = null;
						}
						if (Value != null)
						{
							Writer.WriteAttributeString(Field.Name, Value.ToString());
						}
					}
				}
			}

			Writer.WriteEndElement();
		}

		/// <summary>
		/// Returns a string used for trace messages
		/// </summary>
		public string GetTraceString()
		{
			StringBuilder Builder = new StringBuilder();
			using (XmlWriter Writer = XmlWriter.Create(new StringWriter(Builder), new XmlWriterSettings() { OmitXmlDeclaration = true }))
			{
				Write(Writer);
			}
			return Builder.ToString();
		}

		/// <summary>
		/// Find all the tags which are used as inputs to this task
		/// </summary>
		/// <returns>The tag names which are read by this task</returns>
		public abstract IEnumerable<string> FindConsumedTagNames();

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public abstract IEnumerable<string> FindProducedTagNames();

		/// <summary>
		/// Adds tag names from a filespec
		/// </summary>
		/// <param name="Filespec">A filespec, as can be passed to ResolveFilespec</param>
		/// <returns>Tag names from this filespec</returns>
		protected IEnumerable<string> FindTagNamesFromFilespec(string Filespec)
		{
			if(!String.IsNullOrEmpty(Filespec))
			{
				foreach(string Pattern in SplitDelimitedList(Filespec))
				{
					if(Pattern.StartsWith("#"))
					{
						yield return Pattern;
					}
				}
			}
		}

		/// <summary>
		/// Enumerates tag names from a list
		/// </summary>
		/// <param name="TagList">List of tags separated by semicolons</param>
		/// <returns>Tag names from this filespec</returns>
		protected IEnumerable<string> FindTagNamesFromList(string TagList)
		{
			if(!String.IsNullOrEmpty(TagList))
			{
				foreach(string TagName in SplitDelimitedList(TagList))
				{
					yield return TagName;
				}
			}
		}

		/// <summary>
		/// Resolves a single name to a file reference, resolving relative paths to the root of the current path.
		/// </summary>
		/// <param name="Name">Name of the file</param>
		/// <returns>Fully qualified file reference</returns>
		public static FileReference ResolveFile(string Name)
		{
			if(Path.IsPathRooted(Name))
			{
				return new FileReference(Name);
			}
			else
			{
				return new FileReference(Path.Combine(CommandUtils.CmdEnv.LocalRoot, Name));
			}
		}

		/// <summary>
		/// Resolves a directory reference from the given string. Assumes the root directory is the root of the current branch.
		/// </summary>
		/// <param name="Name">Name of the directory. May be null or empty.</param>
		/// <returns>The resolved directory</returns>
		public static DirectoryReference ResolveDirectory(string Name)
		{
			if(String.IsNullOrEmpty(Name))
			{
				return CommandUtils.RootDirectory;
			}
			else if(Path.IsPathRooted(Name))
			{
				return new DirectoryReference(Name);
			}
			else
			{
				return DirectoryReference.Combine(CommandUtils.RootDirectory, Name);
			}
		}

		/// <summary>
		/// Finds or adds a set containing files with the given tag
		/// </summary>
		/// <param name="TagNameToFileSet">Map of tag names to the set of files they contain</param>
		/// <param name="TagName">The tag name to return a set for. A leading '#' character is required.</param>
		/// <returns>Set of files</returns>
		public static HashSet<FileReference> FindOrAddTagSet(Dictionary<string, HashSet<FileReference>> TagNameToFileSet, string TagName)
		{
			// Make sure the tag name contains a single leading hash
			if (TagName.LastIndexOf('#') != 0)
			{
				throw new AutomationException("Tag name '{0}' is not valid - should contain a single leading '#' character", TagName);
			}

			// Any spaces should be later than the second char - most likely to be a typo if directly after the # character
			if (TagName.IndexOf(' ') == 1)
			{
				throw new AutomationException("Tag name '{0}' is not valid - spaces should only be used to separate words", TagName);
			}

			// Find the files which match this tag
			HashSet<FileReference> Files;
			if(!TagNameToFileSet.TryGetValue(TagName, out Files))
			{
				Files = new HashSet<FileReference>();
				TagNameToFileSet.Add(TagName, Files);
			}

			// If we got a null reference, it's because the tag is not listed as an input for this node (see RunGraph.BuildSingleNode). Fill it in, but only with an error.
			if(Files == null)
			{
				CommandUtils.LogError("Attempt to reference tag '{0}', which is not listed as a dependency of this node.", TagName);
				Files = new HashSet<FileReference>();
				TagNameToFileSet.Add(TagName, Files);
			}
			return Files;
		}

		/// <summary>
		/// Resolve a list of files, tag names or file specifications separated by semicolons. Supported entries may be:
		///   a) The name of a tag set (eg. #CompiledBinaries)
		///   b) Relative or absolute filenames
		///   c) A simple file pattern (eg. Foo/*.cpp)
		///   d) A full directory wildcard (eg. Engine/...)
		/// Note that wildcards may only match the last fragment in a pattern, so matches like "/*/Foo.txt" and "/.../Bar.txt" are illegal.
		/// </summary>
		/// <param name="DefaultDirectory">The default directory to resolve relative paths to</param>
		/// <param name="DelimitedPatterns">List of files, tag names, or file specifications to include separated by semicolons.</param>
		/// <param name="TagNameToFileSet">Mapping of tag name to fileset, as passed to the Execute() method</param>
		/// <returns>Set of matching files.</returns>
		public static HashSet<FileReference> ResolveFilespec(DirectoryReference DefaultDirectory, string DelimitedPatterns, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			List<string> ExcludePatterns = new List<string>();
			return ResolveFilespecWithExcludePatterns(DefaultDirectory, DelimitedPatterns, ExcludePatterns, TagNameToFileSet);
		}

		/// <summary>
		/// Resolve a list of files, tag names or file specifications separated by semicolons as above, but preserves any directory references for further processing.
		/// </summary>
		/// <param name="DefaultDirectory">The default directory to resolve relative paths to</param>
		/// <param name="DelimitedPatterns">List of files, tag names, or file specifications to include separated by semicolons.</param>
		/// <param name="ExcludePatterns">Set of patterns to apply to directory searches. This can greatly speed up enumeration by earlying out of recursive directory searches if large directories are excluded (eg. .../Intermediate/...).</param>
		/// <param name="TagNameToFileSet">Mapping of tag name to fileset, as passed to the Execute() method</param>
		/// <returns>Set of matching files.</returns>
		public static HashSet<FileReference> ResolveFilespecWithExcludePatterns(DirectoryReference DefaultDirectory, string DelimitedPatterns, List<string> ExcludePatterns, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Split the argument into a list of patterns
			List<string> Patterns = SplitDelimitedList(DelimitedPatterns);
			return ResolveFilespecWithExcludePatterns(DefaultDirectory, Patterns, ExcludePatterns, TagNameToFileSet);
		}

		/// <summary>
		/// Resolve a list of files, tag names or file specifications as above, but preserves any directory references for further processing.
		/// </summary>
		/// <param name="DefaultDirectory">The default directory to resolve relative paths to</param>
		/// <param name="FilePatterns">List of files, tag names, or file specifications to include separated by semicolons.</param>
		/// <param name="ExcludePatterns">Set of patterns to apply to directory searches. This can greatly speed up enumeration by earlying out of recursive directory searches if large directories are excluded (eg. .../Intermediate/...).</param>
		/// <param name="TagNameToFileSet">Mapping of tag name to fileset, as passed to the Execute() method</param>
		/// <returns>Set of matching files.</returns>
		public static HashSet<FileReference> ResolveFilespecWithExcludePatterns(DirectoryReference DefaultDirectory, List<string> FilePatterns, List<string> ExcludePatterns, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Parse each of the patterns, and add the results into the given sets
			HashSet<FileReference> Files = new HashSet<FileReference>();
			foreach(string Pattern in FilePatterns)
			{
				// Check if it's a tag name
				if(Pattern.StartsWith("#"))
				{
					Files.UnionWith(FindOrAddTagSet(TagNameToFileSet, Pattern));
					continue;
				}

				// If it doesn't contain any wildcards, just add the pattern directly
				int WildcardIdx = FileFilter.FindWildcardIndex(Pattern);
				if(WildcardIdx == -1)
				{
					Files.Add(FileReference.Combine(DefaultDirectory, Pattern));
					continue;
				}

				// Find the base directory for the search. We construct this in a very deliberate way including the directory separator itself, so matches
				// against the OS root directory will resolve correctly both on Mac (where / is the filesystem root) and Windows (where / refers to the current drive).
				int LastDirectoryIdx = Pattern.LastIndexOfAny(new char[]{ Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar }, WildcardIdx);
				DirectoryReference BaseDir = DirectoryReference.Combine(DefaultDirectory, Pattern.Substring(0, LastDirectoryIdx + 1));

				// Construct the absolute include pattern to match against, re-inserting the resolved base directory to construct a canonical path.
				string IncludePattern = BaseDir.FullName.TrimEnd(new char[]{ Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar }) + "/" + Pattern.Substring(LastDirectoryIdx + 1);

				// Construct a filter and apply it to the directory
				if(DirectoryReference.Exists(BaseDir))
				{
					FileFilter Filter = new FileFilter();
					Filter.AddRule(IncludePattern, FileFilterType.Include);
					Filter.AddRules(ExcludePatterns, FileFilterType.Exclude);
					Files.UnionWith(Filter.ApplyToDirectory(BaseDir, BaseDir.FullName, true));
				}
			}

			// If we have exclude rules, create and run a filter against all the output files to catch things that weren't added from an include
			if(ExcludePatterns.Count > 0)
			{
				FileFilter Filter = new FileFilter(FileFilterType.Include);
				Filter.AddRules(ExcludePatterns, FileFilterType.Exclude);
				Files.RemoveWhere(x => !Filter.Matches(x.FullName));
			}
			return Files;
		}

		/// <summary>
		/// Splits a string separated by semicolons into a list, removing empty entries
		/// </summary>
		/// <param name="Text">The input string</param>
		/// <returns>Array of the parsed items</returns>
		public static List<string> SplitDelimitedList(string Text)
		{
			return Text.Split(';').Select(x => x.Trim()).Where(x => x.Length > 0).ToList();
		}
	}
}
