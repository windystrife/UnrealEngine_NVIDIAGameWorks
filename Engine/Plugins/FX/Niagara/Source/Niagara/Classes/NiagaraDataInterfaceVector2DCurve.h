// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "VectorVM.h"
#include "StaticMeshResources.h"
#include "Curves/RichCurve.h"
#include "NiagaraDataInterface.h"

#include "NiagaraDataInterfaceVector2DCurve.generated.h"


/** Data Interface allowing sampling of vector curves. */
UCLASS(EditInlineNew, Category = "Curves", meta = (DisplayName = "Curve for Vector2D's"))
class NIAGARA_API UNiagaraDataInterfaceVector2DCurve : public UNiagaraDataInterfaceCurveBase
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Curve", meta = (AllowedClasses = CurveVector))
		FStringAssetReference CurveToCopy;
#endif

	UPROPERTY(EditAnywhere, Category = "Curve")
	FRichCurve XCurve;

	UPROPERTY(EditAnywhere, Category = "Curve")
	FRichCurve YCurve;

	void UpdateLUT();

	//UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//UObject Interface End

	virtual bool CopyTo(UNiagaraDataInterface* Destination) const override;

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;


	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;

	virtual FVMExternalFunction GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData)override;

	template<typename XParamType>
	void SampleCurve(FVectorVMContext& Context);

	//~ UNiagaraDataInterfaceCurveBase interface
	virtual void GetCurveData(TArray<FCurveData>& OutCurveData) override;

	virtual bool GetFunctionHLSL(FString FunctionName, TArray<DIGPUBufferParamDescriptor> &Descriptors, FString &HLSLInterfaceID, FString &OutHLSL) override;
	virtual void GetBufferDefinitionHLSL(FString DataInterfaceID, TArray<DIGPUBufferParamDescriptor> &BufferDescriptors, FString &OutHLSL) override;
	virtual TArray<FNiagaraDataInterfaceBufferData> &GetBufferDataArray() override;
	virtual void SetupBuffers(TArray<DIGPUBufferParamDescriptor> &BufferDescriptors) override;
};