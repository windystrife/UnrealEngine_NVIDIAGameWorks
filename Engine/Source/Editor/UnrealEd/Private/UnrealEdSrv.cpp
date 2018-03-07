// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "Misc/MessageDialog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "GameFramework/Actor.h"
#include "RawIndexBuffer.h"
#include "Model.h"
#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "Builders/CubeBuilder.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Settings/LevelEditorMiscSettings.h"
#include "Engine/Brush.h"
#include "AssetData.h"
#include "Editor/EditorEngine.h"
#include "ISourceControlModule.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "EditorFramework/AssetImportData.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/BrushComponent.h"
#include "PhysicsEngine/RadialForceComponent.h"
#include "Engine/Polys.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "EditorDirectories.h"
#include "FileHelpers.h"
#include "UnrealEdGlobals.h"
#include "UObject/UObjectIterator.h"
#include "StaticMeshResources.h"
#include "EditorSupportDelegates.h"
#include "BusyCursor.h"
#include "ScopedTransaction.h"
#include "LevelUtils.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "Interfaces/IMainFrameModule.h"
#include "EditorLevelUtils.h"
#include "EditorBuildUtils.h"
#include "ScriptDisassembler.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Editor/GeometryMode/Public/GeometryEdMode.h"
#include "AssetRegistryModule.h"
#include "Matinee/MatineeActor.h"
#include "MatineeExporter.h"
#include "FbxExporter.h"
#include "DesktopPlatformModule.h"
#include "SnappingUtils.h"
#include "AssetSelection.h"
#include "HighResScreenshot.h"
#include "ActorEditorUtils.h"
#include "Editor/ActorPositioning.h"
#include "Matinee/InterpData.h"
#include "LandscapeInfo.h"
#include "LandscapeInfoMap.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Logging/LogScopedCategoryAndVerbosityOverride.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "EngineUtils.h"
#include "AutoReimport/AssetSourceFilenameCache.h"
#if PLATFORM_WINDOWS
	#include "WindowsHWrapper.h"
#endif
#include "ActorGroupingUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealEdSrv, Log, All);

#define LOCTEXT_NAMESPACE "UnrealEdSrv"

//@hack: this needs to be cleaned up!
static TCHAR TempStr[MAX_EDCMD];
static uint16 Word1;


/**
 * Dumps a set of selected objects to debugf.
 */
static void PrivateDumpSelection(USelection* Selection)
{
	for ( FSelectionIterator Itor(*Selection) ; Itor ; ++Itor )
	{
		UObject *CurObject = *Itor;
		if ( CurObject )
		{
			UE_LOG(LogUnrealEdSrv, Log, TEXT("    %s"), *CurObject->GetClass()->GetName() );
		}
		else
		{
			UE_LOG(LogUnrealEdSrv, Log, TEXT("    NULL object"));
		}
	}
}

class SModalWindowTest : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SModalWindowTest ){}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs )
	{
		this->ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew( SVerticalBox )
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Center)
					[
						SNew( STextBlock )
						.Text( LOCTEXT("ModelTestWindowLabel", "This is a modal window test") )
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Center)
					[
						SNew( SButton )
						.Text( LOCTEXT("NewModalTestWindowButtonLabel", "New Modal Window") )
						.OnClicked( this, &SModalWindowTest::OnNewModalWindowClicked )
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew( SButton )
						.Text( NSLOCTEXT("UnrealEd", "OK", "OK") )
						.OnClicked( this, &SModalWindowTest::OnOKClicked )
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew( SButton )
						.Text( NSLOCTEXT("UnrealEd", "Cancel", "Cancel") )
						.OnClicked( this, &SModalWindowTest::OnCancelClicked )
					]
				]
			]
		];
	}
	
	SModalWindowTest()
		: bUserResponse( false )
	{
	}

	void SetWindow( TSharedPtr<SWindow> InWindow )
	{
		MyWindow = InWindow;
	}

	bool GetResponse() const { return bUserResponse; }

private:
	
	FReply OnOKClicked()
	{
		bUserResponse = true;
		MyWindow->RequestDestroyWindow();
		return FReply::Handled();
	}

	FReply OnCancelClicked()
	{
		bUserResponse = false;
		MyWindow->RequestDestroyWindow();
		return FReply::Handled();
	}

	FReply OnNewModalWindowClicked()
	{
		TSharedRef<SModalWindowTest> ModalWindowContent = SNew(SModalWindowTest);
		TSharedRef<SWindow> ModalWindow = SNew(SWindow)
			.Title( LOCTEXT("TestModalWindowTitle", "Modal Window") )
			.ClientSize(FVector2D(250,100))
			[
				ModalWindowContent
			];

		ModalWindowContent->SetWindow( ModalWindow );

		FSlateApplication::Get().AddModalWindow( ModalWindow, AsShared() );

		UE_LOG(LogUnrealEdSrv, Log, TEXT("Modal Window Returned"));

		return FReply::Handled();
	}

	FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
	{
		if( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
		{
			struct Local
			{
				static void FillSubMenuEntries( FMenuBuilder& MenuBuilder )
				{
					MenuBuilder.AddMenuEntry( LOCTEXT("TestItem2", "Test Item 2"), LOCTEXT("TestToolTip", "TestToolTip"), FSlateIcon(), FUIAction() );

					MenuBuilder.AddMenuEntry( LOCTEXT("TestItem3", "Test Item 3"), LOCTEXT("TestToolTip", "TestToolTip"), FSlateIcon(), FUIAction() );

					MenuBuilder.AddSubMenu( LOCTEXT("SubMenu", "Sub Menu"), LOCTEXT("OpensASubmenu", "Opens a submenu"), FNewMenuDelegate::CreateStatic( &Local::FillSubMenuEntries ) );

					MenuBuilder.AddSubMenu( LOCTEXT("SubMenu2", "Sub Menu2"), LOCTEXT("OpensASubmenu", "Opens a submenu"), FNewMenuDelegate::CreateStatic( &Local::FillSubMenuEntries ) );
				}
			};

			FMenuBuilder NewMenu( true, NULL );
			NewMenu.BeginSection("TestMenuModalWindow", LOCTEXT("MenuInAModalWindow", "Menu in a modal window") );
			{
				NewMenu.AddMenuEntry( LOCTEXT("TestItem1", "Test Item 1"), FText::GetEmpty(), FSlateIcon(), FUIAction() );
				NewMenu.AddSubMenu( LOCTEXT("SubMenu", "Sub Menu"), LOCTEXT("OpenASubmenu", "Opens a sub menu"), FNewMenuDelegate::CreateStatic( &Local::FillSubMenuEntries ) );
			}
			NewMenu.EndSection();

			FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
			FSlateApplication::Get().PushMenu(SharedThis(this), WidgetPath, NewMenu.MakeWidget(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::None));

			return FReply::Handled();
		}

		return FReply::Unhandled();
	}

	TSharedPtr<SWindow> MyWindow;
	bool bUserResponse;
};

UPackage* UUnrealEdEngine::GeneratePackageThumbnailsIfRequired( const TCHAR* Str, FOutputDevice& Ar, TArray<FString>& GeneratedThumbNamesList )
{
	UPackage* Pkg = NULL;
	if( FParse::Command( &Str, TEXT( "SavePackage" ) ) )
	{
		static TCHAR TempFname[MAX_EDCMD];
		if( FParse::Value( Str, TEXT( "FILE=" ), TempFname, 256 ) && ParseObject<UPackage>( Str, TEXT( "Package=" ), Pkg, NULL ) )
		{
			// Update any thumbnails for objects in this package that were modified or generate
			// new thumbnails for objects that don't have any

			bool bSilent = false;
			FParse::Bool( Str, TEXT( "SILENT=" ), bSilent );

			// Make a list of packages to query (in our case, just the package we're saving)
			TArray< UPackage* > Packages;
			Packages.Add( Pkg );

			// Allocate a new thumbnail map if we need one
			if( !Pkg->ThumbnailMap )
			{
				Pkg->ThumbnailMap = MakeUnique<FThumbnailMap>();
			}

			// OK, now query all of the browsable objects in the package we're about to save
			TArray< UObject* > BrowsableObjectsInPackage;

			// Load the asset tools module to get access to thumbnail tools
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
				
			// NOTE: The package should really be fully loaded before we try to generate thumbnails
			PackageTools::GetObjectsInPackages(
				&Packages,														// Packages to search
				BrowsableObjectsInPackage );									// Out: Objects

			// Check to see if any of the objects need thumbnails generated
			TSet< UObject* > ObjectsMissingThumbnails;
			TSet< UObject* > ObjectsWithThumbnails;
			for( int32 CurObjectIndex = 0; CurObjectIndex < BrowsableObjectsInPackage.Num(); ++CurObjectIndex )
			{
				UObject* CurObject = BrowsableObjectsInPackage[ CurObjectIndex ];
				check( CurObject != NULL );

				bool bUsesGenericThumbnail = AssetToolsModule.Get().AssetUsesGenericThumbnail(FAssetData(CurObject));

				// Archetypes always use a shared thumbnail
				if( CurObject->HasAllFlags( RF_ArchetypeObject ) )
				{
					bUsesGenericThumbnail = true;
				}

				bool bPrintThumbnailDiagnostics = false;
				GConfig->GetBool(TEXT("Thumbnails"), TEXT("Debug"), bPrintThumbnailDiagnostics, GEditorPerProjectIni);

				const FObjectThumbnail* ExistingThumbnail = ThumbnailTools::FindCachedThumbnail( CurObject->GetFullName() );
				if (bPrintThumbnailDiagnostics)
				{
					UE_LOG(LogUnrealEdSrv, Log, TEXT("Saving Thumb for %s"), *CurObject->GetFullName());
					UE_LOG(LogUnrealEdSrv, Log, TEXT("   Thumb existed = %d"), (ExistingThumbnail!=NULL) ? 1: 0);
					UE_LOG(LogUnrealEdSrv, Log, TEXT("   Shared Thumb = %d"), (bUsesGenericThumbnail) ? 1: 0);
				}
				//if it's not generatable, let's make sure it doesn't have a custom thumbnail before saving
				if (!ExistingThumbnail && bUsesGenericThumbnail)
				{
					//let it load the custom icons from disk
					// @todo CB: Batch up requests for multiple thumbnails!
					TArray< FName > ObjectFullNames;
					FName ObjectFullNameFName( *CurObject->GetFullName() );
					ObjectFullNames.Add( ObjectFullNameFName );

					// Load thumbnails
					FThumbnailMap& LoadedThumbnails = Pkg->AccessThumbnailMap();
					if( ThumbnailTools::ConditionallyLoadThumbnailsForObjects( ObjectFullNames, LoadedThumbnails ) )
					{
						//store off the names of the thumbnails that were loaded as part of a save so we can delete them after the save
					GeneratedThumbNamesList.Add(ObjectFullNameFName.ToString());

						if (bPrintThumbnailDiagnostics)
						{
							UE_LOG(LogUnrealEdSrv, Log, TEXT("   Unloaded thumb loaded successfully"));
						}

						ExistingThumbnail = LoadedThumbnails.Find( ObjectFullNameFName );
						if (bPrintThumbnailDiagnostics)
						{
							UE_LOG(LogUnrealEdSrv, Log, TEXT("   Newly loaded thumb exists = %d"), (ExistingThumbnail!=NULL) ? 1: 0);
							if (ExistingThumbnail)
							{
								UE_LOG(LogUnrealEdSrv, Log, TEXT("   Thumb created after proper version = %d"), (ExistingThumbnail->IsCreatedAfterCustomThumbsEnabled()) ? 1: 0);
							}
						}

						if (ExistingThumbnail && !ExistingThumbnail->IsCreatedAfterCustomThumbsEnabled())
						{
							if (bPrintThumbnailDiagnostics)
							{
								UE_LOG(LogUnrealEdSrv, Log,  TEXT("   WIPING OUT THUMBNAIL!!!!"));
							}

							//Casting away const to save memory behind the scenes
							FObjectThumbnail* ThumbToClear = (FObjectThumbnail*)ExistingThumbnail;
							ThumbToClear->SetImageSize(0, 0);
							ThumbToClear->AccessImageData().Empty();
						}
					}
					else
					{
						if (bPrintThumbnailDiagnostics)
						{
							UE_LOG(LogUnrealEdSrv, Log, TEXT("   Unloaded thumb does not exist"));
						}
					}
				}

				if ( bUsesGenericThumbnail )
				{
					// This is a generic thumbnail object, but it may have a custom thumbnail.
					if( ExistingThumbnail != NULL && !ExistingThumbnail->IsEmpty() )
					{
						ObjectsWithThumbnails.Add( CurObject );
					}
				}
				else
				{
					// This is not a generic thumbnail object, so if it is dirty or missing we will render it.
					if( ExistingThumbnail != NULL && !ExistingThumbnail->IsEmpty() && !ExistingThumbnail->IsDirty() )
					{
						ObjectsWithThumbnails.Add( CurObject );
					}
					else
					{
						ObjectsMissingThumbnails.Add( CurObject );
					}
				}
			}


			if( BrowsableObjectsInPackage.Num() > 0 )
			{
				// Missing some thumbnails, so go ahead and try to generate them now

				// Start a busy cursor
				const FScopedBusyCursor BusyCursor;

				if( !bSilent )
				{
					const bool bWantProgressMeter = true;
					GWarn->BeginSlowTask( NSLOCTEXT("UnrealEd", "SavingPackage_GeneratingThumbnails", "Generating thumbnails..." ), bWantProgressMeter );
				}

				Ar.Logf( TEXT( "OBJ SavePackage: Generating thumbnails for [%i] asset(s) in package [%s] ([%i] browsable assets)..." ), ObjectsMissingThumbnails.Num(), *Pkg->GetName(), BrowsableObjectsInPackage.Num() );

				for( int32 CurObjectIndex = 0; CurObjectIndex < BrowsableObjectsInPackage.Num(); ++CurObjectIndex )
				{
					UObject* CurObject = BrowsableObjectsInPackage[ CurObjectIndex ];
					check( CurObject != NULL );

					if( !bSilent )
					{
						GWarn->UpdateProgress( CurObjectIndex, BrowsableObjectsInPackage.Num() );
					}


					bool bNeedEmptyThumbnail = false;
					if( ObjectsMissingThumbnails.Contains( CurObject ) && !GIsAutomationTesting )
					{
						// Generate a thumbnail!
						FObjectThumbnail* GeneratedThumbnail = ThumbnailTools::GenerateThumbnailForObjectToSaveToDisk( CurObject );
						if( GeneratedThumbnail != NULL )
						{
							Ar.Logf( TEXT( "OBJ SavePackage:     Rendered thumbnail for [%s]" ), *CurObject->GetFullName() );
						}
						else
						{
							// Couldn't generate a thumb; perhaps this object doesn't support thumbnails?
							bNeedEmptyThumbnail = true;
						}
					}
					else if( !ObjectsWithThumbnails.Contains( CurObject ) )
					{
						// Even though this object uses a shared thumbnail, we'll add a "dummy thumbnail" to
						// the package (zero dimension) for all browsable assets so that the Content Browser
						// can quickly verify that existence of assets on the fly.
						bNeedEmptyThumbnail = true;
					}


					// Create an empty thumbnail if we need to.  All browsable assets need at least a placeholder
					// thumbnail so the Content Browser can check for non-existent assets in the background
					if( bNeedEmptyThumbnail )
					{
						UPackage* MyOutermostPackage = CurObject->GetOutermost();
						ThumbnailTools::CacheEmptyThumbnail( CurObject->GetFullName(), MyOutermostPackage );
					}
				}

				Ar.Logf( TEXT( "OBJ SavePackage: Finished generating thumbnails for package [%s]" ), *Pkg->GetName() );

				if( !bSilent )
				{
					GWarn->UpdateProgress( 1, 1 );
					GWarn->EndSlowTask();
				}
			}
		}
	}
	return Pkg;
}

bool UUnrealEdEngine::HandleDumpModelGUIDCommand( const TCHAR* Str, FOutputDevice& Ar )
{
	for (TObjectIterator<UModel> It; It; ++It)
	{
		UE_LOG(LogUnrealEdSrv, Log, TEXT("%s Guid = '%s'"), *It->GetFullName(), *It->LightingGuid.ToString());
	}
return true;
}

bool UUnrealEdEngine::HandleModalTestCommand( const TCHAR* Str, FOutputDevice& Ar )
{
	TSharedRef<SModalWindowTest> MessageBox = SNew(SModalWindowTest);
	TSharedRef<SWindow> ModalWindow = SNew(SWindow)
		.Title( LOCTEXT("WindowTitle", "Modal Window") )
		.ClientSize(FVector2D(250,100))
		[
			MessageBox
		];

	MessageBox->SetWindow( ModalWindow );

	GEditor->EditorAddModalWindow( ModalWindow );

	UE_LOG(LogUnrealEdSrv, Log,  TEXT("User response was: %s"), MessageBox->GetResponse() ? TEXT("OK") : TEXT("Cancel") );
	return true;
}

bool UUnrealEdEngine::HandleDumpBPClassesCommand( const TCHAR* Str, FOutputDevice& Ar )
{
	UE_LOG(LogUnrealEdSrv, Log, TEXT("--- Listing all blueprint generated classes ---"));
	for( TObjectIterator<UClass> it; it; ++it )
	{
		const UClass* CurrentClass = *it;
		if( CurrentClass && CurrentClass->ClassGeneratedBy )
		{
			UE_LOG(LogUnrealEdSrv, Log, TEXT("  %s (%s)"), *CurrentClass->GetName(), *CurrentClass->GetOutermost()->GetName());
		}
	}
	return true;
}

bool UUnrealEdEngine::HandleFindOutdateInstancesCommand( const TCHAR* Str, FOutputDevice& Ar )
{
	UE_LOG(LogUnrealEdSrv, Log, TEXT("--- Finding all actor instances with outdated classes ---"));
	int32 NumFound = 0;
	for( TObjectIterator<UObject> it; it; ++it )
	{
		const UObject* CurrentObj = *it;
		if( CurrentObj->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists) )
		{
			UE_LOG(LogUnrealEdSrv, Log, TEXT("  %s (%s)"), *CurrentObj->GetName(), *CurrentObj->GetClass()->GetName());
			NumFound++;
		}
	}
	UE_LOG(LogUnrealEdSrv, Log, TEXT("Found %d instance(s)."), NumFound);
	return true;
}

bool UUnrealEdEngine::HandleDumpSelectionCommand( const TCHAR* Str, FOutputDevice& Ar )
{
	UE_LOG(LogUnrealEdSrv, Log, TEXT("Selected Actors:"));
	PrivateDumpSelection( GetSelectedActors() );
	UE_LOG(LogUnrealEdSrv, Log, TEXT("Selected Non-Actors:"));
	PrivateDumpSelection( GetSelectedObjects() );
	return true;
}


bool UUnrealEdEngine::HandleBuildLightingCommand( const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld )
{
	return FEditorBuildUtils::EditorBuild(InWorld, FBuildOptions::BuildLighting);	
}

bool UUnrealEdEngine::HandleBuildPathsCommand( const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld )
{
	return FEditorBuildUtils::EditorBuild(InWorld, FBuildOptions::BuildAIPaths);
}

bool UUnrealEdEngine::HandleRecreateLandscapeCollisionCommand(const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld)
{
	if (!PlayWorld && InWorld && InWorld->GetWorldSettings())
	{
		for (auto It = ULandscapeInfoMap::GetLandscapeInfoMap(InWorld).Map.CreateIterator(); It; ++It)
		{
			ULandscapeInfo* Info = It.Value();
			Info->RecreateCollisionComponents();
		}
	}
	return true;
}

bool UUnrealEdEngine::HandleRemoveLandscapeXYOffsetsCommand(const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld)
{
	if (!PlayWorld && InWorld && InWorld->GetWorldSettings())
	{
		for (auto It = ULandscapeInfoMap::GetLandscapeInfoMap(InWorld).Map.CreateIterator(); It; ++It)
		{
			ULandscapeInfo* Info = It.Value();
			Info->RemoveXYOffsets();
		}
	}
	return true;
}

bool UUnrealEdEngine::HandleConvertMatineesCommand( const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld )
{
	FVector StartLocation= FVector::ZeroVector;
	if( InWorld )
	{
		ULevel* Level = InWorld->GetCurrentLevel();
		if( !Level )
		{
			Level = InWorld->PersistentLevel;
	}
		check(Level);
		for( TObjectIterator<UInterpData> It; It; ++It )
		{
			UInterpData* InterpData = *It;
			if( InterpData->IsIn( Level ) ) 
			{
				// We dont care about renaming references or adding redirectors.  References to this will be old seqact_interps
				GEditor->RenameObject( InterpData, Level->GetOutermost(), *InterpData->GetName() );

				AMatineeActor* MatineeActor = Level->OwningWorld->SpawnActor<AMatineeActor>(StartLocation, FRotator::ZeroRotator);
				StartLocation.Y += 50;

				MatineeActor->MatineeData = InterpData;
				UProperty* MatineeDataProp = NULL;
				for( UProperty* Property = MatineeActor->GetClass()->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext )
				{
					if( Property->GetName() == TEXT("MatineeData") )
					{
						MatineeDataProp = Property;
						break;
					}
				}

				FPropertyChangedEvent PropertyChangedEvent( MatineeDataProp ); 
				MatineeActor->PostEditChangeProperty( PropertyChangedEvent );
			}
		}
	}
		return true;
	}

bool UUnrealEdEngine::HandleDisasmScriptCommand( const TCHAR* Str, FOutputDevice& Ar )
	{
		FString ClassName;

		if (FParse::Token(Str, ClassName, false))
		{
			FKismetBytecodeDisassembler::DisassembleAllFunctionsInClasses(Ar, ClassName);
		}

		return true;
	}

bool UUnrealEdEngine::Exec( UWorld* InWorld, const TCHAR* Stream, FOutputDevice& Ar )
{
	const TCHAR* Str = Stream;
	// disallow set commands in the editor as that modifies the default object, affecting object serialization
	if (FParse::Command(&Str, TEXT("SET")) || FParse::Command(&Str, TEXT("SETNOPEC")))
	{
		Ar.Logf(TEXT("Set commands not allowed in the editor"));
		return true;
	}

	//for thumbnail reclamation post save
	UPackage* Pkg = NULL;
	//thumbs that are loaded expressly for the sake of saving.  To be deleted again post-save
	TArray<FString> ThumbNamesToUnload;
	
	// Peek for the SavePackage command and generate thumbnails for the package if we need to
	// NOTE: The actual package saving happens in the UEditorEngine::Exec_Obj, but we do the 
	//		 thumbnail generation here in UnrealEd
	if( FParse::Command(&Str,TEXT("OBJ")) )
	{
		Pkg = GeneratePackageThumbnailsIfRequired( Str, Ar, ThumbNamesToUnload );
	}

	// If we don't have a viewport specified to catch the stat commands, use to the active viewport.  If there is a game viewport ignore this as we do not want 
	if (GStatProcessingViewportClient == NULL && (GameViewport == NULL || GameViewport->IsSimulateInEditorViewport() ) )
	{
		GStatProcessingViewportClient = GLastKeyLevelEditingViewportClient ? GLastKeyLevelEditingViewportClient : GCurrentLevelEditingViewportClient;
	}

	bool bExecSucceeded = UEditorEngine::Exec( InWorld, Stream, Ar );

	GStatProcessingViewportClient = NULL;

	//if we loaded thumbs for saving, purge them back from the package
	//append loaded thumbs onto the existing thumbs list
	if (Pkg)
	{
		for (int32 ThumbRemoveIndex = 0; ThumbRemoveIndex < ThumbNamesToUnload.Num(); ++ThumbRemoveIndex)
		{
			ThumbnailTools::CacheThumbnail(ThumbNamesToUnload[ThumbRemoveIndex], NULL, Pkg);
		}
	}

	if(bExecSucceeded)
	{
		return true;
	}

	if( FParse::Command(&Str, TEXT("DUMPMODELGUIDS")) )
	{
		HandleDumpModelGUIDCommand( Str, Ar );
	}

	if( FParse::Command(&Str, TEXT("ModalTest") ) )
	{
		HandleModalTestCommand( Str, Ar );
		return true;
	}

	if( FParse::Command(&Str, TEXT("DumpBPClasses")) )
	{
		HandleDumpBPClassesCommand( Str, Ar );
	}

	if( FParse::Command(&Str, TEXT("FindOutdatedInstances")) )
	{
		HandleFindOutdateInstancesCommand( Str, Ar );
	}

	if( FParse::Command(&Str, TEXT("DUMPSELECTION")) )
	{
		HandleDumpSelectionCommand( Str, Ar );
	}

	//----------------------------------------------------------------------------------
	// EDIT
	//
	if( FParse::Command(&Str,TEXT("EDIT")) )
	{
		return Exec_Edit( InWorld, Str, Ar );
	}
	//------------------------------------------------------------------------------------
	// ACTOR: Actor-related functions
	//
	else if (FParse::Command(&Str,TEXT("ACTOR")))
	{
		return Exec_Actor( InWorld, Str, Ar );
	}
	//------------------------------------------------------------------------------------
	// MODE management (Global EDITOR mode):
	//
	else if( FParse::Command(&Str,TEXT("MODE")) )
	{
		return Exec_Mode( Str, Ar );
	}
	//----------------------------------------------------------------------------------
	// PIVOT
	//
	else if( FParse::Command(&Str,TEXT("PIVOT")) )
	{
		return Exec_Pivot( Str, Ar );
	}
	else if (FParse::Command(&Str,TEXT("BUILDLIGHTING")))
	{
		HandleBuildLightingCommand( Str, Ar, InWorld );
	}
	// BUILD PATHS
	else if (FParse::Command(&Str,TEXT("BUILDPATHS")))
	{
		HandleBuildPathsCommand( Str, Ar, InWorld );
	}
#if WITH_EDITOR
	else if (FParse::Command(&Str, TEXT("RecreateLandscapeCollision")))
	{
		// InWorld above is the PIE world if PIE is active, but this is specifically an editor command
		UWorld* World = GetEditorWorldContext().World();
		return HandleRecreateLandscapeCollisionCommand(Str, Ar, World);
	}
	else if (FParse::Command(&Str, TEXT("RemoveLandscapeXYOffsets")))
	{
		// InWorld above is the PIE world if PIE is active, but this is specifically an editor command
		UWorld* World = GetEditorWorldContext().World();
		return HandleRemoveLandscapeXYOffsetsCommand(Str, Ar, World);
	}
#endif // WITH_EDITOR
	else if( FParse::Command(&Str, TEXT("CONVERTMATINEES")) )
	{
		return HandleConvertMatineesCommand( Str, Ar, InWorld );
	}
	else if( FParse::Command(&Str, TEXT("DISASMSCRIPT")) )
	{
		return HandleDisasmScriptCommand( Str, Ar );
	}
#if WITH_EDITOR
	else if (FParse::Command(&Str, TEXT("cook")))
	{
		if (CookServer)
		{
			return CookServer->Exec(InWorld, Str, Ar);
		}
	}
#endif
	else if ( FParse::Command(&Str, TEXT("GROUPS")) )
	{
		return Exec_Group( Str, Ar );
	}
	// #ttp 322815 - GDC, temp exec command for scaling the level
	else if ( FParse::Command(&Str,TEXT("SCALELEVEL")) )
	{
		// e.g. ScaleLevel Scale=1,2,3 Snap=4	// Non-uniform scaling
		// e.g. ScaleLevel Scale=2 Snap=4		// Uniform scaling

		// We can only scale radii if the level is given uniform scaling
		bool bScale = false;
		bool bScaleRadii = false;

		float Scale = 1.0f;
		FString ScaleStr;
		FVector ScaleVec( Scale );
		if(FParse::Value( Str, TEXT("Scale="), ScaleStr, false) && GetFVECTOR( *ScaleStr, ScaleVec ))
		{
			// Update uniform incase the user used uniform scale with a vector parm
			Scale = ScaleVec.X;
			bScaleRadii = (Scale == ScaleVec.Y && Scale == ScaleVec.Z ? true : false);
			bScale = true;
		}
		else if(FParse::Value( Str, TEXT("Scale="), Scale ))
		{
			// Copy the uniform scale to our vector param
			ScaleVec = FVector( Scale );
			bScaleRadii = true;
			bScale = true;
		}

		// Can we scale the level?
		if(bScale)
		{
			// See if a snap value was specified for the grid
			float NewGridSize;
			const bool bSnap = FParse::Value( Str, TEXT("Snap="), NewGridSize);

			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ScalingLevel", "Scaling Level") );

			// If it was, force the grid size to be this value temporarily
			const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
			TArray<float>& PosGridSizes = const_cast<TArray<float>&>(GetCurrentPositionGridArray());
			float& CurGridSize = PosGridSizes[ViewportSettings->CurrentPosGridSize];
			const float OldGridSize = CurGridSize;
			if ( bSnap )
			{
				CurGridSize = NewGridSize;
			}

			// "iterates through each actor in the current level"
			bool bBuildBSPs = false;
			for( TActorIterator<AActor> It(InWorld); It; ++It )
			{
				AActor* Actor = *It;

				// "It should skip all static meshes.  The reason for this is that they will scale the static meshes via the static mesh editor with the new BuildScale setting."
				if( Actor )
				{
					/*if (AStaticMeshActor* StaticMesh = Cast< AStaticMeshActor >( Actor ))
					{
						// Skip static meshes?
					}
					else*/if (ABrush* Brush = Cast< ABrush >( Actor ))
					{
						// "For volumes and brushes scale each vertex by the specified amount."
						if ( !FActorEditorUtils::IsABuilderBrush(Brush) && Brush->Brush )
						{
							const FVector OldLocation = Brush->GetActorLocation();
							const FVector NewLocation = OldLocation * ScaleVec;
							Brush->Modify();
							Brush->SetActorLocation( NewLocation );
							
							Brush->Brush->Modify();
							for( int32 poly = 0 ; poly < Brush->Brush->Polys->Element.Num() ; poly++ )
							{
								FPoly* Poly = &(Brush->Brush->Polys->Element[poly]);

								Poly->TextureU /= ScaleVec;
								Poly->TextureV /= ScaleVec;
								Poly->Base = ((Poly->Base - Brush->GetPivotOffset()) * ScaleVec) + Brush->GetPivotOffset();

								for( int32 vtx = 0 ; vtx < Poly->Vertices.Num() ; vtx++ )
								{
									Poly->Vertices[vtx] = ((Poly->Vertices[vtx] - Brush->GetPivotOffset()) * ScaleVec) + Brush->GetPivotOffset();

									// "Then snap the vertices new positions by the specified Snap amount"
									if ( bSnap )
									{
										FSnappingUtils::SnapPointToGrid( Poly->Vertices[vtx], FVector(0, 0, 0) );
									}
								}

								Poly->CalcNormal();
							}
							
							Brush->Brush->BuildBound();
							Brush->MarkPackageDirty();
							bBuildBSPs = true;
						}
					}
					else
					{
						// "Do not scale any child components."
						if( Actor->GetAttachParentActor() == NULL )
						{
							// "Only the root component"
							if (USceneComponent *RootComponent = Actor->GetRootComponent())
							{
								RootComponent->Modify();

								// "scales root component by the specified amount."
								const FVector OldLocation = RootComponent->GetComponentLocation();
								const FVector NewLocation = OldLocation * ScaleVec;
								RootComponent->SetWorldLocation( NewLocation );

								// Scale up the triggers
								if (UBoxComponent* BoxComponent = Cast< UBoxComponent >( RootComponent ))
								{
									const FVector OldExtent = BoxComponent->GetUnscaledBoxExtent();
									const FVector NewExtent = OldExtent * ScaleVec;
									BoxComponent->SetBoxExtent( NewExtent );
								}

								if ( bScaleRadii )
								{
									if (USphereComponent* SphereComponent = Cast< USphereComponent >( RootComponent ))
									{
										const float OldRadius = SphereComponent->GetUnscaledSphereRadius();
										const float NewRadius = OldRadius * Scale;
										SphereComponent->SetSphereRadius( NewRadius );
									}
									else if (UCapsuleComponent* CapsuleComponent = Cast< UCapsuleComponent >( RootComponent ))
									{
										float OldRadius, OldHalfHeight;
										CapsuleComponent->GetUnscaledCapsuleSize( OldRadius, OldHalfHeight );
										const float NewRadius = OldRadius * Scale;
										const float NewHalfHeight = OldHalfHeight * Scale;
										CapsuleComponent->SetCapsuleSize( NewRadius, NewHalfHeight );
									}
									else if (UPointLightComponent* PointLightComponent = Cast< UPointLightComponent >( RootComponent ))
									{
										PointLightComponent->AttenuationRadius *= Scale;
										PointLightComponent->SourceRadius *= Scale;
										PointLightComponent->SourceLength *= Scale;
									}
									else if (URadialForceComponent* RadialForceComponent = Cast< URadialForceComponent >( RootComponent ))
									{
										RadialForceComponent->Radius *= Scale;
									}
									/* Other components that have radii
									UPathFollowingComponent
									USmartNavLinkComponent
									UPawnSensingComponent
									USphereReflectionCaptureComponent
									UAIPerceptionComponent
									*/
								}
							}
						}
					}
				}
			}

			// Restore snap
			if ( bSnap )
			{
				CurGridSize = OldGridSize;
			}

			// Kick off a rebuild if any of the bsps have changed
			if ( bBuildBSPs )
			{
				GUnrealEd->Exec( InWorld, TEXT("MAP REBUILD ALLVISIBLE") );
			}
		}

		return true;
	}
	else if( FParse::Command(&Str, TEXT("ScaleMeshes") ) )
	{
		bool bScale = false;
		bool bScaleVec = false;

		// Was just a scale specified
		float Scale=1.0f;
		FVector BoxVec(Scale);
		if(FParse::Value(Str, TEXT("Scale="), Scale))
		{
			bScale = true;
		}
		else
		{
			// or was a bounding box specified instead
			FString BoxStr;	
			if((FParse::Value( Str, TEXT("BBOX="), BoxStr, false) || FParse::Value( Str, TEXT("FFD="), BoxStr, false)) && GetFVECTOR( *BoxStr, BoxVec ))
			{
				bScaleVec = true;
			}
		}

		if ( bScale || bScaleVec )
		{
			USelection* SelectedObjects = GetSelectedObjects();
			TArray<UStaticMesh*> SelectedMeshes;
			SelectedObjects->GetSelectedObjects(SelectedMeshes);

			if( SelectedMeshes.Num() )
			{
				GWarn->BeginSlowTask(NSLOCTEXT("UnrealEd", "ScalingStaticMeshes", "Scaling Static Meshes"), true, true);

				for (int32 MeshIndex = 0; MeshIndex < SelectedMeshes.Num(); ++MeshIndex)
				{
					UStaticMesh* Mesh = SelectedMeshes[MeshIndex];

					if (Mesh && Mesh->SourceModels.Num() > 0)
					{
						Mesh->Modify();

						GWarn->StatusUpdate(MeshIndex + 1, SelectedMeshes.Num(), FText::Format(NSLOCTEXT("UnrealEd", "ScalingStaticMeshes_Value", "Static Mesh: %s"), FText::FromString(Mesh->GetName())));

						FStaticMeshSourceModel& Model = Mesh->SourceModels[0];

						FVector ScaleVec(Scale, Scale, Scale);	// bScale
						if ( bScaleVec )
						{
							FBoxSphereBounds Bounds = Mesh->GetBounds();
							ScaleVec = BoxVec / (Bounds.BoxExtent * 2.0f);	// x2 as artists wanted length not radius	
						}
						Model.BuildSettings.BuildScale3D *= ScaleVec;	// Scale by the current modification
						
						UE_LOG(LogUnrealEdSrv, Log, TEXT("Rescaling mesh '%s' with scale: %s"), *Mesh->GetName(), *Model.BuildSettings.BuildScale3D.ToString() );
						
						Mesh->Build();
					}
				}
				GWarn->EndSlowTask();
			}
		}
	}
	else if( FParse::Command(&Str, TEXT("ClearSourceFiles") ) )
	{
		struct Local
		{
			static bool RemoveSourcePath( const FAssetImportInfo& ImportInfo, const FAssetData& AssetData, const TArray<FString>* SearchTerms )
			{
				FAssetImportInfo AssetImportInfo;

				bool bModified = false;
				for (const auto& File : ImportInfo.SourceFiles)
				{
					const bool bRemoveFile = File.RelativeFilename.IsEmpty() || !SearchTerms ||
						SearchTerms->ContainsByPredicate([&](const FString& SearchTerm){ return File.RelativeFilename.Contains(SearchTerm); });

					if( bRemoveFile )
					{
						UE_LOG(LogUnrealEdSrv, Log, TEXT("Removing Path: %s"), *File.RelativeFilename);
						bModified = true;
					}
					else
					{
						AssetImportInfo.Insert(File);
					}
				}

				if (bModified)
				{
					if (UObject* Asset = AssetData.GetAsset())
					{
						UAssetImportData* ImportData = nullptr;

						// Root out the asset import data property
						for (UObjectProperty* Property : TFieldRange<UObjectProperty>(Asset->GetClass()))
						{
							ImportData = Cast<UAssetImportData>(Property->GetObjectPropertyValue(Property->ContainerPtrToValuePtr<UObject*>(Asset)));
							if (ImportData)
							{
								Asset->Modify();
								ImportData->SourceData = AssetImportInfo;
								return true;
							}
						}
					}
				}

				return false;
			}

			static void RemoveSourcePaths( const TArray<FAssetData>& AllAssets, const TArray<FString>* SearchTerms )
			{
				FScopedSlowTask SlowTask(AllAssets.Num(), NSLOCTEXT("UnrealEd", "ClearingSourceFiles", "Clearing Source Files"));
				SlowTask.MakeDialog(true);

				for (const FAssetData& Asset : AllAssets)
				{
					SlowTask.EnterProgressFrame();

					// Optimization - check the asset has import information before loading it
					TOptional<FAssetImportInfo> ImportInfo = FAssetSourceFilenameCache::ExtractAssetImportInfo(Asset);
					if (ImportInfo.IsSet() && ImportInfo->SourceFiles.Num())
					{
						RemoveSourcePath(ImportInfo.GetValue(), Asset, SearchTerms);
					}
				}
			}
		};

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		FString Path;
		FParse::Value(Str, TEXT("Path="), Path, false);

		TArray<FAssetData> AllAssets;
		if (!Path.IsEmpty())
		{
			AssetRegistryModule.Get().GetAssetsByPath(*Path, AllAssets, true);
		}
		else
		{
			AssetRegistryModule.Get().GetAllAssets(AllAssets);
		}

		FString SearchTermStr;
		if (FParse::Value(Str, TEXT("Find="), SearchTermStr, false))
		{
			// Searching for particular paths to remove
			TArray<FString> SearchTerms;
			SearchTermStr.ParseIntoArray( SearchTerms, TEXT(","), true );

			TArray<UObject*> ModifiedObjects;
			if( SearchTerms.Num() )
			{
				Local::RemoveSourcePaths(AllAssets, &SearchTerms);
			}
		}
		else
		{
			// Remove every source path on any asset
			Local::RemoveSourcePaths(AllAssets, nullptr);
		}
	}
	else if (FParse::Command(&Str, TEXT("RenameAssets")))
	{
		FString SearchTermStr;
		if ( FParse::Value(Str, TEXT("Find="), SearchTermStr) )
		{
			FString ReplaceStr;
			FParse::Value(Str, TEXT("Replace="), ReplaceStr );

			GWarn->BeginSlowTask(NSLOCTEXT("UnrealEd", "RenamingAssets", "Renaming Assets"), true, true);

			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

			TArray<FAssetData> AllAssets;
			AssetRegistryModule.Get().GetAllAssets( AllAssets );

			TArray<FAssetRenameData> AssetsToRename;
			for( const FAssetData& Asset : AllAssets )
			{
				bool bRenamedPath = false;
				bool bRenamedAsset = false;
				FString NewAssetName = Asset.AssetName.ToString();
				FString NewPathName = Asset.PackagePath.ToString();
				if( NewAssetName.Contains( SearchTermStr ) )
				{
					FString TempPathName = NewAssetName.Replace(*SearchTermStr, *ReplaceStr);
					if (!TempPathName.IsEmpty())
					{
						NewAssetName = TempPathName;
						bRenamedAsset = true;
					}
				}
				
				if( NewPathName.Contains( SearchTermStr ) )
				{
					FString TempPathName = NewPathName.Replace( *SearchTermStr, *ReplaceStr );
					FPaths::RemoveDuplicateSlashes(TempPathName);

					if( !TempPathName.IsEmpty() )
					{
						NewPathName = TempPathName;
						bRenamedPath = true;
					}
				}

				if( bRenamedAsset || bRenamedPath )
				{
					FAssetRenameData RenameData(Asset.GetAsset(), NewPathName, NewAssetName);
					AssetsToRename.Add(RenameData);
				}
			}

			if( AssetsToRename.Num() > 0 )
			{
				AssetTools.RenameAssets( AssetsToRename );
			}

			GWarn->EndSlowTask();
		}
	}
	else if( FParse::Command(&Str, TEXT("HighResShot") ) )
	{
		if (GetHighResScreenshotConfig().ParseConsoleCommand(Str, Ar))
		{
			TakeHighResScreenShots();
		}

		return true;
	}
	else if( FParse::Command(&Str, TEXT("EditorShot")) || FParse::Command(&Str, TEXT("EditorScreenShot")) )
	{
		struct Local
		{
			static void TakeScreenShotOfWidget( TSharedRef<SWidget> InWidget )
			{
				TArray<FColor> OutImageData;
				FIntVector OutImageSize;
				if (FSlateApplication::Get().TakeScreenshot(InWidget, OutImageData, OutImageSize))
				{
					FString FileName;
					const FString BaseFileName = GetDefault<ULevelEditorMiscSettings>()->EditorScreenshotSaveDirectory.Path / TEXT("EditorScreenshot");
					FFileHelper::GenerateNextBitmapFilename(BaseFileName, TEXT("bmp"), FileName);
					FFileHelper::CreateBitmap(*FileName, OutImageSize.X, OutImageSize.Y, OutImageData.GetData());
				}
			}
		};

		if( FSlateApplication::IsInitialized() )
		{
			if( FParse::Command(&Str, TEXT("All") ))
			{
				TArray< TSharedRef<SWindow> > OpenWindows;
				FSlateApplication::Get().GetAllVisibleWindowsOrdered(OpenWindows);
				for( int32 WindowId = 0; WindowId < OpenWindows.Num(); ++WindowId )
				{
					Local::TakeScreenShotOfWidget(OpenWindows[WindowId]);
				}
			}
			else
			{
				FString WindowNameStr;
				if ( FParse::Value(Str, TEXT("Name="), WindowNameStr) )
				{
					TArray< TSharedRef<SWindow> > OpenWindows;
					FSlateApplication::Get().GetAllVisibleWindowsOrdered(OpenWindows);
					for( int32 WindowId = 0; WindowId < OpenWindows.Num(); ++WindowId )
					{
						FString CurrentWindowName = OpenWindows[WindowId]->GetTitle().ToString();

						//Strip off the * from the end if it exists
						if( CurrentWindowName.EndsWith(TEXT("*")) )
						{
							CurrentWindowName = CurrentWindowName.LeftChop(1);
						}

						if( CurrentWindowName == WindowNameStr )
						{
							Local::TakeScreenShotOfWidget(OpenWindows[WindowId]);
						}
					}
				}
				else
				{
					TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
					if( ActiveWindow.IsValid() )
					{
						Local::TakeScreenShotOfWidget(ActiveWindow.ToSharedRef());
					}
				}
			}
		}
		return true;
	}
	return false;
}

bool UUnrealEdEngine::AnyWorldsAreDirty( UWorld* InWorld ) const
{
	// Get the set of all reference worlds.
	TArray<UWorld*> WorldsArray;
	EditorLevelUtils::GetWorlds( InWorld, WorldsArray, true );

	if ( WorldsArray.Num() > 0 )
	{
		FString FinalFilename;
		for ( int32 WorldIndex = 0 ; WorldIndex < WorldsArray.Num() ; ++WorldIndex )
		{
			UWorld* World = WorldsArray[ WorldIndex ];
			UPackage* Package = Cast<UPackage>( World->GetOuter() );
			check( Package );

			// The world needs saving if...
			if ( Package->IsDirty() )
			{
				return true;
			}
		}
	}

	return false;
}


bool UUnrealEdEngine::AnyContentPackagesAreDirty() const
{
	const UPackage* TransientPackage = GetTransientPackage();

	// Check all packages for dirty, non-map, non-transient packages
	for ( TObjectIterator<UPackage> PackageIter; PackageIter; ++PackageIter )
	{
		UPackage* CurPackage = *PackageIter;

		// The package needs saving if it's not the transient package
		if ( CurPackage && ( CurPackage != TransientPackage ) && CurPackage->IsDirty() )
		{
			return true;
		}
	}

	return false;
}


bool UUnrealEdEngine::IsTemplateMap( const FString& MapName ) const
{
	for (TArray<FTemplateMapInfo>::TConstIterator It(TemplateMapInfos); It; ++It)
	{
		if (It->Map == MapName)
		{
			return true;
		}
	}

	return false;
}


bool UUnrealEdEngine::IsUserInteracting()
{
	// Check to see if the user is in the middle of a drag operation.
	bool bUserIsInteracting = false;
	for( int32 ClientIndex = 0 ; ClientIndex < AllViewportClients.Num() ; ++ClientIndex )
	{
		// Check for tracking and capture.  If a viewport has mouse capture, it could be locking the mouse to the viewport, which means if we prompt with a dialog
		// while the mouse is locked to a viewport, we wont be able to interact with the dialog.  
		if ( AllViewportClients[ClientIndex]->IsTracking() ||  AllViewportClients[ClientIndex]->Viewport->HasMouseCapture() )
		{
			bUserIsInteracting = true;
			break;
		}
	}
	
	if( !bUserIsInteracting )
	{
		// When a property window is open and the user is dragging to modify a property with a spinbox control, 
		// the viewport clients will have bIsTracking to false. 
		// We check for the state of the right and left mouse buttons and assume the user is interacting with something if a mouse button is pressed down
		
#if PLATFORM_WINDOWS
		bool bLeftDown = !!(GetAsyncKeyState(VK_LBUTTON) & 0x8000);
		bool bRightDown = !!(GetAsyncKeyState(VK_RBUTTON) & 0x8000);
		bUserIsInteracting = bLeftDown || bRightDown;
#endif
	}

	return bUserIsInteracting;
}

void UUnrealEdEngine::ShowPackageNotification()
{
	if( !FApp::IsUnattended() )
	{
		// Defer prompting for checkout if we cant prompt because of the following:
		// The user is interacting with something,
		// We are performing a slow task
		// We have a play world
		// The user disabled prompting on package modification
		// A window has capture on the mouse
		bool bCanPrompt = !IsUserInteracting() && !GIsSlowTask && !PlayWorld && GetDefault<UEditorLoadingSavingSettings>()->bPromptForCheckoutOnAssetModification && (FSlateApplication::Get().GetMouseCaptureWindow() == NULL);

		if( bCanPrompt )
		{
			bShowPackageNotification = false;
			bool bNeedWarningDialog = false;
			for (const auto& Entry : PackageToNotifyState)
			{
				if (Entry.Value == NS_PendingWarning)
				{
					bNeedWarningDialog = true;
					break;
				}
			}

			// The user is not interacting with anything, prompt to checkout packages that have been modified
			
			struct Local
			{
				static void OpenCheckOutDialog()
				{
					GUnrealEd->PromptToCheckoutModifiedPackages(true);
				}
			};

			if (bNeedWarningDialog)
			{
				Local::OpenCheckOutDialog();
			}
			else
			{
				int32 NumPackagesToCheckOut = GetNumDirtyPackagesThatNeedCheckout();

				FFormatNamedArguments Args;
				Args.Add(TEXT("NumFiles"), NumPackagesToCheckOut);

				FText ErrorText = FText::Format(NSLOCTEXT("SourceControl", "CheckOutNotification", "{NumFiles} files need check-out!"), Args);

				if (!CheckOutNotificationWeakPtr.IsValid())
				{
					FNotificationInfo ErrorNotification(ErrorText);
					ErrorNotification.bFireAndForget = true;;
					ErrorNotification.Hyperlink = FSimpleDelegate::CreateStatic(&Local::OpenCheckOutDialog);
					ErrorNotification.HyperlinkText = NSLOCTEXT("SourceControl", "CheckOutHyperlinkText", "Check-Out");
					ErrorNotification.ExpireDuration = 10.0f; // Need this message to last a little longer than normal since the user will probably want to click the hyperlink to check out files
					ErrorNotification.bUseThrobber = true;

					// For adding notifications.
					CheckOutNotificationWeakPtr = FSlateNotificationManager::Get().AddNotification(ErrorNotification);
				}
				else
				{
					CheckOutNotificationWeakPtr.Pin()->SetText(ErrorText);
					CheckOutNotificationWeakPtr.Pin()->ExpireAndFadeout();
				}
			}
		}
	}
}


void UUnrealEdEngine::AttemptWarnAboutPackageEngineVersions()
{
	if ( bNeedWarningForPkgEngineVer )
	{
		const bool bCanPrompt = !IsUserInteracting() && !GIsSlowTask && !PlayWorld && (FSlateApplication::Get().GetMouseCaptureWindow() == NULL);

		if ( bCanPrompt )
		{
			FString PackageNames;
			for ( TMap<FString, uint8>::TIterator MapIter( PackagesCheckedForEngineVersion ); MapIter; ++MapIter )
			{
				if ( MapIter.Value() == WDWS_PendingWarn )
				{
					PackageNames += FString::Printf( TEXT("%s\n"), *MapIter.Key() );
					MapIter.Value() = WDWS_Warned;
				}
			}
			FFormatNamedArguments Args;
			Args.Add( TEXT("PackageNames"), FText::FromString( PackageNames ) );
			const FText Message = FText::Format( NSLOCTEXT("Core", "PackagesSavedWithNewerVersion", "The following assets have been saved with an engine version newer than the current and therefore will not be able to be saved:\n{PackageNames}"), Args );

			FMessageDialog::Open( EAppMsgType::Ok, Message );
			bNeedWarningForPkgEngineVer = false;
		}
	}
}

void UUnrealEdEngine::AttemptWarnAboutWritePermission()
{
	if ( bNeedWarningForWritePermission )
	{
		const bool bCanPrompt = !IsUserInteracting() && !GIsSlowTask && !PlayWorld && (FSlateApplication::Get().GetMouseCaptureWindow() == NULL);

		if ( bCanPrompt )
		{
			FString PackageNames;
			for ( TMap<FString, uint8>::TIterator MapIter( PackagesCheckedForWritePermission ); MapIter; ++MapIter )
			{
				if ( MapIter.Value() == WDWS_PendingWarn )
				{
					PackageNames += FString::Printf( TEXT("%s\n"), *MapIter.Key() );
					MapIter.Value() = WDWS_Warned;
				}
			}
			
			const FText Message = FText::Format( LOCTEXT("WritePermissionFailure", "You do not have sufficient permission to save the following content to disk. Any changes you make to this content will only apply during the current editor session.\n\n{0}"), FText::FromString( PackageNames ) );
			FMessageDialog::Open( EAppMsgType::Ok, Message );
			
			bNeedWarningForWritePermission = false;
		}
	}
}


void UUnrealEdEngine::PromptToCheckoutModifiedPackages( bool bPromptAll )
{
	TArray<UPackage*> PackagesToCheckout;
	if( bPromptAll )
	{
		for( TMap<TWeakObjectPtr<UPackage>,uint8>::TIterator It(PackageToNotifyState); It; ++It )
		{
			if( It.Key().IsValid() )
			{
				PackagesToCheckout.Add( It.Key().Get() );
			}
		}
	}
	else
	{
		for( TMap<TWeakObjectPtr<UPackage>,uint8>::TIterator It(PackageToNotifyState); It; ++It )
		{
			if( It.Key().IsValid() && (It.Value() == NS_PendingWarning || It.Value() == NS_PendingPrompt) )
			{
				PackagesToCheckout.Add( It.Key().Get() );
				It.Value() = NS_DialogPrompted;
			}
		}
	}

	const bool bCheckDirty = true;
	const bool bPromptingAfterModify = true;
	FEditorFileUtils::PromptToCheckoutPackages( bCheckDirty, PackagesToCheckout, NULL, NULL, bPromptingAfterModify );
}

int32 UUnrealEdEngine::InternalGetNumDirtyPackagesThatNeedCheckout(bool bCheckIfAny) const
{
	int32 PackageCount = 0;
		
	if (ISourceControlModule::Get().IsEnabled())
	{
		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
		for (TMap<TWeakObjectPtr<UPackage>, uint8>::TConstIterator It(PackageToNotifyState); It; ++It)
		{
			const UPackage* Package = It.Key().Get();
			if (Package != NULL)
			{
				FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(Package, EStateCacheUsage::Use);
				if (SourceControlState.IsValid() && (SourceControlState->CanCheckout() || !SourceControlState->IsCurrent() || SourceControlState->IsCheckedOutOther()))
				{
					++PackageCount;
					if (bCheckIfAny)
					{
						break;
					}
				}
			}
		}
	}

	return PackageCount;
}

int32 UUnrealEdEngine::GetNumDirtyPackagesThatNeedCheckout() const
{
	return InternalGetNumDirtyPackagesThatNeedCheckout(false);
}

bool UUnrealEdEngine::DoDirtyPackagesNeedCheckout() const
{
	return InternalGetNumDirtyPackagesThatNeedCheckout(true) > 0;
}

bool UUnrealEdEngine::Exec_Edit( UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar )
{
	const bool bComponentsSelected = GetSelectedComponentCount() > 0;

	if( FParse::Command(&Str,TEXT("CUT")) )
	{
		TArray<FEdMode*> ActiveModes;
		GLevelEditorModeTools().GetActiveModes(ActiveModes);
		for (int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex)
		{
			if (ActiveModes[ModeIndex]->ProcessEditCut())
			{
				return true;
			}
		}

		if (bComponentsSelected)
		{
			edactCopySelected(InWorld);
			edactDeleteSelected(InWorld);
		}
		else
		{
			CopySelectedActorsToClipboard(InWorld, true);
		}
	}
	else if( FParse::Command(&Str,TEXT("COPY")) )
	{
		TArray<FEdMode*> ActiveModes;
		GLevelEditorModeTools().GetActiveModes(ActiveModes);
		for (int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex)
		{
			if (ActiveModes[ModeIndex]->ProcessEditCopy())
			{
				return true;
			}
		}

		if (bComponentsSelected)
		{
			edactCopySelected(InWorld);
		}
		else
		{
			CopySelectedActorsToClipboard(InWorld, false);
		}
	}
	else if( FParse::Command(&Str,TEXT("PASTE")) )
	{
		TArray<FEdMode*> ActiveModes;
		GLevelEditorModeTools().GetActiveModes(ActiveModes);
		for (int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex)
		{
			if (ActiveModes[ModeIndex]->ProcessEditPaste())
			{
				return true;
			}
		}

		if (bComponentsSelected)
		{
			const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "PasteComponents", "Paste Components"));
			edactPasteSelected(InWorld, false, false, true);
		}
		else
		{
			// How should this paste be handled
			EPasteTo PasteTo = PT_OriginalLocation;
			FText TransDescription = NSLOCTEXT("UnrealEd", "Paste", "Paste");
			if (FParse::Value(Str, TEXT("TO="), TempStr, 15))
			{
				if (!FCString::Strcmp(TempStr, TEXT("HERE")))
				{
					PasteTo = PT_Here;
					TransDescription = NSLOCTEXT("UnrealEd", "PasteHere", "Paste Here");
				}
				else
				{
					if (!FCString::Strcmp(TempStr, TEXT("ORIGIN")))
					{
						PasteTo = PT_WorldOrigin;
						TransDescription = NSLOCTEXT("UnrealEd", "PasteToWorldOrigin", "Paste To World Origin");
					}
				}
			}

			PasteSelectedActorsFromClipboard(InWorld, TransDescription, PasteTo);
		}
	}

	return false;
}

bool UUnrealEdEngine::Exec_Pivot( const TCHAR* Str, FOutputDevice& Ar )
{
	if( FParse::Command(&Str,TEXT("HERE")) )
	{
		NoteActorMovement();
		SetPivot( ClickLocation, false, false );
		FinishAllSnaps();
		SetPivotMovedIndependently(true);
		RedrawLevelEditingViewports();
	}
	else if( FParse::Command(&Str,TEXT("SNAPPED")) )
	{
		NoteActorMovement();
		SetPivot( ClickLocation, true, false );
		FinishAllSnaps();
		SetPivotMovedIndependently(true);
		RedrawLevelEditingViewports();
	}
	else if( FParse::Command(&Str,TEXT("CENTERSELECTION")) )
	{
		NoteActorMovement();

		// Figure out the center location of all selections

		int32 Count = 0;
		FVector Center(0,0,0);

		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = CastChecked<AActor>(*It);

			if (ABrush* Brush = Cast<ABrush>(Actor))
			{
				// Treat brushes as a special case; calculate an effective position from the center point of the vertices.
				// This way, "Center on Selection" has a special meaning for brushes.
				TSet<FVector> UniqueVertices;
				FVector VertexCenter = FVector::ZeroVector;

				if (Brush->Brush && Brush->Brush->Polys)
				{
					for (const auto& Element : Brush->Brush->Polys->Element)
					{
						for (const auto& Vertex : Element.Vertices)
						{
							UniqueVertices.Add(Vertex);
						}
					}

					for (const auto& Vertex : UniqueVertices)
					{
						VertexCenter += Vertex;
					}

					if (UniqueVertices.Num() > 0)
					{
						VertexCenter /= UniqueVertices.Num();
					}
				}

				Center += Brush->GetTransform().TransformPosition(VertexCenter);
			}
			else
			{
				Center += Actor->GetActorLocation();
			}

			Count++;
		}

		if( Count > 0 )
		{
			FVector CenterLocation = Center / Count;
			UnsnappedClickLocation = CenterLocation;
			ClickLocation = CenterLocation;
			ClickPlane = FPlane(0.f,0.f,0.f,0.f);

			SetPivot( ClickLocation, false, false );
			FinishAllSnaps();
			SetPivotMovedIndependently(true);
		}

		RedrawLevelEditingViewports();
	}

	return false;
}

static void MirrorActors(const FVector& MirrorScale)
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "MirroringActors", "Mirroring Actors") );

	// Fires ULevel::LevelDirtiedEvent when falling out of scope.
	FScopedLevelDirtied		LevelDirtyCallback;

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		const FVector PivotLocation = GLevelEditorModeTools().PivotLocation;

		Actor->Modify();
		Actor->EditorApplyMirror( MirrorScale, PivotLocation );

		ABrush* Brush = Cast< ABrush >(Actor);
		if (Brush && Brush->GetBrushComponent())
		{
			Brush->GetBrushComponent()->RequestUpdateBrushCollision();
		}

		Actor->InvalidateLightingCache();
		Actor->PostEditMove( true );

		Actor->MarkPackageDirty();
		LevelDirtyCallback.Request();
	}

	if ( GLevelEditorModeTools().IsModeActive( FBuiltinEditorModes::EM_Geometry ) )
	{
		// If we are in geometry mode, make sure to update the mode with new source data for selected brushes
		FEdModeGeometry* Mode = (FEdModeGeometry*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_Geometry );
		Mode->GetFromSource();
	}

	GEditor->RedrawLevelEditingViewports();
}


/**
* Gathers up a list of selection FPolys from selected static meshes.
*
* @return	A TArray containing FPolys representing the triangles in the selected static meshes (note that these
*           triangles are transformed into world space before being added to the array.
*/

TArray<FPoly*> GetSelectedPolygons()
{
	// Build a list of polygons from all selected static meshes

	TArray<FPoly*> SelectedPolys;

	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );
		FTransform ActorToWorld = Actor->ActorToWorld();
		
		TInlineComponentArray<UStaticMeshComponent*> StaticMeshComponents;
		Actor->GetComponents(StaticMeshComponents);

		for(int32 j=0; j<StaticMeshComponents.Num(); j++)
		{
			// If its a static mesh component, with a static mesh
			UStaticMeshComponent* SMComp = StaticMeshComponents[j];
			if(SMComp->IsRegistered() && SMComp->GetStaticMesh())
			{
				UStaticMesh* StaticMesh = SMComp->GetStaticMesh();
				if ( StaticMesh )
				{
					int32 NumLods = StaticMesh->GetNumLODs();
					if ( NumLods )
					{
						const FStaticMeshLODResources& MeshLodZero = StaticMesh->GetLODForExport(0);
						int32 NumTriangles = MeshLodZero.GetNumTriangles();
						int32 NumVertices = MeshLodZero.GetNumVertices();
			
						const FPositionVertexBuffer& PositionVertexBuffer = MeshLodZero.PositionVertexBuffer;
						FIndexArrayView Indices = MeshLodZero.DepthOnlyIndexBuffer.GetArrayView();

						for ( int32 TriangleIndex = 0; TriangleIndex < NumTriangles; TriangleIndex++ )
						{
							const uint32 Idx0 = Indices[(TriangleIndex*3)+0];
							const uint32 Idx1 = Indices[(TriangleIndex*3)+1];
							const uint32 Idx2 = Indices[(TriangleIndex*3)+2];

							FPoly* Polygon = new FPoly;

							// Add the poly
							Polygon->Init();
							Polygon->PolyFlags = PF_DefaultFlags;

							new(Polygon->Vertices) FVector(ActorToWorld.TransformPosition( PositionVertexBuffer.VertexPosition(Idx2) ));
							new(Polygon->Vertices) FVector(ActorToWorld.TransformPosition( PositionVertexBuffer.VertexPosition(Idx1) ));
							new(Polygon->Vertices) FVector(ActorToWorld.TransformPosition( PositionVertexBuffer.VertexPosition(Idx0) ));

							Polygon->CalcNormal(1);
							Polygon->Fix();
							if( Polygon->Vertices.Num() > 2 )
							{
								if( !Polygon->Finalize( NULL, 1 ) )
								{
									SelectedPolys.Add( Polygon );
								}
							}

							// And add a flipped version of it to account for negative scaling
							Polygon = new FPoly;
							Polygon->Init();
							Polygon->PolyFlags = PF_DefaultFlags;

							new(Polygon->Vertices) FVector(ActorToWorld.TransformPosition( PositionVertexBuffer.VertexPosition(Idx2) ));
							new(Polygon->Vertices) FVector(ActorToWorld.TransformPosition( PositionVertexBuffer.VertexPosition(Idx0) ));
							new(Polygon->Vertices) FVector(ActorToWorld.TransformPosition( PositionVertexBuffer.VertexPosition(Idx1) ));
							Polygon->CalcNormal(1);
							Polygon->Fix();
							if( Polygon->Vertices.Num() > 2 )
							{
								if( !Polygon->Finalize( NULL, 1 ) )
								{
									SelectedPolys.Add( Polygon );
								}
							}
						}
					}
				}
			}
		}
	}

	return SelectedPolys;
}

/**
* Creates an axis aligned bounding box based on the bounds of SelectedPolys.  This bounding box
* is then copied into the builder brush.  This function is a set up function that the blocking volume
* creation execs will call before doing anything fancy.
*
* @param	InWorld				The world in which the builder brush needs to be created
* @param	SelectedPolys		The list of selected FPolys to create the bounding box from.
* @param	bSnapVertsToGrid	Should the brush verts snap to grid
*/

void CreateBoundingBoxBuilderBrush( UWorld* InWorld, const TArray<FPoly*> SelectedPolys, bool bSnapVertsToGrid )
{
	int x;
	FPoly* Poly;
	FBox BBox(ForceInit);
	FVector Vertex;

	for( x = 0 ; x < SelectedPolys.Num() ; ++x )
	{
		Poly = SelectedPolys[x];

		for( int v = 0 ; v < Poly->Vertices.Num() ; ++v )
		{
			if( bSnapVertsToGrid )
			{
				Vertex = Poly->Vertices[v].GridSnap(GEditor->GetGridSize());
			}
			else
			{
				Vertex = Poly->Vertices[v];
			}

			BBox += Vertex;
		}
	}

	// Change the builder brush to match the bounding box so that it exactly envelops the selected meshes
	{
		const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "BrushSet", "Brush Set"));

		UCubeBuilder* CubeBuilder = NewObject<UCubeBuilder>(GetTransientPackage(), NAME_None, RF_Transactional);
		FVector Extent = BBox.GetExtent();
		CubeBuilder->X = Extent.X * 2;
		CubeBuilder->Y = Extent.Y * 2;
		CubeBuilder->Z = Extent.Z * 2;
		CubeBuilder->Build(InWorld);

		ABrush* DefaultBrush = InWorld->GetDefaultBrush();
		check(DefaultBrush != nullptr);
		DefaultBrush->SetActorLocation(BBox.GetCenter(), false);
		DefaultBrush->ReregisterAllComponents();
	}
}

/**
* Take a plane and creates a gigantic triangle polygon that lies along it.  The blocking
* volume creation routines call this when they are cutting geometry and need to create
* capping polygons.
*
* This polygon is so huge that it doesn't matter where the vertices actually land.
*
* @param	InPlane		The plane to lay the polygon on
* @return	An FPoly representing the giant triangle we created (NULL if there was a problem)
*/

FPoly* CreateHugeTrianglePolygonOnPlane( const FPlane* InPlane )
{
	// Using the plane normal, get 2 good axis vectors

	FVector A, B;
	InPlane->GetSafeNormal().FindBestAxisVectors( A, B );

	// Create 4 vertices from the plane origin and the 2 axis generated above

	FPoly* Triangle = new FPoly();

	FVector Center = FVector( InPlane->X, InPlane->Y, InPlane->Z ) * InPlane->W;
	FVector V0 = Center + (A * WORLD_MAX);
	FVector V1 = Center + (B * WORLD_MAX);
	FVector V2 = Center - (((A + B) / 2.0f) * WORLD_MAX);

	// Create a triangle that lays on InPlane

	Triangle->Init();
	Triangle->PolyFlags = PF_DefaultFlags;

	new(Triangle->Vertices) FVector( V0 );
	new(Triangle->Vertices) FVector( V2 );
	new(Triangle->Vertices) FVector( V1 );

	Triangle->CalcNormal(1);
	Triangle->Fix();
	if( Triangle->Finalize( NULL, 1 ) )
	{
		delete Triangle;
		Triangle = NULL;
	}

	return Triangle;
}

bool UUnrealEdEngine::Exec_Actor( UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar )
{
	// Keep a pointer to the beginning of the string to use for message displaying purposes
	const TCHAR* const FullStr = Str;

	if( FParse::Command(&Str,TEXT("ADD")) )
	{
		UClass* Class;
		if( ParseObject<UClass>( Str, TEXT("CLASS="), Class, ANY_PACKAGE ) )
		{
			int32 bSnap = 1;
			FParse::Value(Str,TEXT("SNAP="),bSnap);
			
			AActor* Default = Class->GetDefaultObject<AActor>();
			const FTransform ActorTransform = FActorPositioning::GetCurrentViewportPlacementTransform(*Default, !!bSnap);

			AddActor( InWorld->GetCurrentLevel(), Class, ActorTransform );
			RedrawLevelEditingViewports();
			return true;
		}
	}
	else if( FParse::Command(&Str,TEXT("CREATE_BV_BOUNDINGBOX")) )
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "CreateBoundingBoxBlockingVolume", "Create Bounding Box Blocking Volume") );
		InWorld->GetDefaultBrush()->Modify();

		bool bSnapToGrid=0;
		FParse::Bool( Str, TEXT("SNAPTOGRID="), bSnapToGrid );

		// Create a bounding box for the selected static mesh triangles and set the builder brush to match it

		TArray<FPoly*> SelectedPolys = GetSelectedPolygons();
		CreateBoundingBoxBuilderBrush( InWorld, SelectedPolys, bSnapToGrid );

		// Create the blocking volume

		GUnrealEd->Exec( InWorld, TEXT("BRUSH ADDVOLUME CLASS=BlockingVolume") );

		// Clean up memory

		for( int x = 0 ; x < SelectedPolys.Num() ; ++x )
		{
			delete SelectedPolys[x];
		}

		SelectedPolys.Empty();

		// Finish up

		RedrawLevelEditingViewports();
		return true;
	}
	else if( FParse::Command(&Str,TEXT("CREATE_BV_CONVEXVOLUME")) )
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "CreateConvexBlockingVolume", "Create Convex Blocking Volume") );
		InWorld->GetDefaultBrush()->Modify();

		bool bSnapToGrid=0;
		FParse::Bool( Str, TEXT("SNAPTOGRID="), bSnapToGrid );

		// The rejection tolerance.  When figuring out which planes to cut the blocking volume cube with
		// the code will reject any planes that are less than "NormalTolerance" different in their normals.
		//
		// This cuts down on the number of planes that will be used for generating the cutting planes and,
		// as a side effect, eliminates duplicates.

		float NormalTolerance = 0.25f;
		FParse::Value( Str, TEXT("NORMALTOLERANCE="), NormalTolerance );

		FVector NormalLimits( 1.0f, 1.0f, 1.0f );
		FParse::Value( Str, TEXT("NLIMITX="), NormalLimits.X );
		FParse::Value( Str, TEXT("NLIMITY="), NormalLimits.Y );
		FParse::Value( Str, TEXT("NLIMITZ="), NormalLimits.Z );

		// Create a bounding box for the selected static mesh triangles and set the builder brush to match it

		TArray<FPoly*> SelectedPolys = GetSelectedPolygons();
		CreateBoundingBoxBuilderBrush( InWorld, SelectedPolys, bSnapToGrid );

		// Get a list of the polygons that make up the builder brush

		FPoly* poly;
		TArray<FPoly>* BuilderBrushPolys = new TArray<FPoly>( InWorld->GetDefaultBrush()->Brush->Polys->Element );

		// Create a list of valid splitting planes

		TArray<FPlane*> SplitterPlanes;

		for( int p = 0 ; p < SelectedPolys.Num() ; ++p )
		{
			// Get a splitting plane from the first poly in our selection

			poly = SelectedPolys[p];
			FPlane* SplittingPlane = new FPlane( poly->Vertices[0], poly->Normal );

			// Make sure this poly doesn't clip any other polys in the selection.  If it does, we can't use it for generating the convex volume.

			bool bUseThisSplitter = true;

			for( int pp = 0 ; pp < SelectedPolys.Num() && bUseThisSplitter ; ++pp )
			{
				FPoly* ppoly = SelectedPolys[pp];

				if( p != pp && !(poly->Normal - ppoly->Normal).IsNearlyZero() )
				{
					int res = ppoly->SplitWithPlaneFast( *SplittingPlane, NULL, NULL );

					if( res == SP_Split || res == SP_Front )
					{
						// Whoops, this plane clips polygons (and/or sits between static meshes) in the selection so it can't be used
						bUseThisSplitter = false;
					}
				}
			}

			// If this polygons plane doesn't clip the selection in any way, we can carve the builder brush with it. Save it.

			if( bUseThisSplitter )
			{
				// Move the plane into the same coordinate space as the builder brush

				*SplittingPlane = SplittingPlane->TransformBy(InWorld->GetDefaultBrush()->ActorToWorld().ToMatrixWithScale().InverseFast());

				// Before keeping this plane, make sure there aren't any existing planes that have a normal within the rejection tolerance.

				bool bAddPlaneToList = true;

				for( int x = 0 ; x < SplitterPlanes.Num() ; ++x )
				{
					FPlane* plane = SplitterPlanes[x];

					if( plane->GetSafeNormal().Equals( SplittingPlane->GetSafeNormal(), NormalTolerance ) )
					{
						bAddPlaneToList = false;
						break;
					}
				}

				// As a final test, make sure that this planes normal falls within the normal limits that were defined

				if( FMath::Abs( SplittingPlane->GetSafeNormal().X ) > NormalLimits.X )
				{
					bAddPlaneToList = false;
				}
				if( FMath::Abs( SplittingPlane->GetSafeNormal().Y ) > NormalLimits.Y )
				{
					bAddPlaneToList = false;
				}
				if( FMath::Abs( SplittingPlane->GetSafeNormal().Z ) > NormalLimits.Z )
				{
					bAddPlaneToList = false;
				}

				// If this plane passed every test - it's a keeper!

				if( bAddPlaneToList )
				{
					SplitterPlanes.Add( SplittingPlane );
				}
				else
				{
					delete SplittingPlane;
				}
			}
		}

		// The builder brush is a bounding box at this point that fully surrounds the selected static meshes.
		// Now we will carve away at it using the splitting planes we collected earlier.  When this process
		// is complete, we will have a convex volume inside of the builder brush that can then be used to add
		// a blocking volume.

		TArray<FPoly> NewBuilderBrushPolys;

		for( int sp = 0 ; sp < SplitterPlanes.Num() ; ++sp )
		{
			FPlane* plane = SplitterPlanes[sp];

			// Carve the builder brush with each splitting plane we collected.  We place the results into
			// NewBuilderBrushPolys since we don't want to overwrite the original array just yet.

			bool bNeedCapPoly = false;

			for( int bp = 0 ; bp < BuilderBrushPolys->Num() ; ++bp )
			{
				poly = &(*BuilderBrushPolys)[bp];

				FPoly Front, Back;
				int res = poly->SplitWithPlane( FVector( plane->X, plane->Y, plane->Z ) * plane->W, plane->GetSafeNormal(), &Front, &Back, true );
				switch( res )
				{
					// Ignore these results.  We don't want them.
					case SP_Coplanar:
					case SP_Front:
						break;

					// In the case of a split, keep the polygon on the back side of the plane.
					case SP_Split:
					{
						NewBuilderBrushPolys.Add( Back );
						bNeedCapPoly = true;
					}
					break;

					// By default, just keep the polygon that we had.
					default:
					{
						NewBuilderBrushPolys.Add( (*BuilderBrushPolys)[bp] );
					}
					break;
				}
			}

			// NewBuilderBrushPolys contains the newly clipped polygons so copy those into
			// the real array of polygons.

			BuilderBrushPolys = new TArray<FPoly>( NewBuilderBrushPolys );
			NewBuilderBrushPolys.Empty();

			// If any splitting occured, we need to generate a cap polygon to cover the hole.

			if( bNeedCapPoly )
			{
				// Create a large triangle polygon that covers the newly formed hole in the builder brush.

				FPoly* CappingPoly = CreateHugeTrianglePolygonOnPlane( plane );

				if( CappingPoly )
				{
					// Now we do the clipping the other way around.  We are going to use the polygons in the builder brush to
					// create planes which will clip the huge triangle polygon we just created.  When this process is over,
					// we will be left with a new polygon that covers the newly formed hole in the builder brush.

					for( int bp = 0 ; bp < BuilderBrushPolys->Num() ; ++bp )
					{
						poly = &((*BuilderBrushPolys)[bp]);
						plane = new FPlane( poly->Vertices[0], poly->Vertices[1], poly->Vertices[2] );

						FPoly Front, Back;
						int res = CappingPoly->SplitWithPlane( FVector( plane->X, plane->Y, plane->Z ) * plane->W, plane->GetSafeNormal(), &Front, &Back, true );
						switch( res )
						{
							case SP_Split:
							{
								*CappingPoly = Back;
							}
							break;
						}
					}

					// Add that new polygon into the builder brush polys as a capping polygon.

					BuilderBrushPolys->Add( *CappingPoly );
				}
			}
		}

		// Create a new builder brush from the freshly clipped polygons.

		InWorld->GetDefaultBrush()->Brush->Polys->Element.Empty();

		for( int x = 0 ; x < BuilderBrushPolys->Num() ; ++x )
		{
			InWorld->GetDefaultBrush()->Brush->Polys->Element.Add((*BuilderBrushPolys)[x]);
		}

		InWorld->GetDefaultBrush()->ReregisterAllComponents();

		// Create the blocking volume

		GUnrealEd->Exec( InWorld, TEXT("BRUSH ADDVOLUME CLASS=BlockingVolume") );

		// Clean up memory

		for( int x = 0 ; x < SelectedPolys.Num() ; ++x )
		{
			delete SelectedPolys[x];
		}

		SelectedPolys.Empty();

		for( int x = 0 ; x < SplitterPlanes.Num() ; ++x )
		{
			delete SplitterPlanes[x];
		}

		SplitterPlanes.Empty();

		delete BuilderBrushPolys;

		// Finish up

		RedrawLevelEditingViewports();
		return true;
	}
	else if( FParse::Command(&Str,TEXT("MIRROR")) )
	{
		FVector MirrorScale( 1, 1, 1 );
		GetFVECTOR( Str, MirrorScale );
		// We can't have zeroes in the vector
		if( !MirrorScale.X )		MirrorScale.X = 1;
		if( !MirrorScale.Y )		MirrorScale.Y = 1;
		if( !MirrorScale.Z )		MirrorScale.Z = 1;

		const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "MirroringActors", "Mirroring Actors"));
		MirrorActors(MirrorScale);
		RebuildAlteredBSP(); // Update the Bsp of any levels containing a modified brush
		return true;
	}
	else if( FParse::Command(&Str,TEXT("DELTAMOVE")) )
	{
		const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "DeltaMoveActors", "Move Actors by Delta"));
		FVector DeltaMove = FVector::ZeroVector;
		GetFVECTOR( Str, DeltaMove );

		FEditorModeTools& Tools = GLevelEditorModeTools();
		Tools.SetPivotLocation( Tools.PivotLocation + DeltaMove, false );

		if (GCurrentLevelEditingViewportClient)
		{
			GCurrentLevelEditingViewportClient->ApplyDeltaToActors(DeltaMove, FRotator::ZeroRotator, FVector::ZeroVector);
		}
		RedrawLevelEditingViewports();

		return true;
	}
	else if( FParse::Command(&Str,TEXT("HIDE")) )
	{
		if( FParse::Command(&Str,TEXT("SELECTED")) ) // ACTOR HIDE SELECTED
		{
			if ( FParse::Command(&Str,TEXT("STARTUP")) ) // ACTOR HIDE SELECTED STARTUP
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "HideSelectedAtStartup", "Hide Selected at Editor Startup") );
				edactHideSelectedStartup( InWorld );
				return true;
			}
			else
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "HideSelected", "Hide Selected") );
				edactHideSelected( InWorld );
				SelectNone( true, true );
				return true;
			}
		}
		else if( FParse::Command(&Str,TEXT("UNSELECTED")) ) // ACTOR HIDE UNSELECTEED
		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "HideUnselected", "Hide Unselected") );
			edactHideUnselected( InWorld );
			SelectNone( true, true );
			return true;
		}
	}
	else if( FParse::Command(&Str,TEXT("UNHIDE")) ) 
	{
		if ( FParse::Command(&Str,TEXT("ALL")) ) // ACTOR UNHIDE ALL
		{
			if ( FParse::Command(&Str,TEXT("STARTUP")) ) // ACTOR UNHIDE ALL STARTUP
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ShowAllAtStartup", "Show All at Editor Startup") );
				edactUnHideAllStartup( InWorld );
				return true;
			}
			else
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "UnHideAll", "UnHide All") );
				edactUnHideAll( InWorld );
				return true;
			}
		}
		else if( FParse::Command(&Str,TEXT("SELECTED")) )	// ACTOR UNHIDE SELECTED
		{
			if ( FParse::Command(&Str,TEXT("STARTUP")) ) // ACTOR UNHIDE SELECTED STARTUP
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ShowSelectedAtStartup", "Show Selected at Editor Startup") );
				edactUnHideSelectedStartup( InWorld );
				return true;
			}
			else
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "UnhideSelected", "Unhide Selected") );
				edactUnhideSelected( InWorld );
				return true;
			}
		}
	}
	else if( FParse::Command(&Str, TEXT("APPLYTRANSFORM")) )
	{
		CommandIsDeprecated( TEXT("ACTOR APPLYTRANSFORM"), Ar );
	}
	else if( FParse::Command(&Str, TEXT("REPLACE")) )
	{
		UClass* Class;
		if( FParse::Command(&Str, TEXT("BRUSH")) ) // ACTOR REPLACE BRUSH
		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ReplaceSelectedBrushActors", "Replace Selected Brush Actors") );
			edactReplaceSelectedBrush( InWorld );
			return true;
		}
		else if( ParseObject<UClass>( Str, TEXT("CLASS="), Class, ANY_PACKAGE ) ) // ACTOR REPLACE CLASS=<class>
		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ReplaceSelectedNonBrushActors", "Replace Selected Non-Brush Actors") );
			edactReplaceSelectedNonBrushWithClass( Class );
			return true;
		}
	}
	//@todo locked levels - handle the rest of these....is this required, or can we assume that actors in locked levels can't be selected
	else if( FParse::Command(&Str,TEXT("SELECT")) )
	{
		if( FParse::Command(&Str,TEXT("NONE")) ) // ACTOR SELECT NONE
		{
			return Exec( InWorld, TEXT("SELECT NONE") );
		}
		else if( FParse::Command(&Str,TEXT("ALL")) ) // ACTOR SELECT ALL
		{
			if(FParse::Command(&Str, TEXT("FROMOBJ"))) // ACTOR SELECT ALL FROMOBJ
			{		
				bool bHasStaticMeshes = false;
				TArray<UClass*> ClassesToSelect;

				for(FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
				{
					AActor* Actor = static_cast<AActor*>(*It);
					checkSlow(Actor->IsA(AActor::StaticClass()));

					if( Actor->IsA(AStaticMeshActor::StaticClass()) )
					{
						bHasStaticMeshes = true;
					}
					else
					{
						ClassesToSelect.AddUnique(Actor->GetClass());
					}
				}

				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectAll", "Select All") );
				if(bHasStaticMeshes)
				{
					edactSelectMatchingStaticMesh(false);
				}

				if(ClassesToSelect.Num() > 0)
				{
					for(int Index = 0; Index < ClassesToSelect.Num(); ++Index)
					{
						edactSelectOfClass( InWorld, ClassesToSelect[Index]);
					}
				}

				return true;
			}
			else
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectAll", "Select All") );
				edactSelectAll( InWorld );
				return true;
			}
		}
		else if( FParse::Command(&Str,TEXT("INSIDE") ) ) // ACTOR SELECT INSIDE
		{
			CommandIsDeprecated( TEXT("ACTOR SELECT INSIDE"), Ar );
		}
		else if( FParse::Command(&Str,TEXT("INVERT") ) ) // ACTOR SELECT INVERT
		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectInvert", "Select Invert") );
			edactSelectInvert( InWorld );
			return true;
		}
		else if( FParse::Command(&Str,TEXT("OFCLASS")) ) // ACTOR SELECT OFCLASS CLASS=<class>
		{
			UClass* Class;
			if( ParseObject<UClass>(Str,TEXT("CLASS="),Class,ANY_PACKAGE) )
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectOfClass", "Select Of Class") );
				edactSelectOfClass( InWorld, Class );
			}
			else
			{
				UE_SUPPRESS(LogExec, Warning, Ar.Log(TEXT("Missing class") ));
			}
			return true;
		}
		else if( FParse::Command(&Str,TEXT("OFSUBCLASS")) ) // ACTOR SELECT OFSUBCLASS CLASS=<class>
		{
			UClass* Class;
			if( ParseObject<UClass>(Str,TEXT("CLASS="),Class,ANY_PACKAGE) )
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectSubclassOfClass", "Select Subclass Of Class") );
				edactSelectSubclassOf( InWorld, Class );
			}
			else
			{
				UE_SUPPRESS(LogExec, Warning, Ar.Log(TEXT("Missing class") ));
			}
			return true;
		}
		else if( FParse::Command(&Str,TEXT("BASED")) ) // ACTOR SELECT BASED
		{
			// @TODO UE4 - no longer meaningful
			return true;
		}
		else if( FParse::Command(&Str,TEXT("BYPROPERTY")) ) // ACTOR SELECT BYPROPERTY
		{
			GEditor->SelectByPropertyColoration(InWorld);
			return true;
		}
		else if( FParse::Command(&Str,TEXT("DELETED")) ) // ACTOR SELECT DELETED
		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectDeleted", "Select Deleted") );
			edactSelectDeleted( InWorld );
			return true;
		}
		else if( FParse::Command(&Str,TEXT("MATCHINGSTATICMESH")) ) // ACTOR SELECT MATCHINGSTATICMESH
		{
			const bool bAllClasses = FParse::Command( &Str, TEXT("ALLCLASSES") );
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectMatchingStaticMesh", "Select Matching Static Mesh") );
			edactSelectMatchingStaticMesh( bAllClasses );
			return true;
		}
		else if( FParse::Command(&Str,TEXT("MATCHINGSKELETALMESH")) ) // ACTOR SELECT MATCHINGSKELETALMESH
		{
			const bool bAllClasses = FParse::Command( &Str, TEXT("ALLCLASSES") );
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectMatchingSkeletalMesh", "Select Matching Skeletal Mesh") );
			edactSelectMatchingSkeletalMesh( bAllClasses );
			return true;
		}
		else if( FParse::Command(&Str,TEXT("MATCHINGMATERIAL")) )
		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectAllWithMatchingMaterial", "Select All With Matching Material") );
			edactSelectMatchingMaterial();
			return true;
		}
		else if( FParse::Command(&Str,TEXT("MATCHINGEMITTER")) )
		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectMatchingEmitter", "Select Matching Emitters") );
			edactSelectMatchingEmitter();
			return true;
		}
		else if (FParse::Command(&Str, TEXT("RELEVANTLIGHTS")))	// ACTOR SELECT RELEVANTLIGHTS
		{
			UE_LOG(LogUnrealEdSrv, Log, TEXT("Select relevant lights!"));
			edactSelectRelevantLights( InWorld );
		}
		else
		{
			// Get actor name.
			FName ActorName(NAME_None);
			if ( FParse::Value( Str, TEXT("NAME="), ActorName ) )
			{
				AActor* Actor = FindObject<AActor>( InWorld->GetCurrentLevel(), *ActorName.ToString() );
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectToggleSingleActor", "Select Toggle Single Actor") );
				SelectActor( Actor, !(Actor && Actor->IsSelected()), false, true );
			}
			return true;
		}
	}
	else if( FParse::Command(&Str,TEXT("DELETE")) )		// ACTOR SELECT DELETE
	{
		bool bHandled = false;
		TArray<FEdMode*> ActiveModes; 
		GLevelEditorModeTools().GetActiveModes( ActiveModes );
		for( int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
		{
			bHandled |= ActiveModes[ModeIndex]->ProcessEditDelete();
		}

		// if not specially handled by the current editing mode,
		if (!bHandled)
		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "DeleteActors", "Delete Actors") );
			edactDeleteSelected( InWorld );
		}
		return true;
	}
	else if( FParse::Command(&Str,TEXT("UPDATE")) )		// ACTOR SELECT UPDATE
	{
		bool bLockedLevel = false;
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if ( !Actor->IsTemplate() && FLevelUtils::IsLevelLocked(Actor) )
			{
				bLockedLevel = true;
			}
			else
			{
				Actor->PreEditChange(NULL);
				Actor->PostEditChange();
			}
		}

		if ( bLockedLevel )
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_OperationDisallowedOnLockedLevelUpdateActor", "Update Actor: The requested operation could not be completed because the level is locked.") );
		}
		return true;
	}
	else if( FParse::Command(&Str,TEXT("SET")) )
	{
		// @todo DB: deprecate the ACTOR SET exec.
		RedrawLevelEditingViewports();
		return true;
	}
	else if( FParse::Command(&Str,TEXT("BAKEPREPIVOT")) )
	{
		FScopedLevelDirtied				LevelDirtyCallback;
		FScopedActorPropertiesChange	ActorPropertiesChangeCallback;

		// Bakes the current pivot position into all selected actors

		FEditorModeTools& EditorModeTools = GLevelEditorModeTools();

		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			FVector Delta( EditorModeTools.PivotLocation - Actor->GetActorLocation() );

			Actor->Modify();
			Actor->SetPivotOffset(Actor->GetTransform().InverseTransformVector(Delta));
			SetPivotMovedIndependently(false);
			Actor->PostEditMove(true);
		}

		GUnrealEd->NoteSelectionChange();
	} 
	else if( FParse::Command(&Str,TEXT("UNBAKEPREPIVOT")) )
	{
		FScopedLevelDirtied		LevelDirtyCallback;
		FScopedActorPropertiesChange	ActorPropertiesChangeCallback;

		// Resets the PrePivot of the selected actors to 0,0,0 while leaving them in the same world location.

		FEditorModeTools& EditorModeTools = GLevelEditorModeTools();

		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			Actor->Modify();
			Actor->SetPivotOffset(FVector::ZeroVector);
			SetPivotMovedIndependently(false);
			Actor->PostEditMove(true);
		}

		GUnrealEd->NoteSelectionChange();
	}
	else if( FParse::Command(&Str,TEXT("RESET")) )
	{
		FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ResetActors", "Reset Actors") );

		bool bLocation=false;
		bool bPivot=false;
		bool bRotation=false;
		bool bScale=false;
		if( FParse::Command(&Str,TEXT("LOCATION")) )
		{
			bLocation=true;
			ResetPivot();
		}
		else if( FParse::Command(&Str, TEXT("PIVOT")) )
		{
			bPivot=true;
			ResetPivot();
		}
		else if( FParse::Command(&Str,TEXT("ROTATION")) )
		{
			bRotation=true;
		}
		else if( FParse::Command(&Str,TEXT("SCALE")) )
		{
			bScale=true;
		}
		else if( FParse::Command(&Str,TEXT("ALL")) )
		{
			bLocation=bRotation=bScale=true;
			ResetPivot();
		}

		// Fires ULevel::LevelDirtiedEvent when falling out of scope.
		FScopedLevelDirtied		LevelDirtyCallback;

		bool bHadLockedLevels = false;
		bool bModifiedActors = false;
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if ( !Actor->IsTemplate() && FLevelUtils::IsLevelLocked(Actor) )
			{
				bHadLockedLevels = true;
			}
			else
			{
				bModifiedActors = true;

				Actor->PreEditChange(NULL);
				Actor->Modify();

				if( bLocation ) 
				{
					Actor->SetActorLocation(FVector::ZeroVector, false);
				}
				if( bPivot )
				{
					Actor->SetPivotOffset(FVector::ZeroVector);
				}

				if( bScale && Actor->GetRootComponent() != NULL ) 
				{
					Actor->GetRootComponent()->SetRelativeScale3D( FVector(1.f) );
				}

				Actor->MarkPackageDirty();
				LevelDirtyCallback.Request();
			}
		}

		if ( bHadLockedLevels )
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_OperationDisallowedOnLockedLevelResetActor", "Reset Actor: The requested operation could not be completed because the level is locked.") );
		}

		if ( bModifiedActors )
		{
			RedrawLevelEditingViewports();
		}
		else
		{
			Transaction.Cancel();
		}
		return true;
	}
	else if( FParse::Command(&Str,TEXT("DUPLICATE")) )
	{
		bool bHandled = false;
		TArray<FEdMode*> ActiveModes; 
		GLevelEditorModeTools().GetActiveModes( ActiveModes );
		for( int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
		{
			bHandled |= ActiveModes[ModeIndex]->ProcessEditDuplicate();
		}

		// if not specially handled by the current editing mode,
		if (!bHandled)
		{
			//@todo locked levels - if all actor levels are locked, cancel the transaction
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "DuplicateActors", "Duplicate Actors") );

			// duplicate selected
			ABrush::SetSuppressBSPRegeneration(true);
			edactDuplicateSelected(InWorld->GetCurrentLevel(), GetDefault<ULevelEditorViewportSettings>()->GridEnabled);
			ABrush::SetSuppressBSPRegeneration(false);

			// Find out if any of the selected actors will change the BSP.
			// and only then rebuild BSP as this is expensive.
			const FSelectedActorInfo& SelectedActors = AssetSelectionUtils::GetSelectedActorInfo();
			if( SelectedActors.bHaveBrush )
			{
				RebuildAlteredBSP(); // Update the Bsp of any levels containing a modified brush
			}
		}
		RedrawLevelEditingViewports();
		return true;
	}
	else if( FParse::Command(&Str, TEXT("ALIGN")) )
	{
		if( FParse::Command(&Str,TEXT("ORIGIN")) )
		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "Undo_SnapBrushOrigin", "Snap Brush Origin") );
			edactAlignOrigin();
			RedrawLevelEditingViewports();
			return true;
		}
		else // "VERTS" (default)
		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "Undo_SnapBrushVertices", "Snap Brush Vertices") );
			edactAlignVertices();
			RedrawLevelEditingViewports();
			RebuildAlteredBSP(); // Update the Bsp of any levels containing a modified brush
			return true;
		}
	}
	else if( FParse::Command(&Str,TEXT("TOGGLE")) )
	{
		if( FParse::Command(&Str,TEXT("LOCKMOVEMENT")) )			// ACTOR TOGGLE LOCKMOVEMENT
		{
			ToggleSelectedActorMovementLock();
		}

		RedrawLevelEditingViewports();
		return true;
	}
	else if( FParse::Command(&Str,TEXT("LEVELCURRENT")) )
	{
		MakeSelectedActorsLevelCurrent();
		return true;
	}
	else if( FParse::Command(&Str,TEXT("MOVETOCURRENT")) )
	{
		UEditorLevelUtils::MoveSelectedActorsToLevel( InWorld->GetCurrentLevel() );
		return true;
	}
	else if(FParse::Command(&Str, TEXT("DESELECT")))
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "DeselectActors", "Deselect Actor(s)") );
		GEditor->GetSelectedActors()->Modify();

		//deselects everything in UnrealEd
		GUnrealEd->SelectNone(true, true);
		
		return true;
	}
	else if(FParse::Command(&Str, TEXT("EXPORT")))
	{
		if(FParse::Command(&Str, TEXT("FBX")))
		{
			TArray<FString> SaveFilenames;
			IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
			bool bSave = false;
			if ( DesktopPlatform )
			{
				void* ParentWindowWindowHandle = NULL;

				IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
				const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
				if ( MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid() )
				{
					ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
				}

				bSave = DesktopPlatform->SaveFileDialog(
					ParentWindowWindowHandle,
					NSLOCTEXT("UnrealEd", "StaticMeshEditor_ExportToPromptTitle", "Export to...").ToString(), 
					*FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT),
					TEXT(""), 
					TEXT("FBX document|*.fbx"),
					EFileDialogFlags::None,
					SaveFilenames
					);
			}

			// Show dialog and execute the export if the user did not cancel out
			if( bSave )
			{
				// Get the filename from dialog
				FString FileName = SaveFilenames[0];
				FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, FPaths::GetPath(FileName)); // Save path as default for next time.

				INodeNameAdapter NodeNameAdapter;
				UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
				//Show the fbx export dialog options
				bool ExportCancel = false;
				bool ExportAll = false;
				Exporter->FillExportOptions(false, true, FileName, ExportCancel, ExportAll);
				if (!ExportCancel)
				{
					Exporter->CreateDocument();
					for (FSelectionIterator It(GetSelectedActorIterator()); It; ++It)
					{
						AActor* Actor = static_cast<AActor*>(*It);
						if (Actor->IsA(AActor::StaticClass()))
						{
							if (Actor->IsA(AStaticMeshActor::StaticClass()))
							{
								Exporter->ExportStaticMesh(Actor, CastChecked<AStaticMeshActor>(Actor)->GetStaticMeshComponent(), NodeNameAdapter);
							}
							else if (Actor->IsA(ASkeletalMeshActor::StaticClass()))
							{
								Exporter->ExportSkeletalMesh(Actor, CastChecked<ASkeletalMeshActor>(Actor)->GetSkeletalMeshComponent(), NodeNameAdapter);
							}
							else if (Actor->IsA(ABrush::StaticClass()))
							{
								Exporter->ExportBrush(CastChecked<ABrush>(Actor), NULL, true, NodeNameAdapter);
							}
						}
					}
					Exporter->WriteToFile(*FileName);
				}
			}
			return true;
		}

	}
	else if ( FParse::Command(&Str, TEXT("SNAP"))) // ACTOR SNAP
	{
		FSnappingUtils::EnableActorSnap( !FSnappingUtils::IsSnapToActorEnabled() );
		return true;
	}

	return false;
}


bool UUnrealEdEngine::Exec_Mode( const TCHAR* Str, FOutputDevice& Ar )
{
	int32 DWord1;

	if( FParse::Command(&Str, TEXT("WIDGETCOORDSYSTEMCYCLE")) )
	{
		const bool bGetRawValue = true;
		int32 Wk = GLevelEditorModeTools().GetCoordSystem(bGetRawValue);
		Wk++;

		if( Wk == COORD_Max )
		{
			Wk -= COORD_Max;
		}

		GLevelEditorModeTools().SetCoordSystem((ECoordSystem)Wk);
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
		FEditorSupportDelegates::UpdateUI.Broadcast();
	}

	if( FParse::Command(&Str, TEXT("WIDGETMODECYCLE")) )
	{
		GLevelEditorModeTools().CycleWidgetMode();
	}

	if( FParse::Value(Str, TEXT("GRID="), DWord1) )
	{
		FinishAllSnaps();

		ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
		ViewportSettings->GridEnabled = DWord1;
		ViewportSettings->PostEditChange();

		FEditorDelegates::OnGridSnappingChanged.Broadcast(ViewportSettings->GridEnabled, GetGridSize());
		FEditorSupportDelegates::UpdateUI.Broadcast();
	}

	if( FParse::Value(Str, TEXT("ROTGRID="), DWord1) )
	{
		FinishAllSnaps();

		ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
		ViewportSettings->RotGridEnabled = DWord1;
		ViewportSettings->PostEditChange();

		FEditorSupportDelegates::UpdateUI.Broadcast();
	}

	if( FParse::Value(Str, TEXT("SCALEGRID="), DWord1) )
	{
		FinishAllSnaps();

		ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
		ViewportSettings->SnapScaleEnabled = DWord1;
		ViewportSettings->PostEditChange();

		FEditorSupportDelegates::UpdateUI.Broadcast();
	}

	if( FParse::Value(Str, TEXT("SNAPVERTEX="), DWord1) )
	{
		FinishAllSnaps();

		ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
		ViewportSettings->bSnapVertices = !!DWord1;
		ViewportSettings->PostEditChange();

		FEditorSupportDelegates::UpdateUI.Broadcast();
	}

	if( FParse::Value(Str, TEXT("SHOWBRUSHMARKERPOLYS="), DWord1) )
	{
		FinishAllSnaps();
		bShowBrushMarkerPolys = DWord1;
	}

	if( FParse::Value(Str, TEXT("SELECTIONLOCK="), DWord1) )
	{
		FinishAllSnaps();
		// If -1 is passed in, treat it as a toggle.  Otherwise, use the value as a literal assignment.
		if( DWord1 == -1 )
		{
			GEdSelectionLock=(GEdSelectionLock == 0) ? 1 : 0;
		}
		else
		{
			GEdSelectionLock=!!DWord1;
		}

		Word1 = MAX_uint16;
	}

	if( FParse::Value(Str,TEXT("USESIZINGBOX="), DWord1) )
	{
		FinishAllSnaps();
		// If -1 is passed in, treat it as a toggle.  Otherwise, use the value as a literal assignment.
		if( DWord1 == -1 )
			UseSizingBox=(UseSizingBox == 0) ? 1 : 0;
		else
			UseSizingBox=DWord1;
		Word1=MAX_uint16;
	}
	
	if(GCurrentLevelEditingViewportClient)
	{
		int32 NewCameraSpeed = 1;
		if ( FParse::Value( Str, TEXT("SPEED="), NewCameraSpeed ) )
		{
			NewCameraSpeed = FMath::Clamp<int32>(NewCameraSpeed, 1, FLevelEditorViewportClient::MaxCameraSpeeds);
			GetMutableDefault<ULevelEditorViewportSettings>()->CameraSpeed = NewCameraSpeed;
		}
	}

	FParse::Value( Str, TEXT("SNAPDIST="), GetMutableDefault<ULevelEditorViewportSettings>()->SnapDistance );
	
	//
	// Major modes:
	//
	FEditorModeID EditorMode = FBuiltinEditorModes::EM_None;

	if 		(FParse::Command(&Str,TEXT("CAMERAMOVE")))		{ EditorMode = FBuiltinEditorModes::EM_Default;		}
	else if	(FParse::Command(&Str,TEXT("GEOMETRY"))) 		{ EditorMode = FBuiltinEditorModes::EM_Geometry;	}
	else if	(FParse::Command(&Str,TEXT("TEXTURE"))) 		{ EditorMode = FBuiltinEditorModes::EM_Texture;		}
	else if (FParse::Command(&Str,TEXT("MESHPAINT")))		{ EditorMode = FBuiltinEditorModes::EM_MeshPaint;	}
	else if (FParse::Command(&Str,TEXT("LANDSCAPE")))		{ EditorMode = FBuiltinEditorModes::EM_Landscape;	}
	else if (FParse::Command(&Str,TEXT("FOLIAGE")))			{ EditorMode = FBuiltinEditorModes::EM_Foliage;		}

	if ( EditorMode == FBuiltinEditorModes::EM_None )
	{
		FString CommandToken = FParse::Token(Str, false);
		FEdMode* FoundMode = GLevelEditorModeTools().FindMode( FName( *CommandToken ) );

		if ( FoundMode != NULL )
		{
			EditorMode = FName( *CommandToken );
		}
	}

	if( EditorMode != FBuiltinEditorModes::EM_None )
	{
		FEditorDelegates::ChangeEditorMode.Broadcast(EditorMode);
	}

	// Reset the roll on all viewport cameras
	for(uint32 ViewportIndex = 0;ViewportIndex < (uint32)LevelViewportClients.Num();ViewportIndex++)
	{
		if(LevelViewportClients[ViewportIndex]->IsPerspective())
		{
			LevelViewportClients[ViewportIndex]->RemoveCameraRoll();
		}
	}

	FEditorSupportDelegates::RedrawAllViewports.Broadcast();

	return true;
}

bool UUnrealEdEngine::Exec_Group( const TCHAR* Str, FOutputDevice& Ar )
{
	if(UActorGroupingUtils::IsGroupingActive())
	{
		if( FParse::Command(&Str,TEXT("REGROUP")) )
		{
			UActorGroupingUtils::Get()->GroupSelected();
			return true;
		}
		else if ( FParse::Command(&Str,TEXT("UNGROUP")) )
		{
			UActorGroupingUtils::Get()->UngroupSelected();
			return true;
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
