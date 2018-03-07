// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "UObject/GCObject.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "PreviewScene.h"
#include "Framework/Commands/UICommandList.h"
#include "EditorViewportClient.h"
#include "SEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"

class UNiagaraComponent;
class FNiagaraSystemEditorViewportClient;
class FNiagaraSystemInstance;

/**
 * Material Editor Preview viewport widget
 */
class SNiagaraSystemViewport : public SEditorViewport, public FGCObject, public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS( SNiagaraSystemViewport ){}
	//SLATE_ARGUMENT(TWeakPtr<INiagaraSystemEditor>, SystemEditor)
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs);
	~SNiagaraSystemViewport();
	
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	void RefreshViewport();
	
	void SetPreviewComponent(UNiagaraComponent* NiagaraComponent);
	
	void ToggleRealtime();
	
	/** @return The list of commands known by the niagara editor */
	TSharedRef<FUICommandList> GetNiagaraSystemEditorCommands() const;
	
	/** If true, render background object in the preview scene. */
	bool bShowBackground;
	
	/** If true, render grid the preview scene. */
	bool bShowGrid;
	
	TSharedRef<class FAdvancedPreviewScene> GetPreviewScene() { return AdvancedPreviewScene.ToSharedRef(); }

	/** The material editor has been added to a tab */
	void OnAddedToTab( const TSharedRef<SDockTab>& OwnerTab );
	
	/** Event handlers */
	void TogglePreviewGrid();
	bool IsTogglePreviewGridChecked() const;
	void TogglePreviewBackground();
	bool IsTogglePreviewBackgroundChecked() const;
	class UNiagaraComponent *GetPreviewComponent()	{ return PreviewComponent;  }

	// ICommonEditorViewportToolbarInfoProvider interface
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;
	// End of ICommonEditorViewportToolbarInfoProvider interface
protected:
	/** SEditorViewport interface */
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual EVisibility OnGetViewportContentVisibility() const override;
	virtual void BindCommands() override;
	virtual void OnFocusViewportToSelection() override;
	virtual void PopulateViewportOverlays(TSharedRef<class SOverlay> Overlay) override;

private:
	/** The parent tab where this viewport resides */
	TWeakPtr<SDockTab> ParentTab;
	
	/** Preview Scene - uses advanced preview settings */
	TSharedPtr<class FAdvancedPreviewScene> AdvancedPreviewScene;
	
	bool IsVisible() const override;
	
	/** Pointer back to the material editor tool that owns us */
	//TWeakPtr<INiagaraSystemEditor> SystemEditorPtr;
	
	class UNiagaraComponent *PreviewComponent;
	
	/** Level viewport client */
	TSharedPtr<class FNiagaraSystemViewportClient> SystemViewportClient;
};
