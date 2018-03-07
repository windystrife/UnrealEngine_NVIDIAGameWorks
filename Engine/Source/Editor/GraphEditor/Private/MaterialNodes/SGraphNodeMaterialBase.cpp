// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MaterialNodes/SGraphNodeMaterialBase.h"
#include "Rendering/DrawElements.h"
#include "UnrealClient.h"
#include "RenderingThread.h"
#include "Misc/App.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SViewport.h"
#include "Widgets/Input/SCheckBox.h"
#include "MaterialGraph/MaterialGraph.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "GraphEditorSettings.h"
#include "SGraphPanel.h"
#include "TutorialMetaData.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"



/**
* Simple representation of the backbuffer that the preview canvas renders to
* This class may only be accessed from the render thread
*/
class FSlateMaterialPreviewRenderTarget : public FRenderTarget
{
public:
	/** FRenderTarget interface */
	virtual FIntPoint GetSizeXY() const
	{
		return ClippingRect.Size();
	}

	/** Sets the texture that this target renders to */
	void SetRenderTargetTexture( FTexture2DRHIRef& InRHIRef )
	{
		RenderTargetTextureRHI = InRHIRef;
	}

	/** Clears the render target texture */
	void ClearRenderTargetTexture()
	{
		RenderTargetTextureRHI.SafeRelease();
	}

	/** Sets the viewport rect for the render target */
	void SetViewRect( const FIntRect& InViewRect ) 
	{ 
		ViewRect = InViewRect;
	}

	/** Gets the viewport rect for the render target */
	const FIntRect& GetViewRect() const 
	{
		return ViewRect;
	}

	/** Sets the clipping rect for the render target */
	void SetClippingRect( const FIntRect& InClippingRect ) 
	{ 
		ClippingRect = InClippingRect;
	}

	/** Gets the clipping rect for the render target */
	const FIntRect& GetClippingRect() const 
	{
		return ClippingRect;
	}

private:
	FIntRect ViewRect;
	FIntRect ClippingRect;
};

/*-----------------------------------------------------------------------------
   FPreviewViewport
-----------------------------------------------------------------------------*/

FPreviewViewport::FPreviewViewport(class UMaterialGraphNode* InNode)
	: MaterialNode(InNode)
	, PreviewElement( new FPreviewElement )
{
	if (MaterialNode)
	{
		MaterialNode->InvalidatePreviewMaterialDelegate.BindRaw(this, &FPreviewViewport::UpdatePreviewNodeRenderProxy);
	}
}

FPreviewViewport::~FPreviewViewport()
{
	if (MaterialNode)
	{
		MaterialNode->InvalidatePreviewMaterialDelegate.Unbind();
	}
	// Pass the preview element to the render thread so that it's deleted after it's shown for the last time
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
	(
		SafeDeletePreviewElement,
		FThreadSafePreviewPtr, PreviewElementPtr, PreviewElement,
		{
			PreviewElementPtr.Reset();
		}
	);
}

void FPreviewViewport::OnDrawViewport( const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, class FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled )
{
	FSlateRect SlateCanvasRect = AllottedGeometry.GetLayoutBoundingRect();
	FSlateRect ClippedCanvasRect = SlateCanvasRect.IntersectionWith(MyCullingRect);

	FIntRect CanvasRect(
		FMath::TruncToInt( FMath::Max(0.0f, SlateCanvasRect.Left) ),
		FMath::TruncToInt( FMath::Max(0.0f, SlateCanvasRect.Top) ),
		FMath::TruncToInt( FMath::Max(0.0f, SlateCanvasRect.Right) ), 
		FMath::TruncToInt( FMath::Max(0.0f, SlateCanvasRect.Bottom) ) );

	FIntRect ClippingRect(
		FMath::TruncToInt( FMath::Max(0.0f, ClippedCanvasRect.Left) ),
		FMath::TruncToInt( FMath::Max(0.0f, ClippedCanvasRect.Top) ),
		FMath::TruncToInt( FMath::Max(0.0f, ClippedCanvasRect.Right) ), 
		FMath::TruncToInt( FMath::Max(0.0f, ClippedCanvasRect.Bottom) ) );

	bool bIsRealtime = MaterialNode->RealtimeDelegate.IsBound() ? MaterialNode->RealtimeDelegate.Execute() : false;

	if (PreviewElement->BeginRenderingCanvas(CanvasRect, ClippingRect, MaterialNode, bIsRealtime))
	{
		// Draw above everything else
		uint32 PreviewLayer = LayerId+1;
		FSlateDrawElement::MakeCustom( OutDrawElements, PreviewLayer, PreviewElement );
	}
}

FIntPoint FPreviewViewport::GetSize() const
{
	return FIntPoint(96,96);
}

void FPreviewViewport::UpdatePreviewNodeRenderProxy()
{
	if (PreviewElement.IsValid())
	{
		PreviewElement->UpdateExpressionPreview(MaterialNode);
	}
}

/////////////////////////////////////////////////////
// FPreviewElement

FPreviewElement::FPreviewElement()
	: RenderTarget(new FSlateMaterialPreviewRenderTarget)
	, ExpressionPreview(nullptr)
	, bIsRealtime(false)
{
}

FPreviewElement::~FPreviewElement()
{
	delete RenderTarget;
}

bool FPreviewElement::BeginRenderingCanvas( const FIntRect& InCanvasRect, const FIntRect& InClippingRect, UMaterialGraphNode* InGraphNode, bool bInIsRealtime )
{
	if(InCanvasRect.Size().X > 0 && InCanvasRect.Size().Y > 0 && InClippingRect.Size().X > 0 && InClippingRect.Size().Y > 0 && InGraphNode != NULL)
	{
		/**
		 * Struct to contain all info that needs to be passed to the render thread
		 */
		struct FPreviewRenderInfo
		{
			/** Size of the Canvas tile */
			FIntRect CanvasRect;
			/** How to clip the canvas tile */
			FIntRect ClippingRect;
			/** Render proxy for the expression preview */
			FMaterialRenderProxy* RenderProxy;
			/** Whether preview is using realtime values */
			bool bIsRealtime;
		};

		FPreviewRenderInfo RenderInfo;
		RenderInfo.CanvasRect = InCanvasRect;
		RenderInfo.ClippingRect = InClippingRect;
		RenderInfo.RenderProxy = InGraphNode->GetExpressionPreview();
		RenderInfo.bIsRealtime = bInIsRealtime;

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER
		(
			BeginRenderingPreviewCanvas,
			FPreviewElement*, PreviewElement, this, 
			FPreviewRenderInfo, InRenderInfo, RenderInfo,
		{
			PreviewElement->RenderTarget->SetViewRect(InRenderInfo.CanvasRect);
			PreviewElement->RenderTarget->SetClippingRect(InRenderInfo.ClippingRect);
			PreviewElement->ExpressionPreview = InRenderInfo.RenderProxy;
			PreviewElement->bIsRealtime = InRenderInfo.bIsRealtime;
		}
		);
		return true;
	}

	return false;
}

void FPreviewElement::UpdateExpressionPreview(UMaterialGraphNode* MaterialNode)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER
	(
		UpdatePreviewNodeRenderProxy,
		FPreviewElement*, PreviewElement, this,
		FMaterialRenderProxy*, InRenderProxy, MaterialNode ? MaterialNode->GetExpressionPreview() : nullptr,
		{
			PreviewElement->ExpressionPreview = InRenderProxy;
		}
	);
}

void FPreviewElement::DrawRenderThread(FRHICommandListImmediate& RHICmdList, const void* InWindowBackBuffer)
{
	if(ExpressionPreview)
	{
		RenderTarget->SetRenderTargetTexture(*(FTexture2DRHIRef*)InWindowBackBuffer);
		{
			// Check realtime mode for whether to pass current time to canvas
			float CurrentTime = bIsRealtime ? (FApp::GetCurrentTime() - GStartTime) : 0.0f;
			float DeltaTime = bIsRealtime ? FApp::GetDeltaTime() : 0.0f;

			FCanvas Canvas(RenderTarget, NULL, CurrentTime, CurrentTime, DeltaTime, GMaxRHIFeatureLevel);
			{
				Canvas.SetAllowedModes(0);
				Canvas.SetRenderTargetRect(RenderTarget->GetViewRect());
				Canvas.SetRenderTargetScissorRect(RenderTarget->GetClippingRect());

				FCanvasTileItem TileItem(FVector2D::ZeroVector, ExpressionPreview, RenderTarget->GetSizeXY());
				Canvas.DrawItem(TileItem);
			}
			Canvas.Flush_RenderThread(RHICmdList, true);
		}
		RenderTarget->ClearRenderTargetTexture();
		RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	}
}

/////////////////////////////////////////////////////
// SGraphNodeMaterialBase

void SGraphNodeMaterialBase::Construct(const FArguments& InArgs, UMaterialGraphNode* InNode)
{
	this->GraphNode = InNode;
	this->MaterialNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();
}

void SGraphNodeMaterialBase::CreatePinWidgets()
{
	// Create Pin widgets for each of the pins.
	for( int32 PinIndex=0; PinIndex < GraphNode->Pins.Num(); ++PinIndex )
	{
		UEdGraphPin* CurPin = GraphNode->Pins[PinIndex];

		bool bHideNoConnectionPins = false;

		if (OwnerGraphPanelPtr.IsValid())
		{
			bHideNoConnectionPins = OwnerGraphPanelPtr.Pin()->GetPinVisibility() == SGraphEditor::Pin_HideNoConnection;
		}

		const bool bPinHasConections = CurPin->LinkedTo.Num() > 0;

		bool bPinDesiresToBeHidden = CurPin->bHidden || (bHideNoConnectionPins && !bPinHasConections); 

		UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(GraphNode->GetGraph());
		if (MaterialNode && MaterialNode->MaterialExpression && MaterialGraph->MaterialFunction == nullptr)
		{
			bool bIsAMakeAttrNode = MaterialNode->MaterialExpression->IsA(UMaterialExpressionMakeMaterialAttributes::StaticClass());
			bool bIsABreakAttrNode = MaterialNode->MaterialExpression->IsA(UMaterialExpressionBreakMaterialAttributes::StaticClass());
		
			if ((bIsABreakAttrNode && CurPin->Direction == EGPD_Output) || (bIsAMakeAttrNode && CurPin->Direction == EGPD_Input))
			{
				int32 ResultNodeIndex = bIsAMakeAttrNode ? MaterialNode->GetInputIndex(CurPin) : MaterialNode->GetOutputIndex(CurPin);
				bPinDesiresToBeHidden |= !MaterialGraph->MaterialInputs[ResultNodeIndex].IsVisiblePin(MaterialGraph->Material, true);
			}
		}

		if (!bPinDesiresToBeHidden)
		{
			TSharedPtr<SGraphPin> NewPin = CreatePinWidget(CurPin);
			check(NewPin.IsValid());

			this->AddPin(NewPin.ToSharedRef());
		}
	}
}

void SGraphNodeMaterialBase::MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter)
{
	SGraphNode::MoveTo(NewPosition, NodeFilter);

	MaterialNode->MaterialExpression->MaterialExpressionEditorX = MaterialNode->NodePosX;
	MaterialNode->MaterialExpression->MaterialExpressionEditorY = MaterialNode->NodePosY;
	MaterialNode->MaterialExpression->MarkPackageDirty();
	MaterialNode->MaterialDirtyDelegate.ExecuteIfBound();
}

void SGraphNodeMaterialBase::AddPin( const TSharedRef<SGraphPin>& PinToAdd )
{
	PinToAdd->SetOwner( SharedThis(this) );

	if (PinToAdd->GetDirection() == EEdGraphPinDirection::EGPD_Input)
	{
		FMargin Padding = Settings->GetInputPinPadding();
		Padding.Left *= 0.5f;
		Padding.Right = 0.0f;

		LeftNodeBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(Padding)
		[
			PinToAdd
		];
		InputPins.Add(PinToAdd);
	}
	else // Direction == EEdGraphPinDirection::EGPD_Output
	{
		FMargin Padding = Settings->GetOutputPinPadding();
		Padding.Left = 0.0f;
		Padding.Right *= 0.5f;

		RightNodeBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(Padding)
		[
			PinToAdd
		];
		OutputPins.Add(PinToAdd);
	}
}

void SGraphNodeMaterialBase::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox)
{
	if (GraphNode && MainBox.IsValid())
	{
		int32 LeftPinCount = InputPins.Num();
		int32 RightPinCount = OutputPins.Num();

		const float NegativeHPad = FMath::Max<float>(-Settings->PaddingTowardsNodeEdge, 0.0f);
		const float ExtraPad = 0.0f;

		// Place preview widget based on where the least pins are
		if ((LeftPinCount < RightPinCount) || (RightPinCount == 0))
		{
			LeftNodeBox->AddSlot()
			.Padding(FMargin(NegativeHPad + ExtraPad, 0.0f, 0.0f, 0.0f))
			.AutoHeight()
			[
				CreatePreviewWidget()
			];
		}
		else if (LeftPinCount > RightPinCount)
		{
			RightNodeBox->AddSlot()
			.Padding(FMargin(NegativeHPad + ExtraPad, 0.0f, 0.0f, 0.0f))
			.AutoHeight()
			[
				CreatePreviewWidget()
			];
		}
		else
		{
			MainBox->AddSlot()
			.Padding(Settings->GetNonPinNodeBodyPadding())
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					CreatePreviewWidget()
				]
			];
		}
	}
}

void SGraphNodeMaterialBase::SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget)
{
	if (!MaterialNode->MaterialExpression->bHidePreviewWindow)
	{
		DefaultTitleAreaWidget->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(FMargin(5))
		[
			SNew(SCheckBox)
			.OnCheckStateChanged( this, &SGraphNodeMaterialBase::OnExpressionPreviewChanged )
			.IsChecked( IsExpressionPreviewChecked() )
			.Cursor(EMouseCursor::Default)
			.Style(FEditorStyle::Get(), "Graph.Node.AdvancedView")
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					. Image(GetExpressionPreviewArrow())
				]
			]
		];
	}
}

TSharedRef<SWidget> SGraphNodeMaterialBase::CreateNodeContentArea()
{
	// NODE CONTENT AREA
	return SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("NoBorder") )
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.FillWidth(1.0f)
			[
				// LEFT
				SAssignNew(LeftNodeBox, SVerticalBox)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				// RIGHT
				SAssignNew(RightNodeBox, SVerticalBox)
			]
		];
}

TSharedRef<SWidget> SGraphNodeMaterialBase::CreatePreviewWidget()
{
	PreviewViewport.Reset();

	// if this node should currently show a preview
	if (!MaterialNode->MaterialExpression->bHidePreviewWindow && !MaterialNode->MaterialExpression->bCollapsed)
	{
		const float ExpressionPreviewSize = 106.0f;
		const float CentralPadding = 5.0f;

		TSharedPtr<SViewport> ViewportWidget = 
			SNew( SViewport )
			.EnableGammaCorrection(false);

		PreviewViewport = MakeShareable(new FPreviewViewport(MaterialNode));

		// The viewport widget needs an interface so it knows what should render
		ViewportWidget->SetViewportInterface( PreviewViewport.ToSharedRef() );

		return SNew(SBox)
			.WidthOverride(ExpressionPreviewSize)
			.HeightOverride(ExpressionPreviewSize)
			.Visibility(ExpressionPreviewVisibility())
			[
				SNew(SBorder)
				.Padding(CentralPadding)
				.BorderImage( FEditorStyle::GetBrush("NoBorder") )
				[
					ViewportWidget.ToSharedRef()
				]
			];
	}

	return SNullWidget::NullWidget;
}

EVisibility SGraphNodeMaterialBase::ExpressionPreviewVisibility() const
{
	UMaterialExpression* MaterialExpression = MaterialNode->MaterialExpression;
	const bool bShowPreview = !MaterialExpression->bHidePreviewWindow && !MaterialExpression->bCollapsed;
	return bShowPreview ? EVisibility::Visible : EVisibility::Collapsed;
}

void SGraphNodeMaterialBase::OnExpressionPreviewChanged( const ECheckBoxState NewCheckedState )
{
	UMaterialExpression* MaterialExpression = MaterialNode->MaterialExpression;
	const bool bCollapsed = (NewCheckedState != ECheckBoxState::Checked);
	if (MaterialExpression->bCollapsed != bCollapsed)
	{
		UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(MaterialNode->GetGraph());
		MaterialGraph->ToggleCollapsedDelegate.ExecuteIfBound(MaterialExpression);

		// Update the graph node so that preview viewport is created
		UpdateGraphNode();
	}
}

ECheckBoxState SGraphNodeMaterialBase::IsExpressionPreviewChecked() const
{
	return MaterialNode->MaterialExpression->bCollapsed ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
}

const FSlateBrush* SGraphNodeMaterialBase::GetExpressionPreviewArrow() const
{
	return FEditorStyle::GetBrush(MaterialNode->MaterialExpression->bCollapsed ? TEXT("Kismet.TitleBarEditor.ArrowDown") : TEXT("Kismet.TitleBarEditor.ArrowUp"));
}

void SGraphNodeMaterialBase::PopulateMetaTag(FGraphNodeMetaData* TagMeta) const
{
	if (GraphNode != nullptr)
	{
		UMaterialGraph* OuterGraph = MaterialNode->GetTypedOuter<UMaterialGraph>();
		if ((OuterGraph != nullptr) && (MaterialNode->MaterialExpression != nullptr) )
		{
			TagMeta->OuterName = OuterGraph->OriginalMaterialFullName;
			TagMeta->GUID = MaterialNode->MaterialExpression->MaterialExpressionGuid;
			TagMeta->Tag = FName(*FString::Printf(TEXT("MaterialExprNode_%s_%s"), *TagMeta->OuterName, *TagMeta->GUID.ToString()));
		}
		TagMeta->FriendlyName = FString::Printf(TEXT("%s expression node in %s"), *GraphNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString(), *TagMeta->OuterName);		
	}
}
