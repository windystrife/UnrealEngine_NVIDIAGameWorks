// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Misc/NotifyHook.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "PreviewScene.h"
#include "EditorViewportClient.h"
#include "SEditorViewport.h"

class FDestructibleMeshEditorViewportClient;
class IDestructibleMeshEditor;
class SDockableTab;
class UDestructibleComponent;
class UDestructibleMesh;

/**
 * DestructibleMesh Editor Preview viewport widget
 */
class SDestructibleMeshEditorViewport : public SEditorViewport, public FGCObject, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS( SDestructibleMeshEditorViewport ){}
		SLATE_ARGUMENT(TWeakPtr<IDestructibleMeshEditor>, DestructibleMeshEditor)
		SLATE_ARGUMENT(UDestructibleMesh*, ObjectToEdit)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SDestructibleMeshEditorViewport();
	
	// FGCObject interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	// End of FGCObject interface

	// FNotifyHook interface
	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged) override;
	// End of FNotifyHook interface

	void RefreshViewport();
	
	/** Component for the preview Destructible mesh. */
	class UDestructibleComponent* PreviewComponent;

	/** The parent tab where this viewport resides */
	TWeakPtr<SDockableTab> ParentTab;

	/** 
	 *	Causes chunks at the given PreviewDepth to be displayed in the viewport.  Clamped to the range [0, depth count), where (depth count) = the number of chunk hierarchy depths in the destructible.
	 *
	 *	@param	InPreviewDepth			The desired preview depth to display.
	 */
	void SetPreviewDepth(uint32 InPreviewDepth);

	/** 
	 *	The explode amount (distance relative to the original mesh size to expand out the displayed chunks).  Clamped from below at zero.
	 *
	 *	@param	InExplodeAmount			The desired explode amount.
	 */
	void SetExplodeAmount(float InExplodeAmount);

	/** Retrieves the Destructible mesh component. */
	UDestructibleComponent* GetDestructibleComponent() const;

	/** 
	 *	Sets up the Destructible mesh that the Destructible Mesh editor is viewing.
	 *
	 *	@param	InDestructibleMesh		The Destructible mesh being viewed in the editor.
	 */
	void SetPreviewMesh(UDestructibleMesh* InDestructibleMesh);

	/**
	 *	Updates the preview mesh and other viewport specific settings that go with it.
	 *
	 *	@param	InDestructibleMesh		The Destructible mesh being viewed in the editor.
	 */
	void UpdatePreviewMesh(UDestructibleMesh* InDestructibleMesh);

protected:
	/** SEditorViewport interface */
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual void BindCommands() override;

private:

	/** Determines the visibility of the viewport. */
	bool IsVisible() const override;

	/** Callback for toggling the wireframe mode flag. */
	void SetViewModeWireframe();
	
	/** Callback for checking the wireframe mode flag. */
	bool IsInViewModeWireframeChecked() const;

private:
	/** Pointer back to the DestructibleMesh editor tool that owns us */
	TWeakPtr<IDestructibleMeshEditor> DestructibleMeshEditorPtr;

	/** The scene for this viewport. */
	FPreviewScene PreviewScene;

	/** Level viewport client */
	TSharedPtr<class FDestructibleMeshEditorViewportClient> EditorViewportClient;

	/** The currently selected view mode. */
	EViewModeIndex CurrentViewMode;

	/** The mesh currently under consideration */
	UDestructibleMesh* DestructibleMesh;

	/** The currently selected preview depth. */
	uint32 PreviewDepth;

	/** The explode amount (distance relative to the original mesh size to expand out the displayed chunks).  This should be positive. */
	float ExplodeAmount;
};
