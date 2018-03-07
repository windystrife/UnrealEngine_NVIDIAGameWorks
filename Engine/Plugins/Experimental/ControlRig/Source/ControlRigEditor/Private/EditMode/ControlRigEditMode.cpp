// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigEditMode.h"
#include "ControlRigEditModeToolkit.h"
#include "Toolkits/ToolkitManager.h"
#include "SControlRigEditModeTools.h"
#include "Containers/Algo/Transform.h"
#include "ControlRig.h"
#include "HitProxies.h"
#include "HierarchicalRig.h"
#include "HumanRig.h"
#include "ControlRigEditModeSettings.h"
#include "ISequencer.h"
#include "ControlRigSequence.h"
#include "ControlRigBindingTemplate.h"
#include "Sections/MovieSceneSpawnSection.h"
#include "MovieScene.h"
#include "EditorViewportClient.h"
#include "EditorModeManager.h"
#include "Engine/Selection.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRigCommands.h"
#include "SlateApplication.h"
#include "ModuleManager.h"
#include "ISequencerModule.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "ControlRigEditorModule.h"

FName FControlRigEditMode::ModeName("EditMode.ControlRig");

#define LOCTEXT_NAMESPACE "ControlRigEditMode"

/** Base class for ControlRig hit proxies */
struct HControlRigProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	HControlRigProxy(UControlRig* InControlRig, EHitProxyPriority Priority = HPP_Wireframe)
		: HHitProxy(Priority)
		, ControlRig(InControlRig)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}

	virtual bool AlwaysAllowsTranslucentPrimitives() const override
	{
		return true;
	}

	TWeakObjectPtr<UControlRig> ControlRig;
};

IMPLEMENT_HIT_PROXY(HControlRigProxy, HHitProxy);

/** Proxy for a manipulator */
struct HManipulatorNodeProxy : public HControlRigProxy
{
	DECLARE_HIT_PROXY();

	HManipulatorNodeProxy(UControlRig* InControlRig, FName InNodeName)
		: HControlRigProxy(InControlRig, HPP_Foreground)
		, NodeName(InNodeName)
	{}

	FName NodeName;
};

IMPLEMENT_HIT_PROXY(HManipulatorNodeProxy, HControlRigProxy);

FControlRigEditMode::FControlRigEditMode()
	: bIsTransacting(false)
	, bManipulatorMadeChange(false)
	, bSelectedNode(false)
	, bSelecting(false)
	, bSelectingByPath(false)
	, PivotTransform(FTransform::Identity)
{
	Settings = NewObject<UControlRigEditModeSettings>(GetTransientPackage(), *LOCTEXT("SettingsName", "Settings").ToString());
	Settings->AddToRoot();

	OnNodesSelectedDelegate.AddRaw(this, &FControlRigEditMode::HandleSelectionChanged);

	CommandBindings = MakeShareable(new FUICommandList);
	BindCommands();
}

FControlRigEditMode::~FControlRigEditMode()
{
	Settings->RemoveFromRoot();

	CommandBindings = nullptr;
}

void FControlRigEditMode::SetSequencer(TSharedPtr<ISequencer> InSequencer)
{
	static bool bRecursionGuard = false;
	if (!bRecursionGuard)
	{
		TGuardValue<bool> ScopeGuard(bRecursionGuard, true);

		Settings->Sequence = nullptr;

		WeakSequencer = InSequencer;
		StaticCastSharedPtr<SControlRigEditModeTools>(Toolkit->GetInlineContent())->SetSequencer(InSequencer);
		if (InSequencer.IsValid())
		{
			if (UControlRigSequence* Sequence = ExactCast<UControlRigSequence>(InSequencer->GetFocusedMovieSceneSequence()))
			{
				Settings->Sequence = Sequence;
				ReBindToActor();
			}
		}
	}
}

void FControlRigEditMode::SetObjects(const TArray<TWeakObjectPtr<>>& InSelectedObjects, const TArray<FGuid>& InObjectBindings)
{
	ControlRigs.Reset();

	check(InSelectedObjects.Num() == InObjectBindings.Num());

	ControlRigGuids = InObjectBindings;
	Algo::Transform(InSelectedObjects, ControlRigs, [](TWeakObjectPtr<> Object) { return TWeakObjectPtr<UControlRig>(Cast<UControlRig>(Object.Get())); });

	SetObjects_Internal();
}

void FControlRigEditMode::SetObjects_Internal()
{
	TArray<TWeakObjectPtr<>> SelectedObjects;
	Algo::TransformIf(ControlRigs, SelectedObjects, [](TWeakObjectPtr<> Object) { return Object.IsValid(); }, [](TWeakObjectPtr<> Object) { return TWeakObjectPtr<>(Object.Get()); });
	SelectedObjects.Insert(Settings, 0);

	StaticCastSharedPtr<SControlRigEditModeTools>(Toolkit->GetInlineContent())->SetDetailsObjects(SelectedObjects);
}

void FControlRigEditMode::HandleBindToActor(AActor* InActor, bool bFocus)
{
	static bool bRecursionGuard = false;
	if (!bRecursionGuard)
	{
		TGuardValue<bool> ScopeGuard(bRecursionGuard, true);

		FControlRigBindingTemplate::SetObjectBinding(InActor);

		if (WeakSequencer.IsValid())
		{
			TSharedRef<ISequencer> Sequencer = WeakSequencer.Pin().ToSharedRef();

			// Modify the sequence
			if (UControlRigSequence* Sequence = ExactCast<UControlRigSequence>(Sequencer->GetFocusedMovieSceneSequence()))
			{
				Sequence->Modify(false);

				// Also modify the binding tracks in the sequence, so bindings get regenerated to this actor
				UMovieScene* MovieScene = Sequence->GetMovieScene();
				for (UMovieSceneSection* Section : MovieScene->GetAllSections())
				{
					if (UMovieSceneSpawnSection* SpawnSection = Cast<UMovieSceneSpawnSection>(Section))
					{
						SpawnSection->TryModify(false);
					}
				}

				// now notify the sequence (will rebind when it re-evaluates
				Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately);

				// Force a rig evaluation here to make sure our manipulators are up to date
				if (ControlRigs.Num() > 0 && ControlRigs[0].Get())
				{
					if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(ControlRigs[0].Get()))
					{
						HierarchicalRig->PreEvaluate();
						HierarchicalRig->Evaluate();
						HierarchicalRig->PostEvaluate();
					}
				}

				// Now re-display our objects in the details panel (they may have changed)
				if (MovieScene->GetSpawnableCount() > 0)
				{
					FGuid SpawnableGuid = MovieScene->GetSpawnable(0).GetGuid();
					TWeakObjectPtr<> BoundObject = Sequencer->FindSpawnedObjectOrTemplate(SpawnableGuid);
					SetObjects(TArray<TWeakObjectPtr<>>({ BoundObject }), TArray<FGuid>({ SpawnableGuid }));
				}
			}

			if (bFocus && InActor)
			{
				const bool bNotifySelectionChanged = false;
				const bool bDeselectBSP = true;
				const bool bWarnAboutTooManyActors = false;
				const bool bSelectEvenIfHidden = true;

				// Select & focus the actor
				GEditor->GetSelectedActors()->Modify();
				GEditor->GetSelectedActors()->BeginBatchSelectOperation();
				GEditor->SelectNone(bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors);
				GEditor->SelectActor(InActor, true, bNotifySelectionChanged, bSelectEvenIfHidden);
				GEditor->Exec(InActor->GetWorld(), TEXT("CAMERA ALIGN ACTIVEVIEWPORTONLY"));
				GEditor->SelectNone(bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors);
				GEditor->GetSelectedActors()->EndBatchSelectOperation();
			}
		}
	}
}

void FControlRigEditMode::ReBindToActor()
{
	if (Settings->Actor.IsValid())
	{
		HandleBindToActor(Settings->Actor.Get(), false);
	}
}

bool FControlRigEditMode::UsesToolkits() const
{
	return true;
}

void FControlRigEditMode::Enter()
{
	// Call parent implementation
	FEdMode::Enter();

	if (!Toolkit.IsValid())
	{
		Toolkit = MakeShareable(new FControlRigEditModeToolkit);
	}

	Toolkit->Init(Owner->GetToolkitHost());

	SetObjects_Internal();
}

void FControlRigEditMode::Exit()
{
	if (bIsTransacting)
	{
		GEditor->EndTransaction();
		bIsTransacting = false;
		bManipulatorMadeChange = false;
	}

	if (Toolkit.IsValid())
	{
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
	}

	// Call parent implementation
	FEdMode::Exit();
}

static ETransformComponent WidgetModeToTransformComponent(FWidget::EWidgetMode WidgetMode)
{
	switch (WidgetMode)
	{
	case FWidget::WM_Translate:
		return ETransformComponent::Translation;
	case FWidget::WM_Rotate:
		return ETransformComponent::Rotation;
	case FWidget::WM_Scale:
		return ETransformComponent::Scale;
	case FWidget::WM_2D:
	case FWidget::WM_TranslateRotateZ:
	default:
		return ETransformComponent::None;
	}
}

void FControlRigEditMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	FEdMode::Tick(ViewportClient, DeltaTime);

	if (bSelectedNode)
	{
		// cycle the widget mode if it is not supported on this selection
		if (ControlRigs.Num() > 0 && ControlRigs[0].Get())
		{
			if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(ControlRigs[0].Get()))
			{
				if (SelectedNodes.Num() > 0)
				{
					FWidget::EWidgetMode CurrentMode = GetModeManager()->GetWidgetMode();
					bool bModeSupported = false;
					for (const FName& SelectedNode : SelectedNodes)
					{
						UControlManipulator* Manipulator = HierarchicalRig->FindManipulator(SelectedNode);
						if (Manipulator)
						{
							if (Manipulator->SupportsTransformComponent(WidgetModeToTransformComponent(CurrentMode)))
							{
								bModeSupported = true;
							}
						}
					}

					if (!bModeSupported)
					{
						GetModeManager()->CycleWidgetMode();
					}
				}

				ViewportClient->Invalidate();
			}
		}
		
		bSelectedNode = false;
	}

	// check if we need to change selection because we switched modes
	for (TWeakObjectPtr<UControlRig> ControlRig : ControlRigs)
	{
		if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(ControlRig.Get()))
		{
			TArray<FName> LocalSelectedNodes(SelectedNodes);
			for (const FName& SelectedNode : LocalSelectedNodes)
			{
				for (UControlManipulator* Manipulator : HierarchicalRig->Manipulators)
				{
					if (Manipulator)
					{
						if (Manipulator->Name == SelectedNode && !HierarchicalRig->IsManipulatorEnabled(Manipulator))
						{
							// node is selected but disabled, switch our selection
							SetNodeSelection(Manipulator->Name, false);
							if (UControlManipulator* CounterpartManipulator = HierarchicalRig->FindCounterpartManipulator(Manipulator))
							{
								SetNodeSelection(CounterpartManipulator->Name, true);
							}
						}
					}
				}
			}
		}
	}

	// If we have detached from sequencer, unbind the settings UI
	if (!WeakSequencer.IsValid() && Settings->Sequence != nullptr)
	{
		Settings->Sequence = nullptr;
		RefreshObjects();
	}

	// update the pivot transform of our selected objects (they could be animating)
	RecalcPivotTransform();

	// Tick manipulators
	for (TWeakObjectPtr<UControlRig> ControlRig : ControlRigs)
	{
		if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(ControlRig.Get()))
		{
			for (UControlManipulator* Manipulator : HierarchicalRig->Manipulators)
			{
				Manipulator->CurrentProximity = FMath::FInterpTo(Manipulator->CurrentProximity, Manipulator->TargetProximity, DeltaTime, 10.0f);
			}
		}
	}

	if (Settings->bDisplayTrajectories)
	{
		if (WeakSequencer.IsValid() && ControlRigGuids.Num() > 0 && ControlRigGuids[0].IsValid())
		{
			TSharedRef<ISequencer> Sequencer = WeakSequencer.Pin().ToSharedRef();
			UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
			float FrameInterval = MovieScene->GetFixedFrameInterval();
			float FrameSnap = MovieScene->GetFixedFrameInterval() == 0.0f ? 1.0f / 30.0f : FrameInterval;
			TrajectoryCache.Update(Sequencer, ControlRigGuids[0], MovieScene->GetPlaybackRange(), FrameSnap, DeltaTime, FApp::GetCurrentTime());
		}
	}
}

FTransform GetParentTransform(UControlManipulator* Manipulator, UHierarchicalRig* HierarchicalRig)
{
	if (Manipulator->bInLocalSpace)
	{
		const FAnimationHierarchy& Hierarchy = HierarchicalRig->GetHierarchy();
		int32 NodeIndex = Hierarchy.GetNodeIndex(Manipulator->Name);
		if (NodeIndex != INDEX_NONE)
		{
			FName ParentName = Hierarchy.GetParentName(NodeIndex);
			if (ParentName != NAME_None)
			{
				return HierarchicalRig->GetMappedGlobalTransform(ParentName);
			}
		}
	}

	return FTransform::Identity;
}

void FControlRigEditMode::RenderLimb(const FLimbControl& Limb, UHumanRig* HumanRig, FPrimitiveDrawInterface* PDI)
{
	// Look for manipulator of the IK target, we want its color
	UControlManipulator* TargetManip = HumanRig->FindManipulatorForNode(Limb.IKJointTargetName);

	// If we have a (colored) manipulator, and its enabled, draw the line
	UColoredManipulator* ColorManip = Cast<UColoredManipulator>(TargetManip);
	if (ColorManip && HumanRig->IsManipulatorEnabled(ColorManip))
	{
		FLinearColor DrawColor = IsNodeSelected(Limb.IKJointTargetName) ? ColorManip->SelectedColor : ColorManip->Color;
		DrawColor = (DrawColor * 0.5f); // Tone down color of manipulator a bit

		USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(HumanRig->GetBoundObject());
		FTransform ComponentTransform = SkelMeshComp ? SkelMeshComp->GetComponentTransform() : FTransform::Identity;

		// Get joint location
		const FVector JointLocation = ComponentTransform.TransformPosition(HumanRig->GetMappedGlobalTransform(Limb.IKChainName[1]).GetLocation());
		// Get handle location
		const FVector HandleLocation = ComponentTransform.TransformPosition(HumanRig->GetMappedGlobalTransform(Limb.IKJointTargetName).GetLocation());

		PDI->DrawLine(JointLocation, HandleLocation, DrawColor, SDPG_Foreground, 0.25f);
	}
}

void FControlRigEditMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	bool bRender = true;
	if (WeakSequencer.IsValid())
	{
		bRender = WeakSequencer.Pin()->GetPlaybackStatus() != EMovieScenePlayerStatus::Playing || Settings->bShowManipulatorsDuringPlayback;
	}

	// Force off manipulators if hide flag is set
	if (Settings->bHideManipulators)
	{
		bRender = false;
	}

	if (bRender)
	{
		FIntPoint MousePosition;
		FVector Origin;
		FVector Direction;
		Viewport->GetMousePos(MousePosition);
		View->DeprojectFVector2D(MousePosition, Origin, Direction);

		for (TWeakObjectPtr<UControlRig> ControlRig : ControlRigs)
		{
			if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(ControlRig.Get()))
			{
				// now get all node data
				const FAnimationHierarchy& Hierarchy = HierarchicalRig->GetHierarchy();
				const TArray<FNodeObject>& NodeObjects = Hierarchy.GetNodes();

				USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(HierarchicalRig->GetBoundObject());

				const FColor NormalColor = FColor(255, 255, 255, 255);
				const FColor SelectedColor = FColor(255, 0, 255, 255);
				const float GrabHandleSize = 5.0f;

				FTransform ComponentTransform = SkelMeshComp ? SkelMeshComp->GetComponentTransform() : FTransform::Identity;

				if (Settings->bDisplayHierarchy)
				{
					// each hierarchy node
					for (int32 NodeIndex = 0; NodeIndex < NodeObjects.Num(); ++NodeIndex)
					{
						const FNodeObject& CurrentNode = NodeObjects[NodeIndex];
						const FVector Location = ComponentTransform.TransformPosition(HierarchicalRig->GetMappedGlobalTransform(CurrentNode.Name).GetLocation());
						if (CurrentNode.ParentName != NAME_None)
						{
							const FVector ParentLocation = ComponentTransform.TransformPosition(HierarchicalRig->GetMappedGlobalTransform(CurrentNode.ParentName).GetLocation());
							PDI->DrawLine(Location, ParentLocation, SelectedColor, SDPG_Foreground);
						}

						PDI->DrawPoint(Location, NormalColor, GrabHandleSize, SDPG_Foreground);
					}
				}

				// First setup manipulator proximities
				if (!bIsTransacting)
				{
					float ClosestDistance = 50.0f;
					UControlManipulator* ClosestManipulator = nullptr;
					for (UControlManipulator* Manipulator : HierarchicalRig->Manipulators)
					{
						if (Manipulator)
						{
							Manipulator->TargetProximity = 0.8f;

							if (HierarchicalRig->IsManipulatorEnabled(Manipulator))
							{
								if (IsNodeSelected(Manipulator->Name))
								{
									Manipulator->TargetProximity = 1.0f;
								}

								FTransform ManipulatorTransform = Manipulator->GetTransform(HierarchicalRig);
								FTransform ParentTransform = GetParentTransform(Manipulator, HierarchicalRig);
								FTransform DisplayTransform = ManipulatorTransform*ParentTransform*ComponentTransform;

								float DistanceToPoint = FMath::PointDistToLine(DisplayTransform.GetLocation(), Direction, Origin);
								if (DistanceToPoint < ClosestDistance)
								{
									ClosestDistance = DistanceToPoint;
									ClosestManipulator = Manipulator;
								}
							}
						}
					}

					if (ClosestManipulator)
					{
						ClosestManipulator->TargetProximity = 1.3f;
					}
				}

				// Draw each manipulator
				for (UControlManipulator* Manipulator : HierarchicalRig->Manipulators)
				{
					if (Manipulator && HierarchicalRig->IsManipulatorEnabled(Manipulator))
					{
						PDI->SetHitProxy(new HManipulatorNodeProxy(HierarchicalRig, Manipulator->Name));
						FTransform ManipulatorTransform = Manipulator->GetTransform(HierarchicalRig);
						FTransform ParentTransform = GetParentTransform(Manipulator, HierarchicalRig);
						FTransform DisplayTransform = ManipulatorTransform*ParentTransform*ComponentTransform;

						Manipulator->Draw(DisplayTransform, View, PDI, IsNodeSelected(Manipulator->Name));
						PDI->SetHitProxy(nullptr);
					}
				}

				// Special drawing for human rig (e.g. lines to IK target)
				if (UHumanRig* HumanRig = Cast<UHumanRig>(ControlRig.Get()))
				{
					RenderLimb(HumanRig->LeftArm, HumanRig, PDI);
					RenderLimb(HumanRig->RightArm, HumanRig, PDI);
					RenderLimb(HumanRig->LeftLeg, HumanRig, PDI);
					RenderLimb(HumanRig->RightLeg, HumanRig, PDI);
				}

				if (Settings->bDisplayTrajectories)
				{
					TrajectoryCache.RenderTrajectories(ComponentTransform, PDI);
				}
			}
		}
	}
}

bool FControlRigEditMode::InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent)
{
	if (InEvent != IE_Released)
	{
		FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();
		if (CommandBindings->ProcessCommandBindings(InKey, KeyState, (InEvent == IE_Repeat)))
		{
			return true;
		}
	}

	return FEdMode::InputKey(InViewportClient, InViewport, InKey, InEvent);
}

bool FControlRigEditMode::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (bIsTransacting)
	{
		if (bManipulatorMadeChange)
		{
			// One final notify of our manipulators to make sure the property is updated
			for (TWeakObjectPtr<UControlRig>& ControlRig : ControlRigs)
			{
				if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(ControlRig.Get()))
				{
					for (UControlManipulator* Manipulator : HierarchicalRig->Manipulators)
					{
						if (Manipulator)
						{
							Manipulator->bManipulating = false;
							Manipulator->NotifyPostEditChangeProperty(HierarchicalRig);
						}
					}
				}
			}

			if (Settings->bDisplayTrajectories)
			{
				TrajectoryCache.ForceRecalc();
			}
		}

		GEditor->EndTransaction();
		bIsTransacting = false;
		bManipulatorMadeChange = false;
		return true;
	}

	bManipulatorMadeChange = false;

	return false;
}

bool FControlRigEditMode::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (!bIsTransacting)
	{
		GEditor->BeginTransaction(LOCTEXT("MoveManipulatorTransaction", "Move Manipulator"));

		for (TWeakObjectPtr<UControlRig>& ControlRig : ControlRigs)
		{
			if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(ControlRig.Get()))
			{
				HierarchicalRig->SetFlags(RF_Transactional);
				HierarchicalRig->Modify();

				for (UControlManipulator* Manipulator : HierarchicalRig->Manipulators)
				{
					if (Manipulator)
					{
						Manipulator->bManipulating = true;
					}
				}
			}
		}

		bIsTransacting = true;
		bManipulatorMadeChange = false;

		return bIsTransacting;
	}

	return false;
}

bool FControlRigEditMode::UsesTransformWidget() const
{
	if (SelectedNodes.Num() > 0)
	{
		return true;
	}

	return FEdMode::UsesTransformWidget();
}

bool FControlRigEditMode::UsesTransformWidget(FWidget::EWidgetMode CheckMode) const
{
	if(ControlRigs.Num() > 0 && ControlRigs[0].Get())
	{
		if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(ControlRigs[0].Get()))
		{
			for (const FName& SelectedNode : SelectedNodes)
			{
				UControlManipulator* Manipulator = HierarchicalRig->FindManipulator(SelectedNode);
				if (Manipulator)
				{
					return Manipulator->SupportsTransformComponent(WidgetModeToTransformComponent(CheckMode));
				}
			}
		}
	}

	return FEdMode::UsesTransformWidget(CheckMode);
}

FVector FControlRigEditMode::GetWidgetLocation() const
{
	if (ControlRigs.Num() > 0 && ControlRigs[0].Get())
	{
		if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(ControlRigs[0].Get()))
		{
			if (SelectedNodes.Num() > 0)
			{
				USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(HierarchicalRig->GetBoundObject());
				FTransform ComponentTransform = SkelMeshComp ? SkelMeshComp->GetComponentTransform() : FTransform::Identity;
				return ComponentTransform.TransformPosition(PivotTransform.GetLocation());
			}
		}
	}

	return FEdMode::GetWidgetLocation();
}

bool FControlRigEditMode::GetCustomDrawingCoordinateSystem(FMatrix& OutMatrix, void* InData)
{
	if (ControlRigs.Num() > 0 && ControlRigs[0].Get())
	{
		if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(ControlRigs[0].Get()))
		{
			if (SelectedNodes.Num() > 0)
			{
				OutMatrix = PivotTransform.ToMatrixNoScale().RemoveTranslation();
				return true;
			}
		}
	}

	return false;
}

bool FControlRigEditMode::GetCustomInputCoordinateSystem(FMatrix& OutMatrix, void* InData)
{
	return GetCustomDrawingCoordinateSystem(OutMatrix, InData);
}

bool FControlRigEditMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click)
{
	if (HitProxy && HitProxy->IsA(HManipulatorNodeProxy::StaticGetType()))
	{
		HManipulatorNodeProxy* NodeProxy = static_cast<HManipulatorNodeProxy*>(HitProxy);

		if (Click.IsShiftDown() || Click.IsControlDown())
		{
			SetNodeSelection(NodeProxy->NodeName, !IsNodeSelected(NodeProxy->NodeName));
		}
		else
		{
			ClearNodeSelection();
			SetNodeSelection(NodeProxy->NodeName, true);
		}
		return true;
	}

	// clear selected nodes
	ClearNodeSelection();

	return FEdMode::HandleClick(InViewportClient, HitProxy, Click);
}

bool FControlRigEditMode::IntersectSelect(bool InSelect, const TFunctionRef<bool(UControlManipulator*,const FTransform&)>& Intersects)
{
	if (ControlRigs.Num() > 0 && ControlRigs[0].Get())
	{
		if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(ControlRigs[0].Get()))
		{
			USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(HierarchicalRig->GetBoundObject());
			FTransform ComponentTransform = SkelMeshComp ? SkelMeshComp->GetComponentTransform() : FTransform::Identity;

			bool bSelected = false;
			for (UControlManipulator* Manipulator : HierarchicalRig->Manipulators)
			{
				FTransform ManipulatorTransform = HierarchicalRig->GetMappedGlobalTransform(Manipulator->Name) * ComponentTransform;
				if (Intersects(Manipulator, ManipulatorTransform))
				{
					SetNodeSelection(Manipulator->Name, InSelect);
					bSelected = true;
				}
			}

			return bSelected;
		}
	}

	return false;
}

bool FControlRigEditMode::BoxSelect(FBox& InBox, bool InSelect)
{
	bool bIntersects = IntersectSelect(InSelect, [&](UControlManipulator* Manipulator, const FTransform& Transform)
	{ 
		FBox Bounds = Manipulator->GetLocalBoundingBox();
		Bounds = Bounds.TransformBy(Transform);
		return InBox.Intersect(Bounds);
	});

	if (bIntersects)
	{
		return true;
	}

	return FEdMode::BoxSelect(InBox, InSelect);
}

bool FControlRigEditMode::FrustumSelect(const FConvexVolume& InFrustum, bool InSelect)
{
	bool bIntersects = IntersectSelect(InSelect, [&](UControlManipulator* Manipulator, const FTransform& Transform) 
	{
		FSphere Bounds = Manipulator->GetLocalBoundingSphere();
		Bounds = Bounds.TransformBy(Transform);
		return InFrustum.IntersectSphere(Bounds.Center, Bounds.W);
	});

	if (bIntersects)
	{
		return true;
	}

	return FEdMode::FrustumSelect(InFrustum, InSelect);
}

void FControlRigEditMode::SelectNone()
{
	ClearNodeSelection();

	FEdMode::SelectNone();
}

bool FControlRigEditMode::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (ControlRigs.Num() > 0 && ControlRigs[0].Get())
	{
		FVector Drag = InDrag;
		FRotator Rot = InRot;
		FVector Scale = InScale;

		const bool bCtrlDown = InViewport->KeyState(EKeys::LeftControl) || InViewport->KeyState(EKeys::RightControl);
		const bool bShiftDown = InViewport->KeyState(EKeys::LeftShift) || InViewport->KeyState(EKeys::RightShift);
		const bool bAltDown = InViewport->KeyState(EKeys::LeftAlt) || InViewport->KeyState(EKeys::RightAlt);
		const bool bMouseButtonDown = InViewport->KeyState(EKeys::LeftMouseButton);

		const FWidget::EWidgetMode WidgetMode = InViewportClient->GetWidgetMode();
		const EAxisList::Type CurrentAxis = InViewportClient->GetCurrentWidgetAxis();
		const ECoordSystem CoordSystem = InViewportClient->GetWidgetCoordSystemSpace();

		if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(ControlRigs[0].Get()))
		{
			if (SelectedNodes.Num() > 0 && bIsTransacting && bMouseButtonDown && !bCtrlDown && !bShiftDown && !bAltDown && CurrentAxis != EAxisList::None)
			{
				const bool bDoRotation = !Rot.IsZero() && (WidgetMode == FWidget::WM_Rotate || WidgetMode == FWidget::WM_TranslateRotateZ);
				const bool bDoTranslation = !Drag.IsZero() && (WidgetMode == FWidget::WM_Translate || WidgetMode == FWidget::WM_TranslateRotateZ);
				const bool bDoScale = !Scale.IsZero() && WidgetMode == FWidget::WM_Scale;

				USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(HierarchicalRig->GetBoundObject());
				FTransform ComponentTransform = SkelMeshComp ? SkelMeshComp->GetComponentTransform() : FTransform::Identity;

				// manipulator transform is always on actor base - (actor origin being 0)
				for (const FName& SelectedNode : SelectedNodes)
				{
					UControlManipulator* Manipulator = HierarchicalRig->FindManipulator(SelectedNode);
					if (Manipulator)
					{
						FTransform NewTransform = HierarchicalRig->GetMappedGlobalTransform(SelectedNode) * ComponentTransform;

						bool bTransformChanged = false;
						if (bDoRotation && Manipulator->bUsesRotation)
						{
							FQuat CurrentRotation = NewTransform.GetRotation();
							CurrentRotation = (Rot.Quaternion() * CurrentRotation);
							NewTransform.SetRotation(CurrentRotation);
							bTransformChanged = true;
						}

						if (bDoTranslation && Manipulator->bUsesTranslation)
						{
							FVector ManipulatorLocation = NewTransform.GetLocation();
							ManipulatorLocation = ManipulatorLocation + Drag;
							NewTransform.SetLocation(ManipulatorLocation);
							bTransformChanged = true;
						}

						if (bDoScale && Manipulator->bUsesScale)
						{
							FVector ManipulatorScale = NewTransform.GetScale3D();
							ManipulatorScale = ManipulatorScale + Scale;
							NewTransform.SetScale3D(ManipulatorScale);
							bTransformChanged = true;
						}

						if (bTransformChanged)
						{
							HierarchicalRig->SetMappedGlobalTransform(SelectedNode, NewTransform * ComponentTransform.Inverse());

							if (Manipulator->bInLocalSpace)
							{
								FTransform ParentTransform = GetParentTransform(Manipulator, HierarchicalRig);
								Manipulator->SetTransform(NewTransform.GetRelativeTransform(ParentTransform), HierarchicalRig);
							}
							else
							{
								Manipulator->SetTransform(NewTransform, HierarchicalRig);
							}

							// have to update manipulator to node when children modifies from set global transform
							HierarchicalRig->UpdateManipulatorToNode(true);

							bManipulatorMadeChange = true;
						}
					}
				}

				RecalcPivotTransform();

				return true;
			}
		}
	}

	return false;
}

bool FControlRigEditMode::ShouldDrawWidget() const
{
	if (SelectedNodes.Num() > 0)
	{
		return true;
	}

	return FEdMode::ShouldDrawWidget();
}

bool FControlRigEditMode::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	if (OtherModeID == FBuiltinEditorModes::EM_Placement)
	{
		return false;
	}
	return true;
}

void FControlRigEditMode::ClearNodeSelection()
{
	if (!bSelecting)
	{
		TGuardValue<bool> ReentrantGuard(bSelecting, true);

		SelectedNodes.Empty();

		bSelectedNode = true;
		OnNodesSelectedDelegate.Broadcast(SelectedNodes);
	}
}

void FControlRigEditMode::SetNodeSelection(const FName& NodeName, bool bSelected)
{
	if (!bSelecting)
	{
		TGuardValue<bool> ReentrantGuard(bSelecting, true);

		if (bSelected)
		{
			SelectedNodes.AddUnique(NodeName);
		}
		else
		{
			SelectedNodes.Remove(NodeName);
		}

		bSelectedNode = true;
		OnNodesSelectedDelegate.Broadcast(SelectedNodes);
	}
}

void FControlRigEditMode::SetNodeSelection(const TArray<FName>& NodeNames, bool bSelected)
{
	if (!bSelecting)
	{
		TGuardValue<bool> ReentrantGuard(bSelecting, true);

		for (const FName& NodeName : NodeNames)
		{
			if (bSelected)
			{
				SelectedNodes.AddUnique(NodeName);
			}
			else
			{
				SelectedNodes.Remove(NodeName);
			}
		}

		bSelectedNode = true;
		OnNodesSelectedDelegate.Broadcast(SelectedNodes);
	}
}

const TArray<FName>& FControlRigEditMode::GetSelectedNodes() const
{
	return SelectedNodes;
}

bool FControlRigEditMode::IsNodeSelected(const FName& NodeName) const
{
	return SelectedNodes.Contains(NodeName);
}

void FControlRigEditMode::SetNodeSelectionByPropertyPath(const TArray<FString>& InPropertyPaths)
{
	if (!bSelecting)
	{
		TGuardValue<bool> SelectingReentrantGuard(bSelecting, true);
		TGuardValue<bool> SelectingByPathReentrantGuard(bSelectingByPath, true);

		TArray<FName> NodesToSelect;

		for (TWeakObjectPtr<UControlRig> ControlRig : ControlRigs)
		{
			if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(ControlRig.Get()))
			{
				for (UControlManipulator* Manipulator : HierarchicalRig->Manipulators)
				{
					for (const FString& PropertyPath : InPropertyPaths)
					{
						if (PropertyPath == Manipulator->PropertyToManipulate.ToString())
						{
							NodesToSelect.Add(Manipulator->Name);
							break;
						}
					}
				}
			}
		}

		if (NodesToSelect.Num() > 0)
		{
			SelectedNodes.Sort();
			NodesToSelect.Sort();

			if (NodesToSelect != SelectedNodes)
			{
				SelectedNodes.Empty();
				for (const FName& NodeName : NodesToSelect)
				{
					SelectedNodes.AddUnique(NodeName);
				}

				bSelectedNode = true;
				OnNodesSelectedDelegate.Broadcast(SelectedNodes);
			}
		}
	}
}

void FControlRigEditMode::HandleObjectSpawned(FGuid InObjectBinding, UObject* SpawnedObject, IMovieScenePlayer& Player)
{
	if (WeakSequencer.IsValid())
	{
		// check whether this spawned object is from our sequence
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer.Get() == &Player)
		{
			RefreshObjects();

			// check if the object is being displayed currently
			check(ControlRigs.Num() == ControlRigGuids.Num());
			for (int32 ObjectIndex = 0; ObjectIndex < ControlRigGuids.Num(); ObjectIndex++)
			{
				if (ControlRigGuids[ObjectIndex] == InObjectBinding)
				{
					if (ControlRigs[ObjectIndex] != Cast<UControlRig>(SpawnedObject))
					{
						ControlRigs[ObjectIndex] = Cast<UControlRig>(SpawnedObject);
						SetObjects_Internal();
					}
					return;
				}
			}

			// We didnt find an existing Guid, so set up our internal cache
			if (ControlRigGuids.Num() == 0)
			{
				TArray<TWeakObjectPtr<>> SelectedObjects({ SpawnedObject });
				TArray<FGuid> SelectedGuids({ InObjectBinding });
				SetObjects(SelectedObjects, SelectedGuids);
				if (UControlRig* ControlRig = Cast<UControlRig>(SpawnedObject))
				{
					if (Settings->Actor.IsValid() && ControlRig->GetBoundObject() == nullptr)
					{
						ControlRig->BindToObject(Settings->Actor.Get());
					}
				}
				ReBindToActor();
			}
		}
	}
}

void FControlRigEditMode::RefreshObjects()
{
	if (WeakSequencer.IsValid())
	{
		TSharedRef<ISequencer> Sequencer = WeakSequencer.Pin().ToSharedRef();
		UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence() ? Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene() : nullptr;
		if (MovieScene)
		{
			check(ControlRigs.Num() == ControlRigGuids.Num());
			TArray<int32> InvalidIndices;
			for (int32 ObjectIndex = 0; ObjectIndex < ControlRigGuids.Num(); ObjectIndex++)
			{
				// check if we have an invalid Guid & invalidate Guid if so
				if (ControlRigGuids[ObjectIndex].IsValid() && MovieScene->FindSpawnable(ControlRigGuids[ObjectIndex]) == nullptr)
				{
					ControlRigGuids[ObjectIndex].Invalidate();
					ControlRigs[ObjectIndex] = nullptr;
					InvalidIndices.Add(ObjectIndex);
				}
			}

			if (InvalidIndices.Num() > 0)
			{
				for (int32 InvalidIndex : InvalidIndices)
				{
					ControlRigs.RemoveAt(InvalidIndex);
					ControlRigGuids.RemoveAt(InvalidIndex);
				}

				SetObjects_Internal();
			}
		}
	}
	else
	{
		ControlRigs.Empty();
		ControlRigGuids.Empty();

		SetObjects_Internal();
	}
}

void FControlRigEditMode::RecalcPivotTransform()
{
	PivotTransform = FTransform::Identity;

	if (ControlRigs.Num() > 0 && ControlRigs[0].Get())
	{
		if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(ControlRigs[0].Get()))
		{
			if (SelectedNodes.Num() > 0)
			{
				// Use average location as pivot location
				FVector PivotLocation = FVector::ZeroVector;
				for (const FName& SelectedNode : SelectedNodes)
				{
					PivotLocation += HierarchicalRig->GetMappedGlobalTransform(SelectedNode).GetLocation();
				}

				PivotLocation /= (float)SelectedNodes.Num();
				PivotTransform.SetLocation(PivotLocation);

				// recalc coord system too
				USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(HierarchicalRig->GetBoundObject());
				FTransform ComponentTransform = SkelMeshComp ? SkelMeshComp->GetComponentTransform() : FTransform::Identity;

				if (SelectedNodes.Num() == 1)
				{
					// A single node just uses its own transform
					FTransform WorldTransform = HierarchicalRig->GetMappedGlobalTransform(SelectedNodes[0]) * ComponentTransform;
					PivotTransform.SetRotation(WorldTransform.GetRotation());
				}
				else if (SelectedNodes.Num() > 1)
				{
					// If we have more than one node selected, use the coordinate space of the component
					PivotTransform.SetRotation(ComponentTransform.GetRotation());
				}
			}
		}
	}
}

void FControlRigEditMode::HandleSelectionChanged(const TArray<FName>& InSelectedNodes)
{
	SelectedIndices.Reset();

	TArray<FString> PropertyPaths;

	for (TWeakObjectPtr<UControlRig> ControlRig : ControlRigs)
	{
		if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(ControlRig.Get()))
		{
			const FAnimationHierarchy& Hierarchy = HierarchicalRig->GetHierarchy();

			for (UControlManipulator* Manipulator : HierarchicalRig->Manipulators)
			{
				if (IsNodeSelected(Manipulator->Name))
				{
					PropertyPaths.Add(Manipulator->PropertyToManipulate.ToString());
					SelectedIndices.Add(Hierarchy.GetNodeIndex(Manipulator->Name));
				}
			}
		}
	}

	if (WeakSequencer.IsValid() && !bSelectingByPath)
	{
		if (PropertyPaths.Num())
		{
			WeakSequencer.Pin()->SelectByPropertyPaths(PropertyPaths);
		}
	}

	if (Settings->bDisplayTrajectories)
	{
		TrajectoryCache.RebuildMesh(SelectedIndices);
	}
}

void FControlRigEditMode::BindCommands()
{
	const FControlRigCommands& Commands = FControlRigCommands::Get();

	CommandBindings->MapAction(
		Commands.SetKey,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::SetKeysForSelectedManipulators));

	CommandBindings->MapAction(
		Commands.ToggleManipulators,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::ToggleManipulators));

	CommandBindings->MapAction(
		Commands.ToggleTrajectories,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::ToggleTrajectories));
}

void FControlRigEditMode::SetKeysForSelectedManipulators()
{
	for (TWeakObjectPtr<UControlRig> ControlRig : ControlRigs)
	{
		if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(ControlRig.Get()))
		{
			for (UControlManipulator* Manipulator : HierarchicalRig->Manipulators)
			{
				if (IsNodeSelected(Manipulator->Name))
				{
					SetKeyForManipulator(HierarchicalRig, Manipulator);
				}
			}
		}
	}
}

void FControlRigEditMode::SetKeyForManipulator(UHierarchicalRig* HierarchicalRig, UControlManipulator* Manipulator)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		TArray<UObject*> Objects({ HierarchicalRig });
		FKeyPropertyParams KeyPropertyParams(Objects, Manipulator->CachedPropertyPath, ESequencerKeyMode::ManualKeyForced);
		Sequencer->KeyProperty(KeyPropertyParams);
	}
}

void FControlRigEditMode::ToggleManipulators()
{
	// Toggle flag (is used in drawing code)
	Settings->bHideManipulators = !Settings->bHideManipulators;
}

void FControlRigEditMode::ToggleTrajectories()
{
	Settings->bDisplayTrajectories = !Settings->bDisplayTrajectories;
	TrajectoryCache.RebuildMesh(SelectedIndices);
}


void FControlRigEditMode::RefreshTrajectoryCache()
{
	TrajectoryCache.ForceRecalc();
}

#undef LOCTEXT_NAMESPACE