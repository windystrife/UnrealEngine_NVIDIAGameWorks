// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;

namespace UnrealVS
{
    // GUID for this class.
    [Guid(GuidList.UnrealVSPackageManString)]

    class ProfileManager : Component, IProfileManager
    {
        public void SaveSettingsToXml(IVsSettingsWriter writer)
        {
            throw new NotImplementedException();
        }

        public void LoadSettingsFromXml(IVsSettingsReader reader)
        {
            throw new NotImplementedException();
        }

        public void SaveSettingsToStorage()
        {
            throw new NotImplementedException();
        }

        public void LoadSettingsFromStorage()
        {
            throw new NotImplementedException();
        }

        public void ResetSettings()
        {
            throw new NotImplementedException();
        }
    }
}
