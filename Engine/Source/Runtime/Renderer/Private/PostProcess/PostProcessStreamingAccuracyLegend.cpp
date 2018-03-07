// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PostProcessVisualizeComplexity.cpp: Contains definitions for complexity viewmode.
=============================================================================*/

#include "PostProcess/PostProcessStreamingAccuracyLegend.h"
#include "CanvasTypes.h"
#include "UnrealEngine.h"
#include "RenderTargetTemp.h"
#include "SceneUtils.h"
#include "DebugViewModeRendering.h"
#include "SceneRendering.h"

void FRCPassPostProcessStreamingAccuracyLegend::DrawDesc(FCanvas& Canvas, float PosX, float PosY, const FText& Text)
{
	Canvas.DrawShadowedText(PosX + 18, PosY, Text, GetStatsFont(), FLinearColor(0.7f, 0.7f, 0.7f), FLinearColor::Black);
}

void FRCPassPostProcessStreamingAccuracyLegend::DrawBox(FCanvas& Canvas, float PosX, float PosY, const FLinearColor& Color, const FText& Text)
{
	Canvas.DrawTile(PosX, PosY, 16, 16, 0, 0, 1, 1, FLinearColor::Black);
	Canvas.DrawTile(PosX + 1, PosY + 1, 14, 14, 0, 0, 1, 1, Color);
	Canvas.DrawShadowedText(PosX + 18, PosY, Text, GetStatsFont(), FLinearColor(0.7f, 0.7f, 0.7f), FLinearColor::Black);
}

void FRCPassPostProcessStreamingAccuracyLegend::DrawCheckerBoard(FCanvas& Canvas, float PosX, float PosY, const FLinearColor& Color0, const FLinearColor& Color1, const FText& Text)
{
	Canvas.DrawTile(PosX, PosY, 16, 16, 0, 0, 1, 1, FLinearColor::Black);
	Canvas.DrawTile(PosX + 1, PosY + 1, 14, 14, 0, 0, 1, 1, Color0);
	Canvas.DrawTile(PosX + 1, PosY + 1, 7, 7, 0, 0, 1, 1, Color1);
	Canvas.DrawTile(PosX + 8, PosY + 8, 7, 7, 0, 0, 1, 1, Color1);
	Canvas.DrawShadowedText(PosX + 18, PosY, Text, GetStatsFont(), FLinearColor(0.7f, 0.7f, 0.7f), FLinearColor::Black);
}

#define LOCTEXT_NAMESPACE "TextureStreamingBuild"

void FRCPassPostProcessStreamingAccuracyLegend::DrawCustom(FRenderingCompositePassContext& Context)
{
	if (Colors.Num() != NumStreamingAccuracyColors) return;

	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessStreamingAccuracyLegend);

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	FIntRect DestRect = View.UnscaledViewRect;

	FRenderTargetTemp TempRenderTarget(View, (const FTexture2DRHIRef&)PassOutputs[0].RequestSurface(Context).TargetableTexture);
	FCanvas Canvas(&TempRenderTarget, NULL, ViewFamily.CurrentRealTime, ViewFamily.CurrentWorldTime, ViewFamily.DeltaWorldTime, Context.GetFeatureLevel());

	if (ViewFamily.GetDebugViewShaderMode() == DVSM_RequiredTextureResolution)
	{
		DrawDesc(Canvas, DestRect.Min.X + 115, DestRect.Max.Y - 75, LOCTEXT("DescRequiredTextureResolution", "Shows the ratio between the currently streamed texture resolution and the resolution wanted by the GPU."));
	}
	else if (ViewFamily.GetDebugViewShaderMode() == DVSM_MaterialTextureScaleAccuracy)
	{
		DrawDesc(Canvas, DestRect.Min.X + 115, DestRect.Max.Y - 75, LOCTEXT("DescMaterialTextureScaleAccuracy", "Shows under/over texture streaming caused by the material texture scales applied when sampling."));
	}
	else if (ViewFamily.GetDebugViewShaderMode() == DVSM_MeshUVDensityAccuracy)
	{
		DrawDesc(Canvas, DestRect.Min.X + 115, DestRect.Max.Y - 75, LOCTEXT("DescUVDensityAccuracy", "Shows under/over texture streaming caused by the mesh UV densities."));
	}
	else if (ViewFamily.GetDebugViewShaderMode() == DVSM_PrimitiveDistanceAccuracy)
	{
		DrawDesc(Canvas, DestRect.Min.X + 115, DestRect.Max.Y - 100, LOCTEXT("DescPrimitiveDistanceAccuracy", "Shows under/over texture streaming caused by the difference between the streamer calculated"));
		DrawDesc(Canvas, DestRect.Min.X + 165, DestRect.Max.Y - 75, LOCTEXT("DescPrimitiveDistanceAccuracy2", "distance-to-mesh via bounding box versus the actual per-pixel depth value."));
	}

	DrawBox(Canvas, DestRect.Min.X + 115, DestRect.Max.Y - 25, Colors[0], LOCTEXT("2XUnder", "2X+ Under"));
	DrawBox(Canvas, DestRect.Min.X + 215, DestRect.Max.Y - 25, Colors[1], LOCTEXT("1XUnder", "1X Under"));
	DrawBox(Canvas, DestRect.Min.X + 315, DestRect.Max.Y - 25, Colors[2], LOCTEXT("Good", "Good"));
	DrawBox(Canvas, DestRect.Min.X + 415, DestRect.Max.Y - 25, Colors[3], LOCTEXT("1xOver", "1X Over"));
	DrawBox(Canvas, DestRect.Min.X + 515, DestRect.Max.Y - 25, Colors[4], LOCTEXT("2XOver", "2X+ Over"));

	const FLinearColor UndefColor(UndefinedStreamingAccuracyIntensity, UndefinedStreamingAccuracyIntensity, UndefinedStreamingAccuracyIntensity, 1.f);
	DrawBox(Canvas, DestRect.Min.X + 615, DestRect.Max.Y - 25, UndefColor, LOCTEXT("Undefined", "Undefined"));
	
	if (ViewFamily.GetDebugViewShaderMode() == DVSM_MaterialTextureScaleAccuracy || ViewFamily.GetDebugViewShaderMode() == DVSM_MeshUVDensityAccuracy)
	{
		DrawCheckerBoard(Canvas, DestRect.Min.X + 715, DestRect.Max.Y - 25, Colors[0], Colors[4], LOCTEXT("WorstUnderAndOver", "Worst Under / Worst Over"));
	}

	Canvas.Flush_RenderThread(Context.RHICmdList);
}
	
#undef LOCTEXT_NAMESPACE
