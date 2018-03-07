using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Representation of a reference to a plugin from a project file
	/// </summary>
	[DebuggerDisplay("Name={Name}")]
	public class PluginReferenceDescriptor
	{
		/// <summary>
		/// Name of the plugin
		/// </summary>
		public string Name;

		/// <summary>
		/// Whether it should be enabled by default
		/// </summary>
		public bool bEnabled;

		/// <summary>
		/// Whether this plugin is optional, and the game should silently ignore it not being present
		/// </summary>
		public bool bOptional;

		/// <summary>
		/// Description of the plugin for users that do not have it installed.
		/// </summary>
		public string Description;

		/// <summary>
		/// URL for this plugin on the marketplace, if the user doesn't have it installed.
		/// </summary>
		public string MarketplaceURL;

		/// <summary>
		/// If enabled, list of platforms for which the plugin should be enabled (or all platforms if blank).
		/// </summary>
		public UnrealTargetPlatform[] WhitelistPlatforms;

		/// <summary>
		/// If enabled, list of platforms for which the plugin should be disabled.
		/// </summary>
		public UnrealTargetPlatform[] BlacklistPlatforms;

		/// <summary>
		/// If enabled, list of targets for which the plugin should be enabled (or all targets if blank).
		/// </summary>
		public TargetType[] WhitelistTargets;

		/// <summary>
		/// If enabled, list of targets for which the plugin should be disabled.
		/// </summary>
		public TargetType[] BlacklistTargets;

		/// <summary>
		/// The list of supported platforms for this plugin. This field is copied from the plugin descriptor, and supplements the user's whitelisted and blacklisted platforms.
		/// </summary>
		public UnrealTargetPlatform[] SupportedTargetPlatforms;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InName">Name of the plugin</param>
		/// <param name="InMarketplaceURL">The marketplace URL for plugins which are not installed</param>
		/// <param name="bInEnabled">Whether the plugin is enabled</param>
		public PluginReferenceDescriptor(string InName, string InMarketplaceURL, bool bInEnabled)
		{
			Name = InName;
			MarketplaceURL = InMarketplaceURL;
			bEnabled = bInEnabled;
		}

		/// <summary>
		/// Construct a PluginReferenceDescriptor from a Json object
		/// </summary>
		/// <param name="Writer">The writer for output fields</param>
		public void Write(JsonWriter Writer)
		{
			Writer.WriteObjectStart();
			Writer.WriteValue("Name", Name);
			Writer.WriteValue("Enabled", bEnabled);
			if(bEnabled && bOptional)
			{
				Writer.WriteValue("Optional", bOptional);
			}
			if(!String.IsNullOrEmpty(Description))
			{
				Writer.WriteValue("Description", Description);
			}
			if(!String.IsNullOrEmpty(MarketplaceURL))
			{
				Writer.WriteValue("MarketplaceURL", MarketplaceURL);
			}
			if(WhitelistPlatforms != null && WhitelistPlatforms.Length > 0)
			{
				Writer.WriteEnumArrayField("WhitelistPlatforms", WhitelistPlatforms);
			}
			if(BlacklistPlatforms != null && BlacklistPlatforms.Length > 0)
			{
				Writer.WriteEnumArrayField("BlacklistPlatforms", BlacklistPlatforms);
			}
			if(WhitelistTargets != null && WhitelistTargets.Length > 0)
			{
				Writer.WriteEnumArrayField("WhitelistTargets", WhitelistTargets);
			}
			if(BlacklistTargets != null && BlacklistTargets.Length > 0)
			{
				Writer.WriteEnumArrayField("BlacklistTargets", BlacklistTargets);
			}
			if(SupportedTargetPlatforms != null && SupportedTargetPlatforms.Length > 0)
			{
				Writer.WriteEnumArrayField("SupportedTargetPlatforms", SupportedTargetPlatforms);
			}
			Writer.WriteObjectEnd();
		}

		/// <summary>
		/// Write an array of module descriptors
		/// </summary>
		/// <param name="Writer">The Json writer to output to</param>
		/// <param name="Name">Name of the array</param>
		/// <param name="Plugins">Array of plugins</param>
		public static void WriteArray(JsonWriter Writer, string Name, PluginReferenceDescriptor[] Plugins)
		{
			if (Plugins != null && Plugins.Length > 0)
			{
				Writer.WriteArrayStart(Name);
				foreach (PluginReferenceDescriptor Plugin in Plugins)
				{
					Plugin.Write(Writer);
				}
				Writer.WriteArrayEnd();
			}
		}

		/// <summary>
		/// Construct a PluginReferenceDescriptor from a Json object
		/// </summary>
		/// <param name="RawObject">The Json object containing a plugin reference descriptor</param>
		/// <returns>New PluginReferenceDescriptor object</returns>
		public static PluginReferenceDescriptor FromJsonObject(JsonObject RawObject)
		{
			PluginReferenceDescriptor Descriptor = new PluginReferenceDescriptor(RawObject.GetStringField("Name"), null, RawObject.GetBoolField("Enabled"));
			RawObject.TryGetBoolField("Optional", out Descriptor.bOptional);
			RawObject.TryGetStringField("Description", out Descriptor.Description);
			RawObject.TryGetStringField("MarketplaceURL", out Descriptor.MarketplaceURL);
			RawObject.TryGetEnumArrayField<UnrealTargetPlatform>("WhitelistPlatforms", out Descriptor.WhitelistPlatforms);
			RawObject.TryGetEnumArrayField<UnrealTargetPlatform>("BlacklistPlatforms", out Descriptor.BlacklistPlatforms);
			RawObject.TryGetEnumArrayField<TargetType>("WhitelistTargets", out Descriptor.WhitelistTargets);
			RawObject.TryGetEnumArrayField<TargetType>("BlacklistTargets", out Descriptor.BlacklistTargets);
			RawObject.TryGetEnumArrayField<UnrealTargetPlatform>("SupportedTargetPlatforms", out Descriptor.SupportedTargetPlatforms);
			return Descriptor;
		}

		/// <summary>
		/// Determines if this reference enables the plugin for a given platform
		/// </summary>
		/// <param name="Platform">The platform to check</param>
		/// <returns>True if the plugin should be enabled</returns>
		public bool IsEnabledForPlatform(UnrealTargetPlatform Platform)
		{
			if (!bEnabled)
			{
				return false;
			}
			if (WhitelistPlatforms != null && WhitelistPlatforms.Length > 0 && !WhitelistPlatforms.Contains(Platform))
			{
				return false;
			}
			if (BlacklistPlatforms != null && BlacklistPlatforms.Contains(Platform))
			{
				return false;
			}
			return true;
		}

		/// <summary>
		/// Determines if this reference enables the plugin for a given target
		/// </summary>
		/// <param name="Target">The target to check</param>
		/// <returns>True if the plugin should be enabled</returns>
		public bool IsEnabledForTarget(TargetType Target)
		{
			if (!bEnabled)
			{
				return false;
			}
			if (WhitelistTargets != null && WhitelistTargets.Length > 0 && !WhitelistTargets.Contains(Target))
			{
				return false;
			}
			if (BlacklistTargets != null && BlacklistTargets.Contains(Target))
			{
				return false;
			}
			return true;
		}

		/// <summary>
		/// Determines if this reference is valid for the given target platform.
		/// </summary>
		/// <param name="Platform">The platform to check</param>
		/// <returns>True if the plugin for this target platform</returns>
		public bool IsSupportedTargetPlatform(UnrealTargetPlatform Platform)
		{
			return SupportedTargetPlatforms == null || SupportedTargetPlatforms.Length == 0 || SupportedTargetPlatforms.Contains(Platform);
		}
	}
}
