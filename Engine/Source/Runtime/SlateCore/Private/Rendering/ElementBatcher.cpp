// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Rendering/ElementBatcher.h"
#include "Fonts/SlateFontInfo.h"
#include "Fonts/FontCache.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingPolicy.h"
#include "Stats/SlateStats.h"
#include "Widgets/SWindow.h"
#include "HAL/IConsoleManager.h"

DECLARE_CYCLE_STAT(TEXT("Find Batch For Element Time"), STAT_SlateFindBatchForElement, STATGROUP_SlateVerbose);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Elements (Prebatch)"), STAT_SlateNumPrebatchElements, STATGROUP_Slate);

DECLARE_CYCLE_STAT(TEXT("Add Elements Time"), STAT_SlateAddElements, STATGROUP_Slate);

SLATE_DECLARE_CYCLE_COUNTER(GSlateAddElements, "Add Elements");
SLATE_DECLARE_CYCLE_COUNTER(GSlateFindBatchTime, "FindElementForBatch");
SLATE_DECLARE_CYCLE_COUNTER(GSlateFillBatchBuffers, "FillBatchBuffers");

DECLARE_DWORD_COUNTER_STAT(TEXT("Elements (Box)"), STAT_SlateNumBoxElements, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Elements (Text)"), STAT_SlateNumTextElements, STATGROUP_Slate);

FSlateElementBatcher::FSlateElementBatcher( TSharedRef<FSlateRenderingPolicy> InRenderingPolicy )
	: BatchData( nullptr )
	, DrawLayer( nullptr )
	, RenderingPolicy( &InRenderingPolicy.Get() )
	, NumDrawnBatchesStat(0)
	, NumPostProcessPasses(0)
	, PixelCenterOffset( InRenderingPolicy->GetPixelCenterOffset() )
	, bSRGBVertexColor( !InRenderingPolicy->IsVertexColorInLinearSpace() )
	, bRequiresVsync(false)
{
}

FSlateElementBatcher::~FSlateElementBatcher()
{
}

void FSlateElementBatcher::AddElements(FSlateWindowElementList& WindowElementList)
{
	SLATE_CYCLE_COUNTER_SCOPE(GSlateAddElements);
	FPlatformMisc::BeginNamedEvent(FColor::Magenta, "Slate::AddElements");

	SCOPE_CYCLE_COUNTER(STAT_SlateAddElements);

	NumDrawnBatchesStat = NumDrawnBoxesStat = NumDrawnTextsStat = 0;

	BatchData = &WindowElementList.GetBatchData();
	DrawLayer = &WindowElementList.GetRootDrawLayer();
	
	FVector2D ViewportSize = WindowElementList.GetWindow()->GetViewportSize();
	
	AddElements(WindowElementList.GetRootDrawLayer().DrawElements, ViewportSize);

	TMap< TSharedPtr<FSlateDrawLayerHandle, ESPMode::ThreadSafe>, TSharedPtr<FSlateDrawLayer> >& DrawLayers = WindowElementList.GetChildDrawLayers();
	for ( auto& Entry : DrawLayers )
	{
		DrawLayer = Entry.Value.Get();
		AddElements(Entry.Value.Get()->DrawElements, ViewportSize);
	}

	BatchData->CopyClippingStates(WindowElementList.ClippingManager.GetClippingStates());

	// Done with the element list
	BatchData = nullptr;
	DrawLayer = nullptr;

	SET_DWORD_STAT(STAT_SlateNumPrebatchElements, NumDrawnBatchesStat);
	SET_DWORD_STAT(STAT_SlateNumBoxElements, NumDrawnBoxesStat);
	SET_DWORD_STAT(STAT_SlateNumTextElements, NumDrawnTextsStat);

	FPlatformMisc::EndNamedEvent();
}

void FSlateElementBatcher::AddElements(const TArray<FSlateDrawElement>& DrawElements, const FVector2D& ViewportSize)
{
	// This stuff is just for the counters. Could be scoped by an #ifdef if necessary.
	static_assert(
		FSlateDrawElement::EElementType::ET_Box == 0 &&
		FSlateDrawElement::EElementType::ET_DebugQuad == 1 &&
		FSlateDrawElement::EElementType::ET_Text == 2 &&
		FSlateDrawElement::EElementType::ET_ShapedText == 3 &&
		FSlateDrawElement::EElementType::ET_Spline == 4 &&
		FSlateDrawElement::EElementType::ET_Line == 5 &&
		FSlateDrawElement::EElementType::ET_Gradient == 6 &&
		FSlateDrawElement::EElementType::ET_Viewport == 7 &&
		FSlateDrawElement::EElementType::ET_Border == 8 &&
		FSlateDrawElement::EElementType::ET_Custom == 9 &&
		FSlateDrawElement::EElementType::ET_CustomVerts == 10 &&
		FSlateDrawElement::EElementType::ET_CachedBuffer == 11 &&
		FSlateDrawElement::EElementType::ET_Layer == 12 &&
		FSlateDrawElement::EElementType::ET_PostProcessPass == 13 &&
		FSlateDrawElement::EElementType::ET_Count == 14,
		"If FSlateDrawElement::EElementType is modified, this array must be made to match." );

	static FName ElementFNames[] =
	{
		FName(TEXT("Box")),
		FName(TEXT("DebugQuad")),
		FName(TEXT("Text")),
		FName(TEXT("ShapedText")),
		FName(TEXT("Spline")),
		FName(TEXT("Line")),
		FName(TEXT("Gradient")),
		FName(TEXT("Viewport")),
		FName(TEXT("Border")),
		FName(TEXT("Custom")),
		FName(TEXT("CustomVerts")),
		FName(TEXT("CachedBuffer")),
		FName(TEXT("Layer")),
		FName(TEXT("FXPass")),
	};

	checkSlow(DrawLayer);

	for ( int32 DrawElementIndex = 0; DrawElementIndex < DrawElements.Num(); ++DrawElementIndex )
	{
		const FSlateDrawElement& DrawElement = DrawElements[DrawElementIndex];

		++NumDrawnBatchesStat;

		const bool EnablePixelSnapping = !EnumHasAllFlags(DrawElement.GetDrawEffects(), ESlateDrawEffect::NoPixelSnapping);

		// time just the adding of the element. The clipping stuff will be counted in exclusive time for the non-typed timer.
		SLATE_CYCLE_COUNTER_SCOPE_CUSTOM_DETAILED(SLATE_STATS_DETAIL_LEVEL_MED, GSlateAddElements, ElementFNames[DrawElement.GetElementType()]);
		// Determine what type of element to add
		switch ( DrawElement.GetElementType() )
		{
		case FSlateDrawElement::ET_Box:
			EnablePixelSnapping ? AddBoxElement<ESlateVertexRounding::Enabled>(DrawElement) : AddBoxElement<ESlateVertexRounding::Disabled>(DrawElement);
			break;
		case FSlateDrawElement::ET_DebugQuad:
			EnablePixelSnapping ? AddQuadElement<ESlateVertexRounding::Enabled>(DrawElement) : AddQuadElement<ESlateVertexRounding::Disabled>(DrawElement);
			break;
		case FSlateDrawElement::ET_Text:
			EnablePixelSnapping ? AddTextElement<ESlateVertexRounding::Enabled>(DrawElement) : AddTextElement<ESlateVertexRounding::Disabled>(DrawElement);
			break;
		case FSlateDrawElement::ET_ShapedText:
			EnablePixelSnapping ? AddShapedTextElement<ESlateVertexRounding::Enabled>(DrawElement) : AddShapedTextElement<ESlateVertexRounding::Disabled>(DrawElement);
			break;
		case FSlateDrawElement::ET_Spline:
			EnablePixelSnapping ? AddSplineElement<ESlateVertexRounding::Enabled>(DrawElement) : AddSplineElement<ESlateVertexRounding::Disabled>(DrawElement);
			break;
		case FSlateDrawElement::ET_Line:
			EnablePixelSnapping ? AddLineElement<ESlateVertexRounding::Enabled>(DrawElement) : AddLineElement<ESlateVertexRounding::Disabled>(DrawElement);
			break;
		case FSlateDrawElement::ET_Gradient:
			EnablePixelSnapping ? AddGradientElement<ESlateVertexRounding::Enabled>(DrawElement) : AddGradientElement<ESlateVertexRounding::Disabled>(DrawElement);
			break;
		case FSlateDrawElement::ET_Viewport:
			EnablePixelSnapping ? AddViewportElement<ESlateVertexRounding::Enabled>(DrawElement) : AddViewportElement<ESlateVertexRounding::Disabled>(DrawElement);
			break;
		case FSlateDrawElement::ET_Border:
			EnablePixelSnapping ? AddBorderElement<ESlateVertexRounding::Enabled>(DrawElement) : AddBorderElement<ESlateVertexRounding::Disabled>(DrawElement);
			break;
		case FSlateDrawElement::ET_Custom:
			AddCustomElement(DrawElement);
			break;
		case FSlateDrawElement::ET_CustomVerts:
			AddCustomVerts(DrawElement);
			break;
		case FSlateDrawElement::ET_CachedBuffer:
			AddCachedBuffer(DrawElement);
			break;
		case FSlateDrawElement::ET_Layer:
			AddLayer(DrawElement);
			break;
		case FSlateDrawElement::ET_PostProcessPass:
			AddPostProcessPass(DrawElement, ViewportSize);
			break;
		default:
			checkf(0, TEXT("Invalid element type"));
			break;
		}
	}
}

FColor FSlateElementBatcher::PackVertexColor(const FLinearColor& InLinearColor)
{
	//NOTE: Using pow(x,2) instead of a full sRGB conversion has been tried, but it ended up
	// causing too much loss of data in the lower levels of black.
	return InLinearColor.ToFColor(bSRGBVertexColor);
}

template<ESlateVertexRounding Rounding>
void FSlateElementBatcher::AddQuadElement( const FSlateDrawElement& DrawElement, FColor Color )
{
	const FSlateRenderTransform& RenderTransform = DrawElement.GetRenderTransform();
	const FVector2D& LocalSize = DrawElement.GetLocalSize();
	ESlateDrawEffect InDrawEffects = DrawElement.GetDrawEffects();
	uint32 Layer = DrawElement.GetLayer();

	FSlateElementBatch& ElementBatch = FindBatchForElement( Layer, FShaderParams(), nullptr, ESlateDrawPrimitive::TriangleList, ESlateShader::Default, ESlateDrawEffect::None, ESlateBatchDrawFlag::Wireframe|ESlateBatchDrawFlag::NoBlending, DrawElement.GetClippingIndex(), DrawElement.GetSceneIndex());
	FSlateVertexArray& BatchVertices = BatchData->GetBatchVertexList(ElementBatch);
	FSlateIndexArray& BatchIndices = BatchData->GetBatchIndexList(ElementBatch);

	// Determine the four corners of the quad
	FVector2D TopLeft = FVector2D::ZeroVector;
	FVector2D TopRight = FVector2D(LocalSize.X, 0);
	FVector2D BotLeft = FVector2D(0, LocalSize.Y);
	FVector2D BotRight = FVector2D(LocalSize.X, LocalSize.Y);

	// The start index of these vertices in the index buffer
	uint32 IndexStart = BatchVertices.Num();

	// Add four vertices to the list of verts to be added to the vertex buffer
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, TopLeft, FVector2D(0.0f,0.0f), Color ) );
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, TopRight, FVector2D(1.0f,0.0f), Color ) );
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, BotLeft, FVector2D(0.0f,1.0f), Color ) );
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, BotRight, FVector2D(1.0f,1.0f),Color ) );

	// The offset into the index buffer where this quads indices start
	uint32 IndexOffsetStart = BatchIndices.Num();
	// Add 6 indices to the vertex buffer.  (2 tri's per quad, 3 indices per tri)
	BatchIndices.Add( IndexStart + 0 );
	BatchIndices.Add( IndexStart + 1 );
	BatchIndices.Add( IndexStart + 2 );

	BatchIndices.Add( IndexStart + 2 );
	BatchIndices.Add( IndexStart + 1 );
	BatchIndices.Add( IndexStart + 3 );
}


FSlateRenderTransform GetBoxRenderTransform(const FSlateDrawElement& DrawElement)
{
	const FSlateRenderTransform& ElementRenderTransform = DrawElement.GetRenderTransform();
	const float RotationAngle = DrawElement.GetDataPayload().Angle;
	if (RotationAngle == 0.0f)
	{
		return ElementRenderTransform;
	}
	const FVector2D RotationPoint = DrawElement.GetDataPayload().RotationPoint;
	const FSlateRenderTransform RotationTransform = Concatenate(Inverse(RotationPoint), FQuat2D(RotationAngle), RotationPoint);
	return Concatenate(RotationTransform, ElementRenderTransform);
}

int32 SlateFeathering = 0;
static FAutoConsoleVariableRef CVarSlateFeathering(TEXT("Slate.Feathering"), SlateFeathering, TEXT(""), ECVF_Default);

FORCEINLINE void IndexQuad(FSlateIndexArray& BatchIndices, int32 TopLeft, int32 TopRight, int32 BottomRight, int32 BottomLeft)
{
	BatchIndices.Add(TopLeft);
	BatchIndices.Add(TopRight);
	BatchIndices.Add(BottomRight);

	BatchIndices.Add(BottomRight);
	BatchIndices.Add(BottomLeft);
	BatchIndices.Add(TopLeft);
}

template<ESlateVertexRounding Rounding>
void FSlateElementBatcher::AddBoxElement(const FSlateDrawElement& DrawElement)
{
	NumDrawnBoxesStat++;

	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();

	check(InPayload.BrushResource);
	const FSlateBrush* BrushResource = InPayload.BrushResource;

	ensureMsgf(BrushResource->DrawAs != ESlateBrushDrawType::NoDrawType, TEXT("This should have been filtered out earlier in the Make... call."));

	const FColor Tint = PackVertexColor(InPayload.Tint);
	const FSlateRenderTransform& ElementRenderTransform = DrawElement.GetRenderTransform();
	const FSlateRenderTransform RenderTransform = GetBoxRenderTransform(DrawElement);
	const FVector2D& LocalSize = DrawElement.GetLocalSize();

	const ESlateDrawEffect InDrawEffects = DrawElement.GetDrawEffects();
	const uint32 Layer = DrawElement.GetLayer();

	const float DrawScale = DrawElement.GetScale();

	// Do pixel snapping
	FVector2D TopLeft(0,0);
	FVector2D BotRight(LocalSize);

	uint32 TextureWidth = 1;
	uint32 TextureHeight = 1;

	// Get the default start and end UV.  If the texture is atlased this value will be a subset of this
	FVector2D StartUV = FVector2D(0.0f,0.0f);
	FVector2D EndUV = FVector2D(1.0f,1.0f);
	FVector2D SizeUV;

	FVector2D HalfTexel;

	const FSlateShaderResourceProxy* ResourceProxy = InPayload.ResourceProxy;
	FSlateShaderResource* Resource = nullptr;
	if( ResourceProxy )
	{
		// The actual texture for rendering.  If the texture is atlased this is the atlas
		Resource = ResourceProxy->Resource;
		// The width and height of the texture (non-atlased size)
		TextureWidth = ResourceProxy->ActualSize.X != 0 ? ResourceProxy->ActualSize.X : 1;
		TextureHeight = ResourceProxy->ActualSize.Y != 0 ? ResourceProxy->ActualSize.Y : 1;

		// Texel offset
		HalfTexel = FVector2D( PixelCenterOffset/TextureWidth, PixelCenterOffset/TextureHeight );

		FBox2D BrushUV = BrushResource->GetUVRegion();
		//In case brush has valid UV region - use it instead of proxy UV
		if (BrushUV.bIsValid)
		{
			SizeUV = BrushUV.GetSize();
			StartUV = BrushUV.Min + HalfTexel;
			EndUV = StartUV + SizeUV;
		}
		else
		{
			SizeUV = ResourceProxy->SizeUV;
			StartUV = ResourceProxy->StartUV + HalfTexel;
			EndUV = StartUV + ResourceProxy->SizeUV;
		}
	}
	else
	{
		// no texture
		SizeUV = FVector2D(1.0f,1.0f);
		HalfTexel = FVector2D( PixelCenterOffset, PixelCenterOffset );
	}


	const ESlateBrushTileType::Type TilingRule = BrushResource->Tiling;
	const bool bTileHorizontal = (TilingRule == ESlateBrushTileType::Both || TilingRule == ESlateBrushTileType::Horizontal);
	const bool bTileVertical = (TilingRule == ESlateBrushTileType::Both || TilingRule == ESlateBrushTileType::Vertical);

	const ESlateBrushMirrorType::Type MirroringRule = BrushResource->Mirroring;
	const bool bMirrorHorizontal = (MirroringRule == ESlateBrushMirrorType::Both || MirroringRule == ESlateBrushMirrorType::Horizontal);
	const bool bMirrorVertical = (MirroringRule == ESlateBrushMirrorType::Both || MirroringRule == ESlateBrushMirrorType::Vertical);

	// Pass the tiling information as a flag so we can pick the correct texture addressing mode
	ESlateBatchDrawFlag DrawFlags = InPayload.BatchFlags;
	DrawFlags |= ( ( bTileHorizontal ? ESlateBatchDrawFlag::TileU : ESlateBatchDrawFlag::None ) | ( bTileVertical ? ESlateBatchDrawFlag::TileV : ESlateBatchDrawFlag::None ) );

	FSlateElementBatch& ElementBatch = FindBatchForElement( Layer, FShaderParams(), Resource, ESlateDrawPrimitive::TriangleList, ESlateShader::Default, InDrawEffects, DrawFlags, DrawElement.GetClippingIndex(), DrawElement.GetSceneIndex());
	FSlateVertexArray& BatchVertices = BatchData->GetBatchVertexList(ElementBatch);
	FSlateIndexArray& BatchIndices = BatchData->GetBatchIndexList(ElementBatch);

	float HorizontalTiling = bTileHorizontal ? LocalSize.X / TextureWidth : 1.0f;
	float VerticalTiling = bTileVertical ? LocalSize.Y / TextureHeight : 1.0f;

	const FVector2D Tiling( HorizontalTiling, VerticalTiling );

	// The start index of these vertices in the index buffer
	uint32 IndexStart = BatchVertices.Num();
	// The offset into the index buffer where this elements indices start
	uint32 IndexOffsetStart = BatchIndices.Num();

	const FMargin& Margin = BrushResource->Margin;

	const FVector2D TopRight = FVector2D(BotRight.X, TopLeft.Y);
	const FVector2D BotLeft = FVector2D(TopLeft.X, BotRight.Y);

	const FColor FeatherColor(0, 0, 0, 0);

	if ( BrushResource->DrawAs != ESlateBrushDrawType::Image &&
		( Margin.Left != 0.0f || Margin.Top != 0.0f || Margin.Right != 0.0f || Margin.Bottom != 0.0f ) )
	{
		// Create 9 quads for the box element based on the following diagram
		//     ___LeftMargin    ___RightMargin
		//    /                /
		//  +--+-------------+--+
		//  |  |c1           |c2| ___TopMargin
		//  +--o-------------o--+
		//  |  |             |  |
		//  |  |c3           |c4|
		//  +--o-------------o--+
		//  |  |             |  | ___BottomMargin
		//  +--+-------------+--+


		// Determine the texture coordinates for each quad
		// These are not scaled.
		float LeftMarginU = (Margin.Left > 0.0f)
			? StartUV.X + Margin.Left * SizeUV.X + HalfTexel.X
			: StartUV.X;
		float TopMarginV = (Margin.Top > 0.0f)
			? StartUV.Y + Margin.Top * SizeUV.Y + HalfTexel.Y
			: StartUV.Y;
		float RightMarginU = (Margin.Right > 0.0f)
			? EndUV.X - Margin.Right * SizeUV.X + HalfTexel.X
			: EndUV.X;
		float BottomMarginV = (Margin.Bottom > 0.0f)
			? EndUV.Y - Margin.Bottom * SizeUV.Y + HalfTexel.Y
			: EndUV.Y;

		if( bMirrorHorizontal || bMirrorVertical )
		{
			const FVector2D UVMin = StartUV;
			const FVector2D UVMax = EndUV;

			if( bMirrorHorizontal )
			{
				StartUV.X = UVMax.X - ( StartUV.X - UVMin.X );
				EndUV.X = UVMax.X - ( EndUV.X - UVMin.X );
				LeftMarginU = UVMax.X - ( LeftMarginU - UVMin.X );
				RightMarginU = UVMax.X - ( RightMarginU - UVMin.X );
			}
			if( bMirrorVertical )
			{
				StartUV.Y = UVMax.Y - ( StartUV.Y - UVMin.Y );
				EndUV.Y = UVMax.Y - ( EndUV.Y - UVMin.Y );
				TopMarginV = UVMax.Y - ( TopMarginV - UVMin.Y );
				BottomMarginV = UVMax.Y - ( BottomMarginV - UVMin.Y );
			}
		}

		// Determine the margins for each quad

		float LeftMarginX = TextureWidth * Margin.Left;
		float TopMarginY = TextureHeight * Margin.Top;
		float RightMarginX = LocalSize.X - TextureWidth * Margin.Right;
		float BottomMarginY = LocalSize.Y - TextureHeight * Margin.Bottom;

		// If the margins are overlapping the margins are too big or the button is too small
		// so clamp margins to half of the box size
		if( RightMarginX < LeftMarginX )
		{
			LeftMarginX = LocalSize.X / 2;
			RightMarginX = LeftMarginX;
		}

		if( BottomMarginY < TopMarginY )
		{
			TopMarginY = LocalSize.Y / 2;
			BottomMarginY = TopMarginY;
		}

		FVector2D Position = TopLeft;
		FVector2D EndPos = BotRight;

		BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( Position.X, Position.Y ),		LocalSize, DrawScale, FVector4(StartUV,										Tiling),	Tint ) ); //0
		BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( Position.X, TopMarginY ),		LocalSize, DrawScale, FVector4(FVector2D( StartUV.X, TopMarginV ),			Tiling),	Tint ) ); //1
		BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( LeftMarginX, Position.Y ),		LocalSize, DrawScale, FVector4(FVector2D( LeftMarginU, StartUV.Y ),			Tiling),	Tint ) ); //2
		BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( LeftMarginX, TopMarginY ),		LocalSize, DrawScale, FVector4(FVector2D( LeftMarginU, TopMarginV ),		Tiling),	Tint ) ); //3
		BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( RightMarginX, Position.Y ),	LocalSize, DrawScale, FVector4(FVector2D( RightMarginU, StartUV.Y ),		Tiling),	Tint ) ); //4
		BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( RightMarginX, TopMarginY ),	LocalSize, DrawScale, FVector4(FVector2D( RightMarginU,TopMarginV),			Tiling),	Tint ) ); //5
		BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( EndPos.X, Position.Y ),		LocalSize, DrawScale, FVector4(FVector2D( EndUV.X, StartUV.Y ),				Tiling),	Tint ) ); //6
		BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( EndPos.X, TopMarginY ),		LocalSize, DrawScale, FVector4(FVector2D( EndUV.X, TopMarginV),				Tiling),	Tint ) ); //7

		BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( Position.X, BottomMarginY ),	LocalSize, DrawScale, FVector4(FVector2D( StartUV.X, BottomMarginV ),		Tiling),	Tint ) ); //8
		BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( LeftMarginX, BottomMarginY ),	LocalSize, DrawScale, FVector4(FVector2D( LeftMarginU, BottomMarginV ),		Tiling),	Tint ) ); //9
		BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( RightMarginX, BottomMarginY ),	LocalSize, DrawScale, FVector4(FVector2D( RightMarginU, BottomMarginV ),	Tiling),	Tint ) ); //10
		BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( EndPos.X, BottomMarginY ),		LocalSize, DrawScale, FVector4(FVector2D( EndUV.X, BottomMarginV ),			Tiling),	Tint ) ); //11
		BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( Position.X, EndPos.Y ),		LocalSize, DrawScale, FVector4(FVector2D( StartUV.X, EndUV.Y ),				Tiling),	Tint ) ); //12
		BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( LeftMarginX, EndPos.Y ),		LocalSize, DrawScale, FVector4(FVector2D( LeftMarginU, EndUV.Y ),			Tiling),	Tint ) ); //13
		BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( RightMarginX, EndPos.Y ),		LocalSize, DrawScale, FVector4(FVector2D( RightMarginU, EndUV.Y ),			Tiling),	Tint ) ); //14
		BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( EndPos.X, EndPos.Y ),			LocalSize, DrawScale, FVector4(EndUV,										Tiling),	Tint ) ); //15

		// Top
		BatchIndices.Add( IndexStart + 0 );
		BatchIndices.Add( IndexStart + 1 );
		BatchIndices.Add( IndexStart + 2 );
		BatchIndices.Add( IndexStart + 2 );
		BatchIndices.Add( IndexStart + 1 );
		BatchIndices.Add( IndexStart + 3 );

		BatchIndices.Add( IndexStart + 2 );
		BatchIndices.Add( IndexStart + 3 );
		BatchIndices.Add( IndexStart + 4 );
		BatchIndices.Add( IndexStart + 4 );
		BatchIndices.Add( IndexStart + 3 );
		BatchIndices.Add( IndexStart + 5 );

		BatchIndices.Add( IndexStart + 4 );
		BatchIndices.Add( IndexStart + 5 );
		BatchIndices.Add( IndexStart + 6 );
		BatchIndices.Add( IndexStart + 6 );
		BatchIndices.Add( IndexStart + 5 );
		BatchIndices.Add( IndexStart + 7 );

		// Middle
		BatchIndices.Add( IndexStart + 1 );
		BatchIndices.Add( IndexStart + 8 );
		BatchIndices.Add( IndexStart + 3 );
		BatchIndices.Add( IndexStart + 3 );
		BatchIndices.Add( IndexStart + 8 );
		BatchIndices.Add( IndexStart + 9 );

		BatchIndices.Add( IndexStart + 3 );
		BatchIndices.Add( IndexStart + 9 );
		BatchIndices.Add( IndexStart + 5 );
		BatchIndices.Add( IndexStart + 5 );
		BatchIndices.Add( IndexStart + 9 );
		BatchIndices.Add( IndexStart + 10 );

		BatchIndices.Add( IndexStart + 5 );
		BatchIndices.Add( IndexStart + 10 );
		BatchIndices.Add( IndexStart + 7 );
		BatchIndices.Add( IndexStart + 7 );
		BatchIndices.Add( IndexStart + 10 );
		BatchIndices.Add( IndexStart + 11 );

		// Bottom
		BatchIndices.Add( IndexStart + 8 );
		BatchIndices.Add( IndexStart + 12 );
		BatchIndices.Add( IndexStart + 9 );
		BatchIndices.Add( IndexStart + 9 );
		BatchIndices.Add( IndexStart + 12 );
		BatchIndices.Add( IndexStart + 13 );

		BatchIndices.Add( IndexStart + 9 );
		BatchIndices.Add( IndexStart + 13 );
		BatchIndices.Add( IndexStart + 10 );
		BatchIndices.Add( IndexStart + 10 );
		BatchIndices.Add( IndexStart + 13 );
		BatchIndices.Add( IndexStart + 14 );

		BatchIndices.Add( IndexStart + 10 );
		BatchIndices.Add( IndexStart + 14 );
		BatchIndices.Add( IndexStart + 11 );
		BatchIndices.Add( IndexStart + 11 );
		BatchIndices.Add( IndexStart + 14 );
		BatchIndices.Add( IndexStart + 15 );

		if ( SlateFeathering && Rounding == ESlateVertexRounding::Disabled )
		{
			const int32 FeatherStart = BatchVertices.Num();

			// Top
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, FVector2D(Position.X, Position.Y) + FVector2D(-1, -1) / DrawScale, LocalSize, DrawScale, FVector4(StartUV, Tiling), FeatherColor)); //0
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, FVector2D(LeftMarginX, Position.Y) + FVector2D(0, -1) / DrawScale, LocalSize, DrawScale, FVector4(FVector2D(LeftMarginU, StartUV.Y), Tiling), FeatherColor)); //1
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, FVector2D(RightMarginX, Position.Y) + FVector2D(0, -1) / DrawScale, LocalSize, DrawScale, FVector4(FVector2D(RightMarginU, StartUV.Y), Tiling), FeatherColor)); //2
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, FVector2D(EndPos.X, Position.Y) + FVector2D(1, -1) / DrawScale, LocalSize, DrawScale, FVector4(FVector2D(EndUV.X, StartUV.Y), Tiling), FeatherColor)); //3

			// Left
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, FVector2D(Position.X, TopMarginY) + FVector2D(-1, 0) / DrawScale, LocalSize, DrawScale, FVector4(FVector2D(StartUV.X, TopMarginV), Tiling), FeatherColor)); //4
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, FVector2D(Position.X, BottomMarginY) + FVector2D(-1, 0) / DrawScale, LocalSize, DrawScale, FVector4(FVector2D(StartUV.X, BottomMarginV), Tiling), FeatherColor)); //5

			// Right
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, FVector2D(EndPos.X, TopMarginY) + FVector2D(1, 0) / DrawScale, LocalSize, DrawScale, FVector4(FVector2D(EndUV.X, TopMarginV), Tiling), FeatherColor)); //6
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, FVector2D(EndPos.X, BottomMarginY) + FVector2D(1, 0) / DrawScale, LocalSize, DrawScale, FVector4(FVector2D(EndUV.X, BottomMarginV), Tiling), FeatherColor)); //7

			// Bottom
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, FVector2D(Position.X, EndPos.Y) + FVector2D(-1, 1) / DrawScale, LocalSize, DrawScale, FVector4(FVector2D(StartUV.X, EndUV.Y), Tiling), FeatherColor)); //8
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, FVector2D(LeftMarginX, EndPos.Y) + FVector2D(0, 1) / DrawScale, LocalSize, DrawScale, FVector4(FVector2D(LeftMarginU, EndUV.Y), Tiling), FeatherColor)); //9
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, FVector2D(RightMarginX, EndPos.Y) + FVector2D(0, 1) / DrawScale, LocalSize, DrawScale, FVector4(FVector2D(RightMarginU, EndUV.Y), Tiling), FeatherColor)); //10
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, FVector2D(EndPos.X, EndPos.Y) + FVector2D(1, 1) / DrawScale, LocalSize, DrawScale, FVector4(EndUV, Tiling), FeatherColor)); //11

			// Top Left
			IndexQuad(BatchIndices, FeatherStart + 0, FeatherStart + 1, IndexStart + 2, IndexStart + 0);
			// Top Middle
			IndexQuad(BatchIndices, FeatherStart + 1, FeatherStart + 2, IndexStart + 4, IndexStart + 2);
			// Top Right
			IndexQuad(BatchIndices, FeatherStart + 2, FeatherStart + 3, IndexStart + 6, IndexStart + 4);

			//-----------------------------------------------------------

			// Left Top
			IndexQuad(BatchIndices, FeatherStart + 0, IndexStart + 0, IndexStart + 1, FeatherStart + 4);
			// Left Middle
			IndexQuad(BatchIndices, FeatherStart + 4, IndexStart + 1, IndexStart + 8, FeatherStart + 5);
			// Left Bottom
			IndexQuad(BatchIndices, FeatherStart + 5, IndexStart + 8, IndexStart + 12, FeatherStart + 8);

			//-----------------------------------------------------------

			// Right Top
			IndexQuad(BatchIndices, IndexStart + 6, FeatherStart + 3, FeatherStart + 6, IndexStart + 7);
			// Right Middle
			IndexQuad(BatchIndices, IndexStart + 7, FeatherStart + 6, FeatherStart + 7, IndexStart + 11);
			// Right Bottom
			IndexQuad(BatchIndices, IndexStart + 11, FeatherStart + 7, FeatherStart + 11, IndexStart + 15);

			//-----------------------------------------------------------

			// Bottom Left
			IndexQuad(BatchIndices, IndexStart + 12, IndexStart + 13, FeatherStart + 9, FeatherStart + 8);
			// Bottom Middle
			IndexQuad(BatchIndices, IndexStart + 13, IndexStart + 14, FeatherStart + 10, FeatherStart + 9);
			// Bottom Right
			IndexQuad(BatchIndices, IndexStart + 14, IndexStart + 15, FeatherStart + 11, FeatherStart + 10);
		}
	}
	else
	{
		if( bMirrorHorizontal || bMirrorVertical )
		{
			const FVector2D UVMin = StartUV;
			const FVector2D UVMax = EndUV;

			if( bMirrorHorizontal )
			{
				StartUV.X = UVMax.X - ( StartUV.X - UVMin.X );
				EndUV.X = UVMax.X - ( EndUV.X - UVMin.X );
			}
			if( bMirrorVertical )
			{
				StartUV.Y = UVMax.Y - ( StartUV.Y - UVMin.Y );
				EndUV.Y = UVMax.Y - ( EndUV.Y - UVMin.Y );
			}
		}

		// Add four vertices to the list of verts to be added to the vertex buffer
		BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, TopLeft, LocalSize, DrawScale, FVector4(StartUV, Tiling), Tint));
		BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, TopRight, LocalSize, DrawScale, FVector4(FVector2D(EndUV.X, StartUV.Y), Tiling), Tint));
		BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, BotLeft, LocalSize, DrawScale, FVector4(FVector2D(StartUV.X, EndUV.Y), Tiling), Tint));
		BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, BotRight, LocalSize, DrawScale, FVector4(EndUV, Tiling), Tint));

		BatchIndices.Add( IndexStart + 0 );
		BatchIndices.Add( IndexStart + 1 );
		BatchIndices.Add( IndexStart + 2 );

		BatchIndices.Add( IndexStart + 2 );
		BatchIndices.Add( IndexStart + 1 );
		BatchIndices.Add( IndexStart + 3 );

		int32 TopLeftIndex = IndexStart + 0;
		int32 TopRightIndex = IndexStart + 1;
		int32 BottomLeftIndex = IndexStart + 2;
		int32 BottomRightIndex = IndexStart + 3;

		if ( SlateFeathering && Rounding == ESlateVertexRounding::Disabled )
		{
			const int32 FeatherStart = BatchVertices.Num();

			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, TopLeft + FVector2D(-1, -1) / DrawScale, LocalSize, DrawScale, FVector4(StartUV, Tiling), FeatherColor));
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, TopRight + FVector2D(1, -1) / DrawScale, LocalSize, DrawScale, FVector4(FVector2D(EndUV.X, StartUV.Y), Tiling), FeatherColor));
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, BotLeft + FVector2D(-1, 1) / DrawScale, LocalSize, DrawScale, FVector4(FVector2D(StartUV.X, EndUV.Y), Tiling), FeatherColor));
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, BotRight + FVector2D(1, 1) / DrawScale, LocalSize, DrawScale, FVector4(EndUV, Tiling), FeatherColor));

			// Top-Top
			BatchIndices.Add(FeatherStart + 0);
			BatchIndices.Add(FeatherStart + 1);
			BatchIndices.Add(TopRightIndex);

			// Top-Bottom
			BatchIndices.Add(FeatherStart + 0);
			BatchIndices.Add(TopRightIndex);
			BatchIndices.Add(TopLeftIndex);

			// Left-Top
			BatchIndices.Add(FeatherStart + 0);
			BatchIndices.Add(BottomLeftIndex);
			BatchIndices.Add(FeatherStart + 2);

			// Left-Bottom
			BatchIndices.Add(FeatherStart + 0);
			BatchIndices.Add(TopLeftIndex);
			BatchIndices.Add(BottomLeftIndex);

			// Right-Top
			BatchIndices.Add(TopRightIndex);
			BatchIndices.Add(FeatherStart + 1);
			BatchIndices.Add(FeatherStart + 3);

			// Right-Bottom
			BatchIndices.Add(TopRightIndex);
			BatchIndices.Add(FeatherStart + 3);
			BatchIndices.Add(BottomRightIndex);

			// Bottom-Top
			BatchIndices.Add(BottomLeftIndex);
			BatchIndices.Add(BottomRightIndex);
			BatchIndices.Add(FeatherStart + 3);

			// Bottom-Bottom
			BatchIndices.Add(FeatherStart + 3);
			BatchIndices.Add(FeatherStart + 2);
			BatchIndices.Add(BottomLeftIndex);
		}
	}
}

template<ESlateVertexRounding Rounding>
void FSlateElementBatcher::AddTextElement(const FSlateDrawElement& DrawElement)
{
	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	FColor BaseTint = PackVertexColor(InPayload.Tint);

	const FFontOutlineSettings& OutlineSettings = InPayload.FontInfo.OutlineSettings;

	// Don't do anything if there the font would be completely transparent 
	if ((BaseTint.A == 0 && OutlineSettings.OutlineSize == 0) || (BaseTint.A == 0 && OutlineSettings.OutlineColor.A == 0))
	{
		return;
	}

	int32 Len = InPayload.ImmutableText ? FCString::Strlen(InPayload.ImmutableText) : 0;
	// Nothing to do if no text
	if (Len == 0)
	{
		return;
	}


	NumDrawnTextsStat++;

	ESlateDrawEffect InDrawEffects = DrawElement.GetDrawEffects();
	uint32 Layer = DrawElement.GetLayer();

	// extract the layout transform from the draw element
	FSlateLayoutTransform LayoutTransform(DrawElement.GetScale(), DrawElement.GetPosition());

	// We don't just scale up fonts, we draw them in local space pre-scaled so we don't get scaling artifcats.
	// So we need to pull the layout scale out of the layout and render transform so we can apply them
	// in local space with pre-scaled fonts.
	const float FontScale = LayoutTransform.GetScale();
	FSlateLayoutTransform InverseLayoutTransform = Inverse(Concatenate(Inverse(FontScale), LayoutTransform));
	const FSlateRenderTransform RenderTransform = Concatenate(Inverse(FontScale), DrawElement.GetRenderTransform());

	FSlateFontCache& FontCache = *RenderingPolicy->GetFontCache();
	FSlateShaderResourceManager& ResourceManager = *RenderingPolicy->GetResourceManager();

	const UObject* BaseFontMaterial = InPayload.FontInfo.FontMaterial;
	const UObject* OutlineFontMaterial = OutlineSettings.OutlineMaterial;

	bool bOutlineFont = OutlineSettings.OutlineSize > 0.0f;

	const float OutlineSize = OutlineSettings.OutlineSize;


	auto BuildFontGeometry = [&](const FFontOutlineSettings& InOutlineSettings, const FColor& InTint, const UObject* FontMaterial, int32 InLayer, int32 InOutlineHorizontalOffset)
	{
		FCharacterList& CharacterList = FontCache.GetCharacterList(InPayload.FontInfo, FontScale, InOutlineSettings);

		float MaxHeight = CharacterList.GetMaxHeight();

		uint32 FontTextureIndex = 0;
		FSlateShaderResource* FontAtlasTexture = nullptr;
		FSlateShaderResource* FontShaderResource = nullptr;

		FSlateElementBatch* ElementBatch = nullptr;
		FSlateVertexArray* BatchVertices = nullptr;
		FSlateIndexArray* BatchIndices = nullptr;

		uint32 VertexOffset = 0;
		uint32 IndexOffset = 0;

		float InvTextureSizeX = 0;
		float InvTextureSizeY = 0;

		float LineX = 0;

		FCharacterEntry PreviousCharEntry;

		int32 Kerning = 0;

		FVector2D TopLeft(0, 0);

		const float PosX = TopLeft.X;
		float PosY = TopLeft.Y;

		LineX = PosX;

		const bool bIsFontMaterial = FontMaterial != nullptr;

		uint32 NumChars = Len;

		uint32 NumLines = 1;
		for( uint32 CharIndex = 0; CharIndex < NumChars; ++CharIndex )
		{
			const TCHAR CurrentChar = InPayload.ImmutableText[ CharIndex ];

			const bool IsNewline = (CurrentChar == '\n');

			if (IsNewline)
			{
				// Move down: we are drawing the next line.
				PosY += MaxHeight;
				// Carriage return 
				LineX = PosX;

				++NumLines;

			}
			else
			{
				const FCharacterEntry& Entry = CharacterList.GetCharacter(CurrentChar, InPayload.FontInfo.FontFallback);

				if( Entry.Valid && (FontAtlasTexture == nullptr || Entry.TextureIndex != FontTextureIndex) )
				{
					// Font has a new texture for this glyph. Refresh the batch we use and the index we are currently using
					FontTextureIndex = Entry.TextureIndex;

					FontAtlasTexture = FontCache.GetSlateTextureResource( FontTextureIndex );
					check(FontAtlasTexture);

					FontShaderResource = ResourceManager.GetFontShaderResource( FontTextureIndex, FontAtlasTexture, InPayload.FontInfo.FontMaterial );
					check(FontShaderResource);

					ElementBatch = &FindBatchForElement(InLayer, FShaderParams(), FontShaderResource, ESlateDrawPrimitive::TriangleList, ESlateShader::Font, InDrawEffects, ESlateBatchDrawFlag::None, DrawElement.GetClippingIndex(), DrawElement.GetSceneIndex());

					BatchVertices = &BatchData->GetBatchVertexList(*ElementBatch);
					BatchIndices = &BatchData->GetBatchIndexList(*ElementBatch);

					VertexOffset = BatchVertices->Num();
					IndexOffset = BatchIndices->Num();
				
					InvTextureSizeX = 1.0f/FontAtlasTexture->GetWidth();
					InvTextureSizeY = 1.0f/FontAtlasTexture->GetHeight();
				}

				const bool bIsWhitespace = !Entry.Valid || FText::IsWhitespace(CurrentChar);

				if( !bIsWhitespace && PreviousCharEntry.Valid )
				{
					Kerning = CharacterList.GetKerning( PreviousCharEntry, Entry );
				}
				else
				{
					Kerning = 0;
				}

				LineX += Kerning;
				PreviousCharEntry = Entry;

				if( !bIsWhitespace )
				{
					const float X = LineX + Entry.HorizontalOffset+InOutlineHorizontalOffset;
					// Note PosX,PosY is the upper left corner of the bounding box representing the string.  This computes the Y position of the baseline where text will sit

					const float Y = PosY - Entry.VerticalOffset+MaxHeight+Entry.GlobalDescender;
					const float U = Entry.StartU * InvTextureSizeX;
					const float V = Entry.StartV * InvTextureSizeY;
					const float SizeX = Entry.USize;
					const float SizeY = Entry.VSize;
					const float SizeU = Entry.USize * InvTextureSizeX;
					const float SizeV = Entry.VSize * InvTextureSizeY;

					{
						FSlateVertexArray& BatchVerticesRef = *BatchVertices;
						FSlateIndexArray& BatchIndicesRef = *BatchIndices;

						FVector2D UpperLeft( X, Y );
						FVector2D UpperRight( X+SizeX, Y );
						FVector2D LowerLeft( X, Y+SizeY );
						FVector2D LowerRight( X+SizeX, Y+SizeY );

						// Add four vertices for this quad
						BatchVerticesRef.AddUninitialized( 4 );
						// Add six indices for this quad
						BatchIndicesRef.AddUninitialized( 6 );

						// The start index of these vertices in the index buffer
						uint32 IndexStart = VertexOffset;

						float Ut = 0.0f, Vt = 0.0f, UtMax = 0.0f, VtMax = 0.0f;
						if( bIsFontMaterial )
						{
							float DistAlpha = (float)CharIndex/NumChars;
							float DistAlphaNext = (float)(CharIndex+1)/NumChars;

							// This creates a set of UV's that goes from 0-1, from left to right of the string in U and 0-1 baseline to baseline top to bottom in V
							Ut = FMath::Lerp(0.0f, 1.0f, DistAlpha);
							Vt = FMath::Lerp(0.0f, 1.0f, UpperLeft.Y/(MaxHeight*NumLines));

							UtMax = FMath::Lerp(0.0f, 1.0f, DistAlphaNext);
							VtMax = FMath::Lerp(0.0f, 1.0f, LowerLeft.Y/(MaxHeight*NumLines));
						}

						// Add four vertices to the list of verts to be added to the vertex buffer
						BatchVerticesRef[ VertexOffset++ ] = FSlateVertex::Make<Rounding>( RenderTransform, UpperLeft,								FVector4(U,V,				Ut,Vt),			FVector2D(0.0f,0.0f), InTint );
						BatchVerticesRef[ VertexOffset++ ] = FSlateVertex::Make<Rounding>( RenderTransform, FVector2D(LowerRight.X,UpperLeft.Y),	FVector4(U+SizeU, V,		UtMax,Vt),		FVector2D(1.0f,0.0f), InTint );
						BatchVerticesRef[ VertexOffset++ ] = FSlateVertex::Make<Rounding>( RenderTransform, FVector2D(UpperLeft.X,LowerRight.Y),	FVector4(U, V+SizeV,		Ut,VtMax),		FVector2D(0.0f,1.0f), InTint );
						BatchVerticesRef[ VertexOffset++ ] = FSlateVertex::Make<Rounding>( RenderTransform, LowerRight,								FVector4(U+SizeU, V+SizeV,	UtMax,VtMax),	FVector2D(1.0f,1.0f), InTint );

						BatchIndicesRef[IndexOffset++] = IndexStart + 0;
						BatchIndicesRef[IndexOffset++] = IndexStart + 1;
						BatchIndicesRef[IndexOffset++] = IndexStart + 2;
						BatchIndicesRef[IndexOffset++] = IndexStart + 1;
						BatchIndicesRef[IndexOffset++] = IndexStart + 3;
						BatchIndicesRef[IndexOffset++] = IndexStart + 2;
					}
				}

				LineX += Entry.XAdvance;
			}
		}
	};

	if (bOutlineFont)
	{
		// Build geometry for the outline
		BuildFontGeometry(OutlineSettings, PackVertexColor(OutlineSettings.OutlineColor), OutlineFontMaterial, Layer, 0);

		//The fill area was measured without an outline so it must be shifted by the scaled outline size
		float HorizontalOffset = FMath::RoundToFloat(OutlineSize * FontScale);

		// Build geometry for the base font which is always rendered on top of the outline
		BuildFontGeometry(FFontOutlineSettings::NoOutline, BaseTint, BaseFontMaterial, Layer + 1, HorizontalOffset);
	}
	else
	{
		// No outline, draw normally
		BuildFontGeometry(FFontOutlineSettings::NoOutline, BaseTint, BaseFontMaterial, Layer, 0);
	}
}

template<ESlateVertexRounding Rounding>
void FSlateElementBatcher::AddShapedTextElement( const FSlateDrawElement& DrawElement )
{
	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	check(InPayload.ShapedGlyphSequence.IsValid());

	const FFontOutlineSettings& OutlineSettings = InPayload.ShapedGlyphSequence->GetFontOutlineSettings();

	const TArray<FShapedGlyphEntry>& GlyphsToRender = InPayload.ShapedGlyphSequence->GetGlyphsToRender();
	if (GlyphsToRender.Num() == 0)
	{
		return;
	}

	FColor BaseTint = PackVertexColor(InPayload.Tint);


	// Don't do anything if there the font would be completely transparent 
	if((BaseTint.A == 0 && OutlineSettings.OutlineSize == 0) || (BaseTint.A == 0 && InPayload.OutlineTint.A == 0))
	{
		return;
	}

	FSlateFontCache& FontCache = *RenderingPolicy->GetFontCache();
	FSlateShaderResourceManager& ResourceManager = *RenderingPolicy->GetResourceManager();

	const int16 TextBaseline = InPayload.ShapedGlyphSequence->GetTextBaseline();
	const uint16 MaxHeight = InPayload.ShapedGlyphSequence->GetMaxTextHeight();

	NumDrawnTextsStat++;

	ESlateDrawEffect InDrawEffects = DrawElement.GetDrawEffects();
	uint32 Layer = DrawElement.GetLayer();

	// extract the layout transform from the draw element
	FSlateLayoutTransform LayoutTransform(DrawElement.GetScale(), DrawElement.GetPosition());

	// We don't just scale up fonts, we draw them in local space pre-scaled so we don't get scaling artifcats.
	// So we need to pull the layout scale out of the layout and render transform so we can apply them
	// in local space with pre-scaled fonts.
	const float FontScale = LayoutTransform.GetScale();
	FSlateLayoutTransform InverseLayoutTransform = Inverse(Concatenate(Inverse(FontScale), LayoutTransform));
	const FSlateRenderTransform RenderTransform = Concatenate(Inverse(FontScale), DrawElement.GetRenderTransform());

	const UObject* BaseFontMaterial = InPayload.ShapedGlyphSequence->GetFontMaterial();
	const UObject* OutlineFontMaterial = OutlineSettings.OutlineMaterial;

	bool bOutlineFont = OutlineSettings.OutlineSize > 0.0f;

	const float OutlineSize = OutlineSettings.OutlineSize;

	auto BuildFontGeometry = [&](const FFontOutlineSettings& InOutlineSettings, const FColor& InTint, const UObject* FontMaterial, int32 InLayer, int32 InHorizontalOffset)
	{
		FVector2D TopLeft(0, 0);

		const float PosX = TopLeft.X+InHorizontalOffset;
		float PosY = TopLeft.Y;

		float LineX = PosX;
		float LineY = PosY;

		int32 FontTextureIndex = -1;
		FSlateShaderResource* FontAtlasTexture = nullptr;
		FSlateShaderResource* FontShaderResource = nullptr;

		FSlateElementBatch* ElementBatch = nullptr;
		FSlateVertexArray* BatchVertices = nullptr;
		FSlateIndexArray* BatchIndices = nullptr;

		uint32 VertexOffset = 0;
		uint32 IndexOffset = 0;

		float InvTextureSizeX = 0;
		float InvTextureSizeY = 0;

		const bool bIsFontMaterial = FontMaterial != nullptr;

		const int32 NumGlyphs = GlyphsToRender.Num();
		for (int32 GlyphIndex = 0; GlyphIndex < NumGlyphs; ++GlyphIndex)
		{
			const FShapedGlyphEntry& GlyphToRender = GlyphsToRender[GlyphIndex];

			if (GlyphToRender.bIsVisible)
			{
				const FShapedGlyphFontAtlasData GlyphAtlasData = FontCache.GetShapedGlyphFontAtlasData(GlyphToRender, InOutlineSettings);
				 
				if (GlyphAtlasData.Valid)
				{
					if (FontAtlasTexture == nullptr || GlyphAtlasData.TextureIndex != FontTextureIndex)
					{
						// Font has a new texture for this glyph. Refresh the batch we use and the index we are currently using
						FontTextureIndex = GlyphAtlasData.TextureIndex;

						FontAtlasTexture = FontCache.GetSlateTextureResource(FontTextureIndex);
						check(FontAtlasTexture);

						FontShaderResource = ResourceManager.GetFontShaderResource(FontTextureIndex, FontAtlasTexture, FontMaterial);
						check(FontShaderResource);

						ElementBatch = &FindBatchForElement(InLayer, FShaderParams(), FontShaderResource, ESlateDrawPrimitive::TriangleList, ESlateShader::Font, InDrawEffects, ESlateBatchDrawFlag::None, DrawElement.GetClippingIndex(), DrawElement.GetSceneIndex());

						BatchVertices = &BatchData->GetBatchVertexList(*ElementBatch);
						BatchIndices = &BatchData->GetBatchIndexList(*ElementBatch);

						VertexOffset = BatchVertices->Num();
						IndexOffset = BatchIndices->Num();

						InvTextureSizeX = 1.0f / FontAtlasTexture->GetWidth();
						InvTextureSizeY = 1.0f / FontAtlasTexture->GetHeight();
					}

					const float X = LineX + GlyphAtlasData.HorizontalOffset + GlyphToRender.XOffset;
					// Note PosX,PosY is the upper left corner of the bounding box representing the string.  This computes the Y position of the baseline where text will sit

					const float Y = LineY - GlyphAtlasData.VerticalOffset + GlyphToRender.YOffset + MaxHeight + TextBaseline;
					const float U = GlyphAtlasData.StartU * InvTextureSizeX;
					const float V = GlyphAtlasData.StartV * InvTextureSizeY;
					const float SizeX = GlyphAtlasData.USize;
					const float SizeY = GlyphAtlasData.VSize;
					const float SizeU = GlyphAtlasData.USize * InvTextureSizeX;
					const float SizeV = GlyphAtlasData.VSize * InvTextureSizeY;

					{
						FSlateVertexArray& BatchVerticesRef = *BatchVertices;
						FSlateIndexArray& BatchIndicesRef = *BatchIndices;

						const FVector2D UpperLeft(X, Y);
						const FVector2D UpperRight(X + SizeX, Y);
						const FVector2D LowerLeft(X, Y + SizeY);
						const FVector2D LowerRight(X + SizeX, Y + SizeY);

						// Add four vertices for this quad
						BatchVerticesRef.AddUninitialized(4);
						// Add six indices for this quad
						BatchIndicesRef.AddUninitialized(6);

						// The start index of these vertices in the index buffer
						uint32 IndexStart = VertexOffset;

						float Ut = 0.0f, Vt = 0.0f, UtMax = 0.0f, VtMax = 0.0f;
						if (bIsFontMaterial)
						{
							float DistAlpha = (float)GlyphIndex / NumGlyphs;
							float DistAlphaNext = (float)(GlyphIndex + 1) / NumGlyphs;

							// This creates a set of UV's that goes from 0-1, from left to right of the string in U and 0-1 baseline to baseline top to bottom in V
							Ut = FMath::Lerp(0.0f, 1.0f, DistAlpha);
							Vt = FMath::Lerp(0.0f, 1.0f, UpperLeft.Y / MaxHeight);

							UtMax = FMath::Lerp(0.0f, 1.0f, DistAlphaNext);
							VtMax = FMath::Lerp(0.0f, 1.0f, LowerLeft.Y / MaxHeight);
						}

						// Add four vertices to the list of verts to be added to the vertex buffer
						BatchVerticesRef[VertexOffset++] = FSlateVertex::Make<Rounding>(RenderTransform, UpperLeft,								FVector4(U, V, Ut, Vt),							FVector2D(0.0f, 0.0f), InTint );
						BatchVerticesRef[VertexOffset++] = FSlateVertex::Make<Rounding>(RenderTransform, FVector2D(LowerRight.X, UpperLeft.Y),	FVector4(U + SizeU, V, UtMax, Vt),				FVector2D(1.0f, 0.0f), InTint );
						BatchVerticesRef[VertexOffset++] = FSlateVertex::Make<Rounding>(RenderTransform, FVector2D(UpperLeft.X, LowerRight.Y),	FVector4(U, V + SizeV, Ut, VtMax),				FVector2D(0.0f, 1.0f), InTint );
						BatchVerticesRef[VertexOffset++] = FSlateVertex::Make<Rounding>(RenderTransform, LowerRight,							FVector4(U + SizeU, V + SizeV, UtMax, VtMax),	FVector2D(1.0f, 1.0f), InTint );

						BatchIndicesRef[IndexOffset++] = IndexStart + 0;
						BatchIndicesRef[IndexOffset++] = IndexStart + 1;
						BatchIndicesRef[IndexOffset++] = IndexStart + 2;
						BatchIndicesRef[IndexOffset++] = IndexStart + 1;
						BatchIndicesRef[IndexOffset++] = IndexStart + 3;
						BatchIndicesRef[IndexOffset++] = IndexStart + 2;
					}
				}
			}

			LineX += GlyphToRender.XAdvance;
			LineY += GlyphToRender.YAdvance;
		}
	};

	if (bOutlineFont)
	{
		// Build geometry for the outline
		BuildFontGeometry(OutlineSettings, PackVertexColor(InPayload.OutlineTint), OutlineFontMaterial, Layer, 0);
		
		//The fill area was measured without an outline so it must be shifted by the scaled outline size
		float HorizontalOffset = FMath::RoundToFloat(OutlineSize * FontScale);

		// Build geometry for the base font which is always rendered on top of the outline 
		BuildFontGeometry(FFontOutlineSettings::NoOutline, BaseTint, BaseFontMaterial, Layer+1, HorizontalOffset);
	}
	else
	{
		// No outline
		BuildFontGeometry(FFontOutlineSettings::NoOutline, BaseTint, BaseFontMaterial, Layer, 0);
	}
}

template<ESlateVertexRounding Rounding>
void FSlateElementBatcher::AddGradientElement( const FSlateDrawElement& DrawElement )
{
	const FSlateRenderTransform& RenderTransform = DrawElement.GetRenderTransform();
	const FVector2D& LocalSize = DrawElement.GetLocalSize();
	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	ESlateDrawEffect InDrawEffects = DrawElement.GetDrawEffects();
	uint32 Layer = DrawElement.GetLayer();

	// There must be at least one gradient stop
	check( InPayload.GradientStops.Num() > 0 );

	FSlateElementBatch& ElementBatch = 
		FindBatchForElement( 
			Layer,
			FShaderParams(),
			nullptr,
			ESlateDrawPrimitive::TriangleList,
			ESlateShader::Default,
			InDrawEffects,
			InPayload.BatchFlags,
			DrawElement.GetClippingIndex(),
			DrawElement.GetSceneIndex());

	FSlateVertexArray& BatchVertices = BatchData->GetBatchVertexList(ElementBatch);
	FSlateIndexArray& BatchIndices = BatchData->GetBatchIndexList(ElementBatch);

	// Determine the four corners of the quad containing the gradient
	FVector2D TopLeft = FVector2D::ZeroVector;
	FVector2D TopRight = FVector2D(LocalSize.X, 0);
	FVector2D BotLeft = FVector2D(0, LocalSize.Y);
	FVector2D BotRight = FVector2D(LocalSize.X, LocalSize.Y);

	// Copy the gradient stops.. We may need to add more
	TArray<FSlateGradientStop> GradientStops = InPayload.GradientStops;

	const FSlateGradientStop& FirstStop = InPayload.GradientStops[0];
	const FSlateGradientStop& LastStop = InPayload.GradientStops[ InPayload.GradientStops.Num() - 1 ];
		
	// Determine if the first and last stops are not at the start and end of the quad
	// If they are not add a gradient stop with the same color as the first and/or last stop
	if( InPayload.GradientType == Orient_Vertical )
	{
		if( 0.0f < FirstStop.Position.X )
		{
			// The first stop is after the left side of the quad.  Add a stop at the left side of the quad using the same color as the first stop
			GradientStops.Insert( FSlateGradientStop( FVector2D(0.0, 0.0), FirstStop.Color ), 0 );
		}

		if( LocalSize.X > LastStop.Position.X )
		{
			// The last stop is before the right side of the quad.  Add a stop at the right side of the quad using the same color as the last stop
			GradientStops.Add( FSlateGradientStop( LocalSize, LastStop.Color ) ); 
		}
	}
	else
	{
		if( 0.0f < FirstStop.Position.Y )
		{
			// The first stop is after the top side of the quad.  Add a stop at the top side of the quad using the same color as the first stop
			GradientStops.Insert( FSlateGradientStop( FVector2D(0.0, 0.0), FirstStop.Color ), 0 );
		}

		if( LocalSize.Y > LastStop.Position.Y )
		{
			// The last stop is before the bottom side of the quad.  Add a stop at the bottom side of the quad using the same color as the last stop
			GradientStops.Add( FSlateGradientStop( LocalSize, LastStop.Color ) ); 
		}
	}

	uint32 IndexOffsetStart = BatchIndices.Num();

	// Add a pair of vertices for each gradient stop. Connecting them to the previous stop if necessary
	// Assumes gradient stops are sorted by position left to right or top to bottom
	for( int32 StopIndex = 0; StopIndex < GradientStops.Num(); ++StopIndex )
	{
		uint32 IndexStart = BatchVertices.Num();

		const FSlateGradientStop& CurStop = GradientStops[StopIndex];

		// The start vertex at this stop
		FVector2D StartPt;
		// The end vertex at this stop
		FVector2D EndPt;

		if( InPayload.GradientType == Orient_Vertical )
		{
			// Gradient stop is vertical so gradients to left to right
			StartPt = TopLeft;
			EndPt = BotLeft;
			// Gradient stops are interpreted in local space.
			StartPt.X += CurStop.Position.X;
			EndPt.X += CurStop.Position.X;
		}
		else
		{
			// Gradient stop is horizontal so gradients to top to bottom
			StartPt = TopLeft;
			EndPt = TopRight;
			// Gradient stops are interpreted in local space.
			StartPt.Y += CurStop.Position.Y;
			EndPt.Y += CurStop.Position.Y;
		}

		if( StopIndex == 0 )
		{
			// First stop does not have a full quad yet so do not create indices
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, StartPt, FVector2D::ZeroVector, FVector2D::ZeroVector, CurStop.Color.ToFColor(false) ) );
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, EndPt, FVector2D::ZeroVector, FVector2D::ZeroVector, CurStop.Color.ToFColor(false) ) );
		}
		else
		{
			// All stops after the first have indices and generate quads
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, StartPt, FVector2D::ZeroVector, FVector2D::ZeroVector, CurStop.Color.ToFColor(false) ) );
			BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, EndPt, FVector2D::ZeroVector, FVector2D::ZeroVector, CurStop.Color.ToFColor(false) ) );

			// Connect the indices to the previous vertices
			BatchIndices.Add( IndexStart - 2 );
			BatchIndices.Add( IndexStart - 1 );
			BatchIndices.Add( IndexStart + 0 );

			BatchIndices.Add( IndexStart + 0 );
			BatchIndices.Add( IndexStart - 1 );
			BatchIndices.Add( IndexStart + 1 );
		}
	}
}

template<ESlateVertexRounding Rounding>
void FSlateElementBatcher::AddSplineElement( const FSlateDrawElement& DrawElement )
{
	const FSlateRenderTransform& RenderTransform = DrawElement.GetRenderTransform();
	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	ESlateDrawEffect InDrawEffects = DrawElement.GetDrawEffects();
	uint32 Layer = DrawElement.GetLayer();

	//@todo SLATE: Merge with AddLineElement?

	//@todo SLATE: This should probably be done in window space so there are no scaling artifacts?
	const float DirectLength = (InPayload.EndPt - InPayload.StartPt).Size();
	const float HandleLength = ((InPayload.EndPt - InPayload.EndDir) - (InPayload.StartPt + InPayload.StartDir)).Size();
	float NumSteps = FMath::Clamp<float>(FMath::CeilToInt(FMath::Max(DirectLength,HandleLength)/15.0f), 1, 256);
	float GradientSubSteps = 0.f;
	// Is this spline using a color gradient?
	const bool bColorGradient = InPayload.GradientStops.Num() > 0;
	if (bColorGradient)
	{
		const float GradientSteps = InPayload.GradientStops.Num() - 1.f;
		GradientSubSteps = FMath::CeilToInt(NumSteps/GradientSteps);
		NumSteps = GradientSteps * GradientSubSteps;
	}

	// 1 is the minimum thickness we support
	// Thickness is given in screenspace, so convert it to local space before proceeding.
	float InThickness = FMath::Max( 1.0f, DrawElement.GetInverseLayoutTransform().GetScale() * InPayload.Thickness );

	// The radius to use when checking the distance of pixels to the actual line.  Arbitrary value based on what looks the best
	const float Radius = 1.5f;

	// Compute the actual size of the line we need based on thickness.  Need to ensure pixels that are at least Thickness/2 + Sample radius are generated so that we have enough pixels to blend.
	// The idea for the spline anti-alising technique is based on the fast prefiltered lines technique published in GPU Gems 2 
	const float LineThickness = FMath::CeilToInt( (2.0f * Radius + InThickness ) * FMath::Sqrt(2.0f) );

	// The amount we increase each side of the line to generate enough pixels
	const float HalfThickness = LineThickness * .5f + Radius;

	// Find a batch for the element
	FSlateElementBatch& ElementBatch = FindBatchForElement( Layer, FShaderParams::MakePixelShaderParams( FVector4( InPayload.Thickness,Radius,0,0) ), nullptr, ESlateDrawPrimitive::TriangleList, ESlateShader::LineSegment, InDrawEffects, ESlateBatchDrawFlag::None, DrawElement.GetClippingIndex(), DrawElement.GetSceneIndex());
	FSlateVertexArray& BatchVertices = BatchData->GetBatchVertexList(ElementBatch);
	FSlateIndexArray& BatchIndices = BatchData->GetBatchIndexList(ElementBatch);

	const FVector2D StartPt = InPayload.StartPt;
	const FVector2D StartDir = InPayload.StartDir;
	const FVector2D EndPt = InPayload.EndPt;
	const FVector2D EndDir = InPayload.EndDir;
	
	// Compute the normal to the line
	FVector2D Normal = FVector2D( StartPt.Y - EndPt.Y, EndPt.X - StartPt.X ).GetSafeNormal();

	FVector2D Up = Normal * HalfThickness;

	// Generate the first segment
	const float Alpha = 1.0f/NumSteps;
	FVector2D StartPos = StartPt;
	FVector2D EndPos = FVector2D( FMath::CubicInterp( StartPt, StartDir, EndPt, EndDir, Alpha ) );

	FColor VertexCol = bColorGradient ? PackVertexColor(InPayload.GradientStops[0].Color) : PackVertexColor(InPayload.Tint);

	BatchVertices.Add(FSlateVertex::Make<Rounding>( RenderTransform, StartPos + Up, TransformPoint(RenderTransform, StartPos), TransformPoint(RenderTransform, EndPos), VertexCol ) );
	BatchVertices.Add(FSlateVertex::Make<Rounding>( RenderTransform, StartPos - Up, TransformPoint(RenderTransform, StartPos), TransformPoint(RenderTransform, EndPos), VertexCol ) );

	// Generate the rest of the segments
	for( int32 Step = 0; Step < NumSteps; ++Step )
	{
		// Skip the first step as it was already generated
		if( Step > 0 )
		{
			const float StepAlpha = (Step+1.0f)/NumSteps;
			EndPos = FVector2D( FMath::CubicInterp( StartPt, StartDir, EndPt, EndDir, StepAlpha ) );
		}
		if (bColorGradient)
		{
			const float InterpVal = FMath::Min<float>(InPayload.GradientStops.Num()-1, (Step+1.f)/GradientSubSteps);
			const int32 ColorIdx = FMath::CeilToInt(InterpVal);
			const float ColorAlpha = InterpVal - (ColorIdx-1);
			VertexCol = PackVertexColor(FLinearColor::LerpUsingHSV(InPayload.GradientStops[ColorIdx-1].Color, InPayload.GradientStops[ColorIdx].Color, ColorAlpha));
		}

		int32 IndexStart = BatchVertices.Num();

		// Compute the normal to the line
		FVector2D SegmentNormal = FVector2D( StartPos.Y - EndPos.Y, EndPos.X - StartPos.X ).GetSafeNormal();

		// Create the new vertices for the thick line segment
		Up = SegmentNormal * HalfThickness;

		BatchVertices.Add(FSlateVertex::Make<Rounding>( RenderTransform, EndPos + Up, TransformPoint(RenderTransform, StartPos), TransformPoint(RenderTransform, EndPos), VertexCol ) );
		BatchVertices.Add(FSlateVertex::Make<Rounding>( RenderTransform, EndPos - Up, TransformPoint(RenderTransform, StartPos), TransformPoint(RenderTransform, EndPos), VertexCol ) );

		BatchIndices.Add( IndexStart - 2 );
		BatchIndices.Add( IndexStart - 1 );
		BatchIndices.Add( IndexStart + 0 );

		BatchIndices.Add( IndexStart + 0 );
		BatchIndices.Add( IndexStart + 1 );
		BatchIndices.Add( IndexStart - 1 );

		StartPos = EndPos;
	}
}


/**
 * Calculates the intersection of two line segments P1->P2, P3->P4
 * The tolerance setting is used when the lines aren't currently intersecting but will intersect in the future  
 * The higher the tolerance the greater the distance that the intersection point can be.
 *
 * @return true if the line intersects.  Populates Intersection
 */
static bool LineIntersect( const FVector2D& P1, const FVector2D& P2, const FVector2D& P3, const FVector2D& P4, FVector2D& Intersect, float Tolerance = .1f)
{
	float NumA = ( (P4.X-P3.X)*(P1.Y-P3.Y) - (P4.Y-P3.Y)*(P1.X-P3.X) );
	float NumB =  ( (P2.X-P1.X)*(P1.Y-P3.Y) - (P2.Y-P1.Y)*(P1.X-P3.X) );

	float Denom = (P4.Y-P3.Y)*(P2.X-P1.X) - (P4.X-P3.X)*(P2.Y-P1.Y);

	if( FMath::IsNearlyZero( NumA ) && FMath::IsNearlyZero( NumB )  )
	{
		// Lines are the same
		Intersect = (P1 + P2) / 2;
		return true;
	}

	if( FMath::IsNearlyZero(Denom) )
	{
		// Lines are parallel
		return false;
	}
	 
	float B = NumB / Denom;
	float A = NumA / Denom;

	// Note that this is a "tweaked" intersection test for the purpose of joining line segments.  We don't just want to know if the line segments
	// Intersect, but where they would if they don't currently. Except that we don't care in the case that where the segments 
	// intersection is so far away that its infeasible to use the intersection point later.
	if( A >= -Tolerance && A <= (1.0f + Tolerance ) && B >= -Tolerance && B <= (1.0f + Tolerance) )
	{
		Intersect = P1+A*(P2-P1);
		return true;
	}

	return false;
}
	
template<ESlateVertexRounding Rounding>
void FSlateElementBatcher::AddLineElement( const FSlateDrawElement& DrawElement )
{
	const FSlateRenderTransform& RenderTransform = DrawElement.GetRenderTransform();
	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	ESlateDrawEffect DrawEffects = DrawElement.GetDrawEffects();
	uint32 Layer = DrawElement.GetLayer();

	if (InPayload.NumPoints < 2 || !InPayload.Points)
	{
		return;
	}

	const FVector2D* Points = InPayload.Points;
	const FColor FinalTint = PackVertexColor(InPayload.Tint);

	if( InPayload.bAntialias )
	{
		// The radius to use when checking the distance of pixels to the actual line.  Arbitrary value based on what looks the best
		const float Radius = 1.5f;

		// Thickness is given in screen space, so convert it to local space before proceeding.
		float RequestedThickness = InPayload.Thickness;
		
		// Compute the actual size of the line we need based on thickness.  Need to ensure pixels that are at least Thickness/2 + Sample radius are generated so that we have enough pixels to blend.
		// The idea for the anti-aliasing technique is based on the fast prefiltered lines technique published in GPU Gems 2 
		const float LineThickness = FMath::CeilToInt( (2.0f * Radius + RequestedThickness ) * FMath::Sqrt(2.0f) );

		// The amount we increase each side of the line to generate enough pixels
		const float HalfThickness = LineThickness * .5f + Radius;

		// Find a batch for the element
		FSlateElementBatch& ElementBatch = FindBatchForElement( Layer, FShaderParams::MakePixelShaderParams( FVector4(RequestedThickness,Radius,0,0) ), nullptr, ESlateDrawPrimitive::TriangleList, ESlateShader::LineSegment, DrawEffects, ESlateBatchDrawFlag::None, DrawElement.GetClippingIndex(), DrawElement.GetSceneIndex());
		FSlateVertexArray& BatchVertices = BatchData->GetBatchVertexList(ElementBatch);
		FSlateIndexArray& BatchIndices = BatchData->GetBatchIndexList(ElementBatch);

		FVector2D StartPos = Points[0];
		FVector2D EndPos = Points[1];

		FVector2D Normal = FVector2D( StartPos.Y - EndPos.Y, EndPos.X - StartPos.X ).GetSafeNormal();

		FVector2D Up = Normal * HalfThickness;

		FColor StartColor = InPayload.PointColors ? PackVertexColor(InPayload.PointColors[0] * InPayload.Tint) : FinalTint;
		FColor EndColor = 	InPayload.PointColors ? PackVertexColor(InPayload.PointColors[1] * InPayload.Tint) : FinalTint;

		BatchVertices.Add(FSlateVertex::Make<Rounding>( RenderTransform, StartPos + Up, TransformPoint(RenderTransform, StartPos), TransformPoint(RenderTransform, EndPos), StartColor ) );
		BatchVertices.Add(FSlateVertex::Make<Rounding>( RenderTransform, StartPos - Up, TransformPoint(RenderTransform, StartPos), TransformPoint(RenderTransform, EndPos), EndColor ) );

		// Generate the rest of the segments
		for( int32 Point = 1; Point < InPayload.NumPoints; ++Point )
		{
			EndPos = Points[Point];
			// Determine if we should check the intersection point with the next line segment.
			// We will adjust were this line ends to the intersection
			bool bCheckIntersection = (Point + 1) < InPayload.NumPoints;
			uint32 IndexStart = BatchVertices.Num();

			// Compute the normal to the line
			Normal = FVector2D( StartPos.Y - EndPos.Y, EndPos.X - StartPos.X ).GetSafeNormal();

			// Create the new vertices for the thick line segment
			Up = Normal * HalfThickness;

			FColor PointColor = InPayload.PointColors ? PackVertexColor(InPayload.PointColors[Point] * InPayload.Tint) : FinalTint;

			FVector2D IntersectUpper = EndPos + Up;
			FVector2D IntersectLower = EndPos - Up;
			FVector2D IntersectCenter = EndPos;

			if( bCheckIntersection )
			{
				// The end point of the next segment
				const FVector2D NextEndPos = Points[Point+1];

				// The normal of the next segment
				const FVector2D NextNormal = FVector2D( EndPos.Y - NextEndPos.Y, NextEndPos.X - EndPos.X ).GetSafeNormal();

				// The next amount to adjust the vertices by 
				FVector2D NextUp = NextNormal * HalfThickness;

				FVector2D IntersectionPoint;
				if( LineIntersect( StartPos + Up, EndPos + Up, EndPos + NextUp, NextEndPos + NextUp, IntersectionPoint ) )
				{
					// If the lines intersect adjust where the line starts
					IntersectUpper = IntersectionPoint;

					// visualizes the intersection
					//AddQuadElement( IntersectUpper-FVector2D(1,1), FVector2D(2,2), 1, InClippingRect, Layer+1, FColor::Orange);
				}

				if( LineIntersect( StartPos - Up, EndPos - Up, EndPos - NextUp, NextEndPos - NextUp, IntersectionPoint ) )
				{
					// If the lines intersect adjust where the line starts
					IntersectLower = IntersectionPoint;

					// visualizes the intersection
					//AddQuadElement( IntersectLower-FVector2D(1,1), FVector2D(2,2), 1, InClippingRect, Layer+1, FColor::Yellow);
				}
				// the midpoint of the intersection.  Used as the new end to the line segment (not adjusted for anti-aliasing)
				IntersectCenter = (IntersectUpper+IntersectLower) * .5f;
			}

			// We use these points when making the copy of the vert below, so go ahead and cache it.
			FVector2D StartPosRenderSpace = TransformPoint(RenderTransform, StartPos);
			FVector2D IntersectCenterRenderSpace = TransformPoint(RenderTransform, IntersectCenter);

			if( Point > 1 )
			{
				// Make a copy of the last two vertices and update their start and end position to reflect the new line segment
				FSlateVertex StartV1 = BatchVertices[IndexStart-1];
				FSlateVertex StartV2 = BatchVertices[IndexStart-2];

				StartV1.TexCoords[0] = StartPosRenderSpace.X;
				StartV1.TexCoords[1] = StartPosRenderSpace.Y;
				StartV1.TexCoords[2] = IntersectCenterRenderSpace.X;
				StartV1.TexCoords[3] = IntersectCenterRenderSpace.Y;

				StartV2.TexCoords[0] = StartPosRenderSpace.X;
				StartV2.TexCoords[1] = StartPosRenderSpace.Y;
				StartV2.TexCoords[2] = IntersectCenterRenderSpace.X;
				StartV2.TexCoords[3] = IntersectCenterRenderSpace.Y;

				IndexStart += 2;
				BatchVertices.Add( StartV2 );
				BatchVertices.Add( StartV1 );
			}

			BatchVertices.Add(FSlateVertex::Make<Rounding>( RenderTransform, IntersectUpper, StartPosRenderSpace, IntersectCenterRenderSpace, PointColor ) );
			BatchVertices.Add(FSlateVertex::Make<Rounding>( RenderTransform, IntersectLower, StartPosRenderSpace, IntersectCenterRenderSpace, PointColor ) );

			BatchIndices.Add( IndexStart - 1 );
			BatchIndices.Add( IndexStart - 2 );
			BatchIndices.Add( IndexStart + 0 );

			BatchIndices.Add( IndexStart + 0 );
			BatchIndices.Add( IndexStart + 1 );
			BatchIndices.Add( IndexStart - 1 );

			StartPos = EndPos;
		}
	}
	else
	{
		if (InPayload.Thickness == 1)
		{
			// Find a batch for the element
			FSlateElementBatch& ElementBatch = FindBatchForElement(Layer, FShaderParams(), nullptr, ESlateDrawPrimitive::LineList, ESlateShader::Default, DrawEffects, ESlateBatchDrawFlag::None, DrawElement.GetClippingIndex(), DrawElement.GetSceneIndex());
			FSlateVertexArray& BatchVertices = BatchData->GetBatchVertexList(ElementBatch);
			FSlateIndexArray& BatchIndices = BatchData->GetBatchIndexList(ElementBatch);

			// Generate the line segments using the native line rendering of the platform.
			for (int32 Point = 0; Point < InPayload.NumPoints - 1; ++Point)
			{
				uint32 IndexStart = BatchVertices.Num();
				FVector2D StartPos = Points[Point];
				FVector2D EndPos = Points[Point + 1];

				FColor StartColor = InPayload.PointColors ? PackVertexColor(InPayload.PointColors[Point]   * InPayload.Tint) : FinalTint;
				FColor EndColor = 	InPayload.PointColors ? PackVertexColor(InPayload.PointColors[Point+1] * InPayload.Tint) : FinalTint;

				BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, StartPos, FVector2D::ZeroVector, StartColor));
				BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, EndPos, FVector2D::ZeroVector, EndColor));

				BatchIndices.Add(IndexStart);
				BatchIndices.Add(IndexStart + 1);
			}
		}
		else
		{
			// Find a batch for the element
			FSlateElementBatch& ElementBatch = FindBatchForElement(Layer, FShaderParams(), nullptr, ESlateDrawPrimitive::TriangleList, ESlateShader::Default, DrawEffects, ESlateBatchDrawFlag::None, DrawElement.GetClippingIndex(), DrawElement.GetSceneIndex());
			FSlateVertexArray& BatchVertices = BatchData->GetBatchVertexList(ElementBatch);
			FSlateIndexArray& BatchIndices = BatchData->GetBatchIndexList(ElementBatch);

			// Generate the line segments using non-aa'ed polylines.
			for (int32 Point = 0; Point < InPayload.NumPoints - 1; ++Point)
			{
				uint32 IndexStart = BatchVertices.Num();
				const FVector2D StartPos = Points[Point];
				const FVector2D EndPos = Points[Point + 1];

				FColor StartColor	= InPayload.PointColors ? PackVertexColor(InPayload.PointColors[Point]   * InPayload.Tint) : FinalTint;
				FColor EndColor		= InPayload.PointColors ? PackVertexColor(InPayload.PointColors[Point+1] * InPayload.Tint) : FinalTint;
	
				const FVector2D SegmentNormal = (EndPos - StartPos).GetSafeNormal();
				const FVector2D HalfThickNormal = SegmentNormal * (InPayload.Thickness * 0.5f);

				BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, StartPos + FVector2D(HalfThickNormal.Y, -HalfThickNormal.X), FVector2D::ZeroVector, FVector2D::ZeroVector, StartColor));
				BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, StartPos + FVector2D(-HalfThickNormal.Y, HalfThickNormal.X), FVector2D::ZeroVector, FVector2D::ZeroVector, StartColor));
				BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, EndPos + FVector2D(HalfThickNormal.Y, -HalfThickNormal.X), FVector2D::ZeroVector, FVector2D::ZeroVector, EndColor));
				BatchVertices.Add(FSlateVertex::Make<Rounding>(RenderTransform, EndPos + FVector2D(-HalfThickNormal.Y, HalfThickNormal.X), FVector2D::ZeroVector, FVector2D::ZeroVector, EndColor));

				BatchIndices.Add(IndexStart + 0);
				BatchIndices.Add(IndexStart + 1);
				BatchIndices.Add(IndexStart + 2);

				BatchIndices.Add(IndexStart + 2);
				BatchIndices.Add(IndexStart + 1);
				BatchIndices.Add(IndexStart + 3);
			}
		}
	}
}

template<ESlateVertexRounding Rounding>
void FSlateElementBatcher::AddViewportElement( const FSlateDrawElement& DrawElement )
{
	const FSlateRenderTransform& RenderTransform = DrawElement.GetRenderTransform();
	const FVector2D& LocalSize = DrawElement.GetLocalSize();
	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	ESlateDrawEffect InDrawEffects = DrawElement.GetDrawEffects();
	uint32 Layer = DrawElement.GetLayer();

	const FColor FinalColor = PackVertexColor(InPayload.Tint);

	ESlateBatchDrawFlag DrawFlags = InPayload.BatchFlags;

	FSlateShaderResource* ViewportResource = InPayload.RenderTargetResource;
	ESlateShader::Type ShaderType = ESlateShader::Default;

	if( InPayload.bViewportTextureAlphaOnly )
	{
		// This is a slight hack, but the font shader is the same as the general shader except it reads alpha only textures
		ShaderType = ESlateShader::Font;
	}

	FSlateElementBatch& ElementBatch = FindBatchForElement( Layer, FShaderParams(), ViewportResource, ESlateDrawPrimitive::TriangleList, ShaderType, InDrawEffects, DrawFlags, DrawElement.GetClippingIndex(), DrawElement.GetSceneIndex());
	FSlateVertexArray& BatchVertices = BatchData->GetBatchVertexList(ElementBatch);
	FSlateIndexArray& BatchIndices = BatchData->GetBatchIndexList(ElementBatch);

	// Tag this batch as requiring vsync if the viewport requires it.
	if( ViewportResource != nullptr && !InPayload.bAllowViewportScaling )
	{
		bRequiresVsync |= InPayload.bRequiresVSync;
	}

	// Do pixel snapping
	FVector2D TopLeft(0,0);
	FVector2D BotRight(LocalSize);

	// If the viewport disallows scaling, force size to current texture size.
	if (ViewportResource != nullptr && !InPayload.bAllowViewportScaling)
	{
		BotRight = FVector2D(ViewportResource->GetWidth(), ViewportResource->GetHeight());
	}

	FVector2D TopRight = FVector2D( BotRight.X, TopLeft.Y);
	FVector2D BotLeft =	 FVector2D( TopLeft.X, BotRight.Y);

	// The start index of these vertices in the index buffer
	uint32 IndexStart = BatchVertices.Num();

	// Add four vertices to the list of verts to be added to the vertex buffer
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, TopLeft,	FVector2D(0.0f,0.0f),	FinalColor ) );
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, TopRight,	FVector2D(1.0f,0.0f),	FinalColor ) );
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, BotLeft,	FVector2D(0.0f,1.0f),	FinalColor ) );
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, BotRight,	FVector2D(1.0f,1.0f),	FinalColor ) );

	// The offset into the index buffer where this quads indices start
	uint32 IndexOffsetStart = BatchIndices.Num();

	// Add 6 indices to the vertex buffer.  (2 tri's per quad, 3 indices per tri)
	BatchIndices.Add( IndexStart + 0 );
	BatchIndices.Add( IndexStart + 1 );
	BatchIndices.Add( IndexStart + 2 );

	BatchIndices.Add( IndexStart + 2 );
	BatchIndices.Add( IndexStart + 1 );
	BatchIndices.Add( IndexStart + 3 );
}

template<ESlateVertexRounding Rounding>
void FSlateElementBatcher::AddBorderElement( const FSlateDrawElement& DrawElement )
{
	const FSlateRenderTransform& RenderTransform = DrawElement.GetRenderTransform();
	const FVector2D& LocalSize = DrawElement.GetLocalSize();
	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	ESlateDrawEffect InDrawEffects = DrawElement.GetDrawEffects();
	uint32 Layer = DrawElement.GetLayer();

	const float DrawScale = DrawElement.GetScale();

	check( InPayload.BrushResource );

	uint32 TextureWidth = 1;
	uint32 TextureHeight = 1;

	// Currently borders are not atlased because they are tiled.  So we just assume the texture proxy holds the actual texture
	const FSlateShaderResourceProxy* ResourceProxy = InPayload.ResourceProxy;
	FSlateShaderResource* Resource = ResourceProxy ? ResourceProxy->Resource : nullptr;
	if( Resource )
	{
		TextureWidth = Resource->GetWidth();
		TextureHeight = Resource->GetHeight();
	}
	FVector2D TextureSizeLocalSpace = TransformVector(DrawElement.GetInverseLayoutTransform(), FVector2D(TextureWidth, TextureHeight));
 
	// Texel offset
	const FVector2D HalfTexel( PixelCenterOffset/TextureWidth, PixelCenterOffset/TextureHeight );

	const FVector2D StartUV = HalfTexel;
	const FVector2D EndUV = FVector2D( 1.0f, 1.0f ) + HalfTexel;

	const FMargin& Margin = InPayload.BrushResource->Margin;

	// Do pixel snapping
	FVector2D TopLeft(0,0);
	FVector2D BotRight(LocalSize);
	// Determine the margins for each quad
	FVector2D TopLeftMargin(TextureSizeLocalSpace * FVector2D(Margin.Left, Margin.Top));
	FVector2D BotRightMargin(LocalSize - TextureSizeLocalSpace * FVector2D(Margin.Right, Margin.Bottom));

	float LeftMarginX = TopLeftMargin.X;
	float TopMarginY = TopLeftMargin.Y;
	float RightMarginX = BotRightMargin.X;
	float BottomMarginY = BotRightMargin.Y;

	// If the margins are overlapping the margins are too big or the button is too small
	// so clamp margins to half of the box size
	if( RightMarginX < LeftMarginX )
	{
		LeftMarginX = LocalSize.X / 2;
		RightMarginX = LeftMarginX;
	}

	if( BottomMarginY < TopMarginY )
	{
		TopMarginY = LocalSize.Y / 2;
		BottomMarginY = TopMarginY;
	}

	// Determine the texture coordinates for each quad
	float LeftMarginU = (Margin.Left > 0.0f) ? Margin.Left : 0.0f;
	float TopMarginV = (Margin.Top > 0.0f) ? Margin.Top : 0.0f;
	float RightMarginU = (Margin.Right > 0.0f) ? 1.0f - Margin.Right : 1.0f;
	float BottomMarginV = (Margin.Bottom > 0.0f) ? 1.0f - Margin.Bottom : 1.0f;

	LeftMarginU += HalfTexel.X;
	TopMarginV += HalfTexel.Y;
	BottomMarginV += HalfTexel.Y;
	RightMarginU += HalfTexel.X;

	// Determine the amount of tiling needed for the texture in this element.  The formula is number of pixels covered by the tiling portion of the texture / the number number of texels corresponding to the tiled portion of the texture.
	float TopTiling = (RightMarginX-LeftMarginX)/(TextureSizeLocalSpace.X * ( 1 - Margin.GetTotalSpaceAlong<Orient_Horizontal>() ));
	float LeftTiling = (BottomMarginY-TopMarginY)/(TextureSizeLocalSpace.Y * ( 1 - Margin.GetTotalSpaceAlong<Orient_Vertical>() ));
	
	FShaderParams ShaderParams = FShaderParams::MakePixelShaderParams( FVector4(LeftMarginU,RightMarginU,TopMarginV,BottomMarginV) );

	// The tint color applies to all brushes and is passed per vertex
	const FColor Tint = PackVertexColor(InPayload.Tint);

	// Pass the tiling information as a flag so we can pick the correct texture addressing mode
	ESlateBatchDrawFlag DrawFlags = (ESlateBatchDrawFlag::TileU|ESlateBatchDrawFlag::TileV);

	FSlateElementBatch& ElementBatch = FindBatchForElement( Layer, ShaderParams, Resource, ESlateDrawPrimitive::TriangleList, ESlateShader::Border, InDrawEffects, DrawFlags, DrawElement.GetClippingIndex(), DrawElement.GetSceneIndex());
	FSlateVertexArray& BatchVertices = BatchData->GetBatchVertexList(ElementBatch);
	FSlateIndexArray& BatchIndices = BatchData->GetBatchIndexList(ElementBatch);

	// Ensure tiling of at least 1.  
	TopTiling = TopTiling >= 1.0f ? TopTiling : 1.0f;
	LeftTiling = LeftTiling >= 1.0f ? LeftTiling : 1.0f;
	float RightTiling = LeftTiling;
	float BottomTiling = TopTiling;

	FVector2D Position = TopLeft;
	FVector2D EndPos = BotRight;

	// The start index of these vertices in the index buffer
	uint32 IndexStart = BatchVertices.Num();

	// Zero for second UV indicates no tiling and to just pass the UV though (for the corner sections)
	FVector2D Zero(0,0);

	// Add all the vertices needed for this element.  Vertices are duplicated so that we can have some sections with no tiling and some with tiling.
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, Position,									LocalSize, DrawScale, FVector4( StartUV.X, StartUV.Y, 0.0f, 0.0f),				Tint ) ); //0
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( Position.X, TopMarginY ),		LocalSize, DrawScale, FVector4( StartUV.X, TopMarginV, 0.0f, 0.0f),				Tint ) ); //1
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( LeftMarginX, Position.Y ),		LocalSize, DrawScale, FVector4( LeftMarginU, StartUV.Y, 0.0f, 0.0f),			Tint ) ); //2
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( LeftMarginX, TopMarginY ),		LocalSize, DrawScale, FVector4( LeftMarginU, TopMarginV, 0.0f, 0.0f),			Tint ) ); //3

	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( LeftMarginX, Position.Y ),		LocalSize, DrawScale, FVector4( StartUV.X, StartUV.Y, TopTiling, 0.0f),			Tint ) ); //4
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( LeftMarginX, TopMarginY ),		LocalSize, DrawScale, FVector4( StartUV.X, TopMarginV, TopTiling, 0.0f),		Tint ) ); //5
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( RightMarginX, Position.Y ),	LocalSize, DrawScale, FVector4( EndUV.X, StartUV.Y, TopTiling, 0.0f),			Tint ) ); //6
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( RightMarginX, TopMarginY ),	LocalSize, DrawScale, FVector4( EndUV.X, TopMarginV, TopTiling, 0.0f),			Tint ) ); //7

	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( RightMarginX, Position.Y ),	LocalSize, DrawScale, FVector4( RightMarginU, StartUV.Y, 0.0f, 0.0f),			Tint ) ); //8
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( RightMarginX, TopMarginY ),	LocalSize, DrawScale, FVector4( RightMarginU, TopMarginV, 0.0f, 0.0f),			Tint ) ); //9
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( EndPos.X, Position.Y ),		LocalSize, DrawScale, FVector4( EndUV.X, StartUV.Y, 0.0f, 0.0f),				Tint ) ); //10
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( EndPos.X, TopMarginY ),		LocalSize, DrawScale, FVector4( EndUV.X, TopMarginV, 0.0f, 0.0f),				Tint ) ); //11

	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( Position.X, TopMarginY ),		LocalSize, DrawScale, FVector4( StartUV.X, StartUV.Y, 0.0f, LeftTiling),		Tint ) ); //12
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( Position.X, BottomMarginY ),	LocalSize, DrawScale, FVector4( StartUV.X, EndUV.Y, 0.0f, LeftTiling),			Tint ) ); //13
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( LeftMarginX, TopMarginY ),		LocalSize, DrawScale, FVector4( LeftMarginU, StartUV.Y, 0.0f, LeftTiling),		Tint ) ); //14
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( LeftMarginX, BottomMarginY ),	LocalSize, DrawScale, FVector4( LeftMarginU, EndUV.Y, 0.0f, LeftTiling),		Tint ) ); //15

	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( RightMarginX, TopMarginY ),	LocalSize, DrawScale, FVector4( RightMarginU, StartUV.Y, 0.0f, RightTiling),	Tint ) ); //16
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( RightMarginX, BottomMarginY ), LocalSize, DrawScale, FVector4( RightMarginU, EndUV.Y, 0.0f, RightTiling),		Tint ) ); //17
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( EndPos.X, TopMarginY ),		LocalSize, DrawScale, FVector4( EndUV.X, StartUV.Y, 0.0f, RightTiling),			Tint ) ); //18
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( EndPos.X, BottomMarginY ),		LocalSize, DrawScale, FVector4( EndUV.X, EndUV.Y, 0.0f, RightTiling),			Tint ) ); //19

	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( Position.X, BottomMarginY ),	LocalSize, DrawScale, FVector4( StartUV.X, BottomMarginV, 0.0f, 0.0f),			Tint ) ); //20
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( Position.X, EndPos.Y ),		LocalSize, DrawScale, FVector4( StartUV.X, EndUV.Y, 0.0f, 0.0f),				Tint ) ); //21
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( LeftMarginX, BottomMarginY ),	LocalSize, DrawScale, FVector4( LeftMarginU, BottomMarginV, 0.0f, 0.0f),		Tint ) ); //22
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( LeftMarginX, EndPos.Y ),		LocalSize, DrawScale, FVector4( LeftMarginU, EndUV.Y, 0.0f, 0.0f),				Tint ) ); //23

	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( LeftMarginX, BottomMarginY ),	LocalSize, DrawScale, FVector4( StartUV.X, BottomMarginV, BottomTiling, 0.0f),	Tint ) ); //24
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( LeftMarginX, EndPos.Y ),		LocalSize, DrawScale, FVector4( StartUV.X, EndUV.Y, BottomTiling, 0.0f),		Tint ) ); //25
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( RightMarginX,BottomMarginY ),	LocalSize, DrawScale, FVector4( EndUV.X, BottomMarginV, BottomTiling, 0.0f),	Tint ) ); //26
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( RightMarginX, EndPos.Y ),		LocalSize, DrawScale, FVector4( EndUV.X, EndUV.Y, BottomTiling, 0.0f),			Tint ) ); //27

	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( RightMarginX, BottomMarginY ), LocalSize, DrawScale, FVector4( RightMarginU, BottomMarginV, 0.0f, 0.0f),		Tint ) ); //29
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( RightMarginX, EndPos.Y ),		LocalSize, DrawScale, FVector4( RightMarginU, EndUV.Y, 0.0f, 0.0f),				Tint ) ); //30
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( EndPos.X, BottomMarginY ),		LocalSize, DrawScale, FVector4( EndUV.X, BottomMarginV, 0.0f, 0.0f),			Tint ) ); //31
	BatchVertices.Add( FSlateVertex::Make<Rounding>( RenderTransform, FVector2D( EndPos.X, EndPos.Y ),			LocalSize, DrawScale, FVector4( EndUV.X, EndUV.Y, 0.0f, 0.0f),					Tint ) ); //32


	// The offset into the index buffer where this elements indices start
	uint32 IndexOffsetStart = BatchIndices.Num();

	// Top
	BatchIndices.Add( IndexStart + 0 );
	BatchIndices.Add( IndexStart + 1 );
	BatchIndices.Add( IndexStart + 2 );
	BatchIndices.Add( IndexStart + 2 );
	BatchIndices.Add( IndexStart + 1 );
	BatchIndices.Add( IndexStart + 3 );

	BatchIndices.Add( IndexStart + 4 );
	BatchIndices.Add( IndexStart + 5 );
	BatchIndices.Add( IndexStart + 6 );
	BatchIndices.Add( IndexStart + 6 );
	BatchIndices.Add( IndexStart + 5 );
	BatchIndices.Add( IndexStart + 7 );

	BatchIndices.Add( IndexStart + 8 );
	BatchIndices.Add( IndexStart + 9 );
	BatchIndices.Add( IndexStart + 10 );
	BatchIndices.Add( IndexStart + 10 );
	BatchIndices.Add( IndexStart + 9 );
	BatchIndices.Add( IndexStart + 11 );

	// Middle
	BatchIndices.Add( IndexStart + 12 );
	BatchIndices.Add( IndexStart + 13 );
	BatchIndices.Add( IndexStart + 14 );
	BatchIndices.Add( IndexStart + 14 );
	BatchIndices.Add( IndexStart + 13 );
	BatchIndices.Add( IndexStart + 15 );

	BatchIndices.Add( IndexStart + 16 );
	BatchIndices.Add( IndexStart + 17 );
	BatchIndices.Add( IndexStart + 18 );
	BatchIndices.Add( IndexStart + 18 );
	BatchIndices.Add( IndexStart + 17 );
	BatchIndices.Add( IndexStart + 19 );

	// Bottom
	BatchIndices.Add( IndexStart + 20 );
	BatchIndices.Add( IndexStart + 21 );
	BatchIndices.Add( IndexStart + 22 );
	BatchIndices.Add( IndexStart + 22 );
	BatchIndices.Add( IndexStart + 21 );
	BatchIndices.Add( IndexStart + 23 );

	BatchIndices.Add( IndexStart + 24 );
	BatchIndices.Add( IndexStart + 25 );
	BatchIndices.Add( IndexStart + 26 );
	BatchIndices.Add( IndexStart + 26 );
	BatchIndices.Add( IndexStart + 25 );
	BatchIndices.Add( IndexStart + 27 );

	BatchIndices.Add( IndexStart + 28 );
	BatchIndices.Add( IndexStart + 29 );
	BatchIndices.Add( IndexStart + 30 );
	BatchIndices.Add( IndexStart + 30 );
	BatchIndices.Add( IndexStart + 29 );
	BatchIndices.Add( IndexStart + 31 );
}

void FSlateElementBatcher::AddCustomElement( const FSlateDrawElement& DrawElement )
{
	FElementBatchMap& LayerToElementBatches = DrawLayer->GetElementBatchMap();

	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	uint32 Layer = DrawElement.GetLayer();

	if( InPayload.CustomDrawer.IsValid() )
	{
		// See if the layer already exists.
		TUniqueObj<FElementBatchArray>* ElementBatches = LayerToElementBatches.Find( Layer );
		if( !ElementBatches )
		{
			// The layer doesn't exist so make it now
			ElementBatches = &LayerToElementBatches.Add( Layer );
		}
		check( ElementBatches );

		// Custom elements are not batched together 
		(*ElementBatches)->Add( FSlateElementBatch( InPayload.CustomDrawer, DrawElement.GetClippingIndex() ) );
	}
}

void FSlateElementBatcher::AddCustomVerts(const FSlateDrawElement& DrawElement)
{
	FElementBatchMap& LayerToElementBatches = DrawLayer->GetElementBatchMap();

	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	uint32 Layer = DrawElement.GetLayer();

	if (InPayload.CustomVertsData.Num() >0)
	{
		// See if the layer already exists.
		TUniqueObj<FElementBatchArray>* ElementBatches = LayerToElementBatches.Find(Layer);
		if (!ElementBatches)
		{
			// The layer doesn't exist so make it now
			ElementBatches = &LayerToElementBatches.Add( Layer );
		}
		check(ElementBatches);

		FSlateElementBatch NewBatch(
			InPayload.ResourceProxy != nullptr ? InPayload.ResourceProxy->Resource : nullptr,
			FShaderParams(),
			ESlateShader::Custom,
			ESlateDrawPrimitive::TriangleList,
			DrawElement.GetDrawEffects(),
			InPayload.BatchFlags,
			DrawElement.GetClippingIndex(),
			InPayload.NumInstances,
			InPayload.InstanceOffset,
			InPayload.InstanceData,
			DrawElement.GetSceneIndex()
		);

		int32 Index = (*ElementBatches)->Add(NewBatch);
		FSlateElementBatch* ElementBatch = &(**ElementBatches)[Index];

		BatchData->AssignVertexArrayToBatch(*ElementBatch);
		BatchData->AssignIndexArrayToBatch(*ElementBatch);

		FSlateVertexArray& BatchVertices = BatchData->GetBatchVertexList(*ElementBatch);
		FSlateIndexArray& BatchIndices = BatchData->GetBatchIndexList(*ElementBatch);

		// Vertex Buffer since  it is already in slate format it is a straight copy
		BatchVertices = InPayload.CustomVertsData;
		BatchIndices = InPayload.CustomVertsIndexData;
	}
}

void FSlateElementBatcher::AddCachedBuffer(const FSlateDrawElement& DrawElement)
{
	FElementBatchMap& LayerToElementBatches = DrawLayer->GetElementBatchMap();

	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	uint32 Layer = DrawElement.GetLayer();

	if ( InPayload.CachedRenderData )
	{
		// See if the layer already exists.
		TUniqueObj<FElementBatchArray>* ElementBatches = LayerToElementBatches.Find(Layer);
		if ( !ElementBatches )
		{
			// The layer doesn't exist so make it now
			ElementBatches = &LayerToElementBatches.Add(Layer);
		}
		check(ElementBatches);

		// Custom elements are not batched together
		TSharedPtr< FSlateRenderDataHandle, ESPMode::ThreadSafe > RenderData = InPayload.CachedRenderData->AsShared();
		(*ElementBatches)->Add(FSlateElementBatch(RenderData, InPayload.CachedRenderDataOffset, DrawElement.GetClippingIndex()));
	}
}

void FSlateElementBatcher::AddLayer(const FSlateDrawElement& DrawElement)
{
	FElementBatchMap& LayerToElementBatches = DrawLayer->GetElementBatchMap();

	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	uint32 Layer = DrawElement.GetLayer();

	if ( InPayload.LayerHandle )
	{
		// See if the layer already exists.
		TUniqueObj<FElementBatchArray>* ElementBatches = LayerToElementBatches.Find(Layer);
		if ( !ElementBatches )
		{
			// The layer doesn't exist so make it now
			ElementBatches = &LayerToElementBatches.Add(Layer);
		}
		check(ElementBatches);

		// Custom elements are not batched together
		TSharedPtr< FSlateDrawLayerHandle, ESPMode::ThreadSafe > LayerHandle = InPayload.LayerHandle->AsShared();
		(*ElementBatches)->Add(FSlateElementBatch(LayerHandle, DrawElement.GetClippingIndex()));
	}
}

void FSlateElementBatcher::AddPostProcessPass(const FSlateDrawElement& DrawElement, const FVector2D& WindowSize)
{
	++NumPostProcessPasses;

	const FSlateRenderTransform& RenderTransform = DrawElement.GetRenderTransform();
	const FVector2D& LocalSize = DrawElement.GetLocalSize();
	
	const FSlateDataPayload& Payload = DrawElement.GetDataPayload();

	//@todo doesn't work with rotated or skewed objects yet
	const FVector2D& Position = DrawElement.GetPosition();

	uint32 Layer = DrawElement.GetLayer();

	// Determine the four corners of the quad
	FVector2D TopLeft = FVector2D::ZeroVector;
	FVector2D TopRight = FVector2D(LocalSize.X, 0);
	FVector2D BotLeft = FVector2D(0, LocalSize.Y);
	FVector2D BotRight = FVector2D(LocalSize.X, LocalSize.Y);


	// Offset by half a texel if the platform requires it for pixel perfect sampling
	//FVector2D HalfTexel = FVector2D(PixelCenterOffset / WindowSize.X, PixelCenterOffset / WindowSize.Y);

	FVector2D WorldTopLeft = TransformPoint(RenderTransform, TopLeft).RoundToVector();
	FVector2D WorldBotRight = TransformPoint(RenderTransform, BotRight).RoundToVector();

	FVector2D SizeUV = (WorldBotRight - WorldTopLeft) / WindowSize;

	// These could be negative with rotation or negative scales.  This is not supported yet
	if(SizeUV.X > 0 && SizeUV.Y > 0)
	{
		FShaderParams Params = FShaderParams::MakePixelShaderParams(FVector4(WorldTopLeft, WorldBotRight), FVector4(Payload.PostProcessData.X, Payload.PostProcessData.Y, Payload.DownsampleAmount, 0));

		FElementBatchMap& LayerToElementBatches = DrawLayer->GetElementBatchMap();

		// See if the layer already exists.
		TUniqueObj<FElementBatchArray>* ElementBatches = LayerToElementBatches.Find(Layer);
		if (!ElementBatches)
		{
			// The layer doesn't exist so make it now
			ElementBatches = &LayerToElementBatches.Add(Layer);
		}
		check(ElementBatches);

		// Custom elements are not batched together 
		(*ElementBatches)->Add(FSlateElementBatch(nullptr, Params, ESlateShader::PostProcess, ESlateDrawPrimitive::TriangleList, ESlateDrawEffect::None, ESlateBatchDrawFlag::None, DrawElement.GetClippingIndex()));
	}
}

FSlateElementBatch& FSlateElementBatcher::FindBatchForElement(
	uint32 Layer, 
	const FShaderParams& ShaderParams, 
	const FSlateShaderResource* InTexture, 
	ESlateDrawPrimitive::Type PrimitiveType,
	ESlateShader::Type ShaderType, 
	ESlateDrawEffect DrawEffects, 
	ESlateBatchDrawFlag DrawFlags,
	int32 ClippingIndex,
	int32 SceneIndex)
{
	SLATE_CYCLE_COUNTER_SCOPE_DETAILED(SLATE_STATS_DETAIL_LEVEL_HI, GSlateFindBatchTime);

//	SCOPE_CYCLE_COUNTER( STAT_SlateFindBatchForElement );
	FElementBatchMap& LayerToElementBatches = DrawLayer->GetElementBatchMap();

	// See if the layer already exists.
	TUniqueObj<FElementBatchArray>* ElementBatches = LayerToElementBatches.Find( Layer );
	if( !ElementBatches )
	{
		// The layer doesn't exist so make it now
		ElementBatches = &LayerToElementBatches.Add( Layer );
	}

	checkSlow( ElementBatches );

	// Create a temp batch so we can use it as our key to find if the same batch already exists
	FSlateElementBatch TempBatch( InTexture, ShaderParams, ShaderType, PrimitiveType, DrawEffects, DrawFlags, ClippingIndex, 0, 0, nullptr, SceneIndex );

	FSlateElementBatch* ElementBatch = (*ElementBatches)->FindByKey( TempBatch );
	if( !ElementBatch )
	{
		// No batch with the specified parameter exists.  Create it from the temp batch.
		int32 Index = (*ElementBatches)->Add( TempBatch );
		ElementBatch = &(**ElementBatches)[Index];

		BatchData->AssignVertexArrayToBatch(*ElementBatch);
		BatchData->AssignIndexArrayToBatch(*ElementBatch);
	}
	check( ElementBatch );

	// Increment the number of elements in the batch.
	++ElementBatch->NumElementsInBatch;
	return *ElementBatch;
}

void FSlateElementBatcher::ResetBatches()
{
	bRequiresVsync = false;
	NumPostProcessPasses = 0;
}
