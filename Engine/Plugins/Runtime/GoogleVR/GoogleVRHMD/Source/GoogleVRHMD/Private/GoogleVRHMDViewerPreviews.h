// Copyright 2017 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "IHeadMountedDisplay.h"
#include "IGoogleVRHMDPlugin.h"

#if !GOOGLEVRHMD_SUPPORTED_PLATFORMS

struct FDistortionVertex;

namespace GoogleCardboardViewerPreviews
{

// Note: These meshes are generated against 40x40-point grids for each eye.

namespace GoogleCardboard1
{
	extern const float InterpupillaryDistance;

	extern const FMatrix LeftStereoProjectionMatrix;
	extern const FMatrix RightStereoProjectionMatrix;

	extern const unsigned int NumLeftVertices;
	extern const FDistortionVertex* LeftVertices;

	extern const unsigned int NumRightVertices;
	extern const FDistortionVertex* RightVertices;
}

namespace GoogleCardboard2
{
	extern const float InterpupillaryDistance;

	extern const FMatrix LeftStereoProjectionMatrix;
	extern const FMatrix RightStereoProjectionMatrix;

	extern const unsigned int NumLeftVertices;
	extern const FDistortionVertex* LeftVertices;

	extern const unsigned int NumRightVertices;
	extern const FDistortionVertex* RightVertices;
}

namespace ViewMaster
{
	extern const float InterpupillaryDistance;

	extern const FMatrix LeftStereoProjectionMatrix;
	extern const FMatrix RightStereoProjectionMatrix;

	extern const unsigned int NumLeftVertices;
	extern const FDistortionVertex* LeftVertices;

	extern const unsigned int NumRightVertices;
	extern const FDistortionVertex* RightVertices;
}

namespace SnailVR
{
	extern const float InterpupillaryDistance;

	extern const FMatrix LeftStereoProjectionMatrix;
	extern const FMatrix RightStereoProjectionMatrix;

	extern const unsigned int NumLeftVertices;
	extern const FDistortionVertex* LeftVertices;

	extern const unsigned int NumRightVertices;
	extern const FDistortionVertex* RightVertices;
}

namespace RiTech2
{
	extern const float InterpupillaryDistance;

	extern const FMatrix LeftStereoProjectionMatrix;
	extern const FMatrix RightStereoProjectionMatrix;

	extern const unsigned int NumLeftVertices;
	extern const FDistortionVertex* LeftVertices;

	extern const unsigned int NumRightVertices;
	extern const FDistortionVertex* RightVertices;
}

namespace Go4DC1Glass
{
	extern const float InterpupillaryDistance;

	extern const FMatrix LeftStereoProjectionMatrix;
	extern const FMatrix RightStereoProjectionMatrix;

	extern const unsigned int NumLeftVertices;
	extern const FDistortionVertex* LeftVertices;

	extern const unsigned int NumRightVertices;
	extern const FDistortionVertex* RightVertices;
}

}

#endif // !GOOGLEVRHMD_SUPPORTED_PLATFORMS
