// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for platform-specific project generators
	/// </summary>
	class WindowsProjectGenerator : UEPlatformProjectGenerator
	{
		/// <summary>
		/// Register the platform with the UEPlatformProjectGenerator class
		/// </summary>
		public override void RegisterPlatformProjectGenerator()
		{
			// Register this project generator for Win32 and Win64
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Win32.ToString());
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Win64.ToString());
			UEPlatformProjectGenerator.RegisterPlatformProjectGenerator(UnrealTargetPlatform.Win32, this);
			UEPlatformProjectGenerator.RegisterPlatformProjectGenerator(UnrealTargetPlatform.Win64, this);
		}

		///
		///	VisualStudio project generation functions
		///	
		/// <summary>
		/// Return the VisualStudio platform name for this build platform
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <returns>string    The name of the platform that VisualStudio recognizes</returns>
		public override string GetVisualStudioPlatformName(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			if (InPlatform == UnrealTargetPlatform.Win64)
			{
				return "x64";
			}
			return InPlatform.ToString();
		}

		public override bool RequiresVSUserFileGeneration()
		{
			return true;
		}
	}
}
