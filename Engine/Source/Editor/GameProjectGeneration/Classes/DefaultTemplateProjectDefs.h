// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "TemplateProjectDefs.h"
#include "DefaultTemplateProjectDefs.generated.h"

UCLASS()
class GAMEPROJECTGENERATION_API UDefaultTemplateProjectDefs : public UTemplateProjectDefs
{
	GENERATED_UCLASS_BODY()

	virtual bool GeneratesCode(const FString& ProjectTemplatePath) const override;

	virtual bool IsClassRename(const FString& DestFilename, const FString& SrcFilename, const FString& FileExtension) const override;

	virtual void AddConfigValues(TArray<FTemplateConfigValue>& ConfigValuesToSet, const FString& TemplateName, const FString& ProjectName, bool bShouldGenerateCode) const override;
};
