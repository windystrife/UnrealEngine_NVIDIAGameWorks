// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorModeInterpolation : Editor mode for setting up interpolation sequences.

=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "EdMode.h"
#include "EditorModeTools.h"

class FCanvas;
class FEditorViewportClient;
class FPrimitiveDrawInterface;
class FSceneView;
class FViewport;
class UInterpGroup;

//////////////////////////////////////////////////////////////////////////
// FEdModeInterpEdit
//////////////////////////////////////////////////////////////////////////

class FEdModeInterpEdit : public FEdMode
{
public:
	FEdModeInterpEdit();
	~FEdModeInterpEdit();

	virtual bool InputKey( FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event ) override;
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void ActorMoveNotify() override;
	virtual void ActorPropChangeNotify() override;
	virtual bool AllowWidgetMove() override;
	virtual void ActorSelectionChangeNotify() override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override;

	virtual void Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI) override;
	virtual void DrawHUD(FEditorViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas) override;

	void CamMoveNotify(FEditorViewportClient* ViewportClient);
	void InitInterpMode(class AMatineeActor* InMatineeActor);
	UNREALED_API void UpdateSelectedActor();

	class AMatineeActor*	MatineeActor;
	class IMatineeBase*		InterpEd;
	bool					bLeavingMode;

private:
	// Grouping is always disabled while in InterpEdit Mode, re-enable the saved value on exit
	bool					bGroupingActiveSaved;

};

//////////////////////////////////////////////////////////////////////////
// FModeTool_InterpEdit
//////////////////////////////////////////////////////////////////////////

class FModeTool_InterpEdit : public FModeTool
{
public:
	FModeTool_InterpEdit();
	~FModeTool_InterpEdit();

	virtual FString GetName() const override { return TEXT("Interp Edit"); }

	/**
	 * @return		true if the key was handled by this editor mode tool.
	 */
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual bool InputAxis(FEditorViewportClient* InViewportClient, FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime) override;
	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override;

	/**
	 * @return		true if the delta was handled by this editor mode tool.
	 */
	virtual bool InputDelta(FEditorViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale) override;
	virtual void SelectNone() override;


	bool bMovingHandle;
	UInterpGroup* DragGroup;
	int32 DragTrackIndex;
	int32 DragKeyIndex;
	bool bDragArriving;
};
