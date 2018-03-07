//
// Copyright 2016 Sensics Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

#include "ModuleManager.h"
#include "IHeadMountedDisplayModule.h"

#define OSVR_UNREAL_4_11 1
#define OSVR_UNREAL_4_12 1

class OSVREntryPoint;
class FOSVRHMD;

/**
* The public interface to this module.  In most cases, this interface is only public to sibling modules
* within this plugin.
*/
OSVR_API class IOSVR : public IHeadMountedDisplayModule
{
public:
    /** Returns the key into the HMDPluginPriority section of the config file for this module */
    virtual FString GetModulePriorityKeyName() const
    {
        return FString(TEXT("OSVR"));
    }

    /**
    * Singleton-like access to this module's interface.  This is just for convenience!
    * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
    *
    * @return Returns singleton instance, loading the module on demand if needed
    */
    static inline IOSVR& Get()
    {
        return FModuleManager::LoadModuleChecked< IOSVR >("OSVR");
    }

    /**
    * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
    *
    * @return True if the module is loaded and ready to use
    */
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("OSVR");
    }

    virtual void LoadOSVRClientKitModule() = 0;
    virtual TSharedPtr<class OSVREntryPoint, ESPMode::ThreadSafe> GetEntryPoint() = 0;
    virtual TSharedPtr<FOSVRHMD, ESPMode::ThreadSafe> GetHMD() = 0;
};

DECLARE_LOG_CATEGORY_EXTERN(OSVRLog, Log, All);
