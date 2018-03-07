// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VisualLoggerRenderingActor.h"
#include "AI/Navigation/NavigationSystem.h"
#include "VisualLogger/VisualLogger.h"
#include "LogVisualizerSettings.h"
#include "VisualLoggerDatabase.h"
#include "LogVisualizerPrivate.h"
#if WITH_EDITOR
#include "GeomTools.h"
#endif // WITH_EDITOR
#include "VisualLoggerRenderingComponent.h"

class UVisualLoggerRenderingComponent;
class FVisualLoggerSceneProxy : public FDebugRenderSceneProxy
{
public:
	FVisualLoggerSceneProxy(const UVisualLoggerRenderingComponent* InComponent)
		: FDebugRenderSceneProxy(InComponent)
	{
		DrawType = SolidAndWireMeshes;
		ViewFlagName = TEXT("VisLog");
		ViewFlagIndex = uint32(FEngineShowFlags::FindIndexByName(*ViewFlagName));
		bWantsSelectionOutline = false;
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bDynamicRelevance = true;
		// ideally the TranslucencyRelevance should be filled out by the material, here we do it conservative
		Result.bSeparateTranslucencyRelevance = Result.bNormalTranslucencyRelevance = IsShown(View) && GIsEditor;
		return Result;
	}

	virtual uint32 GetMemoryFootprint(void) const override { return sizeof(*this) + GetAllocatedSize(); }

	uint32 GetAllocatedSize(void) const
	{
		return FDebugRenderSceneProxy::GetAllocatedSize();
	}
};

UVisualLoggerRenderingComponent::UVisualLoggerRenderingComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

FPrimitiveSceneProxy* UVisualLoggerRenderingComponent::CreateSceneProxy()
{
	AVisualLoggerRenderingActor* RenderingActor = Cast<AVisualLoggerRenderingActor>(GetOuter());
	if (RenderingActor == NULL)
	{
		return NULL;
	}

	ULogVisualizerSettings *Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	FVisualLoggerSceneProxy *VLogSceneProxy = new FVisualLoggerSceneProxy(this);
	VLogSceneProxy->SolidMeshMaterial = Settings->GetDebugMeshMaterial();

	for (auto& CurrentShapes : RenderingActor->DebugShapesPerRow)
	{
		VLogSceneProxy->Spheres.Append(CurrentShapes.Value.Points);
		VLogSceneProxy->Lines.Append(CurrentShapes.Value.Lines);
		VLogSceneProxy->Boxes.Append(CurrentShapes.Value.Boxes);
		VLogSceneProxy->Meshes.Append(CurrentShapes.Value.Meshes);
		VLogSceneProxy->Cones.Append(CurrentShapes.Value.Cones);
		VLogSceneProxy->Texts.Append(CurrentShapes.Value.Texts);
		VLogSceneProxy->Cylinders.Append(CurrentShapes.Value.Cylinders);
		VLogSceneProxy->Capsles.Append(CurrentShapes.Value.Capsules);
	}

	{
		VLogSceneProxy->Spheres.Append(RenderingActor->TestDebugShapes.Points);
		VLogSceneProxy->Lines.Append(RenderingActor->TestDebugShapes.Lines);
		VLogSceneProxy->Boxes.Append(RenderingActor->TestDebugShapes.Boxes);
		VLogSceneProxy->Meshes.Append(RenderingActor->TestDebugShapes.Meshes);
		VLogSceneProxy->Cones.Append(RenderingActor->TestDebugShapes.Cones);
		VLogSceneProxy->Texts.Append(RenderingActor->TestDebugShapes.Texts);
		VLogSceneProxy->Cylinders.Append(RenderingActor->TestDebugShapes.Cylinders);
		VLogSceneProxy->Capsles.Append(RenderingActor->TestDebugShapes.Capsules);
	}

#if WITH_EDITOR
	if (VLogSceneProxy)
	{
		DebugDrawDelegateHelper.InitDelegateHelper(VLogSceneProxy);
		DebugDrawDelegateHelper.ReregisterDebugDrawDelgate();
	}
#endif
	return VLogSceneProxy;
}

FBoxSphereBounds UVisualLoggerRenderingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox MyBounds;
	MyBounds.Init();

	MyBounds = FBox(FVector(-HALF_WORLD_MAX, -HALF_WORLD_MAX, -HALF_WORLD_MAX), FVector(HALF_WORLD_MAX, HALF_WORLD_MAX, HALF_WORLD_MAX));

	return MyBounds;
}

void UVisualLoggerRenderingComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

#if WITH_EDITOR
	DebugDrawDelegateHelper.RegisterDebugDrawDelgate();
#endif
}

void UVisualLoggerRenderingComponent::DestroyRenderState_Concurrent()
{
#if WITH_EDITOR
	DebugDrawDelegateHelper.UnregisterDebugDrawDelgate();
#endif

	Super::DestroyRenderState_Concurrent();
}

AVisualLoggerRenderingActor::AVisualLoggerRenderingActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	RootComponent = SceneComponent;

	RenderingComponent = CreateDefaultSubobject<UVisualLoggerRenderingComponent>(TEXT("RenderingComponent"));

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		FVisualLoggerDatabase::Get().GetEvents().OnItemSelectionChanged.AddUObject(this, &AVisualLoggerRenderingActor::OnItemSelectionChanged);
		FVisualLoggerDatabase::Get().GetEvents().OnRowSelectionChanged.AddUObject(this, &AVisualLoggerRenderingActor::ObjectSelectionChanged);

		FLogVisualizer::Get().GetEvents().OnFiltersChanged.AddUObject(this, &AVisualLoggerRenderingActor::OnFiltersChanged);
	}
}

AVisualLoggerRenderingActor::~AVisualLoggerRenderingActor()
{
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		FVisualLoggerDatabase::Get().GetEvents().OnItemSelectionChanged.RemoveAll(this);
		FVisualLoggerDatabase::Get().GetEvents().OnRowSelectionChanged.RemoveAll(this);
		FLogVisualizer::Get().GetEvents().OnFiltersChanged.RemoveAll(this);
	}
}

void AVisualLoggerRenderingActor::ObjectSelectionChanged(const TArray<FName>& Selection)
{
	//PrimaryDebugShapes.Reset();

	if (Selection.Num() > 0)
	{
		for (auto CurrentName : Selection)
		{
			if (DebugShapesPerRow.Contains(CurrentName) == false)
			{
				DebugShapesPerRow.Add(CurrentName);
				FVisualLoggerDBRow &DBRow = FVisualLoggerDatabase::Get().GetRowByName(CurrentName);
				FTimelineDebugShapes& ShapesCache = DebugShapesPerRow[CurrentName];
				for (const auto &CurrentEntry : DBRow.GetItems())
				{
					if (CurrentEntry.Entry.Location != FVector::ZeroVector)
					{
						ShapesCache.LogEntriesPath.Add(CurrentEntry.Entry.Location);
					}
				}
			}
		}

		for (auto Element : DebugShapesPerRow)
		{
			if (Selection.Find(Element.Key) == INDEX_NONE)
			{
				DebugShapesPerRow.Remove(Element.Key);
			}
		}
	}
	else
	{
		DebugShapesPerRow.Reset();
	}
	CachedRowSelection = Selection;
	MarkComponentsRenderStateDirty();
}

void AVisualLoggerRenderingActor::OnItemSelectionChanged(const FVisualLoggerDBRow& DBRow, int32 ItemIndex)
{
	const FName RowName = DBRow.GetOwnerName();

	const TMap<FName, FVisualLogExtensionInterface*>& AllExtensions = FVisualLogger::Get().GetAllExtensions();
	for (auto& Extension : AllExtensions)
	{
		Extension.Value->DrawData(FVisualLoggerEditorInterface::Get(), NULL);
	}

	if (DebugShapesPerRow.Contains(RowName) == false)
	{
		return;
	}

	if (FVisualLoggerDatabase::Get().IsRowVisible(RowName) == false || DBRow.GetItems().IsValidIndex(ItemIndex) == false)
	{
		return;
	}

	FTimelineDebugShapes& ShapesCache = DebugShapesPerRow[RowName];
	ShapesCache.Reset();
	const TArray<FVisualLogDevice::FVisualLogEntryItem>& Entries = DBRow.GetItems();
	const int32 CurrentItemIndex = DBRow.GetCurrentItemIndex();
	GetDebugShapes(Entries[CurrentItemIndex], ShapesCache);

	MarkComponentsRenderStateDirty();
}

void AVisualLoggerRenderingActor::ResetRendering()
{
	CachedRowSelection.Reset();
	DebugShapesPerRow.Reset();
	MarkComponentsRenderStateDirty();
}

void AVisualLoggerRenderingActor::OnFiltersChanged()
{
	const TMap<FName, FVisualLogExtensionInterface*>& AllExtensions = FVisualLogger::Get().GetAllExtensions();
	for (auto& Extension : AllExtensions)
	{
		Extension.Value->DrawData(FVisualLoggerEditorInterface::Get(), NULL);
	}

	DebugShapesPerRow.Reset();
	const TArray<FName>& RowNames = FVisualLoggerDatabase::Get().GetSelectedRows();
	for (FName CurrentName : RowNames)
	{
		FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(CurrentName);
		FTimelineDebugShapes& ShapesCache = DebugShapesPerRow.FindOrAdd(CurrentName);
		ShapesCache.Reset();
		if (DBRow.GetCurrentItemIndex() != INDEX_NONE)
		{
			GetDebugShapes(DBRow.GetCurrentItem(), ShapesCache);
		}
	}
	MarkComponentsRenderStateDirty();
}

void AVisualLoggerRenderingActor::AddDebugRendering()
{
	const float Thickness = 2;

	{
		const FVector BoxExtent(100, 100, 100);
		const FBox Box(FVector(128), FVector(300));
		TestDebugShapes.Boxes.Add(FDebugRenderSceneProxy::FDebugBox(Box, FColor::Red));
		FTransform Trans;
		Trans.SetRotation(FQuat::MakeFromEuler(FVector(0.1, 0.2, 1.2)));
		TestDebugShapes.Boxes.Add(FDebugRenderSceneProxy::FDebugBox(Box, FColor::Red, Trans));
	}
	{
		const FVector Orgin = FVector(400,0,128);
		const FVector Direction = FVector(0,0,1);
		const float Length = 300;

		FVector YAxis, ZAxis;
		Direction.FindBestAxisVectors(YAxis, ZAxis);
		TestDebugShapes.Cones.Add(FDebugRenderSceneProxy::FCone(FScaleMatrix(FVector(Length)) * FMatrix(Direction, YAxis, ZAxis, Orgin), 30, 30, FColor::Blue));
	}
	{
		const FVector Start = FVector(700, 0, 128);
		const FVector End = FVector(700, 0, 128+300);
		const float Radius = 200;
		const float HalfHeight = 150;
		TestDebugShapes.Cylinders.Add(FDebugRenderSceneProxy::FWireCylinder(Start + FVector(0, 0, HalfHeight), Radius, HalfHeight, FColor::Magenta));
	}

	{
		const FVector Center = FVector(1000, 0, 128);
		const float HalfHeight = 150;
		const float Radius = 50;
		const FQuat Rotation = FQuat::Identity;

		const FMatrix Axes = FQuatRotationTranslationMatrix(Rotation, FVector::ZeroVector);
		const FVector XAxis = Axes.GetScaledAxis(EAxis::X);
		const FVector YAxis = Axes.GetScaledAxis(EAxis::Y);
		const FVector ZAxis = Axes.GetScaledAxis(EAxis::Z);

		TestDebugShapes.Capsules.Add(FDebugRenderSceneProxy::FCapsule(Center, Radius, XAxis, YAxis, ZAxis, HalfHeight, FColor::Yellow));
	}
	{
		const float Radius = 50;
		TestDebugShapes.Points.Add(FDebugRenderSceneProxy::FSphere(10, FVector(1300, 0, 128), FColor::White));
	}
}

namespace
{
	static bool IsPolygonWindingCorrect(const TArray<FVector>& Verts)
	{
		// this will work only for convex polys, but we're assuming that all logged polygons are convex in the first place
		if (Verts.Num() >= 3)
		{
			const FVector SurfaceNormal = FVector::CrossProduct(Verts[1] - Verts[0], Verts[2] - Verts[0]);
			const float TestDot = FVector::DotProduct(SurfaceNormal, FVector(0, 0, 1));
			return TestDot > 0;
		}

		return false;
	}

	static void GetPolygonMesh(const FVisualLogShapeElement* ElementToDraw, FDebugRenderSceneProxy::FMesh& TestMesh, const FVector& VertexOffset = FVector::ZeroVector)
	{
		TestMesh.Color = ElementToDraw->GetFColor();

		FClipSMPolygon InPoly(ElementToDraw->Points.Num());
		InPoly.FaceNormal = FVector(0, 0, 1);

		const bool bHasCorrectWinding = IsPolygonWindingCorrect(ElementToDraw->Points);
		if (bHasCorrectWinding)
		{
			for (int32 Index = 0; Index < ElementToDraw->Points.Num(); Index++)
			{
				FClipSMVertex v1;
				v1.Pos = ElementToDraw->Points[Index];
				InPoly.Vertices.Add(v1);
			}
		}
		else
		{
			for (int32 Index = ElementToDraw->Points.Num() - 1; Index >= 0; Index--)
			{
				FClipSMVertex v1;
				v1.Pos = ElementToDraw->Points[Index];
				InPoly.Vertices.Add(v1);
			}
		}

		TArray<FClipSMTriangle> OutTris;
		
		const bool bTriangulated = FGeomTools::TriangulatePoly(OutTris, InPoly, false);
		if (bTriangulated)
		{
			int32 LastIndex = 0;

			FGeomTools::RemoveRedundantTriangles(OutTris);
			for (const auto& CurrentTri : OutTris)
			{
				TestMesh.Vertices.Add(FDynamicMeshVertex(CurrentTri.Vertices[0].Pos + VertexOffset));
				TestMesh.Vertices.Add(FDynamicMeshVertex(CurrentTri.Vertices[1].Pos + VertexOffset));
				TestMesh.Vertices.Add(FDynamicMeshVertex(CurrentTri.Vertices[2].Pos + VertexOffset));
				TestMesh.Indices.Add(LastIndex++);
				TestMesh.Indices.Add(LastIndex++);
				TestMesh.Indices.Add(LastIndex++);
			}
		}
	}
}

void AVisualLoggerRenderingActor::GetDebugShapes(const FVisualLogDevice::FVisualLogEntryItem& EntryItem, FTimelineDebugShapes& DebugShapes)
{
	const FVisualLogEntry* Entry = &EntryItem.Entry;
	const FVisualLogShapeElement* ElementToDraw = Entry->ElementsToDraw.GetData();
	const int32 ElementsCount = Entry->ElementsToDraw.Num();

#if 0
	AddDebugRendering();
#endif

	{
		const float Length = 100;
		const FVector DirectionNorm = FVector(0, 0, 1).GetSafeNormal();
		FVector YAxis, ZAxis;
		DirectionNorm.FindBestAxisVectors(YAxis, ZAxis);
		DebugShapes.Cones.Add(FDebugRenderSceneProxy::FCone(FScaleMatrix(FVector(Length)) * FMatrix(DirectionNorm, YAxis, ZAxis, Entry->Location), 5, 5, FColor::Red));
	}

	if (DebugShapes.LogEntriesPath.Num())
	{
		FVector Location = DebugShapes.LogEntriesPath[0];
		for (int32 Index = 1; Index < DebugShapes.LogEntriesPath.Num(); ++Index)
		{
			const FVector CurrentLocation = DebugShapes.LogEntriesPath[Index];
			DebugShapes.Lines.Add(FDebugRenderSceneProxy::FDebugLine(Location, CurrentLocation, FColor(160, 160, 240), 2.0));
			Location = CurrentLocation;
		}
	}

	for (int32 ElementIndex = 0; ElementIndex < ElementsCount; ++ElementIndex, ++ElementToDraw)
	{
		if (!FVisualLoggerFilters::Get().MatchCategoryFilters(ElementToDraw->Category.ToString(), ElementToDraw->Verbosity))
		{
			continue;
		}


		const FVector CorridorOffset = NavigationDebugDrawing::PathOffset * 1.25f;
		const FColor Color = ElementToDraw->GetFColor();

		switch (ElementToDraw->GetType())
		{
		case EVisualLoggerShapeElement::SinglePoint:
		{
			const float Radius = float(ElementToDraw->Radius);
			const bool bDrawLabel = ElementToDraw->Description.IsEmpty() == false;
			if (ElementToDraw->Points.Num() == 1)
			{
				const FVector& Point = ElementToDraw->Points[0];
				DebugShapes.Points.Add(FDebugRenderSceneProxy::FSphere(Radius, Point, Color));
				if (bDrawLabel)
				{
					DebugShapes.Texts.Add(FDebugRenderSceneProxy::FText3d(ElementToDraw->Description, Point, Color));
				}
			}
			else
			{
				for (int32 Index = 0; Index < ElementToDraw->Points.Num(); ++Index)
				{
					const FVector& Point = ElementToDraw->Points[Index];
					DebugShapes.Points.Add(FDebugRenderSceneProxy::FSphere(Radius, Point, Color));
					if (bDrawLabel)
					{
						const FString PrintString = FString::Printf(TEXT("%s_%d"), *ElementToDraw->Description, Index);
						DebugShapes.Texts.Add(FDebugRenderSceneProxy::FText3d(PrintString, Point, Color));
					}
				}
			}
		}
			break;
		case EVisualLoggerShapeElement::Polygon:
		{
			FDebugRenderSceneProxy::FMesh TestMesh;
			GetPolygonMesh(ElementToDraw, TestMesh, CorridorOffset);
			DebugShapes.Meshes.Add(TestMesh);

			for (int32 VIdx = 0; VIdx < ElementToDraw->Points.Num(); VIdx++)
			{
				DebugShapes.Lines.Add(FDebugRenderSceneProxy::FDebugLine(
					ElementToDraw->Points[VIdx] + CorridorOffset,
					ElementToDraw->Points[(VIdx + 1) % ElementToDraw->Points.Num()] + CorridorOffset,
					FColor::Cyan,
					2)
					);
			}
		}
			break;
		case EVisualLoggerShapeElement::Mesh:
		{
			struct FHeaderData
			{
				float VerticesNum, FacesNum;
				FHeaderData(const FVector& InVector) : VerticesNum(InVector.X), FacesNum(InVector.Y) {}
			};
			const FHeaderData HeaderData(ElementToDraw->Points[0]);

			FDebugRenderSceneProxy::FMesh TestMesh;
			TestMesh.Color = ElementToDraw->GetFColor();
			int32 StartIndex = 1;
			int32 EndIndex = StartIndex + HeaderData.VerticesNum;
			for (int32 VIdx = StartIndex; VIdx < EndIndex; VIdx++)
			{
				TestMesh.Vertices.Add(ElementToDraw->Points[VIdx]);
			}


			StartIndex = EndIndex;
			EndIndex = StartIndex + HeaderData.FacesNum;
			for (int32 VIdx = StartIndex; VIdx < EndIndex; VIdx++)
			{
				const FVector &CurrentFace = ElementToDraw->Points[VIdx];
				TestMesh.Indices.Add(CurrentFace.X);
				TestMesh.Indices.Add(CurrentFace.Y);
				TestMesh.Indices.Add(CurrentFace.Z);
			}
			DebugShapes.Meshes.Add(TestMesh);
		}
			break;
		case EVisualLoggerShapeElement::Segment:
		{
			const float Thickness = float(ElementToDraw->Thicknes);
			const bool bDrawLabel = ElementToDraw->Description.IsEmpty() == false && ElementToDraw->Points.Num() > 2;
			const FVector* Location = ElementToDraw->Points.GetData();
			
			if (ElementToDraw->Points.Num() == 1)
			{
				DebugShapes.Lines.Add(FDebugRenderSceneProxy::FDebugLine(*Location, *(Location + 1), Color, Thickness));

				if (bDrawLabel)
				{
					DebugShapes.Texts.Add(FDebugRenderSceneProxy::FText3d(ElementToDraw->Description, (*Location + (*(Location + 1) - *Location) / 2), Color));
				}
			}
			else
			{
				for (int32 Index = 0; Index + 1 < ElementToDraw->Points.Num(); Index += 2, Location += 2)
				{
					DebugShapes.Lines.Add(FDebugRenderSceneProxy::FDebugLine(*Location, *(Location + 1), Color, Thickness));

					if (bDrawLabel)
					{
						const FString PrintString = FString::Printf(TEXT("%s_%d"), *ElementToDraw->Description, Index);
						DebugShapes.Texts.Add(FDebugRenderSceneProxy::FText3d(PrintString, (*Location + (*(Location + 1) - *Location) / 2), Color));
					}
				}
			}

			if (ElementToDraw->Description.IsEmpty() == false)
			{
				DebugShapes.Texts.Add(FDebugRenderSceneProxy::FText3d(ElementToDraw->Description, ElementToDraw->Points[0] + (ElementToDraw->Points[1] - ElementToDraw->Points[0]) / 2, Color));
			}
		}
			break;
		case EVisualLoggerShapeElement::Path:
		{
			const float Thickness = float(ElementToDraw->Thicknes);
			FVector Location = ElementToDraw->Points[0];
			for (int32 Index = 1; Index < ElementToDraw->Points.Num(); ++Index)
			{
				const FVector CurrentLocation = ElementToDraw->Points[Index];
				DebugShapes.Lines.Add(FDebugRenderSceneProxy::FDebugLine(Location, CurrentLocation, Color, Thickness));
				Location = CurrentLocation;
			}
		}
			break;
		case EVisualLoggerShapeElement::Box:
		{
			const float Thickness = float(ElementToDraw->Thicknes);
			const bool bDrawLabel = ElementToDraw->Description.IsEmpty() == false && ElementToDraw->Points.Num() > 2;
			const FVector* BoxExtent = ElementToDraw->Points.GetData();
			
			if (ElementToDraw->Points.Num() == 2)
			{
				const FBox Box = FBox(*BoxExtent, *(BoxExtent + 1));
				DebugShapes.Boxes.Add(FDebugRenderSceneProxy::FDebugBox(Box, Color, FTransform(ElementToDraw->TransformationMatrix)));

				if (bDrawLabel)
				{
					DebugShapes.Texts.Add(FDebugRenderSceneProxy::FText3d(ElementToDraw->Description, Box.GetCenter(), Color));
				}
			}
			else
			{
				for (int32 Index = 0; Index + 1 < ElementToDraw->Points.Num(); Index += 2, BoxExtent += 2)
				{
					const FBox Box = FBox(*BoxExtent, *(BoxExtent + 1));
					DebugShapes.Boxes.Add(FDebugRenderSceneProxy::FDebugBox(Box, Color, FTransform(ElementToDraw->TransformationMatrix)));

					if (bDrawLabel)
					{
						const FString PrintString = FString::Printf(TEXT("%s_%d"), *ElementToDraw->Description, Index);
						DebugShapes.Texts.Add(FDebugRenderSceneProxy::FText3d(PrintString, Box.GetCenter(), Color));
					}
				}
			}

			if (ElementToDraw->Description.IsEmpty() == false)
			{
				DebugShapes.Texts.Add(FDebugRenderSceneProxy::FText3d(ElementToDraw->Description, ElementToDraw->Points[0] + (ElementToDraw->Points[1] - ElementToDraw->Points[0]) / 2, Color));
			}
		}
			break;
		case EVisualLoggerShapeElement::Cone:
		{
			const float Thickness = float(ElementToDraw->Thicknes);
			const bool bDrawLabel = ElementToDraw->Description.IsEmpty() == false;
			for (int32 Index = 0; Index + 2 < ElementToDraw->Points.Num(); Index += 3)
			{
				const FVector Orgin = ElementToDraw->Points[Index];
				const FVector Direction = ElementToDraw->Points[Index + 1].GetSafeNormal();
				const FVector Angles = ElementToDraw->Points[Index + 2];
				const float Length = Angles.X;

				FVector YAxis, ZAxis;
				Direction.FindBestAxisVectors(YAxis, ZAxis);
				DebugShapes.Cones.Add(FDebugRenderSceneProxy::FCone(FScaleMatrix(FVector(Length)) * FMatrix(Direction, YAxis, ZAxis, Orgin), Angles.Y, Angles.Z, Color));

				if (bDrawLabel)
				{
					DebugShapes.Texts.Add(FDebugRenderSceneProxy::FText3d(ElementToDraw->Description, Orgin, Color));
				}
			}
		}
			break;
		case EVisualLoggerShapeElement::Cylinder:
		{
			const float Thickness = float(ElementToDraw->Thicknes);
			const bool bDrawLabel = ElementToDraw->Description.IsEmpty() == false;
			for (int32 Index = 0; Index + 2 < ElementToDraw->Points.Num(); Index += 3)
			{
				const FVector Start = ElementToDraw->Points[Index];
				const FVector End = ElementToDraw->Points[Index + 1];
				const FVector OtherData = ElementToDraw->Points[Index + 2];
				DebugShapes.Cylinders.Add(FDebugRenderSceneProxy::FWireCylinder(Start, OtherData.X, (End - Start).Size()*0.5, Color));
				if (bDrawLabel)
				{
					DebugShapes.Texts.Add(FDebugRenderSceneProxy::FText3d(ElementToDraw->Description, Start, Color));
				}
			}
		}
			break;
		case EVisualLoggerShapeElement::Capsule:
		{
			const float Thickness = float(ElementToDraw->Thicknes);
			const bool bDrawLabel = ElementToDraw->Description.IsEmpty() == false;
			for (int32 Index = 0; Index + 2 < ElementToDraw->Points.Num(); Index += 3)
			{
				const FVector Center = ElementToDraw->Points[Index + 0];
				const FVector FirstData = ElementToDraw->Points[Index + 1];
				const FVector SecondData = ElementToDraw->Points[Index + 2];
				const float HalfHeight = FirstData.X;
				const float Radius = FirstData.Y;
				const FQuat Rotation = FQuat(FirstData.Z, SecondData.X, SecondData.Y, SecondData.Z);

				const FMatrix Axes = FQuatRotationTranslationMatrix(Rotation, FVector::ZeroVector);
				const FVector XAxis = Axes.GetScaledAxis(EAxis::X);
				const FVector YAxis = Axes.GetScaledAxis(EAxis::Y);
				const FVector ZAxis = Axes.GetScaledAxis(EAxis::Z);

				DebugShapes.Capsules.Add(FDebugRenderSceneProxy::FCapsule(Center, Radius, XAxis, YAxis, ZAxis, HalfHeight, Color));
				if (bDrawLabel)
				{
					DebugShapes.Texts.Add(FDebugRenderSceneProxy::FText3d(ElementToDraw->Description, Center, Color));
				}
			}
		}
			break;
		case EVisualLoggerShapeElement::NavAreaMesh:
		{
			if (ElementToDraw->Points.Num() == 0)
				continue;

			struct FHeaderData
			{
				float MinZ, MaxZ;
				FHeaderData(const FVector& InVector) : MinZ(InVector.X), MaxZ(InVector.Y) {}
			};
			const FHeaderData HeaderData(ElementToDraw->Points[0]);

			TArray<FVector> AreaMeshPoints = ElementToDraw->Points;
			AreaMeshPoints.RemoveAt(0, 1, false);
			AreaMeshPoints.Add(ElementToDraw->Points[1]);
			TArray<FVector> Vertices;
			TNavStatArray<FVector> Faces;
			int32 CurrentIndex = 0;
			FDebugRenderSceneProxy::FMesh TestMesh;
			TestMesh.Color = ElementToDraw->GetFColor();

			for (int32 PointIndex = 0; PointIndex < AreaMeshPoints.Num() - 1; PointIndex++)
			{
				FVector Point = AreaMeshPoints[PointIndex];
				FVector NextPoint = AreaMeshPoints[PointIndex + 1];

				FVector P1(Point.X, Point.Y, HeaderData.MinZ);
				FVector P2(Point.X, Point.Y, HeaderData.MaxZ);
				FVector P3(NextPoint.X, NextPoint.Y, HeaderData.MinZ);
				FVector P4(NextPoint.X, NextPoint.Y, HeaderData.MaxZ);

				TestMesh.Vertices.Add(P1); TestMesh.Vertices.Add(P2); TestMesh.Vertices.Add(P3);
				TestMesh.Indices.Add(CurrentIndex + 0);
				TestMesh.Indices.Add(CurrentIndex + 1);
				TestMesh.Indices.Add(CurrentIndex + 2);
				CurrentIndex += 3;
				TestMesh.Vertices.Add(P3); TestMesh.Vertices.Add(P2); TestMesh.Vertices.Add(P4);
				TestMesh.Indices.Add(CurrentIndex + 0);
				TestMesh.Indices.Add(CurrentIndex + 1);
				TestMesh.Indices.Add(CurrentIndex + 2);
				CurrentIndex += 3;
			}
			DebugShapes.Meshes.Add(TestMesh);

			{
				FDebugRenderSceneProxy::FMesh PolygonMesh;
				FVisualLogShapeElement PolygonToDraw(EVisualLoggerShapeElement::Polygon);
				PolygonToDraw.SetColor(ElementToDraw->GetFColor());
				PolygonToDraw.Points.Reserve(AreaMeshPoints.Num());
				PolygonToDraw.Points = AreaMeshPoints;
				GetPolygonMesh(&PolygonToDraw, PolygonMesh, FVector(0, 0, HeaderData.MaxZ));
				DebugShapes.Meshes.Add(PolygonMesh);
			}

			for (int32 VIdx = 0; VIdx < AreaMeshPoints.Num(); VIdx++)
			{
				DebugShapes.Lines.Add(FDebugRenderSceneProxy::FDebugLine(
					AreaMeshPoints[VIdx] + FVector(0, 0, HeaderData.MaxZ),
					AreaMeshPoints[(VIdx + 1) % AreaMeshPoints.Num()] + FVector(0, 0, HeaderData.MaxZ),
					ElementToDraw->GetFColor(),
					2)
					);
			}

		}
			break;
		}
	}
}
