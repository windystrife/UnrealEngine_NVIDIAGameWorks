// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealWidget.h"
#include "AnimNodeEditMode.h"
#include "BoneControllers/AnimNode_ModifyBone.h"

class FModifyBoneEditMode : public FAnimNodeEditMode
{
public:
	FModifyBoneEditMode();

	/** IAnimNodeEditMode interface */
	virtual void EnterMode(class UAnimGraphNode_Base* InEditorNode, struct FAnimNode_Base* InRuntimeNode) override;
	virtual void ExitMode() override;
	virtual ECoordSystem GetWidgetCoordinateSystem() const override;
	virtual FVector GetWidgetLocation() const override;
	virtual FWidget::EWidgetMode GetWidgetMode() const override;
	virtual FWidget::EWidgetMode ChangeToNextWidgetMode(FWidget::EWidgetMode InCurWidgetMode) override;
	virtual bool SetWidgetMode(FWidget::EWidgetMode InWidgetMode) override;
	virtual FName GetSelectedBone() const override;
	virtual void DoTranslation(FVector& InTranslation) override;
	virtual void DoRotation(FRotator& InRotation) override;
	virtual void DoScale(FVector& InScale) override;
	virtual bool ShouldDrawWidget() const override;

private:
	// methods to find a valid widget mode for gizmo because doesn't need to show gizmo when the mode is "Ignore"
	FWidget::EWidgetMode FindValidWidgetMode(FWidget::EWidgetMode InWidgetMode) const;
	EBoneModificationMode GetBoneModificationMode(FWidget::EWidgetMode InWidgetMode) const;
	FWidget::EWidgetMode GetNextWidgetMode(FWidget::EWidgetMode InWidgetMode) const;

private:
	struct FAnimNode_ModifyBone* RuntimeNode;
	class UAnimGraphNode_ModifyBone* GraphNode;

	// storing current widget mode 
	mutable FWidget::EWidgetMode CurWidgetMode;
};
