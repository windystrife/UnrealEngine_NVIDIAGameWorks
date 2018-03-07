// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateColor.h"
#include "Layout/Margin.h"

#include "SlateBrush.generated.h"

/**
 * Enumerates ways in which an image can be drawn.
 */
UENUM(BlueprintType)
namespace ESlateBrushDrawType
{
	enum Type
	{
		/** Don't do anything */
		NoDrawType UMETA(DisplayName="None"),

		/** Draw a 3x3 box, where the sides and the middle stretch based on the Margin */
		Box,

		/** Draw a 3x3 border where the sides tile and the middle is empty */
		Border,

		/** Draw an image; margin is ignored */
		Image
	};
}


/**
 * Enumerates tiling options for image drawing.
 */
UENUM(BlueprintType)
namespace ESlateBrushTileType
{
	enum Type
	{
		/** Just stretch */
		NoTile,

		/** Tile the image horizontally */
		Horizontal,

		/** Tile the image vertically */
		Vertical,

		/** Tile in both directions */
		Both
	};
}


/**
 * Possible options for mirroring the brush image
 */
UENUM()
namespace ESlateBrushMirrorType
{
	enum Type
	{
		/** Don't mirror anything, just draw the texture as it is. */
		NoMirror,

		/** Mirror the image horizontally. */
		Horizontal,

		/** Mirror the image vertically. */
		Vertical,

		/** Mirror in both directions. */
		Both
	};
}


/**
 * Enumerates brush image types.
 */
UENUM()
namespace ESlateBrushImageType
{
	enum Type
	{
		/** No image is loaded.  Color only brushes, transparent brushes etc. */
		NoImage,

		/** The image to be loaded is in full color. */
		FullColor,

		/** The image is a special texture in linear space (usually a rendering resource such as a lookup table). */
		Linear,
	};
}

namespace SlateBrushDefs
{
	static const float DefaultImageSize = 32.0f;
}

/**
 * An brush which contains information about how to draw a Slate element
 */
USTRUCT(BlueprintType)
struct SLATECORE_API FSlateBrush
{
	GENERATED_USTRUCT_BODY()

	/** Size of the resource in Slate Units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Brush)
	FVector2D ImageSize;

	/** The margin to use in Box and Border modes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Brush, meta=( UVSpace="true" ))
	FMargin Margin;

#if WITH_EDITORONLY_DATA
	/** Tinting applied to the image. */
	UPROPERTY()
	FLinearColor Tint_DEPRECATED;
#endif

	/** Tinting applied to the image. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Brush, meta=( DisplayName="Tint", sRGB="true" ))
	FSlateColor TintColor;

public:

	/**
	 * Default constructor.
	 */
	FSlateBrush()
		: ImageSize(SlateBrushDefs::DefaultImageSize, SlateBrushDefs::DefaultImageSize)
		, Margin(0.0f)
#if WITH_EDITORONLY_DATA
		, Tint_DEPRECATED(FLinearColor::White)
#endif
		, TintColor(FLinearColor::White)
		, ResourceObject(nullptr)
		, ResourceName(NAME_None)
		, UVRegion(ForceInit)
		, DrawAs(ESlateBrushDrawType::Image)
		, Tiling(ESlateBrushTileType::NoTile)
		, Mirroring(ESlateBrushMirrorType::NoMirror)
		, ImageType(ESlateBrushImageType::NoImage)
		, bIsDynamicallyLoaded(false)
		, bHasUObject_DEPRECATED(false)
	{ }

	virtual ~FSlateBrush(){}

public:

	/**
	 * Gets the name of the resource object, if any.
	 *
	 * @return Resource name, or NAME_None if the resource object is not set.
	 */
	const FName GetResourceName() const
	{
		return ( ( ResourceName != NAME_None ) || ( ResourceObject == nullptr ) )
			? ResourceName
			: ResourceObject->GetFName();
	}

	/**
	 * Gets the UObject that represents the brush resource, if any.
	 *
	 * The object may be a UMaterialInterface or a UTexture.
	 *
	 * @return The resource object, or nullptr if it is not set.
	 */
	class UObject* GetResourceObject( ) const
	{
		return ResourceObject;
	}

	/**
	 * Sets the UObject that represents the brush resource.
	 */
	void SetResourceObject(class UObject* InResourceObject)
	{
		ResourceObject = InResourceObject;
	}

	/**
	 * Gets the brush's tint color.
	 *
	 * @param InWidgetStyle The widget style to get the tint for.
	 * @return Tint color.
	 */
	FLinearColor GetTint( const FWidgetStyle& InWidgetStyle ) const
	{
		return TintColor.GetColor(InWidgetStyle);
	}

	/**
	 * Checks whether this brush has a UTexture object
	 *
	 * @return true if it has a UTexture object, false otherwise.
	 */
	bool HasUObject( ) const
	{
		return (ResourceObject != nullptr) || (bHasUObject_DEPRECATED);
	}

	/**
	 * Checks whether the brush resource is loaded dynamically.
	 *
	 * @return true if loaded dynamically, false otherwise.
	 */
	bool IsDynamicallyLoaded( ) const
	{
		return bIsDynamicallyLoaded;
	}

	/**
	 * Get brush UV region, should check if region is valid before using it
	 *
	 * @return UV region
	 */
	FBox2D GetUVRegion() const
	{
		return UVRegion;
	}

	/**
	 * Set brush UV region
	 *
	 * @param InUVRegion When valid - overrides UV region specified in resource proxy
	 */
	void SetUVRegion(const FBox2D& InUVRegion)
	{
		UVRegion = InUVRegion;
	}

	/**
	 * Compares this brush with another for equality.
	 *
	 * @param Other The other brush.
	 *
	 * @return true if the two brushes are equal, false otherwise.
	 */
	bool operator==( const FSlateBrush& Other ) const 
	{
		return ImageSize == Other.ImageSize
			&& DrawAs == Other.DrawAs
			&& Margin == Other.Margin
			&& TintColor == Other.TintColor
			&& Tiling == Other.Tiling
			&& Mirroring == Other.Mirroring
			&& ResourceObject == Other.ResourceObject
			&& ResourceName == Other.ResourceName
			&& bIsDynamicallyLoaded == Other.bIsDynamicallyLoaded
			&& UVRegion == Other.UVRegion;
	}

	/**
	 * Compares this brush with another for inequality.
	 *
	 * @param Other The other brush.
	 *
	 * @return false if the two brushes are equal, true otherwise.
	 */
	bool operator!=( const FSlateBrush& Other ) const 
	{
		return !(*this == Other);
	}

	void PostSerialize(const FArchive& Ar);

	void AddReferencedObjects(FReferenceCollector& Collector);

	/**
	 * Gets the identifier for UObject based texture paths.
	 *
	 * @return Texture identifier string.
	 */
	static const FString UTextureIdentifier( );

protected:

	/**
	 * The image to render for this brush, can be a UTexture or UMaterialInterface or an object implementing 
	 * the AtlasedTextureInterface. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Brush, meta=( DisplayThumbnail="true", DisplayName="Image", AllowedClasses="Texture,MaterialInterface,SlateTextureAtlasInterface" ))
	UObject* ResourceObject;

	/** The name of the rendering resource to use */
	UPROPERTY()
	FName ResourceName;

	/** 
	 *  Optional UV region for an image
	 *  When valid - overrides UV region specified in resource proxy
	 */
	UPROPERTY()
	FBox2D UVRegion;

public:
	/** How to draw the image */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Brush)
	TEnumAsByte<enum ESlateBrushDrawType::Type > DrawAs;

	/** How to tile the image in Image mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Brush)
	TEnumAsByte<enum ESlateBrushTileType::Type> Tiling;

	/** How to mirror the image in Image mode.  This is normally only used for dynamic image brushes where the source texture
	    comes from a hardware device such as a web camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Brush)
	TEnumAsByte<enum ESlateBrushMirrorType::Type> Mirroring;

	/** The type of image */
	UPROPERTY()
	TEnumAsByte<enum ESlateBrushImageType::Type> ImageType;

protected:

	/** Whether or not the brush path is a path to a UObject */
	UPROPERTY()
	uint8 bIsDynamicallyLoaded:1;

	/** Whether or not the brush has a UTexture resource */
	UPROPERTY()
	uint8 bHasUObject_DEPRECATED:1;

	/** 
	 * This constructor is protected; use one of the deriving classes instead.
	 *
	 * @param InDrawType      How to draw the texture
	 * @param InResourceName  The name of the resource
	 * @param InMargin        Margin to use in border and box modes
	 * @param InTiling        Tile horizontally/vertically or both? (only in image mode)
	 * @param InImageType	  The type of image
	 * @param InTint		  Tint to apply to the element.
	 */
	 FORCENOINLINE FSlateBrush( ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const FVector2D& InImageSize, const FLinearColor& InTint = FLinearColor::White, UObject* InObjectResource = nullptr, bool bInDynamicallyLoaded = false );

	 FORCENOINLINE FSlateBrush( ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const FVector2D& InImageSize, const TSharedRef< FLinearColor >& InTint, UObject* InObjectResource = nullptr, bool bInDynamicallyLoaded = false );

	 FORCENOINLINE FSlateBrush( ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const FVector2D& InImageSize, const FSlateColor& InTint, UObject* InObjectResource = nullptr, bool bInDynamicallyLoaded = false );
};
