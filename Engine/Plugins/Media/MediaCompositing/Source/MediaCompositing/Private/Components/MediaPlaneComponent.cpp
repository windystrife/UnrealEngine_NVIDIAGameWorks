// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaPlaneComponent.h"

#include "EngineGlobals.h"
#include "RHI.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "VertexFactory.h"
#include "PackedNormal.h"
#include "LocalVertexFactory.h"
#include "PrimitiveViewRelevance.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PrimitiveSceneProxy.h"
#include "Engine/CollisionProfile.h"
#include "Curves/CurveFloat.h"
#include "SceneManagement.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreaming.h"
#include "LevelUtils.h"
#include "ShowFlags.h"
#include "Camera/CameraTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/Texture2DDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "DynamicMeshBuilder.h"
#include "Components/SceneCaptureComponent2D.h"
#include "MediaPlaneFrustumComponent.h"
#include "MediaPlaneComponent.h"

namespace
{
	FMatrix CalculateProjectionMatrix(const FMinimalViewInfo& MinimalView)
	{
		FMatrix ProjectionMatrix;

		if (MinimalView.ProjectionMode == ECameraProjectionMode::Orthographic)
		{
			const float YScale = 1.0f / MinimalView.AspectRatio;

			const float HalfOrthoWidth = MinimalView.OrthoWidth / 2.0f;
			const float ScaledOrthoHeight = MinimalView.OrthoWidth / 2.0f * YScale;

			const float NearPlane = MinimalView.OrthoNearClipPlane;
			const float FarPlane = MinimalView.OrthoFarClipPlane;

			const float ZScale = 1.0f / (FarPlane - NearPlane);
			const float ZOffset = -NearPlane;

			ProjectionMatrix = FReversedZOrthoMatrix(
				HalfOrthoWidth,
				ScaledOrthoHeight,
				ZScale,
				ZOffset
				);
		}
		else
		{
			// Avoid divide by zero in the projection matrix calculation by clamping FOV
			ProjectionMatrix = FReversedZPerspectiveMatrix(
				FMath::Max(0.001f, MinimalView.FOV) * (float)PI / 360.0f,
				MinimalView.AspectRatio,
				1.0f,
				GNearClippingPlane );
		}

		if (!MinimalView.OffCenterProjectionOffset.IsZero())
		{
			const float Left = -1.0f + MinimalView.OffCenterProjectionOffset.X;
			const float Right = Left + 2.0f;
			const float Bottom = -1.0f + MinimalView.OffCenterProjectionOffset.Y;
			const float Top = Bottom + 2.0f;
			ProjectionMatrix.M[2][0] = (Left + Right) / (Left - Right);
			ProjectionMatrix.M[2][1] = (Bottom + Top) / (Bottom - Top);
		}

		return ProjectionMatrix;
	}

	class FMediaPlaneVertexBuffer : public FVertexBuffer
	{
	public:

		TArray<FDynamicMeshVertex> Vertices;

		virtual void InitRHI() override
		{
			FRHIResourceCreateInfo CreateInfo;
			void* VertexBufferData = nullptr;
			VertexBufferRHI = RHICreateAndLockVertexBuffer(Vertices.Num() * sizeof(FDynamicMeshVertex),BUF_Static,CreateInfo, VertexBufferData);

			// Copy the vertex data into the vertex buffer.
			FMemory::Memcpy(VertexBufferData,Vertices.GetData(),Vertices.Num() * sizeof(FDynamicMeshVertex));
			RHIUnlockVertexBuffer(VertexBufferRHI);
		}
	};

	struct FMediaPlaneVertexFactory : public FLocalVertexFactory
	{
		/** Initialization */
		void Init(const FMediaPlaneVertexBuffer* VertexBuffer)
		{
			check(IsInRenderingThread())

			FDataType NewData;
			NewData.PositionComponent = 			STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Position,VET_Float3);
			NewData.TangentBasisComponents[0] = 	STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentX,VET_PackedNormal);
			NewData.TangentBasisComponents[1] = 	STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentZ,VET_PackedNormal);
			NewData.ColorComponent = 				STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Color,VET_Color);

			NewData.TextureCoordinates.Add(
				FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FDynamicMeshVertex,TextureCoordinate),sizeof(FDynamicMeshVertex),VET_Float2)
				);

			SetData(NewData);
		}
	};

	class FMediaPlaneIndexBuffer : public FIndexBuffer
	{
	public:
		TArray<uint16> Indices;

		virtual void InitRHI() override
		{
			FRHIResourceCreateInfo CreateInfo;
			void* Buffer = nullptr;
			IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(uint16), Indices.Num() * sizeof(uint16), BUF_Static, CreateInfo, Buffer);

			// Copy the index data into the index buffer.		
			FMemory::Memcpy(Buffer, Indices.GetData(), Indices.Num() * sizeof(uint16));
			RHIUnlockIndexBuffer(IndexBufferRHI);
		}
	};

	/** Represents a sprite to the scene manager. */
	class FMediaPlaneSceneProxy : public FPrimitiveSceneProxy
	{
	public:

		/** Initialization constructor. */
		FMediaPlaneSceneProxy(UMediaPlaneComponent* InComponent)
			: FPrimitiveSceneProxy(InComponent)
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

			Material = InComponent->GetPlane().DynamicMaterial ? InComponent->GetPlane().DynamicMaterial : InComponent->GetPlane().Material;
			if (Material)
			{
				MaterialRelevance |= Material->GetRelevance(GetScene().GetFeatureLevel());
			}

			FColor NewPropertyColor;
			GEngine->GetPropertyColorationColor(InComponent, NewPropertyColor);
			PropertyColor = NewPropertyColor;
		}

		~FMediaPlaneSceneProxy()
		{
			VertexBuffer.ReleaseResource();
			IndexBuffer.ReleaseResource();
			VertexFactory.ReleaseResource();
		}

		virtual void CreateRenderThreadResources() override
		{
			BuildMesh();

			VertexFactory.Init(&VertexBuffer);

			VertexBuffer.InitResource();
			IndexBuffer.InitResource();
			VertexFactory.InitResource();
		}

		void BuildMesh()
		{
			VertexBuffer.Vertices.Empty(4);
			VertexBuffer.Vertices.AddUninitialized(4);

			// Set up the sprite vertex positions and texture coordinates.
			VertexBuffer.Vertices[0].Position  = FVector(0, -1.f,  1.f);
			VertexBuffer.Vertices[1].Position  = FVector(0, -1.f, -1.f);
			VertexBuffer.Vertices[2].Position  = FVector(0,  1.f,  1.f);
			VertexBuffer.Vertices[3].Position  = FVector(0,  1.f, -1.f);

			VertexBuffer.Vertices[0].TextureCoordinate = FVector2D(0,0);
			VertexBuffer.Vertices[1].TextureCoordinate = FVector2D(0,1);
			VertexBuffer.Vertices[2].TextureCoordinate = FVector2D(1,0);
			VertexBuffer.Vertices[3].TextureCoordinate = FVector2D(1,1);

			IndexBuffer.Indices.Empty(6);
			IndexBuffer.Indices.AddUninitialized(6);

			IndexBuffer.Indices[0] = 0;
			IndexBuffer.Indices[1] = 1;
			IndexBuffer.Indices[2] = 2;
			IndexBuffer.Indices[3] = 1;
			IndexBuffer.Indices[4] = 3;
			IndexBuffer.Indices[5] = 2;
		}

		virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override
		{
			QUICK_SCOPE_CYCLE_COUNTER( STAT_MediaPlaneSceneProxy_DrawStaticElements );

			if (Material)
			{
				FMeshBatch Mesh;
				Mesh.VertexFactory           = &VertexFactory;
				Mesh.MaterialRenderProxy     = Material->GetRenderProxy(false);
				Mesh.ReverseCulling          = IsLocalToWorldDeterminantNegative();
				Mesh.CastShadow              = false;
				Mesh.DepthPriorityGroup      = SDPG_World;
				Mesh.Type                    = PT_TriangleList;
				Mesh.bDisableBackfaceCulling = true;

				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.IndexBuffer = &IndexBuffer;
				BatchElement.FirstIndex             = 0;
				BatchElement.MinVertexIndex         = 0;
				BatchElement.MaxVertexIndex         = 3;
				BatchElement.NumPrimitives          = 2;
				BatchElement.PrimitiveUniformBufferResource = &GetUniformBuffer();

				PDI->DrawMesh(Mesh, 1.f);
			}
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
		{
			QUICK_SCOPE_CYCLE_COUNTER( STAT_MediaPlaneSceneProxy_GetDynamicMeshElements );

			if (!Material)
			{
				return;
			}

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (!(VisibilityMap & (1 << ViewIndex)))
				{
					continue;
				}

				const FSceneView* View = Views[ViewIndex];

				// Draw the mesh.
				FMeshBatch& Mesh = Collector.AllocateMesh();
				Mesh.VertexFactory = &VertexFactory;
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.CastShadow = false;
				Mesh.bDisableBackfaceCulling = false;
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = (ESceneDepthPriorityGroup)GetDepthPriorityGroup(View);
				Mesh.bCanApplyViewModeOverrides = true;
				Mesh.MaterialRenderProxy = Material->GetRenderProxy(View->Family->EngineShowFlags.Selection && IsSelected(), IsHovered());

				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.IndexBuffer = &IndexBuffer;
				BatchElement.FirstIndex			= 0;
				BatchElement.MinVertexIndex		= 0;
				BatchElement.MaxVertexIndex		= 3;
				BatchElement.NumPrimitives		= 2;
				BatchElement.PrimitiveUniformBufferResource = &GetUniformBuffer();

				Collector.AddMesh(ViewIndex, Mesh);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				RenderBounds(Collector.GetPDI(ViewIndex), View->Family->EngineShowFlags, GetBounds(), IsSelected());
#endif
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View) && View->Family->EngineShowFlags.MediaPlanes;
			Result.bRenderCustomDepth = ShouldRenderCustomDepth();
			Result.bRenderInMainPass = ShouldRenderInMainPass();
			Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();

			Result.bShadowRelevance = IsShadowCast(View);
			if( IsRichView(*View->Family) ||
				View->Family->EngineShowFlags.Bounds ||
				View->Family->EngineShowFlags.Collision ||
				IsSelected() ||
				IsHovered())
			{
				Result.bDynamicRelevance = true;
			}
			else
			{
				Result.bStaticRelevance = true;
			}
			
			MaterialRelevance.SetPrimitiveViewRelevance(Result);
			return Result;
		}
		virtual bool CanBeOccluded() const override { return !MaterialRelevance.bDisableDepthTest; }
		virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }
		uint32 GetAllocatedSize() const { return FPrimitiveSceneProxy::GetAllocatedSize(); }

	private:
		UMaterialInterface* Material;
		FMaterialRelevance MaterialRelevance;
		FMediaPlaneVertexBuffer VertexBuffer;
		FMediaPlaneIndexBuffer IndexBuffer;
		FMediaPlaneVertexFactory VertexFactory;
	};
}

FMediaPlaneParameters::FMediaPlaneParameters()
	: Material(LoadObject<UMaterialInterface>(nullptr, TEXT("/MediaCompositing/DefaultMediaPlaneMaterial.DefaultMediaPlaneMaterial")))
	, TextureParameterName("InputTexture")
	, bFillScreen(true)
	, FillScreenAmount(100, 100)
	, FixedSize(100,100)
	, RenderTexture(nullptr)
	, DynamicMaterial(nullptr)
{
}

UMediaPlaneComponent::UMediaPlaneComponent(const FObjectInitializer& Init)
	: Super(Init)
{
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	bUseAsOccluder = false;
	bTickInEditor = true;
	PrimaryComponentTick.bCanEverTick = true;
	bReenetrantTranformChange = false;
}

void UMediaPlaneComponent::OnRegister()
{
	Super::OnRegister();
	UpdateMaterialParametersForMedia();
	UpdateTransformScale();

#if WITH_EDITORONLY_DATA
	if (AActor* ComponentOwner = GetOwner())
	{
		if (!EditorFrustum)
		{
			EditorFrustum = NewObject<UMediaPlaneFrustumComponent>(ComponentOwner, NAME_None, RF_Transactional | RF_TextExportTransient);
			EditorFrustum->SetupAttachment(this);
			/*EditorFrustum->bIsEditorOnly = true;*/
			EditorFrustum->CreationMethod = CreationMethod;
			EditorFrustum->RegisterComponentWithWorld(GetWorld());
		}
	}
#endif
}

void UMediaPlaneComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateTransformScale();
}

void UMediaPlaneComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

	if (!bReenetrantTranformChange)
	{
		bReenetrantTranformChange = true;
		UpdateTransformScale();
		bReenetrantTranformChange = false;
	}
}

void UMediaPlaneComponent::SetMediaPlane(FMediaPlaneParameters NewPlane)
{
	Plane = NewPlane;

	UpdateMaterialParametersForMedia();
}

void UMediaPlaneComponent::OnRenderTextureChanged()
{
	UpdateMaterialParametersForMedia();
}

void UMediaPlaneComponent::UpdateTransformScale()
{
	// Cache the view projection matrices of our target
	AActor* ViewTarget = FindViewTarget();
	if (ViewTarget && Plane.bFillScreen)
	{
		GetProjectionMatricesFromViewTarget(ViewTarget, ViewProjectionMatrix, InvViewProjectionMatrix);

		const FMatrix LocalToWorld = GetComponentTransform().ToMatrixNoScale();
		const FMatrix WorldToLocal = LocalToWorld.Inverse();
		const FMatrix ScreenToLocalSpace = InvViewProjectionMatrix * WorldToLocal;


		// Just use the current view projection matrix
		FVector4 HGLocalPosition = (LocalToWorld * ViewProjectionMatrix).TransformPosition(FVector::ZeroVector);
		FVector ScreenSpaceLocalPosition = HGLocalPosition;
		if (HGLocalPosition.W != 0.0f)
		{
			ScreenSpaceLocalPosition /= HGLocalPosition.W;
		}

		FVector HorizontalScale		= UMediaPlaneComponent::TransfromFromProjection(ScreenToLocalSpace, FVector4(Plane.FillScreenAmount.X/100.f, 0.f, ScreenSpaceLocalPosition.Z, 1.0f));
		FVector VerticalScale		= UMediaPlaneComponent::TransfromFromProjection(ScreenToLocalSpace, FVector4(0.f, Plane.FillScreenAmount.Y/100.f, ScreenSpaceLocalPosition.Z, 1.0f));

		SetRelativeScale3D(FVector(RelativeScale3D.X, HorizontalScale.Size(), VerticalScale.Size()));
		SetRelativeLocation(FVector(RelativeLocation.X, 0.f, 0.f));
	}
	else
	{
		SetRelativeScale3D(FVector(RelativeScale3D.X, Plane.FixedSize.X*.5f, Plane.FixedSize.Y*.5f));
	}
}

void UMediaPlaneComponent::UpdateMaterialParametersForMedia()
{
	if (!Plane.TextureParameterName.IsNone() && Plane.Material && Plane.RenderTexture)
	{
		if (!Plane.DynamicMaterial)
		{
			Plane.DynamicMaterial = UMaterialInstanceDynamic::Create(Plane.Material, this);
			Plane.DynamicMaterial->SetFlags(RF_Transient);
		}

		Plane.DynamicMaterial->SetTextureParameterValue(Plane.TextureParameterName, Plane.RenderTexture);
	}
	else
	{
		Plane.DynamicMaterial = nullptr;
	}

	MarkRenderStateDirty();

#if WITH_EDITORONLY_DATA
	if (EditorFrustum)
	{
		EditorFrustum->MarkRenderStateDirty();
	}
#endif
}

FPrimitiveSceneProxy* UMediaPlaneComponent::CreateSceneProxy()
{
	return new FMediaPlaneSceneProxy(this);
}

UMaterialInterface* UMediaPlaneComponent::GetMaterial(int32 Index) const
{
	if (Index == 0)
	{
		return Plane.Material;
	}
	return nullptr;
}

void UMediaPlaneComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* NewMaterial)
{
	if (ElementIndex == 0)
	{
		Plane.Material = NewMaterial;
		UpdateMaterialParametersForMedia();
	}
}

void UMediaPlaneComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	OutMaterials.AddUnique(Plane.DynamicMaterial ? Plane.DynamicMaterial : Plane.Material);
}

void UMediaPlaneComponent::GetProjectionMatricesFromViewTarget(AActor* InViewTarget, FMatrix& OutViewProjectionMatrix, FMatrix& OutInvViewProjectionMatrix)
{
	check(InViewTarget);

	FMinimalViewInfo MinimalViewInfo;
	
	ASceneCapture2D* SceneCapture = Cast<ASceneCapture2D>(InViewTarget);
	USceneCaptureComponent2D* SceneCaptureComponent = SceneCapture ? SceneCapture->GetCaptureComponent2D() : nullptr;
	if (SceneCaptureComponent)
	{
		MinimalViewInfo.Location = SceneCaptureComponent->GetComponentLocation();
		MinimalViewInfo.Rotation = SceneCaptureComponent->GetComponentRotation();

		MinimalViewInfo.FOV = SceneCaptureComponent->FOVAngle;
		MinimalViewInfo.AspectRatio = SceneCaptureComponent->TextureTarget ? float(SceneCaptureComponent->TextureTarget->SizeX) / SceneCaptureComponent->TextureTarget->SizeY : 1.f;
		MinimalViewInfo.bConstrainAspectRatio = false;
		MinimalViewInfo.ProjectionMode = SceneCaptureComponent->ProjectionType;
		MinimalViewInfo.OrthoWidth = SceneCaptureComponent->OrthoWidth;
	}
	else
	{
		InViewTarget->CalcCamera(0.f, MinimalViewInfo);
	}

	FMatrix ViewRotationMatrix = FInverseRotationMatrix(MinimalViewInfo.Rotation) * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

	FMatrix ProjectionMatrix;
	if (SceneCaptureComponent && SceneCaptureComponent->bUseCustomProjectionMatrix)
	{
		ProjectionMatrix = AdjustProjectionMatrixForRHI(SceneCaptureComponent->CustomProjectionMatrix);
	}
	else
	{
		ProjectionMatrix = AdjustProjectionMatrixForRHI(CalculateProjectionMatrix(MinimalViewInfo));
	}

	FMatrix ViewMatrix = FTranslationMatrix(-MinimalViewInfo.Location) * ViewRotationMatrix;
	FMatrix InvProjectionMatrix = ProjectionMatrix.Inverse();
	FMatrix InvViewMatrix = ViewRotationMatrix.GetTransposed() * FTranslationMatrix(MinimalViewInfo.Location);

	OutViewProjectionMatrix = ViewMatrix * ProjectionMatrix;
	OutInvViewProjectionMatrix = InvProjectionMatrix * InvViewMatrix;
}

FBoxSphereBounds UMediaPlaneComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox MaxBox({
		FVector(0,  1,  1),
		FVector(0, -1,  1),
		FVector(0,  1, -1),
		FVector(0, -1, -1) });
	MaxBox = MaxBox.TransformBy(LocalToWorld);
	return FBoxSphereBounds(MaxBox);
}

AActor* UMediaPlaneComponent::FindViewTarget() const
{
	AActor* Actor = GetOwner();
	while(Actor)
	{
		if (Actor->HasActiveCameraComponent() || Actor->FindComponentByClass<USceneCaptureComponent2D>())
		{
			return Actor;
		}
		Actor = Actor->GetAttachParentActor();
	}

	return nullptr;
}

#if WITH_EDITOR
void UMediaPlaneComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateMaterialParametersForMedia();
	UpdateTransformScale();
}

void UMediaPlaneComponent::PostEditUndo()
{
	Super::PostEditUndo();
	UpdateMaterialParametersForMedia();
}

UStructProperty* UMediaPlaneComponent::GetMediaPlaneProperty()
{
	return FindField<UStructProperty>(StaticClass(), GET_MEMBER_NAME_CHECKED(UMediaPlaneComponent, Plane));
}

#endif