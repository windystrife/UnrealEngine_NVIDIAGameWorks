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
	/// Parameters for a task that compiles a C# project
	/// </summary>
	public class CsCompileTaskParameters
	{
		/// <summary>
		/// The C# project file to be compile. More than one project file can be specified by separating with semicolons.
		/// </summary>
		[TaskParameter]
		public string Project;

		/// <summary>
		/// The configuration to compile
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Configuration;

		/// <summary>
		/// The platform to compile
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Platform;

		/// <summary>
		/// Additional options to pass to the compiler
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;

		/// <summary>
		/// Only enumerate build products; do not actually compile the projects.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool EnumerateOnly;

		/// <summary>
		/// Tag to be applied to build products of this task
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;

		/// <summary>
		/// Tag to be applied to any non-private references the projects have
		/// (i.e. those that are external and not copied into the output dir)
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string TagReferences;
	}

	/// <summary>
	/// Compiles C# project files, and their dependencies.
	/// </summary>
	[TaskElement("CsCompile", typeof(CsCompileTaskParameters))]
	public class CsCompileTask : CustomTask
	{
		/// <summary>
		/// Parameters for the task
		/// </summary>
		CsCompileTaskParameters Parameters;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public CsCompileTask(CsCompileTaskParameters InParameters)
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
			// Get the project file
			HashSet<FileReference> ProjectFiles = ResolveFilespec(CommandUtils.RootDirectory, Parameters.Project, TagNameToFileSet);
			foreach(FileReference ProjectFile in ProjectFiles)
			{
				if(!FileReference.Exists(ProjectFile))
				{
					throw new AutomationException("Couldn't find project file '{0}'", ProjectFile.FullName);
				}
				if(!ProjectFile.HasExtension(".csproj"))
				{
					throw new AutomationException("File '{0}' is not a C# project", ProjectFile.FullName);
				}
			}

			// Get the default properties
			Dictionary<string, string> Properties = new Dictionary<string,string>(StringComparer.InvariantCultureIgnoreCase);
			if(!String.IsNullOrEmpty(Parameters.Platform))
			{
				Properties["Platform"] = Parameters.Platform;
			}
			if(!String.IsNullOrEmpty(Parameters.Configuration))
			{
				Properties["Configuration"] = Parameters.Configuration;
			}
	
			// Build the arguments and run the build
			if(!Parameters.EnumerateOnly)
			{
				List<string> Arguments = new List<string>();
				foreach(KeyValuePair<string, string> PropertyPair in Properties)
				{
					Arguments.Add(String.Format("/property:{0}={1}", CommandUtils.MakePathSafeToUseWithCommandLine(PropertyPair.Key), CommandUtils.MakePathSafeToUseWithCommandLine(PropertyPair.Value)));
				}
				if(!String.IsNullOrEmpty(Parameters.Arguments))
				{
					Arguments.Add(Parameters.Arguments);
				}
				Arguments.Add("/verbosity:minimal");
				Arguments.Add("/nologo");
				foreach(FileReference ProjectFile in ProjectFiles)
				{
					CommandUtils.MsBuild(CommandUtils.CmdEnv, ProjectFile.FullName, String.Join(" ", Arguments), null);
				}
			}

			// Try to figure out the output files
			HashSet<FileReference> ProjectBuildProducts;
			HashSet<FileReference> ProjectReferences;
			FindBuildProducts(ProjectFiles, Properties, out ProjectBuildProducts, out ProjectReferences);

			// Apply the optional tag to the produced archive
			foreach(string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(ProjectBuildProducts);
			}

			if (!String.IsNullOrEmpty(Parameters.TagReferences))
			{
				// Apply the optional tag to any references
				foreach (string TagName in FindTagNamesFromList(Parameters.TagReferences))
				{
					FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(ProjectReferences);
				}
			}

			// Merge them into the standard set of build products
			BuildProducts.UnionWith(ProjectBuildProducts);
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
			return FindTagNamesFromFilespec(Parameters.Project);
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			foreach (string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				yield return TagName;
			}

			foreach (string TagName in FindTagNamesFromList(Parameters.TagReferences))
			{
				yield return TagName;
			}
		}

		/// <summary>
		/// Find all the build products created by compiling the given project file
		/// </summary>
		/// <param name="ProjectFiles">Initial project file to read. All referenced projects will also be read.</param>
		/// <param name="InitialProperties">Mapping of property name to value</param>
		/// <param name="OutBuildProducts">Receives a set of build products on success</param>
		/// <param name="OutReferences">Receives a set of non-private references on success</param>
		/// <returns>True if the build products were found, false otherwise.</returns>
		static void FindBuildProducts(HashSet<FileReference> ProjectFiles, Dictionary<string, string> InitialProperties, out HashSet<FileReference> OutBuildProducts, out HashSet<FileReference> OutReferences)
		{
			// Read all the project information into a dictionary
			Dictionary<FileReference, CsProjectInfo> FileToProjectInfo = new Dictionary<FileReference,CsProjectInfo>();
			foreach(FileReference ProjectFile in ProjectFiles)
			{
				ReadProjectsRecursively(ProjectFile, InitialProperties, FileToProjectInfo);
			}

			// Find all the build products and references
			HashSet<FileReference> BuildProducts = new HashSet<FileReference>();
			HashSet<FileReference> References = new HashSet<FileReference>();
			foreach(KeyValuePair<FileReference, CsProjectInfo> Pair in FileToProjectInfo)
			{
				CsProjectInfo ProjectInfo = Pair.Value;

				// Add the standard build products
				DirectoryReference OutputDir = ProjectInfo.GetOutputDir(Pair.Key.Directory);
				ProjectInfo.AddBuildProducts(OutputDir, BuildProducts);

				// Add the referenced assemblies
				foreach(KeyValuePair<FileReference, bool> Reference in ProjectInfo.References)
				{
					FileReference OtherAssembly = Reference.Key;
					if (Reference.Value)
					{
						// Add reference from the output dir
						FileReference OutputFile = FileReference.Combine(OutputDir, OtherAssembly.GetFileName());
						BuildProducts.Add(OutputFile);

						FileReference OutputSymbolFile = OutputFile.ChangeExtension(".pdb");
						if(FileReference.Exists(OutputSymbolFile))
						{
							BuildProducts.Add(OutputSymbolFile);
						}
					}
					else
					{
						// Add reference directly
						References.Add(OtherAssembly);
						FileReference SymbolFile = OtherAssembly.ChangeExtension(".pdb");
						if(FileReference.Exists(SymbolFile))
						{
							References.Add(SymbolFile);
						}
					}
				}

				// Add build products from all the referenced projects. MSBuild only copy the directly referenced build products, not recursive references or other assemblies.
				foreach(CsProjectInfo OtherProjectInfo in ProjectInfo.ProjectReferences.Where(x => x.Value).Select(x => FileToProjectInfo[x.Key]))
				{
					OtherProjectInfo.AddBuildProducts(OutputDir, BuildProducts);
				}
			}

			// Update the output set
			OutBuildProducts = BuildProducts;
			OutReferences = References;
		}

		/// <summary>
		/// Read a project file, plus all the project files it references.
		/// </summary>
		/// <param name="File">Project file to read</param>
		/// <param name="InitialProperties">Mapping of property name to value for the initial project</param>
		/// <param name="FileToProjectInfo"></param>
		/// <returns>True if the projects were read correctly, false (and prints an error to the log) if not</returns>
		static void ReadProjectsRecursively(FileReference File, Dictionary<string, string> InitialProperties, Dictionary<FileReference, CsProjectInfo> FileToProjectInfo)
		{
			// Early out if we've already read this project
			if (!FileToProjectInfo.ContainsKey(File))
			{
				// Try to read this project
				CsProjectInfo ProjectInfo;
				if (!CsProjectInfo.TryRead(File, InitialProperties, out ProjectInfo))
				{
					throw new AutomationException("Couldn't read project '{0}'", File.FullName);
				}

				// Add it to the project lookup, and try to read all the projects it references
				FileToProjectInfo.Add(File, ProjectInfo);
				foreach(FileReference ProjectReference in ProjectInfo.ProjectReferences.Keys)
				{
					ReadProjectsRecursively(ProjectReference, InitialProperties, FileToProjectInfo);
				}
			}
		}
	}	
}
