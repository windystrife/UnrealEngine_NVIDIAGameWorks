using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Stores information about a plugin that is being built for a target
	/// </summary>
	class UEBuildPlugin
	{
		/// <summary>
		/// Information about the plugin
		/// </summary>
		public PluginInfo Info;

		/// <summary>
		/// Modules that this plugin belongs to
		/// </summary>
		public List<UEBuildModule> Modules = new List<UEBuildModule>();

		/// <summary>
		/// Recursive
		/// </summary>
		public HashSet<UEBuildPlugin> Dependencies;

		/// <summary>
		/// Whether the descriptor for this plugin is needed at runtime; because it has modules or content which is used, or because it references another module that does.
		/// </summary>
		public bool bDescriptorNeededAtRuntime;

		/// <summary>
		/// Whether this descriptor is referenced non-optionally by something else; a project file or other plugin. This is recursively applied to the plugin's references.
		/// </summary>
		public bool bDescriptorReferencedExplicitly;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Info">The static plugin information</param>
		public UEBuildPlugin(PluginInfo Info)
		{
			this.Info = Info;
		}

		/// <summary>
		/// Accessor for the name of this plugin
		/// </summary>
		public string Name
		{
			get { return Info.Name; }
		}

		/// <summary>
		/// Accessor for the file for this plugin
		/// </summary>
		public FileReference File
		{
			get { return Info.File; }
		}

		/// <summary>
		/// Accessor for the type of the plugin
		/// </summary>
		public PluginType Type
		{
			get { return Info.Type; }
		}

		/// <summary>
		/// Accessor for this plugin's root directory
		/// </summary>
		public DirectoryReference Directory
		{
			get { return Info.Directory; }
		}

		/// <summary>
		/// Accessor for this plugin's descriptor
		/// </summary>
		public PluginDescriptor Descriptor
		{
			get { return Info.Descriptor; }
		}

		/// <summary>
		/// Returns the name of this plugin for debugging
		/// </summary>
		/// <returns>Name of the plugin</returns>
		public override string ToString()
		{
			return Info.Name;
		}
	}
}
