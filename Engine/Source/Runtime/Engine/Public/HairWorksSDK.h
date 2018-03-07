// @third party code - BEGIN HairWorks
#pragma once

#include <Nv/HairWorks/NvHairSdk.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
class FRHIShaderResourceView;
class FCanvas;

namespace HairWorks{
	class FD3DHelper
	{
	public:
		virtual void SetShaderResourceView(ID3D11ShaderResourceView* Srv, int32 Index) = 0;
		virtual ID3D11ShaderResourceView* GetShaderResourceView(FRHIShaderResourceView* RHIShaderResourceView) = 0;
		virtual void CommitShaderResources() = 0;
	};

	ENGINE_API NvHair::Sdk* GetSDK();
	ENGINE_API const NvHair::ConversionSettings& GetAssetConversionSettings();
	ENGINE_API FD3DHelper& GetD3DHelper();
	ENGINE_API void Initialize(ID3D11Device& D3DDevice, ID3D11DeviceContext& D3DContext, FD3DHelper& D3DHelper);
	ENGINE_API void ShutDown();
#if STATS
	void RenderStats(int32 X, int32& Y, FCanvas* Canvas);
	ENGINE_API void AccumulateStats(const NvHair::Stats& HairStats);
#endif
}
// @third party code - END HairWorks
