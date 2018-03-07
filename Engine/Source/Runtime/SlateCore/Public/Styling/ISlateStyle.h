// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/StyleDefaults.h"

class Error;
class UTexture2D;
struct FSlateDynamicImageBrush;
struct FSlateWidgetStyle;

/**
 * 
 */
class SLATECORE_API ISlateStyle
{
public:
	/** Constructor. */
	ISlateStyle()
	{}

	/** Destructor. */
	virtual ~ISlateStyle()
	{}

	/** @return The name used to identity this style set */
	virtual const FName& GetStyleSetName() const = 0;

	/**
	 * Populate an array of FSlateBrush with resources consumed by this style.
	 * @param OutResources - the array to populate.
	 */
	virtual void GetResources( TArray< const FSlateBrush* >& OutResources ) const = 0;

	template< typename WidgetStyleType >            
	const WidgetStyleType& GetWidgetStyle( FName PropertyName, const ANSICHAR* Specifier = nullptr ) const 
	{
		const FSlateWidgetStyle* ExistingStyle = GetWidgetStyleInternal( WidgetStyleType::TypeName, Join( PropertyName, Specifier ) );

		if ( ExistingStyle == nullptr )
		{
			return WidgetStyleType::GetDefault();
		}

		return *static_cast< const WidgetStyleType* >( ExistingStyle );
	}

	template< typename WidgetStyleType >            
	bool HasWidgetStyle( FName PropertyName, const ANSICHAR* Specifier = nullptr ) const
	{
		return GetWidgetStyleInternal( WidgetStyleType::TypeName, Join( PropertyName, Specifier ) ) != nullptr;
	}

	/**
	 * @param PropertyName   Name of the property to get
	 * @param Specifier      An optional string to append to the property name
	 *
	 * @return a float property.
	 */
	virtual float GetFloat( const FName PropertyName, const ANSICHAR* Specifier = nullptr ) const = 0;

	/**
	 * @param PropertyName   Name of the property to get
	 * @param Specifier      An optional string to append to the property name
	 *
	 * @return a FVector2D property.
	 */
	virtual FVector2D GetVector( const FName PropertyName, const ANSICHAR* Specifier = nullptr ) const = 0;

	/**
	 * @param PropertyName   Name of the property to get
	 * @param Specifier      An optional string to append to the property name
	 *
	 * @return a FLinearColor property.
	 */
	virtual const FLinearColor& GetColor( const FName PropertyName, const ANSICHAR* Specifier = nullptr ) const = 0;

	/**
	 * @param PropertyName   Name of the property to get
	 * @param Specifier      An optional string to append to the property name
	 *
	 * @return a FLinearColor property.
	 */
	virtual const FSlateColor GetSlateColor( const FName PropertyName, const ANSICHAR* Specifier = nullptr ) const = 0;

	/**
	 * @param PropertyName   Name of the property to get
	 * @param Specifier      An optional string to append to the property name
	 *
	 * @return a FMargin property.
	 */
	virtual const FMargin& GetMargin( const FName PropertyName, const ANSICHAR* Specifier = nullptr ) const = 0;

	/**
	 * @param PropertyName   Name of the property to get
	 * @param Specifier      An optional string to append to the property name
	 *
	 * @return an FSlateBrush property.
	 */		
	virtual const FSlateBrush* GetBrush( const FName PropertyName, const ANSICHAR* Specifier = nullptr ) const = 0;

	/**
	 * Just like GetBrush, but returns DefaultBrush instead of the
	 * "missing brush" image when the resource is not found.
	 * 
	 * @param PropertyName   Name of the property to get
	 * @param Specifier      An optional string to append to the property name
	 *
	 * @return an FSlateBrush property.
	 */		
	virtual const FSlateBrush* GetOptionalBrush( const FName PropertyName, const ANSICHAR* Specifier = nullptr, const FSlateBrush* const DefaultBrush = FStyleDefaults::GetNoBrush() ) const = 0;

	virtual const TSharedPtr< FSlateDynamicImageBrush > GetDynamicImageBrush( const FName BrushTemplate, const FName TextureName, const ANSICHAR* Specifier = nullptr ) = 0;

	virtual const TSharedPtr< FSlateDynamicImageBrush > GetDynamicImageBrush( const FName BrushTemplate, const ANSICHAR* Specifier, UTexture2D* TextureResource, const FName TextureName ) = 0;

	virtual const TSharedPtr< FSlateDynamicImageBrush > GetDynamicImageBrush( const FName BrushTemplate, UTexture2D* TextureResource, const FName TextureName ) = 0;

	/**
	 * Get default FSlateBrush.
	 * @return -	Default Slate brush value.
	 */
	virtual FSlateBrush* GetDefaultBrush() const = 0;

	/** Look up a sound property specified by PropertyName and optional Specifier. */
	virtual const FSlateSound& GetSound( const FName PropertyName, const ANSICHAR* Specifier = nullptr ) const = 0;

	/**
	 * @param Propertyname   Name of the property to get
	 * @param InFontInfo     The value to set
	 *
	 * @return a InFontInfo property.
	 */		
	virtual FSlateFontInfo GetFontStyle( const FName PropertyName, const ANSICHAR* Specifier = nullptr ) const = 0;

	static FName Join( FName A, const ANSICHAR* B )
	{
		if( B == nullptr )
		{
			return A;
		}
		else
		{
			return FName( *( A.ToString() + B ) );
		}
	}

protected:

	/** 
	 * The severity of the message type 
	 * Ordered according to their severity
	 */
	enum EStyleMessageSeverity
	{
		CriticalError		= 0,
		Error				= 1,
		PerformanceWarning	= 2,
		Warning				= 3,
		Info				= 4,	// Should be last
	};


protected:

	/**
	 * 
	 */
	virtual const FSlateWidgetStyle* GetWidgetStyleInternal( const FName DesiredTypeName, const FName StyleName ) const = 0;

	/**
	 * 
	 */
	virtual void Log( EStyleMessageSeverity Severity, const FText& Message ) const = 0;
};
