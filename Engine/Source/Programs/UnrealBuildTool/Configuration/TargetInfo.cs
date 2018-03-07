using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Information about a target, passed along when creating a module descriptor
	/// </summary>
	[Serializable]
	public class TargetInfo : ISerializable
	{
		/// <summary>
		/// Name of the target
		/// </summary>
		public readonly string Name;

		/// <summary>
		/// The platform that the target is being built for
		/// </summary>
		public readonly UnrealTargetPlatform Platform;

		/// <summary>
		/// The configuration being built
		/// </summary>
		public readonly UnrealTargetConfiguration Configuration;

		/// <summary>
		/// Architecture that the target is being built for (or an empty string for the default)
		/// </summary>
		public readonly string Architecture;

		/// <summary>
		/// The project containing the target
		/// </summary>
		public readonly FileReference ProjectFile;

		/// <summary>
		/// The type of the target (if known)
		/// </summary>
		public readonly TargetType? Type;

		/// <summary>
		/// Whether the target is monolithic or not (if known)
		/// </summary>
		public readonly bool? bIsMonolithic;

		/// <summary>
		/// Constructs a TargetInfo for passing to the TargetRules constructor.
		/// </summary>
		/// <param name="Name">Name of the target being built</param>
		/// <param name="Platform">The platform that the target is being built for</param>
		/// <param name="Configuration">The configuration being built</param>
		/// <param name="Architecture">The architecture being built for</param>
		/// <param name="ProjectFile">Path to the project file containing the target</param>
		public TargetInfo(string Name, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string Architecture, FileReference ProjectFile)
		{
			this.Name = Name;
			this.Platform = Platform;
			this.Configuration = Configuration;
			this.Architecture = Architecture;
			this.ProjectFile = ProjectFile;
		}

		/// <summary>
		/// Constructs a TargetInfo for passing to the ModuleRules constructor.
		/// </summary>
		/// <param name="Rules">The constructed target rules object. This is null when passed into a TargetRules constructor, but should be valid at all other times.</param>
		public TargetInfo(ReadOnlyTargetRules Rules)
		{
			this.Name = Rules.Name;
			this.Platform = Rules.Platform;
			this.Configuration = Rules.Configuration;
			this.Architecture = Rules.Architecture;
			this.ProjectFile = Rules.ProjectFile;
			this.Type = Rules.Type;
			this.bIsMonolithic = (Rules.LinkType == TargetLinkType.Monolithic);
		}

		/// <summary>
		/// Reads a TargetInfo object from a binary archve
		/// </summary>
		/// <param name="Info">Serialization info</param>
		/// <param name="Context">Streaming context</param>
		public TargetInfo(SerializationInfo Info, StreamingContext Context)
		{
			Name = Info.GetString("nm");
			Platform = (UnrealTargetPlatform)Info.GetInt32("pl");
			Architecture = Info.GetString("ar");
			Configuration = (UnrealTargetConfiguration)Info.GetInt32("co");
			ProjectFile = (FileReference)Info.GetValue("pf", typeof(FileReference));
			if (Info.GetBoolean("t?"))
			{
				Type = (TargetType)Info.GetInt32("tt");
			}
			if (Info.GetBoolean("m?"))
			{
				bIsMonolithic = Info.GetBoolean("mo");
			}
		}

		/// <summary>
		/// Serialize the object
		/// </summary>
		/// <param name="Info">Serialization info</param>
		/// <param name="Context">Streaming context</param>
		public void GetObjectData(SerializationInfo Info, StreamingContext Context)
		{
			Info.AddValue("nm", Name);
			Info.AddValue("pl", (int)Platform);
			Info.AddValue("ar", Architecture);
			Info.AddValue("co", (int)Configuration);
			Info.AddValue("pf", ProjectFile);
			Info.AddValue("t?", Type.HasValue);
			if (Type.HasValue)
			{
				Info.AddValue("tt", (int)Type.Value);
			}
			Info.AddValue("m?", bIsMonolithic.HasValue);
			if (bIsMonolithic.HasValue)
			{
				Info.AddValue("mo", bIsMonolithic);
			}
		}

		/// <summary>
		/// True if the target type is a cooked game.
		/// </summary>
		public bool IsCooked
		{
			get
			{
				if (!Type.HasValue)
				{
					throw new BuildException("Trying to access TargetInfo.IsCooked when TargetInfo.Type is not set. Make sure IsCooked is used only in ModuleRules.");
				}
				return Type == TargetType.Client || Type == TargetType.Game || Type == TargetType.Server;
			}
		}

		/// <summary>
		/// True if the target type is a monolithic binary
		/// </summary>
		public bool IsMonolithic
		{
			get
			{
				if (!bIsMonolithic.HasValue)
				{
					throw new BuildException("Trying to access TargetInfo.IsMonolithic when bIsMonolithic is not set. Make sure IsMonolithic is used only in ModuleRules.");
				}
				return bIsMonolithic.Value;
			}
		}
	}
}
