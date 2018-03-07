// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/StyleDefaults.h"
#include "Styling/ISlateStyle.h"

struct FSlateDynamicImageBrush;

/**
 * A collection of named properties that guide the appearance of Slate.
 */
class EDITORSTYLE_API FEditorStyle
{
public:

	template< class T >            
	static const T& GetWidgetStyle( FName PropertyName, const ANSICHAR* Specifier = NULL  ) 
	{
		return Instance->GetWidgetStyle< T >( PropertyName, Specifier );
	}

	static float GetFloat( FName PropertyName, const ANSICHAR* Specifier = NULL )
	{
		return Instance->GetFloat( PropertyName, Specifier );
	}

	static FVector2D GetVector( FName PropertyName, const ANSICHAR* Specifier = NULL ) 
	{
		return Instance->GetVector( PropertyName, Specifier );
	}

	static const FLinearColor& GetColor( FName PropertyName, const ANSICHAR* Specifier = NULL )
	{
		return Instance->GetColor( PropertyName, Specifier );
	}

	static const FSlateColor GetSlateColor( FName PropertyName, const ANSICHAR* Specifier = NULL )
	{
		return Instance->GetSlateColor( PropertyName, Specifier );
	}

	static const FMargin& GetMargin( FName PropertyName, const ANSICHAR* Specifier = NULL )
	{
		return Instance->GetMargin( PropertyName, Specifier );
	}

	static const FSlateBrush* GetBrush( FName PropertyName, const ANSICHAR* Specifier = NULL )
	{
		return Instance->GetBrush( PropertyName, Specifier );
	}

	static const TSharedPtr< FSlateDynamicImageBrush > GetDynamicImageBrush( FName BrushTemplate, FName TextureName, const ANSICHAR* Specifier = NULL )
	{
		return Instance->GetDynamicImageBrush( BrushTemplate, TextureName, Specifier );
	}

	static const TSharedPtr< FSlateDynamicImageBrush > GetDynamicImageBrush( FName BrushTemplate, const ANSICHAR* Specifier, class UTexture2D* TextureResource, FName TextureName )
	{
		return Instance->GetDynamicImageBrush( BrushTemplate, Specifier, TextureResource, TextureName );
	}

	static const TSharedPtr< FSlateDynamicImageBrush > GetDynamicImageBrush( FName BrushTemplate, class UTexture2D* TextureResource, FName TextureName )
	{
		return Instance->GetDynamicImageBrush( BrushTemplate, TextureResource, TextureName );
	}

	static const FSlateSound& GetSound( FName PropertyName, const ANSICHAR* Specifier = NULL )
	{
		return Instance->GetSound( PropertyName, Specifier );
	}
	
	static FSlateFontInfo GetFontStyle( FName PropertyName, const ANSICHAR* Specifier = NULL )
	{
		return Instance->GetFontStyle( PropertyName, Specifier );
	}

	static const FSlateBrush* GetDefaultBrush()
	{
		return Instance->GetDefaultBrush();
	}

	static const FSlateBrush* GetNoBrush()
	{
		return FStyleDefaults::GetNoBrush();
	}

	static const FSlateBrush* GetOptionalBrush( FName PropertyName, const ANSICHAR* Specifier = NULL, const FSlateBrush* const DefaultBrush = FStyleDefaults::GetNoBrush() )
	{
		return Instance->GetOptionalBrush( PropertyName, Specifier, DefaultBrush );
	}

	static void GetResources( TArray< const FSlateBrush* >& OutResources )
	{
		return Instance->GetResources( OutResources );
	}

	static ISlateStyle& Get()
	{
		return *(Instance.Get());
	}

	static const FName& GetStyleSetName()
	{
		return Instance->GetStyleSetName();
	}

	/**
	 * Concatenates two FNames.e If A and B are "Path.To" and ".Something"
	 * the result "Path.To.Something".
	 *
	 * @param A  First FName
	 * @param B  Second name
	 *
	 * @return New FName that is A concatenated with B.
	 */
	static FName Join( FName A, const ANSICHAR* B )
	{
		if( B == NULL )
		{
			return A;
		}
		else
		{
			return FName( *( A.ToString() + B ) );
		}
	}

	static void ResetToDefault();


protected:

	static void SetStyle( const TSharedRef< class ISlateStyle >& NewStyle );

private:

	/** Singleton instance of the slate style */
	static TSharedPtr< class ISlateStyle > Instance;
};
