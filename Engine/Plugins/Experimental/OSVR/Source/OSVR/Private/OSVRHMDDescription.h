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

#include "OSVRPrivate.h"
#include "IHeadMountedDisplay.h"

#include <osvr/ClientKit/DisplayC.h>

DECLARE_LOG_CATEGORY_EXTERN(OSVRHMDDescriptionLog, Log, All);

struct DescriptionData
{
    FVector2D DisplaySize[2];
    FVector2D Fov[2];

    DescriptionData();
};

class OSVRHMDDescription
{
public:
	OSVRHMDDescription();
	~OSVRHMDDescription();

	bool Init(OSVR_ClientContext OSVRClientContext, OSVR_DisplayConfig displayConfig);
	bool IsValid() const
	{
		return bValid;
	}

	enum EEye
	{
		LEFT_EYE = 0,
		RIGHT_EYE
	};

	FVector2D GetDisplaySize(EEye Eye) const;
	FVector2D GetDisplayOrigin(EEye Eye) const;
	FVector2D GetFov(EEye Eye) const;
    FVector2D GetFov(OSVR_EyeCount Eye) const;
	FVector GetLocation(EEye Eye) const;
    FMatrix GetProjectionMatrix(float left, float right, float bottom, float top, float nearClip, float farClip) const;
	FMatrix GetProjectionMatrix(EEye Eye, OSVR_DisplayConfig displayConfig, float nearClip, float farClip) const;
    bool OSVRViewerFitsUnrealModel(OSVR_DisplayConfig displayConfig);

	// Helper function
	// IPD    = ABS(GetLocation(LEFT_EYE).X - GetLocation(RIGHT_EYE).X);
	float GetInterpupillaryDistance() const;

	// Helper function
	// Width  = GetDisplaySize(LEFT_EYE).X + GetDisplaySize(RIGHT_EYE).X;
	// Height = MAX(GetDisplaySize(LEFT_EYE).Y, GetDisplaySize(RIGHT_EYE).Y);
	FVector2D GetResolution() const;

private:
	OSVRHMDDescription(OSVRHMDDescription&);
	OSVRHMDDescription& operator=(OSVRHMDDescription&);

    bool InitIPD(OSVR_DisplayConfig displayConfig);
    bool InitDisplaySize(OSVR_DisplayConfig displayConfig);
    bool InitFOV(OSVR_DisplayConfig displayConfig);

    float m_ipd;
	bool bValid;
    DescriptionData* Data;
};
