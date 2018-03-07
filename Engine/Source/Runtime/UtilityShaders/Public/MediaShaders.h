// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderResource.h"
#include "Shader.h"
#include "GlobalShader.h"


namespace MediaShaders
{
	/** Color transform from YUV to sRGB (using values from MSDN). */
	UTILITYSHADERS_API extern const FMatrix YuvToSrgbDefault;

	/** Color transform from YUV to sRGB (in JPEG color space). */
	UTILITYSHADERS_API extern const FMatrix YuvToSrgbJpeg;

	/** Color transform from YUV to sRGB (using values from PS4 AvPlayer codec). */
	UTILITYSHADERS_API extern const FMatrix YuvToSrgbPs4;

	/** Color transform from YUV to sRGB (in Rec. 601 color space). */
	UTILITYSHADERS_API extern const FMatrix YuvToSrgbRec601;

	/** Color transform from YUV to sRGB (in Rec. 709 color space). */
	UTILITYSHADERS_API extern const FMatrix YuvToRgbRec709;
}


/**
 * Stores media drawing vertices.
 */
struct FMediaElementVertex
{
	FVector4 Position;
	FVector2D TextureCoordinate;

	FMediaElementVertex() { }

	FMediaElementVertex(const FVector4& InPosition, const FVector2D& InTextureCoordinate)
		: Position(InPosition)
		, TextureCoordinate(InTextureCoordinate)
	{ }
};


/**
 * The simple element vertex declaration resource type.
 */
class FMediaVertexDeclaration
	: public FRenderResource
{
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual ~FMediaVertexDeclaration() { }

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		uint16 Stride = sizeof(FMediaElementVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FMediaElementVertex, Position), VET_Float4, 0, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FMediaElementVertex, TextureCoordinate), VET_Float2, 1, Stride));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};


UTILITYSHADERS_API extern TGlobalResource<FMediaVertexDeclaration> GMediaVertexDeclaration;


/**
 * Media vertex shader (shared by all media shaders).
 */
class FMediaShadersVS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FMediaShadersVS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}

	/** Default constructor. */
	FMediaShadersVS() { }

	/** Initialization constructor. */
	FMediaShadersVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
};


/**
 * Pixel shader to convert an AYUV texture to RGBA.
 *
 * This shader expects a single texture consisting of a N x M array of pixels
 * in AYUV format. Each pixel is encoded as four consecutive unsigned chars
 * with the following layout: [V0 U0 Y0 A0][V1 U1 Y1 A1]..
 */
class FAYUVConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FAYUVConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}

	FAYUVConvertPS() { }

	FAYUVConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> AYUVTexture, const FMatrix& ColorTransform, bool SrgbToLinear);
};


/**
 * Pixel shader to convert a Windows Bitmap texture.
 *
 * This shader expects a BMP frame packed into a single texture in PF_B8G8R8A8 format.
 */
class FBMPConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FBMPConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}

	FBMPConvertPS() { }

	FBMPConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> BMPTexture, const FIntPoint& OutputDimensions, bool SrgbToLinear);
};


/**
 * Pixel shader to convert a NV12 frame to RGBA.
 *
 * This shader expects an NV12 frame packed into a single texture in PF_G8 format.
 *
 * @see http://www.fourcc.org/yuv.php#NV12
 */
class FNV12ConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FNV12ConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}

	FNV12ConvertPS() { }

	FNV12ConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> NV12Texture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, bool SrgbToLinear);
};


/**
 * Pixel shader to convert a NV21 frame to RGBA.
 *
 * This shader expects an NV21 frame packed into a single texture in PF_G8 format.
 *
 * @see http://www.fourcc.org/yuv.php#NV21
 */
class FNV21ConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FNV21ConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}

	FNV21ConvertPS() { }

	FNV21ConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> NV21Texture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, bool SrgbToLinear);
};


/**
 * Pixel shader to resize an RGB texture.
 *
 * This shader expects an RGB or RGBA frame packed into a single texture
 * in PF_B8G8R8A8 or PF_FloatRGB format.
 */
class FRGBConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FRGBConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}

	FRGBConvertPS() { }

	FRGBConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> RGBTexture, const FIntPoint& OutputDimensions);
};


/**
 * Pixel shader to convert a PS4 YCbCr texture to RGBA.
 *
 * This shader expects a separate chroma and luma plane stored in two textures
 * in PF_B8G8R8A8 format. The full-size luma plane contains the Y-components.
 * The half-size chroma plane contains the UV components in the following
 * memory layout: [U0, V0][U1, V1]
 * 
 */
class FYCbCrConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FYCbCrConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}

	FYCbCrConvertPS() { }

	FYCbCrConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);		
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> LumaTexture, TRefCountPtr<FRHITexture2D> CbCrTexture, const FMatrix& ColorTransform, bool SrgbToLinear);
};

class FYCbCrConvertPS_4x4Matrix : public FYCbCrConvertPS
{
    DECLARE_EXPORTED_SHADER_TYPE(FYCbCrConvertPS_4x4Matrix, Global, UTILITYSHADERS_API);
    
public:
    
    static bool ShouldCache(EShaderPlatform Platform)
    {
        return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
    }
    
    FYCbCrConvertPS_4x4Matrix() { }
    
    FYCbCrConvertPS_4x4Matrix(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FYCbCrConvertPS(Initializer)
    { }
};


/**
 * Pixel shader to convert a UYVY (Y422, UYNV) frame to RGBA.
 *
 * This shader expects a UYVY frame packed into a single texture in PF_B8G8R8A8
 * format with the following memory layout: [U0, Y0, V1, Y1][U1, Y2, V1, Y3]..
 *
 * @see http://www.fourcc.org/yuv.php#UYVY
 */
class FUYVYConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FUYVYConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}

	FUYVYConvertPS() { }

	FUYVYConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> UYVYTexture, const FMatrix& ColorTransform, bool SrgbToLinear);
};


/**
 * Pixel shader to convert Y, U, and V planes to RGBA.
 *
 * This shader expects three textures in PF_G8 format,
 * one for each plane of Y, U, and V components.
 */
class FYUVConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FYUVConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}

	FYUVConvertPS() { }

	FYUVConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> YTexture, TRefCountPtr<FRHITexture2D> UTexture, TRefCountPtr<FRHITexture2D> VTexture, const FMatrix& ColorTransform, bool SrgbToLinear);
};


/**
 * Pixel shader to convert a YUY2 frame to RGBA.
 *
 * This shader expects an YUY2 frame packed into a single texture in PF_B8G8R8A8
 * format with the following memory layout: [Y0, U0, Y1, V0][Y2, U1, Y3, V1]...
 *
 * @see http://www.fourcc.org/yuv.php#YUY2
 */
class FYUY2ConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FYUY2ConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}

	FYUY2ConvertPS() { }

	FYUY2ConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> YUY2Texture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, bool SrgbToLinear);
};


/**
 * Pixel shader to convert a YVYU frame to RGBA.
 *
 * This shader expects a YVYU frame packed into a single texture in PF_B8G8R8A8
 * format with the following memory layout: [Y0, V0, Y1, U0][Y2, V1, Y3, U1]..
 *
 * @see http://www.fourcc.org/yuv.php#YVYU
 */
class FYVYUConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FYVYUConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}

	FYVYUConvertPS() { }

	FYVYUConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> YVYUTexture, const FMatrix& ColorTransform, bool SrgbToLinear);
};
