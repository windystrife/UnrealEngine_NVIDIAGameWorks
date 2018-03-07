// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "VisualStudioSourceCodeAccessor.h"

class FVisualStudioSourceCodeAccessorWrapper : public ISourceCodeAccessor
{
public:
	FVisualStudioSourceCodeAccessorWrapper(const FName& InName, const FText& InNameText, const FText& InDescriptionText, const TSharedRef<FVisualStudioSourceCodeAccessor>& InInner);

	/** ISourceCodeAccessor implementation */
	virtual void RefreshAvailability() override;
	virtual bool CanAccessSourceCode() const override;
	virtual FName GetFName() const override;
	virtual FText GetNameText() const override;
	virtual FText GetDescriptionText() const override;
	virtual bool OpenSolution() override;
	virtual bool OpenSolutionAtPath(const FString& InSolutionPath) override;
	virtual bool DoesSolutionExist() const override;
	virtual bool OpenFileAtLine(const FString& FullPath, int32 LineNumber, int32 ColumnNumber = 0) override;
	virtual bool OpenSourceFiles(const TArray<FString>& AbsoluteSourcePaths) override;
	virtual bool AddSourceFiles(const TArray<FString>& AbsoluteSourcePaths, const TArray<FString>& AvailableModules) override;
	virtual bool SaveAllOpenDocuments() const override;
	virtual void Tick(const float DeltaTime) override;

private:
	FName Name;
	FText NameText;
	FText DescriptionText;
	TSharedRef<FVisualStudioSourceCodeAccessor> Inner;
};
