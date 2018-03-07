using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// A special Makefile that UBT is able to create in "-gather" mode, then load in "-assemble" mode to accelerate iterative compiling and linking
	/// </summary>
	[Serializable]
	class UBTMakefile : ISerializable
	{
		public const int CurrentVersion = 5;

		public int Version;

		/// <summary>
		/// Every action in the action graph
		/// </summary>
		public List<Action> AllActions;

		/// <summary>
		/// List of the actions that need to be run in order to build the targets' final output items
		/// </summary>
		public Action[] PrerequisiteActions;

		/// <summary>
		/// Environment variables that we'll need in order to invoke the platform's compiler and linker
		/// </summary>
		// @todo ubtmake: Really we want to allow a different set of environment variables for every Action.  This would allow for targets on multiple platforms to be built in a single assembling phase.  We'd only have unique variables for each platform that has actions, so we'd want to make sure we only store the minimum set.
		public readonly List<Tuple<string, string>> EnvironmentVariables = new List<Tuple<string, string>>();

		/// <summary>
		/// Maps each target to a list of UObject module info structures
		/// </summary>
		public Dictionary<string, List<UHTModuleInfo>> TargetNameToUObjectModules;

		/// <summary>
		/// List of targets being built
		/// </summary>
		public List<UEBuildTarget> Targets;

		/// <summary>
		/// Whether adaptive unity build is enabled for any of these targets
		/// </summary>
		public bool bUseAdaptiveUnityBuild;

		/// <summary>
		/// Current working set of source files, for when bUseAdaptiveUnityBuild is enabled
		/// </summary>
		public HashSet<FileItem> SourceFileWorkingSet = new HashSet<FileItem>();

		/// <summary>
		/// Set of source files which are included in unity files, but which should invalidate the makefile if modified (for when bUseAdaptiveUnityBuild is enabled)
		/// </summary>
		public HashSet<FileItem> CandidateSourceFilesForWorkingSet = new HashSet<FileItem>();

		public UBTMakefile()
		{
			Version = CurrentVersion;
		}

		public UBTMakefile(SerializationInfo Info, StreamingContext Context)
		{
			Version = Info.GetInt32("ve");
			if (Version != CurrentVersion)
			{
				throw new Exception(string.Format("Makefile version does not match - found {0}, expected: {1}", Version, CurrentVersion));
			}

			AllActions = (List<Action>)Info.GetValue("ac", typeof(List<Action>));
			PrerequisiteActions = (Action[])Info.GetValue("pa", typeof(Action[]));
			EnvironmentVariables = ((string[])Info.GetValue("e1", typeof(string[]))).Zip((string[])Info.GetValue("e2", typeof(string[])), (i1, i2) => new Tuple<string, string>(i1, i2)).ToList();
			TargetNameToUObjectModules = (Dictionary<string, List<UHTModuleInfo>>)Info.GetValue("nu", typeof(Dictionary<string, List<UHTModuleInfo>>));
			Targets = (List<UEBuildTarget>)Info.GetValue("ta", typeof(List<UEBuildTarget>));
			bUseAdaptiveUnityBuild = Info.GetBoolean("ua");
			SourceFileWorkingSet = (HashSet<FileItem>)Info.GetValue("ws", typeof(HashSet<FileItem>));
			CandidateSourceFilesForWorkingSet = (HashSet<FileItem>)Info.GetValue("wc", typeof(HashSet<FileItem>));
		}

		public void GetObjectData(SerializationInfo Info, StreamingContext Context)
		{
			Info.AddValue("ve", Version);
			Info.AddValue("ac", AllActions);
			Info.AddValue("pa", PrerequisiteActions);
			Info.AddValue("e1", EnvironmentVariables.Select(x => x.Item1).ToArray());
			Info.AddValue("e2", EnvironmentVariables.Select(x => x.Item2).ToArray());
			Info.AddValue("nu", TargetNameToUObjectModules);
			Info.AddValue("ta", Targets);
			Info.AddValue("ua", bUseAdaptiveUnityBuild);
			Info.AddValue("ws", SourceFileWorkingSet);
			Info.AddValue("wc", CandidateSourceFilesForWorkingSet);
		}


		/// <returns> True if this makefile's contents look valid.  Called after loading the file to make sure it is legit.</returns>
		public bool IsValidMakefile()
		{
			return
				AllActions != null && AllActions.Count > 0 &&
				PrerequisiteActions != null && PrerequisiteActions.Length > 0 &&
				EnvironmentVariables != null &&
				TargetNameToUObjectModules != null && TargetNameToUObjectModules.Count > 0 &&
				Targets != null && Targets.Count > 0 &&
				SourceFileWorkingSet != null &&
				CandidateSourceFilesForWorkingSet != null;
		}
	}
}
