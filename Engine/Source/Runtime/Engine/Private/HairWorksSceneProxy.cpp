// @third party code - BEGIN HairWorks
#include "HairWorksSceneProxy.h"
#include <Nv/Common/NvCoMemoryReadStream.h>
#include "AllowWindowsPlatformTypes.h"
#include <Nv/Common/Render/Dx11/NvCoDx11Handle.h>
#include "HideWindowsPlatformTypes.h"
#include "HairWorksSDK.h"
#include "ScopeLock.h"
#include "ShaderParameterUtils.h"
#include "SkeletalRenderGPUSkin.h"
#include "Engine/Texture2D.h"
#include "Engine/HairWorksAsset.h"

static TAutoConsoleVariable<float> CVarWindScale(TEXT("r.HairWorks.WindScale"), 50, TEXT(""), ECVF_RenderThreadSafe);

// Debug render console variables.
#define HairVisualizationCVarDefine(Name)	\
	static TAutoConsoleVariable<int> CVarHairVisualization##Name(TEXT("r.HairWorks.Visualization.") TEXT(#Name), 0, TEXT(""), ECVF_RenderThreadSafe)

static TAutoConsoleVariable<int> CVarHairVisualizationHair(TEXT("r.HairWorks.Visualization.")TEXT("Hair"), 1, TEXT(""), ECVF_RenderThreadSafe);
HairVisualizationCVarDefine(GuideCurves);
HairVisualizationCVarDefine(SkinnedGuideCurves);
HairVisualizationCVarDefine(ControlPoints);
HairVisualizationCVarDefine(GrowthMesh);
HairVisualizationCVarDefine(Bones);
HairVisualizationCVarDefine(BoundingBox);
HairVisualizationCVarDefine(CollisionCapsules);
HairVisualizationCVarDefine(HairInteraction);
HairVisualizationCVarDefine(PinConstraints);
HairVisualizationCVarDefine(ShadingNormal);
HairVisualizationCVarDefine(ShadingNormalCenter);

#undef HairVisualizationCVarDefine

class FHairWorksCopyMorphDeltasCs: public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHairWorksCopyMorphDeltasCs, Global);

	FHairWorksCopyMorphDeltasCs()
	{}

	FHairWorksCopyMorphDeltasCs(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		MorphVertexCount.Bind(Initializer.ParameterMap, TEXT("MorphVertexCount"));
		MorphIndexBuffer.Bind(Initializer.ParameterMap, TEXT("MorphIndexBuffer"));
		MorphVertexBuffer.Bind(Initializer.ParameterMap, TEXT("MorphVertexBuffer"));
		MorphPositionDeltaBuffer.Bind(Initializer.ParameterMap, TEXT("MorphPositionDeltaBuffer"));
		MorphNormalDeltaBuffer.Bind(Initializer.ParameterMap, TEXT("MorphNormalDeltaBuffer"));
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MorphVertexCount << MorphIndexBuffer << MorphVertexBuffer << MorphPositionDeltaBuffer << MorphNormalDeltaBuffer;
		return bShaderHasOutdatedParameters;
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return Platform == EShaderPlatform::SP_PCD3D_SM5;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FShaderParameter MorphVertexCount;
	FShaderResourceParameter MorphIndexBuffer;
	FShaderResourceParameter MorphVertexBuffer;
	FShaderResourceParameter MorphPositionDeltaBuffer;
	FShaderResourceParameter MorphNormalDeltaBuffer;
};

IMPLEMENT_SHADER_TYPE(, FHairWorksCopyMorphDeltasCs, TEXT("/Engine/Private/HairWorks/HairWorks.usf"), TEXT("CopyMorphDeltas"), SF_Compute);


FHairWorksSceneProxy* FHairWorksSceneProxy::HairInstances = nullptr;

FHairWorksSceneProxy::FHairWorksSceneProxy(const UPrimitiveComponent* InComponent, NvHair::AssetId InHairAssetId):
	FPrimitiveSceneProxy(InComponent),
	HairAssetId(InHairAssetId),
	HairInstanceId(NvHair::INSTANCE_ID_NULL)
{
	check(InHairAssetId != NvHair::ASSET_ID_NULL);

	HairTextures.SetNumZeroed(NvHair::ETextureType::COUNT_OF);
}

FHairWorksSceneProxy::~FHairWorksSceneProxy()
{
	if(HairInstanceId != NvHair::INSTANCE_ID_NULL)
	{
		::HairWorks::GetSDK()->freeInstance(HairInstanceId);
		HairInstanceId = NvHair::INSTANCE_ID_NULL;

		Unlink();
	}
}

uint32 FHairWorksSceneProxy::GetMemoryFootprint(void) const
{
	return 0;
}

void FHairWorksSceneProxy::Draw(FRHICommandList& RHICmdList, EDrawType DrawType)const
{
	// The real render function
	auto DoRender = [this, DrawType]()
	{
		// Can't call it here. It changes render states, so we must call it earlier before we set render states. 
		//HairWorks::GetSDK()->preRender(RenderInterp);

		// Flush render states
		::HairWorks::GetD3DHelper().CommitShaderResources();

		// Draw
		if(DrawType == EDrawType::Visualization)
		{
			NvHair::VisualizationSettings VisSettings;
			VisSettings.m_depthOp = NvHair::DepthOp::WRITE_GREATER;
			::HairWorks::GetSDK()->renderVisualization(HairInstanceId, &VisSettings);
		}
		else
		{
			// Special for shadow
			NvHair::InstanceDescriptor HairDesc;
			::HairWorks::GetSDK()->getInstanceDescriptor(HairInstanceId, HairDesc);

			if(DrawType == EDrawType::Shadow)
			{
				HairDesc.m_useBackfaceCulling = false;
				HairDesc.m_useViewfrustrumCulling = false;

				::HairWorks::GetSDK()->updateInstanceDescriptor(HairInstanceId, HairDesc);
			}

			// Handle shader cache.
			NvHair::ShaderCacheSettings ShaderCacheSetting;
			ShaderCacheSetting.setFromInstanceDescriptor(HairDesc);
			check(HairTextures.Num() == NvHair::ETextureType::COUNT_OF);
			for(int i = 0; i < NvHair::ETextureType::COUNT_OF; i++)
			{
				ShaderCacheSetting.setTextureUsed(i, HairTextures[i] != nullptr);
			}

			::HairWorks::GetSDK()->addToShaderCache(ShaderCacheSetting);

			// Draw
			NvHair::ShaderSettings HairShaderSettings;
			HairShaderSettings.m_useCustomConstantBuffer = true;
			HairShaderSettings.m_shadowPass = (DrawType == EDrawType::Shadow);

			::HairWorks::GetSDK()->renderHairs(HairInstanceId, &HairShaderSettings);
		}
	};

	// Call or schedule the render function
	if(RHICmdList.Bypass())
		DoRender();
	else
	{
		struct FRHICmdDraw: public FRHICommand<FRHICmdDraw>
		{
			FRHICmdDraw(decltype(DoRender) InRender)
				:Render(InRender)
			{}

			void Execute(FRHICommandListBase& CmdList)
			{
				Render();
			}

			decltype(DoRender) Render;
		};

		new (RHICmdList.AllocCommand<FRHICmdDraw>())FRHICmdDraw(DoRender);
	}
}

void FHairWorksSceneProxy::SetPinMatrices(const TArray<FMatrix>& PinMatrices)
{
	FScopeLock ObjObjectsLock(&ThreadLock);

	HairPinMatrices = PinMatrices;
}

const TArray<FMatrix>& FHairWorksSceneProxy::GetPinMatrices()
{	
	FScopeLock ObjObjectsLock(&ThreadLock);

	return HairPinMatrices;
}

FHairWorksSceneProxy * FHairWorksSceneProxy::GetHairInstances()
{
	return HairInstances;
}

FPrimitiveViewRelevance FHairWorksSceneProxy::GetViewRelevance(const FSceneView* View)const
{
	FPrimitiveViewRelevance ViewRel;
	ViewRel.bDrawRelevance = IsShown(View);
	ViewRel.bShadowRelevance = IsShadowCast(View);
	ViewRel.bDynamicRelevance = true;
	ViewRel.bRenderInMainPass = false;	// Hair is rendered in a special path.
	ViewRel.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();

	ViewRel.bHairWorks = View->Family->EngineShowFlags.HairWorks && HairInstanceId != NvHair::INSTANCE_ID_NULL;

	return ViewRel;
}

void FHairWorksSceneProxy::CreateRenderThreadResources()
{
	// Initialize hair instance
	check(::HairWorks::GetSDK() != nullptr &&  HairAssetId != NvHair::ASSET_ID_NULL && HairInstanceId == NvHair::INSTANCE_ID_NULL);

	auto& HairSdk = *::HairWorks::GetSDK();
	HairSdk.createInstance(HairAssetId, HairInstanceId);
	if(HairInstanceId == NvHair::INSTANCE_ID_NULL)
		return;

	// Add to list
	LinkHead(HairInstances);

	// Disable this instance at first.
	NvHair::InstanceDescriptor HairInstanceDesc;
	HairSdk.getInstanceDescriptor(HairInstanceId, HairInstanceDesc);
	if(HairInstanceDesc.m_enable)
	{
		HairInstanceDesc.m_enable = false;
		HairSdk.updateInstanceDescriptor(HairInstanceId, HairInstanceDesc);
	}
}

void FHairWorksSceneProxy::OnTransformChanged()
{
	// Update new matrix to hair
	FPrimitiveSceneProxy::OnTransformChanged();

	if(HairInstanceId == NvHair::INSTANCE_ID_NULL)
		return;

	NvHair::InstanceDescriptor InstDesc;
	::HairWorks::GetSDK()->getInstanceDescriptor(HairInstanceId, InstDesc);

	InstDesc.m_modelToWorld = (NvHair::Mat4x4&)GetLocalToWorld().M;

	::HairWorks::GetSDK()->updateInstanceDescriptor(HairInstanceId, InstDesc);
}

void FHairWorksSceneProxy::UpdateDynamicData_RenderThread(FDynamicRenderData& DynamicData)
{
	// Set skinning data
	if(DynamicData.BoneMatrices.Num() > 0)
	{
		if (DynamicData.bSimulateInWorldSpace)
		{
			for (auto& BoneMatrix : DynamicData.BoneMatrices)
			{
				BoneMatrix *= GetLocalToWorld();
			}
		}

		::HairWorks::GetSDK()->updateSkinningMatrices(
			HairInstanceId, DynamicData.BoneMatrices.Num(),
			reinterpret_cast<const NvHair::Mat4x4*>(DynamicData.BoneMatrices.GetData())
		);

		if(CurrentSkinningMatrices.Num() > 0)
			PrevSkinningMatrices = MoveTemp(CurrentSkinningMatrices);
		else
			PrevSkinningMatrices = DynamicData.BoneMatrices;

		CurrentSkinningMatrices = MoveTemp(DynamicData.BoneMatrices);
	}

	// Morph data. It's too early to update morph data here. It's not ready yet. Delay it just before simulation.
	if(DynamicData.ParentSkin != nullptr && DynamicData.ParentSkin->GetMorphVertexBuffer().bHasBeenUpdated)
	{
		DynamicData.ParentSkin->GetMorphVertexBuffer().RequireSRV();
		MorphVertexBuffer = DynamicData.ParentSkin->GetMorphVertexBuffer().GetSRV();
	}
	else
	{
		MorphVertexBuffer = nullptr;
	}

	MorphVertexUpdateFrameNumber = GFrameNumberRenderThread;

	// Update normal center bone
	auto HairDesc = DynamicData.HairInstanceDesc;

	// Merge global visualization flags.
#define HairVisualizationCVarUpdate(CVarName, MemberVarName)	\
	HairDesc.m_visualize##MemberVarName |= CVarHairVisualization##CVarName.GetValueOnRenderThread() != 0

	HairVisualizationCVarUpdate(GuideCurves, GuideHairs);
	HairVisualizationCVarUpdate(SkinnedGuideCurves, SkinnedGuideHairs);
	HairVisualizationCVarUpdate(ControlPoints, ControlVertices);
	HairVisualizationCVarUpdate(GrowthMesh, GrowthMesh);
	HairVisualizationCVarUpdate(Bones, Bones);
	HairVisualizationCVarUpdate(BoundingBox, BoundingBox);
	HairVisualizationCVarUpdate(CollisionCapsules, Capsules);
	HairVisualizationCVarUpdate(HairInteraction, HairInteractions);
	HairVisualizationCVarUpdate(PinConstraints, PinConstraints);
	HairVisualizationCVarUpdate(ShadingNormal, ShadingNormals);
	HairVisualizationCVarUpdate(ShadingNormalCenter, ShadingNormalBone);

#undef HairVisualizerCVarUpdate

	HairDesc.m_drawRenderHairs &= CVarHairVisualizationHair.GetValueOnRenderThread() != 0;

	// World transform
	if (DynamicData.bSimulateInWorldSpace)
	{
		HairDesc.m_modelToWorld = reinterpret_cast<const NvHair::Mat4x4&>(FMatrix::Identity.M);
	}
	else
	{
		HairDesc.m_modelToWorld = reinterpret_cast<const NvHair::Mat4x4&>(GetLocalToWorld().M);
	}

	// Wind
	if (reinterpret_cast<FVector&>(HairDesc.m_wind).Size() == 0)
	{
		FVector WindDirection;
		float WindSpeed;
		float WindMinGustAmt;
		float WindMaxGustAmt;
		GetScene().GetWindParameters(GetBounds().Origin, WindDirection, WindSpeed, WindMinGustAmt, WindMaxGustAmt);

		auto& ModelToWorld = reinterpret_cast<FMatrix&>(HairDesc.m_modelToWorld);
		reinterpret_cast<FVector&>(HairDesc.m_wind) = ModelToWorld.Inverse().TransformVector(WindDirection) * WindSpeed * CVarWindScale.GetValueOnRenderThread() * (FMath::FRand() * 0.5f + 1);
	}

	// Set parameters to HairWorks
	::HairWorks::GetSDK()->updateInstanceDescriptor(HairInstanceId, HairDesc);	// Mainly for simulation.

	// Update textures
	check(DynamicData.Textures.Num() == NvHair::ETextureType::COUNT_OF);
	HairTextures.SetNumZeroed(NvHair::ETextureType::COUNT_OF);
	for(auto Idx = 0; Idx < HairTextures.Num(); ++Idx)
	{
		auto* Texture = DynamicData.Textures[Idx];
		if(Texture == nullptr)
			continue;

		if(Texture->Resource == nullptr)
			continue;

		HairTextures[Idx] = static_cast<FTexture2DResource*>(Texture->Resource)->GetTexture2DRHI();
	}

	for (auto Idx = 0; Idx < NvHair::ETextureType::COUNT_OF; ++Idx)
	{
		auto TextureRef = HairTextures[Idx];
		::HairWorks::GetSDK()->setTexture(
			HairInstanceId,
			(NvHair::ETextureType)Idx,
			NvCo::Dx11Type::wrap(static_cast<ID3D11ShaderResourceView*>(TextureRef ? TextureRef->GetNativeShaderResourceView() : nullptr))
		);
	}

	// Add pin meshes
	HairPinMeshes = DynamicData.PinMeshes;
}

void FHairWorksSceneProxy::UpdateMorphIndices_RenderThread(const TArray<int32>& MorphIndices)
{
	if (MorphIndexBuffer.NumBytes != MorphIndices.Num() * sizeof(int32))
		MorphIndexBuffer.Initialize(sizeof(int32), MorphIndices.Num(), EPixelFormat::PF_R32_SINT);

	if (MorphIndexBuffer.NumBytes <= 0)
		return;

	void* LockedMorphIndices = RHILockVertexBuffer(MorphIndexBuffer.Buffer, 0, MorphIndexBuffer.NumBytes, EResourceLockMode::RLM_WriteOnly);

	FMemory::BigBlockMemcpy(LockedMorphIndices, MorphIndices.GetData(), MorphIndexBuffer.NumBytes);

	RHIUnlockVertexBuffer(MorphIndexBuffer.Buffer);

}

void FHairWorksSceneProxy::PreSimulate(FRHICommandList& RHICmdList)
{
	// Pass morph delta to HairWorks
	if (GFrameNumberRenderThread > MorphVertexUpdateFrameNumber)
		return;		

	if (MorphVertexBuffer && MorphIndexBuffer.NumBytes > 0)
	{
		// Create buffers
		const auto VertexCount = MorphIndexBuffer.NumBytes / sizeof(int32);

		if (MorphPositionDeltaBuffer.NumBytes != VertexCount * sizeof(FVector))
		{
			MorphPositionDeltaBuffer.Initialize(sizeof(FVector), VertexCount);
			MorphNormalDeltaBuffer.Initialize(sizeof(FVector), VertexCount);
		}

		// Copy position and normal delta
		TShaderMapRef<FHairWorksCopyMorphDeltasCs> CopyMorphDeltasCs(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

		RHICmdList.SetComputeShader(CopyMorphDeltasCs->GetComputeShader());

		SetShaderValue(RHICmdList, CopyMorphDeltasCs->GetComputeShader(), CopyMorphDeltasCs->MorphVertexCount, VertexCount);
		SetSRVParameter(RHICmdList, CopyMorphDeltasCs->GetComputeShader(), CopyMorphDeltasCs->MorphIndexBuffer, MorphIndexBuffer.SRV);
		SetSRVParameter(RHICmdList, CopyMorphDeltasCs->GetComputeShader(), CopyMorphDeltasCs->MorphVertexBuffer, MorphVertexBuffer);
		SetUAVParameter(RHICmdList, CopyMorphDeltasCs->GetComputeShader(), CopyMorphDeltasCs->MorphPositionDeltaBuffer, MorphPositionDeltaBuffer.UAV);
		SetUAVParameter(RHICmdList, CopyMorphDeltasCs->GetComputeShader(), CopyMorphDeltasCs->MorphNormalDeltaBuffer, MorphNormalDeltaBuffer.UAV);

		RHICmdList.DispatchComputeShader(VertexCount / 256 + 1, 1, 1);

		SetUAVParameter(RHICmdList, CopyMorphDeltasCs->GetComputeShader(), CopyMorphDeltasCs->MorphPositionDeltaBuffer, nullptr);
		SetUAVParameter(RHICmdList, CopyMorphDeltasCs->GetComputeShader(), CopyMorphDeltasCs->MorphNormalDeltaBuffer, nullptr);

		// In editor, it would be invalid sometimes. So set it to null and wait for update. 
		MorphVertexBuffer = nullptr;
	}
	else
	{
		MorphPositionDeltaBuffer.Release();
		MorphNormalDeltaBuffer.Release();
	}

	// Pass morph data
	::HairWorks::GetSDK()->updateMorphDeltas(
		HairInstanceId,
		NvCo::Dx11Type::wrap(::HairWorks::GetD3DHelper().GetShaderResourceView(MorphPositionDeltaBuffer.SRV)),
		NvCo::Dx11Type::wrap(::HairWorks::GetD3DHelper().GetShaderResourceView(MorphNormalDeltaBuffer.SRV))
	);
}
// @third party code - END HairWorks
