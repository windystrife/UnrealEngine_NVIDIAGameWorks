// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraScriptDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailWidgetRow.h"
#include "SInlineEditableTextBlock.h"
#include "NiagaraParameterCollectionViewModel.h"
#include "NiagaraScriptInputCollectionViewModel.h"
#include "NiagaraScriptOutputCollectionViewModel.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraParameterViewModel.h"
#include "NiagaraEditorStyle.h"
#include "IDetailChildrenBuilder.h"
#include "MultiBoxBuilder.h"
#include "SComboButton.h"
#include "SImage.h"
#include "NiagaraEditorModule.h"
#include "ModuleManager.h"
#include "INiagaraEditorTypeUtilities.h"
#include "SBox.h"


#define LOCTEXT_NAMESPACE "NiagaraScriptDetails"

class FNiagaraCustomNodeBuilder : public IDetailCustomNodeBuilder
{
public:
	FNiagaraCustomNodeBuilder(TSharedRef<INiagaraParameterCollectionViewModel> InViewModel)
		: ViewModel(InViewModel)
	{
		ViewModel->OnCollectionChanged().AddRaw(this, &FNiagaraCustomNodeBuilder::OnCollectionViewModelChanged);
	}

	~FNiagaraCustomNodeBuilder()
	{
		ViewModel->OnCollectionChanged().RemoveAll(this);
	}

	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override
	{
		OnRebuildChildren = InOnRegenerateChildren;
	}

	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow){}
	virtual void Tick(float DeltaTime) override {}
	virtual bool RequiresTick() const override { return false; }
	virtual bool InitiallyCollapsed() const { return false; }
	virtual FName GetName() const  override
	{ 
		static const FName NiagaraCustomNodeBuilder("NiagaraCustomNodeBuilder");
		return NiagaraCustomNodeBuilder;
	}

	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override
	{
		const TArray<TSharedRef<INiagaraParameterViewModel>>& Parameters = ViewModel->GetParameters();

		FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");

		for (const TSharedRef<INiagaraParameterViewModel>& Parameter : Parameters)
		{
			TSharedPtr<SWidget> NameWidget;

			if (Parameter->CanRenameParameter())
			{
				NameWidget =
					SNew(SInlineEditableTextBlock)
					.Style(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterInlineEditableText")
					.Text(Parameter, &INiagaraParameterViewModel::GetNameText)
					.OnVerifyTextChanged(Parameter, &INiagaraParameterViewModel::VerifyNodeNameTextChanged)
					.OnTextCommitted(Parameter, &INiagaraParameterViewModel::NameTextComitted);
			}
			else
			{
				NameWidget =
					SNew(STextBlock)
					.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
					.Text(Parameter, &INiagaraParameterViewModel::GetNameText);
			}

			

			IDetailPropertyRow* Row = nullptr;

			TSharedPtr<SWidget> CustomValueWidget;

			if (Parameter->GetDefaultValueType() == INiagaraParameterViewModel::EDefaultValueType::Struct)
			{
				Row = ChildrenBuilder.AddExternalStructureProperty(Parameter->GetDefaultValueStruct(), NAME_None, Parameter->GetName());
				
			}
			else if (Parameter->GetDefaultValueType() == INiagaraParameterViewModel::EDefaultValueType::Object)
			{
				UObject* DefaultValueObject = Parameter->GetDefaultValueObject();

				TArray<UObject*> Objects;
				Objects.Add(DefaultValueObject);
				Row = ChildrenBuilder.AddExternalObjects(Objects, Parameter->GetName());

				CustomValueWidget =
					SNew(STextBlock)
					.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
					.Text(FText::FromString(FName::NameToDisplayString(DefaultValueObject->GetClass()->GetName(), false)));
			}

			if(Row)
			{
				TSharedPtr<SWidget> DefaultNameWidget;
				TSharedPtr<SWidget> DefaultValueWidget;

				TSharedPtr<IPropertyHandle> PropertyHandle = Row->GetPropertyHandle();

				FDetailWidgetRow& CustomWidget = Row->CustomWidget(true);

				Row->GetDefaultWidgets(DefaultNameWidget, DefaultValueWidget, CustomWidget);

				PropertyHandle->SetOnPropertyValueChanged(
					FSimpleDelegate::CreateSP(Parameter, &INiagaraParameterViewModel::NotifyDefaultValueChanged));
				PropertyHandle->SetOnChildPropertyValueChanged(
					FSimpleDelegate::CreateSP(Parameter, &INiagaraParameterViewModel::NotifyDefaultValueChanged));

				CustomWidget
					.NameContent()
					[
						SNew(SBox)
						.Padding(FMargin(0.0f, 2.0f))
						[
							NameWidget.ToSharedRef()
						]
					];

				if (CustomValueWidget.IsValid())
				{
					CustomWidget.ValueContent()
					[
						CustomValueWidget.ToSharedRef()
					];
				}
			}

		}


	}

private:
	void OnCollectionViewModelChanged()
	{
		OnRebuildChildren.ExecuteIfBound();
	}

private:
	TSharedRef<INiagaraParameterCollectionViewModel> ViewModel;
	FSimpleDelegate OnRebuildChildren;
};

class SAddParameterButton : public SCompoundWidget
{	
	SLATE_BEGIN_ARGS(SAddParameterButton)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<INiagaraParameterCollectionViewModel> InViewModel)
	{
		CollectionViewModel = InViewModel;

		ChildSlot
		.HAlign(HAlign_Right)
		[
			SAssignNew(ComboButton, SComboButton)
			.HasDownArrow(false)
			.ButtonStyle(FEditorStyle::Get(), "RoundButton")
			.ForegroundColor(FSlateColor::UseForeground())
			.OnGetMenuContent(this, &SAddParameterButton::GetAddParameterMenuContent)
			.Visibility(CollectionViewModel.ToSharedRef(), &INiagaraParameterCollectionViewModel::GetAddButtonVisibility)
			.ContentPadding(FMargin(2.0f, 1.0f, 0.0f, 1.0f))
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(0, 1, 2, 1)
				.AutoWidth()
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FEditorStyle::GetBrush("Plus"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFontBold())
					.Text(CollectionViewModel.ToSharedRef(), &INiagaraParameterCollectionViewModel::GetAddButtonText)
					.Visibility(this, &SAddParameterButton::OnGetAddParameterTextVisibility)
					.ShadowOffset(FVector2D(1, 1))
				]
			]
		];
	}

private:
	EVisibility OnGetAddParameterTextVisibility() const
	{
		return IsHovered() || ComboButton->IsOpen() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	TSharedRef<SWidget> GetAddParameterMenuContent() const
	{
		FMenuBuilder AddMenuBuilder(true, nullptr);
		for (TSharedPtr<FNiagaraTypeDefinition> AvailableType : CollectionViewModel->GetAvailableTypes())
		{
			AddMenuBuilder.AddMenuEntry
			(
				AvailableType->GetNameText(),
				FText(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(CollectionViewModel.ToSharedRef(), &INiagaraParameterCollectionViewModel::AddParameter, AvailableType))
			);
		}
		return AddMenuBuilder.MakeWidget();
	}

	
private:
	TSharedPtr<INiagaraParameterCollectionViewModel> CollectionViewModel;
	TSharedPtr<SComboButton> ComboButton;
};

TSharedRef<IDetailCustomization> FNiagaraScriptDetails::MakeInstance(TWeakPtr<FNiagaraScriptViewModel> ScriptViewModel)
{
	return MakeShared<FNiagaraScriptDetails>(ScriptViewModel.Pin());
}

FNiagaraScriptDetails::FNiagaraScriptDetails(TSharedPtr<FNiagaraScriptViewModel> InScriptViewModel)
	: ScriptViewModel(InScriptViewModel)
{}

void FNiagaraScriptDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	static const FName InputParamCategoryName = TEXT("NiagaraScript_InputParams");
	static const FName OutputParamCategoryName = TEXT("NiagaraScript_OutputParams");
	static const FName ScriptCategoryName = TEXT("Script");

	DetailBuilder.EditCategory(ScriptCategoryName);

	TSharedRef<INiagaraParameterCollectionViewModel> InputCollectionViewModel = ScriptViewModel->GetInputCollectionViewModel();
	TSharedRef<INiagaraParameterCollectionViewModel> OutputCollectionViewModel = ScriptViewModel->GetOutputCollectionViewModel();

	IDetailCategoryBuilder& InputParamCategory = DetailBuilder.EditCategory(InputParamCategoryName, LOCTEXT("InputParamCategoryName", "Input Parameters"));
	InputParamCategory.HeaderContent(SNew(SAddParameterButton, InputCollectionViewModel));
	InputParamCategory.AddCustomBuilder(MakeShared<FNiagaraCustomNodeBuilder>(InputCollectionViewModel));


	IDetailCategoryBuilder& OutputParamCategory = DetailBuilder.EditCategory(OutputParamCategoryName, LOCTEXT("OutputParamCategoryName", "Output Parameters"));
	OutputParamCategory.HeaderContent(SNew(SAddParameterButton, OutputCollectionViewModel));
	OutputParamCategory.AddCustomBuilder(MakeShared<FNiagaraCustomNodeBuilder>(OutputCollectionViewModel));
	
}


#undef LOCTEXT_NAMESPACE
