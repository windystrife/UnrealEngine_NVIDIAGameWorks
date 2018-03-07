// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SReplaceNodeReferences.h"
#include "UObject/UObjectHash.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Engine/MemberReference.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Variable.h"

#include "FindInBlueprintManager.h"
#include "FindInBlueprints.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ImaginaryBlueprintData.h"
#include "ObjectEditorUtils.h"
#include "EditorCategoryUtils.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SNodeVariableReferences"

class FTargetCategoryReplaceReferences : public FTargetReplaceReferences
{
public:
	FTargetCategoryReplaceReferences(FText InCategoryTitle)
		: CategoryTitle(InCategoryTitle)
	{}

	// FTargetReplaceReferences interface
	virtual TSharedRef<SWidget> CreateWidget() const override
	{
		return 
			SNew(STextBlock)
			.Text(CategoryTitle);
	}

	virtual bool GetMemberReference(FMemberReference& OutVariableReference) const override
	{
		return false;
	}

	virtual FText GetDisplayTitle() const override
	{
		return CategoryTitle;
	}

	virtual bool IsCategory() const override { return true; }
	// End of FTargetReplaceReferences interface

public:
	/** Category title to display for this item */
	FText CategoryTitle;
};

class FTargetVariableReplaceReferences : public FTargetReplaceReferences
{
public:
	// FTargetReplaceReferences interface
	virtual TSharedRef<SWidget> CreateWidget() const override
	{
		return 
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f)
			[
				SNew(SImage)
				.Image(GetIcon())
				.ColorAndOpacity(GetIconColor())
			]

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(FText::FromString(VariableReference.GetMemberName().ToString()))
			];
	}

	virtual bool GetMemberReference(FMemberReference& OutVariableReference) const override
	{
		OutVariableReference = VariableReference;
		return true;
	}

	virtual FText GetDisplayTitle() const override
	{
		return FText::FromName(VariableReference.GetMemberName());
	}

	virtual const struct FSlateBrush* GetIcon() const override
	{
		return FBlueprintEditorUtils::GetIconFromPin(PinType);
	}

	virtual FSlateColor GetIconColor() const override
	{
		UEdGraphSchema_K2 const* K2Schema = GetDefault<UEdGraphSchema_K2>();
		return K2Schema->GetPinTypeColor(PinType);
	}
	// End of FTargetReplaceReferences interface

public:
	/** Variable reference for this item */
	FMemberReference VariableReference;

	/** Pin type representing the UProperty of this item */
	FEdGraphPinType PinType;
};

void SReplaceNodeReferences::Construct(const FArguments& InArgs, TSharedPtr<class FBlueprintEditor> InBlueprintEditor)
{
	BlueprintEditor = InBlueprintEditor;
	Refresh();

	ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3.0f, 5.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FindWhat", "Find what:"))
				]

				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Visibility(this, &SReplaceNodeReferences::GetPickSourceReferenceVisibility)
					.Text(LOCTEXT("PickSourceVariable", "Pick a source variable from the My Blueprints list!"))
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f)
				[
					SNew(SImage)
					.Visibility(this, &SReplaceNodeReferences::GetSourceReferenceVisibility)
					.Image(this, &SReplaceNodeReferences::GetSourceReferenceIcon)
					.ColorAndOpacity(this, &SReplaceNodeReferences::GetSourceReferenceIconColor)
				]

				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Visibility(this, &SReplaceNodeReferences::GetSourceReferenceVisibility)
						.Text(this, &SReplaceNodeReferences::GetSourceDisplayText)
				]
			]

			+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ReplaceWith", "Replace with:"))
				]

			+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Left)
				[
					SNew(SBox)
					.MinDesiredWidth(150.0f)
					[
						SAssignNew( TargetReferencesComboBox, SComboButton )
						.OnGetMenuContent(this, &SReplaceNodeReferences::GetMenuContent)
						.ContentPadding(0)
						.ToolTipText(this, &SReplaceNodeReferences::GetTargetDisplayText)
						.HasDownArrow(true)
						.ButtonContent()
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(2.0f, 0.0f)
							[
								SNew(SImage)
								.Image(this, &SReplaceNodeReferences::GetTargetIcon)
								.ColorAndOpacity(this, &SReplaceNodeReferences::GetTargetIconColor)
							]

							+SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(this, &SReplaceNodeReferences::GetTargetDisplayText)
								]
						]
					]
				]

			+SVerticalBox::Slot()
				[
					SNew(SBox)
						.MinDesiredHeight(150.0f)
						[
							SAssignNew(FindInBlueprints, SFindInBlueprints, InBlueprintEditor)
								.bIsSearchWindow(false)
								.bHideSearchBar(true)
						]

				]

			+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.Padding(2.0f)
					.HAlign(HAlign_Left)
					.AutoWidth()
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Find All")))
						.OnClicked(this, &SReplaceNodeReferences::OnFindAll)
					]

					+SHorizontalBox::Slot()
					.Padding(2.0f)
					.HAlign(HAlign_Left)
					.AutoWidth()
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Find and Replace All")))
						.OnClicked(this, &SReplaceNodeReferences::OnFindAndReplaceAll)
					]
					
				]
		];
}

void SReplaceNodeReferences::Refresh()
{
	SetSourceVariable(nullptr);

	BlueprintVariableList.Empty();
	TargetClass = BlueprintEditor.Pin()->GetBlueprintObj()->SkeletonGeneratedClass;
	GatherAllAvailableBlueprintVariables(TargetClass);
}

void SReplaceNodeReferences::SetSourceVariable(UProperty* InProperty)
{
	if (InProperty)
	{
		UEdGraphSchema_K2 const* K2Schema = GetDefault<UEdGraphSchema_K2>();
		K2Schema->ConvertPropertyToPinType(InProperty, SourcePinType);

		SourceProperty = InProperty;

		BlueprintVariableList.Empty();
		GatherAllAvailableBlueprintVariables(TargetClass);

		if (AvailableTargetReferencesTreeView.IsValid())
		{
			AvailableTargetReferencesTreeView->RequestTreeRefresh();
		}
	}
	else
	{
		SourceProperty = nullptr;
	}
}

SReplaceNodeReferences::~SReplaceNodeReferences()
{

}

TSharedRef<SWidget>	SReplaceNodeReferences::GetMenuContent()
{
	return SAssignNew(AvailableTargetReferencesTreeView, SReplaceReferencesTreeViewType)
		.ItemHeight(24)
		.TreeItemsSource( &BlueprintVariableList )
		.OnSelectionChanged(this, &SReplaceNodeReferences::OnSelectionChanged)
		.OnGenerateRow( this, &SReplaceNodeReferences::OnGenerateRow )
		.OnGetChildren( this, &SReplaceNodeReferences::OnGetChildren );
}

void SReplaceNodeReferences::GatherAllAvailableBlueprintVariables(UClass* InTargetClass)
{
	if (InTargetClass == nullptr)
	{
		return;
	}
	GatherAllAvailableBlueprintVariables(InTargetClass->GetSuperClass());

	TMap<FString, TSharedPtr< FTargetCategoryReplaceReferences > > CategoryMap;

	UObject* PathObject = InTargetClass->ClassGeneratedBy? InTargetClass->ClassGeneratedBy : InTargetClass;
	TSharedPtr< FTargetCategoryReplaceReferences > BlueprintCategory = MakeShareable(new FTargetCategoryReplaceReferences(FText::FromString(PathObject->GetPathName())));
	for (TFieldIterator<UProperty> PropertyIt(InTargetClass, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* Property = *PropertyIt;

		if (Property == SourceProperty)
		{
			continue;
		}
		FName PropName = Property->GetFName();

		// Don't show delegate properties, there is special handling for these
		const bool bMulticastDelegateProp = Property->IsA(UMulticastDelegateProperty::StaticClass());
		const bool bDelegateProp = (Property->IsA(UDelegateProperty::StaticClass()) || bMulticastDelegateProp);
		const bool bShouldShowAsVar = (!Property->HasAnyPropertyFlags(CPF_Parm) && Property->HasAllPropertyFlags(CPF_BlueprintVisible)) && !bDelegateProp;
		const bool bShouldShowAsDelegate = !Property->HasAnyPropertyFlags(CPF_Parm) && bMulticastDelegateProp 
			&& Property->HasAnyPropertyFlags(CPF_BlueprintAssignable | CPF_BlueprintCallable);
		UObjectPropertyBase* Obj = Cast<UObjectPropertyBase>(Property);
		if(!bShouldShowAsVar && !bShouldShowAsDelegate)
		{
			continue;
		}

		const FText PropertyTooltip = Property->GetToolTipText();
		const FName PropertyName = Property->GetFName();
		const FText PropertyDesc = FText::FromName(PropertyName);

		FText CategoryName = FObjectEditorUtils::GetCategoryText(Property);
		FText PropertyCategory = FObjectEditorUtils::GetCategoryText(Property);
		const FString UserCategoryName = FEditorCategoryUtils::GetCategoryDisplayString( PropertyCategory.ToString() );

		UEdGraphSchema_K2 const* K2Schema = GetDefault<UEdGraphSchema_K2>();
		if (CategoryName.EqualTo(FText::FromString(PathObject->GetName())) || CategoryName.EqualTo(K2Schema->VR_DefaultCategory))
		{
			CategoryName = FText::GetEmpty();		// default, so place in 'non' category
			PropertyCategory = FText::GetEmpty();
		}

		if (bShouldShowAsVar)
		{
			const bool bComponentProperty = Obj && Obj->PropertyClass ? Obj->PropertyClass->IsChildOf<UActorComponent>() : false;

			// By default components go into the variable section under the component category unless a custom category is specified.
			if ( bComponentProperty && CategoryName.IsEmpty() )
			{
				PropertyCategory = LOCTEXT("Components", "Components");
			}

			TSharedPtr< FTargetCategoryReplaceReferences > CategoryReference = BlueprintCategory;
			if (!CategoryName.IsEmpty())
			{
				TSharedPtr< FTargetCategoryReplaceReferences >* CategoryReferencePtr = CategoryMap.Find(PropertyCategory.ToString());
				if (CategoryReferencePtr)
				{
					CategoryReference = *CategoryReferencePtr;
				}
				else
				{
					CategoryReference = MakeShareable(new FTargetCategoryReplaceReferences(PropertyCategory));
					CategoryMap.Add(PropertyCategory.ToString(), CategoryReference);
				}
			}

			TSharedPtr< FTargetVariableReplaceReferences > VariableItem = MakeShareable(new FTargetVariableReplaceReferences);
			VariableItem->VariableReference.SetFromField<UProperty>(Property, true);

			FEdGraphPinType Type;
			K2Schema->ConvertPropertyToPinType(Property, VariableItem->PinType);

			if (VariableItem->PinType == SourcePinType)
			{
				CategoryReference->Children.Add(VariableItem);

				// If this is the first Child, add the category to the main Blueprint category
				if (CategoryReference != BlueprintCategory && CategoryReference->Children.Num() == 1)
				{
					BlueprintCategory->Children.Add(CategoryReference);
				}
			}
		}
	}

	if (BlueprintCategory->Children.Num())
	{
		BlueprintVariableList.Add(BlueprintCategory);
		// Sort markers
		struct FCompareCategoryTitles
		{
			FORCEINLINE bool operator()(const TSharedPtr<FTargetReplaceReferences> A, const TSharedPtr<FTargetReplaceReferences> B) const
			{
				if (A->Children.Num() > 0 && B->Children.Num() == 0)
				{
					return true;
				}
				else if (A->Children.Num() == 0 && B->Children.Num() > 0)
				{
					return false;
				}
				return A->GetDisplayTitle().CompareTo(B->GetDisplayTitle()) < 0;
			}
		};
		BlueprintCategory->Children.Sort(FCompareCategoryTitles());
	}
}

TSharedRef<ITableRow> SReplaceNodeReferences::OnGenerateRow(FTreeViewItem InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew( STableRow< TSharedPtr<FFindInBlueprintsResult> >, OwnerTable )
		[
			InItem->CreateWidget()
		];
}

void SReplaceNodeReferences::OnGetChildren( FTreeViewItem InItem, TArray< FTreeViewItem >& OutChildren )
{
	OutChildren += InItem->Children;
}

FReply SReplaceNodeReferences::OnFindAll()
{
	OnSubmitSearchQuery(false);
	return FReply::Handled();
}

FReply SReplaceNodeReferences::OnFindAndReplaceAll()
{
	if (SelectedTargetReferenceItem.IsValid())
	{
		FindInBlueprints->CacheAllBlueprints(FSimpleDelegate::CreateSP(this, &SReplaceNodeReferences::OnSubmitSearchQuery, true), EFiBVersion::FIB_VER_VARIABLE_REFERENCE);
	}
	return FReply::Handled();
}

void SReplaceNodeReferences::OnSubmitSearchQuery(bool bFindAndReplace)
{
	FString SearchTerm;

	FMemberReference SourceVariableReference;
	SourceVariableReference.SetFromField<UProperty>(SourceProperty, true);
	SearchTerm = SourceVariableReference.GetReferenceSearchString(SourceProperty->GetOwnerClass());

	FOnSearchComplete OnSearchComplete;
	if (bFindAndReplace)
	{
		OnSearchComplete = FOnSearchComplete::CreateSP(this, &SReplaceNodeReferences::FindAllReplacementsComplete);
	}

	FindInBlueprints->MakeSearchQuery(SearchTerm, false, ESearchQueryFilter::NodesFilter, EFiBVersion::FIB_VER_VARIABLE_REFERENCE, OnSearchComplete);
}

void SReplaceNodeReferences::FindAllReplacementsComplete(TArray<TSharedPtr<class FImaginaryFiBData>>& InRawDataList)
{
	if (SelectedTargetReferenceItem.IsValid())
	{
		FMemberReference VariableReference;
		if (SelectedTargetReferenceItem->GetMemberReference(VariableReference))
		{
			FText TransactionTitle = FText::Format(LOCTEXT("FindReplaceAllTransaction", "{0} replaced with {1}"), FText::FromString(SourceProperty->GetName()), FText::FromName(VariableReference.GetMemberName()));
			const FScopedTransaction Transaction( TransactionTitle );
			BlueprintEditor.Pin()->GetBlueprintObj()->Modify();

			TArray< UBlueprint* > BlueprintsModified;
			for (TSharedPtr<class FImaginaryFiBData> ImaginaryData : InRawDataList)
			{
				BlueprintsModified.AddUnique(ImaginaryData->GetBlueprint());
				UObject* Node = ImaginaryData->GetObject(ImaginaryData->GetBlueprint());
				UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node);
				if (ensure(VariableNode))
				{
					VariableNode->Modify();
					if (VariableNode->VariableReference.IsLocalScope() || VariableNode->VariableReference.IsSelfContext())
					{
						VariableNode->VariableReference = VariableReference;
					}
					else
					{
						UBlueprint* Blueprint = BlueprintEditor.Pin()->GetBlueprintObj();
						VariableNode->VariableReference.SetFromField<UProperty>(VariableReference.ResolveMember<UProperty>(Blueprint), Blueprint->GeneratedClass);
					}
					VariableNode->ReconstructNode();
				}
			}

			for (UBlueprint* Blueprint : BlueprintsModified)
			{
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
				FFindInBlueprintSearchManager::Get().AddOrUpdateBlueprintSearchMetadata(Blueprint);
			}
			FindInBlueprints->MakeSearchQuery(VariableReference.GetReferenceSearchString(SourceProperty->GetOwnerClass()), false, ESearchQueryFilter::NodesFilter, EFiBVersion::FIB_VER_VARIABLE_REFERENCE, FOnSearchComplete());
		}
	}
}

void SReplaceNodeReferences::OnSelectionChanged(FTreeViewItem Selection, ESelectInfo::Type SelectInfo)
{
	// When the user is navigating, do not act upon the selection change
	if(SelectInfo == ESelectInfo::OnNavigation || (Selection.IsValid() && Selection->IsCategory()))
	{
		return;
	}
	
	SelectedTargetReferenceItem = Selection;
	TargetReferencesComboBox->SetIsOpen(false);
}

FText SReplaceNodeReferences::GetSourceDisplayText() const
{
	if (SourceProperty == nullptr)
	{
		return FText::FromString(TEXT("Hello World!"));
	}
	return FText::FromString(SourceProperty->GetName());
}

const FSlateBrush* SReplaceNodeReferences::GetSourceReferenceIcon() const
{
	return FBlueprintEditorUtils::GetIconFromPin(SourcePinType);
}

FSlateColor SReplaceNodeReferences::GetSourceReferenceIconColor() const
{
	UEdGraphSchema_K2 const* K2Schema = GetDefault<UEdGraphSchema_K2>();
	return K2Schema->GetPinTypeColor(SourcePinType);
}

FText SReplaceNodeReferences::GetTargetDisplayText() const
{
	FText ReturnText = LOCTEXT("UnselectedTargetReference", "Please select a target reference!");

	if (SelectedTargetReferenceItem.IsValid())
	{
		ReturnText = SelectedTargetReferenceItem->GetDisplayTitle();
	}
	return ReturnText;
}

const FSlateBrush* SReplaceNodeReferences::GetTargetIcon() const
{
	const FSlateBrush* ReturnBrush = nullptr;

	if (SelectedTargetReferenceItem.IsValid())
	{
		ReturnBrush = SelectedTargetReferenceItem->GetIcon();
	}
	return ReturnBrush;
}

FSlateColor SReplaceNodeReferences::GetTargetIconColor() const
{
	FSlateColor ReturnColor = FLinearColor::White;

	if (SelectedTargetReferenceItem.IsValid())
	{
		ReturnColor = SelectedTargetReferenceItem->GetIconColor();
	}
	return ReturnColor;
}

#undef LOCTEXT_NAMESPACE
