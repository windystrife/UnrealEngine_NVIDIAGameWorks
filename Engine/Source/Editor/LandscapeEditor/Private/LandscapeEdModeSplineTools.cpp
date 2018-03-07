// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "InputCoreTypes.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"
#include "Components/MeshComponent.h"
#include "Exporters/Exporter.h"
#include "Editor/UnrealEdEngine.h"
#include "UObject/PropertyPortFlags.h"
#include "EngineUtils.h"
#include "EditorUndoClient.h"
#include "UnrealWidget.h"
#include "EditorModeManager.h"
#include "UnrealEdGlobals.h"
#include "EditorViewportClient.h"
#include "LandscapeToolInterface.h"
#include "LandscapeProxy.h"
#include "LandscapeEdMode.h"
#include "ScopedTransaction.h"
#include "LandscapeRender.h"
#include "LandscapeSplineProxies.h"
#include "PropertyEditorModule.h"
#include "LandscapeSplineImportExport.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeSplineSegment.h"
#include "LandscapeSplineControlPoint.h"
#include "ControlPointMeshComponent.h"
#include "Containers/Algo/Copy.h"
#include "HAL/PlatformApplicationMisc.h"
#include "UnrealExporter.h"


#define LOCTEXT_NAMESPACE "Landscape"

//
// FLandscapeToolSplines
//
class FLandscapeToolSplines : public FLandscapeTool, public FEditorUndoClient
{
public:
	FLandscapeToolSplines(FEdModeLandscape* InEdMode)
		: EdMode(InEdMode)
		, LandscapeInfo(NULL)
		, SelectedSplineControlPoints()
		, SelectedSplineSegments()
		, DraggingTangent_Segment(NULL)
		, DraggingTangent_End(false)
		, bMovingControlPoint(false)
		, bAutoRotateOnJoin(true)
		, bAutoChangeConnectionsOnMove(true)
		, bDeleteLooseEnds(false)
		, bCopyMeshToNewControlPoint(false)
	{
		// Register to update when an undo/redo operation has been called to update our list of actors
		GEditor->RegisterForUndo(this);
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(LandscapeInfo);
		Collector.AddReferencedObjects(SelectedSplineControlPoints);
		Collector.AddReferencedObjects(SelectedSplineSegments);
		Collector.AddReferencedObject(DraggingTangent_Segment);
	}

	~FLandscapeToolSplines()
	{
		// GEditor is invalid at shutdown as the object system is unloaded before the landscape module.
		if (UObjectInitialized())
		{
			// Remove undo delegate
			GEditor->UnregisterForUndo(this);
		}
	}

	virtual const TCHAR* GetToolName() override { return TEXT("Splines"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Splines", "Splines"); };

	virtual void SetEditRenderType() override { GLandscapeEditRenderMode = ELandscapeEditRenderMode::None | (GLandscapeEditRenderMode & ELandscapeEditRenderMode::BitMaskForMask); }
	virtual bool SupportsMask() override { return false; }

	void CreateSplineComponent(ALandscapeProxy* Landscape, FVector Scale3D)
	{
		Landscape->Modify();
		Landscape->SplineComponent = NewObject<ULandscapeSplinesComponent>(Landscape, NAME_None, RF_Transactional);
		Landscape->SplineComponent->RelativeScale3D = Scale3D;
		Landscape->SplineComponent->AttachToComponent(Landscape->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		Landscape->SplineComponent->ShowSplineEditorMesh(true);
	}

	void UpdatePropertiesWindows()
	{
		if (GLevelEditorModeTools().IsModeActive(EdMode->GetID()))
		{
			TArray<UObject*> Objects;
			Objects.Reset(SelectedSplineControlPoints.Num() + SelectedSplineSegments.Num());
			Algo::Copy(SelectedSplineControlPoints, Objects);
			Algo::Copy(SelectedSplineSegments, Objects);

			FPropertyEditorModule& PropertyModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
			PropertyModule.UpdatePropertyViews(Objects);
		}
	}

	void ClearSelectedControlPoints()
	{
		for (ULandscapeSplineControlPoint* ControlPoint : SelectedSplineControlPoints)
		{
			checkSlow(ControlPoint->IsSplineSelected());
			ControlPoint->Modify(false);
			ControlPoint->SetSplineSelected(false);
		}
		SelectedSplineControlPoints.Empty();
	}

	void ClearSelectedSegments()
	{
		for (ULandscapeSplineSegment* Segment : SelectedSplineSegments)
		{
			checkSlow(Segment->IsSplineSelected());
			Segment->Modify(false);
			Segment->SetSplineSelected(false);
		}
		SelectedSplineSegments.Empty();
	}

	void ClearSelection()
	{
		ClearSelectedControlPoints();
		ClearSelectedSegments();
	}

	void DeselectControlPoint(ULandscapeSplineControlPoint* ControlPoint)
	{
		checkSlow(ControlPoint->IsSplineSelected());
		SelectedSplineControlPoints.Remove(ControlPoint);
		ControlPoint->Modify(false);
		ControlPoint->SetSplineSelected(false);
	}

	void DeSelectSegment(ULandscapeSplineSegment* Segment)
	{
		checkSlow(Segment->IsSplineSelected());
		SelectedSplineSegments.Remove(Segment);
		Segment->Modify(false);
		Segment->SetSplineSelected(false);
	}

	void SelectControlPoint(ULandscapeSplineControlPoint* ControlPoint)
	{
		checkSlow(!ControlPoint->IsSplineSelected());
		SelectedSplineControlPoints.Add(ControlPoint);
		ControlPoint->Modify(false);
		ControlPoint->SetSplineSelected(true);
	}

	void SelectSegment(ULandscapeSplineSegment* Segment)
	{
		checkSlow(!Segment->IsSplineSelected());
		SelectedSplineSegments.Add(Segment);
		Segment->Modify(false);
		Segment->SetSplineSelected(true);

		GLevelEditorModeTools().SetWidgetMode(FWidget::WM_Scale);
	}

	void SelectConnected()
	{
		TArray<ULandscapeSplineControlPoint*> ControlPointsToProcess = SelectedSplineControlPoints.Array();

		while (ControlPointsToProcess.Num() > 0)
		{
			const ULandscapeSplineControlPoint* ControlPoint = ControlPointsToProcess.Pop();

			for (const FLandscapeSplineConnection& Connection : ControlPoint->ConnectedSegments)
			{
				ULandscapeSplineControlPoint* OtherEnd = Connection.GetFarConnection().ControlPoint;

				if (!OtherEnd->IsSplineSelected())
				{
					SelectControlPoint(OtherEnd);
					ControlPointsToProcess.Add(OtherEnd);
				}
			}
		}

		TArray<ULandscapeSplineSegment*> SegmentsToProcess = SelectedSplineSegments.Array();

		while (SegmentsToProcess.Num() > 0)
		{
			const ULandscapeSplineSegment* Segment = SegmentsToProcess.Pop();

			for (const FLandscapeSplineSegmentConnection& SegmentConnection : Segment->Connections)
			{
				for (const FLandscapeSplineConnection& Connection : SegmentConnection.ControlPoint->ConnectedSegments)
				{
					if (Connection.Segment != Segment && !Connection.Segment->IsSplineSelected())
					{
						SelectSegment(Connection.Segment);
						SegmentsToProcess.Add(Connection.Segment);
					}
				}
			}
		}
	}

	void SelectAdjacentControlPoints()
	{
		for (ULandscapeSplineSegment* Segment : SelectedSplineSegments)
		{
			if (!Segment->Connections[0].ControlPoint->IsSplineSelected())
			{
				SelectControlPoint(Segment->Connections[0].ControlPoint);
			}
			if (!Segment->Connections[1].ControlPoint->IsSplineSelected())
			{
				SelectControlPoint(Segment->Connections[1].ControlPoint);
			}
		}
	}

	void SelectAdjacentSegments()
	{
		for (ULandscapeSplineControlPoint* ControlPoint : SelectedSplineControlPoints)
		{
			for (const FLandscapeSplineConnection& Connection : ControlPoint->ConnectedSegments)
			{
				if (!Connection.Segment->IsSplineSelected())
				{
					SelectSegment(Connection.Segment);
				}
			}
		}
	}

	void AddSegment(ULandscapeSplineControlPoint* Start, ULandscapeSplineControlPoint* End, bool bAutoRotateStart, bool bAutoRotateEnd)
	{
		FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_AddSegment", "Add Landscape Spline Segment"));

		if (Start == End)
		{
			//UE_LOG( TEXT("Can't join spline control point to itself.") );
			return;
		}

		if (Start->GetOuterULandscapeSplinesComponent() != End->GetOuterULandscapeSplinesComponent())
		{
			//UE_LOG( TEXT("Can't join spline control points across different terrains.") );
			return;
		}

		for (const FLandscapeSplineConnection& Connection : Start->ConnectedSegments)
		{
			// if the *other* end on the connected segment connects to the "end" control point...
			if (Connection.GetFarConnection().ControlPoint == End)
			{
				//UE_LOG( TEXT("Spline control points already joined connected!") );
				return;
			}
		}

		ULandscapeSplinesComponent* SplinesComponent = Start->GetOuterULandscapeSplinesComponent();
		SplinesComponent->Modify();
		Start->Modify();
		End->Modify();

		ULandscapeSplineSegment* NewSegment = NewObject<ULandscapeSplineSegment>(SplinesComponent, NAME_None, RF_Transactional);
		SplinesComponent->Segments.Add(NewSegment);

		NewSegment->Connections[0].ControlPoint = Start;
		NewSegment->Connections[1].ControlPoint = End;

		NewSegment->Connections[0].SocketName = Start->GetBestConnectionTo(End->Location);
		NewSegment->Connections[1].SocketName = End->GetBestConnectionTo(Start->Location);

		FVector StartLocation; FRotator StartRotation;
		Start->GetConnectionLocationAndRotation(NewSegment->Connections[0].SocketName, StartLocation, StartRotation);
		FVector EndLocation; FRotator EndRotation;
		End->GetConnectionLocationAndRotation(NewSegment->Connections[1].SocketName, EndLocation, EndRotation);

		// Set up tangent lengths
		NewSegment->Connections[0].TangentLen = (EndLocation - StartLocation).Size();
		NewSegment->Connections[1].TangentLen = NewSegment->Connections[0].TangentLen;

		NewSegment->AutoFlipTangents();

		// set up other segment options
		ULandscapeSplineSegment* CopyFromSegment = nullptr;
		if (Start->ConnectedSegments.Num() > 0)
		{
			CopyFromSegment = Start->ConnectedSegments[0].Segment;
		}
		else if (End->ConnectedSegments.Num() > 0)
		{
			CopyFromSegment = End->ConnectedSegments[0].Segment;
		}
		else
		{
			// Use defaults
		}

		if (CopyFromSegment != nullptr)
		{
			NewSegment->LayerName         = CopyFromSegment->LayerName;
			NewSegment->SplineMeshes      = CopyFromSegment->SplineMeshes;
			NewSegment->LDMaxDrawDistance = CopyFromSegment->LDMaxDrawDistance;
			NewSegment->bRaiseTerrain     = CopyFromSegment->bRaiseTerrain;
			NewSegment->bLowerTerrain     = CopyFromSegment->bLowerTerrain;
			NewSegment->bPlaceSplineMeshesInStreamingLevels = CopyFromSegment->bPlaceSplineMeshesInStreamingLevels;
			NewSegment->bEnableCollision  = CopyFromSegment->bEnableCollision;
			NewSegment->bCastShadow       = CopyFromSegment->bCastShadow;
		}

		Start->ConnectedSegments.Add(FLandscapeSplineConnection(NewSegment, 0));
		End->ConnectedSegments.Add(FLandscapeSplineConnection(NewSegment, 1));

		bool bUpdatedStart = false;
		bool bUpdatedEnd = false;
		if (bAutoRotateStart)
		{
			Start->AutoCalcRotation();
			Start->UpdateSplinePoints();
			bUpdatedStart = true;
		}
		if (bAutoRotateEnd)
		{
			End->AutoCalcRotation();
			End->UpdateSplinePoints();
			bUpdatedEnd = true;
		}

		// Control points' points are currently based on connected segments, so need to be updated.
		if (!bUpdatedStart && Start->Mesh)
		{
			Start->UpdateSplinePoints();
		}
		if (!bUpdatedEnd && End->Mesh)
		{
			End->UpdateSplinePoints();
		}

		// If we've called UpdateSplinePoints on either control point it will already have called UpdateSplinePoints on the new segment
		if (!(bUpdatedStart || bUpdatedEnd))
		{
			NewSegment->UpdateSplinePoints();
		}
	}

	void AddControlPoint(ULandscapeSplinesComponent* SplinesComponent, const FVector& LocalLocation)
	{
		FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_AddControlPoint", "Add Landscape Spline Control Point"));

		SplinesComponent->Modify();

		ULandscapeSplineControlPoint* NewControlPoint = NewObject<ULandscapeSplineControlPoint>(SplinesComponent, NAME_None, RF_Transactional);
		SplinesComponent->ControlPoints.Add(NewControlPoint);

		NewControlPoint->Location = LocalLocation;

		if (SelectedSplineControlPoints.Num() > 0)
		{
			ULandscapeSplineControlPoint* FirstPoint = *SelectedSplineControlPoints.CreateConstIterator();
			NewControlPoint->Rotation    = (NewControlPoint->Location - FirstPoint->Location).Rotation();
			NewControlPoint->Width       = FirstPoint->Width;
			NewControlPoint->SideFalloff = FirstPoint->SideFalloff;
			NewControlPoint->EndFalloff  = FirstPoint->EndFalloff;

			if (bCopyMeshToNewControlPoint)
			{
				NewControlPoint->Mesh             = FirstPoint->Mesh;
				NewControlPoint->MeshScale        = FirstPoint->MeshScale;
				NewControlPoint->bPlaceSplineMeshesInStreamingLevels = FirstPoint->bPlaceSplineMeshesInStreamingLevels;
				NewControlPoint->bEnableCollision = FirstPoint->bEnableCollision;
				NewControlPoint->bCastShadow      = FirstPoint->bCastShadow;
			}

			for (ULandscapeSplineControlPoint* ControlPoint : SelectedSplineControlPoints)
			{
				AddSegment(ControlPoint, NewControlPoint, bAutoRotateOnJoin, true);
			}
		}
		else
		{
			// required to make control point visible
			NewControlPoint->UpdateSplinePoints();
		}

		ClearSelection();
		SelectControlPoint(NewControlPoint);
		UpdatePropertiesWindows();

		if (!SplinesComponent->IsRegistered())
		{
			SplinesComponent->RegisterComponent();
		}
		else
		{
			SplinesComponent->MarkRenderStateDirty();
		}
	}

	void DeleteSegment(ULandscapeSplineSegment* ToDelete, bool bInDeleteLooseEnds)
	{
		FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_DeleteSegment", "Delete Landscape Spline Segment"));

		ULandscapeSplinesComponent* SplinesComponent = ToDelete->GetOuterULandscapeSplinesComponent();
		SplinesComponent->Modify();

		ToDelete->Modify();
		ToDelete->DeleteSplinePoints();

		ToDelete->Connections[0].ControlPoint->Modify();
		ToDelete->Connections[1].ControlPoint->Modify();
		ToDelete->Connections[0].ControlPoint->ConnectedSegments.Remove(FLandscapeSplineConnection(ToDelete, 0));
		ToDelete->Connections[1].ControlPoint->ConnectedSegments.Remove(FLandscapeSplineConnection(ToDelete, 1));

		if (bInDeleteLooseEnds)
		{
			if (ToDelete->Connections[0].ControlPoint->ConnectedSegments.Num() == 0)
			{
				SplinesComponent->ControlPoints.Remove(ToDelete->Connections[0].ControlPoint);
			}
			if (ToDelete->Connections[1].ControlPoint != ToDelete->Connections[0].ControlPoint
				&& ToDelete->Connections[1].ControlPoint->ConnectedSegments.Num() == 0)
			{
				SplinesComponent->ControlPoints.Remove(ToDelete->Connections[1].ControlPoint);
			}
		}

		SplinesComponent->Segments.Remove(ToDelete);

		// Control points' points are currently based on connected segments, so need to be updated.
		if (ToDelete->Connections[0].ControlPoint->Mesh != NULL)
		{
			ToDelete->Connections[0].ControlPoint->UpdateSplinePoints();
		}
		if (ToDelete->Connections[1].ControlPoint->Mesh != NULL)
		{
			ToDelete->Connections[1].ControlPoint->UpdateSplinePoints();
		}

		SplinesComponent->MarkRenderStateDirty();
	}

	void DeleteControlPoint(ULandscapeSplineControlPoint* ToDelete, bool bInDeleteLooseEnds)
	{
		FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_DeleteControlPoint", "Delete Landscape Spline Control Point"));

		ULandscapeSplinesComponent* SplinesComponent = ToDelete->GetOuterULandscapeSplinesComponent();
		SplinesComponent->Modify();

		ToDelete->Modify();
		ToDelete->DeleteSplinePoints();

		if (ToDelete->ConnectedSegments.Num() == 2
			&& ToDelete->ConnectedSegments[0].Segment != ToDelete->ConnectedSegments[1].Segment)
		{
			int32 Result = FMessageDialog::Open(EAppMsgType::YesNoCancel, LOCTEXT("WantToJoinControlPoint", "Control point has two segments attached, do you want to join them?"));
			switch (Result)
			{
			case EAppReturnType::Yes:
			{
				// Copy the other end of connection 1 into the near end of connection 0, then delete connection 1
				TArray<FLandscapeSplineConnection>& Connections = ToDelete->ConnectedSegments;
				Connections[0].Segment->Modify();
				Connections[1].Segment->Modify();

				Connections[0].GetNearConnection() = Connections[1].GetFarConnection();
				Connections[0].Segment->UpdateSplinePoints();

				Connections[1].Segment->DeleteSplinePoints();

				// Get the control point at the *other* end of the segment and remove it from it
				ULandscapeSplineControlPoint* OtherEnd = Connections[1].GetFarConnection().ControlPoint;
				OtherEnd->Modify();

				FLandscapeSplineConnection* OtherConnection = OtherEnd->ConnectedSegments.FindByKey(FLandscapeSplineConnection(Connections[1].Segment, 1 - Connections[1].End));
				*OtherConnection = FLandscapeSplineConnection(Connections[0].Segment, Connections[0].End);

				SplinesComponent->Segments.Remove(Connections[1].Segment);

				ToDelete->ConnectedSegments.Empty();

				SplinesComponent->ControlPoints.Remove(ToDelete);
				SplinesComponent->MarkRenderStateDirty();

				return;
			}
				break;
			case EAppReturnType::No:
				// Use the "delete all segments" code below
				break;
			case EAppReturnType::Cancel:
				// Do nothing
				return;
			}
		}

		for (FLandscapeSplineConnection& Connection : ToDelete->ConnectedSegments)
		{
			Connection.Segment->Modify();
			Connection.Segment->DeleteSplinePoints();

			// Get the control point at the *other* end of the segment and remove it from it
			ULandscapeSplineControlPoint* OtherEnd = Connection.GetFarConnection().ControlPoint;
			OtherEnd->Modify();
			OtherEnd->ConnectedSegments.Remove(FLandscapeSplineConnection(Connection.Segment, 1 - Connection.End));
			SplinesComponent->Segments.Remove(Connection.Segment);

			if (bInDeleteLooseEnds)
			{
				if (OtherEnd != ToDelete
					&& OtherEnd->ConnectedSegments.Num() == 0)
				{
					SplinesComponent->ControlPoints.Remove(OtherEnd);
				}
			}
		}

		ToDelete->ConnectedSegments.Empty();

		SplinesComponent->ControlPoints.Remove(ToDelete);
		SplinesComponent->MarkRenderStateDirty();
	}

	void SplitSegment(ULandscapeSplineSegment* Segment, const FVector& LocalLocation)
	{
		FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_SplitSegment", "Split Landscape Spline Segment"));

		ULandscapeSplinesComponent* SplinesComponent = Segment->GetOuterULandscapeSplinesComponent();
		SplinesComponent->Modify();
		Segment->Modify();
		Segment->Connections[1].ControlPoint->Modify();

		float t;
		FVector Location;
		FVector Tangent;
		Segment->FindNearest(LocalLocation, t, Location, Tangent);

		ULandscapeSplineControlPoint* NewControlPoint = NewObject<ULandscapeSplineControlPoint>(SplinesComponent, NAME_None, RF_Transactional);
		SplinesComponent->ControlPoints.Add(NewControlPoint);

		NewControlPoint->Location = Location;
		NewControlPoint->Rotation = Tangent.Rotation();
		NewControlPoint->Rotation.Roll = FMath::Lerp(Segment->Connections[0].ControlPoint->Rotation.Roll, Segment->Connections[1].ControlPoint->Rotation.Roll, t);
		NewControlPoint->Width = FMath::Lerp(Segment->Connections[0].ControlPoint->Width, Segment->Connections[1].ControlPoint->Width, t);
		NewControlPoint->SideFalloff = FMath::Lerp(Segment->Connections[0].ControlPoint->SideFalloff, Segment->Connections[1].ControlPoint->SideFalloff, t);
		NewControlPoint->EndFalloff = FMath::Lerp(Segment->Connections[0].ControlPoint->EndFalloff, Segment->Connections[1].ControlPoint->EndFalloff, t);

		ULandscapeSplineSegment* NewSegment = NewObject<ULandscapeSplineSegment>(SplinesComponent, NAME_None, RF_Transactional);
		SplinesComponent->Segments.Add(NewSegment);

		NewSegment->Connections[0].ControlPoint = NewControlPoint;
		NewSegment->Connections[0].TangentLen = Tangent.Size() * (1 - t);
		NewSegment->Connections[0].ControlPoint->ConnectedSegments.Add(FLandscapeSplineConnection(NewSegment, 0));
		NewSegment->Connections[1].ControlPoint = Segment->Connections[1].ControlPoint;
		NewSegment->Connections[1].TangentLen = Segment->Connections[1].TangentLen * (1 - t);
		NewSegment->Connections[1].ControlPoint->ConnectedSegments.Add(FLandscapeSplineConnection(NewSegment, 1));
		NewSegment->LayerName = Segment->LayerName;
		NewSegment->SplineMeshes = Segment->SplineMeshes;
		NewSegment->LDMaxDrawDistance = Segment->LDMaxDrawDistance;
		NewSegment->bRaiseTerrain = Segment->bRaiseTerrain;
		NewSegment->bLowerTerrain = Segment->bLowerTerrain;
		NewSegment->bEnableCollision = Segment->bEnableCollision;
		NewSegment->bCastShadow = Segment->bCastShadow;

		Segment->Connections[0].TangentLen *= t;
		Segment->Connections[1].ControlPoint->ConnectedSegments.Remove(FLandscapeSplineConnection(Segment, 1));
		Segment->Connections[1].ControlPoint = NewControlPoint;
		Segment->Connections[1].TangentLen = -Tangent.Size() * t;
		Segment->Connections[1].ControlPoint->ConnectedSegments.Add(FLandscapeSplineConnection(Segment, 1));

		Segment->UpdateSplinePoints();
		NewSegment->UpdateSplinePoints();

		ClearSelection();
		UpdatePropertiesWindows();

		SplinesComponent->MarkRenderStateDirty();
	}

	void FlipSegment(ULandscapeSplineSegment* Segment)
	{
		FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_FlipSegment", "Flip Landscape Spline Segment"));

		ULandscapeSplinesComponent* SplinesComponent = Segment->GetOuterULandscapeSplinesComponent();
		SplinesComponent->Modify();
		Segment->Modify();

		Segment->Connections[0].ControlPoint->Modify();
		Segment->Connections[1].ControlPoint->Modify();
		Segment->Connections[0].ControlPoint->ConnectedSegments.FindByKey(FLandscapeSplineConnection(Segment, 0))->End = 1;
		Segment->Connections[1].ControlPoint->ConnectedSegments.FindByKey(FLandscapeSplineConnection(Segment, 1))->End = 0;
		Swap(Segment->Connections[0], Segment->Connections[1]);

		Segment->UpdateSplinePoints();
	}

	void SnapControlPointToGround(ULandscapeSplineControlPoint* ControlPoint)
	{
		FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_SnapToGround", "Snap Landscape Spline to Ground"));

		ULandscapeSplinesComponent* SplinesComponent = ControlPoint->GetOuterULandscapeSplinesComponent();
		SplinesComponent->Modify();
		ControlPoint->Modify();

		const FTransform LocalToWorld = SplinesComponent->GetComponentToWorld();
		const FVector Start = LocalToWorld.TransformPosition(ControlPoint->Location);
		const FVector End = Start + FVector(0, 0, -HALF_WORLD_MAX);

		static FName TraceTag = FName(TEXT("SnapLandscapeSplineControlPointToGround"));
		FHitResult Hit;
		UWorld* World = SplinesComponent->GetWorld();
		check(World);
		if (World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(NAME_None, FCollisionQueryParams::GetUnknownStatId(),true)))
		{
			ControlPoint->Location = LocalToWorld.InverseTransformPosition(Hit.Location);
			ControlPoint->UpdateSplinePoints();
			SplinesComponent->MarkRenderStateDirty();
		}
	}

	void MoveSelectedToLevel()
	{
		TSet<ALandscapeProxy*> FromProxies;
		ALandscapeProxy* ToLandscape = nullptr;

		for (ULandscapeSplineControlPoint* ControlPoint : SelectedSplineControlPoints)
		{
			ULandscapeSplinesComponent* LandscapeSplinesComp = ControlPoint->GetOuterULandscapeSplinesComponent();
			ALandscapeProxy* FromProxy = LandscapeSplinesComp ? Cast<ALandscapeProxy>(LandscapeSplinesComp->GetOuter()) : nullptr;
			if (FromProxy)
			{
				if (!ToLandscape)
				{
					ULandscapeInfo* ProxyLandscapeInfo = FromProxy->GetLandscapeInfo();
					check(ProxyLandscapeInfo);
					ToLandscape = ProxyLandscapeInfo->GetCurrentLevelLandscapeProxy(true);
					if (!ToLandscape)
					{
						// No Landscape Proxy, don't support for creating only for Spline now
						return;
					}
				}

				if (ToLandscape != FromProxy)
				{
					ToLandscape->Modify();
					if (ToLandscape->SplineComponent == nullptr)
					{
						CreateSplineComponent(ToLandscape, FromProxy->SplineComponent->RelativeScale3D);
						check(ToLandscape->SplineComponent);
					}
					ToLandscape->SplineComponent->Modify();

					const FTransform OldToNewTransform =
						FromProxy->SplineComponent->GetComponentTransform().GetRelativeTransform(ToLandscape->SplineComponent->GetComponentTransform());

					if (FromProxies.Find(FromProxy) == nullptr)
					{
						FromProxies.Add(FromProxy);
						FromProxy->Modify();
						FromProxy->SplineComponent->Modify();
						FromProxy->SplineComponent->MarkRenderStateDirty();
					}

					// Handle control point mesh
					if (ControlPoint->bPlaceSplineMeshesInStreamingLevels)
					{
						// Mark previously local component as Foreign
						if (ControlPoint->LocalMeshComponent)
						{
							auto* MeshComponent = ControlPoint->LocalMeshComponent;
							verifySlow(FromProxy->SplineComponent->MeshComponentLocalOwnersMap.Remove(MeshComponent) == 1);
							FromProxy->SplineComponent->AddForeignMeshComponent(ControlPoint, MeshComponent);
						}
						ControlPoint->LocalMeshComponent = nullptr;

						// Mark previously foreign component as local
						auto* MeshComponent = ToLandscape->SplineComponent->GetForeignMeshComponent(ControlPoint);
						if (MeshComponent)
						{
							ToLandscape->SplineComponent->RemoveForeignMeshComponent(ControlPoint, MeshComponent);
							ToLandscape->SplineComponent->MeshComponentLocalOwnersMap.Add(MeshComponent, ControlPoint);
						}
						ControlPoint->LocalMeshComponent = MeshComponent;
					}
					else
					{
						// non-streaming case
						if (ControlPoint->LocalMeshComponent)
						{
							UControlPointMeshComponent* MeshComponent = ControlPoint->LocalMeshComponent;
							MeshComponent->Modify();
							MeshComponent->UnregisterComponent();
							MeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
							MeshComponent->InvalidateLightingCache();
							MeshComponent->Rename(nullptr, ToLandscape);
							MeshComponent->AttachToComponent(ToLandscape->SplineComponent, FAttachmentTransformRules::KeepWorldTransform);

							verifySlow(FromProxy->SplineComponent->MeshComponentLocalOwnersMap.Remove(MeshComponent) == 1);
							ToLandscape->SplineComponent->MeshComponentLocalOwnersMap.Add(MeshComponent, ControlPoint);
						}
					}

					// Move control point to new level
					FromProxy->SplineComponent->ControlPoints.Remove(ControlPoint);
					ControlPoint->Rename(nullptr, ToLandscape->SplineComponent);
					ToLandscape->SplineComponent->ControlPoints.Add(ControlPoint);

					ControlPoint->Location = OldToNewTransform.TransformPosition(ControlPoint->Location);

					ControlPoint->UpdateSplinePoints(true, false);
				}
			}
		}

		for (ULandscapeSplineSegment* Segment : SelectedSplineSegments)
		{
			ULandscapeSplinesComponent* LandscapeSplinesComp = Segment->GetOuterULandscapeSplinesComponent();
			ALandscapeProxy* FromProxy = LandscapeSplinesComp ? Cast<ALandscapeProxy>(LandscapeSplinesComp->GetOuter()) : nullptr;
			if (FromProxy)
			{
				if (!ToLandscape)
				{
					ULandscapeInfo* ProxyLandscapeInfo = FromProxy->GetLandscapeInfo();
					check(ProxyLandscapeInfo);
					ToLandscape = ProxyLandscapeInfo->GetCurrentLevelLandscapeProxy(true);
					if (!ToLandscape)
					{
						// No Landscape Proxy, don't support for creating only for Spline now
						return;
					}
				}

				if (ToLandscape != FromProxy)
				{
					ToLandscape->Modify();
					if (ToLandscape->SplineComponent == nullptr)
					{
						CreateSplineComponent(ToLandscape, FromProxy->SplineComponent->RelativeScale3D);
						check(ToLandscape->SplineComponent);
					}
					ToLandscape->SplineComponent->Modify();

					if (FromProxies.Find(FromProxy) == nullptr)
					{
						FromProxies.Add(FromProxy);
						FromProxy->Modify();
						FromProxy->SplineComponent->Modify();
						FromProxy->SplineComponent->MarkRenderStateDirty();
					}

					// Handle spline meshes
					if (Segment->bPlaceSplineMeshesInStreamingLevels)
					{
						// Mark previously local components as Foreign
						for (auto* MeshComponent : Segment->LocalMeshComponents)
						{
							verifySlow(FromProxy->SplineComponent->MeshComponentLocalOwnersMap.Remove(MeshComponent) == 1);
							FromProxy->SplineComponent->AddForeignMeshComponent(Segment, MeshComponent);
						}
						Segment->LocalMeshComponents.Empty();

						// Mark previously foreign components as local
						TArray<USplineMeshComponent*> MeshComponents = ToLandscape->SplineComponent->GetForeignMeshComponents(Segment);
						ToLandscape->SplineComponent->RemoveAllForeignMeshComponents(Segment);
						for (auto* MeshComponent : MeshComponents)
						{
							ToLandscape->SplineComponent->MeshComponentLocalOwnersMap.Add(MeshComponent, Segment);
						}
						Segment->LocalMeshComponents = MoveTemp(MeshComponents);
					}
					else
					{
						// non-streaming case
						for (auto* MeshComponent : Segment->LocalMeshComponents)
						{
							MeshComponent->Modify();
							MeshComponent->UnregisterComponent();
							MeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
							MeshComponent->InvalidateLightingCache();
							MeshComponent->Rename(nullptr, ToLandscape);
							MeshComponent->AttachToComponent(ToLandscape->SplineComponent, FAttachmentTransformRules::KeepWorldTransform);

							verifySlow(FromProxy->SplineComponent->MeshComponentLocalOwnersMap.Remove(MeshComponent) == 1);
							ToLandscape->SplineComponent->MeshComponentLocalOwnersMap.Add(MeshComponent, Segment);
						}
					}

					// Move segment to new level
					FromProxy->SplineComponent->Segments.Remove(Segment);
					Segment->Rename(nullptr, ToLandscape->SplineComponent);
					ToLandscape->SplineComponent->Segments.Add(Segment);

					Segment->UpdateSplinePoints();
				}
			}
		}

		if (ToLandscape && ToLandscape->SplineComponent)
		{
			if (!ToLandscape->SplineComponent->IsRegistered())
			{
				ToLandscape->SplineComponent->RegisterComponent();
			}
			else
			{
				ToLandscape->SplineComponent->MarkRenderStateDirty();
			}
		}

		GUnrealEd->RedrawLevelEditingViewports();
	}

	void ShowSplineProperties()
	{
		TArray<UObject*> Objects;
		Objects.Reset(SelectedSplineControlPoints.Num() + SelectedSplineSegments.Num());
		Algo::Copy(SelectedSplineControlPoints, Objects);
		Algo::Copy(SelectedSplineSegments, Objects);

		FPropertyEditorModule& PropertyModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		if (!PropertyModule.HasUnlockedDetailViews())
		{
			PropertyModule.CreateFloatingDetailsView(Objects, true);
		}
		else
		{
			PropertyModule.UpdatePropertyViews(Objects);
		}
	}

	virtual bool BeginTool(FEditorViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, const FVector& InHitLocation) override
	{
		if (ViewportClient->IsCtrlPressed())
		{
			LandscapeInfo = InTarget.LandscapeInfo.Get();
			ALandscapeProxy* Landscape = LandscapeInfo->GetCurrentLevelLandscapeProxy(true);
			if (!Landscape)
			{
				return false;
			}

			ULandscapeSplinesComponent* SplinesComponent = nullptr;
			if (SelectedSplineControlPoints.Num() > 0)
			{
				ULandscapeSplineControlPoint* FirstPoint = *SelectedSplineControlPoints.CreateConstIterator();
				SplinesComponent = FirstPoint->GetOuterULandscapeSplinesComponent();
			}

			if (!SplinesComponent)
			{
				if (!Landscape->SplineComponent)
				{
					CreateSplineComponent(Landscape, FVector(1.0f) / Landscape->GetRootComponent()->RelativeScale3D);
					check(Landscape->SplineComponent);
				}
				SplinesComponent = Landscape->SplineComponent;
			}

			const FTransform LandscapeToSpline = Landscape->LandscapeActorToWorld().GetRelativeTransform(SplinesComponent->GetComponentTransform());

			AddControlPoint(SplinesComponent, LandscapeToSpline.TransformPosition(InHitLocation));

			GUnrealEd->RedrawLevelEditingViewports();

			return true;
		}

		return false;
	}

	virtual void EndTool(FEditorViewportClient* ViewportClient) override
	{
		LandscapeInfo = NULL;
	}

	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override
	{
		FVector HitLocation;
		if (EdMode->LandscapeMouseTrace(ViewportClient, x, y, HitLocation))
		{
			//if( bToolActive )
			//{
			//	// Apply tool
			//	ApplyTool(ViewportClient);
			//}
		}

		return true;
	}

	virtual void ApplyTool(FEditorViewportClient* ViewportClient)
	{
	}

	virtual bool HandleClick(HHitProxy* HitProxy, const FViewportClick& Click) override
	{
		if ((!HitProxy || !HitProxy->IsA(HWidgetAxis::StaticGetType()))
			&& !Click.IsShiftDown())
		{
			ClearSelection();
			UpdatePropertiesWindows();
			GUnrealEd->RedrawLevelEditingViewports();
		}

		if (HitProxy)
		{
			ULandscapeSplineControlPoint* ClickedControlPoint = NULL;
			ULandscapeSplineSegment* ClickedSplineSegment = NULL;

			if (HitProxy->IsA(HLandscapeSplineProxy_ControlPoint::StaticGetType()))
			{
				HLandscapeSplineProxy_ControlPoint* SplineProxy = (HLandscapeSplineProxy_ControlPoint*)HitProxy;
				ClickedControlPoint = SplineProxy->ControlPoint;
			}
			else if (HitProxy->IsA(HLandscapeSplineProxy_Segment::StaticGetType()))
			{
				HLandscapeSplineProxy_Segment* SplineProxy = (HLandscapeSplineProxy_Segment*)HitProxy;
				ClickedSplineSegment = SplineProxy->SplineSegment;
			}
			else if (HitProxy->IsA(HActor::StaticGetType()))
			{
				HActor* ActorProxy = (HActor*)HitProxy;
				AActor* Actor = ActorProxy->Actor;
				const UMeshComponent* MeshComponent = Cast<UMeshComponent>(ActorProxy->PrimComponent);
				if (MeshComponent)
				{
					ULandscapeSplinesComponent* SplineComponent = Actor->FindComponentByClass<ULandscapeSplinesComponent>();
					if (SplineComponent)
					{
						UObject* ComponentOwner = SplineComponent->GetOwnerForMeshComponent(MeshComponent);
						if (ComponentOwner)
						{
							if (ULandscapeSplineControlPoint* ControlPoint = Cast<ULandscapeSplineControlPoint>(ComponentOwner))
							{
								ClickedControlPoint = ControlPoint;
							}
							else if (ULandscapeSplineSegment* SplineSegment = Cast<ULandscapeSplineSegment>(ComponentOwner))
							{
								ClickedSplineSegment = SplineSegment;
							}
						}
					}
				}
			}

			if (ClickedControlPoint != NULL)
			{
				if (Click.IsShiftDown() && ClickedControlPoint->IsSplineSelected())
				{
					DeselectControlPoint(ClickedControlPoint);
				}
				else
				{
					SelectControlPoint(ClickedControlPoint);
				}
				GEditor->SelectNone(true, true);
				UpdatePropertiesWindows();

				GUnrealEd->RedrawLevelEditingViewports();
				return true;
			}
			else if (ClickedSplineSegment != NULL)
			{
				// save info about what we grabbed
				if (Click.IsShiftDown() && ClickedSplineSegment->IsSplineSelected())
				{
					DeSelectSegment(ClickedSplineSegment);
				}
				else
				{
					SelectSegment(ClickedSplineSegment);
				}
				GEditor->SelectNone(true, true);
				UpdatePropertiesWindows();

				GUnrealEd->RedrawLevelEditingViewports();
				return true;
			}
		}

		return false;
	}

	virtual bool InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) override
	{
		if (InKey == EKeys::F4 && InEvent == IE_Pressed)
		{
			if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
			{
				ShowSplineProperties();
				return true;
			}
		}

		if (InKey == EKeys::R && InEvent == IE_Pressed)
		{
			if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
			{
				FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_AutoRotate", "Auto-rotate Landscape Spline Control Points"));

				for (ULandscapeSplineControlPoint* ControlPoint : SelectedSplineControlPoints)
				{
					ControlPoint->AutoCalcRotation();
					ControlPoint->UpdateSplinePoints();
				}

				for (ULandscapeSplineSegment* Segment : SelectedSplineSegments)
				{
					Segment->Connections[0].ControlPoint->AutoCalcRotation();
					Segment->Connections[0].ControlPoint->UpdateSplinePoints();
					Segment->Connections[1].ControlPoint->AutoCalcRotation();
					Segment->Connections[1].ControlPoint->UpdateSplinePoints();
				}

				return true;
			}
		}

		if (InKey == EKeys::F && InEvent == IE_Pressed)
		{
			if (SelectedSplineSegments.Num() > 0)
			{
				FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_FlipSegments", "Flip Landscape Spline Segments"));

				for (ULandscapeSplineSegment* Segment : SelectedSplineSegments)
				{
					FlipSegment(Segment);
				}

				return true;
			}
		}

		if (InKey == EKeys::T && InEvent == IE_Pressed)
		{
			if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
			{
				FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_AutoFlipTangents", "Auto-flip Landscape Spline Tangents"));

				for (ULandscapeSplineControlPoint* ControlPoint : SelectedSplineControlPoints)
				{
					ControlPoint->AutoFlipTangents();
					ControlPoint->UpdateSplinePoints();
				}

				for (ULandscapeSplineSegment* Segment : SelectedSplineSegments)
				{
					Segment->Connections[0].ControlPoint->AutoFlipTangents();
					Segment->Connections[0].ControlPoint->UpdateSplinePoints();
					Segment->Connections[1].ControlPoint->AutoFlipTangents();
					Segment->Connections[1].ControlPoint->UpdateSplinePoints();
				}

				return true;
			}
		}

		if (InKey == EKeys::End && InEvent == IE_Pressed)
		{
			if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
			{
				FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_SnapToGround", "Snap Landscape Spline to Ground"));

				for (ULandscapeSplineControlPoint* ControlPoint : SelectedSplineControlPoints)
				{
					SnapControlPointToGround(ControlPoint);
				}
				for (ULandscapeSplineSegment* Segment : SelectedSplineSegments)
				{
					SnapControlPointToGround(Segment->Connections[0].ControlPoint);
					SnapControlPointToGround(Segment->Connections[1].ControlPoint);
				}
				UpdatePropertiesWindows();

				GUnrealEd->RedrawLevelEditingViewports();
				return true;
			}
		}

		if (InKey == EKeys::A && InEvent == IE_Pressed
			&& IsCtrlDown(InViewport))
		{
			if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
			{
				SelectConnected();

				UpdatePropertiesWindows();

				GUnrealEd->RedrawLevelEditingViewports();
				return true;
			}
		}

		if (SelectedSplineControlPoints.Num() > 0)
		{
			if (InKey == EKeys::LeftMouseButton && InEvent == IE_Pressed
				&& IsCtrlDown(InViewport))
			{
				int32 HitX = InViewport->GetMouseX();
				int32 HitY = InViewport->GetMouseY();
				HHitProxy* HitProxy = InViewport->GetHitProxy(HitX, HitY);
				if (HitProxy != NULL)
				{
					ULandscapeSplineControlPoint* ClickedControlPoint = NULL;

					if (HitProxy->IsA(HLandscapeSplineProxy_ControlPoint::StaticGetType()))
					{
						HLandscapeSplineProxy_ControlPoint* SplineProxy = (HLandscapeSplineProxy_ControlPoint*)HitProxy;
						ClickedControlPoint = SplineProxy->ControlPoint;
					}
					else if (HitProxy->IsA(HActor::StaticGetType()))
					{
						HActor* ActorProxy = (HActor*)HitProxy;
						AActor* Actor = ActorProxy->Actor;
						const UMeshComponent* MeshComponent = Cast<UMeshComponent>(ActorProxy->PrimComponent);
						if (MeshComponent)
						{
							ULandscapeSplinesComponent* SplineComponent = Actor->FindComponentByClass<ULandscapeSplinesComponent>();
							if (SplineComponent)
							{
								UObject* ComponentOwner = SplineComponent->GetOwnerForMeshComponent(MeshComponent);
								if (ComponentOwner)
								{
									if (ULandscapeSplineControlPoint* ControlPoint = Cast<ULandscapeSplineControlPoint>(ComponentOwner))
									{
										ClickedControlPoint = ControlPoint;
									}
								}
							}
						}
					}

					if (ClickedControlPoint != NULL)
					{
						FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_AddSegment", "Add Landscape Spline Segment"));

						for (ULandscapeSplineControlPoint* ControlPoint : SelectedSplineControlPoints)
						{
							AddSegment(ControlPoint, ClickedControlPoint, bAutoRotateOnJoin, bAutoRotateOnJoin);
						}

						GUnrealEd->RedrawLevelEditingViewports();

						return true;
					}
				}
			}
		}

		if (SelectedSplineControlPoints.Num() == 0)
		{
			if (InKey == EKeys::LeftMouseButton && InEvent == IE_Pressed
				&& IsCtrlDown(InViewport))
			{
				int32 HitX = InViewport->GetMouseX();
				int32 HitY = InViewport->GetMouseY();
				HHitProxy* HitProxy = InViewport->GetHitProxy(HitX, HitY);
				if (HitProxy)
				{
					ULandscapeSplineSegment* ClickedSplineSegment = NULL;
					FTransform LandscapeToSpline;

					if (HitProxy->IsA(HLandscapeSplineProxy_Segment::StaticGetType()))
					{
						HLandscapeSplineProxy_Segment* SplineProxy = (HLandscapeSplineProxy_Segment*)HitProxy;
						ClickedSplineSegment = SplineProxy->SplineSegment;
						ALandscapeProxy* LandscapeProxy = ClickedSplineSegment->GetTypedOuter<ALandscapeProxy>();
						check(LandscapeProxy);
						LandscapeToSpline = LandscapeProxy->LandscapeActorToWorld().GetRelativeTransform(ClickedSplineSegment->GetOuterULandscapeSplinesComponent()->GetComponentTransform());
					}
					else if (HitProxy->IsA(HActor::StaticGetType()))
					{
						HActor* ActorProxy = (HActor*)HitProxy;
						AActor* Actor = ActorProxy->Actor;
						const UMeshComponent* MeshComponent = Cast<UMeshComponent>(ActorProxy->PrimComponent);
						if (MeshComponent)
						{
							ULandscapeSplinesComponent* SplineComponent = Actor->FindComponentByClass<ULandscapeSplinesComponent>();
							if (SplineComponent)
							{
								UObject* ComponentOwner = SplineComponent->GetOwnerForMeshComponent(MeshComponent);
								if (ComponentOwner)
								{
									if (ULandscapeSplineSegment* SplineSegment = Cast<ULandscapeSplineSegment>(ComponentOwner))
									{
										ClickedSplineSegment = SplineSegment;
										ALandscapeProxy* LandscapeProxy = CastChecked<ALandscapeProxy>(SplineComponent->GetOwner());
										LandscapeToSpline = LandscapeProxy->LandscapeActorToWorld().GetRelativeTransform(SplineComponent->GetComponentTransform());
									}
								}
							}
						}
					}

					if (ClickedSplineSegment != NULL)
					{
						FVector HitLocation;
						if (EdMode->LandscapeMouseTrace(InViewportClient, HitLocation))
						{
							FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_SplitSegment", "Split Landscape Spline Segment"));

							SplitSegment(ClickedSplineSegment, LandscapeToSpline.TransformPosition(HitLocation));

							GUnrealEd->RedrawLevelEditingViewports();
						}

						return true;
					}
				}
			}
		}

		if (InKey == EKeys::LeftMouseButton)
		{
			// Press mouse button
			if (InEvent == IE_Pressed)
			{
				// See if we clicked on a spline handle..
				int32 HitX = InViewport->GetMouseX();
				int32 HitY = InViewport->GetMouseY();
				HHitProxy*	HitProxy = InViewport->GetHitProxy(HitX, HitY);
				if (HitProxy)
				{
					if (HitProxy->IsA(HWidgetAxis::StaticGetType()))
					{
						checkSlow(SelectedSplineControlPoints.Num() > 0);
						bMovingControlPoint = true;

						GEditor->BeginTransaction(LOCTEXT("LandscapeSpline_ModifyControlPoint", "Modify Landscape Spline Control Point"));
						for (ULandscapeSplineControlPoint* ControlPoint : SelectedSplineControlPoints)
						{
							ControlPoint->Modify();
							ControlPoint->GetOuterULandscapeSplinesComponent()->Modify();
						}

						return false; // We're not actually handling this case ourselves, just wrapping it in a transaction
					}
					else if (HitProxy->IsA(HLandscapeSplineProxy_Tangent::StaticGetType()))
					{
						HLandscapeSplineProxy_Tangent* SplineProxy = (HLandscapeSplineProxy_Tangent*)HitProxy;
						DraggingTangent_Segment = SplineProxy->SplineSegment;
						DraggingTangent_End = SplineProxy->End;

						GEditor->BeginTransaction(LOCTEXT("LandscapeSpline_ModifyTangent", "Modify Landscape Spline Tangent"));
						ULandscapeSplinesComponent* SplinesComponent = DraggingTangent_Segment->GetOuterULandscapeSplinesComponent();
						SplinesComponent->Modify();
						DraggingTangent_Segment->Modify();

						return false; // false to let FEditorViewportClient.InputKey start mouse tracking and enable InputDelta() so we can use it
					}
				}
			}
			else if (InEvent == IE_Released)
			{
				if (bMovingControlPoint)
				{
					bMovingControlPoint = false;

					for (ULandscapeSplineControlPoint* ControlPoint : SelectedSplineControlPoints)
					{
						ControlPoint->UpdateSplinePoints(true);
					}

					GEditor->EndTransaction();

					return false; // We're not actually handling this case ourselves, just wrapping it in a transaction
				}
				else if (DraggingTangent_Segment)
				{
					DraggingTangent_Segment->UpdateSplinePoints(true);

					DraggingTangent_Segment = NULL;

					GEditor->EndTransaction();

					return false; // false to let FEditorViewportClient.InputKey end mouse tracking
				}
			}
		}

		return false;
	}

	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) override
	{
		FVector Drag = InDrag;

		if (DraggingTangent_Segment)
		{
			const ULandscapeSplinesComponent* SplinesComponent = DraggingTangent_Segment->GetOuterULandscapeSplinesComponent();
			FLandscapeSplineSegmentConnection& Connection = DraggingTangent_Segment->Connections[DraggingTangent_End];

			FVector StartLocation; FRotator StartRotation;
			Connection.ControlPoint->GetConnectionLocationAndRotation(Connection.SocketName, StartLocation, StartRotation);

			float OldTangentLen = Connection.TangentLen;
			Connection.TangentLen += SplinesComponent->GetComponentTransform().InverseTransformVector(-Drag) | StartRotation.Vector();

			// Disallow a tangent of exactly 0
			if (Connection.TangentLen == 0)
			{
				if (OldTangentLen > 0)
				{
					Connection.TangentLen = SMALL_NUMBER;
				}
				else
				{
					Connection.TangentLen = -SMALL_NUMBER;
				}
			}

			// Flipping the tangent is only allowed if not using a socket
			if (Connection.SocketName != NAME_None)
			{
				Connection.TangentLen = FMath::Max(SMALL_NUMBER, Connection.TangentLen);
			}

			DraggingTangent_Segment->UpdateSplinePoints(false);

			return true;
		}

		if (SelectedSplineControlPoints.Num() > 0 && InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
		{
			for (ULandscapeSplineControlPoint* ControlPoint : SelectedSplineControlPoints)
			{
				const ULandscapeSplinesComponent* SplinesComponent = ControlPoint->GetOuterULandscapeSplinesComponent();

				ControlPoint->Location += SplinesComponent->GetComponentTransform().InverseTransformVector(Drag);

				FVector RotAxis; float RotAngle;
				InRot.Quaternion().ToAxisAndAngle(RotAxis, RotAngle);
				RotAxis = (SplinesComponent->GetComponentTransform().GetRotation().Inverse() * ControlPoint->Rotation.Quaternion().Inverse()).RotateVector(RotAxis);

				// Hack: for some reason FQuat.Rotator() Clamps to 0-360 range, so use .GetNormalized() to recover the original negative rotation.
				ControlPoint->Rotation += FQuat(RotAxis, RotAngle).Rotator().GetNormalized();

				ControlPoint->Rotation.Yaw = FRotator::NormalizeAxis(ControlPoint->Rotation.Yaw);
				ControlPoint->Rotation.Pitch = FMath::Clamp(ControlPoint->Rotation.Pitch, -85.0f, 85.0f);
				ControlPoint->Rotation.Roll = FMath::Clamp(ControlPoint->Rotation.Roll, -85.0f, 85.0f);

				if (bAutoChangeConnectionsOnMove)
				{
					ControlPoint->AutoSetConnections(true);
				}

				ControlPoint->UpdateSplinePoints(false);
			}

			return true;
		}

		return false;
	}

	void FixSelection()
	{
		SelectedSplineControlPoints.Empty();
		SelectedSplineSegments.Empty();

		if (EdMode->CurrentTool != nullptr && EdMode->CurrentTool == this)
		{
			for (const FLandscapeListInfo& Info : EdMode->GetLandscapeList())
			{
				Info.Info->ForAllLandscapeProxies([this](ALandscapeProxy* Proxy)
				{
					if (Proxy->SplineComponent)
					{
						Algo::CopyIf(Proxy->SplineComponent->ControlPoints, SelectedSplineControlPoints, &ULandscapeSplineControlPoint::IsSplineSelected);
						Algo::CopyIf(Proxy->SplineComponent->Segments,      SelectedSplineSegments,      &ULandscapeSplineSegment::IsSplineSelected);
					}
				});
			}
		}
		else
		{
			for (const FLandscapeListInfo& Info : EdMode->GetLandscapeList())
			{
				Info.Info->ForAllLandscapeProxies([](ALandscapeProxy* Proxy)
				{
					if (Proxy->SplineComponent)
					{
						for (ULandscapeSplineControlPoint* ControlPoint : Proxy->SplineComponent->ControlPoints)
						{
							ControlPoint->SetSplineSelected(false);
						}

						for (ULandscapeSplineSegment* Segment : Proxy->SplineComponent->Segments)
						{
							Segment->SetSplineSelected(false);
						}
					}
				});
			}
		}
	}

	void OnUndo()
	{
		FixSelection();
		UpdatePropertiesWindows();
	}

	virtual void EnterTool() override
	{
		GEditor->SelectNone(true, true, false);

		for (const FLandscapeListInfo& Info : EdMode->GetLandscapeList())
		{
			Info.Info->ForAllLandscapeProxies([](ALandscapeProxy* Proxy)
			{
				if (Proxy->SplineComponent)
				{
					Proxy->SplineComponent->ShowSplineEditorMesh(true);
				}
			});
		}
	}

	virtual void ExitTool() override
	{
		ClearSelection();
		UpdatePropertiesWindows();

		for (const FLandscapeListInfo& Info : EdMode->GetLandscapeList())
		{
			Info.Info->ForAllLandscapeProxies([](ALandscapeProxy* Proxy)
			{
				if (Proxy->SplineComponent)
				{
					Proxy->SplineComponent->ShowSplineEditorMesh(false);
				}
			});
		}
	}

	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override
	{
		// The editor can try to render the tool before the UpdateLandscapeEditorData command runs and the landscape editor realizes that the landscape has been hidden/deleted
		const ALandscapeProxy* LandscapeProxy = EdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();
		if (LandscapeProxy)
		{
			for (ULandscapeSplineControlPoint* ControlPoint : SelectedSplineControlPoints)
			{
				const ULandscapeSplinesComponent* SplinesComponent = ControlPoint->GetOuterULandscapeSplinesComponent();

				FVector HandlePos0 = SplinesComponent->GetComponentTransform().TransformPosition(ControlPoint->Location + ControlPoint->Rotation.Vector() * -20);
				FVector HandlePos1 = SplinesComponent->GetComponentTransform().TransformPosition(ControlPoint->Location + ControlPoint->Rotation.Vector() * 20);
				DrawDashedLine(PDI, HandlePos0, HandlePos1, FColor::White, 20, SDPG_Foreground);

				if (GLevelEditorModeTools().GetWidgetMode() == FWidget::WM_Scale)
				{
					for (const FLandscapeSplineConnection& Connection : ControlPoint->ConnectedSegments)
					{
						FVector StartLocation; FRotator StartRotation;
						Connection.GetNearConnection().ControlPoint->GetConnectionLocationAndRotation(Connection.GetNearConnection().SocketName, StartLocation, StartRotation);

						FVector StartPos = SplinesComponent->GetComponentTransform().TransformPosition(StartLocation);
						FVector HandlePos = SplinesComponent->GetComponentTransform().TransformPosition(StartLocation + StartRotation.Vector() * Connection.GetNearConnection().TangentLen / 2);
						PDI->DrawLine(StartPos, HandlePos, FColor::White, SDPG_Foreground);

						if (PDI->IsHitTesting()) PDI->SetHitProxy(new HLandscapeSplineProxy_Tangent(Connection.Segment, Connection.End));
						PDI->DrawPoint(HandlePos, FColor::White, 10.0f, SDPG_Foreground);
						if (PDI->IsHitTesting()) PDI->SetHitProxy(NULL);
					}
				}
			}

			if (GLevelEditorModeTools().GetWidgetMode() == FWidget::WM_Scale)
			{
				for (ULandscapeSplineSegment* Segment : SelectedSplineSegments)
				{
					const ULandscapeSplinesComponent* SplinesComponent = Segment->GetOuterULandscapeSplinesComponent();
					for (int32 End = 0; End <= 1; End++)
					{
						const FLandscapeSplineSegmentConnection& Connection = Segment->Connections[End];

						FVector StartLocation; FRotator StartRotation;
						Connection.ControlPoint->GetConnectionLocationAndRotation(Connection.SocketName, StartLocation, StartRotation);

						FVector EndPos = SplinesComponent->GetComponentTransform().TransformPosition(StartLocation);
						FVector EndHandlePos = SplinesComponent->GetComponentTransform().TransformPosition(StartLocation + StartRotation.Vector() * Connection.TangentLen / 2);

						PDI->DrawLine(EndPos, EndHandlePos, FColor::White, SDPG_Foreground);
						if (PDI->IsHitTesting()) PDI->SetHitProxy(new HLandscapeSplineProxy_Tangent(Segment, !!End));
						PDI->DrawPoint(EndHandlePos, FColor::White, 10.0f, SDPG_Foreground);
						if (PDI->IsHitTesting()) PDI->SetHitProxy(NULL);
					}
				}
			}
		}
	}

	virtual bool OverrideSelection() const override
	{
		return true;
	}

	virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelection) const override
	{
		// Only filter selection not deselection
		if (bInSelection)
		{
			return false;
		}

		return true;
	}

	virtual bool UsesTransformWidget() const override
	{
		if (SelectedSplineControlPoints.Num() > 0)
		{
			// The editor can try to render the transform widget before the landscape editor ticks and realizes that the landscape has been hidden/deleted
			const ALandscapeProxy* LandscapeProxy = EdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();
			if (LandscapeProxy)
			{
				return true;
			}
		}

		return false;
	}

	virtual EAxisList::Type GetWidgetAxisToDraw(FWidget::EWidgetMode CheckMode) const override
	{
		if (SelectedSplineControlPoints.Num() > 0)
		{
			//if (CheckMode == FWidget::WM_Rotate
			//	&& SelectedSplineControlPoints.Num() >= 2)
			//{
			//	return AXIS_X;
			//}
			//else
			if (CheckMode != FWidget::WM_Scale)
			{
				return EAxisList::XYZ;
			}
			else
			{
				return EAxisList::None;
			}
		}

		return EAxisList::None;
	}

	virtual FVector GetWidgetLocation() const override
	{
		if (SelectedSplineControlPoints.Num() > 0)
		{
			const ALandscapeProxy* LandscapeProxy = EdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();
			if (LandscapeProxy)
			{
				ULandscapeSplineControlPoint* FirstPoint = *SelectedSplineControlPoints.CreateConstIterator();
				ULandscapeSplinesComponent* SplinesComponent = FirstPoint->GetOuterULandscapeSplinesComponent();
				return SplinesComponent->GetComponentTransform().TransformPosition(FirstPoint->Location);
			}
		}

		return FVector::ZeroVector;
	}

	virtual FMatrix GetWidgetRotation() const override
	{
		if (SelectedSplineControlPoints.Num() > 0)
		{
			const ALandscapeProxy* LandscapeProxy = EdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();
			if (LandscapeProxy)
			{
				ULandscapeSplineControlPoint* FirstPoint = *SelectedSplineControlPoints.CreateConstIterator();
				ULandscapeSplinesComponent* SplinesComponent = FirstPoint->GetOuterULandscapeSplinesComponent();
				return FQuatRotationTranslationMatrix(FirstPoint->Rotation.Quaternion() * SplinesComponent->GetComponentTransform().GetRotation(), FVector::ZeroVector);
			}
		}

		return FMatrix::Identity;
	}

	virtual EEditAction::Type GetActionEditDuplicate() override
	{
		if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
		{
			return EEditAction::Process;
		}

		return EEditAction::Skip;
	}

	virtual EEditAction::Type GetActionEditDelete() override
	{
		if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
		{
			return EEditAction::Process;
		}

		return EEditAction::Skip;
	}

	virtual EEditAction::Type GetActionEditCut() override
	{
		if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
		{
			return EEditAction::Process;
		}

		return EEditAction::Skip;
	}

	virtual EEditAction::Type GetActionEditCopy() override
	{
		if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
		{
			return EEditAction::Process;
		}

		return EEditAction::Skip;
	}

	virtual EEditAction::Type GetActionEditPaste() override
	{
		FString PasteString;
		FPlatformApplicationMisc::ClipboardPaste(PasteString);
		if (PasteString.StartsWith("BEGIN SPLINES"))
		{
			return EEditAction::Process;
		}

		return EEditAction::Skip;
	}

	virtual bool ProcessEditDuplicate() override
	{
		InternalProcessEditDuplicate();
		return true;
	}

	virtual bool ProcessEditDelete() override
	{
		InternalProcessEditDelete();
		return true;
	}

	virtual bool ProcessEditCut() override
	{
		InternalProcessEditCut();
		return true;
	}

	virtual bool ProcessEditCopy() override
	{
		InternalProcessEditCopy();
		return true;
	}

	virtual bool ProcessEditPaste() override
	{
		InternalProcessEditPaste();
		return true;
	}

	void InternalProcessEditDuplicate()
	{
		if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
		{
			FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_Duplicate", "Duplicate Landscape Splines"));

			FString Data;
			InternalProcessEditCopy(&Data);
			InternalProcessEditPaste(&Data, true);
		}
	}

	void InternalProcessEditDelete()
	{
		if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
		{
			FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_Delete", "Delete Landscape Splines"));

			for (ULandscapeSplineControlPoint* ControlPoint : SelectedSplineControlPoints)
			{
				DeleteControlPoint(ControlPoint, bDeleteLooseEnds);
			}
			for (ULandscapeSplineSegment* Segment : SelectedSplineSegments)
			{
				DeleteSegment(Segment, bDeleteLooseEnds);
			}
			ClearSelection();
			UpdatePropertiesWindows();

			GUnrealEd->RedrawLevelEditingViewports();
		}
	}

	void InternalProcessEditCut()
	{
		if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
		{
			FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_Cut", "Cut Landscape Splines"));

			InternalProcessEditCopy();
			InternalProcessEditDelete();
		}
	}

	void InternalProcessEditCopy(FString* OutData = NULL)
	{
		if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
		{
			TArray<UObject*> Objects;
			Objects.Reserve(SelectedSplineControlPoints.Num() + SelectedSplineSegments.Num() * 3); // worst case

			// Control Points then segments
			for (ULandscapeSplineControlPoint* ControlPoint : SelectedSplineControlPoints)
			{
				Objects.Add(ControlPoint);
			}
			for (ULandscapeSplineSegment* Segment : SelectedSplineSegments)
			{
				Objects.AddUnique(Segment->Connections[0].ControlPoint);
				Objects.AddUnique(Segment->Connections[1].ControlPoint);
			}
			for (ULandscapeSplineSegment* Segment : SelectedSplineSegments)
			{
				Objects.Add(Segment);
			}

			// Perform export to text format
			FStringOutputDevice Ar;
			const FExportObjectInnerContext Context;

			Ar.Logf(TEXT("Begin Splines\r\n"));
			for (UObject* Object : Objects)
			{
				UExporter::ExportToOutputDevice(&Context, Object, NULL, Ar, TEXT("copy"), 3, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false);
			}
			Ar.Logf(TEXT("End Splines\r\n"));

			if (OutData != NULL)
			{
				*OutData = MoveTemp(Ar);
			}
			else
			{
				FPlatformApplicationMisc::ClipboardCopy(*Ar);
			}
		}
	}

	void InternalProcessEditPaste(FString* InData = NULL, bool bOffset = false)
	{
		FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_Paste", "Paste Landscape Splines"));

		ALandscapeProxy* Landscape = EdMode->CurrentToolTarget.LandscapeInfo->GetCurrentLevelLandscapeProxy(true);
		if (!Landscape)
		{
			return;
		}
		if (!Landscape->SplineComponent)
		{
			CreateSplineComponent(Landscape, FVector(1.0f) / Landscape->GetRootComponent()->RelativeScale3D);
			check(Landscape->SplineComponent);
		}
		Landscape->SplineComponent->Modify();

		const TCHAR* Data = NULL;
		FString PasteString;
		if (InData != NULL)
		{
			Data = **InData;
		}
		else
		{
			FPlatformApplicationMisc::ClipboardPaste(PasteString);
			Data = *PasteString;
		}

		FLandscapeSplineTextObjectFactory Factory;
		TArray<UObject*> OutObjects = Factory.ImportSplines(Landscape->SplineComponent, Data);

		if (bOffset)
		{
			for (UObject* Object : OutObjects)
			{
				ULandscapeSplineControlPoint* ControlPoint = Cast<ULandscapeSplineControlPoint>(Object);
				if (ControlPoint != NULL)
				{
					Landscape->SplineComponent->ControlPoints.Add(ControlPoint);
					ControlPoint->Location += FVector(500, 500, 0);

					ControlPoint->UpdateSplinePoints();
				}
			}
		}
	}

protected:
	// Begin FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override { OnUndo(); }
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
	// End of FEditorUndoClient

protected:
	FEdModeLandscape* EdMode;
	ULandscapeInfo* LandscapeInfo;

	TSet<ULandscapeSplineControlPoint*> SelectedSplineControlPoints;
	TSet<ULandscapeSplineSegment*> SelectedSplineSegments;

	ULandscapeSplineSegment* DraggingTangent_Segment;
	uint32 DraggingTangent_End : 1;

	uint32 bMovingControlPoint : 1;

	uint32 bAutoRotateOnJoin : 1;
	uint32 bAutoChangeConnectionsOnMove : 1;
	uint32 bDeleteLooseEnds : 1;
	uint32 bCopyMeshToNewControlPoint : 1;

	friend FEdModeLandscape;
};


void FEdModeLandscape::ShowSplineProperties()
{
	if (SplinesTool /*&& SplinesTool == CurrentTool*/)
	{
		SplinesTool->ShowSplineProperties();
	}
}

void FEdModeLandscape::SelectAllConnectedSplineControlPoints()
{
	if (SplinesTool /*&& SplinesTool == CurrentTool*/)
	{
		SplinesTool->SelectAdjacentControlPoints();
		SplinesTool->ClearSelectedSegments();
		SplinesTool->SelectConnected();

		SplinesTool->UpdatePropertiesWindows();
		GUnrealEd->RedrawLevelEditingViewports();
	}
}

void FEdModeLandscape::SelectAllConnectedSplineSegments()
{
	if (SplinesTool /*&& SplinesTool == CurrentTool*/)
	{
		SplinesTool->SelectAdjacentSegments();
		SplinesTool->ClearSelectedControlPoints();
		SplinesTool->SelectConnected();

		SplinesTool->UpdatePropertiesWindows();
		GUnrealEd->RedrawLevelEditingViewports();
	}
}

void FEdModeLandscape::SplineMoveToCurrentLevel()
{
	FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_MoveToCurrentLevel", "Move Landscape Spline to current level"));

	if (SplinesTool /*&& SplinesTool == CurrentTool*/)
	{
		// Select all connected control points
		SplinesTool->SelectAdjacentSegments();
		SplinesTool->SelectAdjacentControlPoints();
		SplinesTool->SelectConnected();

		SplinesTool->MoveSelectedToLevel();

		SplinesTool->ClearSelection();
		SplinesTool->UpdatePropertiesWindows();
	}
}

void FEdModeLandscape::SetbUseAutoRotateOnJoin(bool InbAutoRotateOnJoin)
{
	if (SplinesTool /*&& SplinesTool == CurrentTool*/)
	{
		SplinesTool->bAutoRotateOnJoin = InbAutoRotateOnJoin;
	}
}

bool FEdModeLandscape::GetbUseAutoRotateOnJoin()
{
	if (SplinesTool /*&& SplinesTool == CurrentTool*/)
	{
		return SplinesTool->bAutoRotateOnJoin;
	}
	return true; // default value
}

void FEdModeLandscape::InitializeTool_Splines()
{
	auto Tool_Splines = MakeUnique<FLandscapeToolSplines>(this);
	Tool_Splines->ValidBrushes.Add("BrushSet_Splines");
	SplinesTool = Tool_Splines.Get();
	LandscapeTools.Add(MoveTemp(Tool_Splines));
}

#undef LOCTEXT_NAMESPACE
