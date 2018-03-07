// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealWidget.h"
#include "AnimNodeEditMode.h"

class FFabrikEditMode : public FAnimNodeEditMode
{
public:
	/** IAnimNodeEditMode interface */
	virtual void EnterMode(class UAnimGraphNode_Base* InEditorNode, struct FAnimNode_Base* InRuntimeNode) override;
	virtual void ExitMode() override;
	virtual FVector GetWidgetLocation() const override;
	virtual FWidget::EWidgetMode GetWidgetMode() const override;
	virtual void DoTranslation(FVector& InTranslation) override;

private:
	struct FAnimNode_Fabrik* RuntimeNode;
	class UAnimGraphNode_Fabrik* GraphNode;
};
