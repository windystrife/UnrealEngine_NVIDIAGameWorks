// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Toolkits/SimpleAssetEditor.h"
#include "Modules/ModuleManager.h"
#include "EditorStyleSet.h"
#include "Widgets/Docking/SDockTab.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Editor.h"


#define LOCTEXT_NAMESPACE "GenericEditor"

const FName FSimpleAssetEditor::ToolkitFName( TEXT( "GenericAssetEditor" ) );
const FName FSimpleAssetEditor::PropertiesTabId( TEXT( "GenericEditor_Properties" ) );

void FSimpleAssetEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_GenericAssetEditor", "Asset Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner( PropertiesTabId, FOnSpawnTab::CreateSP(this, &FSimpleAssetEditor::SpawnPropertiesTab) )
		.SetDisplayName( LOCTEXT("PropertiesTab", "Details") )
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FSimpleAssetEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner( PropertiesTabId );
}

const FName FSimpleAssetEditor::SimpleEditorAppIdentifier( TEXT( "GenericEditorApp" ) );

FSimpleAssetEditor::~FSimpleAssetEditor()
{
	FEditorDelegates::OnAssetPostImport.RemoveAll(this);

	DetailsView.Reset();
	PropertiesTab.Reset();
}


void FSimpleAssetEditor::InitEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit, FGetDetailsViewObjects GetDetailsViewObjects )
{
	const bool bIsUpdatable = false;
	const bool bAllowFavorites = true;
	const bool bIsLockable = false;

	EditingObjects = ObjectsToEdit;
	FEditorDelegates::OnAssetPostImport.AddRaw(this, &FSimpleAssetEditor::HandleAssetPostImport);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	const FDetailsViewArgs DetailsViewArgs( bIsUpdatable, bIsLockable, true, FDetailsViewArgs::ObjectsUseNameArea, false );
	DetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_SimpleAssetEditor_Layout_v3" )
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->SetHideTabWell( true )
			->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewSplitter()
			->Split
			(
				FTabManager::NewStack()
				->AddTab( PropertiesTabId, ETabState::OpenedTab )
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, FSimpleAssetEditor::SimpleEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit );

	// @todo toolkit world centric editing
	// Setup our tool's layout
	/*if( IsWorldCentricAssetEditor() && !PropertiesTab.IsValid() )
	{
		const FString TabInitializationPayload(TEXT(""));		// NOTE: Payload not currently used for asset properties
		SpawnToolkitTab(GetToolbarTabId(), FString(), EToolkitTabSpot::ToolBar);
		PropertiesTab = SpawnToolkitTab( PropertiesTabId, TabInitializationPayload, EToolkitTabSpot::Details );
	}*/
	
	// Get the list of objects to edit the details of
	const TArray<UObject*> ObjectsToEditInDetailsView = ( GetDetailsViewObjects.IsBound() ) ? GetDetailsViewObjects.Execute( ObjectsToEdit ) : ObjectsToEdit;

	// Ensure all objects are transactable for undo/redo in the details panel
	for( UObject* ObjectToEditInDetailsView : ObjectsToEditInDetailsView )
	{
		ObjectToEditInDetailsView->SetFlags(RF_Transactional);
	}

	if( DetailsView.IsValid() )
	{
		// Make sure details window is pointing to our object
		DetailsView->SetObjects( ObjectsToEditInDetailsView );
	}
}

FName FSimpleAssetEditor::GetToolkitFName() const
{
	return ToolkitFName;
}

FText FSimpleAssetEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Generic Asset Editor");
}

FText FSimpleAssetEditor::GetToolkitName() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();

	check( EditingObjs.Num() > 0 );

	FFormatNamedArguments Args;
	Args.Add( TEXT("ToolkitName"), GetBaseToolkitName() );

	if( EditingObjs.Num() == 1 )
	{
		const UObject* EditingObject = EditingObjs[ 0 ];

		const bool bDirtyState = EditingObject->GetOutermost()->IsDirty();

		Args.Add( TEXT("ObjectName"), FText::FromString( EditingObject->GetName() ) );
		Args.Add( TEXT("DirtyState"), bDirtyState ? FText::FromString( TEXT( "*" ) ) : FText::GetEmpty() );
		return FText::Format( LOCTEXT("ToolkitTitle", "{ObjectName}{DirtyState} - {ToolkitName}"), Args );
	}
	else
	{
		bool bDirtyState = false;
		UClass* SharedBaseClass = nullptr;
		for( int32 x = 0; x < EditingObjs.Num(); ++x )
		{
			UObject* Obj = EditingObjs[ x ];
			check( Obj );

			UClass* ObjClass = Cast<UClass>(Obj);
			if (ObjClass == nullptr)
			{
				ObjClass = Obj->GetClass();
			}
			check( ObjClass );

			// Initialize with the class of the first object we encounter.
			if( SharedBaseClass == nullptr )
			{
				SharedBaseClass = ObjClass;
			}

			// If we've encountered an object that's not a subclass of the current best baseclass,
			// climb up a step in the class hierarchy.
			while( !ObjClass->IsChildOf( SharedBaseClass ) )
			{
				SharedBaseClass = SharedBaseClass->GetSuperClass();
			}

			// If any of the objects are dirty, flag the label
			bDirtyState |= Obj->GetOutermost()->IsDirty();
		}

		check(SharedBaseClass);

		Args.Add( TEXT("NumberOfObjects"), EditingObjs.Num() );
		Args.Add( TEXT("ClassName"), FText::FromString( SharedBaseClass->GetName() ) );
		Args.Add( TEXT("DirtyState"), bDirtyState ? FText::FromString( TEXT( "*" ) ) : FText::GetEmpty() );
		return FText::Format( LOCTEXT("ToolkitTitle_EditingMultiple", "{NumberOfObjects} {ClassName}{DirtyState} - {ToolkitName}"), Args );
	}
}

FText FSimpleAssetEditor::GetToolkitToolTipText() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();

	check( EditingObjs.Num() > 0 );

	FFormatNamedArguments Args;
	Args.Add( TEXT("ToolkitName"), GetBaseToolkitName() );

	if( EditingObjs.Num() == 1 )
	{
		const UObject* EditingObject = EditingObjs[ 0 ];
		return FAssetEditorToolkit::GetToolTipTextForObject(EditingObject);
	}
	else
	{
		UClass* SharedBaseClass = NULL;
		for( int32 x = 0; x < EditingObjs.Num(); ++x )
		{
			UObject* Obj = EditingObjs[ x ];
			check( Obj );

			UClass* ObjClass = Cast<UClass>(Obj);
			if (ObjClass == nullptr)
			{
				ObjClass = Obj->GetClass();
			}
			check( ObjClass );

			// Initialize with the class of the first object we encounter.
			if( SharedBaseClass == nullptr )
			{
				SharedBaseClass = ObjClass;
			}

			// If we've encountered an object that's not a subclass of the current best baseclass,
			// climb up a step in the class hierarchy.
			while( !ObjClass->IsChildOf( SharedBaseClass ) )
			{
				SharedBaseClass = SharedBaseClass->GetSuperClass();
			}
		}

		check(SharedBaseClass);

		Args.Add( TEXT("NumberOfObjects"), EditingObjs.Num() );
		Args.Add( TEXT("ClassName"), FText::FromString( SharedBaseClass->GetName() ) );
		return FText::Format( LOCTEXT("ToolkitTitle_EditingMultipleToolTip", "{NumberOfObjects} {ClassName} - {ToolkitName}"), Args );
	}
}

FLinearColor FSimpleAssetEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.5f, 0.0f, 0.0f, 0.5f );
}

void FSimpleAssetEditor::SetPropertyVisibilityDelegate(FIsPropertyVisible InVisibilityDelegate)
{
	DetailsView->SetIsPropertyVisibleDelegate(InVisibilityDelegate);
}

TSharedRef<SDockTab> FSimpleAssetEditor::SpawnPropertiesTab( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == PropertiesTabId );

	return SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("GenericEditor.Tabs.Properties") )
		.Label( LOCTEXT("GenericDetailsTitle", "Details") )
		.TabColorScale( GetTabColorScale() )
		[
			DetailsView.ToSharedRef()
		];
}

void FSimpleAssetEditor::HandleAssetPostImport(UFactory* InFactory, UObject* InObject)
{
	if (EditingObjects.Contains(InObject))
	{
		// The details panel likely needs to be refreshed if an asset was imported again
		DetailsView->SetObjects(EditingObjects);
	}
}

FString FSimpleAssetEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Generic Asset ").ToString();
}

TSharedRef<FSimpleAssetEditor> FSimpleAssetEditor::CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit, FGetDetailsViewObjects GetDetailsViewObjects )
{
	TSharedRef< FSimpleAssetEditor > NewEditor( new FSimpleAssetEditor() );

	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add( ObjectToEdit );
	NewEditor->InitEditor( Mode, InitToolkitHost, ObjectsToEdit, GetDetailsViewObjects );

	return NewEditor;
}

TSharedRef<FSimpleAssetEditor> FSimpleAssetEditor::CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit, FGetDetailsViewObjects GetDetailsViewObjects )
{
	TSharedRef< FSimpleAssetEditor > NewEditor( new FSimpleAssetEditor() );
	NewEditor->InitEditor( Mode, InitToolkitHost, ObjectsToEdit, GetDetailsViewObjects );
	return NewEditor;
}

#undef LOCTEXT_NAMESPACE
