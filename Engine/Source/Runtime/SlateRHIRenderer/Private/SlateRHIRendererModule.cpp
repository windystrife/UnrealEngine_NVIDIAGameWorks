// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "Fonts/FontTypes.h"
#include "Fonts/FontCache.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Interfaces/ISlate3DRenderer.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "SlateRHIFontTexture.h"
#include "SlateRHIResourceManager.h"
#include "SlateRHIRenderer.h"
#include "Slate3DRenderer.h"
#include "SlateUpdatableBuffer.h"

class FSlateRHIFontAtlasFactory : public ISlateFontAtlasFactory
{
public:
	FSlateRHIFontAtlasFactory()
	{
		if (GIsEditor)
		{
			AtlasSize = 2048;
		}
		else
		{
			AtlasSize = 1024;
			if (GConfig)
			{
				GConfig->GetInt(TEXT("SlateRenderer"), TEXT("FontAtlasSize"), AtlasSize, GEngineIni);
				AtlasSize = FMath::Clamp(AtlasSize, 0, 2048);
			}
		}
	}

	virtual ~FSlateRHIFontAtlasFactory()
	{
	}

	virtual FIntPoint GetAtlasSize() const override
	{
		return FIntPoint(AtlasSize, AtlasSize);
	}

	virtual TSharedRef<FSlateFontAtlas> CreateFontAtlas() const override
	{
		return MakeShareable(new FSlateFontAtlasRHI(AtlasSize, AtlasSize));
	}

	virtual TSharedPtr<ISlateFontTexture> CreateNonAtlasedTexture(const uint32 InWidth, const uint32 InHeight, const TArray<uint8>& InRawData) const override
	{
		if (GIsEditor)
		{
			const uint32 MaxFontTextureDimension = FMath::Min(AtlasSize * 4u, GetMax2DTextureDimension()); // Don't allow textures greater than 4x our atlas size, but still honor the platform limit
			if (InWidth <= MaxFontTextureDimension && InHeight <= MaxFontTextureDimension)
			{
				return MakeShareable(new FSlateFontTextureRHI(InWidth, InHeight, InRawData));
			}
		}
		return nullptr;
	}

private:
	/** Size of each font texture, width and height */
	int32 AtlasSize;
};


/**
 * Implements the Slate RHI Renderer module.
 */
class FSlateRHIRendererModule
	: public ISlateRHIRendererModule
{
public:

	// ISlateRHIRendererModule interface
	virtual TSharedRef<FSlateRenderer> CreateSlateRHIRenderer( ) override
	{
		ConditionalCreateResources();

		return MakeShareable( new FSlateRHIRenderer( SlateFontServices.ToSharedRef(), ResourceManager.ToSharedRef() ) );
	}

	virtual TSharedRef<ISlate3DRenderer, ESPMode::ThreadSafe> CreateSlate3DRenderer(bool bUseGammaCorrection) override
	{
		ConditionalCreateResources();

		return MakeShareable(new FSlate3DRenderer(SlateFontServices.ToSharedRef(), ResourceManager.ToSharedRef(), bUseGammaCorrection), [=] (FSlate3DRenderer* Renderer) {
			Renderer->Cleanup();
		});
	}

	virtual TSharedRef<ISlateFontAtlasFactory> CreateSlateFontAtlasFactory() override
	{
		return MakeShareable(new FSlateRHIFontAtlasFactory);
	}

	virtual TSharedRef<ISlateUpdatableInstanceBuffer> CreateInstanceBuffer( int32 InitialInstanceCount ) override
	{
		return MakeShareable( new FSlateUpdatableInstanceBuffer(InitialInstanceCount) );
	}

	virtual void StartupModule( ) override { }
	virtual void ShutdownModule( ) override { }

private:
	/** Creates resource managers if they do not exist */
	void ConditionalCreateResources()
	{
		if( !ResourceManager.IsValid() )
		{
			ResourceManager = MakeShareable( new FSlateRHIResourceManager );
			FSlateDataPayload::ResourceManager = ResourceManager.Get();
		}

		if( !SlateFontServices.IsValid() )
		{
			const TSharedRef<FSlateFontCache> GameThreadFontCache = MakeShareable(new FSlateFontCache(MakeShareable(new FSlateRHIFontAtlasFactory)));
			const TSharedRef<FSlateFontCache> RenderThreadFontCache = MakeShareable(new FSlateFontCache(MakeShareable(new FSlateRHIFontAtlasFactory)));

			SlateFontServices = MakeShareable(new FSlateFontServices(GameThreadFontCache, RenderThreadFontCache));
		}
	}
private:
	/** Resource manager used for all renderers */
	TSharedPtr<FSlateRHIResourceManager> ResourceManager;

	/** Font services used for all renderers */
	TSharedPtr<FSlateFontServices> SlateFontServices;
};


IMPLEMENT_MODULE( FSlateRHIRendererModule, SlateRHIRenderer ) 
