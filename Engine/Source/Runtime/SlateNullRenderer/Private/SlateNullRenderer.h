// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateShaderResource.h"
#include "Rendering/SlateRenderer.h"

class FSlateDrawBuffer;
class FSlateUpdatableTexture;
class SWindow;
struct Rect;

/** A slate null rendering implementation */
class FSlateNullRenderer : public FSlateRenderer
{
public:
	explicit FSlateNullRenderer(const TSharedRef<FSlateFontServices>& InSlateFontServices);

	/** FSlateRenderer interface */
	virtual bool Initialize() override;
	virtual void Destroy() override;
	virtual FSlateDrawBuffer& GetDrawBuffer() override;
	virtual void OnWindowDestroyed( const TSharedRef<SWindow>& InWindow ) override;
	virtual void RequestResize( const TSharedPtr<SWindow>& Window, uint32 NewWidth, uint32 NewHeight ) override;
	virtual void CreateViewport( const TSharedRef<SWindow> Window ) override;
	virtual void UpdateFullscreenState( const TSharedRef<SWindow> Window, uint32 OverrideResX, uint32 OverrideResY ) override;
	virtual void RestoreSystemResolution(const TSharedRef<SWindow> InWindow) override;
	virtual void DrawWindows( FSlateDrawBuffer& InWindowDrawBuffer ) override;
	virtual void ReleaseDynamicResource( const FSlateBrush& InBrush ) override;
	virtual void RemoveDynamicBrushResource( TSharedPtr<FSlateDynamicImageBrush> BrushToRemove ) override;
	virtual FIntPoint GenerateDynamicImageResource(const FName InTextureName) override;
	virtual bool GenerateDynamicImageResource( FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& Bytes ) override;
	virtual void PrepareToTakeScreenshot(const FIntRect& Rect, TArray<FColor>* OutColorData) override;
	virtual FSlateResourceHandle GetResourceHandle( const FSlateBrush& Brush ) override;
	virtual FSlateUpdatableTexture* CreateUpdatableTexture(uint32 Width, uint32 Height) override;
	virtual void ReleaseUpdatableTexture(FSlateUpdatableTexture* Texture) override;
	virtual int32 RegisterCurrentScene(FSceneInterface* Scene) override;
	virtual int32 GetCurrentSceneIndex() const override;
	virtual void ClearScenes() override;
};
