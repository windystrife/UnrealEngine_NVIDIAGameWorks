// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;
using AutomationTool;
using System.Xml;
using Tools.DotNETCommon;

namespace AutomationTool
{
	/// <summary>
	/// Parameters for a compile task
	/// </summary>
	public class CompileTaskParameters
	{
		/// <summary>
		/// The target to compile
		/// </summary>
		[TaskParameter]
		public string Target;

		/// <summary>
		/// The configuration to compile
		/// </summary>
		[TaskParameter]
		public UnrealTargetConfiguration Configuration;

		/// <summary>
		/// The platform to compile for
		/// </summary>
		[TaskParameter]
		public UnrealTargetPlatform Platform;

		/// <summary>
		/// Additional arguments for UnrealBuildTool
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;

		/// <summary>
		/// Whether to allow using XGE for compilation
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool AllowXGE = true;

		/// <summary>
		/// Whether to allow using the parallel executor for this compile
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool AllowParallelExecutor = true;

		/// <summary>
		/// Tag to be applied to build products of this task
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;
	}

	/// <summary>
	/// Executor for compile tasks, which can compile multiple tasks simultaneously
	/// </summary>
	public class CompileTaskExecutor : ITaskExecutor
	{
		/// <summary>
		/// List of targets to compile. As well as the target specifically added for this task, additional compile tasks may be merged with it.
		/// </summary>
		List<UE4Build.BuildTarget> Targets = new List<UE4Build.BuildTarget>();

		/// <summary>
		/// Mapping of receipt filename to its corresponding tag name
		/// </summary>
		Dictionary<UE4Build.BuildTarget, string> TargetToTagName = new Dictionary<UE4Build.BuildTarget,string>();

		/// <summary>
		/// Whether to allow using XGE for this job
		/// </summary>
		bool bAllowXGE = true;

		/// <summary>
		/// Whether to allow using the parallel executor for this job
		/// </summary>
		bool bAllowParallelExecutor = true;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Task">Initial task to execute</param>
		public CompileTaskExecutor(CompileTask Task)
		{
			Add(Task);
		}

		/// <summary>
		/// Adds another task to this executor
		/// </summary>
		/// <param name="Task">Task to add</param>
		/// <returns>True if the task could be added, false otherwise</returns>
		public bool Add(CustomTask Task)
		{
			CompileTask CompileTask = Task as CompileTask;
			if(CompileTask == null)
			{
				return false;
			}

			if(Targets.Count > 0)
			{
				if (bAllowXGE != CompileTask.Parameters.AllowXGE)
				{
					return false;
				}
				if (!bAllowParallelExecutor || !CompileTask.Parameters.AllowParallelExecutor)
				{
					return false;
				}
			}

			CompileTaskParameters Parameters = CompileTask.Parameters;
			bAllowXGE &= Parameters.AllowXGE;
			bAllowParallelExecutor &= Parameters.AllowParallelExecutor;

			UE4Build.BuildTarget Target = new UE4Build.BuildTarget { TargetName = Parameters.Target, Platform = Parameters.Platform, Config = Parameters.Configuration, UBTArgs = "-nobuilduht " + (Parameters.Arguments ?? "") };
			if(!String.IsNullOrEmpty(Parameters.Tag))
			{
				TargetToTagName.Add(Target, Parameters.Tag);
			}
			Targets.Add(Target);

			return true;
		}

		/// <summary>
		/// Execute all the tasks added to this executor.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		/// <returns>Whether the task succeeded or not. Exiting with an exception will be caught and treated as a failure.</returns>
		public void Execute(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Create the agenda
            UE4Build.BuildAgenda Agenda = new UE4Build.BuildAgenda();
			Agenda.Targets.AddRange(Targets);

			// Build everything
			Dictionary<UE4Build.BuildTarget, BuildManifest> TargetToManifest = new Dictionary<UE4Build.BuildTarget,BuildManifest>();
            UE4Build Builder = new UE4Build(Job.OwnerCommand);

			bool bCanUseParallelExecutor = (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64 && bAllowParallelExecutor);	// parallel executor is only available on Windows as of 2016-09-22
			Builder.Build(Agenda, InDeleteBuildProducts: null, InUpdateVersionFiles: false, InForceNoXGE: !bAllowXGE, InUseParallelExecutor: bCanUseParallelExecutor, InTargetToManifest: TargetToManifest);

			UE4Build.CheckBuildProducts(Builder.BuildProductFiles);

			// Tag all the outputs
			foreach(KeyValuePair<UE4Build.BuildTarget, string> TargetTagName in TargetToTagName)
			{
				BuildManifest Manifest;
				if(!TargetToManifest.TryGetValue(TargetTagName.Key, out Manifest))
				{
					throw new AutomationException("Missing manifest for target {0} {1} {2}", TargetTagName.Key.TargetName, TargetTagName.Key.Platform, TargetTagName.Key.Config);
				}

				foreach(string TagName in CustomTask.SplitDelimitedList(TargetTagName.Value))
				{
					HashSet<FileReference> FileSet = CustomTask.FindOrAddTagSet(TagNameToFileSet, TagName);
					FileSet.UnionWith(Manifest.BuildProducts.Select(x => new FileReference(x)));
					FileSet.UnionWith(Manifest.LibraryBuildProducts.Select(x => new FileReference(x)));
				}
			}

			// Add everything to the list of build products
			BuildProducts.UnionWith(Builder.BuildProductFiles.Select(x => new FileReference(x)));
			BuildProducts.UnionWith(Builder.LibraryBuildProductFiles.Select(x => new FileReference(x)));
		}
	}

	/// <summary>
	/// Compiles a target with UnrealBuildTool.
	/// </summary>
	[TaskElement("Compile", typeof(CompileTaskParameters))]
	public class CompileTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		public CompileTaskParameters Parameters;

		/// <summary>
		/// Construct a compile task
		/// </summary>
		/// <param name="Parameters">Parameters for this task</param>
		public CompileTask(CompileTaskParameters Parameters)
		{
			this.Parameters = Parameters;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		public override void Execute(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			GetExecutor().Execute(Job, BuildProducts, TagNameToFileSet);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		public override ITaskExecutor GetExecutor()
		{
			return new CompileTaskExecutor(this);
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
