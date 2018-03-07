// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialBillboardComponent.h"
#include "EngineGlobals.h"
#include "RHI.h"
#include "RenderResource.h"
#include "VertexFactory.h"
#include "PackedNormal.h"
#include "LocalVertexFactory.h"
#include "PrimitiveViewRelevance.h"
#include "Materials/MaterialInterface.h"
#include "PrimitiveSceneProxy.h"
#include "Engine/CollisionProfile.h"
#include "Curves/CurveFloat.h"
#include "SceneManagement.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreaming.h"
#include "LevelUtils.h"
#include "PrimitiveSceneProxy.h"

/** A material sprite vertex. */
struct FMaterialSpriteVertex
{
	FVector Position;
	FPackedNormal TangentX;
	FPackedNormal TangentZ;
	FColor Color;
	FVector2D TexCoords;
};

/** A dummy vertex buffer used to give the FMaterialSpriteVertexFactory something to reference as a stream source. */
class FMaterialSpriteVertexBuffer : public FVertexBuffer
{
public:

	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		VertexBufferRHI = RHICreateVertexBuffer(sizeof(FMaterialSpriteVertex),BUF_Static,CreateInfo);
	}
};
static TGlobalResource<FMaterialSpriteVertexBuffer> GDummyMaterialSpriteVertexBuffer;

/** The vertex factory used to draw material sprites. */
class FMaterialSpriteVertexFactory : public FLocalVertexFactory
{
public:

	FMaterialSpriteVertexFactory()
	{
		FLocalVertexFactory::FDataType VertexData;

		VertexData.PositionComponent = FVertexStreamComponent(
			&GDummyMaterialSpriteVertexBuffer,
			STRUCT_OFFSET(FMaterialSpriteVertex,Position),
			sizeof(FMaterialSpriteVertex),
			VET_Float3
			);
		VertexData.TangentBasisComponents[0] = FVertexStreamComponent(
			&GDummyMaterialSpriteVertexBuffer,
			STRUCT_OFFSET(FMaterialSpriteVertex,TangentX),
			sizeof(FMaterialSpriteVertex),
			VET_PackedNormal
			);
		VertexData.TangentBasisComponents[1] = FVertexStreamComponent(
			&GDummyMaterialSpriteVertexBuffer,
			STRUCT_OFFSET(FMaterialSpriteVertex,TangentZ),
			sizeof(FMaterialSpriteVertex),
			VET_PackedNormal
			);
		VertexData.ColorComponent = FVertexStreamComponent(
			&GDummyMaterialSpriteVertexBuffer,
			STRUCT_OFFSET(FMaterialSpriteVertex,Color),
			sizeof(FMaterialSpriteVertex),
			VET_Color
			);
		VertexData.TextureCoordinates.Empty();
		VertexData.TextureCoordinates.Add(FVertexStreamComponent(
			&GDummyMaterialSpriteVertexBuffer,
			STRUCT_OFFSET(FMaterialSpriteVertex,TexCoords),
			sizeof(FMaterialSpriteVertex),
			VET_Float2
			));

		SetData(VertexData);
	}
};
static TGlobalResource<FMaterialSpriteVertexFactory> GMaterialSpriteVertexFactory;

class FMaterialSpriteVertexArray : public FOneFrameResource
{
public:
	TArray<FMaterialSpriteVertex, TInlineAllocator<4> > Vertices;
};

/** Represents a sprite to the scene manager. */
class FMaterialSpriteSceneProxy : public FPrimitiveSceneProxy
{
public:

	/** Initialization constructor. */
	FMaterialSpriteSceneProxy(const UMaterialBillboardComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
	, Elements(InComponent->Elements)
	, BaseColor(FColor::White)
	{
		AActor* Owner = InComponent->GetOwner();
		if (Owner)
		{
			// Level colorization
			ULevel* Level = Owner->GetLevel();
			ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( Level );
			if ( LevelStreaming )
			{
				// Selection takes priority over level coloration.
				LevelColor = LevelStreaming->LevelColor;
			}
		}

		for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
		{
			UMaterialInterface* Material = Elements[ElementIndex].Material;
			if (Material)
			{
				MaterialRelevance |= Material->GetRelevance(GetScene().GetFeatureLevel());
			}
		}

		FColor NewPropertyColor;
		GEngine->GetPropertyColorationColor( (UObject*)InComponent, NewPropertyColor );
		PropertyColor = NewPropertyColor;
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_MaterialSpriteSceneProxy_GetDynamicMeshElements );

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];

				const bool bIsWireframe = View->Family->EngineShowFlags.Wireframe;
				// Determine the position of the source
				const FVector SourcePosition = GetLocalToWorld().GetOrigin();
				const FVector CameraToSource = View->ViewMatrices.GetViewOrigin() - SourcePosition;
				const float DistanceToSource = CameraToSource.Size();

				const FVector CameraUp      = -View->ViewMatrices.GetInvViewProjectionMatrix().TransformVector(FVector(1.0f,0.0f,0.0f));
				const FVector CameraRight   = -View->ViewMatrices.GetInvViewProjectionMatrix().TransformVector(FVector(0.0f,1.0f,0.0f));
				const FVector CameraForward = -View->ViewMatrices.GetInvViewProjectionMatrix().TransformVector(FVector(0.0f,0.0f,1.0f));
				const FMatrix WorldToLocal	= GetLocalToWorld().InverseFast();
				const FVector LocalCameraUp = WorldToLocal.TransformVector(CameraUp);
				const FVector LocalCameraRight = WorldToLocal.TransformVector(CameraRight);
				const FVector LocalCameraForward = WorldToLocal.TransformVector(CameraForward);

				// Draw the elements ordered so the last is on top of the first.
				for(int32 ElementIndex = 0;ElementIndex < Elements.Num();++ElementIndex)
				{
					const FMaterialSpriteElement& Element = Elements[ElementIndex];

					if(Element.Material)
					{
						// Evaluate the size of the sprite.
						float SizeX = Element.BaseSizeX;
						float SizeY = Element.BaseSizeY;
						if(Element.DistanceToSizeCurve)
						{
							const float SizeFactor = Element.DistanceToSizeCurve->GetFloatValue(DistanceToSource);
							SizeX *= SizeFactor;
							SizeY *= SizeFactor;
						}
						
						// Convert the size into world-space.
						const float W = View->ViewMatrices.GetViewProjectionMatrix().TransformPosition(SourcePosition).W;
						const float AspectRatio = CameraRight.Size() / CameraUp.Size();
						const float WorldSizeX = Element.bSizeIsInScreenSpace ? (SizeX               * W) : (SizeX / CameraRight.Size());
						const float WorldSizeY = Element.bSizeIsInScreenSpace ? (SizeY * AspectRatio * W) : (SizeY / CameraUp.Size());
			
						// Evaluate the color/opacity of the sprite.
						FLinearColor Color = BaseColor;
						if(Element.DistanceToOpacityCurve)
						{
							Color.A *= Element.DistanceToOpacityCurve->GetFloatValue(DistanceToSource);
						}

						// Set up the sprite vertex attributes that are constant across the sprite.
						FMaterialSpriteVertexArray& VertexArray = Collector.AllocateOneFrameResource<FMaterialSpriteVertexArray>();
						VertexArray.Vertices.Empty(4);
						VertexArray.Vertices.AddUninitialized(4);

						for(uint32 VertexIndex = 0;VertexIndex < 4;++VertexIndex)
						{
							VertexArray.Vertices[VertexIndex].Color = Color.ToFColor(true);
							VertexArray.Vertices[VertexIndex].TangentX = FPackedNormal(LocalCameraRight.GetSafeNormal());
							VertexArray.Vertices[VertexIndex].TangentZ = FPackedNormal(-LocalCameraForward.GetSafeNormal());
						}

						// Set up the sprite vertex positions and texture coordinates.
						VertexArray.Vertices[0].Position  = -WorldSizeX * LocalCameraRight + +WorldSizeY * LocalCameraUp;
						VertexArray.Vertices[0].TexCoords = FVector2D(0,0);
						VertexArray.Vertices[1].Position  = +WorldSizeX * LocalCameraRight + +WorldSizeY * LocalCameraUp;
						VertexArray.Vertices[1].TexCoords = FVector2D(0,1);
						VertexArray.Vertices[2].Position  = -WorldSizeX * LocalCameraRight + -WorldSizeY * LocalCameraUp;
						VertexArray.Vertices[2].TexCoords = FVector2D(1,0);
						VertexArray.Vertices[3].Position  = +WorldSizeX * LocalCameraRight + -WorldSizeY * LocalCameraUp;
						VertexArray.Vertices[3].TexCoords = FVector2D(1,1);
			
						// Set up the FMeshElement.
						FMeshBatch& Mesh = Collector.AllocateMesh();
						Mesh.UseDynamicData      = true;
						Mesh.DynamicVertexData   = VertexArray.Vertices.GetData();
						Mesh.DynamicVertexStride = sizeof(FMaterialSpriteVertex);

						Mesh.VertexFactory           = &GMaterialSpriteVertexFactory;
						Mesh.MaterialRenderProxy     = Element.Material->GetRenderProxy((View->Family->EngineShowFlags.Selection) && IsSelected(),IsHovered());
						Mesh.LCI                     = NULL;
						Mesh.ReverseCulling          = IsLocalToWorldDeterminantNegative() ? true : false;
						Mesh.CastShadow              = false;
						Mesh.DepthPriorityGroup      = (ESceneDepthPriorityGroup)GetDepthPriorityGroup(View);
						Mesh.Type                    = PT_TriangleStrip;
						Mesh.bDisableBackfaceCulling = true;

						// Set up the FMeshBatchElement.
						FMeshBatchElement& BatchElement = Mesh.Elements[0];
						BatchElement.IndexBuffer            = NULL;
						BatchElement.DynamicIndexData       = NULL;
						BatchElement.DynamicIndexStride     = 0;
						BatchElement.FirstIndex             = 0;
						BatchElement.MinVertexIndex         = 0;
						BatchElement.MaxVertexIndex         = 3;
						BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
						BatchElement.NumPrimitives          = 2;

						Mesh.bCanApplyViewModeOverrides = true;
						Mesh.bUseWireframeSelectionColoring = IsSelected();

						Collector.AddMesh(ViewIndex, Mesh);
					}
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		bool bVisible = View->Family->EngineShowFlags.BillboardSprites;
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = IsShadowCast(View);
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}
	virtual bool CanBeOccluded() const override { return !MaterialRelevance.bDisableDepthTest; }
	virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }
	uint32 GetAllocatedSize() const { return FPrimitiveSceneProxy::GetAllocatedSize(); }

private:
	TArray<FMaterialSpriteElement> Elements;
	FMaterialRelevance MaterialRelevance;
	FColor BaseColor;
};

UMaterialBillboardComponent::UMaterialBillboardComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

FPrimitiveSceneProxy* UMaterialBillboardComponent::CreateSceneProxy()
{
	return new FMaterialSpriteSceneProxy(this);
}

FBoxSphereBounds UMaterialBillboardComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	float BoundsSize = 1.0f;
	for (int32 i = 0; i < Elements.Num(); ++i)
	{
		if (Elements[i].bSizeIsInScreenSpace)
		{
			// Workaround static bounds by disabling culling. Still allows override such as 'use parent bounds', etc.
			// Note: Bounds are dynamically calculated at draw time per view, so difficult to cull correctly. (UE-4725)
			BoundsSize = float(HALF_WORLD_MAX); 
			break;
		}
		else
		{
			BoundsSize = FMath::Max3(BoundsSize, Elements[i].BaseSizeX, Elements[i].BaseSizeY);
		}
	}
	BoundsSize *= LocalToWorld.GetMaximumAxisScale();

	return FBoxSphereBounds(LocalToWorld.GetLocation(),FVector(BoundsSize,BoundsSize,BoundsSize),FMath::Sqrt(3.0f * FMath::Square(BoundsSize)));
}

void UMaterialBillboardComponent::AddElement(
	class UMaterialInterface* Material,
	UCurveFloat* DistanceToOpacityCurve,
	bool bSizeIsInScreenSpace,
	float BaseSizeX,
	float BaseSizeY,
	UCurveFloat* DistanceToSizeCurve
	)
{
	FMaterialSpriteElement* Element = new(Elements) FMaterialSpriteElement;
	Element->Material = Material;
	Element->DistanceToOpacityCurve = DistanceToOpacityCurve;
	Element->bSizeIsInScreenSpace = bSizeIsInScreenSpace;
	Element->BaseSizeX = BaseSizeX;
	Element->BaseSizeY = BaseSizeY;
	Element->DistanceToSizeCurve = DistanceToSizeCurve;

	MarkRenderStateDirty();
}

void UMaterialBillboardComponent::SetElements(const TArray<FMaterialSpriteElement>& NewElements)
{
	// Replace existing array
	Elements = NewElements;

	// Indicate scene proxy needs to be updated
	MarkRenderStateDirty();
}


UMaterialInterface* UMaterialBillboardComponent::GetMaterial(int32 Index) const
{
	UMaterialInterface* ResultMI = nullptr;
	if (Elements.IsValidIndex(Index))
	{
		ResultMI = Elements[Index].Material;
	}
	return ResultMI;
}

void UMaterialBillboardComponent::SetMaterial(int32 ElementIndex, class UMaterialInterface* Material)
{
	if (Elements.IsValidIndex(ElementIndex))
	{
		Elements[ElementIndex].Material = Material;

		MarkRenderStateDirty();
	}
}

void UMaterialBillboardComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
	{
		OutMaterials.AddUnique(GetMaterial(ElementIndex));
	}
}