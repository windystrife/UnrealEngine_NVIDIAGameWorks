// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/ISlateStyle.h"

struct FSlateDynamicImageBrush;

/**
 * Core slate style
 */
class SLATECORE_API FCoreStyle 
{
public:

	static TSharedRef<class ISlateStyle> Create( const FName& InStyleSetName = "CoreStyle" );

	/** @return the singleton instance. */
	static const ISlateStyle& Get( )
	{
		return *(Instance.Get());
	}

	static void ResetToDefault( );

	/** Used to override the default selection colors */
	static void SetSelectorColor( const FLinearColor& NewColor );
	static void SetSelectionColor( const FLinearColor& NewColor );
	static void SetInactiveSelectionColor( const FLinearColor& NewColor );
	static void SetPressedSelectionColor( const FLinearColor& NewColor );
	static void SetFocusBrush(FSlateBrush* NewBrush);

	// todo: jdale - These are only here because of UTouchInterface::Activate and the fact that GetDynamicImageBrush is non-const
	static const TSharedPtr<FSlateDynamicImageBrush> GetDynamicImageBrush( FName BrushTemplate, FName TextureName, const ANSICHAR* Specifier = nullptr );
	static const TSharedPtr<FSlateDynamicImageBrush> GetDynamicImageBrush( FName BrushTemplate, const ANSICHAR* Specifier, class UTexture2D* TextureResource, FName TextureName );
	static const TSharedPtr<FSlateDynamicImageBrush> GetDynamicImageBrush( FName BrushTemplate, class UTexture2D* TextureResource, FName TextureName );

private:

	static void SetStyle( const TSharedRef< class ISlateStyle >& NewStyle );

private:

	/** Singleton instances of this style. */
	static TSharedPtr< class ISlateStyle > Instance;
};
