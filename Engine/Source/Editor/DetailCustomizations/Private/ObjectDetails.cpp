// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ObjectDetails.h"
#include "ScopedTransaction.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/UnrealType.h"
#include "EditorStyleSet.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"
#include "EdGraphSchema_K2.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "ObjectEditorUtils.h"

#define LOCTEXT_NAMESPACE "ObjectDetails"

TSharedRef<IDetailCustomization> FObjectDetails::MakeInstance()
{
	return MakeShareable(new FObjectDetails);
}

void FObjectDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	AddExperimentalWarningCategory(DetailBuilder);
	AddCallInEditorMethods(DetailBuilder);
}

void FObjectDetails::AddExperimentalWarningCategory(IDetailLayoutBuilder& DetailBuilder)
{
	bool bBaseClassIsExperimental = false;
	bool bBaseClassIsEarlyAccess = false;

	FObjectEditorUtils::GetClassDevelopmentStatus(DetailBuilder.GetBaseClass(), bBaseClassIsExperimental, bBaseClassIsEarlyAccess);

	if (bBaseClassIsExperimental || bBaseClassIsEarlyAccess)
	{
		const FName CategoryName(TEXT("Warning"));
		const FText CategoryDisplayName = LOCTEXT("WarningCategoryDisplayName", "Warning");
		FString ClassUsed = DetailBuilder.GetTopLevelProperty().ToString();
		const FText WarningText = bBaseClassIsExperimental ? FText::Format( LOCTEXT("ExperimentalClassWarning", "Uses experimental class: {0}") , FText::FromString(ClassUsed) )
			: FText::Format( LOCTEXT("EarlyAccessClassWarning", "Uses early access class {0}"), FText::FromString(*ClassUsed) );
		const FText SearchString = WarningText;
		const FText Tooltip = bBaseClassIsExperimental ? LOCTEXT("ExperimentalClassTooltip", "Here be dragons!  Uses one or more unsupported 'experimental' classes") : LOCTEXT("EarlyAccessClassTooltip", "Uses one or more 'early access' classes");
		const FString ExcerptName = bBaseClassIsExperimental ? TEXT("ObjectUsesExperimentalClass") : TEXT("ObjectUsesEarlyAccessClass");
		const FSlateBrush* WarningIcon = FEditorStyle::GetBrush(bBaseClassIsExperimental ? "PropertyEditor.ExperimentalClass" : "PropertyEditor.EarlyAccessClass");

		IDetailCategoryBuilder& WarningCategory = DetailBuilder.EditCategory(CategoryName, CategoryDisplayName, ECategoryPriority::Transform);

		FDetailWidgetRow& WarningRow = WarningCategory.AddCustomRow(SearchString)
			.WholeRowContent()
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("SettingsEditor.CheckoutWarningBorder"))
				.BorderBackgroundColor(FColor (166,137,0))
				[
					SNew(SHorizontalBox)
					.ToolTip(IDocumentation::Get()->CreateToolTip(Tooltip, nullptr, TEXT("Shared/LevelEditor"), ExcerptName))
					.Visibility(EVisibility::Visible)

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SImage)
						.Image(WarningIcon)
					]

					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(WarningText)
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
			];
	}
}

void FObjectDetails::AddCallInEditorMethods(IDetailLayoutBuilder& DetailBuilder)
{
	// Get all of the functions we need to display (done ahead of time so we can sort them)
	TArray<UFunction*, TInlineAllocator<8>> CallInEditorFunctions;
	for (TFieldIterator<UFunction> FunctionIter(DetailBuilder.GetBaseClass(), EFieldIteratorFlags::IncludeSuper); FunctionIter; ++FunctionIter)
	{
		UFunction* TestFunction = *FunctionIter;

		if (TestFunction->GetBoolMetaData(FBlueprintMetadata::MD_CallInEditor) && (TestFunction->ParmsSize == 0))
		{
			CallInEditorFunctions.Add(*FunctionIter);
		}
	}

	if (CallInEditorFunctions.Num() > 0)
	{
		// Copy off the objects being customized so we can invoke a function on them later, removing any that are a CDO
		DetailBuilder.GetObjectsBeingCustomized(/*out*/ SelectedObjectsList);
		SelectedObjectsList.RemoveAllSwap([](TWeakObjectPtr<UObject> ObjPtr) { UObject* Obj = ObjPtr.Get(); return (Obj == nullptr) || Obj->HasAnyFlags(RF_ClassDefaultObject); });
		if (SelectedObjectsList.Num() == 0)
		{
			return;
		}

		// Sort the functions by category and then by name
		CallInEditorFunctions.Sort([](UFunction& A, UFunction& B)
		{
			const int32 CategorySort = A.GetMetaData(FBlueprintMetadata::MD_FunctionCategory).Compare(B.GetMetaData(FBlueprintMetadata::MD_FunctionCategory));
			return (CategorySort == 0) ? (A.GetName() <= B.GetName()) : (CategorySort <= 0);
		});

		struct FCategoryEntry
		{
			FName CategoryName;
			TSharedPtr<SWrapBox> WrapBox;
			FTextBuilder FunctionSearchText;

			FCategoryEntry(FName InCategoryName)
				: CategoryName(InCategoryName)
			{
				WrapBox = SNew(SWrapBox).UseAllottedWidth(true);
			}
		};

		// Build up a set of functions for each category, accumulating search text and buttons in a wrap box
		FName ActiveCategory;
		TArray<FCategoryEntry, TInlineAllocator<8>> CategoryList;
		for (UFunction* Function : CallInEditorFunctions)
		{
			FName FunctionCategoryName(NAME_Default);
			if (Function->HasMetaData(FBlueprintMetadata::FBlueprintMetadata::MD_FunctionCategory))
			{
				FunctionCategoryName = FName(*Function->GetMetaData(FBlueprintMetadata::MD_FunctionCategory));
			}

			if (FunctionCategoryName != ActiveCategory)
			{
				ActiveCategory = FunctionCategoryName;
				CategoryList.Emplace(FunctionCategoryName);
			}
			FCategoryEntry& CategoryEntry = CategoryList.Last();

			//@TODO: Expose the code in UK2Node_CallFunction::GetUserFacingFunctionName / etc...
			const FText ButtonCaption = FText::FromString(FName::NameToDisplayString(*Function->GetName(), false));
			FText FunctionTooltip = Function->GetToolTipText();
			if (FunctionTooltip.IsEmpty())
			{
				FunctionTooltip = FText::FromString(Function->GetName());
			}
			

			TWeakObjectPtr<UFunction> WeakFunctionPtr(Function);
			CategoryEntry.WrapBox->AddSlot()
			.Padding(0.0f, 0.0f, 5.0f, 3.0f)
			[
				SNew(SButton)
				.Text(ButtonCaption)
				.OnClicked(FOnClicked::CreateSP(this, &FObjectDetails::OnExecuteCallInEditorFunction, WeakFunctionPtr))
				.ToolTipText(FText::Format(LOCTEXT("CallInEditorTooltip", "Call an event on the selected object(s)\n\n\n{0}"), FunctionTooltip))
			];

			CategoryEntry.FunctionSearchText.AppendLine(ButtonCaption);
			CategoryEntry.FunctionSearchText.AppendLine(FunctionTooltip);
		}
		
		// Now edit the categories, adding the button strips to the details panel
		for (FCategoryEntry& CategoryEntry : CategoryList)
		{
			IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(CategoryEntry.CategoryName);
			CategoryBuilder.AddCustomRow(CategoryEntry.FunctionSearchText.ToText())
			[
				CategoryEntry.WrapBox.ToSharedRef()
			];
		}
	}
}

FReply FObjectDetails::OnExecuteCallInEditorFunction(TWeakObjectPtr<UFunction> WeakFunctionPtr)
{
	if (UFunction* Function = WeakFunctionPtr.Get())
	{
		//@TODO: Consider naming the transaction scope after the fully qualified function name for better UX
		FScopedTransaction Transaction(LOCTEXT("ExecuteCallInEditorMethod", "Call In Editor Action"));

		FEditorScriptExecutionGuard ScriptGuard;
		for (TWeakObjectPtr<UObject> SelectedObjectPtr : SelectedObjectsList)
		{
			if (UObject* Object = SelectedObjectPtr.Get())
			{
				Object->ProcessEvent(Function, nullptr);
			}
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
