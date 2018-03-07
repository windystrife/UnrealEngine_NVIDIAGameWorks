// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Styling/SlateBrush.h"


/* FSlateBrush constructors
 *****************************************************************************/

FSlateBrush::FSlateBrush( ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const FVector2D& InImageSize, const FLinearColor& InTint, UObject* InObjectResource, bool bInDynamicallyLoaded )
	: ImageSize( InImageSize )
	, Margin( InMargin )
	, TintColor( InTint )
	, ResourceObject( InObjectResource )
	, ResourceName( InResourceName )
	, UVRegion( ForceInit )
	, DrawAs( InDrawType )
	, Tiling( InTiling )
	, Mirroring( ESlateBrushMirrorType::NoMirror )
	, ImageType( InImageType )
	, bIsDynamicallyLoaded( bInDynamicallyLoaded )
{
	bHasUObject_DEPRECATED = (InObjectResource != nullptr) || InResourceName.ToString().StartsWith(FSlateBrush::UTextureIdentifier());

	//Useful for debugging style breakages
	//if ( !bHasUObject_DEPRECATED && InResourceName.IsValid() && InResourceName != NAME_None )
	//{
	//	checkf( FPaths::FileExists( InResourceName.ToString() ), *FPaths::ConvertRelativePathToFull( InResourceName.ToString() ) );
	//}
}


FSlateBrush::FSlateBrush( ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const FVector2D& InImageSize, const TSharedRef< FLinearColor >& InTint, UObject* InObjectResource, bool bInDynamicallyLoaded )
	: ImageSize( InImageSize )
	, Margin( InMargin )
	, TintColor( InTint )
	, ResourceObject( InObjectResource )
	, ResourceName( InResourceName )
	, UVRegion( ForceInit )
	, DrawAs( InDrawType )
	, Tiling( InTiling )
	, Mirroring( ESlateBrushMirrorType::NoMirror )
	, ImageType( InImageType )
	, bIsDynamicallyLoaded( bInDynamicallyLoaded )
{
	bHasUObject_DEPRECATED = (InObjectResource != nullptr) || InResourceName.ToString().StartsWith(FSlateBrush::UTextureIdentifier());

	//Useful for debugging style breakages
	//if ( !bHasUObject_DEPRECATED && InResourceName.IsValid() && InResourceName != NAME_None )
	//{
	//	checkf( FPaths::FileExists( InResourceName.ToString() ), *FPaths::ConvertRelativePathToFull( InResourceName.ToString() ) );
	//}
}


FSlateBrush::FSlateBrush( ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const FVector2D& InImageSize, const FSlateColor& InTint, UObject* InObjectResource, bool bInDynamicallyLoaded )
	: ImageSize(InImageSize)
	, Margin(InMargin)
	, TintColor(InTint)
	, ResourceObject(InObjectResource)
	, ResourceName(InResourceName)
	, UVRegion(ForceInit)
	, DrawAs(InDrawType)
	, Tiling(InTiling)
	, Mirroring( ESlateBrushMirrorType::NoMirror )
	, ImageType(InImageType)
	, bIsDynamicallyLoaded(bInDynamicallyLoaded)
{
	bHasUObject_DEPRECATED = (InObjectResource != nullptr) || InResourceName.ToString().StartsWith(FSlateBrush::UTextureIdentifier());

	//Useful for debugging style breakages
	//if ( !bHasUObject_DEPRECATED && InResourceName.IsValid() && InResourceName != NAME_None )
	//{
	//	checkf( FPaths::FileExists( InResourceName.ToString() ), *FPaths::ConvertRelativePathToFull( InResourceName.ToString() ) );
	//}
}


void FSlateBrush::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject(ResourceObject);
}


/* FSlateBrush static functions
 *****************************************************************************/

const FString FSlateBrush::UTextureIdentifier( )
{
	return FString(TEXT("texture:/"));
}
