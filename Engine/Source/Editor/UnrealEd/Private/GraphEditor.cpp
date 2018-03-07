// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "GraphEditor.h"
#include "EdGraph/EdGraph.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBox.h"
#include "GraphEditorModule.h"
#include "Widgets/Layout/SMissingWidget.h"

// List of all active GraphEditor wrappers
TArray< TWeakPtr<SGraphEditor> > SGraphEditor::AllInstances;



void SGraphEditor::ConstructImplementation( const FArguments& InArgs )
{
	FGraphEditorModule& GraphEdModule = FModuleManager::LoadModuleChecked<FGraphEditorModule>(TEXT("GraphEditor"));
	
	// Construct the implementation and make it the contents of this widget.
	Implementation = GraphEdModule.PRIVATE_MakeGraphEditor( InArgs._AdditionalCommands, 
		InArgs._IsEditable, 
		InArgs._DisplayAsReadOnly, 
		InArgs._IsEmpty,
		InArgs._Appearance,
		InArgs._TitleBar,
		InArgs._GraphToEdit,
		InArgs._GraphEvents,
		InArgs._AutoExpandActionMenu,
		InArgs._GraphToDiff,
		InArgs._OnNavigateHistoryBack,
		InArgs._OnNavigateHistoryForward,
		InArgs._ShowGraphStateOverlay
		);

	this->ChildSlot
	[
		SNew( SBox )
		.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("GraphEditorPanel")))
		[
			Implementation.ToSharedRef()
		]
	];
}


/**
 * Loads the GraphEditorModule and constructs a GraphEditor as a child of this widget.
 *
 * @param InArgs   Declaration params from which to construct the widget.
 */
void SGraphEditor::Construct( const FArguments& InArgs )
{
	EdGraphObj = InArgs._GraphToEdit;
	OnGraphModuleReloadedCallback = InArgs._OnGraphModuleReloaded;

	// Register this widget with the module so that we can gracefully handle the module being unloaded.
	// See OnModuleUnloading()
	RegisterGraphEditor( SharedThis(this) );

	// Register a graph modified handler
	if (EdGraphObj != NULL)
	{
		EdGraphObj->AddOnGraphChangedHandler( FOnGraphChanged::FDelegate::CreateSP( this, &SGraphEditor::OnGraphChanged ) );
	}

	// Make the actual GraphEditor instance
	ConstructImplementation(InArgs);
}

// Invoked to let this widget know that the GraphEditor module has been reloaded
void SGraphEditor::OnModuleReloaded()
{
	OnGraphModuleReloadedCallback.ExecuteIfBound(EdGraphObj);
}

// Invoked to let this widget know that the GraphEditor module is being unloaded.
void SGraphEditor::OnModuleUnloading()
{
	this->ChildSlot
	[
		SMissingWidget::MakeMissingWidget()
	];

	check( Implementation.IsUnique() ); 
	Implementation.Reset();
}

void SGraphEditor::RegisterGraphEditor( const TSharedRef<SGraphEditor>& InGraphEditor )
{
	// Compact the list of GraphEditor instances
	for (int32 WidgetIndex = 0; WidgetIndex < AllInstances.Num(); ++WidgetIndex)
	{
		if (!AllInstances[WidgetIndex].IsValid())
		{
			AllInstances.RemoveAt(WidgetIndex);
			--WidgetIndex;
		}
	}

	AllInstances.Add(InGraphEditor);
}

void SGraphEditor::NotifyPrePropertyChange( const FString& PropertyName )
{
	if (EdGraphObj)
	{
		EdGraphObj->NotifyPreChange(PropertyName);
	}
}

void SGraphEditor::NotifyPostPropertyChange( const FPropertyChangedEvent& PropertyChangedEvent, const FString& PropertyName )
{
	if (EdGraphObj)
	{
		EdGraphObj->NotifyPostChange(PropertyChangedEvent, PropertyName);
	}
}

