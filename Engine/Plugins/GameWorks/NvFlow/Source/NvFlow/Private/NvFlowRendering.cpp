#include "NvFlowCommon.h"

#include "RHI.h"
#include "NvFlowInterop.h"

#include "FlowGridAsset.h"
#include "FlowGridActor.h"
#include "FlowGridComponent.h"
#include "FlowEmitterComponent.h"
#include "FlowMaterial.h"
#include "FlowRenderMaterial.h"
#include "FlowGridSceneProxy.h"

#include "NvFlowModule.h"

// NvFlow begin
#if WITH_NVFLOW

DECLARE_LOG_CATEGORY_EXTERN(LogFlow, Log, All);
DEFINE_LOG_CATEGORY(LogFlow);

/*=============================================================================
NFlowRendering.cpp: Translucent rendering implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "RenderUtils.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "PostProcess/SceneFilterRendering.h"
#include "LightPropagationVolume.h"
#include "SceneUtils.h"
#include "HardwareInfo.h"

#include "Stats.h"
#include "GridAccessHooksNvFlow.h"


#if NVFLOW_ADAPTIVE
// For HMD support
#include "IHeadMountedDisplay.h"
#endif

// For dedicate GPU support
#if WITH_CUDA_CONTEXT
#include "PhysicsPublic.h"
#endif

DEFINE_STAT(STAT_Flow_SimulateGrids);
DEFINE_STAT(STAT_Flow_RenderGrids);

#include "NvFlowScene.h"

#if WITH_NVFLOW_BACKEND

namespace NvFlow
{
	Context gContextImpl;
	Context* gContext = nullptr;
}

// ------------------ NvFlow::Context -----------------

void NvFlow::cleanupContext(void* ptr)
{
	auto context = (Context*)ptr;
	if (context)
	{
		context->release();
	}
	gContext = nullptr;
}

void NvFlow::cleanupScene(void* ptr)
{
	auto scene = (Scene*)ptr;
	if (scene)
	{
		scene->m_context->m_criticalSection.Lock();

		TArray<Scene*>& sceneList = scene->m_context->m_sceneList;
		check(sceneList.Contains(scene));

		UE_LOG(LogFlow, Display, TEXT("NvFlow cleanup scene %p scheduled tid(%d)"), scene, getThreadID());

		scene->m_context->m_cleanupSceneList.Add(scene);

		sceneList.RemoveSingleSwap(scene);

		scene->m_context->m_criticalSection.Unlock();
	}
}

void NvFlow::Context::cleanupSceneListDeferred()
{
	m_criticalSection.Lock();
	for (int32 sceneIdx = 0u; sceneIdx < m_cleanupSceneList.Num(); sceneIdx++)
	{
		auto scene = m_cleanupSceneList[sceneIdx];

		UE_LOG(LogFlow, Display, TEXT("NvFlow cleanup scene %p executed tid(%d)"), scene, getThreadID());
		
		delete scene;
	}
	m_cleanupSceneList.Reset();
	m_criticalSection.Unlock();
}

void NvFlow::Context::cleanupSceneListCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx)
{
	auto context = (Context*)paramData;
	context->cleanupSceneListDeferred();
}

void NvFlow::Context::init(FRHICommandListImmediate& RHICmdList)
{
	UE_LOG(LogFlow, Display, TEXT("NvFlow Context Init"));

	m_needNvFlowDeferredRelease = true;

	RHICmdList.NvFlowWork(initCallback, this, 0u);
}

void NvFlow::Context::initCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx)
{
	auto context = (NvFlow::Context*)paramData;
	context->initDeferred(RHICmdCtx);
}

void NvFlow::Context::conditionalInitMultiGPU(FRHICommandListImmediate& RHICmdList)
{
	//UE_LOG(LogFlow, Display, TEXT("NvFlow Context MultiGPU init"));

	RHICmdList.NvFlowWork(conditionalInitMultiGPUCallback, this, 0u);
}

void NvFlow::Context::conditionalInitMultiGPUCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx)
{
	auto context = (NvFlow::Context*)paramData;
	context->conditionalInitMultiGPUDeferred(RHICmdCtx);
}

void NvFlow::Context::conditionalInitMultiGPUDeferred(IRHICommandContext* RHICmdCtx)
{
	auto& appctx = *RHICmdCtx;
	if (!m_multiGPUActive)
	{
		m_multiGPUActive = m_multiGPUSupported && (UFlowGridAsset::sGlobalMultiGPU > 0);
	}
	if (!m_asyncComputeActive)
	{
		m_asyncComputeActive = m_asyncComputeSupported && (UFlowGridAsset::sGlobalAsyncCompute > 0);
	}
	if (m_multiGPUActive || m_asyncComputeActive)
	{
		// All multi-queue systems have a proxy device and copy queue
		if (m_renderDevice == nullptr)
		{
			NvFlowDeviceDesc deviceDesc = {};
			NvFlowDeviceDescDefaultsInline(&deviceDesc);
			deviceDesc.mode = eNvFlowDeviceModeProxy;

			m_renderDevice = NvFlowCreateDevice(m_renderContext, &deviceDesc);

			NvFlowDeviceQueueDesc deviceQueueDesc = {};
			deviceQueueDesc.queueType = eNvFlowDeviceQueueTypeCopy;
			deviceQueueDesc.lowLatency = false;
			m_renderCopyQueue = NvFlowCreateDeviceQueue(m_renderDevice, &deviceQueueDesc);
			m_renderCopyContext = NvFlowDeviceQueueCreateContext(m_renderCopyQueue);
		}

		// async compute just adds a compute queue on the render device
		if (m_asyncComputeActive && m_renderDeviceComputeQueue == nullptr)
		{
			NvFlowDeviceQueueDesc deviceQueueDesc = {};
			deviceQueueDesc.queueType = eNvFlowDeviceQueueTypeCompute;
			deviceQueueDesc.lowLatency = true;

			m_renderDeviceComputeQueue = NvFlowCreateDeviceQueue(m_renderDevice, &deviceQueueDesc);
			m_renderDeviceComputeContext = NvFlowDeviceQueueCreateContext(m_renderDeviceComputeQueue);
		}

		// multiGPU adds a unique device with a direct queue and a copy queue
		if (m_multiGPUActive && m_gridDevice == nullptr)
		{
			NvFlowDeviceDesc deviceDesc = {};
			NvFlowDeviceDescDefaultsInline(&deviceDesc);
			deviceDesc.mode = eNvFlowDeviceModeUnique;

			m_gridDevice = NvFlowCreateDevice(m_renderContext, &deviceDesc);

			NvFlowDeviceQueueDesc deviceQueueDesc = {};
			deviceQueueDesc.queueType = eNvFlowDeviceQueueTypeGraphics;
			deviceQueueDesc.lowLatency = false;

			m_gridQueue = NvFlowCreateDeviceQueue(m_gridDevice, &deviceQueueDesc);
			m_gridContext = NvFlowDeviceQueueCreateContext(m_gridQueue);

			deviceQueueDesc.queueType = eNvFlowDeviceQueueTypeCopy;
			m_gridCopyQueue = NvFlowCreateDeviceQueue(m_gridDevice, &deviceQueueDesc);
			m_gridCopyContext = NvFlowDeviceQueueCreateContext(m_gridCopyQueue);

			NvFlowDeviceQueueStatus status = {};
			NvFlowDeviceQueueUpdateContext(m_gridQueue, m_gridContext, &status);
		}
	}
}

void NvFlow::Context::initDeferred(IRHICommandContext* RHICmdCtx)
{
	auto& appctx = *RHICmdCtx;

	FString RHIName = TEXT("");
	{
		// Create the folder name based on the hardware specs we have been provided
		FString HardwareDetails = FHardwareInfo::GetHardwareDetailsString();
		FString RHILookup = NAME_RHI.ToString() + TEXT("=");
		FParse::Value(*HardwareDetails, *RHILookup, RHIName);
	}
	if (RHIName == TEXT("D3D11"))
	{
		m_flowInterop = NvFlowCreateInteropD3D11();
	}
	else
	if (RHIName == TEXT("D3D12"))
	{
		m_flowInterop = NvFlowCreateInteropD3D12();
	}
	else
	{
		UE_LOG(LogInit, Error, TEXT("Unsupported RHI type: %s"), *RHIName);
	}
	m_renderContext = m_flowInterop->CreateContext(appctx);

	// register cleanup
	m_flowInterop->CleanupFunc(appctx, cleanupContext, this);

	// create compute device if available
	// NVCHANGE: LRR - Check that the device is also dedicated to PhysX from the control panel
	bool bDedicatedPhysXGPU = true;
#if WITH_CUDA_CONTEXT
	NvIsPhysXHighSupported(bDedicatedPhysXGPU);
	UE_LOG(LogInit, Display, TEXT("NvFlow using dedicated PhysX GPU: %s"), bDedicatedPhysXGPU ? TEXT("true") : TEXT("false"));
#endif
	m_multiGPUSupported = NvFlowDedicatedDeviceAvailable(m_renderContext) && bDedicatedPhysXGPU;
	m_asyncComputeSupported = NvFlowDedicatedDeviceQueueAvailable(m_renderContext);
	conditionalInitMultiGPUDeferred(RHICmdCtx);
}

void NvFlow::Context::interopBegin(FRHICommandList& RHICmdList, bool computeOnly, bool updateRenderTarget)
{
	InteropBeginEndParams params = {};
	params.context = this;
	params.computeOnly = computeOnly;
	params.shouldFlush = false;
	params.updateRenderTarget = updateRenderTarget;

	if (!computeOnly)
	{
		FSceneRenderTargets& SceneContext = RHICmdList.IsImmediate() ? FSceneRenderTargets::Get(static_cast<FRHICommandListImmediate&>(RHICmdList)) : FSceneRenderTargets::Get(RHICmdList);

		params.sceneDepthSurface = SceneContext.GetSceneDepthSurface();
		params.sceneDepthTexture = SceneContext.GetSceneDepthTexture();
	}

	RHICmdList.NvFlowWork(interopBeginCallback, &params, sizeof(params));
}

void NvFlow::Context::interopBeginCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx)
{
	auto params = (NvFlow::Context::InteropBeginEndParams*)paramData;
	params->context->interopBeginDeferred(RHICmdCtx, params->computeOnly, params->updateRenderTarget, params->sceneDepthSurface, params->sceneDepthTexture);
}

void NvFlow::Context::interopBeginDeferred(IRHICommandContext* RHICmdCtx, bool computeOnly, bool updateRenderTarget, const FTexture2DRHIRef& sceneDepthSurface, const FTexture2DRHIRef& sceneDepthTexture)
{
	auto& appctx = *RHICmdCtx;

	m_flowInterop->UpdateContext(appctx, m_renderContext);
	if (!computeOnly)
	{
		if (updateRenderTarget)
		{
			if (m_rtv == nullptr) m_rtv = m_flowInterop->CreateRenderTargetView(appctx, m_renderContext);

			m_flowInterop->UpdateRenderTargetView(appctx, m_renderContext, m_rtv);
		}

		if (m_dsv == nullptr) m_dsv = m_flowInterop->CreateDepthStencilView(appctx, sceneDepthSurface, sceneDepthTexture, m_renderContext);

		m_flowInterop->UpdateDepthStencilView(appctx, sceneDepthSurface, sceneDepthTexture, m_renderContext, m_dsv);
	}

	if (m_gridDevice)
	{
		NvFlowDeviceQueueStatus status = {};
		NvFlowDeviceQueueUpdateContext(m_gridQueue, m_gridContext, &status);
		m_framesInFlightMultiGPU = status.framesInFlight;

		NvFlowDeviceQueueUpdateContext(m_gridCopyQueue, m_gridCopyContext, &status);
	}
	if (m_renderDevice)
	{
		NvFlowDeviceQueueStatus status = {};
		NvFlowDeviceQueueUpdateContext(m_renderCopyQueue, m_renderCopyContext, &status);
	}
	if (m_renderDeviceComputeContext)
	{
		NvFlowDeviceQueueStatus status = {};
		NvFlowDeviceQueueUpdateContext(m_renderDeviceComputeQueue, m_renderDeviceComputeContext, &status);
		m_framesInFlightAsyncCompute = status.framesInFlight;
	}

	m_flowInterop->Push(appctx, m_renderContext);
}

void NvFlow::Context::interopEnd(FRHICommandList& RHICmdList, bool computeOnly, bool shouldFlush)
{
	InteropBeginEndParams params = {};
	params.context = this;
	params.computeOnly = computeOnly;
	params.shouldFlush = shouldFlush;
	RHICmdList.NvFlowWork(interopEndCallback, &params, sizeof(params));
}

void NvFlow::Context::interopEndCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx)
{
	auto params = (NvFlow::Context::InteropBeginEndParams*)paramData;
	params->context->interopEndDeferred(RHICmdCtx, params->computeOnly, params->shouldFlush);
}

void NvFlow::Context::interopEndDeferred(IRHICommandContext* RHICmdCtx, bool computeOnly, bool shouldFlush)
{
	auto& appctx = *RHICmdCtx;

	if (computeOnly/* && shouldFlush*/)
	{
		if (m_gridDevice)
		{
			NvFlowDeviceQueueConditionalFlush(m_gridQueue, m_gridContext);
			NvFlowDeviceQueueConditionalFlush(m_gridCopyQueue, m_gridCopyContext);
		}
		if (m_renderDevice)
		{
			NvFlowDeviceQueueConditionalFlush(m_renderCopyQueue, m_renderCopyContext);
		}
		if (m_renderDeviceComputeContext)
		{
			NvFlowDeviceQueueConditionalFlush(m_renderDeviceComputeQueue, m_renderDeviceComputeContext);
		}
	}

	m_flowInterop->Pop(appctx, m_renderContext);
}

void NvFlow::Context::updateGridView(FRHICommandListImmediate& RHICmdList)
{
	RHICmdList.NvFlowWork(updateGridViewStart, this, 0u);

	// iterate scenes and update grid view
	for (int32 i = 0; i < m_sceneList.Num(); i++)
	{
		Scene* scene = m_sceneList[i];
		scene->updateGridView(RHICmdList);
	}

	RHICmdList.NvFlowWork(updateGridViewFinish, this, 0u);
}

void NvFlow::Context::updateGridViewStart(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx)
{
	NvFlowDebugInfoQueue.StartSubmitInfo();
}

void NvFlow::Context::updateGridViewFinish(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx)
{
	NvFlowDebugInfoQueue.FinishSubmitInfo();
}


void NvFlow::Context::renderScene(FRHICommandList& RHICmdList, const FViewInfo& View, FFlowGridSceneProxy* FlowGridSceneProxy)
{
	if (FlowGridSceneProxy->scenePtr != nullptr)
	{
		auto scene = (Scene*)FlowGridSceneProxy->scenePtr;
		scene->render(RHICmdList, View);
	}
}

void NvFlow::Context::renderScenePreComposite(FRHICommandList& RHICmdList, const FViewInfo& View, FFlowGridSceneProxy* FlowGridSceneProxy)
{
	if (FlowGridSceneProxy->scenePtr != nullptr)
	{
		auto scene = (Scene*)FlowGridSceneProxy->scenePtr;
		scene->renderDepth(RHICmdList, View);
	}
}

void NvFlow::Context::release()
{
	cleanupSceneListDeferred();

	// proxies and scenes should all be released by now
	check(m_sceneList.Num() == 0);

	if (m_renderContext)
	{
		UE_LOG(LogFlow, Display, TEXT("NvFlow Context Cleanup"));
	}

	if (m_rtv) NvFlowReleaseRenderTargetView(m_rtv);
	if (m_dsv) NvFlowReleaseDepthStencilView(m_dsv);
	if (m_renderContext) NvFlowReleaseContext(m_renderContext);

	if (m_gridContext) NvFlowReleaseContext(m_gridContext);
	if (m_gridCopyContext) NvFlowReleaseContext(m_gridCopyContext);
	if (m_renderCopyContext) NvFlowReleaseContext(m_renderCopyContext);
	if (m_renderDeviceComputeContext) NvFlowReleaseContext(m_renderDeviceComputeContext);

	if (m_gridQueue) NvFlowReleaseDeviceQueue(m_gridQueue);
	if (m_gridCopyQueue) NvFlowReleaseDeviceQueue(m_gridCopyQueue);
	if (m_renderCopyQueue) NvFlowReleaseDeviceQueue(m_renderCopyQueue);
	if (m_renderDeviceComputeQueue) NvFlowReleaseDeviceQueue(m_renderDeviceComputeQueue);

	if (m_gridDevice) NvFlowReleaseDevice(m_gridDevice);
	if (m_renderDevice) NvFlowReleaseDevice(m_renderDevice);

	m_renderDevice = nullptr;
	m_renderCopyQueue = nullptr;
	m_renderDeviceComputeQueue = nullptr;
	m_renderCopyContext = nullptr;
	m_renderDeviceComputeContext = nullptr;

	m_gridDevice = nullptr;
	m_gridQueue = nullptr;
	m_gridCopyQueue = nullptr;
	m_gridContext = nullptr;
	m_gridCopyContext = nullptr;

	if (m_flowInterop) NvFlowReleaseInterop(m_flowInterop);

	m_rtv = nullptr;
	m_dsv = nullptr;
	m_renderContext = nullptr;
	m_flowInterop = nullptr;

	if (m_needNvFlowDeferredRelease)
	{
		NvFlowDeferredRelease(1000.f);
	}
}

void NvFlow::Context::updateScene(FRHICommandListImmediate& RHICmdList, FFlowGridSceneProxy* FlowGridSceneProxy, bool& shouldFlush, const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData)
{
	// proxy not active, release scene if necessary and return
	if (!FlowGridSceneProxy->FlowGridProperties->bActive)
	{
		cleanupScene(FlowGridSceneProxy->scenePtr);
		FlowGridSceneProxy->scenePtr = nullptr;
		FlowGridSceneProxy->cleanupSceneFunc = nullptr;
		return;
	}

	// create scene if necessary
	if (FlowGridSceneProxy->scenePtr == nullptr)
	{
		Scene* newScene = new Scene();
		newScene->init(this, RHICmdList, FlowGridSceneProxy);
		m_sceneList.Add(newScene);
	}

	auto scene = (Scene*)FlowGridSceneProxy->scenePtr;

	auto Properties = FlowGridSceneProxy->FlowGridProperties;

	if (Properties->Version > scene->LatestVersion)
	{
		scene->LatestVersion = Properties->Version;

		scene->updateParameters(RHICmdList);

		// process simulation events
		if (Properties->SubstepSize > 0.0f)
		{
			for (uint32 i = 0; i < uint32(Properties->NumScheduledSubsteps); i++)
			{
				scene->updateSubstep(RHICmdList, Properties->SubstepSize, i, uint32(Properties->NumScheduledSubsteps), shouldFlush, GlobalDistanceFieldParameterData);
			}
		}
	}

	scene->finalizeUpdate(RHICmdList);
}

// ------------------ NvFlow::Scene -----------------

NvFlow::Scene::~Scene()
{
	release();
}

void NvFlow::Scene::release()
{
	if (m_context)
	{
		UE_LOG(LogFlow, Display, TEXT("NvFlow Scene %p Cleanup"), this);
	}

	if (m_grid) NvFlowReleaseGrid(m_grid);
	if (m_gridProxy) NvFlowReleaseGridProxy(m_gridProxy);
	if (m_volumeRender) NvFlowReleaseVolumeRender(m_volumeRender);
	if (m_volumeShadow) NvFlowReleaseVolumeShadow(m_volumeShadow);
	if (m_renderMaterialPool) NvFlowReleaseRenderMaterialPool(m_renderMaterialPool);

	m_grid = nullptr;
	m_gridProxy = nullptr;
	m_volumeRender = nullptr;
	m_volumeShadow = nullptr;
	m_renderMaterialPool = nullptr;

	m_context = nullptr;

	FlowGridSceneProxy = nullptr;
}

void NvFlow::Scene::init(Context* context, FRHICommandListImmediate& RHICmdList, FFlowGridSceneProxy* InFlowGridSceneProxy)
{
	UE_LOG(LogFlow, Display, TEXT("NvFlow Scene %p Init"), this);

	m_context = context;
	FlowGridSceneProxy = InFlowGridSceneProxy;
	FlowGridSceneProxy->scenePtr = this;
	FlowGridSceneProxy->cleanupSceneFunc = NvFlow::cleanupScene;

	RHICmdList.NvFlowWork(initCallback, this, 0u);
}

void NvFlow::Scene::initCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx)
{
	auto scene = (NvFlow::Scene*)paramData;
	scene->initDeferred(RHICmdCtx);
}

void NvFlow::Scene::initDeferred(IRHICommandContext* RHICmdCtx)
{
	auto& appctx = *RHICmdCtx;

	// create local grid desc copy
	m_gridDesc = FlowGridSceneProxy->FlowGridProperties->GridDesc;

	// set initial location using proxy location
	FVector FlowOrigin = FlowGridSceneProxy->GetLocalToWorld().GetOrigin() * scaleInv;
	m_gridDesc.initialLocation = *(NvFlowFloat3*)(&FlowOrigin.X);

	bool multiAdapterEnabled = FlowGridSceneProxy->FlowGridProperties->bMultiAdapterEnabled;
	bool asyncComputeEnabled = FlowGridSceneProxy->FlowGridProperties->bAsyncComputeEnabled;
	m_multiAdapter = multiAdapterEnabled && m_context->m_multiGPUActive;
	m_asyncCompute = !m_multiAdapter && asyncComputeEnabled && m_context->m_asyncComputeActive;
	if (UFlowGridAsset::sGlobalMultiGPU > 1)
	{
		m_multiAdapter = m_context->m_multiGPUActive;
	}
	if (UFlowGridAsset::sGlobalAsyncCompute > 1)
	{
		m_asyncCompute = !m_multiAdapter && m_context->m_asyncComputeActive;
	}
	if (UFlowGridAsset::sGlobalAsyncCompute > 2 && UFlowGridAsset::sGlobalAsyncCompute > UFlowGridAsset::sGlobalMultiGPU)
	{
		m_multiAdapter = false;
		m_asyncCompute = m_context->m_asyncComputeActive;
	}

	if (m_multiAdapter)
	{
		m_renderContext = m_context->m_renderContext;
		m_gridContext = m_context->m_gridContext;
		m_gridCopyContext = m_context->m_gridCopyContext;
		m_renderCopyContext = m_context->m_renderCopyContext;
	}
	else if (m_asyncCompute)
	{
		m_renderContext = m_context->m_renderContext;
		m_gridContext = m_context->m_renderDeviceComputeContext;
		m_gridCopyContext = m_context->m_renderCopyContext;
		m_renderCopyContext = m_context->m_renderCopyContext;
	}
	else
	{
		m_renderContext = m_context->m_renderContext;
		m_gridContext = m_context->m_renderContext;
		m_gridCopyContext = m_context->m_renderContext;
		m_renderCopyContext = m_context->m_renderContext;
	}

	m_grid = NvFlowCreateGrid(m_gridContext, &m_gridDesc);

	auto proxyGridExport = NvFlowGridGetGridExport(m_gridContext, m_grid);

	NvFlowGridProxyDesc proxyDesc = {};
	proxyDesc.gridContext = m_gridContext;
	proxyDesc.renderContext = m_renderContext;
	proxyDesc.gridCopyContext = m_gridCopyContext;
	proxyDesc.renderCopyContext = m_renderCopyContext;
	proxyDesc.gridExport = proxyGridExport;
	proxyDesc.proxyType = eNvFlowGridProxyTypePassThrough;
	if (m_multiAdapter)
	{
		proxyDesc.proxyType = eNvFlowGridProxyTypeMultiGPU;
	}
	else if (m_asyncCompute)
	{
		proxyDesc.proxyType = eNvFlowGridProxyTypeInterQueue;
	}

	m_gridProxy = NvFlowCreateGridProxy(&proxyDesc);

	NvFlowVolumeRenderDesc volumeRenderDesc;
	volumeRenderDesc.gridExport = NvFlowGridProxyGetGridExport(m_gridProxy, m_renderContext);

	m_volumeRender = NvFlowCreateVolumeRender(m_renderContext, &volumeRenderDesc);

	NvFlowRenderMaterialPoolDesc renderMaterialPoolDesc;
	renderMaterialPoolDesc.colorMapResolution = FlowGridSceneProxy->FlowGridProperties->ColorMapResolution;

	m_renderMaterialPool = NvFlowCreateRenderMaterialPool(m_renderContext, &renderMaterialPoolDesc);
}

static TAutoConsoleVariable<float> CVarNvFlowDepthAlphaThreshold(
	TEXT("flowdepthalphathreshold"),
	0.9f,
	TEXT("Alpha threshold for depth write")
);

static TAutoConsoleVariable<float> CVarNvFlowDepthIntensityThreshold(
	TEXT("flowdepthintensitythreshold"),
	4.f,
	TEXT("Intensity threshold for depth write")
);

void NvFlow::Scene::updateParameters(FRHICommandListImmediate& RHICmdList)
{
	auto& appctx = RHICmdList.GetContext();

	const FFlowGridProperties& Properties = *FlowGridSceneProxy->FlowGridProperties;

	// configure grid params
	m_gridParams = Properties.GridParams;

	// configure render params
	NvFlowVolumeRenderParamsDefaultsInline(&m_renderParams);
	m_renderParams.renderMode = Properties.RenderParams.RenderMode;
	m_renderParams.renderChannel = Properties.RenderParams.RenderChannel;
	m_renderParams.debugMode = Properties.RenderParams.bDebugWireframe;
	m_renderParams.materialPool = m_renderMaterialPool;

	m_renderParams.generateDepth = (UFlowGridAsset::sGlobalDepth > 0) && (Properties.RenderParams.bGenerateDepth || (UFlowGridAsset::sGlobalDepth > 1));
	m_renderParams.generateDepthDebugMode = (UFlowGridAsset::sGlobalDepthDebugDraw > 0);
	if (UFlowGridAsset::sGlobalDepth > 1)
	{
		m_renderParams.depthAlphaThreshold = CVarNvFlowDepthAlphaThreshold.GetValueOnRenderThread();
		m_renderParams.depthIntensityThreshold = CVarNvFlowDepthIntensityThreshold.GetValueOnRenderThread();
	}
	else
	{
		m_renderParams.depthAlphaThreshold = Properties.RenderParams.DepthAlphaThreshold;
		m_renderParams.depthIntensityThreshold = Properties.RenderParams.DepthIntensityThreshold;
	}

#if NVFLOW_ADAPTIVE
	// adaptive screen percentage
	bool bHMDConnected = (GEngine && GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHMDConnected());
	if (Properties.RenderParams.bAdaptiveScreenPercentage && bHMDConnected)
	{
		const float decayRate = 0.98f;
		const float reactRate = 0.002f;
		const float recoverRate = 0.001f;

		if (m_currentAdaptiveScale < 0.f) m_currentAdaptiveScale = Properties.RenderParams.MaxScreenPercentage;

		//float lastFrameTime = FPlatformTime::ToMilliseconds(GGPUFrameTime);

		float lastFrameTime = 1000.f * float(FApp::GetCurrentTime() - FApp::GetLastTime());
		if (GEngine)
		{
			if (GEngine->HMDDevice.Get())
			{
				float timing = GEngine->HMDDevice->GetFrameTiming();
				if (timing > 1.f)
				{
					lastFrameTime = timing;
				}
			}
		}

		float targetFrameTime = Properties.RenderParams.AdaptiveTargetFrameTime;

		m_frameTimeSum += lastFrameTime;
		m_frameTimeCount += 1.f;
		m_frameTimeSum *= decayRate;
		m_frameTimeCount *= decayRate;

		m_frameTimeAverage = float(m_frameTimeSum) / float(m_frameTimeCount);

		float error = m_frameTimeAverage - targetFrameTime;
		if (error > 0.f)
		{
			m_currentAdaptiveScale -= reactRate;
		}
		else if (error < 0.f)
		{
			m_currentAdaptiveScale += recoverRate;
		}

		// enforce clamps
		if (m_currentAdaptiveScale < Properties.RenderParams.MinScreenPercentage)
		{
			m_currentAdaptiveScale = Properties.RenderParams.MinScreenPercentage;
		}
		if (m_currentAdaptiveScale > Properties.RenderParams.MaxScreenPercentage)
		{
			m_currentAdaptiveScale = Properties.RenderParams.MaxScreenPercentage;
		}

		// ScreenPercentage > 1.0 not supported (would require reallocation)
		if (m_currentAdaptiveScale > 1.f)
		{
			m_currentAdaptiveScale = 1.f;
		}

		m_renderParams.screenPercentage = m_currentAdaptiveScale;
	}
	else
#endif
	{
		m_renderParams.screenPercentage = Properties.RenderParams.MaxScreenPercentage;
	}

	// deferred parameter updates
	RHICmdList.NvFlowWork(updateParametersCallback, this, 0u);
}

void NvFlow::Scene::updateParametersCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx)
{
	auto scene = (NvFlow::Scene*)paramData;
	scene->updateParametersDeferred(RHICmdCtx);
}

namespace
{
	void NvFlowCopyDistanceFieldFP16(NvFlowShapeSDFData& mappedShapeData, const TArray<uint8>& UncompressedData)
	{
		for (NvFlowUint z = 0; z < mappedShapeData.dim.z; ++z)
		{
			for (NvFlowUint y = 0; y < mappedShapeData.dim.y; ++y)
			{
				for (NvFlowUint x = 0; x < mappedShapeData.dim.x; ++x)
				{
					uint32 idx = x + mappedShapeData.dim.x * (y + mappedShapeData.dim.y * z);

					FFloat16 val16f;
					val16f.Encoded = UncompressedData[2 * idx + 0];
					val16f.Encoded |= ((UncompressedData[2 * idx + 1]) << 8u);
					float value = val16f.GetFloat();

					mappedShapeData.data[x + mappedShapeData.rowPitch * y + mappedShapeData.depthPitch * z] = value;
				}
			}
		}
	}

	void NvFlowCopyDistanceFieldG8(NvFlowShapeSDFData& mappedShapeData, const TArray<uint8>& UncompressedData,
		const FVector2D& DistanceFieldMAD)
	{
		for (NvFlowUint z = 0; z < mappedShapeData.dim.z; ++z)
		{
			for (NvFlowUint y = 0; y < mappedShapeData.dim.y; ++y)
			{
				for (NvFlowUint x = 0; x < mappedShapeData.dim.x; ++x)
				{
					uint32 idx = x + mappedShapeData.dim.x * (y + mappedShapeData.dim.y * z);

					uint32 val32u = UncompressedData[idx];
					float value = (1.f / 255.f) * (float(val32u)) * DistanceFieldMAD.X + DistanceFieldMAD.Y;

					mappedShapeData.data[x + mappedShapeData.rowPitch * y + mappedShapeData.depthPitch * z] = value;
				}
			}
		}
	}

	void NvFlowCopyDistanceField(NvFlowShapeSDFData& mappedShapeData, const TArray<uint8>& UncompressedData,
		const FVector2D& DistanceFieldMAD, const EPixelFormat Format)
	{
		if (Format == PF_R16F)
		{
			NvFlowCopyDistanceFieldFP16(mappedShapeData, UncompressedData);
		}
		else if (Format == PF_G8)
		{
			NvFlowCopyDistanceFieldG8(mappedShapeData, UncompressedData, DistanceFieldMAD);
		}
	}
}

void NvFlow::Scene::updateParametersDeferred(IRHICommandContext* RHICmdCtx)
{
	auto& appctx = *RHICmdCtx;

	FFlowGridProperties& Properties = *FlowGridSceneProxy->FlowGridProperties;

	for (auto It = Properties.Materials.CreateConstIterator(); It; ++It)
	{
		updateMaterial(It->Key, Properties.DefaultMaterialKey, Properties.bParticleModeEnabled, It->Value);
	}

	for (auto It = Properties.NewDistanceFieldList.CreateConstIterator(); It; ++It)
	{
		const FFlowDistanceFieldParams& DistanceFieldParams = *It;

		NvFlowShapeSDF* &shapeSDF = m_context->m_mapForShapeSDF.FindOrAdd(DistanceFieldParams.StaticMesh);
		if (shapeSDF != nullptr)
		{
			NvFlowReleaseShapeSDF(shapeSDF);
		}

		//create new NvFlowShapeSDF
		NvFlowShapeSDFDesc descSDF;
		descSDF.resolution.x = DistanceFieldParams.Size.X;
		descSDF.resolution.y = DistanceFieldParams.Size.Y;
		descSDF.resolution.z = DistanceFieldParams.Size.Z;

		shapeSDF = NvFlowCreateShapeSDF(m_gridContext, &descSDF);

		NvFlowShapeSDFData mappedShapeData = NvFlowShapeSDFMap(shapeSDF, m_gridContext);

		check(mappedShapeData.dim.x == descSDF.resolution.x);
		check(mappedShapeData.dim.y == descSDF.resolution.y);
		check(mappedShapeData.dim.z == descSDF.resolution.z);

		// From DistanceFieldAtlas.cpp, to determine if compression is enabled
		static const auto CVarCompress = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFieldBuild.Compress"));
		const bool bDataIsCompressed = CVarCompress->GetValueOnAnyThread() != 0;

		static const auto CVarEightBit = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFieldBuild.EightBit"));
		const bool bEightBitFixedPoint = CVarEightBit->GetValueOnAnyThread() != 0;

		EPixelFormat Format = bEightBitFixedPoint ? PF_G8 : PF_R16F;
		const int32 FormatSize = GPixelFormats[Format].BlockBytes;

		const auto& DistanceMinMax = DistanceFieldParams.DistanceMinMax;
		FVector2D DistanceFieldMAD = FVector2D(DistanceMinMax.Y - DistanceMinMax.X, DistanceMinMax.X);

		if (bDataIsCompressed)
		{
			const int32 UncompressedSize = descSDF.resolution.x * descSDF.resolution.y * descSDF.resolution.z * FormatSize;

			TArray<uint8> UncompressedData;
			UncompressedData.Empty(UncompressedSize);
			UncompressedData.AddUninitialized(UncompressedSize);

			verify(FCompression::UncompressMemory((ECompressionFlags)COMPRESS_ZLIB, UncompressedData.GetData(), UncompressedSize, 
				DistanceFieldParams.CompressedDistanceFieldVolume.GetData(), DistanceFieldParams.CompressedDistanceFieldVolume.Num()));

			NvFlowCopyDistanceField(mappedShapeData, UncompressedData, DistanceFieldMAD, Format);
		}
		else
		{
			auto& UncompressedData = DistanceFieldParams.CompressedDistanceFieldVolume;

			NvFlowCopyDistanceField(mappedShapeData, UncompressedData, DistanceFieldMAD, Format);
		}

		NvFlowShapeSDFUnmap(shapeSDF, m_gridContext);
	}

	check(Properties.GridEmitParams.Num() == Properties.GridEmitMaterialKeys.Num());
	for (int32 i = 0; i < Properties.GridEmitParams.Num(); ++i)
	{
		uint32 emitMaterialIdx = ~0u;

		FlowMaterialKeyType materialKey = Properties.GridEmitMaterialKeys[i];
		if (materialKey != nullptr)
		{
			MaterialData* materialData = m_materialMap.Find(materialKey);
			if (materialData != nullptr)
			{
				emitMaterialIdx = materialData->emitMaterialIndex;
			}
		}

		Properties.GridEmitParams[i].emitMaterialIndex = emitMaterialIdx;
	}

	// update material array
	{
		NvFlowGridUpdateEmitMaterials(m_grid, m_emitMaterialsArray.GetData(), m_emitMaterialsArray.Num());
	}

	// update SDF array
	bool bNeedUpdateSDFs = false;
	m_sdfs.SetNumZeroed(Properties.DistanceFieldKeys.Num(), false);
	for (int32 i = 0; i < m_sdfs.Num(); ++i)
	{
		const class UStaticMesh* StaticMesh = Properties.DistanceFieldKeys[i];
		check(StaticMesh != nullptr);

		NvFlowShapeSDF* sdf = m_context->m_mapForShapeSDF.FindChecked(StaticMesh);
		check(sdf != nullptr);
		if (m_sdfs[i] != sdf)
		{
			m_sdfs[i] = sdf;
			bNeedUpdateSDFs = true;
		}
	}
	if (bNeedUpdateSDFs)
	{
		NvFlowGridUpdateEmitSDFs(m_grid, m_sdfs.GetData(), m_sdfs.Num());
	}
}

const NvFlow::Scene::MaterialData& NvFlow::Scene::updateMaterial(FlowMaterialKeyType materialKey, FlowMaterialKeyType defaultKey, bool particleMode, const FFlowMaterialParams& materialParams)
{
	MaterialData* materialData = m_materialMap.Find(materialKey);
	if (materialData == nullptr)
	{
		materialData = &m_materialMap.Add(materialKey);

		if (particleMode && materialKey == defaultKey)
		{
			materialData->gridMaterialHandle = NvFlowGridGetDefaultMaterial(m_grid);
			NvFlowGridSetMaterialParams(m_grid, materialData->gridMaterialHandle, &materialParams.GridParams);
		}
		else
		{
			materialData->gridMaterialHandle = NvFlowGridCreateMaterial(m_grid, &materialParams.GridParams);
		}
		materialData->emitMaterialIndex = m_emitMaterialsArray.Num();

		m_emitMaterialsArray.Add(materialData->gridMaterialHandle);
	}
	else
	{
		//TODO: add dirty check
		NvFlowGridSetMaterialParams(m_grid, materialData->gridMaterialHandle, &materialParams.GridParams);
	}

	for (auto It = materialData->renderMaterialMap.CreateIterator(); It; ++It)
	{
		RenderMaterialData& renderMaterialData = It.Value();
		renderMaterialData.state &= ~RenderMaterialData::CREATED; // CREATED -> PENDING_RELEASE
	}

	for (auto It = materialParams.RenderMaterials.CreateConstIterator(); It; ++It)
	{
		const FFlowRenderMaterialParams& renderMaterialParams = *It;

		NvFlowRenderMaterialParams renderMaterialParamsCopy = renderMaterialParams;
		renderMaterialParamsCopy.material = materialData->gridMaterialHandle;

		RenderMaterialData& renderMaterialData = materialData->renderMaterialMap.FindOrAdd(renderMaterialParams.Key);
		if (renderMaterialData.state == RenderMaterialData::RELEASED)
		{
			renderMaterialData.renderMaterialHandle = NvFlowCreateRenderMaterial(m_renderContext, m_renderMaterialPool, &renderMaterialParamsCopy);
		}
		else
		{
			NvFlowRenderMaterialUpdate(renderMaterialData.renderMaterialHandle, &renderMaterialParamsCopy);
		}
		renderMaterialData.state = RenderMaterialData::CREATED;

		// update color map
		{
			//TODO: add dirty check
			auto mapped = NvFlowRenderMaterialColorMap(m_renderContext, renderMaterialData.renderMaterialHandle);
			if (mapped.data)
			{
				check(mapped.dim == renderMaterialParams.ColorMap.Num());
				FMemory::Memcpy(mapped.data, renderMaterialParams.ColorMap.GetData(), sizeof(NvFlowFloat4)*mapped.dim);
				NvFlowRenderMaterialColorUnmap(m_renderContext, renderMaterialData.renderMaterialHandle);
			}
		}
	}

	for (auto It = materialData->renderMaterialMap.CreateIterator(); It; ++It)
	{
		RenderMaterialData& renderMaterialData = It.Value();
		if (renderMaterialData.state == RenderMaterialData::PENDING_RELEASE)
		{
			NvFlowReleaseRenderMaterial(renderMaterialData.renderMaterialHandle);

			renderMaterialData.state = RenderMaterialData::RELEASED;
		}
	}

	return *materialData;
}


void NvFlow::Scene::updateSubstep(FRHICommandListImmediate& RHICmdList, float dt, uint32 substep, uint32 numSubsteps, bool& shouldFlush, const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData)
{
	bool shouldUpdateGrid = true;
	if (m_multiAdapter)
	{
		shouldUpdateGrid = (m_context->m_framesInFlightMultiGPU < m_context->m_maxFramesInFlight);
	}
	else if (m_asyncCompute)
	{
		shouldUpdateGrid = (m_context->m_framesInFlightAsyncCompute < m_context->m_maxFramesInFlight);
	}

	shouldFlush = shouldFlush || (m_multiAdapter && shouldUpdateGrid) || (m_asyncCompute && shouldUpdateGrid);

	m_updateSubstep_dt = dt;

	if (shouldUpdateGrid)
	{
		// Do this on the stack for thread safety
		UpdateParams updateParams = {};
		updateParams.Scene = this;
		updateParams.GlobalDistanceFieldParameterData = GlobalDistanceFieldParameterData;

		RHICmdList.NvFlowWork(updateSubstepCallback, &updateParams, sizeof(updateParams));
	}
}

#define FLOW_EMIT_LOGGER 0

#if FLOW_EMIT_LOGGER
#include <stdio.h>

struct MyLogger
{
	FILE* file = nullptr;
	int parity = 0;
	MyLogger()
	{
		fopen_s(&file, "FlowEmitLog.txt", "w");
	}
	~MyLogger()
	{
		fclose(file);
	}
};
#endif

void NvFlow::Scene::updateSubstepDeferred(IRHICommandContext* RHICmdCtx, UpdateParams* updateParams)
{
	auto& appctx = *RHICmdCtx;

	NvFlowContextFlushRequestPush(m_gridContext);
	NvFlowContextFlushRequestPush(m_gridCopyContext);
	NvFlowContextFlushRequestPush(m_renderCopyContext);

	float dt = m_updateSubstep_dt;

	NvFlowGridSetParams(m_grid, &m_gridParams);

	FFlowGridProperties& Properties = *FlowGridSceneProxy->FlowGridProperties;

#if FLOW_EMIT_LOGGER
	static MyLogger myLogger;

	myLogger.parity ^= 0x01;

	for (int32 i = 0; i < Properties.GridEmitParams.Num(); i++)
	{
		auto& EmitParams = Properties.GridEmitParams[i];

		fprintf(myLogger.file, "%d, %d, %f, %f, %f, %f, %f, %f\n",
			myLogger.parity, i,
			EmitParams.bounds.w.x, EmitParams.bounds.w.y, EmitParams.bounds.w.z,
			EmitParams.velocityLinear.x, EmitParams.velocityLinear.y, EmitParams.velocityLinear.z
		);
	}
#endif

	// update emitters
	{
		// emit
		NvFlowGridEmit(
			m_grid,
			Properties.GridEmitShapeDescs.GetData(),
			Properties.GridEmitShapeDescs.Num(),
			Properties.GridEmitParams.GetData(),
			Properties.GridEmitParams.Num()
			);

		// collide
		NvFlowGridEmit(
			m_grid,
			Properties.GridCollideShapeDescs.GetData(),
			Properties.GridCollideShapeDescs.Num(),
			Properties.GridCollideParams.GetData(),
			Properties.GridCollideParams.Num()
			);
	}

	{
		CallbackUserData callbackUserData;
		callbackUserData.Scene = this;
		callbackUserData.RHICmdCtx = RHICmdCtx;
		callbackUserData.DeltaTime = dt;
		callbackUserData.GlobalDistanceFieldParameterData = updateParams->GlobalDistanceFieldParameterData;

		NvFlowGridEmitCustomRegisterAllocFunc(m_grid, &NvFlow::Scene::sEmitCustomAllocCallback, &callbackUserData);
		NvFlowGridEmitCustomRegisterEmitFunc(m_grid, eNvFlowGridTextureChannelVelocity, &NvFlow::Scene::sEmitCustomEmitVelocityCallback, &callbackUserData);
		NvFlowGridEmitCustomRegisterEmitFunc(m_grid, eNvFlowGridTextureChannelDensity, &NvFlow::Scene::sEmitCustomEmitDensityCallback, &callbackUserData);

		// check for grid location or halfSize change
		{
			const FVector FlowOrigin = FlowGridSceneProxy->GetLocalToWorld().GetOrigin() * scaleInv;
			const NvFlowFloat3 TargetLocation = *(const NvFlowFloat3*)(&FlowOrigin.X);
			const NvFlowFloat3 TargetHalfSize = FlowGridSceneProxy->FlowGridProperties->GridDesc.halfSize;

			const bool bChangedLocation = (
				TargetLocation.x != m_gridDesc.initialLocation.x ||
				TargetLocation.y != m_gridDesc.initialLocation.y ||
				TargetLocation.z != m_gridDesc.initialLocation.z);

			const bool bChangedHalfSize = (
				TargetHalfSize.x != m_gridDesc.halfSize.x ||
				TargetHalfSize.y != m_gridDesc.halfSize.y ||
				TargetHalfSize.z != m_gridDesc.halfSize.z);

			if (bChangedLocation || bChangedHalfSize)
			{
				if (bChangedLocation && !bChangedHalfSize)
				{
					NvFlowGridSetTargetLocation(m_grid, TargetLocation);

					m_gridDesc.initialLocation = TargetLocation;
				}
				else
				{
					NvFlowGridResetDesc resetDesc = {};
					resetDesc.initialLocation = TargetLocation;
					resetDesc.halfSize = TargetHalfSize;

					NvFlowGridReset(m_grid, &resetDesc);

					m_gridDesc.initialLocation = TargetLocation;
					m_gridDesc.halfSize = TargetHalfSize;
				}
			}
		}

		NvFlowGridUpdate(m_grid, m_gridContext, dt);

		NvFlowGridEmitCustomRegisterAllocFunc(m_grid, nullptr, nullptr);
		NvFlowGridEmitCustomRegisterEmitFunc(m_grid, eNvFlowGridTextureChannelVelocity, nullptr, nullptr);
		NvFlowGridEmitCustomRegisterEmitFunc(m_grid, eNvFlowGridTextureChannelDensity, nullptr, nullptr);
	}

	auto gridExport = NvFlowGridGetGridExport(m_gridContext, m_grid);

	NvFlowGridProxyFlushParams flushParams = {};
	flushParams.gridContext = m_gridContext;
	flushParams.gridCopyContext = m_gridCopyContext;
	flushParams.renderCopyContext = m_renderCopyContext;
	NvFlowGridProxyPush(m_gridProxy, gridExport, &flushParams);
}

void NvFlow::Scene::updateSubstepCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx)
{
	auto updateParams = (UpdateParams*)paramData;
	updateParams->Scene->updateSubstepDeferred(RHICmdCtx, updateParams);
}

void NvFlow::Scene::finalizeUpdate(FRHICommandListImmediate& RHICmdList)
{
	RHICmdList.NvFlowWork(finalizeUpdateCallback, this, 0u);
}

void NvFlow::Scene::finalizeUpdateCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx)
{
	auto scene = (NvFlow::Scene*)paramData;
	scene->finalizeUpdateDeferred(RHICmdCtx);
}

void NvFlow::Scene::finalizeUpdateDeferred(IRHICommandContext* RHICmdCtx)
{
	m_particleParamsArray.Reset();
}

void NvFlow::Scene::updateGridView(FRHICommandListImmediate& RHICmdList)
{
	m_shadowLightType = LightType_MAX;
	if (FlowGridSceneProxy)
	{
		auto RenderScene = FlowGridSceneProxy->GetScene().GetRenderScene();
		if (RenderScene)
		{
			FLightSceneProxy* FoundLightSceneProxy = nullptr;
			for (auto It = RenderScene->Lights.CreateIterator(); It; ++It)
			{
				auto LightSceneProxy = (*It).LightSceneInfo->Proxy;
				if (LightSceneProxy->GetFlowGridShadowEnabled() && LightSceneProxy->GetFlowGridShadowChannel() == FlowGridSceneProxy->FlowGridProperties->RenderParams.ShadowChannel)
				{
					FoundLightSceneProxy = LightSceneProxy;
					break;
				}
			}

			// select Default DirectionalLight if there are no enabled lights
			if (FoundLightSceneProxy == nullptr && RenderScene->SimpleDirectionalLight != nullptr)
			{
				FoundLightSceneProxy = RenderScene->SimpleDirectionalLight->Proxy;
			}

			if (FoundLightSceneProxy != nullptr)
			{
				m_shadowLightType = FoundLightSceneProxy->GetLightType();
				m_shadowWorldToLight = FoundLightSceneProxy->GetWorldToLight();
				m_shadowOuterConeAngle = FoundLightSceneProxy->GetOuterConeAngle();
				m_shadowRadius = FoundLightSceneProxy->GetRadius();
			}
		}
	}

	RHICmdList.NvFlowWork(updateGridViewCallback, this, 0u);
}

void NvFlow::Scene::updateGridViewCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx)
{
	auto scene = (NvFlow::Scene*)paramData;
	scene->updateGridViewDeferred(RHICmdCtx);
}

void NvFlow::Scene::updateGridViewDeferred(IRHICommandContext* RHICmdCtx)
{
	NvFlowGridProxyFlushParams flushParams = {};
	flushParams.gridContext = m_gridContext;
	flushParams.gridCopyContext = m_gridCopyContext;
	flushParams.renderCopyContext = m_renderCopyContext;

	NvFlowGridProxyFlush(m_gridProxy, &flushParams);

	FFlowGridProperties& Properties = *FlowGridSceneProxy->FlowGridProperties;

	m_gridExport4Render = NvFlowGridProxyGetGridExport(m_gridProxy, m_renderContext);

	FString gridName;
	auto SubmitInfo = NvFlowDebugInfoQueue.GetSubmitInfo();
	if (SubmitInfo)
	{
		FlowGridSceneProxy->GetOwnerName().ToString(gridName);

		NvFlowGridExportHandle gridVelocityHandle = NvFlowGridExportGetHandle(m_gridExport4Render, m_renderContext, eNvFlowGridTextureChannelVelocity);
		NvFlowGridExportLayeredView gridVelocityLayeredView;
		NvFlowGridExportGetLayeredView(gridVelocityHandle, &gridVelocityLayeredView);

		NvFlowGridExportHandle gridDensityHandle = NvFlowGridExportGetHandle(m_gridExport4Render, m_renderContext, eNvFlowGridTextureChannelDensity);
		NvFlowGridExportLayeredView gridDensityLayeredView;
		NvFlowGridExportGetLayeredView(gridDensityHandle, &gridDensityLayeredView);

		SubmitInfo->Add(FString::Printf(TEXT("Grid '%s': velocity blocks = %d of %d"), *gridName, gridVelocityLayeredView.mapping.layeredNumBlocks, gridVelocityLayeredView.mapping.maxBlocks));
		SubmitInfo->Add(FString::Printf(TEXT("Grid '%s': density blocks  = %d of %d"), *gridName, gridDensityLayeredView.mapping.layeredNumBlocks, gridDensityLayeredView.mapping.maxBlocks));
	}

	if (Properties.RenderParams.bVolumeShadowEnabled && (m_shadowLightType == LightType_Directional || m_shadowLightType == LightType_Spot))
	{
		if (m_volumeShadow == nullptr || 
			m_shadowResolution != Properties.RenderParams.ShadowResolution ||
			m_shadowMinResidentScale != Properties.RenderParams.ShadowMinResidentScale ||
			m_shadowMaxResidentScale != Properties.RenderParams.ShadowMaxResidentScale)
		{
			if (m_volumeShadow != nullptr)
			{
				NvFlowReleaseVolumeShadow(m_volumeShadow);
			}

			NvFlowVolumeShadowDesc volumeShadowDesc;
			volumeShadowDesc.gridExport = m_gridExport4Render;
			volumeShadowDesc.mapWidth = Properties.RenderParams.ShadowResolution;
			volumeShadowDesc.mapHeight = Properties.RenderParams.ShadowResolution;
			volumeShadowDesc.mapDepth = Properties.RenderParams.ShadowResolution;
			volumeShadowDesc.minResidentScale = Properties.RenderParams.ShadowMinResidentScale;
			volumeShadowDesc.maxResidentScale = Properties.RenderParams.ShadowMaxResidentScale;

			m_volumeShadow = NvFlowCreateVolumeShadow(m_renderContext, &volumeShadowDesc);

			m_shadowResolution = Properties.RenderParams.ShadowResolution;
			m_shadowMinResidentScale = Properties.RenderParams.ShadowMinResidentScale;
			m_shadowMaxResidentScale = Properties.RenderParams.ShadowMaxResidentScale;
		}

		NvFlowVolumeShadowParams shadowParams;
		shadowParams.materialPool = m_renderMaterialPool;
		shadowParams.renderMode = Properties.RenderParams.RenderMode;
		shadowParams.renderChannel = Properties.RenderParams.RenderChannel;
		shadowParams.intensityScale = Properties.RenderParams.ShadowIntensityScale;
		shadowParams.minIntensity = Properties.RenderParams.ShadowMinIntensity;
		shadowParams.shadowBlendCompMask = Properties.RenderParams.ShadowBlendCompMask;
		shadowParams.shadowBlendBias = Properties.RenderParams.ShadowBlendBias;

		FMatrix ShadowViewMatrix = m_shadowWorldToLight;
		ShadowViewMatrix *= FMatrix(
			FPlane(0, 0, 1, 0),
			FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 0, 1));

		FBox BoundBox = FlowGridSceneProxy->GetBounds().GetBox();
		BoundBox.Min *= scaleInv;
		BoundBox.Max *= scaleInv;

		bool bShadowProjValid = true;
		FMatrix ShadowProjMatrix = FMatrix::Identity;
		if (m_shadowLightType == LightType_Spot)
		{
			ShadowViewMatrix.SetOrigin(ShadowViewMatrix.GetOrigin() * scaleInv);

			// Creates an array of vertices and edges for a bounding box.
			FVector BoundVertices[8];
			for (int32 X = 0; X < 2; X++)
			{
				for (int32 Y = 0; Y < 2; Y++)
				{
					for (int32 Z = 0; Z < 2; Z++)
					{
						BoundVertices[X * 4 + Y * 2 + Z] = ShadowViewMatrix.TransformPosition(FVector(
							X ? BoundBox.Min.X : BoundBox.Max.X,
							Y ? BoundBox.Min.Y : BoundBox.Max.Y,
							Z ? BoundBox.Min.Z : BoundBox.Max.Z
						));
					}
				}
			}

			struct FBoundEdge
			{
				int FirstIndex;
				int SecondIndex;

				FBoundEdge(int InFirstIndex, int InSecondIndex)	: FirstIndex(InFirstIndex), SecondIndex(InSecondIndex) {}
				FBoundEdge() {}
			};
			FBoundEdge BoundEdges[12];
			for (int X = 0; X < 2; X++)
			{
				int BaseIndex = X * 4;
				BoundEdges[X * 4 + 0] = FBoundEdge(BaseIndex, BaseIndex + 1);
				BoundEdges[X * 4 + 1] = FBoundEdge(BaseIndex + 1, BaseIndex + 3);
				BoundEdges[X * 4 + 2] = FBoundEdge(BaseIndex + 3, BaseIndex + 2);
				BoundEdges[X * 4 + 3] = FBoundEdge(BaseIndex + 2, BaseIndex);
			}
			for (int XEdge = 0; XEdge < 4; XEdge++)
			{
				BoundEdges[8 + XEdge] = FBoundEdge(XEdge, XEdge + 4);
			}


			const float MinZ = Properties.RenderParams.ShadowNearDistance * scaleInv;
			const float MaxZ = m_shadowRadius * scaleInv;
			const float TanOuterCone = FMath::Tan(m_shadowOuterConeAngle);

			FVector BoundMin = FVector(+FLT_MAX, +FLT_MAX, +FLT_MAX);
			FVector BoundMax = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);

			for (int32 VIdx = 0; VIdx < 8; VIdx++)
			{
				const FVector& BoundVertex = BoundVertices[VIdx];
				if (BoundVertex.Z >= MinZ)
				{
					const FVector ProjectedVertex(BoundVertex.X / BoundVertex.Z, BoundVertex.Y / BoundVertex.Z, BoundVertex.Z);
					BoundMin = BoundMin.ComponentMin(ProjectedVertex);
					BoundMax = BoundMax.ComponentMax(ProjectedVertex);
				}
			}
			for (int32 EIdx = 0; EIdx < 12; EIdx++)
			{
				const FVector& BoundVertex1 = BoundVertices[BoundEdges[EIdx].FirstIndex];
				const FVector& BoundVertex2 = BoundVertices[BoundEdges[EIdx].SecondIndex];

				const float DeltaZ1 = BoundVertex1.Z - MinZ;
				const float DeltaZ2 = MinZ - BoundVertex2.Z;
				if (DeltaZ1 * DeltaZ2 > 0.0f)
				{
					const float DeltaZ = BoundVertex1.Z - BoundVertex2.Z;
					const FVector EdgeVertex = BoundVertex1 * (DeltaZ2 / DeltaZ) + BoundVertex2 * (DeltaZ1 / DeltaZ);

					const FVector ProjectedVertex(EdgeVertex.X / MinZ, EdgeVertex.Y / MinZ, MinZ);
					BoundMin = BoundMin.ComponentMin(ProjectedVertex);
					BoundMax = BoundMax.ComponentMax(ProjectedVertex);
				}
			}

			// clip to SpotLight frustrum
			FVector LightMin = FVector(-TanOuterCone, -TanOuterCone, MinZ);
			FVector LightMax = FVector(+TanOuterCone, +TanOuterCone, MaxZ);

			BoundMin = BoundMin.ComponentMin(LightMax).ComponentMax(LightMin);
			BoundMax = BoundMax.ComponentMin(LightMax).ComponentMax(LightMin);

			bShadowProjValid = (BoundMax.X > BoundMin.X) && (BoundMax.Y > BoundMin.Y) && (BoundMax.Z > BoundMin.Z);
			if (!bShadowProjValid)
			{
				BoundMin = LightMin;
				BoundMax = LightMax;
			}

			const float XSum = BoundMax.X + BoundMin.X;
			const float YSum = BoundMax.Y + BoundMin.Y;
			const float XFactor = 1.0f / (BoundMax.X - BoundMin.X);
			const float YFactor = 1.0f / (BoundMax.Y - BoundMin.Y);
			const float ZFactor = BoundMax.Z / (BoundMax.Z - BoundMin.Z);
			ShadowProjMatrix = FMatrix(
				FPlane(2.0f * XFactor,  0.0f,            0.0f,                  0.0f),
				FPlane(0.0f,            2.0f * YFactor,  0.0f,                  0.0f),
				FPlane(0.0f,            0.0f,            ZFactor,               1.0f),
				FPlane(-XSum * XFactor, -YSum * YFactor, -BoundMin.Z * ZFactor, 0.0f));
		}
		else
		{
			check(m_shadowLightType == LightType_Directional);

			//set view origin to the center of bounding box
			ShadowViewMatrix.SetOrigin(-FVector(ShadowViewMatrix.TransformVector(BoundBox.GetCenter())));

			FVector Extent = BoundBox.GetExtent();

			float ExtentX = FVector::DotProduct(ShadowViewMatrix.GetColumn(0).GetAbs(), Extent) * Properties.RenderParams.ShadowFrustrumScale;
			float ExtentY = FVector::DotProduct(ShadowViewMatrix.GetColumn(1).GetAbs(), Extent) * Properties.RenderParams.ShadowFrustrumScale;
			float ExtentZ = FVector::DotProduct(ShadowViewMatrix.GetColumn(2).GetAbs(), Extent) * Properties.RenderParams.ShadowFrustrumScale;

			ShadowProjMatrix.M[0][0] = 1.0f / ExtentX;
			ShadowProjMatrix.M[1][1] = 1.0f / ExtentY;
			ShadowProjMatrix.M[2][2] = 0.5f / ExtentZ;
			ShadowProjMatrix.M[3][2] = 0.5f;
		}

		memcpy(&shadowParams.projectionMatrix, &ShadowProjMatrix.M[0][0], sizeof(shadowParams.projectionMatrix));
		memcpy(&shadowParams.viewMatrix, &ShadowViewMatrix.M[0][0], sizeof(shadowParams.viewMatrix));

		if (bShadowProjValid)
		{
			NvFlowVolumeShadowUpdate(m_volumeShadow, m_renderContext, m_gridExport4Render, &shadowParams);

			m_gridExport4Render = NvFlowVolumeShadowGetGridExport(m_volumeShadow, m_renderContext);

			if (SubmitInfo)
			{
				NvFlowVolumeShadowStats shadowStats;
				NvFlowVolumeShadowGetStats(m_volumeShadow, &shadowStats);

				SubmitInfo->Add(FString::Printf(TEXT("Grid '%s': shadow blocks active = %d"), *gridName, shadowStats.shadowBlocksActive));
			}
		}
	}
	else
	{
		if (m_volumeShadow)
		{
			NvFlowReleaseVolumeShadow(m_volumeShadow);
			m_volumeShadow = nullptr;
		}
	}
}

void NvFlow::Scene::render(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	FMatrix viewMatrix = View.ViewMatrices.GetViewMatrix();
	FMatrix projMatrix = View.ViewMatrices.GetProjectionMatrix();

	// Do this on the stack for thread safety
	RenderParams renderParams = {};
	renderParams.scene = this;
	renderParams.volumeRenderParams = m_renderParams;

	auto& rp = renderParams.volumeRenderParams;

	for (int j = 0; j < 3; j++)
	{
		for (int i = 0; i < 3; i++)
		{
			viewMatrix.M[j][i] *= scale;
		}
	}

	memcpy(&rp.projectionMatrix, &projMatrix.M[0][0], sizeof(rp.projectionMatrix));
	memcpy(&rp.viewMatrix, &viewMatrix.M[0][0], sizeof(rp.viewMatrix));

#if NVFLOW_SMP
	auto& multiResConfig = View.MultiResConf;

	rp.multiRes.enabled = View.bVRProjectEnabled && (View.VRProjMode == FSceneView::EVRProjectMode::MultiRes);
	rp.multiRes.centerWidth = multiResConfig.CenterWidth;
	rp.multiRes.centerHeight = multiResConfig.CenterHeight;
	rp.multiRes.centerX = multiResConfig.CenterX;
	rp.multiRes.centerY = multiResConfig.CenterY;
	rp.multiRes.densityScaleX[0] = multiResConfig.DensityScaleX[0];
	rp.multiRes.densityScaleX[1] = multiResConfig.DensityScaleX[1];
	rp.multiRes.densityScaleX[2] = multiResConfig.DensityScaleX[2];
	rp.multiRes.densityScaleY[0] = multiResConfig.DensityScaleY[0];
	rp.multiRes.densityScaleY[1] = multiResConfig.DensityScaleY[1];
	rp.multiRes.densityScaleY[2] = multiResConfig.DensityScaleY[2];
	rp.multiRes.viewport.topLeftX = View.ViewRect.Min.X;
	rp.multiRes.viewport.topLeftY = View.ViewRect.Min.Y;
	rp.multiRes.viewport.width = View.ViewRect.Width();
	rp.multiRes.viewport.height = View.ViewRect.Height();
	rp.multiRes.nonMultiResWidth = View.NonVRProjectViewRect.Width();
	rp.multiRes.nonMultiResHeight = View.NonVRProjectViewRect.Height();

	auto& LMSConfig = View.LensMatchedShadingConf;

	rp.lensMatchedShading.enabled = View.bVRProjectEnabled && (View.VRProjMode == FSceneView::EVRProjectMode::LensMatched);
	rp.lensMatchedShading.warpLeft = LMSConfig.WarpLeft;
	rp.lensMatchedShading.warpRight = LMSConfig.WarpRight;
	rp.lensMatchedShading.warpUp = LMSConfig.WarpUp;
	rp.lensMatchedShading.warpDown = LMSConfig.WarpDown;
	rp.lensMatchedShading.sizeLeft = FMath::CeilToInt(LMSConfig.RelativeSizeLeft * View.NonVRProjectViewRect.Width());
	rp.lensMatchedShading.sizeRight = FMath::CeilToInt(LMSConfig.RelativeSizeRight * View.NonVRProjectViewRect.Width());
	rp.lensMatchedShading.sizeUp = FMath::CeilToInt(LMSConfig.RelativeSizeUp * View.NonVRProjectViewRect.Height());
	rp.lensMatchedShading.sizeDown = FMath::CeilToInt(LMSConfig.RelativeSizeDown * View.NonVRProjectViewRect.Height());
	rp.lensMatchedShading.viewport.topLeftX = View.ViewRect.Min.X;
	rp.lensMatchedShading.viewport.topLeftY = View.ViewRect.Min.Y;
	rp.lensMatchedShading.viewport.width = View.ViewRect.Width();
	rp.lensMatchedShading.viewport.height = View.ViewRect.Height();
	rp.lensMatchedShading.nonLMSWidth = View.NonVRProjectViewRect.Width();
	rp.lensMatchedShading.nonLMSHeight = View.NonVRProjectViewRect.Height();

	if (rp.lensMatchedShading.enabled)
	{
		RHICmdList.SetModifiedWMode(View.LensMatchedShadingConf, true, false);
	}

#endif

	// push work
	RHICmdList.NvFlowWork(renderCallback, &renderParams, sizeof(RenderParams));

#if NVFLOW_SMP
	if (rp.lensMatchedShading.enabled)
	{
		RHICmdList.SetModifiedWMode(View.LensMatchedShadingConf, true, true);
	}
#endif
}

void NvFlow::Scene::renderDepth(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	if (!m_renderParams.generateDepth)
	{
		return;
	}

	// render depth
	{
		FMatrix viewMatrix = View.ViewMatrices.GetViewMatrix();
		FMatrix projMatrix = View.ViewMatrices.GetProjectionMatrix();

		// Do this on the stack for thread safety
		RenderParams renderParams = {};
		renderParams.scene = this;
		renderParams.volumeRenderParams = m_renderParams;

		auto& rp = renderParams.volumeRenderParams;

		for (int j = 0; j < 3; j++)
		{
			for (int i = 0; i < 3; i++)
			{
				viewMatrix.M[j][i] *= scale;
			}
		}

		memcpy(&rp.projectionMatrix, &projMatrix.M[0][0], sizeof(rp.projectionMatrix));
		memcpy(&rp.viewMatrix, &viewMatrix.M[0][0], sizeof(rp.viewMatrix));

	#if NVFLOW_SMP
		auto& multiResConfig = View.MultiResConf;

		rp.multiRes.enabled = View.bVRProjectEnabled && (View.VRProjMode == FSceneView::EVRProjectMode::MultiRes);
		rp.multiRes.centerWidth = multiResConfig.CenterWidth;
		rp.multiRes.centerHeight = multiResConfig.CenterHeight;
		rp.multiRes.centerX = multiResConfig.CenterX;
		rp.multiRes.centerY = multiResConfig.CenterY;
		rp.multiRes.densityScaleX[0] = multiResConfig.DensityScaleX[0];
		rp.multiRes.densityScaleX[1] = multiResConfig.DensityScaleX[1];
		rp.multiRes.densityScaleX[2] = multiResConfig.DensityScaleX[2];
		rp.multiRes.densityScaleY[0] = multiResConfig.DensityScaleY[0];
		rp.multiRes.densityScaleY[1] = multiResConfig.DensityScaleY[1];
		rp.multiRes.densityScaleY[2] = multiResConfig.DensityScaleY[2];
		rp.multiRes.viewport.topLeftX = View.ViewRect.Min.X;
		rp.multiRes.viewport.topLeftY = View.ViewRect.Min.Y;
		rp.multiRes.viewport.width = View.ViewRect.Width();
		rp.multiRes.viewport.height = View.ViewRect.Height();
		rp.multiRes.nonMultiResWidth = View.NonVRProjectViewRect.Width();
		rp.multiRes.nonMultiResHeight = View.NonVRProjectViewRect.Height();

		auto& LMSConfig = View.LensMatchedShadingConf;

		rp.lensMatchedShading.enabled = View.bVRProjectEnabled && (View.VRProjMode == FSceneView::EVRProjectMode::LensMatched);
		rp.lensMatchedShading.warpLeft = LMSConfig.WarpLeft;
		rp.lensMatchedShading.warpRight = LMSConfig.WarpRight;
		rp.lensMatchedShading.warpUp = LMSConfig.WarpUp;
		rp.lensMatchedShading.warpDown = LMSConfig.WarpDown;
		rp.lensMatchedShading.sizeLeft = FMath::CeilToInt(LMSConfig.RelativeSizeLeft * View.NonVRProjectViewRect.Width());
		rp.lensMatchedShading.sizeRight = FMath::CeilToInt(LMSConfig.RelativeSizeRight * View.NonVRProjectViewRect.Width());
		rp.lensMatchedShading.sizeUp = FMath::CeilToInt(LMSConfig.RelativeSizeUp * View.NonVRProjectViewRect.Height());
		rp.lensMatchedShading.sizeDown = FMath::CeilToInt(LMSConfig.RelativeSizeDown * View.NonVRProjectViewRect.Height());
		rp.lensMatchedShading.viewport.topLeftX = View.ViewRect.Min.X;
		rp.lensMatchedShading.viewport.topLeftY = View.ViewRect.Min.Y;
		rp.lensMatchedShading.viewport.width = View.ViewRect.Width();
		rp.lensMatchedShading.viewport.height = View.ViewRect.Height();
		rp.lensMatchedShading.nonLMSWidth = View.NonVRProjectViewRect.Width();
		rp.lensMatchedShading.nonLMSHeight = View.NonVRProjectViewRect.Height();

		if (rp.lensMatchedShading.enabled)
		{
			RHICmdList.SetModifiedWMode(View.LensMatchedShadingConf, true, false);
		}

	#endif

		// push work
		RHICmdList.NvFlowWork(renderDepthCallback, &renderParams, sizeof(RenderParams));

	#if NVFLOW_SMP
		if (rp.lensMatchedShading.enabled)
		{
			RHICmdList.SetModifiedWMode(View.LensMatchedShadingConf, true, true);
		}
	#endif
	}
}

void NvFlow::Scene::renderDepthDeferred(IRHICommandContext* RHICmdCtx, RenderParams* renderParams)
{
	auto& volumeRenderParams = renderParams->volumeRenderParams;

	volumeRenderParams.depthStencilView = m_context->m_dsv;
	volumeRenderParams.renderTargetView = nullptr;

	volumeRenderParams.preColorCompositeOnly = true;
	volumeRenderParams.colorCompositeOnly = false;

	NvFlowVolumeRenderGridExport(m_volumeRender, m_renderContext, m_gridExport4Render, &volumeRenderParams);
}

void NvFlow::Scene::renderDepthCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx)
{
	auto renderParams = (RenderParams*)paramData;
	renderParams->scene->renderDepthDeferred(RHICmdCtx, renderParams);
}

void NvFlow::Scene::renderCallback(void* paramData, SIZE_T numBytes, IRHICommandContext* RHICmdCtx)
{
	auto renderParams = (RenderParams*)paramData;
	renderParams->scene->renderDeferred(RHICmdCtx, renderParams);
}

void NvFlow::Scene::renderDeferred(IRHICommandContext* RHICmdCtx, RenderParams* renderParams)
{
	auto& volumeRenderParams = renderParams->volumeRenderParams;

	volumeRenderParams.depthStencilView = m_context->m_dsv;
	volumeRenderParams.renderTargetView = m_context->m_rtv;

	if (volumeRenderParams.generateDepth)
	{
		volumeRenderParams.preColorCompositeOnly = false;
		volumeRenderParams.colorCompositeOnly = true;
	}
	else
	{
		volumeRenderParams.preColorCompositeOnly = false;
		volumeRenderParams.colorCompositeOnly = false;
	}

	NvFlowVolumeRenderGridExport(m_volumeRender, m_renderContext, m_gridExport4Render, &volumeRenderParams);

	if (m_volumeShadow && UFlowGridAsset::sGlobalDebugDrawShadow)
	{
		NvFlowVolumeShadowDebugRenderParams params = {};

		params.renderTargetView = m_context->m_rtv;

		params.projectionMatrix = volumeRenderParams.projectionMatrix;
		params.viewMatrix = volumeRenderParams.viewMatrix;

		NvFlowVolumeShadowDebugRender(m_volumeShadow, m_renderContext, &params);
	}

}

#endif // WITH_NVFLOW_BACKEND

// ---------------- global interface functions ---------------------

bool NvFlowUsesGlobalDistanceField()
{
	bool bResult = false;
#if WITH_NVFLOW_BACKEND
	if (NvFlow::gContext)
	{
		for (int32 i = 0; i < NvFlow::gContext->m_sceneList.Num(); i++)
		{
			NvFlow::Scene* Scene = NvFlow::gContext->m_sceneList[i];
			FFlowGridSceneProxy* FlowGridSceneProxy = Scene->FlowGridSceneProxy;
			bResult |= FlowGridSceneProxy->FlowGridProperties->bDistanceFieldCollisionEnabled;
		}
	}
#endif // WITH_NVFLOW_BACKEND
	return bResult;
}

void NvFlowUpdateScene(FRHICommandListImmediate& RHICmdList, TArray<FPrimitiveSceneInfo*>& Primitives, const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData)
{
	if (GUsingNullRHI)
	{
		return;
	}

#if WITH_NVFLOW_BACKEND

	bool shouldFlush = false;

	SCOPE_CYCLE_COUNTER(STAT_Flow_SimulateGrids);
	SCOPED_DRAW_EVENT(RHICmdList, FlowSimulateGrids);
	{
		SCOPED_DRAW_EVENT(RHICmdList, FlowContextSimulate);

		// create a context if one does not exist
		if (NvFlow::gContext == nullptr)
		{
			NvFlow::gContext = &NvFlow::gContextImpl;

			NvFlow::gContext->init(RHICmdList);
		}

		NvFlow::gContext->conditionalInitMultiGPU(RHICmdList);

		NvFlow::gContext->interopBegin(RHICmdList, true, false);

		// look for FFlowGridSceneProxy, TODO replace with adding special member to FScene
		FFlowGridSceneProxy* FlowGridSceneProxy = nullptr;
		for (int32 i = 0; i < Primitives.Num(); i++)
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = Primitives[i];
			if (PrimitiveSceneInfo->Proxy->FlowData.bFlowGrid)
			{
				FlowGridSceneProxy = (FFlowGridSceneProxy*)PrimitiveSceneInfo->Proxy;

				if (UFlowGridAsset::sGlobalMultiGPUResetRequest)
				{
					FlowGridSceneProxy->FlowGridProperties->bActive = false;
				}

				NvFlow::gContext->updateScene(RHICmdList, FlowGridSceneProxy, shouldFlush, GlobalDistanceFieldParameterData);
			}
		}

		if (UFlowGridAsset::sGlobalMultiGPUResetRequest)
		{
			UFlowGridAsset::sGlobalMultiGPUResetRequest = false;
		}
	}
	{
		SCOPED_DRAW_EVENT(RHICmdList, FlowUpdateGridViews);
		{
			SCOPED_DRAW_EVENT(RHICmdList, FlowContextUpdateGridView);

			NvFlow::gContext->updateGridView(RHICmdList);
		}

		NvFlow::gContext->interopEnd(RHICmdList, true, shouldFlush);
	}
	if (NvFlow::gContext)
	{
		RHICmdList.NvFlowWork(NvFlow::Context::cleanupSceneListCallback, NvFlow::gContext, 0u);
	}

#endif // WITH_NVFLOW_BACKEND
}

bool NvFlowDoRenderPrimitive(FRHICommandList& RHICmdList, const FViewInfo& View, FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
#if WITH_NVFLOW_BACKEND
	if (!GUsingNullRHI && NvFlow::gContext)
	{
		if (PrimitiveSceneInfo->Proxy->FlowData.bFlowGrid)
		{
			FFlowGridSceneProxy* FlowGridSceneProxy = (FFlowGridSceneProxy*)PrimitiveSceneInfo->Proxy;
			if (FlowGridSceneProxy->FlowGridProperties->bParticleModeEnabled != 0 &&
				FlowGridSceneProxy->FlowGridProperties->RenderParams.bDebugWireframe == 0)
			{
				return false;
			}

			SCOPE_CYCLE_COUNTER(STAT_Flow_RenderGrids);
			SCOPED_DRAW_EVENT(RHICmdList, FlowRenderGrids);
			{
				SCOPED_DRAW_EVENT(RHICmdList, FlowContextRenderGrids);

				NvFlow::gContext->interopBegin(RHICmdList, false, true);

				NvFlow::gContext->renderScene(RHICmdList, View, FlowGridSceneProxy);

				NvFlow::gContext->interopEnd(RHICmdList, false, false);
			}
			return true;
		}
	}
#endif // WITH_NVFLOW_BACKEND
	return false;
}

void NvFlowDoRenderFinish(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
#if WITH_NVFLOW_BACKEND
	//if (!GUsingNullRHI && NvFlow::gContext)
	//{
	//}
#endif // WITH_NVFLOW_BACKEND
}

bool NvFlowShouldDoPreComposite(FRHICommandListImmediate& RHICmdList)
{
#if WITH_NVFLOW_BACKEND
	if (!GUsingNullRHI && NvFlow::gContext && UFlowGridAsset::sGlobalDepth > 0)
	{
		uint32 Count = 0;
		for (int32 i = 0; i < NvFlow::gContext->m_sceneList.Num(); i++)
		{
			NvFlow::Scene* Scene = NvFlow::gContext->m_sceneList[i];
			//FFlowGridSceneProxy* FlowGridSceneProxy = Scene->FlowGridSceneProxy;
			if (Scene->m_renderParams.generateDepth)
			{
				Count++;
			}
		}
		return (Count > 0);
	}
#endif // WITH_NVFLOW_BACKEND
	return false;
}

void NvFlowDoPreComposite(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
#if WITH_NVFLOW_BACKEND
	if (!GUsingNullRHI && NvFlow::gContext && UFlowGridAsset::sGlobalDepth > 0)
	{
		NvFlow::gContext->interopBegin(RHICmdList, false, false);

		for (int32 i = 0; i < NvFlow::gContext->m_sceneList.Num(); i++)
		{
			NvFlow::Scene* Scene = NvFlow::gContext->m_sceneList[i];
			FFlowGridSceneProxy* FlowGridSceneProxy = Scene->FlowGridSceneProxy;
			if (FlowGridSceneProxy && Scene->m_renderParams.generateDepth)
			{
				NvFlow::gContext->renderScenePreComposite(RHICmdList, View, FlowGridSceneProxy);
			}
		}

		NvFlow::gContext->interopEnd(RHICmdList, false, false);
	}
#endif // WITH_NVFLOW_BACKEND
}

uint32 NvFlowQueryGridExportParams(FRHICommandListImmediate& RHICmdList, const ParticleSimulationParamsNvFlow& ParticleSimulationParams, uint32 MaxCount, GridExportParamsNvFlow* ResultParamsList)
{
#if WITH_NVFLOW_BACKEND
	if (NvFlow::gContext)
	{
		uint32 Count = 0;
		for (int32 i = 0; i < NvFlow::gContext->m_sceneList.Num() && Count < MaxCount; i++)
		{
			NvFlow::Scene* Scene = NvFlow::gContext->m_sceneList[i];
			FFlowGridSceneProxy* FlowGridSceneProxy = Scene->FlowGridSceneProxy;
			if (FlowGridSceneProxy &&
				FlowGridSceneProxy->FlowGridProperties->bParticlesInteractionEnabled &&
				ParticleSimulationParams.Bounds.Intersect(FlowGridSceneProxy->GetBounds().GetBox()))
			{
				EInteractionResponseNvFlow ParticleSystemResponse = ParticleSimulationParams.ResponseToInteractionChannels.GetResponse(FlowGridSceneProxy->FlowGridProperties->InteractionChannel);
				EInteractionResponseNvFlow GridRespone = FlowGridSceneProxy->FlowGridProperties->ResponseToInteractionChannels.GetResponse(ParticleSimulationParams.InteractionChannel);

				bool GridAffectsParticleSystem =
					(ParticleSystemResponse == EIR_Receive || ParticleSystemResponse == EIR_TwoWay) &&
					(GridRespone == EIR_Produce || GridRespone == EIR_TwoWay);

				bool ParticleSystemAffectsGrid =
					(GridRespone == EIR_Receive || GridRespone == EIR_TwoWay) &&
					(ParticleSystemResponse == EIR_Produce || ParticleSystemResponse == EIR_TwoWay);

				if (GridAffectsParticleSystem && Scene->getExportParams(RHICmdList, ResultParamsList[Count]))
				{
					++Count;
				}
				if (ParticleSystemAffectsGrid)
				{
					Scene->m_particleParamsArray.Push(ParticleSimulationParams);
				}
			}
		}
		return Count;
	}
#endif // WITH_NVFLOW_BACKEND
	return 0;
}

#endif
// NvFlow end
