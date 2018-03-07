// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BrushComponent.cpp: Unreal brush component implementation
=============================================================================*/

#include "Components/BrushComponent.h"
#include "PrimitiveSceneProxy.h"
#include "EngineGlobals.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "VertexFactory.h"
#include "PackedNormal.h"
#include "LocalVertexFactory.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "Model.h"
#include "Engine/Brush.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "GameFramework/Volume.h"
#include "Engine/Polys.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreaming.h"
#include "LevelUtils.h"
#include "ActorEditorUtils.h"
#include "SceneManagement.h"
#include "PhysicsEngine/BodySetup.h"

DEFINE_LOG_CATEGORY_STATIC(LogBrushComponent, Log, All);

#if WITH_EDITORONLY_DATA
struct FModelWireVertex
{
	FVector Position;
	FPackedNormal TangentX;
	FPackedNormal TangentZ;
	FVector2D UV;
};

class FModelWireVertexBuffer : public FVertexBuffer
{
public:

	/** Initialization constructor. */
	FModelWireVertexBuffer(UModel* InModel):
		NumVertices(0)
	{
#if WITH_EDITOR
		Polys.Append(InModel->Polys->Element);
		for(int32 PolyIndex = 0;PolyIndex < InModel->Polys->Element.Num();PolyIndex++)
		{
			NumVertices += InModel->Polys->Element[PolyIndex].Vertices.Num();
		}
#endif
	}

	// FRenderResource interface.
	virtual void InitRHI() override
	{
		if(NumVertices)
		{
			FRHIResourceCreateInfo CreateInfo;
			VertexBufferRHI = RHICreateVertexBuffer(NumVertices * sizeof(FModelWireVertex),BUF_Static, CreateInfo);

			FModelWireVertex* DestVertex = (FModelWireVertex*)RHILockVertexBuffer(VertexBufferRHI,0,NumVertices * sizeof(FModelWireVertex),RLM_WriteOnly);
			for(int32 PolyIndex = 0;PolyIndex < Polys.Num();PolyIndex++)
			{
				FPoly& Poly = Polys[PolyIndex];
				for(int32 VertexIndex = 0;VertexIndex < Poly.Vertices.Num();VertexIndex++)
				{
					DestVertex->Position = Poly.Vertices[VertexIndex];
					DestVertex->TangentX = FVector(1,0,0);
					DestVertex->TangentZ = FVector(0,0,1);
					// TangentZ.w contains the sign of the tangent basis determinant. Assume +1
					DestVertex->TangentZ.Vector.W = 255;
					DestVertex->UV.X	 = 0.0f;
					DestVertex->UV.Y	 = 0.0f;
					DestVertex++;
				}
			}
			RHIUnlockVertexBuffer(VertexBufferRHI);
		}
	}

	// Accessors.
	uint32 GetNumVertices() const { return NumVertices; }

private:
	TArray<FPoly> Polys;
	uint32 NumVertices;
};

class FModelWireIndexBuffer : public FIndexBuffer
{
public:

	/** Initialization constructor. */
	FModelWireIndexBuffer(UModel* InModel):
		NumEdges(0)
	{
#if WITH_EDITOR
		Polys.Append(InModel->Polys->Element);
		for(int32 PolyIndex = 0;PolyIndex < InModel->Polys->Element.Num();PolyIndex++)
		{
			NumEdges += InModel->Polys->Element[PolyIndex].Vertices.Num();
		}
#endif
	}

	// FRenderResource interface.
	virtual void InitRHI() override
	{
		if(NumEdges)
		{
			FRHIResourceCreateInfo CreateInfo;
			IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16),NumEdges * 2 * sizeof(uint16),BUF_Static, CreateInfo);

			uint16* DestIndex = (uint16*)RHILockIndexBuffer(IndexBufferRHI,0,NumEdges * 2 * sizeof(uint16),RLM_WriteOnly);
			uint16 BaseIndex = 0;
			for(int32 PolyIndex = 0;PolyIndex < Polys.Num();PolyIndex++)
			{
				FPoly&	Poly = Polys[PolyIndex];
				for(int32 VertexIndex = 0;VertexIndex < Poly.Vertices.Num();VertexIndex++)
				{
					*DestIndex++ = BaseIndex + VertexIndex;
					*DestIndex++ = BaseIndex + ((VertexIndex + 1) % Poly.Vertices.Num());
				}
				BaseIndex += Poly.Vertices.Num();
			}
			RHIUnlockIndexBuffer(IndexBufferRHI);
		}
	}

	// Accessors.
	uint32 GetNumEdges() const { return NumEdges; }

private:
	TArray<FPoly> Polys;
	uint32 NumEdges;
};
#endif // WITH_EDITORONLY_DATA

class FBrushSceneProxy : public FPrimitiveSceneProxy
{
public:
	FBrushSceneProxy(UBrushComponent* Component, ABrush* Owner):
		FPrimitiveSceneProxy(Component),
#if WITH_EDITORONLY_DATA
		WireIndexBuffer(Component->Brush),
		WireVertexBuffer(Component->Brush),
#endif
		bVolume(false),
		bBuilder(false),
		bSolidWhenSelected(false),
		bInManipulation(false),
		BrushColor(GEngine->C_BrushWire),
		BodySetup(Component->BrushBodySetup),
		CollisionResponse(Component->GetCollisionResponseToChannels())
	{
		bWillEverBeLit = false;

		if(Owner)
		{
			// If the editor is in a state where drawing the brush wireframe isn't desired, bail out.
			if( !GEngine->ShouldDrawBrushWireframe( Owner ) )
			{
				return;
			}

			// Determine the type of brush this is.
			bVolume = Owner->IsVolumeBrush();
			bBuilder = FActorEditorUtils::IsABuilderBrush( Owner );
			BrushColor = Owner->GetWireColor();
			bSolidWhenSelected = Owner->bSolidWhenSelected;
			bInManipulation = Owner->bInManipulation;

			// Builder brushes should be unaffected by level coloration, so if this is a builder brush, use
			// the brush color as the level color.
			if ( bBuilder )
			{
				LevelColor = BrushColor;
			}
			else
			{
				// Try to find a color for level coloration.
				ULevel* Level = Owner->GetLevel();
				ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( Level );
				if ( LevelStreaming )
				{
					LevelColor = LevelStreaming->LevelColor;
				}
			}
		}

		bUseEditorDepthTest = !bInManipulation;

		// Get a color for property coloration.
		FColor NewPropertyColor;
		GEngine->GetPropertyColorationColor( (UObject*)Component, NewPropertyColor );
		PropertyColor = NewPropertyColor;

#if WITH_EDITORONLY_DATA
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitBrushVertexFactory,
			FLocalVertexFactory*,VertexFactory,&VertexFactory,
			FVertexBuffer*,WireVertexBuffer,&WireVertexBuffer,
			{
				FLocalVertexFactory::FDataType Data;
				Data.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(WireVertexBuffer,FModelWireVertex,Position,VET_Float3);
				Data.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(WireVertexBuffer,FModelWireVertex,TangentX,VET_PackedNormal);
				Data.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(WireVertexBuffer,FModelWireVertex,TangentZ,VET_PackedNormal);
				Data.TextureCoordinates.Add( STRUCTMEMBER_VERTEXSTREAMCOMPONENT(WireVertexBuffer,FModelWireVertex,UV,VET_Float2) );
				VertexFactory->SetData(Data);
			});
#endif
	}

	virtual ~FBrushSceneProxy()
	{
#if WITH_EDITORONLY_DATA
		VertexFactory.ReleaseResource();
		WireIndexBuffer.ReleaseResource();
		WireVertexBuffer.ReleaseResource();
#endif

	}

	bool IsCollisionView(const FEngineShowFlags& EngineShowFlags, bool & bDrawCollision) const
	{
		const bool bInCollisionView = EngineShowFlags.CollisionVisibility || EngineShowFlags.CollisionPawn;
		if (bInCollisionView && IsCollisionEnabled())
		{
			bDrawCollision = EngineShowFlags.CollisionPawn && (CollisionResponse.GetResponse(ECC_Pawn) != ECR_Ignore);
			bDrawCollision |= EngineShowFlags.CollisionVisibility && (CollisionResponse.GetResponse(ECC_Visibility) != ECR_Ignore);
		}
		else
		{
			bDrawCollision = false;
		}

		return bInCollisionView;
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_BrushSceneProxy_GetDynamicMeshElements );

		if( AllowDebugViewmodes() )
		{
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					const FSceneView* View = Views[ViewIndex];

					bool bDrawCollision = false;
					const bool bInCollisionView = IsCollisionView(ViewFamily.EngineShowFlags, bDrawCollision);

					// Draw solid if 'solid when selected' and selected, or we are in a 'collision view'
					const bool bDrawSolid = ((bSolidWhenSelected && IsSelected()) || (bInCollisionView && bDrawCollision));
					// Don't draw wireframe if in a collision view mode and not drawing solid
					const bool bDrawWireframe = !bInCollisionView;

					// Choose color to draw it
					FLinearColor DrawColor = BrushColor;
					// In a collision view mode
					if(bInCollisionView)
					{
						DrawColor = BrushColor;
					}
					else if(View->Family->EngineShowFlags.PropertyColoration)
					{
						DrawColor = PropertyColor;
					}
					else if(View->Family->EngineShowFlags.LevelColoration)
					{
						DrawColor = LevelColor;
					}


					// SOLID
					if(bDrawSolid)
					{
						if(BodySetup != NULL)
						{
							auto SolidMaterialInstance = new FColoredMaterialRenderProxy(
								GEngine->ShadedLevelColorationUnlitMaterial->GetRenderProxy(IsSelected(), IsHovered()),
								DrawColor
								);

							Collector.RegisterOneFrameMaterialProxy(SolidMaterialInstance);

							FTransform GeomTransform(GetLocalToWorld());
							BodySetup->AggGeom.GetAggGeom(GeomTransform, DrawColor.ToFColor(true), /*Material=*/SolidMaterialInstance, false, /*bSolid=*/ true, UseEditorDepthTest(), ViewIndex, Collector);
						}
					}
					// WIREFRAME
					else if(bDrawWireframe)
					{
						// If we have the editor data (Wire Buffers) use those for wireframe
#if WITH_EDITOR
						if(WireIndexBuffer.GetNumEdges() && WireVertexBuffer.GetNumVertices())
						{
							auto WireframeMaterial = new FColoredMaterialRenderProxy(
								GEngine->LevelColorationUnlitMaterial->GetRenderProxy(IsSelected(), IsHovered()),
								GetViewSelectionColor(DrawColor, *View, !(GIsEditor && (View->Family->EngineShowFlags.Selection)) || IsSelected(), IsHovered(), false, IsIndividuallySelected() )
								);

							Collector.RegisterOneFrameMaterialProxy(WireframeMaterial);

							FMeshBatch& Mesh = Collector.AllocateMesh();
							FMeshBatchElement& BatchElement = Mesh.Elements[0];
							BatchElement.IndexBuffer = &WireIndexBuffer;
							Mesh.VertexFactory = &VertexFactory;
							Mesh.MaterialRenderProxy = WireframeMaterial;
							BatchElement.PrimitiveUniformBufferResource = &GetUniformBuffer();
							BatchElement.FirstIndex = 0;
							BatchElement.NumPrimitives = WireIndexBuffer.GetNumEdges();
							BatchElement.MinVertexIndex = 0;
							BatchElement.MaxVertexIndex = WireVertexBuffer.GetNumVertices() - 1;
							Mesh.CastShadow = false;
							Mesh.Type = PT_LineList;
							Mesh.DepthPriorityGroup = IsSelected() ? SDPG_Foreground : SDPG_World;
							Collector.AddMesh(ViewIndex, Mesh);
						}
						else 
#endif
						if(BodySetup != NULL)
							// If not, use the body setup for wireframe
						{
							FTransform GeomTransform(GetLocalToWorld());
							BodySetup->AggGeom.GetAggGeom(GeomTransform, GetSelectionColor(DrawColor, IsSelected(), IsHovered()).ToFColor(true), /* Material=*/ NULL, false, /* bSolid=*/ false, UseEditorDepthTest(), ViewIndex, Collector);
						}

					}
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		bool bVisible = false;


		// We render volumes in collision view. In game, always, in editor, if the EngineShowFlags.Volumes option is on.
		if(bSolidWhenSelected && IsSelected())
		{
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = true;
			Result.bDynamicRelevance = true;
			return Result;
		}

		const bool bInCollisionView = (View->Family->EngineShowFlags.Collision || View->Family->EngineShowFlags.CollisionVisibility || View->Family->EngineShowFlags.CollisionPawn);

		if(IsShown(View))
		{
			bool bNeverShow = false;

			if( GIsEditor )
			{
				const bool bShowBuilderBrush = View->Family->EngineShowFlags.BuilderBrush != 0;

				// Only render builder brush and only if the show flags indicate that we should render builder brushes.
				if( bBuilder && (!bShowBuilderBrush) )
				{
					bNeverShow = true;
				}
			}

			if(bNeverShow == false)
			{
				const bool bBSPVisible = View->Family->EngineShowFlags.BSP;
				const bool bBrushesVisible = View->Family->EngineShowFlags.Brushes;

				if ( !bVolume ) // EngineShowFlags.Collision does not apply to volumes
				{
					if( (bBSPVisible && bBrushesVisible) )
					{
						bVisible = true;
					}
				}

				// See if we should be visible because we are in a 'collision view' and have collision enabled
				if (bInCollisionView && IsCollisionEnabled())
				{
					bVisible = true;
				}

				// Always show the build brush and any brushes that are selected in the editor.
				if( GIsEditor )
				{
					if( bBuilder || IsSelected() )
					{
						bVisible = true;
					}
				}

				if ( bVolume )
				{
					const bool bVolumesVisible = View->Family->EngineShowFlags.Volumes;
					if(!GIsEditor || View->bIsGameView || bVolumesVisible)
					{
						bVisible = true;
					}
				}		
			}
		}
		
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = bVisible;
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = IsShadowCast(View);
		if(bInManipulation)
		{
			Result.bEditorNoDepthTestPrimitiveRelevance = true;
		}

		// Don't render on top in 'collision view' modes
		if(!bInCollisionView && !View->bIsGameView)
		{
			Result.bEditorPrimitiveRelevance = true;
		}

		return Result;
	}

	virtual void CreateRenderThreadResources() override
	{
#if WITH_EDITORONLY_DATA
		VertexFactory.InitResource();
		WireIndexBuffer.InitResource();
		WireVertexBuffer.InitResource();
#endif

	}

	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }
	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:
#if WITH_EDITORONLY_DATA
	FLocalVertexFactory VertexFactory;
	FModelWireIndexBuffer WireIndexBuffer;
	FModelWireVertexBuffer WireVertexBuffer;
#endif

	uint32 bVolume : 1;
	uint32 bBuilder : 1;	
	uint32 bSolidWhenSelected : 1;
	uint32 bInManipulation : 1;

	FColor BrushColor;
	FLinearColor LevelColor;
	FColor PropertyColor;

	/** Collision Response of this component**/
	UBodySetup* BodySetup;
	FCollisionResponseContainer CollisionResponse;
};

UBrushComponent::UBrushComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHiddenInGame = true;
	AlwaysLoadOnClient = false;
	AlwaysLoadOnServer = false;
	bUseAsOccluder = true;
	bUseEditorCompositing = true;
	bCanEverAffectNavigation = true;
	PrePivot_DEPRECATED = FVector::ZeroVector;
}

FPrimitiveSceneProxy* UBrushComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	
	if (Brush != NULL)	
	{
		// Check to make sure that we want to draw this brushed based on editor settings.
		ABrush*	Owner = Cast<ABrush>(GetOwner());
		if(Owner)
		{
			// If the editor is in a state where drawing the brush wireframe isn't desired, bail out.
			if( GEngine->ShouldDrawBrushWireframe( Owner ) )
			{
				Proxy = new FBrushSceneProxy(this, Owner);
			}
		}
		else
		{
			Proxy = new FBrushSceneProxy(this, Owner);
		}
	}

	return Proxy;
}


FBoxSphereBounds UBrushComponent::CalcBounds(const FTransform& LocalToWorld) const
{
#if WITH_EDITOR
	if(Brush && Brush->Polys && Brush->Polys->Element.Num())
	{
		TArray<FVector> Points;
		for( int32 i=0; i<Brush->Polys->Element.Num(); i++ )
		{
			for( int32 j=0; j<Brush->Polys->Element[i].Vertices.Num(); j++ )
			{
				Points.Add(Brush->Polys->Element[i].Vertices[j]);
			}
		}
		return FBoxSphereBounds( Points.GetData(), Points.Num() ).TransformBy(LocalToWorld);
	}
	else 
#endif // WITH_EDITOR
	if ((BrushBodySetup != NULL) && (BrushBodySetup->AggGeom.GetElementCount() > 0))
	{
		FBoxSphereBounds NewBounds;
		BrushBodySetup->AggGeom.CalcBoxSphereBounds(NewBounds, LocalToWorld);
		return NewBounds;
	}
	else
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
	}
}

void UBrushComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials ) const
{
#if WITH_EDITOR
	// Get the material from each polygon making up the brush.
	if( Brush && Brush->Polys )
	{
		UPolys* Polys = Brush->Polys;
		for( int32 ElementIdx = 0; ElementIdx < Polys->Element.Num(); ++ElementIdx )
		{
			OutMaterials.Add( Polys->Element[ ElementIdx ].Material );
		}
	}
#endif // WITH_EDITOR
}

void UBrushComponent::PostLoad()
{
	Super::PostLoad();

	// Stop existing BrushComponents from generating mirrored collision mesh
	if ((GetLinkerUE4Version() < VER_UE4_NO_MIRROR_BRUSH_MODEL_COLLISION) && (BrushBodySetup != NULL))
	{
		BrushBodySetup->bGenerateMirroredCollision = false;
	}

#if WITH_EDITOR
	// If loading a brush with mirroring whose body setup has not been created correctly, request that it be rebuilt now.
	// The rebuilding will actually happen in the UBodySetup::PostLoad.
	RequestUpdateBrushCollision();

	AActor* Owner = GetOwner();

	if (Owner)
	{
		AddRelativeLocation(GetComponentTransform().TransformVector(-PrePivot_DEPRECATED));
		Owner->SetPivotOffset(PrePivot_DEPRECATED);
		PrePivot_DEPRECATED = FVector::ZeroVector;
	}
#endif
}

void UBrushComponent::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	// Count the bodysetup we own as well for 'inclusive' stats
	if((CumulativeResourceSize.GetResourceSizeMode() == EResourceSizeMode::Inclusive) && (BrushBodySetup != NULL))
	{
		BrushBodySetup->GetResourceSizeEx(CumulativeResourceSize);
	}
}

uint8 UBrushComponent::GetStaticDepthPriorityGroup() const
{
	ABrush* BrushOwner = Cast<ABrush>(GetOwner());

	// Draw selected and builder brushes in the foreground DPG.
	if(BrushOwner && (IsOwnerSelected() || FActorEditorUtils::IsABuilderBrush(BrushOwner)))
	{
		return SDPG_Foreground;
	}
	else
	{
		return DepthPriorityGroup;
	}
}

#if WITH_EDITOR
static bool IsComponentTypeShown(AActor* Actor, const FEngineShowFlags& ShowFlags)
{
	if (Actor != nullptr)
	{
		return (Actor->IsA(AVolume::StaticClass())) ? ShowFlags.Volumes : ShowFlags.BSP;
	}

	return false;
}

bool UBrushComponent::ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	if (!IsComponentTypeShown(GetOwner(), ShowFlags))
	{
		return false;
	}

	if (Brush != nullptr && Brush->Polys != nullptr)
	{
		TArray<FVector> Vertices;

		if (!bMustEncompassEntireComponent)
		{
			const bool bMustContainVertex = true;
			if (bMustContainVertex)
			{
				// Test if any poly vertex intersects the box
				for (const auto& Poly : Brush->Polys->Element)
				{
					for (const auto& Vertex : Poly.Vertices)
					{
						const FVector Location = GetComponentTransform().TransformPosition(Vertex);
						const bool bLocationIntersected = FMath::PointBoxIntersection(Location, InSelBBox);

						// If the selection box doesn't have to encompass the entire component and any poly vertex intersects with the selection
						// box, this component qualifies
						if (bLocationIntersected)
						{
							return true;
						}
					}
				}
			}
			else
			{
				// Alternative method, which can be enabled by setting bMustContainVertex = false:
				// Test if any poly edge intersects the box
				for (const auto& Poly : Brush->Polys->Element)
				{
					const int32 NumVerts = Poly.Vertices.Num();
					if (NumVerts > 0)
					{
						FVector StartVert = GetComponentTransform().TransformPosition(Poly.Vertices[NumVerts - 1]);
						for (int32 Index = 0; Index < NumVerts; ++Index)
						{
							const FVector EndVert = GetComponentTransform().TransformPosition(Poly.Vertices[Index]);

							if (FMath::LineBoxIntersection(InSelBBox, StartVert, EndVert, EndVert - StartVert))
							{
								return true;
							}

							StartVert = EndVert;
						}
					}
				}
			}

			// No poly intersected with the bounding box
			return false;
		}
		else
		{
			for (const auto& Poly : Brush->Polys->Element)
			{
				// The component must be entirely within the bounding box...
				for (const auto& Vertex : Poly.Vertices)
				{
					const FVector Location = GetComponentTransform().TransformPosition(Vertex);
					const bool bLocationIntersected = FMath::PointBoxIntersection(Location, InSelBBox);

					// If the selection box has to encompass the entire component and a poly vertex didn't intersect with the selection
					// box, this component does not qualify
					if (!bLocationIntersected)
					{
						return false;
					}
				}
			}

			// All points lay within the selection box
			return true;
		}
	}

	return false;
}


bool UBrushComponent::ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	if (!IsComponentTypeShown(GetOwner(), ShowFlags))
	{
		return false;
	}

	if (Brush != nullptr && Brush->Polys != nullptr)
	{
		TArray<FVector> Vertices;

		for (const auto& Poly : Brush->Polys->Element)
		{
			for (const auto& Vertex : Poly.Vertices)
			{
				const FVector Location = GetComponentTransform().TransformPosition(Vertex);
				const bool bIntersect = InFrustum.IntersectSphere(Location, 0.0f);

				if (bIntersect && !bMustEncompassEntireComponent)
				{
					// If we intersected a vertex and we don't require the box to encompass the entire component
					// then the actor should be selected and we can stop checking
					return true;
				}
				else if (!bIntersect && bMustEncompassEntireComponent)
				{
					// If we didn't intersect a vertex but we require the box to encompass the entire component
					// then this test failed and we can stop checking
					return false;
				}
			}
		}

		// If the selection box has to encompass all of the component and none of the component's verts failed the intersection test, this component
		// is considered touching
		return true;
	}

	return false;
}

void UBrushComponent::RequestUpdateBrushCollision()
{
	if (BrushBodySetup)
	{
		const bool bIsMirrored = (RelativeScale3D.X * RelativeScale3D.Y * RelativeScale3D.Z) < 0.0f;
		if ((BrushBodySetup->bGenerateNonMirroredCollision && bIsMirrored) || (BrushBodySetup->bGenerateMirroredCollision && !bIsMirrored))
		{
			// Brushes only maintain one convex mesh as they can't be transformed at runtime.
			// Here we invalidate the body setup, and specify whether we wish to build a non-mirrored or a mirrored mesh.
			BrushBodySetup->bGenerateNonMirroredCollision = !bIsMirrored;
			BrushBodySetup->bGenerateMirroredCollision = bIsMirrored;
			BrushBodySetup->InvalidatePhysicsData();
		}
	}
}
#endif

void UBrushComponent::BuildSimpleBrushCollision()
{
	AActor* Owner = GetOwner();
	if(!Owner)
	{
		UE_LOG(LogBrushComponent, Warning, TEXT("BuildSimpleBrushCollision: BrushComponent with no Owner!") );
		return;
	}

	if(BrushBodySetup == NULL)
	{
		BrushBodySetup = NewObject<UBodySetup>(this);
		check(BrushBodySetup);
	}

	// No complex collision, so use the simple for that
	BrushBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;

#if WITH_EDITOR
	RequestUpdateBrushCollision();

	// Convert collision model into convex hulls.
	BrushBodySetup->CreateFromModel( Brush, true );

	RecreatePhysicsState();
#endif // WITH_EDITOR

	MarkPackageDirty();
}

bool UBrushComponent::IsEditorOnly() const
{
	// Default to actor component behavior instead of primitive component behavior as brush actors handle it themselves
	return bIsEditorOnly;
}

#if WITH_EDITOR
static FVector GetPolyCenter(const FPoly& Poly)
{
	FVector Result = FVector::ZeroVector;
	for (const auto& Vertex : Poly.Vertices)
	{
		Result += Vertex;
	}

	return Result / Poly.Vertices.Num();
}

bool UBrushComponent::HasInvertedPolys() const
{
	// Determine if a brush looks as if it has had its sense inverted
	// (due to the old behavior of inverting the poly winding and normal when performing a Mirror operation).

	const bool bIsMirrored = (RelativeScale3D.X * RelativeScale3D.Y * RelativeScale3D.Z < 0.0f);
	// Only attempt to fix up brushes with negative scale
	if (bIsMirrored)
	{
		int NumInwardFacingPolys = 0;
		for (auto& Poly : Brush->Polys->Element)
		{
			// Calculate a nominal center point for the poly
			const FVector PolyCenter = GetPolyCenter(Poly);
			bool bIntersected = false;

			// Find intersections of a ray cast out from the center in the normal direction with the other polys
			for (auto& OtherPoly : Brush->Polys->Element)
			{
				if (&Poly != &OtherPoly)
				{
					// Calculate a nominal center point for the poly being tested for intersection
					const FVector OtherPolyCenter = GetPolyCenter(OtherPoly);
					const float Dot = FVector::DotProduct(Poly.Normal, OtherPoly.Normal);
					// If normals are perpendicular, skip it - this implies that the poly normal is parallel to the plane
					if (Dot != 0.0f)
					{
						const float Distance = FVector::DotProduct(OtherPolyCenter - PolyCenter, OtherPoly.Normal) / Dot;
						// Only consider intersections in the direction of the poly normal
						if (Distance > 0.0f)
						{
							const FVector Intersection = PolyCenter + Poly.Normal * Distance;

							// Does the ray intersect with the actual poly?
							if (OtherPoly.OnPoly(Intersection))
							{
								// If so, toggle the intersected flag.
								// An odd number of intersections implies an inwards facing poly.
								// An even number of intersections implies an outwards facing poly.
								bIntersected = !bIntersected;
							}
						}
					}
				}
			}

			if (bIntersected)
			{
				NumInwardFacingPolys++;
			}
		}

		// If more than half of the polys are deemed to be inwards facing, consider this to be an inside out brush
		if (NumInwardFacingPolys > Brush->Polys->Element.Num() / 2)
		{
			return true;
		}
	}

	return false;
}
#endif
