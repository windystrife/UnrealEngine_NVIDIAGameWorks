// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IntegralKeyDetailsCustomization.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateTypes.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SCheckBox.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Curves/IntegralCurve.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneParticleSection.h"
#include "MovieSceneToolHelpers.h"

#define LOCTEXT_NAMESPACE "IntegralKeyDetailsCustomization"

TSharedRef<class IDetailCustomization> FIntegralKeyDetailsCustomization::MakeInstance(TWeakObjectPtr<const UMovieSceneSection> InSection)
{
	return MakeShareable(new FIntegralKeyDetailsCustomization(InSection));
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FIntegralKeyDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	if(Section.IsValid())
	{
		TSharedPtr<IPropertyHandle> ValueProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(FIntegralKey, Value));
		if(ValueProperty.IsValid())
		{
			TSharedPtr<SWidget> ValueWidget = nullptr;

			if(Section->IsA<UMovieSceneBoolSection>())
			{
				ValueWidget = SNew(SCheckBox)
					.IsChecked_Lambda([ValueProperty]()
					{
						int32 Value = 0;
						if(ValueProperty->GetValue(Value) == FPropertyAccess::Success)
						{
							return (Value != 0) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						}

						return ECheckBoxState::Undetermined;
					})
					.OnCheckStateChanged_Lambda([ValueProperty](ECheckBoxState InState) 
					{ 
						int32 Value = (InState == ECheckBoxState::Checked) ? 1 : 0;
						ValueProperty->SetValue(Value);
					});
			}
			else if(Section->IsA<UMovieSceneParticleSection>())
			{
				UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EParticleKey"), true);
				check(Enum);

				ValueWidget = MovieSceneToolHelpers::MakeEnumComboBox(
					Enum, 
					TAttribute<int32>::Create(TAttribute<int32>::FGetter::CreateLambda([ValueProperty]() 
					{ 
						int32 Value = 0;
						ValueProperty->GetValue(Value);
						return Value;
					})),
					FOnEnumSelectionChanged::CreateLambda([ValueProperty](int32 Selection, ESelectInfo::Type SelectionType)
					{
						ValueProperty->SetValue(Selection);
					}));
			}
			else
			{
				ValueWidget = ValueProperty->CreatePropertyValueWidget();
			}

			DetailBuilder.EditCategory("Key")
				.AddProperty("Value")
				.CustomWidget()
				.NameContent()
				[
					ValueProperty->CreatePropertyNameWidget()
				]
				.ValueContent()
				[
					ValueWidget.ToSharedRef()
				];
		}
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
