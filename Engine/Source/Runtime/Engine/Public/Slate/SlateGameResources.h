// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Styling/StyleDefaults.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyle.h"

struct FAssetData;
class UCurveFloat;
class UCurveLinearColor;
class UCurveVector;
struct FSlateWidgetStyle;

class ENGINE_API FSlateGameResources : public FSlateStyleSet, public FGCObject
{
public:	

	/** Create a new Slate resource set that is scoped to the ScopeToDirectory.
	 *
	 * All paths will be formed as InBasePath + "/" + AssetPath. Lookups will be restricted to ScopeToDirectory.
	 * e.g. Resources.Initialize( "/Game/Widgets/SFortChest", "/Game/Widgets" )
	 *      Resources.GetBrush( "SFortChest/SomeBrush" );         // Will look up "/Game/Widgets" + "/" + "SFortChest/SomeBrush" = "/Game/Widgets/SFortChest/SomeBrush"
	 *      Resources.GetBrush( "SSomeOtherWidget/SomeBrush" );   // Will fail because "/Game/Widgets/SSomeOtherWidget/SomeBrush" is outside directory to which we scoped.
	 */
	static TSharedRef<FSlateGameResources> New( const FName& InStyleSetName, const FString& ScopeToDirectory, const FString& InBasePath = FString() );

	/**
	 * Construct a style chunk.
	 * @param InStyleSetName The name used to identity this style set
	 */
	FSlateGameResources( const FName& InStyleSetName );

	virtual ~FSlateGameResources();

	/** @See New() */
	void Initialize( const FString& ScopeToDirectory, const FString& InBasePath );

	/**
	 * Populate an array of FSlateBrush with resources consumed by this style chunk.
	 * @param OutResources - the array to populate.
	 */
	virtual void GetResources( TArray< const FSlateBrush* >& OutResources ) const override;

	virtual void SetContentRoot( const FString& InContentRootDir ) override;

	virtual const FSlateBrush* GetBrush( const FName PropertyName, const ANSICHAR* Specifier = NULL ) const override;
	virtual const FSlateBrush* GetOptionalBrush(const FName PropertyName, const ANSICHAR* Specifier = NULL, const FSlateBrush* const DefaultBrush = FStyleDefaults::GetNoBrush()) const override;

	UCurveFloat* GetCurveFloat( const FName AssetName ) const;
	UCurveVector* GetCurveVector( const FName AssetName ) const;
	UCurveLinearColor* GetCurveLinearColor( const FName AssetName ) const;

protected:

	virtual const FSlateWidgetStyle* GetWidgetStyleInternal( const FName DesiredTypeName, const FName StyleName ) const override;

	virtual void Log( ISlateStyle::EStyleMessageSeverity Severity, const FText& Message ) const override;

	virtual void Log( const TSharedRef< class FTokenizedMessage >& Message ) const;

	/** Callback registered to the Asset Registry to be notified when an asset is added. */
	void AddAsset(const FAssetData& InAddedAssetData);

	/** Callback registered to the Asset Registry to be notified when an asset is removed. */
	void RemoveAsset(const FAssetData& InRemovedAssetData);

	bool ShouldCache( const FAssetData& InAssetData );

	void AddAssetToCache( UObject* InStyleObject, bool bEnsureUniqueness );

	void RemoveAssetFromCache( const FAssetData& AssetData );

	FName GenerateMapName( const FAssetData& AssetData );

	FName GenerateMapName( UObject* StyleObject );

	/** Takes paths from the Editor's "Copy Reference" button and turns them into paths accepted by this object.
	 *
	 *   Example: 
	 *   This: "SlateBrushAsset'/Game/UI/STeamAndHeroSelection/CS_Port_Brush.CS_Port_Brush'"
	 *	 into
	 *	 This: "/Game/UI/STeamAndHeroSelection/CS_Port_Brush"
	*/
	FName GetCleanName(const FName& AssetName) const;

	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

private:

	TMap< FName, UObject* > UIResources;
	FString BasePath;
	bool HasBeenInitialized;
};
