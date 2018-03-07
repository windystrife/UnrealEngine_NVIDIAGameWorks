#pragma once

// NvFlow begin
struct FRHINvFlowDeviceDescD3D11 : FRHINvFlowDeviceDesc
{
	ID3D11Device* device;
	ID3D11DeviceContext* deviceContext;
};

struct FRHINvFlowDepthStencilViewDescD3D11 : FRHINvFlowDepthStencilViewDesc
{
	ID3D11DepthStencilView* dsv;
	ID3D11ShaderResourceView* srv;
	D3D11_VIEWPORT viewport;
};

struct FRHINvFlowRenderTargetViewDescD3D11 : FRHINvFlowRenderTargetViewDesc
{
	ID3D11RenderTargetView* rtv;
	D3D11_VIEWPORT viewport;
};

struct FRHINvFlowResourceViewDescD3D11 : FRHINvFlowResourceViewDesc
{
	ID3D11ShaderResourceView* srv;
};

struct FRHINvFlowResourceRWViewDescD3D11 : FRHINvFlowResourceRWViewDesc
{
	ID3D11ShaderResourceView* srv;
	ID3D11UnorderedAccessView* uav;
};
// NvFlow end