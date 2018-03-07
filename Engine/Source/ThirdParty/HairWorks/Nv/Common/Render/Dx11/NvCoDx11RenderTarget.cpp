#include <Nv/Common/NvCoCommon.h>

#include <Nv/Common/Render/Context/Dx11/NvCoDx11RenderInterface.h>

#include "NvCoDx11RenderTarget.h"

#include <Nv/Common/Render/Dx/NvCoDxFormatUtil.h>

namespace nvidia {
namespace Common {

Dx11RenderTarget::Dx11RenderTarget()
{
}

static D3D11_TEXTURE2D_DESC _getTextureDesc(DXGI_FORMAT format, UINT width, UINT height,
	UINT bindFlags, UINT sampleCount = 1, D3D11_USAGE usage = D3D11_USAGE_DEFAULT, UINT cpuAccessFlags = 0,
	UINT miscFlags = 0, UINT arraySize = 1, UINT mipLevels = 1)
{
	D3D11_TEXTURE2D_DESC desc;
	desc.Format = format;
	desc.Width = width;
	desc.Height = height;

	desc.ArraySize = arraySize;
	desc.MiscFlags = miscFlags;
	desc.MipLevels = mipLevels;

	desc.SampleDesc.Count = sampleCount;
	desc.SampleDesc.Quality = 0;
	desc.BindFlags = bindFlags;
	desc.Usage = usage;
	desc.CPUAccessFlags = cpuAccessFlags;
	return desc;
}

static D3D11_DEPTH_STENCIL_VIEW_DESC _getDsvDesc(DXGI_FORMAT format, D3D11_DSV_DIMENSION viewDimension, UINT flags = 0, UINT mipSlice = 0)
{
	D3D11_DEPTH_STENCIL_VIEW_DESC desc;
	desc.Format = format;
	desc.ViewDimension = viewDimension;
	desc.Flags = flags;
	desc.Texture2D.MipSlice = mipSlice;
	return desc;
}

Result Dx11RenderTarget::init(ID3D11Device* device, const Desc& desc)
{
	// set viewport
	{
		m_viewport.Width = float(desc.m_width);
		m_viewport.Height = float(desc.m_height);
		m_viewport.MinDepth = 0;
		m_viewport.MaxDepth = 1;
		m_viewport.TopLeftX = 0;
		m_viewport.TopLeftY = 0;
	}

	// create shadow render target
	if (desc.m_targetFormat != DXGI_FORMAT_UNKNOWN)
	{
		if (DxFormatUtil::isTypeless(desc.m_targetFormat))
		{
			const DXGI_FORMAT resourceFormat = DxFormatUtil::calcResourceFormat(DxFormatUtil::USAGE_TARGET, m_desc.m_usageFlags, desc.m_targetFormat);
			const DXGI_FORMAT targetFormat = DxFormatUtil::calcFormat(DxFormatUtil::USAGE_TARGET, resourceFormat);

			D3D11_TEXTURE2D_DESC texDesc = _getTextureDesc(resourceFormat, UINT(desc.m_width), UINT(desc.m_height),
				D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);

			NV_RETURN_ON_FAIL(device->CreateTexture2D(&texDesc, NV_NULL, m_backTexture.writeRef()));

			{
				D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
				srvDesc.Format = targetFormat;
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D.MipLevels = 1;
				srvDesc.Texture2D.MostDetailedMip = 0;
				NV_RETURN_ON_FAIL(device->CreateShaderResourceView(m_backTexture, &srvDesc, m_backSrv.writeRef()));
			}
			{
				D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
				rtvDesc.Format= targetFormat;
				rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
				rtvDesc.Texture2D.MipSlice = 0;
				NV_RETURN_ON_FAIL(device->CreateRenderTargetView(m_backTexture, &rtvDesc, m_backRtv.writeRef()));
			}
		}
		else
		{
			D3D11_TEXTURE2D_DESC texDesc = _getTextureDesc(desc.m_targetFormat, UINT(desc.m_width), UINT(desc.m_height),
				D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);

			NV_RETURN_ON_FAIL(device->CreateTexture2D(&texDesc, NV_NULL, m_backTexture.writeRef()));
			NV_RETURN_ON_FAIL(device->CreateShaderResourceView(m_backTexture, NV_NULL, m_backSrv.writeRef()));
			NV_RETURN_ON_FAIL(device->CreateRenderTargetView(m_backTexture, NV_NULL, m_backRtv.writeRef()));
		}
	}

	// create shadow depth stencil
	if (desc.m_depthStencilFormat != DXGI_FORMAT_UNKNOWN)
	{
		const DXGI_FORMAT resourceFormat = DxFormatUtil::calcResourceFormat(DxFormatUtil::USAGE_DEPTH_STENCIL, m_desc.m_usageFlags, desc.m_depthStencilFormat);
		const DXGI_FORMAT depthStencilFormat = DxFormatUtil::calcFormat(DxFormatUtil::USAGE_DEPTH_STENCIL, resourceFormat);

		D3D11_TEXTURE2D_DESC texDesc = _getTextureDesc(resourceFormat, UINT(desc.m_width), UINT(desc.m_height),
			D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE);
		NV_RETURN_ON_FAIL(device->CreateTexture2D(&texDesc, NV_NULL, m_depthTexture.writeRef()));
	
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = _getDsvDesc(depthStencilFormat, D3D11_DSV_DIMENSION_TEXTURE2D);
		NV_RETURN_ON_FAIL(device->CreateDepthStencilView(m_depthTexture, &dsvDesc, m_depthDsv.writeRef()));
	}

	m_desc = desc;

	return NV_OK;
}

void Dx11RenderTarget::bindAndClear(ID3D11DeviceContext* context)
{
	if (m_backTexture)
	{
		context->OMSetRenderTargets(1, m_backRtv.readRef(), m_depthDsv);
	}
	else
	{
		context->OMSetRenderTargets(0, NV_NULL, m_depthDsv);
	}

	context->RSSetViewports(1, &m_viewport);

	float clearDepth = FLT_MAX;
	float clearColor[4] = { clearDepth, clearDepth, clearDepth, clearDepth };

	if (m_backTexture)
	{
		context->ClearRenderTargetView(m_backRtv, clearColor);
	}
	if (m_depthTexture)
	{
		context->ClearDepthStencilView(m_depthDsv, D3D11_CLEAR_DEPTH, 1.0, 0);
	}
}

void Dx11RenderTarget::setShadowDefaultLight(FXMVECTOR eye, FXMVECTOR at, FXMVECTOR up)
{
	float sizeX = 50.0f;
	float sizeY = 50.0f;
	float znear = -200.0f;
	float zfar = 200.0f;

	setShadowLightMatrices(eye, at, up, sizeX, sizeY, znear, zfar);
}

void Dx11RenderTarget::setShadowLightMatrices(FXMVECTOR eye, FXMVECTOR lookAt, FXMVECTOR up, float sizeX, float sizeY, float zNear, float zFar)
{
	m_shadowLightView = XMMatrixLookAtLH(eye, lookAt, up);

	m_shadowLightProjection = XMMatrixOrthographicLH(sizeX, sizeY, zNear, zFar);

	DirectX::XMMATRIX clip2Tex(0.5, 0, 0, 0,
		0, -0.5, 0, 0,
		0, 0, 1, 0,
		0.5, 0.5, 0, 1);

	DirectX::XMMATRIX viewProjection = m_shadowLightView * m_shadowLightProjection;
	m_shadowLightWorldToTex = viewProjection * clip2Tex;
}

} // namespace Common 
} // namespace nvidia
