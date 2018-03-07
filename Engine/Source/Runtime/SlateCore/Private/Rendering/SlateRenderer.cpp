// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Rendering/SlateRenderer.h"
#include "Textures/TextureAtlas.h"
#include "Fonts/FontCache.h"
#include "Fonts/FontMeasure.h"
#include "Widgets/SWindow.h"


/* FSlateFontCacheProvider interface
 *****************************************************************************/

FSlateFontServices::FSlateFontServices(TSharedRef<class FSlateFontCache> InGameThreadFontCache, TSharedRef<class FSlateFontCache> InRenderThreadFontCache)
	: GameThreadFontCache(InGameThreadFontCache)
	, RenderThreadFontCache(InRenderThreadFontCache)
	, GameThreadFontMeasure(FSlateFontMeasure::Create(GameThreadFontCache))
	, RenderThreadFontMeasure((GameThreadFontCache == RenderThreadFontCache) ? GameThreadFontMeasure : FSlateFontMeasure::Create(RenderThreadFontCache))
{
}


TSharedRef<FSlateFontCache> FSlateFontServices::GetFontCache() const
{
	const ESlateTextureAtlasThreadId AtlasThreadId = GetCurrentSlateTextureAtlasThreadId();
	check(AtlasThreadId != ESlateTextureAtlasThreadId::Unknown);

	if (AtlasThreadId == ESlateTextureAtlasThreadId::Game)
	{
		return GameThreadFontCache;
	}
	else
	{
		return RenderThreadFontCache;
	}
}


TSharedRef<class FSlateFontMeasure> FSlateFontServices::GetFontMeasureService() const
{
	const ESlateTextureAtlasThreadId AtlasThreadId = GetCurrentSlateTextureAtlasThreadId();
	check(AtlasThreadId != ESlateTextureAtlasThreadId::Unknown);

	if (AtlasThreadId == ESlateTextureAtlasThreadId::Game)
	{
		return GameThreadFontMeasure;
	}
	else
	{
		return RenderThreadFontMeasure;
	}
}


void FSlateFontServices::FlushFontCache()
{
	const ESlateTextureAtlasThreadId AtlasThreadId = GetCurrentSlateTextureAtlasThreadId();
	check(AtlasThreadId != ESlateTextureAtlasThreadId::Unknown);

	if (AtlasThreadId == ESlateTextureAtlasThreadId::Game)
	{
		return FlushGameThreadFontCache();
	}
	else
	{
		return FlushRenderThreadFontCache();
	}
}


void FSlateFontServices::FlushGameThreadFontCache()
{
	GameThreadFontCache->RequestFlushCache();
	GameThreadFontMeasure->FlushCache();
}


void FSlateFontServices::FlushRenderThreadFontCache()
{
	RenderThreadFontCache->RequestFlushCache();
	RenderThreadFontMeasure->FlushCache();
}


void FSlateFontServices::ReleaseResources()
{
	GameThreadFontCache->ReleaseResources();

	if (GameThreadFontCache != RenderThreadFontCache)
	{
		RenderThreadFontCache->ReleaseResources();
	}
}


/* FSlateRenderer interface
 *****************************************************************************/

bool FSlateRenderer::IsViewportFullscreen( const SWindow& Window ) const
{
	checkSlow( IsThreadSafeForSlateRendering() );

	bool bFullscreen = false;

	if (FPlatformProperties::SupportsWindowedMode())
	{
		if( GIsEditor)
		{
			bFullscreen = false;
		}
		else
		{
			bFullscreen = Window.GetWindowMode() == EWindowMode::Fullscreen;
		}
	}
	else
	{
		bFullscreen = true;
	}

	return bFullscreen;
}


ISlateAtlasProvider* FSlateRenderer::GetTextureAtlasProvider()
{
	return nullptr;
}


ISlateAtlasProvider* FSlateRenderer::GetFontAtlasProvider()
{
	return &SlateFontServices->GetGameThreadFontCache().Get();
}

TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe> FSlateRenderer::CacheElementRenderData(const ILayoutCache* Cacher, FSlateWindowElementList& ElementList)
{
	return MakeShareable(new FSlateRenderDataHandle(Cacher, nullptr));
}

void FSlateRenderer::ReleaseCachingResourcesFor(const ILayoutCache* Cacher)
{

}

/* Global functions
 *****************************************************************************/

bool IsThreadSafeForSlateRendering()
{
	return ( ( GSlateLoadingThreadId != 0 ) || IsInGameThread() );
}

bool DoesThreadOwnSlateRendering()
{
	if ( IsInGameThread() )
	{
		return GSlateLoadingThreadId == 0;
	}
	else
	{
		return FPlatformTLS::GetCurrentThreadId() == GSlateLoadingThreadId;
	}

	return false;
}
