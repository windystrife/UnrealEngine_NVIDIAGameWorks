// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Matinee.h"
#include "Engine/Texture2D.h"
#include "Widgets/Layout/SBorder.h"
#include "CanvasTypes.h"
#include "Engine/InterpCurveEdSetup.h"
#include "Misc/MessageDialog.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSplitter.h"
#include "CanvasItem.h"
#include "Matinee/MatineeAnimInterface.h"
#include "Editor/TransBuffer.h"
#include "Camera/CameraActor.h"
#include "Engine/Light.h"
#include "LevelEditorViewport.h"
#include "UObject/UObjectHash.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "EditorModeInterpolation.h"
#include "MatineeModule.h"

#include "Matinee/InterpFilter.h"
#include "Matinee/MatineeActor.h"
#include "Matinee/MatineeActorCameraAnim.h"
#include "Matinee/InterpGroupInst.h"
#include "Matinee/InterpTrackToggle.h"
#include "Matinee/InterpTrackSound.h"
#include "Matinee/InterpTrackDirector.h"
#include "Matinee/InterpTrackVisibility.h"
#include "Matinee/InterpTrackEvent.h"
#include "Matinee/InterpGroupDirector.h"

#include "MatineeOptions.h"
#include "MatineeTransBuffer.h"
#include "MatineeViewportClient.h"
#include "MatineeFilterButton.h"

#include "DistCurveEditorModule.h"


#include "CameraController.h"
#include "MatineeConstants.h"

#include "SubtitleManager.h"
#include "InterpolationHitProxy.h"

#include "LevelEditor.h"
#include "EditorSupportDelegates.h"

#include "Logging/MessageLog.h"
#include "IDetailsView.h"

#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"

#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/STextComboBox.h"
#include "Framework/Commands/GenericCommands.h"
#include "Camera/CameraAnim.h"

#include "MovieSceneCaptureDialogModule.h"

#include "LevelCapture.h"

DEFINE_LOG_CATEGORY(LogSlateMatinee);

#define LOCTEXT_NAMESPACE "Matinee"

const FColor FMatinee::ActiveCamColor = FColor::Yellow;
const FColor FMatinee::SelectedCurveColor = FColor::Yellow;
const int32	FMatinee::DuplicateKeyOffset = 10;
const int32	FMatinee::KeySnapPixels = 5;

const float FMatinee::InterpEditor_ZoomIncrement = 1.2f;

const FColor FMatinee::PositionMarkerLineColor = FColor(255, 222, 206);
const FColor FMatinee::LoopRegionFillColor = FColor(80,255,80,24);
const FColor FMatinee::Track3DSelectedColor = FColor::Yellow;

const float FMatinee::InterpEdSnapSizes[5] = { 0.01f, 0.05f, 0.1f, 0.5f, 1.0f };
const float FMatinee::InterpEdFPSSnapSizes[9] =
{
	1.0f / 15.0f,
	1.0f / 24.0f,
	1.0f / 25.0f,
	1.0f / ( 30.0f / 1.001f ),	// 1.0f / 29.97...
	1.0f / 30.0f,
	1.0f / 50.0f,
	1.0f / ( 60.0f / 1.001f ),	// 1.0f / 59.94...
	1.0f / 60.0f,
	1.0f / 120.0f,
};


IMPLEMENT_HIT_PROXY(HMatineeTrackBkg,HHitProxy);
IMPLEMENT_HIT_PROXY(HMatineeGroupTitle,HHitProxy);
IMPLEMENT_HIT_PROXY(HMatineeGroupCollapseBtn,HHitProxy);
IMPLEMENT_HIT_PROXY(HMatineeTrackCollapseBtn,HHitProxy);
IMPLEMENT_HIT_PROXY(HMatineeGroupLockCamBtn,HHitProxy);
IMPLEMENT_HIT_PROXY(HMatineeTrackTitle,HHitProxy);
IMPLEMENT_HIT_PROXY(HMatineeSubGroupTitle,HHitProxy);
IMPLEMENT_HIT_PROXY(HMatineeTrackTimeline,HHitProxy);
IMPLEMENT_HIT_PROXY(HMatineeTrackTrajectoryButton,HHitProxy);
IMPLEMENT_HIT_PROXY(HMatineeTrackGraphPropBtn,HHitProxy);
IMPLEMENT_HIT_PROXY(HMatineeTrackDisableTrackBtn,HHitProxy);
IMPLEMENT_HIT_PROXY(HMatineeEventDirBtn,HHitProxy);
IMPLEMENT_HIT_PROXY(HMatineeTimelineBkg,HHitProxy);
IMPLEMENT_HIT_PROXY(HMatineeNavigatorBackground,HHitProxy);
IMPLEMENT_HIT_PROXY(HMatineeNavigator,HHitProxy);
IMPLEMENT_HIT_PROXY(HMatineeMarker,HHitProxy);

FName FMatinee::GetToolkitFName() const
{
	return FName("Matinee");
}

FText FMatinee::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Matinee");
}

FString FMatinee::GetWorldCentricTabPrefix() const
{
	return NSLOCTEXT("Matinee", "WorldCentricTabPrefix", "Matinee ").ToString();
}

FLinearColor FMatinee::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

static const FName MatineeRecordingViewportName("Matinee_RecordingViewport");
static const FName MatineeCurveEdName("Matinee_CurveEditor");
static const FName MatineeTrackWindowName("Matinee_TrackWindow");
static const FName MatineePropertyWindowName("Matinee_PropertyWindow");

void FMatinee::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_MatineeEditor", "Matinee"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(MatineeRecordingViewportName, FOnSpawnTab::CreateRaw(this, &FMatinee::SpawnRecordingViewport))
		.SetDisplayName(NSLOCTEXT("Matinee", "RecordingViewport", "Matinee Recorder"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Matinee.Tabs.RecordingViewport"));

	InTabManager->RegisterTabSpawner(MatineeCurveEdName, FOnSpawnTab::CreateSP(this, &FMatinee::SpawnTab, MatineeCurveEdName))
		.SetDisplayName(NSLOCTEXT("Matinee", "CurveEditorTitle", "Curve Editor"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Matinee.Tabs.CurveEditor"));

	InTabManager->RegisterTabSpawner(MatineeTrackWindowName, FOnSpawnTab::CreateSP(this, &FMatinee::SpawnTab, MatineeTrackWindowName))
		.SetDisplayName(NSLOCTEXT("Matinee", "TrackViewEditorTitle", "Tracks"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Matinee.Tabs.Tracks"));


	InTabManager->RegisterTabSpawner( MatineePropertyWindowName, FOnSpawnTab::CreateSP(this, &FMatinee::SpawnTab, MatineePropertyWindowName) )
		.SetDisplayName(NSLOCTEXT("Matinee", "PropertiesEditorTitle", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));

}

void FMatinee::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(MatineeCurveEdName);
	InTabManager->UnregisterTabSpawner(MatineeTrackWindowName);
	InTabManager->UnregisterTabSpawner(MatineePropertyWindowName);
}

TSharedRef<SDockTab> FMatinee::SpawnTab( const FSpawnTabArgs& TabSpawnArgs, FName TabIdentifier )
{
	if ( TabIdentifier == MatineeCurveEdName )
	{
		TSharedRef<SDockTab> NewCurveTab = SNew(SDockTab)
			.Label(NSLOCTEXT("Matinee", "CurveEditorTitle", "Curve Editor"))
			[
				CurveEd.ToSharedRef()
			];

		CurveEdTab = NewCurveTab;

		return NewCurveTab;
	}
	else if ( TabIdentifier == MatineeTrackWindowName )
	{
		TSharedRef<SDockTab> Tab = SNew(SDockTab)
			.Label(NSLOCTEXT("Matinee", "MatineeTrackEditorTitle", "Tracks"))
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)

				+ SSplitter::Slot()
				.Value(1.0f / 3.0f)
				[
					DirectorTrackWindow.ToSharedRef()
				]

				+ SSplitter::Slot()
				.Value(2.0f / 3.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0,0,0,2)
					[
						SAssignNew(GroupFilterContainer, SBorder)
						.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
						.Padding(2.0f)
						[
							BuildGroupFilterToolbar()
						]
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						TrackWindow.ToSharedRef()
					]
				]
			];
		DirectorTrackWindow->InterpEdVC->SetParentTab(Tab);
		TrackWindow->InterpEdVC->SetParentTab(Tab);
		return Tab;
	}
	else if (TabIdentifier == MatineePropertyWindowName)
	{
		return SNew(SDockTab)
			.Label(NSLOCTEXT("Matinee", "PropertiesEditorTitle", "Details"))
			[
				PropertyWindow.ToSharedRef()
			];
	}
	else
	{
		ensure(false);
		return SNew(SDockTab);
	}
}

void FMatinee::SetCurveTabVisibility(bool Visible)
{
	if ( CurveEdTab.IsValid() && !Visible )
	{
		TSharedPtr<SDockTab> PinnedCurveEdTab = CurveEdTab.Pin();
		PinnedCurveEdTab->RequestCloseTab();
	}
	else if ( !CurveEdTab.IsValid() && Visible )
	{
		TabManager->InvokeTab(MatineeCurveEdName);
	}
}

TSharedRef<SWidget> FMatinee::BuildGroupFilterToolbar()
{
	TSharedRef<SHorizontalBox> FilterList = SNew(SHorizontalBox);

	for ( int32 TabIdx=0; TabIdx < IData->DefaultFilters.Num(); TabIdx++ )
	{
		UInterpFilter* Filter = IData->DefaultFilters[TabIdx];
		FilterList->AddSlot()
		.AutoWidth()
		.Padding(2, 1)
		[
			AddFilterButton(Filter)
		];
	}

	// Draw user custom filters last.
	for ( int32 TabIdx=0; TabIdx < IData->InterpFilters.Num(); TabIdx++ )
	{
		UInterpFilter* Filter = IData->InterpFilters[TabIdx];
		FilterList->AddSlot()
		.AutoWidth()
		.Padding(2, 0)
		[
			AddFilterButton(Filter)
		];
	}

	return FilterList;
}

TSharedRef<SWidget> FMatinee::AddFilterButton(UInterpFilter* Filter)
{
	return SNew(SMatineeFilterButton)
		.Text(FText::FromString(Filter->Caption))
		.IsChecked(this, &FMatinee::GetFilterActive, Filter)
		.OnCheckStateChanged(this, &FMatinee::SetFilterActive, Filter)
		.OnContextMenuOpening(this, &FMatinee::CreateTabMenu);
}

void FMatinee::SetFilterActive(ECheckBoxState CheckStatus, UInterpFilter* Filter)
{
	if ( CheckStatus == ECheckBoxState::Checked )
	{
		SetSelectedFilter(Filter);
		InvalidateTrackWindowViewports();
	}
}

ECheckBoxState FMatinee::GetFilterActive(UInterpFilter* Filter) const
{
	if ( IData->SelectedFilter == Filter )
	{
		return ECheckBoxState::Checked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}

/*-----------------------------------------------------------------------------
 FMatinee
 -----------------------------------------------------------------------------*/

bool				FMatinee::bInterpTrackClassesInitialized = false;
TArray<UClass*>		FMatinee::InterpTrackClasses;

// On init, find all track classes. Will use later on to generate menus.
void FMatinee::InitInterpTrackClasses()
{
	if(bInterpTrackClassesInitialized)
	{
		return;
	}

	// Construct list of non-abstract gameplay sequence object classes.
	for(TObjectIterator<UClass> It; It; ++It)
	{
		if( It->IsChildOf(UInterpTrack::StaticClass()) && !(It->HasAnyClassFlags(CLASS_Abstract)) )
		{
			InterpTrackClasses.Add(*It);
		}
	}

	bInterpTrackClassesInitialized = true;
}

void FMatinee::SetAudioRealtimeOverride( bool bAudioIsRealtime ) const
{
	for(int32 i=0; i<GEditor->LevelViewportClients.Num(); i++)
	{
		FEditorViewportClient* const LevelVC = GEditor->LevelViewportClients[i];
		if (LevelVC)
		{
			if(LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview() )
			{
				LevelVC->SetForcedAudioRealtime(bAudioIsRealtime);
			}
		}
	}
}

void FMatinee::OnToggleAspectRatioBars()
{
	if (GCurrentLevelEditingViewportClient)
	{
		if(GCurrentLevelEditingViewportClient->IsPerspective() && GCurrentLevelEditingViewportClient->AllowsCinematicPreview() )
		{
			bool bEnabled = !AreAspectRatioBarsEnabled();
			GCurrentLevelEditingViewportClient->SetShowAspectRatioBarDisplay(bEnabled);

			GConfig->SetBool(TEXT("Matinee"), TEXT("AspectRatioBars"), bEnabled, GEditorPerProjectIni);
		}
	}
}

void FMatinee::OnToggleSafeFrames()
{
	if (GCurrentLevelEditingViewportClient)
	{
		if(GCurrentLevelEditingViewportClient->IsPerspective() && GCurrentLevelEditingViewportClient->AllowsCinematicPreview() )
		{
			bool bEnabled = !IsSafeFrameDisplayEnabled();
			GCurrentLevelEditingViewportClient->SetShowSafeFrameBoxDisplay(bEnabled);

			GConfig->SetBool(TEXT("Matinee"), TEXT("SafeFrames"), bEnabled, GEditorPerProjectIni);
		}
	}
}

bool FMatinee::AreAspectRatioBarsEnabled() const
{
	bool bEnabled = false;
	if ( !GConfig->GetBool(TEXT("Matinee"), TEXT("AspectRatioBars"), bEnabled, GEditorPerProjectIni) )
	{
		// We enable them by default
		return true;
	}

	return bEnabled;
}

bool FMatinee::IsSafeFrameDisplayEnabled() const
{
	bool bEnabled = false;
	if ( !GConfig->GetBool(TEXT("Matinee"), TEXT("SafeFrames"), bEnabled, GEditorPerProjectIni) )
	{
		// We not enabled by default
		return false;
	}

	return bEnabled;
}

void FMatinee::BuildCurveEditor()
{
	if(!IData->CurveEdSetup)
	{
		IData->CurveEdSetup = NewObject<UInterpCurveEdSetup>(IData, NAME_None);
	}

	// Create graph editor to work on MatineeData's CurveEd setup.
	IDistributionCurveEditorModule* CurveEditorModule = &FModuleManager::LoadModuleChecked<IDistributionCurveEditorModule>( "DistCurveEditor" );
	IDistributionCurveEditor::FCurveEdOptions CurveEdOptions;
	CurveEdOptions.bAlwaysShowScrollbar = true;
	CurveEd = CurveEditorModule->CreateCurveEditorWidget(IData->CurveEdSetup, this, CurveEdOptions);

	// Set graph view to match track view.
	SyncCurveEdView();

	PosMarkerColor = PositionMarkerLineColor;
	RegionFillColor = LoopRegionFillColor;

	CurveEd->SetEndMarker(true, IData->InterpLength);
	CurveEd->SetPositionMarker(true, 0.f, PosMarkerColor);
	CurveEd->SetRegionMarker(true, IData->EdSectionStart, IData->EdSectionEnd, RegionFillColor);
};

/** Should NOT open an InterpEd unless InInterp has a valid MatineeData attached! */
void FMatinee::InitMatinee(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* InObjectToEdit)
{
	//initializers
	bClosed = false;
	bIsInitialized = false;
	bViewportFrameStatsEnabled = true;
	bEditingCrosshairEnabled = false;
	bEditingGridEnabled = false;
	bBakeTransforms = false;
	bAllowKeyframeBarSelection = false;
	bAllowKeyframeTextSelection = false;
	bLockCameraPitch = true;
	EditingGridSize = 0;
	RecordMenuSelection = MatineeConstants::ERecordMenu::RECORD_MENU_RECORD_MODE;
	bDisplayRecordingMenu = true;
	RecordingState = MatineeConstants::ERecordingState::RECORDING_COMPLETE;
	RecordMode = MatineeConstants::ERecordMode::RECORD_MODE_NEW_CAMERA;
	RecordRollSmoothingSamples = 5;
	RecordPitchSmoothingSamples = 5;
	RecordCameraMovementScheme = MatineeConstants::ECameraScheme::CAMERA_SCHEME_FREE_CAM;
	RecordingStateStartTime = 0;
	bUpdatingCameraGuard = false;

	FMatineeCommands::Register();
	BindCommands();

	// Make sure we have a list of available track classes
	InitInterpTrackClasses();

	// NOTE: This should match the curve editor's label width!
	LabelWidth = 200;

	// 3D tracks should be visible by default
	bHide3DTrackView = false;
	GConfig->GetBool( TEXT("Matinee"), TEXT("Hide3DTracks"), bHide3DTrackView, GEditorPerProjectIni );

	// Zoom to scrub position defaults to off.  We want zoom to cursor position by default.
	bZoomToScrubPos = false;
	GConfig->GetBool( TEXT("Matinee"), TEXT("ZoomToScrubPos"), bZoomToScrubPos, GEditorPerProjectIni );

	// Setup 'viewport frame stats' preference
	bViewportFrameStatsEnabled = true;
	GConfig->GetBool( TEXT("Matinee"), TEXT("ViewportFrameStats"), bViewportFrameStatsEnabled, GEditorPerProjectIni );

	// Get the editing grid size from user settings
	EditingGridSize = 1;
	GConfig->GetInt( TEXT("Matinee"), TEXT("EditingGridSize"), EditingGridSize, GEditorPerProjectIni );

	// Look to see if the crosshair should be enabled
	// Disabled by default
	bEditingCrosshairEnabled = false;
	GConfig->GetBool( TEXT("Matinee"), TEXT("EditingCrosshair"), bEditingCrosshairEnabled, GEditorPerProjectIni );

	// Look to see if the editing grid should be enabled
	bEditingGridEnabled = false;
	GConfig->GetBool( TEXT("Matinee"), TEXT("EnableEditingGrid"), bEditingGridEnabled, GEditorPerProjectIni );

	// Setup "allow keyframe bar selection" preference
	GConfig->GetBool( TEXT("Matinee"), TEXT("AllowKeyframeBarSelection"), bAllowKeyframeBarSelection, GEditorPerProjectIni ); 

	// Setup "allow keyframe text selection" preference
	GConfig->GetBool( TEXT("Matinee"), TEXT("AllowKeyframeTextSelection"), bAllowKeyframeTextSelection, GEditorPerProjectIni );

	bInvertPan = true;
	GConfig->GetBool( TEXT("Matinee"), TEXT("InterpEdPanInvert"), bInvertPan, GEditorPerProjectIni );

	// Setup "lock camera pitch" preference
	GetLockCameraPitchFromConfig();

	// Create options object.
	Opt = NewObject<UMatineeOptions>(GetTransientPackage(), NAME_None, RF_Transactional);
	check(Opt);

	// Swap out regular UTransactor for our special one
	GEditor->ResetTransaction( NSLOCTEXT("UnrealEd", "OpenMatinee", "Open UnrealMatinee") );

	NormalTransactor = GEditor->Trans;
	InterpEdTrans = NewObject<UMatineeTransBuffer>();
	InterpEdTrans->Initialize(8 * 1024 * 1024);
	InterpEdTrans->OnUndo().AddRaw(this, &FMatinee::OnPostUndoRedo);
	InterpEdTrans->OnRedo().AddRaw(this, &FMatinee::OnPostUndoRedo);
	GEditor->Trans = InterpEdTrans;

	// Save viewports' data before it gets overridden by UpdateLevelViewport
	SaveLevelViewports();

	// Set up pointers to interp objects
	MatineeActor = Cast<AMatineeActor>(InObjectToEdit);
	MatineeActor->EnsureActorGroupConsistency();

	// Do all group/track instancing and variable hook-up.
	MatineeActor->InitInterp();

	// Flag this action as 'being edited'
	MatineeActor->bIsBeingEdited = true;

	// Always start out with gore preview turned on in the editor!
	MatineeActor->bShouldShowGore = true;

	// Should always find some data.
	check(MatineeActor->MatineeData);
	IData = MatineeActor->MatineeData;

	// Repair any folder/group hierarchy problems in the data set
	RepairHierarchyProblems();

	PixelsPerSec = 1.f;
	TrackViewSizeX = 0;
	NavPixelsPerSecond = 1.0f;

	// Set initial zoom range
	ViewStartTime = 0.f;
	ViewEndTime = IData->InterpLength;

	bDrawSnappingLine = false;
	SnappingLinePosition = 0.f;
	UnsnappedMarkerPos = 0.f;

	// Set the default filter for the data
	if(IData->DefaultFilters.Num())
	{
		SetSelectedFilter(IData->DefaultFilters[0]);
	}
	else
	{
		SetSelectedFilter(NULL);
	}

	// Slight hack to ensure interpolation data is transactional.
	MatineeActor->SetFlags( RF_Transactional );
	IData->SetFlags( RF_Transactional );
	for(int32 i=0; i<IData->InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = IData->InterpGroups[i];
		Group->SetFlags( RF_Transactional );

		for(int32 j=0; j<Group->InterpTracks.Num(); j++)
		{
			Group->InterpTracks[j]->SetFlags( RF_Transactional );
		}
	}
	
	FMessageLog EditorErrors("EditorErrors");
	EditorErrors.NewPage(LOCTEXT("MatineeInitLogPageLabel", "Matinee Initialization"));

	// For each track let it save the state of the object its going to work on before being changed at all by Matinee.
	for(int32 i=0; i<MatineeActor->GroupInst.Num(); i++)
	{
		UInterpGroupInst* GrInst = MatineeActor->GroupInst[i];
		GrInst->SaveGroupActorState();

		AActor* GroupActor = GrInst->GetGroupActor();
		if ( GroupActor )
		{
			// Save this actor's transformations if we need to (along with its children)
			MatineeActor->ConditionallySaveActorState( GrInst, GroupActor );

			// Check for bStatic actors that have dynamic tracks associated with them and report a warning to the user
			if ( GroupActor->IsRootComponentStatic() )
			{
				bool bHasTrack = false;
				FString TrackNames;

				for(int32 TrackIdx=0; TrackIdx<GrInst->Group->InterpTracks.Num(); TrackIdx++)
				{
					if( GrInst->Group->InterpTracks[TrackIdx]->AllowStaticActors()==false )
					{
						bHasTrack = true;

						if( TrackNames.Len() > 0 )
						{
							TrackNames += ", ";
						}
						TrackNames += GrInst->Group->InterpTracks[TrackIdx]->GetClass()->GetDescription();
					}
				}

				if(bHasTrack)
				{
					// Warn if any groups with dynamic tracks are trying to act on bStatic actors!

					// Add to list of warnings of this type
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("TrackNames"), FText::FromString( TrackNames ));
					Arguments.Add(TEXT("GroupName"), FText::FromName( GrInst->Group->GroupName ));
					Arguments.Add(TEXT("ActorName"), FText::FromString( GroupActor->GetName() ));
					EditorErrors.Warning(FText::Format(LOCTEXT("GroupOnStaticActor_F", "Tracks [{TrackNames}] in Group {GroupName} require a dynamic actor but are instead acting on a Static Actor {ActorName} - this is probably incorrect!"), Arguments));
				}
			}


			// Check for toggle tracks bound to non-toggleable light sources
			ALight* LightActor = Cast< ALight>( GroupActor );
			if( LightActor != NULL && !LightActor->IsToggleable() )
			{
				bool bHasTrack = false;
				FString TrackNames;

				for( int32 TrackIdx = 0; TrackIdx < GrInst->Group->InterpTracks.Num(); ++TrackIdx )
				{
					if( GrInst->Group->InterpTracks[ TrackIdx ]->IsA( UInterpTrackToggle::StaticClass() ) )
					{
						bHasTrack = true;

						if( TrackNames.Len() > 0 )
						{
							TrackNames += ", ";
						}
						TrackNames += GrInst->Group->InterpTracks[ TrackIdx ]->GetClass()->GetDescription();
					}
				}

				if( bHasTrack )
				{
					// Warn if any groups with toggle tracks are trying to act on non-toggleable light sources!

					// Add to list of warnings of this type
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("TrackNames"), FText::FromString( TrackNames ));
					Arguments.Add(TEXT("GroupName"), FText::FromName( GrInst->Group->GroupName ));
					Arguments.Add(TEXT("LightActor"), FText::FromString( GroupActor->GetName() ));
					EditorErrors.Warning(FText::Format(LOCTEXT("InterpEd_ToggleTrackOnNonToggleableLight_F", "Toggle tracks [{TrackNames}] in Group {GroupName} are bound to a non-toggleable light actor {LightActor} - this light will be not be toggled by UnrealMatinee!  Consider changing the light to an appropriate toggleable light class."), Arguments));
				}
			}
		}
	}


	// Is "force start pos" enabled?  If so, check for some common problems with use of that
	if( MatineeActor->bForceStartPos && MatineeActor->ForceStartPosition > 0.0f )
	{
		for( int32 CurGroupIndex = 0; CurGroupIndex < MatineeActor->MatineeData->InterpGroups.Num(); ++CurGroupIndex )
		{
			const UInterpGroup* CurGroup = MatineeActor->MatineeData->InterpGroups[ CurGroupIndex ];

			for( int32 CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
			{
				const UInterpTrack* CurTrack = CurGroup->InterpTracks[ CurTrackIndex ];


				bool bNeedWarning = false;


				// @todo: Abstract these checks!  Should be accessor check in UInterpTrack!

				// @todo: These checks don't involve actors or group instances, so we should move them to
				//     the Map Check phase instead of Matinee startup!


				// Toggle tracks don't play nice with bForceStartPos since they currently cannot 'fast forward',
				// except in certain cases
				const UInterpTrackToggle* ToggleTrack = Cast<const  UInterpTrackToggle >( CurTrack );
				if( ToggleTrack != NULL )
				{
					for( int32 CurKeyIndex = 0; CurKeyIndex < ToggleTrack->ToggleTrack.Num(); ++CurKeyIndex )
					{
						const FToggleTrackKey& CurKey = ToggleTrack->ToggleTrack[ CurKeyIndex ];

						// Trigger events will be skipped entirely when jumping forward
						if( !ToggleTrack->bFireEventsWhenJumpingForwards ||
							CurKey.ToggleAction == ETTA_Trigger )
						{
							// Is this key's time within the range that we'll be skipping over due to the Force Start
							// Position being set to a later time, we'll warn the user about that!
							if( CurKey.Time < MatineeActor->ForceStartPosition )
							{
								// One warning per track is plenty!
								bNeedWarning = true;
								break;
							}
						}
					}
				}


				// Visibility tracks don't play nice with bForceStartPos since they currently cannot 'fast forward'
				const UInterpTrackVisibility* VisibilityTrack = Cast<const  UInterpTrackVisibility >( CurTrack );
				if( VisibilityTrack != NULL && !VisibilityTrack->bFireEventsWhenJumpingForwards )
				{
					for( int32 CurKeyIndex = 0; CurKeyIndex < VisibilityTrack->VisibilityTrack.Num(); ++CurKeyIndex )
					{
						const FVisibilityTrackKey& CurKey = VisibilityTrack->VisibilityTrack[ CurKeyIndex ];

						// Is this key's time within the range that we'll be skipping over due to the Force Start
						// Position being set to a later time, we'll warn the user about that!
						if( CurKey.Time < MatineeActor->ForceStartPosition )
						{
							// One warning per track is plenty!
							bNeedWarning = true;
							break;
						}
					}
				}


				// Sound tracks don't play nice with bForceStartPos since we can't start playing from the middle
				// of an audio clip (not supported, yet)
				const UInterpTrackSound* SoundTrack = Cast<const  UInterpTrackSound >( CurTrack );
				if( SoundTrack != NULL )
				{
					for( int32 CurKeyIndex = 0; CurKeyIndex < SoundTrack->Sounds.Num(); ++CurKeyIndex )
					{
						const FSoundTrackKey& CurKey = SoundTrack->Sounds[ CurKeyIndex ];

						// Is this key's time within the range that we'll be skipping over due to the Force Start
						// Position being set to a later time, we'll warn the user about that!
						if( CurKey.Time < MatineeActor->ForceStartPosition )
						{
							// One warning per track is plenty!
							bNeedWarning = true;
							break;
						}
					}
				}


				// Event tracks are only OK if bFireEventsWhenJumpingForwards is also set, since that will go
				// back and fire off events between 0 and the ForceStartPosition
				const UInterpTrackEvent* EventTrack = Cast<const  UInterpTrackEvent >( CurTrack );
				if( EventTrack != NULL && ( EventTrack->bFireEventsWhenJumpingForwards == false ) )
				{
					for( int32 CurKeyIndex = 0; CurKeyIndex < EventTrack->EventTrack.Num(); ++CurKeyIndex )
					{
						const FEventTrackKey& CurKey = EventTrack->EventTrack[ CurKeyIndex ];

						// Is this key's time within the range that we'll be skipping over due to the Force Start
						// Position being set to a later time, we'll warn the user about that!
						if( CurKey.Time < MatineeActor->ForceStartPosition )
						{
							// One warning per track is plenty!
							bNeedWarning = true;
							break;
						}
					}
				}

				if( bNeedWarning )
				{
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("TrackTitle"), FText::FromString( CurTrack->TrackTitle ));
					Arguments.Add(TEXT("GroupName"), FText::FromName( CurGroup->GroupName ));
					Arguments.Add(TEXT("Time"), MatineeActor->ForceStartPosition);
					EditorErrors.Warning(FText::Format(LOCTEXT("InterpEd_TrackKeyAffectedByForceStartPosition_F", "bForceStartPos is enabled but a {TrackTitle} Track in Group '{GroupName}' has a key frame before ForceStartPosition time of {Time}.  (Key frame will be IGNORED when sequence is played back in-game!)"), Arguments));
				}
			}
		}
	}

	EditorErrors.Notify(NSLOCTEXT( "Matinee", "MatineeWarnings", "Matinee Generated Warnings" ));

	// Set position to the start of the interpolation.
	// Will position objects as the first frame of the sequence.
	MatineeActor->UpdateInterp(0.f, true);

	CamViewGroup = NULL;

	bLoopingSection = false;
	bDragging3DHandle = false;

	PlaybackSpeed = 1.0f;
	PlaybackStartRealTime = 0.0;
	NumContinuousFixedTimeStepFrames = 0;


	// Update cam frustum colours.
	UpdateCamColours();

	// Setup property window
	BuildPropertyWindow();

	// Do not override realtime audio by default
	SetAudioRealtimeOverride(false);

	// Setup track windows
	BuildTrackWindow();

	// Create new curve editor setup if not already done
	BuildCurveEditor();

	// Setup docked windows
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_Matinee_Layout_v4")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.1)
				->SetHideTabWell(true)
				->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.9)
				->Split
				(
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Horizontal)
					->SetSizeCoefficient(0.7)
					->Split
					(
						FTabManager::NewSplitter()
						->SetOrientation(Orient_Vertical)
						->Split
						(
							FTabManager::NewStack()
							->SetSizeCoefficient(1.0f / 3.0f)
							->AddTab(MatineeCurveEdName, ETabState::OpenedTab)
						)
						->Split
						(
							FTabManager::NewStack()
							->SetSizeCoefficient(2.0f / 3.0f)
							->AddTab(MatineeTrackWindowName, ETabState::OpenedTab)
						)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.3)
						->AddTab(MatineePropertyWindowName, ETabState::OpenedTab)
					)
				)
			)
		);

	UObject* ObjectToEdit = InObjectToEdit;
	if(IsCameraAnim())
	{
		// camera anims edit the asset, rather than the temporary matinee actor
		ObjectToEdit = CastChecked<AMatineeActorCameraAnim>(MatineeActor)->CameraAnim;
	}

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, MatineeAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectToEdit);
	
	ExtendToolbar();
	ExtendDefaultToolbarMenu();
	RegenerateMenusAndToolbars();

	// Initialize snap settings.
	bSnapToKeys = false;
	bSnapEnabled = false;
	bSnapToFrames = false;	
	bSnapTimeToFrames = false;
	bFixedTimeStepPlayback = false;
	bPreferFrameNumbers = true;
	bShowTimeCursorPosForAllKeys = false;

	// Restore the director timeline setting
	if ( DirectorTrackWindow.IsValid() && DirectorTrackWindow->InterpEdVC.IsValid() )
	{
		GConfig->GetBool(TEXT("Matinee"), TEXT("DirectorTimelineEnabled"), DirectorTrackWindow->InterpEdVC->bWantTimeline, GEditorPerProjectIni);
	}

	// Load fixed time step setting
	GConfig->GetBool( TEXT("Matinee"), TEXT("FixedTimeStepPlayback"), bFixedTimeStepPlayback, GEditorPerProjectIni );

	// Load 'prefer frame numbers' setting
	GConfig->GetBool( TEXT("Matinee"), TEXT("PreferFrameNumbers"), bPreferFrameNumbers, GEditorPerProjectIni );

	// Load 'show time cursor pos for all keys' setting
	GConfig->GetBool( TEXT("Matinee"), TEXT("ShowTimeCursorPosForAllKeys"), bShowTimeCursorPosForAllKeys, GEditorPerProjectIni );

	// Restore selected snap mode from INI.
	GConfig->GetBool( TEXT("Matinee"), TEXT("SnapEnabled"), bSnapEnabled, GEditorPerProjectIni );
	GConfig->GetBool( TEXT("Matinee"), TEXT("SnapTimeToFrames"), bSnapTimeToFrames, GEditorPerProjectIni );
	int32 SelectedSnapMode = 3; // default 0.5 sec
	GConfig->GetInt(TEXT("Matinee"), TEXT("SelectedSnapMode"), SelectedSnapMode, GEditorPerProjectIni );

	//OnChangeSnapSize(SelectedSnapMode);
	SnapCombo->SetSelectedItem(SnapComboStrings[SelectedSnapMode]);

	// Update snap button & synchronize with curve editor
	SetSnapEnabled( bSnapEnabled );
	SetSnapTimeToFrames( bSnapTimeToFrames );
	SetFixedTimeStepPlayback( bFixedTimeStepPlayback );
	SetPreferFrameNumbers( bPreferFrameNumbers );
	SetShowTimeCursorPosForAllKeys( bShowTimeCursorPosForAllKeys );

	// We always default to Curve (Auto/Clamped) when we have no other settings
	InitialInterpMode = CIM_CurveAutoClamped;

	// Restore user's "initial curve interpolation mode" setting from their preferences file
	{
		// NOTE: InitialInterpMode now has a '2' suffix after a version bump to change the default
		int32 DesiredInitialInterpMode = ( int32 )InitialInterpMode;
		GConfig->GetInt( TEXT( "Matinee" ), TEXT( "InitialInterpMode2" ), DesiredInitialInterpMode, GEditorPerProjectIni );
		InitialInterpModeComboBox->SetSelectedItem( InitialInterpModeStrings[DesiredInitialInterpMode] );
	}

	// Will look at current selection to set active track
	ActorSelectionChange();

	// Load gradient texture for bars
	BarGradText = LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorMaterials/MatineeGreyGrad.MatineeGreyGrad"), NULL, LOAD_None, NULL);

	// If there is a Director group in this data, default to locking the camera to it.
	UInterpGroupDirector* DirGroup = IData->FindDirectorGroup();
	if(DirGroup)
	{
		LockCamToGroup(DirGroup);
	}

	for(int32 i=0; i<GEditor->LevelViewportClients.Num(); i++)
	{
		FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[i];
		if(LevelVC)
		{
			// If there is a director group, set the perspective viewports to realtime automatically.
			if(LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview())
			{				
				//Ensure Realtime is turned on and store the original setting so we can restore it later.
				LevelVC->SetRealtime(true, true);
			}
		}
	}

	TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout( "RecordingViewport_Layout" )
		->AddArea
		(
			FTabManager::NewArea( 800, 600 )
				->Split
				(
					FTabManager::NewStack()
					->AddTab( "RecordingViewport", ETabState::ClosedTab )
				)	
		);

	FGlobalTabmanager::Get()->RestoreFrom( Layout, TSharedPtr<SWindow>() );

	// OK, we're now initialized!
	bIsInitialized = true;

	// register for any actor move change
	OnActorMovedDelegateHandle = GEngine->OnActorMoved().AddRaw(this, &FMatinee::OnActorMoved);

	// register for any objects replaced
	GEditor->OnObjectsReplaced().AddSP(this, &FMatinee::OnObjectsReplaced);

	// Make sure any particle replay tracks are filled in with the correct state
	UpdateParticleReplayTracks();

	// Now that we've filled in the track window's contents, reconfigure our scroll bar
	UpdateTrackWindowScrollBars();

	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	FEditorSupportDelegates::UpdateUI.Broadcast();
}

FMatinee::~FMatinee()
{
	OnClose();

	RestoreLevelViewports();

	TSharedPtr< SDockTab > PinnedMatineeRecorderTab = MatineeRecorderTab.Pin();
	if ( PinnedMatineeRecorderTab.IsValid() && PinnedMatineeRecorderTab->GetParentWindow().IsValid() )
	{
		if(!FSlateApplication::Get().IsWindowInDestroyQueue(PinnedMatineeRecorderTab->GetParentWindow().ToSharedRef()))
		{
			PinnedMatineeRecorderTab->RequestCloseTab();
		}
	}

	FGlobalTabmanager::Get()->UnregisterTabSpawner("RecordingViewport");

	DestroyColorPicker();
}

void FMatinee::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( BarGradText );
	Collector.AddReferencedObject( MatineeActor );
	Collector.AddReferencedObject( IData );
	Collector.AddReferencedObject( CamViewGroup );
	Collector.AddReferencedObject( NormalTransactor );
	Collector.AddReferencedObject( InterpEdTrans );
	Collector.AddReferencedObject( Opt );
	Collector.AddReferencedObject( PreviousCamera );

	Collector.AddReferencedObjects(RecordingTracks);
	Collector.AddReferencedObjects(RecordingParentOffsets);
	Collector.AddReferencedObjects(TrackToNewKeyIndexMap);

	for (TPair<UInterpTrack*, AddKeyInfo>& Pair : AddKeyInfoMap)
	{
		Collector.AddReferencedObject( Pair.Key );
		Collector.AddReferencedObject( Pair.Value.TrInst );
		Collector.AddReferencedObject( Pair.Value.TrackHelper );
	}

	// Check for non-NULL, as these references will be cleared in OnClose.
	if ( TrackWindow.IsValid() && TrackWindow->InterpEdVC.IsValid() )
	{
		TrackWindow->InterpEdVC->AddReferencedObjects( Collector );
	}
	if ( DirectorTrackWindow.IsValid() && DirectorTrackWindow->InterpEdVC.IsValid() )
	{
		DirectorTrackWindow->InterpEdVC->AddReferencedObjects( Collector );
	}
}

/** Bind the toolbar/menu items to functions */
void FMatinee::BindCommands()
{
	const FMatineeCommands& Commands = FMatineeCommands::Get();

	ToolkitCommands->MapAction(Commands.AddKey,FExecuteAction::CreateSP(this,&FMatinee::OnMenuAddKey));
	
	ToolkitCommands->MapAction( Commands.Play, FExecuteAction::CreateSP(this,&FMatinee::OnMenuPlay, false, true) );
	ToolkitCommands->MapAction( Commands.PlayLoop, FExecuteAction::CreateSP(this,&FMatinee::OnMenuPlay, true, true) );
	ToolkitCommands->MapAction( Commands.PlayReverse, FExecuteAction::CreateSP(this,&FMatinee::OnMenuPlay, false, false) );
	ToolkitCommands->MapAction( Commands.Stop, FExecuteAction::CreateSP(this, &FMatinee::OnMenuStop));

	//there is no menu UI for this
	ToolkitCommands->MapAction( Commands.PlayPause, FExecuteAction::CreateSP(this, &FMatinee::OnMenuPause));

	ToolkitCommands->MapAction( Commands.CreateCameraActor,FExecuteAction::CreateSP(this, &FMatinee::OnCreateCameraActorAtCurrentCameraLocation));
	
	ToolkitCommands->MapAction(FGenericCommands::Get().Undo, FExecuteAction::CreateSP(this, &FMatinee::OnMenuUndo));
	ToolkitCommands->MapAction(FGenericCommands::Get().Redo, FExecuteAction::CreateSP(this, &FMatinee::OnMenuRedo));
	
	ToolkitCommands->MapAction( Commands.ToggleSnap, 
		FExecuteAction::CreateSP(this,&FMatinee::OnToggleSnap),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this,&FMatinee::IsSnapToggled)
		);
	ToolkitCommands->MapAction( Commands.ToggleSnapTimeToFrames, 
		FExecuteAction::CreateSP(this,&FMatinee::OnToggleSnapTimeToFrames),
		FCanExecuteAction::CreateSP(this,&FMatinee::IsSnapTimeToFramesEnabled),
		FIsActionChecked::CreateSP(this,&FMatinee::IsSnapTimeToFramesToggled)
		);
	ToolkitCommands->MapAction( Commands.FixedTimeStepPlayback, 
		FExecuteAction::CreateSP(this,&FMatinee::OnFixedTimeStepPlaybackCommand),
		FCanExecuteAction::CreateSP(this,&FMatinee::IsFixedTimeStepPlaybackEnabled),
		FIsActionChecked::CreateSP(this,&FMatinee::IsFixedTimeStepPlaybackToggled)
		);
	
	ToolkitCommands->MapAction( Commands.FitSequence, FExecuteAction::CreateSP(this,&FMatinee::OnViewFitSequence));
	ToolkitCommands->MapAction( Commands.FitViewToSelected, FExecuteAction::CreateSP(this,&FMatinee::OnViewFitToSelected) );
	ToolkitCommands->MapAction( Commands.FitLoop, FExecuteAction::CreateSP(this,&FMatinee::OnViewFitLoop) );
	ToolkitCommands->MapAction( Commands.FitLoopSequence, FExecuteAction::CreateSP(this,&FMatinee::OnViewFitLoopSequence) );
	ToolkitCommands->MapAction( Commands.ViewEndofTrack, FExecuteAction::CreateSP(this,&FMatinee::OnViewEndOfTrack) );

	ToolkitCommands->MapAction( Commands.ToggleGorePreview, 
		FExecuteAction::CreateSP(this,&FMatinee::OnToggleGorePreview),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this,&FMatinee::IsGorePreviewToggled)
		);
	
	ToolkitCommands->MapAction( Commands.LaunchRecordWindow, FExecuteAction::CreateSP(this, &FMatinee::OnLaunchRecordingViewport) );
	ToolkitCommands->MapAction( Commands.CreateMovie, FExecuteAction::CreateSP(this, &FMatinee::OnMenuCreateMovie) );

	ToolkitCommands->MapAction( Commands.FileImport, FExecuteAction::CreateSP(this, &FMatinee::OnMenuImport) );
	ToolkitCommands->MapAction( Commands.FileExport, FExecuteAction::CreateSP(this, &FMatinee::OnMenuExport) );
	ToolkitCommands->MapAction( Commands.ExportSoundCueInfo, FExecuteAction::CreateSP(this, &FMatinee::OnExportSoundCueInfoCommand) );
	ToolkitCommands->MapAction( Commands.ExportAnimInfo, FExecuteAction::CreateSP(this, &FMatinee::OnExportAnimationInfoCommand) );
	ToolkitCommands->MapAction( Commands.FileExportBakeTransforms, 
		FExecuteAction::CreateSP(this, &FMatinee::OnToggleBakeTransforms),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::IsBakeTransformsToggled)
		);
	ToolkitCommands->MapAction(Commands.FileExportKeepHierarchy,
		FExecuteAction::CreateSP(this, &FMatinee::OnToggleKeepHierarchy),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::IsKeepHierarchyToggled)
		);
	
	ToolkitCommands->MapAction( Commands.DeleteSelectedKeys, FExecuteAction::CreateSP(this, &FMatinee::OnDeleteSelectedKeys) );
	ToolkitCommands->MapAction( Commands.DuplicateKeys, FExecuteAction::CreateSP(this, &FMatinee::OnMenuDuplicateSelectedKeys) );
	ToolkitCommands->MapAction( Commands.InsertSpace, FExecuteAction::CreateSP(this, &FMatinee::OnMenuInsertSpace) );
	ToolkitCommands->MapAction( Commands.StretchSection, FExecuteAction::CreateSP(this, &FMatinee::OnMenuStretchSection) );
	ToolkitCommands->MapAction( Commands.StretchSelectedKeyFrames, FExecuteAction::CreateSP(this, &FMatinee::OnMenuStretchSelectedKeyframes) );
	ToolkitCommands->MapAction( Commands.DeleteSection, FExecuteAction::CreateSP(this, &FMatinee::OnMenuDeleteSection) );
	ToolkitCommands->MapAction( Commands.SelectInSection, FExecuteAction::CreateSP(this, &FMatinee::OnMenuSelectInSection) );
	ToolkitCommands->MapAction( Commands.ReduceKeys, FExecuteAction::CreateSP(this, &FMatinee::OnMenuReduceKeys) );
	ToolkitCommands->MapAction( Commands.SavePathTime, FExecuteAction::CreateSP(this, &FMatinee::OnSavePathTime) );
	ToolkitCommands->MapAction( Commands.JumpToPathTime, FExecuteAction::CreateSP(this, &FMatinee::OnJumpToPathTime) );
	
	ToolkitCommands->MapAction( Commands.Draw3DTrajectories, 
		FExecuteAction::CreateSP(this, &FMatinee::OnViewHide3DTracks),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::IsViewHide3DTracksToggled)
		);
	ToolkitCommands->MapAction( Commands.ShowAll3DTrajectories, FExecuteAction::CreateSP(this, &FMatinee::OnViewShowOrHideAll3DTrajectories, true));
	ToolkitCommands->MapAction( Commands.HideAll3DTrajectories, FExecuteAction::CreateSP(this, &FMatinee::OnViewShowOrHideAll3DTrajectories, false));
	ToolkitCommands->MapAction( Commands.PreferFrameNumbers, 
		FExecuteAction::CreateSP(this, &FMatinee::OnPreferFrameNumbersCommand),
		FCanExecuteAction::CreateSP(this, &FMatinee::IsPreferFrameNumbersEnabled),
		FIsActionChecked::CreateSP(this, &FMatinee::IsPreferFrameNumbersToggled)
		);
	ToolkitCommands->MapAction( Commands.ShowTimeCursorPosForAllKeys, 
		FExecuteAction::CreateSP(this, &FMatinee::OnShowTimeCursorPosForAllKeysCommand),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::IsShowTimeCursorPosForAllKeysToggled)
		);

	ToolkitCommands->MapAction( Commands.ZoomToTimeCursorPosition, 
		FExecuteAction::CreateSP(this, &FMatinee::OnViewZoomToScrubPos),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::IsViewZoomToScrubPosToggled)
		);
	ToolkitCommands->MapAction( Commands.ViewFrameStats, 
		FExecuteAction::CreateSP(this, &FMatinee::OnToggleViewportFrameStats),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::IsViewportFrameStatsToggled)
		);
	ToolkitCommands->MapAction( Commands.EditingCrosshair, 
		FExecuteAction::CreateSP(this, &FMatinee::OnToggleEditingCrosshair),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::IsEditingCrosshairToggled)
		);
	
	ToolkitCommands->MapAction( Commands.EnableEditingGrid, 
		FExecuteAction::CreateSP(this, &FMatinee::OnEnableEditingGrid),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::IsEnableEditingGridToggled)
		);
	ToolkitCommands->MapAction( Commands.TogglePanInvert, 
		FExecuteAction::CreateSP(this, &FMatinee::OnToggleInvertPan),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::IsInvertPanToggled)
		);
	ToolkitCommands->MapAction( Commands.ToggleAllowKeyframeBarSelection, 
		FExecuteAction::CreateSP(this, &FMatinee::OnToggleKeyframeBarSelection),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::IsKeyframeBarSelectionToggled)
		);
	ToolkitCommands->MapAction( Commands.ToggleAllowKeyframeTextSelection,
		FExecuteAction::CreateSP(this, &FMatinee::OnToggleKeyframeTextSelection),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::IsKeyframeTextSelectionToggled)
		);

	ToolkitCommands->MapAction( Commands.ToggleLockCameraPitch,
		FExecuteAction::CreateSP(this, &FMatinee::OnToggleLockCameraPitch),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::IsLockCameraPitchToggled)
		);

	//Context Menus
	ToolkitCommands->MapAction( Commands.EditCut, FExecuteAction::CreateSP(this, &FMatinee::OnMenuCut), FCanExecuteAction::CreateSP(this, &FMatinee::CanCut) );
	ToolkitCommands->MapAction( Commands.EditCopy, FExecuteAction::CreateSP(this, &FMatinee::OnMenuCopy) );
	ToolkitCommands->MapAction( Commands.EditPaste, 
		FExecuteAction::CreateSP(this, &FMatinee::OnMenuPaste),
		FCanExecuteAction::CreateSP(this, &FMatinee::CanPasteGroupOrTrack)
		);

	//Tab Menu
	ToolkitCommands->MapAction( Commands.GroupDeleteTab, FExecuteAction::CreateSP(this, &FMatinee::OnContextDeleteGroupTab) );

	//Group Menu
	ToolkitCommands->MapAction( Commands.ActorSelectAll, FExecuteAction::CreateSP(this, &FMatinee::OnContextSelectAllActors) );
	ToolkitCommands->MapAction( Commands.ActorAddAll, FExecuteAction::CreateSP(this, &FMatinee::OnContextAddAllActors) );
	ToolkitCommands->MapAction( Commands.ActorReplaceAll, FExecuteAction::CreateSP(this, &FMatinee::OnContextReplaceAllActors) );
	ToolkitCommands->MapAction( Commands.ActorRemoveAll, FExecuteAction::CreateSP(this, &FMatinee::OnContextRemoveAllActors) );
	ToolkitCommands->MapAction( Commands.ExportCameraAnim, FExecuteAction::CreateSP(this, &FMatinee::OnContextSaveAsCameraAnimation) );
	ToolkitCommands->MapAction( Commands.ExportAnimGroupFBX, FExecuteAction::CreateSP(this, &FMatinee::OnContextGroupExportAnimFBX) );
	ToolkitCommands->MapAction( 
		Commands.GroupDuplicate, 
		FExecuteAction::CreateSP(this, &FMatinee::OnContextNewGroup, FMatineeCommands::EGroupAction::DuplicateGroup),
		FCanExecuteAction::CreateSP(this, &FMatinee::CanCreateNewGroup, FMatineeCommands::EGroupAction::DuplicateGroup)
		);
	ToolkitCommands->MapAction( Commands.GroupDelete, FExecuteAction::CreateSP(this, &FMatinee::OnContextGroupDelete), FCanExecuteAction::CreateSP(this, &FMatinee::CanGroupDelete) );
	ToolkitCommands->MapAction( Commands.GroupCreateTab, FExecuteAction::CreateSP(this, &FMatinee::OnContextGroupCreateTab), FCanExecuteAction::CreateSP(this, &FMatinee::CanGroupCreateTab) );
	ToolkitCommands->MapAction( Commands.GroupRemoveFromTab, FExecuteAction::CreateSP(this, &FMatinee::OnContextGroupRemoveFromTab) );
	ToolkitCommands->MapAction( 
		Commands.RemoveFromGroupFolder, 
		FExecuteAction::CreateSP(this, &FMatinee::OnContextGroupChangeGroupFolder, FMatineeCommands::EGroupAction::RemoveFromGroupFolder, -1) 
		);

	//Track Context Menu
	ToolkitCommands->MapAction( Commands.TrackRename, FExecuteAction::CreateSP(this, &FMatinee::OnContextTrackRename) );
	ToolkitCommands->MapAction( Commands.TrackDelete, FExecuteAction::CreateSP(this, &FMatinee::OnContextTrackDelete) );
	ToolkitCommands->MapAction( Commands.Show3DTrajectory, FExecuteAction::CreateSP(this, &FMatinee::OnContextTrackShow3DTrajectory) );
	ToolkitCommands->MapAction( Commands.TrackSplitTransAndRot, FExecuteAction::CreateSP(this, &FMatinee::OnSplitTranslationAndRotation) );
	ToolkitCommands->MapAction( Commands.TrackNormalizeVelocity, FExecuteAction::CreateSP(this, &FMatinee::NormalizeVelocity) );
	ToolkitCommands->MapAction( Commands.ScaleTranslation, FExecuteAction::CreateSP(this, &FMatinee::ScaleMoveTrackTranslation) );
	ToolkitCommands->MapAction( Commands.ParticleReplayTrackContextStartRecording, FExecuteAction::CreateSP(this, &FMatinee::OnParticleReplayTrackContext_ToggleCapture, true) );
	ToolkitCommands->MapAction( Commands.ParticleReplayTrackContextStopRecording, FExecuteAction::CreateSP(this, &FMatinee::OnParticleReplayTrackContext_ToggleCapture, false) );
	ToolkitCommands->MapAction( Commands.ExportAnimTrackFBX, FExecuteAction::CreateSP(this, &FMatinee::OnContextTrackExportAnimFBX) );

	//Background Context Menu
	ToolkitCommands->MapAction( Commands.NewFolder, FExecuteAction::CreateSP(this, &FMatinee::OnContextNewGroup, FMatineeCommands::EGroupAction::NewFolder) );
	ToolkitCommands->MapAction( Commands.NewEmptyGroup, FExecuteAction::CreateSP(this, &FMatinee::OnContextNewGroup, FMatineeCommands::EGroupAction::NewEmptyGroup) );
	ToolkitCommands->MapAction( Commands.NewCameraGroup, FExecuteAction::CreateSP(this, &FMatinee::OnContextNewGroup, FMatineeCommands::EGroupAction::NewCameraGroup) );
	ToolkitCommands->MapAction( Commands.NewParticleGroup, FExecuteAction::CreateSP(this, &FMatinee::OnContextNewGroup, FMatineeCommands::EGroupAction::NewParticleGroup) );
	ToolkitCommands->MapAction( Commands.NewSkeletalMeshGroup, FExecuteAction::CreateSP(this, &FMatinee::OnContextNewGroup, FMatineeCommands::EGroupAction::NewSkeletalMeshGroup) );
	ToolkitCommands->MapAction( Commands.NewLightingGroup, FExecuteAction::CreateSP(this, &FMatinee::OnContextNewGroup, FMatineeCommands::EGroupAction::NewLightingGroup) );
	ToolkitCommands->MapAction( Commands.NewDirectorGroup, FExecuteAction::CreateSP(this, &FMatinee::OnContextNewGroup, FMatineeCommands::EGroupAction::NewDirectorGroup) );

	// Menu
	ToolkitCommands->MapAction(Commands.ToggleCurveEditor,
		FExecuteAction::CreateSP(this, &FMatinee::OnToggleCurveEditor),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::IsCurveEditorToggled)
		);
	ToolkitCommands->MapAction( Commands.ToggleDirectorTimeline, 
		FExecuteAction::CreateSP(this, &FMatinee::OnToggleDirectorTimeline),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::IsDirectorTimelineToggled)
		);

	//Key Context Menu
	ToolkitCommands->MapAction( Commands.KeyModeCurveAuto, FExecuteAction::CreateSP(this, &FMatinee::OnContextKeyInterpMode, FMatineeCommands::EKeyAction::KeyModeCurveAuto) );
	ToolkitCommands->MapAction( Commands.KeyModeCurveAutoClamped, FExecuteAction::CreateSP(this, &FMatinee::OnContextKeyInterpMode, FMatineeCommands::EKeyAction::KeyModeCurveAutoClamped) );
	ToolkitCommands->MapAction( Commands.KeyModeCurveBreak, FExecuteAction::CreateSP(this, &FMatinee::OnContextKeyInterpMode, FMatineeCommands::EKeyAction::KeyModeCurveBreak) );
	ToolkitCommands->MapAction( Commands.KeyModeLinear, FExecuteAction::CreateSP(this, &FMatinee::OnContextKeyInterpMode, FMatineeCommands::EKeyAction::KeyModeLinear) );
	ToolkitCommands->MapAction( Commands.KeyModeConstant, FExecuteAction::CreateSP(this, &FMatinee::OnContextKeyInterpMode, FMatineeCommands::EKeyAction::KeyModeConstant) );
	ToolkitCommands->MapAction( Commands.KeySetTime, FExecuteAction::CreateSP(this, &FMatinee::OnContextSetKeyTime) );
	ToolkitCommands->MapAction( Commands.KeySetValue, FExecuteAction::CreateSP(this, &FMatinee::OnContextSetValue) );
	ToolkitCommands->MapAction( Commands.KeySetBool, FExecuteAction::CreateSP(this, &FMatinee::OnContextSetBool) );
	ToolkitCommands->MapAction( Commands.KeySetColor, FExecuteAction::CreateSP(this, &FMatinee::OnContextSetColor) );
	ToolkitCommands->MapAction( Commands.EventKeyRename, FExecuteAction::CreateSP(this, &FMatinee::OnContextRenameEventKey) );
	ToolkitCommands->MapAction( Commands.DirKeySetTransitionTime, FExecuteAction::CreateSP(this, &FMatinee::OnContextDirKeyTransitionTime) );
	ToolkitCommands->MapAction( Commands.DirKeyRenameCameraShot, FExecuteAction::CreateSP(this, &FMatinee::OnContextDirKeyRenameCameraShot) );
	ToolkitCommands->MapAction( Commands.KeySetMasterVolume, FExecuteAction::CreateSP(this, &FMatinee::OnKeyContext_SetMasterVolume) );
	ToolkitCommands->MapAction( Commands.KeySetMasterPitch, FExecuteAction::CreateSP(this, &FMatinee::OnKeyContext_SetMasterPitch) );
	ToolkitCommands->MapAction( Commands.ToggleKeyFlip, FExecuteAction::CreateSP(this, &FMatinee::OnFlipToggleKey) );

	ToolkitCommands->MapAction( Commands.KeySetConditionAlways, 
		FExecuteAction::CreateSP(this, &FMatinee::OnKeyContext_SetCondition, FMatineeCommands::EKeyAction::ConditionAlways),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::KeyContext_IsSetConditionToggled, FMatineeCommands::EKeyAction::ConditionAlways)
		);
	ToolkitCommands->MapAction( Commands.KeySetConditionGoreEnabled, 
		FExecuteAction::CreateSP(this, &FMatinee::OnKeyContext_SetCondition, FMatineeCommands::EKeyAction::ConditionGoreEnabled),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::KeyContext_IsSetConditionToggled, FMatineeCommands::EKeyAction::ConditionGoreEnabled)
		);
	ToolkitCommands->MapAction( Commands.KeySetConditionGoreDisabled, 
		FExecuteAction::CreateSP(this, &FMatinee::OnKeyContext_SetCondition, FMatineeCommands::EKeyAction::ConditionGoreDisabled),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::KeyContext_IsSetConditionToggled, FMatineeCommands::EKeyAction::ConditionGoreDisabled)
		);

	ToolkitCommands->MapAction( Commands.AnimKeyLoop, FExecuteAction::CreateSP(this, &FMatinee::OnSetAnimKeyLooping, true) );
	ToolkitCommands->MapAction( Commands.AnimKeyNoLoop, FExecuteAction::CreateSP(this, &FMatinee::OnSetAnimKeyLooping, false) );
	ToolkitCommands->MapAction( Commands.AnimKeySetStartOffset, FExecuteAction::CreateSP(this, &FMatinee::OnSetAnimOffset, false) );
	ToolkitCommands->MapAction( Commands.AnimKeySetEndOffset, FExecuteAction::CreateSP(this, &FMatinee::OnSetAnimOffset, true) );
	ToolkitCommands->MapAction( Commands.AnimKeySetPlayRate, FExecuteAction::CreateSP(this, &FMatinee::OnSetAnimPlayRate) );
	ToolkitCommands->MapAction( 
		Commands.AnimKeyToggleReverse, 
		FExecuteAction::CreateSP(this, &FMatinee::OnToggleReverseAnim), 
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMatinee::IsReverseAnimToggled)
		);
	ToolkitCommands->MapAction( Commands.KeySyncGenericBrowserToSoundCue, FExecuteAction::CreateSP(this, &FMatinee::OnKeyContext_SyncGenericBrowserToSoundCue) );
	ToolkitCommands->MapAction( Commands.ParticleReplayKeySetClipIDNumber, FExecuteAction::CreateSP(this, &FMatinee::OnParticleReplayKeyContext_SetClipIDNumber) );
	ToolkitCommands->MapAction( Commands.ParticleReplayKeySetDuration, FExecuteAction::CreateSP(this, &FMatinee::OnParticleReplayKeyContext_SetDuration) );
	ToolkitCommands->MapAction( Commands.SoundKeySetVolume, FExecuteAction::CreateSP(this, &FMatinee::OnSetSoundVolume) );
	ToolkitCommands->MapAction( Commands.SoundKeySetPitch, FExecuteAction::CreateSP(this, &FMatinee::OnSetSoundPitch) );
	ToolkitCommands->MapAction( Commands.MoveKeySetLookup, FExecuteAction::CreateSP(this, &FMatinee::OnSetMoveKeyLookupGroup) );
	ToolkitCommands->MapAction( Commands.MoveKeyClearLookup, FExecuteAction::CreateSP(this, &FMatinee::OnClearMoveKeyLookupGroup) );

	//Collapse/Expand context menu
	ToolkitCommands->MapAction( Commands.ExpandAllGroups, FExecuteAction::CreateSP(this, &FMatinee::OnExpandAllGroups) );
	ToolkitCommands->MapAction( Commands.CollapseAllGroups, FExecuteAction::CreateSP(this, &FMatinee::OnCollapseAllGroups) );

	//Marker Context Menu
	ToolkitCommands->MapAction( Commands.MarkerMoveToBeginning, FExecuteAction::CreateSP(this, &FMatinee::OnContextMoveMarkerToBeginning) );
	ToolkitCommands->MapAction( Commands.MarkerMoveToEnd, FExecuteAction::CreateSP(this, &FMatinee::OnContextMoveMarkerToEnd) );
	ToolkitCommands->MapAction( Commands.MarkerMoveToEndOfLongestTrack, FExecuteAction::CreateSP(this, &FMatinee::OnContextMoveMarkerToEndOfLongestTrack) );
	ToolkitCommands->MapAction( Commands.MarkerMoveToEndOfSelectedTrack, FExecuteAction::CreateSP(this, &FMatinee::OnContextMoveMarkerToEndOfSelectedTrack) );
	ToolkitCommands->MapAction( Commands.MarkerMoveToCurrentPosition, FExecuteAction::CreateSP(this, &FMatinee::OnContextMoveMarkerToCurrentPosition) );

	//Viewport/Key Commands
	ToolkitCommands->MapAction( Commands.ZoomIn, FExecuteAction::CreateSP(this, &FMatinee::ZoomView, 1.0f / InterpEditor_ZoomIncrement, true ) );
	ToolkitCommands->MapAction( Commands.ZoomOut, FExecuteAction::CreateSP(this, &FMatinee::ZoomView, InterpEditor_ZoomIncrement, true ) ); 
	ToolkitCommands->MapAction( Commands.ZoomInAlt, FExecuteAction::CreateSP(this, &FMatinee::ZoomView, 1.0f / InterpEditor_ZoomIncrement, true ) ); 
	ToolkitCommands->MapAction( Commands.ZoomOutAlt, FExecuteAction::CreateSP(this, &FMatinee::ZoomView, InterpEditor_ZoomIncrement, true ) ); 
	ToolkitCommands->MapAction( Commands.MarkInSection, FExecuteAction::CreateSP(this, &FMatinee::OnMarkInSection) );  
	ToolkitCommands->MapAction( Commands.MarkOutSection, FExecuteAction::CreateSP(this, &FMatinee::OnMarkOutSection) );
	ToolkitCommands->MapAction( Commands.IncrementPosition, FExecuteAction::CreateSP(this, &FMatinee::IncrementSelection) );
	ToolkitCommands->MapAction( Commands.DecrementPosition, FExecuteAction::CreateSP(this, &FMatinee::DecrementSelection) );
	ToolkitCommands->MapAction( Commands.MoveToNextKey, FExecuteAction::CreateSP(this, &FMatinee::SelectNextKey) );
	ToolkitCommands->MapAction( Commands.MoveToPrevKey, FExecuteAction::CreateSP(this, &FMatinee::SelectPreviousKey) );
	ToolkitCommands->MapAction( Commands.SplitAnimKey, FExecuteAction::CreateSP(this, &FMatinee::SplitAnimKey) );
	ToolkitCommands->MapAction( Commands.MoveActiveUp, FExecuteAction::CreateSP(this, &FMatinee::MoveActiveUp) );
	ToolkitCommands->MapAction( Commands.MoveActiveDown, FExecuteAction::CreateSP(this, &FMatinee::MoveActiveDown) );
	ToolkitCommands->MapAction( Commands.DuplicateSelectedKeys, FExecuteAction::CreateSP(this, &FMatinee::DuplicateSelectedKeys) );
	ToolkitCommands->MapAction( Commands.CropAnimationBeginning, FExecuteAction::CreateSP(this, &FMatinee::CropAnimKey, true) );
	ToolkitCommands->MapAction( Commands.CropAnimationEnd, FExecuteAction::CreateSP(this, &FMatinee::CropAnimKey, false) );
	ToolkitCommands->MapAction( Commands.ChangeKeyInterpModeAuto, FExecuteAction::CreateSP(this, &FMatinee::ChangeKeyInterpMode, CIM_CurveAuto) );
	ToolkitCommands->MapAction( Commands.ChangeKeyInterpModeAutoClamped, FExecuteAction::CreateSP(this, &FMatinee::ChangeKeyInterpMode, CIM_CurveAutoClamped) );
	ToolkitCommands->MapAction( Commands.ChangeKeyInterpModeUser, FExecuteAction::CreateSP(this, &FMatinee::ChangeKeyInterpMode, CIM_CurveUser) );
	ToolkitCommands->MapAction( Commands.ChangeKeyInterpModeBreak, FExecuteAction::CreateSP(this, &FMatinee::ChangeKeyInterpMode, CIM_CurveBreak) );
	ToolkitCommands->MapAction( Commands.ChangeKeyInterpModeLinear, FExecuteAction::CreateSP(this, &FMatinee::ChangeKeyInterpMode, CIM_Linear) );
	ToolkitCommands->MapAction( Commands.ChangeKeyInterpModeConstant, FExecuteAction::CreateSP(this, &FMatinee::ChangeKeyInterpMode, CIM_Constant) );
	ToolkitCommands->MapAction( Commands.DeleteSelection, FExecuteAction::CreateSP(this, &FMatinee::DeleteSelection) );
	
}

/** 
 * Starts playing the current sequence. 
 * @param bPlayLoop		Whether or not we should play the looping section.
 * @param bPlayForward	true if we should play forwards, or false for reverse
 */
void FMatinee::StartPlaying(bool bPlayLoop, bool bPlayForward)
{
	bLoopingSection = bPlayLoop;
	//if looping or the marker is already at the end of the section.
	if ( bLoopingSection )
	{
		// If looping - jump to start of looping section.
		SetInterpPosition(IData->EdSectionStart);
	}

	// Were we already in the middle of playback?
	const bool bWasAlreadyPlaying = MatineeActor->bIsPlaying;

	if ( !bWasAlreadyPlaying )
	{
		MatineeActor->bReversePlayback = !bPlayForward;
	}
	else
	{
		// Switch playback directions if we need to
		if ( MatineeActor->bReversePlayback == bPlayForward )
		{
			MatineeActor->ChangePlaybackDirection();

			// Reset our playback start time so fixed time step playback can gate frame rate properly
			PlaybackStartRealTime = FPlatformTime::Seconds();
			NumContinuousFixedTimeStepFrames = 0;
		}
	}

	ResumePlaying();
}

void FMatinee::ResumePlaying()
{
	// Force audio to play in realtime
	SetAudioRealtimeOverride(true);

	//make sure to turn off recording
	StopRecordingInterpValues();

	// Were we already in the middle of playback?
	const bool bWasAlreadyPlaying = MatineeActor->bIsPlaying;

	Opt->bAdjustingKeyframe = false;
	Opt->bAdjustingGroupKeyframes = false;

	// Start playing if we need to
	if ( !bWasAlreadyPlaying )
	{
		// If we're at the end we need to restart, but only do this if we're
		// looping the section
		if ( bLoopingSection )
		{
			if ( MatineeActor->bReversePlayback )
			{
				if ( MatineeActor->InterpPosition <= IData->EdSectionStart )
				{
					SetInterpPosition(IData->EdSectionEnd);
				}
			}
			else
			{
				if ( MatineeActor->InterpPosition >= IData->EdSectionEnd )
				{
					SetInterpPosition(IData->EdSectionStart);
				}
			}
		}
		// If we're not looping, check if we're at the absolute beginning or end and adjust
		// the position accordingly to begin playing again.
		else
		{
			if ( MatineeActor->bReversePlayback )
			{
				if ( MatineeActor->InterpPosition <= 0.0f )
				{
					SetInterpPosition(IData->InterpLength);
				}
			}
			else
			{
				if ( MatineeActor->InterpPosition >= IData->InterpLength )
				{
					SetInterpPosition(0.0f);
				}
			}
		}

		// If 'snap time to frames' or 'fixed time step playback' is turned on, we'll make sure that we
		// start playback exactly on the closest frame
		if ( bSnapToFrames && ( bSnapTimeToFrames || bFixedTimeStepPlayback ) )
		{
			SetInterpPosition(SnapTimeToNearestFrame(MatineeActor->InterpPosition));
		}

		// Start playing
		MatineeActor->bIsPlaying = true;

		// Remember the real-time that we started playing the sequence
		PlaybackStartRealTime = FPlatformTime::Seconds();
		NumContinuousFixedTimeStepFrames = 0;

		// Reset previous camera variable, used to detect cuts in editor playback
		PreviousCamera = NULL;

		// Switch the Matinee windows to real-time so the track editor and curve editor update during playback
		TrackWindow->InterpEdVC->SetRealtime(true);
		if ( DirectorTrackWindow->GetVisibility() == EVisibility::Visible )
		{
			DirectorTrackWindow->InterpEdVC->SetRealtime(true);
		}
	}

	// Make sure fixed time step mode is set correctly based on whether we're currently 'playing' or not
	UpdateFixedTimeStepPlayback();
}

/** Stops playing the current sequence. */
void FMatinee::StopPlaying()
{
	// Stop forcing audio to play in realtime
	SetAudioRealtimeOverride( false );

	//make sure to turn off recording
	StopRecordingInterpValues();

	// If already stopped, pressing stop again winds you back to the beginning.
	if(!MatineeActor->bIsPlaying)
	{
		SetInterpPosition(0.f);
		return;
	}

	// Iterate over each group/track giving it a chance to stop things.
	for(int32 i=0; i<MatineeActor->GroupInst.Num(); i++)
	{
		UInterpGroupInst* GrInst = MatineeActor->GroupInst[i];
		UInterpGroup* Group = GrInst->Group;

		check(Group->InterpTracks.Num() == GrInst->TrackInst.Num());
		for(int32 j=0; j<Group->InterpTracks.Num(); j++)
		{
			UInterpTrack* Track = Group->InterpTracks[j];
			UInterpTrackInst* TrInst = GrInst->TrackInst[j];

			Track->PreviewStopPlayback(TrInst);
		}
	}

	// Set flag to indicate stopped
	MatineeActor->bIsPlaying = false;

	// Stop viewport being realtime
	TrackWindow->InterpEdVC->SetRealtime( false );
	DirectorTrackWindow->InterpEdVC->SetRealtime( false );

	// If the 'snap time to frames' option is enabled, we'll need to snap the time cursor position to
	// the nearest frame
	if( bSnapToFrames && bSnapTimeToFrames )
	{
		SetInterpPosition( SnapTimeToNearestFrame( MatineeActor->InterpPosition ) );
	}

	// Make sure fixed time step mode is set correctly based on whether we're currently 'playing' or not
	UpdateFixedTimeStepPlayback();
}

void FMatinee::OnPostUndoRedo(FUndoSessionContext SessionContext, bool Succeeded)
{
	InvalidateTrackWindowViewports();
}

//Key Command Helpers
void FMatinee::OnMarkInSection()
{
	MoveLoopMarker(MatineeActor->InterpPosition, true);
}

void FMatinee::OnMarkOutSection()
{
	MoveLoopMarker(MatineeActor->InterpPosition, false);
}

/** Creates a popup context menu based on the item under the mouse cursor.
* @param	Viewport	FViewport for the FInterpEdViewportClient.
* @param	HitResult	HHitProxy returned by FViewport::GetHitProxy( ).
* @return	A new Menu with context-appropriate menu options or NULL if there are no appropriate menu options.
*/
TSharedPtr<SWidget> FMatinee::CreateContextMenu( FViewport *Viewport, const HHitProxy *HitResult, bool bIsDirectorTrackWindow )
{
	TSharedPtr<SWidget> Menu;

	if(HitResult->IsA(HMatineeTrackBkg::StaticGetType()))
	{
		DeselectAll();				

		if(!IsCameraAnim())
		{
			Menu = CreateBkgMenu(bIsDirectorTrackWindow);
		}
	}
	else if(HitResult->IsA(HMatineeGroupTitle::StaticGetType()))
	{
		UInterpGroup* Group = ((HMatineeGroupTitle*)HitResult)->Group;

		if( !IsGroupSelected(Group) )
		{
			// do not select actors
			SelectGroup(Group, true, false );
		}

		Menu = CreateGroupMenu();
	}
	else if(HitResult->IsA(HMatineeTrackTitle::StaticGetType()))
	{
		HMatineeTrackTitle* TrackProxy = ((HMatineeTrackTitle*)HitResult);
		UInterpGroup* Group = TrackProxy->Group;
		UInterpTrack* TrackToSelect = TrackProxy->Track;

		check( TrackToSelect );

		if( !TrackToSelect->IsSelected() )
		{
			SelectTrack( Group, TrackToSelect );
		}

		// Dont allow subtracks to have a menu as this could cause the ability to copy/paste subtracks which would be bad
		if( TrackToSelect->GetOuter()->IsA( UInterpGroup::StaticClass() ) )
		{
			Menu = CreateTrackMenu();
		}
	}
	else if(HitResult->IsA(HInterpTrackKeypointProxy::StaticGetType()))
	{
		HInterpTrackKeypointProxy* KeyProxy = ((HInterpTrackKeypointProxy*)HitResult);
		UInterpGroup* Group = KeyProxy->Group;
		UInterpTrack* Track = KeyProxy->Track ;
		const int32 KeyIndex = KeyProxy->KeyIndex;

		const bool bAlreadySelected = KeyIsInSelection(Group, Track, KeyIndex);
		if(bAlreadySelected)
		{
			Menu = CreateKeyMenu();
		}
	}
	else if(HitResult->IsA(HMatineeGroupCollapseBtn::StaticGetType()))
	{
		// Use right-clicked on the 'Expand/Collapse' track editor widget for a group
		Menu = CreateCollapseExpandMenu();
	}
	else if(HitResult->IsA(HMatineeMarker::StaticGetType()))
	{
		GrabbedMarkerType = ((HMatineeMarker*)HitResult)->Type;

		// Don't create a context menu for the sequence 
		// start marker because it should not be moved.
		if( GrabbedMarkerType != EMatineeMarkerType::ISM_SeqStart )
		{
			Menu = CreateMarkerMenu( GrabbedMarkerType );
		}
	}

	return Menu;
}

/**Preps Matinee to record/stop-recording realtime camera movement*/
void FMatinee::ToggleRecordInterpValues(void)
{
	//if we're already sampling, just stop sampling
	if (RecordingState != MatineeConstants::ERecordingState::RECORDING_COMPLETE)
	{
		StopRecordingInterpValues();

		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.Matinee.Recorded"));
		}
	}
	else
	{
		RecordingState = MatineeConstants::ERecordingState::RECORDING_GET_READY_PAUSE;
		RecordingStateStartTime = FPlatformTime::Seconds();

		InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "RecordTracks", "Record Tracks") );

		TSharedPtr< SDockTab > PinnedMatineeRecorderTab = MatineeRecorderTab.Pin();
		if ( PinnedMatineeRecorderTab.IsValid() )
		{
			FGlobalTabmanager::Get()->DrawAttention( PinnedMatineeRecorderTab.ToSharedRef() );
		}

		// Stop time if it's playing.
		MatineeActor->Modify();
		MatineeActor->Stop();
		// Move to proper start time
		SetInterpPosition(GetRecordingStartTime());
	}
}

/**Helper function to properly shut down matinee recording*/
void FMatinee::StopRecordingInterpValues(void)
{
	if (RecordingState != MatineeConstants::ERecordingState::RECORDING_COMPLETE)
	{
		//STOP SAMPLING!!!
		RecordingState = MatineeConstants::ERecordingState::RECORDING_COMPLETE;

		for (int32 i = 0; i < RecordingTracks.Num(); ++i)
		{
			RecordingTracks[i]->bIsRecording = false;
		}

		//Clear recording tracks
		RecordingTracks.Empty();

		InterpEdTrans->EndSpecial();

		// Stop time if it's playing.
		MatineeActor->Stop();
		// Move to proper start time
		SetInterpPosition(GetRecordingStartTime());
	}
}

/**
 * Increments or decrements the currently selected recording menu item
 * @param bInNext - true if going forward in the menu system, false if going backward
 */
void FMatinee::ChangeRecordingMenu(const bool bInNext)
{
	RecordMenuSelection += (bInNext ? 1 : -1);
	if (RecordMenuSelection < 0)
	{
		RecordMenuSelection = MatineeConstants::ERecordMenu::NUM_RECORD_MENU_ITEMS-1;
	} 
	else if (RecordMenuSelection == MatineeConstants::ERecordMenu::NUM_RECORD_MENU_ITEMS)
	{
		RecordMenuSelection = 0;
	}
}

/**
 * Increases or decreases the recording menu value
 * @param bInIncrease - true if increasing the value, false if decreasing the value
 */
void FMatinee::ChangeRecordingMenuValue(FEditorViewportClient* InClient, const bool bInIncrease)
{
	check(InClient);
	FEditorCameraController* CameraController = InClient->GetCameraController();
	check(CameraController);
	FCameraControllerConfig CameraConfig = CameraController->GetConfig(); 

	float DecreaseMultiplier = .99f;
	float IncreaseMultiplier = 1.0f / DecreaseMultiplier;

	switch (RecordMenuSelection)
	{
		case MatineeConstants::ERecordMenu::RECORD_MENU_RECORD_MODE:
			RecordMode += (bInIncrease ? 1 : -1);
			if (RecordMode < 0)
			{
				RecordMode = MatineeConstants::ERecordMode::NUM_RECORD_MODES-1;
			} 
			else if (RecordMode == MatineeConstants::ERecordMode::NUM_RECORD_MODES)
			{
				RecordMode = 0;
			}
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_TRANSLATION_SPEED:
			CameraConfig.TranslationMultiplier *= (bInIncrease ? IncreaseMultiplier : DecreaseMultiplier);
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_ROTATION_SPEED:
			CameraConfig.RotationMultiplier *= (bInIncrease ? IncreaseMultiplier : DecreaseMultiplier);
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_ZOOM_SPEED:
			CameraConfig.ZoomMultiplier*= (bInIncrease ? IncreaseMultiplier : DecreaseMultiplier);
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_TRIM:
			CameraConfig.PitchTrim += (bInIncrease ? 0.2f : -0.2f );
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_INVERT_X_AXIS:
			CameraConfig.bInvertX = !CameraConfig.bInvertX;
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_INVERT_Y_AXIS:
			CameraConfig.bInvertY = !CameraConfig.bInvertY;
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_ROLL_SMOOTHING:
			RecordRollSmoothingSamples += (bInIncrease ? 1 : -1);
			if (RecordRollSmoothingSamples < 1)
			{
				RecordRollSmoothingSamples = MatineeConstants::MaxSmoothingSamples-1;
			} 
			else if (RecordRollSmoothingSamples == MatineeConstants::MaxSmoothingSamples)
			{
				RecordRollSmoothingSamples = 1;
			}
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_PITCH_SMOOTHING:
			RecordPitchSmoothingSamples += (bInIncrease ? 1 : -1);
			if (RecordPitchSmoothingSamples < 1)
			{
				RecordPitchSmoothingSamples = MatineeConstants::MaxSmoothingSamples-1;
			} 
			else if (RecordPitchSmoothingSamples == MatineeConstants::MaxSmoothingSamples)
			{
				RecordPitchSmoothingSamples = 1;
			}
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_CAMERA_MOVEMENT_SCHEME:
			RecordCameraMovementScheme += (bInIncrease ? 1 : -1);
			if (RecordCameraMovementScheme < 0)
			{
				RecordCameraMovementScheme = MatineeConstants::ECameraScheme::NUM_CAMERA_SCHEMES-1;
			} 
			else if (RecordCameraMovementScheme == MatineeConstants::ECameraScheme::NUM_CAMERA_SCHEMES)
			{
				RecordCameraMovementScheme = 0;
			}
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_ZOOM_DISTANCE:
			{
				FLevelEditorViewportClient* LevelVC = GetRecordingViewport();
				if (LevelVC)
				{
					LevelVC->ViewFOV += (bInIncrease ? 5.0f : -5.0f);
				}
			}
			break;
	}

	SaveRecordingSettings(CameraConfig);

	CameraController->SetConfig(CameraConfig); 
}

/**
 * Resets the recording menu value to the default
 */
void FMatinee::ResetRecordingMenuValue(FEditorViewportClient* InClient)
{
	check(InClient);
	FEditorCameraController* CameraController = InClient->GetCameraController();
	check(CameraController);
	FCameraControllerConfig CameraConfig = CameraController->GetConfig(); 

	switch (RecordMenuSelection)
	{
		case MatineeConstants::ERecordMenu::RECORD_MENU_RECORD_MODE:
			RecordMode = 0;
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_TRANSLATION_SPEED:
			CameraConfig.TranslationMultiplier = 1.0f;
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_ROTATION_SPEED:
			CameraConfig.RotationMultiplier = 1.0f;
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_ZOOM_SPEED:
			CameraConfig.ZoomMultiplier = 1.0f;
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_TRIM:
			CameraConfig.PitchTrim = 0.0f;
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_INVERT_X_AXIS:
			CameraConfig.bInvertX = false;
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_INVERT_Y_AXIS:
			CameraConfig.bInvertY = false;
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_ROLL_SMOOTHING:
			RecordRollSmoothingSamples = 1;
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_PITCH_SMOOTHING:
			RecordPitchSmoothingSamples = 1;
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_CAMERA_MOVEMENT_SCHEME:
			RecordCameraMovementScheme = MatineeConstants::ECameraScheme::CAMERA_SCHEME_FREE_CAM;
			break;
		case MatineeConstants::ERecordMenu::RECORD_MENU_ZOOM_DISTANCE:
			{
				FLevelEditorViewportClient* LevelVC = GetRecordingViewport();
				if (LevelVC)
				{
					LevelVC->ViewFOV = EditorViewportDefs::DefaultPerspectiveFOVAngle;
				}
			}
			break;
	}

	SaveRecordingSettings(CameraConfig);

	CameraController->SetConfig(CameraConfig); 
}

/**
 * Determines whether only the first click event is allowed or all repeat events are allowed
 * @return - true, if the value should change multiple times.  false, if the user should have to release and reclick
 */
bool FMatinee::IsRecordMenuChangeAllowedRepeat (void) const 
{
	bool bAllowRepeat = true;
	switch (RecordMenuSelection)
	{
		case MatineeConstants::ERecordMenu::RECORD_MENU_RECORD_MODE:
		case MatineeConstants::ERecordMenu::RECORD_MENU_INVERT_X_AXIS:
		case MatineeConstants::ERecordMenu::RECORD_MENU_INVERT_Y_AXIS:
		case MatineeConstants::ERecordMenu::RECORD_MENU_ROLL_SMOOTHING:
		case MatineeConstants::ERecordMenu::RECORD_MENU_PITCH_SMOOTHING:
		case MatineeConstants::ERecordMenu::RECORD_MENU_CAMERA_MOVEMENT_SCHEME:
		case MatineeConstants::ERecordMenu::RECORD_MENU_ZOOM_DISTANCE:
			bAllowRepeat = false;
			break;
		default:
			break;
	}

	return bAllowRepeat;
}


/** Sets the record mode for matinee */
void FMatinee::SetRecordMode(const uint32 InNewMode)
{
	check (FMath::IsWithin<uint32>(InNewMode, 0, MatineeConstants::ERecordMode::NUM_RECORD_MODES));
	RecordMode = InNewMode;
}


/**If true, real time camera recording mode has been enabled*/
bool FMatinee::IsRecordingInterpValues (void) const
{
	return (RecordingState != MatineeConstants::ERecordingState::RECORDING_COMPLETE);
}

/** Returns The time that sampling should start at */
const double FMatinee::GetRecordingStartTime (void) const
{
	if (IData->EdSectionStart == IData->EdSectionEnd)
	{
		return 0.0;
	}
	return (IData->EdSectionStart);
}

/** Returns The time that sampling should end at */
const double FMatinee::GetRecordingEndTime (void) const
{
	if (IData->EdSectionStart == IData->EdSectionEnd)
	{
		return IData->InterpLength;
	}
	return (IData->EdSectionEnd);
}

/** Save record settings for next run */
void FMatinee::SaveRecordingSettings(const FCameraControllerConfig& InCameraConfig)
{
	GConfig->SetInt(TEXT("InterpEd.Recording"), TEXT("Mode"), RecordMode, GEditorPerProjectIni);

	GConfig->SetFloat(TEXT("InterpEd.Recording"), TEXT("TranslationSpeed"), InCameraConfig.TranslationMultiplier, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("InterpEd.Recording"), TEXT("RotationSpeed"), InCameraConfig.RotationMultiplier, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("InterpEd.Recording"), TEXT("ZoomSpeed"), InCameraConfig.ZoomMultiplier, GEditorPerProjectIni);

	GConfig->SetBool(TEXT("InterpEd.Recording"), TEXT("InvertX"), InCameraConfig.bInvertX, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("InterpEd.Recording"), TEXT("InvertY"), InCameraConfig.bInvertY, GEditorPerProjectIni);
	
	GConfig->SetInt(TEXT("InterpEd.Recording"), TEXT("RollSamples"), RecordRollSmoothingSamples, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("InterpEd.Recording"), TEXT("PitchSamples"), RecordPitchSmoothingSamples, GEditorPerProjectIni);

	GConfig->SetInt(TEXT("InterpEd.Recording"), TEXT("CameraMovement"), RecordCameraMovementScheme, GEditorPerProjectIni);

	FLevelEditorViewportClient* LevelVC = GetRecordingViewport();
	if (LevelVC)
	{
		GConfig->SetFloat(TEXT("InterpEd.Recording"), TEXT("ZoomDistance"), LevelVC->ViewFOV, GEditorPerProjectIni);
	}
}

/** Load record settings for next run */
void FMatinee::LoadRecordingSettings(FCameraControllerConfig& InCameraConfig)
{
	GConfig->GetInt(TEXT("InterpEd.Recording"), TEXT("Mode"), RecordMode, GEditorPerProjectIni);

	GConfig->GetFloat(TEXT("InterpEd.Recording"), TEXT("TranslationSpeed"), InCameraConfig.TranslationMultiplier, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("InterpEd.Recording"), TEXT("RotationSpeed"), InCameraConfig.RotationMultiplier, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("InterpEd.Recording"), TEXT("ZoomSpeed"), InCameraConfig.ZoomMultiplier, GEditorPerProjectIni);

	GConfig->GetBool(TEXT("InterpEd.Recording"), TEXT("InvertX"), InCameraConfig.bInvertX, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("InterpEd.Recording"), TEXT("InvertY"), InCameraConfig.bInvertY, GEditorPerProjectIni);

	GConfig->GetInt(TEXT("InterpEd.Recording"), TEXT("RollSamples"), RecordRollSmoothingSamples, GEditorPerProjectIni);
	GConfig->GetInt(TEXT("InterpEd.Recording"), TEXT("PitchSamples"), RecordPitchSmoothingSamples, GEditorPerProjectIni);

	GConfig->GetInt(TEXT("InterpEd.Recording"), TEXT("CameraMovement"), RecordCameraMovementScheme, GEditorPerProjectIni);

	FLevelEditorViewportClient* LevelVC = GetRecordingViewport();
	if (LevelVC)
	{
		GConfig->GetFloat(TEXT("InterpEd.Recording"), TEXT("ZoomDistance"), LevelVC->ViewFOV, GEditorPerProjectIni);
	}
}

/**
 * Access function to appropriate camera actor
 * @param InCameraIndex - The index of the camera actor to return
 * 
 */
ACameraActor* FMatinee::GetCameraActor(const int32 InCameraIndex)
{
	//quick early out
	if (InCameraIndex >= 0)
	{
		int32 CurrentCameraIndex = 0;
		for( TArray<UInterpGroupInst*>::TIterator GroupInstIt(MatineeActor->GroupInst); GroupInstIt; ++GroupInstIt )
		{
			UInterpGroupInst* Inst = *GroupInstIt;
			AActor* TempActor = Inst->GetGroupActor();
			if (TempActor)
			{
				ACameraActor* TempCameraActor = Cast<ACameraActor>(TempActor);
				if (TempCameraActor != NULL)
				{
					if (CurrentCameraIndex == InCameraIndex)
					{
						return TempCameraActor;
					}
					CurrentCameraIndex++;
				}
			}
		}
	}

	return NULL;
}

/**
 * Access function to return the number of used camera actors
 */
int32 FMatinee::GetNumCameraActors(void) const
{
	int32 CameraCount = 0;

	for( TArray<UInterpGroupInst*>::TIterator GroupInstIt(MatineeActor->GroupInst); GroupInstIt; ++GroupInstIt )
	{
		UInterpGroupInst* Inst = *GroupInstIt;
		AActor* TempActor = Inst->GetGroupActor();
		if (Cast<ACameraActor>(TempActor))
		{
			CameraCount++;
		}
	}
	return CameraCount;
}


void FMatinee::OnClose()
{
	// Safely stop recording if it is in progress
	if (IsRecordingInterpValues())
	{
		ToggleRecordInterpValues();
	}

	// Unregister call back events
	GEngine->OnActorMoved().Remove(OnActorMovedDelegateHandle);
	GEditor->OnObjectsReplaced().RemoveAll(this);

	// Restore the perspective viewport audio settings when matinee closes.
	SetAudioRealtimeOverride(false);

	// Re-instate regular transactor
	check( GEditor->Trans == InterpEdTrans );
	check( NormalTransactor->IsA( UTransBuffer::StaticClass() ) );

	GEditor->ResetTransaction( NSLOCTEXT("UnrealEd", "ExitMatinee", "Exit UnrealMatinee") );
	GEditor->Trans = NormalTransactor;

	// Detach editor camera from any group and clear any previewing stuff.
	LockCamToGroup(NULL);

	// Restore the saved state of any actors we were previewing interpolation on.
	for(int32 i=0; i<MatineeActor->GroupInst.Num(); i++)
	{
		// Restore Actors to the state they were in when we opened Matinee.
		MatineeActor->GroupInst[i]->RestoreGroupActorState();

		// Call TermTrackInst, but don't actually delete them. Leave them for GC.
		// Because we don't delete groups/tracks so undo works, we could end up deleting the Outer of a valid object.
		MatineeActor->GroupInst[i]->TermGroupInst(false);

		// Set any manipulated cameras back to default frustum colours.
		if (ACameraActor* Cam = Cast<ACameraActor>(MatineeActor->GroupInst[i]->GroupActor))
		{
			Cam->GetCameraComponent()->RestoreFrustumColor();
		}
	}

	// Restore the bHidden state of all actors with visibility tracks
	MatineeActor->RestoreActorVisibilities();

	// Movement tracks dont save/restore relative actor positions.  Instead, the MatineeActor
	// stores actor positions/orientations so they can be precisely restored on Matinee close.
	// Note that this must happen before MatineeActor's GroupInst array is emptied.
	MatineeActor->RestoreActorTransforms();

	DeselectAllGroups(false);
	DeselectAllTracks(false);

	MatineeActor->GroupInst.Empty();

	// Indicate action is no longer being edited.
	MatineeActor->bIsBeingEdited = false;

	// Reset interpolation to the beginning when quitting.
	MatineeActor->bIsPlaying = false;
	MatineeActor->InterpPosition = 0.f;

	Opt->bAdjustingKeyframe = false;
	Opt->bAdjustingGroupKeyframes = false;

	// When they close the window - change the mode away from InterpEdit.
	if( GLevelEditorModeTools().IsModeActive( FBuiltinEditorModes::EM_InterpEdit ) )
	{
		FEdModeInterpEdit* InterpEditMode = (FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );

		// Only change mode if this window closing wasn't instigated by someone changing mode!
		if( !InterpEditMode->bLeavingMode )
		{
			InterpEditMode->InterpEd = NULL;
			GLevelEditorModeTools().DeactivateMode( FBuiltinEditorModes::EM_InterpEdit );
		}
	}

	// Undo any weird settings to editor level viewports.
	for(int32 i=0; i<GEditor->LevelViewportClients.Num(); i++)
	{
		FLevelEditorViewportClient* LevelVC =GEditor->LevelViewportClients[i];
		if(LevelVC)
		{
			// Turn off realtime when exiting.
			if( LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview() )
			{				
				//Specify true so RestoreRealtime will allow us to disable Realtime if it was original disabled
				LevelVC->RestoreRealtime(true);
			}
		}
	}

	// Un-highlight selected track.
	if( HasATrackSelected() )
	{
		for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
		{
			IData->CurveEdSetup->ChangeCurveColor(*TrackIt, TrackIt.GetGroup()->GroupColor);
		}
	}

	// Make sure benchmarking mode is disabled (we may have turned it on for 'fixed time step playback')
	FApp::SetBenchmarking(false);

	// Update UI to reflect any change in realtime status
	FEditorSupportDelegates::UpdateUI.Broadcast();

	// Redraw viewport as well, to show reset state of stuff.
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();

	// Clear references to serialized members so they won't be serialized in the time
	// between the window closing and deletion.
	bClosed = true;
	MatineeActor = NULL;
	IData = NULL;
	NormalTransactor = NULL;
	Opt = NULL;
	CurveEd = NULL;
}

void FMatinee::DrawTracks3D(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	for(int32 i=0; i<MatineeActor->GroupInst.Num(); i++)
	{
		UInterpGroupInst* GrInst = MatineeActor->GroupInst[i];
		check( GrInst->Group );
		check( GrInst->TrackInst.Num() == GrInst->Group->InterpTracks.Num() );

		for(int32 j=0; j<GrInst->TrackInst.Num(); j++)
		{
			UInterpTrackInst* TrInst = GrInst->TrackInst[j];
			UInterpTrack* Track = GrInst->Group->InterpTracks[j];

			//don't draw disabled tracks
			if ( Track->IsDisabled() )
			{
				continue;
			}

			bool bTrackSelected = Track->IsSelected();
			FColor TrackColor = bTrackSelected ? Track3DSelectedColor : GrInst->Group->GroupColor;

			Track->Render3DTrack( TrInst, View, PDI, j, TrackColor, Opt->SelectedKeys);
		}
	}
}

/** 
 * Draws a line with a 1 pixel dark border around it
 * 
 * @param Canvas	The canvas to draw on
 * @param Start		The start of the line
 * @param End		The end of the line
 * @param bVertical	true if the line is vertical, false if horizontal
 */
static void DrawShadowedLine( FCanvas* Canvas, const FVector2D& Start, const FVector2D& End, bool bVertical )
{
	// This method uses DrawTile instead of DrawLine because DrawLine does not support alpha.
	if( bVertical )
	{
		Canvas->DrawTile( Start.X-1.0f, Start.Y, 1, Start.Y+End.Y-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(0.0,0.0f,0.0f,0.50f) );
		Canvas->DrawTile( Start.X, Start.Y, 1, Start.Y+End.Y, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(1.0,1.0f,1.0f,0.75f) );
		Canvas->DrawTile( Start.X+1.0f, Start.Y, 1, Start.Y+End.Y+1.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(0.0,0.0f,0.0f,0.50f) );
	}
	else
	{
		Canvas->DrawTile( Start.X, Start.Y-1.0f, Start.X+End.X-1.0f, 1, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(0.0,0.0f,0.0f,0.50f) );
		Canvas->DrawTile( Start.X, Start.Y, Start.X+End.X, 1, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(1.0,1.0f,1.0f,.75f) );
		Canvas->DrawTile( Start.X, Start.Y+1.0f, Start.X+End.X+1.0f, 1,  0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(0.0,0.0f,0.0f,0.50f) );
	}
}


/** 
 * Draws a line with alpha
 * 
 * @param Canvas	The canvas to draw on
 * @param Start		The start of the line
 * @param End		The end of the line
 * @param Alpha		The Alpha value to use
 * @param bVertical	true if the line is vertical, false if horizontal
 */
static void DrawTransparentLine( FCanvas* Canvas, const FVector2D& Start, const FVector2D& End, float Alpha, bool bVertical )
{
	// This method uses DrawTile instead of DrawLine because DrawLine does not support alpha.
	if( bVertical )
	{
		Canvas->DrawTile(  Start.X, Start.Y, 1, Start.Y+End.Y, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(1.0,1.0f,1.0f,Alpha) );
	}
	else
	{
		Canvas->DrawTile(  Start.X, Start.Y, Start.X+End.X, 1, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(1.0,1.0f,1.0f,Alpha) );
	}
}

void FMatinee::DrawModeHUD(FEditorViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas)
{
	if( ViewportClient->AllowsCinematicPreview() )
	{
		// Get the size of the viewport
		const int32 SizeX = Viewport->GetSizeXY().X;
		const int32 SizeY = Viewport->GetSizeXY().Y;

		if( IsEditingGridEnabled() )
		{
			// The main lines are rule of thirds lines so there should be 2 horizontal and vertical lines
			const uint32 NumLines = 2;

			// Calculate the step size 
			const float InvSize = 1.0f/3.0f;
			const float StepX = SizeX * InvSize;
			const float StepY = SizeY * InvSize;

			// Draw each line 
			for( uint32 Step = 1; Step <= NumLines; ++Step )
			{
				DrawShadowedLine( Canvas, FVector2D( StepX*Step, 0.0f ), FVector2D(StepX*Step, SizeY), true );
				DrawShadowedLine( Canvas, FVector2D( 0.0f, StepY*Step), FVector2D(SizeX, StepY*Step), false );
			}

			// Get the number of sub grid lines that should be drawn
			const int32 GridSize = GetEditingGridSize();
		
			// Do nothing if the user doesnt want to draw any lines
			if( GridSize > 1 )
			{
				// The size of each rule of thirds block
				const FVector2D BlockSize(StepX,StepY);

				// The number of sub lines to draw is the number of lines in each block times the number of horizontal and vertical blocks
				const uint32 NumRowsAndColumns = 6;
				uint32 NumSubLines = NumRowsAndColumns*(GridSize - 1);

				// Calculate the step size for each sub grid line 
				const float InvGridSize = 1.0f/GridSize;
				const float SubStepX = BlockSize.X * InvGridSize;
				const float SubStepY = BlockSize.Y * InvGridSize;

				// Draw each line
				for( uint32 Step = 1; Step <= NumSubLines; ++Step )
				{
					DrawTransparentLine( Canvas, FVector2D( SubStepX*Step, 0 ), FVector2D(SubStepX*Step, SizeY ), .15f, true );
					DrawTransparentLine( Canvas, FVector2D( 0, SubStepY*Step), FVector2D(SizeX, SubStepY*Step), .15f, false );
				}
			}
		}

		if( IsEditingCrosshairEnabled() )
		{
			// Get the center point for the crosshair, accounting for half pixel offset
			float CenterX = SizeX / 2.0f + .5f;
			float CenterY = SizeY / 2.0f + .5f;
			
			FCanvasLineItem LineItem;
			FVector2D Center(CenterX,CenterY);

			// Draw the line a line in X and Y extending out from the center.
			LineItem.SetColor( FLinearColor::Red );
			LineItem.Draw( Canvas, Center+FVector2D(-10.0f,0.0f), Center+FVector2D(10.0f,0.0f) );
			LineItem.Draw( Canvas, Center+FVector2D(0.0f,-10.0f), Center+FVector2D(0.0f,10.0f));
		}

		// If 'frame stats' are turned on and this viewport is configured for Matinee preview, then draw some text
		if( IsViewportFrameStatsEnabled() )
		{
			int32 XL, YL;
			int32 YPos = 23;
			int32 XPos = 5;

			// Title
			{
				FString StatsString = NSLOCTEXT("UnrealEd", "Matinee", "Matinee" ).ToString();
				Canvas->DrawShadowedString(  XPos, YPos, *StatsString, GEngine->GetLargeFont(), FLinearColor::White );
				StringSize( GEngine->GetLargeFont(), XL, YL, *StatsString );
				XPos += XL;
				XPos += 32;
			}

			// Viewport resolution
			{
				FString StatsString = FString::Printf( TEXT("%dx%d"), SizeX, SizeY );
				Canvas->DrawShadowedString(  XPos, YPos, *StatsString, GEngine->GetTinyFont(), FLinearColor( 0, 1, 1 ) );
				StringSize( GEngine->GetTinyFont(), XL, YL, *StatsString );
				YPos += YL;
			}

			// Frame counts
			{
				FString StatsString =
					FString::Printf(
						TEXT("%3.1f / %3.1f %s"),
						(1.0 / SnapAmount) * MatineeActor->InterpPosition,
						(1.0 / SnapAmount) * IData->InterpLength,
						*NSLOCTEXT("UnrealEd", "InterpEd_TimelineInfo_Frames", "frames").ToString() );
				Canvas->DrawShadowedString(  XPos, YPos, *StatsString, GEngine->GetTinyFont(), FLinearColor( 0, 1, 0 ) );
				StringSize( GEngine->GetTinyFont(), XL, YL, *StatsString );
				YPos += YL;
			}

			// SMTPE-style timecode
			if( bSnapToFrames )
			{
				FString StatsString = MakeTimecodeString( MatineeActor->InterpPosition );
				Canvas->DrawShadowedString(  XPos, YPos, *StatsString, GEngine->GetTinyFont(), FLinearColor( 1, 1, 0 ) );
				StringSize( GEngine->GetTinyFont(), XL, YL, *StatsString );
				YPos += YL;
			}
		}

		// Draw subtitles (toggle is handled internally)
		FVector2D MinPos(0.f, 0.f);
		FVector2D MaxPos(1.f, .9f);
		FIntRect SubtitleRegion(FMath::TruncToInt(SizeX * MinPos.X), FMath::TruncToInt(SizeY * MinPos.Y), FMath::TruncToInt(SizeX * MaxPos.X), FMath::TruncToInt(SizeY * MaxPos.Y));
		FSubtitleManager::GetSubtitleManager()->DisplaySubtitles( Canvas, SubtitleRegion, ViewportClient->GetWorld()->GetAudioTimeSeconds() );
	}
	// Camera Shot Names
	{
		TArray<UInterpTrack*> Results;
		UInterpGroupDirector* DirGroup = IData->FindDirectorGroup();
		if (DirGroup)
		{
			IData->FindTracksByClass( UInterpTrackDirector::StaticClass(), Results );
			for (int32 i=0; i < Results.Num(); i++)
			{
				if (!Results[i]->IsDisabled()) 
				{
					FString Name = Cast<UInterpTrackDirector>(Results[i])->GetViewedCameraShotName(MatineeActor->InterpPosition);
					if (Name != TEXT(""))
					{
						FString ShotNameString = FString::Printf(TEXT("[%s]"),*Name);
						int32 XL, YL;
						StringSize( GEngine->GetLargeFont(), XL, YL, *ShotNameString );
						int32 LeftXPos = 10;
						int32 RightXPos = Viewport->GetSizeXY().X - (XL + 10);
						int32 BottomYPos = Viewport->GetSizeXY().Y - (YL + 10);
						Canvas->DrawShadowedString(  RightXPos, BottomYPos, *ShotNameString, GEngine->GetLargeFont(), FLinearColor::White );
						
						FString CinemaNameString = FString::Printf(TEXT("[%s]"),*DirGroup->GroupName.ToString());
						Canvas->DrawShadowedString( LeftXPos, BottomYPos, *CinemaNameString, GEngine->GetLargeFont(), FLinearColor::White );
					}
				}
			}
		}
	}

	// Show a notification if we are adjusting a particular keyframe.
	if(Opt->bAdjustingKeyframe)
	{
		check(Opt->SelectedKeys.Num() == 1);

		FInterpEdSelKey& rSelKey = Opt->SelectedKeys[0];
		FString KeyTitle = FString::Printf( TEXT("%s%d"), ( rSelKey.Track ? *rSelKey.Track->TrackTitle : TEXT( "?" ) ), rSelKey.KeyIndex );
		FString AdjustNotify = FText::Format( NSLOCTEXT("UnrealEd", "AdjustKey_F", "ADJUST KEY {0}"), FText::FromString(KeyTitle) ).ToString();

		int32 XL, YL;
		StringSize(GEngine->GetLargeFont(), XL, YL, *AdjustNotify);
		Canvas->DrawShadowedString( 5, Viewport->GetSizeXY().Y - (3 + YL) , *AdjustNotify, GEngine->GetLargeFont(), FLinearColor( 1, 0, 0 ) );
	}
	else if(Opt->bAdjustingGroupKeyframes)
	{
		check(Opt->SelectedKeys.Num() > 1);

		// Make a list of all the unique subgroups within the selection, cache for fast lookup
		TArray<FString> UniqueSubGroupNames;
		TArray<FString> KeySubGroupNames;
		TArray<FString> KeyTitles;
		for( int32 iSelectedKey = 0; iSelectedKey < Opt->SelectedKeys.Num(); iSelectedKey++ )
		{
			FInterpEdSelKey& rSelKey = Opt->SelectedKeys[iSelectedKey];
			FString SubGroupName = rSelKey.GetOwningTrackSubGroupName();
			UniqueSubGroupNames.AddUnique( SubGroupName );
			KeySubGroupNames.Add( SubGroupName );
			FString KeyTitle = FString::Printf( TEXT("%s%d"), ( rSelKey.Track ? *rSelKey.Track->TrackTitle : TEXT( "?" ) ), rSelKey.KeyIndex );
			KeyTitles.Add( KeyTitle );
		}

		// Order the string in the format subgroup[tracktrack] subgroup[track]
		FString AdjustNotify( "AdjustKeys_F " );
		for( int32 iUSubGroupName = 0; iUSubGroupName < UniqueSubGroupNames.Num(); iUSubGroupName++ )
		{
			FString& rUniqueSubGroupName = UniqueSubGroupNames[iUSubGroupName];
			AdjustNotify += rUniqueSubGroupName;
			AdjustNotify += TEXT( "[" );
			for( int32 iKSubGroupName = 0; iKSubGroupName < KeySubGroupNames.Num(); iKSubGroupName++ )
			{
				FString& KeySubGroupName = KeySubGroupNames[ iKSubGroupName ];
				if ( rUniqueSubGroupName == KeySubGroupName )
				{
					AdjustNotify += KeyTitles[ iKSubGroupName ];
				}
			}
			AdjustNotify += TEXT( "] " );
		}

		int32 XL, YL;
		StringSize(GEngine->GetLargeFont(), XL, YL, *AdjustNotify);
		Canvas->DrawShadowedString( 5, Viewport->GetSizeXY().Y - (3 + YL) , *AdjustNotify, GEngine->GetLargeFont(), FLinearColor( 1, 0, 0 ) );
	}

	//Draw menu for real time track value recording
	if (ViewportClient->IsMatineeRecordingWindow() && bDisplayRecordingMenu)
	{
		//reset x position to left aligned
		int32 XL, YL;
		int32 XPos = 5;
		int32 ValueXPos = 450;
		int32 YPos = 50;
		FLinearColor ActiveMenuColor (1.0f, 1.0f, 0.0f);
		FLinearColor NormalMenuColor (1.0f, 1.0f, 1.0f);

		//if we're not actively recording
		if (RecordingState == MatineeConstants::ERecordingState::RECORDING_COMPLETE)
		{
			//display record menu item
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::ERecordMenu::RECORD_MENU_RECORD_MODE) ? ActiveMenuColor : NormalMenuColor;

			FString RecordTracksString = NSLOCTEXT("UnrealEd", "InterpEd_RecordMenu_RecordMode", "Record Mode").ToString();
			StringSize(GEngine->GetLargeFont(), XL, YL, *RecordTracksString);
			Canvas->DrawShadowedString( XPos, YPos , *RecordTracksString, GEngine->GetLargeFont(), DisplayColor );

			switch (RecordMode)
			{
			case MatineeConstants::ERecordMode::RECORD_MODE_NEW_CAMERA :
					RecordTracksString = NSLOCTEXT("UnrealEd", "InterpEd_RecordMode_NewCameraMode", "New Camera Mode").ToString();
					break;
				case MatineeConstants::ERecordMode::RECORD_MODE_NEW_CAMERA_ATTACHED:
					RecordTracksString = NSLOCTEXT("UnrealEd", "InterpEd_RecordMode_NewCameraAttachedMode", "New Attached Camera Mode").ToString();
					break;
				case MatineeConstants::ERecordMode::RECORD_MODE_DUPLICATE_TRACKS:
					RecordTracksString = NSLOCTEXT("UnrealEd", "InterpEd_RecordMode_DuplicateTracksMode", "Duplicate Selected Tracks").ToString();
					break;
				case MatineeConstants::ERecordMode::RECORD_MODE_REPLACE_TRACKS:
					RecordTracksString = NSLOCTEXT("UnrealEd", "InterpEd_RecordMode_ReplaceTracksMode", "Replace Selected Tracks").ToString();
					break;
				default:
					break;
			}
			StringSize(GEngine->GetLargeFont(), XL, YL, *RecordTracksString);
			Canvas->DrawShadowedString( ValueXPos, YPos , *RecordTracksString, GEngine->GetLargeFont(), DisplayColor );

			YPos += YL;
		}
		else
		{
			//Time since we began recording
			double CurrentTime = FPlatformTime::Seconds();
			double TimeSinceStateStart = (CurrentTime - RecordingStateStartTime);
			double SelectedRegionDuration = GetRecordingEndTime() - GetRecordingStartTime();

			FLinearColor DisplayColor(1.0, 1.0f, 0.0f);

			//draw recording state
			FString RecordingStateString;
			switch (RecordingState)
			{
				case MatineeConstants::ERecordingState::RECORDING_GET_READY_PAUSE:
					RecordingStateString = FText::Format(
						NSLOCTEXT("UnrealEd", "InterpEd_RecordingStateGetReadyPause", "Recording will begin in {0}"),
						FText::AsNumber(MatineeConstants::CountdownDurationInSeconds - TimeSinceStateStart) ).ToString();
					break;
				case MatineeConstants::ERecordingState::RECORDING_ACTIVE:
					RecordingStateString = FText::Format(
						NSLOCTEXT("UnrealEd", "InterpEd_RecordingStateActive", "Recording {0} / {1}"),
						FText::AsNumber(MatineeActor->InterpPosition - GetRecordingStartTime()), FText::AsNumber(SelectedRegionDuration) ).ToString();
					DisplayColor = FLinearColor(1.0f, 0.0f, 0.0f);
					break;
			}
			StringSize(GEngine->GetLargeFont(), XL, YL, *RecordingStateString);
			Canvas->DrawShadowedString( XPos, YPos , *RecordingStateString, GEngine->GetLargeFont(), DisplayColor );
			YPos += YL;
		}

		FEditorCameraController* CameraController = ViewportClient->GetCameraController();
		check(CameraController);
		FCameraControllerConfig& CameraConfig = CameraController->GetConfig();

		//display translation speed adjustment factor
		{
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::ERecordMenu::RECORD_MENU_TRANSLATION_SPEED) ? ActiveMenuColor : NormalMenuColor;

			FString TranslationSpeedString = NSLOCTEXT("UnrealEd", "InterpEd_RecordMenu_TranslationSpeedMultiplier", "Translation Speed").ToString();
			StringSize(GEngine->GetLargeFont(), XL, YL, *TranslationSpeedString);
			Canvas->DrawShadowedString( XPos, YPos , *TranslationSpeedString, GEngine->GetLargeFont(), DisplayColor );

			TranslationSpeedString = FString::Printf(TEXT("%f"), CameraConfig.TranslationMultiplier);
			StringSize(GEngine->GetLargeFont(), XL, YL, *TranslationSpeedString);
			Canvas->DrawShadowedString( ValueXPos, YPos , *TranslationSpeedString, GEngine->GetLargeFont(), DisplayColor );

			YPos += YL;
		}

		//display rotational speed adjustment factor
		{
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::ERecordMenu::RECORD_MENU_ROTATION_SPEED) ? ActiveMenuColor : NormalMenuColor;

			FString RotationSpeedString = NSLOCTEXT("UnrealEd", "InterpEd_RecordMenu_RotationSpeedMultiplier", "Rotation Speed").ToString();
			StringSize(GEngine->GetLargeFont(), XL, YL, *RotationSpeedString);
			Canvas->DrawShadowedString( XPos, YPos , *RotationSpeedString, GEngine->GetLargeFont(), DisplayColor );

			RotationSpeedString = FString::Printf(TEXT("%f"), CameraConfig.RotationMultiplier);
			StringSize(GEngine->GetLargeFont(), XL, YL, *RotationSpeedString);
			Canvas->DrawShadowedString( ValueXPos, YPos , *RotationSpeedString, GEngine->GetLargeFont(), DisplayColor );

			YPos += YL;
		}

		//display zoom speed adjustment factor
		{
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::ERecordMenu::RECORD_MENU_ZOOM_SPEED) ? ActiveMenuColor : NormalMenuColor;

			FString ZoomSpeedString = NSLOCTEXT("UnrealEd", "InterpEd_RecordMenu_ZoomSpeedMultiplier", "Zoom Speed").ToString();
			StringSize(GEngine->GetLargeFont(), XL, YL, *ZoomSpeedString );
			Canvas->DrawShadowedString( XPos, YPos , *ZoomSpeedString , GEngine->GetLargeFont(), DisplayColor );

			ZoomSpeedString = FString::Printf(TEXT("%f"), CameraConfig.ZoomMultiplier);
			StringSize(GEngine->GetLargeFont(), XL, YL, *ZoomSpeedString );
			Canvas->DrawShadowedString( ValueXPos, YPos , *ZoomSpeedString , GEngine->GetLargeFont(), DisplayColor );

			YPos += YL;
		}

		//Trim
		{
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::ERecordMenu::RECORD_MENU_TRIM) ? ActiveMenuColor : NormalMenuColor;

			FString TrimString = NSLOCTEXT("UnrealEd", "InterpEd_RecordMenu_Trim", "Trim").ToString();
			StringSize(GEngine->GetLargeFont(), XL, YL, *TrimString );
			Canvas->DrawShadowedString( XPos, YPos , *TrimString , GEngine->GetLargeFont(), DisplayColor );

			TrimString = FString::Printf(TEXT("%f"), CameraConfig.PitchTrim);
			StringSize(GEngine->GetLargeFont(), XL, YL, *TrimString );
			Canvas->DrawShadowedString( ValueXPos, YPos , *TrimString , GEngine->GetLargeFont(), DisplayColor );

			YPos += YL;
		}

		//Display Invert Mouse X & Mouse Y settings
		for (int32 i = 0; i < 2; ++i)
		{
			int32 SettingToCheck = (i==0) ? MatineeConstants::ERecordMenu::RECORD_MENU_INVERT_X_AXIS : MatineeConstants::ERecordMenu::RECORD_MENU_INVERT_Y_AXIS;
			FString InvertString = (i==0) ? NSLOCTEXT("UnrealEd", "InterpEd_RecordMenu_InvertXAxis", "Invert X Axis").ToString() : LOCTEXT("InterpEd_RecordMenu_InvertYAxis", "Invert Y Axis").ToString();
			bool InvertValue = (i==0) ? CameraConfig.bInvertX : CameraConfig.bInvertY;
			FString InvertValueString = (InvertValue ? NSLOCTEXT("UnrealEd", "Yes", "Yes").ToString() : LOCTEXT("No", "No").ToString());

			FLinearColor DisplayColor = (RecordMenuSelection == SettingToCheck) ? ActiveMenuColor : NormalMenuColor;

			StringSize(GEngine->GetLargeFont(), XL, YL, *InvertString );
			Canvas->DrawShadowedString( XPos, YPos , *InvertString , GEngine->GetLargeFont(), DisplayColor );

			StringSize(GEngine->GetLargeFont(), XL, YL, *InvertValueString );
			Canvas->DrawShadowedString( ValueXPos, YPos , *InvertValueString , GEngine->GetLargeFont(), DisplayColor );

			YPos += YL;
		}

		//display roll smoothing
		{
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::ERecordMenu::RECORD_MENU_ROLL_SMOOTHING) ? ActiveMenuColor : NormalMenuColor;

			FString RollSmoothingString = NSLOCTEXT("UnrealEd", "InterpEd_RecordMenu_RollSmoothing", "Roll Smoothing").ToString();
			StringSize(GEngine->GetLargeFont(), XL, YL, *RollSmoothingString);
			Canvas->DrawShadowedString( XPos, YPos , *RollSmoothingString, GEngine->GetLargeFont(), DisplayColor );

			
			FString RollSmoothingStateString = FString::Printf(TEXT("%d"), RecordRollSmoothingSamples);
			StringSize(GEngine->GetLargeFont(), XL, YL, *RollSmoothingStateString);
			Canvas->DrawShadowedString( ValueXPos, YPos , *RollSmoothingStateString, GEngine->GetLargeFont(), DisplayColor );

			YPos += YL;
		}

		//display roll smoothing
		{
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::ERecordMenu::RECORD_MENU_PITCH_SMOOTHING) ? ActiveMenuColor : NormalMenuColor;

			FString PitchSmoothingString = NSLOCTEXT("UnrealEd", "InterpEd_RecordMenu_PitchSmoothing", "Pitch Smoothing").ToString();
			StringSize(GEngine->GetLargeFont(), XL, YL, *PitchSmoothingString);
			Canvas->DrawShadowedString( XPos, YPos , *PitchSmoothingString, GEngine->GetLargeFont(), DisplayColor );


			FString PitchSmoothingStateString = FString::Printf(TEXT("%d"), RecordPitchSmoothingSamples);
			StringSize(GEngine->GetLargeFont(), XL, YL, *PitchSmoothingStateString);
			Canvas->DrawShadowedString( ValueXPos, YPos , *PitchSmoothingStateString, GEngine->GetLargeFont(), DisplayColor );

			YPos += YL;
		}

		//display roll smoothing
		{
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::ERecordMenu::RECORD_MENU_CAMERA_MOVEMENT_SCHEME) ? ActiveMenuColor : NormalMenuColor;

			FString CameraMovmentString = NSLOCTEXT("UnrealEd", "InterpEd_RecordMenu_CameraMovementScheme", "Camera Movement").ToString();
			StringSize(GEngine->GetLargeFont(), XL, YL, *CameraMovmentString);
			Canvas->DrawShadowedString( XPos, YPos , *CameraMovmentString, GEngine->GetLargeFont(), DisplayColor );

			FString CameraMovmentStateString;
			switch (RecordCameraMovementScheme)
			{
			case MatineeConstants::ECameraScheme::CAMERA_SCHEME_FREE_CAM:
				CameraMovmentStateString = NSLOCTEXT("UnrealEd", "InterpEd_RecordMenu_CameraMovementScheme_FreeCam", "Free Camera").ToString();
				break;
			case MatineeConstants::ECameraScheme::CAMERA_SCHEME_PLANAR_CAM:
				CameraMovmentStateString = NSLOCTEXT("UnrealEd", "InterpEd_RecordMenu_CameraMovementScheme_PlanarCam", "Planar Camera").ToString();
				break;
			}
			StringSize(GEngine->GetLargeFont(), XL, YL, *CameraMovmentStateString);
			Canvas->DrawShadowedString( ValueXPos, YPos , *CameraMovmentStateString, GEngine->GetLargeFont(), DisplayColor );

			YPos += YL;
		}

		//give some space before giving the live stats
		YPos += 20;
		//display current zoom distance
		{
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::ERecordMenu::RECORD_MENU_ZOOM_DISTANCE) ? ActiveMenuColor : NormalMenuColor;

			FString ZoomDistanceString = NSLOCTEXT("UnrealEd", "InterpEd_RecordMenu_ZoomDistance", "Zoom Distance").ToString();
			StringSize(GEngine->GetLargeFont(), XL, YL, *ZoomDistanceString );
			Canvas->DrawShadowedString( XPos, YPos , *ZoomDistanceString , GEngine->GetLargeFont(), DisplayColor );

			ZoomDistanceString = FString::Printf(TEXT("%f"), ViewportClient->ViewFOV);
			StringSize(GEngine->GetLargeFont(), XL, YL, *ZoomDistanceString );
			Canvas->DrawShadowedString( ValueXPos, YPos , *ZoomDistanceString , GEngine->GetLargeFont(), DisplayColor );

			YPos += YL;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////
// Properties window NotifyHook stuff
void FMatinee::NotifyPreChange( UProperty* PropertyAboutToChange )
{

}

void FMatinee::NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged )
{
	CurveEd->CurveChanged();

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();

	// If we are changing the properties of a Group, propagate changes to the GroupAnimSets array to the Actors being controlled by this group.
	for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		UInterpGroup* CurrentSelectedGroup = *GroupIt;

		if( CurrentSelectedGroup->HasAnimControlTrack() )
		{
			for( TArray<UInterpGroupInst*>::TIterator GroupInstIt(MatineeActor->GroupInst); GroupInstIt; ++GroupInstIt )
			{
				UInterpGroupInst* Inst = *GroupInstIt;

				if( CurrentSelectedGroup == Inst->Group  )
				{
					AActor* Actor = Inst->GetGroupActor();
					if(Actor)
					{
						IMatineeAnimInterface * MatineeAnimInterface = Cast<IMatineeAnimInterface>(Actor);
						if (MatineeAnimInterface)
						{
							MatineeAnimInterface->PreviewBeginAnimControl(CurrentSelectedGroup);
						}
					}
				}
			}

			// Update to current position - so changes in AnimSets take affect now.
			RefreshInterpPosition();
		}
	}
}

////////////////////////////////
// FCallbackEventDevice interface

void FMatinee::OnActorMoved(AActor* InObject)
{
	if ( MatineeActor == NULL )
	{
		return;
	}
}

/**
 * Event handler for when objects are replaced, allows us to fix any references that
 * aren't automatically hooked up
 */

void FMatinee::OnObjectsReplaced(const TMap<UObject*,UObject*>& ReplacementMap)
{
	MatineeActor->OnObjectsReplaced(ReplacementMap);
}

/**
 * Either shows or hides the director track window by splitting/unsplitting the parent window
 */
EVisibility FMatinee::GetDirectorTrackWindowVisibility() const
{
	// Do we have a director group?  If so, then the director track window will be implicitly visible!
	UInterpGroupDirector* DirGroup = IData->FindDirectorGroup();
	bool bWantDirectorTrackWindow = ( DirGroup != NULL );

	//Show the director tab
	if (bWantDirectorTrackWindow)
	{
		return EVisibility::Visible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

/**
 * Locates the specified group's parent group folder, if it has one
 *
 * @param ChildGroup The group who's parent we should search for
 *
 * @return Returns the parent group pointer or NULL if one wasn't found
 */
UInterpGroup* FMatinee::FindParentGroupFolder( UInterpGroup* ChildGroup ) const
{
	// Does this group even have a parent?
	if( ChildGroup->bIsParented )
	{
		check( !ChildGroup->bIsFolder );

		// Find the child group list index
		int32 ChildGroupIndex = -1;
		if( IData->InterpGroups.Find( ChildGroup, ChildGroupIndex ) )
		{
			// Iterate backwards in the group list starting at the child group index, looking for its parent
			for( int32 CurGroupIndex = ChildGroupIndex - 1; CurGroupIndex >= 0; --CurGroupIndex )
			{
				UInterpGroup* CurGroup = IData->InterpGroups[ CurGroupIndex ];

				// Just skip the director group if we find it; it's not allowed to be a parent
				if( !CurGroup->IsA( UInterpGroupDirector::StaticClass() ) )
				{
					// Is the current group a top level folder?
					if( !CurGroup->bIsParented )
					{
						check( CurGroup->bIsFolder );

						// Found it!
						return CurGroup;
					}
				}
			}
		}
	}

	// Not found
	return NULL;
}


/**
 * Counts the number of children that the specified group folder has
 *
 * @param GroupFolder The group who's children we should count
 *
 * @return Returns the number of child groups
 */
int32 FMatinee::CountGroupFolderChildren( UInterpGroup* const GroupFolder ) const
{
	int32 ChildCount = 0;

	// Child groups currently don't support containing their own children
	if( GroupFolder->bIsFolder && !GroupFolder->bIsParented )
	{
		int32 StartIndex = IData->InterpGroups.Find( GroupFolder ) + 1;
		for( int32 CurGroupIndex = StartIndex; CurGroupIndex < IData->InterpGroups.Num(); ++CurGroupIndex	)
		{
			UInterpGroup* CurGroup = IData->InterpGroups[ CurGroupIndex ];

			// Children always appear sequentially after their parent in the array, so if we find an unparented item, then 
			// we know we've reached the last child
			if( CurGroup->bIsParented )
			{
				// Found a child!
				++ChildCount;
			}
			else
			{
				// No more children
				break;
			}
		}
	}

	return ChildCount;
}

/**
* @param	InGroup	The group to check if its a parent or has a parent. 
* @return	A structure containing information about the given group's parent relationship.
*/
FInterpGroupParentInfo FMatinee::GetParentInfo( UInterpGroup* InGroup ) const
{
	check(InGroup);

	FInterpGroupParentInfo Info(InGroup);

	Info.Parent			= FindParentGroupFolder(InGroup);
	Info.GroupIndex		= IData->InterpGroups.Find(InGroup);
	Info.bHasChildren	= !!CountGroupFolderChildren(InGroup);

	return Info;
}

/**
 * Determines if the child candidate can be parented (or re-parented) by the parent candiddate.
 *
 * @param	ChildCandidate	The group that desires to become the child to the parent candidate.
 * @param	ParentCandidate	The group that, if a folder, desires to parent the child candidate.
 *
 * @return	true if the parent candidate can parent the child candidate. 
 */
bool FMatinee::CanReparent( const FInterpGroupParentInfo& ChildCandidate, const FInterpGroupParentInfo& ParentCandidate ) const
{
	// Can re-parent if both groups are the same!
	if( ParentCandidate.Group == ChildCandidate.Group )
	{
		return false;
	}

	const UClass* DirectorClass = UInterpGroupDirector::StaticClass();

	// Neither group can be a director
	if( ParentCandidate.Group->IsA(DirectorClass) || ChildCandidate.Group->IsA(DirectorClass) )
	{
		return false;
	}

	// We can't allow the user to re-parent groups that already have children, 
	// since we currently don't support multi-level nesting.
	if( ChildCandidate.IsAParent() )
	{
		return false;
	}

	// The group candidate can't be a folder because we don't support folders 
	// parenting folders. This is similar to the multi-level parent nesting. 
	if( ChildCandidate.Group->bIsFolder )
	{
		return false;
	}

	// The folder candidate must be a folder, obviously.
	if( !ParentCandidate.Group->bIsFolder )
	{
		return false;
	}

	// The parent candidate can't already be a parent to the child.
	if( ChildCandidate.IsParent(ParentCandidate) )
	{
		return false;
	}

	// At this point we verified the folder candidate is actually a folder. 
	check( !ParentCandidate.HasAParent() );

	return true;
}


/**
 * Fixes up any problems in the folder/group hierarchy caused by bad parenting in previous builds
 */
void FMatinee::RepairHierarchyProblems()
{
	bool bAnyRepairsMade = false;

	bool bPreviousGroupWasFolder = false;
	bool bPreviousGroupWasParented = false;

	for( int32 CurGroupIndex = 0; CurGroupIndex < IData->InterpGroups.Num(); ++CurGroupIndex )
	{
		UInterpGroup* CurGroup = IData->InterpGroups[ CurGroupIndex ];
		if( CurGroup != NULL )
		{
			if( CurGroup->bIsFolder )
			{
				// This is a folder group.
				
				// Folders are never allowed to be parented
				if( CurGroup->bIsParented )
				{
					// Repair parenting problem
					CurGroup->bIsParented = false;
					bAnyRepairsMade = true;
				}
			}
			else if( CurGroup->bIsParented )
			{
				// This group is parented to a folder
				
				// Make sure the previous group in the list was either a folder OR a parented group
				if( !bPreviousGroupWasFolder && !bPreviousGroupWasParented )
				{
					// Uh oh, the current group thinks its parented but the previous item is not a folder
					// or another parented group.  This means the current group thinks its parented to
					// another root group.  No good!  We'll unparent the group to fix this.
					CurGroup->bIsParented = false;
					bAnyRepairsMade = true;
				}
			}

			// If this is a 'director group', its never allowed to be parented (or act as a folder)
			if( CurGroup->IsA( UInterpGroupDirector::StaticClass() ) )
			{
				if( CurGroup->bIsParented )
				{
					// Director groups cannot be parented
					CurGroup->bIsParented = false;
					bAnyRepairsMade = true;
				}

				if( CurGroup->bIsFolder )
				{
					// Director groups cannot act as a folder
					CurGroup->bIsFolder = false;
					bAnyRepairsMade = true;
				}
			}

			// Keep track of this group's status for the next iteration's tests
			bPreviousGroupWasFolder	 = CurGroup->bIsFolder;
			bPreviousGroupWasParented = CurGroup->bIsParented;
		}
		else
		{
			// Bad group pointer, so remove this element from the list
			IData->InterpGroups.RemoveAt( 0 );
			--CurGroupIndex;
			bAnyRepairsMade = true;
		}
	}


	if( bAnyRepairsMade )
	{
		// Dirty the package so that editor changes will be saved
		IData->MarkPackageDirty();

		// Notify the user
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "InterpEd_HierachyRepairsNotification", "Warning:  Problems were detected with the organizational data in this Matinee sequence and repairs have been made.  Some groups may have been removed from the folders they were previously in.  No changes were made to the actual Matinee sequence data.  You should resave the level to make these repairs permanent." ) );
	}
}

/**
 * Return the specified localized InterpEd FPS Snap size name
 */
FString FMatinee::GetInterpEdFPSSnapSizeLocName( int32 StringIndex )
{
	check( StringIndex >= 0 && StringIndex < ARRAY_COUNT( InterpEdFPSSnapSizes ) );

	switch( StringIndex )
	{
	case 0:
		return NSLOCTEXT("UnrealEd", "InterpEd_FrameRate_15_fps", "15 fps" ).ToString();
	case 1:
		return NSLOCTEXT("UnrealEd", "InterpEd_FrameRate_24_fps", "24 fps (film)" ).ToString();
	case 2:
		return NSLOCTEXT("UnrealEd", "InterpEd_FrameRate_25_fps", "25 fps (PAL/25)" ).ToString();
	case 3:
		return NSLOCTEXT("UnrealEd", "InterpEd_FrameRate_29_97_fps", "29.97 fps (NTSC/30)" ).ToString();
	case 4:
		return NSLOCTEXT("UnrealEd", "InterpEd_FrameRate_30_fps", "30 fps" ).ToString();
	case 5:
		return NSLOCTEXT("UnrealEd", "InterpEd_FrameRate_50_fps", "50 fps (PAL/50)" ).ToString();
	case 6:
		return NSLOCTEXT("UnrealEd", "InterpEd_FrameRate_59_94_fps", "59.94 fps (NTSC/60)" ).ToString();
	case 7:
		return NSLOCTEXT("UnrealEd", "InterpEd_FrameRate_60_fps", "60 fps" ).ToString();
	case 8:
		return NSLOCTEXT("UnrealEd", "InterpEd_FrameRate_120_fps", "120 fps" ).ToString();
	}

	return TEXT( "" );
}

bool FMatinee::IsCameraAnim() const
{
	return MatineeActor->IsA(AMatineeActorCameraAnim::StaticClass());
}

void FMatinee::OnMenuCreateMovie()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

	// Create a new movie scene capture object for a generic level capture
	ULevelCapture* MovieSceneCapture = NewObject<ULevelCapture>(GetTransientPackage(), ULevelCapture::StaticClass(), NAME_None, RF_Transient);
	MovieSceneCapture->LoadFromConfig();

	// Ensure that this matinee is up and running before we start capturing
	MovieSceneCapture->SetPrerequisiteActor(MatineeActor);

	IMovieSceneCaptureDialogModule::Get().OpenDialog(LevelEditorModule.GetLevelEditorTabManager().ToSharedRef(), MovieSceneCapture);
}

#undef LOCTEXT_NAMESPACE
