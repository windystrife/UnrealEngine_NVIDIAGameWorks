#include <Nv/Common/NvCoLogger.h>

#include <Nv/Common/Render/Dx/NvCoDxIncludeHandler.h>
#include <d3dcompiler.h>

// this
#include "NvCoDxShaderUtil.h"

namespace nvidia {
namespace Common {

DxShaderUtil::Options::Options(Int version) :
	m_shaderVersion(version),
	m_flags1(0),
	m_flags2(0),
	m_defines(NV_NULL),
	m_numDefines(0)
{
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	m_flags1 |= D3DCOMPILE_DEBUG;
#endif
}

/* static */Void DxShaderUtil::writeToLog(ID3DBlob* blob)
{
	if (blob)
	{
		String work;
		work = SubString((const Char*)blob->GetBufferPointer(), IndexT(blob->GetBufferSize()));
		NV_CO_LOG_ERROR(work.getCstr());
	}
}

static SubString _getShaderPrefix(DxShaderUtil::ShaderType type)
{
	typedef DxShaderUtil Util;
	switch (type)
	{
		case Util::SHADER_VS: return "vs";
		case Util::SHADER_PS: return "ps";
		case Util::SHADER_GS: return "gs";
		case Util::SHADER_DS: return "ds";
		case Util::SHADER_HS: return "hs";
		default: return SubString::getEmpty();
	}
}

/* static */Result DxShaderUtil::calcTarget(ShaderType type, const Options& options, String& targetOut) 
{
	targetOut = _getShaderPrefix(type);
	targetOut << '_' << (options.m_shaderVersion / 100) << '_' << (options.m_shaderVersion % 100);
	return NV_OK;
}

/* static */Result DxShaderUtil::compile(ShaderType type, const SubString& code, const Char* entryPoint, const Options& options, ComPtr<ID3DBlob>& blobOut)
{
	ComPtr<ID3DBlob> errorBlob;
	String target;
	NV_RETURN_ON_FAIL(calcTarget(type, options, target));
	Array<D3D_SHADER_MACRO> macros;
	String macroBuffer;
	calcMacros(options, macroBuffer, macros);

	Result res = D3DCompile(code.begin(), code.getSize(), NV_NULL, macros.begin(), NV_NULL, entryPoint, target.getCstr(), options.m_flags1, options.m_flags2, blobOut.writeRef(), errorBlob.writeRef());
	if (NV_FAILED(res))
	{
		writeToLog(errorBlob);
	}
	return res;
}

Result DxShaderUtil::findAndReadShader(ShaderType type, const RenderReadInfo& readInfo, const SubString& pathIn, const char* entryPoint, const Options& options, ComPtr<ID3DBlob>& blobOut)
{
	String path;
	if (!readInfo.m_finder->findPath(pathIn, path))
	{
		String buf;
		buf << "Couldn't find file '" << pathIn << "'";
		NV_CO_LOG_ERROR(buf.getCstr());
		return NV_FAIL;
	}

	return readShader(type, readInfo, path, entryPoint, options, blobOut);
}

/* static */Result DxShaderUtil::readShader(ShaderType type, const SubString& path, const char* entryPoint, const Options& options, ComPtr<ID3DBlob>& blobOut)
{
	RenderReadInfo readInfo;
	readInfo.m_includePaths.pushBack(SubString("."));
	return readShader(type, readInfo, path, entryPoint, options, blobOut);
}

Result DxShaderUtil::readShader(ShaderType type, const RenderReadInfo& readInfo, const SubString& path, const char* entryPoint, const Options& options, ComPtr<ID3DBlob>& blobOut)
{
	// Set up include handler for absolute and relative path lookup
	DxIncludeHandler includeHandler;
	includeHandler.addPathFromFile(path);

	for (Int i = 0; i < readInfo.m_includePaths.getSize(); i++)
	{
		includeHandler.addPath(readInfo.m_includePaths[i]);
	}

	String target;
	NV_RETURN_ON_FAIL(DxShaderUtil::calcTarget(type, options, target));
	Array<D3D_SHADER_MACRO> macros;
	String macroBuffer;
	calcMacros(options, macroBuffer, macros);
	
	ComPtr<ID3DBlob> errorBlob;

	WCHAR buffer[MAX_PATH];
	int size = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, path.begin(), int(path.getSize()), buffer, MAX_PATH);
	buffer[size] = 0;
	Result res = D3DCompileFromFile(buffer, macros.begin(), &includeHandler, entryPoint, target.getCstr(), options.m_flags1, options.m_flags2, blobOut.writeRef(), errorBlob.writeRef());

	if (NV_FAILED(res))
	{
		// Output to log unable to compile
		String buf;
		buf << "Unable compile '" << path << "'";
		NV_CO_LOG_ERROR(buf.getCstr());
		if (errorBlob)
		{
			String desc(SubString((const Char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize()));
			NV_CO_LOG_ERROR(desc.getCstr());
		}
		return res;
	}

	return NV_OK;
}

/* static */void DxShaderUtil::calcMacros(const Options& options, String& buffer, Array<D3D_SHADER_MACRO>& macrosOut)
{
	D3D_SHADER_MACRO end = {};
	
	macrosOut.clear();
	if (options.m_numDefines <= 0)
	{
		macrosOut.pushBack(end);
		return;
	}

	NV_CORE_ASSERT(options.m_defines);
	if (!options.m_defines)
	{
		return;
	}

	buffer.clear();
	// Okay need to make space in the buffer
	for (IndexT i = 0; i < options.m_numDefines; i++)
	{
		const Define& def = options.m_defines[i];
		buffer.concat(def.m_name);
		buffer.concat('\0');
		buffer.concat(def.m_value);
		buffer.concat('\0');
	}

	// Set up the macros list, need + 1 to have terminating value
	macrosOut.setSize(options.m_numDefines + 1);

	{
		const Char* pos = buffer.begin();
		for (IndexT i = 0; i < options.m_numDefines; i++)
		{
			const Define& def = options.m_defines[i];
			D3D_SHADER_MACRO& macro = macrosOut[i];

			macro.Name = pos;
			pos += def.m_name.getSize() + 1;
			macro.Definition = pos;
			pos += def.m_value.getSize() + 1;
		}
		// Mark the end
		macrosOut[options.m_numDefines] = end;
	}
}

} // namespace Common 
} // namespace nvidia
