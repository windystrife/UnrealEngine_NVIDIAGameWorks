// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceColorCurve.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveFloat.h"
#include "NiagaraTypes.h"

//////////////////////////////////////////////////////////////////////////
//Color Curve

UNiagaraDataInterfaceColorCurve::UNiagaraDataInterfaceColorCurve(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UpdateLUT();
}

void UNiagaraDataInterfaceColorCurve::PostInitProperties()
{
	Super::PostInitProperties();

	//Can we regitser data interfaces as regular types and fold them into the FNiagaraVariable framework for UI and function calls etc?
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);
	}

	UpdateLUT();
}

void UNiagaraDataInterfaceColorCurve::PostLoad()
{
	Super::PostLoad();
	UpdateLUT();
}

#if WITH_EDITOR

void UNiagaraDataInterfaceColorCurve::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceColorCurve, CurveToCopy))
	{
		UCurveLinearColor* ColorCurveAsset = Cast<UCurveLinearColor>(CurveToCopy.TryLoad());
		if(ColorCurveAsset)
		{
			Modify();
			RedCurve = ColorCurveAsset->FloatCurves[0];
			BlueCurve = ColorCurveAsset->FloatCurves[1];
			GreenCurve = ColorCurveAsset->FloatCurves[2];
			AlphaCurve = ColorCurveAsset->FloatCurves[3];
		}
		UpdateLUT();
	}
}

#endif

void UNiagaraDataInterfaceColorCurve::UpdateLUT()
{
	ShaderLUT.Empty();
	for (uint32 i = 0; i < CurveLUTWidth; i++)
	{
		float X = i / (float)CurveLUTWidth;
		FLinearColor C(RedCurve.Eval(X), GreenCurve.Eval(X), BlueCurve.Eval(X), AlphaCurve.Eval(X));
		ShaderLUT.Add(C.R);
		ShaderLUT.Add(C.G);
		ShaderLUT.Add(C.B);
		ShaderLUT.Add(C.A);
	}
	GPUBufferDirty = true;
}

bool UNiagaraDataInterfaceColorCurve::CopyTo(UNiagaraDataInterface* Destination) const 
{
	if (!Super::CopyTo(Destination))
	{
		return false;
	}
	UNiagaraDataInterfaceColorCurve* DestinationColorCurve = CastChecked<UNiagaraDataInterfaceColorCurve>(Destination);
	DestinationColorCurve->RedCurve = RedCurve;
	DestinationColorCurve->GreenCurve = GreenCurve;
	DestinationColorCurve->BlueCurve = BlueCurve;
	DestinationColorCurve->AlphaCurve = AlphaCurve;
	DestinationColorCurve->UpdateLUT();

	return true;
}

bool UNiagaraDataInterfaceColorCurve::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}
	const UNiagaraDataInterfaceColorCurve* OtherColoRedCurve = CastChecked<const UNiagaraDataInterfaceColorCurve>(Other);
	return OtherColoRedCurve->RedCurve == RedCurve &&
		OtherColoRedCurve->GreenCurve == GreenCurve &&
		OtherColoRedCurve->BlueCurve == BlueCurve &&
		OtherColoRedCurve->AlphaCurve == AlphaCurve;
}

void UNiagaraDataInterfaceColorCurve::GetCurveData(TArray<FCurveData>& OutCurveData)
{
	OutCurveData.Add(FCurveData(&RedCurve, TEXT("Red"), FLinearColor::Red));
	OutCurveData.Add(FCurveData(&GreenCurve, TEXT("Green"), FLinearColor::Green));
	OutCurveData.Add(FCurveData(&BlueCurve, TEXT("Blue"), FLinearColor::Blue));
	OutCurveData.Add(FCurveData(&AlphaCurve, TEXT("Alpha"), FLinearColor::White));
}

void UNiagaraDataInterfaceColorCurve::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("SampleColorCurve");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("ColorCurve")));
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("X")));
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Value")));
	//Sig.Owner = *GetName();

	OutFunctions.Add(Sig);
}

// build the shader function HLSL; function name is passed in, as it's defined per-DI; that way, configuration could change
// the HLSL in the spirit of a static switch
// TODO: need a way to identify each specific function here
// 
bool UNiagaraDataInterfaceColorCurve::GetFunctionHLSL(FString FunctionName, TArray<DIGPUBufferParamDescriptor> &Descriptors, FString &HLSLInterfaceID, FString &OutHLSL)
{
	FString BufferName = Descriptors[0].BufferParamName;
	OutHLSL += TEXT("void ") + FunctionName + TEXT("(in float In_X, out float4 Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value.x = ") + BufferName + TEXT("[(int)(In_X *") + FString::FromInt(CurveLUTWidth) + TEXT(")* 4 ];");
	OutHLSL += TEXT("\t Out_Value.y = ") + BufferName + TEXT("[1+ (int)(In_X *") + FString::FromInt(CurveLUTWidth) + TEXT(")* 4 ];");
	OutHLSL += TEXT("\t Out_Value.z = ") + BufferName + TEXT("[2+ (int)(In_X *") + FString::FromInt(CurveLUTWidth) + TEXT(")* 4 ];");
	OutHLSL += TEXT("\t Out_Value.w = ") + BufferName + TEXT("[3+ (int)(In_X *") + FString::FromInt(CurveLUTWidth) + TEXT(")* 4 ];");
	OutHLSL += TEXT("\n}\n");
	return true;
}

// build buffer definition hlsl
// 1. Choose a buffer name, add the data interface ID (important!)
// 2. add a DIGPUBufferParamDescriptor to the array argument; that'll be passed on to the FNiagaraShader for binding to a shader param, that can
// then later be found by name via FindDIBufferParam for setting; 
// 3. store buffer declaration hlsl in OutHLSL
// multiple buffers can be defined at once here
//
void UNiagaraDataInterfaceColorCurve::GetBufferDefinitionHLSL(FString DataInterfaceID, TArray<DIGPUBufferParamDescriptor> &BufferDescriptors, FString &OutHLSL)
{
	FString BufferName = "CurveLUT" + DataInterfaceID;
	OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

	BufferDescriptors.Add( DIGPUBufferParamDescriptor(BufferName, 0) );		// add a descriptor for shader parameter binding

}

// called after translate, to setup buffers matching the buffer descriptors generated during hlsl translation
// need to do this because the script used during translate is a clone, including its DIs
//
void UNiagaraDataInterfaceColorCurve::SetupBuffers(TArray<DIGPUBufferParamDescriptor> &BufferDescriptors)
{
	for (DIGPUBufferParamDescriptor &Desc : BufferDescriptors)
	{
		FNiagaraDataInterfaceBufferData BufferData(*Desc.BufferParamName);	// store off the data for later use
		GPUBuffers.Add(BufferData);
	}
}

// return the GPU buffer array (called from NiagaraInstanceBatcher to get the buffers for setting to the shader)
// we lazily update the buffer with a new LUT here if necessary
//
TArray<FNiagaraDataInterfaceBufferData> &UNiagaraDataInterfaceColorCurve::GetBufferDataArray()
{
	check(IsInRenderingThread());
	if (GPUBufferDirty)
	{
		check(GPUBuffers.Num() > 0);

		FNiagaraDataInterfaceBufferData &GPUBuffer = GPUBuffers[0];
		GPUBuffer.Buffer.Release();
		GPUBuffer.Buffer.Initialize(sizeof(float), CurveLUTWidth*4 , EPixelFormat::PF_R32_FLOAT, BUF_Volatile);	// always allocate for up to 64 data sets
		uint32 BufferSize = ShaderLUT.Num() * sizeof(float);
		int32 *BufferData = static_cast<int32*>(RHILockVertexBuffer(GPUBuffer.Buffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
		FPlatformMemory::Memcpy(BufferData, ShaderLUT.GetData(), BufferSize);
		RHIUnlockVertexBuffer(GPUBuffer.Buffer.Buffer);
		GPUBufferDirty = false;
	}

	return GPUBuffers;
}




DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceColorCurve, SampleCurve);
FVMExternalFunction UNiagaraDataInterfaceColorCurve::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData)
{
	if (BindingInfo.Name == TEXT("SampleColorCurve") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 4)
	{
		return TNDIParamBinder<0, float, NDI_FUNC_BINDER(UNiagaraDataInterfaceColorCurve, SampleCurve)>::Bind(this, BindingInfo, InstanceData);
	}
	else
	{
		UE_LOG(LogNiagara, Error, TEXT("Could not find data interface external function.\n\tExpected Name: SampleColorCurve  Actual Name: %s\n\tExpected Inputs: 1  Actual Inputs: %i\n\tExpected Outputs: 4  Actual Outputs: %i"),
			*BindingInfo.Name.ToString(), BindingInfo.GetNumInputs(), BindingInfo.GetNumOutputs());
		return FVMExternalFunction();
	}
}

template<typename XParamType>
void UNiagaraDataInterfaceColorCurve::SampleCurve(FVectorVMContext& Context)
{
	//TODO: Create some SIMDable optimized representation of the curve to do this faster.
	XParamType XParam(Context);
	FRegisterHandler<float> SamplePtrR(Context);
	FRegisterHandler<float> SamplePtrG(Context);
	FRegisterHandler<float> SamplePtrB(Context);
	FRegisterHandler<float> SamplePtrA(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		float X = XParam.Get();
		FLinearColor C(RedCurve.Eval(X), GreenCurve.Eval(X), BlueCurve.Eval(X), AlphaCurve.Eval(X));
		*SamplePtrR.GetDest() = C.R;
		*SamplePtrG.GetDest() = C.G;
		*SamplePtrB.GetDest() = C.B;
		*SamplePtrA.GetDest() = C.A;
		XParam.Advance();
		SamplePtrR.Advance();
		SamplePtrG.Advance();
		SamplePtrB.Advance();
		SamplePtrA.Advance();
	}
}
