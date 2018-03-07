//
// Copyright 2014, 2015 Razer Inc.
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

#include "CoreMinimal.h"
#include "OSVRPrivate.h"
#include "Tickable.h"
#include "Engine/World.h"

#if OSVR_DEPRECATED_BLUEPRINT_API_ENABLED
#include "OSVRInterfaceCollection.h"
#endif

DECLARE_LOG_CATEGORY_EXTERN(OSVREntryPointLog, Log, All);

OSVR_API class OSVREntryPoint : public FTickableGameObject
{
public:

	OSVREntryPoint();
	virtual ~OSVREntryPoint();

	virtual void Tick(float DeltaTime) override;

	virtual bool IsTickable() const override
	{
		return (!GWorld->HasBegunPlay() && GIsEditor) ? false : true;
	}

	virtual bool IsTickableWhenPaused() const override
	{
		return true;
	}

	virtual bool IsTickableInEditor() const override
	{
		return false;
	}

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(OSVREntryPoint, STATGROUP_Tickables);
	}

    virtual OSVR_ClientContext GetClientContext()
    {
        return osvrClientContext;
    }

    virtual FCriticalSection* GetClientContextMutex()
    {
        return &mContextMutex;
    }

    virtual bool IsOSVRConnected();
#if OSVR_DEPRECATED_BLUEPRINT_API_ENABLED
	OSVRInterfaceCollection* GetInterfaceCollection();
#endif

private:
    OSVR_ClientContext osvrClientContext = nullptr;
    FCriticalSection mContextMutex;

#if OSVR_DEPRECATED_BLUEPRINT_API_ENABLED
	TSharedPtr< OSVRInterfaceCollection > InterfaceCollection;
#endif
};
