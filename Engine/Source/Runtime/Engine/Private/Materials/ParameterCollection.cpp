// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ParameterCollection.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "RenderingThread.h"
#include "UniformBuffer.h"
#include "Engine/World.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialExpressionCollectionParameter.h"

TMap<FGuid, FMaterialParameterCollectionInstanceResource*> GDefaultMaterialParameterCollectionInstances;

UMaterialParameterCollection::UMaterialParameterCollection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultResource = NULL;
}

void UMaterialParameterCollection::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		DefaultResource = new FMaterialParameterCollectionInstanceResource();
	}
}

void UMaterialParameterCollection::PostLoad()
{
	Super::PostLoad();
	
	if (!StateId.IsValid())
	{
		StateId = FGuid::NewGuid();
	}

	CreateBufferStruct();

	// Create an instance for this collection in every world
	for (TObjectIterator<UWorld> It; It; ++It)
	{
		UWorld* CurrentWorld = *It;
		CurrentWorld->AddParameterCollectionInstance(this, true);
	}

	UpdateDefaultResource();
}

void UMaterialParameterCollection::BeginDestroy()
{
	if (DefaultResource)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FRemoveDefaultResourceCommand,
			FGuid,Id,StateId,
		{
			GDefaultMaterialParameterCollectionInstances.Remove(Id);
		});

		DefaultResource->GameThread_Destroy();
		DefaultResource = NULL;
	}

	Super::BeginDestroy();
}

#if WITH_EDITOR

template<typename ParameterType>
FName CreateUniqueName(TArray<ParameterType>& Parameters, int32 RenameParameterIndex)
{
	FString RenameString;
	Parameters[RenameParameterIndex].ParameterName.ToString(RenameString);

	int32 NumberStartIndex = RenameString.FindLastCharByPredicate([](TCHAR Letter){ return !FChar::IsDigit(Letter); }) + 1;
	
	int32 RenameNumber = 0;
	if (NumberStartIndex < RenameString.Len() - 1)
	{
		FString RenameStringNumberPart = RenameString.RightChop(NumberStartIndex);
		ensure(RenameStringNumberPart.IsNumeric());

		TTypeFromString<int32>::FromString(RenameNumber, *RenameStringNumberPart);
	}

	FString BaseString = RenameString.Left(NumberStartIndex);

	FName Renamed = FName(*FString::Printf(TEXT("%s%u"), *BaseString, ++RenameNumber));

	bool bMatchFound = false;
	
	do
	{
		bMatchFound = false;

		for (int32 i = 0; i < Parameters.Num(); ++i)
		{
			if (Parameters[i].ParameterName == Renamed && RenameParameterIndex != i)
			{
				Renamed = FName(*FString::Printf(TEXT("%s%u"), *BaseString, ++RenameNumber));
				bMatchFound = true;
				break;
			}
		}
	} while (bMatchFound);
	
	return Renamed;
}

template<typename ParameterType>
void SanitizeParameters(TArray<ParameterType>& Parameters)
{
	for (int32 i = 0; i < Parameters.Num() - 1; ++i)
	{
		for (int32 j = i + 1; j < Parameters.Num(); ++j)
		{
			if (Parameters[i].Id == Parameters[j].Id)
			{
				FPlatformMisc::CreateGuid(Parameters[j].Id);
			}

			if (Parameters[i].ParameterName == Parameters[j].ParameterName)
			{
				Parameters[j].ParameterName = CreateUniqueName(Parameters, j);
			}
		}
	}
}

TArray<FCollectionScalarParameter> PreviousScalarParameters;
TArray<FCollectionVectorParameter> PreviousVectorParameters;

void UMaterialParameterCollection::PreEditChange(class FEditPropertyChain& PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	PreviousScalarParameters = ScalarParameters;
	PreviousVectorParameters = VectorParameters;
}

void UMaterialParameterCollection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SanitizeParameters(ScalarParameters);
	SanitizeParameters(VectorParameters);

	// If the array counts have changed, an element has been added or removed, and we need to update the uniform buffer layout,
	// Which also requires recompiling any referencing materials
	if (ScalarParameters.Num() != PreviousScalarParameters.Num()
		|| VectorParameters.Num() != PreviousVectorParameters.Num())
	{
		// Limit the count of parameters to fit within uniform buffer limits
		const uint32 MaxScalarParameters = 1024;

		if (ScalarParameters.Num() > MaxScalarParameters)
		{
			ScalarParameters.RemoveAt(MaxScalarParameters, ScalarParameters.Num() - MaxScalarParameters);
		}

		const uint32 MaxVectorParameters = 1024;

		if (VectorParameters.Num() > MaxVectorParameters)
		{
			VectorParameters.RemoveAt(MaxVectorParameters, VectorParameters.Num() - MaxVectorParameters);
		}

		// Generate a new Id so that unloaded materials that reference this collection will update correctly on load
		StateId = FGuid::NewGuid();

		// Update the uniform buffer layout
		CreateBufferStruct();

		// Recreate each instance of this collection
		for (TObjectIterator<UWorld> It; It; ++It)
		{
			UWorld* CurrentWorld = *It;
			CurrentWorld->AddParameterCollectionInstance(this, false);
		}

		// Build set of changed parameter names
		TSet<FName> ParameterNames;
		for (const FCollectionVectorParameter& Param : PreviousVectorParameters)
		{
			ParameterNames.Add(Param.ParameterName);
		}

		for (const FCollectionScalarParameter& Param : PreviousScalarParameters)
		{
			ParameterNames.Add(Param.ParameterName);
		}

		for (const FCollectionVectorParameter& Param : VectorParameters)
		{
			ParameterNames.Remove(Param.ParameterName);
		}

		for (const FCollectionScalarParameter& Param : ScalarParameters)
		{
			ParameterNames.Remove(Param.ParameterName);
		}

		// Create a material update context so we can safely update materials using this parameter collection.
		{
			FMaterialUpdateContext UpdateContext;

			// Go through all materials in memory and recompile them if they use this material parameter collection
			for (TObjectIterator<UMaterial> It; It; ++It)
			{
				UMaterial* CurrentMaterial = *It;

				bool bRecompile = false;

				// Preview materials often use expressions for rendering that are not in their Expressions array, 
				// And therefore their MaterialParameterCollectionInfos are not up to date.
				if (CurrentMaterial->bIsPreviewMaterial)
				{
					bRecompile = true;
				}
				else
				{
					for (int32 FunctionIndex = 0; FunctionIndex < CurrentMaterial->MaterialParameterCollectionInfos.Num() && !bRecompile; FunctionIndex++)
					{
						if (CurrentMaterial->MaterialParameterCollectionInfos[FunctionIndex].ParameterCollection == this)
						{
							TArray<UMaterialExpressionCollectionParameter*> CollectionParameters;
							CurrentMaterial->GetAllExpressionsInMaterialAndFunctionsOfType(CollectionParameters);
							for (UMaterialExpressionCollectionParameter* CollectionParameter : CollectionParameters)
							{
								if (ParameterNames.Contains(CollectionParameter->ParameterName))
								{
									bRecompile = true;
									break;
								}
							}
						}
					}
				}

				if (bRecompile)
				{
					UpdateContext.AddMaterial(CurrentMaterial);

					// Propagate the change to this material
					CurrentMaterial->PreEditChange(NULL);
					CurrentMaterial->PostEditChange();
					CurrentMaterial->MarkPackageDirty();
				}
			}
		}
	}

	// Update each world's scene with the new instance, and update each instance's uniform buffer to reflect the changes made by the user
	for (TObjectIterator<UWorld> It; It; ++It)
	{
		UWorld* CurrentWorld = *It;
		CurrentWorld->UpdateParameterCollectionInstances(true);
	}

	UpdateDefaultResource();

	PreviousScalarParameters.Empty();
	PreviousVectorParameters.Empty();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR

FName UMaterialParameterCollection::GetParameterName(const FGuid& Id) const
{
	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		if (Parameter.Id == Id)
		{
			return Parameter.ParameterName;
		}
	}

	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];

		if (Parameter.Id == Id)
		{
			return Parameter.ParameterName;
		}
	}

	return NAME_None;
}

FGuid UMaterialParameterCollection::GetParameterId(FName ParameterName) const
{
	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		if (Parameter.ParameterName == ParameterName)
		{
			return Parameter.Id;
		}
	}

	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];

		if (Parameter.ParameterName == ParameterName)
		{
			return Parameter.Id;
		}
	}

	return FGuid();
}

void UMaterialParameterCollection::GetParameterIndex(const FGuid& Id, int32& OutIndex, int32& OutComponentIndex) const
{
	// The parameter and component index allocated in this function must match the memory layout in UMaterialParameterCollectionInstance::GetParameterData

	OutIndex = -1;
	OutComponentIndex = -1;

	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		if (Parameter.Id == Id)
		{
			// Scalar parameters are packed into float4's
			OutIndex = ParameterIndex / 4;
			OutComponentIndex = ParameterIndex % 4;
			break;
		}
	}

	const int32 VectorParameterBase = FMath::DivideAndRoundUp(ScalarParameters.Num(), 4);

	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];

		if (Parameter.Id == Id)
		{
			OutIndex = ParameterIndex + VectorParameterBase;
			break;
		}
	}
}

void UMaterialParameterCollection::GetParameterNames(TArray<FName>& OutParameterNames, bool bVectorParameters) const
{
	if (bVectorParameters)
	{
		for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
		{
			const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];
			OutParameterNames.Add(Parameter.ParameterName);
		}
	}
	else
	{
		for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
		{
			const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];
			OutParameterNames.Add(Parameter.ParameterName);
		}
	}
}

const FCollectionScalarParameter* UMaterialParameterCollection::GetScalarParameterByName(FName ParameterName) const
{
	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		if (Parameter.ParameterName == ParameterName)
		{
			return &Parameter;
		}
	}

	return NULL;
}

const FCollectionVectorParameter* UMaterialParameterCollection::GetVectorParameterByName(FName ParameterName) const
{
	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];

		if (Parameter.ParameterName == ParameterName)
		{
			return &Parameter;
		}
	}

	return NULL;
}

FShaderUniformBufferParameter* ConstructCollectionUniformBufferParameter() { return NULL; }

void UMaterialParameterCollection::CreateBufferStruct()
{	
	TArray<FUniformBufferStruct::FMember> Members;
	uint32 NextMemberOffset = 0;

	const uint32 NumVectors = FMath::DivideAndRoundUp(ScalarParameters.Num(), 4) + VectorParameters.Num();
	new(Members) FUniformBufferStruct::FMember(TEXT("Vectors"),TEXT(""),NextMemberOffset,UBMT_FLOAT32,EShaderPrecisionModifier::Half,1,4,NumVectors,NULL);
	const uint32 VectorArraySize = NumVectors * sizeof(FVector4);
	NextMemberOffset += VectorArraySize;
	static FName LayoutName(TEXT("MaterialCollection"));
	const uint32 StructSize = Align(NextMemberOffset,UNIFORM_BUFFER_STRUCT_ALIGNMENT);
	UniformBufferStruct = MakeUnique<FUniformBufferStruct>(
		LayoutName,
		TEXT("MaterialCollection"),
		TEXT("MaterialCollection"),
		ConstructCollectionUniformBufferParameter,
		StructSize,
		Members,
		false
		);
}

void UMaterialParameterCollection::GetDefaultParameterData(TArray<FVector4>& ParameterData) const
{
	// The memory layout created here must match the index assignment in UMaterialParameterCollection::GetParameterIndex

	ParameterData.Empty(FMath::DivideAndRoundUp(ScalarParameters.Num(), 4) + VectorParameters.Num());

	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		// Add a new vector for each packed vector
		if (ParameterIndex % 4 == 0)
		{
			ParameterData.Add(FVector4(0, 0, 0, 0));
		}

		FVector4& CurrentVector = ParameterData.Last();
		// Pack into the appropriate component of this packed vector
		CurrentVector[ParameterIndex % 4] = Parameter.DefaultValue;
	}

	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];
		ParameterData.Add(Parameter.DefaultValue);
	}
}

void UMaterialParameterCollection::UpdateDefaultResource()
{
	// Propagate the new values to the rendering thread
	TArray<FVector4> ParameterData;
	GetDefaultParameterData(ParameterData);
	DefaultResource->GameThread_UpdateContents(StateId, ParameterData);

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FUpdateDefaultResourceCommand,
		FGuid,Id,StateId,
		FMaterialParameterCollectionInstanceResource*,Resource,DefaultResource,
	{
		GDefaultMaterialParameterCollectionInstances.Add(Id, Resource);
	});
}

UMaterialParameterCollectionInstance::UMaterialParameterCollectionInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Resource = NULL;
}

void UMaterialParameterCollectionInstance::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		Resource = new FMaterialParameterCollectionInstanceResource();
	}
}

void UMaterialParameterCollectionInstance::SetCollection(UMaterialParameterCollection* InCollection, UWorld* InWorld)
{
	Collection = InCollection;
	World = InWorld;

	UpdateRenderState();
}

bool UMaterialParameterCollectionInstance::SetScalarParameterValue(FName ParameterName, float ParameterValue)
{
	check(World && Collection);

	if (Collection->GetScalarParameterByName(ParameterName))
	{
		float* ExistingValue = ScalarParameterValues.Find(ParameterName);
		bool bUpdateUniformBuffer = false;

		if (ExistingValue && *ExistingValue != ParameterValue)
		{
			// Update the existing instance override if the new value is different
			bUpdateUniformBuffer = true;
			*ExistingValue = ParameterValue;
		}
		else if (!ExistingValue)
		{
			// Add a new instance override
			bUpdateUniformBuffer = true;
			ScalarParameterValues.Add(ParameterName, ParameterValue);
		}

		if (bUpdateUniformBuffer)
		{
			//@todo - only update uniform buffers max once per frame
			UpdateRenderState();
		}

		return true;
	}

	return false;
}

bool UMaterialParameterCollectionInstance::SetVectorParameterValue(FName ParameterName, const FLinearColor& ParameterValue)
{
	check(World && Collection);

	if (Collection->GetVectorParameterByName(ParameterName))
	{
		FLinearColor* ExistingValue = VectorParameterValues.Find(ParameterName);
		bool bUpdateUniformBuffer = false;

		if (ExistingValue && *ExistingValue != ParameterValue)
		{
			// Update the existing instance override if the new value is different
			bUpdateUniformBuffer = true;
			*ExistingValue = ParameterValue;
		}
		else if (!ExistingValue)
		{
			// Add a new instance override
			bUpdateUniformBuffer = true;
			VectorParameterValues.Add(ParameterName, ParameterValue);
		}

		if (bUpdateUniformBuffer)
		{
			//@todo - only update uniform buffers max once per frame
			UpdateRenderState();
		}

		return true;
	}

	return false;
}

bool UMaterialParameterCollectionInstance::GetScalarParameterValue(FName ParameterName, float& OutParameterValue) const
{
	const FCollectionScalarParameter* Parameter = Collection->GetScalarParameterByName(ParameterName);

	if (Parameter)
	{
		const float* InstanceValue = ScalarParameterValues.Find(ParameterName);
		OutParameterValue = InstanceValue != NULL ? *InstanceValue : Parameter->DefaultValue;
		return true;
	}

	return false;
}

bool UMaterialParameterCollectionInstance::GetVectorParameterValue(FName ParameterName, FLinearColor& OutParameterValue) const
{
	const FCollectionVectorParameter* Parameter = Collection->GetVectorParameterByName(ParameterName);

	if (Parameter)
	{
		const FLinearColor* InstanceValue = VectorParameterValues.Find(ParameterName);
		OutParameterValue = InstanceValue != NULL ? *InstanceValue : Parameter->DefaultValue;
		return true;
	}

	return false;
}

void UMaterialParameterCollectionInstance::UpdateRenderState()
{
	// Propagate the new values to the rendering thread
	TArray<FVector4> ParameterData;
	GetParameterData(ParameterData);
	Resource->GameThread_UpdateContents(Collection ? Collection->StateId : FGuid(), ParameterData);
	// Update the world's scene with the new uniform buffer pointer
	World->UpdateParameterCollectionInstances(false);
}

void UMaterialParameterCollectionInstance::GetParameterData(TArray<FVector4>& ParameterData) const
{
	// The memory layout created here must match the index assignment in UMaterialParameterCollection::GetParameterIndex

	if (Collection)
	{
		ParameterData.Empty(FMath::DivideAndRoundUp(Collection->ScalarParameters.Num(), 4) + Collection->VectorParameters.Num());

		for (int32 ParameterIndex = 0; ParameterIndex < Collection->ScalarParameters.Num(); ParameterIndex++)
		{
			const FCollectionScalarParameter& Parameter = Collection->ScalarParameters[ParameterIndex];

			// Add a new vector for each packed vector
			if (ParameterIndex % 4 == 0)
			{
				ParameterData.Add(FVector4(0, 0, 0, 0));
			}

			FVector4& CurrentVector = ParameterData.Last();
			const float* InstanceData = ScalarParameterValues.Find(Parameter.ParameterName);
			// Pack into the appropriate component of this packed vector
			CurrentVector[ParameterIndex % 4] = InstanceData ? *InstanceData : Parameter.DefaultValue;
		}

		for (int32 ParameterIndex = 0; ParameterIndex < Collection->VectorParameters.Num(); ParameterIndex++)
		{
			const FCollectionVectorParameter& Parameter = Collection->VectorParameters[ParameterIndex];
			const FLinearColor* InstanceData = VectorParameterValues.Find(Parameter.ParameterName);
			ParameterData.Add(InstanceData ? *InstanceData : Parameter.DefaultValue);
		}
	}
}

void UMaterialParameterCollectionInstance::FinishDestroy()
{
	if (Resource)
	{
		Resource->GameThread_Destroy();
		Resource = NULL;
	}

	Super::FinishDestroy();
}

void FMaterialParameterCollectionInstanceResource::GameThread_UpdateContents(const FGuid& InGuid, const TArray<FVector4>& Data)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		FUpdateCollectionCommand,
		FGuid,Id,InGuid,
		TArray<FVector4>,Data,Data,
		FMaterialParameterCollectionInstanceResource*,Resource,this,
	{
		Resource->UpdateContents(Id, Data);
	});
}

void FMaterialParameterCollectionInstanceResource::GameThread_Destroy()
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FDestroyCollectionCommand,
		FMaterialParameterCollectionInstanceResource*,Resource,this,
	{
		delete Resource;
	});
}

static FName MaterialParameterCollectionInstanceResourceName(TEXT("MaterialParameterCollectionInstanceResource"));
FMaterialParameterCollectionInstanceResource::FMaterialParameterCollectionInstanceResource() :
	UniformBufferLayout(MaterialParameterCollectionInstanceResourceName)
{
}

FMaterialParameterCollectionInstanceResource::~FMaterialParameterCollectionInstanceResource()
{
	check(IsInRenderingThread());
	UniformBuffer.SafeRelease();
}

void FMaterialParameterCollectionInstanceResource::UpdateContents(const FGuid& InId, const TArray<FVector4>& Data)
{
	UniformBuffer.SafeRelease();

	Id = InId;

	if (InId != FGuid() && Data.Num() > 0)
	{
		UniformBuffer.SafeRelease();
		UniformBufferLayout.ConstantBufferSize = Data.GetTypeSize() * Data.Num();
		UniformBufferLayout.ResourceOffset = 0;
		check(UniformBufferLayout.Resources.Num() == 0);
		UniformBuffer = RHICreateUniformBuffer(Data.GetData(), UniformBufferLayout, UniformBuffer_MultiFrame);
	}
}
