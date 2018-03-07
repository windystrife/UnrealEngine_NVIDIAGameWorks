// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace UnrealBuildTool
{
	class TVOSProjectGenerator : IOSProjectGenerator
    {
        /**
         *	Register the platform with the UEPlatformProjectGenerator class
         */
        public override void RegisterPlatformProjectGenerator()
        {
            // Register this project generator for TVOS
            Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.TVOS.ToString());
            UEPlatformProjectGenerator.RegisterPlatformProjectGenerator(UnrealTargetPlatform.TVOS, this);
        }
    }
}
