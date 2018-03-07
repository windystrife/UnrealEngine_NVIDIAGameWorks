// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_StressTester.h"

#if OCULUS_STRESS_TESTS_ENABLED
#include "OculusHMD.h"
#include "GlobalShader.h"
#include "UniformBuffer.h"
#include "RHICommandList.h"
#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"
#include "PipelineStateCache.h"
#include "Runtime/UtilityShaders/Public/OculusShaders.h"
#include "SceneUtils.h" // for SCOPED_DRAW_EVENT()

DECLARE_STATS_GROUP(TEXT("Oculus"), STATGROUP_Oculus, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("GPUStressRendering"), STAT_GPUStressRendering, STATGROUP_Oculus);

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// Uniform buffers
//-------------------------------------------------------------------------------------------------

//This buffer should contain variables that never, or rarely change
BEGIN_UNIFORM_BUFFER_STRUCT(FOculusPixelShaderConstantParameters,)
//DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, Name)
END_UNIFORM_BUFFER_STRUCT(FOculusPixelShaderConstantParameters)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FOculusPixelShaderConstantParameters, TEXT("PSConstants"))

typedef TUniformBufferRef<FOculusPixelShaderConstantParameters> FOculusPixelShaderConstantParametersRef;


//This buffer is for variables that change very often (each frame for example)
BEGIN_UNIFORM_BUFFER_STRUCT(FOculusPixelShaderVariableParameters,)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int, IterationsMultiplier)
END_UNIFORM_BUFFER_STRUCT(FOculusPixelShaderVariableParameters)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FOculusPixelShaderVariableParameters, TEXT("PSVariables"))

typedef TUniformBufferRef<FOculusPixelShaderVariableParameters> FOculusPixelShaderVariableParametersRef;


//-------------------------------------------------------------------------------------------------
// FTextureVertexDeclaration
//-------------------------------------------------------------------------------------------------

struct FTextureVertex
{
	FVector4	Position;
	FVector2D	UV;
};

class FTextureVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		uint32 Stride = sizeof(FTextureVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FTextureVertex, Position), VET_Float4, 0, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FTextureVertex, UV), VET_Float2, 1, Stride));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

static TGlobalResource<FTextureVertexDeclaration> GOculusTextureVertexDeclaration;


//-------------------------------------------------------------------------------------------------
// FOculusStressShadersPS
//-------------------------------------------------------------------------------------------------

class FOculusStressShadersPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FOculusStressShadersPS, Global);

public:

	FOculusStressShadersPS() {}

	explicit FOculusStressShadersPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		// Bind shader's params
		TextureParameter.Bind(Initializer.ParameterMap, TEXT("TextureParameter"));
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParams = FGlobalShader::Serialize(Ar);

		Ar << TextureParameter;

		return bShaderHasOutdatedParams;
	}

	//This function is required to let us bind our runtime surface to the shader using an SRV.
	void SetSurfaces(FRHICommandList& RHICmdList, FShaderResourceViewRHIRef TextureParameterSRV)
	{
		FPixelShaderRHIParamRef PixelShaderRHI = GetPixelShader();

		if (TextureParameter.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(PixelShaderRHI, TextureParameter.GetBaseIndex(), TextureParameterSRV);
		}
	}

	//This function is required to bind our constant / uniform buffers to the shader.
	void SetUniformBuffers(FRHICommandList& RHICmdList, FOculusPixelShaderConstantParameters& ConstantParameters, FOculusPixelShaderVariableParameters& VariableParameters)
	{
		FOculusPixelShaderConstantParametersRef ConstantParametersBuffer;
		FOculusPixelShaderVariableParametersRef VariableParametersBuffer;

		ConstantParametersBuffer = FOculusPixelShaderConstantParametersRef::CreateUniformBufferImmediate(ConstantParameters, UniformBuffer_SingleDraw);
		VariableParametersBuffer = FOculusPixelShaderVariableParametersRef::CreateUniformBufferImmediate(VariableParameters, UniformBuffer_SingleDraw);

		SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FOculusPixelShaderConstantParameters>(), ConstantParametersBuffer);
		SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FOculusPixelShaderVariableParameters>(), VariableParametersBuffer);
	}

	//This is used to clean up the buffer binds after each invocation to let them be changed and used elsewhere if needed.
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FPixelShaderRHIParamRef PixelShaderRHI = GetPixelShader();

		if (TextureParameter.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(PixelShaderRHI, TextureParameter.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
	}

private:
	//This is how you declare resources that are going to be made available in the HLSL
	FShaderResourceParameter TextureParameter;
};

IMPLEMENT_SHADER_TYPE(, FOculusStressShadersPS, TEXT("/Plugin/OculusVR/Private/OculusStressShaders.usf"), TEXT("MainPixelShader"), SF_Pixel);

//-------------------------------------------------------------------------------------------------
// FStressTester
//-------------------------------------------------------------------------------------------------

TSharedPtr<FStressTester, ESPMode::ThreadSafe> FStressTester::SharedInstance;

TSharedRef<class FStressTester, ESPMode::ThreadSafe> FStressTester::Get()
{
	CheckInGameThread();
	if (!SharedInstance.IsValid())
	{
		SharedInstance = TSharedPtr<class FStressTester, ESPMode::ThreadSafe>(new FStressTester());
		check(SharedInstance.IsValid());
	}
	return SharedInstance.ToSharedRef();
}

FStressTester::FStressTester()
	: Mode(STM_None)
	, CPUSpinOffInSeconds(0.011 / 3.) // one third of the frame (default value)
	, PDsTimeLimitInSeconds(10.) // 10 secs
	, CPUsTimeLimitInSeconds(10.)// 10 secs
	, GPUsTimeLimitInSeconds(10.)// 10 secs
	, GPUIterationsMultiplier(0.)
	, PDStartTimeInSeconds(0.)
	, GPUStartTimeInSeconds(0.)
	, CPUStartTimeInSeconds(0.)
{

}

// multiple masks could be set, see EStressTestMode
void FStressTester::SetStressMode(uint32 InStressMask)
{
	check((InStressMask & (~STM__All)) == 0);
	Mode = InStressMask;

	for (uint32 m = 1; m < STM__All; m <<= 1)
	{
		if (InStressMask & m)
		{
			switch (m)
			{
			case STM_EyeBufferRealloc: UE_LOG(LogHMD, Log, TEXT("PD of EyeBuffer stress test is started")); break;
			case STM_CPUSpin: UE_LOG(LogHMD, Log, TEXT("CPU stress test is started")); break;
			case STM_GPU: UE_LOG(LogHMD, Log, TEXT("GPU stress test is started")); break;
			}
		}
	}
}

void FStressTester::DoTickCPU_GameThread(FOculusHMD* pPlugin)
{
	CheckInGameThread();

	if (Mode & STM_EyeBufferRealloc)
	{
		// Change PixelDensity every frame within MinPixelDensity..MaxPixelDensity range
		if (PDStartTimeInSeconds == 0.)
		{
			PDStartTimeInSeconds = FPlatformTime::Seconds();
		}
		else
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - PDStartTimeInSeconds >= PDsTimeLimitInSeconds)
			{
				PDStartTimeInSeconds = 0.;
				Mode &= ~STM_EyeBufferRealloc;
				UE_LOG(LogHMD, Log, TEXT("PD of EyeBuffer stress test is finished"));
			}
		}

		const int divisor = int((MaxPixelDensity - MinPixelDensity)*10.f);
		float NewPD = float(uint64(FPlatformTime::Seconds()*1000) % divisor) / 10.f + MinPixelDensity;

		pPlugin->SetPixelDensity(NewPD);
	}

	if (Mode & STM_CPUSpin)
	{
		// Simulate heavy CPU load within specified time limits

		if (CPUStartTimeInSeconds == 0.)
		{
			CPUStartTimeInSeconds = FPlatformTime::Seconds();
		}
		else
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - CPUStartTimeInSeconds >= CPUsTimeLimitInSeconds)
			{
				CPUStartTimeInSeconds = 0.;
				Mode &= ~STM_CPUSpin;
				UE_LOG(LogHMD, Log, TEXT("CPU stress test is finished"));
			}
		}

		const double StartSeconds = FPlatformTime::Seconds();
		int i, num = 1, primes = 0;

		bool bFinish = false;
		while (!bFinish)
		{
			i = 2;
			while (i <= num)
			{
				if (num % i == 0)
				{
					break;
				}
				i++;
				const double NowSeconds = FPlatformTime::Seconds();
				if (NowSeconds - StartSeconds >= CPUSpinOffInSeconds)
				{
					bFinish = true;
				}
			}
			if (i == num)
			{
				++primes;
			}

			++num;
		}
	}

	if (Mode & STM_GPU)
	{
		// Simulate heavy CPU load within specified time limits

		if (GPUStartTimeInSeconds == 0.)
		{
			GPUStartTimeInSeconds = FPlatformTime::Seconds();
		}
		else
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - GPUStartTimeInSeconds >= GPUsTimeLimitInSeconds)
			{
				GPUStartTimeInSeconds = 0.;
				Mode &= ~STM_GPU;
				UE_LOG(LogHMD, Log, TEXT("GPU stress test is finished"));
			}
		}
	}
}

void FStressTester::DoTickGPU_RenderThread(FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture)
{
	CheckInRenderThread();

	if (Mode & STM_GPU)
	{
		SCOPED_DRAW_EVENT(RHICmdList, StressGPURendering);
		SCOPE_CYCLE_COUNTER(STAT_GPUStressRendering);

		// Draw a quad with our shader
		FOculusPixelShaderConstantParameters ConstantParameters;
		FOculusPixelShaderVariableParameters VariableParameters;
		if (GPUIterationsMultiplier <= 0)
		{
			VariableParameters.IterationsMultiplier = int(uint64(FPlatformTime::Seconds()*1000) % 20 + 1);
		}
		else
		{
			VariableParameters.IterationsMultiplier = GPUIterationsMultiplier;
		}

		//This is where the magic happens
		SetRenderTarget(RHICmdList, BackBuffer, FTextureRHIRef());

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		const auto FeatureLevel = GMaxRHIFeatureLevel;
		TShaderMapRef<FOculusVertexShader> VertexShader(GetGlobalShaderMap(FeatureLevel));
		TShaderMapRef<FOculusStressShadersPS> PixelShader(GetGlobalShaderMap(FeatureLevel));

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GOculusTextureVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		FShaderResourceViewRHIRef TextureParameterSRV = RHICreateShaderResourceView(SrcTexture, 0);
		PixelShader->SetSurfaces(RHICmdList, TextureParameterSRV);
		PixelShader->SetUniformBuffers(RHICmdList, ConstantParameters, VariableParameters);

		// Draw a fullscreen quad that we can run our pixel shader on
		FTextureVertex Vertices[4];
		Vertices[0].Position = FVector4(-1.0f, 1.0f, 0, 1.0f);
		Vertices[1].Position = FVector4(1.0f, 1.0f, 0, 1.0f);
		Vertices[2].Position = FVector4(-1.0f, -1.0f, 0, 1.0f);
		Vertices[3].Position = FVector4(1.0f, -1.0f, 0, 1.0f);
		Vertices[0].UV = FVector2D(0, 0);
		Vertices[1].UV = FVector2D(1, 0);
		Vertices[2].UV = FVector2D(0, 1);
		Vertices[3].UV = FVector2D(1, 1);

		DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]));

		PixelShader->UnbindBuffers(RHICmdList);
	}
}


//-------------------------------------------------------------------------------------------------
// Console commands for managing the stress tester:
//-------------------------------------------------------------------------------------------------

static void StressGPUCmdHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	auto StressTester = FStressTester::Get();
	StressTester->SetStressMode(FStressTester::STM_GPU | StressTester->GetStressMode());
	if (Args.Num() > 0)
	{
		const int GpuMult = FCString::Atoi(*Args[0]);
		StressTester->SetGPULoadMultiplier(GpuMult);
	}
	if (Args.Num() > 1)
	{
		const float GpuTimeLimit = FCString::Atof(*Args[1]);
		StressTester->SetGPUsTimeLimitInSeconds(GpuTimeLimit);
	}
}

static FAutoConsoleCommand CStressGPUCmd(
	TEXT("vr.oculus.Stress.GPU"),
	*NSLOCTEXT("OculusRift", "CCommandText_StressGPU",
		"Initiates a GPU stress test.\n Usage: vr.oculus.Stress.GPU [LoadMultiplier [TimeLimit]]").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(StressGPUCmdHandler));

static void StressCPUCmdHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	auto StressTester = FStressTester::Get();
	StressTester->SetStressMode(FStressTester::STM_CPUSpin | StressTester->GetStressMode());
	if (Args.Num() > 0)
	{
		const float CpuLimit = FCString::Atof(*Args[0]);
		StressTester->SetCPUSpinOffPerFrameInSeconds(CpuLimit);
	}
	if (Args.Num() > 1)
	{
		const float CpuTimeLimit = FCString::Atof(*Args[1]);
		StressTester->SetCPUsTimeLimitInSeconds(CpuTimeLimit);
	}
}

static FAutoConsoleCommand CStressCPUCmd(
	TEXT("vr.oculus.Stress.CPU"),
	*NSLOCTEXT("OculusRift", "CCommandText_StressCPU",
		"Initiates a CPU stress test.\n Usage: vr.oculus.Stress.CPU [PerFrameTime [TotalTimeLimit]]").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(StressCPUCmdHandler));

static void StressPDCmdHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	auto StressTester = FStressTester::Get();
	StressTester->SetStressMode(FStressTester::STM_EyeBufferRealloc | StressTester->GetStressMode());
	if (Args.Num() > 0)
	{
		const float TimeLimit = FCString::Atof(*Args[0]);
		StressTester->SetPDsTimeLimitInSeconds(TimeLimit);
	}
}

static FAutoConsoleCommand CStressPDCmd(
	TEXT("vr.oculus.Stress.PD"),
	*NSLOCTEXT("OculusRift", "CCommandText_StressPD",
		"Initiates a pixel density stress test wher pixel density is changed every frame for TotalTimeLimit seconds.\n Usage: vr.oculus.Stress.PD [TotalTimeLimit]").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(StressPDCmdHandler));

static void StressResetCmdHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	auto StressTester = FStressTester::Get();
	StressTester->SetStressMode(0);
}

static FAutoConsoleCommand CStressResetCmd(
	TEXT("vr.oculus.Stress.Reset"),
	*NSLOCTEXT("OculusRift", "CCommandText_StressReset",
		"Resets the stress tester and stops all currently running stress tests.\n Usage: vr.oculus.Stress.Reset").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(StressResetCmdHandler));


} // namespace OculusHMD

#endif // #if OCULUS_STRESS_TESTS_ENABLED