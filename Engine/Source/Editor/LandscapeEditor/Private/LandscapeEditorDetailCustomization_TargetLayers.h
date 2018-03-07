// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Layout/Margin.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Editor/LandscapeEditor/Private/LandscapeEdMode.h"
#include "IDetailCustomNodeBuilder.h"
#include "IDetailCustomization.h"
#include "AssetThumbnail.h"
#include "Framework/SlateDelegates.h"
#include "Editor/LandscapeEditor/Private/LandscapeEditorDetailCustomization_Base.h"

class FDetailWidgetRow;
class IDetailChildrenBuilder;
class IDetailLayoutBuilder;
class SDragAndDropVerticalBox;

/**
 * Slate widgets customizer for the target layers list in the Landscape Editor
 */

class FLandscapeEditorDetailCustomization_TargetLayers : public FLandscapeEditorDetailCustomization_Base
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
	static bool ShouldShowTargetLayers();
	static bool ShouldShowPaintingRestriction();
	static EVisibility GetVisibility_PaintingRestriction();
	static bool ShouldShowVisibilityTip();
	static EVisibility GetVisibility_VisibilityTip();
};

class FLandscapeEditorCustomNodeBuilder_TargetLayers : public IDetailCustomNodeBuilder, public TSharedFromThis<FLandscapeEditorCustomNodeBuilder_TargetLayers>
{
public:
	FLandscapeEditorCustomNodeBuilder_TargetLayers(TSharedRef<FAssetThumbnailPool> ThumbnailPool, TSharedRef<IPropertyHandle> InTargetDisplayOrderPropertyHandle, TSharedRef<IPropertyHandle> InTargetShowUnusedLayersPropertyHandle);
	~FLandscapeEditorCustomNodeBuilder_TargetLayers();

	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRegenerateChildren ) override;
	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) override;
	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) override;
	virtual void Tick( float DeltaTime ) override {}
	virtual bool RequiresTick() const override { return false; }
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const override { return "TargetLayers"; }

protected:
	TSharedRef<FAssetThumbnailPool> ThumbnailPool;
	TSharedRef<IPropertyHandle> TargetDisplayOrderPropertyHandle;
	TSharedRef<IPropertyHandle> TargetShowUnusedLayersPropertyHandle;

	static class FEdModeLandscape* GetEditorMode();

	TSharedPtr<SWidget> GenerateRow(const TSharedRef<FLandscapeTargetListInfo> Target);

	static bool GetTargetLayerIsSelected(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnTargetSelectionChanged(const TSharedRef<FLandscapeTargetListInfo> Target);
	static TSharedPtr<SWidget> OnTargetLayerContextMenuOpening(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnExportLayer(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnImportLayer(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnReimportLayer(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnFillLayer(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void FillEmptyLayers(ULandscapeInfo* LandscapeInfo, ULandscapeLayerInfoObject* LandscapeInfoObject);
	static void OnClearLayer(const TSharedRef<FLandscapeTargetListInfo> Target);
	static bool ShouldFilterLayerInfo(const struct FAssetData& AssetData, FName LayerName);
	static void OnTargetLayerSetObject(const FAssetData& AssetData, const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetTargetLayerInfoSelectorVisibility(const TSharedRef<FLandscapeTargetListInfo> Target);
	static bool GetTargetLayerCreateEnabled(const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetTargetLayerMakePublicVisibility(const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetTargetLayerDeleteVisibility(const TSharedRef<FLandscapeTargetListInfo> Target);
	static TSharedRef<SWidget> OnGetTargetLayerCreateMenu(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnTargetLayerCreateClicked(const TSharedRef<FLandscapeTargetListInfo> Target, bool bNoWeightBlend);
	static FReply OnTargetLayerMakePublicClicked(const TSharedRef<FLandscapeTargetListInfo> Target);
	static FReply OnTargetLayerDeleteClicked(const TSharedRef<FLandscapeTargetListInfo> Target);
	static FSlateColor GetLayerUsageDebugColor(const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetDebugModeLayerUsageVisibility(const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetDebugModeLayerUsageVisibility_Invert(const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetDebugModeColorChannelVisibility(const TSharedRef<FLandscapeTargetListInfo> Target);
	static ECheckBoxState DebugModeColorChannelIsChecked(const TSharedRef<FLandscapeTargetListInfo> Target, int32 Channel);
	static void OnDebugModeColorChannelChanged(ECheckBoxState NewCheckedState, const TSharedRef<FLandscapeTargetListInfo> Target, int32 Channel);

	// Drag/Drop handling
	FReply HandleDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, int32 SlotIndex, SVerticalBox::FSlot* Slot);
	TOptional<SDragAndDropVerticalBox::EItemDropZone> HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, SDragAndDropVerticalBox::EItemDropZone DropZone, SVerticalBox::FSlot* Slot);
	FReply HandleAcceptDrop(FDragDropEvent const& DragDropEvent, SDragAndDropVerticalBox::EItemDropZone DropZone, int32 SlotIndex, SVerticalBox::FSlot* Slot);

	const FSlateBrush* GetTargetLayerDisplayOrderBrush() const;
	TSharedRef<SWidget> GetTargetLayerDisplayOrderButtonMenuContent();
	void SetSelectedDisplayOrder(ELandscapeLayerDisplayMode InDisplayOrder);
	bool IsSelectedDisplayOrder(ELandscapeLayerDisplayMode InDisplayOrder) const;

	TSharedRef<SWidget> GetTargetLayerShowUnusedButtonMenuContent();
	void ShowUnusedLayers(bool Result);
	bool ShouldShowUnusedLayers(bool Result) const;
	EVisibility ShouldShowLayer(TSharedRef<FLandscapeTargetListInfo> Target) const;
};

class SLandscapeEditorSelectableBorder : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SLandscapeEditorSelectableBorder)
		: _Content()
		, _HAlign(HAlign_Fill)
		, _VAlign(VAlign_Fill)
		, _Padding(FMargin(2.0f))
	{ }
		SLATE_DEFAULT_SLOT(FArguments, Content)

		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_ATTRIBUTE(FMargin, Padding)

		SLATE_EVENT(FOnContextMenuOpening, OnContextMenuOpening)
		SLATE_EVENT(FSimpleDelegate, OnSelected)
		SLATE_ATTRIBUTE(bool, IsSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	const FSlateBrush* GetBorder() const;

protected:
	FOnContextMenuOpening OnContextMenuOpening;
	FSimpleDelegate OnSelected;
	TAttribute<bool> IsSelected;
};

class FTargetLayerDragDropOp : public FDragAndDropVerticalBoxOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FTargetLayerDragDropOp, FDragAndDropVerticalBoxOp)

	TSharedPtr<SWidget> WidgetToShow;

	static TSharedRef<FTargetLayerDragDropOp> New(int32 InSlotIndexBeingDragged, SVerticalBox::FSlot* InSlotBeingDragged, TSharedPtr<SWidget> InWidgetToShow);

public:
	virtual ~FTargetLayerDragDropOp();

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;
};