#pragma once

#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNvFlowEditor, Verbose, All);

#define NV_FLOW_SHADER_UTILS 0
#include "NvFlow.h"
#include "NvFlowContext.h"

#include "NvFlowEditorModule.h"

#include "FlowGridAsset.h"
#include "FlowMaterial.h"
#include "FlowRenderMaterial.h"
#include "FlowGridComponent.h"
#include "FlowGridActor.h"
