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

#include "OSVRHMDDescription.h"
#include <osvr/RenderKit/RenderManagerC.h>

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY(OSVRHMDDescriptionLog);


DescriptionData::DescriptionData()
{
	// Set defaults...
	for (int i = 0; i < 2; ++i)
	{
		DisplaySize[i].Set(960, 1080);
		Fov[i].Set(90, 101.25f);
	}
}

OSVRHMDDescription::OSVRHMDDescription()
	: bValid(false),
	  Data(new DescriptionData())
{
}

OSVRHMDDescription::~OSVRHMDDescription()
{
	delete Data;
}

bool OSVRHMDDescription::OSVRViewerFitsUnrealModel(OSVR_DisplayConfig displayConfig)
{
    // if the display config hasn't started up, we can't tell yet
    if (osvrClientCheckDisplayStartup(displayConfig) == OSVR_RETURN_FAILURE)
    {
        UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("osvrClientCheckDisplayStartup call failed. Perhaps the HMD isn't connected?"));
        return false;
    }

    OSVR_ReturnCode returnCode;
    
    // must be only one display input
    OSVR_DisplayInputCount numDisplayInputs;
    returnCode = osvrClientGetNumDisplayInputs(displayConfig, &numDisplayInputs);
    if (returnCode == OSVR_RETURN_FAILURE || numDisplayInputs != 1)
    {
        UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("osvrClientGetNumDisplayInputs call failed or number of display inputs not equal to 1"));
        return false;
    }

    // must be only one viewer
    OSVR_ViewerCount numViewers;
    returnCode = osvrClientGetNumViewers(displayConfig, &numViewers);
    if (returnCode == OSVR_RETURN_FAILURE || numViewers != 1)
    {
        UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("osvrClientGetNumViewers call failed or number of viewers not equal to 1"));
        return false;
    }

    // the one viewer must have two eyes
    OSVR_EyeCount numEyes;
    returnCode = osvrClientGetNumEyesForViewer(displayConfig, 0, &numEyes);
    if (returnCode == OSVR_RETURN_FAILURE || numEyes != 2)
    {
        UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("osvrClientGetNumEyesForViewer call failed or number of eyes not equal to 2"));
        return false;
    }

    // each eye must have only one surface

    // left eye
    OSVR_ViewerCount numLeftEyeSurfaces, numRightEyeSurfaces;
    returnCode = osvrClientGetNumSurfacesForViewerEye(displayConfig, 0, 0, &numLeftEyeSurfaces);
    if (returnCode == OSVR_RETURN_FAILURE || numLeftEyeSurfaces != 1)
    {
        UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("osvrClientGetNumSurfacesForViewerEye call failed for the left eye, or number of surfaces not equal to 1"));
        return false;
    }

    // right eye
    returnCode = osvrClientGetNumSurfacesForViewerEye(displayConfig, 0, 1, &numRightEyeSurfaces);
    if (returnCode == OSVR_RETURN_FAILURE || numRightEyeSurfaces != 1)
    {
        UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("osvrClientGetNumSurfacesForViewerEye call failed for the right eye, or number of surfaces not equal to 1"));
        return false;
    }

    // I think we're good.
    return true;
}

bool OSVRHMDDescription::InitIPD(OSVR_DisplayConfig displayConfig)
{
    OSVR_Pose3 leftEye, rightEye;
    OSVR_ReturnCode returnCode;

    returnCode = osvrClientGetViewerEyePose(displayConfig, 0, 0, &leftEye);
    if (returnCode == OSVR_RETURN_FAILURE)
    {
        UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("osvrClientGetViewerEyePose call failed for left eye"));
        return false;
    }

    returnCode = osvrClientGetViewerEyePose(displayConfig, 0, 1, &rightEye);
    if (returnCode == OSVR_RETURN_FAILURE)
    {
        UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("osvrClientGetViewerEyePose call failed for right eye"));
    }

    double dx = leftEye.translation.data[0] - rightEye.translation.data[0];
    double dy = leftEye.translation.data[1] - rightEye.translation.data[1];
    double dz = leftEye.translation.data[2] - rightEye.translation.data[2];

    m_ipd = FMath::Sqrt(dx * dx + dy * dy + dz * dz);
    return true;
}

bool OSVRHMDDescription::InitDisplaySize(OSVR_DisplayConfig displayConfig)
{
#if PLATFORM_ANDROID
    // On Android, we just use the resolution Unreal sets for us.
    // This may be a downscaled resolution for performance reasons.
    Data->DisplaySize[0].Set(GSystemResolution.ResX / 2, GSystemResolution.ResY);
    Data->DisplaySize[1].Set(GSystemResolution.ResX - Data->DisplaySize[0].X, GSystemResolution.ResY);
#else
    OSVR_ReturnCode returnCode;

    // left eye surface (only one surface per eye supported)
    OSVR_ViewportDimension leftViewportLeft, leftViewportBottom, leftViewportWidth, leftViewportHeight;
    returnCode = osvrClientGetRelativeViewportForViewerEyeSurface(displayConfig, 0, 0, 0,
        &leftViewportLeft, &leftViewportBottom, &leftViewportWidth, &leftViewportHeight);
    if (returnCode == OSVR_RETURN_FAILURE)
    {
        UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("osvrClientGetRelativeViewportForViewerEyeSurface call failed for left eye surface"));
        return false;
    }

    // right eye surface (only one surface per eye supported)
    OSVR_ViewportDimension rightViewportLeft, rightViewportBottom, rightViewportWidth, rightViewportHeight;
    returnCode = osvrClientGetRelativeViewportForViewerEyeSurface(displayConfig, 0, 1, 0,
        &rightViewportLeft, &rightViewportBottom, &rightViewportWidth, &rightViewportHeight);
    if (returnCode == OSVR_RETURN_FAILURE)
    {
        UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("osvrClientGetRelativeViewportForViewerEyeSurface call failed for right eye surface"));
        return false;
    }

    Data->DisplaySize[0].Set(leftViewportWidth, leftViewportHeight);
    Data->DisplaySize[1].Set(rightViewportWidth, rightViewportHeight);
#endif
    return true;
}

bool OSVRHMDDescription::InitFOV(OSVR_DisplayConfig displayConfig)
{
    OSVR_ReturnCode returnCode;
    for (OSVR_EyeCount eye = 0; eye < 2; eye++)
    {
        double left, right, top, bottom;
        returnCode = osvrClientGetViewerEyeSurfaceProjectionClippingPlanes(displayConfig, 0, 0, eye, &left, &right, &bottom, &top);
        if (returnCode == OSVR_RETURN_FAILURE)
        {
            UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("osvrClientGetViewerEyeSurfaceProjectionClippingPlanes call failed"));
            return false;
        }

        double horizontalFOV = FMath::RadiansToDegrees(FMath::Atan(FMath::Abs(left)) + FMath::Atan(FMath::Abs(right)));
        double verticalFOV = FMath::RadiansToDegrees(FMath::Atan(FMath::Abs(top)) + FMath::Atan(FMath::Abs(bottom)));
        Data->Fov[eye].Set(horizontalFOV, verticalFOV);
    }
    return true;
}

bool OSVRHMDDescription::Init(OSVR_ClientContext OSVRClientContext, OSVR_DisplayConfig displayConfig)
{
	bValid = false;

    // if the OSVR viewer doesn't fit nicely with the Unreal HMD model, don't
    // bother trying to fill everything else out.
    if (!OSVRViewerFitsUnrealModel(displayConfig))
    {
        UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("OSVRHMDDescription::Init() viewer doesn't fit unreal model."));
        return false;
    }

    if (!InitIPD(displayConfig))
    { 
        UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("OSVRHMDDescription::Init() InitIPD failed"));
        return false; 
    }
    if (!InitDisplaySize(displayConfig))
    { 
        UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("OSVRHMDDescription::Init() InitDisplaySize failed."));
        return false; 
    }
    if (!InitFOV(displayConfig))
    { 
        UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("OSVRHMDDescription::Init() InitFOV failed."));
        return false; 
    }
    bValid = true;
	return bValid;
}

FVector2D OSVRHMDDescription::GetDisplaySize(EEye Eye) const
{
    if (Eye == EEye::LEFT_EYE)
    {
        return Data->DisplaySize[0];
    }
    else if (Eye == EEye::RIGHT_EYE)
    {
        return Data->DisplaySize[1];
    }
    UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("OSVRHMDDescription::GetDisplaySize() Invalid EEye."));
	return FVector2D();
}

FVector2D OSVRHMDDescription::GetFov(OSVR_EyeCount Eye) const
{
	return Data->Fov[Eye];
}

FVector2D OSVRHMDDescription::GetFov(EEye Eye) const
{
    if (Eye == EEye::LEFT_EYE)
    {
        return Data->Fov[0];
    }
    else if (Eye == EEye::RIGHT_EYE)
    {
        return Data->Fov[1];
    }
    UE_LOG(OSVRHMDDescriptionLog, Warning, TEXT("OSVRHMDDescription::GetFov() Invalid EEye."));
    return FVector2D();
}

FMatrix OSVRHMDDescription::GetProjectionMatrix(float left, float right, float bottom, float top, float nearClip, float farClip) const
{
    // original code
    //float sumRightLeft = static_cast<float>(right + left);
    //float sumTopBottom = static_cast<float>(top + bottom);
    //float inverseRightLeft = 1.0f / static_cast<float>(right - left);
    //float inverseTopBottom = 1.0f / static_cast<float>(top - bottom);
    //FPlane row1(2.0f * inverseRightLeft, 0.0f, 0.0f, 0.0f);
    //FPlane row2(0.0f, 2.0f * inverseTopBottom, 0.0f, 0.0f);
    //FPlane row3((sumRightLeft * inverseRightLeft), (sumTopBottom * inverseTopBottom), 0.0f, 1.0f);
    //FPlane row4(0.0f, 0.0f, zNear, 0.0f);
    
    // OSVR Render Manager OSVR_Projection_to_D3D with adjustment for unreal (from steamVR plugin)
    OSVR_ProjectionMatrix projection;
    projection.left = static_cast<double>(left);
    projection.right = static_cast<double>(right);
    projection.top = static_cast<double>(top);
    projection.bottom = static_cast<double>(bottom);
    projection.nearClip = static_cast<double>(nearClip);
    // @todo Since farClip may be FLT_MAX, round-trip casting to double
    // and back (via the OSVR_Projection_to_Unreal call) it should
    // be checked for conversion issues.
    projection.farClip = static_cast<double>(farClip);
    float p[16];
    OSVR_Projection_to_Unreal(p, projection);

    FPlane row1(p[0], p[1], p[2], p[3]);
    FPlane row2(p[4], p[5], p[6], p[7]);
    FPlane row3(p[8], p[9], p[10], p[11]);
    FPlane row4(p[12], p[13], p[14], p[15]);
    FMatrix ret = FMatrix(row1, row2, row3, row4);

    //ret.M[3][3] = 0.0f;
    //ret.M[2][3] = 1.0f;
    //ret.M[2][2] = 0.0f;
    //ret.M[3][2] = nearClip;

    // This was suggested by Nick at Epic, but doesn't seem to work? Black screen.
    //ret.M[2][2] = nearClip / (nearClip - farClip);
    //ret.M[3][2] = -nearClip * nearClip / (nearClip - farClip);

    // Adjustment suggested on a forum post for an off-axis projection. Doesn't work.
    //ret *= 1.0f / ret.M[0][0];
    //ret.M[3][2] = GNearClippingPlane;
    return ret;
}

// implemented to match the steamvr projection calculation but with OSVR calculated clipping planes.
FMatrix OSVRHMDDescription::GetProjectionMatrix(EEye Eye, OSVR_DisplayConfig displayConfig, float nearClip, float farClip) const
{
    OSVR_EyeCount eye = (Eye == LEFT_EYE ? 0 : 1);
    double left, right, bottom, top;
    OSVR_ReturnCode rc;
    rc = osvrClientGetViewerEyeSurfaceProjectionClippingPlanes(displayConfig, 0, eye, 0, &left, &right, &bottom, &top);
    check(rc == OSVR_RETURN_SUCCESS);

    // The steam plugin inverts the clipping planes here, but that doesn't appear to
    // be necessary for the OSVR calculated planes.

    return GetProjectionMatrix(
        static_cast<float>(left),
        static_cast<float>(right),
        static_cast<float>(bottom),
        static_cast<float>(top),
        nearClip, farClip);
}

float OSVRHMDDescription::GetInterpupillaryDistance() const
{
    return m_ipd;
}
