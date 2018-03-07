// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "WidgetBlueprintEditor.h"
#include "EdGraph/EdGraphSchema.h"
#include "PropertyHandle.h"

class FMenuBuilder;
class UEdGraph;
class UWidgetBlueprint;
struct FEditorPropertyPath;

class SPropertyBinding : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SPropertyBinding)
		: _GeneratePureBindings(true)
		{}
		SLATE_ARGUMENT(bool, GeneratePureBindings)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FWidgetBlueprintEditor> InEditor, UDelegateProperty* DelegateProperty, TSharedRef<IPropertyHandle> Property);

protected:
	struct FFunctionInfo
	{
		FFunctionInfo()
			: Function(nullptr)
		{
		}

		FText DisplayName;
		FString Tooltip;

		FName FuncName;
		UFunction* Function;
	};

	TSharedRef<SWidget> OnGenerateDelegateMenu(UWidget* Widget, TSharedRef<IPropertyHandle> PropertyHandle);
	void FillPropertyMenu(FMenuBuilder& MenuBuilder, TSharedRef<IPropertyHandle> PropertyHandle, UStruct* OwnerStruct, TArray<UField*> BindingChain);

	const FSlateBrush* GetCurrentBindingImage(TSharedRef<IPropertyHandle> PropertyHandle) const;
	FText GetCurrentBindingText(TSharedRef<IPropertyHandle> PropertyHandle) const;

	bool CanRemoveBinding(TSharedRef<IPropertyHandle> PropertyHandle);
	void HandleRemoveBinding(TSharedRef<IPropertyHandle> PropertyHandle);

	void HandleAddFunctionBinding(TSharedRef<IPropertyHandle> PropertyHandle, TSharedPtr<FFunctionInfo> SelectedFunction, TArray<UField*> BindingChain);
	void HandleAddFunctionBinding(TSharedRef<IPropertyHandle> PropertyHandle, TSharedPtr<FFunctionInfo> SelectedFunction, FEditorPropertyPath& BindingPath);
	void HandleAddPropertyBinding(TSharedRef<IPropertyHandle> PropertyHandle, UProperty* SelectedProperty, TArray<UField*> BindingChain);

	void HandleCreateAndAddBinding(UWidget* Widget, TSharedRef<IPropertyHandle> PropertyHandle);
	void GotoFunction(UEdGraph* FunctionGraph);

	EVisibility GetGotoBindingVisibility(TSharedRef<IPropertyHandle> PropertyHandle) const;

	FReply HandleGotoBindingClicked(TSharedRef<IPropertyHandle> PropertyHandle);

	FReply AddOrViewEventBinding(TSharedPtr<FEdGraphSchemaAction> Action);

private:

	template <typename Predicate>
	void ForEachBindableProperty(UStruct* InStruct, Predicate Pred) const;

	template <typename Predicate>
	void ForEachBindableFunction(UClass* FromClass, Predicate Pred) const;

	TWeakPtr<FWidgetBlueprintEditor> Editor;

	UWidgetBlueprint* Blueprint;

	bool GeneratePureBindings;
	UFunction* BindableSignature;
};
