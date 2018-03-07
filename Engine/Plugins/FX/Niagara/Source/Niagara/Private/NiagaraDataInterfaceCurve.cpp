// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceCurve.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveFloat.h"
#include "NiagaraTypes.h"


UNiagaraDataInterfaceCurve::UNiagaraDataInterfaceCurve(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UpdateLUT();
}

void UNiagaraDataInterfaceCurve::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);
	}

	UpdateLUT();
}

#if WITH_EDITOR

void UNiagaraDataInterfaceCurve::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceCurve, CurveToCopy))
	{
		UCurveFloat* CurveAsset = Cast<UCurveFloat>(CurveToCopy.TryLoad());
		if (CurveAsset)
		{
			Modify();
			Curve = CurveAsset->FloatCurve;
		}
		UpdateLUT();
	}
}

#endif

bool UNiagaraDataInterfaceCurve::CopyTo(UNiagaraDataInterface* Destination) const 
{
	if (!Super::CopyTo(Destination))
	{
		return false;
	}
	CastChecked<UNiagaraDataInterfaceCurve>(Destination)->Curve = Curve;
	CastChecked<UNiagaraDataInterfaceCurve>(Destination)->UpdateLUT();

	return true;
}

bool UNiagaraDataInterfaceCurve::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}

	return CastChecked<UNiagaraDataInterfaceCurve>(Other)->Curve == Curve;
}

void UNiagaraDataInterfaceCurve::GetCurveData(TArray<FCurveData>& OutCurveData)
{
	OutCurveData.Add(FCurveData(&Curve, NAME_None, FLinearColor::Red));
}

void UNiagaraDataInterfaceCurve::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("SampleCurve");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Curve")));
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("X")));
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));
//	Sig.Owner = *GetName();

	OutFunctions.Add(Sig);
}


void UNiagaraDataInterfaceCurve::UpdateLUT()
{
	ShaderLUT.Empty();
	for (uint32 i = 0; i < CurveLUTWidth; i++)
	{
		float X = i / (float)CurveLUTWidth;
		float C = Curve.Eval(X);
		ShaderLUT.Add(C);
	}
	GPUBufferDirty = true;
}

// build the shader function HLSL; function name is passed in, as it's defined per-DI; that way, configuration could change
// the HLSL in the spirit of a static switch
// TODO: need a way to identify each specific function here
// 
bool UNiagaraDataInterfaceCurve::GetFunctionHLSL(FString FunctionName, TArray<DIGPUBufferParamDescriptor> &Descriptors, FString &HLSLInterfaceID, FString &OutHLSL)
{
	FString BufferName = Descriptors[0].BufferParamName;
	OutHLSL += TEXT("void ") + FunctionName + TEXT("(in float In_X, out float Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value = ") + BufferName + TEXT("[(int)(clamp(In_X, 0.0, 1.0) *") + FString::FromInt(CurveLUTWidth) + TEXT(") ];");
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
void UNiagaraDataInterfaceCurve::GetBufferDefinitionHLSL(FString DataInterfaceID, TArray<DIGPUBufferParamDescriptor> &BufferDescriptors, FString &OutHLSL)
{
	FString BufferName = "CurveLUT" + DataInterfaceID;
	OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

	BufferDescriptors.Add(DIGPUBufferParamDescriptor(BufferName, 0));		// add a descriptor for shader parameter binding

}

// called after translate, to setup buffers matching the buffer descriptors generated during hlsl translation
// need to do this because the script used during translate is a clone, including its DIs
//
void UNiagaraDataInterfaceCurve::SetupBuffers(TArray<DIGPUBufferParamDescriptor> &BufferDescriptors)
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
TArray<FNiagaraDataInterfaceBufferData> &UNiagaraDataInterfaceCurve::GetBufferDataArray()
{
	check(IsInRenderingThread());
	if (GPUBufferDirty)
	{
		check(GPUBuffers.Num() > 0);

		FNiagaraDataInterfaceBufferData &GPUBuffer = GPUBuffers[0];
		GPUBuffer.Buffer.Release();
		GPUBuffer.Buffer.Initialize(sizeof(float), CurveLUTWidth * 4, EPixelFormat::PF_R32_FLOAT);	// always allocate for up to 64 data sets
		uint32 BufferSize = ShaderLUT.Num() * sizeof(float);
		int32 *BufferData = static_cast<int32*>(RHILockVertexBuffer(GPUBuffer.Buffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
		FPlatformMemory::Memcpy(BufferData, ShaderLUT.GetData(), BufferSize);
		RHIUnlockVertexBuffer(GPUBuffer.Buffer.Buffer);
		GPUBufferDirty = false;
	}

	return GPUBuffers;
}



DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceCurve, SampleCurve);
FVMExternalFunction UNiagaraDataInterfaceCurve::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData)
{
	if (BindingInfo.Name == TEXT("SampleCurve") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
	{
		return TNDIParamBinder<0, float, NDI_FUNC_BINDER(UNiagaraDataInterfaceCurve, SampleCurve)>::Bind(this, BindingInfo, InstanceData);
	}
	else
	{
		UE_LOG(LogNiagara, Error, TEXT("Could not find data interface external function.\n\tExpected Name: SampleCurve  Actual Name: %s\n\tExpected Inputs: 1  Actual Inputs: %i\n\tExpected Outputs: 1  Actual Outputs: %i"),
			*BindingInfo.Name.ToString(), BindingInfo.GetNumInputs(), BindingInfo.GetNumOutputs());
		return FVMExternalFunction();
	}
}

template<typename XParamType>
void UNiagaraDataInterfaceCurve::SampleCurve(FVectorVMContext& Context)
{
	XParamType XParam(Context);
	FRegisterHandler<float> OutSample(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		*OutSample.GetDest() = Curve.Eval(XParam.Get());
		XParam.Advance();
		OutSample.Advance();
	}
}

