// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PhysicsAssetEditorEditMode.h"
#include "PhysicsAssetEditorSkeletalMeshComponent.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "AssetEditorModeManager.h"
#include "EngineUtils.h"
#include "PhysicsAssetEditorSharedData.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Preferences/PhysicsAssetEditorOptions.h"
#include "IPersonaPreviewScene.h"
#include "PhysicsAssetEditor.h"
#include "PhysicsAssetEditorHitProxies.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "DrawDebugHelpers.h"
#include "SEditorViewport.h"
#include "IPersonaToolkit.h"
#include "MultiBoxBuilder.h"
#include "SlateApplication.h"

#define LOCTEXT_NAMESPACE "PhysicsAssetEditorEditMode"

FName FPhysicsAssetEditorEditMode::ModeName("PhysicsAssetEditor");

FPhysicsAssetEditorEditMode::FPhysicsAssetEditorEditMode()
	: MinPrimSize(0.5f)
	, PhysicsAssetEditor_TranslateSpeed(0.25f)
	, PhysicsAssetEditor_RotateSpeed(1.0f * (PI / 180.0f))
	, PhysicsAssetEditor_LightRotSpeed(0.22f)
	, SimGrabCheckDistance(5000.0f)
	, SimHoldDistanceChangeDelta(20.0f)
	, SimMinHoldDistance(10.0f)
	, SimGrabMoveSpeed(1.0f)
{
	// Disable grid drawing for this mode as the viewport handles this
	bDrawGrid = false;

	PhysicsAssetEditorFont = GEngine->GetSmallFont();
	check(PhysicsAssetEditorFont);
}

bool FPhysicsAssetEditorEditMode::GetCameraTarget(FSphere& OutTarget) const
{
	bool bHandled = false;

	float NumSelected = 0.0f;
	FBox Bounds(ForceInit);
	for (int32 BodyIndex = 0; BodyIndex < SharedData->SelectedBodies.Num(); ++BodyIndex)
	{
		FPhysicsAssetEditorSharedData::FSelection& SelectedObject = SharedData->SelectedBodies[BodyIndex];
		int32 BoneIndex = SharedData->EditorSkelComp->GetBoneIndex(SharedData->PhysicsAsset->SkeletalBodySetups[SelectedObject.Index]->BoneName);
		UBodySetup* BodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[SelectedObject.Index];
		const FKAggregateGeom& AggGeom = BodySetup->AggGeom;

		FTransform BoneTM = SharedData->EditorSkelComp->GetBoneTransform(BoneIndex);
		const float Scale = BoneTM.GetScale3D().GetAbsMax();
		BoneTM.RemoveScaling();

		if (SelectedObject.PrimitiveType == EAggCollisionShape::Sphere)
		{
			Bounds += AggGeom.SphereElems[SelectedObject.PrimitiveIndex].CalcAABB(BoneTM, Scale);
		}
		else if (SelectedObject.PrimitiveType == EAggCollisionShape::Box)
		{
			Bounds += AggGeom.BoxElems[SelectedObject.PrimitiveIndex].CalcAABB(BoneTM, Scale);
		}
		else if (SelectedObject.PrimitiveType == EAggCollisionShape::Sphyl)
		{
			Bounds += AggGeom.SphylElems[SelectedObject.PrimitiveIndex].CalcAABB(BoneTM, Scale);
		}
		else if (SelectedObject.PrimitiveType == EAggCollisionShape::Convex)
		{
			Bounds += AggGeom.ConvexElems[SelectedObject.PrimitiveIndex].CalcAABB(BoneTM, BoneTM.GetScale3D());
		}

		bHandled = true;
	}

	for (int32 ConstraintIndex = 0; ConstraintIndex < SharedData->SelectedConstraints.Num(); ++ConstraintIndex)
	{
		FPhysicsAssetEditorSharedData::FSelection& SelectedObject = SharedData->SelectedConstraints[ConstraintIndex];
		Bounds += SharedData->GetConstraintWorldTM(&SelectedObject, EConstraintFrame::Frame2).GetLocation();

		bHandled = true;
	}

	OutTarget.Center = Bounds.GetCenter();
	OutTarget.W = Bounds.GetExtent().Size();	// @TODO: calculate correct bounds

	return bHandled;
}

IPersonaPreviewScene& FPhysicsAssetEditorEditMode::GetAnimPreviewScene() const
{
	return *static_cast<IPersonaPreviewScene*>(static_cast<FAssetEditorModeManager*>(Owner)->GetPreviewScene());
}

void FPhysicsAssetEditorEditMode::GetOnScreenDebugInfo(TArray<FText>& OutDebugInfo) const
{

}

bool FPhysicsAssetEditorEditMode::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	const EAxisList::Type CurrentAxis = InViewportClient->GetCurrentWidgetAxis();
	if(!SharedData->bManipulating && CurrentAxis != EAxisList::None)
	{
		if(SharedData->GetSelectedBody() || SharedData->GetSelectedConstraint())
		{
			if(SharedData->GetSelectedBody())
			{
				GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "MoveElement", "Move Element"));
			}
			else
			{
				GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "MoveConstraint", "Move Constraint"));
			}
		}

		if (SharedData->GetSelectedBody())
		{
			for (int32 i = 0; i<SharedData->SelectedBodies.Num(); ++i)
			{
				SharedData->PhysicsAsset->SkeletalBodySetups[SharedData->SelectedBodies[i].Index]->Modify();
				SharedData->SelectedBodies[i].ManipulateTM = FTransform::Identity;
			}

			SharedData->bManipulating = true;
		}

		if (SharedData->GetSelectedConstraint())
		{
			const int32 Count = SharedData->SelectedConstraints.Num();
			StartManRelConTM.SetNumUninitialized(Count);
			StartManParentConTM.SetNumUninitialized(Count);
			StartManChildConTM.SetNumUninitialized(Count);

			for (int32 i = 0; i < SharedData->SelectedConstraints.Num(); ++i)
			{
				FPhysicsAssetEditorSharedData::FSelection& Constraint = SharedData->SelectedConstraints[i];
				SharedData->PhysicsAsset->ConstraintSetup[Constraint.Index]->Modify();
				Constraint.ManipulateTM = FTransform::Identity;

				const FTransform WParentFrame = SharedData->GetConstraintWorldTM(&Constraint, EConstraintFrame::Frame2);
				const FTransform WChildFrame = SharedData->GetConstraintWorldTM(&Constraint, EConstraintFrame::Frame1);
				const UPhysicsConstraintTemplate* Setup = SharedData->PhysicsAsset->ConstraintSetup[Constraint.Index];

				StartManRelConTM[i] = WChildFrame * WParentFrame.Inverse();
				StartManParentConTM[i] = Setup->DefaultInstance.GetRefFrame(EConstraintFrame::Frame2);
				StartManChildConTM[i] = Setup->DefaultInstance.GetRefFrame(EConstraintFrame::Frame1);
			}

			SharedData->bManipulating = true;
		}
	}
	
	return SharedData->bManipulating;
}

bool FPhysicsAssetEditorEditMode::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (SharedData->bManipulating)
	{
		SharedData->bManipulating = false;

		for (int32 i = 0; i<SharedData->SelectedBodies.Num(); ++i)
		{
			FPhysicsAssetEditorSharedData::FSelection & SelectedObject = SharedData->SelectedBodies[i];
			UBodySetup* BodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[SelectedObject.Index];

			FKAggregateGeom* AggGeom = &BodySetup->AggGeom;


			if (SelectedObject.PrimitiveType == EAggCollisionShape::Convex)
			{
				FKConvexElem& Convex = AggGeom->ConvexElems[SelectedObject.PrimitiveIndex];
				Convex.SetTransform(SelectedObject.ManipulateTM * Convex.GetTransform());

				BodySetup->InvalidatePhysicsData();
				BodySetup->CreatePhysicsMeshes();
			}
		}

		GEditor->EndTransaction();
		SharedData->RefreshPhysicsAssetChange(SharedData->PhysicsAsset);
		InViewport->Invalidate();

		return true;
	}

	return false;
}

bool FPhysicsAssetEditorEditMode::InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey Key, EInputEvent Event)
{
	int32 HitX = InViewport->GetMouseX();
	int32 HitY = InViewport->GetMouseY();
	bool bCtrlDown = InViewport->KeyState(EKeys::LeftControl) || InViewport->KeyState(EKeys::RightControl);
	bool bShiftDown = InViewport->KeyState(EKeys::LeftShift) || InViewport->KeyState(EKeys::RightShift);

	bool bHandled = false;
	if (SharedData->bRunningSimulation)
	{
		if (Key == EKeys::RightMouseButton || Key == EKeys::LeftMouseButton)
		{
			if (Event == IE_Pressed)
			{
				bHandled = SimMousePress(InViewportClient, Key);
			}
			else if (Event == IE_Released)
			{
				bHandled = SimMouseRelease();
			}
			else
			{
				// Handle repeats/double clicks etc. so we dont fall through
				bHandled = true;
			}
		}
		else if (Key == EKeys::MouseScrollUp)
		{
			bHandled = SimMouseWheelUp(InViewportClient);
		}
		else if (Key == EKeys::MouseScrollDown)
		{
			bHandled = SimMouseWheelDown(InViewportClient);
		}
		else if (InViewportClient->IsFlightCameraActive())
		{
			// If the flight camera is active (user is looking or moving around the scene)
			// consume the event so hotkeys don't fire.
			bHandled = true;
		}
	}

	if (!bHandled)
	{
		bHandled = IPersonaEditMode::InputKey(InViewportClient, InViewport, Key, Event);
	}

	if (bHandled)
	{
		InViewportClient->Invalidate();
	}

	return bHandled;
}

bool FPhysicsAssetEditorEditMode::InputAxis(FEditorViewportClient* InViewportClient, FViewport* InViewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime)
{
	bool bHandled = false;
	// If we are 'manipulating' don't move the camera but do something else with mouse input.
	if (SharedData->bManipulating)
	{
		bool bCtrlDown = InViewport->KeyState(EKeys::LeftControl) || InViewport->KeyState(EKeys::RightControl);

		if (SharedData->bRunningSimulation)
		{
			if (Key == EKeys::MouseX)
			{
				SimMouseMove(InViewportClient, Delta, 0.0f);
			}
			else if (Key == EKeys::MouseY)
			{
				SimMouseMove(InViewportClient, 0.0f, Delta);
			}
			bHandled = true;
		}
	}

	if (!bHandled)
	{
		bHandled = IPersonaEditMode::InputAxis(InViewportClient, InViewport, ControllerId, Key, Delta, DeltaTime);
	}

	InViewportClient->Invalidate();

	return bHandled;
}

bool FPhysicsAssetEditorEditMode::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	bool bHandled = false;
	const EAxisList::Type CurrentAxis = InViewportClient->GetCurrentWidgetAxis();
	if (!SharedData->bRunningSimulation && SharedData->bManipulating && CurrentAxis != EAxisList::None)
	{
		for (int32 i = 0; i< SharedData->SelectedBodies.Num(); ++i)
		{
			FPhysicsAssetEditorSharedData::FSelection & SelectedObject = SharedData->SelectedBodies[i];
			if (SharedData->bManipulating)
			{
				float BoneScale = 1.f;

				int32 BoneIndex = SharedData->EditorSkelComp->GetBoneIndex(SharedData->PhysicsAsset->SkeletalBodySetups[SelectedObject.Index]->BoneName);

				FTransform BoneTM = SharedData->EditorSkelComp->GetBoneTransform(BoneIndex);
				BoneScale = BoneTM.GetScale3D().GetAbsMax();
				BoneTM.RemoveScaling();

				SelectedObject.WidgetTM = SharedData->EditorSkelComp->GetPrimitiveTransform(BoneTM, SelectedObject.Index, SelectedObject.PrimitiveType, SelectedObject.PrimitiveIndex, BoneScale);

				if(InViewportClient->GetWidgetMode() == FWidget::WM_Translate || InViewportClient->GetWidgetMode() == FWidget::WM_Rotate)
				{
					if (InViewportClient->GetWidgetMode() == FWidget::WM_Translate)
					{
						FVector Dir = SelectedObject.WidgetTM.InverseTransformVector(InDrag.GetSafeNormal());
						FVector DragVec = Dir * InDrag.Size() / BoneScale;
						SelectedObject.ManipulateTM.AddToTranslation(DragVec);
					}
					else if (InViewportClient->GetWidgetMode() == FWidget::WM_Rotate)
					{
						FVector Axis;
						float Angle;
						InRot.Quaternion().ToAxisAndAngle(Axis, Angle);

						Axis = SelectedObject.WidgetTM.InverseTransformVectorNoScale(Axis);

						const FQuat Start = SelectedObject.ManipulateTM.GetRotation();
						const FQuat Delta = FQuat(Axis, Angle);
						const FQuat Result = Delta * Start;

						SelectedObject.ManipulateTM = FTransform(Result);
					}
				
					UBodySetup* BodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[SelectedObject.Index];
					FKAggregateGeom* AggGeom = &BodySetup->AggGeom;

					// for all but convex shapes, we apply straight away
					if (SelectedObject.PrimitiveType == EAggCollisionShape::Sphere)
					{
						AggGeom->SphereElems[SelectedObject.PrimitiveIndex].Center = (SelectedObject.ManipulateTM * AggGeom->SphereElems[SelectedObject.PrimitiveIndex].GetTransform()).GetLocation();
						SelectedObject.ManipulateTM.SetIdentity();
					}
					else if (SelectedObject.PrimitiveType == EAggCollisionShape::Box)
					{
						AggGeom->BoxElems[SelectedObject.PrimitiveIndex].SetTransform(SelectedObject.ManipulateTM * AggGeom->BoxElems[SelectedObject.PrimitiveIndex].GetTransform());
						SelectedObject.ManipulateTM.SetIdentity();
					}
					else if (SelectedObject.PrimitiveType == EAggCollisionShape::Sphyl)
					{
						AggGeom->SphylElems[SelectedObject.PrimitiveIndex].SetTransform(SelectedObject.ManipulateTM * AggGeom->SphylElems[SelectedObject.PrimitiveIndex].GetTransform());
						SelectedObject.ManipulateTM.SetIdentity();
					}
				}
				else if (InViewportClient->GetWidgetMode() == FWidget::WM_Scale)
				{
					ModifyPrimitiveSize(SelectedObject.Index, SelectedObject.PrimitiveType, SelectedObject.PrimitiveIndex, InScale);
				}

				bHandled = true;
			}
		}

		for (int32 i = 0; i<SharedData->SelectedConstraints.Num(); ++i)
		{
			FPhysicsAssetEditorSharedData::FSelection & SelectedObject = SharedData->SelectedConstraints[i];
			if (SharedData->bManipulating)
			{
				float BoneScale = 1.f;
				SelectedObject.WidgetTM = SharedData->GetConstraintMatrix(SelectedObject.Index, EConstraintFrame::Frame2, 1.f);

				if (InViewportClient->GetWidgetMode() == FWidget::WM_Translate)
				{
					FVector Dir = SelectedObject.WidgetTM.InverseTransformVector(InDrag.GetSafeNormal());
					FVector DragVec = Dir * InDrag.Size() / BoneScale;
					SelectedObject.ManipulateTM.AddToTranslation(DragVec);
				}
				else if (InViewportClient->GetWidgetMode() == FWidget::WM_Rotate)
				{
					FVector Axis;
					float Angle;
					InRot.Quaternion().ToAxisAndAngle(Axis, Angle);

					Axis = SelectedObject.WidgetTM.InverseTransformVectorNoScale(Axis);

					const FQuat Start = SelectedObject.ManipulateTM.GetRotation();
					const FQuat Delta = FQuat(Axis, Angle);
					const FQuat Result = Delta * Start;

					SelectedObject.ManipulateTM = FTransform(Result);
				}

				UPhysicsConstraintTemplate* ConstraintSetup = SharedData->PhysicsAsset->ConstraintSetup[SelectedObject.Index];

				ConstraintSetup->DefaultInstance.SetRefFrame(EConstraintFrame::Frame2, SelectedObject.ManipulateTM * StartManParentConTM[i]);

				//Rotation by default only rotates one frame, but translation by default moves both
				bool bMultiFrame = (InViewportClient->IsAltPressed() && InViewportClient->GetWidgetMode() == FWidget::WM_Rotate) || (!InViewportClient->IsAltPressed() && InViewportClient->GetWidgetMode() == FWidget::WM_Translate);

				if (bMultiFrame)
				{
					SharedData->SetConstraintRelTM(&SelectedObject, StartManRelConTM[i]);
				}
				else
				{
					ConstraintSetup->DefaultInstance.SetRefFrame(EConstraintFrame::Frame1, FTransform(StartManChildConTM[i]));
				}

				bHandled = true;
			}
		}
	}
	
	return bHandled;
}

void FPhysicsAssetEditorEditMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	if (SharedData->bRunningSimulation)
	{
		// check if PIE disabled the realtime viewport and quit sim if so
		if (!ViewportClient->IsRealtime())
		{
			SharedData->ToggleSimulation();

			ViewportClient->Invalidate();
		}

		UWorld* World = SharedData->PreviewScene.Pin()->GetWorld();
		AWorldSettings* Setting = World->GetWorldSettings();
		Setting->WorldGravityZ = SharedData->bNoGravitySimulation ? 0.0f : UPhysicsSettings::Get()->DefaultGravityZ*SharedData->EditorOptions->GravScale;
		Setting->bWorldGravitySet = true;

		// We back up the transforms array now
		SharedData->EditorSkelComp->AnimationSpaceBases = SharedData->EditorSkelComp->GetComponentSpaceTransforms();
		SharedData->EditorSkelComp->SetPhysicsBlendWeight(SharedData->EditorOptions->PhysicsBlend);
		SharedData->EditorSkelComp->bUpdateJointsFromAnimation = SharedData->EditorOptions->bUpdateJointsFromAnimation;
		SharedData->EditorSkelComp->PhysicsTransformUpdateMode = SharedData->EditorOptions->PhysicsUpdateMode;

		static FPhysicalAnimationData EmptyProfile;

		SharedData->PhysicalAnimationComponent->ApplyPhysicalAnimationProfileBelow(NAME_None, SharedData->PhysicsAsset->CurrentPhysicalAnimationProfileName, /*Include Self=*/true, /*Clear Not Found=*/true);
	}
}

void FPhysicsAssetEditorEditMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	EPhysicsAssetEditorRenderMode MeshViewMode = SharedData->GetCurrentMeshViewMode(SharedData->bRunningSimulation);

	if (MeshViewMode != EPhysicsAssetEditorRenderMode::None)
	{
		SharedData->EditorSkelComp->SetVisibility(true);

		if (MeshViewMode == EPhysicsAssetEditorRenderMode::Wireframe)
		{
			SharedData->EditorSkelComp->SetForceWireframe(true);
		}
		else
		{
			SharedData->EditorSkelComp->SetForceWireframe(false);
		}
	}
	else
	{
		SharedData->EditorSkelComp->SetVisibility(false);
	}

	// Draw phat skeletal component.
	SharedData->EditorSkelComp->DebugDraw(View, PDI);
}

void FPhysicsAssetEditorEditMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	float W, H;
	PhysicsAssetEditorFont->GetCharSize(TEXT('L'), W, H);

	const float XOffset = 5.0f;
	const float YOffset = 48.0f;

	FCanvasTextItem TextItem(FVector2D::ZeroVector, FText::GetEmpty(), PhysicsAssetEditorFont, FLinearColor::White);

	// Write body/constraint count at top.
	FString StatusString = FText::Format(
		NSLOCTEXT("UnrealEd", "BodiesConstraints_F", "{0} Bodies  {1} Considered for bounds  {2} Ratio  {3} Constraints"),
		FText::AsNumber(SharedData->PhysicsAsset->SkeletalBodySetups.Num()),
		FText::AsNumber(SharedData->PhysicsAsset->BoundsBodies.Num()),
		FText::AsNumber(static_cast<float>(SharedData->PhysicsAsset->BoundsBodies.Num()) / static_cast<float>(SharedData->PhysicsAsset->SkeletalBodySetups.Num())),
		FText::AsNumber(SharedData->PhysicsAsset->ConstraintSetup.Num())).ToString();

	TextItem.Text = FText::FromString(StatusString);
	Canvas->DrawItem(TextItem, XOffset, YOffset);

	TextItem.Text = FText::GetEmpty();
	if (SharedData->bRunningSimulation)
	{
#if PLATFORM_MAC
		TextItem.Text = LOCTEXT("Sim_Mac", "Command+RightMouse to interact with bodies");
#else
		TextItem.Text = LOCTEXT("Sim", "Ctrl+RightMouse to interact with bodies");
#endif
	}
	else if(SharedData->GetSelectedConstraint() != nullptr)
	{
		if (ViewportClient->GetWidgetMode() == FWidget::WM_Translate)
		{
			TextItem.Text = LOCTEXT("SingleMove", "Hold ALT to move a single reference frame");
		}
		else if (ViewportClient->GetWidgetMode() == FWidget::WM_Rotate)
		{
			TextItem.Text = LOCTEXT("DoubleRotate", "Hold ALT to rotate both reference frames");
		}
	}

	Canvas->DrawItem(TextItem, XOffset, Viewport->GetSizeXY().Y - (3 + H));

	// Draw current physics weight
	if (SharedData->bRunningSimulation)
	{
		FString PhysWeightString = FString::Printf(TEXT("Phys Blend: %3.0f pct"), SharedData->EditorOptions->PhysicsBlend * 100.f);
		int32 PWLW, PWLH;
		StringSize(PhysicsAssetEditorFont, PWLW, PWLH, *PhysWeightString);
		TextItem.Text = FText::FromString(PhysWeightString);
		Canvas->DrawItem(TextItem, Viewport->GetSizeXY().X - (3 + PWLW + 2 * W), Viewport->GetSizeXY().Y - (3 + H));
	}

	int32 HalfX = (Viewport->GetSizeXY().X - XOffset) / 2;
	int32 HalfY = Viewport->GetSizeXY().Y / 2;

	// If showing center-of-mass, and physics is started up..
	if (SharedData->bShowCOM)
	{
		// iterate over each bone
		for (int32 i = 0; i <SharedData->EditorSkelComp->Bodies.Num(); ++i)
		{
			FBodyInstance* BodyInst = SharedData->EditorSkelComp->Bodies[i];
			check(BodyInst);

			FVector BodyCOMPos = BodyInst->GetCOMPosition();
			float BodyMass = BodyInst->GetBodyMass();

			FPlane Projection = View->Project(BodyCOMPos);
			if (Projection.W > 0.f) // This avoids drawing bone names that are behind us.
			{
				int32 XPos = HalfX + (HalfX * Projection.X);
				int32 YPos = HalfY + (HalfY * (Projection.Y * -1));

				FString COMString = FString::Printf(TEXT("%3.3f"), BodyMass);
				TextItem.Text = FText::FromString(COMString);
				TextItem.SetColor(SharedData->COMRenderColor);
				Canvas->DrawItem(TextItem, XPos, YPos);
			}
		}
	}
}

bool FPhysicsAssetEditorEditMode::AllowWidgetMove()
{
	return ShouldDrawWidget();
}

bool FPhysicsAssetEditorEditMode::ShouldDrawWidget() const
{
	return !SharedData->bRunningSimulation && (SharedData->GetSelectedBody() || SharedData->GetSelectedConstraint());
}

bool FPhysicsAssetEditorEditMode::UsesTransformWidget() const
{
	return ShouldDrawWidget();
}

bool FPhysicsAssetEditorEditMode::UsesTransformWidget(FWidget::EWidgetMode CheckMode) const
{
	if (SharedData->GetSelectedConstraint() && CheckMode == FWidget::WM_Scale)
	{
		return false;
	}

	return ShouldDrawWidget() && (CheckMode == FWidget::WM_Scale || CheckMode == FWidget::WM_Translate || CheckMode == FWidget::WM_Rotate);
}

bool FPhysicsAssetEditorEditMode::GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	// Don't draw widget if nothing selected.
	if (SharedData->GetSelectedBody())
	{
		int32 BoneIndex = SharedData->EditorSkelComp->GetBoneIndex(SharedData->PhysicsAsset->SkeletalBodySetups[SharedData->GetSelectedBody()->Index]->BoneName);

		FTransform BoneTM = SharedData->EditorSkelComp->GetBoneTransform(BoneIndex);
		BoneTM.RemoveScaling();

		InMatrix = SharedData->EditorSkelComp->GetPrimitiveTransform(BoneTM, SharedData->GetSelectedBody()->Index, SharedData->GetSelectedBody()->PrimitiveType, SharedData->GetSelectedBody()->PrimitiveIndex, 1.f).ToMatrixNoScale().RemoveTranslation();
		return true;
	}
	else if (SharedData->GetSelectedConstraint())
	{
		InMatrix = SharedData->GetConstraintMatrix(SharedData->GetSelectedConstraint()->Index, EConstraintFrame::Frame2, 1.f).ToMatrixNoScale().RemoveTranslation();
		return true;
	}

	return false;
}

bool FPhysicsAssetEditorEditMode::GetCustomInputCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	return GetCustomDrawingCoordinateSystem(InMatrix, InData);
}

FVector FPhysicsAssetEditorEditMode::GetWidgetLocation() const
{
	// Don't draw widget if nothing selected.
	if (SharedData->GetSelectedBody())
	{
		int32 BoneIndex = SharedData->EditorSkelComp->GetBoneIndex(SharedData->PhysicsAsset->SkeletalBodySetups[SharedData->GetSelectedBody()->Index]->BoneName);

		FTransform BoneTM = SharedData->EditorSkelComp->GetBoneTransform(BoneIndex);
		const float Scale = BoneTM.GetScale3D().GetAbsMax();
		BoneTM.RemoveScaling();

		return SharedData->EditorSkelComp->GetPrimitiveTransform(BoneTM, SharedData->GetSelectedBody()->Index, SharedData->GetSelectedBody()->PrimitiveType, SharedData->GetSelectedBody()->PrimitiveIndex, Scale).GetTranslation();
	}
	else if (SharedData->GetSelectedConstraint())
	{
		return SharedData->GetConstraintMatrix(SharedData->GetSelectedConstraint()->Index, EConstraintFrame::Frame2, 1.f).GetTranslation();
	}

	return FVector::ZeroVector;
}

bool FPhysicsAssetEditorEditMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click)
{
	if (!SharedData->bRunningSimulation)
	{
		if (Click.GetKey() == EKeys::LeftMouseButton)
		{
			if (HitProxy && HitProxy->IsA(HPhysicsAssetEditorEdBoneProxy::StaticGetType()))
			{
				HPhysicsAssetEditorEdBoneProxy* BoneProxy = (HPhysicsAssetEditorEdBoneProxy*)HitProxy;

				SharedData->HitBone(BoneProxy->BodyIndex, BoneProxy->PrimType, BoneProxy->PrimIndex, InViewportClient->IsCtrlPressed() || InViewportClient->IsShiftPressed());
				return true;
			}
			else if (HitProxy && HitProxy->IsA(HPhysicsAssetEditorEdConstraintProxy::StaticGetType()))
			{
				HPhysicsAssetEditorEdConstraintProxy* ConstraintProxy = (HPhysicsAssetEditorEdConstraintProxy*)HitProxy;

				SharedData->HitConstraint(ConstraintProxy->ConstraintIndex, InViewportClient->IsCtrlPressed() || InViewportClient->IsShiftPressed());
				return true;
			}
			else
			{
				HitNothing(InViewportClient);
			}
		}
		else if (Click.GetKey() == EKeys::RightMouseButton)
		{
			if (HitProxy && HitProxy->IsA(HPhysicsAssetEditorEdBoneProxy::StaticGetType()))
			{
				HPhysicsAssetEditorEdBoneProxy* BoneProxy = (HPhysicsAssetEditorEdBoneProxy*)HitProxy;

				// Select body under cursor if not already selected	(if ctrl is held down we only add, not remove)
				FPhysicsAssetEditorSharedData::FSelection Selection(BoneProxy->BodyIndex, BoneProxy->PrimType, BoneProxy->PrimIndex);
				if (!SharedData->IsBodySelected(Selection))
				{
					SharedData->SetSelectedBody(Selection, true);
				}

				// Pop up menu, if we have a body selected.
				if (SharedData->GetSelectedBody())
				{
					OpenBodyMenu(InViewportClient);
				}

				return true;
			}
			else if (HitProxy && HitProxy->IsA(HPhysicsAssetEditorEdConstraintProxy::StaticGetType()))
			{
				HPhysicsAssetEditorEdConstraintProxy* ConstraintProxy = (HPhysicsAssetEditorEdConstraintProxy*)HitProxy;

				// Select constraint under cursor if not already selected (if ctrl is held down we only add, not remove)
				if (!SharedData->IsConstraintSelected(ConstraintProxy->ConstraintIndex))
				{
					SharedData->SetSelectedConstraint(ConstraintProxy->ConstraintIndex, InViewportClient->IsCtrlPressed());
				}

				// Pop up menu, if we have a constraint selected.
				if (SharedData->GetSelectedConstraint())
				{
					OpenConstraintMenu(InViewportClient);
				}

				return true;
			}
			else
			{
				OpenSelectionMenu(InViewportClient);
				return true;
			}
		}
	}

	return false;
}

/** Helper function to open a viewport context menu */
static void OpenContextMenu(const TSharedRef<FPhysicsAssetEditor>& PhysicsAssetEditor, FEditorViewportClient* InViewportClient, TFunctionRef<void(FMenuBuilder&)> InBuildMenu)
{
	FMenuBuilder MenuBuilder(true, PhysicsAssetEditor->GetToolkitCommands());

	InBuildMenu(MenuBuilder);

	TSharedPtr<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	TSharedPtr<SWidget> ParentWidget = InViewportClient->GetEditorViewportWidget();

	if (MenuWidget.IsValid() && ParentWidget.IsValid())
	{
		const FVector2D MouseCursorLocation = FSlateApplication::Get().GetCursorPos();

		FSlateApplication::Get().PushMenu(
			ParentWidget.ToSharedRef(),
			FWidgetPath(),
			MenuWidget.ToSharedRef(),
			MouseCursorLocation,
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
		);
	}
}

void FPhysicsAssetEditorEditMode::OpenBodyMenu(FEditorViewportClient* InViewportClient)
{
	OpenContextMenu(PhysicsAssetEditorPtr.Pin().ToSharedRef(), InViewportClient, 
		[this](FMenuBuilder& InMenuBuilder)
		{
			PhysicsAssetEditorPtr.Pin()->BuildMenuWidgetBody(InMenuBuilder);
			PhysicsAssetEditorPtr.Pin()->BuildMenuWidgetSelection(InMenuBuilder);
		});
}

void FPhysicsAssetEditorEditMode::OpenConstraintMenu(FEditorViewportClient* InViewportClient)
{
	OpenContextMenu(PhysicsAssetEditorPtr.Pin().ToSharedRef(), InViewportClient, 
		[this](FMenuBuilder& InMenuBuilder)
		{
			PhysicsAssetEditorPtr.Pin()->BuildMenuWidgetConstraint(InMenuBuilder);
			PhysicsAssetEditorPtr.Pin()->BuildMenuWidgetSelection(InMenuBuilder);
		});
}

void FPhysicsAssetEditorEditMode::OpenSelectionMenu(FEditorViewportClient* InViewportClient)
{
	OpenContextMenu(PhysicsAssetEditorPtr.Pin().ToSharedRef(), InViewportClient, 
		[this](FMenuBuilder& InMenuBuilder)
		{
			PhysicsAssetEditorPtr.Pin()->BuildMenuWidgetSelection(InMenuBuilder);
		});
}

bool FPhysicsAssetEditorEditMode::SimMousePress(FEditorViewportClient* InViewportClient, FKey Key)
{
	FViewport* Viewport = InViewportClient->Viewport;

	bool bCtrlDown = Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl);
	bool bShiftDown = Viewport->KeyState(EKeys::LeftShift) || Viewport->KeyState(EKeys::RightShift);

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(Viewport, InViewportClient->GetScene(), InViewportClient->EngineShowFlags));
	FSceneView* View = InViewportClient->CalcSceneView(&ViewFamily);

	const FViewportClick Click(View, InViewportClient, EKeys::Invalid, IE_Released, Viewport->GetMouseX(), Viewport->GetMouseY());
#if DEBUG_CLICK_VIEWPORT	
	SharedData->LastClickOrigin = Click.GetOrigin();
	SharedData->LastClickDirection = Click.GetDirection();
#endif
	SharedData->LastClickPos = Click.GetClickPos();
	FHitResult Result(1.f);
	bool bHit = SharedData->EditorSkelComp->LineTraceComponent(Result, Click.GetOrigin() - Click.GetDirection() * SimGrabCheckDistance, Click.GetOrigin() + Click.GetDirection() * SimGrabCheckDistance, FCollisionQueryParams(NAME_None, true));

	if (bHit)
	{
		if(bCtrlDown || bShiftDown)
		{
			check(Result.Item != INDEX_NONE);
			FName BoneName = SharedData->PhysicsAsset->SkeletalBodySetups[Result.Item]->BoneName;

			//UE_LOG(LogPhysics, Warning, TEXT("Hit Bone Name (%s)"), *BoneName.ToString());

			// Right mouse is for dragging things around
			if (Key == EKeys::RightMouseButton)
			{
				SharedData->bManipulating = true;
				DragX = 0.0f;
				DragY = 0.0f;
				SimGrabPush = 0.0f;

				// Update mouse force properties from sim options.
				SharedData->MouseHandle->LinearDamping = SharedData->EditorOptions->HandleLinearDamping;
				SharedData->MouseHandle->LinearStiffness = SharedData->EditorOptions->HandleLinearStiffness;
				SharedData->MouseHandle->AngularDamping = SharedData->EditorOptions->HandleAngularDamping;
				SharedData->MouseHandle->AngularStiffness = SharedData->EditorOptions->HandleAngularStiffness;
				SharedData->MouseHandle->InterpolationSpeed = SharedData->EditorOptions->InterpolationSpeed;

				// Create handle to object.
				SharedData->MouseHandle->GrabComponentAtLocationWithRotation(SharedData->EditorSkelComp, BoneName, Result.Location, FRotator::ZeroRotator);

				FMatrix	InvViewMatrix = View->ViewMatrices.GetInvViewMatrix();

				SimGrabMinPush = SimMinHoldDistance - (Result.Time * SimGrabCheckDistance);

				SimGrabLocation = Result.Location;
				SimGrabX = InvViewMatrix.GetUnitAxis(EAxis::X);
				SimGrabY = InvViewMatrix.GetUnitAxis(EAxis::Y);
				SimGrabZ = InvViewMatrix.GetUnitAxis(EAxis::Z);
			}
			// Left mouse is for poking things
			else if (Key == EKeys::LeftMouseButton)
			{
				SharedData->EditorSkelComp->AddImpulseAtLocation(Click.GetDirection() * SharedData->EditorOptions->PokeStrength, Result.Location, BoneName);
			}
		}

		return true;
	}

	return false;
}

void FPhysicsAssetEditorEditMode::SimMouseMove(FEditorViewportClient* InViewportClient, float DeltaX, float DeltaY)
{
	DragX = InViewportClient->Viewport->GetMouseX() - SharedData->LastClickPos.X;
	DragY = InViewportClient->Viewport->GetMouseY() - SharedData->LastClickPos.Y;

	if (!SharedData->MouseHandle->GrabbedComponent)
	{
		return;
	}

	//We need to convert Pixel Delta into Screen position (deal with different viewport sizes)
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(InViewportClient->Viewport, SharedData->PreviewScene.Pin()->GetScene(), InViewportClient->EngineShowFlags));
	FSceneView* View = InViewportClient->CalcSceneView(&ViewFamily);
	FVector4 ScreenOldPos = View->PixelToScreen(SharedData->LastClickPos.X, SharedData->LastClickPos.Y, 1.f);
	FVector4 ScreenNewPos = View->PixelToScreen(DragX + SharedData->LastClickPos.X, DragY + SharedData->LastClickPos.Y, 1.f);
	FVector4 ScreenDelta = ScreenNewPos - ScreenOldPos;
	FVector4 ProjectedDelta = View->ScreenToWorld(ScreenDelta);
	FVector4 WorldDelta;

	//Now we project new ScreenPos to xy-plane of SimGrabLocation
	FVector LocalOffset = View->ViewMatrices.GetViewMatrix().TransformPosition(SimGrabLocation + SimGrabZ * SimGrabPush);
	float ZDistance = InViewportClient->GetViewportType() == ELevelViewportType::LVT_Perspective ? FMath::Abs(LocalOffset.Z) : 1.f;	//in the ortho case we don't need to do any fixup because there is no perspective
	WorldDelta = ProjectedDelta * ZDistance;

	//Now we convert back into WorldPos
	FVector WorldPos = SimGrabLocation + WorldDelta + SimGrabZ * SimGrabPush;
	FVector NewLocation = WorldPos;
	float QuickRadius = 5 - SimGrabPush / SimHoldDistanceChangeDelta;
	QuickRadius = QuickRadius < 2 ? 2 : QuickRadius;

	DrawDebugPoint(GetWorld(), NewLocation, QuickRadius, FColorList::Red, false, 0.3);

	SharedData->MouseHandle->SetTargetLocation(NewLocation);
	SharedData->MouseHandle->GrabbedComponent->WakeRigidBody(SharedData->MouseHandle->GrabbedBoneName);
}

bool FPhysicsAssetEditorEditMode::SimMouseRelease()
{
	SharedData->bManipulating = false;

	if (!SharedData->MouseHandle->GrabbedComponent)
	{
		return false;
	}

	SharedData->MouseHandle->GrabbedComponent->WakeRigidBody(SharedData->MouseHandle->GrabbedBoneName);
	SharedData->MouseHandle->ReleaseComponent();

	return true;
}

bool FPhysicsAssetEditorEditMode::SimMouseWheelUp(FEditorViewportClient* InViewportClient)
{
	if (!SharedData->MouseHandle->GrabbedComponent)
	{
		return false;
	}

	SimGrabPush += SimHoldDistanceChangeDelta;

	SimMouseMove(InViewportClient, 0.0f, 0.0f);

	return true;
}

bool FPhysicsAssetEditorEditMode::SimMouseWheelDown(FEditorViewportClient* InViewportClient)
{
	if (!SharedData->MouseHandle->GrabbedComponent)
	{
		return false;
	}

	SimGrabPush -= SimHoldDistanceChangeDelta;
	SimGrabPush = FMath::Max(SimGrabMinPush, SimGrabPush);

	SimMouseMove(InViewportClient, 0.0f, 0.0f);

	return true;
}

void FPhysicsAssetEditorEditMode::ModifyPrimitiveSize(int32 BodyIndex, EAggCollisionShape::Type PrimType, int32 PrimIndex, FVector DeltaSize)
{
	check(SharedData->GetSelectedBody());

	FKAggregateGeom* AggGeom = &SharedData->PhysicsAsset->SkeletalBodySetups[BodyIndex]->AggGeom;

	if (PrimType == EAggCollisionShape::Sphere)
	{
		check(AggGeom->SphereElems.IsValidIndex(PrimIndex));
		AggGeom->SphereElems[PrimIndex].ScaleElem(DeltaSize, MinPrimSize);
	}
	else if (PrimType == EAggCollisionShape::Box)
	{
		check(AggGeom->BoxElems.IsValidIndex(PrimIndex));
		AggGeom->BoxElems[PrimIndex].ScaleElem(DeltaSize, MinPrimSize);
	}
	else if (PrimType == EAggCollisionShape::Sphyl)
	{
		check(AggGeom->SphylElems.IsValidIndex(PrimIndex));
		AggGeom->SphylElems[PrimIndex].ScaleElem(DeltaSize, MinPrimSize);
	}
	else if (PrimType == EAggCollisionShape::Convex)
	{
		check(AggGeom->ConvexElems.IsValidIndex(PrimIndex));

		FVector ModifiedSize = DeltaSize;
		if (GEditor->UsePercentageBasedScaling())
		{
			ModifiedSize = DeltaSize * ((GEditor->GetScaleGridSize() / 100.0f) / GEditor->GetGridSize());
		}

		AggGeom->ConvexElems[PrimIndex].ScaleElem(ModifiedSize, MinPrimSize);
	}
}

void FPhysicsAssetEditorEditMode::HitNothing(FEditorViewportClient* InViewportClient)
{
	if (InViewportClient->IsCtrlPressed() == false)	//we only want to deselect if Ctrl is not used
	{
		SharedData->ClearSelectedBody();
		SharedData->ClearSelectedConstraints();
	}

	InViewportClient->Invalidate();
	PhysicsAssetEditorPtr.Pin()->RefreshHierachyTree();
}

#undef LOCTEXT_NAMESPACE
