// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Brushes/SlateDynamicImageBrush.h"
#include "Application/SlateApplicationBase.h"


/* FSlateDynamicImageBrush structors
 *****************************************************************************/


TSharedPtr<FSlateDynamicImageBrush> FSlateDynamicImageBrush::CreateWithImageData(
	const FName InTextureName,
	const FVector2D& InImageSize,
	const TArray<uint8>& InImageData,
	const FLinearColor& InTint,
	ESlateBrushTileType::Type InTiling,
	ESlateBrushImageType::Type InImageType)
{
	TSharedPtr<FSlateDynamicImageBrush> Brush;
	if (FSlateApplicationBase::IsInitialized() &&
		FSlateApplicationBase::Get().GetRenderer()->GenerateDynamicImageResource(InTextureName, InImageSize.X, InImageSize.Y, InImageData))
	{
		Brush = MakeShareable(new FSlateDynamicImageBrush(
			InTextureName,
			InImageSize,
			InTint,
			InTiling,
			InImageType));
	}
	return Brush;
}

void FSlateDynamicImageBrush::ReleaseResource()
{
	ReleaseResourceInternal();	
}

void FSlateDynamicImageBrush::ReleaseResourceInternal()
{
	if (bIsInitalized)
	{
		bIsInitalized = false;
		if (FSlateApplicationBase::IsInitialized())
		{
			// Brush resource is no longer referenced by this object
			if (ResourceObject && bRemoveResourceFromRootSet)
			{
				ResourceObject->RemoveFromRoot();
			}

			if (FSlateRenderer* Renderer = FSlateApplicationBase::Get().GetRenderer())
			{
				Renderer->ReleaseDynamicResource(*this);
			}
		}
	}
}

FSlateDynamicImageBrush::~FSlateDynamicImageBrush( )
{
	ReleaseResourceInternal();
}
