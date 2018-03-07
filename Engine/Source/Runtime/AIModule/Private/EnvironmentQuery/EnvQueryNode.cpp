// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQuery/EnvQueryNode.h"
#include "UObject/UnrealType.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "DataProviders/AIDataProvider_QueryParams.h"

UEnvQueryNode::UEnvQueryNode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UEnvQueryNode::UpdateNodeVersion()
{
	VerNum = 0;
}

FText UEnvQueryNode::GetDescriptionTitle() const
{
	return UEnvQueryTypes::GetShortTypeName(this);
}

FText UEnvQueryNode::GetDescriptionDetails() const
{
	return FText::GetEmpty();
}

#if WITH_EDITOR
void UEnvQueryNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_DataBinding = GET_MEMBER_NAME_CHECKED(FAIDataProviderValue, DataBinding);
	
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : FName();
	if (PropertyName == NAME_DataBinding)
	{
		// populate ParamName value
		UStructProperty* TestStruct = Cast<UStructProperty>(PropertyChangedEvent.MemberProperty);
		if (TestStruct != nullptr)
		{
			const FString TypeDesc = TestStruct->GetCPPType(nullptr, CPPF_None);

#define GET_STRUCT_NAME_CHECKED(StructName) ((void)sizeof(StructName), TEXT(#StructName))

			if (TypeDesc.Contains(GET_STRUCT_NAME_CHECKED(FAIDataProviderIntValue))
				|| TypeDesc.Contains(GET_STRUCT_NAME_CHECKED(FAIDataProviderFloatValue))
				|| TypeDesc.Contains(GET_STRUCT_NAME_CHECKED(FAIDataProviderBoolValue)))
			{				
				const FAIDataProviderTypedValue* PropertyValue = TestStruct->ContainerPtrToValuePtr<FAIDataProviderTypedValue>(this);
				if (PropertyValue)
				{
					UAIDataProvider_QueryParams* QueryParamProvider = Cast<UAIDataProvider_QueryParams>(PropertyValue->DataBinding);
					if (QueryParamProvider && QueryParamProvider->ParamName.IsNone())
					{
						FString Stripped = TEXT("");
						const FString NodeName = GetFName().GetPlainNameString();
						if (NodeName.Split(TEXT("_"), nullptr, &Stripped))
						{
							QueryParamProvider->ParamName = *FString::Printf(TEXT("%s.%s"), *Stripped, *PropertyChangedEvent.MemberProperty->GetName());
						}
						else
						{
							QueryParamProvider->ParamName = PropertyChangedEvent.MemberProperty->GetFName();
						}
					}
				}
			}
#undef GET_STRUCT_NAME_CHECKED
		}
	}

#if USE_EQS_DEBUGGER
	UEnvQueryManager::NotifyAssetUpdate(NULL);
#endif // USE_EQS_DEBUGGER
}
#endif //WITH_EDITOR && USE_EQS_DEBUGGER
