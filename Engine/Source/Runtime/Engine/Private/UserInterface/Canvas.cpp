// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Canvas.cpp: Unreal canvas rendering.
=============================================================================*/

#include "Engine/Canvas.h"
#include "UObject/Package.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineStats.h"
#include "EngineGlobals.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Engine.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Texture2D.h"
#include "SceneUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "EngineFontServices.h"
#include "Internationalization/BreakIterator.h"
#include "Misc/CoreDelegates.h"
#include "DrawingPolicy.h"
#include "OneColorShader.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"

#include "StereoRendering.h"
#include "Debug/ReporterGraph.h"

DEFINE_LOG_CATEGORY_STATIC(LogCanvas, Log, All);


DEFINE_STAT(STAT_Canvas_FlushTime);
DEFINE_STAT(STAT_Canvas_DrawTextureTileTime);
DEFINE_STAT(STAT_Canvas_DrawMaterialTileTime);
DEFINE_STAT(STAT_Canvas_DrawStringTime);
DEFINE_STAT(STAT_Canvas_WordWrappingTime);
DEFINE_STAT(STAT_Canvas_GetBatchElementsTime);
DEFINE_STAT(STAT_Canvas_AddTileRenderTime);
DEFINE_STAT(STAT_Canvas_AddTriangleRenderTime);
DEFINE_STAT(STAT_Canvas_NumBatchesCreated);

FCanvasWordWrapper::FCanvasWordWrapper()
	: GraphemeBreakIterator(FBreakIterator::CreateCharacterBoundaryIterator())
	, LineBreakIterator(FBreakIterator::CreateLineBreakIterator())
{
}

void FCanvasWordWrapper::Execute(const TCHAR* const InString, const FTextSizingParameters& InParameters, TArray<FWrappedStringElement>& OutStrings, FWrappedLineData* const OutWrappedLineData)
{
	SCOPE_CYCLE_COUNTER(STAT_Canvas_WordWrappingTime);

	FWrappingState WrappingState(InString, FCString::Strlen(InString), InParameters, OutStrings, OutWrappedLineData);
	if (WrappingState.WrappedLineData)
	{
		WrappingState.WrappedLineData->Empty();
	}

	if (WrappingState.StringLength > 0)
	{
		GraphemeBreakIterator->SetString(WrappingState.String, WrappingState.StringLength);
		LineBreakIterator->SetString(WrappingState.String, WrappingState.StringLength);

		for(int32 i = 0; i < WrappingState.StringLength; ++i) // Sanity check: Doesn't seem valid to have more lines than code units.
		{	
			if( !ProcessLine(WrappingState) )
			{
				break;
			}
		}
	}
}

bool FCanvasWordWrapper::ProcessLine(FWrappingState& WrappingState)
{
	bool bHasAddedLine = false;
	if(WrappingState.StartIndex < WrappingState.StringLength)
	{
		int32 BreakIndex = FindFirstMandatoryBreakBetween(WrappingState, WrappingState.StringLength);

		int32 NextStartIndex;
		if( BreakIndex == INDEX_NONE || !DoesSubstringFit(WrappingState, BreakIndex) )
		{
			BreakIndex = INDEX_NONE;
			int32 WrapIndex = FindIndexAtOrAfterWrapWidth(WrappingState);

			if (WrapIndex == WrappingState.StringLength)
			{
				BreakIndex = WrapIndex;
			}

			if (BreakIndex <= WrappingState.StartIndex) // No mandatory break.
			{
				BreakIndex = FindLastBreakCandidateBetween(WrappingState.StartIndex, WrapIndex);
			}

			if (BreakIndex <= WrappingState.StartIndex) // No candidate break.
			{
				// Break after minimum length that would preserve the meaning/appearance.
				BreakIndex = FindEndOfLastWholeGraphemeCluster(WrappingState.StartIndex, WrapIndex);
			}

			if (BreakIndex <= WrappingState.StartIndex) // No complete grapheme cluster.
			{
				BreakIndex = WrapIndex; // Break at wrap.
			}

			// Index for the next search
			NextStartIndex = BreakIndex;
		}
		else
		{
			// Index for the next search
			NextStartIndex = BreakIndex;
			// The index is inclusive of the break - we don't want the break char in the string
			--BreakIndex;			
		}

		while (BreakIndex > 0 && FText::IsWhitespace(WrappingState.String[BreakIndex - 1]))
		{
			--BreakIndex;
		}

		if (WrappingState.StartIndex <= BreakIndex)
		{
			AddLine(WrappingState, BreakIndex);

			bHasAddedLine = true;
		}

		while (NextStartIndex < WrappingState.StringLength && FText::IsWhitespace(WrappingState.String[NextStartIndex]))
		{
			++NextStartIndex;
		}

		if(WrappingState.WrappedLineData)
		{
			WrappingState.WrappedLineData->Emplace(WrappingState.StartIndex, BreakIndex);
		}

		WrappingState.StartIndex = NextStartIndex;
	}
	return bHasAddedLine;
}

bool FCanvasWordWrapper::DoesSubstringFit(FWrappingState& WrappingState, const int32 EndIndex)
{
	FTextSizingParameters MeasureParameters(WrappingState.Parameters);
	int32 Unused;
	UCanvas::MeasureStringInternal( MeasureParameters, WrappingState.String + WrappingState.StartIndex, EndIndex - WrappingState.StartIndex, 0, UCanvas::ELastCharacterIndexFormat::Unused, Unused );
	return MeasureParameters.DrawXL <= WrappingState.Parameters.DrawXL;
}

int32 FCanvasWordWrapper::FindIndexAtOrAfterWrapWidth(FWrappingState& WrappingState)
{
	FTextSizingParameters MeasureParameters(WrappingState.Parameters);
	int32 Return = INDEX_NONE;
	UCanvas::MeasureStringInternal(MeasureParameters, WrappingState.String + WrappingState.StartIndex, WrappingState.StringLength - WrappingState.StartIndex, WrappingState.Parameters.DrawXL, UCanvas::ELastCharacterIndexFormat::CharacterAtOffset, Return);
	return WrappingState.StartIndex + Return;
}

void FCanvasWordWrapper::AddLine(FWrappingState& WrappingState, const int32 EndIndex)
{
	FTextSizingParameters MeasureParameters(WrappingState.Parameters);
	FString Substring(EndIndex - WrappingState.StartIndex, WrappingState.String + WrappingState.StartIndex);
	FWrappedStringElement Element(*Substring, 0.0f, 0.0f);
	UCanvas::CanvasStringSize(MeasureParameters, *Element.Value);
	Element.LineExtent.X = MeasureParameters.DrawXL;
	Element.LineExtent.Y = MeasureParameters.DrawYL;
	WrappingState.Results.Add(Element);
}

int32 FCanvasWordWrapper::FindFirstMandatoryBreakBetween(FWrappingState& WrappingState, const int32 WrapIndex)
{
	int32 BreakIndex = INDEX_NONE;
	for(int32 i = WrappingState.StartIndex + 1; i < WrapIndex; ++i)
	{
		const TCHAR Previous = WrappingState.String[i - 1];
		if( FChar::IsLinebreak(Previous) ) // Line break occurs *after* linebreak character.
		{
			const TCHAR* const Current = i < WrapIndex ? WrappingState.String + i : NULL;
			if(	Previous != FChar::CarriageReturn || !(Current && *Current == FChar::LineFeed) ) // Line break cannot occur within CR LF pair.
			{
				BreakIndex = i;
				break;
			}
		}
	}
	// If we reached the end of the string we must also check that the last char is not a newline
	if( BreakIndex == INDEX_NONE )
	{
		const TCHAR Previous = WrappingState.String[WrapIndex - 1];
		if( FChar::IsLinebreak(Previous) ) // Line break occurs *after* linebreak character.
		{
			BreakIndex = WrapIndex;
		}
	}
	return BreakIndex;
}

int32 FCanvasWordWrapper::FindLastBreakCandidateBetween(const int32 InStartIndex, const int32 WrapIndex)
{
	int32 BreakIndex = LineBreakIterator->MoveToCandidateBefore(WrapIndex + 1);
	if(BreakIndex < InStartIndex)
	{
		BreakIndex = INDEX_NONE;
	}
	return BreakIndex;
}

int32 FCanvasWordWrapper::FindEndOfLastWholeGraphemeCluster(const int32 InStartIndex, const int32 WrapIndex)
{
	int32 BreakIndex = GraphemeBreakIterator->MoveToCandidateBefore(WrapIndex + 1);
	if(BreakIndex < InStartIndex)
	{
		BreakIndex = INDEX_NONE;
	}
	return BreakIndex;
}

FCanvas::FCanvas(FRenderTarget* InRenderTarget, FHitProxyConsumer* InHitProxyConsumer, UWorld* InWorld, ERHIFeatureLevel::Type InFeatureLevel, ECanvasDrawMode InDrawMode)
:	ViewRect(0,0,0,0)
,	ScissorRect(0,0,0,0)
,	RenderTarget(InRenderTarget)
,	HitProxyConsumer(InHitProxyConsumer)
,	Scene(InWorld ? InWorld->Scene : NULL)
,	AllowedModes(0xFFFFFFFF)
,	bRenderTargetDirty(false)
,	CurrentRealTime(0)
,	CurrentWorldTime(0)
,	CurrentDeltaWorldTime(0)
,	FeatureLevel(InFeatureLevel)
,	bUseInternalTexture(false)
,	StereoDepth(150)
,	DrawMode(InDrawMode)
{
	Construct();

	if (InWorld)
	{
		CurrentRealTime = InWorld->GetRealTimeSeconds();
		CurrentWorldTime = InWorld->GetTimeSeconds();
		CurrentDeltaWorldTime = InWorld->GetDeltaSeconds();
	}
}

FCanvas::FCanvas(FRenderTarget* InRenderTarget,FHitProxyConsumer* InHitProxyConsumer, float InRealTime, float InWorldTime, float InWorldDeltaTime, ERHIFeatureLevel::Type InFeatureLevel)
:	ViewRect(0,0,0,0)
,	ScissorRect(0,0,0,0)
,	RenderTarget(InRenderTarget)
,	HitProxyConsumer(InHitProxyConsumer)
,	Scene(NULL)
,	AllowedModes(0xFFFFFFFF)
,	bRenderTargetDirty(false)
,	CurrentRealTime(InRealTime)
,	CurrentWorldTime(InWorldTime)
,	CurrentDeltaWorldTime(InWorldDeltaTime)
,	FeatureLevel(InFeatureLevel)
,	bUseInternalTexture(false)
,	StereoDepth(150)
,	DrawMode(CDM_DeferDrawing)
{
	Construct();
}

void FCanvas::Construct()
{
	check(RenderTarget);

	CachedOrthoProjection[0] = CachedOrthoProjection[1] = FMatrix::Identity;
	CachedRTWidth = CachedRTHeight = CachedDrawDepth = -1;
	bStereoRendering = false;
	bScaledToRenderTarget = false;
	bAllowsToSwitchVerticalAxis = true;

	// Push the viewport transform onto the stack.  Default to using a 2D projection. 
	new(TransformStack) FTransformEntry( 
		FMatrix( CalcBaseTransform2D(RenderTarget->GetSizeXY().X,RenderTarget->GetSizeXY().Y) ) 
		);

	// init alpha to 1
	AlphaModulate=1.0;

	// make sure the LastElementIndex is invalid
	LastElementIndex = INDEX_NONE;

	// init sort key to 0
	PushDepthSortKey(0);
}

void FCanvas::SetBaseTransform(const FMatrix& Transform)
{
	// set the base transform
	if( TransformStack.Num() > 0 )
	{
		TransformStack[0].SetMatrix(Transform);
	}
	else
	{
		new(TransformStack) FTransformEntry(Transform);
	}
}

FMatrix FCanvas::CalcBaseTransform2D(uint32 ViewSizeX, uint32 ViewSizeY)
{
	// Guard against division by zero.
	ViewSizeX = FMath::Max<uint32>(ViewSizeX, 1.f);
	ViewSizeY = FMath::Max<uint32>(ViewSizeY, 1.f);

	return AdjustProjectionMatrixForRHI(
		FTranslationMatrix(FVector(0, 0, 0)) *
		FMatrix(
			FPlane(	1.0f / (ViewSizeX / 2.0f),	0.0,										0.0f,	0.0f	),
			FPlane(	0.0f,						-1.0f / (ViewSizeY / 2.0f),					0.0f,	0.0f	),
			FPlane(	0.0f,						0.0f,										1.0f,	0.0f	),
			FPlane(	-1.0f,						1.0f,										0.0f,	1.0f	)
			)
		);
}


FMatrix FCanvas::CalcBaseTransform3D(uint32 ViewSizeX, uint32 ViewSizeY, float fFOV, float NearPlane)
{
	FMatrix ViewMat(CalcViewMatrix(ViewSizeX,ViewSizeY,fFOV));
	FMatrix ProjMat(CalcProjectionMatrix(ViewSizeX,ViewSizeY,fFOV,NearPlane));
	return ViewMat * ProjMat;
}


FMatrix FCanvas::CalcViewMatrix(uint32 ViewSizeX, uint32 ViewSizeY, float fFOV)
{
	// convert FOV to randians
	float FOVRad = fFOV * (float)PI / 360.0f;
	// move camera back enough so that the canvas items being rendered are at the same screen extents as regular canvas 2d rendering	
	FTranslationMatrix CamOffsetMat(-FVector(0,0,-FMath::Tan(FOVRad)*ViewSizeX/2));
	// adjust so that canvas items render as if they start at [0,0] upper left corner of screen 
	// and extend to the lower right corner [ViewSizeX,ViewSizeY]. 
	FMatrix OrientCanvasMat(
		FPlane(	1.0f,				0.0f,				0.0f,	0.0f	),
		FPlane(	0.0f,				-1.0f,				0.0f,	0.0f	),
		FPlane(	0.0f,				0.0f,				1.0f,	0.0f	),
		FPlane(	ViewSizeX * -0.5f,	ViewSizeY * 0.5f,	0.0f, 1.0f		)
		);
	return 
		// also apply screen offset to align to pixel centers
		FTranslationMatrix(FVector(0, 0, 0)) * 
		OrientCanvasMat * 
		CamOffsetMat;
}


FMatrix FCanvas::CalcProjectionMatrix(uint32 ViewSizeX, uint32 ViewSizeY, float fFOV, float NearPlane)
{
	// convert FOV to randians
	float FOVRad = fFOV * (float)PI / 360.0f;
	// project based on the FOV and near plane given
	if ((bool)ERHIZBuffer::IsInverted)
	{
		return AdjustProjectionMatrixForRHI(
			FReversedZPerspectiveMatrix(
				FOVRad,
				ViewSizeX,
				ViewSizeY,
				NearPlane
				)
			);
	}
	else
	{
		return AdjustProjectionMatrixForRHI(
			FPerspectiveMatrix(
				FOVRad,
				ViewSizeX,
				ViewSizeY,
				NearPlane
				)
			);
	}
}

bool FCanvasBatchedElementRenderItem::Render_RenderThread(FRHICommandListImmediate& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const FCanvas* Canvas)
{
	checkSlow(Data);
	bool bDirty = false;
	if (Data->BatchedElements.HasPrimsToDraw())
	{
		bDirty = true;

		// current render target set for the canvas
		const FRenderTarget* CanvasRenderTarget = Canvas->GetRenderTarget();
		float Gamma = 1.0f / CanvasRenderTarget->GetDisplayGamma();
		if (Data->Texture && Data->Texture->bIgnoreGammaConversions)
		{
			Gamma = 1.0f;
		}

		bool bNeedsToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(Canvas->GetShaderPlatform()) && !Canvas->GetAllowSwitchVerticalAxis();

		// draw batched items
		Data->BatchedElements.Draw(
			RHICmdList,
			DrawRenderState,
			Canvas->GetFeatureLevel(),
			bNeedsToSwitchVerticalAxis,
			FBatchedElements::CreateProxySceneView(Data->Transform.GetMatrix(), FIntRect(0, 0, CanvasRenderTarget->GetSizeXY().X, CanvasRenderTarget->GetSizeXY().Y)),
			Canvas->IsHitTesting(),
			Gamma
			);

		if (Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender)
		{
			// delete data since we're done rendering it
			delete Data;
		}
	}
	if (Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender)
	{
		Data = NULL;
	}
	return bDirty;
}

bool FCanvasBatchedElementRenderItem::Render_GameThread(const FCanvas* Canvas)
{	
	checkSlow(Data);
	bool bDirty=false;
	if( Data->BatchedElements.HasPrimsToDraw() )
	{
		bDirty = true;

		// current render target set for the canvas
		const FRenderTarget* CanvasRenderTarget = Canvas->GetRenderTarget();
		float Gamma = 1.0f / CanvasRenderTarget->GetDisplayGamma();
		if ( Data->Texture && Data->Texture->bIgnoreGammaConversions )
		{
			Gamma = 1.0f;
		}

		bool bNeedsToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(Canvas->GetShaderPlatform()) && !Canvas->GetAllowSwitchVerticalAxis();

		// Render the batched elements.
		struct FBatchedDrawParameters
		{
			FRenderData* RenderData;
			uint32 bHitTesting : 1;
			uint32 bNeedsToSwitchVerticalAxis : 1;
			uint32 ViewportSizeX;
			uint32 ViewportSizeY;
			float DisplayGamma;
			uint32 AllowedCanvasModes;
			ERHIFeatureLevel::Type FeatureLevel;
			EShaderPlatform ShaderPlatform;
		};
		// all the parameters needed for rendering
		FBatchedDrawParameters DrawParameters =
		{
			Data,
			(uint32)(Canvas->IsHitTesting() ? 1 : 0),
			(uint32)(bNeedsToSwitchVerticalAxis ? 1 : 0),
			(uint32)CanvasRenderTarget->GetSizeXY().X,
			(uint32)CanvasRenderTarget->GetSizeXY().Y,
			Gamma,
			Canvas->GetAllowedModes(),
			Canvas->GetFeatureLevel(),
			Canvas->GetShaderPlatform()
		};
		ENQUEUE_RENDER_COMMAND(BatchedDrawCommand)(
			[DrawParameters](FRHICommandList& RHICmdList)
			{
				FSceneView SceneView = FBatchedElements::CreateProxySceneView(DrawParameters.RenderData->Transform.GetMatrix(), FIntRect(0, 0, DrawParameters.ViewportSizeX, DrawParameters.ViewportSizeY));

				FDrawingPolicyRenderState DrawRenderState(SceneView);

				// disable depth test & writes
				DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
				DrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());

				// draw batched items
				DrawParameters.RenderData->BatchedElements.Draw(
					RHICmdList,
					DrawRenderState,
					DrawParameters.FeatureLevel,
					DrawParameters.bNeedsToSwitchVerticalAxis,
					SceneView,
					DrawParameters.bHitTesting,
					DrawParameters.DisplayGamma);

				if(DrawParameters.AllowedCanvasModes & FCanvas::Allow_DeleteOnRender )
				{
					delete DrawParameters.RenderData;
				}
			});
	}
	if( Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender )
	{
		Data = NULL;
	}
	return bDirty;
}




FCanvas::FCanvasSortElement& FCanvas::GetSortElement(int32 DepthSortKey)
{
	// Optimization to resuse last index so that the more expensive Find() does not
	// need to be called as much.
	if (SortedElements.IsValidIndex(LastElementIndex))
	{
		FCanvasSortElement & LastElement = SortedElements[LastElementIndex];
		if (LastElement.DepthSortKey == DepthSortKey)
		{
			return LastElement;
		}
	}

	// find the FCanvasSortElement array entry based on the sortkey
	int32 ElementIdx = INDEX_NONE;
	int32* ElementIdxFromMap = SortedElementLookupMap.Find(DepthSortKey);
	if( ElementIdxFromMap )
	{
		ElementIdx = *ElementIdxFromMap;
		checkSlow( SortedElements.IsValidIndex(ElementIdx) );
	}	
	// if it doesn't exist then add a new entry (no duplicates allowed)
	else
	{
		new(SortedElements) FCanvasSortElement(DepthSortKey);
		ElementIdx = SortedElements.Num()-1;
		// keep track of newly added array index for later lookup
		SortedElementLookupMap.Add( DepthSortKey, ElementIdx );
	}
	LastElementIndex = ElementIdx;
	return SortedElements[ElementIdx];
}


FBatchedElements* FCanvas::GetBatchedElements(EElementType InElementType, FBatchedElementParameters* InBatchedElementParameters, const FTexture* InTexture, ESimpleElementBlendMode InBlendMode, const FDepthFieldGlowInfo& GlowInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_Canvas_GetBatchElementsTime);

	// get sort element based on the current sort key from top of sort key stack
	FCanvasSortElement& SortElement = FCanvas::GetSortElement(TopDepthSortKey());
	// find a batch to use 
	FCanvasBatchedElementRenderItem* RenderBatch = NULL;
	// get the current transform entry from top of transform stack
	const FTransformEntry& TopTransformEntry = TransformStack.Top();

	// try to use the current top entry in the render batch array
	if( SortElement.RenderBatchArray.Num() > 0 )
	{
		checkSlow( SortElement.RenderBatchArray.Last() );
		RenderBatch = SortElement.RenderBatchArray.Last()->GetCanvasBatchedElementRenderItem();
	}	

	// if a matching entry for this batch doesn't exist then allocate a new entry
	if( RenderBatch == NULL ||		
		!RenderBatch->IsMatch(InBatchedElementParameters, InTexture, InBlendMode, InElementType, TopTransformEntry, GlowInfo) )
	{
		INC_DWORD_STAT(STAT_Canvas_NumBatchesCreated);

		RenderBatch = new FCanvasBatchedElementRenderItem( InBatchedElementParameters, InTexture, InBlendMode, InElementType, TopTransformEntry, GlowInfo);
		SortElement.RenderBatchArray.Add(RenderBatch);
	}
	return RenderBatch->GetBatchedElements();
}


void FCanvas::AddTileRenderItem(float X, float Y, float SizeX, float SizeY, float U, float V, float SizeU, float SizeV, const FMaterialRenderProxy* MaterialRenderProxy, FHitProxyId HitProxyId, bool bFreezeTime, FColor InColor)
{
	SCOPE_CYCLE_COUNTER(STAT_Canvas_AddTileRenderTime);

	// get sort element based on the current sort key from top of sort key stack
	FCanvasSortElement& SortElement = FCanvas::GetSortElement(TopDepthSortKey());
	// find a batch to use 
	FCanvasTileRendererItem* RenderBatch = NULL;
	// get the current transform entry from top of transform stack
	const FTransformEntry& TopTransformEntry = TransformStack.Top();	

	// try to use the current top entry in the render batch array
	if( SortElement.RenderBatchArray.Num() > 0 )
	{
		checkSlow( SortElement.RenderBatchArray.Last() );
		RenderBatch = SortElement.RenderBatchArray.Last()->GetCanvasTileRendererItem();
	}	
	// if a matching entry for this batch doesn't exist then allocate a new entry
	if( RenderBatch == NULL ||		
		!RenderBatch->IsMatch(MaterialRenderProxy,TopTransformEntry) )
	{
		INC_DWORD_STAT(STAT_Canvas_NumBatchesCreated);

		RenderBatch = new FCanvasTileRendererItem( MaterialRenderProxy,TopTransformEntry,bFreezeTime );
		SortElement.RenderBatchArray.Add(RenderBatch);
	}
	// add the quad to the tile render batch
	RenderBatch->AddTile( X,Y,SizeX,SizeY,U,V,SizeU,SizeV,HitProxyId,InColor);
}

void FCanvas::AddTriangleRenderItem(const FCanvasUVTri& Tri, const FMaterialRenderProxy* MaterialRenderProxy, FHitProxyId HitProxyId, bool bFreezeTime)
{
	SCOPE_CYCLE_COUNTER(STAT_Canvas_AddTriangleRenderTime);
	
	// get sort element based on the current sort key from top of sort key stack
	FCanvasSortElement& SortElement = FCanvas::GetSortElement(TopDepthSortKey());
	// find a batch to use 
	FCanvasTriangleRendererItem* RenderBatch = NULL;

	// get the current transform entry from top of transform stack
	const FTransformEntry& TopTransformEntry = TransformStack.Top();
	
	// try to use the current top entry in the render batch array
	if (SortElement.RenderBatchArray.Num() > 0)
	{
		checkSlow(SortElement.RenderBatchArray.Last());
		RenderBatch = SortElement.RenderBatchArray.Last()->GetCanvasTriangleRendererItem();
	}
	// if a matching entry for this batch doesn't exist then allocate a new entry
	if (RenderBatch == nullptr || !RenderBatch->IsMatch(MaterialRenderProxy, TopTransformEntry))
	{
		INC_DWORD_STAT(STAT_Canvas_NumBatchesCreated);
	
		RenderBatch = new FCanvasTriangleRendererItem(MaterialRenderProxy, TopTransformEntry, bFreezeTime);
		SortElement.RenderBatchArray.Add(RenderBatch);
	}
	// add the triangle to the triangle render batch
	RenderBatch->AddTriangle(Tri, HitProxyId);
}

FCanvas::~FCanvas()
{
	// delete batches from elements entries
	for( int32 Idx=0; Idx < SortedElements.Num(); Idx++ )
	{
		FCanvasSortElement& SortElement = SortedElements[Idx];
		for( int32 BatchIdx=0; BatchIdx < SortElement.RenderBatchArray.Num(); BatchIdx++ )
		{
			FCanvasBaseRenderItem* RenderItem = SortElement.RenderBatchArray[BatchIdx];
			delete RenderItem;
		}
	}
}

void FCanvas::Flush_RenderThread(FRHICommandListImmediate& RHICmdList, bool bForce)
{
	SCOPE_CYCLE_COUNTER(STAT_Canvas_FlushTime);

	if (!(AllowedModes&Allow_Flush) && !bForce)
	{
		return;
	}

	// current render target set for the canvas
	check(RenderTarget);

	// no need to set the render target if we aren't going to draw anything to it!
	if (SortedElements.Num() == 0)
	{
		return;
	}

	// Update the font cache with new text before elements are drawn
	{
		FEngineFontServices::Get().UpdateCache();
	}

	// FCanvasSortElement compare class
	struct FCompareFCanvasSortElement
	{
		FORCEINLINE bool operator()(const FCanvasSortElement& A, const FCanvasSortElement& B) const
		{
			return B.DepthSortKey < A.DepthSortKey;
		}
	};
	// sort the array of FCanvasSortElement entries so that higher sort keys render first (back-to-front)
	SortedElements.Sort(FCompareFCanvasSortElement());

	SCOPED_DRAW_EVENT(RHICmdList, CanvasFlush);
	const FTexture2DRHIRef& RenderTargetTexture = RenderTarget->GetRenderTargetTexture();

	check(IsValidRef(RenderTargetTexture));
	
	// Set the RHI render target.
	if (IsUsingInternalTexture())
	{
		::SetRenderTarget(RHICmdList, RenderTargetTexture, FTexture2DRHIRef(), ESimpleRenderTargetMode::EClearColorAndDepth);
	}
	else
	{
		::SetRenderTarget(RHICmdList, RenderTargetTexture, FTexture2DRHIRef());
	}

	FDrawingPolicyRenderState DrawRenderState;
	// disable depth test & writes
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	if (ViewRect.Area() <= 0)
	{
		ViewRect = FIntRect(FIntPoint::ZeroValue, RenderTarget->GetSizeXY());
	}

	// set viewport to RT size
	RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);

	// Set scissor rect if valid applied
	if (ScissorRect.Area() > 0)
	{
		RHICmdList.SetScissorRect(true, ScissorRect.Min.X, ScissorRect.Min.Y, ScissorRect.Max.X, ScissorRect.Max.Y);
	}

	// iterate over the FCanvasSortElements in sorted order and render all the batched items for each entry
	for (int32 Idx = 0; Idx < SortedElements.Num(); Idx++)
	{
		FCanvasSortElement& SortElement = SortedElements[Idx];
		for (int32 BatchIdx = 0; BatchIdx < SortElement.RenderBatchArray.Num(); BatchIdx++)
		{
			FCanvasBaseRenderItem* RenderItem = SortElement.RenderBatchArray[BatchIdx];
			if (RenderItem)
			{
				// mark current render target as dirty since we are drawing to it
				bRenderTargetDirty |= RenderItem->Render_RenderThread(RHICmdList, DrawRenderState, this);
				if (AllowedModes & Allow_DeleteOnRender)
				{
					delete RenderItem;
				}
			}
		}
		if (AllowedModes & Allow_DeleteOnRender)
		{
			SortElement.RenderBatchArray.Empty();
		}
	}
	if (AllowedModes & Allow_DeleteOnRender)
	{
		// empty the array of FCanvasSortElement entries after finished with rendering	
		SortedElements.Empty();
		SortedElementLookupMap.Empty();
		LastElementIndex = INDEX_NONE;
	}
}

void FCanvas::Flush_GameThread(bool bForce)
{
	SCOPE_CYCLE_COUNTER(STAT_Canvas_FlushTime);

	if( !(AllowedModes&Allow_Flush) && !bForce ) 
	{ 
		return; 
	}

	// current render target set for the canvas
	check(RenderTarget);	 	

	// no need to set the render target if we aren't going to draw anything to it!
	if (SortedElements.Num() == 0)
	{
		return;
	}

	// Update the font cache with new text before elements are drawn
	if (FEngineFontServices::IsInitialized())
	{
		FEngineFontServices::Get().UpdateCache();
	}

	// FCanvasSortElement compare class
	struct FCompareFCanvasSortElement
	{
		FORCEINLINE bool operator()( const FCanvasSortElement& A, const FCanvasSortElement& B ) const
		{
			return B.DepthSortKey < A.DepthSortKey;
		}
	};
	// sort the array of FCanvasSortElement entries so that higher sort keys render first (back-to-front)
	SortedElements.Sort(FCompareFCanvasSortElement());
	
	const FIntPoint RenderTargetSize = RenderTarget->GetSizeXY();
	if (ViewRect.Area() <= 0)
	{
		ViewRect = FIntRect(FIntPoint::ZeroValue, RenderTargetSize);
	}

	struct FCanvasFlushParameters
	{
		FIntRect ViewRect;
		FIntRect ScissorRect;
		const FRenderTarget* CanvasRenderTarget;
		bool bIsScaledToRenderTarget;
	};
	FCanvasFlushParameters FlushParameters =
	{
		ViewRect,
		ScissorRect,
		RenderTarget,
		IsScaledToRenderTarget()
	};
	bool bEmitCanvasDrawEvents = GEmitDrawEvents;

	ENQUEUE_RENDER_COMMAND(CanvasFlushSetupCommand)(
		[FlushParameters](FRHICommandList& RHICmdList)
		{
			// Set the RHI render target.
			::SetRenderTarget(RHICmdList, FlushParameters.CanvasRenderTarget->GetRenderTargetTexture(), FTextureRHIRef(), true);

			FIntRect ViewportRect = FlushParameters.ViewRect;
			FIntRect ScissorRectParam = FlushParameters.ScissorRect;
			if (FlushParameters.bIsScaledToRenderTarget)
			{
				FIntPoint CanvasSize = FlushParameters.CanvasRenderTarget->GetSizeXY();
				ViewportRect = FIntRect(0, 0, CanvasSize.X, CanvasSize.Y);
			}
			else if (ScissorRectParam.Area() > 0)
			{
				RHICmdList.SetScissorRect(true, ScissorRectParam.Min.X, ScissorRectParam.Min.Y, ScissorRectParam.Max.X, ScissorRectParam.Max.Y);
			}

			// set viewport to RT size
			RHICmdList.SetViewport(ViewportRect.Min.X, ViewportRect.Min.Y, 0.0f, ViewportRect.Max.X, ViewportRect.Max.Y, 1.0f);
		});

	// iterate over the FCanvasSortElements in sorted order and render all the batched items for each entry
	for( int32 Idx=0; Idx < SortedElements.Num(); Idx++ )
	{
		FCanvasSortElement& SortElement = SortedElements[Idx];
		for( int32 BatchIdx=0; BatchIdx < SortElement.RenderBatchArray.Num(); BatchIdx++ )
		{
			FCanvasBaseRenderItem* RenderItem = SortElement.RenderBatchArray[BatchIdx];
			if( RenderItem )
			{
				// mark current render target as dirty since we are drawing to it
				bRenderTargetDirty |= RenderItem->Render_GameThread(this);
				if( AllowedModes & Allow_DeleteOnRender )
				{
					delete RenderItem;
				}
			}			
		}
		if( AllowedModes & Allow_DeleteOnRender )
		{
			SortElement.RenderBatchArray.Empty();
		}
	}
	if( AllowedModes & Allow_DeleteOnRender )
	{
		// empty the array of FCanvasSortElement entries after finished with rendering	
		SortedElements.Empty();
		SortedElementLookupMap.Empty();
		LastElementIndex = INDEX_NONE;
	}
}

void FCanvas::PushRelativeTransform(const FMatrix& Transform)
{
	int32 PreviousTopIndex = TransformStack.Num() - 1;
#if 0
	static bool DEBUG_NoRotation=1;
	if( DEBUG_NoRotation )
	{
		FMatrix TransformNoRotation(FMatrix::Identity);
		TransformNoRotation.SetOrigin(Transform.GetOrigin());
		TransformStack.AddItem( FTransformEntry(TransformNoRotation * TransformStack(PreviousTopIndex).GetMatrix()) );
	}
	else
#endif
	{
		TransformStack.Add( FTransformEntry(Transform * TransformStack[PreviousTopIndex].GetMatrix()) );
	}
}

void FCanvas::PushAbsoluteTransform(const FMatrix& Transform) 
{
	if(TransformStack.Num()>0)
	{
		TransformStack.Add(FTransformEntry(Transform * TransformStack[0].GetMatrix()));
	}
	else
	{
		TransformStack.Add(FTransformEntry(Transform));
	}
}

void FCanvas::PopTransform()
{
	TransformStack.Pop();
}

void FCanvas::SetHitProxy(HHitProxy* HitProxy)
{
	// Change the current hit proxy.
	CurrentHitProxy = HitProxy;

	if(HitProxyConsumer && HitProxy)
	{
		// Notify the hit proxy consumer of the new hit proxy.
		HitProxyConsumer->AddHitProxy(HitProxy);
	}
}

bool FCanvas::HasBatchesToRender() const
{
	for( int32 Idx=0; Idx < SortedElements.Num(); Idx++ )
	{
		const FCanvasSortElement& SortElement = SortedElements[Idx];
		for( int32 BatchIdx=0; BatchIdx < SortElement.RenderBatchArray.Num(); BatchIdx++ )
		{
			if( SortElement.RenderBatchArray[BatchIdx] )
			{
				return true;
			}
		}
	}
	return false;
}


void FCanvas::CopyTransformStack(const FCanvas& Copy)
{ 
	TransformStack = Copy.TransformStack;
}

void FCanvas::SetRenderTarget_GameThread(FRenderTarget* NewRenderTarget)
{
	if( RenderTarget != NewRenderTarget )
	{
		// flush whenever we swap render targets
		if( RenderTarget )
		{
			Flush_GameThread();
		}
		// Change the current render target.
		RenderTarget = NewRenderTarget;
	}
}

void FCanvas::SetRenderTargetRect( const FIntRect& InViewRect )
{
	ViewRect = InViewRect;
}

void FCanvas::SetRenderTargetScissorRect( const FIntRect& InScissorRect )
{
	ScissorRect = InScissorRect;
}

void FCanvas::Clear(const FLinearColor& ClearColor)
{
	FRenderTarget* CanvasRenderTarget = GetRenderTarget();
	ENQUEUE_RENDER_COMMAND(ClearCommand)(
		[ClearColor, CanvasRenderTarget](FRHICommandList& RHICmdList)
		{
			SCOPED_DRAW_EVENT(RHICmdList, CanvasClear);
			if (CanvasRenderTarget)
			{
				// possibility for the RTT to be null for nullrhi
				if (CanvasRenderTarget->GetRenderTargetTexture() && CanvasRenderTarget->GetRenderTargetTexture()->GetClearBinding() == FClearValueBinding(ClearColor))
				{
					// do fast clear
					SetRenderTarget(RHICmdList, CanvasRenderTarget->GetRenderTargetTexture(), FTextureRHIRef(), ESimpleRenderTargetMode::EClearColorAndDepth);
				}
				else
				{
					::SetRenderTarget(RHICmdList, CanvasRenderTarget->GetRenderTargetTexture(), FTextureRHIRef(), true);
					RHICmdList.SetViewport(0, 0, 0.0f, CanvasRenderTarget->GetSizeXY().X, CanvasRenderTarget->GetSizeXY().Y, 1.0f);
					DrawClearQuad(RHICmdList, ClearColor);
				}
			}
			else
			{
				ensureMsgf(0, TEXT("What is the current render target here?"));
				//RHICmdList.ClearColorTexture(CanvasRenderTarget->GetRenderTargetTexture(), ClearColor, FIntRect());
			}
		});
}

void FCanvas::DrawTile( float X, float Y, float SizeX,	float SizeY, float U, float V, float SizeU, float SizeV, const FLinearColor& Color,	const FTexture* Texture, bool AlphaBlend )
{
	SCOPE_CYCLE_COUNTER(STAT_Canvas_DrawTextureTileTime);

	FCanvasTileItem TileItem(FVector2D(X,Y), Texture ? Texture : GWhiteTexture, FVector2D(SizeX,SizeY), FVector2D(U,V), FVector2D(SizeU,SizeV), Color);
	TileItem.BlendMode = AlphaBlend ? 
		(bUseInternalTexture ? SE_BLEND_TranslucentAlphaOnlyWriteAlpha : SE_BLEND_Translucent) :
		SE_BLEND_Opaque;
	DrawItem(TileItem);
}

int32 FCanvas::DrawShadowedString( float StartX,float StartY,const TCHAR* Text,const UFont* Font,const FLinearColor& Color, const float TextScale, const FLinearColor& ShadowColor )
{
	const float Z = 1.0f;
	FCanvasTextItem TextItem( FVector2D( StartX, StartY ), FText::FromString( Text ), Font, Color );
	TextItem.Scale = FVector2D(TextScale, TextScale);
	// just render text in single pass for distance field drop shadow
	if (Font && Font->ImportOptions.bUseDistanceFieldAlpha)
	{	
		TextItem.BlendMode = SE_BLEND_MaskedDistanceFieldShadowed;
	}
	else
	{
		TextItem.EnableShadow( ShadowColor );
	}
	
	DrawItem( TextItem );
	return TextItem.DrawnSize.Y;	
}

void FCanvas::DrawNGon(const FVector2D& Center, const FColor& Color, int32 NumSides, float Radius)
{
	FCanvasNGonItem NGonItem(Center, FVector2D(Radius, Radius), FMath::Clamp(NumSides, 3, 255), Color);
	DrawItem(NGonItem);
}

int32 FCanvas::DrawShadowedText( float StartX,float StartY,const FText& Text,const UFont* Font,const FLinearColor& Color, const FLinearColor& ShadowColor )
{
	const float Z = 1.0f;
	FCanvasTextItem TextItem( FVector2D( StartX, StartY ), Text, Font, Color );
	// just render text in single pass for distance field drop shadow
	if (Font && Font->ImportOptions.bUseDistanceFieldAlpha)
	{	
		TextItem.BlendMode = SE_BLEND_MaskedDistanceFieldShadowed;
	}
	else
	{
		TextItem.EnableShadow( ShadowColor );
	}

	DrawItem( TextItem );
	return TextItem.DrawnSize.Y;	
}

void FCanvas::WrapString( FTextSizingParameters& Parameters, const float InCurX, const TCHAR* const pText, TArray<FWrappedStringElement>& out_Lines, FCanvasWordWrapper::FWrappedLineData* const OutWrappedLineData)
{
	if (!WordWrapper.IsValid())
	{
		WordWrapper = MakeShareable(new FCanvasWordWrapper());
	}
	UCanvas::WrapString( *WordWrapper, Parameters, InCurX, pText, out_Lines, OutWrappedLineData);
}

ENGINE_API void StringSize(const UFont* Font,int32& XL,int32& YL,const TCHAR* Text)
{
	// this functionality has been moved to a static function in UIString
	FTextSizingParameters Parameters(Font,1.f,1.f);
	UCanvas::CanvasStringSize(Parameters, Text);
	XL = FMath::TruncToInt(Parameters.DrawXL);
	YL = FMath::TruncToInt(Parameters.DrawYL);
}

/**
 * Calculates the width and height of a typical character in the specified font.
 *
 * @param	DrawFont			the font to use for calculating the character size
 * @param	DefaultCharWidth	[out] will be set to the width of the typical character
 * @param	DefaultCharHeight	[out] will be set to the height of the typical character
 * @param	pDefaultChar		if specified, pointer to a single character to use for calculating the default character size
 */
static void GetDefaultCharSize( const UFont* DrawFont, float& DefaultCharWidth, float& DefaultCharHeight, const TCHAR* pDefaultChar=NULL )
{
	TCHAR DefaultChar = pDefaultChar != NULL ? *pDefaultChar : TEXT('0');
	DrawFont->GetCharSize(DefaultChar, DefaultCharWidth, DefaultCharHeight);
	if ( DefaultCharWidth == 0 )
	{
		// this font doesn't contain '0', try 'A'
		DrawFont->GetCharSize(TEXT('A'), DefaultCharWidth, DefaultCharHeight);
	}
}

void UCanvas::MeasureStringInternal( FTextSizingParameters& Parameters, const TCHAR* const pText, const int32 TextLength, const int32 StopAfterHorizontalOffset, const ELastCharacterIndexFormat CharIndexFormat, int32& OutLastCharacterIndex )
{
	// initialize output so it always makes some sense
	OutLastCharacterIndex = INDEX_NONE;

	Parameters.DrawXL = 0.f;
	Parameters.DrawYL = 0.f;

	if( Parameters.DrawFont )
	{
		// get a default character width and height to be used for non-renderable characters
		float DefaultCharWidth, DefaultCharHeight;
		GetDefaultCharSize( Parameters.DrawFont, DefaultCharWidth, DefaultCharHeight );

		// we'll need to use scaling in multiple places, so create a variable to hold it so that if we modify
		// how the scale is calculated we only have to update these two lines
		const float ScaleX = Parameters.Scaling.X;
		const float ScaleY = Parameters.Scaling.Y;

		const float DefaultCharIncrement = Parameters.SpacingAdjust.X * ScaleX;
		const float DefaultScaledHeight = DefaultCharHeight * ScaleY + Parameters.SpacingAdjust.Y * ScaleY;
		const TCHAR* pCurrentPos;
		const TCHAR* pPrevPos = nullptr;
		for ( pCurrentPos = pText; *pCurrentPos && pCurrentPos < pText + TextLength; ++pCurrentPos )
		{
			float CharWidth, CharHeight;
			const TCHAR* const pNextPos = pCurrentPos + 1;

			TCHAR Ch = *pCurrentPos;
			Parameters.DrawFont->GetCharSize(Ch, CharWidth, CharHeight);
			if ( CharHeight == 0 && Ch == TEXT('\n') )
			{
				CharHeight = DefaultCharHeight;
			}

			float CharSpacing = DefaultCharIncrement;
			if ( pPrevPos )
			{
				CharSpacing += Parameters.DrawFont->GetCharKerning( *pPrevPos, Ch ) * ScaleX;
			}

			CharWidth *= ScaleX;
			CharHeight *= ScaleY;

			// never add character spacing if the next character is whitespace
			if ( !FChar::IsWhitespace(*pNextPos) )
			{
				// if we have another character, append the character spacing
				if ( *pNextPos )
				{
					CharWidth += CharSpacing;
				}
			}

			const float ScaledVertSpacing = Parameters.SpacingAdjust.Y * ScaleY;

			Parameters.DrawXL += CharWidth;
			Parameters.DrawYL = FMath::Max<float>(Parameters.DrawYL, CharHeight + ScaledVertSpacing );

			// Were we asked to stop measuring after the specified horizontal offset in pixels?
			if( StopAfterHorizontalOffset != INDEX_NONE )
			{
				if( CharIndexFormat == ELastCharacterIndexFormat::CharacterAtOffset )
				{
					// Round our test toward the character's center position
					const float TotalCharWidth = CharWidth + Parameters.DrawFont->GetCharHorizontalOffset(Ch);
					if( StopAfterHorizontalOffset < Parameters.DrawXL - TotalCharWidth / 2 )
					{
						// We've reached the stopping point, so bail
						break;
					}
				}
				else if( CharIndexFormat == ELastCharacterIndexFormat::LastWholeCharacterBeforeOffset )
				{
					if( StopAfterHorizontalOffset < Parameters.DrawXL - CharWidth )
					{
						--pCurrentPos;
						// We've reached the stopping point, so bail
						break;
					}
				}
			}

			pPrevPos = pCurrentPos;
		}

		OutLastCharacterIndex = pCurrentPos - pText;
	}
}

void UCanvas::CanvasStringSize( FTextSizingParameters& Parameters, const TCHAR* const pText )
{
	int32 Unused;
	MeasureStringInternal(Parameters, pText, FCString::Strlen(pText), 0, ELastCharacterIndexFormat::Unused, Unused);
}

void UCanvas::WrapString( FCanvasWordWrapper& Wrapper, FTextSizingParameters& Parameters, const float InCurX, const TCHAR* const pText, TArray<FWrappedStringElement>& out_Lines, FCanvasWordWrapper::FWrappedLineData* const OutWrappedLineData)
{
	Wrapper.Execute(pText, Parameters, out_Lines, OutWrappedLineData);
}

void UCanvas::WrapString( FTextSizingParameters& Parameters, const float InCurX, const TCHAR* const pText, TArray<FWrappedStringElement>& out_Lines, FCanvasWordWrapper::FWrappedLineData* const OutWrappedLineData)
{
	Canvas->WrapString(Parameters, InCurX, pText, out_Lines, OutWrappedLineData);
}

/*-----------------------------------------------------------------------------
	UCanvas object functions.
-----------------------------------------------------------------------------*/

UCanvas::UCanvas(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UTexture2D> WhiteSquareTexture;
		ConstructorHelpers::FObjectFinder<UPackage> WhiteSquareTextureP;
		ConstructorHelpers::FObjectFinder<UTexture2D> GradientTexture0;
		ConstructorHelpers::FObjectFinder<UPackage> GradientTexture0P;
		FConstructorStatics()
			: WhiteSquareTexture(TEXT("/Engine/EngineResources/WhiteSquareTexture"))
			, WhiteSquareTextureP(TEXT("/Engine/EngineResources/WhiteSquareTexture."))
			, GradientTexture0(TEXT("/Engine/EngineResources/GradientTexture0"))
			, GradientTexture0P(TEXT("/Engine/EngineResources/GradientTexture0."))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	DrawColor = FColor(127, 127, 127, 255);
	ColorModulate = FPlane(1.0f, 1.0f, 1.0f, 1.0f);

	DefaultTexture = ConstructorStatics.WhiteSquareTexture.Object;
	GradientTexture0 = ConstructorStatics.GradientTexture0.Object;
	HmdOrientation = FQuat::Identity;
	ViewProjectionMatrix = FMatrix::Identity;

	UnsafeSizeX = 0;
	UnsafeSizeY = 0;
	SafeZonePadX = 0;
	SafeZonePadY = 0;
	CachedDisplayWidth = 0;
	CachedDisplayHeight = 0;

	// only call once on construction.  Expensive on some platforms (occulus).
	// Init gets called every frame.	
	UpdateSafeZoneData();

	FCoreDelegates::OnSafeFrameChangedEvent.AddUObject(this, &UCanvas::UpdateSafeZoneData);
}

void UCanvas::Init(int32 InSizeX, int32 InSizeY, FSceneView* InSceneView, FCanvas* InCanvas)
{
	HmdOrientation = FQuat::Identity;
	SizeX = InSizeX;
	SizeY = InSizeY;
	UnsafeSizeX = SizeX;
	UnsafeSizeY = SizeY;
	SceneView = InSceneView;
	Canvas = InCanvas;

	Update();
}


void UCanvas::BeginDestroy()
{
	Super::BeginDestroy();
	FCoreDelegates::OnSafeFrameChangedEvent.RemoveAll(this);
}


void UCanvas::ApplySafeZoneTransform()
{
	// if there is no required safezone padding, then we can bail on the whole operation.
	// this is also basically punting on a bug we have on PC where GetDisplayMetrics isn't the right thing
	// to do, as it gives the monitor size, not the window size which we actually want.
	if (SafeZonePadX == 0 && SafeZonePadY == 0)
	{
		return;
	}

	//The basic idea behind this type of safezone handling is that we shrink the canvas to only the safe size, then we apply a transform
	//to place the canvas such that the all the safezone space is empty.  Another method that was attempted was to modify OrgXY and ClipXY,
	//however given the usage of those members inside and OUTSIDE of the canvas system it was deemed not safe at this time.  For example, adding 100
	//to OrgX, might have the effect of actually stretching out some HUD elements.	

	//We must account for the view's position and the existing origin when applying the safe zone.  A view might already be safe and not require or desire having its components
	//shoved around.  e.g.  The top viewport in horizontal splitscreen doesn't need its bottom adjusted as that is already in the middle of the screen.	
	int32 ViewOrigX = 0;
	int32 ViewOrigY = 0;

	int32 ViewMaxX = SizeX;
	int32 ViewMaxY = SizeY;

	//get the view's location and extents on the screen if we can
	if (SceneView)
	{
		ViewOrigX = SceneView->UnscaledViewRect.Min.X;
		ViewOrigY = SceneView->UnscaledViewRect.Min.Y;

		ViewMaxX = SceneView->UnscaledViewRect.Max.X;
		ViewMaxY = SceneView->UnscaledViewRect.Max.Y;
	}	

	//compute the absolute origin position on the screen.  We may not need to move the origin, or move it only a fraction of the full safe zone value to get it to be safe.
	int32 AbsOrgX = OrgX + ViewOrigX;
	int32 AbsOrgY = OrgY + ViewOrigY;

	//if the origin is already in the safe region, no need to adjust it any further.
	int32 OrgXPad = AbsOrgX >= SafeZonePadX ? 0 : SafeZonePadX - AbsOrgX;
	int32 OrgYPad = AbsOrgY >= SafeZonePadY ? 0 : SafeZonePadY - AbsOrgY;	

	//get the canvas's extents on the screen
	int32 AbsMaxX = AbsOrgX + SizeX;
	int32 AbsMaxY = AbsOrgY + SizeY;

	//get the distance from the canvas edge to the screen border
	int32 DistToXBorder = CachedDisplayWidth - AbsMaxX;
	int32 DistToYBorder = CachedDisplayHeight - AbsMaxY;

	//compute how much more we must move the canvas away from the border to meet safe zone requirements.
	int32 SizeXPad = AbsMaxX <= CachedDisplayWidth - SafeZonePadX ? 0 : SafeZonePadX - DistToXBorder;
	int32 SizeYPad = AbsMaxY <= CachedDisplayHeight - SafeZonePadY ? 0 : SafeZonePadY - DistToYBorder;

	int32 OrigClipOffsetX = SizeX - ClipX;
	int32 OrigClipOffsetY = SizeY - ClipY;

	//store the original size so we can restore it if the user wants to remove the safezone transform
	UnsafeSizeX = SizeX;
	UnsafeSizeY = SizeY;

	//set the size to the virtual safe size.
	SizeX = SizeX - SizeXPad - OrgXPad;
	SizeY = SizeY - SizeYPad - OrgYPad;

	//adjust clip to be within new bounds by the same absolute amount.  we aren't trying to preserve ratios at the moment.
	ClipX = SizeX - OrigClipOffsetX;
	ClipY = SizeY - OrigClipOffsetY;
	


	Canvas->PushRelativeTransform(FTranslationMatrix(FVector(OrgXPad, OrgYPad, 0)));
}

void UCanvas::PopSafeZoneTransform()
{
	// if there is no required safezone padding, then we can bail on the whole operation.
	// this is also basically punting on a bug we have on PC where GetDisplayMetrics isn't the right thing
	// to do, as it gives the monitor size, not the window size which we actually want.
	if (SafeZonePadX == 0 && SafeZonePadY == 0)
	{
		return;
	}

	Canvas->PopTransform();	

	//put our size and clip back to what they were before applying the safezone.
	int32 OrigClipOffsetX = SizeX - ClipX;
	int32 OrigClipOffsetY = SizeY - ClipY;

	SizeX = UnsafeSizeX;
	SizeY = UnsafeSizeY;
	
	ClipX = SizeX - OrigClipOffsetX;
	ClipY = SizeY - OrigClipOffsetY;
}

void UCanvas::UpdateSafeZoneData()
{
	if(GEngine && GEngine->IsStereoscopic3D())
	{
		FVector2D SafeRegionPercentage = GEngine->StereoRenderingDevice->GetTextSafeRegionBounds();

		CachedDisplayWidth = UnsafeSizeX;
		CachedDisplayHeight = UnsafeSizeY;

		SafeZonePadX = (CachedDisplayWidth - (CachedDisplayWidth * SafeRegionPercentage.X))/2.f;
		SafeZonePadY = (CachedDisplayHeight - (CachedDisplayHeight * SafeRegionPercentage.Y))/2.f;
	}
	else if(FSlateApplication::IsInitialized())
	{
		FDisplayMetrics DisplayMetrics;

		FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);

		SafeZonePadX = FMath::CeilToInt(DisplayMetrics.TitleSafePaddingSize.X);
		SafeZonePadY = FMath::CeilToInt(DisplayMetrics.TitleSafePaddingSize.Y);

		CachedDisplayWidth = DisplayMetrics.PrimaryDisplayWidth;
		CachedDisplayHeight = DisplayMetrics.PrimaryDisplayHeight;
	}

}

void UCanvas::UpdateAllCanvasSafeZoneData()
{
	for (TObjectIterator<UCanvas> It; It; ++It)
	{
		It->UpdateSafeZoneData();
	}
}


void UCanvas::SetStereoDepth(uint32 depth)
{
	Canvas->SetStereoDepth(depth);
}

void UCanvas::Update()
{
	// Reset canvas params.
	Reset();

	// Copy size parameters from viewport.
	ClipX = SizeX;
	ClipY = SizeY;

	if (Canvas)
	{
		Canvas->SetParentCanvasSize(FIntPoint(SizeX, SizeY));
	}
}

/*-----------------------------------------------------------------------------
	UCanvas scaled sprites.
-----------------------------------------------------------------------------*/

/** Set DrawColor with a FLinearColor and optional opacity override */
void UCanvas::SetLinearDrawColor(FLinearColor InColor, float OpacityOverride)
{
	DrawColor = InColor.ToFColor(true);

	if( OpacityOverride != -1 )
	{
		DrawColor.A = FMath::Clamp(FMath::TruncToInt(OpacityOverride * 255.0f),0,255);
	}
}

void UCanvas::SetDrawColor(uint8 R, uint8 G, uint8 B, uint8 A)
{
	DrawColor.R = R;
	DrawColor.G = G;
	DrawColor.B = B;
	DrawColor.A = A;
}

void UCanvas::SetDrawColor(FColor const& C)
{
	DrawColor = C;
}

/**
 * Translate EBlendMode into ESimpleElementBlendMode used by tiles
 * 
 * @param BlendMode Normal Unreal blend mode enum
 *
 * @return simple element rendering blend mode enum
 */
ESimpleElementBlendMode FCanvas::BlendToSimpleElementBlend(EBlendMode BlendMode)
{
	switch (BlendMode)
	{
		case BLEND_Opaque:
			return SE_BLEND_Opaque;
		case BLEND_Masked:
			return SE_BLEND_Masked;
		case BLEND_Additive:
			return SE_BLEND_Additive;
		case BLEND_Modulate:
			return SE_BLEND_Modulate;
		case BLEND_AlphaComposite:
			return SE_BLEND_AlphaComposite;
		case BLEND_Translucent:
		default:
			return SE_BLEND_Translucent;
	};
}


void UCanvas::DrawTile( UTexture* Tex, float X, float Y, float XL, float YL, float U, float V, float UL, float VL, EBlendMode BlendMode )
{
	if ( !Tex ) 
	{
		return;
	}
	float MyClipX = OrgX + ClipX;
	float MyClipY = OrgY + ClipY;
	float w = X + XL > MyClipX ? MyClipX - X : XL;
	float h = Y + YL > MyClipY ? MyClipY - Y : YL;
	if (XL > 0.f &&
		YL > 0.f)
	{
		// here we use the original size of the texture, not the current size (for instance, 
		// PVRTC textures, on some platforms anyway, need to be square), but the script code
		// was written using 0..TexSize coordinates, not 0..1, so to make the 0..1 coords
		// we divide by what the texture was when the script code was written
		// TEXTURE_TODO: This info is no longer stored outside of the Editor. Needed?
		float TexSurfaceWidth= Tex->GetSurfaceWidth();
		float TexSurfaceHeight = Tex->GetSurfaceHeight();

		FCanvasTileItem TileItem( FVector2D( X, Y ), Tex->Resource,  FVector2D( w, h ),  
			FVector2D(U / TexSurfaceWidth, V / TexSurfaceHeight),
			FVector2D(U / TexSurfaceWidth + UL / TexSurfaceWidth * w / XL, V / TexSurfaceHeight + VL / TexSurfaceHeight * h / YL),
			DrawColor );
		TileItem.BlendMode = FCanvas::BlendToSimpleElementBlend( BlendMode );
		Canvas->DrawItem( TileItem );	
	}
}


void UCanvas::ClippedStrLen( const UFont* Font, float ScaleX, float ScaleY, int32& XL, int32& YL, const TCHAR* Text )
{
	XL = 0;
	YL = 0;
	if (Font != NULL)
	{
		FTextSizingParameters Parameters(Font,ScaleX,ScaleY);
		CanvasStringSize(Parameters, Text);

		XL = FMath::TruncToInt(Parameters.DrawXL);
		YL = FMath::TruncToInt(Parameters.DrawYL);
	}
}

void VARARGS UCanvas::WrappedStrLenf( const UFont* Font, float ScaleX, float ScaleY, int32& XL, int32& YL, const TCHAR* Fmt, ... ) 
{
	TCHAR Text[4096];
	GET_VARARGS( Text, ARRAY_COUNT(Text), ARRAY_COUNT(Text)-1, Fmt, Fmt );

	FFontRenderInfo Info;
	WrappedPrint( false, 0.0f, 0.0f, XL, YL, Font, ScaleX, ScaleY, false, false, Text, Info ); 
}

float UCanvas::DrawText(const UFont* InFont, const FText& InText, float X, float Y, float XScale, float YScale, const FFontRenderInfo& RenderInfo)
{
	ensure(InFont);
	int32		XL		= 0;
	int32		YL		= 0; 
	// need this call in any case to update YL and XL - one of them will be needed anyway
	WrappedPrint(RenderInfo.bClipText == false, X, Y, XL, YL, InFont, XScale, YScale, bCenterX, bCenterY, *InText.ToString(), RenderInfo);

	if (RenderInfo.bClipText)
	{
		FCanvasTextItem TextItem(FVector2D(FMath::TruncToFloat(OrgX + X), FMath::TruncToFloat(OrgY + Y)), InText, InFont, DrawColor);
		TextItem.Scale = FVector2D( XScale, YScale ), 
		TextItem.BlendMode = SE_BLEND_Translucent;
		TextItem.FontRenderInfo = RenderInfo;
		Canvas->DrawItem( TextItem );	
	}

	return (float)YL;
}

float UCanvas::DrawText(const UFont* InFont, const FString& InText, float X, float Y, float XScale, float YScale, const FFontRenderInfo& RenderInfo)
{
	return DrawText(InFont, FText::FromString(InText), X, Y, XScale, YScale, RenderInfo);
}

int32 UCanvas::WrappedPrint(bool Draw, float X, float Y, int32& out_XL, int32& out_YL, const UFont* Font, float ScaleX, float ScaleY, bool bCenterTextX, bool bCenterTextY, const TCHAR* Text, const FFontRenderInfo& RenderInfo) 
{
	if (ClipX < 0 || ClipY < 0)
	{
		return 0;
	}
	if (Font == NULL)
	{
		UE_LOG(LogCanvas, Warning, TEXT("UCanvas::WrappedPrint() called with a NULL Font!"));
		return 0;
	}

	FTextSizingParameters RenderParms(0.f, 0.f, ClipX - (OrgX + X), 0.f, Font );
	RenderParms.Scaling.X = ScaleX;
	RenderParms.Scaling.Y = ScaleY;
	TArray<FWrappedStringElement> WrappedStrings;
	WrapString(RenderParms, 0, Text, WrappedStrings);

	float DrawX = OrgX + X;
	float DrawY = OrgY + Y;
	if (bCenterTextY)
	{
		// Center text about DrawY
		float MeasuredHeight = 0.f;
		for (const FWrappedStringElement& WrappedString : WrappedStrings)
		{
			MeasuredHeight += WrappedString.LineExtent.Y;
		}
		DrawY -= (MeasuredHeight * 0.5f);
	}

	float XL = 0.f;
	float YL = 0.f;
	FCanvasTextItem TextItem( FVector2D::ZeroVector, FText::GetEmpty(), Font, DrawColor );
	TextItem.Scale = FVector2D( ScaleX, ScaleY );
	TextItem.BlendMode = SE_BLEND_Translucent;
	TextItem.FontRenderInfo = RenderInfo;
	for (const FWrappedStringElement& WrappedString : WrappedStrings)
	{
		float LineDrawX = DrawX;
		float LineDrawY = DrawY;

		if (bCenterTextX)
		{
			// Center text about DrawX
			LineDrawX -= (WrappedString.LineExtent.X * 0.5f);
		}

		float LineXL = 0.0f;
		if( Draw )
		{
			TextItem.Text = FText::FromString(WrappedString.Value);
			Canvas->DrawItem( TextItem, LineDrawX, LineDrawY );
			LineXL = TextItem.DrawnSize.X;
		}
		else
		{
			int32 TempX;
			int32 TempY;
			ClippedStrLen(Font, ScaleX, ScaleY, TempX, TempY, *WrappedString.Value);
			LineXL = TempX;
		}
		XL = FMath::Max<float>(XL, LineXL);
		DrawY += Font->GetMaxCharHeight() * ScaleY;
		YL += Font->GetMaxCharHeight() * ScaleY;
	}

	out_XL = FMath::TruncToInt(XL);
	out_YL = FMath::TruncToInt(YL);
	return WrappedStrings.Num();
}

void UCanvas::StrLen( const UFont* InFont, const FString& InText, float& XL, float& YL)
{
	if (InFont == NULL)
	{
		UE_LOG(LogScript, Warning, TEXT("No Font"));
	}
	else
	{
		FTextSizingParameters Parameters(InFont,1.0f,1.0f);
		UCanvas::CanvasStringSize(Parameters, *InText);

		XL = Parameters.DrawXL;
		YL = Parameters.DrawYL;
	}
}

void UCanvas::TextSize(const UFont* InFont, const FString& InText, float& XL, float& YL, float ScaleX, float ScaleY)
{
	int32 XLi, YLi;

	if (!InFont)
	{
		UE_LOG(LogCanvas, Log,  TEXT("TextSize: No font") ); 
		return;
	}

	ClippedStrLen(InFont, ScaleX, ScaleY, XLi, YLi, *InText);

	XL = XLi;
	YL = YLi;
}

FVector UCanvas::Project(FVector Location) const
{
	FPlane V(0,0,0,0);

	if (SceneView != NULL)
	{
		Location.DiagnosticCheckNaN();
		V = SceneView->Project(Location);
	}

	FVector resultVec(V);
	resultVec.X = (ClipX/2.f) + (resultVec.X*(ClipX/2.f));
	resultVec.Y *= -1.f * GProjectionSignY;
	resultVec.Y = (ClipY/2.f) + (resultVec.Y*(ClipY/2.f));

	// if behind the screen, clamp depth to the screen
	if (V.W <= 0.0f)
	{
		resultVec.Z = 0.0f;
	}
	return resultVec;
}

void UCanvas::Deproject(FVector2D ScreenPos, /*out*/ FVector& WorldOrigin, /*out*/ FVector& WorldDirection) const
{
	if (SceneView != NULL)
	{
		SceneView->DeprojectFVector2D(ScreenPos, /*out*/ WorldOrigin, /*out*/ WorldDirection);
	}
}


FFontRenderInfo UCanvas::CreateFontRenderInfo(bool bClipText, bool bEnableShadow, FLinearColor GlowColor, FVector2D GlowOuterRadius, FVector2D GlowInnerRadius)
{
	FFontRenderInfo Result;

	Result.bClipText = bClipText;
	Result.bEnableShadow = bEnableShadow;
	Result.GlowInfo.bEnableGlow = (GlowColor.A != 0.0);
	if (Result.GlowInfo.bEnableGlow)
	{
		Result.GlowInfo.GlowOuterRadius = GlowOuterRadius;
		Result.GlowInfo.GlowInnerRadius = GlowInnerRadius;
	}
	return Result;
}

void UCanvas::Reset(bool bKeepOrigin)
{
	if ( !bKeepOrigin )
	{
		OrgX        = GetDefault<UCanvas>(GetClass())->OrgX;
		OrgY        = GetDefault<UCanvas>(GetClass())->OrgY;
	}
	DrawColor   = GetDefault<UCanvas>(GetClass())->DrawColor;
	bCenterX    = false;
	bCenterY    = false;
	bNoSmooth   = false;
}

void UCanvas::SetClip(float X, float Y)
{
	ClipX = X;
	ClipY = Y;
}

FCanvasIcon UCanvas::MakeIcon(UTexture* Texture, float U, float V, float UL, float VL)
{
	FCanvasIcon Icon;
	if (Texture != NULL)
	{
		Icon.Texture = Texture;
		Icon.U = U;
		Icon.V = V;
		Icon.UL = (UL != 0.f ? UL : Texture->GetSurfaceWidth());
		Icon.VL = (VL != 0.f ? VL : Texture->GetSurfaceHeight());
	}
	return Icon;
}


void UCanvas::DrawScaledIcon(FCanvasIcon Icon, float X, float Y, FVector Scale)
{
	if (Icon.Texture != NULL)
	{
		// verify properties are valid
		if (Scale.Size() <= 0.f)
		{
			Scale.X = 1.f;
			Scale.Y = 1.f;
		}
		if (Icon.UL == 0.f)
		{
			Icon.UL = Icon.Texture->GetSurfaceWidth();
		}
		if (Icon.VL == 0.f)
		{
			Icon.VL = Icon.Texture->GetSurfaceHeight();
		}

		// and draw the texture
		DrawTile(Icon.Texture, X, Y, FMath::Abs(Icon.UL) * Scale.X, FMath::Abs(Icon.VL) * Scale.Y, Icon.U, Icon.V, Icon.UL, Icon.VL);
	}
}


void UCanvas::DrawIcon(FCanvasIcon Icon, float X, float Y, float Scale)
{
	if (Icon.Texture != NULL)
	{
		// verify properties are valid
		if (Scale <= 0.f)
		{
			Scale = 1.f;
		}
		if (Icon.UL == 0.f)
		{
			Icon.UL = Icon.Texture->GetSurfaceWidth();
		}
		if (Icon.VL == 0.f)
		{
			Icon.VL = Icon.Texture->GetSurfaceHeight();
		}
		
		// and draw the texture
		DrawTile(Icon.Texture, X, Y, FMath::Abs(Icon.UL) * Scale, FMath::Abs(Icon.VL) * Scale, Icon.U, Icon.V, Icon.UL, Icon.VL );
	}
}


void UCanvas::DrawDebugGraph(const FString& Title, float ValueX, float ValueY, float UL_X, float UL_Y, float W, float H, FVector2D RangeX, FVector2D RangeY)
{
	static const int32 GRAPH_ICONSIZE = 8;

	const int32 X = UL_X + ((RangeX.Y == RangeX.X) ? RangeX.X : (ValueX - RangeX.X) / (RangeX.Y - RangeX.X)) * W - GRAPH_ICONSIZE/2;
	const int32 Y = UL_Y + ((RangeY.Y == RangeY.X) ? RangeY.X : (ValueY - RangeY.X) / (RangeY.Y - RangeY.X)) * H - GRAPH_ICONSIZE/2;
	FCanvasBoxItem Box( FVector2D(  UL_X, UL_Y ), FVector2D(  W, H ) );
	DrawItem( Box );

	FCanvasTileItem Tile( FVector2D( X, Y ), GWhiteTexture, FVector2D( GRAPH_ICONSIZE, GRAPH_ICONSIZE ), FLinearColor::Yellow );
	DrawItem( Tile );

	FCanvasLineItem Line( FVector2D( UL_X, Y ), FVector2D(  UL_X+W, Y ) );
	Line.SetColor( FLinearColor( 0.5f, 0.5f, 0, 0.5f ) );
	DrawItem( Line );
	Line.Origin = FVector( X, UL_Y, 0.0f );
	Line.SetEndPos( FVector2D(  X, UL_Y+H ) );
	DrawItem( Line );

	FString ValText;		
	ValText = FString::Printf(TEXT("%f"), ValueX);
	FCanvasTextItem Text( FVector2D( X, UL_Y+H+16.0f ), FText::FromString( ValText ), GEngine->GetSmallFont(), FLinearColor::Yellow );
	DrawItem( Text );

	ValText = FString::Printf(TEXT("%f"), ValueY);
	Text.Text = FText::FromString( ValText );
	DrawItem( Text, FVector2D( UL_X+W+8.0f, Y ) );

	// title
	if (!Title.IsEmpty())
	{
		Text.Text = FText::FromString( Title );
		DrawItem( Text, FVector2D( UL_X, UL_Y-16.0f) );
	}	
}

void UCanvas::GetCenter(float& outX, float& outY) const
{
	outX = OrgX + ClipX/2;
	outY = OrgY + ClipY/2;
}


void UCanvas::DrawItem( class FCanvasItem& Item )
{
	Canvas->DrawItem( Item );
}

void UCanvas::DrawItem( class FCanvasItem& Item , const FVector2D& InPosition )
{
	Canvas->DrawItem( Item, InPosition );
}
	
void UCanvas::DrawItem( class FCanvasItem& Item, float X, float Y )
{
	Canvas->DrawItem( Item, X, Y  );
}

bool FCanvas::GetOrthoProjectionMatrices(float InDrawDepth, FMatrix OutOrthoProjection[2])
{
	bool rv = false;

	if (bStereoRendering)
	{
		rv = true;
		const int32 RTWidth = RenderTarget->GetSizeXY().X;
		const int32 RTHeight = RenderTarget->GetSizeXY().Y;
		if (RTWidth != CachedRTWidth || RTHeight != CachedRTHeight || InDrawDepth != CachedDrawDepth)
		{
			rv = false;
			if (GEngine && GEngine->StereoRenderingDevice.IsValid())
			{
				GEngine->StereoRenderingDevice->GetOrthoProjection(RTWidth, RTHeight, InDrawDepth, CachedOrthoProjection);
				CachedRTWidth = RTWidth;
				CachedRTHeight= RTHeight;
				CachedDrawDepth=InDrawDepth;
				rv = true;
			}
		}
		OutOrthoProjection[0] = CachedOrthoProjection[0];
		OutOrthoProjection[1] = CachedOrthoProjection[1];
	}
	return rv;
}

void FCanvas::DrawItem(FCanvasItem& Item)
{
	const uint32 DrawDepth = Item.StereoDepth ? Item.StereoDepth : StereoDepth;
	FMatrix OrthoProjection[2];
	if (GetOrthoProjectionMatrices(DrawDepth, OrthoProjection))
	{
		//left eye
		PushRelativeTransform(OrthoProjection[0]); //apply projection matrix
		Item.Draw(this);
		PopTransform();
		if (!bUseInternalTexture)
		{
			//right eye
			PushRelativeTransform(OrthoProjection[1]);
			Item.Draw(this);
			PopTransform();
		}
	}
	else
	{
		Item.Draw(this);
	}

	if (DrawMode == CDM_ImmediateDrawing)
	{
		Flush_GameThread();
	}
}

void FCanvas::DrawItem(FCanvasItem& Item, const FVector2D& InPosition)
{
	uint32 DrawDepth = Item.StereoDepth ? Item.StereoDepth : StereoDepth;
	FMatrix OrthoProjection[2];
	if (GetOrthoProjectionMatrices(DrawDepth, OrthoProjection))
	{
		//left eye
		PushRelativeTransform(OrthoProjection[0]); //apply projection matrix
		Item.Draw(this, InPosition);
		PopTransform();
		if (!bUseInternalTexture)
		{
			//right eye
			PushRelativeTransform(OrthoProjection[1]);
			Item.Draw(this, InPosition);
			PopTransform();
		}
	}
	else
	{
		Item.Draw(this , InPosition);
	}

	if (DrawMode == CDM_ImmediateDrawing)
	{
		Flush_GameThread();
	}
}

void FCanvas::DrawItem(FCanvasItem& Item, float X, float Y)
{
	uint32 DrawDepth = Item.StereoDepth ? Item.StereoDepth : StereoDepth;
	FMatrix OrthoProjection[2];
	if (GetOrthoProjectionMatrices(DrawDepth, OrthoProjection))
	{
		//left eye
		PushRelativeTransform(OrthoProjection[0]); //apply projection matrix
		Item.Draw(this, X, Y);
		PopTransform();
		if (!bUseInternalTexture)
		{
			//right eye
			PushRelativeTransform(OrthoProjection[1]);
			Item.Draw(this, X, Y);
			PopTransform();
		}
	}
	else
	{
		Item.Draw(this, X, Y);
	}

	if (DrawMode == CDM_ImmediateDrawing)
	{
		Flush_GameThread();
	}
}

void UCanvas::SetView(FSceneView* InView)
{
	SceneView = InView;
	if (InView)
	{
		if (GEngine->StereoRenderingDevice.IsValid() && InView->StereoPass != eSSP_FULL)
		{
			GEngine->StereoRenderingDevice->InitCanvasFromView(InView, this);
		}
		else
		{
			ViewProjectionMatrix = InView->ViewMatrices.GetViewProjectionMatrix();
		}
	}
	else
	{
		ViewProjectionMatrix.SetIdentity();
	}
}

TWeakObjectPtr<class UReporterGraph> UCanvas::GetReporterGraph()
{
	if (!ReporterGraph)
	{
		ReporterGraph = NewObject<UReporterGraph>(this);
	}

	return ReporterGraph;
}

void UCanvas::K2_DrawLine(FVector2D ScreenPositionA, FVector2D ScreenPositionB, float Thickness, FLinearColor RenderColor)
{
	if (FMath::Square(ScreenPositionB.X - ScreenPositionA.X) + FMath::Square(ScreenPositionB.Y - ScreenPositionA.Y) > 0 && Canvas)
	{
		FCanvasLineItem LineItem(ScreenPositionA, ScreenPositionB);
		LineItem.LineThickness = Thickness;
		LineItem.SetColor(RenderColor);
		DrawItem(LineItem);
	}
}

void UCanvas::K2_DrawTexture(UTexture* RenderTexture, FVector2D ScreenPosition, FVector2D ScreenSize, FVector2D CoordinatePosition, FVector2D CoordinateSize, FLinearColor RenderColor, EBlendMode BlendMode, float Rotation, FVector2D PivotPoint)
{
	if (ScreenSize.X > 0.0f && ScreenSize.Y > 0.0f && Canvas)
	{
		FTexture* RenderTextureResource = (RenderTexture) ? RenderTexture->Resource : GWhiteTexture;
		FCanvasTileItem TileItem(ScreenPosition, RenderTextureResource, ScreenSize, CoordinatePosition, CoordinatePosition + CoordinateSize, RenderColor);
		TileItem.Rotation = FRotator(0, Rotation, 0);
		TileItem.PivotPoint = PivotPoint;
		TileItem.BlendMode = FCanvas::BlendToSimpleElementBlend(BlendMode);
		DrawItem(TileItem);
	}
}

void UCanvas::K2_DrawMaterial(UMaterialInterface* RenderMaterial, FVector2D ScreenPosition, FVector2D ScreenSize, FVector2D CoordinatePosition, FVector2D CoordinateSize, float Rotation, FVector2D PivotPoint)
{
	if (RenderMaterial 
		&& ScreenSize.X > 0.0f 
		&& ScreenSize.Y > 0.0f 
		// Canvas can be NULL if the user tried to draw after EndDrawCanvasToRenderTarget
		&& Canvas)
	{
		FCanvasTileItem TileItem(ScreenPosition, RenderMaterial->GetRenderProxy(0), ScreenSize, CoordinatePosition, CoordinatePosition + CoordinateSize);
		TileItem.Rotation = FRotator(0, Rotation, 0);
		TileItem.PivotPoint = PivotPoint;
		TileItem.SetColor(DrawColor);
		DrawItem(TileItem);
	}
}

void UCanvas::K2_DrawText(UFont* RenderFont, const FString& RenderText, FVector2D ScreenPosition, FLinearColor RenderColor, float Kerning, FLinearColor ShadowColor, FVector2D ShadowOffset, bool bCentreX, bool bCentreY, bool bOutlined, FLinearColor OutlineColor)
{
	if (!RenderText.IsEmpty() && Canvas)
	{
		FCanvasTextItem TextItem(ScreenPosition, FText::FromString(RenderText), RenderFont, RenderColor);
		TextItem.HorizSpacingAdjust = Kerning;
		TextItem.ShadowColor = ShadowColor;
		TextItem.ShadowOffset = ShadowOffset;
		TextItem.bCentreX = bCentreX;
		TextItem.bCentreY = bCentreY;
		TextItem.bOutlined = bOutlined;
		TextItem.OutlineColor = OutlineColor;
		DrawItem(TextItem);
	}
}

void UCanvas::K2_DrawBorder(UTexture* BorderTexture, UTexture* BackgroundTexture, UTexture* LeftBorderTexture, UTexture* RightBorderTexture, UTexture* TopBorderTexture, UTexture* BottomBorderTexture, FVector2D ScreenPosition, FVector2D ScreenSize, FVector2D CoordinatePosition, FVector2D CoordinateSize, FLinearColor RenderColor, FVector2D BorderScale, FVector2D BackgroundScale, float Rotation, FVector2D PivotPoint, FVector2D CornerSize)
{
	if (ScreenSize.X > 0.0f && ScreenSize.Y > 0.0f && BorderTexture && BackgroundTexture && LeftBorderTexture && RightBorderTexture && TopBorderTexture && BottomBorderTexture && Canvas)
	{
		FCanvasBorderItem BorderItem(ScreenPosition, BorderTexture->Resource, BackgroundTexture->Resource, LeftBorderTexture->Resource, RightBorderTexture->Resource, TopBorderTexture->Resource, BottomBorderTexture->Resource, ScreenSize, RenderColor);
		BorderItem.BorderScale = BorderScale;
		BorderItem.BackgroundScale = BackgroundScale;
		BorderItem.BorderUV0 = CoordinatePosition;
		BorderItem.BorderUV1 = CoordinatePosition + CoordinateSize;
		BorderItem.Rotation = FRotator(0, Rotation, 0);
		BorderItem.PivotPoint = PivotPoint;
		BorderItem.CornerSize = CornerSize;
		DrawItem(BorderItem);
	}
}

void UCanvas::K2_DrawBox(FVector2D ScreenPosition, FVector2D ScreenSize, float Thickness)
{
	if (ScreenSize.X > 0.0f && ScreenSize.Y > 0.0f && Canvas)
	{
		FCanvasBoxItem BoxItem(ScreenPosition, ScreenSize);
		BoxItem.LineThickness = Thickness;
		DrawItem(BoxItem);
	}
}

void UCanvas::K2_DrawTriangle(UTexture* RenderTexture, TArray<FCanvasUVTri> Triangles)
{
	if (Triangles.Num() > 0 && Canvas)
	{
		FCanvasTriangleItem TriangleItem(FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, (RenderTexture) ? RenderTexture->Resource : GWhiteTexture);
		TriangleItem.TriangleList = MoveTemp(Triangles);
		DrawItem(TriangleItem);
	}
}

void UCanvas::K2_DrawMaterialTriangle(UMaterialInterface* RenderMaterial, TArray<FCanvasUVTri> Triangles)
{
	if (RenderMaterial && Triangles.Num() > 0 && Canvas)
	{
		FCanvasTriangleItem TriangleItem(FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, NULL);
		TriangleItem.MaterialRenderProxy = RenderMaterial->GetRenderProxy(0);
		TriangleItem.TriangleList = MoveTemp(Triangles);
		DrawItem(TriangleItem);
	}
}

void UCanvas::K2_DrawPolygon(UTexture* RenderTexture, FVector2D ScreenPosition, FVector2D Radius, int32 NumberOfSides, FLinearColor RenderColor)
{
	if (Radius.X > 0.0f && Radius.Y > 0.0f && NumberOfSides >= 3 && Canvas)
	{
		FCanvasNGonItem NGonItem(ScreenPosition, Radius, NumberOfSides, (RenderTexture) ? RenderTexture->Resource : GWhiteTexture, RenderColor);
		DrawItem(NGonItem);
	}
}

FVector UCanvas::K2_Project(FVector WorldLocation)
{
	return Project(WorldLocation);
}

void UCanvas::K2_Deproject(FVector2D ScreenPosition, FVector& WorldOrigin, FVector& WorldDirection)
{
	Deproject(ScreenPosition, WorldOrigin, WorldDirection);
}

FVector2D UCanvas::K2_StrLen(UFont* RenderFont, const FString& RenderText)
{
	if (!RenderText.IsEmpty())
	{
		FVector2D OutTextSize;
		StrLen(RenderFont, RenderText, OutTextSize.X, OutTextSize.Y);
		return OutTextSize;
	}

	return FVector2D::ZeroVector;
}

FVector2D UCanvas::K2_TextSize(UFont* RenderFont, const FString& RenderText, FVector2D Scale)
{
	if (!RenderText.IsEmpty())
	{
		FVector2D OutTextSize;
		TextSize(RenderFont, RenderText, OutTextSize.X, OutTextSize.Y, Scale.X, Scale.Y);
		return OutTextSize;
	}
	
	return FVector2D::ZeroVector;
}

void FDisplayDebugManager::DrawString(const FString& InDebugString, const float& OptionalXOffset)
{
	if (Canvas)
	{
		const float TextScale = GetTextScale();
		DebugTextItem.Scale = FVector2D(TextScale, TextScale);

		DebugTextItem.Text = FText::FromString(InDebugString);
		Canvas->DrawItem(DebugTextItem, FVector2D(CurrentPos.X + OptionalXOffset, CurrentPos.Y));

		NextColumXPos = FMath::Max(NextColumXPos, CurrentPos.X + OptionalXOffset + DebugTextItem.DrawnSize.X);
		CurrentPos.Y += FMath::Max(GetYStep(), DebugTextItem.DrawnSize.Y);
		AddColumnIfNeeded();
	}
}

float FDisplayDebugManager::GetTextScale() const
{
	return Canvas ? FMath::Max(Canvas->SizeX / 1920.f, 1.f) : 1.f;
}

void FDisplayDebugManager::AddColumnIfNeeded()
{
	if (Canvas)
	{
		const float YStep = GetYStep();
		if ((CurrentPos.Y + YStep) > Canvas->SizeY)
		{
			CurrentPos.Y = InitialPos.Y;
			CurrentPos.X = NextColumXPos + YStep * 2.f;
		}
	}
}
