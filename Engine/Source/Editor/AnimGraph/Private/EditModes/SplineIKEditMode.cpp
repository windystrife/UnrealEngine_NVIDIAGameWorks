// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SplineIKEditMode.h"
#include "IPersonaPreviewScene.h"
#include "AnimGraphNode_SplineIK.h"
#include "Engine/StaticMeshActor.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "SceneManagement.h"
#include "Materials/Material.h"

FSplineIKEditMode::FSplineIKEditMode()
	: SplineIKRuntimeNode(nullptr)
	, SplineIKGraphNode(nullptr)
	, SelectedSplinePoint(0)
	, WidgetMode(FWidget::WM_None)
{
}

void FSplineIKEditMode::EnterMode(UAnimGraphNode_Base* InEditorNode, FAnimNode_Base* InRuntimeNode)
{
	SplineIKRuntimeNode = static_cast<FAnimNode_SplineIK*>(InRuntimeNode);
	SplineIKGraphNode = CastChecked<UAnimGraphNode_SplineIK>(InEditorNode);

	WidgetMode = FindValidWidgetMode(FWidget::WM_None);

	FAnimNodeEditMode::EnterMode(InEditorNode, InRuntimeNode);
}

void FSplineIKEditMode::ExitMode()
{
	SplineIKGraphNode = nullptr;
	SplineIKRuntimeNode = nullptr;

	FAnimNodeEditMode::ExitMode();
}

ECoordSystem FSplineIKEditMode::GetWidgetCoordinateSystem() const
{ 
	return COORD_Local;
}

struct HSplineHandleHitProxy : public HHitProxy
{
	DECLARE_HIT_PROXY()

	int32 SplineHandleIndex;

	HSplineHandleHitProxy(int32 InSplineHandleIndex)
		: HHitProxy(HPP_World)
		, SplineHandleIndex(InSplineHandleIndex)
	{
	}

	// HHitProxy interface
	virtual EMouseCursor::Type GetMouseCursor() override { return EMouseCursor::CardinalCross; }
	// End of HHitProxy interface
};

IMPLEMENT_HIT_PROXY(HSplineHandleHitProxy, HHitProxy)

void FSplineIKEditMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	UDebugSkelMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();

	USplineComponent::Draw(PDI, View, SplineIKRuntimeNode->GetTransformedSplineCurves().Position, SkelComp->GetComponentTransform().ToMatrixWithScale(), FLinearColor::Yellow, SDPG_Foreground);

	for (int32 SplineHandleIndex = 0; SplineHandleIndex < SplineIKRuntimeNode->GetNumControlPoints(); SplineHandleIndex++)
	{
		PDI->SetHitProxy(new HSplineHandleHitProxy(SplineHandleIndex));
		FTransform StartTransform = SplineIKRuntimeNode->GetTransformedSplinePoint(SplineHandleIndex);
		const float Scale = View->WorldToScreen(StartTransform.GetLocation()).W * (4.0f / View->UnscaledViewRect.Width() / View->ViewMatrices.GetProjectionMatrix().M[0][0]);
		DrawSphere(PDI, StartTransform.GetLocation(), FRotator::ZeroRotator, FVector(4.0f) * Scale, 64, 64, GEngine->ArrowMaterial->GetRenderProxy(SelectedSplinePoint == SplineHandleIndex), SDPG_Foreground);
		DrawCoordinateSystem(PDI, StartTransform.GetLocation(), StartTransform.GetRotation().Rotator(), 30.0f * Scale, SDPG_Foreground);
	}

	PDI->SetHitProxy(nullptr);
}

FVector FSplineIKEditMode::GetWidgetLocation() const
{
	if (SelectedSplinePoint != INDEX_NONE)
	{
		FVector Location = SplineIKRuntimeNode->GetTransformedSplinePoint(SelectedSplinePoint).GetLocation();

		UDebugSkelMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();
		return SkelComp->GetComponentTransform().TransformPosition(Location);
	}

	return FVector::ZeroVector;
}

FWidget::EWidgetMode FSplineIKEditMode::GetWidgetMode() const
{
	return WidgetMode;
}

bool FSplineIKEditMode::IsModeValid(FWidget::EWidgetMode InWidgetMode) const
{
	// @TODO: when transforms are exposed as pin, deny editing via widget
	return true;
}


FWidget::EWidgetMode FSplineIKEditMode::GetNextWidgetMode(FWidget::EWidgetMode InWidgetMode) const
{
	FWidget::EWidgetMode InMode = InWidgetMode;
	switch (InMode)
	{
	case FWidget::WM_Translate:
		return FWidget::WM_Rotate;
	case FWidget::WM_Rotate:
		return FWidget::WM_Scale;
	case FWidget::WM_Scale:
		return FWidget::WM_Translate;
	case FWidget::WM_TranslateRotateZ:
	case FWidget::WM_2D:
		break;
	}

	return FWidget::WM_None;
}

FWidget::EWidgetMode FSplineIKEditMode::FindValidWidgetMode(FWidget::EWidgetMode InWidgetMode) const
{
	FWidget::EWidgetMode InMode = InWidgetMode;
	FWidget::EWidgetMode ValidMode = InMode;
	if (InMode == FWidget::WM_None)
	{	
		// starts from translate mode
		ValidMode = FWidget::WM_Translate;
	}

	// find from current widget mode and loop 1 cycle until finding a valid mode
	for (int32 Index = 0; Index < 3; Index++)
	{
		if (IsModeValid(ValidMode))
		{
			return ValidMode;
		}

		ValidMode = GetNextWidgetMode(ValidMode);
	}

	// if couldn't find a valid mode, returns None
	ValidMode = FWidget::WM_None;

	return ValidMode;
}

FWidget::EWidgetMode FSplineIKEditMode::ChangeToNextWidgetMode(FWidget::EWidgetMode CurWidgetMode)
{
	FWidget::EWidgetMode NextWidgetMode = GetNextWidgetMode(CurWidgetMode);
	WidgetMode = FindValidWidgetMode(NextWidgetMode);

	return WidgetMode;
}

bool FSplineIKEditMode::SetWidgetMode(FWidget::EWidgetMode InWidgetMode)
{
	WidgetMode = InWidgetMode;
	return true;
}

FName FSplineIKEditMode::GetSelectedBone() const
{
	return NAME_None;
}

bool FSplineIKEditMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	bool bResult = FAnimNodeEditMode::HandleClick(InViewportClient, HitProxy, Click);

	if (HitProxy != nullptr && HitProxy->IsA(HSplineHandleHitProxy::StaticGetType()))
	{
		HSplineHandleHitProxy* HandleHitProxy = static_cast<HSplineHandleHitProxy*>(HitProxy);
		SelectedSplinePoint = HandleHitProxy->SplineHandleIndex;
		bResult = true;
	}

	return bResult;
}

bool FSplineIKEditMode::GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	UDebugSkelMeshComponent* SkelMeshComp = GetAnimPreviewScene().GetPreviewMeshComponent();
	if (SkelMeshComp)
	{
		if (SelectedSplinePoint != INDEX_NONE)
		{
			FTransform Transform = SplineIKRuntimeNode->GetTransformedSplinePoint(SelectedSplinePoint);
			FTransform WorldTransform = Transform * SkelMeshComp->GetComponentTransform();
			InMatrix = WorldTransform.ToMatrixNoScale().RemoveTranslation();
		}

		return true;
	}

	return false;
}

void FSplineIKEditMode::DoTranslation(FVector& InTranslation)
{
	if (SelectedSplinePoint != INDEX_NONE)
	{
		FVector NewLocation = SplineIKRuntimeNode->GetControlPoint(SelectedSplinePoint).GetLocation() + InTranslation;
		SplineIKRuntimeNode->SetControlPointLocation(SelectedSplinePoint, NewLocation);
		SplineIKGraphNode->Node.SetControlPointLocation(SelectedSplinePoint, NewLocation);
	}
}

void FSplineIKEditMode::DoRotation(FRotator& InRot)
{
	if (SelectedSplinePoint != INDEX_NONE)
	{
		FQuat NewRotation = SplineIKRuntimeNode->GetControlPoint(SelectedSplinePoint).GetRotation() * InRot.Quaternion();
		SplineIKRuntimeNode->SetControlPointRotation(SelectedSplinePoint, NewRotation);
		SplineIKGraphNode->Node.SetControlPointRotation(SelectedSplinePoint, NewRotation);
	}
}

void FSplineIKEditMode::DoScale(FVector& InScale)
{
	if (SelectedSplinePoint != INDEX_NONE)
	{
		FVector NewScale = SplineIKRuntimeNode->GetControlPoint(SelectedSplinePoint).GetScale3D() + InScale;
		SplineIKRuntimeNode->SetControlPointScale(SelectedSplinePoint, NewScale);
		SplineIKGraphNode->Node.SetControlPointScale(SelectedSplinePoint, NewScale);
	}
}
