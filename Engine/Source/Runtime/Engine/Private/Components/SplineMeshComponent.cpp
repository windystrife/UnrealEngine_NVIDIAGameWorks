// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/SplineMeshComponent.h"
#include "Serialization/MemoryWriter.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"
#include "Engine/CollisionProfile.h"
#include "StaticMeshResources.h"
#include "SplineMeshSceneProxy.h"
#include "ShaderParameterUtils.h"
#include "AI/NavigationSystemHelpers.h"
#include "AI/Navigation/NavCollision.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BodySetup.h"

#if WITH_EDITOR
#include "IHierarchicalLODUtilities.h"
#include "HierarchicalLODUtilitiesModule.h"
#endif // WITH_EDITOR

int32 GNoRecreateSplineMeshProxy = 1;
static FAutoConsoleVariableRef CVarNoRecreateSplineMeshProxy(
	TEXT("r.SplineMesh.NoRecreateProxy"),
	GNoRecreateSplineMeshProxy,
	TEXT("Optimization. If true, spline mesh proxies will not be recreated every time they are changed. They are simply updated."));

//////////////////////////////////////////////////////////////////////////
// FSplineMeshVertexFactoryShaderParameters

void FSplineMeshVertexFactoryShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	SplineStartPosParam.Bind(ParameterMap, TEXT("SplineStartPos"), SPF_Mandatory);
	SplineStartTangentParam.Bind(ParameterMap, TEXT("SplineStartTangent"), SPF_Mandatory);
	SplineStartRollParam.Bind(ParameterMap, TEXT("SplineStartRoll"), SPF_Mandatory);
	SplineStartScaleParam.Bind(ParameterMap, TEXT("SplineStartScale"), SPF_Mandatory);
	SplineStartOffsetParam.Bind(ParameterMap, TEXT("SplineStartOffset"), SPF_Mandatory);

	SplineEndPosParam.Bind(ParameterMap, TEXT("SplineEndPos"), SPF_Mandatory);
	SplineEndTangentParam.Bind(ParameterMap, TEXT("SplineEndTangent"), SPF_Mandatory);
	SplineEndRollParam.Bind(ParameterMap, TEXT("SplineEndRoll"), SPF_Mandatory);
	SplineEndScaleParam.Bind(ParameterMap, TEXT("SplineEndScale"), SPF_Mandatory);
	SplineEndOffsetParam.Bind(ParameterMap, TEXT("SplineEndOffset"), SPF_Mandatory);

	SplineUpDirParam.Bind(ParameterMap, TEXT("SplineUpDir"), SPF_Mandatory);
	SmoothInterpRollScaleParam.Bind(ParameterMap, TEXT("SmoothInterpRollScale"), SPF_Mandatory);

	SplineMeshMinZParam.Bind(ParameterMap, TEXT("SplineMeshMinZ"), SPF_Mandatory);
	SplineMeshScaleZParam.Bind(ParameterMap, TEXT("SplineMeshScaleZ"), SPF_Mandatory);

	SplineMeshDirParam.Bind(ParameterMap, TEXT("SplineMeshDir"), SPF_Mandatory);
	SplineMeshXParam.Bind(ParameterMap, TEXT("SplineMeshX"), SPF_Mandatory);
	SplineMeshYParam.Bind(ParameterMap, TEXT("SplineMeshY"), SPF_Mandatory);
}

void FSplineMeshVertexFactoryShaderParameters::SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const
{
	if (BatchElement.bUserDataIsColorVertexBuffer)
	{
		FColorVertexBuffer* OverrideColorVertexBuffer = (FColorVertexBuffer*)BatchElement.UserData;
		check(OverrideColorVertexBuffer);
		static_cast<const FLocalVertexFactory*>(VertexFactory)->SetColorOverrideStream(RHICmdList, OverrideColorVertexBuffer);
	}

	FVertexShaderRHIRef VertexShader = Shader->GetVertexShader();

	if (VertexShader)
	{
		checkSlow(BatchElement.bIsSplineProxy);
		FSplineMeshSceneProxy* SplineProxy = BatchElement.SplineMeshSceneProxy;
		FSplineMeshParams& SplineParams = SplineProxy->SplineParams;

		SetShaderValue(RHICmdList, VertexShader, SplineStartPosParam, SplineParams.StartPos);
		SetShaderValue(RHICmdList, VertexShader, SplineStartTangentParam, SplineParams.StartTangent);
		SetShaderValue(RHICmdList, VertexShader, SplineStartRollParam, SplineParams.StartRoll);
		SetShaderValue(RHICmdList, VertexShader, SplineStartScaleParam, SplineParams.StartScale);
		SetShaderValue(RHICmdList, VertexShader, SplineStartOffsetParam, SplineParams.StartOffset);

		SetShaderValue(RHICmdList, VertexShader, SplineEndPosParam, SplineParams.EndPos);
		SetShaderValue(RHICmdList, VertexShader, SplineEndTangentParam, SplineParams.EndTangent);
		SetShaderValue(RHICmdList, VertexShader, SplineEndRollParam, SplineParams.EndRoll);
		SetShaderValue(RHICmdList, VertexShader, SplineEndScaleParam, SplineParams.EndScale);
		SetShaderValue(RHICmdList, VertexShader, SplineEndOffsetParam, SplineParams.EndOffset);

		SetShaderValue(RHICmdList, VertexShader, SplineUpDirParam, SplineProxy->SplineUpDir);
		SetShaderValue(RHICmdList, VertexShader, SmoothInterpRollScaleParam, SplineProxy->bSmoothInterpRollScale);

		SetShaderValue(RHICmdList, VertexShader, SplineMeshMinZParam, SplineProxy->SplineMeshMinZ);
		SetShaderValue(RHICmdList, VertexShader, SplineMeshScaleZParam, SplineProxy->SplineMeshScaleZ);

		FVector DirMask(0, 0, 0);
		DirMask[SplineProxy->ForwardAxis] = 1;
		SetShaderValue(RHICmdList, VertexShader, SplineMeshDirParam, DirMask);
		DirMask = FVector::ZeroVector;
		DirMask[(SplineProxy->ForwardAxis + 1) % 3] = 1;
		SetShaderValue(RHICmdList, VertexShader, SplineMeshXParam, DirMask);
		DirMask = FVector::ZeroVector;
		DirMask[(SplineProxy->ForwardAxis + 2) % 3] = 1;
		SetShaderValue(RHICmdList, VertexShader, SplineMeshYParam, DirMask);
	}
}

//////////////////////////////////////////////////////////////////////////
// SplineMeshVertexFactory

IMPLEMENT_VERTEX_FACTORY_TYPE(FSplineMeshVertexFactory, "/Engine/Private/LocalVertexFactory.ush", true, true, true, true, true);


FVertexFactoryShaderParameters* FSplineMeshVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FSplineMeshVertexFactoryShaderParameters() : NULL;
}

//////////////////////////////////////////////////////////////////////////
// SplineMeshSceneProxy

void FSplineMeshSceneProxy::InitVertexFactory(USplineMeshComponent* InComponent, int32 InLODIndex, FColorVertexBuffer* InOverrideColorVertexBuffer)
{
	if (InComponent == nullptr || InComponent->GetStaticMesh() == nullptr)
	{
		return;
	}

	uint32 TangentXOffset = 0;
	uint32 TangetnZOffset = 0;
	uint32 UVsBaseOffset = 0;

	FStaticMeshLODResources& RD = InComponent->GetStaticMesh()->RenderData->LODResources[InLODIndex];
	SELECT_STATIC_MESH_VERTEX_TYPE(
		RD.VertexBuffer.GetUseHighPrecisionTangentBasis(),
		RD.VertexBuffer.GetUseFullPrecisionUVs(),
		RD.VertexBuffer.GetNumTexCoords(),
		{
			TangentXOffset = STRUCT_OFFSET(VertexType, TangentX);
			TangetnZOffset = STRUCT_OFFSET(VertexType, TangentZ);
			UVsBaseOffset = STRUCT_OFFSET(VertexType, UVs);
		});

	// Initialize the static mesh's vertex factory.
	ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER(
		InitSplineMeshVertexFactory,
		FStaticMeshLODResources*, RenderData, &InComponent->GetStaticMesh()->RenderData->LODResources[InLODIndex],
		UStaticMesh*, Parent, InComponent->GetStaticMesh(),
		bool, bOverrideColorVertexBuffer, !!InOverrideColorVertexBuffer,
		uint32, TangentXOffset, TangentXOffset,
		uint32, TangetnZOffset, TangetnZOffset,
		uint32, UVsBaseOffset, UVsBaseOffset,
		{

		if ((RenderData->SplineVertexFactory && !bOverrideColorVertexBuffer) || (RenderData->SplineVertexFactoryOverrideColorVertexBuffer && bOverrideColorVertexBuffer))
		{
			// we already have it
			return;
		}
		FSplineMeshVertexFactory* VertexFactory = new FSplineMeshVertexFactory;
		if (bOverrideColorVertexBuffer)
		{
			RenderData->SplineVertexFactoryOverrideColorVertexBuffer = VertexFactory;
		}
		else
		{
			RenderData->SplineVertexFactory = VertexFactory;
		}
		FLocalVertexFactory::FDataType Data;

		Data.PositionComponent = FVertexStreamComponent(
			&RenderData->PositionVertexBuffer,
			STRUCT_OFFSET(FPositionVertex, Position),
			RenderData->PositionVertexBuffer.GetStride(),
			VET_Float3
			);

		Data.TangentBasisComponents[0] = FVertexStreamComponent(
			&RenderData->VertexBuffer,
			TangentXOffset,
			RenderData->VertexBuffer.GetStride(),
			RenderData->VertexBuffer.GetUseHighPrecisionTangentBasis() ?
				TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::HighPrecision>::VertexElementType : 
				TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::VertexElementType
			);

		Data.TangentBasisComponents[1] = FVertexStreamComponent(
			&RenderData->VertexBuffer,
			TangetnZOffset,
			RenderData->VertexBuffer.GetStride(),
			RenderData->VertexBuffer.GetUseHighPrecisionTangentBasis() ?
				TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::HighPrecision>::VertexElementType : 
				TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::VertexElementType
			);

		if (bOverrideColorVertexBuffer)
		{
			Data.ColorComponent = FVertexStreamComponent(
				&GNullColorVertexBuffer,
				0,	// Struct offset to color
				sizeof(FColor), //asserted elsewhere
				VET_Color,
				false, // not instanced
				true // set in SetMesh
				);
		}
		else
		{
			FColorVertexBuffer* LODColorVertexBuffer = &RenderData->ColorVertexBuffer;
			if (LODColorVertexBuffer->GetNumVertices() > 0)
			{
				Data.ColorComponent = FVertexStreamComponent(
					LODColorVertexBuffer,
					0,	// Struct offset to color
					LODColorVertexBuffer->GetStride(),
					VET_Color
					);
			}
		}

		Data.TextureCoordinates.Empty();

		uint32 UVSizeInBytes = RenderData->VertexBuffer.GetUseFullPrecisionUVs() ?
			sizeof(TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>::UVsTypeT) : sizeof(TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::Default>::UVsTypeT);

		EVertexElementType UVDoubleWideVertexElementType = RenderData->VertexBuffer.GetUseFullPrecisionUVs() ?
			VET_Float4 : VET_Half4;

		EVertexElementType UVVertexElementType = RenderData->VertexBuffer.GetUseFullPrecisionUVs() ?
			VET_Float2 : VET_Half2;

		int32 UVIndex;
		for (UVIndex = 0; UVIndex < (int32)RenderData->VertexBuffer.GetNumTexCoords() - 1; UVIndex += 2)
		{
			Data.TextureCoordinates.Add(FVertexStreamComponent(
				&RenderData->VertexBuffer,
				UVsBaseOffset + UVSizeInBytes * UVIndex,
				RenderData->VertexBuffer.GetStride(),
				UVDoubleWideVertexElementType
				));
		}
		// possible last UV channel if we have an odd number
		if (UVIndex < (int32)RenderData->VertexBuffer.GetNumTexCoords())
		{
			Data.TextureCoordinates.Add(FVertexStreamComponent(
				&RenderData->VertexBuffer,
				UVsBaseOffset + UVSizeInBytes * UVIndex,
				RenderData->VertexBuffer.GetStride(),
				UVVertexElementType
				));
		}

		if (Parent->LightMapCoordinateIndex >= 0 && (uint32)Parent->LightMapCoordinateIndex < RenderData->VertexBuffer.GetNumTexCoords())
		{
			Data.LightMapCoordinateComponent = FVertexStreamComponent(
				&RenderData->VertexBuffer,
				UVsBaseOffset + UVSizeInBytes * Parent->LightMapCoordinateIndex,
				RenderData->VertexBuffer.GetStride(),
				UVVertexElementType
				);
		}

		VertexFactory->SetData(Data);

		VertexFactory->InitResource();
	});
}


//////////////////////////////////////////////////////////////////////////
// SplineMeshComponent

USplineMeshComponent::USplineMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Mobility = EComponentMobility::Static;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	bAllowSplineEditingPerInstance = false;
	bSmoothInterpRollScale = false;
	bHasCustomNavigableGeometry = EHasCustomNavigableGeometry::Yes;

	SplineUpDir.Z = 1.0f;

	// Default to useful length and scale
	SplineParams.StartTangent = FVector(100.f, 0.f, 0.f);
	SplineParams.StartScale = FVector2D(1.f, 1.f);

	SplineParams.EndPos = FVector(100.f, 0.f, 0.f);
	SplineParams.EndTangent = FVector(100.f, 0.f, 0.f);
	SplineParams.EndScale = FVector2D(1.f, 1.f);

	SplineBoundaryMin = 0;
	SplineBoundaryMax = 0;

	bMeshDirty = false;
}

FVector USplineMeshComponent::GetStartPosition() const
{
	return SplineParams.StartPos;
}

void USplineMeshComponent::SetStartPosition(FVector StartPos, bool bUpdateMesh)
{
	SplineParams.StartPos = StartPos;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

FVector USplineMeshComponent::GetStartTangent() const
{
	return SplineParams.StartTangent;
}

void USplineMeshComponent::SetStartTangent(FVector StartTangent, bool bUpdateMesh)
{
	SplineParams.StartTangent = StartTangent;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

FVector USplineMeshComponent::GetEndPosition() const
{
	return SplineParams.EndPos;
}

void USplineMeshComponent::SetEndPosition(FVector EndPos, bool bUpdateMesh)
{
	SplineParams.EndPos = EndPos;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

FVector USplineMeshComponent::GetEndTangent() const
{
	return SplineParams.EndTangent;
}

void USplineMeshComponent::SetEndTangent(FVector EndTangent, bool bUpdateMesh)
{
	SplineParams.EndTangent = EndTangent;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

void USplineMeshComponent::SetStartAndEnd(FVector StartPos, FVector StartTangent, FVector EndPos, FVector EndTangent, bool bUpdateMesh)
{
	SplineParams.StartPos = StartPos;
	SplineParams.StartTangent = StartTangent;
	SplineParams.EndPos = EndPos;
	SplineParams.EndTangent = EndTangent;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

FVector2D USplineMeshComponent::GetStartScale() const
{
	return SplineParams.StartScale;
}

void USplineMeshComponent::SetStartScale(FVector2D StartScale, bool bUpdateMesh)
{
	SplineParams.StartScale = StartScale;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

float USplineMeshComponent::GetStartRoll() const
{
	return SplineParams.StartRoll;
}

void USplineMeshComponent::SetStartRoll(float StartRoll, bool bUpdateMesh)
{
	SplineParams.StartRoll = StartRoll;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

FVector2D USplineMeshComponent::GetStartOffset() const
{
	return SplineParams.StartOffset;
}

void USplineMeshComponent::SetStartOffset(FVector2D StartOffset, bool bUpdateMesh)
{
	SplineParams.StartOffset = StartOffset;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

FVector2D USplineMeshComponent::GetEndScale() const
{
	return SplineParams.EndScale;
}

void USplineMeshComponent::SetEndScale(FVector2D EndScale, bool bUpdateMesh)
{
	SplineParams.EndScale = EndScale;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

float USplineMeshComponent::GetEndRoll() const
{
	return SplineParams.EndRoll;
}

void USplineMeshComponent::SetEndRoll(float EndRoll, bool bUpdateMesh)
{
	SplineParams.EndRoll = EndRoll;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

FVector2D USplineMeshComponent::GetEndOffset() const
{
	return SplineParams.EndOffset;
}

void USplineMeshComponent::SetEndOffset(FVector2D EndOffset, bool bUpdateMesh)
{
	SplineParams.EndOffset = EndOffset;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

ESplineMeshAxis::Type USplineMeshComponent::GetForwardAxis() const
{
	return ForwardAxis;
}

void USplineMeshComponent::SetForwardAxis(ESplineMeshAxis::Type InForwardAxis, bool bUpdateMesh)
{
	ForwardAxis = InForwardAxis;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

FVector USplineMeshComponent::GetSplineUpDir() const
{
	return SplineUpDir;
}

void USplineMeshComponent::SetSplineUpDir(const FVector& InSplineUpDir, bool bUpdateMesh)
{
	SplineUpDir = InSplineUpDir.GetSafeNormal();
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

float USplineMeshComponent::GetBoundaryMin() const
{
	return SplineBoundaryMin;
}

void USplineMeshComponent::SetBoundaryMin(float InBoundaryMin, bool bUpdateMesh)
{
	SplineBoundaryMin = InBoundaryMin;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

float USplineMeshComponent::GetBoundaryMax() const
{
	return SplineBoundaryMax;
}

void USplineMeshComponent::SetBoundaryMax(float InBoundaryMax, bool bUpdateMesh)
{
	SplineBoundaryMax = InBoundaryMax;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

void USplineMeshComponent::UpdateMesh()
{
	if (bMeshDirty)
	{
		UpdateRenderStateAndCollision();
	}
}

void USplineMeshComponent::UpdateMesh_Concurrent()
{
	if (bMeshDirty)
	{
		UpdateRenderStateAndCollision_Internal(true);
	}
}

void USplineMeshComponent::CalculateScaleZAndMinZ(float& OutScaleZ, float& OutMinZ) const
{
	if (GetStaticMesh())
	{
		if (FMath::IsNearlyEqual(SplineBoundaryMin, SplineBoundaryMax))
		{
			FBoxSphereBounds StaticMeshBounds = GetStaticMesh()->GetBounds();
			OutScaleZ = 0.5f / USplineMeshComponent::GetAxisValue(StaticMeshBounds.BoxExtent, ForwardAxis); // 1/(2 * Extent)
			OutMinZ = USplineMeshComponent::GetAxisValue(StaticMeshBounds.Origin, ForwardAxis) * OutScaleZ - 0.5f;
		}
		else
		{
			OutScaleZ = 1.0f / (SplineBoundaryMax - SplineBoundaryMin);
			OutMinZ = SplineBoundaryMin * OutScaleZ;
		}
	}
}

void USplineMeshComponent::UpdateRenderStateAndCollision()
{
	UpdateRenderStateAndCollision_Internal(false);
}

void USplineMeshComponent::UpdateRenderStateAndCollision_Internal(bool bConcurrent)
{
	if (GNoRecreateSplineMeshProxy && bRenderStateCreated && SceneProxy)
	{
		if (bConcurrent)
		{
			SendRenderTransform_Concurrent();
		}
		else
		{
			MarkRenderTransformDirty();
		}

		FSplineMeshSceneProxy* SplineProxy = static_cast<FSplineMeshSceneProxy*>(SceneProxy);

		float SplineMeshScaleZ = 1.f;
		float SplineMeshMinZ = 1.f;
		CalculateScaleZAndMinZ(SplineMeshScaleZ, SplineMeshMinZ);

		ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER(
			UpdateSplineParamsRTCommand,
			FSplineMeshSceneProxy*, SplineProxy, SplineProxy,
			FSplineMeshParams, SplineParams, SplineParams,
			TEnumAsByte<ESplineMeshAxis::Type>, ForwardAxis, ForwardAxis,
			FVector, SplineUpDir, SplineUpDir,
			float, SplineMeshScaleZ, SplineMeshScaleZ,
			float, SplineMeshMinZ, SplineMeshMinZ,
			{
				SplineProxy->SplineParams = SplineParams;
				SplineProxy->ForwardAxis = ForwardAxis;
				SplineProxy->SplineUpDir = SplineUpDir;
				SplineProxy->SplineMeshScaleZ = SplineMeshScaleZ;
				SplineProxy->SplineMeshMinZ = SplineMeshMinZ;
			});
	}
	else
	{
		if (bConcurrent)
		{
			RecreateRenderState_Concurrent();
		}
		else
		{
			MarkRenderStateDirty();
		}
	}

	CachedMeshBodySetupGuid.Invalidate();
	RecreatePhysicsState();

	bMeshDirty = false;
}

void USplineMeshComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.UE4Ver() < VER_UE4_SPLINE_MESH_ORIENTATION)
	{
		ForwardAxis = ESplineMeshAxis::Z;
		SplineParams.StartRoll -= HALF_PI;
		SplineParams.EndRoll -= HALF_PI;

		float Temp = SplineParams.StartOffset.X;
		SplineParams.StartOffset.X = -SplineParams.StartOffset.Y;
		SplineParams.StartOffset.Y = Temp;
		Temp = SplineParams.EndOffset.X;
		SplineParams.EndOffset.X = -SplineParams.EndOffset.Y;
		SplineParams.EndOffset.Y = Temp;
	}

#if WITH_EDITOR
	if (BodySetup != NULL)
	{
		BodySetup->SetFlags(RF_Transactional);
	}
#endif
}

bool USplineMeshComponent::Modify(bool bAlwaysMarkDirty)
{
	bool bSavedToTransactionBuffer = Super::Modify(bAlwaysMarkDirty);

	if (BodySetup != NULL)
	{
		BodySetup->Modify(bAlwaysMarkDirty);
	}

	return bSavedToTransactionBuffer;
}

FPrimitiveSceneProxy* USplineMeshComponent::CreateSceneProxy()
{
	// Verify that the mesh is valid before using it.
	const bool bMeshIsValid = 
		// make sure we have an actual staticmesh
		GetStaticMesh() &&
		GetStaticMesh()->HasValidRenderData();

	if (bMeshIsValid)
	{
		return ::new FSplineMeshSceneProxy(this);
	}
	else
	{
		return NULL;
	}
}


/**
* Functions used for transforming a static mesh component based on a spline.
* This needs to be updated if the spline functionality changes!
*/
static float SmoothStep(float A, float B, float X)
{
	if (X < A)
	{
		return 0.0f;
	}
	else if (X >= B)
	{
		return 1.0f;
	}
	const float InterpFraction = (X - A) / (B - A);
	return InterpFraction * InterpFraction * (3.0f - 2.0f * InterpFraction);
}

static FVector SplineEvalPos(const FVector& StartPos, const FVector& StartTangent, const FVector& EndPos, const FVector& EndTangent, float A)
{
	const float A2 = A  * A;
	const float A3 = A2 * A;

	return (((2 * A3) - (3 * A2) + 1) * StartPos) + ((A3 - (2 * A2) + A) * StartTangent) + ((A3 - A2) * EndTangent) + (((-2 * A3) + (3 * A2)) * EndPos);
}

static FVector SplineEvalDir(const FVector& StartPos, const FVector& StartTangent, const FVector& EndPos, const FVector& EndTangent, float A)
{
	const FVector C = (6 * StartPos) + (3 * StartTangent) + (3 * EndTangent) - (6 * EndPos);
	const FVector D = (-6 * StartPos) - (4 * StartTangent) - (2 * EndTangent) + (6 * EndPos);
	const FVector E = StartTangent;

	const float A2 = A  * A;

	return ((C * A2) + (D * A) + E).GetSafeNormal();
}


FBoxSphereBounds USplineMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (!GetStaticMesh())
	{
		return FBoxSphereBounds(FBox(ForceInit));
	}

	float MinT = 0.0f;
	float MaxT = 1.0f;

	const FBoxSphereBounds MeshBounds = GetStaticMesh()->GetBounds();

	const bool bHasCustomBoundary = !FMath::IsNearlyEqual(SplineBoundaryMin, SplineBoundaryMax);
	if (bHasCustomBoundary)
	{
		// If there's a custom boundary, alter the min/max of the spline we need to evaluate
		const float MeshMin = GetAxisValue(MeshBounds.Origin - MeshBounds.BoxExtent, ForwardAxis);
		const float MeshMax = GetAxisValue(MeshBounds.Origin + MeshBounds.BoxExtent, ForwardAxis);

		const float MeshMinT = (MeshMin - SplineBoundaryMin) / (SplineBoundaryMax - SplineBoundaryMin);
		const float MeshMaxT = (MeshMax - SplineBoundaryMin) / (SplineBoundaryMax - SplineBoundaryMin);

		// Disallow extrapolation beyond a certain value; enormous bounding boxes cause the render thread to crash
		const float MaxSplineExtrapolation = 4.0f;
		if (FMath::Abs(MeshMinT) < MaxSplineExtrapolation && FMath::Abs(MeshMaxT) < MaxSplineExtrapolation)
		{
			MinT = MeshMinT;
			MaxT = MeshMaxT;
		}
	}

	const FVector AxisMask = GetAxisMask(ForwardAxis);
	const FVector FlattenedMeshOrigin = MeshBounds.Origin * AxisMask;
	const FVector FlattenedMeshExtent = MeshBounds.BoxExtent * AxisMask;
	const FBox MeshBoundingBox = FBox(FlattenedMeshOrigin - FlattenedMeshExtent, FlattenedMeshOrigin + FlattenedMeshExtent);

	FBox BoundingBox(ForceInit);
	BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(MinT));
	BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(MaxT));

	// Work out coefficients of the cubic spline derivative equation dx/dt
	const FVector A = 6.0f * SplineParams.StartPos + 3.0f * SplineParams.StartTangent + 3.0f * SplineParams.EndTangent - 6.0f * SplineParams.EndPos;
	const FVector B = -6.0f * SplineParams.StartPos - 4.0f * SplineParams.StartTangent - 2.0f * SplineParams.EndTangent + 6.0f * SplineParams.EndPos;
	const FVector C = SplineParams.StartTangent;

	// Minima/maxima happen where dx/dt == 0, calculate t values
	const FVector Discriminant = B * B - 4.0f * A * C;

	// Work out minima/maxima component-by-component.
	// Negative discriminant means no solution; A == 0 implies coincident start/end points
	if (Discriminant.X > 0.0f && !FMath::IsNearlyZero(A.X))
	{
		const float SqrtDiscriminant = FMath::Sqrt(Discriminant.X);
		const float Denominator = 0.5f / A.X;
		const float T0 = (-B.X + SqrtDiscriminant) * Denominator;
		const float T1 = (-B.X - SqrtDiscriminant) * Denominator;

		if (T0 >= MinT && T0 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T0));
		}

		if (T1 >= MinT && T1 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T1));
		}
	}

	if (Discriminant.Y > 0.0f && !FMath::IsNearlyZero(A.Y))
	{
		const float SqrtDiscriminant = FMath::Sqrt(Discriminant.Y);
		const float Denominator = 0.5f / A.Y;
		const float T0 = (-B.Y + SqrtDiscriminant) * Denominator;
		const float T1 = (-B.Y - SqrtDiscriminant) * Denominator;

		if (T0 >= MinT && T0 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T0));
		}

		if (T1 >= MinT && T1 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T1));
		}
	}

	if (Discriminant.Z > 0.0f && !FMath::IsNearlyZero(A.Z))
	{
		const float SqrtDiscriminant = FMath::Sqrt(Discriminant.Z);
		const float Denominator = 0.5f / A.Z;
		const float T0 = (-B.Z + SqrtDiscriminant) * Denominator;
		const float T1 = (-B.Z - SqrtDiscriminant) * Denominator;

		if (T0 >= MinT && T0 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T0));
		}

		if (T1 >= MinT && T1 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T1));
		}
	}

	return FBoxSphereBounds(BoundingBox.TransformBy(LocalToWorld));
}

FTransform USplineMeshComponent::GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace) const
{
	if (InSocketName != NAME_None)
	{
		UStaticMeshSocket const* const Socket = GetSocketByName(InSocketName);
		if (Socket)
		{
			FTransform SocketTransform;
			SocketTransform = FTransform(Socket->RelativeRotation, Socket->RelativeLocation * GetAxisMask(ForwardAxis), Socket->RelativeScale);
			SocketTransform = SocketTransform * CalcSliceTransform(GetAxisValue(Socket->RelativeLocation, ForwardAxis));

			switch (TransformSpace)
			{
			case RTS_World:
			{
				return SocketTransform * GetComponentToWorld();
			}
			case RTS_Actor:
			{
				if (const AActor* Actor = GetOwner())
				{
					return (SocketTransform * GetComponentToWorld()).GetRelativeTransform(GetOwner()->GetTransform());
				}
				break;
			}
			case RTS_Component:
			{
				return SocketTransform;
			}
			}
		}
	}

	return Super::GetSocketTransform(InSocketName, TransformSpace);
}


FTransform USplineMeshComponent::CalcSliceTransform(const float DistanceAlong) const
{
	const bool bHasCustomBoundary = !FMath::IsNearlyEqual(SplineBoundaryMin, SplineBoundaryMax);

	// Find how far 'along' mesh we are
	float Alpha = 0.f;
	if (bHasCustomBoundary)
	{
		Alpha = (DistanceAlong - SplineBoundaryMin) / (SplineBoundaryMax - SplineBoundaryMin);
	}
	else if (GetStaticMesh())
	{
		const FBoxSphereBounds StaticMeshBounds = GetStaticMesh()->GetBounds();
		const float MeshMinZ = GetAxisValue(StaticMeshBounds.Origin, ForwardAxis) - GetAxisValue(StaticMeshBounds.BoxExtent, ForwardAxis);
		const float MeshRangeZ = 2.0f * GetAxisValue(StaticMeshBounds.BoxExtent, ForwardAxis);
		Alpha = (DistanceAlong - MeshMinZ) / MeshRangeZ;
	}

	return CalcSliceTransformAtSplineOffset(Alpha);
}

FTransform USplineMeshComponent::CalcSliceTransformAtSplineOffset(const float Alpha) const
{
	// Apply hermite interp to Alpha if desired
	const float HermiteAlpha = bSmoothInterpRollScale ? SmoothStep(0.0, 1.0, Alpha) : Alpha;

	// Then find the point and direction of the spline at this point along
	FVector SplinePos = SplineEvalPos( SplineParams.StartPos, SplineParams.StartTangent, SplineParams.EndPos, SplineParams.EndTangent, Alpha );
	const FVector SplineDir = SplineEvalDir( SplineParams.StartPos, SplineParams.StartTangent, SplineParams.EndPos, SplineParams.EndTangent, Alpha );

	// Find base frenet frame
	const FVector BaseXVec = (SplineUpDir ^ SplineDir).GetSafeNormal();
	const FVector BaseYVec = (SplineDir ^ BaseXVec).GetSafeNormal();

	// Offset the spline by the desired amount
	const FVector2D SliceOffset = FMath::Lerp<FVector2D>(SplineParams.StartOffset, SplineParams.EndOffset, HermiteAlpha);
	SplinePos += SliceOffset.X * BaseXVec;
	SplinePos += SliceOffset.Y * BaseYVec;

	// Apply roll to frame around spline
	const float UseRoll = FMath::Lerp(SplineParams.StartRoll, SplineParams.EndRoll, HermiteAlpha);
	const float CosAng = FMath::Cos(UseRoll);
	const float SinAng = FMath::Sin(UseRoll);
	const FVector XVec = (CosAng * BaseXVec) - (SinAng * BaseYVec);
	const FVector YVec = (CosAng * BaseYVec) + (SinAng * BaseXVec);

	// Find scale at this point along spline
	const FVector2D UseScale = FMath::Lerp(SplineParams.StartScale, SplineParams.EndScale, HermiteAlpha);

	// Build overall transform
	FTransform SliceTransform;
	switch (ForwardAxis)
	{
	case ESplineMeshAxis::X:
		SliceTransform = FTransform(SplineDir, XVec, YVec, SplinePos);
		SliceTransform.SetScale3D(FVector(1, UseScale.X, UseScale.Y));
		break;
	case ESplineMeshAxis::Y:
		SliceTransform = FTransform(YVec, SplineDir, XVec, SplinePos);
		SliceTransform.SetScale3D(FVector(UseScale.Y, 1, UseScale.X));
		break;
	case ESplineMeshAxis::Z:
		SliceTransform = FTransform(XVec, YVec, SplineDir, SplinePos);
		SliceTransform.SetScale3D(FVector(UseScale.X, UseScale.Y, 1));
		break;
	default:
		check(0);
		break;
	}

	return SliceTransform;
}


bool USplineMeshComponent::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
	if (GetStaticMesh())
	{
		GetStaticMesh()->GetPhysicsTriMeshData(CollisionData, InUseAllTriData);

		FVector Mask = FVector(1,1,1);
		GetAxisValue(Mask, ForwardAxis) = 0;

		for (FVector& CollisionVert : CollisionData->Vertices)
		{
			CollisionVert = CalcSliceTransform(GetAxisValue(CollisionVert, ForwardAxis)).TransformPosition(CollisionVert * Mask);
		}

		CollisionData->bDeformableMesh = true;

		return true;
	}

	return false;
}

bool USplineMeshComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	if (GetStaticMesh())
	{
		return GetStaticMesh()->ContainsPhysicsTriMeshData(InUseAllTriData);
	}

	return false;
}

void USplineMeshComponent::GetMeshId(FString& OutMeshId)
{
	// First get the base mesh id from the static mesh
	if (GetStaticMesh())
	{
		GetStaticMesh()->GetMeshId(OutMeshId);
	}

	// new method: Same guid as the base mesh but with a unique DDC-id based on the spline params.
	// This fixes the bug where running a blueprint construction script regenerates the guid and uses
	// a new DDC slot even if the mesh hasn't changed
	// If BodySetup is null that means we're *currently* duplicating one, and haven't transformed its data
	// to fit the spline yet, so just use the data from the base mesh by using a blank MeshId
	// It would be better if we could stop it building data in that case at all...

	if (BodySetup != nullptr && BodySetup->BodySetupGuid == CachedMeshBodySetupGuid)
	{
		TArray<uint8> TempBytes;
		TempBytes.Reserve(256);

		FMemoryWriter Ar(TempBytes);
		Ar << SplineParams.StartPos;
		Ar << SplineParams.StartTangent;
		Ar << SplineParams.StartScale;
		Ar << SplineParams.StartRoll;
		Ar << SplineParams.StartOffset;
		Ar << SplineParams.EndPos;
		Ar << SplineParams.EndTangent;
		Ar << SplineParams.EndScale;
		Ar << SplineParams.EndRoll;
		Ar << SplineParams.EndOffset;
		Ar << SplineUpDir;
		bool bSmoothInterp = bSmoothInterpRollScale;
		Ar << bSmoothInterp; // can't write a bitfield member into an archive
		Ar << ForwardAxis;
		Ar << SplineBoundaryMin;
		Ar << SplineBoundaryMax;

		// Now convert the raw bytes to a string.
		const uint8* SettingsAsBytes = TempBytes.GetData();
		OutMeshId.Reserve(OutMeshId.Len() + TempBytes.Num() + 1);
		for (int32 ByteIndex = 0; ByteIndex < TempBytes.Num(); ++ByteIndex)
		{
			ByteToHex(SettingsAsBytes[ByteIndex], OutMeshId);
		}
	}
}

void USplineMeshComponent::OnCreatePhysicsState()
{
	// With editor code we can recreate the collision if the mesh changes
	const FGuid MeshBodySetupGuid = (GetStaticMesh() != nullptr ? GetStaticMesh()->BodySetup->BodySetupGuid : FGuid());
	if (CachedMeshBodySetupGuid != MeshBodySetupGuid)
	{
		RecreateCollision();
	}

	return Super::OnCreatePhysicsState();
}

UBodySetup* USplineMeshComponent::GetBodySetup()
{
	// Don't return a body setup that has no collision, it means we are interactively moving the spline and don't want to build collision.
	// Instead we explicitly build collision with USplineMeshComponent::RecreateCollision()
	if (BodySetup != NULL && (BodySetup->TriMeshes.Num() || BodySetup->AggGeom.GetElementCount() > 0))
	{
		return BodySetup;
	}
	return NULL;
}

bool USplineMeshComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	// the NavCollision is supposed to be faster than exporting the regular collision,
	// but I'm not sure that's true here, as the regular collision is pre-distorted to the spline

	if (GetStaticMesh() != nullptr && GetStaticMesh()->NavCollision != nullptr)
	{
		UNavCollision* NavCollision = GetStaticMesh()->NavCollision;
		
		if (ensure(!NavCollision->bIsDynamicObstacle))
		{
			if (NavCollision->bHasConvexGeometry)
			{
				FVector Mask = FVector(1, 1, 1);
				GetAxisValue(Mask, ForwardAxis) = 0;

				TArray<FVector> VertexBuffer;
				VertexBuffer.Reserve(FMath::Max(NavCollision->ConvexCollision.VertexBuffer.Num(), NavCollision->TriMeshCollision.VertexBuffer.Num()));

				for (int32 i = 0; i < NavCollision->ConvexCollision.VertexBuffer.Num(); ++i)
				{
					FVector Vertex = NavCollision->ConvexCollision.VertexBuffer[i];
					Vertex = CalcSliceTransform(GetAxisValue(Vertex, ForwardAxis)).TransformPosition(Vertex * Mask);
					VertexBuffer.Add(Vertex);
				}
				GeomExport.ExportCustomMesh(VertexBuffer.GetData(), VertexBuffer.Num(),
					NavCollision->ConvexCollision.IndexBuffer.GetData(), NavCollision->ConvexCollision.IndexBuffer.Num(),
					GetComponentTransform());

				VertexBuffer.Reset();
				for (int32 i = 0; i < NavCollision->TriMeshCollision.VertexBuffer.Num(); ++i)
				{
					FVector Vertex = NavCollision->TriMeshCollision.VertexBuffer[i];
					Vertex = CalcSliceTransform(GetAxisValue(Vertex, ForwardAxis)).TransformPosition(Vertex * Mask);
					VertexBuffer.Add(Vertex);
				}
				GeomExport.ExportCustomMesh(VertexBuffer.GetData(), VertexBuffer.Num(),
					NavCollision->TriMeshCollision.IndexBuffer.GetData(), NavCollision->TriMeshCollision.IndexBuffer.Num(),
					GetComponentTransform());

				return false;
			}
		}
	}

	return true;
}

void USplineMeshComponent::DestroyBodySetup()
{
	if (BodySetup != NULL)
	{
		BodySetup->MarkPendingKill();
		BodySetup = NULL;
#if WITH_EDITORONLY_DATA
		CachedMeshBodySetupGuid.Invalidate();
#endif
	}
}

void USplineMeshComponent::RecreateCollision()
{
	if (GetStaticMesh() && IsCollisionEnabled())
	{
		if (BodySetup == NULL)
		{
			BodySetup = DuplicateObject<UBodySetup>(GetStaticMesh()->BodySetup, this);
			BodySetup->SetFlags(RF_Transactional);
			BodySetup->InvalidatePhysicsData();
		}
		else
		{
			BodySetup->Modify();
			BodySetup->InvalidatePhysicsData();
			BodySetup->CopyBodyPropertiesFrom(GetStaticMesh()->BodySetup);
			BodySetup->CollisionTraceFlag = GetStaticMesh()->BodySetup->CollisionTraceFlag;
		}
		BodySetup->BodySetupGuid = GetStaticMesh()->BodySetup->BodySetupGuid;
		CachedMeshBodySetupGuid = GetStaticMesh()->BodySetup->BodySetupGuid;

		if (BodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple)
		{
			BodySetup->AggGeom.EmptyElements();
		}
		else
		{
			FVector Mask = FVector(1, 1, 1);
			GetAxisValue(Mask, ForwardAxis) = 0;

			// distortion of a sphere can't be done nicely, so we just transform the origin and size
			for (FKSphereElem& SphereElem : BodySetup->AggGeom.SphereElems)
			{
				const float Z = GetAxisValue(SphereElem.Center, ForwardAxis);
				FTransform SliceTransform = CalcSliceTransform(Z);
				SphereElem.Center *= Mask;

				SphereElem.Radius *= SliceTransform.GetMaximumAxisScale();
				SphereElem.Center = SliceTransform.TransformPosition(SphereElem.Center);
			}

			// distortion of a sphyl can't be done nicely, so we just transform the origin and size
			for (FKSphylElem& SphylElem : BodySetup->AggGeom.SphylElems)
			{
				const float Z = GetAxisValue(SphylElem.Center, ForwardAxis);
				FTransform SliceTransform = CalcSliceTransform(Z);
				SphylElem.Center *= Mask;

				FTransform TM = SphylElem.GetTransform();
				SphylElem.Length = (TM * SliceTransform).TransformVector(FVector(0, 0, SphylElem.Length)).Size();
				SphylElem.Radius *= SliceTransform.GetMaximumAxisScale();

				SphylElem.SetTransform(TM * SliceTransform);
			}

			// Convert boxes to convex hulls to better respect distortion
			for (FKBoxElem& BoxElem : BodySetup->AggGeom.BoxElems)
			{
				FKConvexElem& ConvexElem = *new(BodySetup->AggGeom.ConvexElems) FKConvexElem();

				const FVector Radii = FVector(BoxElem.X / 2, BoxElem.Y / 2, BoxElem.Z / 2).ComponentMax(FVector(1.0f));
				const FTransform ElementTM = BoxElem.GetTransform();
				ConvexElem.VertexData.Empty(8);
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(-1,-1,-1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(-1,-1, 1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(-1, 1,-1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(-1, 1, 1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector( 1,-1,-1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector( 1,-1, 1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector( 1, 1,-1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector( 1, 1, 1)));

				ConvexElem.UpdateElemBox();
			}
			BodySetup->AggGeom.BoxElems.Empty();

			// transform the points of the convex hulls into spline space
			for (FKConvexElem& ConvexElem : BodySetup->AggGeom.ConvexElems)
			{
				FTransform TM = ConvexElem.GetTransform();
				for (FVector& Point : ConvexElem.VertexData)
				{
					// pretransform the point by its local transform so we are working in untransformed local space
					FVector TransformedPoint = TM.TransformPosition(Point);
					// apply the transform to spline space
					Point = CalcSliceTransform(GetAxisValue(TransformedPoint, ForwardAxis)).TransformPosition(TransformedPoint * Mask);
				}

				// Set the local transform as an identity as points have already been transformed
				ConvexElem.SetTransform(FTransform::Identity);
				ConvexElem.UpdateElemBox();
			}
		}

		BodySetup->CreatePhysicsMeshes();
	}
	else
	{
		DestroyBodySetup();
	}
}

/** Used to store spline mesh data during RerunConstructionScripts */
class FSplineMeshInstanceData : public FSceneComponentInstanceData
{
public:
	explicit FSplineMeshInstanceData(const USplineMeshComponent* SourceComponent)
		: FSceneComponentInstanceData(SourceComponent)
	{
	}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<USplineMeshComponent>(Component)->ApplyComponentInstanceData(this);
	}

	FVector StartPos;
	FVector EndPos;
	FVector StartTangent;
	FVector EndTangent;
};

FActorComponentInstanceData* USplineMeshComponent::GetComponentInstanceData() const
{
	FActorComponentInstanceData* InstanceData = nullptr;
	if (bAllowSplineEditingPerInstance)
	{
		FSplineMeshInstanceData *SplineMeshInstanceData = new FSplineMeshInstanceData(this);
		SplineMeshInstanceData->StartPos = SplineParams.StartPos;
		SplineMeshInstanceData->EndPos = SplineParams.EndPos;
		SplineMeshInstanceData->StartTangent = SplineParams.StartTangent;
		SplineMeshInstanceData->EndTangent = SplineParams.EndTangent;
		InstanceData = SplineMeshInstanceData;
	}
	else
	{
		InstanceData = Super::GetComponentInstanceData();
	}
	return InstanceData;
}

void USplineMeshComponent::ApplyComponentInstanceData(FSplineMeshInstanceData* SplineMeshInstanceData)
{
	if (SplineMeshInstanceData)
	{
		if (bAllowSplineEditingPerInstance)
		{
			SplineParams.StartPos = SplineMeshInstanceData->StartPos;
			SplineParams.EndPos = SplineMeshInstanceData->EndPos;
			SplineParams.StartTangent = SplineMeshInstanceData->StartTangent;
			SplineParams.EndTangent = SplineMeshInstanceData->EndTangent;
			UpdateRenderStateAndCollision();
		}
	}
}


#include "StaticMeshLight.h"
/** */
class FSplineStaticLightingMesh : public FStaticMeshStaticLightingMesh
{
public:

	FSplineStaticLightingMesh(const USplineMeshComponent* InPrimitive,int32 InLODIndex,const TArray<ULightComponent*>& InRelevantLights):
		FStaticMeshStaticLightingMesh(InPrimitive, InLODIndex, InRelevantLights),
		SplineComponent(InPrimitive)
	{
	}

#if WITH_EDITOR
	virtual const struct FSplineMeshParams* GetSplineParameters() const
	{
		return &SplineComponent->SplineParams;
	}
#endif	//WITH_EDITOR

private:
	const USplineMeshComponent* SplineComponent;
};

FStaticMeshStaticLightingMesh* USplineMeshComponent::AllocateStaticLightingMesh(int32 LODIndex, const TArray<ULightComponent*>& InRelevantLights)
{
	return new FSplineStaticLightingMesh(this, LODIndex, InRelevantLights);
}


float USplineMeshComponent::GetTextureStreamingTransformScale() const
{
	float SplineDeformFactor = 1.f;

	if (GetStaticMesh())
	{
		// We do this by looking at the ratio between current bounds (including deformation) and undeformed (straight from staticmesh)
		const float MinExtent = 1.0f;
		FBoxSphereBounds UndeformedBounds = GetStaticMesh()->GetBounds().TransformBy(GetComponentTransform());
		if (UndeformedBounds.BoxExtent.X >= MinExtent)
		{
			SplineDeformFactor = FMath::Max(SplineDeformFactor, Bounds.BoxExtent.X / UndeformedBounds.BoxExtent.X);
		}
		if (UndeformedBounds.BoxExtent.Y >= MinExtent)
		{
			SplineDeformFactor = FMath::Max(SplineDeformFactor, Bounds.BoxExtent.Y / UndeformedBounds.BoxExtent.Y);
		}
		if (UndeformedBounds.BoxExtent.Z >= MinExtent)
		{
			SplineDeformFactor = FMath::Max(SplineDeformFactor, Bounds.BoxExtent.Z / UndeformedBounds.BoxExtent.Z);
		}
	}

	return SplineDeformFactor * Super::GetTextureStreamingTransformScale();
}

#if WITH_EDITOR
void USplineMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UStaticMeshComponent::PostEditChangeProperty(PropertyChangedEvent);
	UProperty* MemberPropertyThatChanged = PropertyChangedEvent.MemberProperty;
	if (MemberPropertyThatChanged)
	{
		// If the spline params were changed the actual geometry is, so flag the owning HLOD cluster as dirty
		if (MemberPropertyThatChanged->GetNameCPP() == TEXT("SplineParams"))
		{
			IHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<IHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
			IHierarchicalLODUtilities* Utilities = Module.GetUtilities();
			Utilities->HandleActorModified(GetOwner());
		}
	}
}
#endif
