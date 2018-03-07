// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for platform-specific project generators
	/// </summary>
	abstract class UEPlatformProjectGenerator
	{
		static Dictionary<UnrealTargetPlatform, UEPlatformProjectGenerator> ProjectGeneratorDictionary = new Dictionary<UnrealTargetPlatform, UEPlatformProjectGenerator>();

		/// <summary>
		/// Register the given platforms UEPlatformProjectGenerator instance
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform to register with</param>
		/// <param name="InProjectGenerator">The UEPlatformProjectGenerator instance to use for the InPlatform</param>
		public static void RegisterPlatformProjectGenerator(UnrealTargetPlatform InPlatform, UEPlatformProjectGenerator InProjectGenerator)
		{
			// Make sure the build platform is legal
			var BuildPlatform = UEBuildPlatform.GetBuildPlatform(InPlatform, true);
			if (BuildPlatform != null)
			{
				if (ProjectGeneratorDictionary.ContainsKey(InPlatform) == true)
				{
					Log.TraceInformation("RegisterPlatformProjectGenerator Warning: Registering project generator {0} for {1} when it is already set to {2}",
						InProjectGenerator.ToString(), InPlatform.ToString(), ProjectGeneratorDictionary[InPlatform].ToString());
					ProjectGeneratorDictionary[InPlatform] = InProjectGenerator;
				}
				else
				{
					ProjectGeneratorDictionary.Add(InPlatform, InProjectGenerator);
				}
			}
			else
			{
				Log.TraceVerbose("Skipping project file generator registration for {0} due to no valid BuildPlatform.", InPlatform.ToString());
			}
		}

		/// <summary>
		/// Retrieve the UEPlatformProjectGenerator instance for the given TargetPlatform
		/// </summary>
		/// <param name="InPlatform">    The UnrealTargetPlatform being built</param>
		/// <param name="bInAllowFailure">   If true, do not throw an exception and return null</param>
		/// <returns>UEPlatformProjectGenerator The instance of the project generator</returns>
		public static UEPlatformProjectGenerator GetPlatformProjectGenerator(UnrealTargetPlatform InPlatform, bool bInAllowFailure = false)
		{
			if (ProjectGeneratorDictionary.ContainsKey(InPlatform) == true)
			{
				return ProjectGeneratorDictionary[InPlatform];
			}
			if (bInAllowFailure == true)
			{
				return null;
			}
			throw new BuildException("GetPlatformProjectGenerator: No PlatformProjectGenerator found for {0}", InPlatform.ToString());
		}

		/// <summary>
		/// Allow various platform project generators to generate stub projects if required
		/// </summary>
		/// <param name="InGenerator"></param>
		/// <param name="InTargetName"></param>
		/// <param name="InTargetFilepath"></param>
		/// <param name="InTargetRules"></param>
		/// <param name="InPlatforms"></param>
		/// <param name="InConfigurations"></param>
		/// <returns></returns>
		public static bool GenerateGameProjectStubs(ProjectFileGenerator InGenerator, string InTargetName, string InTargetFilepath, TargetRules InTargetRules,
			List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations)
		{
			foreach (KeyValuePair<UnrealTargetPlatform, UEPlatformProjectGenerator> Entry in ProjectGeneratorDictionary)
			{
				UEPlatformProjectGenerator ProjGen = Entry.Value;
				ProjGen.GenerateGameProjectStub(InGenerator, InTargetName, InTargetFilepath, InTargetRules, InPlatforms, InConfigurations);
			}
			return true;
		}

		/// <summary>
		/// Allow various platform project generators to generate any special project properties if required
		/// </summary>
		/// <param name="InPlatform"></param>
		/// <param name="Configuration"></param>
		/// <param name="TargetType"></param>
		/// <param name="VCProjectFileContent"></param>
		/// <param name="RootDirectory"></param>
		/// <param name="TargetFilePath"></param>
		/// <returns></returns>
		public static bool GenerateGamePlatformSpecificProperties(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration Configuration, TargetType TargetType, StringBuilder VCProjectFileContent, DirectoryReference RootDirectory, FileReference TargetFilePath)
		{
			if (ProjectGeneratorDictionary.ContainsKey(InPlatform) == true)
			{
				ProjectGeneratorDictionary[InPlatform].GenerateGameProperties(Configuration, VCProjectFileContent, TargetType, RootDirectory, TargetFilePath); ;
			}
			return true;
		}

		public static bool PlatformRequiresVSUserFileGeneration(List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations)
		{
			bool bRequiresVSUserFileGeneration = false;
			foreach (KeyValuePair<UnrealTargetPlatform, UEPlatformProjectGenerator> Entry in ProjectGeneratorDictionary)
			{
				if (InPlatforms.Contains(Entry.Key))
				{
					UEPlatformProjectGenerator ProjGen = Entry.Value;
					bRequiresVSUserFileGeneration |= ProjGen.RequiresVSUserFileGeneration();
				}
			}
			return bRequiresVSUserFileGeneration;
		}

		/// <summary>
		/// Register the platform with the UEPlatformProjectGenerator class
		/// </summary>
		public abstract void RegisterPlatformProjectGenerator();

		public virtual void GenerateGameProjectStub(ProjectFileGenerator InGenerator, string InTargetName, string InTargetFilepath, TargetRules InTargetRules,
			List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations)
		{
			// Do nothing
		}

		public virtual void GenerateGameProperties(UnrealTargetConfiguration Configuration, StringBuilder VCProjectFileContent, TargetType TargetType, DirectoryReference RootDirectory, FileReference TargetFilePath)
		{
			// Do nothing
		}

		public virtual bool RequiresVSUserFileGeneration()
		{
			return false;
		}


		///
		///	VisualStudio project generation functions
		///	
		/// <summary>
		/// Whether this build platform has native support for VisualStudio
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <param name="ProjectFileFormat">The visual studio project file format being generated</param>
		/// <returns>bool    true if native VisualStudio support (or custom VSI) is available</returns>
		public virtual bool HasVisualStudioSupport(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, VCProjectFileFormat ProjectFileFormat)
		{
			// By default, we assume this is true
			return true;
		}

		/// <summary>
		/// Return the VisualStudio platform name for this build platform
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <returns>string    The name of the platform that VisualStudio recognizes</returns>
		public virtual string GetVisualStudioPlatformName(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			// By default, return the platform string
			return InPlatform.ToString();
		}

		/// <summary>
		/// Return project configuration settings that must be included before the default props file
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <returns>string    The custom configuration section for the project file; Empty string if it doesn't require one</returns>
		public virtual string GetVisualStudioPreDefaultString(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return "";
		}

		/// <summary>
		/// Return the platform toolset string to write into the project configuration
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <param name="InProjectFileFormat">The visual studio project file format being generated</param>
		/// <returns>string    The custom configuration section for the project file; Empty string if it doesn't require one</returns>
		public virtual string GetVisualStudioPlatformToolsetString(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, VCProjectFileFormat InProjectFileFormat)
		{
			return "";
		}

		/// <summary>
		/// Return any custom property group lines
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InProjectFileFormat">The visual studio project file format being generated</param>
		/// <returns>string    The custom property import lines for the project file; Empty string if it doesn't require one</returns>
		public virtual string GetAdditionalVisualStudioPropertyGroups(UnrealTargetPlatform InPlatform, VCProjectFileFormat InProjectFileFormat)
		{
			return "";
		}

		/// <summary>
		/// Return any custom property group lines
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InProjectFileFormat">The visual studio project file format being generated</param>
		/// <returns>string    The platform configuration type.  Defaults to "Makefile" unless overridden</returns>
		public virtual string GetVisualStudioPlatformConfigurationType(UnrealTargetPlatform InPlatform, VCProjectFileFormat InProjectFileFormat)
		{
			return "Makefile";
		}

		/// <summary>
		/// Return any custom paths for VisualStudio this platform requires
		/// This include ReferencePath, LibraryPath, LibraryWPath, IncludePath and ExecutablePath.
		/// </summary>
		/// <param name="InPlatform">The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration">The configuration being built</param>
		/// <param name="TargetType">The type of target (game or program)</param>
		/// <param name="TargetRulesPath">Path to the .target.cs file</param>
		/// <param name="ProjectFilePath"></param>
		/// <param name="NMakeOutputPath"></param>
		/// <param name="InProjectFileFormat">The visual studio project file format being generated</param>
		/// <returns>The custom path lines for the project file; Empty string if it doesn't require one</returns>
		public virtual string GetVisualStudioPathsEntries(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, TargetType TargetType, FileReference TargetRulesPath, FileReference ProjectFilePath, FileReference NMakeOutputPath, VCProjectFileFormat InProjectFileFormat)
		{
			// NOTE: We are intentionally overriding defaults for these paths with empty strings.  We never want Visual Studio's
			//       defaults for these fields to be propagated, since they are version-sensitive paths that may not reflect
			//       the environment that UBT is building in.  We'll set these environment variables ourselves!
			// NOTE: We don't touch 'ExecutablePath' because that would result in Visual Studio clobbering the system "Path"
			//       environment variable
			string PathsLines =
				"		<IncludePath />\n" +
				"		<ReferencePath />\n" +
				"		<LibraryPath />\n" +
				"		<LibraryWPath />\n" +
				"		<SourcePath />\n" +
				"		<ExcludePath />\n";

			return PathsLines;
		}

		/// <summary>
		/// Return any custom property settings. These will be included in the ImportGroup section
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <returns>string    The custom property import lines for the project file; Empty string if it doesn't require one</returns>
		public virtual string GetVisualStudioImportGroupProperties(UnrealTargetPlatform InPlatform)
		{
			return "";
		}

		/// <summary>
		/// Return any custom property settings. These will be included right after Global properties to make values available to all other imports.
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <returns>string    The custom property import lines for the project file; Empty string if it doesn't require one</returns>
		public virtual string GetVisualStudioGlobalProperties(UnrealTargetPlatform InPlatform)
		{
			return "";
		}

		/// <summary>
		/// Return any custom target overrides. These will be included last in the project file so they have the opportunity to override any existing settings.
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InProjectFileFormat">The visual studio project file format being generated</param>
		/// <returns>string    The custom property import lines for the project file; Empty string if it doesn't require one</returns>
		public virtual string GetVisualStudioTargetOverrides(UnrealTargetPlatform InPlatform, VCProjectFileFormat InProjectFileFormat)
		{
			return "";
		}

		/// <summary>
		/// Return any custom layout directory sections
		/// </summary>
		/// <param name="InPlatform">The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration">The configuration being built</param>
		/// <param name="InConditionString"></param>
		/// <param name="TargetType">The type of target (game or program)</param>
		/// <param name="InProjectFileFormat">The visual studio project file format being generated</param>
		/// <param name="NMakeOutputPath"></param>
		/// <param name="ProjectFilePath"></param>
		/// <param name="TargetRulesPath"></param>
		/// <returns>string    The custom property import lines for the project file; Empty string if it doesn't require one</returns>
		public virtual string GetVisualStudioLayoutDirSection(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, string InConditionString, TargetType TargetType, FileReference TargetRulesPath, FileReference ProjectFilePath, FileReference NMakeOutputPath, VCProjectFileFormat InProjectFileFormat)
		{
			return "";
		}

		/// <summary>
		/// Get the output manifest section, if required
		/// </summary>
		/// <param name="InPlatform">The UnrealTargetPlatform being built</param>
		/// <param name="TargetType">The type of the target being built</param>
		/// <param name="TargetRulesPath">Path to the .target.cs file</param>
		/// <param name="ProjectFilePath">Path to the project file</param>
		/// <param name="InProjectFileFormat">The visual studio project file format being generated</param>
		/// <returns>The output manifest section for the project file; Empty string if it doesn't require one</returns>
		public virtual string GetVisualStudioOutputManifestSection(UnrealTargetPlatform InPlatform, TargetType TargetType, FileReference TargetRulesPath, FileReference ProjectFilePath, VCProjectFileFormat InProjectFileFormat)
		{
			return "";
		}

		/// <summary>
		/// Get whether this platform deploys
		/// </summary>
		/// <returns>bool  true if the 'Deploy' option should be enabled</returns>
		public virtual bool GetVisualStudioDeploymentEnabled(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return false;
		}

		/// <summary>
		/// Get the text to insert into the user file for the given platform/configuration/target
		/// </summary>
		/// <param name="InPlatform">The platform being added</param>
		/// <param name="InConfiguration">The configuration being added</param>
		/// <param name="InConditionString">The condition string </param>
		/// <param name="InTargetRules">The target rules </param>
		/// <param name="TargetRulesPath">The target rules path</param>
		/// <param name="ProjectFilePath">The project file path</param>
		/// <returns>The string to append to the user file</returns>
		public virtual string GetVisualStudioUserFileStrings(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration,
			string InConditionString, TargetRules InTargetRules, FileReference TargetRulesPath, FileReference ProjectFilePath)
		{
			return "";
		}

		/// <summary>
		/// For Additional Project Property files that need to be written out.  This is currently used only on Android. 
		/// </summary>
		public virtual void WriteAdditionalPropFile()
		{
		}

		/// <summary>
		/// For additional Project files (ex. *PROJECTNAME*-AndroidRun.androidproj.user) that needs to be written out.  This is currently used only on Android. 
		/// </summary>
		/// <param name="ProjectFile">Project file this will be related to</param>
		public virtual void WriteAdditionalProjUserFile(ProjectFile ProjectFile)
		{
		}

		/// <summary>
		/// For additional Project files (ex. *PROJECTNAME*-AndroidRun.androidproj) that needs to be written out.  This is currently used only on Android. 
		/// </summary>
		/// <param name="ProjectFile">Project file this will be related to</param>
		/// <returns>Project file written out, Solution folder it should be put in</returns>
		public virtual Tuple<ProjectFile, string> WriteAdditionalProjFile(ProjectFile ProjectFile)
		{
			return null;
		}
	}
}
