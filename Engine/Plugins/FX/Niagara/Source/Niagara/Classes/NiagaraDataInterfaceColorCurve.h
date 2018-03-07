// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "VectorVM.h"
#include "StaticMeshResources.h"
#include "Curves/RichCurve.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceColorCurve.generated.h"

class INiagaraCompiler;
class UCurveVector;
class UCurveLinearColor;
class UCurveFloat;
class FNiagaraSystemInstance;


/** Data Interface allowing sampling of color curves. */
UCLASS(EditInlineNew, Category="Curves", meta = (DisplayName = "Curve for Colors"))
class NIAGARA_API UNiagaraDataInterfaceColorCurve : public UNiagaraDataInterfaceCurveBase
{
	GENERATED_UCLASS_BODY()
public:

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Curve", meta=(AllowedClasses = CurveLinearColor))
	FStringAssetReference CurveToCopy;
#endif

	UPROPERTY(EditAnywhere, Category = "Curve")
	FRichCurve RedCurve;

	UPROPERTY(EditAnywhere, Category = "Curve")
	FRichCurve GreenCurve;

	UPROPERTY(EditAnywhere, Category = "Curve")
	FRichCurve BlueCurve;

	UPROPERTY(EditAnywhere, Category = "Curve")
	FRichCurve AlphaCurve;


	//UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//UObject Interface End

	void UpdateLUT();

	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	virtual FVMExternalFunction GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData)override;

	template<typename XParamType>
	void SampleCurve(FVectorVMContext& Context);

	virtual bool CopyTo(UNiagaraDataInterface* Destination) const override;

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	//~ UNiagaraDataInterfaceCurveBase interface
	virtual void GetCurveData(TArray<FCurveData>& OutCurveData) override;

	virtual bool GetFunctionHLSL(FString FunctionName, TArray<DIGPUBufferParamDescriptor> &Descriptors, FString &HLSLInterfaceID, FString &OutHLSL) override;
	virtual void GetBufferDefinitionHLSL(FString DataInterfaceID, TArray<DIGPUBufferParamDescriptor> &BufferDescriptors, FString &OutHLSL) override;
	virtual TArray<FNiagaraDataInterfaceBufferData> &GetBufferDataArray() override;
	virtual void SetupBuffers(TArray<DIGPUBufferParamDescriptor> &BufferDescriptors) override;
};
