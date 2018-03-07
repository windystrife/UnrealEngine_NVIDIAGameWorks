// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// 
	/// </summary>
	public enum LocalizationTargetDescriptorLoadingPolicy
	{
		/// <summary>
		/// 
		/// </summary>
		Never,

		/// <summary>
		/// 
		/// </summary>
		Always,

		/// <summary>
		/// 
		/// </summary>
		Editor,

		/// <summary>
		/// 
		/// </summary>
		Game,

		/// <summary>
		/// 
		/// </summary>
		PropertyNames,

		/// <summary>
		/// 
		/// </summary>
		ToolTips,
	};

	/// <summary>
	/// 
	/// </summary>
	[DebuggerDisplay("Name={Name}")]
	public class LocalizationTargetDescriptor
	{
		/// <summary>
		/// Name of this target
		/// </summary>
		public readonly string Name;

		/// <summary>
		/// When should the localization data associated with a target should be loaded?
		/// </summary>
		public LocalizationTargetDescriptorLoadingPolicy LoadingPolicy;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InName">Name of the target</param>
		/// <param name="InLoadingPolicy">When should the localization data associated with a target should be loaded?</param>
		public LocalizationTargetDescriptor(string InName, LocalizationTargetDescriptorLoadingPolicy InLoadingPolicy)
		{
			Name = InName;
			LoadingPolicy = InLoadingPolicy;
		}

		/// <summary>
		/// Constructs a LocalizationTargetDescriptor from a Json object
		/// </summary>
		/// <param name="InObject"></param>
		/// <returns>The new localization target descriptor</returns>
		public static LocalizationTargetDescriptor FromJsonObject(JsonObject InObject)
		{
			return new LocalizationTargetDescriptor(InObject.GetStringField("Name"), InObject.GetEnumField<LocalizationTargetDescriptorLoadingPolicy>("LoadingPolicy"));
		}

		/// <summary>
		/// Write this target to a JsonWriter
		/// </summary>
		/// <param name="Writer">Writer to output to</param>
		void Write(JsonWriter Writer)
		{
			Writer.WriteObjectStart();
			Writer.WriteValue("Name", Name);
			Writer.WriteValue("LoadingPolicy", LoadingPolicy.ToString());
			Writer.WriteObjectEnd();
		}

		/// <summary>
		/// Write an array of target descriptors
		/// </summary>
		/// <param name="Writer">The Json writer to output to</param>
		/// <param name="Name">Name of the array</param>
		/// <param name="Targets">Array of targets</param>
		public static void WriteArray(JsonWriter Writer, string Name, LocalizationTargetDescriptor[] Targets)
		{
			if (Targets != null && Targets.Length > 0)
			{
				Writer.WriteArrayStart(Name);
				foreach (LocalizationTargetDescriptor Target in Targets)
				{
					Target.Write(Writer);
				}
				Writer.WriteArrayEnd();
			}
		}
	}
}
