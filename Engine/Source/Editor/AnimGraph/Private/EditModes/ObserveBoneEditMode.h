// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealWidget.h"
#include "AnimNodeEditMode.h"

class FObserveBoneEditMode : public FAnimNodeEditMode
{
public:
	/** IAnimNodeEditMode interface */
	virtual void EnterMode(class UAnimGraphNode_Base* InEditorNode, struct FAnimNode_Base* InRuntimeNode) override;
	virtual void ExitMode() override;
	virtual ECoordSystem GetWidgetCoordinateSystem() const override;
	virtual FVector GetWidgetLocation() const override;
	virtual FWidget::EWidgetMode GetWidgetMode() const override;
	virtual FName GetSelectedBone() const override;

private:
	struct FAnimNode_ObserveBone* RuntimeNode;
	class UAnimGraphNode_ObserveBone* GraphNode;
};
