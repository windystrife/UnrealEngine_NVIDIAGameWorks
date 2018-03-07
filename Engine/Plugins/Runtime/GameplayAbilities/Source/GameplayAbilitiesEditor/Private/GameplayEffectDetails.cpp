// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayEffectDetails.h"
#include "UObject/UnrealType.h"
#include "DetailLayoutBuilder.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffect.h"
#include "GameplayEffectTemplate.h"

#define LOCTEXT_NAMESPACE "GameplayEffectDetailsCustomization"

DEFINE_LOG_CATEGORY(LogGameplayEffectDetails);

// --------------------------------------------------------FGameplayEffectDetails---------------------------------------

TSharedRef<IDetailCustomization> FGameplayEffectDetails::MakeInstance()
{
	return MakeShareable(new FGameplayEffectDetails);
}

void FGameplayEffectDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	MyDetailLayout = &DetailLayout;

	TArray< TWeakObjectPtr<UObject> > Objects;
	DetailLayout.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		// I don't know what to do here or what could be expected. Just return to disable all templating functionality
		return;
	}

	TemplateProperty = DetailLayout.GetProperty(FName("Template"), UGameplayEffect::StaticClass());
	ShowAllProperty = DetailLayout.GetProperty(FName("ShowAllProperties"), UGameplayEffect::StaticClass());

	FSimpleDelegate UpdateShowAllDelegate = FSimpleDelegate::CreateSP(this, &FGameplayEffectDetails::OnShowAllChange);
	ShowAllProperty->SetOnPropertyValueChanged(UpdateShowAllDelegate);

	FSimpleDelegate UpdatTemplateDelegate = FSimpleDelegate::CreateSP(this, &FGameplayEffectDetails::OnTemplateChange);
	TemplateProperty->SetOnPropertyValueChanged(UpdatTemplateDelegate);

	TSharedPtr<IPropertyHandle> DurationPolicyProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGameplayEffect, DurationPolicy), UGameplayEffect::StaticClass());
	FSimpleDelegate UpdateDurationPolicyDelegate = FSimpleDelegate::CreateSP(this, &FGameplayEffectDetails::OnDurationPolicyChange);
	DurationPolicyProperty->SetOnPropertyValueChanged(UpdateDurationPolicyDelegate);
	
	// Hide properties where necessary
	UGameplayEffect* Obj = Cast<UGameplayEffect>(Objects[0].Get());
	if (Obj)
	{
		if ( !Obj->ShowAllProperties )
		{
			UGameplayEffectTemplate* Template = Obj->Template;

			if (Template)
			{
				UGameplayEffect* DefObj = Obj->Template->GetClass()->GetDefaultObject<UGameplayEffect>();

				for (TFieldIterator<UProperty> PropIt(UGameplayEffect::StaticClass(), EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
				{
					UProperty* Property = *PropIt;
					TSharedPtr<IPropertyHandle> PropHandle = DetailLayout.GetProperty(Property->GetFName());
					HideProperties(DetailLayout, PropHandle, Template);
				}
			}
		}

		if (Obj->DurationPolicy != EGameplayEffectDurationType::HasDuration)
		{
			TSharedPtr<IPropertyHandle> DurationMagnitudeProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGameplayEffect, DurationMagnitude), UGameplayEffect::StaticClass());
			DetailLayout.HideProperty(DurationMagnitudeProperty);
		}

		if (Obj->DurationPolicy == EGameplayEffectDurationType::Instant)
		{
			TSharedPtr<IPropertyHandle> PeriodProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGameplayEffect, Period), UGameplayEffect::StaticClass());
			TSharedPtr<IPropertyHandle> ExecutePeriodicEffectOnApplicationProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGameplayEffect, bExecutePeriodicEffectOnApplication), UGameplayEffect::StaticClass());
			DetailLayout.HideProperty(PeriodProperty);
			DetailLayout.HideProperty(ExecutePeriodicEffectOnApplicationProperty);
		}

		// The modifier array needs to be told to specifically hide evaluation channel settings for instant effects, as they do not factor evaluation channels at all
		// and instead only operate on base values. To that end, mark the instance metadata so that the customization for the evaluation channel is aware it has to hide
		// (see FGameplayModEvaluationChannelSettingsDetails for handling)
		TSharedPtr<IPropertyHandle> ModifierArrayProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGameplayEffect, Modifiers), UGameplayEffect::StaticClass());
		if (ModifierArrayProperty.IsValid() && ModifierArrayProperty->IsValidHandle())
		{
			FString ForceHideMetadataValue;
			if (Obj->DurationPolicy == EGameplayEffectDurationType::Instant)
			{
				ForceHideMetadataValue = FGameplayModEvaluationChannelSettings::ForceHideMetadataEnabledValue;
			}
			ModifierArrayProperty->SetInstanceMetaData(FGameplayModEvaluationChannelSettings::ForceHideMetadataKey, ForceHideMetadataValue);
		}
	}
}

/** Recursively hide properties that are not default editable according to the Template */
bool FGameplayEffectDetails::HideProperties(IDetailLayoutBuilder& DetailLayout, TSharedPtr<IPropertyHandle> PropHandle, UGameplayEffectTemplate* Template)
{
	UProperty* UProp = PropHandle->GetProperty();

	// Never hide the Template or ShowAllProperties properties
	if (TemplateProperty->GetProperty() == UProp || ShowAllProperty->GetProperty() == UProp)
	{
		return false;
	}

	// Don't hide the EditableProperties
	for (FString MatchStr : Template->EditableProperties)
	{
		if (MatchStr.Equals(UProp->GetName(), ESearchCase::IgnoreCase))
		{			
			return false;
		}
	}
	
	// Recurse into children - if they are all hidden then we are hidden.
	uint32 NumChildren = 0;
	PropHandle->GetNumChildren(NumChildren);

	bool AllChildrenHidden = true;
	for (uint32 ChildIdx = 0; ChildIdx < NumChildren; ++ChildIdx)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = PropHandle->GetChildHandle(ChildIdx);
		AllChildrenHidden &= HideProperties(DetailLayout, ChildHandle, Template);
	}

	if (AllChildrenHidden)
	{
		DetailLayout.HideProperty(PropHandle);
		return true;
	}

	return false;
}

void FGameplayEffectDetails::OnShowAllChange()
{
	MyDetailLayout->ForceRefreshDetails();
}

void FGameplayEffectDetails::OnTemplateChange()
{
	TArray< TWeakObjectPtr<UObject> > Objects;
	MyDetailLayout->GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		return;
	}

	UGameplayEffect* Obj = Cast<UGameplayEffect>(Objects[0].Get());
	UGameplayEffect* Template = Obj->Template;

	if (Template)
	{
		// Copy any non-default properties from the template into the current editable object
		UGameplayEffect* DefObj = Template->GetClass()->GetDefaultObject<UGameplayEffect>();
		for (TFieldIterator<UProperty> PropIt(UGameplayEffect::StaticClass(), EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
		{
			UProperty* Property = *PropIt;
			// don't overwrite the template property
			if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UGameplayEffect, Template))
			{
				continue;
			}
			if (!Property->Identical_InContainer(Template, DefObj))
			{
				Property->CopyCompleteValue_InContainer(Obj, Template);
			}

			// Default to only showing template properties after changing template type
			Obj->ShowAllProperties = false;
		}
	}

	MyDetailLayout->ForceRefreshDetails();
}

void FGameplayEffectDetails::OnDurationPolicyChange()
{
	MyDetailLayout->ForceRefreshDetails();
}


//-------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE
