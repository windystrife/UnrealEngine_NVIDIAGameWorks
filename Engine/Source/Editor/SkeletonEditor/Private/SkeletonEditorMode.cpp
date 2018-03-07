// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SkeletonEditorMode.h"
#include "Modules/ModuleManager.h"
#include "PersonaModule.h"
#include "SkeletonEditor.h"
#include "ISkeletonTree.h"
#include "ISkeletonEditorModule.h"
#include "IPersonaToolkit.h"

FSkeletonEditorMode::FSkeletonEditorMode(TSharedRef<FWorkflowCentricApplication> InHostingApp, TSharedRef<ISkeletonTree> InSkeletonTree)
	: FApplicationMode(SkeletonEditorModes::SkeletonEditorMode)
{
	HostingAppPtr = InHostingApp;

	TSharedRef<FSkeletonEditor> SkeletonEditor = StaticCastSharedRef<FSkeletonEditor>(InHostingApp);

	ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	TabFactories.RegisterFactory(SkeletonEditorModule.CreateSkeletonTreeTabFactory(InHostingApp, InSkeletonTree));

	FOnObjectsSelected OnObjectsSelected = FOnObjectsSelected::CreateSP(&SkeletonEditor.Get(), &FSkeletonEditor::HandleObjectsSelected);
	FOnObjectSelected OnObjectSelected = FOnObjectSelected::CreateSP(&SkeletonEditor.Get(), &FSkeletonEditor::HandleObjectSelected);
	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	TabFactories.RegisterFactory(PersonaModule.CreateDetailsTabFactory(InHostingApp, FOnDetailsCreated::CreateSP(&SkeletonEditor.Get(), &FSkeletonEditor::HandleDetailsCreated)));

	FPersonaViewportArgs ViewportArgs(InSkeletonTree, SkeletonEditor->GetPersonaToolkit()->GetPreviewScene(), SkeletonEditor->OnPostUndo);

	TabFactories.RegisterFactory(PersonaModule.CreatePersonaViewportTabFactory(InHostingApp, ViewportArgs));

	TabFactories.RegisterFactory(PersonaModule.CreateAnimNotifiesTabFactory(InHostingApp, InSkeletonTree->GetEditableSkeleton(), SkeletonEditor->OnChangeAnimNotifies, SkeletonEditor->OnPostUndo, OnObjectsSelected));
	TabFactories.RegisterFactory(PersonaModule.CreateAdvancedPreviewSceneTabFactory(InHostingApp, SkeletonEditor->GetPersonaToolkit()->GetPreviewScene()));
	TabFactories.RegisterFactory(PersonaModule.CreateRetargetManagerTabFactory(InHostingApp, InSkeletonTree->GetEditableSkeleton(), SkeletonEditor->GetPersonaToolkit()->GetPreviewScene(), SkeletonEditor->OnPostUndo));
	TabFactories.RegisterFactory(PersonaModule.CreateCurveViewerTabFactory(InHostingApp, InSkeletonTree->GetEditableSkeleton(), SkeletonEditor->GetPersonaToolkit()->GetPreviewScene(), SkeletonEditor->OnPostUndo, OnObjectsSelected));
	TabFactories.RegisterFactory(PersonaModule.CreateSkeletonSlotNamesTabFactory(InHostingApp, InSkeletonTree->GetEditableSkeleton(), SkeletonEditor->OnPostUndo, OnObjectSelected));

	TabLayout = FTabManager::NewLayout("Standalone_SkeletonEditor_Layout_v1.2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.1f)
				->SetHideTabWell(true)
				->AddTab(InHostingApp->GetToolbarTabId(), ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.9f)
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->SetHideTabWell(true)
					->AddTab(SkeletonEditorTabs::SkeletonTreeTab, ETabState::OpenedTab)
					->AddTab(SkeletonEditorTabs::RetargetManagerTab, ETabState::ClosedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.6f)
					->SetHideTabWell(true)
					->AddTab(SkeletonEditorTabs::ViewportTab, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewSplitter()
					->SetSizeCoefficient(0.2f)
					->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->SetHideTabWell(false)
						->AddTab(SkeletonEditorTabs::DetailsTab, ETabState::OpenedTab)
						->AddTab(SkeletonEditorTabs::AdvancedPreviewTab, ETabState::OpenedTab)
						->SetForegroundTab(SkeletonEditorTabs::DetailsTab)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->SetHideTabWell(false)
						->AddTab(SkeletonEditorTabs::AnimNotifiesTab, ETabState::OpenedTab)
						->AddTab(SkeletonEditorTabs::CurveNamesTab, ETabState::OpenedTab)
						->AddTab(SkeletonEditorTabs::SlotNamesTab, ETabState::ClosedTab)
					)
				)
			)
		);
}

void FSkeletonEditorMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FWorkflowCentricApplication> HostingApp = HostingAppPtr.Pin();
	HostingApp->RegisterTabSpawners(InTabManager.ToSharedRef());
	HostingApp->PushTabFactories(TabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FSkeletonEditorMode::AddTabFactory(FCreateWorkflowTabFactory FactoryCreator)
{
	if (FactoryCreator.IsBound())
	{
		TabFactories.RegisterFactory(FactoryCreator.Execute(HostingAppPtr.Pin()));
	}
}

void FSkeletonEditorMode::RemoveTabFactory(FName TabFactoryID)
{
	TabFactories.UnregisterFactory(TabFactoryID);
}
