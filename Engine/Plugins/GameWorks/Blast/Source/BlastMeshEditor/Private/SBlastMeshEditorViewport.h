// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Misc/NotifyHook.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "AdvancedPreviewScene.h"
#include "EditorViewportClient.h"
#include "SEditorViewport.h"
#include "Editor/UnrealEd/Public/SCommonEditorViewportToolbarBase.h"

class FBlastMeshEditorViewportClient;
class IBlastMeshEditor;
class SDockableTab;
class UViewportBlastMeshComponent;
class UBlastMesh;

/**
 * BlastMesh Editor Preview viewport widget
 */
class SBlastMeshEditorViewport : public SEditorViewport, public FGCObject, public FNotifyHook, public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS( SBlastMeshEditorViewport ){}
		SLATE_ARGUMENT(TWeakPtr<IBlastMeshEditor>, BlastMeshEditor)
		SLATE_ARGUMENT(UBlastMesh*, ObjectToEdit)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SBlastMeshEditorViewport();
	
	// FGCObject interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	// End of FGCObject interface

	// FNotifyHook interface
	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged) override;
	// End of FNotifyHook interface

	void RefreshViewport();
	void RedrawViewport();
	void ResetCamera();

	void HandleBlastVector(const struct FBlastVector* Vector);
	void ResetBlastVectorMode(bool ToDefault = false);
	void UpdateBlastVectorValue(FVector NewValue, FVector NewValueInScreenspace);
	const TArray<FVector>& GetBlastVectorValueInScreenSpace() const;
	void SetBlastVectorMode(EBlastViewportControlMode Mode);
	EBlastViewportControlMode GetBlastVectorMode() const;
	bool IsSelectedBlastVectorMode(EBlastViewportControlMode Mode) const;
	bool IsBlastVectorModeSelectable(EBlastViewportControlMode Mode) const;

	/** Component for the preview Blast mesh. */
	class UViewportBlastMeshComponent* PreviewComponent;

	/** The parent tab where this viewport resides */
	TWeakPtr<SDockableTab> ParentTab;

	/** 
	 *	The explode amount (distance relative to the original mesh size to expand out the displayed chunks).  Clamped from below at zero.
	 *
	 *	@param	InExplodeAmount			The desired explode amount.
	 */
	void SetExplodeAmount(float InExplodeAmount);

	/** Retrieves the Blast mesh component. */
	//UViewportBlastMeshComponent* GetBlastComponent() const;

	/** 
	 *	Sets up the Blast mesh that the Blast Mesh editor is viewing.
	 *
	 *	@param	InBlastMesh		The Blast mesh being viewed in the editor.
	 */
	void SetPreviewMesh(UBlastMesh* InBlastMesh);

	/**
	 *	Updates the preview mesh and other viewport specific settings that go with it.
	 *
	 *	@param	InBlastMesh		The Blast mesh being viewed in the editor.
	 */
	void UpdatePreviewMesh(UBlastMesh* InBlastMesh);

	TSharedPtr<FAdvancedPreviewScene> GetPreviewScene() const { return PreviewScene;  }

	// ICommonEditorViewportToolbarInfoProvider interface
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;
	// End of ICommonEditorViewportToolbarInfoProvider interface

	/** SEditorViewport interface */
	virtual EVisibility GetTransformToolbarVisibility() const override;

protected:
	/** SEditorViewport interface */
	virtual void BindCommands() override;
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;

private:

	/** Determines the visibility of the viewport. */
	bool IsVisible() const override;

	/** Callback for toggling the wireframe mode flag. */
	void SetViewModeWireframe();
	
	/** Callback for checking the wireframe mode flag. */
	bool IsInViewModeWireframeChecked() const;

private:
	/** Pointer back to the BlastMesh editor tool that owns us */
	TWeakPtr<IBlastMeshEditor> BlastMeshEditorPtr;

	/** The scene for this viewport. */
	TSharedPtr<FAdvancedPreviewScene> PreviewScene;

	/** Level viewport client */
	TSharedPtr<class FBlastMeshEditorViewportClient> EditorViewportClient;

	/** The currently selected view mode. */
	EViewModeIndex CurrentViewMode;

	/** The mesh currently under consideration */
	UBlastMesh* BlastMesh;

	/** The explode amount (distance relative to the original mesh size to expand out the displayed chunks).  This should be positive. */
	float ExplodeAmount;

	FDelegateHandle	BlastVectorHandle;
	EBlastViewportControlMode BlastVectorMode;
	FBlastVector* BlastVector = nullptr;
	TArray<FVector> BlastVectorPreviouslyClickedValues;
	TArray<FVector> BlastVectorPreviouslyClickedValuesInScreenSpace;

};
