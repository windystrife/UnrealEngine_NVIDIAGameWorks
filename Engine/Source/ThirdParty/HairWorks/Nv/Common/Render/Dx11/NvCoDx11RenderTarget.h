#ifndef NV_CO_DX11_RENDER_TARGET_H
#define NV_CO_DX11_RENDER_TARGET_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoComPtr.h>

#include <Nv/Common/Math/NvCoMathTypes.h>

#include <Nv/Common/Render/Dx/NvCoDxFormatUtil.h>
#include <Nv/Common/Render/Context/Dx11/NvCoDx11RenderInterface.h>

#include <Nv/Core/1.0/NvResult.h>

#include <DirectXMath.h>

namespace nvidia {
namespace Common {
using namespace DirectX;

class Dx11RenderTarget 
{
	NV_CO_DECLARE_POLYMORPHIC_CLASS_BASE(Dx11RenderTarget);

	struct Desc
	{
		void init(Int width, Int height, DXGI_FORMAT targetFormat = DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_D32_FLOAT, Int usageFlags = 0)
		{
			m_width = width;
			m_height = height;
			m_targetFormat = targetFormat;
			m_depthStencilFormat = depthStencilFormat;
			m_usageFlags = usageFlags;

			const Vec4 clearColor = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
			m_targetClearColor = clearColor;
			m_depthStencilClearDepth = 1.0;
		}

		Int m_usageFlags;								///< Usage flags from DxFormatUtil
		Int m_width;
		Int m_height;
		DXGI_FORMAT m_depthStencilFormat;				///< DXGI_FORMAT_UNKNOWN means don't allocate resource
		DXGI_FORMAT m_targetFormat;						///< DXGI_FORMAT_UNKNOWN means don't allocate resource 
		Vec4 m_targetClearColor;
		float m_depthStencilClearDepth;
	};

	Result init(ID3D11Device* device, const Desc& desc);
	
	void setShadowDefaultLight(FXMVECTOR eye, FXMVECTOR at, FXMVECTOR up);
	void setShadowLightMatrices(FXMVECTOR eye, FXMVECTOR lookAt, FXMVECTOR up, float sizeX, float sizeY, float zNear, float zFar);

	void bindAndClear(ID3D11DeviceContext* context);

		/// Get the desc this was initialized with
	NV_FORCE_INLINE const Desc& getDesc() const { return m_desc; }

	Dx11RenderTarget();

	Desc m_desc;

	XMMATRIX m_shadowLightView;
	XMMATRIX m_shadowLightProjection;
	XMMATRIX m_shadowLightWorldToTex;

	ComPtr<ID3D11Texture2D> m_backTexture;
	ComPtr<ID3D11RenderTargetView> m_backRtv;
	ComPtr<ID3D11ShaderResourceView> m_backSrv;
	
	ComPtr<ID3D11Texture2D> m_depthTexture;
	ComPtr<ID3D11DepthStencilView> m_depthDsv;

	D3D11_VIEWPORT m_viewport;
};

} // namespace Common 
} // namespace nvidia

#endif // NV_CO_DX11_RENDER_TARGET_H