// @third party code - BEGIN HairWorks
#include "HairWorksDetails.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailGroup.h"
#include "Engine/HairWorksMaterial.h"
#include "Engine/HairWorksAsset.h"
#include "Components/HairWorksComponent.h"

TSharedRef<IDetailCustomization> FHairWorksMaterialDetails::MakeInstance()
{
	return MakeShareable(new FHairWorksMaterialDetails);
}

void FHairWorksMaterialDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Build category tree.
	struct FCategory
	{
		TSet<FName> Properties;
		TMap<FName, TUniqueObj<FCategory>> Categories;
	};

	FCategory TopCategory;

	for(TFieldIterator<UProperty> PropIt(UHairWorksMaterial::StaticClass()); PropIt; ++PropIt)
	{
		// Get category path
		auto& Property = **PropIt;

		auto& CategoryPath = Property.GetMetaData("Category");
		TArray<FString> ParsedNames;
		CategoryPath.ParseIntoArray(ParsedNames, TEXT("|"), true);

		// Find or create category path.
		auto* Category = &TopCategory;
		for(auto& CategoryName : ParsedNames)
		{
			Category = &*Category->Categories.FindOrAdd(FName(*CategoryName));
		}

		// Add property.
		Category->Properties.Add(*Property.GetNameCPP());
	}

	// Use asset's hair material as default object if possible.
	static auto GetDefaultHairMaterial = [](const UHairWorksMaterial& HairMaterial)->UHairWorksMaterial&
	{
		if(auto* HairWorksComponent = Cast<UHairWorksComponent>(HairMaterial.GetOuter()))
		{
			if(HairWorksComponent->HairInstance.Hair != nullptr)
				return *HairWorksComponent->HairInstance.Hair->HairMaterial;
		}

		return *UHairWorksMaterial::StaticClass()->GetDefaultObject<UHairWorksMaterial>();
	};

	// Show reset label 
	auto IsResetVisible = [](TSharedPtr<IPropertyHandle> PropertyHandle)
	{
		if(!PropertyHandle.IsValid() || !PropertyHandle->IsValidHandle())
			return false;

		TArray<UObject*> OuterObjects;
		PropertyHandle->GetOuterObjects(OuterObjects);

		for(auto OuterObj : OuterObjects)
		{
			auto* HairMaterial = Cast<UHairWorksMaterial>(OuterObj);
			if(HairMaterial == nullptr)
				continue;

			auto& DefaultHairMaterial = GetDefaultHairMaterial(*HairMaterial);

			auto& Property = *PropertyHandle->GetProperty();
			if(!Property.Identical_InContainer(HairMaterial, &DefaultHairMaterial))
				return true;
		}

		return false;
	};

	// Reset hair material property
	auto ResetProperty = [](TSharedPtr<IPropertyHandle> PropertyHandle)
	{
		if(!PropertyHandle.IsValid() || !PropertyHandle->IsValidHandle())
			return;

		TArray<FString> Values;
		PropertyHandle->GetPerObjectValues(Values);

		TArray<UObject*> OuterObjects;
		PropertyHandle->GetOuterObjects(OuterObjects);

		for(auto Index = 0; Index < Values.Num(); ++Index)
		{
			auto* HairMaterial = Cast<UHairWorksMaterial>(OuterObjects[Index]);
			if(HairMaterial == nullptr)
				continue;

			Values[Index].Reset();

			auto& DefaultHairMaterial = GetDefaultHairMaterial(*HairMaterial);
			PropertyHandle->GetProperty()->ExportText_InContainer(0, Values[Index], &DefaultHairMaterial, &DefaultHairMaterial, &DefaultHairMaterial, 0);
		}

		PropertyHandle->SetPerObjectValues(Values);
	};

	// Hair material can be edited only if override flag is checked
	auto IsEnabled = [](TSharedRef<IPropertyHandle> PropHandle)
	{
		if(!PropHandle->IsValidHandle())
			return true;

		TArray<UObject*> OuterObjects;
		PropHandle->GetOuterObjects(OuterObjects);

		for(auto OuterObj : OuterObjects)
		{
			auto* HairComp = Cast<UHairWorksComponent>(OuterObj->GetOuter());
			if(HairComp == nullptr)
				continue;

			if(!HairComp->HairInstance.bOverride)
				return false;
		}

		return true;
	};

	// To Bind handles
	auto AddPropertyHandler = [&](IDetailPropertyRow& DetailProperty, TSharedRef<IPropertyHandle> PropertyHandle)
	{
		DetailProperty.OverrideResetToDefault(FResetToDefaultOverride::Create(
			FIsResetToDefaultVisible::CreateStatic(IsResetVisible),
			FResetToDefaultHandler::CreateStatic(ResetProperty)
			));

		DetailProperty.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateStatic(IsEnabled, PropertyHandle)));
	};

	// Build property widgets
	for(auto& Category : TopCategory.Categories)
	{
		// Add category
		auto& CategoryBuilder = DetailBuilder.EditCategory(Category.Key, FText::GetEmpty(), ECategoryPriority::Uncommon);

		// Add properties
		for(auto& PropertyName : Category.Value->Properties)
		{
			const auto& PropHandle = DetailBuilder.GetProperty(PropertyName);
			auto& DetailProperty = CategoryBuilder.AddProperty(PropHandle);
			AddPropertyHandler(DetailProperty, PropHandle);
		}

		// Add groups
		for(auto& Group : Category.Value->Categories)
		{
			auto& DetailGroup = CategoryBuilder.AddGroup(Group.Key, FText::FromName(Group.Key));

			// Add properties
			for(auto& PropertyName : Group.Value->Properties)
			{
				const auto& PropertyHandle = DetailBuilder.GetProperty(PropertyName);

				auto& DetailProperty = DetailGroup.AddPropertyRow(PropertyHandle);
				AddPropertyHandler(DetailProperty, PropertyHandle);

				// Spacial case for pins.
				if(PropertyHandle->GetProperty()->GetNameCPP() == "Pins")
				{
					// Pin array should not be reseted
					DetailProperty.OverrideResetToDefault(FResetToDefaultOverride::Hide());

					// Pins should be edited only in assets.
					TArray<UObject*> OuterObjects;
					PropertyHandle->GetOuterObjects(OuterObjects);

					auto IsNotInAsset = [](const auto* OuterObj)
					{
						if(auto* HairMaterial = Cast<UHairWorksMaterial>(OuterObj))
						{
							return !HairMaterial->GetOuter()->IsA<UHairWorksAsset>();
						}

						return false;
					};

					if(OuterObjects.FindByPredicate(IsNotInAsset) != nullptr)
					{
						DetailProperty.IsEnabled(false);
						break;
					}

					// In asset, pin should not be reseted. Because pin bone name should not be reseted
					uint32 ChildNum = 0;
					PropertyHandle->GetNumChildren(ChildNum);
					for(uint32 Index = 0; Index < ChildNum; ++Index)
					{
						const auto& PinHandle = PropertyHandle->GetChildHandle(Index);
						DetailGroup.AddPropertyRow(PinHandle.ToSharedRef()).OverrideResetToDefault(FResetToDefaultOverride::Hide());
					}
				}
			}
		}
	}
}

TSharedRef<IDetailCustomization> FHairWorksComponentDetails::MakeInstance()
{
	return MakeShareable(new FHairWorksComponentDetails);
}

void FHairWorksComponentDetails::CustomizeDetails(IDetailLayoutBuilder & DetailBuilder)
{
	// When a hair asset is assigned, copy hair material from assets to components
	auto OnHairAssetChanged = [](TSharedRef<IPropertyHandle> HairMaterialPropHandle)
	{
		// Set each property of hair material
		if(!HairMaterialPropHandle->IsValidHandle())
			return;

		for(TFieldIterator<UProperty> PropIt(UHairWorksMaterial::StaticClass()); PropIt; ++PropIt)
		{
			auto* Property = *PropIt;

			const auto& PropHandle = HairMaterialPropHandle->GetChildHandle(FName(*Property->GetName()));
			check(PropHandle->IsValidHandle());

			// Get value from hair material in hair asset
			TArray<UObject*> OuterObjects;
			PropHandle->GetOuterObjects(OuterObjects);
			TArray<FString> Values;
			PropHandle->GetPerObjectValues(Values);

			for(auto Index = 0; Index < Values.Num(); ++Index)
			{
				auto* HairMaterial = Cast<UHairWorksMaterial>(OuterObjects[Index]);
				if(HairMaterial == nullptr)
					continue;

				auto* HairComp = Cast<UHairWorksComponent>(HairMaterial->GetOuter());
				if(HairComp == nullptr)
					continue;

				if(HairComp->HairInstance.Hair == nullptr)
					continue;

				auto* HairAssetMaterial = HairComp->HairInstance.Hair->HairMaterial;
				Values[Index].Reset();
				Property->ExportText_InContainer(0, Values[Index], HairAssetMaterial, HairAssetMaterial, HairAssetMaterial, 0);
			}

			// Set to property
			PropHandle->SetPerObjectValues(Values);
		}
	};

	const auto& HairAssetPropHandle = DetailBuilder.GetProperty("HairInstance.Hair");
	const auto& HairMaterialPropHandle = DetailBuilder.GetProperty("HairInstance.HairMaterial");
	HairAssetPropHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateStatic<>(OnHairAssetChanged, HairMaterialPropHandle));
}
// @third party code - END HairWorks
