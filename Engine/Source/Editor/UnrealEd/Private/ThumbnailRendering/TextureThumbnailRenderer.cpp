// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/TextureThumbnailRenderer.h"
#include "CanvasItem.h"
#include "Engine/Engine.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureLightProfile.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "EngineGlobals.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTargetCube.h"

#include "CubemapUnwrapUtils.h"
#include "NormalMapPreview.h"
#include "CanvasTypes.h"

UTextureThumbnailRenderer::UTextureThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTextureThumbnailRenderer::GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const
{
	UTexture* Texture = Cast<UTexture>(Object);
	UTextureLightProfile* TextureLightProfile = Cast<UTextureLightProfile>(Object);

	if (TextureLightProfile)
	{
		// otherwise a 1D texture would result in a very boring thumbnail
		OutWidth = 192;
		OutHeight = 192;
		return;
	}

	if (Texture != nullptr)
	{
		OutWidth = FMath::TruncToInt(Zoom * (float)Texture->GetSurfaceWidth());
		OutHeight = FMath::TruncToInt(Zoom * (float)Texture->GetSurfaceHeight());
	}
	else
	{
		OutWidth = OutHeight = 0;
	}
}

void UTextureThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas)
{
	UTexture* Texture = Cast<UTexture>(Object);
	if (Texture != nullptr && Texture->Resource != nullptr) 
	{
		UTexture2D* Texture2D = Cast<UTexture2D>(Texture);

		// Take the alpha channel into account for textures that have one.
		// This provides a much better preview than just showing RGB,
		// Because the RGB content in areas with an alpha of 0 is often garbage that will not be seen in normal conditions.
		// Non-UI textures often have uncorrelated data in the alpha channel (like a skin mask, specular power, etc) so we only preview UI textures this way.
		const bool bUseTranslucentBlend = Texture2D && Texture2D->HasAlphaChannel() && ((Texture2D->LODGroup == TEXTUREGROUP_UI) || (Texture2D->LODGroup == TEXTUREGROUP_Pixels2D));

		UTextureCube* TextureCube = Cast<UTextureCube>(Texture);
		UTextureRenderTargetCube* RTTextureCube = Cast<UTextureRenderTargetCube>(Texture);
		UTextureLightProfile* TextureLightProfile = Cast<UTextureLightProfile>(Texture);

		TRefCountPtr<FBatchedElementParameters> BatchedElementParameters;

		if(TextureCube || RTTextureCube)
		{
			// is released by the render thread when it was rendered
			BatchedElementParameters = new FMipLevelBatchedElementParameters((float)0);
			
			// If the thumbnail is square then make it 2:1 for cubes.
			if(Width == Height)
			{
				Height = Width / 2;
				Y += Height / 2;
			}
		}
		else if (TextureLightProfile)
		{
			BatchedElementParameters = new FIESLightProfileBatchedElementParameters(TextureLightProfile->Brightness);
		}
		else if (Texture2D && Texture2D->IsNormalMap())
		{
			BatchedElementParameters = new FNormalMapBatchedElementParameters();
		}

		if (bUseTranslucentBlend)
		{
			// If using alpha, draw a checkerboard underneath first.
			const int32 CheckerDensity = 8;
			auto* Checker = UThumbnailManager::Get().CheckerboardTexture;
			Canvas->DrawTile(
				0.0f, 0.0f, Width, Height,							// Dimensions
				0.0f, 0.0f, CheckerDensity, CheckerDensity,			// UVs
				FLinearColor::White, Checker->Resource);			// Tint & Texture
		}

		// Use A canvas tile item to draw
		FCanvasTileItem CanvasTile( FVector2D( X, Y ), Texture->Resource, FVector2D( Width,Height ), FLinearColor::White );
		CanvasTile.BlendMode = bUseTranslucentBlend ? SE_BLEND_Translucent : SE_BLEND_Opaque;
		CanvasTile.BatchedElementParameters = BatchedElementParameters;
		CanvasTile.Draw( Canvas );

		if (TextureLightProfile)
		{
			float Brightness = TextureLightProfile->Brightness;

			// Brightness in Lumens
			FText BrightnessText = FText::AsNumber( Brightness );
			FCanvasTextItem TextItem( FVector2D( 5.0f, 5.0f ), BrightnessText, GEngine->GetLargeFont(), FLinearColor::White );
			TextItem.EnableShadow(FLinearColor::Black);
			TextItem.Scale = FVector2D(Width / 128.0f, Height / 128.0f);
			TextItem.Draw(Canvas);
		}
	}
}
