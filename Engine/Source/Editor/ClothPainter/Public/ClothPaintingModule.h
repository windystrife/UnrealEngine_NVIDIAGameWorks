// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"

#include "WorkflowOrientedApp/WorkflowCentricApplication.h"

class FApplicationMode;
class UEditorExperimentalSettings;
class ISkeletalMeshEditor;
class SClothPaintTab;

const static FName PaintModeID = "ClothPaintMode";

class FClothPaintingModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// Setup and register our editmode
	void SetupMode();
	
	// Unregister and shut down our edit mode
	void ShutdownMode();

protected:

	// Extends the skeletal mesh editor mode
	TSharedRef<FApplicationMode> ExtendApplicationMode(const FName ModeName, TSharedRef<FApplicationMode> InMode);
protected:

	TArray<TWeakPtr<FApplicationMode>> RegisteredApplicationModes;
	FWorkflowApplicationModeExtender Extender;

private:

	// Extends a skeletal mesh editor instance toolbar
	TSharedRef<FExtender> ExtendSkelMeshEditorToolbar(const TSharedRef<FUICommandList> InCommandList, TSharedRef<ISkeletalMeshEditor> InSkeletalMeshEditor);

	// Handle for the extender delegate we created
	FDelegateHandle SkelMeshEditorExtenderHandle;

	// Gets text for the enable paint tools button
	FText GetPaintToolsButtonText(TWeakPtr<ISkeletalMeshEditor> InSkeletalMeshEditor) const;

	// Whether paint mode is active
	bool GetIsPaintToolsButtonChecked(TWeakPtr<ISkeletalMeshEditor> InSkeletalMeshEditor) const;

	// Toggles paint mode on the clothing tab
	void OnToggleMode(TWeakPtr<ISkeletalMeshEditor> InSkeletalMeshEditor);

	// Gets the current active clothing tab, will invoke (spawn or draw attention to) if bInvoke == true
	TSharedPtr<SClothPaintTab> GetActiveClothTab(TWeakPtr<ISkeletalMeshEditor> InSkeletalMeshEditor, bool bInvoke = true) const;
};
