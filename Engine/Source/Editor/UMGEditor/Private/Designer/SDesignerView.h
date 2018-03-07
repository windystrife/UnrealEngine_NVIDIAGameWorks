// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Styling/SlateColor.h"
#include "Layout/SlateRect.h"
#include "Layout/Visibility.h"
#include "Layout/Geometry.h"
#include "Input/Reply.h"
#include "Layout/ArrangedWidget.h"
#include "Widgets/SWidget.h"
#include "UObject/GCObject.h"
#include "Types/SlateStructs.h"
#include "Animation/CurveSequence.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateBrush.h"
#include "Components/Widget.h"
#include "WidgetReference.h"
#include "WidgetBlueprintEditor.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Layout/WidgetPath.h"
#include "IUMGDesigner.h"
#include "DesignerExtension.h"
#include "Designer/SDesignSurface.h"
#include "UMGEditorProjectSettings.h"

class FMenuBuilder;
class FScopedTransaction;
class SBox;
class SCanvas;
class SPaintSurface;
class SRuler;
class SZoomPan;
class UPanelWidget;
class UWidgetBlueprint;
class FHittestGrid;
struct FOnPaintHandlerParams;
struct FWidgetHitResult;

/**
 * The designer for widgets.  Allows for laying out widgets in a drag and drop environment.
 */
class SDesignerView : public SDesignSurface, public FGCObject, public IUMGDesigner
{
public:

	SLATE_BEGIN_ARGS( SDesignerView ) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class FWidgetBlueprintEditor> InBlueprintEditor);
	virtual ~SDesignerView();

	EActiveTimerReturnType EnsureTick(double InCurrentTime, float InDeltaTime);

	TSharedRef<SWidget> CreateOverlayUI();

	// SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	// End of SWidget interface

	void Register(TSharedRef<FDesignerExtension> Extension);

	// IUMGDesigner interface
	virtual float GetPreviewScale() const override;
	virtual const TSet<FWidgetReference>& GetSelectedWidgets() const override;
	virtual FWidgetReference GetSelectedWidget() const override;
	virtual ETransformMode::Type GetTransformMode() const override;
	virtual FGeometry GetDesignerGeometry() const override;
	virtual FVector2D GetWidgetOriginAbsolute() const;
	virtual bool GetWidgetGeometry(const FWidgetReference& Widget, FGeometry& Geometry) const override;
	virtual bool GetWidgetGeometry(const UWidget* PreviewWidget, FGeometry& Geometry) const override;
	virtual bool GetWidgetParentGeometry(const FWidgetReference& Widget, FGeometry& Geometry) const override;
	virtual FGeometry MakeGeometryWindowLocal(const FGeometry& WidgetGeometry) const override;
	virtual void MarkDesignModifed(bool bRequiresRecompile) override;
	virtual void PushDesignerMessage(const FText& Message) override;
	virtual void PopDesignerMessage() override;
	// End of IUMGDesigner interface

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End of FGCObject interface

public:

	/** The width of the preview screen for the UI */
	FOptionalSize GetPreviewAreaWidth() const;

	/** The height of the preview screen for the UI */
	FOptionalSize GetPreviewAreaHeight() const;

	/** The width of the preview widget for the UI */
	FOptionalSize GetPreviewSizeWidth() const;

	/** The height of the preview widget for the UI */
	FOptionalSize GetPreviewSizeHeight() const;

	/** Set the size of the preview screen for the UI */
	void SetPreviewAreaSize(int32 Width, int32 Height);

	void BeginResizingArea();
	void EndResizingArea();

protected:
	virtual void OnPaintBackground(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const override;
	void DrawResolution(const FDebugResolution& Resolution, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	virtual FSlateRect ComputeAreaBounds() const override;
	virtual int32 GetGraphRulePeriod() const override;
	virtual float GetGridScaleAmount() const override;
	virtual int32 GetSnapGridSize() const override;

private:
	/** Establishes the resolution and aspect ratio to use on construction from config settings */
	void SetStartupResolution();

	FVector2D GetAreaResizeHandlePosition() const;
	EVisibility GetAreaResizeHandleVisibility() const;

	const FSlateBrush* GetPreviewBackground() const;

	void GetPreviewAreaAndSize(FVector2D& Area, FVector2D& Size) const;

	/** Gets the DPI scale that would be applied given the current preview width and height */
	float GetPreviewDPIScale() const;

	/** Adds any pending selected widgets to the selection set */
	void ResolvePendingSelectedWidgets();

	/** Updates the designer to display the latest preview widget */
	void UpdatePreviewWidget(bool bForceUpdate);

	void BroadcastDesignerChanged();

	void ClearExtensionWidgets();
	void CreateExtensionWidgetsForSelection();

	EVisibility GetInfoBarVisibility() const;
	FText GetInfoBarText() const;

	/** Displays the context menu when you right click */
	void ShowContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	void OnEditorSelectionChanged();

	/** Called when a new widget is being hovered */
	void OnHoveredWidgetSet(const FWidgetReference& InHoveredWidget);

	/** Called when a widget is no longer being hovered */
	void OnHoveredWidgetCleared();

	/** Gets the blueprint being edited by the designer */
	UWidgetBlueprint* GetBlueprint() const;

	/** Called whenever the blueprint is recompiled or the preview is updated */
	void OnPreviewNeedsRecreation();

	void PopulateWidgetGeometryCache(FArrangedWidget& Root);
	void PopulateWidgetGeometryCache_Loop(FArrangedWidget& Parent, int32 ParentHitTestIndex);

	/** @return Formatted text for the given resolution params */
	FText GetResolutionText(int32 Width, int32 Height, const FString& AspectRatio) const;

	/** Move the selected widget by the nudge amount. */
	FReply NudgeSelectedWidget(FVector2D Nudge);

	FText GetCurrentResolutionText() const;
	FText GetCurrentDPIScaleText() const;
	FSlateColor GetResolutionTextColorAndOpacity() const;
	EVisibility GetResolutionTextVisibility() const;

	TOptional<int32> GetCustomResolutionWidth() const;
	TOptional<int32> GetCustomResolutionHeight() const;
	void OnCustomResolutionWidthChanged(int32 InValue);
	void OnCustomResolutionHeightChanged(int32 InValue);
	EVisibility GetCustomResolutionEntryVisibility() const;
	
	// Handles selecting a common screen resolution.
	void HandleOnCommonResolutionSelected(int32 Width, int32 Height, FString AspectRatio);
	bool HandleIsCommonResolutionSelected(int32 Width, int32 Height) const;
	void AddScreenResolutionSection(FMenuBuilder& MenuBuilder, const TArray<FPlayScreenResolution>& Resolutions, const FText& SectionName);
	TSharedRef<SWidget> GetResolutionsMenu();

	TSharedRef<SWidget> GetScreenSizingFillMenu();
	void CreateScreenFillEntry(FMenuBuilder& MenuBuilder, EDesignPreviewSizeMode SizeMode);
	FText GetScreenSizingFillText() const;
	bool GetIsScreenFillRuleSelected(EDesignPreviewSizeMode SizeMode) const;
	void OnScreenFillRuleSelected(EDesignPreviewSizeMode SizeMode);

	EVisibility GetDesignerOutlineVisibility() const;
	FSlateColor GetDesignerOutlineColor() const;
	FText GetDesignerOutlineText() const;

	// Handles drawing selection and other effects a SPaintSurface widget injected into the hierarchy.
	int32 HandleEffectsPainting(const FOnPaintHandlerParams& PaintArgs);
	void DrawSelectionAndHoverOutline(const FOnPaintHandlerParams& PaintArgs);
	void DrawSafeZone(const FOnPaintHandlerParams& PaintArgs);

	FReply HandleDPISettingsClicked();

	UUserWidget* GetDefaultWidget() const;

	void BeginTransaction(const FText& SessionName);
	bool InTransaction() const;
	void EndTransaction(bool bCancel);

	UWidget* GetWidgetInDesignScopeFromSlateWidget(TSharedRef<SWidget>& InWidget);

private:
	struct FWidgetHitResult
	{
	public:
		FWidgetReference Widget;
		FArrangedWidget WidgetArranged;

		FName NamedSlot;

	public:
		FWidgetHitResult();
	};

	/** @returns Gets the widget under the cursor based on a mouse pointer event. */
	bool FindWidgetUnderCursor(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, TSubclassOf<UWidget> FindType, FWidgetHitResult& HitResult);

private:
	FReply HandleZoomToFitClicked();
	EVisibility GetRulerVisibility() const;

private:
	static const FString ConfigSectionName;
	static const uint32 DefaultResolutionWidth;
	static const uint32 DefaultResolutionHeight;
	static const FString DefaultAspectRatio;

	/** Extensions for the designer to allow for custom widgets to be inserted onto the design surface as selection changes. */
	TArray< TSharedRef<FDesignerExtension> > DesignerExtensions;

private:
	void BindCommands();

	void SetTransformMode(ETransformMode::Type InTransformMode);
	bool CanSetTransformMode(ETransformMode::Type InTransformMode) const;
	bool IsTransformModeActive(ETransformMode::Type InTransformMode) const;

	void ToggleShowingOutlines();
	bool IsShowingOutlines() const;

	void ToggleRespectingLocks();
	bool IsRespectingLocks() const;

	void ProcessDropAndAddWidget(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, const bool bIsPreview);

	FVector2D GetExtensionPosition(TSharedRef<FDesignerSurfaceElement> ExtensionElement) const;

	FVector2D GetExtensionSize(TSharedRef<FDesignerSurfaceElement> ExtensionElement) const;
	
	void ClearDropPreviews();

	void DetermineDragDropPreviewWidgets(TArray<UWidget*>& OutWidgets, const FDragDropEvent& DragDropEvent);

private:
	/** A reference to the BP Editor that owns this designer */
	TWeakPtr<FWidgetBlueprintEditor> BlueprintEditor;

	/** The designer command list */
	TSharedPtr<FUICommandList> CommandList;

	/** The transaction used to commit undoable actions from resize, move...etc */
	FScopedTransaction* ScopedTransaction;

	/** The current preview widget */
	UUserWidget* PreviewWidget;

	/** The current preview widget's slate widget */
	TWeakPtr<SWidget> PreviewSlateWidget;
	
	struct FDropPreview
	{
		UWidget* Widget;
		UPanelWidget* Parent;
		TWeakPtr<FDragDropOperation> DragOperation;
	};

	TArray<FDropPreview> DropPreviews;

	TSharedPtr<SWidget> PreviewHitTestRoot;
	TSharedPtr<SBox> PreviewAreaConstraint;
	TSharedPtr<SDPIScaler> PreviewSurface;

	TSharedPtr<SCanvas> DesignerControls;
	TSharedPtr<SCanvas> DesignerWidgetCanvas;
	TSharedPtr<SCanvas> ExtensionWidgetCanvas;
	TSharedPtr<SPaintSurface> EffectsLayer;

	/** The currently selected preview widgets in the preview GUI, just a cache used to determine changes between selection changes. */
	TSet< FWidgetReference > SelectedWidgetsCache;

	/** The location in selected widget local space where the context menu was summoned. */
	FVector2D SelectedWidgetContextMenuLocation;

	/**
	 * Holds onto a temporary widget that the user may be getting ready to select, or may just 
	 * be the widget that got hit on the initial mouse down before moving the parent.
	 */
	FWidgetReference PendingSelectedWidget;

	/** The position in screen space where the user began dragging a widget */
	FVector2D DraggingStartPositionScreenSpace;

	/** An existing widget is being moved in its current container, or in to a new container. */
	bool bMovingExistingWidget;

	/** The configured Width of the preview area, simulates screen size. */
	int32 PreviewWidth;

	/** The configured Height of the preview area, simulates screen size. */
	int32 PreviewHeight;

	/***/
	bool bShowResolutionOutlines;

	/** The slate brush we use to hold the background image shown in the designer. */
	mutable FSlateBrush BackgroundImage;

	/** We cache the desired preview desired size to maintain the same size between compiles when it lags a frame behind and no widget is available. */
	FVector2D CachedPreviewDesiredSize;

	// Resolution Info
	FString PreviewAspectRatio;

	/** Curve to handle fading of the resolution */
	FCurveSequence ResolutionTextFade;

	/** Curve to handle the fade-in of the border around the hovered widget */
	FCurveSequence HoveredWidgetOutlineFade;

	/**  */
	FWeakWidgetPath SelectedWidgetPath;

	/** The ruler bar at the top of the designer. */
	TSharedPtr<SRuler> TopRuler;

	/** The ruler bar on the left side of the designer. */
	TSharedPtr<SRuler> SideRuler;

	/** */
	ETransformMode::Type TransformMode;

	/**  */
	TMap<TSharedRef<SWidget>, FArrangedWidget> CachedWidgetGeometry;

	/** */
	TSharedPtr<FHittestGrid> DesignerHittestGrid;

	/** The message stack to display the last item to the user in a non-modal fashion. */
	TArray<FText> DesignerMessageStack;
};
