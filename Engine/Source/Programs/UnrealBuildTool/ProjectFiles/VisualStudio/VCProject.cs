// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Xml;
using System.Xml.XPath;
using System.Xml.Linq;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.Security;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	abstract class MSBuildProjectFile : ProjectFile
	{
		/// The project file version string
		static public readonly string VCProjectFileVersionString = "10.0.30319.1";

		/// The build configuration name to use for stub project configurations.  These are projects whose purpose
		/// is to make it easier for developers to find source files and to provide IntelliSense data for the module
		/// to Visual Studio
		static public readonly string StubProjectConfigurationName = "BuiltWithUnrealBuildTool";

		/// The name of the Visual C++ platform to use for stub project configurations
		/// NOTE: We always use Win32 for the stub project's platform, since that is guaranteed to be supported by Visual Studio
		static public readonly string StubProjectPlatformName = "Win32";

		/// override project configuration name for platforms visual studio doesn't natively support
		public string ProjectConfigurationNameOverride = "";

		/// override project platform for platforms visual studio doesn't natively support
		public string ProjectPlatformNameOverride = "";

		/// <summary>
		/// The Guid representing the project type e.g. C# or C++
		/// </summary>
		public virtual string ProjectTypeGUID
		{
			get { throw new BuildException("Unrecognized type of project file for Visual Studio solution"); }
		}

		/// <summary>
		/// Constructs a new project file object
		/// </summary>
		/// <param name="InitFilePath">The path to the project file on disk</param>
		public MSBuildProjectFile(FileReference InitFilePath)
			: base(InitFilePath)
		{
			// Each project gets its own GUID.  This is stored in the project file and referenced in the solution file.

			// First, check to see if we have an existing file on disk.  If we do, then we'll try to preserve the
			// GUID by loading it from the existing file.
			if (FileReference.Exists(ProjectFilePath))
			{
				try
				{
					LoadGUIDFromExistingProject();
				}
				catch (Exception)
				{
					// Failed to find GUID, so just create a new one
					ProjectGUID = Guid.NewGuid();
				}
			}

			if (ProjectGUID == Guid.Empty)
			{
				// Generate a brand new GUID
				ProjectGUID = Guid.NewGuid();
			}
		}


		/// <summary>
		/// Attempts to load the project's GUID from an existing project file on disk
		/// </summary>
		public override void LoadGUIDFromExistingProject()
		{
			// Only load GUIDs if we're in project generation mode.  Regular builds don't need GUIDs for anything.
			if (ProjectFileGenerator.bGenerateProjectFiles)
			{
				XmlDocument Doc = new XmlDocument();
				Doc.Load(ProjectFilePath.FullName);

				// @todo projectfiles: Ideally we could do a better job about preserving GUIDs when only minor changes are made
				// to the project (such as adding a single new file.) It would make diffing changes much easier!

				// @todo projectfiles: Can we "seed" a GUID based off the project path and generate consistent GUIDs each time?

				XmlNodeList Elements = Doc.GetElementsByTagName("ProjectGuid");
				foreach (XmlElement Element in Elements)
				{
					ProjectGUID = Guid.ParseExact(Element.InnerText.Trim("{}".ToCharArray()), "D");
				}
			}
		}


		/// <summary>
		/// Given a target platform and configuration, generates a platform and configuration name string to use in Visual Studio projects.
		/// Unlike with solution configurations, Visual Studio project configurations only support certain types of platforms, so we'll
		/// generate a configuration name that has the platform "built in", and use a default platform type
		/// </summary>
		/// <param name="Platform">Actual platform</param>
		/// <param name="Configuration">Actual configuration</param>
		/// <param name="TargetConfigurationName">The configuration name from the target rules, or null if we don't have one</param>
		/// <param name="ProjectPlatformName">Name of platform string to use for Visual Studio project</param>
		/// <param name="ProjectConfigurationName">Name of configuration string to use for Visual Studio project</param>
		public abstract void MakeProjectPlatformAndConfigurationNames(UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string TargetConfigurationName, out string ProjectPlatformName, out string ProjectConfigurationName);

		static UnrealTargetConfiguration[] GetSupportedConfigurations(TargetRules Rules)
		{
			// Check if the rules object implements the legacy GetSupportedPlatforms() function. If it does, we'll call it for backwards compatibility.
			if (Rules.GetType().GetMethod("GetSupportedConfigurations").DeclaringType != typeof(TargetRules))
			{
				List<UnrealTargetConfiguration> ConfigurationList = new List<UnrealTargetConfiguration>();
#pragma warning disable 0612
				if (Rules.GetSupportedConfigurations(ref ConfigurationList, true))
				{
					return ConfigurationList.Distinct().ToArray();
				}
#pragma warning restore 0612
			}

			// Otherwise take the SupportedConfigurationsAttribute from the first type in the inheritance chain that supports it
			for (Type CurrentType = Rules.GetType(); CurrentType != null; CurrentType = CurrentType.BaseType)
			{
				object[] Attributes = Rules.GetType().GetCustomAttributes(typeof(SupportedConfigurationsAttribute), false);
				if (Attributes.Length > 0)
				{
					return Attributes.OfType<SupportedConfigurationsAttribute>().SelectMany(x => x.Configurations).Distinct().ToArray();
				}
			}

			// Otherwise, get the default for the target type
			if (Rules.Type == TargetType.Program)
			{
				return new[] { UnrealTargetConfiguration.Debug, UnrealTargetConfiguration.Development };
			}
			else if(Rules.Type == TargetType.Editor)
			{
				return new[] { UnrealTargetConfiguration.Debug, UnrealTargetConfiguration.DebugGame, UnrealTargetConfiguration.Development };
			}
			else
			{
				return ((UnrealTargetConfiguration[])Enum.GetValues(typeof(UnrealTargetConfiguration))).Where(x => x != UnrealTargetConfiguration.Unknown).ToArray();
			}
		}

		/// <summary>
		/// Checks to see if the specified solution platform and configuration is able to map to this project
		/// </summary>
		/// <param name="ProjectTarget">The target that we're checking for a valid platform/config combination</param>
		/// <param name="Platform">Platform</param>
		/// <param name="Configuration">Configuration</param>
		/// <returns>True if this is a valid combination for this project, otherwise false</returns>
		public static bool IsValidProjectPlatformAndConfiguration(ProjectTarget ProjectTarget, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration)
		{
			if (!ProjectFileGenerator.bIncludeTestAndShippingConfigs)
			{
				if(Configuration == UnrealTargetConfiguration.Test || Configuration == UnrealTargetConfiguration.Shipping)
				{
					return false;
				}
			}

			UEPlatformProjectGenerator PlatformProjectGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(Platform, true);
			if (PlatformProjectGenerator == null)
			{
				return false;
			}

			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform, true);
			if (BuildPlatform == null)
			{
				return false;
			}

			if (BuildPlatform.HasRequiredSDKsInstalled() != SDKStatus.Valid)
			{
				return false;
			}


			List<UnrealTargetConfiguration> SupportedConfigurations = new List<UnrealTargetConfiguration>();
			List<UnrealTargetPlatform> SupportedPlatforms = new List<UnrealTargetPlatform>();
			if (!ProjectFileGenerator.bCreateDummyConfigsForUnsupportedPlatforms)
			{
				if (ProjectTarget.TargetRules != null)
				{
					SupportedPlatforms.AddRange(ProjectTarget.SupportedPlatforms);
				}
			}
			else
			{
				SupportedPlatforms.AddRange(Utils.GetPlatformsInClass(UnrealPlatformClass.All));
			}

			if (ProjectTarget.TargetRules != null)
			{
				SupportedConfigurations.AddRange(GetSupportedConfigurations(ProjectTarget.TargetRules));
			}

			// Add all of the extra platforms/configurations for this target
			{
				foreach (UnrealTargetPlatform ExtraPlatform in ProjectTarget.ExtraSupportedPlatforms)
				{
					if (!SupportedPlatforms.Contains(ExtraPlatform))
					{
						SupportedPlatforms.Add(ExtraPlatform);
					}
				}
				foreach (UnrealTargetConfiguration ExtraConfig in ProjectTarget.ExtraSupportedConfigurations)
				{
					if (!SupportedConfigurations.Contains(ExtraConfig))
					{
						SupportedConfigurations.Add(ExtraConfig);
					}
				}
			}

			// Only build for supported platforms
			if (SupportedPlatforms.Contains(Platform) == false)
			{
				return false;
			}

			// Only build for supported configurations
			if (SupportedConfigurations.Contains(Configuration) == false)
			{
				return false;
			}

			return true;
		}

		/// <summary>
		/// Escapes characters in a filename so they can be stored in an XML attribute
		/// </summary>
		/// <param name="FileName">The filename to escape</param>
		/// <returns>The escaped filename</returns>
		public static string EscapeFileName(string FileName)
		{
			return SecurityElement.Escape(FileName);
		}

		/// <summary>
		/// GUID for this Visual C++ project file
		/// </summary>
		public Guid ProjectGUID
		{
			get;
			protected set;
		}
	}

	class VCProjectFile : MSBuildProjectFile
	{
		FileReference OnlyGameProject;
		VCProjectFileFormat ProjectFileFormat;
		bool bUseFastPDB;
		bool bUsePerFileIntellisense;
		string BuildToolOverride;

		// This is the GUID that Visual Studio uses to identify a C++ project file in the solution
		public override string ProjectTypeGUID
		{
			get { return "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"; }
		}

		/// <summary>
		/// Constructs a new project file object
		/// </summary>
		/// <param name="InFilePath">The path to the project file on disk</param>
		/// <param name="InOnlyGameProject"></param>
		/// <param name="InProjectFileFormat">Visual C++ project file version</param>
		/// <param name="bUseFastPDB">If true, adds the -FastPDB argument to build command lines</param>
		/// <param name="bUsePerFileIntellisense">If true, generates per-file intellisense data</param>
		/// <param name="BuildToolOverride">Optional arguments to pass to UBT when building</param>
		public VCProjectFile(FileReference InFilePath, FileReference InOnlyGameProject, VCProjectFileFormat InProjectFileFormat, bool bUseFastPDB, bool bUsePerFileIntellisense, string BuildToolOverride)
			: base(InFilePath)
		{
			OnlyGameProject = InOnlyGameProject;
			ProjectFileFormat = InProjectFileFormat;
			this.bUseFastPDB = bUseFastPDB;
			this.bUsePerFileIntellisense = bUsePerFileIntellisense;
			this.BuildToolOverride = BuildToolOverride;
		}

		/// <summary>
		/// Given a target platform and configuration, generates a platform and configuration name string to use in Visual Studio projects.
		/// Unlike with solution configurations, Visual Studio project configurations only support certain types of platforms, so we'll
		/// generate a configuration name that has the platform "built in", and use a default platform type
		/// </summary>
		/// <param name="Platform">Actual platform</param>
		/// <param name="Configuration">Actual configuration</param>
		/// <param name="TargetConfigurationName">The configuration name from the target rules, or null if we don't have one</param>
		/// <param name="ProjectPlatformName">Name of platform string to use for Visual Studio project</param>
		/// <param name="ProjectConfigurationName">Name of configuration string to use for Visual Studio project</param>
		public override void MakeProjectPlatformAndConfigurationNames(UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string TargetConfigurationName, out string ProjectPlatformName, out string ProjectConfigurationName)
		{
			UEPlatformProjectGenerator PlatformProjectGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(Platform, bInAllowFailure: true);

			// Check to see if this platform is supported directly by Visual Studio projects.
			bool HasActualVSPlatform = (PlatformProjectGenerator != null) ? PlatformProjectGenerator.HasVisualStudioSupport(Platform, Configuration, ProjectFileFormat) : false;

			if (HasActualVSPlatform)
			{
				// Great!  Visual Studio supports this platform natively, so we don't need to make up
				// a fake project configuration name.

				// Allow the platform to specify the name used in VisualStudio.
				// Note that the actual name of the platform on the Visual Studio side may be different than what
				// UnrealBuildTool calls it (e.g. "Win64" -> "x64".) GetVisualStudioPlatformName() will figure this out.
				ProjectConfigurationName = Configuration.ToString();
				ProjectPlatformName = PlatformProjectGenerator.GetVisualStudioPlatformName(Platform, Configuration);
			}
			else
			{
				// Visual Studio doesn't natively support this platform, so we fake it by mapping it to
				// a project configuration that has the platform name in that configuration as a suffix,
				// and then using "Win32" as the actual VS platform name
				ProjectConfigurationName = ProjectConfigurationNameOverride == "" ? Platform.ToString() + "_" + Configuration.ToString() : ProjectConfigurationNameOverride;
				ProjectPlatformName = ProjectPlatformNameOverride == "" ? VCProjectFileGenerator.DefaultPlatformName : ProjectPlatformNameOverride;
			}

			if (!String.IsNullOrEmpty(TargetConfigurationName))
			{
				ProjectConfigurationName += "_" + TargetConfigurationName;
			}
		}

		class ProjectConfigAndTargetCombination
		{
			public UnrealTargetPlatform Platform;
			public UnrealTargetConfiguration Configuration;
			public string ProjectPlatformName;
			public string ProjectConfigurationName;
			public ProjectTarget ProjectTarget;

			public ProjectConfigAndTargetCombination(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, string InProjectPlatformName, string InProjectConfigurationName, ProjectTarget InProjectTarget)
			{
				Platform = InPlatform;
				Configuration = InConfiguration;
				ProjectPlatformName = InProjectPlatformName;
				ProjectConfigurationName = InProjectConfigurationName;
				ProjectTarget = InProjectTarget;
			}

			public string ProjectConfigurationAndPlatformName
			{
				get { return (ProjectPlatformName == null) ? null : (ProjectConfigurationName + "|" + ProjectPlatformName); }
			}

			public override string ToString()
			{
				return String.Format("{0} {1} {2}", ProjectTarget, Platform, Configuration);
			}
		}

		WindowsCompiler GetCompilerForIntellisense()
		{
			switch(ProjectFileFormat)
			{
				case VCProjectFileFormat.VisualStudio2017:
					return WindowsCompiler.VisualStudio2017;
				default:
					return WindowsCompiler.VisualStudio2015;
			}
		}

		List<ProjectConfigAndTargetCombination> ProjectConfigAndTargetCombinations = new List<ProjectConfigAndTargetCombination>();

		private void BuildProjectConfigAndTargetCombinations(List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations)
		{
			//no need to do this more than once
			if(ProjectConfigAndTargetCombinations.Count > 0)
			{
				return;
			}

			// Build up a list of platforms and configurations this project will support.  In this list, Unknown simply
			// means that we should use the default "stub" project platform and configuration name.

			// If this is a "stub" project, then only add a single configuration to the project
			if (IsStubProject)
			{
				ProjectConfigAndTargetCombination StubCombination = new ProjectConfigAndTargetCombination(UnrealTargetPlatform.Unknown, UnrealTargetConfiguration.Unknown, StubProjectPlatformName, StubProjectConfigurationName, null);
				ProjectConfigAndTargetCombinations.Add(StubCombination);
			}
			else
			{
				// Figure out all the desired configurations
				foreach (UnrealTargetConfiguration Configuration in InConfigurations)
				{
					//@todo.Rocket: Put this in a commonly accessible place?
					if (UnrealBuildTool.IsValidConfiguration(Configuration) == false)
					{
						continue;
					}
					foreach (UnrealTargetPlatform Platform in InPlatforms)
					{
						if (UnrealBuildTool.IsValidPlatform(Platform) == false)
						{
							continue;
						}
						UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform, true);
						if ((BuildPlatform != null) && (BuildPlatform.HasRequiredSDKsInstalled() == SDKStatus.Valid))
						{
							// Now go through all of the target types for this project
							if (ProjectTargets.Count == 0)
							{
								throw new BuildException("Expecting at least one ProjectTarget to be associated with project '{0}' in the TargetProjects list ", ProjectFilePath);
							}

							foreach (ProjectTarget ProjectTarget in ProjectTargets)
							{
								if (IsValidProjectPlatformAndConfiguration(ProjectTarget, Platform, Configuration))
								{
									string ProjectPlatformName, ProjectConfigurationName;
									MakeProjectPlatformAndConfigurationNames(Platform, Configuration, ProjectTarget.TargetRules.Type.ToString(), out ProjectPlatformName, out ProjectConfigurationName);

									ProjectConfigAndTargetCombination Combination = new ProjectConfigAndTargetCombination(Platform, Configuration, ProjectPlatformName, ProjectConfigurationName, ProjectTarget);
									ProjectConfigAndTargetCombinations.Add(Combination);
								}
							}
						}
					}
				}
			}
		}


		/// <summary>
		/// If found writes a debug project file to disk
		/// </summary>
		/// <returns>True on success</returns>
		public override List<Tuple<ProjectFile, string>> WriteDebugProjectFiles(List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations)
		{
			string ProjectName = ProjectFilePath.GetFileNameWithoutExtension();

			List<UnrealTargetPlatform> ProjectPlatforms = new List<UnrealTargetPlatform>();
			List<Tuple<ProjectFile, string>> ProjectFiles = new List<Tuple<ProjectFile, string>>();

			BuildProjectConfigAndTargetCombinations(InPlatforms, InConfigurations);


			foreach (ProjectConfigAndTargetCombination Combination in ProjectConfigAndTargetCombinations)
			{
				if (!ProjectPlatforms.Contains(Combination.Platform))
				{
					ProjectPlatforms.Add(Combination.Platform);
				}
			}

			//write out any additional project files
			if (!IsStubProject && ProjectTargets.Any(x => x.TargetRules != null && x.TargetRules.Type != TargetType.Program))
			{
				foreach (UnrealTargetPlatform Platform in ProjectPlatforms)
				{
					UEPlatformProjectGenerator ProjGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(Platform, true);
					if (ProjGenerator != null)
					{
						//write out additional prop file
						ProjGenerator.WriteAdditionalPropFile();

						//write out additional project user files
						ProjGenerator.WriteAdditionalProjUserFile(this);

						//write out additional project files
						Tuple<ProjectFile, string> DebugProjectInfo = ProjGenerator.WriteAdditionalProjFile(this);
						if(DebugProjectInfo != null)
						{
							ProjectFiles.Add(DebugProjectInfo);
						}
					}
				}
			}

			return ProjectFiles;
		}

		/// Implements Project interface
		public override bool WriteProjectFile(List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations)
		{
			string ProjectName = ProjectFilePath.GetFileNameWithoutExtension();

			bool bSuccess = true;

			// Build up the new include search path string
			StringBuilder VCIncludeSearchPaths = new StringBuilder();
			{
				foreach (string CurPath in IntelliSenseIncludeSearchPaths)
				{
					VCIncludeSearchPaths.Append(CurPath + ";");
				}
				foreach (string CurPath in IntelliSenseSystemIncludeSearchPaths)
				{
					VCIncludeSearchPaths.Append(CurPath + ";");
				}
				if (InPlatforms.Contains(UnrealTargetPlatform.Win64))
				{
					VCIncludeSearchPaths.Append(VCToolChain.GetVCIncludePaths(CppPlatform.Win64, GetCompilerForIntellisense()) + ";");
				}
				else if (InPlatforms.Contains(UnrealTargetPlatform.Win32))
				{
					VCIncludeSearchPaths.Append(VCToolChain.GetVCIncludePaths(CppPlatform.Win32, GetCompilerForIntellisense()) + ";");
				}
			}

			StringBuilder VCPreprocessorDefinitions = new StringBuilder();
			foreach (string CurDef in IntelliSensePreprocessorDefinitions)
			{
				if (VCPreprocessorDefinitions.Length > 0)
				{
					VCPreprocessorDefinitions.Append(';');
				}
				VCPreprocessorDefinitions.Append(CurDef);
			}

			// Setup VC project file content
			StringBuilder VCProjectFileContent = new StringBuilder();
			StringBuilder VCFiltersFileContent = new StringBuilder();
			StringBuilder VCUserFileContent = new StringBuilder();

			// Visual Studio doesn't require a *.vcxproj.filters file to even exist alongside the project unless
			// it actually has something of substance in it.  We'll avoid saving it out unless we need to.
			bool FiltersFileIsNeeded = false;

			// Project file header
			VCProjectFileContent.Append(
				"<?xml version=\"1.0\" encoding=\"utf-8\"?>" + ProjectFileGenerator.NewLine +
				ProjectFileGenerator.NewLine +
				"<Project DefaultTargets=\"Build\" ToolsVersion=\"" + VCProjectFileGenerator.GetProjectFileToolVersionString(ProjectFileFormat) + "\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">" + ProjectFileGenerator.NewLine);

			bool bGenerateUserFileContent = UEPlatformProjectGenerator.PlatformRequiresVSUserFileGeneration(InPlatforms, InConfigurations);
			if (bGenerateUserFileContent)
			{
				VCUserFileContent.Append(
					"<?xml version=\"1.0\" encoding=\"utf-8\"?>" + ProjectFileGenerator.NewLine +
					ProjectFileGenerator.NewLine +
					"<Project ToolsVersion=\"" + VCProjectFileGenerator.GetProjectFileToolVersionString(ProjectFileFormat) + "\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">" + ProjectFileGenerator.NewLine
					);
			}

			BuildProjectConfigAndTargetCombinations(InPlatforms, InConfigurations);

			VCProjectFileContent.Append(
				"	<ItemGroup Label=\"ProjectConfigurations\">" + ProjectFileGenerator.NewLine);

			// Make a list of the platforms and configs as project-format names
			List<UnrealTargetPlatform> ProjectPlatforms = new List<UnrealTargetPlatform>();
			List<Tuple<string, UnrealTargetPlatform>> ProjectPlatformNameAndPlatforms = new List<Tuple<string, UnrealTargetPlatform>>();	// ProjectPlatformName, Platform
			List<Tuple<string, UnrealTargetConfiguration>> ProjectConfigurationNameAndConfigurations = new List<Tuple<string, UnrealTargetConfiguration>>();	// ProjectConfigurationName, Configuration
			foreach (ProjectConfigAndTargetCombination Combination in ProjectConfigAndTargetCombinations)
			{
				if (!ProjectPlatforms.Contains(Combination.Platform))
				{
					ProjectPlatforms.Add(Combination.Platform);
				}
				if (!ProjectPlatformNameAndPlatforms.Any(ProjectPlatformNameAndPlatformTuple => ProjectPlatformNameAndPlatformTuple.Item1 == Combination.ProjectPlatformName))
				{
					ProjectPlatformNameAndPlatforms.Add(Tuple.Create(Combination.ProjectPlatformName, Combination.Platform));
				}
				if (!ProjectConfigurationNameAndConfigurations.Any(ProjectConfigurationNameAndConfigurationTuple => ProjectConfigurationNameAndConfigurationTuple.Item1 == Combination.ProjectConfigurationName))
				{
					ProjectConfigurationNameAndConfigurations.Add(Tuple.Create(Combination.ProjectConfigurationName, Combination.Configuration));
				}
			}

			// Output ALL the project's config-platform permutations (project files MUST do this)
			foreach (Tuple<string, UnrealTargetConfiguration> ConfigurationTuple in ProjectConfigurationNameAndConfigurations)
			{
				string ProjectConfigurationName = ConfigurationTuple.Item1;
				foreach (Tuple<string, UnrealTargetPlatform> PlatformTuple in ProjectPlatformNameAndPlatforms)
				{
					string ProjectPlatformName = PlatformTuple.Item1;
					VCProjectFileContent.Append(
							"		<ProjectConfiguration Include=\"" + ProjectConfigurationName + "|" + ProjectPlatformName + "\">" + ProjectFileGenerator.NewLine +
							"			<Configuration>" + ProjectConfigurationName + "</Configuration>" + ProjectFileGenerator.NewLine +
							"			<Platform>" + ProjectPlatformName + "</Platform>" + ProjectFileGenerator.NewLine +
							"		</ProjectConfiguration>" + ProjectFileGenerator.NewLine);
				}
			}

			VCProjectFileContent.Append(
				"	</ItemGroup>" + ProjectFileGenerator.NewLine);

			VCFiltersFileContent.Append(
				"<?xml version=\"1.0\" encoding=\"utf-8\"?>" + ProjectFileGenerator.NewLine +
				ProjectFileGenerator.NewLine +
				"<Project ToolsVersion=\"" + VCProjectFileGenerator.GetProjectFileToolVersionString(ProjectFileFormat) + "\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">" + ProjectFileGenerator.NewLine);

			// Platform specific PropertyGroups, etc.
			StringBuilder AdditionalPropertyGroups = new StringBuilder();
			if (!IsStubProject)
			{
				foreach (UnrealTargetPlatform Platform in ProjectPlatforms)
				{
					UEPlatformProjectGenerator ProjGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(Platform, true);
					if (ProjGenerator != null && ProjGenerator.HasVisualStudioSupport(Platform, UnrealTargetConfiguration.Development, ProjectFileFormat))
					{
						AdditionalPropertyGroups.Append(ProjGenerator.GetAdditionalVisualStudioPropertyGroups(Platform, ProjectFileFormat));
					}
				}

				VCProjectFileContent.Append(AdditionalPropertyGroups);
			}

			// Project globals (project GUID, project type, SCC bindings, etc)
			{
				VCProjectFileContent.Append(
					"	<PropertyGroup Label=\"Globals\">" + ProjectFileGenerator.NewLine +
					"		<ProjectGuid>" + ProjectGUID.ToString("B").ToUpperInvariant() + "</ProjectGuid>" + ProjectFileGenerator.NewLine +
					"		<Keyword>MakeFileProj</Keyword>" + ProjectFileGenerator.NewLine +
					"		<RootNamespace>" + ProjectName + "</RootNamespace>" + ProjectFileGenerator.NewLine +
					"		<PlatformToolset>" + VCProjectFileGenerator.GetProjectFilePlatformToolsetVersionString(ProjectFileFormat) + "</PlatformToolset>" + ProjectFileGenerator.NewLine +
					"		<MinimumVisualStudioVersion>" + VCProjectFileGenerator.GetProjectFileToolVersionString(ProjectFileFormat) + "</MinimumVisualStudioVersion>" + ProjectFileGenerator.NewLine +
					"		<TargetRuntime>Native</TargetRuntime>" + ProjectFileGenerator.NewLine +
					"	</PropertyGroup>" + ProjectFileGenerator.NewLine);
			}

			// look for additional import lines for all platforms for non stub projects
			if (!IsStubProject)
			{
				foreach (UnrealTargetPlatform Platform in ProjectPlatforms)
				{
					UEPlatformProjectGenerator ProjGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(Platform, true);
					if (ProjGenerator != null && ProjGenerator.HasVisualStudioSupport(Platform, UnrealTargetConfiguration.Development, ProjectFileFormat))
					{
						VCProjectFileContent.Append(ProjGenerator.GetVisualStudioGlobalProperties(Platform));
					}
				}
			}

			// Write each project configuration PreDefaultProps section
			foreach (Tuple<string, UnrealTargetConfiguration> ConfigurationTuple in ProjectConfigurationNameAndConfigurations)
			{
				string ProjectConfigurationName = ConfigurationTuple.Item1;
				UnrealTargetConfiguration TargetConfiguration = ConfigurationTuple.Item2;
				foreach (Tuple<string, UnrealTargetPlatform> PlatformTuple in ProjectPlatformNameAndPlatforms)
				{
					string ProjectPlatformName = PlatformTuple.Item1;
					UnrealTargetPlatform TargetPlatform = PlatformTuple.Item2;
					WritePreDefaultPropsConfiguration(TargetPlatform, TargetConfiguration, ProjectPlatformName, ProjectConfigurationName, VCProjectFileContent);
				}
			}

			VCProjectFileContent.Append(
				"	<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />" + ProjectFileGenerator.NewLine);

			// Write each project configuration PreDefaultProps section
			foreach (Tuple<string, UnrealTargetConfiguration> ConfigurationTuple in ProjectConfigurationNameAndConfigurations)
			{
				string ProjectConfigurationName = ConfigurationTuple.Item1;
				UnrealTargetConfiguration TargetConfiguration = ConfigurationTuple.Item2;
				foreach (Tuple<string, UnrealTargetPlatform> PlatformTuple in ProjectPlatformNameAndPlatforms)
				{
					string ProjectPlatformName = PlatformTuple.Item1;
					UnrealTargetPlatform TargetPlatform = PlatformTuple.Item2;
					WritePostDefaultPropsConfiguration(TargetPlatform, TargetConfiguration, ProjectPlatformName, ProjectConfigurationName, VCProjectFileContent);
				}
			}

			VCProjectFileContent.Append(
				"	<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />" + ProjectFileGenerator.NewLine +
				"	<ImportGroup Label=\"ExtensionSettings\" />" + ProjectFileGenerator.NewLine +
				"	<PropertyGroup Label=\"UserMacros\" />" + ProjectFileGenerator.NewLine
				);

			// Write each project configuration
			foreach (ProjectConfigAndTargetCombination Combination in ProjectConfigAndTargetCombinations)
			{
				WriteConfiguration(ProjectName, Combination, VCProjectFileContent, bGenerateUserFileContent ? VCUserFileContent : null);
			}

			// Source folders and files
			{
				List<AliasedFile> LocalAliasedFiles = new List<AliasedFile>(AliasedFiles);

				foreach (SourceFile CurFile in SourceFiles)
				{
					// We want all source file and directory paths in the project files to be relative to the project file's
					// location on the disk.  Convert the path to be relative to the project file directory
					string ProjectRelativeSourceFile = CurFile.Reference.MakeRelativeTo(ProjectFilePath.Directory);

					// By default, files will appear relative to the project file in the solution.  This is kind of the normal Visual
					// Studio way to do things, but because our generated project files are emitted to intermediate folders, if we always
					// did this it would yield really ugly paths int he solution explorer
					string FilterRelativeSourceDirectory;
					if (CurFile.BaseFolder == null)
					{
						FilterRelativeSourceDirectory = ProjectRelativeSourceFile;
					}
					else
					{
						FilterRelativeSourceDirectory = CurFile.Reference.MakeRelativeTo(CurFile.BaseFolder);
					}

					// Manually remove the filename for the filter. We run through this code path a lot, so just do it manually.
					int LastSeparatorIdx = FilterRelativeSourceDirectory.LastIndexOf(Path.DirectorySeparatorChar);
					if (LastSeparatorIdx == -1)
					{
						FilterRelativeSourceDirectory = "";
					}
					else
					{
						FilterRelativeSourceDirectory = FilterRelativeSourceDirectory.Substring(0, LastSeparatorIdx);
					}

					LocalAliasedFiles.Add(new AliasedFile(ProjectRelativeSourceFile, FilterRelativeSourceDirectory));
				}

				VCFiltersFileContent.Append(
					"	<ItemGroup>" + ProjectFileGenerator.NewLine);

				VCProjectFileContent.Append(
					"	<ItemGroup>" + ProjectFileGenerator.NewLine);

				// Add all file directories to the filters file as solution filters
				HashSet<string> FilterDirectories = new HashSet<string>();
				UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(BuildHostPlatform.Current.Platform);
				bool bWritePerFilePCHInfo = false;
				if (bUsePerFileIntellisense && ProjectFileFormat >= VCProjectFileFormat.VisualStudio2015)
				{
					string UpdateRegistryLoc = string.Format(@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\DevDiv\vs\Servicing\{0}\devenv", VCProjectFileGenerator.GetProjectFileToolVersionString(ProjectFileFormat));
					object Result = Microsoft.Win32.Registry.GetValue(UpdateRegistryLoc, "UpdateVersion", null);
					if (Result != null)
					{
						int UpdateVersion = 0;
						if (Int32.TryParse(Result.ToString().Split('.').Last(), out UpdateVersion) && UpdateVersion >= 25420)
						{
							bWritePerFilePCHInfo = true;
						}
					}
				}

				foreach (AliasedFile AliasedFile in LocalAliasedFiles)
				{
					// No need to add the root directory relative to the project (it would just be an empty string!)
					if (!String.IsNullOrWhiteSpace(AliasedFile.ProjectPath))
					{
						FiltersFileIsNeeded = EnsureFilterPathExists(AliasedFile.ProjectPath, VCFiltersFileContent, FilterDirectories);
					}

					string VCFileType = GetVCFileType(AliasedFile.FileSystemPath);
					string PCHFileName = null;

					if (bWritePerFilePCHInfo && VCFileType == "ClCompile")
					{
						FileReference TruePath = FileReference.Combine(ProjectFilePath.Directory, AliasedFile.FileSystemPath);
						FileItem SourceFile = FileItem.GetItemByFileReference(TruePath);
						List <DependencyInclude> DirectlyIncludedFilenames = CPPHeaders.GetUncachedDirectIncludeDependencies(SourceFile, null);
						if (DirectlyIncludedFilenames.Count > 0)
						{
							PCHFileName = DirectlyIncludedFilenames[0].IncludeName;
						}
					}

					if (!string.IsNullOrEmpty(PCHFileName))
					{
						VCProjectFileContent.Append(
							"		<" + VCFileType + " Include=\"" + EscapeFileName(AliasedFile.FileSystemPath) + "\">" + ProjectFileGenerator.NewLine +
							"			<AdditionalOptions>$(AdditionalOptions) /Yu" + PCHFileName + "</AdditionalOptions>" + ProjectFileGenerator.NewLine +
							"		</" + VCFileType + " >" + ProjectFileGenerator.NewLine);
					}
					else
					{
						VCProjectFileContent.Append(
							"		<" + VCFileType + " Include=\"" + EscapeFileName(AliasedFile.FileSystemPath) + "\" />" + ProjectFileGenerator.NewLine);
					}

					if (!String.IsNullOrWhiteSpace(AliasedFile.ProjectPath))
					{
						VCFiltersFileContent.Append(
							"		<" + VCFileType + " Include=\"" + EscapeFileName(AliasedFile.FileSystemPath) + "\">" + ProjectFileGenerator.NewLine +
							"			<Filter>" + Utils.CleanDirectorySeparators(AliasedFile.ProjectPath) + "</Filter>" + ProjectFileGenerator.NewLine +
							"		</" + VCFileType + " >" + ProjectFileGenerator.NewLine);

						FiltersFileIsNeeded = true;
					}
					else
					{
						// No need to specify the root directory relative to the project (it would just be an empty string!)
						VCFiltersFileContent.Append(
							"		<" + VCFileType + " Include=\"" + EscapeFileName(AliasedFile.FileSystemPath) + "\" />" + ProjectFileGenerator.NewLine);
					}
				}

				VCProjectFileContent.Append(
					"	</ItemGroup>" + ProjectFileGenerator.NewLine);

				VCFiltersFileContent.Append(
					"	</ItemGroup>" + ProjectFileGenerator.NewLine);
			}

			// For Installed engine builds, include engine source in the source search paths if it exists. We never build it locally, so the debugger can't find it.
			if (UnrealBuildTool.IsEngineInstalled() && !IsStubProject)
			{
				VCProjectFileContent.Append("	<PropertyGroup>" + ProjectFileGenerator.NewLine);
				VCProjectFileContent.Append("		<SourcePath>");
				foreach (string DirectoryName in Directory.EnumerateDirectories(UnrealBuildTool.EngineSourceDirectory.FullName, "*", SearchOption.AllDirectories))
				{
					if (Directory.EnumerateFiles(DirectoryName, "*.cpp").Any())
					{
						VCProjectFileContent.Append(DirectoryName);
						VCProjectFileContent.Append(";");
					}
				}
				VCProjectFileContent.Append("</SourcePath>" + ProjectFileGenerator.NewLine);
				VCProjectFileContent.Append("	</PropertyGroup>" + ProjectFileGenerator.NewLine);
			}

			// Write IntelliSense info
			{
				// @todo projectfiles: Currently we are storing defines/include paths for ALL configurations rather than using ConditionString and storing
				//      this data uniquely for each target configuration.  IntelliSense may behave better if we did that, but it will result in a LOT more
				//      data being stored into the project file, and might make the IDE perform worse when switching configurations!
				VCProjectFileContent.Append(
					"	<PropertyGroup>" + ProjectFileGenerator.NewLine +
					"		<NMakePreprocessorDefinitions>$(NMakePreprocessorDefinitions)" + (VCPreprocessorDefinitions.Length > 0 ? (";" + VCPreprocessorDefinitions) : "") + "</NMakePreprocessorDefinitions>" + ProjectFileGenerator.NewLine +
					"		<NMakeIncludeSearchPath>$(NMakeIncludeSearchPath)" + (VCIncludeSearchPaths.Length > 0 ? (";" + VCIncludeSearchPaths) : "") + "</NMakeIncludeSearchPath>" + ProjectFileGenerator.NewLine +
					"		<NMakeForcedIncludes>$(NMakeForcedIncludes)</NMakeForcedIncludes>" + ProjectFileGenerator.NewLine +
					"		<NMakeAssemblySearchPath>$(NMakeAssemblySearchPath)</NMakeAssemblySearchPath>" + ProjectFileGenerator.NewLine +
					"		<NMakeForcedUsingAssemblies>$(NMakeForcedUsingAssemblies)</NMakeForcedUsingAssemblies>" + ProjectFileGenerator.NewLine +
					"	</PropertyGroup>" + ProjectFileGenerator.NewLine);
			}

			// look for additional import lines for all platforms for non stub projects
			StringBuilder AdditionalTargetSettings = new StringBuilder();
			if (!IsStubProject)
			{
				foreach (UnrealTargetPlatform Platform in ProjectPlatforms)
				{
					UEPlatformProjectGenerator ProjGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(Platform, true);
					if (ProjGenerator != null && ProjGenerator.HasVisualStudioSupport(Platform, UnrealTargetConfiguration.Development, ProjectFileFormat))
					{
						AdditionalTargetSettings.Append(ProjGenerator.GetVisualStudioTargetOverrides(Platform, ProjectFileFormat));
					}
				}
			}

			string OutputManifestString = "";
			if (!IsStubProject)
			{
				foreach (UnrealTargetPlatform Platform in ProjectPlatforms)
				{
					UEPlatformProjectGenerator ProjGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(Platform, true);
					if (ProjGenerator != null && ProjGenerator.HasVisualStudioSupport(Platform, UnrealTargetConfiguration.Development, ProjectFileFormat))
					{
						// @todo projectfiles: Serious hacks here because we are trying to emit one-time platform-specific sections that need information
						//    about a target type, but the project file may contain many types of targets!  Some of this logic will need to move into
						//    the per-target configuration writing code.
						TargetType HackTargetType = TargetType.Game;
						FileReference HackTargetFilePath = null;
						foreach (ProjectConfigAndTargetCombination Combination in ProjectConfigAndTargetCombinations)
						{
							if (Combination.Platform == Platform &&
								Combination.ProjectTarget.TargetRules != null &&
								Combination.ProjectTarget.TargetRules.Type == HackTargetType)
							{
								HackTargetFilePath = Combination.ProjectTarget.TargetFilePath;// ProjectConfigAndTargetCombinations[0].ProjectTarget.TargetFilePath;
								break;
							}
						}

						if (HackTargetFilePath != null)
						{
							OutputManifestString += ProjGenerator.GetVisualStudioOutputManifestSection(Platform, HackTargetType, HackTargetFilePath, ProjectFilePath, ProjectFileFormat);
						}
					}
				}
			}

			VCProjectFileContent.Append(
					OutputManifestString +	// output manifest must come before the Cpp.targets file.
					"	<ItemDefinitionGroup>" + ProjectFileGenerator.NewLine +
					"	</ItemDefinitionGroup>" + ProjectFileGenerator.NewLine +
					"	<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />" + ProjectFileGenerator.NewLine +
					AdditionalTargetSettings.ToString() +
					"	<ImportGroup Label=\"ExtensionTargets\">" + ProjectFileGenerator.NewLine +
					"	</ImportGroup>" + ProjectFileGenerator.NewLine +
					"</Project>" + ProjectFileGenerator.NewLine);

			VCFiltersFileContent.Append(
				"</Project>" + ProjectFileGenerator.NewLine);

			if (bGenerateUserFileContent)
			{
				VCUserFileContent.Append(
					"</Project>" + ProjectFileGenerator.NewLine
					);
			}

			// Save the project file
			if (bSuccess)
			{
				bSuccess = ProjectFileGenerator.WriteFileIfChanged(ProjectFilePath.FullName, VCProjectFileContent.ToString());
			}


			// Save the filters file
			if (bSuccess)
			{
				// Create a path to the project file's filters file
				string VCFiltersFilePath = ProjectFilePath.FullName + ".filters";
				if (FiltersFileIsNeeded)
				{
					bSuccess = ProjectFileGenerator.WriteFileIfChanged(VCFiltersFilePath, VCFiltersFileContent.ToString());
				}
				else
				{
					Log.TraceVerbose("Deleting Visual C++ filters file which is no longer needed: " + VCFiltersFilePath);

					// Delete the filters file, if one exists.  We no longer need it
					try
					{
						File.Delete(VCFiltersFilePath);
					}
					catch (Exception)
					{
						Log.TraceInformation("Error deleting filters file (file may not be writable): " + VCFiltersFilePath);
					}
				}
			}

			// Save the user file, if required
			if (VCUserFileContent.Length > 0)
			{
				// Create a path to the project file's user file
				string VCUserFilePath = ProjectFilePath.FullName + ".user";
				// Never overwrite the existing user path as it will cause them to lose their settings
				if (File.Exists(VCUserFilePath) == false)
				{
					bSuccess = ProjectFileGenerator.WriteFileIfChanged(VCUserFilePath, VCUserFileContent.ToString());
				}
			}

			return bSuccess;
		}

		private static bool EnsureFilterPathExists(string FilterRelativeSourceDirectory, StringBuilder VCFiltersFileContent, HashSet<string> FilterDirectories)
		{
			// We only want each directory to appear once in the filters file
			string PathRemaining = Utils.CleanDirectorySeparators(FilterRelativeSourceDirectory);
			bool FiltersFileIsNeeded = false;
			if (!FilterDirectories.Contains(PathRemaining))
			{
				// Make sure all subdirectories leading up to this directory each have their own filter, too!
				List<string> AllDirectoriesInPath = new List<string>();
				string PathSoFar = "";
				for (; ; )
				{
					if (PathRemaining.Length > 0)
					{
						int SlashIndex = PathRemaining.IndexOf(Path.DirectorySeparatorChar);
						string SplitDirectory;
						if (SlashIndex != -1)
						{
							SplitDirectory = PathRemaining.Substring(0, SlashIndex);
							PathRemaining = PathRemaining.Substring(SplitDirectory.Length + 1);
						}
						else
						{
							SplitDirectory = PathRemaining;
							PathRemaining = "";
						}
						if (!String.IsNullOrEmpty(PathSoFar))
						{
							PathSoFar += Path.DirectorySeparatorChar;
						}
						PathSoFar += SplitDirectory;

						AllDirectoriesInPath.Add(PathSoFar);
					}
					else
					{
						break;
					}
				}

				foreach (string LeadingDirectory in AllDirectoriesInPath)
				{
					if (!FilterDirectories.Contains(LeadingDirectory))
					{
						FilterDirectories.Add(LeadingDirectory);

						// Generate a unique GUID for this folder
						// NOTE: When saving generated project files, we ignore differences in GUIDs if every other part of the file
						//       matches identically with the pre-existing file
						string FilterGUID = Guid.NewGuid().ToString("B").ToUpperInvariant();

						VCFiltersFileContent.Append(
							"		<Filter Include=\"" + LeadingDirectory + "\">" + ProjectFileGenerator.NewLine +
							"			<UniqueIdentifier>" + FilterGUID + "</UniqueIdentifier>" + ProjectFileGenerator.NewLine +
							"		</Filter>" + ProjectFileGenerator.NewLine);

						FiltersFileIsNeeded = true;
					}
				}
			}

			return FiltersFileIsNeeded;
		}

		/// <summary>
		/// Returns the VCFileType element name based on the file path.
		/// </summary>
		/// <param name="Path">The path of the file to return type for.</param>
		/// <returns>Name of the element in MSBuild project file for this file.</returns>
		private string GetVCFileType(string Path)
		{
			// What type of file is this?
			if (Path.EndsWith(".h", StringComparison.InvariantCultureIgnoreCase) ||
				Path.EndsWith(".inl", StringComparison.InvariantCultureIgnoreCase))
			{
				return "ClInclude";
			}
			else if (Path.EndsWith(".cpp", StringComparison.InvariantCultureIgnoreCase))
			{
				return "ClCompile";
			}
			else if (Path.EndsWith(".rc", StringComparison.InvariantCultureIgnoreCase))
			{
				return "ResourceCompile";
			}
			else if (Path.EndsWith(".manifest", StringComparison.InvariantCultureIgnoreCase))
			{
				return "Manifest";
			}
			else
			{
				return "None";
			}
		}

		// Anonymous function that writes pre-Default.props configuration data
		private void WritePreDefaultPropsConfiguration(UnrealTargetPlatform TargetPlatform, UnrealTargetConfiguration TargetConfiguration, string ProjectPlatformName, string ProjectConfigurationName, StringBuilder VCProjectFileContent)
		{
			UEPlatformProjectGenerator ProjGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(TargetPlatform, true);
			if (((ProjGenerator == null) && (TargetPlatform != UnrealTargetPlatform.Unknown)))
			{
				return;
			}

			string ProjectConfigurationAndPlatformName = ProjectConfigurationName + "|" + ProjectPlatformName;
			string ConditionString = "Condition=\"'$(Configuration)|$(Platform)'=='" + ProjectConfigurationAndPlatformName + "'\"";

			string PlatformToolsetString = (ProjGenerator != null) ? ProjGenerator.GetVisualStudioPreDefaultString(TargetPlatform, TargetConfiguration) : "";

			if (!String.IsNullOrEmpty(PlatformToolsetString))
			{
				VCProjectFileContent.Append(
					"	<PropertyGroup " + ConditionString + " Label=\"Configuration\">" + ProjectFileGenerator.NewLine +
							PlatformToolsetString +
					"	</PropertyGroup>" + ProjectFileGenerator.NewLine
				);
			}
		}

		// Anonymous function that writes post-Default.props configuration data
		private void WritePostDefaultPropsConfiguration(UnrealTargetPlatform TargetPlatform, UnrealTargetConfiguration TargetConfiguration, string ProjectPlatformName, string ProjectConfigurationName, StringBuilder VCProjectFileContent)
		{
			UEPlatformProjectGenerator ProjGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(TargetPlatform, true);
			if (((ProjGenerator == null) && (TargetPlatform != UnrealTargetPlatform.Unknown)))
			{
				return;
			}

			string ProjectConfigurationAndPlatformName = ProjectConfigurationName + "|" + ProjectPlatformName;
			string ConditionString = "Condition=\"'$(Configuration)|$(Platform)'=='" + ProjectConfigurationAndPlatformName + "'\"";

			string PlatformToolsetString = (ProjGenerator != null) ? ProjGenerator.GetVisualStudioPlatformToolsetString(TargetPlatform, TargetConfiguration, ProjectFileFormat) : "";
			if (String.IsNullOrEmpty(PlatformToolsetString))
			{
				PlatformToolsetString = "		<PlatformToolset>" + VCProjectFileGenerator.GetProjectFilePlatformToolsetVersionString(ProjectFileFormat) + "</PlatformToolset>" + ProjectFileGenerator.NewLine;
			}

			string PlatformConfigurationType = (ProjGenerator == null) ? "Makefile" : ProjGenerator.GetVisualStudioPlatformConfigurationType(TargetPlatform, ProjectFileFormat);
			VCProjectFileContent.Append(
				"	<PropertyGroup " + ConditionString + " Label=\"Configuration\">" + ProjectFileGenerator.NewLine +
				"		<ConfigurationType>" + PlatformConfigurationType + "</ConfigurationType>" + ProjectFileGenerator.NewLine +
						PlatformToolsetString +
				"	</PropertyGroup>" + ProjectFileGenerator.NewLine
				);
		}

		// Anonymous function that writes project configuration data
		private void WriteConfiguration(string ProjectName, ProjectConfigAndTargetCombination Combination, StringBuilder VCProjectFileContent, StringBuilder VCUserFileContent)
		{
			UnrealTargetPlatform Platform = Combination.Platform;
			UnrealTargetConfiguration Configuration = Combination.Configuration;

			UEPlatformProjectGenerator ProjGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(Platform, true);
			if (((ProjGenerator == null) && (Platform != UnrealTargetPlatform.Unknown)))
			{
				return;
			}

			string UProjectPath = "";
			if (IsForeignProject)
			{
				UProjectPath = "\"$(SolutionDir)$(ProjectName).uproject\"";
			}

			string ConditionString = "Condition=\"'$(Configuration)|$(Platform)'=='" + Combination.ProjectConfigurationAndPlatformName + "'\"";

			{
				string ImportGroupProperties = (ProjGenerator != null) ? ProjGenerator.GetVisualStudioImportGroupProperties(Platform) : "";
				VCProjectFileContent.Append(
					"	<ImportGroup " + ConditionString + " Label=\"PropertySheets\">" + ProjectFileGenerator.NewLine +
					"		<Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />" + ProjectFileGenerator.NewLine +
							ImportGroupProperties +
					"	</ImportGroup>" + ProjectFileGenerator.NewLine);

				DirectoryReference ProjectDirectory = ProjectFilePath.Directory;

				if (IsStubProject)
				{
					string ProjectRelativeUnusedDirectory = NormalizeProjectPath(DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", "Unused"));

					VCProjectFileContent.Append(
						"	<PropertyGroup " + ConditionString + ">" + ProjectFileGenerator.NewLine +
						"		<OutDir>" + ProjectRelativeUnusedDirectory + Path.DirectorySeparatorChar + "</OutDir>" + ProjectFileGenerator.NewLine +
						"		<IntDir>" + ProjectRelativeUnusedDirectory + Path.DirectorySeparatorChar + "</IntDir>" + ProjectFileGenerator.NewLine +
						"		<NMakeBuildCommandLine>@rem Nothing to do.</NMakeBuildCommandLine>" + ProjectFileGenerator.NewLine +
						"		<NMakeReBuildCommandLine>@rem Nothing to do.</NMakeReBuildCommandLine>" + ProjectFileGenerator.NewLine +
						"		<NMakeCleanCommandLine>@rem Nothing to do.</NMakeCleanCommandLine>" + ProjectFileGenerator.NewLine +
						"		<NMakeOutput/>" + ProjectFileGenerator.NewLine +
						"	</PropertyGroup>" + ProjectFileGenerator.NewLine);
				}
				else if (UnrealBuildTool.IsEngineInstalled() && Combination.ProjectTarget != null && Combination.ProjectTarget.TargetRules != null && !Combination.ProjectTarget.SupportedPlatforms.Contains(Combination.Platform))
				{
					string ProjectRelativeUnusedDirectory = NormalizeProjectPath(DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", "Unused"));

					VCProjectFileContent.AppendFormat(
						"	<PropertyGroup " + ConditionString + ">" + ProjectFileGenerator.NewLine +
						"		<OutDir>" + ProjectRelativeUnusedDirectory + Path.DirectorySeparatorChar + "</OutDir>" + ProjectFileGenerator.NewLine +
						"		<IntDir>" + ProjectRelativeUnusedDirectory + Path.DirectorySeparatorChar + "</IntDir>" + ProjectFileGenerator.NewLine +
						"		<NMakeBuildCommandLine>@echo {0} is not a supported platform for {1}. Valid platforms are {2}.</NMakeBuildCommandLine>" + ProjectFileGenerator.NewLine +
						"		<NMakeReBuildCommandLine>@echo {0} is not a supported platform for {1}. Valid platforms are {2}.</NMakeReBuildCommandLine>" + ProjectFileGenerator.NewLine +
						"		<NMakeCleanCommandLine>@echo {0} is not a supported platform for {1}. Valid platforms are {2}.</NMakeCleanCommandLine>" + ProjectFileGenerator.NewLine +
						"		<NMakeOutput/>" + ProjectFileGenerator.NewLine +
						"	</PropertyGroup>" + ProjectFileGenerator.NewLine, Combination.Platform, Combination.ProjectTarget.TargetFilePath.GetFileNameWithoutAnyExtensions(), String.Join(", ", Combination.ProjectTarget.SupportedPlatforms.Select(x => x.ToString())));
				}
				else
				{
					TargetRules TargetRulesObject = Combination.ProjectTarget.TargetRules;
					FileReference TargetFilePath = Combination.ProjectTarget.TargetFilePath;
					string TargetName = TargetFilePath.GetFileNameWithoutAnyExtensions();
					string UBTPlatformName = Platform.ToString();
					string UBTConfigurationName = Configuration.ToString();

					// Setup output path
					UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);

					// Figure out if this is a monolithic build
					bool bShouldCompileMonolithic = BuildPlatform.ShouldCompileMonolithicBinary(Platform);
					bShouldCompileMonolithic |= (Combination.ProjectTarget.CreateRulesDelegate(Platform, Configuration).GetLegacyLinkType(Platform, Configuration) == TargetLinkType.Monolithic);

					// Get the output directory
					DirectoryReference RootDirectory = UnrealBuildTool.EngineDirectory;
					if (TargetRulesObject.Type != TargetType.Program && (bShouldCompileMonolithic || TargetRulesObject.BuildEnvironment == TargetBuildEnvironment.Unique) && !TargetRulesObject.bOutputToEngineBinaries)
					{
						if(Combination.ProjectTarget.UnrealProjectFilePath != null)
						{
							RootDirectory = Combination.ProjectTarget.UnrealProjectFilePath.Directory;
						}
					}

					if (TargetRulesObject.Type == TargetType.Program && !TargetRulesObject.bOutputToEngineBinaries && Combination.ProjectTarget.UnrealProjectFilePath != null)
					{
						RootDirectory = Combination.ProjectTarget.UnrealProjectFilePath.Directory;
					}

					// Get the output directory
					DirectoryReference OutputDirectory = DirectoryReference.Combine(RootDirectory, "Binaries", UBTPlatformName);

					if (!string.IsNullOrEmpty(TargetRulesObject.ExeBinariesSubFolder))
					{
						OutputDirectory = DirectoryReference.Combine(OutputDirectory, TargetRulesObject.ExeBinariesSubFolder);
					}

					// Get the executable name (minus any platform or config suffixes)
					string BaseExeName = TargetName;
					if (!bShouldCompileMonolithic && TargetRulesObject.Type != TargetType.Program)
					{
						// Figure out what the compiled binary will be called so that we can point the IDE to the correct file
						string TargetConfigurationName = TargetRulesObject.Type.ToString();
						if (TargetConfigurationName != TargetType.Game.ToString() && TargetConfigurationName != TargetType.Program.ToString())
						{
							BaseExeName = "UE4" + TargetConfigurationName;
						}
					}

					// Make the output file path
					FileReference NMakePath = FileReference.Combine(OutputDirectory, BaseExeName);
					if (Configuration != TargetRulesObject.UndecoratedConfiguration && (Configuration != UnrealTargetConfiguration.DebugGame || bShouldCompileMonolithic))
					{
						NMakePath += "-" + UBTPlatformName + "-" + UBTConfigurationName;
					}
					NMakePath += TargetRulesObject.Architecture;
					NMakePath += BuildPlatform.GetBinaryExtension(UEBuildBinaryType.Executable);

					VCProjectFileContent.Append(
						"	<PropertyGroup " + ConditionString + ">" + ProjectFileGenerator.NewLine);

					string PathStrings = (ProjGenerator != null) ? ProjGenerator.GetVisualStudioPathsEntries(Platform, Configuration, TargetRulesObject.Type, TargetFilePath, ProjectFilePath, NMakePath, ProjectFileFormat) : "";
					if (string.IsNullOrEmpty(PathStrings) || (PathStrings.Contains("<IntDir>") == false))
					{
						string ProjectRelativeUnusedDirectory = "$(ProjectDir)..\\Build\\Unused";
						VCProjectFileContent.Append(
							PathStrings +
							"		<OutDir>" + ProjectRelativeUnusedDirectory + Path.DirectorySeparatorChar + "</OutDir>" + ProjectFileGenerator.NewLine +
							"		<IntDir>" + ProjectRelativeUnusedDirectory + Path.DirectorySeparatorChar + "</IntDir>" + ProjectFileGenerator.NewLine);
					}
					else
					{
						VCProjectFileContent.Append(PathStrings);
					}

					if (TargetRulesObject.Type == TargetType.Game || TargetRulesObject.Type == TargetType.Client || TargetRulesObject.Type == TargetType.Server)
					{
						// Allow platforms to add any special properties they require... like aumid override for Xbox One
						UEPlatformProjectGenerator.GenerateGamePlatformSpecificProperties(Platform, Configuration, TargetRulesObject.Type, VCProjectFileContent, RootDirectory, TargetFilePath);
					}

					// This is the standard UE4 based project NMake build line:
					//	..\..\Build\BatchFiles\Build.bat <TARGETNAME> <PLATFORM> <CONFIGURATION>
					//	ie ..\..\Build\BatchFiles\Build.bat BlankProgram Win64 Debug

					string BuildArguments = " " + TargetName + " " + UBTPlatformName + " " + UBTConfigurationName;
					if (ProjectFileGenerator.bUsePrecompiled)
					{
						BuildArguments += " -useprecompiled";
					}
					if (IsForeignProject)
					{
						BuildArguments += " " + UProjectPath;
					}

					// Always wait for the mutex between UBT invocations, so that building the whole solution doesn't fail.
					BuildArguments += " -waitmutex";

					if (bUseFastPDB)
					{
						// Pass Fast PDB option to make use of Visual Studio's /DEBUG:FASTLINK option
						BuildArguments += " -FastPDB";
					}

					DirectoryReference BatchFilesDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Build", "BatchFiles");

					if(BuildToolOverride != null)
					{
						BuildArguments += BuildToolOverride;
					}

					// NMake Build command line
					VCProjectFileContent.Append("		<NMakeBuildCommandLine>");
					VCProjectFileContent.Append(EscapePath(NormalizeProjectPath(FileReference.Combine(BatchFilesDirectory, "Build.bat"))) + BuildArguments.ToString());
					VCProjectFileContent.Append("</NMakeBuildCommandLine>" + ProjectFileGenerator.NewLine);

					// NMake ReBuild command line
					VCProjectFileContent.Append("		<NMakeReBuildCommandLine>");
					VCProjectFileContent.Append(EscapePath(NormalizeProjectPath(FileReference.Combine(BatchFilesDirectory, "Rebuild.bat"))) + BuildArguments.ToString());
					VCProjectFileContent.Append("</NMakeReBuildCommandLine>" + ProjectFileGenerator.NewLine);

					// NMake Clean command line
					VCProjectFileContent.Append("		<NMakeCleanCommandLine>");
					VCProjectFileContent.Append(EscapePath(NormalizeProjectPath(FileReference.Combine(BatchFilesDirectory, "Clean.bat"))) + BuildArguments.ToString());
					VCProjectFileContent.Append("</NMakeCleanCommandLine>" + ProjectFileGenerator.NewLine);

					VCProjectFileContent.Append("		<NMakeOutput>");
					VCProjectFileContent.Append(NormalizeProjectPath(NMakePath.FullName));
					VCProjectFileContent.Append("</NMakeOutput>" + ProjectFileGenerator.NewLine);
					VCProjectFileContent.Append("	</PropertyGroup>" + ProjectFileGenerator.NewLine);

					string LayoutDirString = (ProjGenerator != null) ? ProjGenerator.GetVisualStudioLayoutDirSection(Platform, Configuration, ConditionString, Combination.ProjectTarget.TargetRules.Type, Combination.ProjectTarget.TargetFilePath, ProjectFilePath, NMakePath, ProjectFileFormat) : "";
					VCProjectFileContent.Append(LayoutDirString);
				}

				if (VCUserFileContent != null && Combination.ProjectTarget != null)
				{
					TargetRules TargetRulesObject = Combination.ProjectTarget.TargetRules;

					if ((Platform == UnrealTargetPlatform.Win32) || (Platform == UnrealTargetPlatform.Win64))
					{
						VCUserFileContent.Append(
							"	<PropertyGroup " + ConditionString + ">" + ProjectFileGenerator.NewLine);
						if (TargetRulesObject.Type != TargetType.Game)
						{
							string DebugOptions = "";

							if (IsForeignProject)
							{
								DebugOptions += UProjectPath;
								DebugOptions += " -skipcompile";
							}
							else if (TargetRulesObject.Type == TargetType.Editor && ProjectName == ProjectFileGenerator.EnterpriseProjectFileNameBase)
							{
								DebugOptions += " -enterprise";
							}
							else if (TargetRulesObject.Type == TargetType.Editor && ProjectName != "UE4")
							{
								DebugOptions += ProjectName;
							}

							if (Configuration == UnrealTargetConfiguration.Debug || Configuration == UnrealTargetConfiguration.DebugGame)
							{
								DebugOptions += " -debug";
							}

							VCUserFileContent.Append(
								"		<LocalDebuggerCommandArguments>" + DebugOptions + "</LocalDebuggerCommandArguments>" + ProjectFileGenerator.NewLine
								);
						}
						VCUserFileContent.Append(
							"		<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>" + ProjectFileGenerator.NewLine
							);
						VCUserFileContent.Append(
							"	</PropertyGroup>" + ProjectFileGenerator.NewLine
							);
					}

					string PlatformUserFileStrings = (ProjGenerator != null) ? ProjGenerator.GetVisualStudioUserFileStrings(Platform, Configuration, ConditionString, TargetRulesObject, Combination.ProjectTarget.TargetFilePath, ProjectFilePath) : "";
					VCUserFileContent.Append(PlatformUserFileStrings);
				}
			}
		}
	}


	/// <summary>
	/// A Visual C# project.
	/// </summary>
	class VCSharpProjectFile : MSBuildProjectFile
	{
		/// <summary>
		/// This is the GUID that Visual Studio uses to identify a C# project file in the solution
		/// </summary>
		public override string ProjectTypeGUID
		{
			get { return "{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}"; }
		}

		/// <summary>
		/// Constructs a new project file object
		/// </summary>
		/// <param name="InitFilePath">The path to the project file on disk</param>
		public VCSharpProjectFile(FileReference InitFilePath)
			: base(InitFilePath)
		{
		}

		/// <summary>
		/// Extract information from the csproj file based on the supplied configuration
		/// </summary>
		public CsProjectInfo GetProjectInfo(UnrealTargetConfiguration InConfiguration)
		{
			if (CachedProjectInfo.ContainsKey(InConfiguration))
			{
				return CachedProjectInfo[InConfiguration];
			}

			CsProjectInfo Info;

			Dictionary<string, string> Properties = new Dictionary<string, string>();
			Properties.Add("Platform", "AnyCPU");
			Properties.Add("Configuration", InConfiguration.ToString());
			if (CsProjectInfo.TryRead(ProjectFilePath, Properties, out Info))
			{
				CachedProjectInfo.Add(InConfiguration, Info);
			}

			return Info;
		}

		/// <summary>
		/// Determine if this project is a .NET Core project
		/// </summary>
		public bool IsDotNETCoreProject()
		{
			CsProjectInfo Info = GetProjectInfo(UnrealTargetConfiguration.Debug);
			return Info.IsDotNETCoreProject();
		}

		/// <summary>
		/// Reads the list of dependencies from the specified project file.
		/// </summary>
		public List<string> GetCSharpDependencies()
		{
			List<string> RelativeFilePaths = new List<string>();
			XmlDocument Doc = new XmlDocument();
			Doc.Load(ProjectFilePath.FullName);

			string[] Tags = new string[] { "Compile", "Page", "Resource" };
			foreach (string Tag in Tags)
			{
				XmlNodeList Elements = Doc.GetElementsByTagName(Tag);
				foreach (XmlElement Element in Elements)
				{
					RelativeFilePaths.Add(Element.GetAttribute("Include"));
				}
			}

			return RelativeFilePaths;
		}

		/// <summary>
		/// Adds a C# dot net (system) assembly reference to this project
		/// </summary>
		/// <param name="AssemblyReference">The full path to the assembly file on disk</param>
		public void AddDotNetAssemblyReference(string AssemblyReference)
		{
			if (!DotNetAssemblyReferences.Contains(AssemblyReference))
			{
				DotNetAssemblyReferences.Add(AssemblyReference);
			}
		}

		/// <summary>
		/// Adds a C# assembly reference to this project, such as a third party assembly needed for this project to compile
		/// </summary>
		/// <param name="AssemblyReference">The full path to the assembly file on disk</param>
		public void AddAssemblyReference(FileReference AssemblyReference)
		{
			AssemblyReferences.Add(AssemblyReference);
		}

		/// <summary>
		/// Given a target platform and configuration, generates a platform and configuration name string to use in Visual Studio projects.
		/// Unlike with solution configurations, Visual Studio project configurations only support certain types of platforms, so we'll
		/// generate a configuration name that has the platform "built in", and use a default platform type
		/// </summary>
		/// <param name="Platform">Actual platform</param>
		/// <param name="Configuration">Actual configuration</param>
		/// <param name="TargetConfigurationName">The configuration name from the target rules, or null if we don't have one</param>
		/// <param name="ProjectPlatformName">Name of platform string to use for Visual Studio project</param>
		/// <param name="ProjectConfigurationName">Name of configuration string to use for Visual Studio project</param>
		public override void MakeProjectPlatformAndConfigurationNames(UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string TargetConfigurationName, out string ProjectPlatformName, out string ProjectConfigurationName)
		{
			ProjectConfigurationName = Configuration.ToString();
			ProjectPlatformName = VCProjectFileGenerator.DotNetPlatformName;
		}

		/// <summary>
		/// Basic csproj file support. Generates C# library project with one build config.
		/// </summary>
		/// <param name="InPlatforms">Not used.</param>
		/// <param name="InConfigurations">Not Used.</param>
		/// <returns>true if the opration was successful, false otherwise</returns>
		public override bool WriteProjectFile(List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations)
		{
			throw new BuildException("Support for writing C# projects from UnrealBuildTool has been removed.");
		}

		/// Assemblies this project is dependent on
		protected readonly List<FileReference> AssemblyReferences = new List<FileReference>();
		/// System assemblies this project is dependent on
		protected readonly List<string> DotNetAssemblyReferences = new List<string>() { "System", "System.Core", "System.Data", "System.Xml" };
		/// Cache of parsed info about this project
		protected readonly Dictionary<UnrealTargetConfiguration, CsProjectInfo> CachedProjectInfo = new Dictionary<UnrealTargetConfiguration, CsProjectInfo>();
	}

}
