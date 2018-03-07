// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/GCObject.h"
#include "UnrealClient.h"

class FCanvas;
class ITextureEditorToolkit;
class STextureEditorViewport;
class UTexture2D;

class FTextureEditorViewportClient
	: public FViewportClient
	, public FGCObject
{
public:
	/** Constructor */
	FTextureEditorViewportClient(TWeakPtr<ITextureEditorToolkit> InTextureEditor, TWeakPtr<STextureEditorViewport> InTextureEditorViewport);
	~FTextureEditorViewportClient();

	/** FViewportClient interface */
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas) override;
	virtual bool InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed = 1.0f, bool bGamepad = false) override;
	virtual bool InputGesture(FViewport* Viewport, EGestureEvent GestureType, const FVector2D& GestureDelta, bool bIsDirectionInvertedFromDevice) override;
	virtual UWorld* GetWorld() const override { return nullptr; }

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	/** Modifies the checkerboard texture's data */
	void ModifyCheckerboardTextureColors();

	/** Returns a string representation of the currently displayed textures resolution */
	FText GetDisplayedResolution() const;

	/** Returns the ratio of the size of the Texture texture to the size of the viewport */
	float GetViewportVerticalScrollBarRatio() const;
	float GetViewportHorizontalScrollBarRatio() const;

private:
	/** Updates the states of the scrollbars */
	void UpdateScrollBars();

	/** Returns the positions of the scrollbars relative to the Texture textures */
	FVector2D GetViewportScrollBarPositions() const;

	/** Destroy the checkerboard texture if one exists */
	void DestroyCheckerboardTexture();

private:
	/** Pointer back to the Texture editor tool that owns us */
	TWeakPtr<ITextureEditorToolkit> TextureEditorPtr;

	/** Pointer back to the Texture viewport control that owns us */
	TWeakPtr<STextureEditorViewport> TextureEditorViewportPtr;

	/** Checkerboard texture */
	UTexture2D* CheckerboardTexture;
};
