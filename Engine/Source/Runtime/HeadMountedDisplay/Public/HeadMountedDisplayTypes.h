// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RHI.h"
#include "RHIResources.h"
#include "HeadMountedDisplayTypes.generated.h"

struct FFilterVertex;

/**
* The family of HMD device.  Register a new class of device here if you need to branch code for PostProcessing until
*/
namespace EHMDDeviceType
{
	enum Type
	{
		DT_OculusRift,
		DT_Morpheus,
		DT_ES2GenericStereoMesh,
		DT_SteamVR,
		DT_GearVR,
		DT_GoogleVR
	};
}

class HEADMOUNTEDDISPLAY_API FHMDViewMesh
{
public:

	enum EHMDMeshType
	{
		MT_HiddenArea,
		MT_VisibleArea
	};

	FHMDViewMesh();
	~FHMDViewMesh();

	bool IsValid() const
	{
		return NumTriangles > 0;
	}

	void BuildMesh(const FVector2D Positions[], uint32 VertexCount, EHMDMeshType MeshType);

	FFilterVertex* pVertices;
	uint16*   pIndices;
	unsigned  NumVertices;
	unsigned  NumIndices;
	unsigned  NumTriangles;
};

HEADMOUNTEDDISPLAY_API DECLARE_LOG_CATEGORY_EXTERN(LogHMD, Log, All);
HEADMOUNTEDDISPLAY_API DECLARE_LOG_CATEGORY_EXTERN(LogLoadingSplash, Log, All);

UENUM()
namespace EOrientPositionSelector
{
	enum Type
	{
		Orientation UMETA(DisplayName = "Orientation"),
		Position UMETA(DisplayName = "Position"),
		OrientationAndPosition UMETA(DisplayName = "Orientation and Position")
	};
}

/**
* For HMDs that support it, this specifies whether the origin of the tracking universe will be at the floor, or at the user's eye height
*/
UENUM()
namespace EHMDTrackingOrigin
{
	enum Type
	{
		Floor UMETA(DisplayName = "Floor Level"),
		Eye UMETA(DisplayName = "Eye Level"),
	};
}

/**
* Stores if the user is wearing the HMD or not. For HMDs without a sensor to detect the user wearing it, the state defaults to Unknown.
*/
UENUM()
namespace EHMDWornState
{
	enum Type
	{
		Unknown UMETA(DisplayName = "Unknown"),
		Worn UMETA(DisplayName = "Worn"),
		NotWorn UMETA(DisplayName = "Not Worn"),
	};
}
/**
* The Spectator Screen Mode controls what the non-vr video device displays on platforms that support one.
* Not all modes are universal.
* Modes SingleEyeCroppedToFill, Texture, and MirrorPlusTexture are supported on all.
* Disabled is supported on all except PSVR.
*/
UENUM()
enum class ESpectatorScreenMode : uint8
{
	Disabled				UMETA(DisplayName = "Disabled"),
	SingleEyeLetterboxed	UMETA(DisplayName = "SingleEyeLetterboxed"),
	Undistorted				UMETA(DisplayName = "Undistorted"),
	Distorted				UMETA(DisplayName = "Distorted"),
	SingleEye				UMETA(DisplayName = "SingleEye"),
	SingleEyeCroppedToFill	UMETA(DisplayName = "SingleEyeCroppedToFill"),
	Texture					UMETA(DisplayName = "Texture"),
	TexturePlusEye			UMETA(DisplayName = "TexturePlusEye"),
};
const uint8 ESpectatorScreenModeFirst = (uint8)ESpectatorScreenMode::Disabled;
const uint8 ESpectatorScreenModeLast = (uint8)ESpectatorScreenMode::TexturePlusEye;

struct FSpectatorScreenModeTexturePlusEyeLayout
{
	FSpectatorScreenModeTexturePlusEyeLayout()
		: EyeRectMin(0.0f, 0.0f)
		, EyeRectMax(1.0f, 1.0f)
		, TextureRectMin(0.125f, 0.125f)
		, TextureRectMax(0.25f, 0.25f)
		, bDrawEyeFirst(true)
		, bClearBlack(false)
	{}

	FSpectatorScreenModeTexturePlusEyeLayout(FVector2D InEyeRectMin, FVector2D InEyeRectMax, FVector2D InTextureRectMin, FVector2D InTextureRectMax, bool InbDrawEyeFirst, bool InbClearBlack)
		: EyeRectMin(InEyeRectMin)
		, EyeRectMax(InEyeRectMax)
		, TextureRectMin(InTextureRectMin)
		, TextureRectMax(InTextureRectMax)
		, bDrawEyeFirst(InbDrawEyeFirst)
		, bClearBlack(InbClearBlack)
	{}

	bool IsValid() const 
	{
		bool bValid = true;
		if ((EyeRectMax.X <= EyeRectMin.X) || (EyeRectMax.Y <= EyeRectMin.Y))
		{
			UE_LOG(LogHMD, Warning, TEXT("SpectatorScreenModeTexturePlusEyeLayout EyeRect is invalid!  Max is not greater than Min in some dimension."));
			bValid = false;
		}
		if ((TextureRectMax.X <= TextureRectMin.X) || (TextureRectMax.Y <= TextureRectMin.Y))
		{
			UE_LOG(LogHMD, Warning, TEXT("SpectatorScreenModeTexturePlusEyeLayout TextureRect is invalid!  Max is not greater than Min in some dimension."));
			bValid = false;
		}
		if (EyeRectMin.X > 1.0f || EyeRectMin.X < 0.0f ||
			EyeRectMin.Y > 1.0f || EyeRectMin.Y < 0.0f ||
			EyeRectMax.X > 1.0f || EyeRectMax.X < 0.0f ||
			EyeRectMax.Y > 1.0f || EyeRectMax.Y < 0.0f
			)
		{
			UE_LOG(LogHMD, Warning, TEXT("SpectatorScreenModeTexturePlusEyeLayout EyeRect is invalid!  All dimensions must be in 0-1 range."));
			bValid = false;
		}

		if (TextureRectMin.X > 1.0f || TextureRectMin.X < 0.0f ||
			TextureRectMin.Y > 1.0f || TextureRectMin.Y < 0.0f ||
			TextureRectMax.X > 1.0f || TextureRectMax.X < 0.0f ||
			TextureRectMax.Y > 1.0f || TextureRectMax.Y < 0.0f
			 )
		{
			UE_LOG(LogHMD, Warning, TEXT("SpectatorScreenModeTexturePlusEyeLayout TextureRect is invalid!  All dimensions must be in 0-1 range."));
			bValid = false;
		}
		return bValid;
	}

	FIntRect GetScaledEyeRect(int SizeX, int SizeY) const
	{
		return FIntRect(EyeRectMin.X * SizeX, EyeRectMin.Y * SizeY, EyeRectMax.X * SizeX, EyeRectMax.Y * SizeY);
	}

	FIntRect GetScaledTextureRect(int SizeX, int SizeY) const
	{
		return FIntRect(TextureRectMin.X * SizeX, TextureRectMin.Y * SizeY, TextureRectMax.X * SizeX, TextureRectMax.Y * SizeY);
	}

	FVector2D EyeRectMin;
	FVector2D EyeRectMax;
	FVector2D TextureRectMin;
	FVector2D TextureRectMax;
	bool bDrawEyeFirst;
	bool bClearBlack;
};

DECLARE_DELEGATE_FiveParams(FSpectatorScreenRenderDelegate, FRHICommandListImmediate& /* RHICmdList */, FTexture2DRHIRef /* TargetTexture */, FTexture2DRHIRef /* EyeTexture */, FTexture2DRHIRef /* OtherTexture */, FVector2D /* WindowSize */);
