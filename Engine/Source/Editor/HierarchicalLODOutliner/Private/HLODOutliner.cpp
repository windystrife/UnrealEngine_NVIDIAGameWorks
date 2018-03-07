// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HLODOutliner.h"
#include "GameFramework/Actor.h"
#include "Widgets/SOverlay.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Components/DrawSphereComponent.h"
#include "Widgets/Layout/SSplitter.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "SlateOptMacros.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"
#include "Engine/MeshMerging.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/Selection.h"
#include "Editor.h"

#include "Engine/LODActor.h"
#include "HierarchicalLOD.h"

#include "EngineGlobals.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "ScopedTransaction.h"

#include "LODActorItem.h"
#include "LODLevelItem.h"
#include "StaticMeshActorItem.h"
#include "HLODTreeWidgetItem.h"
#include "HLODSelectionActor.h"

#include "Logging/MessageLog.h"

#include "IHierarchicalLODUtilities.h"
#include "HierarchicalLODUtilitiesModule.h"

#define LOCTEXT_NAMESPACE "HLODOutliner"

namespace HLODOutliner
{
	SHLODOutliner::SHLODOutliner()		
	{
		bNeedsRefresh = true;
		CurrentWorld = nullptr;
		CurrentWorldSettings = nullptr;
		ForcedLODLevel = -1;
		ForcedLODSliderValue = 0.0f;
		bForcedSliderValueUpdating = false;

		FHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<FHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
		HierarchicalLODUtilities = Module.GetUtilities();
	}

	SHLODOutliner::~SHLODOutliner()
	{
		DeregisterDelegates();	
		DestroySelectionActors();
		CurrentWorld = nullptr;
		HLODTreeRoot.Empty();
		SelectedNodes.Empty();		
		AllNodes.Empty();
		SelectionActors.Empty();
		LODLevelBuildFlags.Empty();
		LODLevelActors.Empty();
		PendingActions.Empty();
	}

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
		void SHLODOutliner::Construct(const FArguments& InArgs)
	{
		CreateSettingsView();

		/** Holds all widgets for the profiler window like menu bar, toolbar and tabs. */
		MainContentPanel = SNew(SVerticalBox);
		ChildSlot
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBorder)
							.BorderImage(FEditorStyle::GetBrush("SettingsEditor.CheckoutWarningBorder"))
							.BorderBackgroundColor(FColor(166,137,0))							
							[
								SNew(SHorizontalBox)
								.Visibility_Lambda([this]() -> EVisibility 
								{
									bool bVisible = !bNeedsRefresh && CurrentWorld && HierarchicalLODUtilities->IsWorldUsedForStreaming(CurrentWorld);
									return bVisible ? EVisibility::Visible : EVisibility::Collapsed;
								})

								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.AutoWidth()
								.Padding(4.0f, 0.0f, 4.0f, 0.0f)
								[
									SNew(SImage)
									.Image(FEditorStyle::GetBrush("SettingsEditor.WarningIcon"))
								]

								+SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.AutoWidth()
								.Padding(4.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("HLODDisabledSublevel", "Changing the HLOD settings is disabled for sub-levels"))
								]
							]
						]

						// Overlay slot for the main HLOD window area
						+ SVerticalBox::Slot()
						[
							MainContentPanel.ToSharedRef()
						]							
					]						
				]
			];

		// Disable panel if system is not enabled
		MainContentPanel->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SHLODOutliner::OutlinerEnabled)));

		MainContentPanel->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				CreateButtonWidgets()
			];

		MainContentPanel->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[				
				CreateForcedViewSlider()
			];

		MainContentPanel->AddSlot()
			.FillHeight(1.0f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)
				.Style(FEditorStyle::Get(), "ContentBrowser.Splitter")
				+ SSplitter::Slot()
				.Value(0.5)
				[
					CreateTreeviewWidget()										
				]
				+ SSplitter::Slot()
				.Value(0.5)
				[
					SettingsView.ToSharedRef()										
				]		
			];
		
		RegisterDelegates();
	}

	TSharedRef<SWidget> SHLODOutliner::CreateButtonWidgets()
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(FMargin(0.0f, 5.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("GenerateClusters", "Generate Clusters"))
					.OnClicked(this, &SHLODOutliner::HandlePreviewHLODs)
					.ToolTipText(LOCTEXT("GenerateClusterToolTip", "Generates Clusters (but not proxy meshes) for Meshes in the Level"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("DeleteClusters", "Delete Clusters"))
					.OnClicked(this, &SHLODOutliner::HandleDeleteHLODs)
					.ToolTipText(LOCTEXT("DeleteClusterToolTip", "Deletes all Clusters in the Level"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("BuildMeshes", "Generate Proxy Meshes"))
					.OnClicked(this, &SHLODOutliner::HandleBuildLODActors)
					.ToolTipText(LOCTEXT("GenerateProxyMeshesToolTip", "Generates Proxy Mesh for each Clusters in the Level"))
				]
			];	
	}

	TSharedRef<SWidget> SHLODOutliner::CreateTreeviewWidget()
	{
		return SAssignNew(TreeView, SHLODTree)
			.ItemHeight(24.0f)
			.TreeItemsSource(&HLODTreeRoot)
			.OnGenerateRow(this, &SHLODOutliner::OnOutlinerGenerateRow)
			.OnGetChildren(this, &SHLODOutliner::OnOutlinerGetChildren)
			.OnSelectionChanged(this, &SHLODOutliner::OnOutlinerSelectionChanged)
			.OnMouseButtonDoubleClick(this, &SHLODOutliner::OnOutlinerDoubleClick)
			.OnContextMenuOpening(this, &SHLODOutliner::OnOpenContextMenu)
			.OnExpansionChanged(this, &SHLODOutliner::OnItemExpansionChanged)			
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column("SceneActorName")
				.DefaultLabel(LOCTEXT("SceneActorName", "Scene Actor Name"))
				.FillWidth(0.3f)				
				+ SHeaderRow::Column("RawTriangleCount")
				.DefaultLabel(LOCTEXT("RawTriangleCount", "Original Triangle Count"))
				.DefaultTooltip(LOCTEXT("RawTriangleCountToolTip", "Original Number of Triangles in a LOD Mesh"))
				.FillWidth(0.2f)
				+ SHeaderRow::Column("ReducedTriangleCount")
				.DefaultLabel(LOCTEXT("ReducedTriangleCount", "Reduced Triangle Count"))
				.DefaultTooltip(LOCTEXT("ReducedTriangleCountToolTip", "Reduced Number of Triangles in a LOD Mesh"))
				.FillWidth(0.2f)
				+ SHeaderRow::Column("ReductionPercentage")
				.DefaultLabel(LOCTEXT("ReductionPercentage", "% Reduction"))
				.DefaultTooltip(LOCTEXT("ReductionPercentageToolTip", "Percentage of Triangle Reduction in a LOD Mesh"))
				.FillWidth(0.1f)
				+ SHeaderRow::Column("Level")
				.DefaultLabel(LOCTEXT("Level", "Level"))
				.DefaultTooltip(LOCTEXT("LevelToolTip", "Persistent Level of a LOD Mesh"))
				.FillWidth(0.2f)
			);
	}

	TSharedRef<SWidget> SHLODOutliner::CreateForcedViewSlider()
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(FMargin(0.0f, 5.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(FMargin(5.0f, 0.0f))
				.FillWidth(0.5f)
				[
					SNew(STextBlock)
					.Text(this, &SHLODOutliner::HandleForceLevelText)
				]
				+ SHorizontalBox::Slot()
				.Padding(FMargin(5.0f, 0.0f))
				.FillWidth(0.5f)
				[
					SNew(SSlider)
					.OnValueChanged(this, &SHLODOutliner::HandleForcedLevelSliderValueChanged)
					.OnMouseCaptureBegin(this, &SHLODOutliner::HandleForcedLevelSliderCaptureBegin)
					.OnMouseCaptureEnd(this, &SHLODOutliner::HandleForcedLevelSliderCaptureEnd)
					.Orientation(Orient_Horizontal)
					.Value(this, &SHLODOutliner::HandleForcedLevelSliderValue)
				]
			];
	}

	void SHLODOutliner::CreateSettingsView()
	{
		// Create a property view
		FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FNotifyHook* NotifyHook = this;
		FDetailsViewArgs DetailsViewArgs(
			/*bUpdateFromSelection=*/ false,
			/*bLockable=*/ false,
			/*bAllowSearch=*/ false,
			FDetailsViewArgs::HideNameArea,
			/*bHideSelectionTip=*/ true,
			/*InNotifyHook=*/ NotifyHook,
			/*InSearchInitialKeyFocus=*/ false,
			/*InViewIdentifier=*/ NAME_None);
		DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;
		DetailsViewArgs.bShowOptions = false;

		SettingsView = EditModule.CreateDetailView(DetailsViewArgs);

		struct Local
		{
			/** Delegate to show all properties */
			static bool IsPropertyVisible(const FPropertyAndParent& PropertyAndParent, bool bInShouldShowNonEditable)
			{
				if (PropertyAndParent.Property.GetFName() == GET_MEMBER_NAME_CHECKED(FMeshMergingSettings, SpecificLOD)
					|| PropertyAndParent.Property.GetFName() == GET_MEMBER_NAME_CHECKED(FMeshMergingSettings, LODSelectionType)
					|| PropertyAndParent.Property.GetFName() == GET_MEMBER_NAME_CHECKED(AWorldSettings, bEnableHierarchicalLODSystem))
				{
					return false;
				}

				const char* CategoryNames[5] =
				{
					"LODSystem",
					"ProxySettings",
					"LandscapeCulling",
					"MeshSettings",
					"MaterialSettings"
				};

				FString CategoryName = PropertyAndParent.Property.GetMetaData("Category");
				for (uint32 CategoryIndex = 0; CategoryIndex < 5; ++CategoryIndex)
				{
					if (CategoryName == CategoryNames[CategoryIndex])
					{


						return true;
					}
				}

				return false;
			}
		};

		SettingsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateStatic(&Local::IsPropertyVisible, true));
		SettingsView->SetDisableCustomDetailLayouts(true);
	}

	void SHLODOutliner::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		// Get a collection of items and folders which were formerly collapsed
		const FParentsExpansionState ExpansionStateInfo = GetParentsExpansionState();

		if (bNeedsRefresh)
		{
			Populate();
		}

		bool bChangeMade = false;

		// Only deal with 256 at a time
		const int32 End = FMath::Min(PendingActions.Num(), 512);
		for (int32 Index = 0; Index < End; ++Index)
		{
			auto& PendingAction = PendingActions[Index];
			switch (PendingAction.Type)
			{
			case FOutlinerAction::AddItem:
				bChangeMade |= AddItemToTree(PendingAction.Item, PendingAction.ParentItem);
				break;

			case FOutlinerAction::MoveItem:
				MoveItemInTree(PendingAction.Item, PendingAction.ParentItem);
				bChangeMade = true;
				break;

			case FOutlinerAction::RemoveItem:
				RemoveItemFromTree(PendingAction.Item);
				bChangeMade = true;
				break;
			default:
				check(false);
				break;
			}
		}
		PendingActions.RemoveAt(0, End);
				
		if (bChangeMade)
		{
			// Restore the expansion states
			SetParentsExpansionState(ExpansionStateInfo);

			// Restore expansion states
			TreeView->RequestTreeRefresh();		
		}			

		// Update the forced LOD level, as the slider for it is being dragged
		if (bForcedSliderValueUpdating)
		{
			// Snap values
			int32 SnappedValue = FMath::RoundToInt(FMath::Min(ForcedLODSliderValue, 1.0f) * (float)LODLevelTransitionScreenSizes.Num());
			if (SnappedValue - 1 != ForcedLODLevel)
			{
				RestoreForcedLODLevel(ForcedLODLevel);
				ForcedLODLevel = -1;
				SetForcedLODLevel(SnappedValue - 1);

				// Invalidate viewport to make sure HLODs are visible while dragging
				if (GEditor)
				{
					GEditor->GetActiveViewport()->Invalidate();
				}				
			}
		}

	}

	void SHLODOutliner::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
	}

	void SHLODOutliner::OnMouseLeave(const FPointerEvent& MouseEvent)
	{
		SCompoundWidget::OnMouseLeave(MouseEvent);
	}

	FReply SHLODOutliner::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

	FReply SHLODOutliner::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		return SCompoundWidget::OnDrop(MyGeometry, DragDropEvent);
	}

	FReply SHLODOutliner::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		return SCompoundWidget::OnDragOver(MyGeometry, DragDropEvent);
	}

	void SHLODOutliner::PostUndo(bool bSuccess)
	{
		FullRefresh();
	}

	FReply SHLODOutliner::HandleBuildHLODs()
	{
		if (CurrentWorld)
		{
			CurrentWorld->HierarchicalLODBuilder->Build();
		}

		FMessageLog("HLODResults").Open();

		FullRefresh();
		return FReply::Handled();
	}

	FReply SHLODOutliner::HandleDeleteHLODs()
	{
		if (CurrentWorld)
		{
			LODLevelActors.Empty();
			CurrentWorld->HierarchicalLODBuilder->ClearHLODs();
		}
		
		ResetLODLevelForcing();
		for (AHLODSelectionActor* BoundsActor : SelectionActors)
		{
			if (BoundsActor && BoundsActor->IsValidLowLevel())
			{
				CurrentWorld->DestroyActor(BoundsActor);
			}
		}
		SelectionActors.Empty();

		FullRefresh();
		return FReply::Handled();
	}

	FReply SHLODOutliner::HandlePreviewHLODs()
	{
		if (CurrentWorld)
		{
			CurrentWorld->HierarchicalLODBuilder->PreviewBuild();
		}

		FMessageLog("HLODResults").Open();

		FullRefresh();
		return FReply::Handled();
	}

	FReply SHLODOutliner::HandleDeletePreviewHLODs()
	{
		if (CurrentWorld)
		{
			CurrentWorld->HierarchicalLODBuilder->ClearPreviewBuild();
		}
		FullRefresh();
		return FReply::Handled();
	}

	FReply SHLODOutliner::HandleBuildLODActors()
	{
		if (CurrentWorld)
		{
			DestroySelectionActors();
			CurrentWorld->HierarchicalLODBuilder->BuildMeshesForLODActors();
			SetForcedLODLevel(ForcedLODLevel);
		}

		FMessageLog("HLODResults").Open();		

		return FReply::Handled();
	}

	FReply SHLODOutliner::HandleForceRefresh()
	{
		FullRefresh();

		return FReply::Handled();
	}

	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	void SHLODOutliner::RegisterDelegates()
	{
		FEditorDelegates::MapChange.AddSP(this, &SHLODOutliner::OnMapChange);
		FEditorDelegates::NewCurrentLevel.AddSP(this, &SHLODOutliner::OnNewCurrentLevel);
		FEditorDelegates::OnMapOpened.AddSP(this, &SHLODOutliner::OnMapLoaded);
		FWorldDelegates::LevelAddedToWorld.AddSP(this, &SHLODOutliner::OnLevelAdded);
		FWorldDelegates::LevelRemovedFromWorld.AddSP(this, &SHLODOutliner::OnLevelRemoved);
		GEngine->OnLevelActorListChanged().AddSP(this, &SHLODOutliner::FullRefresh);
		GEngine->OnLevelActorAdded().AddSP(this, &SHLODOutliner::OnLevelActorsAdded);
		GEngine->OnLevelActorDeleted().AddSP(this, &SHLODOutliner::OnLevelActorsRemoved);
		GEngine->OnActorMoved().AddSP(this, &SHLODOutliner::OnActorMovedEvent);

		// Register to be notified when properties are edited
		FCoreDelegates::OnActorLabelChanged.AddRaw(this, &SHLODOutliner::OnActorLabelChanged);

		// Selection change
		USelection::SelectionChangedEvent.AddRaw(this, &SHLODOutliner::OnLevelSelectionChanged);
		USelection::SelectObjectEvent.AddRaw(this, &SHLODOutliner::OnLevelSelectionChanged);

		// HLOD related events
		GEditor->OnHLODActorMoved().AddSP(this, &SHLODOutliner::OnHLODActorMovedEvent);
		GEditor->OnHLODActorAdded().AddSP(this, &SHLODOutliner::OnHLODActorAddedEvent);
		GEditor->OnHLODActorMarkedDirty().AddSP(this, &SHLODOutliner::OnHLODActorMarkedDirtyEvent);
		GEditor->OnHLODTransitionScreenSizeChanged().AddSP(this, &SHLODOutliner::OnHLODTransitionScreenSizeChangedEvent);
		GEditor->OnHLODLevelsArrayChanged().AddSP(this, &SHLODOutliner::OnHLODLevelsArrayChangedEvent);
		GEditor->OnHLODActorRemovedFromCluster().AddSP(this, &SHLODOutliner::OnHLODActorRemovedFromClusterEvent);

		// Register to update when an undo/redo operation has been called to update our list of actors
		GEditor->RegisterForUndo(this);
	}

	void SHLODOutliner::DeregisterDelegates()
	{
		FEditorDelegates::MapChange.RemoveAll(this);
		FEditorDelegates::NewCurrentLevel.RemoveAll(this);
		FEditorDelegates::OnMapOpened.RemoveAll(this);

		FWorldDelegates::LevelAddedToWorld.RemoveAll(this);
		FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);
		FCoreDelegates::OnActorLabelChanged.RemoveAll(this);
		USelection::SelectionChangedEvent.RemoveAll(this);
		USelection::SelectObjectEvent.RemoveAll(this);

		if (GEngine)
		{
			GEngine->OnLevelActorListChanged().RemoveAll(this);
			GEngine->OnLevelActorAdded().RemoveAll(this);
			GEngine->OnLevelActorDeleted().RemoveAll(this);
			GEngine->OnActorMoved().RemoveAll(this);
		}	
		
		if (GEditor && UObjectInitialized())
		{
			GEditor->OnHLODActorMoved().RemoveAll(this);
			GEditor->OnHLODActorAdded().RemoveAll(this);
			GEditor->OnHLODActorMarkedDirty().RemoveAll(this);
			GEditor->OnHLODLevelsArrayChanged().RemoveAll(this);
			GEditor->OnHLODActorRemovedFromCluster().RemoveAll(this);

			// Deregister for Undo callbacks
			GEditor->UnregisterForUndo(this);
		}
	}

	void SHLODOutliner::ForceViewLODActor()
	{
		if (CurrentWorld)
		{
			const FScopedTransaction Transaction(LOCTEXT("UndoAction_LODLevelForcedView", "LOD Level Forced View"));

			// This call came from a context menu
			auto SelectedItems = TreeView->GetSelectedItems();

			// Loop over all selected items (context menu can't be called with multiple items selected that aren't of the same types)
			for (auto SelectedItem : SelectedItems)
			{
				FLODActorItem* ActorItem = (FLODActorItem*)(SelectedItem.Get());

				if (ActorItem->LODActor.IsValid())
				{
					ActorItem->LODActor->Modify();
					ActorItem->LODActor->ToggleForceView();
				}
			}
		}
	}

	bool SHLODOutliner::AreHLODsBuild() const
	{
		bool bHLODsBuild = true;
		for (bool Build : LODLevelBuildFlags)
		{
			bHLODsBuild &= Build;
		}

		return (LODLevelTransitionScreenSizes.Num() > 0 && bHLODsBuild);
	}

	void SHLODOutliner::HandleForcedLevelSliderValueChanged(float NewValue)
	{
		ForcedLODSliderValue = NewValue;					
	}

	void SHLODOutliner::HandleForcedLevelSliderCaptureBegin()
	{
		bForcedSliderValueUpdating = true;
	}

	void SHLODOutliner::HandleForcedLevelSliderCaptureEnd()
	{	
		bForcedSliderValueUpdating = false;		
		ForcedLODSliderValue = ((1.0f / (LODLevelTransitionScreenSizes.Num())) * (ForcedLODLevel + 1));
	}

	float SHLODOutliner::HandleForcedLevelSliderValue() const
	{
		return ForcedLODSliderValue;
	}

	FText SHLODOutliner::HandleForceLevelText() const
	{
		return FText::FromString(FString("Forced viewing level: ") + ( (ForcedLODLevel == -1) ? FString("None") : FString::FromInt(ForcedLODLevel)));
	}

	bool SHLODOutliner::CanHLODLevelBeForced(const uint32 LODLevel) const
	{
		return LODLevelBuildFlags[LODLevel];
	}

	void SHLODOutliner::RestoreForcedLODLevel(const uint32 LODLevel)
	{
		if (LODLevel == -1)
		{
			return;
		}

		if (CurrentWorld)
		{
			for (auto LevelActors : LODLevelActors)
			{
				for (auto LODActor : LevelActors)
				{
					if (LODActor->LODLevel == LODLevel + 1)
					{
						LODActor->SetForcedView(false);
					}
					else
					{
						LODActor->SetHiddenFromEditorView(false, LODLevel + 1);
					}
				}
			}
		}
	}

	void SHLODOutliner::SetForcedLODLevel(const uint32 LODLevel)
	{
		if (LODLevel == -1)
		{
			ForcedLODLevel = LODLevel;
			return;
		}

		if (CurrentWorld)
		{
			auto Level = CurrentWorld->GetCurrentLevel();
			for (auto LevelActors : LODLevelActors)
			{
				for (auto LODActor : LevelActors )
				{
					if (LODActor->LODLevel == LODLevel + 1)
					{
						LODActor->SetForcedView(true);
					}
					else
					{
						LODActor->SetHiddenFromEditorView(true, LODLevel + 1);
					}
				}
			}
		}
		ForcedLODLevel = LODLevel;
	}

	void SHLODOutliner::ResetLODLevelForcing()
	{
		RestoreForcedLODLevel(ForcedLODLevel);
		SetForcedLODLevel(-1);
		ForcedLODSliderValue = 0.0f;
	}

	void SHLODOutliner::CreateHierarchicalVolumeForActor()
	{		
		// This call came from a context menu
		auto SelectedItems = TreeView->GetSelectedItems();

		// Loop over all selected items (context menu can't be called with multiple items selected that aren't of the same types)
		for (auto SelectedItem : SelectedItems)
		{
			FLODActorItem* ActorItem = (FLODActorItem*)(SelectedItem.Get());
			ALODActor* LODActor = ActorItem->LODActor.Get();

			AHierarchicalLODVolume* Volume = HierarchicalLODUtilities->CreateVolumeForLODActor(LODActor, CurrentWorld);
			check(Volume);
		}		
	}

	void SHLODOutliner::BuildLODActor()
	{
		if (CurrentWorld)
		{
			// This call came from a context menu
			auto SelectedItems = TreeView->GetSelectedItems();
			
			// Loop over all selected items (context menu can't be called with multiple items selected that aren't of the same types)
			for (auto SelectedItem : SelectedItems )
			{
				FLODActorItem* ActorItem = (FLODActorItem*)(SelectedItem.Get());
				if (ActorItem->LODActor->HasValidSubActors())
				{
					auto Parent = ActorItem->GetParent();

					ITreeItem::TreeItemType Type = Parent->GetTreeItemType();
					if (Type == ITreeItem::HierarchicalLODLevel)
					{
						FLODLevelItem* LevelItem = (FLODLevelItem*)(Parent.Get());
						CurrentWorld->HierarchicalLODBuilder->BuildMeshForLODActor(ActorItem->LODActor.Get(), LevelItem->LODLevelIndex);
					}
				}
			}
			
			SetForcedLODLevel(ForcedLODLevel);
			TreeView->RequestScrollIntoView(SelectedItems[0]);
		}
		
		// Show message log if there was an HLOD message
		FMessageLog("HLODResults").Open();		
	}

	void SHLODOutliner::RebuildLODActor()
	{
		if (CurrentWorld)
		{
			// This call came from a context menu
			auto SelectedItems = TreeView->GetSelectedItems();

			// Loop over all selected items (context menu can't be called with multiple items selected that aren't of the same types)
			for (auto SelectedItem : SelectedItems)
			{
				FLODActorItem* ActorItem = (FLODActorItem*)(SelectedItem.Get());
				if (ActorItem->LODActor->HasValidSubActors())
				{
					auto Parent = ActorItem->GetParent();

					ITreeItem::TreeItemType Type = Parent->GetTreeItemType();
					if (Type == ITreeItem::HierarchicalLODLevel)
					{
						FLODLevelItem* LevelItem = (FLODLevelItem*)(Parent.Get());
						ActorItem->LODActor->SetIsDirty(true);
						CurrentWorld->HierarchicalLODBuilder->BuildMeshForLODActor(ActorItem->LODActor.Get(), LevelItem->LODLevelIndex);
					}
				}
			}

			SetForcedLODLevel(ForcedLODLevel);
			TreeView->RequestScrollIntoView(SelectedItems[0]);
		}

		// Show message log if there was an HLOD message
		FMessageLog("HLODResults").Open();
	}

	void SHLODOutliner::SelectLODActor()
	{
		if (CurrentWorld)
		{
			// This call came from a context menu
			auto SelectedItems = TreeView->GetSelectedItems();

			// Empty selection and setup for multi-selection
			EmptySelection();
			StartSelection();


			bool bChanged = false;
			// Loop over all selected items (context menu can't be called with multiple items selected that aren't of the same types)
			for (auto SelectedItem : SelectedItems)
			{
				FLODActorItem* ActorItem = (FLODActorItem*)(SelectedItem.Get());

				if (ActorItem->LODActor.IsValid())
				{				
					SelectActorInViewport(ActorItem->LODActor.Get(), 0);			
					bChanged = true;
				}
			}
			
			// Done selecting actors
			EndSelection(bChanged);
		}
	}

	void SHLODOutliner::DeleteCluster()
	{
		// This call came from a context menu
		auto SelectedItems = TreeView->GetSelectedItems();

		TArray<AHLODSelectionActor*> SelectionActorsToDelete;
		// Loop over all selected items (context menu can't be called with multiple items selected that aren't of the same types)
		for (auto SelectedItem : SelectedItems)
		{
			FLODActorItem* ActorItem = (FLODActorItem*)(SelectedItem.Get());
			ALODActor* LODActor = ActorItem->LODActor.Get();			
						
			AHLODSelectionActor** SelectionActorPtr = SelectionActors.FindByPredicate([LODActor](const AHLODSelectionActor* Actor) { return Actor->RepresentedActor.Get() == LODActor; });

			if (SelectionActorPtr)
			{
				SelectionActorsToDelete.AddUnique(*SelectionActorPtr);
			}

			HierarchicalLODUtilities->DestroyLODActor(LODActor);
		}

		for (AHLODSelectionActor* BoundsActor : SelectionActorsToDelete)
		{
			if (BoundsActor && BoundsActor->IsValidLowLevel())
			{
				SelectionActors.Remove(BoundsActor);
				CurrentWorld->DestroyActor(BoundsActor);
			}
		}

		ResetLODLevelForcing();
		FullRefresh();
	}

	void SHLODOutliner::RemoveStaticMeshActorFromCluster()
	{
		if (CurrentWorld)
		{
			const FScopedTransaction Transaction(LOCTEXT("UndoAction_RemoveStaticMeshActorFromCluster", "Removed Static Mesh Actor From Cluster"));

			// This call came from a context menu
			auto SelectedItems = TreeView->GetSelectedItems();

			// Loop over all selected items (context menu can't be called with multiple items selected that aren't of the same types)
			for (auto SelectedItem : SelectedItems)
			{
				FStaticMeshActorItem* ActorItem = (FStaticMeshActorItem*)(SelectedItem.Get());
				auto Parent = ActorItem->GetParent();

				ITreeItem::TreeItemType Type = Parent->GetTreeItemType();
				if (Type == ITreeItem::HierarchicalLODActor)
				{
					AActor* Actor = ActorItem->StaticMeshActor.Get();
					
					if (HierarchicalLODUtilities->RemoveActorFromCluster(Actor))
					{
						PendingActions.Emplace(FOutlinerAction::RemoveItem, SelectedItem);
					}
				}
			}
		}
	}

	void SHLODOutliner::ExcludeFromClusterGeneration()
	{
		// This call came from a context menu
		auto SelectedItems = TreeView->GetSelectedItems();

		// Loop over all selected items (context menu can't be called with multiple items selected that aren't of the same types)
		for (auto SelectedItem : SelectedItems)
		{
			FStaticMeshActorItem* ActorItem = (FStaticMeshActorItem*)(SelectedItem.Get());
			HierarchicalLODUtilities->ExcludeActorFromClusterGeneration(ActorItem->StaticMeshActor.Get());
		}		
	}

	void SHLODOutliner::RemoveLODActorFromCluster()
	{
		if (CurrentWorld)
		{
			// This call came from a context menu
			auto SelectedItems = TreeView->GetSelectedItems();

			// Loop over all selected items (context menu can't be called with multiple items selected that aren't of the same types)
			for (auto SelectedItem : SelectedItems)
			{
				FLODActorItem* ActorItem = (FLODActorItem*)(SelectedItem.Get());
				auto Parent = ActorItem->GetParent();

				ITreeItem::TreeItemType Type = Parent->GetTreeItemType();
				if (Type == ITreeItem::HierarchicalLODActor)
				{
					AActor* Actor = ActorItem->LODActor.Get();
					checkf(Actor != nullptr, TEXT("Invalid actor in tree view"));
					
					if (HierarchicalLODUtilities->RemoveActorFromCluster(Actor))
					{
						PendingActions.Emplace(FOutlinerAction::RemoveItem, SelectedItem);
					}					
				}
			}
		}
	}

	void SHLODOutliner::SelectContainedActors()
	{
		// This call came from a context menu
		auto SelectedItems = TreeView->GetSelectedItems();

		// Empty selection and setup for multi-selection
		EmptySelection();
		StartSelection();

		// Loop over all selected items (context menu can't be called with multiple items selected that aren't of the same types)
		for (auto SelectedItem : SelectedItems)
		{
			FLODActorItem* ActorItem = (FLODActorItem*)(SelectedItem.Get());

			ALODActor* LODActor = ActorItem->LODActor.Get();
			SelectContainedActorsInViewport(LODActor, 0);
		}

		// Done selecting actors
		EndSelection(true);
	}

	void SHLODOutliner::UpdateDrawDistancesForLODLevel(const uint32 LODLevelIndex)
	{
		if (CurrentWorld)
		{
			// Loop over all (streaming-)levels in the world
			for (ULevel* Level : CurrentWorld->GetLevels())
			{
				// For each LOD actor in the world update the drawing/transition distance
				for (AActor* Actor : Level->Actors)
				{
					ALODActor* LODActor = Cast<ALODActor>(Actor);
					if (LODActor)
					{
						if (LODActor->LODLevel == LODLevelIndex + 1)
						{
							if (!LODActor->IsDirty() && LODActor->GetStaticMeshComponent())
							{
								const float ScreenSize = LODActor->bOverrideTransitionScreenSize ? LODActor->TransitionScreenSize : LODLevelTransitionScreenSizes[LODLevelIndex];
								LODActor->RecalculateDrawingDistance(ScreenSize);
							}
						}
					}
				}
			}
		}
	}

	void SHLODOutliner::RemoveLODLevelActors(const int32 HLODLevelIndex)
	{
		if (CurrentWorld)
		{
			HierarchicalLODUtilities->DeleteLODActorsInHLODLevel(CurrentWorld, HLODLevelIndex);
		}
	}

	TSharedRef<ITableRow> SHLODOutliner::OnOutlinerGenerateRow(FTreeItemPtr InTreeItem, const TSharedRef<STableViewBase>& OwnerTable)
	{
		TSharedRef<ITableRow> Widget = SNew(SHLODWidgetItem, OwnerTable)
			.TreeItemToVisualize(InTreeItem)
			.Outliner(this)
			.World(CurrentWorld);

		return Widget;
	}

	void SHLODOutliner::OnOutlinerGetChildren(FTreeItemPtr InParent, TArray<FTreeItemPtr>& OutChildren)
	{		
		for (auto& WeakChild : InParent->GetChildren())
		{
			auto Child = WeakChild.Pin();
			// Should never have bogus entries in this list
			check(Child.IsValid());
			OutChildren.Add(Child);
		}
	}

	void SHLODOutliner::OnOutlinerSelectionChanged(FTreeItemPtr TreeItem, ESelectInfo::Type SelectInfo)
	{
		if (SelectInfo == ESelectInfo::Direct)
		{
			return;
		}

		TArray<FTreeItemPtr> NewSelectedNodes = TreeView->GetSelectedItems();

		EmptySelection();
		//TreeView->ClearSelection();

		// Loop over previously retrieve lsit of selected nodes
		StartSelection();

		bool bChanged = false;

		for (FTreeItemPtr SelectedItem : NewSelectedNodes)
		{
			if (SelectedItem.IsValid())
			{
				ITreeItem::TreeItemType Type = SelectedItem->GetTreeItemType();
				switch (Type)
				{
					case ITreeItem::HierarchicalLODLevel:
					{
						// No functionality for select HLOD level items
						break;
					}

					case ITreeItem::HierarchicalLODActor:
					{
						FLODActorItem* ActorItem = (FLODActorItem*)(SelectedItem.Get());
						if (ActorItem->GetParent()->GetTreeItemType() == ITreeItem::HierarchicalLODLevel)
						{
							SelectActorInViewport(ActorItem->LODActor.Get(), 0);
							bChanged = true;
						}
						break;
					}

					case ITreeItem::StaticMeshActor:
					{
						FStaticMeshActorItem* StaticMeshActorItem = (FStaticMeshActorItem*)(SelectedItem.Get());
						SelectActorInViewport(StaticMeshActorItem->StaticMeshActor.Get(), 0);
						bChanged = true;
						break;
					}
				}
			}
		}
		EndSelection(bChanged);

		SelectedNodes = TreeView->GetSelectedItems();
	}

	void SHLODOutliner::OnOutlinerDoubleClick(FTreeItemPtr TreeItem)
	{
		ITreeItem::TreeItemType Type = TreeItem->GetTreeItemType();
		const bool bActiveViewportOnly = false;
		
		switch (Type)
		{
			case ITreeItem::HierarchicalLODLevel:
			{
				break;
			}

			case ITreeItem::HierarchicalLODActor:
			{
				FLODActorItem* ActorItem = (FLODActorItem*)(TreeItem.Get());
				SelectActorInViewport(ActorItem->LODActor.Get(), 0);
				GEditor->MoveViewportCamerasToActor(*ActorItem->LODActor.Get(), bActiveViewportOnly);
				break;
			}

			case ITreeItem::StaticMeshActor:
			{
				FStaticMeshActorItem* StaticMeshActorItem = (FStaticMeshActorItem*)(TreeItem.Get());
				SelectActorInViewport(StaticMeshActorItem->StaticMeshActor.Get(), 0);
				GEditor->MoveViewportCamerasToActor(*StaticMeshActorItem->StaticMeshActor.Get(), bActiveViewportOnly);
				break;
			}
		}	
	}

	TSharedPtr<SWidget> SHLODOutliner::OnOpenContextMenu()
	{
		if (!CurrentWorld)
		{
			return nullptr;
		}

		// Build up the menu for a selection
		const bool bCloseAfterSelection = true;
		TSharedPtr<FExtender> Extender = MakeShareable(new FExtender);

		FMenuBuilder MenuBuilder(bCloseAfterSelection, TSharedPtr<FUICommandList>(), Extender);

		// Multi-selection support, check if all selected items are of the same type, if so return the appropriate context menu
		auto SelectedItems = TreeView->GetSelectedItems();
		ITreeItem::TreeItemType Type = ITreeItem::Invalid;
		bool bSameType = true;
		for (int32 SelectedIndex = 0; SelectedIndex < SelectedItems.Num(); ++SelectedIndex)
		{
			if (SelectedIndex == 0)
			{
				Type = SelectedItems[SelectedIndex]->GetTreeItemType();
			}
			else
			{
				// Not all of the same types
				if (SelectedItems[SelectedIndex]->GetTreeItemType() != Type)
				{
					bSameType = false; 
					break;
				}
			}
		}

		// Currently no context menu actions for ITreeItem::HierarchicalLODLevel type
		if (SelectedItems.Num() && bSameType && Type != ITreeItem::HierarchicalLODLevel)
		{
			TreeView->GetSelectedItems()[0]->GenerateContextMenu(MenuBuilder, *this);
			return MenuBuilder.MakeWidget();
		}

		return TSharedPtr<SWidget>();
	}

	void SHLODOutliner::OnItemExpansionChanged(FTreeItemPtr TreeItem, bool bIsExpanded)
	{
		TreeItem->bIsExpanded = bIsExpanded;

		// Expand any children that are also expanded
		for (auto WeakChild : TreeItem->GetChildren())
		{
			auto Child = WeakChild.Pin();
			if (Child->bIsExpanded)
			{
				TreeView->SetItemExpansion(Child, true);
			}
		}
	}

	void SHLODOutliner::StartSelection()
	{
		GEditor->GetSelectedActors()->BeginBatchSelectOperation();
	}

	void SHLODOutliner::EmptySelection()
	{
		GEditor->SelectNone(false, true, true);
		DestroySelectionActors();
	}

	void SHLODOutliner::DestroySelectionActors()
	{
		if (CurrentWorld)
		{
			for (AHLODSelectionActor* BoundsActor : SelectionActors)
			{
				if (BoundsActor && BoundsActor->IsValidLowLevel())
				{
					CurrentWorld->DestroyActor(BoundsActor);
				}
			}
		}
		SelectionActors.Empty();
	}

	void SHLODOutliner::SelectActorInViewport(AActor* Actor, const uint32 SelectionDepth)
	{
		GEditor->SelectActor(Actor, true, false);

		if (Actor->IsA<ALODActor>() && SelectionDepth == 0)
		{
			CreateBoundingSphereForActor(Actor);
		}
	}

	void SHLODOutliner::SelectLODActorAndContainedActorsInViewport(ALODActor* LODActor, const uint32 SelectionDepth)
	{
		TArray<AActor*> SubActors;
		HierarchicalLODUtilities->ExtractStaticMeshActorsFromLODActor(LODActor, SubActors);
		for (AActor* SubActor : SubActors)
		{
			SelectActorInViewport(SubActor, SelectionDepth + 1);
		}

		SelectActorInViewport(LODActor, SelectionDepth);
	}

	void SHLODOutliner::SelectContainedActorsInViewport(ALODActor* LODActor, const uint32 SelectionDepth /*= 0*/)
	{
		TArray<AActor*> SubActors;
		HierarchicalLODUtilities->ExtractStaticMeshActorsFromLODActor(LODActor, SubActors);
		for (AActor* SubActor : SubActors)
		{
			SelectActorInViewport(SubActor, SelectionDepth + 1);
		}
	}

	UDrawSphereComponent* SHLODOutliner::CreateBoundingSphereForActor(AActor* Actor)
	{
		if (CurrentWorld)
		{
			AHLODSelectionActor* SelectionActor = CurrentWorld->SpawnActorDeferred<AHLODSelectionActor>(AHLODSelectionActor::StaticClass(), FTransform());
			SelectionActor->ClearFlags(RF_Public | RF_Standalone);
			SelectionActor->SetFlags(RF_Transient);
			SelectionActor->RepresentedActor = Actor;

			UDrawSphereComponent* BoundSphereSpawned = SelectionActor->GetDrawSphereComponent();
			BoundSphereSpawned->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
			BoundSphereSpawned->RegisterComponent();

			FVector Origin, Extent;
			FBox BoundingBox = Actor->GetComponentsBoundingBox(true);
			BoundSphereSpawned->SetWorldLocation(BoundingBox.GetCenter());
			BoundSphereSpawned->SetSphereRadius(BoundingBox.GetExtent().GetAbsMax());
			BoundSphereSpawned->ShapeColor = FColor::Red;		

			SelectionActors.Add(SelectionActor);

			return BoundSphereSpawned;
		}

		return nullptr;
	}

	void SHLODOutliner::EndSelection(const bool bChange)
	{
		// Commit selection changes
		GEditor->GetSelectedActors()->EndBatchSelectOperation();

		if (bChange)
		{
			// Fire selection changed event
			GEditor->NoteSelectionChange();
		}
	}

	void SHLODOutliner::OnLevelSelectionChanged(UObject* Obj)
	{		
		USelection* Selection = Cast<USelection>(Obj);
		AActor* SelectedActor = Cast<AActor>(Obj);
		TreeView->ClearSelection();
		if (Selection)
		{
			int32 NumSelected = Selection->Num();
			// TODO changes this for multiple selection support
			for (int32 SelectionIndex = 0; SelectionIndex < NumSelected; ++SelectionIndex)
			{
				AActor* Actor = Cast<AActor>(Selection->GetSelectedObject(SelectionIndex));
				if (Actor)
				{
					auto Item = TreeItemsMap.Find(Actor);
					if (Item)
					{
						SelectItemInTree(*Item);

						if (Item->Get()->GetTreeItemType() == ITreeItem::StaticMeshActor)
						{
							DestroySelectionActors();
						}

						TreeView->RequestScrollIntoView(*Item);
					}
					else
					{
						DestroySelectionActors();
					}
				}
			}			
		}
		else if (SelectedActor)
		{
			auto Item = TreeItemsMap.Find(SelectedActor);
			if (Item)
			{
				SelectItemInTree(*Item);

				if (Item->Get()->GetTreeItemType() == ITreeItem::StaticMeshActor)
				{
					DestroySelectionActors();
				}

				TreeView->RequestScrollIntoView(*Item);
			}	
			else
			{
				DestroySelectionActors();
			}
		}
	}

	void SHLODOutliner::OnLevelAdded(ULevel* InLevel, UWorld* InWorld)
	{
		ResetCachedData();
		FullRefresh();
	}

	void SHLODOutliner::OnLevelRemoved(ULevel* InLevel, UWorld* InWorld)
	{
		ResetCachedData();
		FullRefresh();
	}

	void SHLODOutliner::OnLevelActorsAdded(AActor* InActor)
	{
		if (InActor->GetWorld() == CurrentWorld && !InActor->IsA<AHLODSelectionActor>() && !InActor->IsA<AWorldSettings>())
		{
			FullRefresh();
		}	
	}

	void SHLODOutliner::OnLevelActorsRemoved(AActor* InActor)
	{
		if (!InActor->IsA<AHLODSelectionActor>() && !InActor->IsA<AWorldSettings>())
		{			
			// Remove InActor from LOD actor which contains it
			for (TArray<ALODActor*>& ActorArray : LODLevelActors)
			{				
				for (ALODActor* Actor : ActorArray)
				{
					// Check if actor is not null due to Destroy Actor
					if (Actor)
					{
						checkf(Actor->IsValidLowLevel(), TEXT("Invalid LODActor found in ActorArray"));
						Actor->CleanSubActorArray();
						const bool bRemovedSubActor = Actor->RemoveSubActor(InActor);

						if (bRemovedSubActor && Actor->SubActors.Num() == 0)
						{
							HierarchicalLODUtilities->DestroyCluster(Actor);
							Actor->GetWorld()->DestroyActor(Actor);						
						}
					}
				}
			}
			FullRefresh();
		} 
	}
	
	void SHLODOutliner::OnActorLabelChanged(AActor* ChangedActor)
	{
		if (!ChangedActor->IsA<AHLODSelectionActor>())
		{
			FullRefresh();
		}	
	}

	void SHLODOutliner::OnMapChange(uint32 MapFlags)
	{
		ResetCachedData();
		FullRefresh();
	}

	void SHLODOutliner::OnNewCurrentLevel()
	{	
		ResetCachedData();
		FullRefresh();
	}

	void SHLODOutliner::OnMapLoaded(const FString& Filename, bool bAsTemplate)
	{
		ResetCachedData();
		FullRefresh();
	}

	void SHLODOutliner::OnHLODActorMovedEvent(const AActor* InActor, const AActor* ParentActor)
	{
		FTreeItemPtr* TreeItem = TreeItemsMap.Find(InActor);
		FTreeItemPtr* ParentItem = TreeItemsMap.Find(ParentActor);
		if (TreeItem && ParentItem)
		{			
			PendingActions.Emplace(FOutlinerAction::MoveItem, *TreeItem, *ParentItem);

			auto CurrentParent = (*TreeItem)->GetParent(); 

			if (CurrentParent.IsValid())
			{
				if (CurrentParent->GetTreeItemType() == ITreeItem::HierarchicalLODActor)
				{
					FLODActorItem* ParentLODActorItem = (FLODActorItem*)CurrentParent.Get();
					if (!ParentLODActorItem->LODActor->HasAnySubActors())
					{
						HierarchicalLODUtilities->DestroyLODActor(ParentLODActorItem->LODActor.Get());
						PendingActions.Emplace(FOutlinerAction::RemoveItem, CurrentParent);
					}
				}
			}
		}
	}

	void SHLODOutliner::OnActorMovedEvent(AActor* InActor)
	{
		if (InActor->IsA<ALODActor>())
		{
			return;
		}

		ALODActor* ParentActor = HierarchicalLODUtilities->GetParentLODActor(InActor);
		if (ParentActor)
		{
			ParentActor->Modify();
			ParentActor->SetIsDirty(true);
		}		
	}

	void SHLODOutliner::OnHLODActorAddedEvent(const AActor* InActor, const AActor* ParentActor)
	{
		checkf(InActor != nullptr, TEXT("Invalid InActor found"));
		checkf(ParentActor != nullptr, TEXT("Invalid ParentActor found"));

		FTreeItemPtr* ParentItem = TreeItemsMap.Find(ParentActor);
		if (ParentItem->IsValid())
		{
			const ALODActor* ParentLODActor = Cast<ALODActor>(ParentActor);
			
			FTreeItemPtr* ChildItemPtr = TreeItemsMap.Find(InActor);
			if (ChildItemPtr)
			{
				if (!InActor->IsA<ALODActor>())
				{
					PendingActions.Emplace(FOutlinerAction::MoveItem, *ChildItemPtr, *ParentItem);
				}
				else
				{
					FullRefresh();

					// TODO, handle duplicate entries of same actor in tree view
					/*// Add child item for the new LOD mesh actor
					const ALODActor* LODActor = Cast<ALODActor>(InActor);
					FTreeItemRef ChildItem = MakeShareable(new FLODActorItem(LODActor));
					AllNodes.Add(ChildItem->AsShared());
					(*ParentItem)->AddChild(ChildItem);

					TreeView->RequestTreeRefresh();*/
				}
			}
			else
			{
				// Add child item for the new static mesh actor
				FTreeItemRef ChildItem = MakeShareable(new FStaticMeshActorItem(InActor));
				AllNodes.Add(ChildItem->AsShared());
				PendingActions.Emplace(FOutlinerAction::AddItem, ChildItem, *ParentItem);
			}			

			// Set build flags according to whether or not this LOD actor is dirty 
			LODLevelBuildFlags[ParentLODActor->LODLevel - 1] &= !ParentLODActor->IsDirty();
		}
	}

	void SHLODOutliner::OnHLODActorMarkedDirtyEvent(ALODActor* InActor)
	{	
		bool check = true;
	}

	void SHLODOutliner::OnHLODTransitionScreenSizeChangedEvent()
	{
		if (CurrentWorld)
		{			
			int32 MaxLODLevel = FMath::Min(CurrentWorldSettings->HierarchicalLODSetup.Num(), LODLevelTransitionScreenSizes.Num());
			for (int32 LODLevelIndex = 0; LODLevelIndex < MaxLODLevel; ++LODLevelIndex)
			{
				if (LODLevelTransitionScreenSizes[LODLevelIndex] != CurrentWorldSettings->HierarchicalLODSetup[LODLevelIndex].TransitionScreenSize)
				{
					LODLevelTransitionScreenSizes[LODLevelIndex] = CurrentWorldSettings->HierarchicalLODSetup[LODLevelIndex].TransitionScreenSize;
					UpdateDrawDistancesForLODLevel(LODLevelIndex);
				}
			}
		}
	}

	void SHLODOutliner::OnHLODLevelsArrayChangedEvent()
	{
		if (CurrentWorld)
		{
			FullRefresh();
		}
	}

	void SHLODOutliner::OnHLODActorRemovedFromClusterEvent(const AActor* InActor, const AActor* ParentActor)
	{
		FTreeItemPtr* TreeItem = TreeItemsMap.Find(InActor);
		FTreeItemPtr* ParentItem = TreeItemsMap.Find(ParentActor);
		if (TreeItem->IsValid() && ParentItem->IsValid())
		{
			checkf((*TreeItem)->GetTreeItemType() == ITreeItem::StaticMeshActor, TEXT("Incorrect InActor"));
			checkf((*ParentItem)->GetTreeItemType() == ITreeItem::HierarchicalLODActor, TEXT("Incorrect ParentActor"));
			PendingActions.Emplace(FOutlinerAction::RemoveItem, *TreeItem);
		}
	}

	void SHLODOutliner::FullRefresh()
	{		
		bNeedsRefresh = true;
	}
	
	const bool SHLODOutliner::UpdateCurrentWorldAndSettings()
	{
		CurrentWorld = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE)
			{
				CurrentWorld = Context.World();
				break;
			}
			else if (Context.WorldType == EWorldType::Editor)
			{
				CurrentWorld = Context.World();
			}
		}	

		if (CurrentWorld)
		{
			// Retrieve current world settings
			CurrentWorldSettings = CurrentWorld->GetWorldSettings();
			checkf(CurrentWorldSettings != nullptr, TEXT("CurrentWorld (%s) does not contain a valid WorldSettings actor"), *CurrentWorld->GetName());
						
			// Update settings view
			SettingsView->SetObject(CurrentWorldSettings);
		}
		

		return (CurrentWorld != nullptr);
	}

	void SHLODOutliner::Populate()
	{
		ResetCachedData();
		const bool bUpdatedWorld = UpdateCurrentWorldAndSettings();
		checkf(bUpdatedWorld == true, TEXT("Could not find UWorld* instance in Engine world contexts"));

		TArray<FTreeItemRef> LevelNodes;
		if (CurrentWorldSettings)
		{
			// Iterate over all LOD levels (Number retrieved from world settings) and add Treeview items for them
			const uint32 LODLevels = CurrentWorldSettings->HierarchicalLODSetup.Num();			
			for (uint32 LODLevelIndex = 0; LODLevelIndex < LODLevels; ++LODLevelIndex)
			{
				FTreeItemRef LevelItem = MakeShareable(new FLODLevelItem(LODLevelIndex));

				PendingActions.Emplace(FOutlinerAction::AddItem, LevelItem);

				// Add new HLOD level item to maps and arrays holding cached items
				LevelNodes.Add(LevelItem->AsShared());
				HLODTreeRoot.Add(LevelItem->AsShared());
				AllNodes.Add(LevelItem->AsShared());

				// Initialize lod level actors/screen size and build flag
				LODLevelBuildFlags.Add(true);
				LODLevelActors.AddDefaulted();					
				LODLevelTransitionScreenSizes.Add(CurrentWorldSettings->HierarchicalLODSetup[LODLevelIndex].TransitionScreenSize);

				TreeItemsMap.Add(LevelItem->GetID(), LevelItem);

				// Expand level items by default
				LevelItem->bIsExpanded = true;
			}

			// Loop over all the levels in the current world
			for (ULevel* Level : CurrentWorld->GetLevels())
			{
				// Only handling visible levels (this is to allow filtering the HLOD outliner per level, should change when adding new sortable-column)
				if (Level->bIsVisible)
				{
					for (AActor* Actor : Level->Actors)
					{
						// Only handling LODActors
						if (Actor)
						{
							ALODActor* LODActor = Cast<ALODActor>(Actor);							
							// Add LOD Actor item to the treeview
							if (LODActor)
							{
								checkf((LODActor->LODLevel - 1) < LevelNodes.Num(), TEXT("LODActor (%s) found with LODLevel (%i) that is out of current WorldSettings range (%i)"), *LODActor->GetName(), LODActor->LODLevel - 1, LevelNodes.Num());
								
								// This is to prevent issues with the sub actor array due to deleted scene actors while the HLOD outliner was closed
								LODActor->CleanSubActorArray();

								// Set LOD parents here TODO remove if not needed anymore QQ
								LODActor->UpdateSubActorLODParents();

								FTreeItemRef Item = MakeShareable(new FLODActorItem(LODActor));
								AllNodes.Add(Item->AsShared());

								PendingActions.Emplace(FOutlinerAction::AddItem, Item, LevelNodes[LODActor->LODLevel - 1]);

								for (AActor* ChildActor : LODActor->SubActors)
								{
									if (ChildActor->IsA<ALODActor>())
									{
										FTreeItemRef ChildItem = MakeShareable(new FLODActorItem(CastChecked<ALODActor>(ChildActor)));
										AllNodes.Add(ChildItem->AsShared());
										Item->AddChild(ChildItem);
									}
									else
									{
										FTreeItemRef ChildItem = MakeShareable(new FStaticMeshActorItem(ChildActor));
										AllNodes.Add(ChildItem->AsShared());

										PendingActions.Emplace(FOutlinerAction::AddItem, ChildItem, Item);
									}
								}

								// Set build flags according to whether or not this LOD actor is dirty 
								LODLevelBuildFlags[LODActor->LODLevel - 1] &= !LODActor->IsDirty();
								// Add the actor to it's HLOD levels array
								LODLevelActors[LODActor->LODLevel - 1].Add(LODActor);
							}
						}
					}
				}
			}	

			// Take empty LOD levels into account for the build flags
			for (uint32 LODLevelIndex = 0; LODLevelIndex < LODLevels; ++LODLevelIndex)
			{
				if (LODLevelActors[LODLevelIndex].Num() == 0)
				{
					LODLevelBuildFlags[LODLevelIndex] = true;
				}
			}
		}

		// Request treeview UI item to refresh
		TreeView->RequestTreeRefresh();
		
		DestroySelectionActors();

		// Just finished refreshing
		bNeedsRefresh = false;
	}

	void SHLODOutliner::ResetCachedData()
	{
		HLODTreeRoot.Reset();
		TreeItemsMap.Reset();
		LODLevelBuildFlags.Reset();
		LODLevelTransitionScreenSizes.Reset();

		for (auto& ActorArray : LODLevelActors)
		{
			ActorArray.Reset();
		}

		LODLevelActors.Reset();
	}

	TMap<FTreeItemID, bool> SHLODOutliner::GetParentsExpansionState() const
	{
		FParentsExpansionState States;
		for (const auto& Pair : TreeItemsMap)
		{
			if (Pair.Value->GetChildren().Num())
			{
				States.Add(Pair.Key, Pair.Value->bIsExpanded);
			}
		}

		return States;
	}

	void SHLODOutliner::SetParentsExpansionState(const FParentsExpansionState& ExpansionStateInfo) const
	{
		for (const auto& Pair : TreeItemsMap)
		{
			auto& Item = Pair.Value;
			if (Item->GetChildren().Num())
			{
				const bool* bIsExpanded = ExpansionStateInfo.Find(Pair.Key);
				if (bIsExpanded)
				{
					TreeView->SetItemExpansion(Item, *bIsExpanded);
				}
				else
				{
					TreeView->SetItemExpansion(Item, Item->bIsExpanded);
				}
			}
		}
	}

	const bool SHLODOutliner::AddItemToTree(FTreeItemPtr InItem, FTreeItemPtr InParentItem)
	{
		const auto ItemID = InItem->GetID();

		TreeItemsMap.Add(ItemID, InItem);

		if (InParentItem.Get())
		{
			InParentItem->AddChild(InItem->AsShared());
		}		

		return true;
	}

	void SHLODOutliner::MoveItemInTree(FTreeItemPtr InItem, FTreeItemPtr InParentItem)
	{
		auto CurrentParent = InItem->Parent;
		if (CurrentParent.IsValid())
		{
			CurrentParent.Pin()->RemoveChild(InItem->AsShared());
		}

		if (InParentItem.Get())
		{
			InParentItem->AddChild(InItem->AsShared());
		}
	}

	void SHLODOutliner::RemoveItemFromTree(FTreeItemPtr InItem)
	{
		const int32 NumRemoved = TreeItemsMap.Remove(InItem->GetID());

		if (!NumRemoved)
		{
			return;
		}

		auto ParentItem = InItem->GetParent();
		if (ParentItem.IsValid())
		{
			ParentItem->RemoveChild(InItem->AsShared());
		}
	}

	void SHLODOutliner::SelectItemInTree(FTreeItemPtr InItem)
	{
		auto Parent = InItem->GetParent();
		while (Parent.IsValid() && !Parent->bIsExpanded)
		{
			Parent->bIsExpanded = true;
			TreeView->SetItemExpansion(Parent, true);
			Parent = InItem->GetParent();
		}
		TreeView->SetItemSelection(InItem, true);

		TreeView->RequestTreeRefresh();
	}
	
	FReply SHLODOutliner::RetrieveActors()
	{
		bNeedsRefresh = true;
		return FReply::Handled();
	}

	bool SHLODOutliner::OutlinerEnabled() const
	{
		bool bHLODEnabled = false;

		if (!bNeedsRefresh)
		{
			if (CurrentWorldSettings != nullptr)
			{
				bHLODEnabled = CurrentWorldSettings->bEnableHierarchicalLODSystem;
			}

			if (bHLODEnabled && CurrentWorld != nullptr)
			{
				bHLODEnabled = !HierarchicalLODUtilities->IsWorldUsedForStreaming(CurrentWorld);
			}
		}	

		return bHLODEnabled;
	}
};

#undef LOCTEXT_NAMESPACE
