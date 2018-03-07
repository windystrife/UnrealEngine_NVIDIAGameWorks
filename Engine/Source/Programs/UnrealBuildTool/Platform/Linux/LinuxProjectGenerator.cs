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
	class LinuxProjectGenerator : UEPlatformProjectGenerator
	{
		/// <summary>
		/// Register the platform with the UEPlatformProjectGenerator class
		/// </summary>
		public override void RegisterPlatformProjectGenerator()
		{
			// Register this project generator for both Linux
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Linux.ToString());
			UEPlatformProjectGenerator.RegisterPlatformProjectGenerator(UnrealTargetPlatform.Linux, this);
		}

		///
		///	VisualStudio project generation functions
		///	
		/// <summary>
		/// Whether this build platform has native support for VisualStudio
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <param name="ProjectFileFormat"></param>
		/// <returns>bool    true if native VisualStudio support (or custom VSI) is available</returns>
		public override bool HasVisualStudioSupport(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, VCProjectFileFormat ProjectFileFormat)
		{
			return false;
		}
	}
}
