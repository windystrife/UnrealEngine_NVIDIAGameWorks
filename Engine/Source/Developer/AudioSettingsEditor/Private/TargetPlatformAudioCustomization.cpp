#include "TargetPlatformAudioCustomization.h"

#include "EditorDirectories.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"
#include "UObject/UnrealType.h"
#include "Widgets/Text/STextBlock.h"
#include "SComboButton.h"
#include "SListView.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "SEditableTextBox.h"

#if WITH_ENGINE
#include "AudioDevice.h"
#endif 

#define LOCTEXT_NAMESPACE "PlatformAudio"

FAudioPluginWidgetManager::FAudioPluginWidgetManager()
{
}

FAudioPluginWidgetManager::~FAudioPluginWidgetManager()
{
}

TSharedRef<SWidget> FAudioPluginWidgetManager::MakeAudioPluginSelectorWidget(const TSharedPtr<IPropertyHandle>& PropertyHandle, EAudioPlugin AudioPluginType, EAudioPlatform AudioPlatform)
{
	TArray<TSharedPtr<FText>>* ValidPluginNames = nullptr;
	FText TooltipText;
	TSharedPtr<FText> DefaultEffectName;

	switch (AudioPluginType)
	{
		case EAudioPlugin::SPATIALIZATION:
			ValidPluginNames = &SpatializationPlugins;
			TooltipText = LOCTEXT("Spatialization", "Choose which audio plugin should be used for spatialization. If your desired spatialization isn't found in the drop down menu, ensure that it is enabled on the Plugins panel.");
			DefaultEffectName = TSharedPtr<FText>(new FText(FText::FromString(TEXT("Built-in Spatialization"))));
			SelectedSpatialization = TSharedPtr<FText>(new FText(*DefaultEffectName));
			PropertyHandle->GetValueAsDisplayText(*SelectedSpatialization);
			break;

		case EAudioPlugin::REVERB:
			ValidPluginNames = &ReverbPlugins;
			TooltipText = LOCTEXT("Reverb", "Choose which audio plugin should be used for reverb. If your desired reverb plugin isn't found in the drop down menu, ensure that it is enabled on the Plugins panel.");
			DefaultEffectName = TSharedPtr<FText>(new FText(FText::FromString(TEXT("Built-in Reverb"))));
			SelectedReverb = TSharedPtr<FText>(new FText(*DefaultEffectName));
			PropertyHandle->GetValueAsDisplayText(*SelectedReverb);
			break;

		case EAudioPlugin::OCCLUSION:
			ValidPluginNames = &OcclusionPlugins;
			TooltipText = LOCTEXT("Occlusion", "Choose which audio plugin should be used for occlusion. If your desired occlusion plugin isn't found in the drop down menu, ensure that it is enabled on the Plugins panel.");
			DefaultEffectName = TSharedPtr<FText>(new FText(FText::FromString(TEXT("Built-in Occlusion"))));
			SelectedOcclusion = TSharedPtr<FText>(new FText(*DefaultEffectName));
			PropertyHandle->GetValueAsDisplayText(*SelectedOcclusion);
			break;

		default:
			checkf(false, TEXT("Invalid plugin enumeration type. Need to add a handle for that case here."));
			break;
	}

	ValidPluginNames->Add(DefaultEffectName);
	

#if WITH_ENGINE
	// Scan through all currently enabled audio plugins of this specific type:

	switch (AudioPluginType)
	{
		case EAudioPlugin::SPATIALIZATION:
		{
			TArray<IAudioSpatializationFactory*> AvailableSpatializationPlugins = IModularFeatures::Get().GetModularFeatureImplementations<IAudioSpatializationFactory>(IAudioSpatializationFactory::GetModularFeatureName());
			for (IAudioSpatializationFactory* Plugin : AvailableSpatializationPlugins)
			{
				if (Plugin->SupportsPlatform(AudioPlatform))
				{
					ValidPluginNames->Add(TSharedPtr<FText>(new FText(FText::FromString(Plugin->GetDisplayName()))));
				}
			}
		}
		break;

		case EAudioPlugin::REVERB:
		{
			TArray<IAudioReverbFactory*> AvailableReverbPlugins = IModularFeatures::Get().GetModularFeatureImplementations<IAudioReverbFactory>(IAudioReverbFactory::GetModularFeatureName());
			for (IAudioReverbFactory* Plugin : AvailableReverbPlugins)
			{
				if (Plugin->SupportsPlatform(AudioPlatform))
				{
					ValidPluginNames->Add(TSharedPtr<FText>(new FText(FText::FromString(Plugin->GetDisplayName()))));
				}
			}
		}
		break;

		case EAudioPlugin::OCCLUSION:
		{
			TArray<IAudioOcclusionFactory*> AvailableOcclusionPlugins = IModularFeatures::Get().GetModularFeatureImplementations<IAudioOcclusionFactory>(IAudioOcclusionFactory::GetModularFeatureName());
			for (IAudioOcclusionFactory* Plugin : AvailableOcclusionPlugins)
			{
				if (Plugin->SupportsPlatform(AudioPlatform))
				{
					ValidPluginNames->Add(TSharedPtr<FText>(new FText(FText::FromString(Plugin->GetDisplayName()))));
				}
			}
		}
		break;

		default:
			break;
	}
#endif // #if WITH_ENGINE

	//Generate widget:
	const TSharedRef<SWidget> NewWidget = SNew(SComboButton)
		.ContentPadding(FMargin(0, 0, 5, 0))
		.ToolTipText(TooltipText)
		.ButtonContent()
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.Padding(FMargin(0, 0, 5, 0))
			[
				SNew(SEditableTextBox)
				.Text_Lambda([this, AudioPluginType]() { return OnGetPluginText(AudioPluginType); })
				.OnTextCommitted_Raw(this, &FAudioPluginWidgetManager::OnPluginTextCommitted, AudioPluginType, PropertyHandle)
				.SelectAllTextWhenFocused(true)
				.RevertTextOnEscape(true)
			]
		]
		.MenuContent()
		[
			SNew(SListView<TSharedPtr<FText>>)
			.ListItemsSource(ValidPluginNames)
			.ScrollbarVisibility(EVisibility::Collapsed)
			.OnGenerateRow_Lambda([](TSharedPtr<FText> InItem, const TSharedRef< class STableViewBase >& Owner)
			{
				return
					SNew(STableRow<TSharedPtr<FText>>, Owner)
					.Padding(FMargin(16, 4, 16, 4))
					[
						SNew(STextBlock).Text(*InItem)
					];
			})
			.OnSelectionChanged_Lambda([this, AudioPluginType, PropertyHandle](TSharedPtr<FText> InText, ESelectInfo::Type)
			{
				switch (AudioPluginType)
				{
					case EAudioPlugin::SPATIALIZATION:
						SelectedSpatialization = InText;
						break;

					case EAudioPlugin::REVERB:
						SelectedReverb = InText;
						break;
					
					case EAudioPlugin::OCCLUSION:
						SelectedOcclusion = InText;
						break;
					
					default:
						break;
				}

				OnPluginSelected(InText->ToString(), PropertyHandle);
			})
		];

	return NewWidget;
}

void FAudioPluginWidgetManager::BuildAudioCategory(IDetailLayoutBuilder& DetailLayout, EAudioPlatform AudioPlatform)
{
	IDetailCategoryBuilder& AudioCategory = DetailLayout.EditCategory(TEXT("Audio"));

	TSharedPtr<IPropertyHandle> AudioSpatializationPropertyHandle = DetailLayout.GetProperty("SpatializationPlugin");
	IDetailPropertyRow& AudioSpatializationPropertyRow = AudioCategory.AddProperty(AudioSpatializationPropertyHandle);

	AudioSpatializationPropertyRow.CustomWidget()
		.NameContent()
		[
			AudioSpatializationPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(500.0f)
		.MinDesiredWidth(100.0f)
		[
			MakeAudioPluginSelectorWidget(AudioSpatializationPropertyHandle, EAudioPlugin::SPATIALIZATION, AudioPlatform)
		];

	TSharedPtr<IPropertyHandle> AudioReverbPropertyHandle = DetailLayout.GetProperty("ReverbPlugin");
	IDetailPropertyRow& AudioReverbPropertyRow = AudioCategory.AddProperty(AudioReverbPropertyHandle);

	AudioReverbPropertyRow.CustomWidget()
		.NameContent()
		[
			AudioReverbPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(500.0f)
		.MinDesiredWidth(100.0f)
		[
			MakeAudioPluginSelectorWidget(AudioReverbPropertyHandle, EAudioPlugin::REVERB, AudioPlatform)
		];

	TSharedPtr<IPropertyHandle> AudioOcclusionPropertyHandle = DetailLayout.GetProperty("OcclusionPlugin");
	IDetailPropertyRow& AudioOcclusionPropertyRow = AudioCategory.AddProperty(AudioOcclusionPropertyHandle);

	AudioOcclusionPropertyRow.CustomWidget()
		.NameContent()
		[
			AudioOcclusionPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(500.0f)
		.MinDesiredWidth(100.0f)
		[
			MakeAudioPluginSelectorWidget(AudioOcclusionPropertyHandle, EAudioPlugin::OCCLUSION, AudioPlatform)
		];
}

void FAudioPluginWidgetManager::OnPluginSelected(FString PluginName, TSharedPtr<IPropertyHandle> PropertyHandle)
{
	PropertyHandle->SetValue(PluginName);
}

void FAudioPluginWidgetManager::OnPluginTextCommitted(const FText& InText, ETextCommit::Type CommitType, EAudioPlugin AudioPluginType, TSharedPtr<IPropertyHandle> PropertyHandle)
{
	switch (AudioPluginType)
	{
		case EAudioPlugin::SPATIALIZATION:
			*SelectedSpatialization = InText;
			break;

		case EAudioPlugin::REVERB:
			*SelectedReverb = InText;
			break;

		case EAudioPlugin::OCCLUSION:
			*SelectedOcclusion = InText;
			break;

		default:
			break;
	}

	OnPluginSelected(InText.ToString(), PropertyHandle);
}

FText FAudioPluginWidgetManager::OnGetPluginText(EAudioPlugin AudioPluginType)
{
	switch (AudioPluginType)
	{
		case EAudioPlugin::SPATIALIZATION:
			return *SelectedSpatialization;
			break;
	
		case EAudioPlugin::REVERB:
			return *SelectedReverb;
			break;

		case EAudioPlugin::OCCLUSION:
			return *SelectedOcclusion;
			break;
	
		default:
			return FText::FromString(FString(TEXT("ERROR")));
			break;
	}
}

#undef LOCTEXT_NAMESPACE