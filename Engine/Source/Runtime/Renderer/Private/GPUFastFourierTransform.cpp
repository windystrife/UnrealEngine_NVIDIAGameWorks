
#include "GPUFastFourierTransform.h"
#include "SceneUtils.h"
#include "GlobalShader.h"
#include "PostProcess/RenderTargetPool.h" // used for on-the-fly acquisition of 


uint32 GPUFFT::MaxScanLineLength() 
{
	return 4096;
}


 FString GPUFFT::FFT_TypeName(const FFT_XFORM_TYPE& XFormType)
{
	if (XFormType == FFT_XFORM_TYPE::FORWARD_HORIZONTAL) return FString(TEXT("Forward Horizontal"));
	if (XFormType == FFT_XFORM_TYPE::INVERSE_HORIZONTAL) return FString(TEXT("Inverse Horizontal"));
	if (XFormType == FFT_XFORM_TYPE::FORWARD_VERTICAL)   return FString(TEXT("Forward Vertical"));
	if (XFormType == FFT_XFORM_TYPE::INVERSE_VERTICAL)   return FString(TEXT("Inverse Vertical"));
	return FString(TEXT("Error"));
}

bool GPUFFT::IsHorizontal(const FFT_XFORM_TYPE& XFormType)
{
	return (XFormType == FFT_XFORM_TYPE::FORWARD_HORIZONTAL || XFormType == FFT_XFORM_TYPE::INVERSE_HORIZONTAL);
}
bool GPUFFT::IsForward(const FFT_XFORM_TYPE& XFormType)
{
	return (XFormType == FFT_XFORM_TYPE::FORWARD_HORIZONTAL || XFormType == FFT_XFORM_TYPE::FORWARD_VERTICAL);
}

GPUFFT::FFTDescription::FFTDescription(const GPUFFT::FFT_XFORM_TYPE& XForm, const FIntPoint& XFormExtent)
	:XFormType(XForm)
{
	if (GPUFFT::IsHorizontal(XFormType))
	{
		SignalLength = XFormExtent.X;
		NumScanLines = XFormExtent.Y;
	}
	else
	{
		SignalLength = XFormExtent.Y;
		NumScanLines = XFormExtent.X;
	}
}

FIntPoint GPUFFT::FFTDescription::TransformExtent() const
{
	const bool bIsHornizontal = GPUFFT::IsHorizontal(XFormType);

	FIntPoint Extent = (bIsHornizontal) ? FIntPoint(SignalLength, NumScanLines) : FIntPoint(NumScanLines, SignalLength);
	return Extent;
}

bool GPUFFT::FFTDescription::IsHorizontal() const
{
	return GPUFFT::IsHorizontal(XFormType);
}

bool GPUFFT::FFTDescription::IsForward() const
{
	return GPUFFT::IsForward(XFormType);
}

FString GPUFFT::FFTDescription::FFT_TypeName() const
{
	return GPUFFT::FFT_TypeName(XFormType);
}


namespace GPUFFT
{

	// Encode the transform type in the lower two bits
	static uint32 BitEncode(const GPUFFT::FFT_XFORM_TYPE& XFormType)
	{
		// put a 1 in the low bit for Horizontal
		uint32 BitEncodedValue = GPUFFT::IsHorizontal(XFormType) ? 1 : 0;
		// put a 1 in the second lowest bit for forward
		if (GPUFFT::IsForward(XFormType))
		{
			BitEncodedValue |= 2;
		}
		return BitEncodedValue;

	}

	/** 
	* Computes the minimal number of bits required to represent the in number N
	*/
	uint32 BitSize(uint32 N) 
	{
		uint32 Result = 0;
		while (N > 0) {
			N = N >> 1;
			Result++;
		}
		return Result;
	}

	/**
	* Decompose the input PowTwoLenght, as PowTwoLength = N X PowTwoBase X PowTwoBase X .. X PowTwoBase
	* NB: This assumes the PowTwoLength and PowTwoBase are powers of two. 
	*/
	TArray<uint32> GetFactors(const uint32 PowTwoLength, const uint32 PowTwoBase)
	{
		TArray<uint32> FactorList;

		// Early out. 
		if (!FMath::IsPowerOfTwo(PowTwoLength) || !FMath::IsPowerOfTwo(PowTwoBase)) return FactorList;

		const uint32 LogTwoLength = BitSize(PowTwoLength) - 1;
		const uint32 LogTwoBase = BitSize(PowTwoBase) - 1;

		const uint32 RemainderPower = LogTwoLength % LogTwoBase;
		const uint32 BasePower = LogTwoLength / LogTwoBase;

		for (uint32 idx = 0; idx < BasePower; idx++) 
		{
			FactorList.Add(PowTwoBase);
		}

		if (RemainderPower != 0)
		{
			uint32 Factor = 1 << RemainderPower;
			FactorList.Add(Factor);
		}
			
		return FactorList;
	}


	/**
	* Double buffer to manage RenderTargets during multi-pass FFTs.
	*/
	class  FDoubleBufferTargets
	{
	public:
		FDoubleBufferTargets(FSceneRenderTargetItem& InitialSrc, FSceneRenderTargetItem& InitialDst) :
				SrcIdx(0)
		{
			Targets[0] = &InitialSrc;
			Targets[1] = &InitialDst;
		}

		void Swap() { SrcIdx = 1 - SrcIdx; }

		// Return the index of the current Src target.  If it is 0 than this is the original src, otherwise
		// it is the original dst.
		const uint32&  GetSrcIdx() const { return SrcIdx; }

		// Access to the render targets with a return by reference for clear ownership semantics
		const FSceneRenderTargetItem& SrcTarget() const { return *Targets[SrcIdx]; }
		FSceneRenderTargetItem& DstTarget() { return *Targets[1 - SrcIdx]; }

	private:
		uint32 SrcIdx;
		FSceneRenderTargetItem* Targets[2];
	};
}



namespace GPUFFT
{

	class FReorderFFTPassCS : public FGlobalShader
	{
		// NB:  the following is actually "public:" 
		// due to text in the DECLARE_SHADER_TYPE Macro 
		DECLARE_SHADER_TYPE(FReorderFFTPassCS, Global);

	public:


		typedef FFT_XFORM_TYPE   FFT_XFORM_TYPE;

		FReorderFFTPassCS() {};

		FReorderFFTPassCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FGlobalShader(Initializer)
		{
			using GPUFFTComputeShaderUtils::FComputeParameterBinder;

			FComputeParameterBinder Binder(Initializer.ParameterMap);
			Binder(SrcROTexture,     TEXT("SrcSRV"))
				(DstRWTexture,       TEXT("DstUAV"))
				(TransformType,      TEXT("TransformType"))
				(SrcRect,            TEXT("SrcRect"))
				(DstRect,            TEXT("DstRect"))
				(LogTransformLength, TEXT("LogTwoLength"))
				(BitCount,           TEXT("BitCount"));
		}

		// Used by IMPLEMENT_SHADER_TYPE2
		static const TCHAR* GetSourceFilename() { return TEXT("/Engine/Private/GPUFastFourierTransform.usf"); }
		static const TCHAR* GetFunctionName() { return TEXT("ReorderFFTPassCS"); }

		static bool ShouldCache(EShaderPlatform Platform)
		{
			// @todo MetalMRT: Metal MRT can't cope with the threadgroup storage requirements for these shaders right now
			return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (!IsMetalPlatform(Platform) || RHIGetShaderLanguageVersion(Platform) >= 2) && (Platform != SP_METAL_MRT);
		}

		static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("INCLUDE_REORDER_FFT_PASS"), 1);
		}

		void SetCSParamters(FRHICommandList& RHICmdList,
			const FFT_XFORM_TYPE& XFormType,
			const FTextureRHIRef& SrcTexture, const FIntRect& SrcRectValue, const FIntRect& DstRectValue, const uint32 TransformLength, const uint32 PowTwoSubLengthCount, const bool bScrubNaNs)
		{
			using GPUFFTComputeShaderUtils::FComputeParamterValueSetter;

			const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

			// Set up the input.  We have to do this explicitly because the FFT dispatches multiple compute shaders and manages their input/output.
			FComputeParamterValueSetter ParamSetter(RHICmdList, ShaderRHI);

			ParamSetter(SrcROTexture, SrcTexture);

			// Translate the transform type. 
			uint32 TransformTypeValue = GPUFFT::BitEncode(XFormType);
			if (bScrubNaNs)
			{
				TransformTypeValue |= 4;
			}
		
			const uint32 BitCountValue =    BitSize(PowTwoSubLengthCount) - 1;
			const uint32 LogTwoTransformLength = BitSize(TransformLength) - 1;

			ParamSetter(TransformType, TransformTypeValue)
				(SrcRect, SrcRectValue)
				(DstRect, DstRectValue)
				(LogTransformLength, LogTwoTransformLength)
				(BitCount, BitCountValue);
		}

		// Method for use with the FScopedUAVBind
		FShaderResourceParameter& DestinationResourceParamter() { return DstRWTexture; }


		// FShader interface.
		virtual bool Serialize(FArchive& Ar) override
		{
			bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
			Ar << SrcROTexture
				<< DstRWTexture
				<< TransformType
				<< SrcRect
				<< DstRect
				<< LogTransformLength
				<< BitCount;
			return bShaderHasOutdatedParameters;
		}

	public:

		FShaderResourceParameter SrcROTexture;
		FShaderResourceParameter DstRWTexture;
		FShaderParameter TransformType;
		FShaderParameter SrcRect;
		FShaderParameter DstRect;
		FShaderParameter LogTransformLength;
		FShaderParameter BitCount;
	};


	class FGroupShardSubFFTPassCS : public FGlobalShader
	{
		// NB:  the following is actually "public:" 
		// due to text in the DECLARE_SHADER_TYPE Macro 
		DECLARE_SHADER_TYPE(FGroupShardSubFFTPassCS, Global);

	public:


		typedef FFT_XFORM_TYPE   FFT_XFORM_TYPE;

		FGroupShardSubFFTPassCS() {};

		FGroupShardSubFFTPassCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FGlobalShader(Initializer)
		{
			using GPUFFTComputeShaderUtils::FComputeParameterBinder;

			FComputeParameterBinder Binder(Initializer.ParameterMap);
			Binder(SrcROTexture,  TEXT("SrcTexture"))
				(DstRWTexture,    TEXT("DstTexture"))
				(TransformType,   TEXT("TransformType"))
				(SrcRect,         TEXT("SrcWindow"))
				(TransformLength, TEXT("TransformLength"))
				(NumSubRegions,   TEXT("NumSubRegions"));
		}

		// Used by IMPLEMENT_SHADER_TYPE2
		static const TCHAR* GetSourceFilename() { return TEXT("/Engine/Private/GPUFastFourierTransform.usf"); }
		static const TCHAR* GetFunctionName()   { return TEXT("GroupSharedSubComplexFFTCS"); }
		
		static uint32 SubPassLength() { return 2048;}
		static uint32 Radix() { return 2; }

		static bool ShouldCache(EShaderPlatform Platform)
		{
			// @todo MetalMRT: Metal MRT can't cope with the threadgroup storage requirements for these shaders right now
			return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (!IsMetalPlatform(Platform) || RHIGetShaderLanguageVersion(Platform) >= 2) && (Platform != SP_METAL_MRT);
		}

		static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("INCLUDE_GROUP_SHARED_SUB_COMPLEX_FFT"), 1);
			OutEnvironment.SetDefine(TEXT("SCAN_LINE_LENGTH"), FGroupShardSubFFTPassCS::SubPassLength());
			OutEnvironment.SetDefine(TEXT("RADIX"), FGroupShardSubFFTPassCS::Radix());
		

		}

		void SetCSParamters(FRHICommandList& RHICmdList,
			const FFT_XFORM_TYPE& XFormType,
			const uint32 TransformLengthValue,
			const FIntRect& WindowValue,
			const FTextureRHIRef& SrcTexture,
			const uint32 SubRegionCount)
		{


			using GPUFFTComputeShaderUtils::FComputeParamterValueSetter;

			const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

			// Set up the input.  We have to do this explicitly because the FFT dispatches multiple compute shaders and manages their input/output.
			FComputeParamterValueSetter ParamSetter(RHICmdList, ShaderRHI);

			ParamSetter(SrcROTexture, SrcTexture);

			// Translate the transform type. 
			uint32 TransformTypeValue = GPUFFT::BitEncode(XFormType);

			ParamSetter(TransformType, TransformTypeValue)
				(SrcRect, WindowValue)
				(TransformLength, TransformLengthValue)
				(NumSubRegions, SubRegionCount);
		}

		// Method for use with the FScopedUAVBind
		FShaderResourceParameter& DestinationResourceParamter() { return DstRWTexture; }


		// FShader interface.
		virtual bool Serialize(FArchive& Ar) override
		{
			bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
			Ar << SrcROTexture
				<< DstRWTexture
				<< TransformType
				<< SrcRect
				<< TransformLength
				<< NumSubRegions;
			return bShaderHasOutdatedParameters;
		}

	public:

		FShaderResourceParameter SrcROTexture;
		FShaderResourceParameter DstRWTexture;
		FShaderParameter TransformType;
		FShaderParameter SrcRect;
		FShaderParameter TransformLength;
		FShaderParameter NumSubRegions;
	};

	class FComplexFFTPassCS : public FGlobalShader
	{
	// NB:  the following is actually "public:" 
	// due to text in the DECLARE_SHADER_TYPE Macro 
	DECLARE_SHADER_TYPE(FComplexFFTPassCS, Global);
	
	public:


		typedef FFT_XFORM_TYPE   FFT_XFORM_TYPE;

		FComplexFFTPassCS() {};

		FComplexFFTPassCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FGlobalShader(Initializer)
		{
			using GPUFFTComputeShaderUtils::FComputeParameterBinder;

			FComputeParameterBinder Binder(Initializer.ParameterMap);
			Binder(SrcROTexture, TEXT("SrcSRV"))
				(DstRWTexture, TEXT("DstUAV"))
				(TransformType, TEXT("TransformType"))
				(SrcRect, TEXT("SrcRect"))
				(DstRect, TEXT("DstRect"))
				(BitCount, TEXT("BitCount"))
				(PowTwoLength, TEXT("PowTwoLength"));
		}

		// Used by IMPLEMENT_SHADER_TYPE2
		static const TCHAR* GetSourceFilename() { return TEXT("/Engine/Private/GPUFastFourierTransform.usf"); }
		static const TCHAR* GetFunctionName()   { return TEXT("ComplexFFTPassCS"); }

		static bool ShouldCache(EShaderPlatform Platform)
		{
			// @todo MetalMRT: Metal MRT can't cope with the threadgroup storage requirements for these shaders right now
			return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (!IsMetalPlatform(Platform) || RHIGetShaderLanguageVersion(Platform) >= 2) && (Platform != SP_METAL_MRT);
		}

		static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("INCLUDE_COMPLEX_FFT_PASS"), 1);
		}

		void SetCSParamters(FRHICommandList& RHICmdList,
			const FFT_XFORM_TYPE& XFormType,
			const FTextureRHIRef& SrcTexture, const FIntRect& SrcRectValue, const FIntRect& DstRectValue, const uint32 TransformLength, const uint32 PassLength, const bool bScrubNaNs)
		{
			const uint32 BitCountValue = BitSize(TransformLength);
			const uint32 PowTwo        = PassLength;  // The pass number should be log(2, PassLength)

			using GPUFFTComputeShaderUtils::FComputeParamterValueSetter;

			const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

			// Set up the input.  We have to do this explicitly because the FFT dispatches multiple compute shaders and manages their input/output.
			FComputeParamterValueSetter ParamSetter(RHICmdList, ShaderRHI);

			ParamSetter(SrcROTexture, SrcTexture);

			// Translate the transform type. 
			uint32 TransformTypeValue = GPUFFT::BitEncode(XFormType);
			if (bScrubNaNs)
			{
				TransformTypeValue |= 4;
			}

			ParamSetter(TransformType, TransformTypeValue)
				(SrcRect, SrcRectValue)
				(DstRect, DstRectValue)
				(BitCount, BitCountValue)
				(PowTwoLength, PowTwo);
		}

		// Method for use with the FScopedUAVBind
		FShaderResourceParameter& DestinationResourceParamter() { return DstRWTexture; }


		// FShader interface.
		virtual bool Serialize(FArchive& Ar) override
		{
			bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
			Ar << SrcROTexture
				<< DstRWTexture
				<< TransformType
				<< SrcRect
				<< DstRect
				<< BitCount
				<< PowTwoLength;
			return bShaderHasOutdatedParameters;
		}

	public:

		FShaderResourceParameter SrcROTexture;
		FShaderResourceParameter DstRWTexture;
		FShaderParameter TransformType;
		FShaderParameter SrcRect;
		FShaderParameter DstRect;
		FShaderParameter BitCount;
		FShaderParameter PowTwoLength;
	};


	class FPackTwoForOneFFTPassCS : public FGlobalShader
	{
		// NB:  the following is actually "public:" 
		// due to text in the DECLARE_SHADER_TYPE Macro 
		DECLARE_SHADER_TYPE(FPackTwoForOneFFTPassCS, Global);

	public:


		typedef FFT_XFORM_TYPE   FFT_XFORM_TYPE;

		FPackTwoForOneFFTPassCS() {};

		FPackTwoForOneFFTPassCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FGlobalShader(Initializer)
		{
			using GPUFFTComputeShaderUtils::FComputeParameterBinder;

			FComputeParameterBinder Binder(Initializer.ParameterMap);
			Binder(SrcROTexture,  TEXT("SrcSRV"))
					(DstRWTexture,  TEXT("DstUAV"))
					(TransformType, TEXT("TransformType"))
					(DstRect,       TEXT("DstRect"));
		}

		// Used by IMPLEMENT_SHADER_TYPE2
		static const TCHAR* GetSourceFilename() { return TEXT("/Engine/Private/GPUFastFourierTransform.usf"); }
		static const TCHAR* GetFunctionName() { return TEXT("PackTwoForOneFFTPassCS"); }

		static bool ShouldCache(EShaderPlatform Platform)
		{
			// @todo MetalMRT: Metal MRT can't cope with the threadgroup storage requirements for these shaders right now
			return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (!IsMetalPlatform(Platform) || RHIGetShaderLanguageVersion(Platform) >= 2) && (Platform != SP_METAL_MRT);
		}

		static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("INCLUDE_PACK_TWOFORONE_FFT_PASS"), 1);
		}

		void SetCSParamters(FRHICommandList& RHICmdList,
			const FFT_XFORM_TYPE& XFormType,
			const FTextureRHIRef& SrcTexture, const FIntRect& DstRectValue)
		{
			using GPUFFTComputeShaderUtils::FComputeParamterValueSetter;

			const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

			// Set up the input.  We have to do this explicitly because the FFT dispatches multiple compute shaders and manages their input/output.
			FComputeParamterValueSetter ParamSetter(RHICmdList, ShaderRHI);

			ParamSetter(SrcROTexture, SrcTexture);

			// Translate the transform type. 
			uint32 TransformTypeValue = GPUFFT::BitEncode(XFormType);

			ParamSetter(TransformType, TransformTypeValue)
				(DstRect, DstRectValue);
		}

		// Method for use with the FScopedUAVBind
		FShaderResourceParameter& DestinationResourceParamter() { return DstRWTexture; }


		// FShader interface.
		virtual bool Serialize(FArchive& Ar) override
		{
			bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
			Ar << SrcROTexture
				<< DstRWTexture
				<< TransformType
				<< DstRect;
			return bShaderHasOutdatedParameters;
		}

	public:

		FShaderResourceParameter SrcROTexture;
		FShaderResourceParameter DstRWTexture;
		FShaderParameter TransformType;
		FShaderParameter DstRect;
	};


	class FCopyWindowCS : public FGlobalShader
	{
		// NB:  the following is actually "public:" 
		// due to text in the DECLARE_SHADER_TYPE Macro 
		DECLARE_SHADER_TYPE(FCopyWindowCS, Global);

	public:

		FCopyWindowCS() {};

		FCopyWindowCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FGlobalShader(Initializer)
		{
			using GPUFFTComputeShaderUtils::FComputeParameterBinder;

			FComputeParameterBinder Binder(Initializer.ParameterMap);
			Binder(SrcROTexture, TEXT("SrcSRV"))
				(DstRWTexture, TEXT("DstUAV"))
				(SrcRect, TEXT("SrcRect"))
				(DstRect, TEXT("DstRect"))
				(PreFilter, TEXT("BrightPixelGain"));
		}

		// Used by IMPLEMENT_SHADER_TYPE2
		static const TCHAR* GetSourceFilename() { return TEXT("/Engine/Private/GPUFastFourierTransform.usf"); }
		static const TCHAR* GetFunctionName()   { return TEXT("CopyWindowCS"); }

		static uint32 XThreadCount() { return 1; }
		static uint32 YThreadCount() { return 32; }

		static bool ShouldCache(EShaderPlatform Platform)
		{
			// @todo MetalMRT: Metal MRT can't cope with the threadgroup storage requirements for these shaders right now
			return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (!IsMetalPlatform(Platform) || RHIGetShaderLanguageVersion(Platform) >= 2) && (Platform != SP_METAL_MRT);
		}

		static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("INCLUDE_COPY_WINDOW"), 1);
			OutEnvironment.SetDefine(TEXT("X_THREAD_COUNT"), XThreadCount());
			OutEnvironment.SetDefine(TEXT("Y_THREAD_COUNT"), YThreadCount());
		}

		void SetCSParamters(FRHICommandList& RHICmdList,
			const FIntRect& SrcRectValue,
			const FTextureRHIRef& SrcTexture, const FIntRect& DstRectValue,
			const FPreFilter& PreFilterValue)
		{
			using GPUFFTComputeShaderUtils::FComputeParamterValueSetter;

			const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

			// Set up the input.  We have to do this explicitly because the FFT dispatches multiple compute shaders and manages their input/output.
			FComputeParamterValueSetter ParamSetter(RHICmdList, ShaderRHI);

			ParamSetter(SrcROTexture, SrcTexture);
			ParamSetter(SrcRect, SrcRectValue)
				(DstRect, DstRectValue)
				(PreFilter, PreFilterValue);
		}

		// Method for use with the FScopedUAVBind
		FShaderResourceParameter& DestinationResourceParamter() { return DstRWTexture; }


		// FShader interface.
		virtual bool Serialize(FArchive& Ar) override
		{
			bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
			Ar << SrcROTexture
				<< DstRWTexture
				<< SrcRect
				<< DstRect
				<< PreFilter;
			return bShaderHasOutdatedParameters;
		}

	public:

		FShaderResourceParameter SrcROTexture;
		FShaderResourceParameter DstRWTexture;
		FShaderParameter SrcRect;
		FShaderParameter DstRect;
		FShaderParameter PreFilter;
	};

	class FComplexMultiplyImagesCS : public FGlobalShader
	{
		// NB:  the following is actually "public:" 
		// due to text in the DECLARE_SHADER_TYPE Macro 
		DECLARE_SHADER_TYPE(FComplexMultiplyImagesCS, Global);

	public:

		FComplexMultiplyImagesCS() {};

		FComplexMultiplyImagesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FGlobalShader(Initializer)
		{
			using GPUFFTComputeShaderUtils::FComputeParameterBinder;

			FComputeParameterBinder Binder(Initializer.ParameterMap);
			Binder(SrcROTexture, TEXT("SrcSRV"))
				(KnlROTexture, TEXT("KnlSRV"))
				(DstRWTexture, TEXT("DstUAV"))
				(SrcRect, TEXT("SrcRect"))
				(DataLayout, TEXT("DataLayout"));
		}

		// Used by IMPLEMENT_SHADER_TYPE2
		static const TCHAR* GetSourceFilename() { return TEXT("/Engine/Private/GPUFastFourierTransform.usf"); }
		static const TCHAR* GetFunctionName() { return TEXT("ComplexMultiplyImagesCS"); }


		static bool ShouldCache(EShaderPlatform Platform)
		{
			// @todo MetalMRT: Metal MRT can't cope with the threadgroup storage requirements for these shaders right now
			return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (!IsMetalPlatform(Platform) || RHIGetShaderLanguageVersion(Platform) >= 2) && (Platform != SP_METAL_MRT);
		}

		static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("INCLUDE_COMPLEX_MULTIPLY_IMAGES"), 1);
		}

		void SetCSParamters(FRHICommandList& RHICmdList,
			const bool bHorizontalScanlines,
			const FIntRect& SrcRectValue,
			const FTextureRHIRef& SrcTexture, 
			const FTextureRHIRef& KnlTexture)
		{
			using GPUFFTComputeShaderUtils::FComputeParamterValueSetter;

			const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

			const uint32 DataLayoutValue = (bHorizontalScanlines) ? 1 : 0;
			// Set up the input.  We have to do this explicitly because the FFT dispatches multiple compute shaders and manages their input/output.
			FComputeParamterValueSetter ParamSetter(RHICmdList, ShaderRHI);

			ParamSetter(SrcROTexture, SrcTexture)
				(KnlROTexture, KnlTexture)
				(SrcRect, SrcRectValue)
				(DataLayout, DataLayoutValue);

		}

		// Method for use with the FScopedUAVBind
		FShaderResourceParameter& DestinationResourceParamter() { return DstRWTexture; }


		// FShader interface.
		virtual bool Serialize(FArchive& Ar) override
		{
			bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
			Ar << SrcROTexture
				<< KnlROTexture
				<< DstRWTexture
				<< SrcRect
				<< DataLayout;
			return bShaderHasOutdatedParameters;
		}

	public:

		FShaderResourceParameter SrcROTexture;
		FShaderResourceParameter KnlROTexture;
		FShaderResourceParameter DstRWTexture;
		FShaderParameter SrcRect;
		FShaderParameter DataLayout;
	};

	class FGSComplexTransformBaseCS : public FGlobalShader
	{

	public:
		

		typedef FFT_XFORM_TYPE   FFT_XFORM_TYPE;

		FGSComplexTransformBaseCS() {};

		FGSComplexTransformBaseCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FGlobalShader(Initializer)
		{
			using GPUFFTComputeShaderUtils::FComputeParameterBinder;
		
			FComputeParameterBinder Binder(Initializer.ParameterMap);
			Binder(SrcROTexture,  TEXT("SrcTexture"))
				(DstRWTexture,    TEXT("DstTexture"))
				(TransformType,   TEXT("TransformType"))
				(SrcRectMin,      TEXT("SrcRectMin"))
				(SrcRectMax,      TEXT("SrcRectMax"))
				(DstExtent,       TEXT("DstExtent"))
				(DstRect,         TEXT("DstRect"))
				(BrightPixelGain, TEXT("BrightPixelGain"));
		}


		void SetCSParamters(FRHICommandList& RHICmdList,
			const FFT_XFORM_TYPE& XFormType,
			const FTextureRHIRef& SrcTexture, const FIntRect& SrcRect, const FIntRect& DstRectValue,
			const GPUFFT::FPreFilter& PreFilterParameters = GPUFFT::FPreFilter(TNumericLimits<float>::Max(), TNumericLimits<float>::Lowest(), 0.f))
		{
			using GPUFFTComputeShaderUtils::FComputeParamterValueSetter;

			const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

			// Set up the input.  We have to do this explicitly because the FFT dispatches multiple compute shaders and manages their input/output.
			FComputeParamterValueSetter ParamSetter(RHICmdList, ShaderRHI);

			ParamSetter(SrcROTexture, SrcTexture);

			// Translate the transform type. 
			uint32 TransformTypeValue = GPUFFT::BitEncode(XFormType);

			// We have a valid prefilter if the min is less than the max
			if (PreFilterParameters.Component(0) < PreFilterParameters.Component(1))
			{
				// Encode a bool to turn on the pre-filter.
				TransformTypeValue |= 4;
			}

			ParamSetter(TransformType, TransformTypeValue)
				(SrcRectMin,      SrcRect.Min)
				(SrcRectMax,      SrcRect.Max)
				(DstRect,         DstRectValue)
				(DstExtent,       DstRectValue.Size())
				(BrightPixelGain, PreFilterParameters);
		}

		// Method for use with the FScopedUAVBind
		FShaderResourceParameter& DestinationResourceParamter() { return DstRWTexture; }


		// FShader interface.
		virtual bool Serialize(FArchive& Ar) override
		{
			bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
			Ar << SrcROTexture
				<< DstRWTexture
				<< TransformType
				<< SrcRectMin
				<< SrcRectMax
				<< DstExtent
				<< DstRect
				<< BrightPixelGain;
			return bShaderHasOutdatedParameters;
		}

	public:

		FShaderResourceParameter SrcROTexture;
		FShaderResourceParameter DstRWTexture;
		FShaderParameter TransformType;
		FShaderParameter SrcRectMin;
		FShaderParameter SrcRectMax;
		FShaderParameter DstExtent;
		FShaderParameter DstRect;
		FShaderParameter BrightPixelGain;
	};

	template <int PowRadixSignalLength>
	class TGSComplexTransformCS : public FGSComplexTransformBaseCS
	{
		// NB:  the following is actually "public:" 
		// due to text in the DECLARE_SHADER_TYPE Macro 
		DECLARE_SHADER_TYPE(TGSComplexTransformCS, Global);

		// Used by IMPLEMENT_SHADER_TYPE2
		static const TCHAR* GetSourceFilename() { return TEXT("/Engine/Private/GPUFastFourierTransform.usf"); }
		static const TCHAR* GetFunctionName()   { return TEXT("GroupSharedComplexFFTCS"); }

		static bool ShouldCache(EShaderPlatform Platform)
		{
			// @todo MetalMRT: Metal MRT can't cope with the threadgroup storage requirements for these shaders right now
			return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (!IsMetalPlatform(Platform) || RHIGetShaderLanguageVersion(Platform) >= 2) && (Platform != SP_METAL_MRT);
		}


		static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("INCLUDE_GROUP_SHARED_COMPLEX_FFT"), 1);
			OutEnvironment.SetDefine(TEXT("SCAN_LINE_LENGTH"), PowRadixSignalLength);
		}

		/** Default constructor **/
		TGSComplexTransformCS() {};

	public:

		TGSComplexTransformCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FGSComplexTransformBaseCS(Initializer)
		{}
	};



	template < int PowRadixSignalLength>
	class TGSTwoForOneTransformCS : public FGSComplexTransformBaseCS
	{
		// NB:  the following is actually "public:" 
		// due to text in the DECLARE_SHADER_TYPE Macro 
		DECLARE_SHADER_TYPE(TGSTwoForOneTransformCS, Global);

		// Used by IMPLEMENT_SHADER_TYPE2
		static const TCHAR* GetSourceFilename() { return TEXT("/Engine/Private/GPUFastFourierTransform.usf"); }
		static const TCHAR* GetFunctionName()   { return TEXT("GroupSharedTwoForOneFFTCS"); }

		static bool ShouldCache(EShaderPlatform Platform)
		{
			// @todo MetalMRT: Metal MRT can't cope with the threadgroup storage requirements for these shaders right now
			return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (!IsMetalPlatform(Platform) || RHIGetShaderLanguageVersion(Platform) >= 2) && (Platform != SP_METAL_MRT);
		}


		static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("INCLUDE_GROUP_SHARED_TWO_FOR_ONE_FFT"), 1);
			OutEnvironment.SetDefine(TEXT("SCAN_LINE_LENGTH"), PowRadixSignalLength);
		}

		/** Default constructor **/
		TGSTwoForOneTransformCS() {};

	public:

		TGSTwoForOneTransformCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FGSComplexTransformBaseCS(Initializer)
		{}
	};

	/**
	* Base class used for 1d convolution.  
	*/
	class FGSConvolutionBaseCS : public FGlobalShader
	{

	public:

		FGSConvolutionBaseCS() {};

		FGSConvolutionBaseCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FGlobalShader(Initializer)
		{
			using GPUFFTComputeShaderUtils::FComputeParameterBinder;

			FComputeParameterBinder Binder(Initializer.ParameterMap);
			Binder(SrcROTexture, TEXT("SrcTexture"))
				(DstRWTexture,   TEXT("DstTexture"))
				(SrcRectMin,     TEXT("SrcRectMin"))
				(SrcRectMax,     TEXT("SrcRectMax"))
				(DstExtent,      TEXT("DstExtent"))
				(TransformType,  TEXT("TransformType"));
		}


		// @todo - template this on KernelType.  Have KernelType know how to do its own SetShaderValue
		// Note: We pass in the 2d spectral size (transform size) to help the filter
		void SetCSParamters(FRHICommandList& RHICmdList, const FFT_XFORM_TYPE& XFormType,
			const FTextureRHIRef& SrcTexture, const FIntRect& SrcRect, const FIntPoint& DstExtentValue)
		{

			using GPUFFTComputeShaderUtils::FComputeParamterValueSetter;

			// Translate the transform type. 
			uint32 TransformTypeValue = GPUFFT::BitEncode(XFormType);

			const bool bUseAlpha = true;

			if (bUseAlpha)
			{
				TransformTypeValue |= 8;
			}

			const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

			// Set up the input.  We have to do this explicitly because the FFT dispatches multiple compute shaders and manages their input/output.

			FComputeParamterValueSetter ParamSetter(RHICmdList, ShaderRHI);
			ParamSetter(SrcROTexture, SrcTexture); // set the texture

													// set all the other parameters
			ParamSetter(SrcRectMin, SrcRect.Min)
				(SrcRectMax,        SrcRect.Max)
				(DstExtent,         DstExtentValue)
				(TransformType,     TransformTypeValue);
		}

		// Method for use with the FScopedUAVBind
		FShaderResourceParameter& DestinationResourceParamter() { return DstRWTexture; }


		// FShader interface.
		virtual bool Serialize(FArchive& Ar) override
		{
			bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
			Ar << SrcROTexture
				<< DstRWTexture
				<< SrcRectMin
				<< SrcRectMax
				<< DstExtent
				<< TransformType;
			return bShaderHasOutdatedParameters;
		}

	public:

		FShaderResourceParameter SrcROTexture;
		FShaderResourceParameter DstRWTexture;
		FShaderParameter SrcRectMin;
		FShaderParameter SrcRectMax;
		FShaderParameter DstExtent;
		FShaderParameter TransformType;
	};

	/**
	* Common class used by the shader permutations of the convolution with texture
	*/
	class FGSConvolutionWithTextureKernelBaseCS : public FGSConvolutionBaseCS
	{

	public:

		FGSConvolutionWithTextureKernelBaseCS() {};

		FGSConvolutionWithTextureKernelBaseCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FGSConvolutionBaseCS(Initializer)
		{
			GPUFFTComputeShaderUtils::FComputeParameterBinder Binder(Initializer.ParameterMap);

			Binder(FilterSrcROTexture, TEXT("FilterTexture"));
		}

		// Used by IMPLEMENT_SHADER_TYPE2
		static const TCHAR* GetSourceFilename() { return TEXT("/Engine/Private/GPUFastFourierTransform.usf"); }
		static const TCHAR* GetFunctionName()   { return TEXT("GSConvolutionWithTextureCS"); }
	


		// @todo - template this on KernelType.  Have KernelType know how to do its own SetShaderValue
		// Note: We pass in the 2d spectral size (transform size) to help the filter
		void SetCSParamters(FRHICommandList& RHICmdList,
			const FFT_XFORM_TYPE& XFormType,
			const FTextureRHIRef& FilterSrcTexture,
			const FTextureRHIRef& SrcTexture, const FIntRect& SrcRect, const FIntPoint& DstExtentValue)
		{

			FGSConvolutionBaseCS::SetCSParamters(RHICmdList, XFormType, SrcTexture, SrcRect, DstExtentValue);

			// additional source input for sampling the spectral texture
			const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
			GPUFFTComputeShaderUtils::FComputeParamterValueSetter ParamSetter(RHICmdList, ShaderRHI);
			// set the texture
			ParamSetter(FilterSrcROTexture, FilterSrcTexture);

		}

		// FShader interface.
		virtual bool Serialize(FArchive& Ar) override
		{
			bool bShaderHasOutdatedParameters = FGSConvolutionBaseCS::Serialize(Ar);
			Ar << FilterSrcROTexture;
			return bShaderHasOutdatedParameters;
		}

	public:
		FShaderResourceParameter FilterSrcROTexture;
	};


	template <int PowRadixSignalLength>
	class TGSConvolutionWithTexturerCS : public FGSConvolutionWithTextureKernelBaseCS
	{

		// NB:  the following is actually "public:" 
		// due to text in the DECLARE_SHADER_TYPE Macro 
		DECLARE_SHADER_TYPE(TGSConvolutionWithTexturerCS, Global);



		static bool ShouldCache(EShaderPlatform Platform)
		{
			// @todo MetalMRT: Metal MRT can't cope with the threadgroup storage requirements for these shaders right now
			return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (!IsMetalPlatform(Platform) || RHIGetShaderLanguageVersion(Platform) >= 2) && (Platform != SP_METAL_MRT);
		}


		static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("INCLUDE_GROUP_SHARED_CONVOLUTION_WITH_TEXTURE"), 1);
			OutEnvironment.SetDefine(TEXT("SCAN_LINE_LENGTH"), PowRadixSignalLength);
		}

		/** Default constructor **/
		TGSConvolutionWithTexturerCS() {};

	public:

		TGSConvolutionWithTexturerCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FGSConvolutionWithTextureKernelBaseCS(Initializer)
		{}
	};


	void SwapContents(FSceneRenderTargetItem& TmpBuffer, FSceneRenderTargetItem& DstBuffer)
	{
		// Swap the pointers
		FSceneRenderTargetItem TmpDst = DstBuffer;
		DstBuffer = TmpBuffer;
		TmpBuffer = TmpDst;
	}
} // end namespace

IMPLEMENT_SHADER_TYPE3(GPUFFT::FReorderFFTPassCS, SF_Compute);
IMPLEMENT_SHADER_TYPE3(GPUFFT::FGroupShardSubFFTPassCS, SF_Compute);
IMPLEMENT_SHADER_TYPE3(GPUFFT::FComplexFFTPassCS, SF_Compute);
IMPLEMENT_SHADER_TYPE3(GPUFFT::FPackTwoForOneFFTPassCS, SF_Compute);
IMPLEMENT_SHADER_TYPE3(GPUFFT::FCopyWindowCS, SF_Compute);
IMPLEMENT_SHADER_TYPE3(GPUFFT::FComplexMultiplyImagesCS, SF_Compute);

#define GROUPSHARED_COMPLEX_TRANSFORM(_Length) \
typedef GPUFFT::TGSComplexTransformCS< _Length> FGSComplexTransformCSLength##_Length; \
IMPLEMENT_SHADER_TYPE2(FGSComplexTransformCSLength##_Length, SF_Compute);

GROUPSHARED_COMPLEX_TRANSFORM(2)  GROUPSHARED_COMPLEX_TRANSFORM(16)  GROUPSHARED_COMPLEX_TRANSFORM(128)   GROUPSHARED_COMPLEX_TRANSFORM(1024)

GROUPSHARED_COMPLEX_TRANSFORM(4)  GROUPSHARED_COMPLEX_TRANSFORM(32)  GROUPSHARED_COMPLEX_TRANSFORM(256)   GROUPSHARED_COMPLEX_TRANSFORM(2048)  

GROUPSHARED_COMPLEX_TRANSFORM(8)  GROUPSHARED_COMPLEX_TRANSFORM(64)  GROUPSHARED_COMPLEX_TRANSFORM(512)   GROUPSHARED_COMPLEX_TRANSFORM(4096)
// NB: FFTBLOCK(8192, false/true) won't work because the max number of threads in a group 1024 is less than the requested 8192 / 2 

#undef GROUPSHARED_COMPLEX_TRANSFORM



#define GROUPSHARED_TWO_FOR_ONE_TRANSFORM(_Length) \
typedef GPUFFT::TGSTwoForOneTransformCS<_Length> FGSTwoForOneTransformCSLength##_Length; \
IMPLEMENT_SHADER_TYPE2(FGSTwoForOneTransformCSLength##_Length, SF_Compute);

GROUPSHARED_TWO_FOR_ONE_TRANSFORM(2)  GROUPSHARED_TWO_FOR_ONE_TRANSFORM(16)  GROUPSHARED_TWO_FOR_ONE_TRANSFORM(128)   GROUPSHARED_TWO_FOR_ONE_TRANSFORM(1024)

GROUPSHARED_TWO_FOR_ONE_TRANSFORM(4)  GROUPSHARED_TWO_FOR_ONE_TRANSFORM(32)  GROUPSHARED_TWO_FOR_ONE_TRANSFORM(256)   GROUPSHARED_TWO_FOR_ONE_TRANSFORM(2048)  
 
GROUPSHARED_TWO_FOR_ONE_TRANSFORM(8)  GROUPSHARED_TWO_FOR_ONE_TRANSFORM(64)  GROUPSHARED_TWO_FOR_ONE_TRANSFORM(512)   GROUPSHARED_TWO_FOR_ONE_TRANSFORM(4096)  

// NB: FFTBLOCK(8192, false/true) won't work because the max number of threads in a group 1024 is less than the requested 8192 / 2 

#undef GROUPSHARED_TWO_FOR_ONE_TRANSFORM


#define GROUPSHARED_CONVOLUTION_WTEXTURE(_Length) \
typedef GPUFFT::TGSConvolutionWithTexturerCS<_Length >  FGSConvolutionWithTextureCSLength##_Length; \
IMPLEMENT_SHADER_TYPE2(FGSConvolutionWithTextureCSLength##_Length,  SF_Compute); 

GROUPSHARED_CONVOLUTION_WTEXTURE(2)  GROUPSHARED_CONVOLUTION_WTEXTURE(16)  GROUPSHARED_CONVOLUTION_WTEXTURE(128)   GROUPSHARED_CONVOLUTION_WTEXTURE(1024)

GROUPSHARED_CONVOLUTION_WTEXTURE(4)  GROUPSHARED_CONVOLUTION_WTEXTURE(32)  GROUPSHARED_CONVOLUTION_WTEXTURE(256)   GROUPSHARED_CONVOLUTION_WTEXTURE(2048)

GROUPSHARED_CONVOLUTION_WTEXTURE(8)  GROUPSHARED_CONVOLUTION_WTEXTURE(64)  GROUPSHARED_CONVOLUTION_WTEXTURE(512)   GROUPSHARED_CONVOLUTION_WTEXTURE(4096)
//GROUPSHARED_CONVOLUTION_WTEXTURE(2, 8192)
// NB: FFTBLOCK(2, 4196, false/true) won't work because the max number of threads in a group 1024 is less than the requested 4196 / 2 


#undef GROUPSHARED_CONVOLUTION_WTEXTURE


namespace 
{
	using namespace GPUFFT;

	FCopyWindowCS* GetCopyWindowCS(const FGPUFFTShaderContext::ShaderMapType& ShaderMap)
	{
		return  ShaderMap.GetShader<FCopyWindowCS>();
	}

	FComplexMultiplyImagesCS* GetComplexMultiplyImagesCS(const FGPUFFTShaderContext::ShaderMapType& ShaderMap)
	{
		return  ShaderMap.GetShader<FComplexMultiplyImagesCS>();
	}

	FGroupShardSubFFTPassCS* GetGroupSharedSubFFTPassCS(const FGPUFFTShaderContext::ShaderMapType& ShaderMap, const uint32 TransformLength)
	{
		return ShaderMap.GetShader<FGroupShardSubFFTPassCS>();
	}

	FReorderFFTPassCS* GetReorderFFTPassCS(const FGPUFFTShaderContext::ShaderMapType& ShaderMap)
	{
		return ShaderMap.GetShader<FReorderFFTPassCS>();
	}
	FPackTwoForOneFFTPassCS* GetPackTwoForOneFFTPassCS(const FGPUFFTShaderContext::ShaderMapType& ShaderMap, const uint32 TransformLength)
	{
		return  ShaderMap.GetShader<FPackTwoForOneFFTPassCS>();
	}

	FComplexFFTPassCS* GetComplexFFTPassCS(const FGPUFFTShaderContext::ShaderMapType& ShaderMap, const uint32 TransformLength)
	{
		FComplexFFTPassCS* Result = ShaderMap.GetShader<FComplexFFTPassCS>();
		return Result;
	}

	// Shader Permutation picker.
	FGSComplexTransformBaseCS* GetComplexFFTCS(const FGPUFFTShaderContext::ShaderMapType& ShaderMap, const uint32 TransformLength)
	{
		FGSComplexTransformBaseCS* Result = NULL;

	#define GET_COMPLEX_SHADER(_LENGTH)  ShaderMap.GetShader<TGSComplexTransformCS<_LENGTH>>();

		switch (TransformLength)
		{
		case 2:     Result = GET_COMPLEX_SHADER(2);    break;
		case 4:     Result = GET_COMPLEX_SHADER(4);    break;
		case 8:     Result = GET_COMPLEX_SHADER(8);    break;
		case 16:    Result = GET_COMPLEX_SHADER(16);   break;
		case 32:    Result = GET_COMPLEX_SHADER(32);   break;
		case 64:    Result = GET_COMPLEX_SHADER(64);   break;
		case 128:   Result = GET_COMPLEX_SHADER(128);  break;
		case 256:   Result = GET_COMPLEX_SHADER(256);  break;
		case 512:   Result = GET_COMPLEX_SHADER(512);  break;
		case 1024:  Result = GET_COMPLEX_SHADER(1024); break;
		case 2048:  Result = GET_COMPLEX_SHADER(2048); break;
		case 4096:  Result = GET_COMPLEX_SHADER(4096); break;
		//case 8192:  Result = GET_COMPLEX_SHADER(2, 8192); break;

		default:
			ensureMsgf(0, TEXT("The FFT block height is not supported"));
			break;
		}

	#undef GET_COMPLEX_SHADER

		return Result;
	}

	// Shader Permutation picker.
	FGSComplexTransformBaseCS* GetTwoForOneFFTCS(const FGPUFFTShaderContext::ShaderMapType& ShaderMap, const uint32 TransformLength)
	{
		FGSComplexTransformBaseCS* Result = NULL;

	#define GET_TWOFORONE_SHADER(_LENGTH)  ShaderMap.GetShader<TGSTwoForOneTransformCS<_LENGTH>>();

		switch (TransformLength)
		{
		case 2:     Result = GET_TWOFORONE_SHADER(2);    break;
		case 4:     Result = GET_TWOFORONE_SHADER(4);    break;
		case 8:     Result = GET_TWOFORONE_SHADER(8);    break;
		case 16:    Result = GET_TWOFORONE_SHADER(16);   break;
		case 32:    Result = GET_TWOFORONE_SHADER(32);   break;
		case 64:    Result = GET_TWOFORONE_SHADER(64);   break;
		case 128:   Result = GET_TWOFORONE_SHADER(128);  break;
		case 256:   Result = GET_TWOFORONE_SHADER(256);  break;
		case 512:   Result = GET_TWOFORONE_SHADER(512);  break;
		case 1024:  Result = GET_TWOFORONE_SHADER(1024); break;
		case 2048:  Result = GET_TWOFORONE_SHADER(2048); break;
		case 4096:  Result = GET_TWOFORONE_SHADER(4096); break;
		//case 8192:  Result = GET_TWOFORONE_SHADER(2, 8192); break;

		default:
			ensureMsgf(0, TEXT("The FFT block height is not supported"));
			break;
		}

	#undef GET_TWOFORONE_SHADER

		return Result;
	}


	FGSConvolutionWithTextureKernelBaseCS* GetConvolutionWithTextureKernelCS(const FGPUFFTShaderContext::ShaderMapType& ShaderMap, const uint32 TransformLength)
	{
		FGSConvolutionWithTextureKernelBaseCS* Result = NULL;

		// Get the spectral filter.
	#define GET_GROUP_SHARED_TEXTURE_FILTER(_LENGTH)  ShaderMap.GetShader< TGSConvolutionWithTexturerCS<_LENGTH> >();
		switch (TransformLength)
		{
		case 2:    Result = GET_GROUP_SHARED_TEXTURE_FILTER(2);    break;
		case 4:    Result = GET_GROUP_SHARED_TEXTURE_FILTER(4);    break;
		case 8:    Result = GET_GROUP_SHARED_TEXTURE_FILTER(8);    break;
		case 16:   Result = GET_GROUP_SHARED_TEXTURE_FILTER(16);   break;
		case 32:   Result = GET_GROUP_SHARED_TEXTURE_FILTER(32);   break;
		case 64:   Result = GET_GROUP_SHARED_TEXTURE_FILTER(64);   break;
		case 128:  Result = GET_GROUP_SHARED_TEXTURE_FILTER(128);  break;
		case 256:  Result = GET_GROUP_SHARED_TEXTURE_FILTER(256);  break;
		case 512:  Result = GET_GROUP_SHARED_TEXTURE_FILTER(512);  break;
		case 1024: Result = GET_GROUP_SHARED_TEXTURE_FILTER(1024); break;
		case 2048: Result = GET_GROUP_SHARED_TEXTURE_FILTER(2048); break;
		case 4096: Result = GET_GROUP_SHARED_TEXTURE_FILTER(4096); break;
		//case 8192: Result = GET_GROUP_SHARED_TEXTURE_FILTER(2, 8192); break;
		default:
			ensureMsgf(0, TEXT("The FFT block height is not supported"));
			break;
		}
	#undef GET_GROUP_SHARED_TEXTURE_FILTER

		return Result;

	}
	/**
	* Single Pass that Copies a sub-region of a buffer and potentially 
	* boosts the intensity of select pixels.
	* 
	* Assumes the dst buffer is large enough to hold the result.
	* The Src float4 data is interpreted as a pair of independent complex numbers.
	* The Knl float4 data is interpreted as a pair of independent complex numbers
	*
	* @param Context    - container for RHI and ShanderMap
	* @param SrcWindow  - The region of interest to copy.
	* @param SrcTexture - SRV the source image buffer.
	* @param DstWindow  - The target location for the images.
	*
	* @param DstUAV     - UAV, the destination buffer that will hold the result of the single pass
	* @param Prefilter  - Optional filter to boost selected pixels.
	*/
	void DispatchCopyWindowCS(FGPUFFTShaderContext& Context,
		const FIntRect& SrcWindow, const FTextureRHIRef& SrcTexture,
		const FIntRect& DstWindow, FUnorderedAccessViewRHIRef& DstUAV,
		const FPreFilter& PreFilter = FPreFilter(TNumericLimits<float>::Max(), TNumericLimits<float>::Lowest(), 0.f))
	{
		using namespace GPUFFTComputeShaderUtils;
		// The size of the dst
		const FIntPoint DstExtent = DstWindow.Size();

		const uint32 XThreadCount = FCopyWindowCS::XThreadCount();
		const uint32 YThreadCount = FCopyWindowCS::YThreadCount();

		// Number of thread groups
		const uint32 XGroups = ( DstExtent.X / XThreadCount ) + ( (DstExtent.X % XThreadCount == 0) ? 0 : 1 );
		const uint32 YGroups = ( DstExtent.Y / YThreadCount ) + ( (DstExtent.Y % YThreadCount == 0) ? 0 : 1 );

		const FGPUFFTShaderContext::ShaderMapType& ShaderMap = Context.GetShaderMap();
		FRHICommandListImmediate& RHICmdList = Context.GetRHICmdList();

		SCOPED_DRAW_EVENTF(RHICmdList, CopyWindowCS, TEXT("FFT Multipass: Copy Subwindow"));

		// Get pointer to the shader
		FCopyWindowCS* ComputeShader = GetCopyWindowCS(ShaderMap);

		SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

		// Bind output
		FScopedUAVBind ScopedBind = FScopedUAVBind::BindOutput(RHICmdList, *ComputeShader, DstUAV);

		ComputeShader->SetCSParamters(RHICmdList, SrcWindow, SrcTexture,  DstWindow, PreFilter);

		// Dispatch with a single thread-group for each "column" in the result where the transform direction is the "row" direction.
		RHICmdList.DispatchComputeShader(XGroups, YGroups, 1);
	}

	/**
	* 
	* Single Pass that computes the Frequency Space convolution of two buffers
	* that have already been transformed into frequency space.
	*
	* This is really means the complex product of two buffers divided
	* by the correct values to "normalize" the effect of the kernel buffer.
	*
	* In this case, each float4 is viewed as a pair of complex numbers,
	* and the product of float4 Src, Knl is computed
	* as float4(ComplexMult(Src.xy, Knl.xy) / Na, ComplexMult(Src.zw, Knl.zw)/Nb)
	* where Na and Nb are related to the sums of the weights in the kernel buffer. 
	*
	* Assumes the dst buffer is large enough to hold the result.
	* The Src float4 data is interpreted as a pair of independent complex numbers.
	* The Knl float4 data is interpreted as a pair of independent complex numbers  
	*
	* @param Context    - container for RHI and ShanderMap
	* @param bHorizontalFirst - Describes the layout of the transformed data.
	*                           bHorizontalFirst == true for data that was transformed as 
	*                           Vertical::ComplexFFT following Horizontal::TwoForOneRealFFT
	*                           bHorizontalFirst == false for data that was transformed as
	*                           Horizontal::ComplexFFT following Vertical::TwoForOneRealFFT
	* @param SrcTexture - SRV the source image buffer.
	* @param KnlTexture - SRV the kernel image buffer.
	*
	* @param DstUAV     - UAV, the destination buffer that will hold the result of the single pass
	*
	*/

	void DispatchComplexMultiplyImagesCS(FGPUFFTShaderContext& Context,
		const bool bHorizontalScanlines,
		const FIntRect& SrcWindow, const FTextureRHIRef& SrcTexture,
		const FTextureRHIRef& KnlTexure,
		FUnorderedAccessViewRHIRef& DstUAV)
	{
		using namespace GPUFFTComputeShaderUtils;
		// The size of the dst
		const FIntPoint DstExtent = SrcWindow.Size();

		const FGPUFFTShaderContext::ShaderMapType& ShaderMap = Context.GetShaderMap();
		FRHICommandListImmediate& RHICmdList = Context.GetRHICmdList();

		SCOPED_DRAW_EVENTF(RHICmdList, ComplexMultiplyImagesCS, TEXT("FFT Multipass: Convolution in freq-space"));

		// Get pointer to the shader
		FComplexMultiplyImagesCS* ComputeShader = GetComplexMultiplyImagesCS(ShaderMap);

		SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

		// Bind output
		FScopedUAVBind ScopedBind = FScopedUAVBind::BindOutput(RHICmdList, *ComputeShader, DstUAV);

		ComputeShader->SetCSParamters(RHICmdList, bHorizontalScanlines, SrcWindow, SrcTexture, KnlTexure);
		
		// Align the scanlines in the direction of the first transform.
		const uint32 NumScanLines = (bHorizontalScanlines)  ? DstExtent.Y : DstExtent.X;
		const uint32 SignalLength = (!bHorizontalScanlines) ? DstExtent.Y : DstExtent.X;
	
		// Dispatch with a single thread-group for each "column."
		// Configured this way because the entire column will share the same normalization values (retrieved  from the kernel)
		//RHICmdList.DispatchComputeShader(1, ThreadGroupsPerScanline, NumScanLines);
		RHICmdList.DispatchComputeShader(1, 1, NumScanLines);
	}


	/**
	* Single Pass that separates or merges the transform of four real signals from 
	* viewed as the transform of two complex  signals.  
	*
	* Assumes the dst buffer is large enough to hold the result.
	* The Src float4 data is interprets  as a pair of independent complex numbers.
	*
	* @param Context    - container for RHI and ShanderMap
	* @param FFTDesc    - Metadata that describes the underlying complex FFT
	* @param SrcTexture - SRV the source image buffer
	* @param DstUAV     - UAV, the destination buffer that will hold the result of the single pass
	*
	* The data in the SrcTexture and DstUAV is aligned with (0,0).
	* FFTDesc.IsHorizontal() indicates the transform direction in the buffer that needs to be spit/merged
	* FFTDesc.IsForward() indicates spit (true) vs merge (false).
	*/
	void DispatchPackTwoForOneFFTPassCS(FGPUFFTShaderContext& Context, const FFTDescription& FFTDesc,
		const FTextureRHIRef& SrcTexture, FUnorderedAccessViewRHIRef& DstUAV)
	{

		using namespace GPUFFTComputeShaderUtils;

		const FGPUFFTShaderContext::ShaderMapType& ShaderMap = Context.GetShaderMap();
		FRHICommandListImmediate& RHICmdList = Context.GetRHICmdList();
	
		const uint32 TransformLength = FFTDesc.SignalLength;
		const FString TransformName = FFTDesc.FFT_TypeName();

		// A real signal of length 'TransformLenght' requires  only TransformLength / 2 + 1  complex coefficients, 
		const uint32 RealTransformLength = ( TransformLength / 2 ) + 1;

		// The splitting into two real signals (isForward) or joinging back into a single signal
		const uint32 ResultingLength = (FFTDesc.IsForward()) ? 2 * RealTransformLength : TransformLength;

		SCOPED_DRAW_EVENTF(RHICmdList, PackTwoForOneFFTPass, TEXT("FFT Multipass: TwoForOne Combine/split result of %s of size %d"), *TransformName, TransformLength);

		FIntPoint DstExtent = FFTDesc.TransformExtent();
		if (FFTDesc.IsHorizontal())
		{
			DstExtent.X = ResultingLength;
		}	
		else
		{
			DstExtent.Y = ResultingLength;
		}

		// Get pointer to the shader
		FPackTwoForOneFFTPassCS* ComputeShader = GetPackTwoForOneFFTPassCS(ShaderMap, TransformLength);

		SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

		// Bind output
		FScopedUAVBind ScopedBind = FScopedUAVBind::BindOutput(RHICmdList, *ComputeShader, DstUAV);

		ComputeShader->SetCSParamters(RHICmdList, FFTDesc.XFormType, SrcTexture, FIntRect(FIntPoint(0, 0), DstExtent));

		// Dispatch with a single thread-group for each "column" in the result where the transform direction is the "row" direction.
		RHICmdList.DispatchComputeShader(1, 1, RealTransformLength);
	}

	/**
	* Single Pass of a multi-pass complex FFT.
	* Assumes the dst buffer is large enough to hold the result.
	* The Src float4 data is interpt as a pair of independent complex numbers.
	*
	* @param Context    - container for RHI and ShanderMap
	* @param FFTDesc    - Metadata that describes the underlying complex FFT
	* @param PassStage  - The Depth at which this FFT pass lives. 
	* @param SrcTexture - SRV the source image buffer
	* @param SrcRct     - The region in the Src buffer where the image to transform lives.
	* @param DstUAV     - UAV, the destination buffer that will hold the result of the single pass
	*
	*/
	void DispatchComplexFFTPassCS(FGPUFFTShaderContext& Context, const FFTDescription& FFTDesc,
		const uint32 PassLength,
		const FTextureRHIRef& SrcTexture, const FIntRect& SrcWindow,
		FUnorderedAccessViewRHIRef& DstUAV,
		const bool bScrubNaNs = false)
	{
		// Using multiple radix two passes.
		const uint32 Radix = 2;
		using namespace GPUFFTComputeShaderUtils;

		const FGPUFFTShaderContext::ShaderMapType& ShaderMap = Context.GetShaderMap();
		FRHICommandListImmediate& RHICmdList = Context.GetRHICmdList();

		const uint32 TransformLength = FFTDesc.SignalLength;
		const FString TransformName = FFTDesc.FFT_TypeName();


		SCOPED_DRAW_EVENTF(RHICmdList, ComplexFFTPass, TEXT("FFT Multipass: Pass %d of Complex %s of size %d"), PassLength, *TransformName, TransformLength);
		
		FIntPoint DstExtent = FFTDesc.TransformExtent();
		
		// Get a base pointer to the shader
		FComplexFFTPassCS* ComputeShader = GetComplexFFTPassCS(ShaderMap, TransformLength);


		SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

		// Bind output
		FScopedUAVBind ScopedBind = FScopedUAVBind::BindOutput(RHICmdList, *ComputeShader, DstUAV);

		ComputeShader->SetCSParamters(RHICmdList, FFTDesc.XFormType, SrcTexture, SrcWindow, FIntRect(FIntPoint(0, 0), DstExtent), TransformLength, PassLength, bScrubNaNs);

		RHICmdList.DispatchComputeShader(1, 1, TransformLength / Radix);
	}

	/**
	* Single Pass of a multi-pass complex FFT that just reorders data for
	* a group shared subpass to consume.
	*
	* Assumes the dst buffer is large enough to hold the result.
	* The Src float4 data is interpt as a pair of independent complex numbers.
	*
	* @param Context    - container for RHI and ShanderMap
	* @param FFTDesc    - Metadata that describes the underlying complex FFT
	* @param SrcRct     - The region in the Src buffer where the image to transform lives.
	* @param SrcTexture - SRV the source image buffer
	* @param DstRect    - The target Window.
	* @param DstUAV     - UAV, the destination buffer that will hold the result of the single pass
	*
	*/
	void DispatchReorderFFTPassCS(FGPUFFTShaderContext& Context, const FFTDescription& FFTDesc,
		const FIntRect& SrcWindow, const FTextureRHIRef& SrcTexture,
		const FIntRect& DstWindow, FUnorderedAccessViewRHIRef& DstUAV,
		const bool bScrubNaNs = false)
	{
		// Using multiple radix two passes.
		const uint32 Radix = 2;
		using namespace GPUFFTComputeShaderUtils;

		const FGPUFFTShaderContext::ShaderMapType& ShaderMap = Context.GetShaderMap();
		FRHICommandListImmediate& RHICmdList = Context.GetRHICmdList();

		const uint32 TransformLength = FFTDesc.SignalLength;
		const FString TransformName = FFTDesc.FFT_TypeName();

		// Breaks the data into the correct number of sub-transforms for the later group-shared pass.
		const uint32 SubLength = TransformLength / FGroupShardSubFFTPassCS::SubPassLength();

		SCOPED_DRAW_EVENTF(RHICmdList, ReorderFFTPass, TEXT("FFT Multipass: Complex %s Reorder pass of size %d"), *TransformName, TransformLength);

		FIntPoint DstExtent = FFTDesc.TransformExtent();

		// Get a base pointer to the shader
		FReorderFFTPassCS* ComputeShader = GetReorderFFTPassCS(ShaderMap);


		SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

		// Bind output
		FScopedUAVBind ScopedBind = FScopedUAVBind::BindOutput(RHICmdList, *ComputeShader, DstUAV);

		ComputeShader->SetCSParamters(RHICmdList, FFTDesc.XFormType, SrcTexture, SrcWindow, DstWindow, TransformLength, SubLength, bScrubNaNs);
			

		RHICmdList.DispatchComputeShader(1, 1, TransformLength / Radix);
	}

	/**
	* A Group Shared Single Pass of a multi-pass complex FFT.
	* Assumes the dst buffer is large enough to hold the result.
	* The Src float4 data is interpt as a pair of independent complex numbers.
	*
	* @param Context    - container for RHI and ShanderMap
	* @param FFTDesc    - Metadata that describes the underlying complex FFT
	* @param SrcTexture - SRV the source image buffer
	* @param SrcRct     - The region in the Src buffer where the image to transform lives.
	* @param DstUAV     - UAV, the destination buffer that will hold the result of the single pass
	*
	*/
	void DispatchGSSubComplexFFTPassCS(FGPUFFTShaderContext& Context, const FFTDescription& FFTDesc,
		const FTextureRHIRef& SrcTexture, const FIntRect& SrcWindow,
		FUnorderedAccessViewRHIRef& DstUAV)
	{
	
		using namespace GPUFFTComputeShaderUtils;

		const FGPUFFTShaderContext::ShaderMapType& ShaderMap = Context.GetShaderMap();
		FRHICommandListImmediate& RHICmdList = Context.GetRHICmdList();

		const uint32 TransformLength = FFTDesc.SignalLength;
		const FString TransformName = FFTDesc.FFT_TypeName();

		const uint32 NumSubRegions = TransformLength / FGroupShardSubFFTPassCS::SubPassLength();

		

		SCOPED_DRAW_EVENTF(RHICmdList, GSSubComplexFFTPass, TEXT("FFT Multipass: %d GS Subpasses Complex %s of size %d"), 
			NumSubRegions, *TransformName, FGroupShardSubFFTPassCS::SubPassLength());

		// The window on which a single transform acts.
		FIntRect SubPassWindow = SrcWindow;
		if (FFTDesc.IsHorizontal())
		{
			SubPassWindow.Max.X = SubPassWindow.Min.X + FGroupShardSubFFTPassCS::SubPassLength();
		}
		else
		{
			SubPassWindow.Max.Y = SubPassWindow.Min.Y + FGroupShardSubFFTPassCS::SubPassLength();
		}


		FIntPoint DstExtent = FFTDesc.TransformExtent();

		// Get a base pointer to the shader
		FGroupShardSubFFTPassCS* ComputeShader = GetGroupSharedSubFFTPassCS(ShaderMap, TransformLength);


		SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

		// Bind output
		FScopedUAVBind ScopedBind = FScopedUAVBind::BindOutput(RHICmdList, *ComputeShader, DstUAV);

		ComputeShader->SetCSParamters(RHICmdList, FFTDesc.XFormType, TransformLength, SubPassWindow, SrcTexture, NumSubRegions);
		// The number of signals to transform simultaneously (i.e. number of scan lines)

		const uint32 NumSignals = FFTDesc.IsHorizontal() ? SubPassWindow.Size().Y : SubPassWindow.Size().X;

		RHICmdList.DispatchComputeShader(1, 1, NumSignals);
	}


	/**
	* Complex 1D FFT of two independent complex signals.  
	* Assumes the dst buffer is large enough to hold the result.
	* The Src float4 data is interpt as a pair of independent complex numbers. 
	*
	* @param Context    - container for RHI and ShanderMap
	* @param FFTDesc    - Metadata that describes the underlying complex FFT
	* @param SrcTexture - SRV the source image buffer
	* @param SrcRct     - The region in the Src buffer where the image to transform lives.
	* @param DstUAV     - UAV, the destination buffer that will hold the result of the 1d complex FFT
	*
	*/
	void DispatchGSComplexFFTCS(FGPUFFTShaderContext& Context, const FFTDescription& FFTDesc,
		const FTextureRHIRef& SrcTexture, const FIntRect& SrcRect, 
		FUnorderedAccessViewRHIRef& DstUAV)
	{
		using namespace GPUFFTComputeShaderUtils;

		const FGPUFFTShaderContext::ShaderMapType& ShaderMap = Context.GetShaderMap();
		FRHICommandListImmediate& RHICmdList                 = Context.GetRHICmdList();

		const uint32 TransformLength = FFTDesc.SignalLength;
		const FString TransformName  = FFTDesc.FFT_TypeName();
		const FIntPoint DstExtent    = FFTDesc.TransformExtent();

		SCOPED_DRAW_EVENTF(RHICmdList, ComplexFFTImage, TEXT("FFT: Complex %s of size %d"), *TransformName, TransformLength);


		// Get a base pointer to the shader
		FGSComplexTransformBaseCS* ComputeShader = GetComplexFFTCS(ShaderMap, TransformLength);


		SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

		// Bind output
		FScopedUAVBind ScopedBind = FScopedUAVBind::BindOutput(RHICmdList, *ComputeShader, DstUAV);

		ComputeShader->SetCSParamters(RHICmdList, FFTDesc.XFormType, SrcTexture, SrcRect, FIntRect(FIntPoint(0, 0), DstExtent));

		// The number of signals to transform simultaneously (i.e. number of scan lines)

		const uint32 NumSignals = FFTDesc.IsHorizontal() ? SrcRect.Size().Y : SrcRect.Size().X;

		RHICmdList.DispatchComputeShader(1, 1, NumSignals);
	}

	/**
	* Real 1D FFT of four independent real signals.   
	* Assumes the dst buffer is large enough to hold the result.
	* The Src float4 data is interptd as 4 independent real numbers.
	* The Dst float4 data will be two complex numbers.
	*
	* @param Context    - container for RHI and ShanderMap
	* @param FFTDesc    - Metadata that describes the underlying complex FFT
	* @param SrcTexture - SRV the source image buffer
	* @param SrcRct     - The region in the Src buffer where the image to transform lives.
	* @param DstUAV     - UAV, the destination buffer that will hold the result of the 1d complex FFT
	* @param DstRect    - Where to write the tranform data in the Dst buffer
	* @param PreFilter  - Used to boost the intensity of already bright pixels.
	*
	*/
	void DispatchGSTwoForOneFFTCS(FGPUFFTShaderContext& Context, const FFTDescription& FFTDesc,
		const FTextureRHIRef& SrcTexture, const FIntRect& SrcRect,
		FUnorderedAccessViewRHIRef& DstUAV, const FIntRect& DstRect,
		const FPreFilter& PreFilter)
	{
		using namespace GPUFFTComputeShaderUtils;

		const FGPUFFTShaderContext::ShaderMapType& ShaderMap = Context.GetShaderMap();
		FRHICommandListImmediate& RHICmdList                 = Context.GetRHICmdList();

		const uint32 TransformLength = FFTDesc.SignalLength;
		const FString TransformName  = FFTDesc.FFT_TypeName();
	


		SCOPED_DRAW_EVENTF(RHICmdList, FRCPassFFT, TEXT("FFT: Two-For-One %s of size %d of buffer %d x %d"), *TransformName, TransformLength, SrcRect.Size().X, SrcRect.Size().Y);

		// Get a base pointer to the shader
		FGSComplexTransformBaseCS* ComputeShader = GetTwoForOneFFTCS(ShaderMap, TransformLength);

		// DJH - do we need this SetRenderTarget?
		SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());


		FScopedUAVBind ScopedBind = FScopedUAVBind::BindOutput(RHICmdList, *ComputeShader, DstUAV);
		ComputeShader->SetCSParamters(RHICmdList, FFTDesc.XFormType, SrcTexture, SrcRect, DstRect, PreFilter);


		// The number of signals to transform simultaneously (i.e. number of scan lines)
		const uint32 NumScanLines = (FFTDesc.IsHorizontal()) ? SrcRect.Size().Y : SrcRect.Size().X;

		RHICmdList.DispatchComputeShader(1, 1, NumScanLines);

	}

	/**
	* Complex 1D FFT followed by multiplication in with kernel and inverse transform.
	*
	* @param Context    - container for RHI and ShanderMap
	* @param FFTDesc    - Metadata that describes the underlying complex FFT
	* @param PreTransformedKernel - Pre-transformed kernel used in the convolution.
	* @param SrcTexture - SRV the source image buffer
	* @param SrcRct     - The region in the Src buffer where the image to transform lives.
	* @param DstUAV     - UAV, the destination buffer that will hold the result of the 1d complex FFT
	* @param DstRect    - Where to write the tranform data in the Dst buffer
	*
	*/
	void DispatchGSConvolutionWithTextureCS(FGPUFFTShaderContext& Context,
		const FFTDescription& FFTDesc,
		const FTextureRHIRef& PreTransformedKernel,
		const FTextureRHIRef& SrcTexture, 
		const FIntRect& SrcRect,
		FUnorderedAccessViewRHIRef& DstUAV)
	{
		using GPUFFTComputeShaderUtils::FScopedUAVBind;

		const FGPUFFTShaderContext::ShaderMapType& ShaderMap = Context.GetShaderMap();
		FRHICommandListImmediate& RHICmdList                 = Context.GetRHICmdList();

		// Transform particulars

		const uint32 SignalLength   = FFTDesc.SignalLength;
		const FString XFormDirName  = FFTDesc.FFT_TypeName();
		const bool bIsHornizontal   = FFTDesc.IsHorizontal();

		const FIntPoint& SrcRectSize = SrcRect.Size();
		// The number of signals to transform simultaneously (i.e. number of scan lines)
		// NB: This may be different from the FFTDesc.NumScanlines.
		const uint32 NumSignals = (bIsHornizontal) ? SrcRectSize.Y : SrcRectSize.X;

		
		SCOPED_DRAW_EVENTF(RHICmdList, FRCPassFFTBloom, TEXT("FFT: Apply %s Transform, Multiply Texture, and InverseTransform size %d of buffer %d x %d"), *XFormDirName, SignalLength, SrcRectSize.X, SrcRectSize.Y);

		// Get a base pointer to the shader
		FGSConvolutionWithTextureKernelBaseCS* ComputeShader = GetConvolutionWithTextureKernelCS(ShaderMap, SignalLength);


		SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
		
		FScopedUAVBind ScopedBind = FScopedUAVBind::BindOutput(RHICmdList, *ComputeShader, DstUAV);

		ComputeShader->SetCSParamters(RHICmdList, FFTDesc.XFormType, PreTransformedKernel, SrcTexture, SrcRect, SrcRect.Size());

		RHICmdList.DispatchComputeShader(1, 1, NumSignals);
	}
} // end anonymous namespace 


void GPUFFT::CopyImage2D(FGPUFFTShaderContext& Context,
	const FIntRect& SrcWindow, const FTextureRHIRef& SrcTexture,
	const FIntRect& DstWindow, FUnorderedAccessViewRHIRef& DstUAV,
	const FPreFilter& PreFilter)
{
	DispatchCopyWindowCS(Context, SrcWindow, SrcTexture, DstWindow, DstUAV, PreFilter);
}

void GPUFFT::ComplexFFTImage1D::Requirements(const FFTDescription& FFTDesc, FIntPoint& MinBufferSize, bool& bUseMultipass)
{
	MinBufferSize = FFTDesc.TransformExtent();
	bUseMultipass = !FitsInGroupSharedMemory(FFTDesc);
}


bool GPUFFT::ComplexFFTImage1D::GroupShared(FGPUFFTShaderContext& Context, const FFTDescription& FFTDesc,
	const FIntRect& SrcWindow, const FTextureRHIRef& SrcTexture,  FUnorderedAccessViewRHIRef& DstUAV)
{
	bool SuccessValue = true;

	check(FMath::IsPowerOfTwo(FFTDesc.SignalLength));

	if (FitsInGroupSharedMemory(FFTDesc))
	{
		DispatchGSComplexFFTCS(Context, FFTDesc, SrcTexture, SrcWindow, DstUAV);
	}
	else
	{
		SuccessValue = false;
		// @todo
		ensureMsgf(0, TEXT("The FFT size is too large for group shared memory"));
		// Do forward expensive transform
	}

	return SuccessValue;
}


bool GPUFFT::ComplexFFTImage1D::MultiPass(FGPUFFTShaderContext& Context, const FFTDescription& FFTDesc,
	const FIntRect& Window, const FTextureRHIRef& SrcTexture,  FSceneRenderTargetItem& DstBuffer,
	FSceneRenderTargetItem& TmpBuffer,
	const bool bScrubNaNs)
{
	bool SuccessValue = true;


	const uint32 TransformLength = FFTDesc.SignalLength;
	const FFT_XFORM_TYPE XFormType = FFTDesc.XFormType;

	// The direction of the transform must be a power of two.

	check(FMath::IsPowerOfTwo(TransformLength));

	// Command list
	FRHICommandListImmediate& RHICmdList = Context.GetRHICmdList();

	// The number of iterations required.
	const uint32 Log2TransformLength = BitSize(TransformLength) - 1;

	// Double buffer to manage the Dst and Tmp buffers.
	GPUFFT::FDoubleBufferTargets Targets(TmpBuffer, DstBuffer);

	

	FIntPoint DstExtent = FFTDesc.TransformExtent();

	const FIntRect  XFormWindow(FIntPoint(0, 0), DstExtent);

	// Testing code branch: Breaks the transform into Log2(TransformLength) number of passes.
	// this is the slowest algorithm, and uses no group-shared storage.
	if (0) 
	{
		DispatchComplexFFTPassCS(Context, FFTDesc, 1, SrcTexture, Window, Targets.DstTarget().UAV, bScrubNaNs);

		for (uint32 Ns = 2; Ns < TransformLength; Ns *= 2)
		{
			// Make it safe to read from the buffer we just wrote to.
			RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, Targets.DstTarget().UAV);
			Targets.Swap();

			auto HasValidTargets = [&Targets, &DstExtent]()->bool
			{
				// Verify that the buffers being used are big enough.  Note that we are checking the "src" buffer, but due
				// to the double buffering we will end up testing both buffers.
				FIntPoint SrcBufferSize = Targets.SrcTarget().ShaderResourceTexture->GetTexture2D()->GetSizeXY();
				bool Fits = !(SrcBufferSize.X < DstExtent.X) && !(SrcBufferSize.Y < DstExtent.Y);
				return Fits;
			};

			checkf(HasValidTargets(), TEXT("FFT: Allocated Buffers too small."));

			DispatchComplexFFTPassCS(Context, FFTDesc, Ns, Targets.SrcTarget().ShaderResourceTexture, XFormWindow, Targets.DstTarget().UAV);
		}

		// If this transform requires an even number of passes
		// this swap will insure that the "DstBuffer" is filled last.
		if (Log2TransformLength % 2 == 0)
		{
			SwapContents(TmpBuffer, DstBuffer);
		}
	}
	else
		// Reorder, followed by a High-level group-shared pass followed by Log2(TransformLength / SubPassLength() ) simple passes.
		// In total 2 + Log2(TransformLength / SubPassLength() )  passes.   This will be on the order of 3 or 4 passes 
		// compared with 12 or more ..
	{
		// Re-order the data so we can do a pass of group-shared transforms
		DispatchReorderFFTPassCS(Context, FFTDesc, Window, SrcTexture, XFormWindow, Targets.DstTarget().UAV);
		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, Targets.DstTarget().UAV);
		Targets.Swap();

		DispatchGSSubComplexFFTPassCS(Context, FFTDesc, Targets.SrcTarget().ShaderResourceTexture, XFormWindow, Targets.DstTarget().UAV);

		for (uint32 Ns = FGroupShardSubFFTPassCS::SubPassLength(); Ns < TransformLength; Ns *= 2)
		{
			// Make it safe to read from the buffer we just wrote to.
			RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, Targets.DstTarget().UAV);
			Targets.Swap();

			auto HasValidTargets = [&Targets, &DstExtent]()->bool
			{
				// Verify that the buffers being used are big enough.  Note that we are checking the "src" buffer, but due
				// to the double buffering we will end up testing both buffers.
				FIntPoint SrcBufferSize = Targets.SrcTarget().ShaderResourceTexture->GetTexture2D()->GetSizeXY();
				bool Fits = !(SrcBufferSize.X < DstExtent.X) && !(SrcBufferSize.Y < DstExtent.Y);
				return Fits;
			};

			checkf(HasValidTargets(), TEXT("FFT: Allocated Buffers too small."));

			DispatchComplexFFTPassCS(Context, FFTDesc, Ns, Targets.SrcTarget().ShaderResourceTexture, XFormWindow, Targets.DstTarget().UAV);

		}

		if (Targets.GetSrcIdx() != 0)
		{
			SwapContents(TmpBuffer, DstBuffer);
		}
	}

	return SuccessValue;
}


void GPUFFT::TwoForOneRealFFTImage1D::Requirements(const FFTDescription& FFTDesc, FIntPoint& MinBufferSize, bool& bUseMultipass)
{
	FFTDescription TmpDesc = FFTDesc;
	// The two-for-one produces a result that has two additional elements in the transform direction.
	TmpDesc.SignalLength += 2;

	MinBufferSize = TmpDesc.TransformExtent();
	bUseMultipass = !FitsInGroupSharedMemory(FFTDesc);
}

bool GPUFFT::TwoForOneRealFFTImage1D::GroupShared(FGPUFFTShaderContext& Context, const FFTDescription& FFTDesc,
	const FIntRect& SrcWindow, const FTextureRHIRef& SrcTexture,
	const FIntRect& DstWindow, FUnorderedAccessViewRHIRef& DstUAV,
	const FPreFilter& PreFilter)
{
	bool SuccessValue = true;
				
	if (FitsInGroupSharedMemory(FFTDesc))
	{
		DispatchGSTwoForOneFFTCS(Context, FFTDesc, SrcTexture, SrcWindow, DstUAV, DstWindow, PreFilter);
	}
	else
	{
		SuccessValue = false;
		// @todo
		ensureMsgf(0, TEXT("The FFT size is too large for group shared memory"));
		// Do forward expensive transform
	}

	return SuccessValue;
}


bool GPUFFT::TwoForOneRealFFTImage1D::MultiPass(FGPUFFTShaderContext& Context, const FFTDescription& FFTDesc,
	const FIntRect& SrcWindow, const FTextureRHIRef& SrcTexture,
	FSceneRenderTargetItem& DstBuffer,
	FSceneRenderTargetItem& TmpBuffer,
	const FPreFilter& PreFilter)
{
	
	bool SuccessValue = true;
	if (FFTDesc.IsForward())
	{

		// Only filter on the forward transform.
		if (IsActive(PreFilter))
		{
			// Copy data into DstBuffer
			CopyImage2D(Context, SrcWindow, SrcTexture, SrcWindow, DstBuffer.UAV, PreFilter);

			Context.GetRHICmdList().TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, DstBuffer.UAV);

			// Transform as 2 sets of complex data, putting the result in the DstBuffer.  This performs multiple dispatches.
			// Transform into DstBuffer.
			SuccessValue = SuccessValue &&
				ComplexFFTImage1D::MultiPass(Context, FFTDesc, SrcWindow, DstBuffer.ShaderResourceTexture, TmpBuffer /*result*/, DstBuffer/*tmp*/, true /*scrub nans*/);
		}
		else
		{
			// Transform as 2 sets of complex data, putting the result in the DstBuffer.  This performs multiple dispatches.

			SuccessValue = SuccessValue &&
				ComplexFFTImage1D::MultiPass(Context, FFTDesc, SrcWindow, SrcTexture, DstBuffer /*result*/, TmpBuffer/*tmp*/, true /*scrub nans*/);
			
			SwapContents(DstBuffer, TmpBuffer);
		}

		Context.GetRHICmdList().TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, TmpBuffer.UAV);

		// Unpack the complex transform into transform of real data
		DispatchPackTwoForOneFFTPassCS(Context, FFTDesc, TmpBuffer.ShaderResourceTexture, DstBuffer.UAV);
		
	}
	else  // Inverse transform.
	{
		// Pack the 4 transforms of real data as 2 transforms of complex data 
		DispatchPackTwoForOneFFTPassCS(Context, FFTDesc, SrcTexture, DstBuffer.UAV);

		Context.GetRHICmdList().TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, DstBuffer.UAV);

		// Transform as complex data
		SuccessValue = SuccessValue &&
			ComplexFFTImage1D::MultiPass(Context, FFTDesc, SrcWindow, DstBuffer.ShaderResourceTexture, TmpBuffer/*result*/, DstBuffer/*tmp*/);

		SwapContents(TmpBuffer, DstBuffer);
	}

	return SuccessValue;
}



bool GPUFFT::FFTImage2D(FGPUFFTShaderContext& Context, const FIntPoint& FrequencySize, bool bHorizontalFirst,
	const FIntRect& ROIRect, const FTextureRHIRef& SrcTexture,
	FSceneRenderTargetItem& DstBuffer,
	FSceneRenderTargetItem& TmpBuffer)
{
	using GPUFFT::FFTDescription;

	FRHICommandListImmediate& RHICmdList = Context.GetRHICmdList();

	// This will be for the actual output.
	const FIntRect FFTResultRect = ROIRect;
	const FIntPoint ImageSpaceExtent = ROIRect.Size();

	// Set up the transform descriptions
	FFTDescription TwoForOneFFTDesc = (bHorizontalFirst) ? FFTDescription(FFT_XFORM_TYPE::FORWARD_HORIZONTAL, FrequencySize) : FFTDescription(FFT_XFORM_TYPE::FORWARD_VERTICAL, FrequencySize);
	FFTDescription ComplexFFTDesc = (bHorizontalFirst) ? FFTDescription(FFT_XFORM_TYPE::FORWARD_VERTICAL, FrequencySize) : FFTDescription(FFT_XFORM_TYPE::FORWARD_HORIZONTAL, FrequencySize);


	// The two-for-one transform data storage has two additional elements.
	const uint32 FrequencyPadding = 2;
	// This additional elements translate to two additional scanlines that need to be transformed by the complex fft
	ComplexFFTDesc.NumScanLines += FrequencyPadding;

	// Temp Double Buffers
	const FIntPoint TmpExent = (bHorizontalFirst) ? FIntPoint(FrequencySize.X + FrequencyPadding, ImageSpaceExtent.Y) : FIntPoint(ImageSpaceExtent.X, FrequencySize.Y + FrequencyPadding);

	const FIntRect TmpRect = FIntRect(FIntPoint(0, 0), TmpExent);


	// Perform the transforms and convolutions.
	bool SuccessValue = true;

	// Two-for-one transform: SrcTexture fills the TmpBuffer

	if (FitsInGroupSharedMemory(TwoForOneFFTDesc))
	{
		// Horizontal FFT: input -> tmp buffer 0 
		SuccessValue = SuccessValue &&
			TwoForOneRealFFTImage1D::GroupShared(Context, TwoForOneFFTDesc, ROIRect, SrcTexture, TmpRect, TmpBuffer.UAV);
		//->dst
	}
	else
	{
		// use the dst buffer as a dummy buffer
		FSceneRenderTargetItem& DmyBuffer = DstBuffer;
		// Horizontal FFT: input -> TmpBuffer
		SuccessValue = SuccessValue &&
			TwoForOneRealFFTImage1D::MultiPass(Context, TwoForOneFFTDesc, ROIRect, SrcTexture, TmpBuffer /*dst*/, DmyBuffer/*tmp*/);
		//->dst

	}

	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, TmpBuffer.UAV);


	// Complex transform in the other direction: TmpBuffer fills DstBuffer
	if (FitsInGroupSharedMemory(ComplexFFTDesc))
	{
		SuccessValue = SuccessValue &&
			ComplexFFTImage1D::GroupShared(Context, ComplexFFTDesc, TmpRect, TmpBuffer.ShaderResourceTexture, DstBuffer.UAV);
	}
	else
	{
		SuccessValue = SuccessValue &&
			ComplexFFTImage1D::MultiPass(Context, ComplexFFTDesc, TmpRect, TmpBuffer.ShaderResourceTexture, DstBuffer, TmpBuffer);
	}

	return SuccessValue;

}

void GPUFFT::ConvolutionWithTextureImage1D::Requirements(const FFTDescription& FFTDesc, FIntPoint& MinBufferSize, bool& bUseMultipass)
{
	bUseMultipass = !FitsInGroupSharedMemory(FFTDesc);
	MinBufferSize = FFTDesc.TransformExtent();
}

bool GPUFFT::ConvolutionWithTextureImage1D::GroupShared(FGPUFFTShaderContext& Context, const FFTDescription& FFTDesc,
	const FTextureRHIRef& TransformedKernel,
	const FIntRect& SrcWindow,
	const FTextureRHIRef& SrcTexture,
	FUnorderedAccessViewRHIRef& DstUAV)
{

	bool SuccessValue = true;

	if (FitsInGroupSharedMemory(FFTDesc))
	{
		DispatchGSConvolutionWithTextureCS(Context, FFTDesc, TransformedKernel, SrcTexture, SrcWindow, DstUAV);
	}
	else
	{
		SuccessValue = false;
		// @todo
		ensureMsgf(0, TEXT("The FFT size is too large for group shared memory"));
	}

	return SuccessValue;
}

bool GPUFFT::ConvolutionWithTextureImage1D::MultiPass(FGPUFFTShaderContext& Context, const FFTDescription& FFTDesc,
	const FTextureRHIRef& TransformedKernel,
	const FIntRect& SrcWindow,
	const FTextureRHIRef& SrcTexture,
	FSceneRenderTargetItem& DstBuffer,
	FSceneRenderTargetItem& TmpBuffer)
{
	bool SuccessValue = true;

	
	// Frequency Space size.
	const FIntPoint TargetExtent = FFTDesc.TransformExtent();
	const FIntRect  TargetRect(FIntPoint(0, 0), TargetExtent);

	// Forward transform: Results in DstBuffer
	SuccessValue = SuccessValue &&
		ComplexFFTImage1D::MultiPass(Context, FFTDesc, SrcWindow, SrcTexture, DstBuffer, TmpBuffer);
	Context.GetRHICmdList().TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, DstBuffer.UAV);


	// Convolution: Results in TmpBuffer
	DispatchComplexMultiplyImagesCS(Context, FFTDesc.IsHorizontal(), TargetRect, DstBuffer.ShaderResourceTexture, TransformedKernel,  TmpBuffer.UAV);
	Context.GetRHICmdList().TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, TmpBuffer.UAV);

	// Inverse Transform: Results in DstBuffer
	
	FFTDescription InvFFTDesc = FFTDesc;
	InvFFTDesc.XFormType = GetInverseOfXForm(FFTDesc.XFormType);
	// testing.  Verify that the forward transform works.
	
	SuccessValue = SuccessValue &&
		ComplexFFTImage1D::MultiPass(Context, InvFFTDesc, TargetRect, TmpBuffer.ShaderResourceTexture, DstBuffer, TmpBuffer);
	
	
	Context.GetRHICmdList().TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, DstBuffer.UAV);

	// Copy back to the correct sized sub-window
	DispatchCopyWindowCS(Context, TargetRect, DstBuffer.ShaderResourceTexture, SrcWindow, TmpBuffer.UAV);

	SwapContents(TmpBuffer, DstBuffer);

	return SuccessValue;
}


FIntPoint GPUFFT::Convolution2DBufferSize(const FIntPoint& FrequencySize, const bool bHorizontalFirst, const FIntPoint& ROIExtent)
{
	using GPUFFT::FFTDescription;
	using GPUFFT::FFT_XFORM_TYPE;

	FFTDescription TwoForOneFFTDesc;
	TwoForOneFFTDesc.XFormType    = bHorizontalFirst ? FFT_XFORM_TYPE::FORWARD_HORIZONTAL : FFT_XFORM_TYPE::FORWARD_VERTICAL;
	TwoForOneFFTDesc.SignalLength = bHorizontalFirst ? FrequencySize.X : FrequencySize.Y;
	TwoForOneFFTDesc.NumScanLines = (bHorizontalFirst) ? ROIExtent.Y : ROIExtent.X;


	FFTDescription ConvolutionFFTDesc;
	ConvolutionFFTDesc.XFormType = bHorizontalFirst ? FFT_XFORM_TYPE::FORWARD_VERTICAL : FFT_XFORM_TYPE::FORWARD_HORIZONTAL;
	ConvolutionFFTDesc.SignalLength = bHorizontalFirst ? FrequencySize.Y : FrequencySize.X;
	ConvolutionFFTDesc.NumScanLines = TwoForOneFFTDesc.SignalLength + 2;

	FFTDescription TwoForOneIvnFFTDesc = TwoForOneFFTDesc;
	TwoForOneIvnFFTDesc.XFormType = GetInverseOfXForm(TwoForOneFFTDesc.XFormType);
	FIntPoint BufferSize;

	if (FitsInGroupSharedMemory(ConvolutionFFTDesc))
	{
		FFTDescription TmpDesc = TwoForOneFFTDesc;
		TmpDesc.SignalLength += 2;
		BufferSize = TmpDesc.TransformExtent();
	}
	else
	{
		// a larger buffer is needed when the convolution can't be done in group-shared
		BufferSize = ConvolutionFFTDesc.TransformExtent();
	}
	return BufferSize;
}

bool GPUFFT::ConvolutionWithTextureImage2D(FGPUFFTShaderContext& Context, const FIntPoint& FrequencySize, bool bHorizontalFirst,
	const FTextureRHIRef& TransformedKernel,
	const FIntRect& ROIRect, const FTextureRHIRef& SrcTexture,
	FUnorderedAccessViewRHIRef& ResultUAV,
	FSceneRenderTargetItem& TmpBuffer0,
	FSceneRenderTargetItem& TmpBuffer1,
	const FPreFilter& PreFilter)
{

	using GPUFFT::FFTDescription;

	FRHICommandListImmediate& RHICmdList = Context.GetRHICmdList();

	// This will be for the actual output.
	const FIntRect FFTResultRect = ROIRect;
	const FIntPoint ROISize = ROIRect.Size();

	//DoubleBufferTargets TmpBuffers(Context, TmpExent, PixelFormat);

	// Set up the transform descriptions
	FFTDescription TwoForOneFFTDesc;
	TwoForOneFFTDesc.XFormType = bHorizontalFirst ? FFT_XFORM_TYPE::FORWARD_HORIZONTAL : FFT_XFORM_TYPE::FORWARD_VERTICAL;
	TwoForOneFFTDesc.SignalLength = bHorizontalFirst ? FrequencySize.X : FrequencySize.Y;
	TwoForOneFFTDesc.NumScanLines = (bHorizontalFirst) ? ROISize.Y : ROISize.X;

	// The output has two more elements
	FIntRect TwoForOneOutputRect;
	{
		FFTDescription TmpDesc = TwoForOneFFTDesc;
		TmpDesc.SignalLength += 2;
		TwoForOneOutputRect = FIntRect(FIntPoint(0, 0), TmpDesc.TransformExtent());
	}

	FFTDescription ConvolutionFFTDesc;
	ConvolutionFFTDesc.XFormType = bHorizontalFirst ? FFT_XFORM_TYPE::FORWARD_VERTICAL : FFT_XFORM_TYPE::FORWARD_HORIZONTAL;
	ConvolutionFFTDesc.SignalLength = bHorizontalFirst ? FrequencySize.Y : FrequencySize.X;
	ConvolutionFFTDesc.NumScanLines = TwoForOneFFTDesc.SignalLength + 2; // two-for-one transform generated two additional elements

	FFTDescription TwoForOneIvnFFTDesc = TwoForOneFFTDesc;
	TwoForOneIvnFFTDesc.XFormType = GetInverseOfXForm(TwoForOneFFTDesc.XFormType);
	
	

	// Perform the transforms and convolutions.
	bool SuccessValue = true;


	// ---- Two For One Transform --- 

	// Horizontal FFT: input -> tmp buffer 0 
	if (FitsInGroupSharedMemory(TwoForOneFFTDesc))
	{
		SuccessValue = SuccessValue &&
			TwoForOneRealFFTImage1D::GroupShared(Context, TwoForOneFFTDesc, ROIRect, SrcTexture, TwoForOneOutputRect, TmpBuffer0.UAV, PreFilter);
		//->dst
	}
	else
	{
		SuccessValue = SuccessValue &&
			TwoForOneRealFFTImage1D::MultiPass(Context, TwoForOneFFTDesc, ROIRect, SrcTexture, TmpBuffer0, TmpBuffer1, PreFilter);
	}
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, TmpBuffer0.UAV);
	
	// ---- 1 D Convolution --- 

	if (FitsInGroupSharedMemory(ConvolutionFFTDesc))
	{
		// Vertical FFT / Filter / Vertical InvFFT : tmp buffer 0 -> tmp buffer 1
		SuccessValue = SuccessValue &&
			ConvolutionWithTextureImage1D::GroupShared(Context, ConvolutionFFTDesc, TransformedKernel, TwoForOneOutputRect, TmpBuffer0.ShaderResourceTexture, TmpBuffer1.UAV);
		//->src
	}
	else
	{
		SuccessValue = SuccessValue &&
			ConvolutionWithTextureImage1D::MultiPass(Context, ConvolutionFFTDesc, TransformedKernel, TwoForOneOutputRect, TmpBuffer0.ShaderResourceTexture, TmpBuffer1, TmpBuffer0);
	}
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, TmpBuffer1.UAV);


	// ---- Inverse Two For One ---

	if (FitsInGroupSharedMemory(TwoForOneIvnFFTDesc))
	{
		// Horizontal InvFFT: tmp buffer 1 -> result
		SuccessValue = SuccessValue &&
			TwoForOneRealFFTImage1D::GroupShared(Context, TwoForOneIvnFFTDesc, TwoForOneOutputRect, TmpBuffer1.ShaderResourceTexture, ROIRect, ResultUAV);
		//->dst
	}
	else
	{
		// TmpBuffer1 -> TmpBuffer0
		SuccessValue = SuccessValue &&
			TwoForOneRealFFTImage1D::MultiPass(Context, TwoForOneIvnFFTDesc, TwoForOneOutputRect, TmpBuffer1.ShaderResourceTexture, TmpBuffer0, TmpBuffer1);
		
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, TmpBuffer0.UAV);

		DispatchCopyWindowCS(Context, TwoForOneOutputRect, TmpBuffer0.ShaderResourceTexture, ROIRect, ResultUAV);
	}
	
	return SuccessValue;
}

