#ifndef NV_CO_DX11_RENDER_INTERFACE_H
#define NV_CO_DX11_RENDER_INTERFACE_H

#include <d3d11.h>

#include <Nv/Common/NvCoApiHandle.h>

namespace nvidia {
namespace Common {

class Dx11RenderInterface
{
public:

	enum { API_TYPE = NvCo::ApiType::DX11 };

	virtual ID3D11Device* getDx11Device() const = 0;
	virtual ID3D11DeviceContext* getDx11DeviceContext() const = 0;

	virtual ~Dx11RenderInterface() {}
};

} // namespace Common 
} // namespace nvidia

#endif // NV_CO_DX11_RENDER_INTERFACE_H
