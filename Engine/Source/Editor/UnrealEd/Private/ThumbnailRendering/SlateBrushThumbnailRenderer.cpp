// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/SlateBrushThumbnailRenderer.h"
#include "Styling/SlateBrush.h"
#include "CanvasItem.h"
#include "Engine/Texture2D.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Slate/SlateBrushAsset.h"
// FPreviewScene derived helpers for rendering
#include "CanvasTypes.h"

USlateBrushThumbnailRenderer::USlateBrushThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USlateBrushThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas)
{
	USlateBrushAsset* SlateBrushAsset = Cast<USlateBrushAsset>(Object);
	if (SlateBrushAsset)
	{
		FSlateBrush Brush = SlateBrushAsset->Brush;
		UTexture2D* Texture = Cast<UTexture2D>( Brush.GetResourceObject() );

		// Draw the background checkboard pattern
		const int32 CheckerDensity = 8;
		auto* Checker = UThumbnailManager::Get().CheckerboardTexture;
		Canvas->DrawTile(
			0.0f, 0.0f, Width, Height,							// Dimensions
			0.0f, 0.0f, CheckerDensity, CheckerDensity,			// UVs
			FLinearColor::White, Checker->Resource);			// Tint & Texture

		if (Texture)
		{
			switch(Brush.DrawAs)
			{
			case ESlateBrushDrawType::Image:
				{
					FCanvasTileItem CanvasTile( FVector2D( X, Y ), Texture->Resource, FVector2D( Width,Height ), Brush.TintColor.GetSpecifiedColor() );
					CanvasTile.BlendMode = SE_BLEND_Translucent;
					CanvasTile.Draw( Canvas );
				}
				break;
			case ESlateBrushDrawType::Border:
				{
					FCanvasTileItem CanvasTile( FVector2D( X, Y ), Texture->Resource, FVector2D( Width,Height ), Brush.TintColor.GetSpecifiedColor() );
					CanvasTile.BlendMode = SE_BLEND_Translucent;
					CanvasTile.Draw( Canvas );
				}
				break;
			case ESlateBrushDrawType::Box:
				{
					float NaturalWidth = Texture->GetSurfaceWidth();
					float NaturalHeight = Texture->GetSurfaceHeight();

					float TopPx = FMath::Clamp<float>(NaturalHeight * Brush.Margin.Top, 0, Height);
					float BottomPx = FMath::Clamp<float>(NaturalHeight * Brush.Margin.Bottom, 0, Height);
					float VerticalCenterPx = FMath::Clamp<float>(Height - TopPx - BottomPx, 0, Height);
					float LeftPx = FMath::Clamp<float>(NaturalWidth * Brush.Margin.Left, 0, Width);
					float RightPx = FMath::Clamp<float>(NaturalWidth * Brush.Margin.Right, 0, Width);
					float HorizontalCenterPx = FMath::Clamp<float>(Width - LeftPx - RightPx, 0, Width);

					// Top-Left
					FVector2D TopLeftSize( LeftPx, TopPx );
					{
						FVector2D UV0( 0, 0 );
						FVector2D UV1( Brush.Margin.Left, Brush.Margin.Top );

						FCanvasTileItem CanvasTile( FVector2D( X, Y ), Texture->Resource, TopLeftSize, UV0, UV1, Brush.TintColor.GetSpecifiedColor() );
						CanvasTile.BlendMode = SE_BLEND_Translucent;
						CanvasTile.Draw( Canvas );
					}

					// Bottom-Left
					FVector2D BottomLeftSize( LeftPx, BottomPx );
					{
						FVector2D UV0( 0, 1 - Brush.Margin.Bottom );
						FVector2D UV1( Brush.Margin.Left, 1 );

						FCanvasTileItem CanvasTile( FVector2D( X, Y + Height - BottomPx ), Texture->Resource, BottomLeftSize, UV0, UV1, Brush.TintColor.GetSpecifiedColor() );
						CanvasTile.BlendMode = SE_BLEND_Translucent;
						CanvasTile.Draw( Canvas );
					}

					// Top-Right
					FVector2D TopRightSize( RightPx, TopPx );
					{
						FVector2D UV0( 1 - Brush.Margin.Right, 0 );
						FVector2D UV1( 1, Brush.Margin.Top );

						FCanvasTileItem CanvasTile( FVector2D( X + Width - RightPx, Y ), Texture->Resource, TopRightSize, UV0, UV1, Brush.TintColor.GetSpecifiedColor() );
						CanvasTile.BlendMode = SE_BLEND_Translucent;
						CanvasTile.Draw( Canvas );
					}

					// Bottom-Right
					FVector2D BottomRightSize( RightPx, BottomPx );
					{
						FVector2D UV0( 1 - Brush.Margin.Right, 1 - Brush.Margin.Bottom );
						FVector2D UV1( 1, 1 );

						FCanvasTileItem CanvasTile( FVector2D( X + Width - RightPx, Y + Height - BottomPx ), Texture->Resource, BottomRightSize, UV0, UV1, Brush.TintColor.GetSpecifiedColor() );
						CanvasTile.BlendMode = SE_BLEND_Translucent;
						CanvasTile.Draw( Canvas );
					}

					//-----------------------------------------------------------------------

					// Center-Vertical-Left
					FVector2D CenterVerticalLeftSize( LeftPx, VerticalCenterPx );
					{
						FVector2D UV0( 0, Brush.Margin.Top );
						FVector2D UV1( Brush.Margin.Left, 1 - Brush.Margin.Bottom );

						FCanvasTileItem CanvasTile( FVector2D( X, Y + TopPx), Texture->Resource, CenterVerticalLeftSize, UV0, UV1, Brush.TintColor.GetSpecifiedColor() );
						CanvasTile.BlendMode = SE_BLEND_Translucent;
						CanvasTile.Draw( Canvas );
					}

					// Center-Vertical-Right
					FVector2D CenterVerticalRightSize( RightPx, VerticalCenterPx );
					{
						FVector2D UV0( 1 - Brush.Margin.Right, Brush.Margin.Top );
						FVector2D UV1( 1, 1 - Brush.Margin.Bottom );

						FCanvasTileItem CanvasTile( FVector2D( X + Width - RightPx, Y + TopPx), Texture->Resource, CenterVerticalRightSize, UV0, UV1, Brush.TintColor.GetSpecifiedColor() );
						CanvasTile.BlendMode = SE_BLEND_Translucent;
						CanvasTile.Draw( Canvas );
					}

					//-----------------------------------------------------------------------

					// Center-Horizontal-Top
					FVector2D CenterHorizontalTopSize( HorizontalCenterPx, TopPx );
					{
						FVector2D UV0( Brush.Margin.Left, 0 );
						FVector2D UV1( 1 - Brush.Margin.Right, Brush.Margin.Top );

						FCanvasTileItem CanvasTile( FVector2D( X + LeftPx, Y), Texture->Resource, CenterHorizontalTopSize, UV0, UV1, Brush.TintColor.GetSpecifiedColor() );
						CanvasTile.BlendMode = SE_BLEND_Translucent;
						CanvasTile.Draw( Canvas );
					}

					// Center-Horizontal-Bottom
					FVector2D CenterHorizontalBottomSize( HorizontalCenterPx, BottomPx );
					{
						FVector2D UV0( Brush.Margin.Left, 1 - Brush.Margin.Bottom );
						FVector2D UV1( 1 - Brush.Margin.Right, 1 );

						FCanvasTileItem CanvasTile( FVector2D( X + LeftPx, Y + Height - BottomPx ), Texture->Resource, CenterHorizontalBottomSize, UV0, UV1, Brush.TintColor.GetSpecifiedColor() );
						CanvasTile.BlendMode = SE_BLEND_Translucent;
						CanvasTile.Draw( Canvas );
					}

					//-----------------------------------------------------------------------

					// Center
					FVector2D CenterSize( HorizontalCenterPx, VerticalCenterPx );
					{
						FVector2D UV0( Brush.Margin.Left, Brush.Margin.Top );
						FVector2D UV1( 1 - Brush.Margin.Right, 1 - Brush.Margin.Bottom );

						FCanvasTileItem CanvasTile( FVector2D( X + LeftPx, Y + TopPx), Texture->Resource, CenterSize, UV0, UV1, Brush.TintColor.GetSpecifiedColor() );
						CanvasTile.BlendMode = SE_BLEND_Translucent;
						CanvasTile.Draw( Canvas );
					}
				}
				break;
			case ESlateBrushDrawType::NoDrawType:
				{
					FCanvasTileItem CanvasTile( FVector2D( X, Y ), Texture->Resource, FVector2D( Width,Height ), Brush.TintColor.GetSpecifiedColor() );
					CanvasTile.BlendMode = SE_BLEND_Translucent;
					CanvasTile.Draw( Canvas );
				}
				break;
			default:

				check(false);
			}
		}
	}
}
