#pragma once

// NvFlow begin
struct FPrimitiveSceneProxyNvFlow
{
	mutable uint32 bFlowGrid : 1;

	FPrimitiveSceneProxyNvFlow() : bFlowGrid(0) { }
};
// NvFlow end