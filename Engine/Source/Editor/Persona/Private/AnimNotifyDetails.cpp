// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNotifyDetails.h"
#include "Widgets/Text/STextBlock.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimationAsset.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "Animation/AnimMontage.h"
#include "Animation/EditorNotifyObject.h"
#include "AssetSearchBoxUtilPersona.h"
#include "Widgets/Input/STextComboBox.h"

TSharedRef<IDetailCustomization> FAnimNotifyDetails::MakeInstance()
{
	return MakeShareable( new FAnimNotifyDetails );
}

void FAnimNotifyDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	const UClass* DetailObjectClass = nullptr;
	const UClass* BaseClass = DetailBuilder.GetBaseClass();
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	TArray<UClass*> NotifyClasses;
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);

	check(SelectedObjects.Num() > 0);
	UEditorNotifyObject* EditorObject = Cast<UEditorNotifyObject>(SelectedObjects[0].Get());
	check(EditorObject);
	UpdateSlotNames(EditorObject->AnimObject);

	TSharedRef<IPropertyHandle> EventHandle = DetailBuilder.GetProperty(TEXT("Event"));
	IDetailCategoryBuilder& EventCategory = DetailBuilder.EditCategory(TEXT("Category"));
	EventCategory.AddProperty(EventHandle).OverrideResetToDefault(FResetToDefaultOverride::Hide());

	// Hide notify objects that aren't set
	UObject* NotifyPtr = nullptr;
	FString NotifyClassName;
	TSharedRef<IPropertyHandle> NotifyPropHandle = DetailBuilder.GetProperty(TEXT("Event.Notify"));
	NotifyPropHandle->GetValue(NotifyPtr);
	
	// Don't want to edit the notify name here.
	DetailBuilder.HideProperty(TEXT("Event.NotifyName"));

	IDetailCategoryBuilder& AnimNotifyCategory = DetailBuilder.EditCategory(TEXT("AnimNotify"), FText::GetEmpty(), ECategoryPriority::TypeSpecific);

	// Check existence of notify, get rid of the property if not set
	if(!NotifyPtr)
	{
		DetailBuilder.HideProperty(TEXT("Event.Notify"));

		NotifyPropHandle = DetailBuilder.GetProperty(TEXT("Event.NotifyStateClass"));
		NotifyPropHandle->GetValue(NotifyPtr);

		// Check existence of notify state, get rid of the property if not set
		if(!NotifyPtr)
		{
			DetailBuilder.HideProperty(TEXT("Event.NotifyStateClass"));
			DetailBuilder.HideProperty(TEXT("Event.EndLink"));
		}
		else
		{
			DetailObjectClass = NotifyPtr->GetClass();

			// Get rid of the class selector in the details panel. It's not necessary for notifies
			ClearInstancedSelectionDropDown(AnimNotifyCategory, NotifyPropHandle);
		}
	}
	else
	{
		DetailObjectClass = NotifyPtr->GetClass();
		// Get rid of the class selector in the details panel. It's not necessary for notifies
		ClearInstancedSelectionDropDown(AnimNotifyCategory, NotifyPropHandle);

		// No state present, hide the entry
		DetailBuilder.HideProperty(TEXT("Event.NotifyStateClass"));
	}

	UAnimMontage* CurrentMontage = Cast<UAnimMontage>(EditorObject->AnimObject);

	// If we have a montage, and it has slots (which it should have) generate custom link properties
	if(CurrentMontage && CurrentMontage->SlotAnimTracks.Num() > 0)
	{
		CustomizeLinkProperties(DetailBuilder, EventHandle, EditorObject);
	}
	else
	{
		// No montage, hide link properties
		HideLinkProperties(DetailBuilder, EventHandle);
	}

	// Customizations do not run for instanced properties, so we have to resolve the properties and then
	// customize them here instead.
	if(NotifyPropHandle->IsValidHandle())
	{
		uint32 NumChildren = 0;
		NotifyPropHandle->GetNumChildren(NumChildren);
		if(NumChildren > 0)
		{
			TSharedPtr<IPropertyHandle> BaseHandle = NotifyPropHandle->GetChildHandle(0);
			DetailBuilder.HideProperty(NotifyPropHandle);

			BaseHandle->GetNumChildren(NumChildren);
			DetailBuilder.HideProperty(BaseHandle);

			for(uint32 ChildIdx = 0 ; ChildIdx < NumChildren ; ++ChildIdx)
			{
				TSharedPtr<IPropertyHandle> NotifyProperty = BaseHandle->GetChildHandle(ChildIdx);
				UProperty* Prop = NotifyProperty->GetProperty();
				
				if(Prop && !Prop->HasAnyPropertyFlags(CPF_DisableEditOnInstance))
				{
					if(!CustomizeProperty(AnimNotifyCategory, NotifyPtr, NotifyProperty))
					{
						AnimNotifyCategory.AddProperty(NotifyProperty);
					}
				}
			}
		}
	}

	struct FPropVisPair
	{
		const TCHAR* NotifyName;
		TAttribute<EVisibility> Visibility;
	};

	TriggerFilterModeHandle = DetailBuilder.GetProperty(TEXT("Event.NotifyFilterType"));

	FPropVisPair TriggerSettingNames[] = { { TEXT("Event.NotifyTriggerChance"), TAttribute<EVisibility>(EVisibility::Visible) }
										 , { TEXT("Event.bTriggerOnDedicatedServer"), TAttribute<EVisibility>(EVisibility::Visible) }
										 , { TEXT("Event.NotifyFilterType"), TAttribute<EVisibility>(EVisibility::Visible) }
										 , { TEXT("Event.NotifyFilterLOD"), TAttribute<EVisibility>(this, &FAnimNotifyDetails::VisibilityForLODFilterMode) } };


	IDetailCategoryBuilder& TriggerSettingCategory = DetailBuilder.EditCategory(TEXT("Trigger Settings"), FText::GetEmpty(), ECategoryPriority::TypeSpecific);

	for (FPropVisPair& NotifyPair : TriggerSettingNames)
	{
		TSharedRef<IPropertyHandle> NotifyPropertyHandle = DetailBuilder.GetProperty(NotifyPair.NotifyName);
		DetailBuilder.HideProperty(NotifyPropertyHandle);
		TriggerSettingCategory.AddProperty(NotifyPropertyHandle).Visibility(NotifyPair.Visibility);
	}
}

EVisibility FAnimNotifyDetails::VisibilityForLODFilterMode() const
{
	uint8 FilterModeValue = 0;
	FPropertyAccess::Result Ret = TriggerFilterModeHandle.Get()->GetValue(FilterModeValue);
	if (Ret == FPropertyAccess::Result::Success)
	{
		return (FilterModeValue == ENotifyFilterType::LOD) ? EVisibility::Visible : EVisibility::Hidden;
	}

	return EVisibility::Hidden; //Hidden if we get fail or MultipleValues from the property
}

void FAnimNotifyDetails::AddBoneNameProperty(IDetailCategoryBuilder& CategoryBuilder, UObject* Notify,  TSharedPtr<IPropertyHandle> Property)
{
	int32 PropIndex = NameProperties.Num();

	if(Notify && Property->IsValidHandle())
	{
		NameProperties.Add(Property);
		// get all the possible suggestions for the bones and sockets.
		if(const UAnimationAsset* AnimAsset = Cast<const UAnimationAsset>(Notify->GetOuter()))
		{
			if(const USkeleton* Skeleton = AnimAsset->GetSkeleton())
			{
				CategoryBuilder.AddProperty(Property.ToSharedRef())
					.CustomWidget()
					.NameContent()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.Padding(FMargin(2, 1, 0, 1))
						[
							SNew(STextBlock)
							.Text(Property->GetPropertyDisplayName())
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						]
					]
				.ValueContent()
					[
						SNew(SAssetSearchBoxForBones, Skeleton, Property)
						.IncludeSocketsForSuggestions(true)
						.MustMatchPossibleSuggestions(false)
						.HintText(NSLOCTEXT("AnimNotifyDetails", "Hint Text", "Bone Name..."))
						.OnTextCommitted(this, &FAnimNotifyDetails::OnSearchBoxCommitted, PropIndex)
					];
			}
		}
	}
}

void FAnimNotifyDetails::AddCurveNameProperty(IDetailCategoryBuilder& CategoryBuilder, UObject* Notify, TSharedPtr<IPropertyHandle> Property)
{
	int32 PropIndex = NameProperties.Num();
	
	if(Notify && Property->IsValidHandle())
	{
		NameProperties.Add(Property);

		if(const UAnimationAsset* AnimAsset = Cast<const UAnimationAsset>(Notify->GetOuter()))
		{
			if(const USkeleton* Skeleton = AnimAsset->GetSkeleton())
			{
				CategoryBuilder.AddProperty(Property.ToSharedRef())
					.CustomWidget()
					.NameContent()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.Padding(FMargin(2, 1, 0, 1))
						[
							SNew(STextBlock)
							.Text(Property->GetPropertyDisplayName())
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						]
					]
				.ValueContent()
					[
						SNew(SAssetSearchBoxForCurves, Skeleton, Property)
						.IncludeSocketsForSuggestions(true)
						.MustMatchPossibleSuggestions(true)
						.HintText(NSLOCTEXT("AnimNotifyDetails", "Curve Name Hint Text", "Curve Name..."))
						.OnTextCommitted(this, &FAnimNotifyDetails::OnSearchBoxCommitted, PropIndex)
					];
			}
		}
	}
}

void FAnimNotifyDetails::OnSearchBoxCommitted(const FText& InSearchText, ETextCommit::Type CommitInfo, int32 PropertyIndex )
{
	NameProperties[PropertyIndex]->SetValue( InSearchText.ToString() );
}

void FAnimNotifyDetails::ClearInstancedSelectionDropDown(IDetailCategoryBuilder& CategoryBuilder, TSharedRef<IPropertyHandle> PropHandle, bool bShowChildren /*= true*/)
{
	IDetailPropertyRow& PropRow = CategoryBuilder.AddProperty(PropHandle);
	
	PropRow
	.OverrideResetToDefault(FResetToDefaultOverride::Hide())
	.CustomWidget(bShowChildren)
	.NameContent()
	[
		PropHandle->CreatePropertyNameWidget(FText::GetEmpty(), FText::GetEmpty(), false)
	]
	.ValueContent()
	[
		SNullWidget::NullWidget
	];
}

void FAnimNotifyDetails::CustomizeLinkProperties(IDetailLayoutBuilder& Builder, TSharedRef<IPropertyHandle> NotifyProperty, UEditorNotifyObject* EditorObject)
{
	uint32 NumChildProperties = 0;
	NotifyProperty->GetNumChildren(NumChildProperties);

	if(NumChildProperties > 0)
	{
		IDetailCategoryBuilder& LinkCategory = Builder.EditCategory(TEXT("AnimLink"));
		for(uint32 ChildIdx = 0 ; ChildIdx < NumChildProperties ; ++ChildIdx)
		{
			TSharedPtr<IPropertyHandle> ChildHandle = NotifyProperty->GetChildHandle(ChildIdx);
			FString OuterFieldType = ChildHandle->GetProperty()->GetOuterUField()->GetName();

			if(ChildHandle->GetProperty()->GetName() == GET_MEMBER_NAME_CHECKED(FAnimNotifyEvent, EndLink).ToString()
			   || OuterFieldType == FString(TEXT("AnimLinkableElement")))
			{
				// If we get a slot index property replace it with a dropdown showing the names of the 
				// slots, as the indices are hidden from the user.
				if(ChildHandle->GetProperty()->GetName() == TEXT("SlotIndex"))
				{
					int32 SlotIdx = INDEX_NONE;
					ChildHandle->GetValue(SlotIdx);

					LinkCategory.AddProperty(ChildHandle)
						.CustomWidget()
						.NameContent()
						[
							ChildHandle->CreatePropertyNameWidget(NSLOCTEXT("NotifyDetails", "SlotIndexName", "Slot"))
						]
						.ValueContent()
						[
							SNew(STextComboBox)
							.OptionsSource(&SlotNameItems)
							.OnSelectionChanged(this, &FAnimNotifyDetails::OnSlotSelected, ChildHandle)
							.OnComboBoxOpening(this, &FAnimNotifyDetails::UpdateSlotNames, EditorObject->AnimObject)
							.InitiallySelectedItem(SlotNameItems[SlotIdx])
						];
				}
				else
				{
					LinkCategory.AddProperty(ChildHandle);
				}
			}
		}
	}
}

void FAnimNotifyDetails::HideLinkProperties(IDetailLayoutBuilder& Builder, TSharedRef<IPropertyHandle> NotifyProperty)
{
	uint32 NumChildProperties = 0;
	NotifyProperty->GetNumChildren(NumChildProperties);

	if(NumChildProperties > 0)
	{
		for(uint32 ChildIdx = 0 ; ChildIdx < NumChildProperties ; ++ChildIdx)
		{
			TSharedPtr<IPropertyHandle> ChildHandle = NotifyProperty->GetChildHandle(ChildIdx);
			FString OuterFieldType = ChildHandle->GetProperty()->GetOuterUField()->GetName();
			if(ChildHandle->GetProperty()->GetName() == GET_MEMBER_NAME_CHECKED(FAnimNotifyEvent, EndLink).ToString()
			   || OuterFieldType == FString(TEXT("AnimLinkableElement")))
			{
				Builder.HideProperty(ChildHandle);
			}
		}
	}
}

bool FAnimNotifyDetails::CustomizeProperty(IDetailCategoryBuilder& CategoryBuilder, UObject* Notify, TSharedPtr<IPropertyHandle> Property)
{
	if(Notify && Notify->GetClass() && Property->IsValidHandle())
	{
		FString ClassName = Notify->GetClass()->GetName();
		FString PropertyName = Property->GetProperty()->GetName();

		if(ClassName.Find(TEXT("AnimNotify_PlayParticleEffect")) != INDEX_NONE && PropertyName == TEXT("SocketName"))
		{
			AddBoneNameProperty(CategoryBuilder, Notify, Property);
			return true;
		}
		else if(ClassName.Find(TEXT("AnimNotifyState_TimedParticleEffect")) != INDEX_NONE && PropertyName == TEXT("SocketName"))
		{
			AddBoneNameProperty(CategoryBuilder, Notify, Property);
			return true;
		}
		else if(ClassName.Find(TEXT("AnimNotify_PlaySound")) != INDEX_NONE && PropertyName == TEXT("AttachName"))
		{
			AddBoneNameProperty(CategoryBuilder, Notify, Property);
			return true;
		}
		else if (ClassName.Find(TEXT("AnimNotifyState_Trail")) != INDEX_NONE)
		{
			if(PropertyName == TEXT("FirstSocketName") || PropertyName == TEXT("SecondSocketName"))
			{
				AddBoneNameProperty(CategoryBuilder, Notify, Property);
				return true;
			}
			else if(PropertyName == TEXT("WidthScaleCurve"))
			{
				AddCurveNameProperty(CategoryBuilder, Notify, Property);
				return true;
			}
		}
	}
	return false;
}

void FAnimNotifyDetails::UpdateSlotNames(UAnimSequenceBase* AnimObject)
{
	if(UAnimMontage* MontageObj = Cast<UAnimMontage>(AnimObject))
	{
		for(FSlotAnimationTrack& Slot : MontageObj->SlotAnimTracks)
		{
			if(!SlotNameItems.ContainsByPredicate([&Slot](TSharedPtr<FString>& Item){return Slot.SlotName.ToString() == *Item;}))
			{
				SlotNameItems.Add(MakeShareable(new FString(*Slot.SlotName.ToString()))); 
			}
		}
	}
}

void FAnimNotifyDetails::OnSlotSelected(TSharedPtr<FString> SlotName, ESelectInfo::Type SelectInfo, TSharedPtr<IPropertyHandle> Property)
{
	if(SelectInfo != ESelectInfo::Direct && Property->IsValidHandle())
	{
		int32 NewIndex = SlotNameItems.Find(SlotName);
		if(NewIndex != INDEX_NONE)
		{
			Property->SetValue(NewIndex);
		}
	}
}
