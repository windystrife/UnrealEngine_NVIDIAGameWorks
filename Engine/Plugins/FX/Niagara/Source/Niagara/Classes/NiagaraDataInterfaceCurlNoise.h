// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraCommon.h"
#include "VectorVM.h"
#include "NiagaraDataInterfaceCurlNoise.generated.h"

/** Data Interface allowing sampling of curl noise LUT. */
UCLASS(EditInlineNew, Category = "Curl Noise LUT", meta = (DisplayName = "Curl Noise"))
class NIAGARA_API UNiagaraDataInterfaceCurlNoise : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()
	bool bGPUBufferDirty;
public:

	UNiagaraDataInterfaceCurlNoise()
		: bGPUBufferDirty(true)
	{}
	static VectorRegister NoiseTable[17][17][17];

	static void InitNoiseLUT();

	//UObject Interface
	virtual void PostInitProperties()override;
	//UObject Interface End

	//UNiagaraDataInterface Interface
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	virtual FVMExternalFunction GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData)override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const override { return true; }
	//UNiagaraDataInterface Interface End

	template<typename XType, typename YType, typename ZType>
	void SampleNoiseField(FVectorVMContext& Context);

	// GPU sim functionality
	virtual bool GetFunctionHLSL(FString FunctionName, TArray<DIGPUBufferParamDescriptor> &Descriptors, FString &HLSLInterfaceID, FString &OutHLSL) override;
	virtual void GetBufferDefinitionHLSL(FString DataInterfaceID, TArray<DIGPUBufferParamDescriptor> &BufferDescriptors, FString &OutHLSL) override;
	virtual TArray<FNiagaraDataInterfaceBufferData> &GetBufferDataArray() override;
	virtual void SetupBuffers(TArray<DIGPUBufferParamDescriptor> &BufferDescriptors) override;
};