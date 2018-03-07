#include <Nv/Common/NvCoCommon.h>

#include "NvCoRenderContext.h"

namespace nvidia {
namespace Common {

RenderContext::RenderContext(Int width, Int height, RenderContextListener* listener):
	m_listener(listener)
{
	updateForSizeChange(width, height);
	AlignedVec4 color = { 0.5f, 0.5f, 0.5f, 1.0f };
	m_clearColor = color; 
}

Result RenderContext::initialize(const RenderContextOptions& options, Void* windowsHandle)
{
	NV_UNUSED(windowsHandle);
	m_options = options;
	return NV_OK;
}

void RenderContext::updateForSizeChange(Int clientWidth, Int clientHeight)
{
	m_width = clientWidth;
	m_height = clientHeight;
	m_aspectRatio = Float(clientWidth) / clientHeight;
}

} // namespace Common 
} // namespace nvidia
