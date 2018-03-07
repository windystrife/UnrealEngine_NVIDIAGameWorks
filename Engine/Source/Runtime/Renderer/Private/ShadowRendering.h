// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShadowRendering.h: Shadow rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Templates/RefCounting.h"
#include "RHI.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "HitProxies.h"
#include "ConvexVolume.h"
#include "RHIStaticStates.h"
#include "RendererInterface.h"
#include "SceneManagement.h"
#include "ScenePrivateBase.h"
#include "SceneCore.h"
#include "LightSceneInfo.h"
#include "DrawingPolicy.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "GlobalShader.h"
#include "SystemTextures.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneRenderTargetParameters.h"
#include "ShaderParameterUtils.h"

class FPrimitiveSceneInfo;
class FPrimitiveSceneProxy;
class FProjectedShadowInfo;
class FScene;
class FSceneRenderer;
class FShadowStaticMeshElement;
class FViewInfo;

/** Uniform buffer for rendering deferred lights. */
BEGIN_UNIFORM_BUFFER_STRUCT(FDeferredLightUniformStruct,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector,LightPosition)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,LightInvRadius)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector,LightColor)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,LightFalloffExponent)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector,NormalizedLightDirection)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector,NormalizedLightTangent)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector2D,SpotAngles)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,SourceRadius)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,SoftSourceRadius)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,SourceLength)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,MinRoughness)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,ContactShadowLength)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector2D,DistanceFadeMAD)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4,ShadowMapChannelMask)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32,ShadowedBits)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32,LightingChannelMask)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,VolumetricScatteringIntensity)
END_UNIFORM_BUFFER_STRUCT(FDeferredLightUniformStruct)

extern uint32 GetShadowQuality();

extern float GetLightFadeFactor(const FSceneView& View, const FLightSceneProxy* Proxy);

template<typename ShaderRHIParamRef>
void SetDeferredLightParameters(
	FRHICommandList& RHICmdList, 
	const ShaderRHIParamRef ShaderRHI, 
	const TShaderUniformBufferParameter<FDeferredLightUniformStruct>& DeferredLightUniformBufferParameter, 
	const FLightSceneInfo* LightSceneInfo,
	const FSceneView& View)
{
	FDeferredLightUniformStruct DeferredLightUniformsValue;

	FLightParameters LightParameters;

	LightSceneInfo->Proxy->GetParameters(LightParameters);
	
	DeferredLightUniformsValue.LightPosition = LightParameters.LightPositionAndInvRadius;
	DeferredLightUniformsValue.LightInvRadius = LightParameters.LightPositionAndInvRadius.W;
	DeferredLightUniformsValue.LightColor = LightParameters.LightColorAndFalloffExponent;
	DeferredLightUniformsValue.LightFalloffExponent = LightParameters.LightColorAndFalloffExponent.W;
	DeferredLightUniformsValue.NormalizedLightDirection = LightParameters.NormalizedLightDirection;
	DeferredLightUniformsValue.NormalizedLightTangent = LightParameters.NormalizedLightTangent;
	DeferredLightUniformsValue.SpotAngles = LightParameters.SpotAngles;
	DeferredLightUniformsValue.SourceRadius = LightParameters.LightSourceRadius;
	DeferredLightUniformsValue.SoftSourceRadius = LightParameters.LightSoftSourceRadius;
	DeferredLightUniformsValue.SourceLength = LightParameters.LightSourceLength;
	DeferredLightUniformsValue.MinRoughness = LightParameters.LightMinRoughness;

	const FVector2D FadeParams = LightSceneInfo->Proxy->GetDirectionalLightDistanceFadeParameters(View.GetFeatureLevel(), LightSceneInfo->IsPrecomputedLightingValid(), View.MaxShadowCascades);

	// use MAD for efficiency in the shader
	DeferredLightUniformsValue.DistanceFadeMAD = FVector2D(FadeParams.Y, -FadeParams.X * FadeParams.Y);

	int32 ShadowMapChannel = LightSceneInfo->Proxy->GetShadowMapChannel();

	static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnRenderThread() != 0);

	if (!bAllowStaticLighting)
	{
		ShadowMapChannel = INDEX_NONE;
	}

	DeferredLightUniformsValue.ShadowMapChannelMask = FVector4(
		ShadowMapChannel == 0 ? 1 : 0,
		ShadowMapChannel == 1 ? 1 : 0,
		ShadowMapChannel == 2 ? 1 : 0,
		ShadowMapChannel == 3 ? 1 : 0);

	const bool bDynamicShadows = View.Family->EngineShowFlags.DynamicShadows && GetShadowQuality() > 0;
	const bool bHasLightFunction = LightSceneInfo->Proxy->GetLightFunctionMaterial() != NULL;
	DeferredLightUniformsValue.ShadowedBits  = LightSceneInfo->Proxy->CastsStaticShadow() || bHasLightFunction ? 1 : 0;
	DeferredLightUniformsValue.ShadowedBits |= LightSceneInfo->Proxy->CastsDynamicShadow() && View.Family->EngineShowFlags.DynamicShadows ? 3 : 0;

	DeferredLightUniformsValue.VolumetricScatteringIntensity = LightSceneInfo->Proxy->GetVolumetricScatteringIntensity();

	static auto* ContactShadowsCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ContactShadows"));
	DeferredLightUniformsValue.ContactShadowLength = 0;

	if (ContactShadowsCVar && ContactShadowsCVar->GetValueOnRenderThread() != 0 && View.Family->EngineShowFlags.ContactShadows)
	{
		DeferredLightUniformsValue.ContactShadowLength = LightSceneInfo->Proxy->GetContactShadowLength();
	}

	if( LightSceneInfo->Proxy->IsInverseSquared() )
	{
		// Correction for lumen units
		DeferredLightUniformsValue.LightColor *= 16.0f;
	}

	// When rendering reflection captures, the direct lighting of the light is actually the indirect specular from the main view
	if (View.bIsReflectionCapture)
	{
		DeferredLightUniformsValue.LightColor *= LightSceneInfo->Proxy->GetIndirectLightingScale();
	}

	const ELightComponentType LightType = (ELightComponentType)LightSceneInfo->Proxy->GetLightType();


	if ((LightType == LightType_Point || LightType == LightType_Spot) && View.IsPerspectiveProjection())
		// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
		if (!View.bIsVxgiVoxelization)
#endif
			// NVCHANGE_END: Add VXGI
	{
		DeferredLightUniformsValue.LightColor *= GetLightFadeFactor(View, LightSceneInfo->Proxy);
	}

	DeferredLightUniformsValue.LightingChannelMask = LightSceneInfo->Proxy->GetLightingChannelMask();

	SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI,DeferredLightUniformBufferParameter,DeferredLightUniformsValue);
}

// Forward declarations.
class FProjectedShadowInfo;

/** Utility functions for drawing a sphere */
namespace StencilingGeometry
{
	/**
	* Draws a sphere using RHIDrawIndexedPrimitive, useful as approximate bounding geometry for deferred passes.
	* Note: The sphere will be of unit size unless transformed by the shader. 
	*/
	extern void DrawSphere(FRHICommandList& RHICmdList);
	/**
	 * Draws exactly the same as above, but uses FVector rather than FVector4 vertex data.
	 */
	extern void DrawVectorSphere(FRHICommandList& RHICmdList);
	/** Renders a cone with a spherical cap, used for rendering spot lights in deferred passes. */
	extern void DrawCone(FRHICommandList& RHICmdList);

	/** 
	* Vertex buffer for a sphere of unit size. Used for drawing a sphere as approximate bounding geometry for deferred passes.
	*/
	template<int32 NumSphereSides, int32 NumSphereRings, typename VectorType>
	class TStencilSphereVertexBuffer : public FVertexBuffer
	{
	public:

		int32 GetNumRings() const
		{
			return NumSphereRings;
		}

		/** 
		* Initialize the RHI for this rendering resource 
		*/
		void InitRHI() override
		{
			const int32 NumSides = NumSphereSides;
			const int32 NumRings = NumSphereRings;
			const int32 NumVerts = (NumSides + 1) * (NumRings + 1);

			const float RadiansPerRingSegment = PI / (float)NumRings;
			float Radius = 1;

			TArray<VectorType, TInlineAllocator<NumRings + 1> > ArcVerts;
			ArcVerts.Empty(NumRings + 1);
			// Calculate verts for one arc
			for (int32 i = 0; i < NumRings + 1; i++)
			{
				const float Angle = i * RadiansPerRingSegment;
				ArcVerts.Add(FVector(0.0f, FMath::Sin(Angle), FMath::Cos(Angle)));
			}

			TResourceArray<VectorType, VERTEXBUFFER_ALIGNMENT> Verts;
			Verts.Empty(NumVerts);
			// Then rotate this arc NumSides + 1 times.
			const FVector Center = FVector(0,0,0);
			for (int32 s = 0; s < NumSides + 1; s++)
			{
				FRotator ArcRotator(0, 360.f * ((float)s / NumSides), 0);
				FRotationMatrix ArcRot( ArcRotator );

				for (int32 v = 0; v < NumRings + 1; v++)
				{
					const int32 VIx = (NumRings + 1) * s + v;
					Verts.Add(Center + Radius * ArcRot.TransformPosition(ArcVerts[v]));
				}
			}

			NumSphereVerts = Verts.Num();
			uint32 Size = Verts.GetResourceDataSize();

			// Create vertex buffer. Fill buffer with initial data upon creation
			FRHIResourceCreateInfo CreateInfo(&Verts);
			VertexBufferRHI = RHICreateVertexBuffer(Size,BUF_Static,CreateInfo);
		}

		int32 GetVertexCount() const { return NumSphereVerts; }

	/** 
	* Calculates the world transform for a sphere.
	* @param OutTransform - The output world transform.
	* @param Sphere - The sphere to generate the transform for.
	* @param PreViewTranslation - The pre-view translation to apply to the transform.
	* @param bConservativelyBoundSphere - when true, the sphere that is drawn will contain all positions in the analytical sphere,
	*		 Otherwise the sphere vertices will lie on the analytical sphere and the positions on the faces will lie inside the sphere.
	*/
	void CalcTransform(FVector4& OutPosAndScale, const FSphere& Sphere, const FVector& PreViewTranslation, bool bConservativelyBoundSphere = true)
	{
		float Radius = Sphere.W;
		if (bConservativelyBoundSphere)
		{
			const int32 NumRings = NumSphereRings;
			const float RadiansPerRingSegment = PI / (float)NumRings;

			// Boost the effective radius so that the edges of the sphere approximation lie on the sphere, instead of the vertices
			Radius /= FMath::Cos(RadiansPerRingSegment);
		}

		const FVector Translate(Sphere.Center + PreViewTranslation);
		OutPosAndScale = FVector4(Translate, Radius);
	}

	private:
		int32 NumSphereVerts;
	};

	/** 
	* Stenciling sphere index buffer
	*/
	template<int32 NumSphereSides, int32 NumSphereRings>
	class TStencilSphereIndexBuffer : public FIndexBuffer
	{
	public:
		/** 
		* Initialize the RHI for this rendering resource 
		*/
		void InitRHI() override
		{
			const int32 NumSides = NumSphereSides;
			const int32 NumRings = NumSphereRings;
			TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> Indices;

			// Add triangles for all the vertices generated
			for (int32 s = 0; s < NumSides; s++)
			{
				const int32 a0start = (s + 0) * (NumRings + 1);
				const int32 a1start = (s + 1) * (NumRings + 1);

				for (int32 r = 0; r < NumRings; r++)
				{
					Indices.Add(a0start + r + 0);
					Indices.Add(a1start + r + 0);
					Indices.Add(a0start + r + 1);
					Indices.Add(a1start + r + 0);
					Indices.Add(a1start + r + 1);
					Indices.Add(a0start + r + 1);
				}
			}

			NumIndices = Indices.Num();
			const uint32 Size = Indices.GetResourceDataSize();
			const uint32 Stride = sizeof(uint16);

			// Create index buffer. Fill buffer with initial data upon creation
			FRHIResourceCreateInfo CreateInfo(&Indices);
			IndexBufferRHI = RHICreateIndexBuffer(Stride, Size, BUF_Static, CreateInfo);
		}

		int32 GetIndexCount() const { return NumIndices; }; 
	
	private:
		int32 NumIndices;
	};

	class FStencilConeIndexBuffer : public FIndexBuffer
	{
	public:
		// A side is a line of vertices going from the cone's origin to the edge of its SphereRadius
		static const int32 NumSides = 18;
		// A slice is a circle of vertices in the cone's XY plane
		static const int32 NumSlices = 12;

		static const uint32 NumVerts = NumSides * NumSlices * 2;

		void InitRHI() override
		{
			TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> Indices;

			Indices.Empty((NumSlices - 1) * NumSides * 12);
			// Generate triangles for the vertices of the cone shape
			for (int32 SliceIndex = 0; SliceIndex < NumSlices - 1; SliceIndex++)
			{
				for (int32 SideIndex = 0; SideIndex < NumSides; SideIndex++)
				{
					const int32 CurrentIndex = SliceIndex * NumSides + SideIndex % NumSides;
					const int32 NextSideIndex = SliceIndex * NumSides + (SideIndex + 1) % NumSides;
					const int32 NextSliceIndex = (SliceIndex + 1) * NumSides + SideIndex % NumSides;
					const int32 NextSliceAndSideIndex = (SliceIndex + 1) * NumSides + (SideIndex + 1) % NumSides;

					Indices.Add(CurrentIndex);
					Indices.Add(NextSideIndex);
					Indices.Add(NextSliceIndex);
					Indices.Add(NextSliceIndex);
					Indices.Add(NextSideIndex);
					Indices.Add(NextSliceAndSideIndex);
				}
			}

			// Generate triangles for the vertices of the spherical cap
			const int32 CapIndexStart = NumSides * NumSlices;

			for (int32 SliceIndex = 0; SliceIndex < NumSlices - 1; SliceIndex++)
			{
				for (int32 SideIndex = 0; SideIndex < NumSides; SideIndex++)
				{
					const int32 CurrentIndex = SliceIndex * NumSides + SideIndex % NumSides + CapIndexStart;
					const int32 NextSideIndex = SliceIndex * NumSides + (SideIndex + 1) % NumSides + CapIndexStart;
					const int32 NextSliceIndex = (SliceIndex + 1) * NumSides + SideIndex % NumSides + CapIndexStart;
					const int32 NextSliceAndSideIndex = (SliceIndex + 1) * NumSides + (SideIndex + 1) % NumSides + CapIndexStart;

					Indices.Add(CurrentIndex);
					Indices.Add(NextSliceIndex);
					Indices.Add(NextSideIndex);
					Indices.Add(NextSideIndex);
					Indices.Add(NextSliceIndex);
					Indices.Add(NextSliceAndSideIndex);
				}
			}

			const uint32 Size = Indices.GetResourceDataSize();
			const uint32 Stride = sizeof(uint16);

			NumIndices = Indices.Num();

			// Create index buffer. Fill buffer with initial data upon creation
			FRHIResourceCreateInfo CreateInfo(&Indices);
			IndexBufferRHI = RHICreateIndexBuffer(Stride, Size, BUF_Static,CreateInfo);
		}

		int32 GetIndexCount() const { return NumIndices; } 

	protected:
		int32 NumIndices;
	};

	/** 
	* Vertex buffer for a cone. It holds zero'd out data since the actual math is done on the shader
	*/
	class FStencilConeVertexBuffer : public FVertexBuffer
	{
	public:
		static const int32 NumVerts = FStencilConeIndexBuffer::NumSides * FStencilConeIndexBuffer::NumSlices * 2;

		/** 
		* Initialize the RHI for this rendering resource 
		*/
		void InitRHI() override
		{
			TResourceArray<FVector4, VERTEXBUFFER_ALIGNMENT> Verts;
			Verts.Empty(NumVerts);
			for (int32 s = 0; s < NumVerts; s++)
			{
				Verts.Add(FVector4(0, 0, 0, 0));
			}

			uint32 Size = Verts.GetResourceDataSize();

			// Create vertex buffer. Fill buffer with initial data upon creation
			FRHIResourceCreateInfo CreateInfo(&Verts);
			VertexBufferRHI = RHICreateVertexBuffer(Size, BUF_Static, CreateInfo);
		}

		int32 GetVertexCount() const { return NumVerts; }
	};

	extern TGlobalResource<TStencilSphereVertexBuffer<18, 12, FVector4> > GStencilSphereVertexBuffer;
	extern TGlobalResource<TStencilSphereVertexBuffer<18, 12, FVector> > GStencilSphereVectorBuffer;
	extern TGlobalResource<TStencilSphereIndexBuffer<18, 12> > GStencilSphereIndexBuffer;
	extern TGlobalResource<TStencilSphereVertexBuffer<4, 4, FVector4> > GLowPolyStencilSphereVertexBuffer;
	extern TGlobalResource<TStencilSphereIndexBuffer<4, 4> > GLowPolyStencilSphereIndexBuffer;
	extern TGlobalResource<FStencilConeVertexBuffer> GStencilConeVertexBuffer;
	extern TGlobalResource<FStencilConeIndexBuffer> GStencilConeIndexBuffer;

}; //End StencilingGeometry



/** Renders a cone with a spherical cap, used for rendering spot lights in deferred passes. */
extern void DrawStencilingCone(const FMatrix& ConeToWorld, float ConeAngle, float SphereRadius, const FVector& PreViewTranslation);

template <bool bRenderingReflectiveShadowMaps> class TShadowDepthBasePS;
class FShadowStaticMeshElement;

/**
 * The shadow depth drawing policy's context data.
 */
struct FShadowDepthDrawingPolicyContext : FMeshDrawingPolicy::ContextDataType
{
	/** CAUTION, this is assumed to be a POD type. We allocate the on the scene allocator and NEVER CALL A DESTRUCTOR.
		If you want to add non-pod data, not a huge problem, we just need to track and destruct them at the end of the scene.
	**/
	/** The projected shadow info for which we are rendering shadow depths. */
	const FProjectedShadowInfo* ShadowInfo;

	/** Initialization constructor. */
	explicit FShadowDepthDrawingPolicyContext(const FProjectedShadowInfo* InShadowInfo)
		: ShadowInfo(InShadowInfo)
	{}
};

/**
 * Outputs no color, but can be used to write the mesh's depth values to the depth buffer.
 */
template <bool bRenderingReflectiveShadowMaps>
class FShadowDepthDrawingPolicy : public FMeshDrawingPolicy
{
public:
	typedef FShadowDepthDrawingPolicyContext ContextDataType;

	FShadowDepthDrawingPolicy(
		const FMaterial* InMaterialResource,
		bool bInDirectionalLight,
		bool bInOnePassPointLightShadow,
		bool bInPreShadow,
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
		ERHIFeatureLevel::Type InFeatureLevel,
		const FVertexFactory* InVertexFactory = 0,
		const FMaterialRenderProxy* InMaterialRenderProxy = 0,
		bool bReverseCulling = false
		);

	void UpdateElementState(FShadowStaticMeshElement& State, ERHIFeatureLevel::Type FeatureLevel);

	FShadowDepthDrawingPolicy& operator = (const FShadowDepthDrawingPolicy& Other)
	{ 
		VertexShader = Other.VertexShader;
		GeometryShader = Other.GeometryShader;
		HullShader = Other.HullShader;
		DomainShader = Other.DomainShader;
		PixelShader = Other.PixelShader;
		bDirectionalLight = Other.bDirectionalLight;
		bReverseCulling = Other.bReverseCulling;
		bOnePassPointLightShadow = Other.bOnePassPointLightShadow;
		bUsePositionOnlyVS = Other.bUsePositionOnlyVS;
		bPreShadow = Other.bPreShadow;
		FeatureLevel = Other.FeatureLevel;
		(FMeshDrawingPolicy&)*this = (const FMeshDrawingPolicy&)Other;
		return *this; 
	}

	//~ Begin FMeshDrawingPolicy Interface.
	FDrawingPolicyMatchResult Matches(const FShadowDepthDrawingPolicy& Other) const
	{
		DRAWING_POLICY_MATCH_BEGIN
			DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other)) && 
			DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
			DRAWING_POLICY_MATCH(GeometryShader == Other.GeometryShader) &&
			DRAWING_POLICY_MATCH(HullShader == Other.HullShader) &&
			DRAWING_POLICY_MATCH(DomainShader == Other.DomainShader) &&
			DRAWING_POLICY_MATCH(PixelShader == Other.PixelShader) &&
			DRAWING_POLICY_MATCH(bDirectionalLight == Other.bDirectionalLight) &&
			DRAWING_POLICY_MATCH(bReverseCulling == Other.bReverseCulling) &&
			DRAWING_POLICY_MATCH(bOnePassPointLightShadow == Other.bOnePassPointLightShadow) &&
			DRAWING_POLICY_MATCH(bUsePositionOnlyVS == Other.bUsePositionOnlyVS) &&
			DRAWING_POLICY_MATCH(bPreShadow == Other.bPreShadow) &&
			DRAWING_POLICY_MATCH(FeatureLevel == Other.FeatureLevel);
		DRAWING_POLICY_MATCH_END
	}
	void SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FSceneView* View, const ContextDataType PolicyContext) const;

	/** 
	 * Create bound shader state using the vertex decl from the mesh draw policy
	 * as well as the shaders needed to draw the mesh
	 * @return new bound shader state object
	 */
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const;

	void SetMeshRenderState(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		const FDrawingPolicyRenderState& DrawRenderState,
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
		) const;

	template<bool T2>
	friend int32 CompareDrawingPolicy(const FShadowDepthDrawingPolicy<T2>& A,const FShadowDepthDrawingPolicy<T2>& B);

	bool IsReversingCulling() const
	{
		return bReverseCulling;
	}
	
	/**
	  * Executes the draw commands for a mesh.
	  */
	void DrawMesh(FRHICommandList& RHICmdList, const FMeshBatch& Mesh, int32 BatchElementIndex, const bool bIsInstancedStereo = false) const;

private:

	class FShadowDepthVS* VertexShader;
	class FOnePassPointShadowDepthGS* GeometryShader;
	TShadowDepthBasePS<bRenderingReflectiveShadowMaps>* PixelShader;
	class FBaseHS* HullShader;
	class FShadowDepthDS* DomainShader;
	ERHIFeatureLevel::Type FeatureLevel;

public:

	uint32 bDirectionalLight:1;
	uint32 bReverseCulling:1;
	uint32 bOnePassPointLightShadow:1;
	uint32 bUsePositionOnlyVS:1;
	uint32 bPreShadow:1;
};

/**
 * A drawing policy factory for the shadow depth drawing policy.
 */
class FShadowDepthDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = false };

	struct ContextType
	{
		const FProjectedShadowInfo* ShadowInfo;

		ContextType(const FProjectedShadowInfo* InShadowInfo) :
			ShadowInfo(InShadowInfo)
		{}
	};

	static void AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh);

	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		ContextType Context,
		const FMeshBatch& Mesh,
		bool bPreFog,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);
};

/** 
 * Overrides a material used for shadow depth rendering with the default material when appropriate.
 * Overriding in this manner can reduce state switches and the number of shaders that have to be compiled.
 * This logic needs to stay in sync with shadow depth shader ShouldCache logic.
 */
void OverrideWithDefaultMaterialForShadowDepth(
	const FMaterialRenderProxy*& InOutMaterialRenderProxy, 
	const FMaterial*& InOutMaterialResource,
	bool bReflectiveShadowmap,
	ERHIFeatureLevel::Type InFeatureLevel
	);

/** A single static mesh element for shadow depth rendering. */
class FShadowStaticMeshElement
{
public:

	FShadowStaticMeshElement()
		: RenderProxy(0)
		, MaterialResource(0)
		, Mesh(0)
		, bIsTwoSided(false)
	{
	}

	FShadowStaticMeshElement(const FMaterialRenderProxy* InRenderProxy, const FMaterial* InMaterialResource, const FStaticMesh* InMesh, bool bInIsTwoSided) :
		RenderProxy(InRenderProxy),
		MaterialResource(InMaterialResource),
		Mesh(InMesh),
		bIsTwoSided(bInIsTwoSided)
	{}

	bool DoesDeltaRequireADrawSharedCall(const FShadowStaticMeshElement& rhs) const
	{
		checkSlow(rhs.RenderProxy);
		checkSlow(rhs.Mesh);

		// Note: this->RenderProxy or this->Mesh can be 0
		// but in this case rhs.RenderProxy should not be 0
		// so it will early out and there will be no crash on Mesh->VertexFactory
		checkSlow(!RenderProxy || rhs.RenderProxy);

		return RenderProxy != rhs.RenderProxy
			|| bIsTwoSided != rhs.bIsTwoSided
			|| Mesh->VertexFactory != rhs.Mesh->VertexFactory
			|| Mesh->ReverseCulling != rhs.Mesh->ReverseCulling;
	}

	/** Store the FMaterialRenderProxy pointer since it may be different from the one that FStaticMesh stores. */
	const FMaterialRenderProxy* RenderProxy;
	const FMaterial* MaterialResource;
	const FStaticMesh* Mesh;
	bool bIsTwoSided;
};

enum EShadowDepthRenderMode
{
	/** The render mode used by regular shadows */
	ShadowDepthRenderMode_Normal,

	/** The render mode used when injecting emissive-only objects into the RSM. */
	ShadowDepthRenderMode_EmissiveOnly,

	/** The render mode used when rendering volumes which block global illumination. */
	ShadowDepthRenderMode_GIBlockingVolumes,
};

enum EShadowDepthCacheMode
{
	SDCM_MovablePrimitivesOnly,
	SDCM_StaticPrimitivesOnly,
	SDCM_Uncached
};

inline bool IsShadowCacheModeOcclusionQueryable(EShadowDepthCacheMode CacheMode)
{
	// SDCM_StaticPrimitivesOnly shadowmaps are emitted randomly as the cache needs to be updated,
	// And therefore not appropriate for occlusion queries which are latent and therefore need to be stable.
	// Only one the cache modes from ComputeWholeSceneShadowCacheModes should be queryable
	return CacheMode != SDCM_StaticPrimitivesOnly;
}

class FShadowMapRenderTargets
{
public:
	TArray<IPooledRenderTarget*, SceneRenderingAllocator> ColorTargets;
	IPooledRenderTarget* DepthTarget;

	FShadowMapRenderTargets() :
		DepthTarget(NULL)
	{}

	FIntPoint GetSize() const
	{
		if (DepthTarget)
		{
			return DepthTarget->GetDesc().Extent;
		}
		else 
		{
			check(ColorTargets.Num() > 0);
			return ColorTargets[0]->GetDesc().Extent;
		}
	}
};

typedef TFunctionRef<void(FRHICommandList& RHICmdList, bool bFirst)> FSetShadowRenderTargetFunction;

/**
 * Information about a projected shadow.
 */
class FProjectedShadowInfo : public FRefCountedObject
{
public:
	typedef TArray<const FPrimitiveSceneInfo*,SceneRenderingAllocator> PrimitiveArrayType;

	/** The view to be used when rendering this shadow's depths. */
	FViewInfo* ShadowDepthView;

	/** The depth or color targets this shadow was rendered to. */
	FShadowMapRenderTargets RenderTargets;

	EShadowDepthCacheMode CacheMode;

	/** The main view this shadow must be rendered in, or NULL for a view independent shadow. */
	FViewInfo* DependentView;

	/** Index of the shadow into FVisibleLightInfo::AllProjectedShadows. */
	int32 ShadowId;

	/** A translation that is applied to world-space before transforming by one of the shadow matrices. */
	FVector PreShadowTranslation;

	/** The effective view matrix of the shadow, used as an override to the main view's view matrix when rendering the shadow depth pass. */
	FMatrix ShadowViewMatrix;

	/** 
	 * Matrix used for rendering the shadow depth buffer.  
	 * Note that this does not necessarily contain all of the shadow casters with CSM, since the vertex shader flattens them onto the near plane of the projection.
	 */
	FMatrix SubjectAndReceiverMatrix;
	FMatrix ReceiverMatrix;

	FMatrix InvReceiverMatrix;

	float InvMaxSubjectDepth;

	/** 
	 * Subject depth extents, in world space units. 
	 * These can be used to convert shadow depth buffer values back into world space units.
	 */
	float MaxSubjectZ;
	float MinSubjectZ;

	/** Frustum containing all potential shadow casters. */
	FConvexVolume CasterFrustum;
	FConvexVolume ReceiverFrustum;

	float MinPreSubjectZ;

	FSphere ShadowBounds;

	FShadowCascadeSettings CascadeSettings;

	/** 
	 * X and Y position of the shadow in the appropriate depth buffer.  These are only initialized after the shadow has been allocated. 
	 * The actual contents of the shadowmap are at X + BorderSize, Y + BorderSize.
	 */
	uint32 X;
	uint32 Y;

	/** 
	 * Resolution of the shadow, excluding the border. 
	 * The full size of the region allocated to this shadow is therefore ResolutionX + 2 * BorderSize, ResolutionY + 2 * BorderSize.
	 */
	uint32 ResolutionX;
	uint32 ResolutionY;

	/** Size of the border, if any, used to allow filtering without clamping for shadows stored in an atlas. */
	uint32 BorderSize;

	/** The largest percent of either the width or height of any view. */
	float MaxScreenPercent;

	/** Fade Alpha per view. */
	TArray<float, TInlineAllocator<2> > FadeAlphas;

	/** Whether the shadow has been allocated in the shadow depth buffer, and its X and Y properties have been initialized. */
	uint32 bAllocated : 1;

	/** Whether the shadow's projection has been rendered. */
	uint32 bRendered : 1;

	/** Whether the shadow has been allocated in the preshadow cache, so its X and Y properties offset into the preshadow cache depth buffer. */
	uint32 bAllocatedInPreshadowCache : 1;

	/** Whether the shadow is in the preshadow cache and its depths are up to date. */
	uint32 bDepthsCached : 1;

	// redundant to LightSceneInfo->Proxy->GetLightType() == LightType_Directional, could be made ELightComponentType LightType
	uint32 bDirectionalLight : 1;

	/** Whether the shadow is a point light shadow that renders all faces of a cubemap in one pass. */
	uint32 bOnePassPointLightShadow : 1;

	/** Whether this shadow affects the whole scene or only a group of objects. */
	uint32 bWholeSceneShadow : 1;

	/** Whether the shadow needs to render reflective shadow maps. */ 
	uint32 bReflectiveShadowmap : 1; 

	/** Whether this shadow should support casting shadows from translucent surfaces. */
	uint32 bTranslucentShadow : 1;

	/** Whether the shadow will be computed by ray tracing the distance field. */
	uint32 bRayTracedDistanceField : 1;

	/** Whether this is a per-object shadow that should use capsule shapes to shadow instead of the mesh's triangles. */
	uint32 bCapsuleShadow : 1;

	/** Whether the shadow is a preshadow or not.  A preshadow is a per object shadow that handles the static environment casting on a dynamic receiver. */
	uint32 bPreShadow : 1;

	/** To not cast a shadow on the ground outside the object and having higher quality (useful for first person weapon). */
	uint32 bSelfShadowOnly : 1;

	/** Whether the shadow is a per object shadow or not. */
	uint32 bPerObjectOpaqueShadow : 1;

	TBitArray<SceneRenderingBitArrayAllocator> StaticMeshWholeSceneShadowDepthMap;
	TArray<uint64,SceneRenderingAllocator> StaticMeshWholeSceneShadowBatchVisibility;

	/** View projection matrices for each cubemap face, used by one pass point light shadows. */
	TArray<FMatrix> OnePassShadowViewProjectionMatrices;

	/** Frustums for each cubemap face, used for object culling one pass point light shadows. */
	TArray<FConvexVolume> OnePassShadowFrustums;

	/** Data passed from async compute begin to end. */
	FComputeFenceRHIRef RayTracedShadowsEndFence;
	TRefCountPtr<IPooledRenderTarget> RayTracedShadowsRT;

public:

	// default constructor
	FProjectedShadowInfo();

	/**
	 * for a per-object shadow. e.g. translucent particle system or a dynamic object in a precomputed shadow situation
	 * @param InParentSceneInfo must not be 0
	 * @return success, if false the shadow project is invalid and the projection should nto be created
	 */
	bool SetupPerObjectProjection(
		FLightSceneInfo* InLightSceneInfo,
		const FPrimitiveSceneInfo* InParentSceneInfo,
		const FPerObjectProjectedShadowInitializer& Initializer,
		bool bInPreShadow,
		uint32 InResolutionX,
		uint32 MaxShadowResolutionY,
		uint32 InBorderSize,
		float InMaxScreenPercent,
		bool bInTranslucentShadow
		);

	/** for a whole-scene shadow. */
	void SetupWholeSceneProjection(
		FLightSceneInfo* InLightSceneInfo,
		FViewInfo* InDependentView,
		const FWholeSceneProjectedShadowInitializer& Initializer,
		uint32 InResolutionX,
		uint32 InResolutionY,
		uint32 InBorderSize,
		bool bInReflectiveShadowMap
		);

	float GetShaderDepthBias() const { return ShaderDepthBias; }

	/**
	 * Renders the shadow subject depth.
	 */
	void RenderDepth(FRHICommandList& RHICmdList, class FSceneRenderer* SceneRenderer, FSetShadowRenderTargetFunction SetShadowRenderTargets, EShadowDepthRenderMode RenderMode);

	/** Set state for depth rendering */
	void SetStateForDepth(FRHICommandList& RHICmdList, EShadowDepthRenderMode RenderMode, FDrawingPolicyRenderState& DrawRenderState);

	void ClearDepth(FRHICommandList& RHICmdList, class FSceneRenderer* SceneRenderer, int32 NumColorTextures, FTextureRHIParamRef* ColorTextures, FTextureRHIParamRef DepthTexture, bool bPerformClear);

	/** Renders shadow maps for translucent primitives. */
	void RenderTranslucencyDepths(FRHICommandList& RHICmdList, class FSceneRenderer* SceneRenderer);

	static void SetBlendStateForProjection(
		FGraphicsPipelineStateInitializer& GraphicsPSOInit,
		int32 ShadowMapChannel,
		bool bIsWholeSceneDirectionalShadow,
		bool bUseFadePlane,
		bool bProjectingForForwardShading,
		bool bMobileModulatedProjections);

	void SetBlendStateForProjection(FGraphicsPipelineStateInitializer& GraphicsPSOInit, bool bProjectingForForwardShading, bool bMobileModulatedProjections) const;

	/**
	 * Projects the shadow onto the scene for a particular view.
	 */
	void RenderProjection(FRHICommandListImmediate& RHICmdList, int32 ViewIndex, const class FViewInfo* View, bool bProjectingForForwardShading, bool bMobile
		// @third party code - BEGIN HairWorks
		, bool bHairPass
		// @third party code - END HairWorks
		) const;

	void BeginRenderRayTracedDistanceFieldProjection(FRHICommandListImmediate& RHICmdList, const FViewInfo& View);

	/** Renders ray traced distance field shadows. */
	void RenderRayTracedDistanceFieldProjection(FRHICommandListImmediate& RHICmdList, const class FViewInfo& View, IPooledRenderTarget* ScreenShadowMaskTexture, bool bProjectingForForwardShading);

	/** Render one pass point light shadow projections. */
	void RenderOnePassPointLightProjection(FRHICommandListImmediate& RHICmdList, int32 ViewIndex, const FViewInfo& View, bool bProjectingForForwardShading) const;

	/**
	 * Renders the projected shadow's frustum wireframe with the given FPrimitiveDrawInterface.
	 */
	void RenderFrustumWireframe(FPrimitiveDrawInterface* PDI) const;

	/**
	 * Adds a primitive to the shadow's subject list.
	 */
	void AddSubjectPrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo, TArray<FViewInfo>* ViewArray, ERHIFeatureLevel::Type FeatureLevel, bool bRecordShadowSubjectForMobileShading);

	/**
	* @return TRUE if this shadow info has any casting subject prims to render
	*/
	bool HasSubjectPrims() const;

	/**
	 * Adds a primitive to the shadow's receiver list.
	 */
	void AddReceiverPrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/** Gathers dynamic mesh elements for all the shadow's primitives arrays. */
	void GatherDynamicMeshElements(FSceneRenderer& Renderer, class FVisibleLightInfo& VisibleLightInfo, TArray<const FSceneView*>& ReusedViewsArray);

	/** 
	 * @param View view to check visibility in
	 * @return true if this shadow info has any subject prims visible in the view
	 */
	bool SubjectsVisible(const FViewInfo& View) const;

	/** Clears arrays allocated with the scene rendering allocator. */
	void ClearTransientArrays();
	
	/** Hash function. */
	friend uint32 GetTypeHash(const FProjectedShadowInfo* ProjectedShadowInfo)
	{
		return PointerHash(ProjectedShadowInfo);
	}

	/** Returns a matrix that transforms a screen space position into shadow space. */
	FMatrix GetScreenToShadowMatrix(const FSceneView& View) const
	{
		return GetScreenToShadowMatrix(View, X, Y, ResolutionX, ResolutionY);
	}

	/** Returns a matrix that transforms a screen space position into shadow space. 
		Additional parameters allow overriding of shadow's tile location.
		Used with modulated shadows to reduce precision problems when calculating ScreenToShadow in pixel shader.
	*/
	FMatrix GetScreenToShadowMatrix(const FSceneView& View, uint32 TileOffsetX, uint32 TileOffsetY, uint32 TileResolutionX, uint32 TileResolutionY) const;

	/** Returns a matrix that transforms a world space position into shadow space. */
	FMatrix GetWorldToShadowMatrix(FVector4& ShadowmapMinMax, const FIntPoint* ShadowBufferResolutionOverride = nullptr) const;

	/** Returns the resolution of the shadow buffer used for this shadow, based on the shadow's type. */
	FIntPoint GetShadowBufferResolution() const
	{
		return RenderTargets.GetSize();
	}

	/** Computes and updates ShaderDepthBias */
	void UpdateShaderDepthBias();
	/** How large the soft PCF comparison should be, similar to DepthBias, before this was called TransitionScale and 1/Size */
	float ComputeTransitionSize() const;

	inline bool IsWholeSceneDirectionalShadow() const 
	{ 
		return bWholeSceneShadow && CascadeSettings.ShadowSplitIndex >= 0 && bDirectionalLight; 
	}

	inline bool IsWholeScenePointLightShadow() const
	{
		return bWholeSceneShadow && LightSceneInfo->Proxy->GetLightType() == LightType_Point;
	}

	/** Sorts StaticSubjectMeshElements based on state so that rendering the static elements will set as little state as possible. */
	void SortSubjectMeshElements();

	// 0 if Setup...() wasn't called yet
	const FLightSceneInfo& GetLightSceneInfo() const { return *LightSceneInfo; }
	const FLightSceneInfoCompact& GetLightSceneInfoCompact() const { return LightSceneInfoCompact; }
	/**
	 * Parent primitive of the shadow group that created this shadow, if not a bWholeSceneShadow.
	 * 0 if Setup...() wasn't called yet
	 */	
	const FPrimitiveSceneInfo* GetParentSceneInfo() const { return ParentSceneInfo; }

	/** Creates a new view from the pool and caches it in ShadowDepthView for depth rendering. */
	void SetupShadowDepthView(FRHICommandListImmediate& RHICmdList, FSceneRenderer* SceneRenderer);

	// @third party code - BEGIN HairWorks
	bool ShouldRenderForHair(const FViewInfo& View)const;
	// @third party code - END HairWorks
private:
	// 0 if Setup...() wasn't called yet
	const FLightSceneInfo* LightSceneInfo;
	FLightSceneInfoCompact LightSceneInfoCompact;

	/**
	 * Parent primitive of the shadow group that created this shadow, if not a bWholeSceneShadow.
	 * 0 if Setup...() wasn't called yet or for whole scene shadows
	 */	
	const FPrimitiveSceneInfo* ParentSceneInfo;

	/** dynamic shadow casting elements */
	PrimitiveArrayType DynamicSubjectPrimitives;
	/** For preshadows, this contains the receiver primitives to mask the projection to. */
	PrimitiveArrayType ReceiverPrimitives;
	/** Subject primitives with translucent relevance. */
	PrimitiveArrayType SubjectTranslucentPrimitives;

	/** Translucent LPV injection: dynamic shadow casting elements */
	PrimitiveArrayType EmissiveOnlyPrimitives;
	/** Translucent LPV injection: Static shadow casting elements. */
	TArray<FShadowStaticMeshElement,SceneRenderingAllocator> EmissiveOnlyMeshElements;

	/** GI blocking volume: dynamic shadow casting elements */
	PrimitiveArrayType GIBlockingPrimitives;
	/** GI blocking volume: Static shadow casting elements. */
	TArray<FShadowStaticMeshElement,SceneRenderingAllocator> GIBlockingMeshElements;

	/** Static shadow casting elements. */
	TArray<FShadowStaticMeshElement,SceneRenderingAllocator> StaticSubjectMeshElements;

	/** Dynamic mesh elements for subject primitives. */
	TArray<FMeshBatchAndRelevance,SceneRenderingAllocator> DynamicSubjectMeshElements;
	/** Dynamic mesh elements for receiver primitives. */
	TArray<FMeshBatchAndRelevance,SceneRenderingAllocator> DynamicReceiverMeshElements;
	/** Dynamic mesh elements for translucent subject primitives. */
	TArray<FMeshBatchAndRelevance,SceneRenderingAllocator> DynamicSubjectTranslucentMeshElements;

	/**
	 * Bias during in shadowmap rendering, stored redundantly for better performance 
	 * Set by UpdateShaderDepthBias(), get with GetShaderDepthBias(), -1 if not set
	 */
	float ShaderDepthBias;

	void CopyCachedShadowMap(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, FSceneRenderer* SceneRenderer, const FViewInfo& View, FSetShadowRenderTargetFunction SetShadowRenderTargets);

	/**
	* Renders the shadow subject depth, to a particular hacked view
	*/
	void RenderDepthInner(FRHICommandList& RHICmdList, class FSceneRenderer* SceneRenderer, const FViewInfo* FoundView, FSetShadowRenderTargetFunction SetShadowRenderTargets, EShadowDepthRenderMode RenderMode );

	/**
	* Modifies the passed in view for this shadow
	*/
	void ModifyViewForShadow(FRHICommandList& RHICmdList, FViewInfo* FoundView) const;

	/**
	* Finds a relevant view for a shadow
	*/
	FViewInfo* FindViewForShadow(FSceneRenderer* SceneRenderer) const;

	/**
	* Renders the dynamic shadow subject depth, to a particular hacked view
	*/
	friend class FRenderDepthDynamicThreadTask;
	void RenderDepthDynamic(FRHICommandList& RHICmdList, class FSceneRenderer* SceneRenderer, const FViewInfo* FoundView, const FDrawingPolicyRenderState& DrawRenderState);

	void GetShadowTypeNameForDrawEvent(FString& TypeName) const;

	template <bool bReflectiveShadowmap> friend void DrawShadowMeshElements(FRHICommandList& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, const FProjectedShadowInfo& ShadowInfo);

	/** Updates object buffers needed by ray traced distance field shadows. */
	int32 UpdateShadowCastingObjectBuffers() const;

	/** Gathers dynamic mesh elements for the given primitive array. */
	void GatherDynamicMeshElementsArray(
		FViewInfo* FoundView,
		FSceneRenderer& Renderer, 
		PrimitiveArrayType& PrimitiveArray, 
		TArray<FMeshBatchAndRelevance,SceneRenderingAllocator>& OutDynamicMeshElements, 
		TArray<const FSceneView*>& ReusedViewsArray);

	void SetupFrustumForProjection(const FViewInfo* View, TArray<FVector4, TInlineAllocator<8>>& OutFrustumVertices, bool& bOutCameraInsideShadowFrustum) const;

	void SetupProjectionStencilMask(
		FRHICommandListImmediate& RHICmdList,
		const FViewInfo* View,
		const TArray<FVector4, TInlineAllocator<8>>& FrustumVertices,
		bool bMobileModulatedProjections,
		bool bCameraInsideShadowFrustum
		// @third party code - BEGIN HairWorks
		, bool bHairPass
		// @third party code - END HairWorks
	) const;

	friend class FShadowDepthVS;
	template <bool bRenderingReflectiveShadowMaps> friend class TShadowDepthBasePS;
	friend class FShadowVolumeBoundProjectionVS;
	friend class FShadowProjectionPS;
	friend class FShadowDepthDrawingPolicyFactory;
};

/** Shader parameters for rendering the depth of a mesh for shadowing. */
class FShadowDepthShaderParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		ProjectionMatrix.Bind(ParameterMap,TEXT("ProjectionMatrix"));
		ShadowParams.Bind(ParameterMap,TEXT("ShadowParams"));
		ClampToNearPlane.Bind(ParameterMap,TEXT("bClampToNearPlane"));
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, ShaderRHIParamRef ShaderRHI, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo, const FMaterialRenderProxy* MaterialRenderProxy)
	{
		SetShaderValue(
			RHICmdList, 
			ShaderRHI,
			ProjectionMatrix,
			FTranslationMatrix(ShadowInfo->PreShadowTranslation - View.ViewMatrices.GetPreViewTranslation()) * ShadowInfo->SubjectAndReceiverMatrix
			);

		SetShaderValue(RHICmdList, ShaderRHI, ShadowParams, FVector2D(ShadowInfo->GetShaderDepthBias(), ShadowInfo->InvMaxSubjectDepth));
		// Only clamp vertices to the near plane when rendering whole scene directional light shadow depths or preshadows from directional lights
		const bool bClampToNearPlaneValue = ShadowInfo->IsWholeSceneDirectionalShadow() || (ShadowInfo->bPreShadow && ShadowInfo->bDirectionalLight);
		SetShaderValue(RHICmdList, ShaderRHI,ClampToNearPlane,bClampToNearPlaneValue ? 1.0f : 0.0f);
	}

	/** Set the vertex shader parameter values. */
	void SetVertexShader(FRHICommandList& RHICmdList, FShader* VertexShader, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo, const FMaterialRenderProxy* MaterialRenderProxy)
	{
		Set(RHICmdList, VertexShader->GetVertexShader(), View, ShadowInfo, MaterialRenderProxy);
	}

	/** Set the domain shader parameter values. */
	void SetDomainShader(FRHICommandList& RHICmdList, FShader* DomainShader, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo, const FMaterialRenderProxy* MaterialRenderProxy)
	{
		Set(RHICmdList, DomainShader->GetDomainShader(), View, ShadowInfo, MaterialRenderProxy);
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FShadowDepthShaderParameters& P)
	{
		Ar << P.ProjectionMatrix;
		Ar << P.ShadowParams;
		Ar << P.ClampToNearPlane;
		return Ar;
	}

private:
	FShaderParameter ProjectionMatrix;
	FShaderParameter ShadowParams;
	FShaderParameter ClampToNearPlane;
};

/** 
* Stencil geometry parameters used by multiple shaders. 
*/
class FStencilingGeometryShaderParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		StencilGeometryPosAndScale.Bind(ParameterMap, TEXT("StencilingGeometryPosAndScale"));
		StencilConeParameters.Bind(ParameterMap, TEXT("StencilingConeParameters"));
		StencilConeTransform.Bind(ParameterMap, TEXT("StencilingConeTransform"));
		StencilPreViewTranslation.Bind(ParameterMap, TEXT("StencilingPreViewTranslation"));
	}

	void Set(FRHICommandList& RHICmdList, FShader* Shader, const FVector4& InStencilingGeometryPosAndScale) const
	{
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), StencilGeometryPosAndScale, InStencilingGeometryPosAndScale);
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), StencilConeParameters, FVector4(0.0f, 0.0f, 0.0f, 0.0f));
	}

	void Set(FRHICommandList& RHICmdList, FShader* Shader, const FSceneView& View, const FLightSceneInfo* LightSceneInfo) const
	{
		FVector4 GeometryPosAndScale;
		if(LightSceneInfo->Proxy->GetLightType() == LightType_Point)
		{
			StencilingGeometry::GStencilSphereVertexBuffer.CalcTransform(GeometryPosAndScale, LightSceneInfo->Proxy->GetBoundingSphere(), View.ViewMatrices.GetPreViewTranslation());
			SetShaderValue(RHICmdList, Shader->GetVertexShader(), StencilGeometryPosAndScale, GeometryPosAndScale);
			SetShaderValue(RHICmdList, Shader->GetVertexShader(), StencilConeParameters, FVector4(0.0f, 0.0f, 0.0f, 0.0f));
		}
		else if(LightSceneInfo->Proxy->GetLightType() == LightType_Spot)
		{
			SetShaderValue(RHICmdList, Shader->GetVertexShader(), StencilConeTransform, LightSceneInfo->Proxy->GetLightToWorld());
			SetShaderValue(
				RHICmdList, 
				Shader->GetVertexShader(),
				StencilConeParameters,
				FVector4(
					StencilingGeometry::FStencilConeIndexBuffer::NumSides,
					StencilingGeometry::FStencilConeIndexBuffer::NumSlices,
					LightSceneInfo->Proxy->GetOuterConeAngle(),
					LightSceneInfo->Proxy->GetRadius()));
			SetShaderValue(RHICmdList, Shader->GetVertexShader(), StencilPreViewTranslation, View.ViewMatrices.GetPreViewTranslation());
		}
	}

	/** Serializer. */ 
	friend FArchive& operator<<(FArchive& Ar,FStencilingGeometryShaderParameters& P)
	{
		Ar << P.StencilGeometryPosAndScale;
		Ar << P.StencilConeParameters;
		Ar << P.StencilConeTransform;
		Ar << P.StencilPreViewTranslation;
		return Ar;
	}

private:
	FShaderParameter StencilGeometryPosAndScale;
	FShaderParameter StencilConeParameters;
	FShaderParameter StencilConeTransform;
	FShaderParameter StencilPreViewTranslation;
};

/**
* A generic vertex shader for projecting a shadow depth buffer onto the scene.
*/
class FShadowProjectionVertexShaderInterface : public FGlobalShader
{
public:
	FShadowProjectionVertexShaderInterface() {}
	FShadowProjectionVertexShaderInterface(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo) = 0;
};

/**
* A vertex shader for projecting a shadow depth buffer onto the scene.
*/
class FShadowVolumeBoundProjectionVS : public FShadowProjectionVertexShaderInterface
{
	DECLARE_SHADER_TYPE(FShadowVolumeBoundProjectionVS,Global);
public:

	FShadowVolumeBoundProjectionVS() {}
	FShadowVolumeBoundProjectionVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FShadowProjectionVertexShaderInterface(Initializer) 
	{
		StencilingGeometryParameters.Bind(Initializer.ParameterMap);
	}

	static bool ShouldCache(EShaderPlatform Platform);
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShadowProjectionVertexShaderInterface::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_TRANSFORM"), (uint32)1);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo) override;

	//~ Begin FShader Interface
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << StencilingGeometryParameters;
		return bShaderHasOutdatedParameters;
	}
	//~ Begin  End FShader Interface 

private:
	FStencilingGeometryShaderParameters StencilingGeometryParameters;
};

class FShadowProjectionNoTransformVS : public FShadowProjectionVertexShaderInterface
{
	DECLARE_SHADER_TYPE(FShadowProjectionNoTransformVS,Global);
public:
	FShadowProjectionNoTransformVS() {}
	FShadowProjectionNoTransformVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FShadowProjectionVertexShaderInterface(Initializer) 
	{
	}

	/**
	 * Add any defines required by the shader
	 * @param OutEnvironment - shader environment to modify
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShadowProjectionVertexShaderInterface::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_TRANSFORM"), (uint32)0);
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FUniformBufferRHIParamRef ViewUniformBuffer)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(), ViewUniformBuffer);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FProjectedShadowInfo*) override
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(), View.ViewUniformBuffer);
	}
};

/**
 * FShadowProjectionPixelShaderInterface - used to handle templated versions
 */

class FShadowProjectionPixelShaderInterface : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FShadowProjectionPixelShaderInterface,Global);
public:

	FShadowProjectionPixelShaderInterface() 
		:	FGlobalShader()
	{}

	/**
	 * Constructor - binds all shader params and initializes the sample offsets
	 * @param Initializer - init data from shader compiler
	 */
	FShadowProjectionPixelShaderInterface(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{ }

	/**
	 * Sets the current pixel shader params
	 * @param View - current view
	 * @param ShadowInfo - projected shadow info for a single light
	 */
	virtual void SetParameters(
		FRHICommandList& RHICmdList, 
		int32 ViewIndex,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{ 
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
	}

};

/** Shadow projection parameters used by multiple shaders. */
template<bool bModulatedShadows>
class TShadowProjectionShaderParameters
{
public:
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		DeferredParameters.Bind(ParameterMap);
		ScreenToShadowMatrix.Bind(ParameterMap,TEXT("ScreenToShadowMatrix"));
		SoftTransitionScale.Bind(ParameterMap,TEXT("SoftTransitionScale"));
		ShadowBufferSize.Bind(ParameterMap,TEXT("ShadowBufferSize"));
		ShadowDepthTexture.Bind(ParameterMap,TEXT("ShadowDepthTexture"));
		ShadowDepthTextureSampler.Bind(ParameterMap,TEXT("ShadowDepthTextureSampler"));
		ProjectionDepthBias.Bind(ParameterMap,TEXT("ProjectionDepthBiasParameters"));
		FadePlaneOffset.Bind(ParameterMap,TEXT("FadePlaneOffset"));
		InvFadePlaneLength.Bind(ParameterMap,TEXT("InvFadePlaneLength"));
		ShadowTileOffsetAndSizeParam.Bind(ParameterMap, TEXT("ShadowTileOffsetAndSize"));
	}

	void Set(FRHICommandList& RHICmdList, FShader* Shader, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo)
	{
		const FPixelShaderRHIParamRef ShaderRHI = Shader->GetPixelShader();

		DeferredParameters.Set(RHICmdList, ShaderRHI, View, MD_Surface);

		const FIntPoint ShadowBufferResolution = ShadowInfo->GetShadowBufferResolution();

		if (ShadowTileOffsetAndSizeParam.IsBound())
		{
			FVector2D InverseShadowBufferResolution(1.0f / ShadowBufferResolution.X, 1.0f / ShadowBufferResolution.Y);
			FVector4 ShadowTileOffsetAndSize(
				(ShadowInfo->BorderSize + ShadowInfo->X) * InverseShadowBufferResolution.X,
				(ShadowInfo->BorderSize + ShadowInfo->Y) * InverseShadowBufferResolution.Y,
				ShadowInfo->ResolutionX * InverseShadowBufferResolution.X,
				ShadowInfo->ResolutionY * InverseShadowBufferResolution.Y);
			SetShaderValue(RHICmdList, ShaderRHI, ShadowTileOffsetAndSizeParam, ShadowTileOffsetAndSize);
		}

		// Set the transform from screen coordinates to shadow depth texture coordinates.
		if (bModulatedShadows)
		{
			// UE-29083 : work around precision issues with ScreenToShadowMatrix on low end devices.
			const FMatrix ScreenToShadow = ShadowInfo->GetScreenToShadowMatrix(View, 0, 0, ShadowBufferResolution.X, ShadowBufferResolution.Y);
			SetShaderValue(RHICmdList, ShaderRHI, ScreenToShadowMatrix, ScreenToShadow);
		}
		else
		{
			const FMatrix ScreenToShadow = ShadowInfo->GetScreenToShadowMatrix(View);
			SetShaderValue(RHICmdList, ShaderRHI, ScreenToShadowMatrix, ScreenToShadow);
		}

		if (SoftTransitionScale.IsBound())
		{
			const float TransitionSize = ShadowInfo->ComputeTransitionSize();

			SetShaderValue(RHICmdList, ShaderRHI, SoftTransitionScale, FVector(0, 0, 1.0f / TransitionSize));
		}

		if (ShadowBufferSize.IsBound())
		{
			FVector2D ShadowBufferSizeValue(ShadowBufferResolution.X, ShadowBufferResolution.Y);

			SetShaderValue(RHICmdList, ShaderRHI, ShadowBufferSize,
				FVector4(ShadowBufferSizeValue.X, ShadowBufferSizeValue.Y, 1.0f / ShadowBufferSizeValue.X, 1.0f / ShadowBufferSizeValue.Y));
		}

		FTextureRHIParamRef ShadowDepthTextureValue;

		// Translucency shadow projection has no depth target
		if (ShadowInfo->RenderTargets.DepthTarget)
		{
			ShadowDepthTextureValue = ShadowInfo->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture.GetReference();
		}
		else
		{
			ShadowDepthTextureValue = GSystemTextures.BlackDummy->GetRenderTargetItem().ShaderResourceTexture.GetReference();
		}
			
		FSamplerStateRHIParamRef DepthSamplerState = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

		SetTextureParameter(RHICmdList, ShaderRHI, ShadowDepthTexture, ShadowDepthTextureSampler, DepthSamplerState, ShadowDepthTextureValue);		

		if (ShadowDepthTextureSampler.IsBound())
		{
			RHICmdList.SetShaderSampler(
				ShaderRHI, 
				ShadowDepthTextureSampler.GetBaseIndex(),
				DepthSamplerState
				);
		}

		SetShaderValue(RHICmdList, ShaderRHI, ProjectionDepthBias, FVector2D(ShadowInfo->GetShaderDepthBias(), ShadowInfo->MaxSubjectZ - ShadowInfo->MinSubjectZ));
		SetShaderValue(RHICmdList, ShaderRHI, FadePlaneOffset, ShadowInfo->CascadeSettings.FadePlaneOffset);

		if(InvFadePlaneLength.IsBound())
		{
			check(ShadowInfo->CascadeSettings.FadePlaneLength > 0);
			SetShaderValue(RHICmdList, ShaderRHI, InvFadePlaneLength, 1.0f / ShadowInfo->CascadeSettings.FadePlaneLength);
		}
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar, TShadowProjectionShaderParameters& P)
	{
		Ar << P.DeferredParameters;
		Ar << P.ScreenToShadowMatrix;
		Ar << P.SoftTransitionScale;
		Ar << P.ShadowBufferSize;
		Ar << P.ShadowDepthTexture;
		Ar << P.ShadowDepthTextureSampler;
		Ar << P.ProjectionDepthBias;
		Ar << P.FadePlaneOffset;
		Ar << P.InvFadePlaneLength;
		Ar << P.ShadowTileOffsetAndSizeParam;
		return Ar;
	}

#if !WITH_GFSDK_VXGI
private:
#endif

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter ScreenToShadowMatrix;
	FShaderParameter SoftTransitionScale;
	FShaderParameter ShadowBufferSize;
	FShaderResourceParameter ShadowDepthTexture;
	FShaderResourceParameter ShadowDepthTextureSampler;
	FShaderParameter ProjectionDepthBias;
	FShaderParameter FadePlaneOffset;
	FShaderParameter InvFadePlaneLength;
	FShaderParameter ShadowTileOffsetAndSizeParam;
};

/**
 * TShadowProjectionPS
 * A pixel shader for projecting a shadow depth buffer onto the scene.  Used with any light type casting normal shadows.
 */
template<uint32 Quality, bool bUseFadePlane = false, bool bModulatedShadows = false>
class TShadowProjectionPS : public FShadowProjectionPixelShaderInterface
{
	DECLARE_SHADER_TYPE(TShadowProjectionPS,Global);
public:

	TShadowProjectionPS()
		: FShadowProjectionPixelShaderInterface()
	{ 
	}

	/**
	 * Constructor - binds all shader params and initializes the sample offsets
	 * @param Initializer - init data from shader compiler
	 */
	TShadowProjectionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShadowProjectionPixelShaderInterface(Initializer)
	{
		ProjectionParameters.Bind(Initializer.ParameterMap);
		ShadowFadeFraction.Bind(Initializer.ParameterMap,TEXT("ShadowFadeFraction"));
		ShadowSharpen.Bind(Initializer.ParameterMap,TEXT("ShadowSharpen"));
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/**
	 * Add any defines required by the shader
	 * @param OutEnvironment - shader environment to modify
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShadowProjectionPixelShaderInterface::ModifyCompilationEnvironment(Platform,OutEnvironment);
		TShadowProjectionShaderParameters<bModulatedShadows>::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADOW_QUALITY"), Quality);
		OutEnvironment.SetDefine(TEXT("USE_FADE_PLANE"), (uint32)(bUseFadePlane ? 1 : 0));
	}

	/**
	 * Sets the pixel shader's parameters
	 * @param View - current view
	 * @param ShadowInfo - projected shadow info for a single light
	 */
	virtual void SetParameters(
		FRHICommandList& RHICmdList, 
		int32 ViewIndex,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		) override
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FShadowProjectionPixelShaderInterface::SetParameters(RHICmdList, ViewIndex,View,ShadowInfo);

		ProjectionParameters.Set(RHICmdList, this, View, ShadowInfo);

		SetShaderValue(RHICmdList, ShaderRHI, ShadowFadeFraction, ShadowInfo->FadeAlphas[ViewIndex] );
		SetShaderValue(RHICmdList, ShaderRHI, ShadowSharpen, ShadowInfo->GetLightSceneInfo().Proxy->GetShadowSharpen() * 7.0f + 1.0f );

		auto DeferredLightParameter = GetUniformBufferParameter<FDeferredLightUniformStruct>();

		if (DeferredLightParameter.IsBound())
		{
			SetDeferredLightParameters(RHICmdList, ShaderRHI, DeferredLightParameter, &ShadowInfo->GetLightSceneInfo(), View);
		}
	}

	/**
	 * Serialize the parameters for this shader
	 * @param Ar - archive to serialize to
	 */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FShadowProjectionPixelShaderInterface::Serialize(Ar);
		Ar << ProjectionParameters;
		Ar << ShadowFadeFraction;
		Ar << ShadowSharpen;
		return bShaderHasOutdatedParameters;
	}

protected:
	TShadowProjectionShaderParameters<bModulatedShadows> ProjectionParameters;
	FShaderParameter ShadowFadeFraction;
	FShaderParameter ShadowSharpen;
};

/** Pixel shader to project modulated shadows onto the scene. */
template<uint32 Quality>
class TModulatedShadowProjection : public TShadowProjectionPS<Quality, false, true>
{
	DECLARE_SHADER_TYPE(TModulatedShadowProjection, Global);
public:

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		TShadowProjectionPS<Quality, false, true>::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("MODULATED_SHADOWS"), 1);
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsMobilePlatform(Platform);
	}

	TModulatedShadowProjection() {}

	TModulatedShadowProjection(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		TShadowProjectionPS<Quality, false, true>(Initializer)
	{
		ModulatedShadowColorParameter.Bind(Initializer.ParameterMap, TEXT("ModulatedShadowColor"));
	}

	virtual void SetParameters(
		FRHICommandList& RHICmdList,
		int32 ViewIndex,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo) override
	{
		TShadowProjectionPS<Quality, false, true>::SetParameters(RHICmdList, ViewIndex, View, ShadowInfo);
		const FPixelShaderRHIParamRef ShaderRHI = this->GetPixelShader();
		SetShaderValue(RHICmdList, ShaderRHI, ModulatedShadowColorParameter, ShadowInfo->GetLightSceneInfo().Proxy->GetModulatedShadowColor());
	}

	/**
	* Serialize the parameters for this shader
	* @param Ar - archive to serialize to
	*/
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = TShadowProjectionPS<Quality, false, true>::Serialize(Ar);
		Ar << ModulatedShadowColorParameter;
		return bShaderHasOutdatedParameters;
	}

protected:
	FShaderParameter ModulatedShadowColorParameter;
};

/** Translucency shadow projection parameters used by multiple shaders. */
class FTranslucencyShadowProjectionShaderParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		TranslucencyShadowTransmission0.Bind(ParameterMap,TEXT("TranslucencyShadowTransmission0"));
		TranslucencyShadowTransmission0Sampler.Bind(ParameterMap,TEXT("TranslucencyShadowTransmission0Sampler"));
		TranslucencyShadowTransmission1.Bind(ParameterMap,TEXT("TranslucencyShadowTransmission1"));
		TranslucencyShadowTransmission1Sampler.Bind(ParameterMap,TEXT("TranslucencyShadowTransmission1Sampler"));
	}

	void Set(FRHICommandList& RHICmdList, FShader* Shader, const FProjectedShadowInfo* ShadowInfo) const
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		FTextureRHIParamRef TranslucencyShadowTransmission0Value;
		FTextureRHIParamRef TranslucencyShadowTransmission1Value;

		if (ShadowInfo)
		{
			TranslucencyShadowTransmission0Value = ShadowInfo->RenderTargets.ColorTargets[0]->GetRenderTargetItem().ShaderResourceTexture.GetReference();
			TranslucencyShadowTransmission1Value = ShadowInfo->RenderTargets.ColorTargets[1]->GetRenderTargetItem().ShaderResourceTexture.GetReference();
		}
		else
		{
			TranslucencyShadowTransmission0Value = GSystemTextures.BlackDummy->GetRenderTargetItem().ShaderResourceTexture.GetReference();
			TranslucencyShadowTransmission1Value = GSystemTextures.BlackDummy->GetRenderTargetItem().ShaderResourceTexture.GetReference();
		}
			
		SetTextureParameter(
			RHICmdList, 
			Shader->GetPixelShader(),
			TranslucencyShadowTransmission0,
			TranslucencyShadowTransmission0Sampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			TranslucencyShadowTransmission0Value
			);

		SetTextureParameter(
			RHICmdList, 
			Shader->GetPixelShader(),
			TranslucencyShadowTransmission1,
			TranslucencyShadowTransmission1Sampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			TranslucencyShadowTransmission1Value
			);
	}

	/** Serializer. */ 
	friend FArchive& operator<<(FArchive& Ar,FTranslucencyShadowProjectionShaderParameters& P)
	{
		Ar << P.TranslucencyShadowTransmission0;
		Ar << P.TranslucencyShadowTransmission0Sampler;
		Ar << P.TranslucencyShadowTransmission1;
		Ar << P.TranslucencyShadowTransmission1Sampler;
		return Ar;
	}

private:

	FShaderResourceParameter TranslucencyShadowTransmission0;
	FShaderResourceParameter TranslucencyShadowTransmission0Sampler;
	FShaderResourceParameter TranslucencyShadowTransmission1;
	FShaderResourceParameter TranslucencyShadowTransmission1Sampler;
};

/** Pixel shader to project both opaque and translucent shadows onto opaque surfaces. */
template<uint32 Quality> 
class TShadowProjectionFromTranslucencyPS : public TShadowProjectionPS<Quality>
{
	DECLARE_SHADER_TYPE(TShadowProjectionFromTranslucencyPS,Global);
public:

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		TShadowProjectionPS<Quality>::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("APPLY_TRANSLUCENCY_SHADOWS"), 1);
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && TShadowProjectionPS<Quality>::ShouldCache(Platform);
	}

	TShadowProjectionFromTranslucencyPS() {}

	TShadowProjectionFromTranslucencyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		TShadowProjectionPS<Quality>(Initializer)
	{
		TranslucencyProjectionParameters.Bind(Initializer.ParameterMap);
	}

	virtual void SetParameters(
		FRHICommandList& RHICmdList, 
		int32 ViewIndex,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo) override
	{
		TShadowProjectionPS<Quality>::SetParameters(RHICmdList, ViewIndex, View, ShadowInfo);

		TranslucencyProjectionParameters.Set(RHICmdList, this, ShadowInfo);
	}

	/**
	 * Serialize the parameters for this shader
	 * @param Ar - archive to serialize to
	 */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = TShadowProjectionPS<Quality>::Serialize(Ar);
		Ar << TranslucencyProjectionParameters;
		return bShaderHasOutdatedParameters;
	}

protected:
	FTranslucencyShadowProjectionShaderParameters TranslucencyProjectionParameters;
};


/** One pass point light shadow projection parameters used by multiple shaders. */
class FOnePassPointShadowProjectionShaderParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		ShadowDepthTexture.Bind(ParameterMap,TEXT("ShadowDepthCubeTexture"));
		ShadowDepthCubeComparisonSampler.Bind(ParameterMap,TEXT("ShadowDepthCubeTextureSampler"));
		ShadowViewProjectionMatrices.Bind(ParameterMap, TEXT("ShadowViewProjectionMatrices"));
		InvShadowmapResolution.Bind(ParameterMap, TEXT("InvShadowmapResolution"));
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FProjectedShadowInfo* ShadowInfo) const
	{
		FTextureRHIParamRef ShadowDepthTextureValue = ShadowInfo 
			? ShadowInfo->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture->GetTextureCube()
			: GBlackTextureDepthCube->TextureRHI.GetReference();
		if (!ShadowDepthTextureValue)
		{
			ShadowDepthTextureValue = GBlackTextureDepthCube->TextureRHI.GetReference();
		}

		SetTextureParameter(
			RHICmdList, 
			ShaderRHI, 
			ShadowDepthTexture, 
			ShadowDepthTextureValue
			);

		if (ShadowDepthCubeComparisonSampler.IsBound())
		{
			RHICmdList.SetShaderSampler(
				ShaderRHI, 
				ShadowDepthCubeComparisonSampler.GetBaseIndex(), 
				// Use a comparison sampler to do hardware PCF
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 0, 0, SCF_Less>::GetRHI()
				);
		}

		if (ShadowInfo)
		{
			SetShaderValueArray<ShaderRHIParamRef, FMatrix>(
				RHICmdList, 
				ShaderRHI,
				ShadowViewProjectionMatrices,
				ShadowInfo->OnePassShadowViewProjectionMatrices.GetData(),
				ShadowInfo->OnePassShadowViewProjectionMatrices.Num()
				);

			SetShaderValue(RHICmdList, ShaderRHI,InvShadowmapResolution,1.0f / ShadowInfo->ResolutionX);
		}
		else
		{
			TArray<FMatrix, SceneRenderingAllocator> ZeroMatrices;
			ZeroMatrices.AddZeroed(FMath::DivideAndRoundUp<int32>(ShadowViewProjectionMatrices.GetNumBytes(), sizeof(FMatrix)));

			SetShaderValueArray<ShaderRHIParamRef, FMatrix>(
				RHICmdList, 
				ShaderRHI,
				ShadowViewProjectionMatrices,
				ZeroMatrices.GetData(),
				ZeroMatrices.Num()
				);

			SetShaderValue(RHICmdList, ShaderRHI,InvShadowmapResolution,0);
		}
	}

	/** Serializer. */ 
	friend FArchive& operator<<(FArchive& Ar,FOnePassPointShadowProjectionShaderParameters& P)
	{
		Ar << P.ShadowDepthTexture;
		Ar << P.ShadowDepthCubeComparisonSampler;
		Ar << P.ShadowViewProjectionMatrices;
		Ar << P.InvShadowmapResolution;
		return Ar;
	}

#if !WITH_GFSDK_VXGI
private:
#endif

	FShaderResourceParameter ShadowDepthTexture;
	FShaderResourceParameter ShadowDepthCubeComparisonSampler;
	FShaderParameter ShadowViewProjectionMatrices;
	FShaderParameter InvShadowmapResolution;
};

/**
 * Pixel shader used to project one pass point light shadows.
 */
// Quality = 0 / 1
template <uint32 Quality>
class TOnePassPointShadowProjectionPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TOnePassPointShadowProjectionPS,Global);
public:

	TOnePassPointShadowProjectionPS() {}

	TOnePassPointShadowProjectionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		OnePassShadowParameters.Bind(Initializer.ParameterMap);
		ShadowDepthTextureSampler.Bind(Initializer.ParameterMap,TEXT("ShadowDepthTextureSampler"));
		LightPosition.Bind(Initializer.ParameterMap,TEXT("LightPositionAndInvRadius"));
		ShadowFadeFraction.Bind(Initializer.ParameterMap,TEXT("ShadowFadeFraction"));
		ShadowSharpen.Bind(Initializer.ParameterMap,TEXT("ShadowSharpen"));
		PointLightDepthBiasParameters.Bind(Initializer.ParameterMap,TEXT("PointLightDepthBiasParameters"));
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADOW_QUALITY"), Quality);
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		int32 ViewIndex,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI,View.ViewUniformBuffer);

		DeferredParameters.Set(RHICmdList, ShaderRHI, View, MD_Surface);
		OnePassShadowParameters.Set(RHICmdList, ShaderRHI, ShadowInfo);

		const FLightSceneProxy& LightProxy = *(ShadowInfo->GetLightSceneInfo().Proxy);

		SetShaderValue(RHICmdList, ShaderRHI, LightPosition, FVector4(LightProxy.GetPosition(), 1.0f / LightProxy.GetRadius()));

		SetShaderValue(RHICmdList, ShaderRHI, ShadowFadeFraction, ShadowInfo->FadeAlphas[ViewIndex]);
		SetShaderValue(RHICmdList, ShaderRHI, ShadowSharpen, LightProxy.GetShadowSharpen() * 7.0f + 1.0f);
		SetShaderValue(RHICmdList, ShaderRHI, PointLightDepthBiasParameters, FVector2D(ShadowInfo->GetShaderDepthBias(), 0.0f));

		SetSamplerParameter(RHICmdList, ShaderRHI, ShadowDepthTextureSampler, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		auto DeferredLightParameter = GetUniformBufferParameter<FDeferredLightUniformStruct>();

		if (DeferredLightParameter.IsBound())
		{
			SetDeferredLightParameters(RHICmdList, ShaderRHI, DeferredLightParameter, &ShadowInfo->GetLightSceneInfo(), View);
		}
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << OnePassShadowParameters;
		Ar << ShadowDepthTextureSampler;
		Ar << LightPosition;
		Ar << ShadowFadeFraction;
		Ar << ShadowSharpen;
		Ar << PointLightDepthBiasParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FDeferredPixelShaderParameters DeferredParameters;
	FOnePassPointShadowProjectionShaderParameters OnePassShadowParameters;
	FShaderResourceParameter ShadowDepthTextureSampler;
	FShaderParameter LightPosition;
	FShaderParameter ShadowFadeFraction;
	FShaderParameter ShadowSharpen;
	FShaderParameter PointLightDepthBiasParameters;

};

/** A transform the remaps depth and potentially projects onto some plane. */
struct FShadowProjectionMatrix: FMatrix
{
	FShadowProjectionMatrix(float MinZ,float MaxZ,const FVector4& WAxis):
		FMatrix(
		FPlane(1,	0,	0,													WAxis.X),
		FPlane(0,	1,	0,													WAxis.Y),
		FPlane(0,	0,	(WAxis.Z * MaxZ + WAxis.W) / (MaxZ - MinZ),			WAxis.Z),
		FPlane(0,	0,	-MinZ * (WAxis.Z * MaxZ + WAxis.W) / (MaxZ - MinZ),	WAxis.W)
		)
	{}
};


/** Pixel shader to project directional PCSS onto the scene. */
template<uint32 Quality, bool bUseFadePlane>
class TDirectionalPercentageCloserShadowProjectionPS : public TShadowProjectionPS<Quality, bUseFadePlane>
{
	DECLARE_SHADER_TYPE(TDirectionalPercentageCloserShadowProjectionPS, Global);
public:

	TDirectionalPercentageCloserShadowProjectionPS() {}
	TDirectionalPercentageCloserShadowProjectionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		TShadowProjectionPS<Quality, bUseFadePlane>(Initializer)
	{
		PCSSParameters.Bind(Initializer.ParameterMap, TEXT("PCSSParameters"));
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		TShadowProjectionPS<Quality, bUseFadePlane>::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_PCSS"), 1);
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return TShadowProjectionPS<Quality, bUseFadePlane>::ShouldCache(Platform) && (Platform == SP_PCD3D_SM5 || Platform == SP_VULKAN_SM5 || Platform == SP_METAL_SM5);
	}

	virtual void SetParameters(
		FRHICommandList& RHICmdList,
		int32 ViewIndex,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo) override
	{
		TShadowProjectionPS<Quality, bUseFadePlane>::SetParameters(RHICmdList, ViewIndex, View, ShadowInfo);

		const FPixelShaderRHIParamRef ShaderRHI = this->GetPixelShader();

		// GetLightSourceAngle returns the full angle.
		float TanLightSourceAngle = FMath::Tan(0.5 * FMath::DegreesToRadians(ShadowInfo->GetLightSceneInfo().Proxy->GetLightSourceAngle()));

		static IConsoleVariable* CVarMaxSoftShadowKernelSize = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.MaxSoftKernelSize"));
		check(CVarMaxSoftShadowKernelSize);
		int32 MaxKernelSize = CVarMaxSoftShadowKernelSize->GetInt();

		float SW = 2.0 * ShadowInfo->ShadowBounds.W;
		float SZ = ShadowInfo->MaxSubjectZ - ShadowInfo->MinSubjectZ;

		FVector4 PCSSParameterValues = FVector4(TanLightSourceAngle * SZ / SW, MaxKernelSize / float(ShadowInfo->ResolutionX), 0, 0);
		SetShaderValue(RHICmdList, ShaderRHI, PCSSParameters, PCSSParameterValues);
	}

	/**
	* Serialize the parameters for this shader
	* @param Ar - archive to serialize to
	*/
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = TShadowProjectionPS<Quality, bUseFadePlane>::Serialize(Ar);
		Ar << PCSSParameters;
		return bShaderHasOutdatedParameters;
	}

protected:
	FShaderParameter PCSSParameters;
};


/** Pixel shader to project PCSS spot light onto the scene. */
template<uint32 Quality, bool bUseFadePlane>
class TSpotPercentageCloserShadowProjectionPS : public TShadowProjectionPS<Quality, bUseFadePlane>
{
	DECLARE_SHADER_TYPE(TSpotPercentageCloserShadowProjectionPS, Global);
public:

	TSpotPercentageCloserShadowProjectionPS() {}
	TSpotPercentageCloserShadowProjectionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		TShadowProjectionPS<Quality, bUseFadePlane>(Initializer)
	{
		PCSSParameters.Bind(Initializer.ParameterMap, TEXT("PCSSParameters"));
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (Platform == SP_PCD3D_SM5 || Platform == SP_VULKAN_SM5 || Platform == SP_METAL_SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		TShadowProjectionPS<Quality, bUseFadePlane>::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_PCSS"), 1);
		OutEnvironment.SetDefine(TEXT("SPOT_LIGHT_PCSS"), 1);
	}

	virtual void SetParameters(
		FRHICommandList& RHICmdList,
		int32 ViewIndex,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo) override
	{
		check(ShadowInfo->GetLightSceneInfo().Proxy->GetLightType() == LightType_Spot);

		TShadowProjectionPS<Quality, bUseFadePlane>::SetParameters(RHICmdList, ViewIndex, View, ShadowInfo);

		const FPixelShaderRHIParamRef ShaderRHI = this->GetPixelShader();

		static IConsoleVariable* CVarMaxSoftShadowKernelSize = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.MaxSoftKernelSize"));
		check(CVarMaxSoftShadowKernelSize);
		int32 MaxKernelSize = CVarMaxSoftShadowKernelSize->GetInt();

		FVector4 PCSSParameterValues = FVector4(0, MaxKernelSize / float(ShadowInfo->ResolutionX), 0, 0);
		SetShaderValue(RHICmdList, ShaderRHI, PCSSParameters, PCSSParameterValues);
	}

	/**
	* Serialize the parameters for this shader
	* @param Ar - archive to serialize to
	*/
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = TShadowProjectionPS<Quality, bUseFadePlane>::Serialize(Ar);
		Ar << PCSSParameters;
		return bShaderHasOutdatedParameters;
	}

protected:
	FShaderParameter PCSSParameters;
};


// Sort by descending resolution
struct FCompareFProjectedShadowInfoByResolution
{
	FORCEINLINE bool operator() (const FProjectedShadowInfo& A, const FProjectedShadowInfo& B) const
	{
		return (B.ResolutionX * B.ResolutionY < A.ResolutionX * A.ResolutionY);
	}
};

// Sort by shadow type (CSMs first, then other types).
// Then sort CSMs by descending split index, and other shadows by resolution.
// Used to render shadow cascades in far to near order, whilst preserving the
// descending resolution sort behavior for other shadow types.
// Note: the ordering must match the requirements of blend modes set in SetBlendStateForProjection (blend modes that overwrite must come first)
struct FCompareFProjectedShadowInfoBySplitIndex
{
	FORCEINLINE bool operator()( const FProjectedShadowInfo& A, const FProjectedShadowInfo& B ) const
	{
		if (A.IsWholeSceneDirectionalShadow())
		{
			if (B.IsWholeSceneDirectionalShadow())
			{
				if (A.bRayTracedDistanceField != B.bRayTracedDistanceField)
				{
					// RTDF shadows need to be rendered after all CSM, because they overlap in depth range with Far Cascades, which will use an overwrite blend mode for the fade plane.
					if (!A.bRayTracedDistanceField && B.bRayTracedDistanceField)
					{
						return true;
					}

					if (A.bRayTracedDistanceField && !B.bRayTracedDistanceField)
					{
						return false;
					}
				}

				// Both A and B are CSMs
				// Compare Split Indexes, to order them far to near.
				return (B.CascadeSettings.ShadowSplitIndex < A.CascadeSettings.ShadowSplitIndex);
			}

			// A is a CSM, B is per-object shadow etc.
			// B should be rendered after A.
			return true;
		}
		else
		{
			if (B.IsWholeSceneDirectionalShadow())
			{
				// B should be rendered before A.
				return false;
			}
			
			// Neither shadow is a CSM
			// Sort by descending resolution.
			return FCompareFProjectedShadowInfoByResolution()(A, B);
		}
	}
};

