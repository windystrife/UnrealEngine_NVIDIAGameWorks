// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Models/TextureEditorViewportClient.h"
#include "Widgets/Layout/SScrollBar.h"
#include "CanvasItem.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Texture2D.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"
#include "UnrealEdGlobals.h"
#include "CubemapUnwrapUtils.h"
#include "Slate/SceneViewport.h"
#include "Texture2DPreview.h"
#include "TextureEditorSettings.h"
#include "Widgets/STextureEditorViewport.h"
#include "CanvasTypes.h"
#include "ImageUtils.h"


/* FTextureEditorViewportClient structors
 *****************************************************************************/

FTextureEditorViewportClient::FTextureEditorViewportClient( TWeakPtr<ITextureEditorToolkit> InTextureEditor, TWeakPtr<STextureEditorViewport> InTextureEditorViewport )
	: TextureEditorPtr(InTextureEditor)
	, TextureEditorViewportPtr(InTextureEditorViewport)
	, CheckerboardTexture(NULL)
{
	check(TextureEditorPtr.IsValid() && TextureEditorViewportPtr.IsValid());

	ModifyCheckerboardTextureColors();
}


FTextureEditorViewportClient::~FTextureEditorViewportClient( )
{
	DestroyCheckerboardTexture();
}


/* FViewportClient interface
 *****************************************************************************/

void FTextureEditorViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	if (!TextureEditorPtr.IsValid())
	{
		return;
	}
	
	UTexture* Texture = TextureEditorPtr.Pin()->GetTexture();
	FVector2D Ratio = FVector2D(GetViewportHorizontalScrollBarRatio(), GetViewportVerticalScrollBarRatio());
	FVector2D ViewportSize = FVector2D(TextureEditorViewportPtr.Pin()->GetViewport()->GetSizeXY().X, TextureEditorViewportPtr.Pin()->GetViewport()->GetSizeXY().Y);
	FVector2D ScrollBarPos = GetViewportScrollBarPositions();
	int32 YOffset = (Ratio.Y > 1.0f)? ((ViewportSize.Y - (ViewportSize.Y / Ratio.Y)) * 0.5f): 0;
	int32 YPos = YOffset - ScrollBarPos.Y;
	int32 XOffset = (Ratio.X > 1.0f)? ((ViewportSize.X - (ViewportSize.X / Ratio.X)) * 0.5f): 0;
	int32 XPos = XOffset - ScrollBarPos.X;
	
	UpdateScrollBars();

	const UTextureEditorSettings& Settings = *GetDefault<UTextureEditorSettings>();

	Canvas->Clear( Settings.BackgroundColor );

	TextureEditorPtr.Pin()->PopulateQuickInfo();
	
	// Get the rendering info for this object
	FThumbnailRenderingInfo* RenderInfo = GUnrealEd->GetThumbnailManager()->GetRenderingInfo(Texture);

	// If there is an object configured to handle it, draw the thumbnail
	if (RenderInfo != NULL && RenderInfo->Renderer != NULL)
	{
		UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
		UTextureCube* TextureCube = Cast<UTextureCube>(Texture);
		UTextureRenderTarget2D* TextureRT2D = Cast<UTextureRenderTarget2D>(Texture);
		UTextureRenderTargetCube* RTTextureCube = Cast<UTextureRenderTargetCube>(Texture);

		// Fully stream in the texture before drawing it.
		if (Texture2D)
		{
			Texture2D->SetForceMipLevelsToBeResident(30.0f);
			Texture2D->WaitForStreaming();
		}

		// Figure out the size we need
		uint32 Width, Height;
		TextureEditorPtr.Pin()->CalculateTextureDimensions(Width, Height);

		TRefCountPtr<FBatchedElementParameters> BatchedElementParameters;

		if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4)
		{
			if (TextureCube || RTTextureCube)
			{
				BatchedElementParameters = new FMipLevelBatchedElementParameters((float)TextureEditorPtr.Pin()->GetMipLevel(), false);
			}
			else if (Texture2D)
			{
				float MipLevel = (float)TextureEditorPtr.Pin()->GetMipLevel();
				bool bIsNormalMap = Texture2D->IsNormalMap();
				bool bIsSingleChannel = Texture2D->CompressionSettings == TC_Grayscale || Texture2D->CompressionSettings == TC_Alpha;
				BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, bIsNormalMap, bIsSingleChannel);
			}
			else if (TextureRT2D)
			{
				float MipLevel = (float)TextureEditorPtr.Pin()->GetMipLevel();
				BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, false, false);
			}
			else
			{
				// Default to treating any UTexture derivative as a 2D texture resource
				float MipLevel = (float)TextureEditorPtr.Pin()->GetMipLevel();
				BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, false, false);
			}
		}

		// Draw the background checkerboard pattern in the same size/position as the render texture so it will show up anywhere
		// the texture has transparency
		if (Settings.Background == TextureEditorBackground_CheckeredFill)
		{
			Canvas->DrawTile( 0.0f, 0.0f, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, 0.0f, 0.0f, (Viewport->GetSizeXY().X / CheckerboardTexture->GetSizeX()), (Viewport->GetSizeXY().Y / CheckerboardTexture->GetSizeY()), FLinearColor::White, CheckerboardTexture->Resource);
		}
		else if (Settings.Background == TextureEditorBackground_Checkered)
		{
			Canvas->DrawTile( XPos, YPos, Width, Height, 0.0f, 0.0f, (Width / CheckerboardTexture->GetSizeX()), (Height / CheckerboardTexture->GetSizeY()), FLinearColor::White, CheckerboardTexture->Resource);
		}

		float Exposure = FMath::Pow(2.0f, (float)TextureEditorViewportPtr.Pin()->GetExposureBias());

		if ( Texture->Resource != nullptr )
		{
			FCanvasTileItem TileItem( FVector2D( XPos, YPos ), Texture->Resource, FVector2D( Width, Height ), FLinearColor(Exposure, Exposure, Exposure) );
			TileItem.BlendMode = TextureEditorPtr.Pin()->GetColourChannelBlendMode();
			TileItem.BatchedElementParameters = BatchedElementParameters;
			Canvas->DrawItem( TileItem );

			// Draw a white border around the texture to show its extents
			if (Settings.TextureBorderEnabled)
			{
				FCanvasBoxItem BoxItem( FVector2D(XPos, YPos), FVector2D(Width , Height ) );
				BoxItem.SetColor( Settings.TextureBorderColor );
				Canvas->DrawItem( BoxItem );
			}
		}
	}
}


bool FTextureEditorViewportClient::InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool Gamepad)
{
	if (Key == EKeys::MouseScrollUp)
	{
		TextureEditorPtr.Pin()->ZoomIn();

		return true;
	}
	else if (Key == EKeys::MouseScrollDown)
	{
		TextureEditorPtr.Pin()->ZoomOut();

		return true;
	}

	return false;
}


bool FTextureEditorViewportClient::InputGesture(FViewport* Viewport, EGestureEvent GestureType, const FVector2D& GestureDelta, bool bIsDirectionInvertedFromDevice)
{
	const bool LeftMouseButtonDown = Viewport->KeyState(EKeys::LeftMouseButton);
	const bool RightMouseButtonDown = Viewport->KeyState(EKeys::RightMouseButton);

	if (GestureType == EGestureEvent::Scroll && !LeftMouseButtonDown && !RightMouseButtonDown)
	{
		double CurrentZoom = TextureEditorPtr.Pin()->GetZoom();
		TextureEditorPtr.Pin()->SetZoom(CurrentZoom + GestureDelta.Y * 0.01);
		return true;
	}

	return false;
}


void FTextureEditorViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(CheckerboardTexture);
}


void FTextureEditorViewportClient::ModifyCheckerboardTextureColors()
{
	DestroyCheckerboardTexture();

	const UTextureEditorSettings& Settings = *GetDefault<UTextureEditorSettings>();
	CheckerboardTexture = FImageUtils::CreateCheckerboardTexture(Settings.CheckerColorOne, Settings.CheckerColorTwo, Settings.CheckerSize);
}


FText FTextureEditorViewportClient::GetDisplayedResolution() const
{
	uint32 Height = 1;
	uint32 Width = 1;
	TextureEditorPtr.Pin()->CalculateTextureDimensions(Width, Height);
	return FText::Format( NSLOCTEXT("TextureEditor", "DisplayedResolution", "Displayed: {0}x{1}"), FText::AsNumber( FMath::Max((uint32)1, Width) ), FText::AsNumber( FMath::Max((uint32)1, Height)) );
}


float FTextureEditorViewportClient::GetViewportVerticalScrollBarRatio() const
{
	uint32 Height = 1;
	uint32 Width = 1;
	float WidgetHeight = 1.0f;
	if (TextureEditorViewportPtr.Pin()->GetVerticalScrollBar().IsValid())
	{
		TextureEditorPtr.Pin()->CalculateTextureDimensions(Width, Height);

		WidgetHeight = TextureEditorViewportPtr.Pin()->GetViewport()->GetSizeXY().Y;
	}

	return WidgetHeight / Height;
}


float FTextureEditorViewportClient::GetViewportHorizontalScrollBarRatio() const
{
	uint32 Width = 1;
	uint32 Height = 1;
	float WidgetWidth = 1.0f;
	if (TextureEditorViewportPtr.Pin()->GetHorizontalScrollBar().IsValid())
	{
		TextureEditorPtr.Pin()->CalculateTextureDimensions(Width, Height);

		WidgetWidth = TextureEditorViewportPtr.Pin()->GetViewport()->GetSizeXY().X;
	}

	return WidgetWidth / Width;
}


void FTextureEditorViewportClient::UpdateScrollBars()
{
	TSharedPtr<STextureEditorViewport> Viewport = TextureEditorViewportPtr.Pin();

	if (!Viewport.IsValid() || !Viewport->GetVerticalScrollBar().IsValid() || !Viewport->GetHorizontalScrollBar().IsValid())
	{
		return;
	}

	float VRatio = GetViewportVerticalScrollBarRatio();
	float HRatio = GetViewportHorizontalScrollBarRatio();
	float VDistFromBottom = Viewport->GetVerticalScrollBar()->DistanceFromBottom();
	float HDistFromBottom = Viewport->GetHorizontalScrollBar()->DistanceFromBottom();

	if (VRatio < 1.0f)
	{
		if (VDistFromBottom < 1.0f)
		{
			Viewport->GetVerticalScrollBar()->SetState(FMath::Clamp(1.0f - VRatio - VDistFromBottom, 0.0f, 1.0f), VRatio);
		}
		else
		{
			Viewport->GetVerticalScrollBar()->SetState(0.0f, VRatio);
		}
	}

	if (HRatio < 1.0f)
	{
		if (HDistFromBottom < 1.0f)
		{
			Viewport->GetHorizontalScrollBar()->SetState(FMath::Clamp(1.0f - HRatio - HDistFromBottom, 0.0f, 1.0f), HRatio);
		}
		else
		{
			Viewport->GetHorizontalScrollBar()->SetState(0.0f, HRatio);
		}
	}
}


FVector2D FTextureEditorViewportClient::GetViewportScrollBarPositions() const
{
	FVector2D Positions = FVector2D::ZeroVector;
	if (TextureEditorViewportPtr.Pin()->GetVerticalScrollBar().IsValid() && TextureEditorViewportPtr.Pin()->GetHorizontalScrollBar().IsValid())
	{
		uint32 Width, Height;
		UTexture* Texture = TextureEditorPtr.Pin()->GetTexture();
		float VRatio = GetViewportVerticalScrollBarRatio();
		float HRatio = GetViewportHorizontalScrollBarRatio();
		float VDistFromBottom = TextureEditorViewportPtr.Pin()->GetVerticalScrollBar()->DistanceFromBottom();
		float HDistFromBottom = TextureEditorViewportPtr.Pin()->GetHorizontalScrollBar()->DistanceFromBottom();
	
		TextureEditorPtr.Pin()->CalculateTextureDimensions(Width, Height);

		if ((TextureEditorViewportPtr.Pin()->GetVerticalScrollBar()->GetVisibility() == EVisibility::Visible) && VDistFromBottom < 1.0f)
		{
			Positions.Y = FMath::Clamp(1.0f - VRatio - VDistFromBottom, 0.0f, 1.0f) * Height;
		}
		else
		{
			Positions.Y = 0.0f;
		}

		if ((TextureEditorViewportPtr.Pin()->GetHorizontalScrollBar()->GetVisibility() == EVisibility::Visible) && HDistFromBottom < 1.0f)
		{
			Positions.X = FMath::Clamp(1.0f - HRatio - HDistFromBottom, 0.0f, 1.0f) * Width;
		}
		else
		{
			Positions.X = 0.0f;
		}
	}

	return Positions;
}

void FTextureEditorViewportClient::DestroyCheckerboardTexture()
{
	if (CheckerboardTexture)
	{
		if (CheckerboardTexture->Resource)
		{
			CheckerboardTexture->ReleaseResource();
		}
		CheckerboardTexture->MarkPendingKill();
		CheckerboardTexture = NULL;
	}
}