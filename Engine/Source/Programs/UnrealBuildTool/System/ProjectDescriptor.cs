// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// The version format for .uproject files. This rarely changes now; project descriptors should maintain backwards compatibility automatically.
	/// </summary>
	enum ProjectDescriptorVersion
	{
		/// <summary>
		/// Invalid
		/// </summary>
		Invalid = 0,

		/// <summary>
		/// Initial version
		/// </summary>
		Initial = 1,

		/// <summary>
		/// Adding SampleNameHash
		/// </summary>
		NameHash = 2,

		/// <summary>
		/// Unifying plugin/project files (since abandoned, but backwards compatibility maintained)
		/// </summary>
		ProjectPluginUnification = 3,

		/// <summary>
        /// This needs to be the last line, so we can calculate the value of Latest below
		/// </summary>
        LatestPlusOne,

		/// <summary>
		/// The latest plugin descriptor version
		/// </summary>
		Latest = LatestPlusOne - 1
	}

	/// <summary>
	/// In-memory representation of a .uproject file
	/// </summary>
	public class ProjectDescriptor
	{
		/// <summary>
		/// Descriptor version number.
		/// </summary>
		public int FileVersion;

		/// <summary>
		/// The engine to open this project with.
		/// </summary>
		public string EngineAssociation;

		/// <summary>
		/// Category to show under the project browser
		/// </summary>
		public string Category;

		/// <summary>
		/// Description to show in the project browser
		/// </summary>
		public string Description;

		/// <summary>
		/// List of all modules associated with this project
		/// </summary>
		public ModuleDescriptor[] Modules;

		/// <summary>
		/// List of plugins for this project (may be enabled/disabled)
		/// </summary>
		public PluginReferenceDescriptor[] Plugins;

		/// <summary>
        /// List of additional plugin directories to scan for available plugins
		/// </summary>
        public string[] AdditionalPluginDirectories;

		/// <summary>
		/// Array of platforms that this project is targeting
		/// </summary>
		public string[] TargetPlatforms;

		/// <summary>
		/// A hash that is used to determine if the project was forked from a sample
		/// </summary>
		public uint EpicSampleNameHash;

		/// <summary>
		/// Steps to execute before building targets in this project
		/// </summary>
		public CustomBuildSteps PreBuildSteps;

		/// <summary>
		/// Steps to execute before building targets in this project
		/// </summary>
		public CustomBuildSteps PostBuildSteps;

		/// <summary>
		/// Indicates if this project is an Enterprise project
		/// </summary>
		public bool IsEnterpriseProject;

		/// <summary>
		/// Constructor.
		/// </summary>
		public ProjectDescriptor()
		{
			FileVersion = (int)ProjectDescriptorVersion.Latest;
			IsEnterpriseProject = false;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="RawObject">Raw JSON object to parse</param>
		public ProjectDescriptor(JsonObject RawObject)
		{
			// Read the version
			if (!RawObject.TryGetIntegerField("FileVersion", out FileVersion))
			{
				if (!RawObject.TryGetIntegerField("ProjectFileVersion", out FileVersion))
				{
					throw new BuildException("Project does not contain a valid FileVersion entry");
				}
			}

			// Check it's not newer than the latest version we can parse
			if (FileVersion > (int)PluginDescriptorVersion.Latest)
			{
				throw new BuildException("Project descriptor appears to be in a newer version ({0}) of the file format that we can load (max version: {1}).", FileVersion, (int)ProjectDescriptorVersion.Latest);
			}

			// Read simple fields
			RawObject.TryGetStringField("EngineAssociation", out EngineAssociation);
			RawObject.TryGetStringField("Category", out Category);
			RawObject.TryGetStringField("Description", out Description);
			RawObject.TryGetBoolField("Enterprise", out IsEnterpriseProject);

			// Read the modules
			JsonObject[] ModulesArray;
			if (RawObject.TryGetObjectArrayField("Modules", out ModulesArray))
			{
				Modules = Array.ConvertAll(ModulesArray, x => ModuleDescriptor.FromJsonObject(x));
			}

			// Read the plugins
			JsonObject[] PluginsArray;
			if (RawObject.TryGetObjectArrayField("Plugins", out PluginsArray))
			{
				Plugins = Array.ConvertAll(PluginsArray, x => PluginReferenceDescriptor.FromJsonObject(x));
			}

			// Read the additional plugin directories
            RawObject.TryGetStringArrayField("AdditionalPluginDirectories", out AdditionalPluginDirectories);

            // Read the target platforms
            RawObject.TryGetStringArrayField("TargetPlatforms", out TargetPlatforms);

            // Get the sample name hash
            RawObject.TryGetUnsignedIntegerField("EpicSampleNameHash", out EpicSampleNameHash);

			// Read the pre and post-build steps
			CustomBuildSteps.TryRead(RawObject, "PreBuildSteps", out PreBuildSteps);
			CustomBuildSteps.TryRead(RawObject, "PostBuildSteps", out PostBuildSteps);
		}

		/// <summary>
		/// Creates a plugin descriptor from a file on disk
		/// </summary>
		/// <param name="FileName">The filename to read</param>
		/// <returns>New plugin descriptor</returns>
		public static ProjectDescriptor FromFile(FileReference FileName)
		{
			JsonObject RawObject = JsonObject.Read(FileName);
			try
			{
				return new ProjectDescriptor(RawObject);
			}
			catch (JsonParseException ParseException)
			{
				throw new JsonParseException("{0} (in {1})", ParseException.Message, FileName);
			}
		}

		/// <summary>
		/// Saves the descriptor to disk
		/// </summary>
		/// <param name="FileName">The filename to write to</param>
		public void Save(string FileName)
		{
			using (JsonWriter Writer = new JsonWriter(FileName))
			{
				Writer.WriteObjectStart();
				Write(Writer);
				Writer.WriteObjectEnd();
			}
		}

		/// <summary>
		/// Writes the plugin descriptor to an existing Json writer
		/// </summary>
		/// <param name="Writer">The writer to receive plugin data</param>
		public void Write(JsonWriter Writer)
		{
			Writer.WriteValue("FileVersion", (int)ProjectDescriptorVersion.Latest);
			Writer.WriteValue("EngineAssociation", EngineAssociation);
			Writer.WriteValue("Category", Category);
			Writer.WriteValue("Description", Description);

			// Write the enterprise flag
			if (IsEnterpriseProject)
			{
				Writer.WriteValue("Enterprise", IsEnterpriseProject);
			}

			// Write the module list
			ModuleDescriptor.WriteArray(Writer, "Modules", Modules);

			// Write the plugin list
			PluginReferenceDescriptor.WriteArray(Writer, "Plugins", Plugins);

			// Write out the additional plugin directories to scan
			if(AdditionalPluginDirectories != null && AdditionalPluginDirectories.Length > 0)
			{
				Writer.WriteStringArrayField("AdditionalPluginDirectories", AdditionalPluginDirectories);
			}

			// Write the target platforms
			if(TargetPlatforms != null && TargetPlatforms.Length > 0)
			{
				Writer.WriteStringArrayField("TargetPlatforms", TargetPlatforms);
			}

			// If it's a signed sample, write the name hash
			if(EpicSampleNameHash != 0)
			{
				Writer.WriteValue("EpicSampleNameHash", (uint)EpicSampleNameHash);
			}

			// Write the custom build steps
			if(PreBuildSteps != null)
			{
				PreBuildSteps.Write(Writer, "PreBuildSteps");
			}
			if(PostBuildSteps != null)
			{
				PostBuildSteps.Write(Writer, "PostBuildSteps");
			}
		}
	}
}
