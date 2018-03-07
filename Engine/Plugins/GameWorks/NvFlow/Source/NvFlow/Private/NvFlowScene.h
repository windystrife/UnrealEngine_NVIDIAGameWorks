// NvFlow begin

#pragma once

#define WITH_NVFLOW_BACKEND PLATFORM_WINDOWS

#if WITH_NVFLOW_BACKEND

namespace NvFlow
{
	struct Scene;

	struct Context
	{
		Context() { }
		~Context() { release(); }

		FCriticalSection m_criticalSection;

		void init(FRHICommandListImmediate& RHICmdList);
		void conditionalInitMultiGPU(FRHICommandListImmediate& RHICmdList);
		void interopBegin(FRHICommandList& RHICmdList, bool computeOnly, bool updateRenderTarget);
		void interopEnd(FRHICommandList& RHICmdList, bool computeOnly, bool shouldFlush);

		void updateGridView(FRHICommandListImmediate& RHICmdList);
		void renderScene(FRHICommandList& RHICmdList, const FViewInfo& View, FFlowGridSceneProxy* FlowGridSceneProxy);
		void renderScenePreComposite(FRHICommandList& RHICmdList, const FViewInfo& View, FFlowGridSceneProxy* FlowGridSceneProxy);
		void release();

		void updateScene(FRHICommandListImmediate& RHICmdList, FFlowGridSceneProxy* FlowGridSceneProxy, bool& shouldFlush, const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData);

		TArray<Scene*> m_sceneList;
		TArray<Scene*> m_cleanupSceneList;

		NvFlowInterop* m_flowInterop = nullptr;
		NvFlowContext* m_renderContext = nullptr;
		NvFlowDepthStencilView* m_dsv = nullptr;
		NvFlowRenderTargetView* m_rtv = nullptr;

		int m_maxFramesInFlight = 3u;
		int m_framesInFlightMultiGPU = 0;
		int m_framesInFlightAsyncCompute = 0;

		// optional graphics GPU queues
		NvFlowDevice* m_renderDevice = nullptr;
		NvFlowDeviceQueue* m_renderCopyQueue = nullptr;
		NvFlowDeviceQueue* m_renderDeviceComputeQueue = nullptr;
		NvFlowContext* m_renderCopyContext = nullptr;
		NvFlowContext* m_renderDeviceComputeContext = nullptr;

		// optional simulation GPU queues
		NvFlowDevice* m_gridDevice = nullptr;
		NvFlowDeviceQueue* m_gridQueue = nullptr;
		NvFlowDeviceQueue* m_gridCopyQueue = nullptr;
		NvFlowContext* m_gridContext = nullptr;
		NvFlowContext* m_gridCopyContext = nullptr;

		bool m_multiGPUSupported = false;
		bool m_multiGPUActive = false;

		bool m_asyncComputeSupported = false;
		bool m_asyncComputeActive = false;

		bool m_needNvFlowDeferredRelease = false;

		TMap<const class UStaticMesh*, NvFlowShapeSDF*> m_mapForShapeSDF;

		// deferred mechanism for proper RHI command list support
		void initDeferred(IRHICommandContext* RHICmdCtx);
		void conditionalInitMultiGPUDeferred(IRHICommandContext* RHICmdCtx);
		void interopBeginDeferred(IRHICommandContext* RHICmdCtx, bool computeOnly, bool updateRenderTarget, const FTexture2DRHIRef& sceneDepthSurface, const FTexture2DRHIRef& sceneDepthTexture);
		void interopEndDeferred(IRHICommandContext* RHICmdCtx, bool computeOnly, bool shouldFlush);
		void cleanupSceneListDeferred();

		struct InteropBeginEndParams
		{
			Context* context;
			bool computeOnly;
			bool shouldFlush;
			bool updateRenderTarget;

			FTexture2DRHIRef sceneDepthSurface;
			FTexture2DRHIRef sceneDepthTexture;
		};

		static void initCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx);
		static void conditionalInitMultiGPUCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx);
		static void interopBeginCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx);
		static void interopEndCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx);
		static void cleanupSceneListCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx);

		static void updateGridViewStart(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx);
		static void updateGridViewFinish(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx);
	};

	struct Scene
	{
		Scene() {}
		~Scene();

		void init(Context* context, FRHICommandListImmediate& RHICmdList, FFlowGridSceneProxy* InFlowGridSceneProxy);
		void release();
		void updateParameters(FRHICommandListImmediate& RHICmdList);

		void updateSubstep(FRHICommandListImmediate& RHICmdList, float dt, uint32 substep, uint32 numSubsteps, bool& shouldFlush, const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData);
		void finalizeUpdate(FRHICommandListImmediate& RHICmdList);

		void updateGridView(FRHICommandListImmediate& RHICmdList);
		void render(FRHICommandList& RHICmdList, const FViewInfo& View);
		void renderDepth(FRHICommandList& RHICmdList, const FViewInfo& View);

		bool getExportParams(FRHICommandListImmediate& RHICmdList, GridExportParamsNvFlow& OutParams);

		struct CallbackUserData
		{
			NvFlow::Scene* Scene;
			IRHICommandContext* RHICmdCtx;
			float DeltaTime;
			const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData;
		};

		static void sEmitCustomAllocCallback(void* userdata, const NvFlowGridEmitCustomAllocParams* params)
		{
			CallbackUserData* callbackUserData = (CallbackUserData*)userdata;
			callbackUserData->Scene->emitCustomAllocCallback(callbackUserData->RHICmdCtx, params, callbackUserData->GlobalDistanceFieldParameterData);
		}
		static void sEmitCustomEmitVelocityCallback(void* userdata, NvFlowUint* dataFrontIdx, const NvFlowGridEmitCustomEmitParams* params)
		{
			CallbackUserData* callbackUserData = (CallbackUserData*)userdata;
			callbackUserData->Scene->emitCustomEmitVelocityCallback(callbackUserData->RHICmdCtx, dataFrontIdx, params, callbackUserData->GlobalDistanceFieldParameterData, callbackUserData->DeltaTime);
		}
		static void sEmitCustomEmitDensityCallback(void* userdata, NvFlowUint* dataFrontIdx, const NvFlowGridEmitCustomEmitParams* params)
		{
			CallbackUserData* callbackUserData = (CallbackUserData*)userdata;
			callbackUserData->Scene->emitCustomEmitDensityCallback(callbackUserData->RHICmdCtx, dataFrontIdx, params, callbackUserData->GlobalDistanceFieldParameterData, callbackUserData->DeltaTime);
		}
		void emitCustomAllocCallback(IRHICommandContext* RHICmdCtx, const NvFlowGridEmitCustomAllocParams* params, const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData);
		void emitCustomEmitVelocityCallback(IRHICommandContext* RHICmdCtx, NvFlowUint* dataFrontIdx, const NvFlowGridEmitCustomEmitParams* params, const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData, float dt);
		void emitCustomEmitDensityCallback(IRHICommandContext* RHICmdCtx, NvFlowUint* dataFrontIdx, const NvFlowGridEmitCustomEmitParams* params, const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData, float dt);

		void applyDistanceField(IRHICommandContext* RHICmdCtx, NvFlowUint dataFrontIdx, const NvFlowGridEmitCustomEmitLayerParams& layerParams, const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData, float dt,
			FShaderResourceViewRHIRef DataInSRV, FUnorderedAccessViewRHIRef DataOutUAV, FShaderResourceViewRHIRef BlockListSRV, FShaderResourceViewRHIRef BlockTableSRV,
			float InSlipFactor = 0, float InSlipThickness = 0, FVector4 InEmitValue = FVector4(ForceInitToZero));

		uint64 LatestVersion = 0ul;

		bool m_multiAdapter = false;
		bool m_asyncCompute = false;

#if NVFLOW_ADAPTIVE
		float m_frameTimeSum = 0.f;
		float m_frameTimeCount = 0.f;
		float m_frameTimeAverage = 0.f;
		float m_currentAdaptiveScale = -1.f;
#endif

		Context* m_context = nullptr;

		// Cache context pointers here, since some grid can be multi-GPU, some not
		NvFlowContext* m_renderContext = nullptr;
		NvFlowContext* m_gridContext = nullptr;
		NvFlowContext* m_gridCopyContext = nullptr;
		NvFlowContext* m_renderCopyContext = nullptr;

		NvFlowGrid* m_grid = nullptr;
		NvFlowGridProxy* m_gridProxy = nullptr;
		NvFlowVolumeRender* m_volumeRender = nullptr;
		NvFlowVolumeShadow* m_volumeShadow = nullptr;
		NvFlowRenderMaterialPool* m_renderMaterialPool = nullptr;

		float m_shadowMinResidentScale;
		float m_shadowMaxResidentScale;
		uint32 m_shadowResolution;

		FMatrix m_shadowWorldToLight;
		uint8 m_shadowLightType = LightType_MAX;
		float m_shadowOuterConeAngle;
		float m_shadowRadius;

		NvFlowGridExport* m_gridExport4Render = nullptr;

		NvFlowGridDesc m_gridDesc;
		NvFlowGridParams m_gridParams;
		NvFlowVolumeRenderParams m_renderParams;

		FFlowGridSceneProxy* FlowGridSceneProxy = nullptr;

		TArray<ParticleSimulationParamsNvFlow> m_particleParamsArray;

		TArray<NvFlowShapeSDF*> m_sdfs;

		struct RenderMaterialData
		{
			enum
			{
				RELEASED = 2u,
				PENDING_RELEASE = 0u,
				CREATED = 1u
			};
			uint32 state = RELEASED;

			NvFlowRenderMaterialHandle renderMaterialHandle;
		};

		struct MaterialData
		{
			NvFlowGridMaterialHandle gridMaterialHandle;
			uint32 emitMaterialIndex;

			TMap<FlowRenderMaterialKeyType, RenderMaterialData> renderMaterialMap;
		};
		TMap<FlowMaterialKeyType, MaterialData> m_materialMap;
		TArray<NvFlowGridMaterialHandle> m_emitMaterialsArray;

		const MaterialData& updateMaterial(FlowMaterialKeyType materialKey, FlowMaterialKeyType defaultKey, bool particleMode, const FFlowMaterialParams& materialParams);

		// deferred mechanism for proper RHI command list support
		float m_updateSubstep_dt = 0.f;

		struct UpdateParams
		{
			Scene* Scene;
			const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData;
		};
		struct RenderParams
		{
			Scene* scene;
			NvFlowVolumeRenderParams volumeRenderParams;
		};

		void initDeferred(IRHICommandContext* RHICmdCtx);
		void updateParametersDeferred(IRHICommandContext* RHICmdCtx);
		void updateSubstepDeferred(IRHICommandContext* RHICmdCtx, UpdateParams* updateParams);
		void finalizeUpdateDeferred(IRHICommandContext* RHICmdCtx);
		void updateGridViewDeferred(IRHICommandContext* RHICmdCtx);
		void renderDeferred(IRHICommandContext* RHICmdCtx, RenderParams* renderParams);
		void renderDepthDeferred(IRHICommandContext* RHICmdCtx, RenderParams* renderParams);

		static void initCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx);
		static void updateParametersCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx);
		static void updateSubstepCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx);
		static void finalizeUpdateCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx);
		static void updateGridViewCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx);
		static void renderCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx);
		static void renderDepthCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx);
	};

	void cleanupContext(void* ptr);
	void cleanupScene(void* ptr);

	extern Context gContextImpl;
	extern Context* gContext;

	inline uint32 getThreadID()
	{
		return FPlatformTLS::GetCurrentThreadId();
	}
}

#endif

// NvFlow end