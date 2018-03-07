// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyEditorModule.h"

class IDetailTreeNode;

struct FPropertyRowGeneratorArgs
{
	FPropertyRowGeneratorArgs()
		: NotifyHook(nullptr)
		, DefaultsOnlyVisibility(EEditDefaultsOnlyNodeVisibility::Show)
		, bAllowMultipleTopLevelObjects(false)
	{}

	/** Notify hook to call when properties are changed */
	FNotifyHook* NotifyHook;

	/** Controls how CPF_DisableEditOnInstance nodes will be treated */
	EEditDefaultsOnlyNodeVisibility DefaultsOnlyVisibility;

	bool bAllowMultipleTopLevelObjects;
};

class IPropertyRowGenerator
{
public:
	virtual ~IPropertyRowGenerator() {}

	virtual void SetObjects(const TArray<UObject*>& InObjects) = 0;

	DECLARE_EVENT(IPropertyRowGenerator, FOnRefreshRows);
	virtual FOnRefreshRows& OnRefreshRows() = 0;

	virtual const TArray<TSharedRef<IDetailTreeNode>>& GetRootTreeNodes() const = 0;

	virtual void RegisterInstancedCustomPropertyLayout(UStruct* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate) = 0;
	virtual void RegisterInstancedCustomPropertyTypeLayout(FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate, TSharedPtr<IPropertyTypeIdentifier> Identifier = nullptr) = 0;
	virtual void UnregisterInstancedCustomPropertyLayout(UStruct* Class) = 0;
	virtual void UnregisterInstancedCustomPropertyTypeLayout(FName PropertyTypeName, TSharedPtr<IPropertyTypeIdentifier> Identifier = nullptr) = 0;

};