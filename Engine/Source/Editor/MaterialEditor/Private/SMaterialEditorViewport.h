// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "UObject/GCObject.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "PreviewScene.h"
#include "EditorViewportClient.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "SEditorViewport.h"
#include "Editor/UnrealEd/Public/SCommonEditorViewportToolbarBase.h"

class FMaterialEditorViewportClient;
class IMaterialEditor;
class SMaterialEditorUIPreviewZoomer;
class UMaterialInterface;
class UMeshComponent;

/**
 * Material Editor Preview viewport widget
 */
class SMaterialEditor3DPreviewViewport : public SEditorViewport, public FGCObject, public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS( SMaterialEditor3DPreviewViewport ){}
		SLATE_ARGUMENT(TWeakPtr<IMaterialEditor>, MaterialEditor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SMaterialEditor3DPreviewViewport();
	
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	void RefreshViewport();
	
	/**
	 * Sets the mesh on which to preview the material.  One of either InStaticMesh or InSkeletalMesh must be non-NULL!
	 * Does nothing if a skeletal mesh was specified but the material has bUsedWithSkeletalMesh=false.
	 *
	 * @return	true if a mesh was set successfully, false otherwise.
	 */
	bool SetPreviewAsset(UObject* InAsset);

	/**
	 * Sets the preview asset to the named asset, if it can be found and is convertible into a UMeshComponent subclass.
	 * Does nothing if the named asset is not found or if the named asset is a skeletal mesh but the material has
	 * bUsedWithSkeletalMesh=false.
	 *
	 * @return	true if the named asset was found and set successfully, false otherwise.
	 */
	bool SetPreviewAssetByName(const TCHAR* InAssetName);

	void SetPreviewMaterial(UMaterialInterface* InMaterialInterface);

	/** Component for the preview mesh. */
	UMeshComponent* PreviewMeshComponent;

	/** Material for the preview mesh */
	UMaterialInterface* PreviewMaterial;
	
	/** The preview primitive we are using. */
	EThumbnailPrimType PreviewPrimType;
	
	/** If true, render background object in the preview scene. */
	bool bShowBackground;

	/** If true, render grid the preview scene. */
	bool bShowGrid;
	
	/** The material editor has been added to a tab */
	void OnAddedToTab( const TSharedRef<SDockTab>& OwnerTab );

	TSharedRef<class FAdvancedPreviewScene> GetPreviewScene() { return AdvancedPreviewScene.ToSharedRef(); }

	/** Event handlers */
	void OnSetPreviewPrimitive(EThumbnailPrimType PrimType, bool bInitialLoad = false);
	bool IsPreviewPrimitiveChecked(EThumbnailPrimType PrimType) const;
	void OnSetPreviewMeshFromSelection();
	bool IsPreviewMeshFromSelectionChecked() const;
	void TogglePreviewGrid();
	bool IsTogglePreviewGridChecked() const;
	void TogglePreviewBackground();
	bool IsTogglePreviewBackgroundChecked() const;
	/** Call back for when the user changes preview scene settings in the UI */
	void OnAssetViewerSettingsChanged(const FName& InPropertyName);

	// ICommonEditorViewportToolbarInfoProvider interface
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;
	// End of ICommonEditorViewportToolbarInfoProvider interface

protected:
	/** SEditorViewport interface */
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual void PopulateViewportOverlays(TSharedRef<class SOverlay> Overlay) override;
	virtual EVisibility OnGetViewportContentVisibility() const override;
	virtual void BindCommands() override;
	virtual void OnFocusViewportToSelection() override;
private:
	/** The parent tab where this viewport resides */
	TWeakPtr<SDockTab> ParentTab;

	bool IsVisible() const override;

	/** Pointer back to the material editor tool that owns us */
	TWeakPtr<IMaterialEditor> MaterialEditorPtr;

	/** Level viewport client */
	TSharedPtr<class FMaterialEditorViewportClient> EditorViewportClient;

	/** Preview Scene - uses advanced preview settings */
	TSharedPtr<class FAdvancedPreviewScene> AdvancedPreviewScene;
};

/**
 * A preview viewport used for 2D UI materials
 */
class SMaterialEditorUIPreviewViewport : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SMaterialEditorUIPreviewViewport ){}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, UMaterialInterface* PreviewMaterial );
	void SetPreviewMaterial(UMaterialInterface* InMaterialInterface);

private:
	void OnPreviewXChanged( int32 NewValue );
	void OnPreviewXCommitted( int32 NewValue, ETextCommit::Type );
	void OnPreviewYChanged( int32 NewValue );
	void OnPreviewYCommitted( int32 NewValue, ETextCommit::Type );
	TOptional<int32> OnGetPreviewXValue() const { return PreviewSize.X; }
	TOptional<int32> OnGetPreviewYValue() const { return PreviewSize.Y; }
private:
	FIntPoint PreviewSize;
	TSharedPtr<class SMaterialEditorUIPreviewZoomer> PreviewZoomer;
};
