#pragma once

// NvFlow begin
struct FRHINvFlowDescriptorReserveHandle {};
struct FRHINvFlowDeviceDesc {};
struct FRHINvFlowDepthStencilViewDesc {};
struct FRHINvFlowRenderTargetViewDesc {};
struct FRHINvFlowResourceViewDesc {};
struct FRHINvFlowResourceRWViewDesc {};

class FRHINvFlowResourceRW : public FRHIResource
{
};

struct FRHINvFlowCleanup
{
protected:
	void(*m_func)(void*);
	void* m_ptr;
public:
	void Set(void(*func)(void*), void* ptr)
	{
		m_func = func;
		m_ptr = ptr;
	}
	FRHINvFlowCleanup() : m_func(nullptr), m_ptr(nullptr) {}
	~FRHINvFlowCleanup() { if (m_func) m_func(m_ptr); }
};
// NvFlow end