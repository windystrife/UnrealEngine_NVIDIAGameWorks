// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealWidget.h"
#include "IPersonaEditMode.h"

/** Base interface for skeletal control edit modes */
class IAnimNodeEditMode : public IPersonaEditMode
{
public:
	/** Returns the coordinate system that should be used for this bone */
	virtual ECoordSystem GetWidgetCoordinateSystem() const = 0;

	/** @return current widget mode this anim graph node supports */
	virtual FWidget::EWidgetMode GetWidgetMode() const = 0;

	/** Called when the user changed widget mode by pressing "Space" key */
	virtual FWidget::EWidgetMode ChangeToNextWidgetMode(FWidget::EWidgetMode CurWidgetMode) = 0;

	/** Called when the user set widget mode directly, returns true if InWidgetMode is available */
	virtual bool SetWidgetMode(FWidget::EWidgetMode InWidgetMode) = 0;

	/** Get the bone that the skeletal control is manipulating */
	virtual FName GetSelectedBone() const = 0;

	/** Called when the widget is dragged in translation mode */
	virtual void DoTranslation(FVector& InTranslation) = 0;

	/** Called when the widget is dragged in rotation mode */
	virtual void DoRotation(FRotator& InRotation) = 0;

	/** Called when the widget is dragged in scale mode */
	virtual void DoScale(FVector& InScale) = 0;

	/** Called when entering this edit mode */
	virtual void EnterMode(class UAnimGraphNode_Base* InEditorNode, struct FAnimNode_Base* InRuntimeNode) = 0;

	/** Called when exiting this edit mode */
	virtual void ExitMode() = 0;
};
