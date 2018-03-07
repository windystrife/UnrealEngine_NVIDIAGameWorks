using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace AutomationTool
{
	[Help("Re-save all the project descriptors under a given path")]
	[Help("EngineAssociation", "Sets the EngineAssociation field for each project")]
	[Help("Force", "Remove the read-only flag from output files")]
	class ResaveProjectDescriptors : BuildCommand
	{
		public static void FindProjects(DirectoryReference BaseDir, List<FileReference> ProjectFiles)
		{
			if(BaseDir != CommandUtils.EngineDirectory)
			{
				int InitialProjectFileCount = ProjectFiles.Count;

				ProjectFiles.AddRange(DirectoryReference.EnumerateFiles(BaseDir, "*.uproject"));

				if(InitialProjectFileCount == ProjectFiles.Count)
				{
					foreach(DirectoryReference SubDir in DirectoryReference.EnumerateDirectories(BaseDir))
					{
						FindProjects(SubDir, ProjectFiles);
					}
				}
			}
		}

		public override void ExecuteBuild()
		{
			string RootDirParam = ParseParamValue("RootDir", null);
			if(RootDirParam == null)
			{
				throw new AutomationException("Missing -BaseDir=... parameter");
			}

			string EngineAssociation = ParseParamValue("EngineAssociation", null);

			bool bForce = ParseParam("Force");

			List<FileReference> ProjectFiles = new List<FileReference>();
			FindProjects(new DirectoryReference(RootDirParam), ProjectFiles);

			foreach (FileReference ProjectFile in ProjectFiles)
			{
				Log("Reading {0}", ProjectFile);
				string InputText = File.ReadAllText(ProjectFile.FullName);

				// Parse the descriptor
				ProjectDescriptor Descriptor;
				try
				{
					Descriptor = new ProjectDescriptor(JsonObject.Parse(InputText));
				}
				catch(JsonParseException Ex)
				{
					LogError("Unable to parse {0}: {1}", ProjectFile, Ex.ToString());
					continue;
				}

				// Update any metadata for the project
				if(EngineAssociation != null)
				{
					Descriptor.EngineAssociation = EngineAssociation;
				}

				// Find all the plugins for this project, and update all the plugin references in the descriptor
				if(Descriptor.Plugins != null)
				{
					Dictionary<string, PluginInfo> NameToPluginInfo = new Dictionary<string, PluginInfo>(StringComparer.InvariantCultureIgnoreCase);
					foreach(PluginInfo Plugin in Plugins.ReadEnginePlugins(EngineDirectory))
					{
						NameToPluginInfo[Plugin.Name] = Plugin;
					}
					foreach(PluginInfo Plugin in Plugins.ReadProjectPlugins(ProjectFile.Directory))
					{
						NameToPluginInfo[Plugin.Name] = Plugin;
					}
					foreach(PluginReferenceDescriptor Reference in Descriptor.Plugins)
					{
						if(Reference.bEnabled)
						{
							PluginInfo Info;
							if(NameToPluginInfo.TryGetValue(Reference.Name, out Info))
							{
								Reference.SupportedTargetPlatforms = Info.Descriptor.SupportedTargetPlatforms;
							}
						}
						else
						{
							Reference.SupportedTargetPlatforms = null;
						}
					}
				}

				// Format the output text
				StringBuilder Output = new StringBuilder();
				using (JsonWriter Writer = new JsonWriter(new StringWriter(Output)))
				{
					Writer.WriteObjectStart();
					Descriptor.Write(Writer);
					Writer.WriteObjectEnd();
				}

				// Compare the output and input; write it if it differs
				string OutputText = Output.ToString();
				if(InputText != OutputText)
				{
					if(CommandUtils.IsReadOnly(ProjectFile.FullName))
					{
						if(!bForce)
						{
							LogWarning("File is read only; skipping write.");
							continue;
						}
						CommandUtils.SetFileAttributes(ProjectFile.FullName, ReadOnly: false);
					}
					Log("  Writing updated file.", ProjectFile);
					FileReference.WriteAllText(ProjectFile, OutputText);
				}
			}
		}
	}
}
