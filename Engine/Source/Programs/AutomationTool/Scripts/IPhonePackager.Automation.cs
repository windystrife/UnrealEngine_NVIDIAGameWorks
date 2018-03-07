// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;

[Help("UAT command to call into the integrated IPhonePackager code")]
class IPhonePackager : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("************************* Calling IPP");

		Platform IOS = Platform.GetPlatform(UnrealTargetPlatform.IOS);
		string Command = ParseParamValue("cmd", "");

		// check the return value
		int ReturnValue = IOS.RunCommand("IPP:" + Command);
		if (ReturnValue != 0)
		{
			throw new AutomationException("Internal IPP returned {0}", ReturnValue);
		}
	}
}
