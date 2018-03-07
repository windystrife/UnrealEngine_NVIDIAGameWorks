// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using AutomationTool;
using UnrealBuildTool;
using Tools.DotNETCommon;

namespace AutomationScripts.Automation
{
	public class ListMobileDevices : BuildCommand
	{
		public override void ExecuteBuild()
		{
			Log("======= ListMobileDevices - Start =======");

			var GlobalParams = new ProjectParams(
				Command: this,
				RawProjectPath: new FileReference(@"D:\UE-Main\UE4\Samples\Games\TappyChicken\TappyChicken.uproject")
				);

			if (ParseParam("android"))
			{
				GetConnectedDevices(GlobalParams, Platform.GetPlatform(UnrealTargetPlatform.Android));
			}

			if (ParseParam("ios"))
			{
				throw new AutomationException("iOS is not yet implemented.");
			}

			Log("======= ListMobileDevices - Done ========");
		}

		private static void GetConnectedDevices(ProjectParams Params, Platform TargetPlatform)
		{
			var PlatformName = TargetPlatform.PlatformType.ToString();
			List<string> ConnectedDevices;
			TargetPlatform.GetConnectedDevices(Params, out ConnectedDevices);

			try
			{
				foreach (var DeviceName in ConnectedDevices)
				{
					Log("Device:{0}:{1}", PlatformName, DeviceName);
				}
			}
			catch
			{
				throw new AutomationException("No {0} devices", PlatformName);
			}		
		}
	}
}

