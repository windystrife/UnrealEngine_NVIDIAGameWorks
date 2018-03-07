// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Misc/Attribute.h"
#include "AssetData.h"
#include "Rendering/RenderingCommon.h"
#include "Widgets/SWidget.h"
#include "TickableEditorObject.h"

class AActor;
class FAssetThumbnailPool;
class FSlateShaderResource;
class FSlateTexture2DRHIRef;
class FSlateTextureRenderTarget2DResource;
struct FPropertyChangedEvent;

namespace EThumbnailLabel
{
	enum Type
	{
		ClassName,
		AssetName,
		NoLabel
	};
};


/** A struct containing details about how the asset thumbnail should behave */
struct FAssetThumbnailConfig
{
	FAssetThumbnailConfig()
		: bAllowFadeIn( false )
		, bForceGenericThumbnail( false )
		, bAllowHintText( true )
		, bAllowAssetSpecificThumbnailOverlay( false )
		, ClassThumbnailBrushOverride( NAME_None )
		, ThumbnailLabel( EThumbnailLabel::ClassName )
		, HighlightedText( FText::GetEmpty() )
		, HintColorAndOpacity( FLinearColor( 0.0f, 0.0f, 0.0f, 0.0f ) )
		, AssetTypeColorOverride()
	{
	}

	bool bAllowFadeIn;
	bool bForceGenericThumbnail;
	bool bAllowHintText;
	bool bAllowAssetSpecificThumbnailOverlay;
	FName ClassThumbnailBrushOverride;
	EThumbnailLabel::Type ThumbnailLabel;
	TAttribute< FText > HighlightedText;
	TAttribute< FLinearColor > HintColorAndOpacity;
	TOptional< FLinearColor > AssetTypeColorOverride;
};


/**
 * Interface for rendering a thumbnail in a slate viewport                   
 */
class FAssetThumbnail
	: public ISlateViewport
	, public TSharedFromThis<FAssetThumbnail>
{
public:
	/**
	 * @param InAsset	The asset to display a thumbnail for
	 * @param InWidth		The width that the thumbnail should be
	 * @param InHeight	The height that the thumbnail should be
	 * @param InThumbnailPool	The thumbnail pool to request textures from
	 */
	UNREALED_API FAssetThumbnail( UObject* InAsset, uint32 InWidth, uint32 InHeight, const TSharedPtr<class FAssetThumbnailPool>& InThumbnailPool );
	UNREALED_API FAssetThumbnail( const FAssetData& InAsset, uint32 InWidth, uint32 InHeight, const TSharedPtr<class FAssetThumbnailPool>& InThumbnailPool );
	UNREALED_API ~FAssetThumbnail();

	/**
	 * @return	The size of the viewport (thumbnail size)                   
	 */
	virtual FIntPoint GetSize() const override;

	/**
	 * @return The texture used to display the viewports content                  
	 */
	virtual FSlateShaderResource* GetViewportRenderTargetTexture() const override;

	/**
	 * Returns true if the viewport should be vsynced.
	 */
	virtual bool RequiresVsync() const override { return false; }

	/**
	 * @return The object we are rendering the thumbnail for                
	 */
	UNREALED_API UObject* GetAsset() const;

	/**
	 * @return The asset data for the object we are rendering the thumbnail for
	 */
	UNREALED_API const FAssetData& GetAssetData() const;

	/**
	 * Sets the asset to render the thumnail for 
	 *
	 * @param InAsset	The new asset
	 */
	UNREALED_API void SetAsset( const UObject* InAsset );

	/**
	 * Sets the asset to render the thumnail for
	 *
	 * @param InAssetData	Asset data containin the the new asset to render
	 */
	UNREALED_API void SetAsset( const FAssetData& InAssetData );

	/**
	 * @return A slate widget representing this thumbnail
	 */
	UNREALED_API TSharedRef<SWidget> MakeThumbnailWidget( const FAssetThumbnailConfig& InConfig = FAssetThumbnailConfig() );

	/** Re-renders this thumbnail */
	UNREALED_API void RefreshThumbnail();

	DECLARE_EVENT(FAssetThumbnail, FOnAssetDataChanged);
	FOnAssetDataChanged& OnAssetDataChanged() { return AssetDataChangedEvent; }

private:
	/** Thumbnail pool for rendering the thumbnail */
	TWeakPtr<FAssetThumbnailPool> ThumbnailPool;
	/** Triggered when the asset data changes */
	FOnAssetDataChanged AssetDataChangedEvent;
	/** The asset data for the object we are rendering the thumbnail for */
	FAssetData AssetData;
	/** Width of the thumbnail */
	uint32 Width;
	/** Height of the thumbnail */
	uint32 Height;
};

/**
 * Utility class for keeping track of, rendering, and recycling thumbnails rendered in Slate              
 */
class FAssetThumbnailPool : public FTickableEditorObject
{
public:

	/**
	 * Constructor 
	 *
	 * @param InNumInPool						The number of thumbnails allowed in the pool
	 * @param InAreRealTimeThumbnailsAllowed	Attribute that determines if thumbnails should be rendered in real-time
	 * @param InMaxFrameTimeAllowance			The maximum number of seconds per tick to spend rendering thumbnails
	 * @param InMaxRealTimeThumbnailsPerFrame	The maximum number of real-time thumbnails to render per tick
	 */
	UNREALED_API FAssetThumbnailPool( uint32 InNumInPool, const TAttribute<bool>& InAreRealTimeThumbnailsAllowed = true, double InMaxFrameTimeAllowance = 0.005, uint32 InMaxRealTimeThumbnailsPerFrame = 3 );

	/** Destructor to free all remaining resources */
	UNREALED_API ~FAssetThumbnailPool();

	//~ Begin FTickableObject Interface
	UNREALED_API virtual TStatId GetStatId() const override;

	/** Checks if any new thumbnails are queued */
	UNREALED_API virtual bool IsTickable() const override;

	/** Ticks the pool, rendering new thumbnails as needed */
	UNREALED_API virtual void Tick( float DeltaTime ) override;

	//~ End FTickableObject Interface

	/**
	 * Releases all rendering resources held by the pool
	 */
	UNREALED_API void ReleaseResources();
	
	/**
	 * Accesses the texture for an object.  If a thumbnail was recently rendered this function simply returns the thumbnail.  If it was not, it requests a new one be generated
	 * No assumptions should be made about whether or not it was rendered
	 *
	 * @param Asset The asset to get the thumbnail for
	 * @param Width	The width of the thumbnail
	 * @param Height The height of the thumbnail
	 * @return The thumbnail for the asset or NULL if one could not be produced
	 */
	FSlateTexture2DRHIRef* AccessTexture( const FAssetData& AssetData, uint32 Width, uint32 Height );

	/**
	 * Adds a referencer to keep textures around as long as they are needed
	 */
	void AddReferencer( const FAssetThumbnail& AssetThumbnail );

	/**
	 * Removes a referencer to clean up textures that are no longer needed
	 */
	void RemoveReferencer( const FAssetThumbnail& AssetThumbnail );

	/** Returns true if the thumbnail for the specified asset in the specified size is in the stack of thumbnails to render */
	bool IsInRenderStack( const TSharedPtr<FAssetThumbnail>& Thumbnail ) const;

	/** Returns true if the thumbnail for the specified asset in the specified size has been rendered */
	bool IsRendered( const TSharedPtr<FAssetThumbnail>& Thumbnail ) const;

	/** Brings all items in ThumbnailsToPrioritize to the front of the render stack if they are actually in the stack */
	UNREALED_API void PrioritizeThumbnails( const TArray< TSharedPtr<FAssetThumbnail> >& ThumbnailsToPrioritize, uint32 Width, uint32 Height );

	/** Register/Unregister a callback for when thumbnails are rendered */
	DECLARE_EVENT_OneParam( FAssetThumbnailPool, FThumbnailRendered, const FAssetData& );
	FThumbnailRendered& OnThumbnailRendered() { return ThumbnailRenderedEvent; }

	/** Register/Unregister a callback for when thumbnails fail to render */
	DECLARE_EVENT_OneParam( FAssetThumbnailPool, FThumbnailRenderFailed, const FAssetData& );
	FThumbnailRenderFailed& OnThumbnailRenderFailed() { return ThumbnailRenderFailedEvent; }

	/** Re-renders the specified thumbnail */
	UNREALED_API void RefreshThumbnail( const TSharedPtr<FAssetThumbnail>& ThumbnailToRefresh );

private:
	/**
	 * Frees the rendering resources and clears a slot in the pool for an asset thumbnail at the specified width and height
	 *
	 * @param ObjectPath	The path to the asset whose thumbnail should be free
	 * @param Width 		The width of the thumbnail to free
	 * @param Height		The height of the thumbnail to free
	 */
	void FreeThumbnail( const FName& ObjectPath, uint32 Width, uint32 Height );

	/** Adds the thumbnails associated with the object found at ObjectPath to the render stack */
	void RefreshThumbnailsFor( FName ObjectPath );

	/** Handler for when an asset is loaded */
	void OnAssetLoaded( UObject* Asset );

	/** Handler for when an actor is moved in a level. Used to update world asset thumbnails. */
	void OnActorPostEditMove( AActor* Actor );

	/** Handler for when an asset is loaded */
	void OnObjectPropertyChanged( UObject* Asset, FPropertyChangedEvent& PropertyChangedEvent );

	/** Handler to dirty cached thumbnails in packages to make sure they are re-rendered later */
	void DirtyThumbnailForObject( UObject* ObjectBeingModified );

private:
	/** Information about a thumbnail */
	struct FThumbnailInfo
	{
		/** The object whose thumbnail is rendered */
		FAssetData AssetData;
		/** Rendering resource for slate */
		FSlateTexture2DRHIRef* ThumbnailTexture;
		/** Render target for slate */
		FSlateTextureRenderTarget2DResource* ThumbnailRenderTarget;
		/** The time since last access */
		float LastAccessTime;
		/** The time since last update */
		float LastUpdateTime;
		/** Width of the thumbnail */
		uint32 Width;
		/** Height of the thumbnail */
		uint32 Height;
		~FThumbnailInfo();
	};

	struct FThumbnailInfo_RenderThread
	{
		/** Rendering resource for slate */
		FSlateTexture2DRHIRef* ThumbnailTexture;
		/** Render target for slate */
		FSlateTextureRenderTarget2DResource* ThumbnailRenderTarget;
		/** Width of the thumbnail */
		uint32 Width;
		/** Height of the thumbnail */
		uint32 Height;

		FThumbnailInfo_RenderThread(const FThumbnailInfo& Info)
			: ThumbnailTexture(Info.ThumbnailTexture)
			, ThumbnailRenderTarget(Info.ThumbnailRenderTarget)
			, Width(Info.Width)
			, Height(Info.Height)
		{}
	};
	
	/** Key for looking up thumbnails in a map */
	struct FThumbId
	{
		FName ObjectPath;
		uint32 Width;
		uint32 Height;

		FThumbId( const FName& InObjectPath, uint32 InWidth, uint32 InHeight )
			: ObjectPath( InObjectPath )
			, Width( InWidth )
			, Height( InHeight )
		{}

		bool operator==( const FThumbId& Other ) const
		{
			return ObjectPath == Other.ObjectPath && Width == Other.Width && Height == Other.Height;
		}

		friend uint32 GetTypeHash( const FThumbId& Id )
		{
			return GetTypeHash( Id.ObjectPath ) ^ GetTypeHash( Id.Width ) ^ GetTypeHash( Id.Height );
		}
	};
	/** The delegate to execute when a thumbnail is rendered */
	FThumbnailRendered ThumbnailRenderedEvent;

	/** The delegate to execute when a thumbnail failed to render */
	FThumbnailRenderFailed ThumbnailRenderFailedEvent;

	/** A mapping of objects to their thumbnails */
	TMap< FThumbId, TSharedRef<FThumbnailInfo> > ThumbnailToTextureMap;

	/** List of thumbnails to render when possible */
	TArray< TSharedRef<FThumbnailInfo> > ThumbnailsToRenderStack;

	/** List of thumbnails that can be rendered in real-time */
	TArray< TSharedRef<FThumbnailInfo> > RealTimeThumbnails;

	/** List of real-time thumbnails that are queued to be rendered */
	TArray< TSharedRef<FThumbnailInfo> > RealTimeThumbnailsToRender;

	/** List of free thumbnails that can be reused */
	TArray< TSharedRef<FThumbnailInfo> > FreeThumbnails;

	/** A mapping of objects to the number of referencers */
	TMap< FThumbId, int32 > RefCountMap;

	/** A list of object paths for recently loaded assets whose thumbnails need to be refreshed. */
	TArray<FName> RecentlyLoadedAssets;

	/** Attribute that determines if real-time thumbnails are allowed. Called every frame. */
	TAttribute<bool> AreRealTimeThumbnailsAllowed;

	/** Max number of thumbnails in the pool */
	uint32 NumInPool;

	/** Max number of dynamic thumbnails to update per frame */
	uint32 MaxRealTimeThumbnailsPerFrame;

	/** Max number of seconds per tick to spend rendering thumbnails */
	double MaxFrameTimeAllowance;
};
