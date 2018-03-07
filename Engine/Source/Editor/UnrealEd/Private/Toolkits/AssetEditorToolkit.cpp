// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Toolkits/AssetEditorToolkit.h"
#include "Widgets/Layout/SBorder.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "Editor.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "EditorStyleSet.h"
#include "EditorStyleSettings.h"
#include "EditorReimportHandler.h"
#include "FileHelpers.h"
#include "Toolkits/SStandaloneAssetEditorToolkitHost.h"
#include "Toolkits/ToolkitManager.h"
#include "Toolkits/AssetEditorCommonCommands.h"
#include "Toolkits/GlobalEditorCommonCommands.h"
#include "Styling/SlateIconFinder.h"
#include "CollectionManagerTypes.h"
#include "ICollectionManager.h"
#include "CollectionManagerModule.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "ReferenceViewer.h"
#include "ISizeMapModule.h"
#include "IIntroTutorials.h"
#include "Widgets/Docking/SDockTab.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "AssetEditorToolkit"

TWeakPtr< IToolkitHost > FAssetEditorToolkit::PreviousWorldCentricToolkitHostForNewAssetEditor;


const FName FAssetEditorToolkit::ToolbarTabId( TEXT( "AssetEditorToolkit_Toolbar" ) );

FAssetEditorToolkit::FAssetEditorToolkit()
	: GCEditingObjects(*this)
	, bCheckDirtyOnAssetSave(false)
	, AssetEditorModeManager(nullptr)
	, bIsToolbarFocusable(false)
{
	WorkspaceMenuCategory = FWorkspaceItem::NewGroup(LOCTEXT("WorkspaceMenu_BaseAssetEditor", "Asset Editor"));
}


void FAssetEditorToolkit::InitAssetEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, const FName AppIdentifier, const TSharedRef<FTabManager::FLayout>& StandaloneDefaultLayout, const bool bCreateDefaultStandaloneMenu, const bool bCreateDefaultToolbar, UObject* ObjectToEdit, const bool bInIsToolbarFocusable )
{
	TArray< UObject* > ObjectsToEdit;
	ObjectsToEdit.Add( ObjectToEdit );

	InitAssetEditor( Mode, InitToolkitHost, AppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit, bInIsToolbarFocusable );
}

void FAssetEditorToolkit::InitAssetEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, const FName AppIdentifier, const TSharedRef<FTabManager::FLayout>& StandaloneDefaultLayout, const bool bCreateDefaultStandaloneMenu, const bool bCreateDefaultToolbar, const TArray<UObject*>& ObjectsToEdit, const bool bInIsToolbarFocusable )
{
	// Must not already be editing an object
	check( ObjectsToEdit.Num() > 0 );
	check( EditingObjects.Num() == 0 );

	bIsToolbarFocusable = bInIsToolbarFocusable;

	// cache reference to ToolkitManager; also ensure it was initialized.
	FToolkitManager& ToolkitManager = FToolkitManager::Get();

	EditingObjects.Append( ObjectsToEdit );

	// Store "previous" asset editing toolkit host, and clear it out
	PreviousWorldCentricToolkitHost = PreviousWorldCentricToolkitHostForNewAssetEditor;
	PreviousWorldCentricToolkitHostForNewAssetEditor.Reset();

	ToolkitMode = Mode;

	TSharedPtr<SWindow> ParentWindow;

	TSharedPtr<SDockTab> NewMajorTab;

	TSharedPtr< SStandaloneAssetEditorToolkitHost > NewStandaloneHost;
	if( ToolkitMode == EToolkitMode::WorldCentric )		// @todo toolkit major: Do we need to remember this setting on a per-asset editor basis?  Probably.
	{
		// Keep track of the level editor we're attached to (if any)
		ToolkitHost = InitToolkitHost;
	}
	else if( ensure( ToolkitMode == EToolkitMode::Standalone ) )
	{
		// Open a standalone app to edit this asset.
		check( AppIdentifier != NAME_None );

		// Create the label and the link for the toolkit documentation.
		TAttribute<FText> Label = TAttribute<FText>( this, &FAssetEditorToolkit::GetToolkitName );
		TAttribute<FText> ToolTipText = TAttribute<FText>( this, &FAssetEditorToolkit::GetToolkitToolTipText );
		FString DocLink = GetDocumentationLink();
		if ( !DocLink.StartsWith( "Shared/" ) )
		{
			DocLink = FString("Shared/") + DocLink;
		}

		// Create a new SlateToolkitHost
		NewMajorTab = SNew(SDockTab) 
			.ContentPadding(0.0f) 
			.TabRole(ETabRole::MajorTab)
			.ToolTip(IDocumentation::Get()->CreateToolTip(ToolTipText, nullptr, DocLink, GetToolkitFName().ToString()))
			.Icon( this, &FAssetEditorToolkit::GetDefaultTabIcon )
			.TabColorScale( this, &FAssetEditorToolkit::GetDefaultTabColor )
			.Label( Label );

		{
			static_assert(sizeof(EAssetEditorToolkitTabLocation) == sizeof(int32), "EAssetEditorToolkitTabLocation is the incorrect size");

			const UEditorStyleSettings* StyleSettings = GetDefault<UEditorStyleSettings>();

			FName PlaceholderId(TEXT("StandaloneToolkit"));
			TSharedPtr<FTabManager::FSearchPreference> SearchPreference = nullptr;
			if ( StyleSettings->AssetEditorOpenLocation == EAssetEditorOpenLocation::Default )
			{
				// Work out where we should create this asset editor
				EAssetEditorToolkitTabLocation SavedAssetEditorToolkitTabLocation = EAssetEditorToolkitTabLocation::Standalone;
				GConfig->GetInt(
					TEXT("AssetEditorToolkitTabLocation"),
					*ObjectsToEdit[0]->GetPathName(),
					reinterpret_cast<int32&>( SavedAssetEditorToolkitTabLocation ),
					GEditorPerProjectIni
					);

				PlaceholderId = ( SavedAssetEditorToolkitTabLocation == EAssetEditorToolkitTabLocation::Docked ) ? TEXT("DockedToolkit") : TEXT("StandaloneToolkit");
				SearchPreference = MakeShareable(new FTabManager::FLiveTabSearch());
			}
			else if ( StyleSettings->AssetEditorOpenLocation == EAssetEditorOpenLocation::NewWindow )
			{
				PlaceholderId = TEXT("StandaloneToolkit");
				SearchPreference = MakeShareable(new FTabManager::FRequireClosedTab());
			}
			else if ( StyleSettings->AssetEditorOpenLocation == EAssetEditorOpenLocation::MainWindow )
			{
				PlaceholderId = TEXT("DockedToolkit");
				SearchPreference = MakeShareable(new FTabManager::FLiveTabSearch(TEXT("LevelEditor")));
			}
			else if ( StyleSettings->AssetEditorOpenLocation == EAssetEditorOpenLocation::ContentBrowser )
			{
				PlaceholderId = TEXT("DockedToolkit");
				SearchPreference = MakeShareable(new FTabManager::FLiveTabSearch(TEXT("ContentBrowserTab1")));
			}
			else if ( StyleSettings->AssetEditorOpenLocation == EAssetEditorOpenLocation::LastDockedWindowOrNewWindow )
			{
				PlaceholderId = TEXT("StandaloneToolkit");
				SearchPreference = MakeShareable(new FTabManager::FLastMajorOrNomadTab(NAME_None));
			}
			else if ( StyleSettings->AssetEditorOpenLocation == EAssetEditorOpenLocation::LastDockedWindowOrMainWindow )
			{
				PlaceholderId = TEXT("StandaloneToolkit");
				SearchPreference = MakeShareable(new FTabManager::FLastMajorOrNomadTab(TEXT("LevelEditor")));
			}
			else if ( StyleSettings->AssetEditorOpenLocation == EAssetEditorOpenLocation::LastDockedWindowOrContentBrowser )
			{
				PlaceholderId = TEXT("StandaloneToolkit");
				SearchPreference = MakeShareable(new FTabManager::FLastMajorOrNomadTab(TEXT("ContentBrowserTab1")));
			}
			else
			{
				// Add more cases!
				check(false);
			}

			FGlobalTabmanager::Get()->InsertNewDocumentTab(PlaceholderId, *SearchPreference, NewMajorTab.ToSharedRef());

			// Bring the window to front.  The tab manager will not do this for us to avoid intrusive stealing focus behavior
			// However, here the expectation is that opening an new asset editor is something that should steal focus so the user can see their asset
			TSharedPtr<SWindow> Window = NewMajorTab->GetParentWindow();
			if(Window.IsValid())
			{
				Window->BringToFront();
			}
		}

		IIntroTutorials& IntroTutorials = FModuleManager::LoadModuleChecked<IIntroTutorials>(TEXT("IntroTutorials"));
		TSharedRef<SWidget> TutorialWidget = IntroTutorials.CreateTutorialsWidget(GetToolkitContextFName(), NewMajorTab->GetParentWindow());

		NewMajorTab->SetRightContent(
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 8.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					TutorialWidget
				]	
			);

		const TSharedRef<FTabManager> NewTabManager = FGlobalTabmanager::Get()->NewTabManager( NewMajorTab.ToSharedRef() );		
		NewTabManager->SetOnPersistLayout(FTabManager::FOnPersistLayout::CreateRaw(this, &FAssetEditorToolkit::HandleTabManagerPersistLayout));
		this->TabManager = NewTabManager;

		NewMajorTab->SetContent
		( 

			SAssignNew( NewStandaloneHost, SStandaloneAssetEditorToolkitHost, NewTabManager, AppIdentifier )
			.OnRequestClose(this, &FAssetEditorToolkit::OnRequestClose)
		);

		// Assign our toolkit host before we setup initial content.  (Important: We must cache this pointer here as SetupInitialContent
		// will callback into the toolkit host.)
		ToolkitHost = NewStandaloneHost;
	}


	check( ToolkitHost.IsValid() );
	ToolkitManager.RegisterNewToolkit( SharedThis( this ) );
	
	if (ToolkitMode == EToolkitMode::Standalone)
	{
		TSharedRef<FTabManager::FLayout> LayoutToUse = FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni, StandaloneDefaultLayout);

		// Actually create the widget content
		NewStandaloneHost->SetupInitialContent( LayoutToUse, NewMajorTab, bCreateDefaultStandaloneMenu );
	}
	StandaloneHost = NewStandaloneHost;
	

	if (bCreateDefaultToolbar)
	{
		GenerateToolbar();
	}
	else
	{
		Toolbar = SNullWidget::NullWidget;
	}

	ToolkitCommands->MapAction(
		FAssetEditorCommonCommands::Get().SaveAsset,
		FExecuteAction::CreateSP( this, &FAssetEditorToolkit::SaveAsset_Execute ),
		FCanExecuteAction::CreateSP( this, &FAssetEditorToolkit::CanSaveAsset ));

	ToolkitCommands->MapAction(
		FAssetEditorCommonCommands::Get().SaveAssetAs,
		FExecuteAction::CreateSP( this, &FAssetEditorToolkit::SaveAssetAs_Execute ),
		FCanExecuteAction::CreateSP( this, &FAssetEditorToolkit::CanSaveAssetAs ));

	ToolkitCommands->MapAction(
		FGlobalEditorCommonCommands::Get().FindInContentBrowser,
		FExecuteAction::CreateSP( this, &FAssetEditorToolkit::FindInContentBrowser_Execute ),
		FCanExecuteAction::CreateSP( this, &FAssetEditorToolkit::CanFindInContentBrowser ));
	
	ToolkitCommands->MapAction(
		FGlobalEditorCommonCommands::Get().ViewReferences,
		FExecuteAction::CreateSP( this, &FAssetEditorToolkit::ViewReferences_Execute ),
		FCanExecuteAction::CreateSP( this, &FAssetEditorToolkit::CanViewReferences ));
	
	ToolkitCommands->MapAction(
		FGlobalEditorCommonCommands::Get().ViewSizeMap,
		FExecuteAction::CreateSP( this, &FAssetEditorToolkit::ViewSizeMap_Execute ),
		FCanExecuteAction::CreateSP( this, &FAssetEditorToolkit::CanViewSizeMap ));
	
	ToolkitCommands->MapAction(
		FGlobalEditorCommonCommands::Get().OpenDocumentation,
		FExecuteAction::CreateSP( this, &FAssetEditorToolkit::BrowseDocumentation_Execute ) );

	ToolkitCommands->MapAction(
		FAssetEditorCommonCommands::Get().ReimportAsset,
		FExecuteAction::CreateSP( this, &FAssetEditorToolkit::Reimport_Execute ) );

	FGlobalEditorCommonCommands::MapActions(ToolkitCommands);

	if( IsWorldCentricAssetEditor() )
	{
		ToolkitCommands->MapAction(
			FAssetEditorCommonCommands::Get().SwitchToStandaloneEditor,
			FExecuteAction::CreateStatic( &FAssetEditorToolkit::SwitchToStandaloneEditor_Execute, TWeakPtr< FAssetEditorToolkit >( AsShared() ) ) );
	}
	else
	{
		if( GetPreviousWorldCentricToolkitHost().IsValid() )
		{
			ToolkitCommands->MapAction(
				FAssetEditorCommonCommands::Get().SwitchToWorldCentricEditor,
				FExecuteAction::CreateStatic( &FAssetEditorToolkit::SwitchToWorldCentricEditor_Execute, TWeakPtr< FAssetEditorToolkit >( AsShared() ) ) );
		}
	}

	// NOTE: Currently, the AssetEditorManager will keep a hard reference to our object as we're editing it
	FAssetEditorManager::Get().NotifyAssetsOpened( EditingObjects, this );
}


FAssetEditorToolkit::~FAssetEditorToolkit()
{
	EditingObjects.Empty();

	// We're no longer editing this object, so let the editor know
	FAssetEditorManager::Get().NotifyEditorClosed( this );
}


void FAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	// Use the first child category of the local workspace root if there is one, otherwise use the root itself
	const auto& LocalCategories = InTabManager->GetLocalWorkspaceMenuRoot()->GetChildItems();
	TSharedRef<FWorkspaceItem> ToolbarSpawnerCategory = LocalCategories.Num() > 0 ? LocalCategories[0] : InTabManager->GetLocalWorkspaceMenuRoot();

	InTabManager->RegisterTabSpawner( ToolbarTabId, FOnSpawnTab::CreateSP(this, &FAssetEditorToolkit::SpawnTab_Toolbar) )
		.SetDisplayName( LOCTEXT("ToolbarTab", "Toolbar") )
		.SetGroup( ToolbarSpawnerCategory )
		.SetIcon( FSlateIcon(FEditorStyle::GetStyleSetName(), "Toolbar.Icon") );
}

void FAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner( ToolbarTabId );
	InTabManager->ClearLocalWorkspaceMenuCategories();
}

bool FAssetEditorToolkit::IsAssetEditor() const
{
	return true;
}

FText FAssetEditorToolkit::GetToolkitName() const
{
	const UObject* EditingObject = GetEditingObject();

	check (EditingObject != NULL);

	return GetLabelForObject(EditingObject);
}

FText FAssetEditorToolkit::GetToolkitToolTipText() const
{
	const UObject* EditingObject = GetEditingObject();

	check (EditingObject != NULL);

	return GetToolTipTextForObject(EditingObject);
}

FText FAssetEditorToolkit::GetLabelForObject(const UObject* InObject)
{
	const bool bDirtyState = InObject->GetOutermost()->IsDirty();
	FString NameString;
	if(const AActor* ObjectAsActor = Cast<AActor>(InObject))
	{
		NameString = ObjectAsActor->GetActorLabel();
	}
	else
	{
		NameString = InObject->GetName();
	}

	FFormatNamedArguments Args;
	Args.Add( TEXT("ObjectName"), FText::FromString( NameString ));
	Args.Add( TEXT("DirtyState"), bDirtyState ? FText::FromString( TEXT( "*" ) ) : FText::GetEmpty() );
	return FText::Format( LOCTEXT("AssetEditorAppLabel", "{ObjectName}{DirtyState}"), Args );
}

FText FAssetEditorToolkit::GetToolTipTextForObject(const UObject* InObject)
{
	FString ToolTipString;
	if(const AActor* ObjectAsActor = Cast<AActor>(InObject))
	{
		ToolTipString += LOCTEXT("ToolTipActorLabel", "Actor").ToString();
		ToolTipString += TEXT(": ");
		ToolTipString += ObjectAsActor->GetActorLabel();
	}
	else
	{
		ToolTipString += LOCTEXT("ToolTipAssetLabel", "Asset").ToString();
		ToolTipString += TEXT(": ");
		ToolTipString += InObject->GetName();

		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

		const FString CollectionNames = CollectionManagerModule.Get().GetCollectionsStringForObject(*InObject->GetPathName(), ECollectionShareType::CST_All);
		if (!CollectionNames.IsEmpty())
		{
			ToolTipString += TEXT("\n");

			ToolTipString += LOCTEXT("ToolTipCollectionsLabel", "Collections").ToString();
			ToolTipString += TEXT(": ");
			ToolTipString += CollectionNames;
		}
	}

	return FText::FromString(ToolTipString);
}

class FEdMode* FAssetEditorToolkit::GetEditorMode() const
{
	return NULL;
}

const TArray< UObject* >* FAssetEditorToolkit::GetObjectsCurrentlyBeingEdited() const
{
	return &EditingObjects;
}

FName FAssetEditorToolkit::GetEditorName() const
{
	return GetToolkitFName();
}

void FAssetEditorToolkit::FocusWindow(UObject* ObjectToFocusOn)
{
	BringToolkitToFront();
}


bool FAssetEditorToolkit::CloseWindow()
{
	if (OnRequestClose())
	{
		// Close this toolkit
		FToolkitManager::Get().CloseToolkit( AsShared() );
	}
	return true;
}

void FAssetEditorToolkit::InvokeTab(const FTabId& TabId)
{
	GetTabManager()->InvokeTab(TabId);
}

TSharedPtr<class FTabManager> FAssetEditorToolkit::GetAssociatedTabManager()
{
	return TabManager;
}

double FAssetEditorToolkit::GetLastActivationTime()
{
	double MostRecentTime = 0.0;

	if (TabManager.IsValid())
	{
		TSharedPtr<SDockTab> OwnerTab = TabManager->GetOwnerTab();
		if (OwnerTab.IsValid())
		{
			MostRecentTime = OwnerTab->GetLastActivationTime();
		}
	}

	return MostRecentTime;
}

TSharedPtr< IToolkitHost > FAssetEditorToolkit::GetPreviousWorldCentricToolkitHost()
{
	return PreviousWorldCentricToolkitHost.Pin();
}


void FAssetEditorToolkit::SetPreviousWorldCentricToolkitHostForNewAssetEditor( TSharedRef< IToolkitHost > ToolkitHost )
{
	PreviousWorldCentricToolkitHostForNewAssetEditor = ToolkitHost;
}


UObject* FAssetEditorToolkit::GetEditingObject() const
{
	check( EditingObjects.Num() == 1 );
	return EditingObjects[ 0 ];
}


const TArray< UObject* >& FAssetEditorToolkit::GetEditingObjects() const
{
	check( EditingObjects.Num() > 0 );
	return EditingObjects;
}


void FAssetEditorToolkit::GetSaveableObjects(TArray<UObject*>& OutObjects) const
{
	for (const auto Object : EditingObjects)
	{
		// If we are editing a subobject of asset (e.g., a level script blueprint which is contained in a map asset), still provide the
		// option to work with it but treat save operations/etc... as working on the top level asset itself
		for (UObject* TestObject = Object; TestObject != nullptr; TestObject = TestObject->GetOuter())
		{
			if (TestObject->IsAsset())
			{
				OutObjects.Add(TestObject);
				break;
			}
		}
	}
}


void FAssetEditorToolkit::AddEditingObject(UObject* Object)
{
	EditingObjects.Add(Object);
	FAssetEditorManager::Get().NotifyAssetOpened( Object, this );
}


void FAssetEditorToolkit::RemoveEditingObject(UObject* Object)
{
	EditingObjects.Remove(Object);
	FAssetEditorManager::Get().NotifyAssetClosed( Object, this );
}


void FAssetEditorToolkit::SaveAsset_Execute()
{
	if (EditingObjects.Num() == 0)
	{
		return;
	}

	TArray<UObject*> ObjectsToSave;
	GetSaveableObjects(ObjectsToSave);

	if (ObjectsToSave.Num() == 0)
	{
		return;
	}

	TArray<UPackage*> PackagesToSave;

	for (auto Object : ObjectsToSave)
	{
		check((Object != nullptr) && Object->IsAsset());
		PackagesToSave.Add(Object->GetOutermost());
	}

	FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirtyOnAssetSave, /*bPromptToSave=*/ false);
}


void FAssetEditorToolkit::SaveAssetAs_Execute()
{
	if (EditingObjects.Num() == 0)
	{
		return;
	}

	TSharedPtr<IToolkitHost> MyToolkitHost = ToolkitHost.Pin();

	if (!MyToolkitHost.IsValid())
	{
		return;
	}

	// get collection of objects to save
	TArray<UObject*> ObjectsToSave;
	GetSaveableObjects(ObjectsToSave);

	if (ObjectsToSave.Num() == 0)
	{
		return;
	}

	// save assets under new name
	TArray<UObject*> SavedObjects;
	FEditorFileUtils::SaveAssetsAs(ObjectsToSave, SavedObjects);

	if (SavedObjects.Num() == 0)
	{
		return;
	}

	// close existing asset editors for resaved assets
	FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();

	/* @todo editor: Persona does not behave well when closing specific objects
	for (int32 Index = 0; Index < ObjectsToSave.Num(); ++Index)
	{
		if ((SavedObjects[Index] != ObjectsToSave[Index]) && (SavedObjects[Index] != nullptr))
		{
			AssetEditorManager.CloseAllEditorsForAsset(ObjectsToSave[Index]);
		}
	}

	// reopen asset editor
	AssetEditorManager.OpenEditorForAssets(TArrayBuilder<UObject*>().Add(SavedObjects[0]), ToolkitMode, MyToolkitHost.ToSharedRef());
	*/
	// hack
	TArray<UObject*> ObjectsToReopen;
	for (auto Object : EditingObjects)
	{
		if (Object->IsAsset() && !ObjectsToSave.Contains(Object))
		{
			ObjectsToReopen.Add(Object);
		}
	}
	for (auto Object : SavedObjects)
	{
		ObjectsToReopen.AddUnique(Object);
	}
	for (auto Object : EditingObjects)
	{
		AssetEditorManager.CloseAllEditorsForAsset(Object);
		FAssetEditorManager::Get().NotifyAssetClosed(Object, this);
	}
	AssetEditorManager.OpenEditorForAssets(ObjectsToReopen, ToolkitMode, MyToolkitHost.ToSharedRef());
	// end hack
}


const FSlateBrush* FAssetEditorToolkit::GetDefaultTabIcon() const
{
	if (EditingObjects.Num() == 0)
	{
		return nullptr;
	}

	const FSlateBrush* IconBrush = nullptr;

	for (UObject* Object : EditingObjects)
	{
		if(Object)
		{
			// Find the first object that has a valid brush
			const FSlateBrush* ThisAssetBrush = FSlateIconFinder::FindIconBrushForClass(Object->GetClass());
			if (ThisAssetBrush != nullptr)
			{
				IconBrush = ThisAssetBrush;
				break;
			}
		}
	}

	if (!IconBrush)
	{
		IconBrush = FEditorStyle::GetBrush(TEXT("ClassIcon.Default"));;
	}

	return IconBrush;
}

FLinearColor FAssetEditorToolkit::GetDefaultTabColor() const
{
	FLinearColor TabColor = FLinearColor::Transparent;
	if (EditingObjects.Num() == 0 || !GetDefault<UEditorStyleSettings>()->bEnableColorizedEditorTabs)
	{
		return TabColor;
	}

	FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
	IAssetTools& AssetTools = AssetToolsModule.Get();
	for (auto ObjectIt = EditingObjects.CreateConstIterator(); ObjectIt; ++ObjectIt)
	{
		TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetTools.GetAssetTypeActionsForClass((*ObjectIt)->GetClass());
		if (AssetTypeActions.IsValid())
		{
			const FLinearColor ThisAssetColor = AssetTypeActions.Pin()->GetTypeColor();
			if (ThisAssetColor != FLinearColor::Transparent)
			{
				return ThisAssetColor;
			}
		}
	}

	return TabColor;
}

FAssetEditorModeManager* FAssetEditorToolkit::GetAssetEditorModeManager() const
{
	return AssetEditorModeManager;
}

void FAssetEditorToolkit::SetAssetEditorModeManager(FAssetEditorModeManager* InModeManager)
{
	AssetEditorModeManager = InModeManager;
}

void FAssetEditorToolkit::RemoveEditingAsset(UObject* Asset)
{
	// Just close the editor tab if it's the last element
	if (EditingObjects.Num() == 1 && EditingObjects.Contains(Asset))
	{
		CloseWindow();
	}
	else
	{
		RemoveEditingObject(Asset);
	}
}

void FAssetEditorToolkit::SwitchToStandaloneEditor_Execute( TWeakPtr< FAssetEditorToolkit > ThisToolkitWeakRef )
{
	// NOTE: We're being very careful here with pointer handling because we need to make sure the tookit's
	// destructor is called when we call CloseToolkit, as it needs to be fully unregistered before we go
	// and try to open a new asset editor for the same asset

	// First, close the world-centric toolkit 
	TArray< FWeakObjectPtr > ObjectsToEditStandaloneWeak;
	TSharedPtr< IToolkitHost > PreviousWorldCentricToolkitHost;
	{
		TSharedRef< FAssetEditorToolkit > ThisToolkit = ThisToolkitWeakRef.Pin().ToSharedRef();
		check( ThisToolkit->IsWorldCentricAssetEditor() );
		PreviousWorldCentricToolkitHost = ThisToolkit->GetToolkitHost();

		const auto& EditingObjects = *ThisToolkit->GetObjectsCurrentlyBeingEdited();

		for( auto ObjectIter = EditingObjects.CreateConstIterator(); ObjectIter; ++ObjectIter )
		{
			ObjectsToEditStandaloneWeak.Add( *ObjectIter );
		}

		FToolkitManager::Get().CloseToolkit( ThisToolkit );

		// At this point, we should be the only referencer of the toolkit!  It will be fully destroyed when
		// as the code pointer exits this block.
		ensure( ThisToolkit.IsUnique() );
	}

	// Now, reopen the toolkit in "standalone" mode
	TArray< UObject* > ObjectsToEdit;

	for( auto ObjectPtrItr = ObjectsToEditStandaloneWeak.CreateIterator(); ObjectPtrItr; ++ObjectPtrItr )
	{
		const auto WeakObjectPtr = *ObjectPtrItr;
		if( WeakObjectPtr.IsValid() )
		{
			ObjectsToEdit.Add( WeakObjectPtr.Get() );
		}
	}

	if( ObjectsToEdit.Num() > 0 )
	{
		ensure( FAssetEditorManager::Get().OpenEditorForAssets( ObjectsToEdit, EToolkitMode::Standalone, PreviousWorldCentricToolkitHost.ToSharedRef() ) );
	}
}


void FAssetEditorToolkit::SwitchToWorldCentricEditor_Execute( TWeakPtr< FAssetEditorToolkit > ThisToolkitWeakRef )
{
	// @todo toolkit minor: Maybe also allow the user to drag and drop the standalone editor's tab into a specific level editor to switch to world-centric mode?
	
	// NOTE: We're being very careful here with pointer handling because we need to make sure the tookit's
	// destructor is called when we call CloseToolkit, as it needs to be fully unregistered before we go
	// and try to open a new asset editor for the same asset

	// First, close the standalone toolkit 
	TArray< FWeakObjectPtr > ObjectToEditWorldCentricWeak;
	TSharedPtr< IToolkitHost > WorldCentricLevelEditor;
	{
		TSharedRef< FAssetEditorToolkit > ThisToolkit = ThisToolkitWeakRef.Pin().ToSharedRef();
		const auto& EditingObjects = *ThisToolkit->GetObjectsCurrentlyBeingEdited();

		for( auto ObjectIter = EditingObjects.CreateConstIterator(); ObjectIter; ++ObjectIter )
		{
			ObjectToEditWorldCentricWeak.Add( *ObjectIter );
		}

		check( !ThisToolkit->IsWorldCentricAssetEditor() );
		WorldCentricLevelEditor = ThisToolkit->GetPreviousWorldCentricToolkitHost();

		FToolkitManager::Get().CloseToolkit( ThisToolkit );

		// At this point, we should be the only referencer of the toolkit!  It will be fully destroyed when
		// as the code pointer exits this block.
		ensure( ThisToolkit.IsUnique() );
	}

	// Now, reopen the toolkit in "world-centric" mode
	TArray< UObject* > ObjectsToEdit;
	for( auto ObjectPtrItr = ObjectToEditWorldCentricWeak.CreateIterator(); ObjectPtrItr; ++ObjectPtrItr )
	{
		const auto WeakObjectPtr = *ObjectPtrItr;
		if( WeakObjectPtr.IsValid() )
		{
			ObjectsToEdit.Add( WeakObjectPtr.Get() );
		}
	}

	if( ObjectsToEdit.Num() > 0 )
	{
		ensure( FAssetEditorManager::Get().OpenEditorForAssets( ObjectsToEdit, EToolkitMode::WorldCentric, WorldCentricLevelEditor ) );
	}
}


void FAssetEditorToolkit::FindInContentBrowser_Execute()
{
	TArray< UObject* > ObjectsToSyncTo;
	GetSaveableObjects(ObjectsToSyncTo);

	if (ObjectsToSyncTo.Num() > 0)
	{
		GEditor->SyncBrowserToObjects( ObjectsToSyncTo );
	}
}

void FAssetEditorToolkit::BrowseDocumentation_Execute() const
{
	IDocumentation::Get()->Open(GetDocumentationLink(), FDocumentationSourceInfo(TEXT("help_menu_asset")));
}

void FAssetEditorToolkit::ViewReferences_Execute()
{
	if (ensure( ViewableObjects.Num() > 0))
	{
		IReferenceViewerModule::Get().InvokeReferenceViewerTab(ViewableObjects);
	}
}

bool FAssetEditorToolkit::CanViewReferences()
{
	ViewableObjects.Empty();
	for (const auto EditingObject : EditingObjects)
	{
		// Don't allow user to perform certain actions on objects that aren't actually assets (e.g. Level Script blueprint objects)
		if (EditingObject != NULL && EditingObject->IsAsset())
		{
			ViewableObjects.Add(EditingObject->GetOuter()->GetFName());
		}
	}

	return ViewableObjects.Num() > 0;
}

void FAssetEditorToolkit::ViewSizeMap_Execute()
{
	if (ensure( ViewableObjects.Num() > 0))
	{
		ISizeMapModule::Get().InvokeSizeMapTab(ViewableObjects);
	}
}

bool FAssetEditorToolkit::CanViewSizeMap()
{
	ViewableObjects.Empty();
	for (const auto EditingObject : EditingObjects)
	{
		// Don't allow user to perform certain actions on objects that aren't actually assets (e.g. Level Script blueprint objects)
		if (EditingObject != NULL && EditingObject->IsAsset())
		{
			ViewableObjects.Add(EditingObject->GetOuter()->GetFName());
		}
	}

	return ViewableObjects.Num() > 0;
}

FString FAssetEditorToolkit::GetDocumentationLink() const
{
	return FString(TEXT("%ROOT%"));
}

bool FAssetEditorToolkit::CanReimport() const
{
	for( auto ObjectIter = EditingObjects.CreateConstIterator(); ObjectIter; ++ObjectIter )
	{
		auto EditingObject = *ObjectIter;
		if ( CanReimport( EditingObject ) )
		{
			return true;
		}
	}
	return false;
}


bool FAssetEditorToolkit::CanReimport( UObject* EditingObject ) const
{
	// Don't allow user to perform certain actions on objects that aren't actually assets (e.g. Level Script blueprint objects)
	if( EditingObject != NULL && EditingObject->IsAsset() )
	{
		if ( FReimportManager::Instance()->CanReimport( EditingObject ) )
		{
			return true;
		}
	}
	return false;
}


void FAssetEditorToolkit::Reimport_Execute()
{
	if( ensure( EditingObjects.Num() > 0 ) )
	{
		for( auto ObjectIter = EditingObjects.CreateConstIterator(); ObjectIter; ++ObjectIter )
		{
			const auto EditingObject = *ObjectIter;
			Reimport_Execute( EditingObject );
		}
	}
}


void FAssetEditorToolkit::Reimport_Execute( UObject* EditingObject )
{
	// Don't allow user to perform certain actions on objects that aren't actually assets (e.g. Level Script blueprint objects)
	if( EditingObject != NULL && EditingObject->IsAsset() )
	{
		// Reimport the asset
		FReimportManager::Instance()->Reimport(EditingObject, ShouldPromptForNewFilesOnReload(*EditingObject));
	}
}

bool FAssetEditorToolkit::ShouldPromptForNewFilesOnReload(const UObject& EditingObject) const
{
	return true;
}

TSharedRef<SDockTab> FAssetEditorToolkit::SpawnTab_Toolbar( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == ToolbarTabId );

	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.Label( NSLOCTEXT("AssetEditorToolkit", "Toolbar_TabTitle", "Toolbar") )
		.Icon(FEditorStyle::GetBrush("LevelEditor.Tabs.Toolbar"))
		.ShouldAutosize(true)
		[
			SAssignNew(ToolbarWidgetContent, SBorder)
			.Padding(0)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
		];

	if (Toolbar.IsValid())
	{
		ToolbarWidgetContent->SetContent(Toolbar.ToSharedRef());
	}

	return DockTab;
}



void FAssetEditorToolkit::FillDefaultFileMenuCommands( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.AddMenuEntry(FAssetEditorCommonCommands::Get().SaveAsset, "SaveAsset", TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "AssetEditor.SaveAsset.Greyscale"));
	if( IsActuallyAnAsset() )
	{
		MenuBuilder.AddMenuEntry(FAssetEditorCommonCommands::Get().SaveAssetAs, "SaveAssetAs", TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "AssetEditor.SaveAssetAs.Small"));
	}
	MenuBuilder.AddMenuSeparator();

	if( IsWorldCentricAssetEditor() )
	{
		// @todo toolkit minor: It would be awesome if the user could just "tear off" the SToolkitDisplay to do SwitchToStandaloneEditor
		//			Would need to probably drop at mouseup location though instead of using saved layout pos.
		MenuBuilder.AddMenuEntry( FAssetEditorCommonCommands::Get().SwitchToStandaloneEditor );
	}
	else
	{
		if( GetPreviousWorldCentricToolkitHost().IsValid() )
		{
			// @todo toolkit checkin: Disabled temporarily until we have world-centric "ready to use"!
			if( 0 )
			{
				MenuBuilder.AddMenuEntry( FAssetEditorCommonCommands::Get().SwitchToWorldCentricEditor );
			}
		}
	}
}


void FAssetEditorToolkit::FillDefaultAssetMenuCommands( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.AddMenuEntry(FGlobalEditorCommonCommands::Get().FindInContentBrowser, "FindInContentBrowser", LOCTEXT("FindInContentBrowser", "Find in Content Browser..."));

	// Commands we only want to be accessible when editing an asset should go here 
	if( IsActuallyAnAsset() )
	{
		MenuBuilder.AddMenuEntry( FGlobalEditorCommonCommands::Get().ViewReferences);
		MenuBuilder.AddMenuEntry( FGlobalEditorCommonCommands::Get().ViewSizeMap);

		// Add a reimport menu entry for each supported editable object
		for( auto ObjectIter = EditingObjects.CreateConstIterator(); ObjectIter; ++ObjectIter )
		{
			const auto EditingObject = *ObjectIter;
			if( EditingObject != NULL && EditingObject->IsAsset() )
			{
				if ( CanReimport( EditingObject ) )
				{
					FFormatNamedArguments LabelArguments;
					LabelArguments.Add(TEXT("Name"), FText::FromString( EditingObject->GetName() ));
					const FText LabelText = FText::Format( LOCTEXT("Reimport_Label", "Reimport {Name}..."), LabelArguments );
					FFormatNamedArguments ToolTipArguments;
					ToolTipArguments.Add(TEXT("Type"), FText::FromString( EditingObject->GetClass()->GetName() ));
					const FText ToolTipText = FText::Format( LOCTEXT("Reimport_ToolTip", "Reimports this {Type}"), ToolTipArguments );
					const FName IconName = TEXT( "AssetEditor.Reimport" );
					FUIAction UIAction;
					UIAction.ExecuteAction.BindRaw( this, &FAssetEditorToolkit::Reimport_Execute, EditingObject );
					MenuBuilder.AddMenuEntry( LabelText, ToolTipText, FSlateIcon(FEditorStyle::GetStyleSetName(), IconName), UIAction );
				}
			}
		}		
	}
}

void FAssetEditorToolkit::FillDefaultHelpMenuCommands( FMenuBuilder& MenuBuilder )
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Editor"), GetBaseToolkitName());
	const FText ToolTip = FText::Format(LOCTEXT("BrowseDocumentationTooltip", "Browse {Editor} documentation..."), Args);

	MenuBuilder.AddMenuEntry(FGlobalEditorCommonCommands::Get().OpenDocumentation, NAME_None, ToolTip);
}

void FAssetEditorToolkit::GenerateToolbar()
{
	TSharedPtr<FExtender> Extender = FExtender::Combine(ToolbarExtenders);

	FToolBarBuilder ToolbarBuilder( GetToolkitCommands(), FMultiBoxCustomization::AllowCustomization( GetToolkitFName() ), Extender);
	ToolbarBuilder.SetIsFocusable(bIsToolbarFocusable);
	ToolbarBuilder.BeginSection("Asset");
	{
		ToolbarBuilder.AddToolBarButton(FAssetEditorCommonCommands::Get().SaveAsset);
		ToolbarBuilder.AddToolBarButton(FGlobalEditorCommonCommands::Get().FindInContentBrowser, NAME_None, LOCTEXT("FindInContentBrowserButton", "Browse"));
	}
	ToolbarBuilder.EndSection();

	TSharedRef<SHorizontalBox> MiscWidgets = SNew(SHorizontalBox);

	for (int32 WidgetIdx = 0; WidgetIdx < ToolbarWidgets.Num(); ++WidgetIdx)
	{
		MiscWidgets->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 2.0f, 0.0f, 2.0f)
		[
			ToolbarWidgets[WidgetIdx]
		];
	}
	
	Toolbar = 
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Bottom)
			[
				ToolbarBuilder.MakeWidget()
			]
		]
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Bottom)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush(TEXT("Toolbar.Background")))
				.Visibility(ToolbarWidgets.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed)
				[
					MiscWidgets
				]
			]
		];

	if (ToolbarWidgetContent.IsValid())
	{
		ToolbarWidgetContent->SetContent(Toolbar.ToSharedRef());
	}
}

void FAssetEditorToolkit::RegenerateMenusAndToolbars()
{
	RemoveAllToolbarWidgets();

	StandaloneHost.Pin()->GenerateMenus(false);

	if (Toolbar != SNullWidget::NullWidget)
	{
		GenerateToolbar();
	}

	PostRegenerateMenusAndToolbars();
}



void FAssetEditorToolkit::RestoreFromLayout(const TSharedRef<FTabManager::FLayout>& NewLayout)
{
	TSharedPtr< class SStandaloneAssetEditorToolkitHost > HostWidget = StandaloneHost.Pin();
	if (HostWidget.Get() != NULL)
	{
		// Save the old layout
		FLayoutSaveRestore::SaveToConfig(GEditorIni, TabManager->PersistLayout());

		// Load the potentially previously saved new layout
		TSharedRef<FTabManager::FLayout> UserConfiguredNewLayout = FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni, NewLayout);

		for (TSharedPtr<FLayoutExtender> LayoutExtender : LayoutExtenders)
		{
			NewLayout->ProcessExtensions(*LayoutExtender);
		}

		// Apply the new layout
		HostWidget->RestoreFromLayout(UserConfiguredNewLayout);
	}
}

bool FAssetEditorToolkit::IsActuallyAnAsset() const
{
	// Don't allow user to perform certain actions on objects that aren't actually assets (e.g. Level Script blueprint objects)
	bool bIsActuallyAnAsset = false;
	for( auto ObjectIter = GetObjectsCurrentlyBeingEdited()->CreateConstIterator(); !bIsActuallyAnAsset && ObjectIter; ++ObjectIter )
	{
		const auto ObjectBeingEdited = *ObjectIter;
		bIsActuallyAnAsset |= ObjectBeingEdited != NULL && ObjectBeingEdited->IsAsset();
	}
	return bIsActuallyAnAsset;
}

void FAssetEditorToolkit::AddMenuExtender(TSharedPtr<FExtender> Extender)
{
	StandaloneHost.Pin()->GetMenuExtenders().AddUnique(Extender);
}

void FAssetEditorToolkit::RemoveMenuExtender(TSharedPtr<FExtender> Extender)
{
	StandaloneHost.Pin()->GetMenuExtenders().Remove(Extender);
}

void FAssetEditorToolkit::AddToolbarExtender(TSharedPtr<FExtender> Extender)
{
	ToolbarExtenders.AddUnique(Extender);
}

void FAssetEditorToolkit::RemoveToolbarExtender(TSharedPtr<FExtender> Extender)
{
	ToolbarExtenders.Remove(Extender);
}

void FAssetEditorToolkit::SetMenuOverlay( TSharedRef<SWidget> Widget )
{
	StandaloneHost.Pin()->SetMenuOverlay( Widget );
}

void FAssetEditorToolkit::AddToolbarWidget(TSharedRef<SWidget> Widget)
{
	ToolbarWidgets.AddUnique(Widget);
}

void FAssetEditorToolkit::RemoveAllToolbarWidgets()
{
	ToolbarWidgets.Empty();
}


void FAssetEditorToolkit::FGCEditingObjects::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(OwnerToolkit.EditingObjects);
}




TSharedPtr<FExtender> FExtensibilityManager::GetAllExtenders()
{
	return FExtender::Combine(Extenders);
}
	
TSharedPtr<FExtender> FExtensibilityManager::GetAllExtenders(const TSharedRef<FUICommandList>& CommandList, const TArray<UObject*>& ContextSensitiveObjects)
{
	auto OutExtenders = Extenders;
	for (int32 i = 0; i < ExtenderDelegates.Num(); ++i)
	{
		if (ExtenderDelegates[i].IsBound())
		{
			OutExtenders.Add(ExtenderDelegates[i].Execute(CommandList, ContextSensitiveObjects));
		}
	}
	return FExtender::Combine(OutExtenders);
}
	
#undef LOCTEXT_NAMESPACE
