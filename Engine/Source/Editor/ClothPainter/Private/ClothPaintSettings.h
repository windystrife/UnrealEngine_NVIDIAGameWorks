// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshPaintSettings.h"
#include "Delegates/Delegate.h"

#include "ClothPaintSettings.generated.h"

class UClothingAsset;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnClothingAssetSelectionChangedMulticaster, UClothingAsset*, int32, int32);
typedef FOnClothingAssetSelectionChangedMulticaster::FDelegate FOnClothingAssetSelectionChanged;

UENUM()
enum class EPaintableClothProperty
{
	/** Max distances cloth property */
	MaxDistances,
	/** Backstop cloth property */
	BackstopDistances,
	/** Backstop radius property */
	BackstopRadius
};

UCLASS()
class UClothPainterSettings : public UMeshPaintSettings
{
	GENERATED_BODY()

public:

	UClothPainterSettings()
		: UMeshPaintSettings()
		, ViewMin(0.0f)
		, ViewMax(100.0f)
		, bFlipNormal(false)
		, bCullBackface(false)
		, Opacity(0.8f)
	{}

	float GetViewMin()
	{
		// Zero is reserved, but conceptually we should allow it as that's an
		// implementation detail the user is unlikely to care about
		return FMath::Clamp(ViewMin, SMALL_NUMBER, MAX_flt);
	}

	float GetViewMax()
	{
		return ViewMax;
	}

	// Delegates to communicate with objects concerned with the settings changing
	FOnClothingAssetSelectionChangedMulticaster OnAssetSelectionChanged;

protected:
	/** When painting float/1D values, this is considered the zero or black point */
	UPROPERTY(EditAnywhere, Category = View, meta = (UIMin = 0, UIMax = 100000, ClampMin = 0, ClampMax = 100000))
	float ViewMin;

	/** When painting float/1D values, this is considered the one or white point */
	UPROPERTY(EditAnywhere, Category = View, meta = (UIMin = 0, UIMax = 100000, ClampMin = 0, ClampMax = 100000))
	float ViewMax;

public:

	/** Array of Clothing assets */
	UPROPERTY()
	TArray<UClothingAsset*> ClothingAssets;

	/** Whether to flip normals on the mesh preview */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = View)
	bool bFlipNormal;

	/** Whether to cull backfacing triangles when rendering the mesh preview */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = View)
	bool bCullBackface;

	/** Opacity of the mesh preview */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = View, meta = (UIMin = 0, UIMax = 1, ClampMin = 0, ClampMax = 1))
	float Opacity;
};
