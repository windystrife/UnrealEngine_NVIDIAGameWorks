// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceVectorCurve.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveFloat.h"
#include "NiagaraTypes.h"

//////////////////////////////////////////////////////////////////////////
//Vector Curve

UNiagaraDataInterfaceVectorCurve::UNiagaraDataInterfaceVectorCurve(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UpdateLUT();
}

void UNiagaraDataInterfaceVectorCurve::PostInitProperties()
{
	Super::PostInitProperties();

	//Can we regitser data interfaces as regular types and fold them into the FNiagaraVariable framework for UI and function calls etc?
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);
	}

	UpdateLUT();
}

void UNiagaraDataInterfaceVectorCurve::PostLoad()
{
	Super::PostLoad();
	UpdateLUT();
}

#if WITH_EDITOR

void UNiagaraDataInterfaceVectorCurve::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceVectorCurve, CurveToCopy))
	{
		UCurveVector* VectorCurveAsset = Cast<UCurveVector>(CurveToCopy.TryLoad());
		if (VectorCurveAsset)
		{
			Modify();
			XCurve = VectorCurveAsset->FloatCurves[0];
			YCurve = VectorCurveAsset->FloatCurves[1];
			ZCurve = VectorCurveAsset->FloatCurves[2];
		}
		UpdateLUT();
	}
}

#endif


void UNiagaraDataInterfaceVectorCurve::UpdateLUT()
{
	ShaderLUT.Empty();
	for (uint32 i = 0; i < CurveLUTWidth; i++)
	{
		float X = i / (float)CurveLUTWidth;
		FVector C(XCurve.Eval(X), YCurve.Eval(X), ZCurve.Eval(X));
		ShaderLUT.Add(C.X);
		ShaderLUT.Add(C.Y);
		ShaderLUT.Add(C.Z);
		ShaderLUT.Add(1.0f);
	}
	GPUBufferDirty = true;
}

bool UNiagaraDataInterfaceVectorCurve::CopyTo(UNiagaraDataInterface* Destination) const 
{
	if (!Super::CopyTo(Destination))
	{
		return false;
	}
	UNiagaraDataInterfaceVectorCurve* DestinationVectorCurve = CastChecked<UNiagaraDataInterfaceVectorCurve>(Destination);
	DestinationVectorCurve->XCurve = XCurve;
	DestinationVectorCurve->YCurve = YCurve;
	DestinationVectorCurve->ZCurve = ZCurve;
	DestinationVectorCurve->UpdateLUT();

	return true;
}

bool UNiagaraDataInterfaceVectorCurve::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}
	const UNiagaraDataInterfaceVectorCurve* OtherVectorCurve = CastChecked<const UNiagaraDataInterfaceVectorCurve>(Other);
	return OtherVectorCurve->XCurve == XCurve &&
		OtherVectorCurve->YCurve == YCurve &&
		OtherVectorCurve->ZCurve == ZCurve;
}

void UNiagaraDataInterfaceVectorCurve::GetCurveData(TArray<FCurveData>& OutCurveData)
{
	OutCurveData.Add(FCurveData(&XCurve, TEXT("X"), FLinearColor::Red));
	OutCurveData.Add(FCurveData(&YCurve, TEXT("Y"), FLinearColor::Green));
	OutCurveData.Add(FCurveData(&ZCurve, TEXT("Z"), FLinearColor::Blue));
}

void UNiagaraDataInterfaceVectorCurve::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("SampleVectorCurve");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VectorCurve")));
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("X")));
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));
	//Sig.Owner = *GetName();

	OutFunctions.Add(Sig);
}



// build the shader function HLSL; function name is passed in, as it's defined per-DI; that way, configuration could change
// the HLSL in the spirit of a static switch
// TODO: need a way to identify each specific function here
// 
bool UNiagaraDataInterfaceVectorCurve::GetFunctionHLSL(FString FunctionName, TArray<DIGPUBufferParamDescriptor> &Descriptors, FString &HLSLInterfaceID, FString &OutHLSL)
{
	FString BufferName = Descriptors[0].BufferParamName;
	OutHLSL += TEXT("void ") + FunctionName + TEXT("(in float In_X, out float3 Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value.x = ") + BufferName + TEXT("[(int)(In_X *") + FString::FromInt(CurveLUTWidth) + TEXT(")* 3 ];");
	OutHLSL += TEXT("\t Out_Value.y = ") + BufferName + TEXT("[1+ (int)(In_X *") + FString::FromInt(CurveLUTWidth) + TEXT(")* 3 ];");
	OutHLSL += TEXT("\t Out_Value.z = ") + BufferName + TEXT("[2+ (int)(In_X *") + FString::FromInt(CurveLUTWidth) + TEXT(")* 3 ];");
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
void UNiagaraDataInterfaceVectorCurve::GetBufferDefinitionHLSL(FString DataInterfaceID, TArray<DIGPUBufferParamDescriptor> &BufferDescriptors, FString &OutHLSL)
{
	FString BufferName = "CurveLUT" + DataInterfaceID;
	OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

	BufferDescriptors.Add(DIGPUBufferParamDescriptor(BufferName, 0));		// add a descriptor for shader parameter binding

}

// called after translate, to setup buffers matching the buffer descriptors generated during hlsl translation
// need to do this because the script used during translate is a clone, including its DIs
//
void UNiagaraDataInterfaceVectorCurve::SetupBuffers(TArray<DIGPUBufferParamDescriptor> &BufferDescriptors)
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
TArray<FNiagaraDataInterfaceBufferData> &UNiagaraDataInterfaceVectorCurve::GetBufferDataArray()
{
	check(IsInRenderingThread());
	if (GPUBufferDirty)
	{
		check(GPUBuffers.Num() > 0);

		FNiagaraDataInterfaceBufferData &GPUBuffer = GPUBuffers[0];
		GPUBuffer.Buffer.Release();
		GPUBuffer.Buffer.Initialize(sizeof(float), CurveLUTWidth * 3, EPixelFormat::PF_R32_FLOAT);	// always allocate for up to 64 data sets
		uint32 BufferSize = ShaderLUT.Num() * sizeof(float);
		int32 *BufferData = static_cast<int32*>(RHILockVertexBuffer(GPUBuffer.Buffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
		FPlatformMemory::Memcpy(BufferData, ShaderLUT.GetData(), BufferSize);
		RHIUnlockVertexBuffer(GPUBuffer.Buffer.Buffer);
		GPUBufferDirty = false;
	}

	return GPUBuffers;
}


DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceVectorCurve, SampleCurve);
FVMExternalFunction UNiagaraDataInterfaceVectorCurve::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData)
{
	if (BindingInfo.Name == TEXT("SampleVectorCurve") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
	{
		return TNDIParamBinder<0, float, NDI_FUNC_BINDER(UNiagaraDataInterfaceVectorCurve, SampleCurve)>::Bind(this, BindingInfo, InstanceData);
	}
	else
	{
		UE_LOG(LogNiagara, Error, TEXT("Could not find data interface external function.\n\tExpected Name: SampleVectorCurve  Actual Name: %s\n\tExpected Inputs: 1  Actual Inputs: %i\n\tExpected Outputs: 3  Actual Outputs: %i"),
			*BindingInfo.Name.ToString(), BindingInfo.GetNumInputs(), BindingInfo.GetNumOutputs());
		return FVMExternalFunction();
	}
}

template<typename XParamType>
void UNiagaraDataInterfaceVectorCurve::SampleCurve(FVectorVMContext& Context)
{
	//TODO: Create some SIMDable optimized representation of the curve to do this faster.
	XParamType XParam(Context);
	FRegisterHandler<float> OutSampleX(Context);
	FRegisterHandler<float> OutSampleY(Context);
	FRegisterHandler<float> OutSampleZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		float X = XParam.Get();
		FVector V(XCurve.Eval(X), YCurve.Eval(X), ZCurve.Eval(X));
		*OutSampleX.GetDest() = V.X;
		*OutSampleY.GetDest() = V.Y;
		*OutSampleZ.GetDest() = V.Z;
		XParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
	}
}
