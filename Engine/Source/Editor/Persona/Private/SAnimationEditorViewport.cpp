// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SAnimationEditorViewport.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Animation/AnimMontage.h"
#include "Preferences/PersonaOptions.h"

#include "SAnimationScrubPanel.h"
#include "SAnimMontageScrubPanel.h"
#include "SAnimViewportToolBar.h"
#include "AnimViewportMenuCommands.h"
#include "AnimViewportShowCommands.h"
#include "AnimViewportLODCommands.h"
#include "AnimViewportPlaybackCommands.h"
#include "AnimPreviewInstance.h"
#include "Widgets/Input/STextComboBox.h"
#include "IEditableSkeleton.h"
#include "EditorViewportCommands.h"
#include "TabSpawners.h"
#include "SkeletalMeshTypes.h"

#define LOCTEXT_NAMESPACE "PersonaViewportToolbar"

//////////////////////////////////////////////////////////////////////////
// SAnimationEditorViewport

void SAnimationEditorViewport::Construct(const FArguments& InArgs, const FAnimationEditorViewportRequiredArgs& InRequiredArgs)
{
	SkeletonTreePtr = InRequiredArgs.SkeletonTree;
	PreviewScenePtr = InRequiredArgs.PreviewScene;
	TabBodyPtr = InRequiredArgs.TabBody;
	AssetEditorToolkitPtr = InRequiredArgs.AssetEditorToolkit;
	Extenders = InArgs._Extenders;
	bShowShowMenu = InArgs._ShowShowMenu;
	bShowLODMenu = InArgs._ShowLODMenu;
	bShowPlaySpeedMenu = InArgs._ShowPlaySpeedMenu;
	bShowStats = InArgs._ShowStats;
	bShowFloorOptions = InArgs._ShowFloorOptions;
	bShowTurnTable = InArgs._ShowTurnTable;
	bShowPhysicsMenu = InArgs._ShowPhysicsMenu;
	InRequiredArgs.OnPostUndo.Add(FSimpleDelegate::CreateSP(this, &SAnimationEditorViewport::OnUndoRedo));

	SEditorViewport::Construct(
		SEditorViewport::FArguments()
			.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
			.AddMetaData<FTagMetaData>(TEXT("Persona.Viewport"))
		);

	Client->VisibilityDelegate.BindSP(this, &SAnimationEditorViewport::IsVisible);
}

TSharedRef<FEditorViewportClient> SAnimationEditorViewport::MakeEditorViewportClient()
{
	// Create an animation viewport client
	LevelViewportClient = MakeShareable(new FAnimationViewportClient(SkeletonTreePtr.Pin().ToSharedRef(), PreviewScenePtr.Pin().ToSharedRef(), SharedThis(this), AssetEditorToolkitPtr.Pin().ToSharedRef(), bShowStats));

	LevelViewportClient->ViewportType = LVT_Perspective;
	LevelViewportClient->bSetListenerPosition = false;
	LevelViewportClient->SetViewLocation(EditorViewportDefs::DefaultPerspectiveViewLocation);
	LevelViewportClient->SetViewRotation(EditorViewportDefs::DefaultPerspectiveViewRotation);

	return LevelViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SAnimationEditorViewport::MakeViewportToolbar()
{
	return SNew(SAnimViewportToolBar, TabBodyPtr.Pin(), SharedThis(this))
		.Cursor(EMouseCursor::Default)
		.Extenders(Extenders)
		.ShowShowMenu(bShowShowMenu)
		.ShowLODMenu(bShowLODMenu)
		.ShowPlaySpeedMenu(bShowPlaySpeedMenu)
		.ShowFloorOptions(bShowFloorOptions)
		.ShowTurnTable(bShowTurnTable)
		.ShowPhysicsMenu(bShowPhysicsMenu);
}

void SAnimationEditorViewport::OnUndoRedo()
{
	LevelViewportClient->Invalidate();
}

void SAnimationEditorViewport::OnFocusViewportToSelection()
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->FocusViewportOnPreviewMesh(false);
}

//////////////////////////////////////////////////////////////////////////
// SAnimationEditorViewportTabBody

SAnimationEditorViewportTabBody::SAnimationEditorViewportTabBody()
	: SelectedTurnTableSpeed(EAnimationPlaybackSpeeds::Normal)
	, SelectedTurnTableMode(EPersonaTurnTableMode::Stopped)
{
}

SAnimationEditorViewportTabBody::~SAnimationEditorViewportTabBody()
{
	// Close viewport
	if (LevelViewportClient.IsValid())
	{
		LevelViewportClient->Viewport = NULL;
	}

	// Release our reference to the viewport client
	LevelViewportClient.Reset();
}

bool SAnimationEditorViewportTabBody::CanUseGizmos() const
{
	if (bAlwaysShowTransformToolbar)
	{
		return true;
	}

	class UDebugSkelMeshComponent* Component = GetPreviewScene()->GetPreviewMeshComponent();

	if (Component != NULL)
	{
		if (Component->bForceRefpose)
		{
			return false;
		}
		else if (Component->IsPreviewOn())
		{
			return true;
		}
	}

	return false;
}

FText SAnimationEditorViewportTabBody::GetDisplayString() const
{
	class UDebugSkelMeshComponent* Component = GetPreviewScene()->GetPreviewMeshComponent();
	FName TargetSkeletonName = GetSkeletonTree()->GetEditableSkeleton()->GetSkeleton().GetFName();

	if (Component != NULL)
	{
		if (Component->bForceRefpose)
		{
			return LOCTEXT("ReferencePose", "Reference pose");
		}
		else if (Component->IsPreviewOn())
		{
			return FText::Format(LOCTEXT("Previewing", "Previewing {0}"), FText::FromString(Component->GetPreviewText()));
		}
		else if (Component->AnimClass != NULL)
		{
			const bool bWarnAboutBoneManip = BlueprintEditorPtr.Pin()->IsModeCurrent(FPersonaModes::AnimBlueprintEditMode);
			if (bWarnAboutBoneManip)
			{
				return FText::Format(LOCTEXT("PreviewingAnimBP_WarnDisabled", "Previewing {0}. \nBone manipulation is disabled in this mode. "), FText::FromString(Component->AnimClass->GetName()));
			}
			else
			{
				return FText::Format(LOCTEXT("PreviewingAnimBP", "Previewing {0}"), FText::FromString(Component->AnimClass->GetName()));
			}
		}
		else if (Component->SkeletalMesh == NULL)
		{
			return FText::Format(LOCTEXT("NoMeshFound", "No skeletal mesh found for skeleton '{0}'"), FText::FromName(TargetSkeletonName));
		}
	}

	return FText();
}

TSharedRef<IPersonaViewportState> SAnimationEditorViewportTabBody::SaveState() const
{
	TSharedRef<FPersonaModeSharedData> State = MakeShareable(new(FPersonaModeSharedData));
	State->Save(StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef()));
	return State;
}

void SAnimationEditorViewportTabBody::RestoreState(TSharedRef<IPersonaViewportState> InState)
{
	TSharedRef<FPersonaModeSharedData> State = StaticCastSharedRef<FPersonaModeSharedData>(InState);
	State->Restore(StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef()));
}

FEditorViewportClient& SAnimationEditorViewportTabBody::GetViewportClient() const
{
	return *LevelViewportClient;
}

void SAnimationEditorViewportTabBody::RefreshViewport()
{
	LevelViewportClient->Invalidate();
}

bool SAnimationEditorViewportTabBody::IsVisible() const
{
	return ViewportWidget.IsValid();
}

FReply SAnimationEditorViewportTabBody::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (UICommandList.IsValid() && UICommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}


void SAnimationEditorViewportTabBody::Construct(const FArguments& InArgs, const TSharedRef<class ISkeletonTree>& InSkeletonTree, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, const TSharedRef<class FAssetEditorToolkit>& InAssetEditorToolkit, FSimpleMulticastDelegate& InOnUndoRedo)
{
	UICommandList = MakeShareable(new FUICommandList);

	SkeletonTreePtr = InSkeletonTree;
	PreviewScenePtr = StaticCastSharedRef<FAnimationEditorPreviewScene>(InPreviewScene);
	AssetEditorToolkitPtr = InAssetEditorToolkit;
	BlueprintEditorPtr = InArgs._BlueprintEditor;
	bShowTimeline = InArgs._ShowTimeline;
	bAlwaysShowTransformToolbar = InArgs._AlwaysShowTransformToolbar;
	OnInvokeTab = InArgs._OnInvokeTab;

	// register delegates for change notifications
	InPreviewScene->RegisterOnAnimChanged(FOnAnimChanged::CreateSP(this, &SAnimationEditorViewportTabBody::AnimChanged));
	InPreviewScene->RegisterOnPreviewMeshChanged(FOnPreviewMeshChanged::CreateSP(this, &SAnimationEditorViewportTabBody::HandlePreviewMeshChanged));

	const FSlateFontInfo SmallLayoutFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10 );

	FAnimViewportMenuCommands::Register();
	FAnimViewportShowCommands::Register();
	FAnimViewportLODCommands::Register();
	FAnimViewportPlaybackCommands::Register();

	// Build toolbar widgets
	UVChannelCombo = SNew(STextComboBox)
		.OptionsSource(&UVChannels)
		.OnSelectionChanged(this, &SAnimationEditorViewportTabBody::ComboBoxSelectionChanged);

	FAnimationEditorViewportRequiredArgs ViewportArgs(InSkeletonTree, InPreviewScene, SharedThis(this), InAssetEditorToolkit, InOnUndoRedo);

	ViewportWidget = SNew(SAnimationEditorViewport, ViewportArgs)
		.Extenders(InArgs._Extenders)
		.ShowShowMenu(InArgs._ShowShowMenu)
		.ShowLODMenu(InArgs._ShowLODMenu)
		.ShowPlaySpeedMenu(InArgs._ShowPlaySpeedMenu)
		.ShowStats(InArgs._ShowStats)
		.ShowFloorOptions(InArgs._ShowFloorOptions)
		.ShowTurnTable(InArgs._ShowTurnTable)
		.ShowPhysicsMenu(InArgs._ShowPhysicsMenu);

	TSharedPtr<SVerticalBox> ViewportContainer = nullptr;
	this->ChildSlot
	[
		SAssignNew(ViewportContainer, SVerticalBox)

		// Build our toolbar level toolbar
		+SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew(SOverlay)

			// The viewport
			+SOverlay::Slot()
			[
				ViewportWidget.ToSharedRef()
			]

			// The 'dirty/in-error' indicator text in the bottom-right corner
			+SOverlay::Slot()
			.Padding(8)
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Right)
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.Visibility(this, &SAnimationEditorViewportTabBody::GetViewportCornerTextVisibility)
				.OnClicked(this, &SAnimationEditorViewportTabBody::ClickedOnViewportCornerText)
				.Content()
				[
					SNew(STextBlock)
					.TextStyle(FEditorStyle::Get(), "Persona.Viewport.BlueprintDirtyText")
					.Text(this, &SAnimationEditorViewportTabBody::GetViewportCornerText)
					.ToolTipText(this, &SAnimationEditorViewportTabBody::GetViewportCornerTooltip)
				]
			]
		]
	];

	if(bShowTimeline && ViewportContainer.IsValid())
	{
		ViewportContainer->AddSlot()
		.AutoHeight()
		[
			SAssignNew(ScrubPanelContainer, SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SAnimationScrubPanel, GetPreviewScene())
				.ViewInputMin(this, &SAnimationEditorViewportTabBody::GetViewMinInput)
				.ViewInputMax(this, &SAnimationEditorViewportTabBody::GetViewMaxInput)
				.bAllowZoom(true)
			]
		];

		UpdateScrubPanel(InPreviewScene->GetPreviewAnimationAsset());
	}

	LevelViewportClient = ViewportWidget->GetViewportClient();

	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());

	// Load the view mode from config
	AnimViewportClient->SetViewMode(AnimViewportClient->ConfigOption->ViewModeIndex);
	UpdateShowFlagForMeshEdges();


	OnSetTurnTableMode(SelectedTurnTableMode);
	OnSetTurnTableSpeed(SelectedTurnTableSpeed);

	BindCommands();

	PopulateNumUVChannels();
}

void SAnimationEditorViewportTabBody::BindCommands()
{
	FUICommandList& CommandList = *UICommandList;

	//Bind menu commands
	const FAnimViewportMenuCommands& MenuActions = FAnimViewportMenuCommands::Get();

	CommandList.MapAction(
		MenuActions.CameraFollow,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::ToggleCameraFollow),
		FCanExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::CanChangeCameraMode),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsCameraFollowEnabled));

	CommandList.MapAction(
		MenuActions.JumpToDefaultCamera,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::JumpToDefaultCamera),
		FCanExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::HasDefaultCameraSet));

	CommandList.MapAction(
		MenuActions.SaveCameraAsDefault,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::SaveCameraAsDefault));

	CommandList.MapAction(
		MenuActions.ClearDefaultCamera,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::ClearDefaultCamera),
		FCanExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::HasDefaultCameraSet));

	CommandList.MapAction(
		MenuActions.PreviewSceneSettings,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OpenPreviewSceneSettings));

	TSharedRef<FAnimationViewportClient> EditorViewportClientRef = GetAnimationViewportClient();

	CommandList.MapAction(
		MenuActions.SetCPUSkinning,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FAnimationViewportClient::ToggleCPUSkinning),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FAnimationViewportClient::IsSetCPUSkinningChecked));

	CommandList.MapAction(
		MenuActions.SetShowNormals,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FAnimationViewportClient::ToggleShowNormals ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FAnimationViewportClient::IsSetShowNormalsChecked ) );

	CommandList.MapAction(
		MenuActions.SetShowTangents,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FAnimationViewportClient::ToggleShowTangents ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FAnimationViewportClient::IsSetShowTangentsChecked ) );

	CommandList.MapAction(
		MenuActions.SetShowBinormals,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FAnimationViewportClient::ToggleShowBinormals ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FAnimationViewportClient::IsSetShowBinormalsChecked ) );

	CommandList.MapAction(
		MenuActions.AnimSetDrawUVs,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FAnimationViewportClient::ToggleDrawUVOverlay ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FAnimationViewportClient::IsSetDrawUVOverlayChecked ) );

	//Bind Show commands
	const FAnimViewportShowCommands& ViewportShowMenuCommands = FAnimViewportShowCommands::Get();

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowRetargetBasePose,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::ShowRetargetBasePose),
		FCanExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::CanShowRetargetBasePose),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowRetargetBasePoseEnabled));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowBound,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::ShowBound),
		FCanExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::CanShowBound),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowBoundEnabled));

	CommandList.MapAction(
		ViewportShowMenuCommands.UseInGameBound,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::UseInGameBound),
		FCanExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::CanUseInGameBound),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsUsingInGameBound));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowPreviewMesh,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::ToggleShowPreviewMesh),
		FCanExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::CanShowPreviewMesh),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowPreviewMeshEnabled));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowMorphTargets,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowMorphTargets),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingMorphTargets));

	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowBoneNames,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowBoneNames),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingBoneNames));

	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowRawAnimation,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowRawAnimation),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingRawAnimation));

	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowNonRetargetedAnimation,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowNonRetargetedAnimation),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingNonRetargetedPose));

	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowAdditiveBaseBones,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowAdditiveBase),
		FCanExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::IsPreviewingAnimation),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingAdditiveBase));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowSourceRawAnimation,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowSourceRawAnimation),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingSourceRawAnimation));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowBakedAnimation,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowBakedAnimation),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingBakedAnimation));

	//Display info
	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowDisplayInfoBasic,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowDisplayInfo, (int32)EDisplayInfoMode::Basic),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingMeshInfo, (int32)EDisplayInfoMode::Basic));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowDisplayInfoDetailed,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowDisplayInfo, (int32)EDisplayInfoMode::Detailed),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingMeshInfo, (int32)EDisplayInfoMode::Detailed));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowDisplayInfoSkelControls,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowDisplayInfo, (int32)EDisplayInfoMode::SkeletalControls),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingMeshInfo, (int32)EDisplayInfoMode::SkeletalControls));

	CommandList.MapAction(
		ViewportShowMenuCommands.HideDisplayInfo,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowDisplayInfo, (int32)EDisplayInfoMode::None),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingMeshInfo, (int32)EDisplayInfoMode::None));

	//Material overlay option
	CommandList.MapAction(
		ViewportShowMenuCommands.ShowOverlayNone,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowOverlayNone),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingOverlayNone));

	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowBoneWeight,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowOverlayBoneWeight),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingOverlayBoneWeight));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowMorphTargetVerts,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowOverlayMorphTargetVert),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingOverlayMorphTargetVerts));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowVertexColors,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowVertexColorsChanged),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingVertexColors));

	// Show sockets
	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowSockets,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowSockets),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingSockets));

	// Set bone drawing mode
	CommandList.MapAction(
		ViewportShowMenuCommands.ShowBoneDrawNone,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetBoneDrawMode, (int32)EBoneDrawMode::None),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsBoneDrawModeSet, (int32)EBoneDrawMode::None));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowBoneDrawSelected,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetBoneDrawMode, (int32)EBoneDrawMode::Selected),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsBoneDrawModeSet, (int32)EBoneDrawMode::Selected));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowBoneDrawSelectedAndParents,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetBoneDrawMode, (int32)EBoneDrawMode::SelectedAndParents),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsBoneDrawModeSet, (int32)EBoneDrawMode::SelectedAndParents));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowBoneDrawAll,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetBoneDrawMode, (int32)EBoneDrawMode::All),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsBoneDrawModeSet, (int32)EBoneDrawMode::All));

	// Set bone local axes mode
	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowLocalAxesNone,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetLocalAxesMode, (int32)ELocalAxesMode::None),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsLocalAxesModeSet, (int32)ELocalAxesMode::None));
	
	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowLocalAxesSelected,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetLocalAxesMode, (int32)ELocalAxesMode::Selected),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsLocalAxesModeSet, (int32)ELocalAxesMode::Selected));
	
	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowLocalAxesAll,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetLocalAxesMode, (int32)ELocalAxesMode::All),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsLocalAxesModeSet, (int32)ELocalAxesMode::All));

#if WITH_APEX_CLOTHING

	//Clothing show options
	CommandList.MapAction( 
		ViewportShowMenuCommands.DisableClothSimulation,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnDisableClothSimulation),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsDisablingClothSimulation));

	//Apply wind
	CommandList.MapAction( 
		ViewportShowMenuCommands.ApplyClothWind,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnApplyClothWind),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsApplyingClothWind));

	CommandList.MapAction( 
		ViewportShowMenuCommands.EnableCollisionWithAttachedClothChildren,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnEnableCollisionWithAttachedClothChildren),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsEnablingCollisionWithAttachedClothChildren));

	CommandList.MapAction(
		ViewportShowMenuCommands.PauseClothWithAnim,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnPauseClothingSimWithAnim),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsPausingClothingSimWithAnim));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowAllSections,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetSectionsDisplayMode, (int32)UDebugSkelMeshComponent::ESectionDisplayMode::ShowAll),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsSectionsDisplayMode, (int32)UDebugSkelMeshComponent::ESectionDisplayMode::ShowAll));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowOnlyClothSections,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetSectionsDisplayMode, (int32)UDebugSkelMeshComponent::ESectionDisplayMode::ShowOnlyClothSections),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsSectionsDisplayMode, (int32)UDebugSkelMeshComponent::ESectionDisplayMode::ShowOnlyClothSections));

	CommandList.MapAction(
		ViewportShowMenuCommands.HideOnlyClothSections,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetSectionsDisplayMode, (int32)UDebugSkelMeshComponent::ESectionDisplayMode::HideOnlyClothSections),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsSectionsDisplayMode, (int32)UDebugSkelMeshComponent::ESectionDisplayMode::HideOnlyClothSections));

#endif// #if WITH_APEX_CLOTHING		

	GetPreviewScene()->RegisterOnSelectedLODChanged(FOnSelectedLODChanged::CreateSP(this, &SAnimationEditorViewportTabBody::OnLODModelChanged));
	//Bind LOD preview menu commands
	const FAnimViewportLODCommands& ViewportLODMenuCommands = FAnimViewportLODCommands::Get();

	//LOD Auto
	CommandList.MapAction( 
		ViewportLODMenuCommands.LODAuto,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetLODModel, 0),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsLODModelSelected, 0));

	// LOD 0
	CommandList.MapAction(
		ViewportLODMenuCommands.LOD0,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetLODModel, 1),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsLODModelSelected, 1));

	// all other LODs will be added dynamically 
	
	CommandList.MapAction( 
		ViewportShowMenuCommands.ToggleGrid,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowGrid),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingGrid));

	CommandList.MapAction(
		ViewportShowMenuCommands.AutoAlignFloorToMesh,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnToggleAutoAlignFloor),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsAutoAlignFloor));
	
	//Bind LOD preview menu commands
	const FAnimViewportPlaybackCommands& ViewportPlaybackCommands = FAnimViewportPlaybackCommands::Get();

	//Create a menu item for each playback speed in EAnimationPlaybackSpeeds
	for(int32 i = 0; i < int(EAnimationPlaybackSpeeds::NumPlaybackSpeeds); ++i)
	{
		CommandList.MapAction( 
			ViewportPlaybackCommands.PlaybackSpeedCommands[i],
			FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetPlaybackSpeed, i),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsPlaybackSpeedSelected, i));
	}

	CommandList.MapAction(
		ViewportShowMenuCommands.MuteAudio,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnToggleMuteAudio),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsAudioMuted));

	CommandList.MapAction(
		ViewportShowMenuCommands.UseAudioAttenuation,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnToggleUseAudioAttenuation),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsAudioAttenuationEnabled));

	CommandList.MapAction(
		ViewportShowMenuCommands.ProcessRootMotion,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnTogglePreviewRootMotion),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsPreviewingRootMotion));

	// Turn Table Controls
	for (int32 i = 0; i < int(EAnimationPlaybackSpeeds::NumPlaybackSpeeds); ++i)
	{
		CommandList.MapAction(
			ViewportPlaybackCommands.TurnTableSpeeds[i],
			FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetTurnTableSpeed, i),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsTurnTableSpeedSelected, i));
	}

	CommandList.MapAction(
		ViewportPlaybackCommands.PersonaTurnTablePlay,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetTurnTableMode, int32(EPersonaTurnTableMode::Playing)),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsTurnTableModeSelected, int32(EPersonaTurnTableMode::Playing)));

	CommandList.MapAction(
		ViewportPlaybackCommands.PersonaTurnTablePause,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetTurnTableMode, int32(EPersonaTurnTableMode::Paused)),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsTurnTableModeSelected, int32(EPersonaTurnTableMode::Paused)));

	CommandList.MapAction(
		ViewportPlaybackCommands.PersonaTurnTableStop,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetTurnTableMode, int32(EPersonaTurnTableMode::Stopped)),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsTurnTableModeSelected, int32(EPersonaTurnTableMode::Stopped)));

	CommandList.MapAction(
		FEditorViewportCommands::Get().FocusViewportToSelection,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::HandleFocusCamera));
}

void SAnimationEditorViewportTabBody::OnSetTurnTableSpeed(int32 SpeedIndex)
{
	SelectedTurnTableSpeed = (EAnimationPlaybackSpeeds::Type)SpeedIndex;

	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	if (PreviewComponent)
	{
		PreviewComponent->TurnTableSpeedScaling = EAnimationPlaybackSpeeds::Values[SelectedTurnTableSpeed];
	}
}

bool SAnimationEditorViewportTabBody::IsTurnTableSpeedSelected(int32 SpeedIndex) const
{
	return (SelectedTurnTableSpeed == SpeedIndex);
}

void SAnimationEditorViewportTabBody::OnSetTurnTableMode(int32 ModeIndex)
{
	SelectedTurnTableMode = (EPersonaTurnTableMode::Type)ModeIndex;

	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	if (PreviewComponent)
	{
		PreviewComponent->TurnTableMode = SelectedTurnTableMode;

		if (SelectedTurnTableMode == EPersonaTurnTableMode::Stopped)
		{
			PreviewComponent->SetRelativeRotation(FRotator::ZeroRotator);
		}
	}
}

bool SAnimationEditorViewportTabBody::IsTurnTableModeSelected(int32 ModeIndex) const
{
	return (SelectedTurnTableMode == ModeIndex);
}

int32 SAnimationEditorViewportTabBody::GetLODModelCount() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	if( PreviewComponent && PreviewComponent->SkeletalMesh )
	{
		return PreviewComponent->SkeletalMesh->GetImportedResource()->LODModels.Num();
	}
	return 0;
}

void SAnimationEditorViewportTabBody::OnShowMorphTargets()
{
	if (UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		PreviewComponent->bDisableMorphTarget = !PreviewComponent->bDisableMorphTarget;
		PreviewComponent->MarkRenderStateDirty();
		RefreshViewport();
	}
}

void SAnimationEditorViewportTabBody::OnShowBoneNames()
{
	if (UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		PreviewComponent->bShowBoneNames = !PreviewComponent->bShowBoneNames;
		PreviewComponent->MarkRenderStateDirty();
		RefreshViewport();
	}
}

void SAnimationEditorViewportTabBody::OnShowRawAnimation()
{
	if (UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		PreviewComponent->bDisplayRawAnimation = !PreviewComponent->bDisplayRawAnimation;
		PreviewComponent->MarkRenderStateDirty();
	}
}

void SAnimationEditorViewportTabBody::OnShowNonRetargetedAnimation()
{
	if (UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		PreviewComponent->bDisplayNonRetargetedPose = !PreviewComponent->bDisplayNonRetargetedPose;
		PreviewComponent->MarkRenderStateDirty();
	}
}

void SAnimationEditorViewportTabBody::OnShowSourceRawAnimation()
{
	if (UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		PreviewComponent->bDisplaySourceAnimation = !PreviewComponent->bDisplaySourceAnimation;
		PreviewComponent->MarkRenderStateDirty();
	}
}

void SAnimationEditorViewportTabBody::OnShowBakedAnimation()
{
	if (UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		PreviewComponent->bDisplayBakedAnimation = !PreviewComponent->bDisplayBakedAnimation;
		PreviewComponent->MarkRenderStateDirty();
	}
}

void SAnimationEditorViewportTabBody::OnShowAdditiveBase()
{
	if (UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		PreviewComponent->bDisplayAdditiveBasePose = !PreviewComponent->bDisplayAdditiveBasePose;
		PreviewComponent->MarkRenderStateDirty();
	}
}

bool SAnimationEditorViewportTabBody::IsPreviewingAnimation() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return (PreviewComponent && PreviewComponent->PreviewInstance && (PreviewComponent->PreviewInstance == PreviewComponent->GetAnimInstance()));
}

bool SAnimationEditorViewportTabBody::IsShowingMorphTargets() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewComponent != NULL && PreviewComponent->bDisableMorphTarget == false;
}

bool SAnimationEditorViewportTabBody::IsShowingBoneNames() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewComponent != NULL && PreviewComponent->bShowBoneNames;
}

bool SAnimationEditorViewportTabBody::IsShowingRawAnimation() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewComponent != NULL && PreviewComponent->bDisplayRawAnimation;
}

bool SAnimationEditorViewportTabBody::IsShowingNonRetargetedPose() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewComponent != NULL && PreviewComponent->bDisplayNonRetargetedPose;
}

bool SAnimationEditorViewportTabBody::IsShowingAdditiveBase() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewComponent != NULL && PreviewComponent->bDisplayAdditiveBasePose;
}

bool SAnimationEditorViewportTabBody::IsShowingSourceRawAnimation() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewComponent != NULL && PreviewComponent->bDisplaySourceAnimation;
}

bool SAnimationEditorViewportTabBody::IsShowingBakedAnimation() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewComponent != NULL && PreviewComponent->bDisplayBakedAnimation;
}

void SAnimationEditorViewportTabBody::OnShowDisplayInfo(int32 DisplayInfoMode)
{
	GetAnimationViewportClient()->OnSetShowMeshStats(DisplayInfoMode);
}

bool SAnimationEditorViewportTabBody::IsShowingMeshInfo(int32 DisplayInfoMode) const
{
	return GetAnimationViewportClient()->GetShowMeshStats() == DisplayInfoMode;
}

void SAnimationEditorViewportTabBody::OnShowOverlayNone()
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	if (PreviewComponent)
	{
		PreviewComponent->SetShowBoneWeight(false);
		PreviewComponent->SetShowMorphTargetVerts(false);
		UpdateShowFlagForMeshEdges();
		PreviewComponent->MarkRenderStateDirty();
		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsShowingOverlayNone() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewComponent != NULL && !PreviewComponent->bDrawBoneInfluences && !PreviewComponent->bDrawMorphTargetVerts;
}

void SAnimationEditorViewportTabBody::OnShowOverlayBoneWeight()
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	if( PreviewComponent )
	{
		PreviewComponent->SetShowBoneWeight( !PreviewComponent->bDrawBoneInfluences );
		UpdateShowFlagForMeshEdges();
		PreviewComponent->MarkRenderStateDirty();
		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsShowingOverlayBoneWeight() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewComponent != NULL && PreviewComponent->bDrawBoneInfluences;
}

void SAnimationEditorViewportTabBody::OnShowOverlayMorphTargetVert()
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	if (PreviewComponent)
	{
		PreviewComponent->SetShowMorphTargetVerts(!PreviewComponent->bDrawMorphTargetVerts);
		UpdateShowFlagForMeshEdges();
		PreviewComponent->MarkRenderStateDirty();
		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsShowingOverlayMorphTargetVerts() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewComponent != NULL && PreviewComponent->bDrawMorphTargetVerts;
}

void SAnimationEditorViewportTabBody::OnSetBoneDrawMode(int32 BoneDrawMode)
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->SetBoneDrawMode((EBoneDrawMode::Type)BoneDrawMode);
}

bool SAnimationEditorViewportTabBody::IsBoneDrawModeSet(int32 BoneDrawMode) const
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	return AnimViewportClient->IsBoneDrawModeSet((EBoneDrawMode::Type)BoneDrawMode);
}

void SAnimationEditorViewportTabBody::OnSetLocalAxesMode(int32 LocalAxesMode)
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->SetLocalAxesMode((ELocalAxesMode::Type)LocalAxesMode);
}

bool SAnimationEditorViewportTabBody::IsLocalAxesModeSet(int32 LocalAxesMode) const
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	return AnimViewportClient->IsLocalAxesModeSet((ELocalAxesMode::Type)LocalAxesMode);
}

void SAnimationEditorViewportTabBody::OnShowSockets()
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	if( PreviewComponent )
	{
		PreviewComponent->bDrawSockets = !PreviewComponent->bDrawSockets;
		PreviewComponent->MarkRenderStateDirty();
		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsShowingSockets() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewComponent != NULL && PreviewComponent->bDrawSockets;
}


void SAnimationEditorViewportTabBody::OnShowGrid()
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->OnToggleShowGrid();
}

bool SAnimationEditorViewportTabBody::IsShowingGrid() const
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	return AnimViewportClient->IsShowingGrid();
}

void SAnimationEditorViewportTabBody::OnToggleAutoAlignFloor()
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->OnToggleAutoAlignFloor();
}

bool SAnimationEditorViewportTabBody::IsAutoAlignFloor() const
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	return AnimViewportClient->IsAutoAlignFloor();
}

/** Function to set the current playback speed*/
void SAnimationEditorViewportTabBody::OnSetPlaybackSpeed(int32 PlaybackSpeedMode)
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->SetPlaybackSpeedMode((EAnimationPlaybackSpeeds::Type)PlaybackSpeedMode);
}

bool SAnimationEditorViewportTabBody::IsPlaybackSpeedSelected(int32 PlaybackSpeedMode)
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	return PlaybackSpeedMode == AnimViewportClient->GetPlaybackSpeedMode();
}

void SAnimationEditorViewportTabBody::ShowRetargetBasePose()
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	if(PreviewComponent && PreviewComponent->PreviewInstance)
	{
		PreviewComponent->PreviewInstance->SetForceRetargetBasePose(!PreviewComponent->PreviewInstance->GetForceRetargetBasePose());
	}
}

bool SAnimationEditorViewportTabBody::CanShowRetargetBasePose() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewComponent != NULL && PreviewComponent->PreviewInstance;
}

bool SAnimationEditorViewportTabBody::IsShowRetargetBasePoseEnabled() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	if(PreviewComponent && PreviewComponent->PreviewInstance)
	{
		return PreviewComponent->PreviewInstance->GetForceRetargetBasePose();
	}
	return false;
}

void SAnimationEditorViewportTabBody::ShowBound()
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());	
	AnimViewportClient->ToggleShowBounds();

	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	if(PreviewComponent)
	{
		PreviewComponent->bDisplayBound = AnimViewportClient->EngineShowFlags.Bounds;
		PreviewComponent->RecreateRenderState_Concurrent();
	}
}

bool SAnimationEditorViewportTabBody::CanShowBound() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewComponent != NULL;
}

bool SAnimationEditorViewportTabBody::IsShowBoundEnabled() const
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());	
	return AnimViewportClient->IsSetShowBoundsChecked();
}

void SAnimationEditorViewportTabBody::ToggleShowPreviewMesh()
{
	if (UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		bool bCurrentlyVisible = IsShowPreviewMeshEnabled();
		PreviewComponent->SetVisibility(!bCurrentlyVisible);
	}
}

bool SAnimationEditorViewportTabBody::CanShowPreviewMesh() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewComponent != NULL;
}

bool SAnimationEditorViewportTabBody::IsShowPreviewMeshEnabled() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return (PreviewComponent != NULL) && PreviewComponent->IsVisible();
}

void SAnimationEditorViewportTabBody::UseInGameBound()
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	if (PreviewComponent != NULL)
	{
		PreviewComponent->UseInGameBounds(! PreviewComponent->IsUsingInGameBounds());
	}
}

bool SAnimationEditorViewportTabBody::CanUseInGameBound() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewComponent != NULL && IsShowBoundEnabled();
}

bool SAnimationEditorViewportTabBody::IsUsingInGameBound() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewComponent != NULL && PreviewComponent->IsUsingInGameBounds();
}

void SAnimationEditorViewportTabBody::HandlePreviewMeshChanged(class USkeletalMesh* OldSkeletalMesh, class USkeletalMesh* NewSkeletalMesh)
{
	PopulateNumUVChannels();
}

void SAnimationEditorViewportTabBody::AnimChanged(UAnimationAsset* AnimAsset)
{
	UpdateScrubPanel(AnimAsset);
}

void SAnimationEditorViewportTabBody::ComboBoxSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	int32 NewUVSelection = UVChannels.Find(NewSelection);

	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->SetUVChannelToDraw(NewUVSelection);

	RefreshViewport();
}

void SAnimationEditorViewportTabBody::PopulateNumUVChannels()
{
	NumUVChannels.Empty();

	if (UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		if (FSkeletalMeshResource* MeshResource = PreviewComponent->GetSkeletalMeshResource())
		{
			int32 NumLods = MeshResource->LODModels.Num();
			NumUVChannels.AddZeroed(NumLods);
			for(int32 LOD = 0; LOD < NumLods; ++LOD)
			{
				NumUVChannels[LOD] = MeshResource->LODModels[LOD].VertexBufferGPUSkin.GetNumTexCoords();
			}
		}
	}

	PopulateUVChoices();
}

void SAnimationEditorViewportTabBody::PopulateUVChoices()
{
	// Fill out the UV channels combo.
	UVChannels.Empty();
	
	if (UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		int32 CurrentLOD = FMath::Clamp(PreviewComponent->ForcedLodModel - 1, 0, NumUVChannels.Num() - 1);

		if (NumUVChannels.IsValidIndex(CurrentLOD))
		{
			for (int32 UVChannelID = 0; UVChannelID < NumUVChannels[CurrentLOD]; ++UVChannelID)
			{
				UVChannels.Add( MakeShareable( new FString( FText::Format( NSLOCTEXT("AnimationEditorViewport", "UVChannel_ID", "UV Channel {0}"), FText::AsNumber( UVChannelID ) ).ToString() ) ) );
			}

			TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
			int32 CurrentUVChannel = AnimViewportClient->GetUVChannelToDraw();
			if (!UVChannels.IsValidIndex(CurrentUVChannel))
			{
				CurrentUVChannel = 0;
			}

			AnimViewportClient->SetUVChannelToDraw(CurrentUVChannel);

			if (UVChannelCombo.IsValid() && UVChannels.IsValidIndex(CurrentUVChannel))
			{
				UVChannelCombo->SetSelectedItem(UVChannels[CurrentUVChannel]);
			}
		}
	}
}

void SAnimationEditorViewportTabBody::UpdateScrubPanel(UAnimationAsset* AnimAsset)
{
	// We might not have a scrub panel if we're in animation mode.
	if (ScrubPanelContainer.IsValid())
	{
		ScrubPanelContainer->ClearChildren();
		bool bUseDefaultScrubPanel = true;
		if (UAnimMontage* Montage = Cast<UAnimMontage>(AnimAsset))
		{
			ScrubPanelContainer->AddSlot()
				.AutoHeight()
				[
					SNew(SAnimMontageScrubPanel, GetPreviewScene())
					.ViewInputMin(this, &SAnimationEditorViewportTabBody::GetViewMinInput)
					.ViewInputMax(this, &SAnimationEditorViewportTabBody::GetViewMaxInput)
					.bAllowZoom(true)
				];
			bUseDefaultScrubPanel = false;
		}
		if(bUseDefaultScrubPanel)
		{
			ScrubPanelContainer->AddSlot()
				.AutoHeight()
				[
					SNew(SAnimationScrubPanel, GetPreviewScene())
					.ViewInputMin(this, &SAnimationEditorViewportTabBody::GetViewMinInput)
					.ViewInputMax(this, &SAnimationEditorViewportTabBody::GetViewMaxInput)
					.bAllowZoom(true)
				];
		}
	}
}

float SAnimationEditorViewportTabBody::GetViewMinInput() const
{
	if (UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		UObject* PreviewAsset = GetPreviewScene()->GetPreviewAnimationAsset();
		if (PreviewAsset != NULL)
		{
			return 0.0f;
		}
		else if (PreviewComponent->GetAnimInstance() != NULL)
		{
			return FMath::Max<float>((float)(PreviewComponent->GetAnimInstance()->LifeTimer - 30.0), 0.0f);
		}
	}

	return 0.f; 
}

float SAnimationEditorViewportTabBody::GetViewMaxInput() const
{ 
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	if (PreviewComponent != NULL)
	{
		UObject* PreviewAsset = GetPreviewScene()->GetPreviewAnimationAsset();
		if ((PreviewAsset != NULL) && (PreviewComponent->PreviewInstance != NULL))
		{
			return PreviewComponent->PreviewInstance->GetLength();
		}
		else if (PreviewComponent->GetAnimInstance() != NULL)
		{
			return PreviewComponent->GetAnimInstance()->LifeTimer;
		}
	}

	return 0.f;
}

void SAnimationEditorViewportTabBody::UpdateShowFlagForMeshEdges()
{
	bool bUseOverlayMaterial = false;
	if (UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		bUseOverlayMaterial = PreviewComponent->bDrawBoneInfluences || PreviewComponent->bDrawMorphTargetVerts;
	}

	//@TODO: SNOWPOCALYPSE: broke UnlitWithMeshEdges
	bool bShowMeshEdgesViewMode = false;
#if 0
	bShowMeshEdgesViewMode = (CurrentViewMode == EAnimationEditorViewportMode::UnlitWithMeshEdges);
#endif

	LevelViewportClient->EngineShowFlags.SetMeshEdges(bUseOverlayMaterial || bShowMeshEdgesViewMode);
}

int32 SAnimationEditorViewportTabBody::GetLODSelection() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();

	if (PreviewComponent)
	{
		return PreviewComponent->ForcedLodModel;
	}
	return 0;
}

bool SAnimationEditorViewportTabBody::IsLODModelSelected(int32 LODSelectionType) const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();

	if (PreviewComponent)
	{
		return (PreviewComponent->ForcedLodModel == LODSelectionType) ? true : false;
	}
	return false;
}

void SAnimationEditorViewportTabBody::OnSetLODModel(int32 LODSelectionType)
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	
	if( PreviewComponent )
	{
		LODSelection = LODSelectionType;
		PreviewComponent->ForcedLodModel = LODSelectionType;
		PopulateUVChoices();
		GetPreviewScene()->BroadcastOnSelectedLODChanged();
	}
}

void SAnimationEditorViewportTabBody::OnLODModelChanged()
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();

	if (PreviewComponent && LODSelection != PreviewComponent->ForcedLodModel)
	{
		LODSelection = PreviewComponent->ForcedLodModel;
		PopulateUVChoices();
	}
}

TSharedRef<FAnimationViewportClient> SAnimationEditorViewportTabBody::GetAnimationViewportClient() const
{
	return StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
}

void SAnimationEditorViewportTabBody::OpenPreviewSceneSettings()
{
	OnInvokeTab.ExecuteIfBound(FPersonaTabs::AdvancedPreviewSceneSettingsID);
}

void SAnimationEditorViewportTabBody::ToggleCameraFollow()
{
	// Switch to rotation mode
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->SetCameraFollow();
}

bool SAnimationEditorViewportTabBody::IsCameraFollowEnabled() const
{
	// need a single selected bone in the skeletal tree
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	return (AnimViewportClient->IsSetCameraFollowChecked());
}

void SAnimationEditorViewportTabBody::SaveCameraAsDefault()
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->SaveCameraAsDefault();
}

void SAnimationEditorViewportTabBody::ClearDefaultCamera()
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->ClearDefaultCamera();
}

void SAnimationEditorViewportTabBody::JumpToDefaultCamera()
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->JumpToDefaultCamera();
}

bool SAnimationEditorViewportTabBody::HasDefaultCameraSet() const
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	return (AnimViewportClient->HasDefaultCameraSet());
}

bool SAnimationEditorViewportTabBody::CanChangeCameraMode() const
{
	//Not allowed to change camera type when we are in an ortho camera
	return !LevelViewportClient->IsOrtho();
}

void SAnimationEditorViewportTabBody::OnToggleMuteAudio()
{
	GetAnimationViewportClient()->OnToggleMuteAudio();
}

bool SAnimationEditorViewportTabBody::IsAudioMuted() const
{
	return GetAnimationViewportClient()->IsAudioMuted();
}

void SAnimationEditorViewportTabBody::OnToggleUseAudioAttenuation()
{
	GetAnimationViewportClient()->OnToggleUseAudioAttenuation();
}

bool SAnimationEditorViewportTabBody::IsAudioAttenuationEnabled() const
{
	return GetAnimationViewportClient()->IsUsingAudioAttenuation();
}

void SAnimationEditorViewportTabBody::OnTogglePreviewRootMotion()
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();

	if (PreviewComponent)
	{
		PreviewComponent->SetPreviewRootMotion(!PreviewComponent->GetPreviewRootMotion());
	}
}

bool SAnimationEditorViewportTabBody::IsPreviewingRootMotion() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();

	if (PreviewComponent)
	{
		return PreviewComponent->GetPreviewRootMotion();
	}
	return false;
}

bool SAnimationEditorViewportTabBody::IsShowingVertexColors() const
{
	return GetAnimationViewportClient()->EngineShowFlags.VertexColors;
}

void SAnimationEditorViewportTabBody::OnShowVertexColorsChanged()
{
	FEngineShowFlags& ShowFlags = GetAnimationViewportClient()->EngineShowFlags;

	if(UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		if(!ShowFlags.VertexColors)
		{
			ShowFlags.SetVertexColors(true);
			ShowFlags.SetLighting(false);
			ShowFlags.SetIndirectLightingCache(false);
			PreviewComponent->bDisplayVertexColors = true;
		}
		else
		{
			ShowFlags.SetVertexColors(false);
			ShowFlags.SetLighting(true);
			ShowFlags.SetIndirectLightingCache(true);
			PreviewComponent->bDisplayVertexColors = false;
		}

		PreviewComponent->RecreateRenderState_Concurrent();
	}

	RefreshViewport();
}

#if WITH_APEX_CLOTHING
bool SAnimationEditorViewportTabBody::IsDisablingClothSimulation() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();

	if( PreviewComponent )
	{
		return PreviewComponent->bDisableClothSimulation;
	}

	return false;
}

void SAnimationEditorViewportTabBody::OnDisableClothSimulation()
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();

	if( PreviewComponent )
	{
		PreviewComponent->bDisableClothSimulation = !PreviewComponent->bDisableClothSimulation;

		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsApplyingClothWind() const
{
	return GetPreviewScene()->IsWindEnabled();
}

void SAnimationEditorViewportTabBody::OnApplyClothWind()
{
	GetPreviewScene()->EnableWind(!GetPreviewScene()->IsWindEnabled());
	RefreshViewport();
}

void SAnimationEditorViewportTabBody::OnPauseClothingSimWithAnim()
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();

	if(PreviewComponent)
	{
		PreviewComponent->bPauseClothingSimulationWithAnim = !PreviewComponent->bPauseClothingSimulationWithAnim;

		bool bShouldPause = PreviewComponent->bPauseClothingSimulationWithAnim;

		if(PreviewComponent->IsPreviewOn() && PreviewComponent->PreviewInstance)
		{
			UAnimSingleNodeInstance* PreviewInstance = PreviewComponent->PreviewInstance;
			const bool bPlaying = PreviewInstance->IsPlaying();

			if(!bPlaying && bShouldPause)
			{
				PreviewComponent->SuspendClothingSimulation();
			}
			else if(!bShouldPause && PreviewComponent->IsClothingSimulationSuspended())
			{
				PreviewComponent->ResumeClothingSimulation();
			}
		}
	}
}

bool SAnimationEditorViewportTabBody::IsPausingClothingSimWithAnim()
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
	
	if(PreviewComponent)
	{
		return PreviewComponent->bPauseClothingSimulationWithAnim;
	}

	return false;
}

void SAnimationEditorViewportTabBody::SetWindStrength(float SliderPos)
{
	GetPreviewScene()->SetWindStrength(SliderPos);
	RefreshViewport();
}

float SAnimationEditorViewportTabBody::GetWindStrengthSliderValue() const
{
	return GetPreviewScene()->GetWindStrength();
}

FText SAnimationEditorViewportTabBody::GetWindStrengthLabel() const
{
	//Clamp slide value so that minimum value displayed is 0.00 and maximum is 1.0
	float SliderValue = FMath::Clamp<float>(GetWindStrengthSliderValue(), 0.0f, 1.0f);

	static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(2)
		.SetMaximumFractionalDigits(2);
	return FText::AsNumber(SliderValue, &FormatOptions);
}

void SAnimationEditorViewportTabBody::SetGravityScale(float SliderPos)
{
	GetPreviewScene()->SetGravityScale(SliderPos);
	RefreshViewport();
}

float SAnimationEditorViewportTabBody::GetGravityScaleSliderValue() const
{
	return GetPreviewScene()->GetGravityScale();
}

FText SAnimationEditorViewportTabBody::GetGravityScaleLabel() const
{
	//Clamp slide value so that minimum value displayed is 0.00 and maximum is 4.0
	float SliderValue = FMath::Clamp<float>(GetGravityScaleSliderValue() * 4, 0.0f, 4.0f);

	static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(2)
		.SetMaximumFractionalDigits(2);
	return FText::AsNumber(SliderValue, &FormatOptions);
}

void SAnimationEditorViewportTabBody::OnEnableCollisionWithAttachedClothChildren()
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();

	if( PreviewComponent )
	{
		PreviewComponent->bCollideWithAttachedChildren = !PreviewComponent->bCollideWithAttachedChildren;
		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsEnablingCollisionWithAttachedClothChildren() const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();

	if( PreviewComponent )
	{
		return PreviewComponent->bCollideWithAttachedChildren;
	}

	return false;
}

void SAnimationEditorViewportTabBody::OnSetSectionsDisplayMode(int32 DisplayMode)
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();

	if (!PreviewComponent)
	{
		return;
	}

	PreviewComponent->SectionsDisplayMode = DisplayMode;

	switch (DisplayMode)
	{
	case UDebugSkelMeshComponent::ESectionDisplayMode::ShowAll:
		// restore to the original states
		PreviewComponent->RestoreClothSectionsVisibility();
		break;
	case UDebugSkelMeshComponent::ESectionDisplayMode::ShowOnlyClothSections:
		// disable all except clothing sections and shows only cloth sections
		PreviewComponent->ToggleClothSectionsVisibility(true);
		break;
	case UDebugSkelMeshComponent::ESectionDisplayMode::HideOnlyClothSections:
		// disables only clothing sections
		PreviewComponent->ToggleClothSectionsVisibility(false);
		break;
	}

	RefreshViewport();
}

bool SAnimationEditorViewportTabBody::IsSectionsDisplayMode(int32 DisplayMode) const
{
	UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();

	if (ensure(PreviewComponent))
	{
		return (PreviewComponent->SectionsDisplayMode == DisplayMode);
	}

	return false;
}
#endif // #if WITH_APEX_CLOTHING

EVisibility SAnimationEditorViewportTabBody::GetViewportCornerTextVisibility() const
{
	if (GetPreviewScene()->IsRecording())
	{
		return EVisibility::Visible;
	}
	else if (BlueprintEditorPtr.IsValid() && BlueprintEditorPtr.Pin()->IsModeCurrent(FPersonaModes::AnimBlueprintEditMode))
	{
		if (UBlueprint* Blueprint = BlueprintEditorPtr.Pin()->GetBlueprintObj())
		{
			const bool bUpToDate = (Blueprint->Status == BS_UpToDate) || (Blueprint->Status == BS_UpToDateWithWarnings);
			return bUpToDate ? EVisibility::Collapsed : EVisibility::Visible;
		}		
	}

	return EVisibility::Collapsed;
}

FText SAnimationEditorViewportTabBody::GetViewportCornerText() const
{
	if(GetPreviewScene()->IsRecording())
	{
		UAnimSequence* Recording = GetPreviewScene()->GetCurrentRecording();
		const FString& Name = Recording ? Recording->GetName() : TEXT("None");
		float TimeRecorded = GetPreviewScene()->GetCurrentRecordingTime();
		FNumberFormattingOptions NumberOption;
		NumberOption.MaximumFractionalDigits = 2;
		NumberOption.MinimumFractionalDigits = 2;
		return FText::Format(LOCTEXT("AnimRecorder", "Recording '{0}' [{1} sec(s)]"),
			FText::FromString(Name), FText::AsNumber(TimeRecorded, &NumberOption));
	}

	if (BlueprintEditorPtr.IsValid() && BlueprintEditorPtr.Pin()->IsModeCurrent(FPersonaModes::AnimBlueprintEditMode))
	{
		if (UBlueprint* Blueprint = BlueprintEditorPtr.Pin()->GetBlueprintObj())
		{
			switch (Blueprint->Status)
			{
			case BS_UpToDate:
			case BS_UpToDateWithWarnings:
				// Fall thru and return empty string
				break;
			case BS_Dirty:
				return LOCTEXT("AnimBP_Dirty", "Preview out of date\nClick to recompile");
			case BS_Error:
				return LOCTEXT("AnimBP_CompileError", "Compile Error");
			default:
				return LOCTEXT("AnimBP_UnknownStatus", "Unknown Status");
			}
		}		
	}

	return FText::GetEmpty();
}

FText SAnimationEditorViewportTabBody::GetViewportCornerTooltip() const
{
	if(GetPreviewScene()->IsRecording())
	{
		return LOCTEXT("RecordingStatusTooltip", "Shows the status of animation recording.");
	}

	if(BlueprintEditorPtr.IsValid() && BlueprintEditorPtr.Pin()->IsModeCurrent(FPersonaModes::AnimBlueprintEditMode))
	{
		return LOCTEXT("BlueprintStatusTooltip", "Shows the status of the animation blueprint.\nClick to recompile a dirty blueprint");
	}

	return FText::GetEmpty();
}

FReply SAnimationEditorViewportTabBody::ClickedOnViewportCornerText()
{
	if(BlueprintEditorPtr.IsValid())
	{
		if (UBlueprint* Blueprint = BlueprintEditorPtr.Pin()->GetBlueprintObj())
		{
			if (!Blueprint->IsUpToDate())
			{
				BlueprintEditorPtr.Pin()->Compile();
			}
		}
	}

	return FReply::Handled();
}

void SAnimationEditorViewportTabBody::HandleFocusCamera()
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->FocusViewportOnPreviewMesh(false);
}

#undef LOCTEXT_NAMESPACE
