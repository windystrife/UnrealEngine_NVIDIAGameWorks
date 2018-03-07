#pragma once

#ifndef NVFLOW_ADAPTIVE
#define NVFLOW_ADAPTIVE 0
#endif

#ifndef WITH_CUDA_CONTEXT
#define WITH_CUDA_CONTEXT 0
#endif

#ifndef NVFLOW_SMP
#define NVFLOW_SMP 0
#endif

#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNvFlow, Verbose, All);

#define NV_FLOW_SHADER_UTILS 0
#include "NvFlow.h"
#include "NvFlowInline.h"
#include "NvFlowContext.h"
