// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Sound/SlateSound.h"
#include "Styling/StyleDefaults.h"
#include "Styling/ISlateStyle.h"
#include "Brushes/SlateBorderBrush.h"
#include "Brushes/SlateBoxBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateDynamicImageBrush.h"

class UTexture2D;
struct FSlateWidgetStyle;

/**
 * A slate style chunk that contains a collection of named properties that guide the appearance of Slate.
 * At the moment, basically FEditorStyle.
 */
class SLATECORE_API FSlateStyleSet : public ISlateStyle
{
public:
	/**
	 * Construct a style chunk.
	 * @param InStyleSetName The name used to identity this style set
	 */
	FSlateStyleSet(const FName& InStyleSetName);
	
	/** Destructor. */
	virtual ~FSlateStyleSet();

	virtual const FName& GetStyleSetName() const override;
	virtual void GetResources(TArray< const FSlateBrush* >& OutResources) const override;
	virtual void SetContentRoot(const FString& InContentRootDir);
	virtual FString RootToContentDir(const ANSICHAR* RelativePath, const TCHAR* Extension);
	virtual FString RootToContentDir(const WIDECHAR* RelativePath, const TCHAR* Extension);
	virtual FString RootToContentDir(const FString& RelativePath, const TCHAR* Extension);
	virtual FString RootToContentDir(const ANSICHAR* RelativePath);
	virtual FString RootToContentDir(const WIDECHAR* RelativePath);
	virtual FString RootToContentDir(const FString& RelativePath);

	virtual void SetCoreContentRoot(const FString& InCoreContentRootDir);

	virtual FString RootToCoreContentDir(const ANSICHAR* RelativePath, const TCHAR* Extension);
	virtual FString RootToCoreContentDir(const WIDECHAR* RelativePath, const TCHAR* Extension);
	virtual FString RootToCoreContentDir(const FString& RelativePath, const TCHAR* Extension);
	virtual FString RootToCoreContentDir(const ANSICHAR* RelativePath);
	virtual FString RootToCoreContentDir(const WIDECHAR* RelativePath);
	virtual FString RootToCoreContentDir(const FString& RelativePath);

	virtual float GetFloat(const FName PropertyName, const ANSICHAR* Specifier = nullptr) const override;
	virtual FVector2D GetVector(const FName PropertyName, const ANSICHAR* Specifier = nullptr) const override;
	virtual const FLinearColor& GetColor(const FName PropertyName, const ANSICHAR* Specifier = nullptr) const override;
	virtual const FSlateColor GetSlateColor(const FName PropertyName, const ANSICHAR* Specifier = nullptr) const override;
	virtual const FMargin& GetMargin(const FName PropertyName, const ANSICHAR* Specifier = nullptr) const override;

	virtual const FSlateBrush* GetBrush(const FName PropertyName, const ANSICHAR* Specifier = nullptr) const override;
	virtual const FSlateBrush* GetOptionalBrush(const FName PropertyName, const ANSICHAR* Specifier = nullptr, const FSlateBrush* const DefaultBrush = FStyleDefaults::GetNoBrush()) const override;

	virtual const TSharedPtr< FSlateDynamicImageBrush > GetDynamicImageBrush(const FName BrushTemplate, const FName TextureName, const ANSICHAR* Specifier = nullptr) override;
	virtual const TSharedPtr< FSlateDynamicImageBrush > GetDynamicImageBrush(const FName BrushTemplate, const ANSICHAR* Specifier, UTexture2D* TextureResource, const FName TextureName) override;
	virtual const TSharedPtr< FSlateDynamicImageBrush > GetDynamicImageBrush(const FName BrushTemplate, UTexture2D* TextureResource, const FName TextureName) override;

	virtual FSlateBrush* GetDefaultBrush() const override;

	virtual const FSlateSound& GetSound(const FName PropertyName, const ANSICHAR* Specifier = nullptr) const override;
	virtual FSlateFontInfo GetFontStyle(const FName PropertyName, const ANSICHAR* Specifier = nullptr) const override;

public:

	template< typename DefinitionType >            
	FORCENOINLINE void Set( const FName PropertyName, const DefinitionType& InStyleDefintion )
	{
		WidgetStyleValues.Add( PropertyName, MakeShareable( new DefinitionType( InStyleDefintion ) ) );
	}

	/**
	 * Set float properties
	 * @param PropertyName - Name of the property to set.
	 * @param InFloat - The value to set.
	 */
	FORCENOINLINE void Set( const FName PropertyName, const float InFloat )
	{
		FloatValues.Add( PropertyName, InFloat );
	}

	/**
	 * Add a FVector2D property to this style's collection.
	 * @param PropertyName - Name of the property to set.
	 * @param InVector - The value to set.
	 */
	FORCENOINLINE void Set( const FName PropertyName, const FVector2D InVector )
	{
		Vector2DValues.Add( PropertyName, InVector );
	}

	/**
	 * Set FLinearColor property.
	 * @param PropertyName - Name of the property to set.
	 * @param InColor - The value to set.
	 */
	FORCENOINLINE void Set( const FName PropertyName, const FLinearColor& InColor )
	{
		ColorValues.Add( PropertyName, InColor );
	}

	FORCENOINLINE void Set( const FName PropertyName, const FColor& InColor )
	{
		ColorValues.Add( PropertyName, InColor );
	}

	/**
	 * Add a FSlateLinearColor property to this style's collection.
	 * @param PropertyName - Name of the property to add.
	 * @param InColor - The value to add.
	 */
	FORCENOINLINE void Set( const FName PropertyName, const FSlateColor& InColor )
	{
		SlateColorValues.Add( PropertyName, InColor );
	}

	/**
	 * Add a FMargin property to this style's collection.
	 * @param PropertyName - Name of the property to add.
	 * @param InMargin - The value to add.
	 */
	FORCENOINLINE void Set( const FName PropertyName, const FMargin& InMargin )
	{
		MarginValues.Add( PropertyName, InMargin );
	}

	/**
	 * Add a FSlateBrush property to this style's collection
	 * @param PropertyName - Name of the property to set.
	 * @param InBrush - The brush to set.
	 */
	FORCENOINLINE void Set( const FName PropertyName, FSlateBrush* InBrush )
	{
		BrushResources.Add( PropertyName, InBrush );
	}

	FORCENOINLINE void Set( const FName PropertyName, FSlateNoResource* InBrush )
	{
		BrushResources.Add( PropertyName, InBrush );
	}

	FORCENOINLINE void Set( const FName PropertyName, FSlateBoxBrush* InBrush )
	{
		BrushResources.Add( PropertyName, InBrush );
	}

	FORCENOINLINE void Set( const FName PropertyName, FSlateBorderBrush* InBrush )
	{
		BrushResources.Add( PropertyName, InBrush );
	}

	FORCENOINLINE void Set( const FName PropertyName, FSlateImageBrush* InBrush )
	{
		BrushResources.Add( PropertyName, InBrush );
	}

	FORCENOINLINE void Set( const FName PropertyName, FSlateDynamicImageBrush* InBrush )
	{
		BrushResources.Add( PropertyName, InBrush );
	}

	FORCENOINLINE void Set( const FName PropertyName, FSlateColorBrush* InBrush )
	{
		BrushResources.Add( PropertyName, InBrush );
	}

	/**
	 * Set FSlateSound properties
	 *
	 * @param PropertyName  Name of the property to set
	 * @param InSound		Sound to set
	 */
	FORCENOINLINE void Set( FName PropertyName, const FSlateSound& InSound )
	{
		Sounds.Add( PropertyName, InSound );
	}
		
	/**
	 * Set FSlateFontInfo properties
	 *
	 * @param PropertyName  Name of the property to set
	 * @param InFontStyle   The value to set
	 */
	FORCENOINLINE void Set( FName PropertyName, const FSlateFontInfo& InFontInfo )
	{
		FontInfoResources.Add( PropertyName, InFontInfo );
	}

protected:

	virtual const FSlateWidgetStyle* GetWidgetStyleInternal(const FName DesiredTypeName, const FName StyleName) const override;

	virtual void Log(ISlateStyle::EStyleMessageSeverity Severity, const FText& Message) const override;

	virtual void LogUnusedBrushResources();

protected:

	bool IsBrushFromFile(const FString& FilePath, const FSlateBrush* Brush);

	/** The name used to identity this style set */
	FName StyleSetName;

	/** This dir is Engine/Editor/Slate folder **/
	FString ContentRootDir;

	/** This dir is Engine/Slate folder to share the items **/
	FString CoreContentRootDir;

	TMap< FName, TSharedRef< struct FSlateWidgetStyle > > WidgetStyleValues;

	/** float property storage. */
	TMap< FName, float > FloatValues;

	/** FVector2D property storage. */
	TMap< FName, FVector2D > Vector2DValues;

	/** Color property storage. */
	TMap< FName, FLinearColor > ColorValues;
	
	/** FSlateColor property storage. */
	TMap< FName, FSlateColor > SlateColorValues;

	/** FMargin property storage. */
	TMap< FName, FMargin > MarginValues;

	/* FSlateBrush property storage */
	FSlateBrush* DefaultBrush;
	TMap< FName, FSlateBrush* > BrushResources;

	/** SlateSound property storage */
	TMap< FName, FSlateSound > Sounds;
	
	/** FSlateFontInfo property storage. */
	TMap< FName, FSlateFontInfo > FontInfoResources;

	/** A list of dynamic brushes */
	TMap< FName, TWeakPtr< FSlateDynamicImageBrush > > DynamicBrushes;

	/** A set of resources that were requested, but not found. */
	mutable TSet< FName > MissingResources;
};
