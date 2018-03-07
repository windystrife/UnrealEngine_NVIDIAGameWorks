// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorModule.h"
#include "Modules/ModuleManager.h"

#include "GraphEditorActions.h"
#include "SGraphEditorImpl.h"

IMPLEMENT_MODULE(FGraphEditorModule, GraphEditor);

#define LOCTEXT_NAMESPACE "GraphEditorModule"

/////////////////////////////////////////////////////
// FGraphEditorModule

void FGraphEditorModule::StartupModule()
{
	FGraphEditorCommands::Register();

	TArray< TWeakPtr<SGraphEditor> >& Instances = SGraphEditor::AllInstances;
	for (auto InstanceIt = SGraphEditor::AllInstances.CreateIterator(); InstanceIt; ++InstanceIt)
	{
		TWeakPtr<SGraphEditor>& Instance = *InstanceIt;
		if (Instance.IsValid())
		{
			Instance.Pin()->OnModuleReloaded();
		}		
	}
}

void FGraphEditorModule::ShutdownModule()
{
	// Notify all the instances of GraphEditor that their code is about to be unloaded.
	for (auto InstanceIt = SGraphEditor::AllInstances.CreateIterator(); InstanceIt; ++InstanceIt)
	{
		TWeakPtr<SGraphEditor>& Instance = *InstanceIt;
		if (Instance.IsValid())
		{
			Instance.Pin()->OnModuleUnloading();
		}		
	}

	FGraphEditorCommands::Unregister();
}

/**
 * DO NOT CALL THIS METHOD. Use SNew(SGraphEditor) to make instances of SGraphEditor.
 *
 * @return A GraphEditor implementation.
 */
TSharedRef<SGraphEditor> FGraphEditorModule::PRIVATE_MakeGraphEditor( 
	const TSharedPtr<FUICommandList>& InAdditionalCommands, 
	const TAttribute<bool>& InIsEditable,
	const TAttribute<bool>& InDisplayAsReadOnly,
	const TAttribute<bool>& InIsEmpty,
	TAttribute<FGraphAppearanceInfo> Appearance,
	TSharedPtr<SWidget> InTitleBar,
	UEdGraph* InGraphToEdit,
	SGraphEditor::FGraphEditorEvents InGraphEvents,
	bool InAutoExpandActionMenu,
	UEdGraph* InGraphToDiff,
	FSimpleDelegate InOnNavigateHistoryBack,
	FSimpleDelegate InOnNavigateHistoryForward,
	TAttribute<bool> ShowGraphStateOverlay)
{
	return
		SNew(SGraphEditorImpl)
		.AdditionalCommands(InAdditionalCommands)
		.IsEditable(InIsEditable)
		.DisplayAsReadOnly(InDisplayAsReadOnly)
		.Appearance(Appearance)
		.TitleBar(InTitleBar)
		.GraphToEdit(InGraphToEdit)
		.GraphEvents(InGraphEvents)
		.AutoExpandActionMenu(InAutoExpandActionMenu)
		.GraphToDiff(InGraphToDiff)
		.OnNavigateHistoryBack(InOnNavigateHistoryBack)
		.OnNavigateHistoryForward(InOnNavigateHistoryForward)
		.ShowGraphStateOverlay(ShowGraphStateOverlay);
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE 
