// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessVisualizeBuffer.h: Post processing buffer visualization
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: SceneColor
// ePId_Input1: SeparateTranslucency
class FRCPassPostProcessVisualizeBuffer : public TRenderingCompositePassBase<2, 1>
{
public:

	/** Data for a single buffer overview tile **/
	struct TileData
	{
		FRenderingCompositeOutputRef Source;
		FString Name;

		TileData(FRenderingCompositeOutputRef InSource, const FString& InName)
		: Source (InSource)
		, Name (InName)
		{

		}
	};

	// constructor
	void AddVisualizationBuffer(FRenderingCompositeOutputRef InSource, const FString& InName);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:

	// @return VertexShader
	template <bool bDrawTileTemplate>
	FShader* SetShaderTempl(const FRenderingCompositePassContext& Context);

	TArray<TileData> Tiles;
};
