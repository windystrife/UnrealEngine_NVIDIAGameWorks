// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	enum BuildStepType
	{
		Compile,
		Cook,
		Other,
	}

	[DebuggerDisplay("{Description}")]
	class BuildStep
	{
		public const string UniqueIdKey = "UniqueId";

		public Guid UniqueId;
		public int OrderIndex;
		public string Description;
		public string StatusText;
		public int EstimatedDuration;
		public BuildStepType Type;
		public string Target;
		public string Platform;
		public string Configuration;
		public string FileName;
		public string Arguments;
		public bool bUseLogWindow;
		public bool bNormalSync;
		public bool bScheduledSync;
		public bool bShowAsTool;

		public BuildStep(Guid InUniqueId, int InOrderIndex, string InDescription, string InStatusText, int InEstimatedDuration, string InTarget, string InPlatform, string InConfiguration, string InArguments, bool bInSyncDefault)
		{
			UniqueId = InUniqueId;
			OrderIndex = InOrderIndex;
			Description = InDescription;
			StatusText = InStatusText;
			EstimatedDuration = InEstimatedDuration;
			Type = BuildStepType.Compile;
			Target = InTarget;
			Platform = InPlatform;
			Configuration = InConfiguration;
			Arguments = InArguments;
			bUseLogWindow = true;
			bNormalSync = bInSyncDefault;
			bScheduledSync = bInSyncDefault;
		}

		public BuildStep(ConfigObject Object)
		{
			if(!Guid.TryParse(Object.GetValue(UniqueIdKey, ""), out UniqueId))
			{
				UniqueId = Guid.NewGuid();
			}
			if(!Int32.TryParse(Object.GetValue("OrderIndex", ""), out OrderIndex))
			{
				OrderIndex = -1;
			}

			Description = Object.GetValue("Description", "Untitled");
			StatusText = Object.GetValue("StatusText", "Untitled");

			if(!int.TryParse(Object.GetValue("EstimatedDuration", ""), out EstimatedDuration) || EstimatedDuration < 1)
			{
				EstimatedDuration = 1;
			}

			if(!Enum.TryParse(Object.GetValue("Type", ""), true, out Type))
			{
				Type = BuildStepType.Other;
			}

			Target = Object.GetValue("Target");
			Platform = Object.GetValue("Platform");
			Configuration = Object.GetValue("Configuration");
			FileName = Object.GetValue("FileName");
			Arguments = Object.GetValue("Arguments");

			if(!Boolean.TryParse(Object.GetValue("bUseLogWindow", ""), out bUseLogWindow))
			{
				bUseLogWindow = true;
			}
			if(!Boolean.TryParse(Object.GetValue("bNormalSync", ""), out bNormalSync))
			{
				bNormalSync = true;
			}
			if(!Boolean.TryParse(Object.GetValue("bScheduledSync", ""), out bScheduledSync))
			{
				bScheduledSync = bNormalSync;
			}
			if(!Boolean.TryParse(Object.GetValue("bShowAsTool", ""), out bShowAsTool))
			{
				bShowAsTool = false;
			}
		}

		public static void MergeBuildStepObjects(Dictionary<Guid, ConfigObject> BuildStepObjects, IEnumerable<ConfigObject> ModifyObjects)
		{
			foreach(ConfigObject ModifyObject in ModifyObjects)
			{
				Guid UniqueId;
				if(Guid.TryParse(ModifyObject.GetValue(UniqueIdKey, ""), out UniqueId))
				{
					ConfigObject DefaultObject;
					if(BuildStepObjects.TryGetValue(UniqueId, out DefaultObject))
					{
						ModifyObject.SetDefaults(DefaultObject);
					}
					BuildStepObjects[UniqueId] = ModifyObject;
				}
			}
		}

		public ConfigObject ToConfigObject()
		{
			ConfigObject Result = new ConfigObject();
			Result["UniqueId"] = UniqueId.ToString();
			Result["Description"] = Description;
			Result["StatusText"] = StatusText;
			Result["EstimatedDuration"] = EstimatedDuration.ToString();
			Result["Type"] = Type.ToString();
			switch(Type)
			{
				case BuildStepType.Compile:
					Result["Target"] = Target;
					Result["Platform"] = Platform;
					Result["Configuration"] = Configuration;
					Result["Arguments"] = Arguments;
					break;
				case BuildStepType.Cook:
					Result["FileName"] = FileName;
					break;
				case BuildStepType.Other:
					Result["FileName"] = FileName;
					Result["Arguments"] = Arguments;
					Result["bUseLogWindow"] = bUseLogWindow.ToString();
					break;
			}
			Result["OrderIndex"] = OrderIndex.ToString();
			Result["bNormalSync"] = bNormalSync.ToString();
			Result["bScheduledSync"] = bScheduledSync.ToString();
			Result["bShowAsTool"] = bShowAsTool.ToString();
			return Result;
		}

		public ConfigObject ToConfigObject(ConfigObject DefaultObject)
		{
			ConfigObject Result = new ConfigObject();
			Result[UniqueIdKey] = UniqueId.ToString();
			Result.AddOverrides(ToConfigObject(), DefaultObject);
			return (Result.Pairs.Count <= 1)? null : Result;
		}
	}
}
