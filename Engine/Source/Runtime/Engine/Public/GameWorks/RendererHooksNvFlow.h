#pragma once

// NvFlow begin

#include "Containers/Array.h"

class FRHICommandListImmediate;
class FRHICommandList;
class FPrimitiveSceneInfo;
class FViewInfo;

class FGlobalDistanceFieldParameterData;

struct RendererHooksNvFlow
{
	virtual bool NvFlowUsesGlobalDistanceField() const = 0;
	virtual void NvFlowUpdateScene(FRHICommandListImmediate& RHICmdList, TArray<FPrimitiveSceneInfo*>& Primitives, const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData) = 0;
	virtual bool NvFlowDoRenderPrimitive(FRHICommandList& RHICmdList, const FViewInfo& View, FPrimitiveSceneInfo* PrimitiveSceneInfo) = 0;
	virtual void NvFlowDoRenderFinish(FRHICommandListImmediate& RHICmdList, const FViewInfo& View) = 0;
	virtual bool NvFlowShouldDoPreComposite(FRHICommandListImmediate& RHICmdList) = 0;
	virtual void NvFlowDoPreComposite(FRHICommandListImmediate& RHICmdList, const FViewInfo& View) = 0;
};

extern ENGINE_API struct RendererHooksNvFlow* GRendererNvFlowHooks;

class FComponentVisualizersModule;

struct EditorRendererHooksNvFlow
{
	virtual void NvFlowRegisterVisualizer(FComponentVisualizersModule* module) = 0;
};

extern ENGINE_API struct EditorRendererHooksNvFlow* GEditorRendererHooksNvFlow;

// NvFlow end