// Copyright 2017 Google Inc.

#include "GoogleVRHMDViewerPreviews.h"
#include "GoogleVRHMD.h"

#if !GOOGLEVRHMD_SUPPORTED_PLATFORMS

namespace GoogleCardboardViewerPreviews
{

namespace GoogleCardboard1
{
#pragma region GoogleCardboard1

	const float InterpupillaryDistance = 0.060000;

	const FMatrix LeftStereoProjectionMatrix = FMatrix(
		FPlane(1.213739f,  0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 1.191754f,  0.0f, 0.0f),
		FPlane(-0.018448f,  0.000000f,  0.0f, 1.0f),
		FPlane(0.0f, 0.0f, 10.000000f,  0.0f)
	);
	const FMatrix RightStereoProjectionMatrix = FMatrix(
		FPlane(1.213739f,  0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 1.191754f,  0.0f, 0.0f),
		FPlane(0.018448f,  0.000000f,  0.0f, 1.0f),
		FPlane(0.0f, 0.0f, 10.000000f,  0.0f)
	);

	const unsigned int NumLeftVertices = 4;
	const FDistortionVertex LeftVerticesRaw[NumLeftVertices] = {
		{ FVector2D(-0.9f, -0.9f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(-0.1f, -0.9f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(-0.1f, 0.9f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 1.0f, 0.0f },
		{ FVector2D(-0.9f, 0.9f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), 1.0f, 0.0f } };
	const FDistortionVertex* LeftVertices = LeftVerticesRaw;

	const unsigned int NumRightVertices = 4;
	const FDistortionVertex RightVerticesRaw[NumRightVertices] = {
		{ FVector2D(0.1f, -0.9f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(0.9f, -0.9f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(0.9f, 0.9f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), 1.0f, 0.0f },
		{ FVector2D(0.1f, 0.9f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 1.0f, 0.0f } };
	const FDistortionVertex* RightVertices = RightVerticesRaw;

#pragma endregion
}

namespace GoogleCardboard2
{
#pragma region GoogleCardboard2

	const float InterpupillaryDistance = 0.062000f;

	const FMatrix LeftStereoProjectionMatrix = FMatrix(
		FPlane(1.001753f,  0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 0.844889f,  0.0f, 0.0f),
		FPlane(-0.114962f,  -0.006899f,  0.0f, 1.0f),
		FPlane(0.0f, 0.0f, 10.000000f,  0.0f)
	);
	const FMatrix RightStereoProjectionMatrix = FMatrix(
		FPlane(1.001753f,  0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 0.844889f,  0.0f, 0.0f),
		FPlane(0.114962f,  -0.006899f,  0.0f, 1.0f),
		FPlane(0.0f, 0.0f, 10.000000f,  0.0f)
	);

	const unsigned int NumLeftVertices = 4;
	const FDistortionVertex LeftVerticesRaw[NumLeftVertices] = {
		{ FVector2D(-0.9f, -0.9f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(-0.1f, -0.9f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(-0.1f, 0.9f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 1.0f, 0.0f },
		{ FVector2D(-0.9f, 0.9f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), 1.0f, 0.0f } };
	const FDistortionVertex* LeftVertices = LeftVerticesRaw;

	const unsigned int NumRightVertices = 4;
	const FDistortionVertex RightVerticesRaw[NumRightVertices] = {
		{ FVector2D(0.1f, -0.9f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(0.9f, -0.9f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(0.9f, 0.9f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), 1.0f, 0.0f },
		{ FVector2D(0.1f, 0.9f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 1.0f, 0.0f } };
	const FDistortionVertex* RightVertices = RightVerticesRaw;

#pragma endregion
}

namespace ViewMaster
{
#pragma region ViewMaster

	const float InterpupillaryDistance = 0.058000f;

	const FMatrix LeftStereoProjectionMatrix = FMatrix(
		FPlane(1.182311f,   0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 0.969238f,   0.0f, 0.0f),
		FPlane(-0.006302f,   0.000000f,   0.0f, 1.0f),
		FPlane(0.0f, 0.0f, 10.000000f,   0.0f)
	);
	const FMatrix RightStereoProjectionMatrix = FMatrix(
		FPlane(1.182311f,   0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 0.969238f,   0.0f, 0.0f),
		FPlane(0.006302f,   0.000000f,   0.0f, 1.0f),
		FPlane(0.0f, 0.0f, 10.000000f,   0.0f)
	);

	const unsigned int NumLeftVertices = 4;
	const FDistortionVertex LeftVerticesRaw[NumLeftVertices] = {
		{ FVector2D(-0.9f, -0.9f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(-0.1f, -0.9f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(-0.1f, 0.9f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 1.0f, 0.0f },
		{ FVector2D(-0.9f, 0.9f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), 1.0f, 0.0f } };
	const FDistortionVertex* LeftVertices = LeftVerticesRaw;

	const unsigned int NumRightVertices = 4;
	const FDistortionVertex RightVerticesRaw[NumRightVertices] = {
		{ FVector2D(0.1f, -0.9f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(0.9f, -0.9f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(0.9f, 0.9f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), 1.0f, 0.0f },
		{ FVector2D(0.1f, 0.9f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 1.0f, 0.0f } };
	const FDistortionVertex* RightVertices = RightVerticesRaw;

#pragma endregion
}

namespace SnailVR
{
#pragma region SnailVR

	const float InterpupillaryDistance = 0.044000f;

	const FMatrix LeftStereoProjectionMatrix = FMatrix(
		FPlane(1.363999f,  0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 1.212874f,  0.0f, 0.0f),
		FPlane(0.248558f,  0.053343f,  0.0f, 1.0f),
		FPlane(0.0f, 0.0f, 10.000000f,  0.0f)
	);
	const FMatrix RightStereoProjectionMatrix = FMatrix(
		FPlane(1.363999f,  0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 1.212874f,  0.0f, 0.0f),
		FPlane(-0.248558f,  0.053343f,  0.0f, 1.0f),
		FPlane(0.0f, 0.0f, 10.000000f,  0.0f)
	);

	const unsigned int NumLeftVertices = 4;
	const FDistortionVertex LeftVerticesRaw[NumLeftVertices] = {
		{ FVector2D(-0.9f, -0.9f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(-0.1f, -0.9f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(-0.1f, 0.9f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 1.0f, 0.0f },
		{ FVector2D(-0.9f, 0.9f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), 1.0f, 0.0f } };
	const FDistortionVertex* LeftVertices = LeftVerticesRaw;

	const unsigned int NumRightVertices = 4;
	const FDistortionVertex RightVerticesRaw[NumRightVertices] = {
		{ FVector2D(0.1f, -0.9f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(0.9f, -0.9f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(0.9f, 0.9f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), 1.0f, 0.0f },
		{ FVector2D(0.1f, 0.9f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 1.0f, 0.0f } };
	const FDistortionVertex* RightVertices = RightVerticesRaw;

#pragma endregion
}

namespace RiTech2
{
#pragma region RiTech2

	const float InterpupillaryDistance = 0.062000f;

	const FMatrix LeftStereoProjectionMatrix = FMatrix(
		FPlane(1.661536f,  0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 1.484056f,  0.0f, 0.0f),
		FPlane(-0.073075f,  0.000000f,  0.0f, 1.0f),
		FPlane(0.0f, 0.0f, 10.000000f,  0.0f)
	);
	const FMatrix RightStereoProjectionMatrix = FMatrix(
		FPlane(1.661536f,  0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 1.484056f,  0.0f, 0.0f),
		FPlane(0.073075f,  0.000000f,  0.0f, 1.0f),
		FPlane(0.0f, 0.0f, 10.000000f,  0.0f)
	);

	const unsigned int NumLeftVertices = 4;
	const FDistortionVertex LeftVerticesRaw[NumLeftVertices] = {
		{ FVector2D(-0.9f, -0.9f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(-0.1f, -0.9f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(-0.1f, 0.9f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 1.0f, 0.0f },
		{ FVector2D(-0.9f, 0.9f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), 1.0f, 0.0f } };
	const FDistortionVertex* LeftVertices = LeftVerticesRaw;

	const unsigned int NumRightVertices = 4;
	const FDistortionVertex RightVerticesRaw[NumRightVertices] = {
		{ FVector2D(0.1f, -0.9f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(0.9f, -0.9f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(0.9f, 0.9f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), 1.0f, 0.0f },
		{ FVector2D(0.1f, 0.9f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 1.0f, 0.0f } };
	const FDistortionVertex* RightVertices = RightVerticesRaw;

#pragma endregion
}

namespace Go4DC1Glass
{
#pragma region Go4DC1Glass

	const float InterpupillaryDistance = 0.060000f;

	const FMatrix LeftStereoProjectionMatrix = FMatrix(
		FPlane(1.922603f,  0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 1.694811f,  0.0f, 0.0f),
		FPlane(-0.042374f,  -0.012087f,  0.0f, 1.0f),
		FPlane(0.0f, 0.0f, 10.000000f,  0.0f)
	);
	const FMatrix RightStereoProjectionMatrix = FMatrix(
		FPlane(1.922603f,  0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 1.694811f,  0.0f, 0.0f),
		FPlane(0.042374f,  -0.012087f,  0.0f, 1.0f),
		FPlane(0.0f, 0.0f, 10.000000f,  0.0f)
	);

	const unsigned int NumLeftVertices = 4;
	const FDistortionVertex LeftVerticesRaw[NumLeftVertices] = {
		{ FVector2D(-0.9f, -0.9f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(-0.1f, -0.9f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(-0.1f, 0.9f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 1.0f, 0.0f },
		{ FVector2D(-0.9f, 0.9f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), 1.0f, 0.0f } };
	const FDistortionVertex* LeftVertices = LeftVerticesRaw;

	const unsigned int NumRightVertices = 4;
	const FDistortionVertex RightVerticesRaw[NumRightVertices] = {
		{ FVector2D(0.1f, -0.9f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(0.9f, -0.9f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(0.9f, 0.9f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), 1.0f, 0.0f },
		{ FVector2D(0.1f, 0.9f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 1.0f, 0.0f } };
	const FDistortionVertex* RightVertices = RightVerticesRaw;

#pragma endregion
}

}

#endif
