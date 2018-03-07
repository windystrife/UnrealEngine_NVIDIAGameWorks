// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PaintModeSettingsCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "STextBlock.h"
#include "PropertyRestriction.h"

#include "Engine/Texture2D.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SCheckBox.h"

#include "PaintModeSettings.h"
#include "PaintModePainter.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "PropertyCustomizationHelpers.h"

TSharedRef<IPropertyTypeCustomization> FTexturePaintSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FTexturePaintSettingsCustomization());
}

void FTexturePaintSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	static const TArray<FName> CustomProperties = { GET_MEMBER_NAME_CHECKED(FTexturePaintSettings, PaintTexture), GET_MEMBER_NAME_CHECKED(FTexturePaintSettings, UVChannel), GET_MEMBER_NAME_CHECKED(FTexturePaintSettings, bWriteRed), GET_MEMBER_NAME_CHECKED(FTexturePaintSettings, bWriteBlue), GET_MEMBER_NAME_CHECKED(FTexturePaintSettings, bWriteAlpha), GET_MEMBER_NAME_CHECKED(FTexturePaintSettings, bWriteGreen) };
	TMap<FName, TSharedRef<IPropertyHandle>> CustomizedProperties;

	/** Caches paint mode painter instance */
	MeshPainter = FPaintModePainter::Get();
	// Cache vertex paint settings ptr
	PaintSettings = &(UPaintModeSettings::Get()->TexturePaintSettings);

	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);

	/** Add child properties except of paint texture property (needs customization)*/
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();

		const int32 ArrayIndex = CustomProperties.IndexOfByKey(ChildHandle->GetProperty()->GetFName());

		if (ArrayIndex == INDEX_NONE)
		{
			IDetailPropertyRow& Property = ChildBuilder.AddProperty(ChildHandle);			
		}
		else
		{
			CustomizedProperties.Add(ChildHandle->GetProperty()->GetFName(), ChildHandle);
		}
	}

	TSharedRef<IPropertyHandle> RedChannel = CustomizedProperties.FindChecked(GET_MEMBER_NAME_CHECKED(FTexturePaintSettings, bWriteRed));
	TSharedRef<IPropertyHandle> GreenChannel = CustomizedProperties.FindChecked(GET_MEMBER_NAME_CHECKED(FTexturePaintSettings, bWriteGreen));
	TSharedRef<IPropertyHandle> BlueChannel = CustomizedProperties.FindChecked(GET_MEMBER_NAME_CHECKED(FTexturePaintSettings, bWriteBlue));
	TSharedRef<IPropertyHandle> AlphaChannel = CustomizedProperties.FindChecked(GET_MEMBER_NAME_CHECKED(FTexturePaintSettings, bWriteAlpha));
	TArray<TSharedRef<IPropertyHandle>> Channels = { RedChannel, GreenChannel, BlueChannel, AlphaChannel };
	TSharedPtr<SHorizontalBox> ChannelsWidget;

	ChildBuilder.AddCustomRow(NSLOCTEXT("ColorMask", "ChannelLabel", "Channels"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("VertexPaintSettings", "ChannelsLabel", "Channels"))
			.ToolTipText(NSLOCTEXT("VertexPaintSettings", "ChannelsToolTip", "Colors Channels which should be influenced during Painting."))
			.Font(CustomizationUtils.GetRegularFont())
			]
			.ValueContent()
			.MaxDesiredWidth(250.0f)
			[
				SAssignNew(ChannelsWidget, SHorizontalBox)
			];

	for (TSharedRef<IPropertyHandle> Channel : Channels)
	{
		ChannelsWidget->AddSlot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				CreateColorChannelWidget(Channel)
			];
	}


	if (CustomizedProperties.Find(GET_MEMBER_NAME_CHECKED(FTexturePaintSettings, UVChannel)))
	{
		TSharedRef<IPropertyHandle> UVChannel = CustomizedProperties.FindChecked(GET_MEMBER_NAME_CHECKED(FTexturePaintSettings, UVChannel));

		ChildBuilder.AddCustomRow(NSLOCTEXT("TexturePainting", "TexturePaintingUVLabel", "Texture Painting UV Channel"))
		.NameContent()
		[
			UVChannel->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)			
			[
				SNew(SNumericEntryBox<int32>)
				.Font(CustomizationUtils.GetRegularFont())	
				.AllowSpin(true)
				.Value_Lambda([=]() -> int32 { return PaintSettings->UVChannel; })
				.MinValue(0)
				.MaxValue_Lambda([=]() -> int32 { return FPaintModePainter::Get()->GetMaxUVIndexToPaint(); })
				.OnValueChanged(SNumericEntryBox<int32>::FOnValueChanged::CreateLambda([=](int32 Value) { PaintSettings->UVChannel = Value; }))
				.OnValueCommitted(SNumericEntryBox<int32>::FOnValueCommitted::CreateLambda([=](int32 Value, ETextCommit::Type CommitType) { PaintSettings->UVChannel = Value; }))
			]
		];
	}

	/** If we have a valid texture property handle add custom UI for it */
	if (CustomizedProperties.Find(GET_MEMBER_NAME_CHECKED(FTexturePaintSettings, PaintTexture)))
	{
		TSharedRef<IPropertyHandle> TextureProperty = CustomizedProperties.FindChecked(GET_MEMBER_NAME_CHECKED(FTexturePaintSettings, PaintTexture));
		TSharedPtr<SHorizontalBox> TextureWidget;
		FDetailWidgetRow& Row = ChildBuilder.AddCustomRow(NSLOCTEXT("TexturePaintSetting", "TextureSearchString", "Texture"))
			.NameContent()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("TexturePaintSettings", "PaintTextureLabel", "Paint Texture"))
				.ToolTipText(NSLOCTEXT("TexturePaintSettings", "PaintTextureToolTip", "Texture to Apply Painting to."))
				.Font(CustomizationUtils.GetRegularFont())
			]
			.ValueContent()
			.MaxDesiredWidth(250.0f)
			[
				SAssignNew(TextureWidget, SHorizontalBox)
			];

			/** Use a SObjectPropertyEntryBox to benefit from its functionality */
			TextureWidget->AddSlot()
			[
				SNew(SObjectPropertyEntryBox)
				.PropertyHandle(TextureProperty)
				.AllowedClass(UTexture2D::StaticClass())
				.OnShouldFilterAsset(FOnShouldFilterAsset::CreateRaw(MeshPainter, &FPaintModePainter::ShouldFilterTextureAsset))
				.OnObjectChanged(FOnSetObject::CreateRaw(MeshPainter, &FPaintModePainter::PaintTextureChanged))
				.DisplayUseSelected(false)
				.ThumbnailPool(CustomizationUtils.GetThumbnailPool())
			];
	}
}

/** List of property names which require customization, will be filtered out of property */
TArray<FName> FVertexPaintSettingsCustomization::CustomPropertyNames = { GET_MEMBER_NAME_CHECKED(FVertexPaintSettings, bWriteRed), GET_MEMBER_NAME_CHECKED(FVertexPaintSettings, bWriteGreen), GET_MEMBER_NAME_CHECKED(FVertexPaintSettings, bWriteBlue), GET_MEMBER_NAME_CHECKED(FVertexPaintSettings, bWriteAlpha), GET_MEMBER_NAME_CHECKED(FVertexPaintSettings, bPaintOnSpecificLOD), GET_MEMBER_NAME_CHECKED(FVertexPaintSettings, LODIndex) };

TSharedRef<IPropertyTypeCustomization> FVertexPaintSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FVertexPaintSettingsCustomization);
}

void FVertexPaintSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Cache vertex paint settings ptr
	PaintSettings = &(UPaintModeSettings::Get()->VertexPaintSettings);
	
	TMap<FName, TSharedRef<IPropertyHandle>> CustomizedProperties;
	TMap<FName, TSharedRef<IPropertyHandle>> Properties;

	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);

	// Add child properties to UI and pick out the properties which need customization
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();

		if (!CustomPropertyNames.Contains(ChildHandle->GetProperty()->GetFName()))
		{
			// Uses metadata to tag properties which should show for either vertex color of vertex blend weight painting
			IDetailPropertyRow& Property = ChildBuilder.AddProperty(ChildHandle);
			static const FName EditConditionName = "EnumCondition";
			if (ChildHandle->HasMetaData(EditConditionName))
			{
				int32 EnumCondition = ChildHandle->GetINTMetaData(EditConditionName);
				Property.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FVertexPaintSettingsCustomization::ArePropertiesVisible, EnumCondition)));
			}
		}
		else
		{
			CustomizedProperties.Add(ChildHandle->GetProperty()->GetFName(), ChildHandle);
		}

		Properties.Add(ChildHandle->GetProperty()->GetFName(), ChildHandle);
	}

	/** Creates a custom widget row containing all color channel flags */	
	TSharedRef<IPropertyHandle> RedChannel = CustomizedProperties.FindChecked(GET_MEMBER_NAME_CHECKED(FVertexPaintSettings, bWriteRed));
	TSharedRef<IPropertyHandle> GreenChannel = CustomizedProperties.FindChecked(GET_MEMBER_NAME_CHECKED(FVertexPaintSettings, bWriteGreen));
	TSharedRef<IPropertyHandle> BlueChannel = CustomizedProperties.FindChecked(GET_MEMBER_NAME_CHECKED(FVertexPaintSettings, bWriteBlue));
	TSharedRef<IPropertyHandle> AlphaChannel = CustomizedProperties.FindChecked(GET_MEMBER_NAME_CHECKED(FVertexPaintSettings, bWriteAlpha));
	TArray<TSharedRef<IPropertyHandle>> Channels = { RedChannel, GreenChannel, BlueChannel, AlphaChannel };
	TSharedPtr<SHorizontalBox> ChannelsWidget;

	ChildBuilder.AddCustomRow(NSLOCTEXT("ColorMask", "ChannelLabel", "Channels"))
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FVertexPaintSettingsCustomization::ArePropertiesVisible, 0)))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("VertexPaintSettings", "ChannelsLabel", "Channels"))
			.ToolTipText(NSLOCTEXT("VertexPaintSettings", "ChannelsToolTip", "Colors Channels which should be influenced during Painting."))
			.Font(CustomizationUtils.GetRegularFont())
		]
		.ValueContent()
		.MaxDesiredWidth(250.0f)
		[
			SAssignNew(ChannelsWidget, SHorizontalBox)
		];

	for (TSharedRef<IPropertyHandle> Channel : Channels)
	{
		ChannelsWidget->AddSlot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				CreateColorChannelWidget(Channel)
			];
	}

	/** Add property restrictions to the drop-down boxes used for blend weight painting */
	TSharedRef<IPropertyHandle> WeightTypeProperty = Properties.FindChecked(GET_MEMBER_NAME_CHECKED(FVertexPaintSettings, TextureWeightType));
	TSharedRef<IPropertyHandle> PaintWeightProperty = Properties.FindChecked(GET_MEMBER_NAME_CHECKED(FVertexPaintSettings, PaintTextureWeightIndex));
	TSharedRef<IPropertyHandle> EraseWeightProperty = Properties.FindChecked(GET_MEMBER_NAME_CHECKED(FVertexPaintSettings, EraseTextureWeightIndex));

	WeightTypeProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FVertexPaintSettingsCustomization::OnTextureWeightTypeChanged, WeightTypeProperty, PaintWeightProperty, EraseWeightProperty));

	static FText RestrictReason = NSLOCTEXT("VertexPaintSettings", "TextureIndexRestriction", "Unable to paint this Texture, change Texture Weight Type");
	BlendPaintEnumRestriction = MakeShareable(new FPropertyRestriction(RestrictReason));
	PaintWeightProperty->AddRestriction(BlendPaintEnumRestriction.ToSharedRef());
	EraseWeightProperty->AddRestriction(BlendPaintEnumRestriction.ToSharedRef());

	OnTextureWeightTypeChanged(WeightTypeProperty, PaintWeightProperty, EraseWeightProperty);

	/** Add custom row for painting on specific LOD level with callbacks to the painter to update the data */
	TSharedRef<IPropertyHandle> LODPaintingEnabled = CustomizedProperties.FindChecked(GET_MEMBER_NAME_CHECKED(FVertexPaintSettings, bPaintOnSpecificLOD));
	TSharedRef<IPropertyHandle> LODPaintingIndex = CustomizedProperties.FindChecked(GET_MEMBER_NAME_CHECKED(FVertexPaintSettings, LODIndex));
	TSharedPtr<SWidget> LODIndexWidget = LODPaintingIndex->CreatePropertyValueWidget();

	ChildBuilder.AddCustomRow(NSLOCTEXT("LODPainting", "LODPaintingLabel", "LOD Model Painting"))
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FVertexPaintSettingsCustomization::ArePropertiesVisible, 0)))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("LODPainting", "LODPaintingSetupLabel", "LOD Model Painting"))
			.ToolTipText(NSLOCTEXT("LODPainting", "LODPaintingSetupToolTip", "Allows for Painting Vertex Colors on Specific LOD Models."))
			.Font(CustomizationUtils.GetRegularFont())
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)			
			[
				SNew(SNumericEntryBox<int32>)
				.Font(CustomizationUtils.GetRegularFont())	
				.IsEnabled_Lambda([=]() -> bool { return PaintSettings->bPaintOnSpecificLOD;  })
				.AllowSpin(true)
				.Value_Lambda([=]() -> int32 { return PaintSettings->LODIndex; })
				.MinValue(0)
				.MaxValue_Lambda([=]() -> int32 { return FPaintModePainter::Get()->GetMaxLODIndexToPaint(); })
				.MaxSliderValue_Lambda([=]() -> int32 { return FPaintModePainter::Get()->GetMaxLODIndexToPaint(); })
				.OnValueChanged(SNumericEntryBox<int32>::FOnValueChanged::CreateLambda([=](int32 Value) { PaintSettings->LODIndex = Value; }))
				.OnValueCommitted(SNumericEntryBox<int32>::FOnValueCommitted::CreateLambda([=](int32 Value, ETextCommit::Type CommitType) { PaintSettings->LODIndex = Value; FPaintModePainter::Get()->PaintLODChanged(); }))
			]
			+SHorizontalBox::Slot()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([=]() -> ECheckBoxState { return (PaintSettings->bPaintOnSpecificLOD ? ECheckBoxState::Checked : ECheckBoxState::Unchecked); })
				.OnCheckStateChanged(FOnCheckStateChanged::CreateLambda([=](ECheckBoxState State) { FPaintModePainter::Get()->LODPaintStateChanged(State == ECheckBoxState::Checked); }))
			]
		];
}

EVisibility FVertexPaintSettingsCustomization::ArePropertiesVisible(const int32 VisibleType) const
{
	return ((int32)PaintSettings->MeshPaintMode == VisibleType) ? EVisibility::Visible : EVisibility::Collapsed;
}

void FVertexPaintSettingsCustomization::OnTextureWeightTypeChanged(TSharedRef<IPropertyHandle> WeightTypeProperty, TSharedRef<IPropertyHandle> PaintWeightProperty, TSharedRef<IPropertyHandle> EraseWeightProperty)
{
	UEnum* ImportTypeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ETexturePaintIndex"));
	uint8 EnumValue = 0;
	WeightTypeProperty->GetValue(EnumValue);	

	BlendPaintEnumRestriction->RemoveAll();
	for (uint8 EnumIndex = 0; EnumIndex < (ImportTypeEnum->GetMaxEnumValue() + 1); ++EnumIndex)
	{
		if ((EnumIndex + 1) > EnumValue)
		{
			FString EnumName = ImportTypeEnum->GetNameByValue(EnumIndex).ToString();
			EnumName.RemoveFromStart("ETexturePaintIndex::");
			BlendPaintEnumRestriction->AddDisabledValue(EnumName);
		}
	}

	uint8 Value = 0;
	PaintWeightProperty->GetValue(Value);
	Value = FMath::Clamp<uint8>(Value, 0, EnumValue - 1);
	PaintWeightProperty->SetValue(Value);

	EraseWeightProperty->GetValue(Value);
	Value = FMath::Clamp<uint8>(Value, 0, EnumValue - 1);
	EraseWeightProperty->SetValue(Value);
}

TSharedRef<IDetailCustomization> FPaintModeSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FPaintModeSettingsCustomization);
}

void FPaintModeSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	UPaintModeSettings* Settings = UPaintModeSettings::Get();

	TSharedRef<IPropertyHandle> PaintModeProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPaintModeSettings, PaintMode));
	uint8 EnumValue = 0;
	PaintModeProperty->GetValue(EnumValue);
	FSimpleDelegate OnPaintTypeChangedDelegate = FSimpleDelegate::CreateSP(this, &FPaintModeSettingsCustomization::OnPaintTypeChanged, &DetailBuilder);
	PaintModeProperty->SetOnPropertyValueChanged(OnPaintTypeChangedDelegate);
	
	IDetailCategoryBuilder& TexturePaintingCategory = DetailBuilder.EditCategory(FName("TexturePainting"));
	TexturePaintingCategory.SetCategoryVisibility(Settings->PaintMode == EPaintMode::Textures);

	IDetailCategoryBuilder& VertexPaintingCategory = DetailBuilder.EditCategory(FName("VertexPainting"));
	VertexPaintingCategory.SetCategoryVisibility(Settings->PaintMode == EPaintMode::Vertices);
}

void FPaintModeSettingsCustomization::OnPaintTypeChanged(IDetailLayoutBuilder* LayoutBuilder)
{
	LayoutBuilder->ForceRefreshDetails();
}

TSharedPtr<SWidget> FPaintModeSettingsRootObjectCustomization::CustomizeObjectHeader(const UObject* InRootObject)
{
	return SNullWidget::NullWidget;
}

TSharedRef<SHorizontalBox> CreateColorChannelWidget(TSharedRef<IPropertyHandle> ChannelProperty)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			ChannelProperty->CreatePropertyValueWidget()
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			ChannelProperty->CreatePropertyNameWidget()
		];
}
