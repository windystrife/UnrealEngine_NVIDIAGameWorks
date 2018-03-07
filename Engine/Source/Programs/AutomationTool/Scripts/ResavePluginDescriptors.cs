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
	[Help("Re-save all the plugin descriptors under a given path, optionally applying standard metadata to them")]
	[Help("RootDir=<Path>", "The root directory to enumerate plugins under")]
	[Help("CreatedBy=<Name>", "Author to specify in the 'Created By' field.")]
	[Help("CreatedByUrl=<Url>", "URL to link to for the 'Created By' field.")]
	[Help("Force", "Remove the read-only flag from output files")]
	class ResavePluginDescriptors : BuildCommand
	{
		public override void ExecuteBuild()
		{
			string RootDirParam = ParseParamValue("RootDir", null);
			if(RootDirParam == null)
			{
				throw new AutomationException("Missing -BaseDir=... parameter");
			}

			string CreatedBy = ParseParamValue("CreatedBy", null);
			string CreatedByUrl = ParseParamValue("CreatedByUrl", null);
			bool bForce = ParseParam("Force");

			foreach(FileReference PluginFile in DirectoryReference.EnumerateFiles(new DirectoryReference(RootDirParam), "*.uplugin", System.IO.SearchOption.AllDirectories))
			{
				Log("Reading {0}", PluginFile);
				string InputText = File.ReadAllText(PluginFile.FullName);

				// Parse the descriptor
				PluginDescriptor Descriptor;
				try
				{
					Descriptor = new PluginDescriptor(JsonObject.Parse(InputText));
				}
				catch(JsonParseException Ex)
				{
					LogError("Unable to parse {0}: {1}", PluginFile, Ex.ToString());
					continue;
				}

				// Update the fields
				if(CreatedBy != null && Descriptor.CreatedBy != CreatedBy)
				{
					Log("  Updating 'CreatedBy' field from '{0}' to '{1}'", Descriptor.CreatedBy ?? "<empty>", CreatedBy);
					Descriptor.CreatedBy = CreatedBy;
				}
				if(CreatedByUrl != null)
				{
					Log("  Updating 'CreatedByURL' field from '{0}' to '{1}'", Descriptor.CreatedByURL ?? "<empty>", CreatedByUrl);
					Descriptor.CreatedByURL = CreatedByUrl;
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
					if(CommandUtils.IsReadOnly(PluginFile.FullName))
					{
						if(!bForce)
						{
							LogWarning("File is read only; skipping write.");
							continue;
						}
						CommandUtils.SetFileAttributes(PluginFile.FullName, ReadOnly: false);
					}
					Log("  Writing updated file.", PluginFile);
					FileReference.WriteAllText(PluginFile, OutputText);
				}
			}
		}
	}
}
