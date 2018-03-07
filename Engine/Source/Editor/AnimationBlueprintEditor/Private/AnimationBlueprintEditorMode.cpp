// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimationBlueprintEditorMode.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/AnimInstance.h"



#include "IPersonaToolkit.h"
#include "BlueprintEditorTabs.h"
#include "ISkeletonEditorModule.h"
#include "PersonaModule.h"
#include "SBlueprintEditorToolbar.h"
#include "IPersonaPreviewScene.h"
/////////////////////////////////////////////////////
// FAnimationBlueprintEditorMode

FAnimationBlueprintEditorMode::FAnimationBlueprintEditorMode(const TSharedRef<FAnimationBlueprintEditor>& InAnimationBlueprintEditor)
	: FBlueprintEditorApplicationMode(InAnimationBlueprintEditor, FAnimationBlueprintEditorModes::AnimationBlueprintEditorMode, FAnimationBlueprintEditorModes::GetLocalizedMode, false, false)
{
	PreviewScenePtr = InAnimationBlueprintEditor->GetPreviewScene();
	AnimBlueprintPtr = CastChecked<UAnimBlueprint>(InAnimationBlueprintEditor->GetBlueprintObj());

	TabLayout = FTabManager::NewLayout( "Stanalone_AnimationBlueprintEditMode_Layout_v1.3" )
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				// Top toolbar
				FTabManager::NewStack() 
				->SetSizeCoefficient(0.186721f)
				->SetHideTabWell(true)
				->AddTab(InAnimationBlueprintEditor->GetToolbarTabId(), ETabState::OpenedTab )
			)
			->Split
			(
				// Main application area
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					// Left side
					FTabManager::NewSplitter()
					->SetSizeCoefficient(0.25f)
					->SetOrientation(Orient_Vertical)
					->Split
					(
						// Left top - viewport
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->SetHideTabWell(true)
						->AddTab(AnimationBlueprintEditorTabs::ViewportTab, ETabState::OpenedTab)
					)
					->Split
					(
						//	Left bottom - preview settings
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->AddTab(AnimationBlueprintEditorTabs::CurveNamesTab, ETabState::ClosedTab)
						->AddTab(AnimationBlueprintEditorTabs::SkeletonTreeTab, ETabState::ClosedTab)
						->AddTab(FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab)
					)
				)
				->Split
				(
					// Middle 
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.55f)
					->Split
					(
						// Middle top - document edit area
						FTabManager::NewStack()
						->SetSizeCoefficient(0.8f)
						->AddTab("Document", ETabState::ClosedTab)
					)
					->Split
					(
						// Middle bottom - compiler results & find
						FTabManager::NewStack()
						->SetSizeCoefficient(0.2f)
						->AddTab(FBlueprintEditorTabs::CompilerResultsID, ETabState::ClosedTab)
						->AddTab(FBlueprintEditorTabs::FindResultsID, ETabState::ClosedTab)
					)
				)
				->Split
				(
					// Right side
					FTabManager::NewSplitter()
					->SetSizeCoefficient(0.2f)
					->SetOrientation(Orient_Vertical)
					->Split
					(
						// Right top - selection details panel & overrides
						FTabManager::NewStack()
						->SetHideTabWell(false)
						->SetSizeCoefficient(0.5f)
						->AddTab(FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab)
						->AddTab(AnimationBlueprintEditorTabs::AdvancedPreviewTab, ETabState::OpenedTab)
						->AddTab(AnimationBlueprintEditorTabs::AssetOverridesTab, ETabState::ClosedTab)
						->SetForegroundTab(FBlueprintEditorTabs::DetailsID)
					)
					->Split
					(
						// Right bottom - Asset browser & advanced preview settings
						FTabManager::NewStack()
						->SetHideTabWell(false)
						->SetSizeCoefficient(0.5f)
						->AddTab(AnimationBlueprintEditorTabs::AnimBlueprintPreviewEditorTab, ETabState::OpenedTab)
						->AddTab(AnimationBlueprintEditorTabs::AssetBrowserTab, ETabState::OpenedTab)
						->AddTab(AnimationBlueprintEditorTabs::SlotNamesTab, ETabState::ClosedTab)
						->SetForegroundTab(AnimationBlueprintEditorTabs::AnimBlueprintPreviewEditorTab)
					)
				)
			)
		);

	ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	TabFactories.RegisterFactory(SkeletonEditorModule.CreateSkeletonTreeTabFactory(InAnimationBlueprintEditor, InAnimationBlueprintEditor->GetSkeletonTree()));

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");

	FPersonaViewportArgs ViewportArgs(InAnimationBlueprintEditor->GetSkeletonTree(), InAnimationBlueprintEditor->GetPersonaToolkit()->GetPreviewScene(), InAnimationBlueprintEditor->OnPostUndo);
	ViewportArgs.BlueprintEditor = InAnimationBlueprintEditor;
	ViewportArgs.bShowStats = false;

	TabFactories.RegisterFactory(PersonaModule.CreatePersonaViewportTabFactory(InAnimationBlueprintEditor, ViewportArgs));


	TabFactories.RegisterFactory(PersonaModule.CreateAdvancedPreviewSceneTabFactory(InAnimationBlueprintEditor, InAnimationBlueprintEditor->GetPersonaToolkit()->GetPreviewScene()));
	TabFactories.RegisterFactory(PersonaModule.CreateAnimationAssetBrowserTabFactory(InAnimationBlueprintEditor, InAnimationBlueprintEditor->GetPersonaToolkit(), FOnOpenNewAsset::CreateSP(&InAnimationBlueprintEditor.Get(), &FAnimationBlueprintEditor::HandleOpenNewAsset), FOnAnimationSequenceBrowserCreated(), true));
	TabFactories.RegisterFactory(PersonaModule.CreateAnimBlueprintPreviewTabFactory(InAnimationBlueprintEditor, InAnimationBlueprintEditor->GetPersonaToolkit()->GetPreviewScene()));
	TabFactories.RegisterFactory(PersonaModule.CreateAnimBlueprintAssetOverridesTabFactory(InAnimationBlueprintEditor, InAnimationBlueprintEditor->GetPersonaToolkit()->GetAnimBlueprint(), InAnimationBlueprintEditor->OnPostUndo));
	TabFactories.RegisterFactory(PersonaModule.CreateSkeletonSlotNamesTabFactory(InAnimationBlueprintEditor, InAnimationBlueprintEditor->GetSkeletonTree()->GetEditableSkeleton(), InAnimationBlueprintEditor->OnPostUndo, FOnObjectSelected::CreateSP(&InAnimationBlueprintEditor.Get(), &FAnimationBlueprintEditor::HandleObjectSelected)));
	TabFactories.RegisterFactory(PersonaModule.CreateCurveViewerTabFactory(InAnimationBlueprintEditor, InAnimationBlueprintEditor->GetSkeletonTree()->GetEditableSkeleton(), InAnimationBlueprintEditor->GetPersonaToolkit()->GetPreviewScene(), InAnimationBlueprintEditor->OnPostUndo, FOnObjectsSelected::CreateSP(&InAnimationBlueprintEditor.Get(), &FAnimationBlueprintEditor::HandleObjectsSelected)));

	// setup toolbar - clear existing toolbar extender from the BP mode
	//@TODO: Keep this in sync with BlueprintEditorModes.cpp
	ToolbarExtender = MakeShareable(new FExtender);
	InAnimationBlueprintEditor->GetToolbarBuilder()->AddCompileToolbar(ToolbarExtender);
	InAnimationBlueprintEditor->GetToolbarBuilder()->AddScriptingToolbar(ToolbarExtender);
	InAnimationBlueprintEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(ToolbarExtender);
	InAnimationBlueprintEditor->GetToolbarBuilder()->AddDebuggingToolbar(ToolbarExtender);
}

void FAnimationBlueprintEditorMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(CoreTabFactories);
	BP->PushTabFactories(BlueprintEditorTabFactories);
	BP->PushTabFactories(TabFactories);
}

void FAnimationBlueprintEditorMode::PostActivateMode()
{
	if (UAnimBlueprint* AnimBlueprint = AnimBlueprintPtr.Get())
	{
		// Switch off any active preview when going to graph editing mode
		PreviewScenePtr.Pin()->SetPreviewAnimationAsset(NULL, false);

		// When switching to anim blueprint mode, make sure the object being debugged is either a valid world object or the preview instance
		UDebugSkelMeshComponent* PreviewComponent = PreviewScenePtr.Pin()->GetPreviewMeshComponent();
		if ((AnimBlueprint->GetObjectBeingDebugged() == NULL) && (PreviewComponent->IsAnimBlueprintInstanced()))
		{
			AnimBlueprint->SetObjectBeingDebugged(PreviewComponent->GetAnimInstance());
		}

		// If we are a derived anim blueprint always show the overrides tab
		if(UAnimBlueprint::FindRootAnimBlueprint(AnimBlueprint))
		{
			MyBlueprintEditor.Pin()->GetTabManager()->InvokeTab(AnimationBlueprintEditorTabs::AssetOverridesTab);
		}
	}

	FBlueprintEditorApplicationMode::PostActivateMode();
}
