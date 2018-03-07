#ifndef NV_CO_DX_SHADER_UTIL_H
#define NV_CO_DX_SHADER_UTIL_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoString.h>
#include <Nv/Common/NvCoComPtr.h>

#include <Nv/Common/NvCoPathFinder.h>
#include <Nv/Common/Container/NvCoArray.h>

#include <Nv/Common/Render/NvCoRenderReadInfo.h>

#include <Nv/Core/1.0/NvResult.h>

#include <d3dcommon.h>
#include <dxgi.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

struct DxShaderUtil
{
	enum ShaderType
	{
		SHADER_VS,
		SHADER_GS,
		SHADER_HS,
		SHADER_DS,
		SHADER_PS,
	};
	// Versions are encoded as major * 100 + minor. 5.1 = 501
	enum Version
	{
		VERSION_5_0 = 5 * 100 + 0,
		VERSION_4_0 = 4 * 100 + 0,
	};

	struct Define
	{
		SubString m_name;
		SubString m_value;
	};
	struct Options
	{
		Options(Int version);

		Int m_shaderVersion;
		UINT m_flags1;
		UINT m_flags2;
		const Define* m_defines;
		Int m_numDefines;
	};

		/// Calculate the target string
	static Result calcTarget(ShaderType type, const Options& options, String& targetOut);

		/// Compile from code passed in
	static Result compile(ShaderType type, const SubString& code, const Char* entryPoint, const Options& options, ComPtr<ID3DBlob>& blobOut);

		/// Writes the text contained in blob to the log
	static Void writeToLog(ID3DBlob* blob);

	/// Works out asset path using app 
	/// Compiler flags are Flags1 from D3dCompile 
	/// https://msdn.microsoft.com/en-us/library/windows/desktop/gg615083%28v=vs.85%29.aspx

		/// Read a shader with shader paths
	static Result readShader(ShaderType type, const SubString& path, const char* entryPoint, const Options& options, ComPtr<ID3DBlob>& blobOut);

		/// Read a shader with shader paths
	static Result readShader(ShaderType type, const RenderReadInfo& info, const SubString& path, const char* entryPoint, const Options& options, ComPtr<ID3DBlob>& blobOut);

		/// Find a shader, and read it. (finds by searching paths from the executable path, upwards)
	static Result findAndReadShader(ShaderType type, const RenderReadInfo& info, const SubString& pathIn, const char* entryPoint, const Options& options, ComPtr<ID3DBlob>& blobOut);

		/// Calc macros. The buffer is used for storage of the output, and must stay in scope as long as macrosOut is valid
	static void calcMacros(const Options& options, String& buffer, Array<D3D_SHADER_MACRO>& macrosOut);
};

} // namespace Common
} // namespace nvidia

/** @} */

#endif // NV_CO_DX_SHADER_UTIL_H

