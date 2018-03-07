#ifndef NV_CO_RENDER_READ_INFO_H
#define NV_CO_RENDER_READ_INFO_H

#include <Nv/Common/Render/Context/NvCoRenderContext.h>
#include <Nv/Common/NvCoPathFinder.h>
#include <Nv/Common/Container/NvCoArray.h>

namespace nvidia {
namespace Common {

/* Structure used for passing around information to enable reading data associated with the context */
struct RenderReadInfo
{
	RenderReadInfo():	
		m_context(NV_NULL), 
		m_finder(NV_NULL) 
	{
	}
	RenderContext* m_context;
	PathFinder* m_finder;
	Array<String> m_includePaths;					///< Semi colon delimited paths
};

} // namespace Common 
} // namespace nvidia

#endif // NV_CO_RENDER_READ_INFO_H