// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "VectorVM.h"
#include "StaticMeshResources.h"
#include "Curves/RichCurve.h"
#include "NiagaraDataInterface.generated.h"

class INiagaraCompiler;
class UCurveVector;
class UCurveLinearColor;
class UCurveFloat;
class FNiagaraSystemInstance;

//////////////////////////////////////////////////////////////////////////
// Some helper classes allowing neat, init time binding of templated vm external functions.
USTRUCT()
struct FNiagaraDataInterfaceBufferData
{
	GENERATED_BODY()
public:
	FNiagaraDataInterfaceBufferData()
		:UniformName(TEXT("Undefined"))
	{}

	FNiagaraDataInterfaceBufferData(FName InName)
		:UniformName(InName)
	{
	}
	FReadBuffer Buffer;
	FName UniformName;
};

// Adds a known type to the parameters
template<typename DirectType, typename NextBinder>
struct TNDIExplicitBinder
{
	template<typename... ParamTypes>
	static FVMExternalFunction Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData)
	{
		return NextBinder::template Bind<ParamTypes..., DirectType>(Interface, BindingInfo, InstanceData);
	}
};

// Binder that tests the location of an operand and adds the correct handler type to the Binding parameters.
template<int32 ParamIdx, typename DataType, typename NextBinder>
struct TNDIParamBinder
{
	template<typename... ParamTypes>
	static FVMExternalFunction Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData)
	{
		if (BindingInfo.InputParamLocations[ParamIdx])
		{
			return NextBinder::template Bind<ParamTypes..., FConstantHandler<DataType>>(Interface, BindingInfo, InstanceData);
		}
		else
		{
			return NextBinder::template Bind<ParamTypes..., FRegisterHandler<DataType>>(Interface, BindingInfo, InstanceData);
		}
	}
};

//Helper macros allowing us to define the final binding structs for each vm external function function more concisely.
#define NDI_FUNC_BINDER(ClassName, FuncName) T##ClassName##_##FuncName##Binder

#define DEFINE_NDI_FUNC_BINDER(ClassName, FuncName)\
struct NDI_FUNC_BINDER(ClassName, FuncName)\
{\
	template<typename ... ParamTypes>\
	static FVMExternalFunction Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData)\
	{\
		return FVMExternalFunction::CreateUObject(CastChecked<ClassName>(Interface), &ClassName::FuncName<ParamTypes...>);\
	}\
};

//////////////////////////////////////////////////////////////////////////

/** Base class for all Niagara data interfaces. */
UCLASS(abstract, EditInlineNew)
class NIAGARA_API UNiagaraDataInterface : public UObject
{
	GENERATED_UCLASS_BODY()
		 
public: 

	/** Initializes the per instance data for this interface. Returns false if there was some error and the simulation should be disabled. */
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) { return true; }

	/** Destroys the per instence data for this interface. */
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) {}

	/** Ticks the per instance data for this interface, if it has any. */
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) { return false; }

	/** 
	Returns the size of the per instance data for this interface. 0 if this interface has no per instance data. 
	Must depend solely on the class of the interface and not on any particular member data of a individual interface.
	*/
	virtual int32 PerInstanceDataSize()const { return 0; }

	/** Gets all the available functions for this data interface. */
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) {}

	/** Returns the delegate for the passed function signature. */
	virtual FVMExternalFunction GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData) { return FVMExternalFunction(); };

	/** Copies the contents of this DataInterface to another.*/
	virtual bool CopyTo(UNiagaraDataInterface* Destination) const;

	/** Determines if this DataInterface is the same as another.*/
	virtual bool Equals(const UNiagaraDataInterface* Other) const;

	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const { return false; }

	/** Determines if this type definition matches to a known data interface type.*/
	static bool IsDataInterfaceType(const FNiagaraTypeDefinition& TypeDef);

	virtual bool GetFunctionHLSL(FString FunctionName, TArray<DIGPUBufferParamDescriptor> &Descriptors, FString &HLSLInterfaceID, FString &OutHLSL)
	{
		check("Unefined HLSL in data interface. Interfaces need to be able to return HLSL for each function they define in GetFunctions.");
		return false;
	}
	virtual void GetBufferDefinitionHLSL(FString DataInterfaceID, TArray<DIGPUBufferParamDescriptor> &BufferDescriptors, FString &OutHLSL)
	{
		check("Unefined HLSL in data interface. Interfaces need to define HLSL for uniforms their functions access.");
	}
	virtual TArray<FNiagaraDataInterfaceBufferData> &GetBufferDataArray()
	{
		check("Undefined buffer array access.");
		return GPUBuffers;
	}

	virtual void SetupBuffers(TArray<DIGPUBufferParamDescriptor> &BufferDescriptors)
	{
		check("Undefined buffer setup.");
	}

protected:
	UPROPERTY()
	TArray<FNiagaraDataInterfaceBufferData> GPUBuffers;  // not sure whether storing this on the base is the right idea
};

/** Base class for curve data interfaces which facilitates handling the curve data in a standardized way. */
UCLASS(EditInlineNew, Category = "Curves", meta = (DisplayName = "Float Curve"))
class NIAGARA_API UNiagaraDataInterfaceCurveBase : public UNiagaraDataInterface
{
protected:
	GENERATED_BODY()
	UPROPERTY()
	bool GPUBufferDirty;
	UPROPERTY()
	TArray<float> ShaderLUT;

public:
	UNiagaraDataInterfaceCurveBase()
		: GPUBufferDirty(false)
	{
	}

	UNiagaraDataInterfaceCurveBase(FObjectInitializer const& ObjectInitializer)
		: GPUBufferDirty(false)
	{}

	enum
	{
		CurveLUTWidth = 128
	};
	/** Structure to facilitate getting standardized curve information from a curve data interface. */
	struct FCurveData
	{
		FCurveData(FRichCurve* InCurve, FName InName, FLinearColor InColor)
			: Curve(InCurve)
			, Name(InName)
			, Color(InColor)
		{
		}
		/** A pointer to the curve. */
		FRichCurve* Curve;
		/** The name of the curve, unique within the data interface, which identifies the curve in the UI. */
		FName Name;
		/** The color to use when displaying this curve in the UI. */
		FLinearColor Color;
	};

	/** Gets information for all of the curves owned by this curve data interface. */
	virtual void GetCurveData(TArray<FCurveData>& OutCurveData) { }

	virtual bool GetFunctionHLSL(FString FunctionName, TArray<DIGPUBufferParamDescriptor> &Descriptors, FString &HLSLInterfaceID, FString &OutHLSL)
	{
		check("Undefined HLSL in data interface. All curve interfaces need to define this function and return SampleCurve code");
		return false;
	}
	virtual void GetBufferDefinitionHLSL(FString DataInterfaceID, TArray<DIGPUBufferParamDescriptor> &BufferDescriptors, FString &OutHLSL)
	{
		check("Unefined HLSL in data interface. Interfaces need to define HLSL for uniforms their functions access.");
	}
	virtual TArray<FNiagaraDataInterfaceBufferData> &GetBufferDataArray()
	{
		check("Undefined buffer array access.");
		return GPUBuffers;
	}
	virtual void SetupBuffers(TArray<DIGPUBufferParamDescriptor> &BufferDescriptors)
	{
		check("Undefined buffer setup.");
	}

	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const override { return true; }
};
