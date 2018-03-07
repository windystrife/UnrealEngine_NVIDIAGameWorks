// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "K2Node_GetSequenceBinding.h"
#include "BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "Framework/Application/SlateApplication.h"
#include "PropertyCustomizationHelpers.h"
#include "MovieSceneSequence.h"
#include "MultiBox/MultiBoxBuilder.h"
#include "MovieSceneObjectBindingIDPicker.h"
#include "SGraphNode.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "GraphEditorSettings.h"
#include "Engine/Selection.h"
#include "SBox.h"
#include "SButton.h"
#include "SImage.h"
#include "SComboBox.h"
#include "Editor.h"

static const FString OutputPinName(TEXT("Output"));
static const FString SequencePinName(TEXT("Sequence"));

#define LOCTEXT_NAMESPACE "UK2Node_GetSequenceBinding"

void EnsureFullyLoaded(UObject* Object)
{
	if (!Object)
	{
		return;
	}

	bool bLoadInternalReferences = false;

	if (Object->HasAnyFlags(RF_NeedLoad))
	{
		FLinkerLoad* Linker = Object->GetLinker();
		if (ensure(Linker))
		{
			Linker->Preload(Object);
			bLoadInternalReferences = true;
			check(!Object->HasAnyFlags(RF_NeedLoad));
		}
	}

	bLoadInternalReferences = bLoadInternalReferences || Object->HasAnyFlags(RF_NeedPostLoad | RF_NeedPostLoadSubobjects);

	Object->ConditionalPostLoad();
	Object->ConditionalPostLoadSubobjects();
	
	if (bLoadInternalReferences)
	{
		// Collect a list of all things this element owns
		TArray<UObject*> ObjectReferences;
		FReferenceFinder(ObjectReferences, nullptr, false, true, false, true).FindReferences(Object);

		// Iterate over the list, and preload everything so it is valid for refreshing
		for (UObject* Reference : ObjectReferences)
		{
			if (Reference->IsA<UMovieSceneSequence>() || Reference->IsA<UMovieScene>() || Reference->IsA<UMovieSceneTrack>() || Reference->IsA<UMovieSceneSection>())
			{
				EnsureFullyLoaded(Reference);
			}
		}
	}
}

class FKCHandler_GetSequenceBinding : public FNodeHandlingFunctor
{
public:
	FKCHandler_GetSequenceBinding(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_GetSequenceBinding* BindingNode = CastChecked<UK2Node_GetSequenceBinding>(Node);
		
		for (UEdGraphPin* Pin : BindingNode->GetAllPins())
		{
			if (Pin->Direction == EGPD_Output && Pin->LinkedTo.Num())
			{
				FBPTerminal* Term = RegisterLiteral(Context, Pin);
				FMovieSceneObjectBindingID::StaticStruct()->ExportText(Term->Name, &BindingNode->Binding, nullptr, nullptr, 0, nullptr);
			}
		}
	}
};

UMovieSceneSequence* UK2Node_GetSequenceBinding::GetSequence() const
{
	return Cast<UMovieSceneSequence>(SourceSequence.TryLoad());
}

void UK2Node_GetSequenceBinding::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	UMovieScene* MovieScene = GetObjectMovieScene();
	if (!MovieScene)
	{
		const FText MessageText = LOCTEXT("InvalidSequenceBinding_NoSequence", "Invalid sequence binding specified on node @@ (could not find sequence).");
		MessageLog.Warning(*MessageText.ToString(), this);
	}
	else if (!MovieScene->FindPossessable(Binding.GetGuid()) && !MovieScene->FindSpawnable(Binding.GetGuid()))
	{
		const FText MessageText = LOCTEXT("InvalidSequenceBinding_Unresolved", "Invalid sequence binding specified on node @@.");
		MessageLog.Warning(*MessageText.ToString(), this);
	}
}

void UK2Node_GetSequenceBinding::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	CreatePin(EGPD_Input, K2Schema->PC_Object, FString(), UMovieSceneSequence::StaticClass(), SequencePinName);

	// Result pin
	UEdGraphPin* ResultPin = CreatePin(EGPD_Output, K2Schema->PC_Struct, FString(), FMovieSceneObjectBindingID::StaticStruct(), K2Schema->PN_ReturnValue);
	ResultPin->PinFriendlyName = LOCTEXT("SequenceBindingOutput", "Binding");

	Super::AllocateDefaultPins();
}

UMovieScene* UK2Node_GetSequenceBinding::GetObjectMovieScene() const
{
	UMovieSceneSequence* Sequence = GetSequence();
	if (Sequence && Binding.IsValid())
	{
		// Ensure that the sequence data is as loaded as it can be - we many only be able to partially load the structural information as part of a blueprint compile as that may happen at Preload time
		EnsureFullyLoaded(Sequence);

		FMovieSceneSequenceID SequenceID = Binding.GetSequenceID();
		if (SequenceID == MovieSceneSequenceID::Root)
		{
			// Look it up in the moviescene itself
			return Sequence->GetMovieScene();
		}
		else
		{
			// Ensure the template is up to date
			FMovieSceneTrackCompilationParams Params = Sequence->TemplateParameters;
			// Indicate that the template was generated during a blueprint compile so it can be regenerated with the full data when necessary
			Params.bDuringBlueprintCompile = true;
			// Regenerate the template
			Sequence->EvaluationTemplate.Regenerate(Params);

			UMovieScene* MovieScene = nullptr;
			if (const FMovieSceneSubSequenceData* SubData = Sequence->EvaluationTemplate.Hierarchy.FindSubData(SequenceID))
			{
				UMovieSceneSequence* SubSequence = SubData->Sequence;
				return SubSequence ? SubSequence->GetMovieScene() : nullptr;
			}
		}
	}

	return nullptr;
}

FNodeHandlingFunctor* UK2Node_GetSequenceBinding::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_GetSequenceBinding(CompilerContext);
}

FText UK2Node_GetSequenceBinding::GetSequenceName() const
{
	UMovieSceneSequence* Sequence = GetSequence();
	return Sequence ? FText::FromName(Sequence->GetFName()) : LOCTEXT("NoSequence", "No Sequence");
}

FText UK2Node_GetSequenceBinding::GetBindingName() const
{
	UMovieScene* MovieScene = GetObjectMovieScene();
	return MovieScene ? MovieScene->GetObjectDisplayName(Binding.GetGuid()) : FText();
}

FText UK2Node_GetSequenceBinding::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FText BindingName = GetBindingName();

	return BindingName.IsEmpty() ? LOCTEXT("NodeTitle", "Get Sequence Binding") : FText::Format(LOCTEXT("NodeTitle_Format", "Get Sequence Binding ({0})"), BindingName);
}

FText UK2Node_GetSequenceBinding::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Access an identifier for any object binding within a sequence");
}

FSlateIcon UK2Node_GetSequenceBinding::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.GetSequenceBinding");
	return Icon;
}

void UK2Node_GetSequenceBinding::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	Super::GetContextMenuActions(Context);

	if (!Context.bIsDebugging)
	{
		Context.MenuBuilder->BeginSection("K2NodeGetSequenceBinding", LOCTEXT("ThisNodeHeader", "This Node"));
		if (!Context.Pin)
		{
			UMovieSceneSequence* Sequence = GetSequence();

			auto SubMenu = [=](FMenuBuilder& SubMenuBuilder)
				{
					TArray<const UClass*> AllowedClasses({ UMovieSceneSequence::StaticClass() });

					TSharedRef<SWidget> MenuContent = PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
						FAssetData(Sequence),
						true /* bAllowClear */,
						AllowedClasses,
						PropertyCustomizationHelpers::GetNewAssetFactoriesForClasses(AllowedClasses),
						FOnShouldFilterAsset(),
						FOnAssetSelected::CreateUObject(this, &UK2Node_GetSequenceBinding::SetSequence),
						FSimpleDelegate());
					
					SubMenuBuilder.AddWidget(MenuContent, FText::GetEmpty(), false);
				};

			Context.MenuBuilder->AddSubMenu(
				LOCTEXT("SetSequence_Text", "Sequence"),
				LOCTEXT("SetSequence_ToolTip", "Sets the sequence to get a binding from"),
				FNewMenuDelegate::CreateLambda(SubMenu)
				);
		}

		Context.MenuBuilder->EndSection();
	}
}

void UK2Node_GetSequenceBinding::SetSequence(const FAssetData& InAssetData)
{
	FSlateApplication::Get().DismissAllMenus();
	SourceSequence = Cast<UMovieSceneSequence>(InAssetData.GetAsset());
}

void UK2Node_GetSequenceBinding::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

#if WITH_EDITOR

TSharedPtr<SGraphNode> UK2Node_GetSequenceBinding::CreateVisualWidget()
{
	class SGraphNodeGetSequenceBinding : public SGraphNode, FMovieSceneObjectBindingIDPicker
	{
	public:
		SLATE_BEGIN_ARGS(SGraphNodeGetSequenceBinding){}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, UK2Node_GetSequenceBinding* InNode)
		{
			bNeedsUpdate = false;
			GraphNode = InNode;
			Initialize();
			UpdateGraphNode();
		}

		virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
		{
			UK2Node_GetSequenceBinding* Node = CastChecked<UK2Node_GetSequenceBinding>(GraphNode);
			UMovieSceneSequence* Sequence = Node->GetSequence();

			if (bNeedsUpdate || Sequence != LastSequence.Get())
			{
				Initialize();
				UpdateGraphNode();

				bNeedsUpdate = false;
			}

			LastSequence = Sequence;

			SGraphNode::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
		}

		virtual void CreateStandardPinWidget(UEdGraphPin* Pin) override
		{
			if (Pin->PinName == SequencePinName)
			{
				CreateDetailsPickers();
			}
			else
			{
				SGraphNode::CreateStandardPinWidget(Pin);
			}
		}

		void OnAssetSelectedFromPicker(const FAssetData& AssetData)
		{
			CastChecked<UK2Node_GetSequenceBinding>(GraphNode)->SetSequence(AssetData);
			Initialize();
			UpdateGraphNode();
		}

		FText GetAssetName() const
		{
			return CastChecked<UK2Node_GetSequenceBinding>(GraphNode)->GetSequenceName();
		}

		TSharedRef<SWidget> GenerateAssetPicker()
		{
			FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

			FAssetPickerConfig AssetPickerConfig;
			AssetPickerConfig.Filter.ClassNames.Add(UMovieSceneSequence::StaticClass()->GetFName());
			AssetPickerConfig.bAllowNullSelection = true;
			AssetPickerConfig.Filter.bRecursiveClasses = true;
			AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SGraphNodeGetSequenceBinding::OnAssetSelectedFromPicker);
			AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
			AssetPickerConfig.bAllowDragging = false;

			return SNew(SBox)
				.HeightOverride(300)
				.WidthOverride(300)
				[
					SNew(SBorder)
					.BorderImage( FEditorStyle::GetBrush("Menu.Background") )
					[
						ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
					]
				];
		}

		FReply UseSelectedAsset()
		{
			UMovieSceneSequence* Sequence = Cast<UMovieSceneSequence>(GEditor->GetSelectedObjects()->GetTop(UMovieSceneSequence::StaticClass()));
			if (Sequence)
			{
				CastChecked<UK2Node_GetSequenceBinding>(GraphNode)->SetSequence(Sequence);
				Initialize();
				UpdateGraphNode();
			}
			return FReply::Handled();
		}

		FReply BrowseToAsset()
		{
			UMovieSceneSequence* Sequence = CastChecked<UK2Node_GetSequenceBinding>(GraphNode)->GetSequence();
			if (Sequence)
			{
				TArray<UObject*> Objects{ Sequence };
				GEditor->SyncBrowserToObjects(Objects);
			}
			return FReply::Handled();
		}

		void CreateDetailsPickers()
		{
			LeftNodeBox->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(Settings->GetInputPinPadding())
			[
				SNew(SHorizontalBox)

				// Asset Combo
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2,0)
				.MaxWidth(200.0f)
				[
					SNew(SComboButton)
					.ButtonStyle( FEditorStyle::Get(), "PropertyEditor.AssetComboStyle" )
					.ForegroundColor(this, &SGraphNodeGetSequenceBinding::OnGetComboForeground)
					.ButtonColorAndOpacity(this, &SGraphNodeGetSequenceBinding::OnGetWidgetBackground)
					.ContentPadding(FMargin(2,2,2,1))
					.MenuPlacement(MenuPlacement_BelowAnchor)
					.ButtonContent()
					[
						SNew(STextBlock)
						.ColorAndOpacity(this, &SGraphNodeGetSequenceBinding::OnGetComboForeground)
						.TextStyle( FEditorStyle::Get(), "PropertyEditor.AssetClass" )
						.Font( FEditorStyle::GetFontStyle( "PropertyWindow.NormalFont" ) )
						.Text( this, &SGraphNodeGetSequenceBinding::GetAssetName )
					]
					.OnGetMenuContent(this, &SGraphNodeGetSequenceBinding::GenerateAssetPicker)
				]

				// Use button
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(1,0)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
					.OnClicked(this, &SGraphNodeGetSequenceBinding::UseSelectedAsset)
					.ButtonColorAndOpacity(this, &SGraphNodeGetSequenceBinding::OnGetWidgetBackground)
					.ContentPadding(1.f)
					.ToolTipText(LOCTEXT("GraphNodeGetSequenceBinding_Use_Tooltip", "Use asset browser selection"))
					[
						SNew(SImage)
						.ColorAndOpacity(this, &SGraphNodeGetSequenceBinding::OnGetWidgetForeground)
						.Image( FEditorStyle::GetBrush(TEXT("PropertyWindow.Button_Use")) )
					]
				]

				// Browse button
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(1,0)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
					.OnClicked(this, &SGraphNodeGetSequenceBinding::BrowseToAsset)
					.ButtonColorAndOpacity(this, &SGraphNodeGetSequenceBinding::OnGetWidgetBackground)
					.ContentPadding(0)
					.ToolTipText(LOCTEXT("GraphNodeGetSequenceBinding_Browse_Tooltip", "Browse"))
					[
						SNew(SImage)
						.ColorAndOpacity(this, &SGraphNodeGetSequenceBinding::OnGetWidgetForeground)
						.Image( FEditorStyle::GetBrush(TEXT("PropertyWindow.Button_Browse")) )
					]
				]
			];

			LeftNodeBox->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(Settings->GetInputPinPadding())
			[
				SNew(SBox)
				.MaxDesiredWidth(200.0f)
				.Padding(FMargin(2,0))
				[
					SNew(SComboButton)
					.ButtonStyle( FEditorStyle::Get(), "PropertyEditor.AssetComboStyle" )
					.ToolTipText(this, &SGraphNodeGetSequenceBinding::GetToolTipText)
					.ForegroundColor(this, &SGraphNodeGetSequenceBinding::OnGetComboForeground)
					.ButtonColorAndOpacity(this, &SGraphNodeGetSequenceBinding::OnGetWidgetBackground)
					.ContentPadding(FMargin(2,2,2,1))
					.MenuPlacement(MenuPlacement_BelowAnchor)
					.ButtonContent()
					[
						GetCurrentItemWidget(
							SNew(STextBlock)
							.TextStyle( FEditorStyle::Get(), "PropertyEditor.AssetClass" )
							.Font( FEditorStyle::GetFontStyle( "PropertyWindow.NormalFont" ) )
							.ColorAndOpacity(this, &SGraphNodeGetSequenceBinding::OnGetComboForeground)
						)
					]
					.OnGetMenuContent(this, &SGraphNodeGetSequenceBinding::GetPickerMenu)
				]
			];
		}

	private:

		virtual void SetCurrentValue(const FMovieSceneObjectBindingID& InBindingId) override
		{
			CastChecked<UK2Node_GetSequenceBinding>(GraphNode)->Binding = InBindingId;
			bNeedsUpdate = true;
		}
		virtual FMovieSceneObjectBindingID GetCurrentValue() const override
		{
			return CastChecked<UK2Node_GetSequenceBinding>(GraphNode)->Binding;
		}
		virtual UMovieSceneSequence* GetSequence() const override
		{
			return CastChecked<UK2Node_GetSequenceBinding>(GraphNode)->GetSequence();
		}

		FSlateColor OnGetComboForeground() const
		{
			return FSlateColor( FLinearColor( 1.f, 1.f, 1.f, IsHovered() ? 1.f : 0.6f ) );
		}

		FSlateColor OnGetWidgetForeground() const
		{
			return FSlateColor( FLinearColor( 1.f, 1.f, 1.f, IsHovered() ? 1.f : 0.15f ) );
		}

		FSlateColor OnGetWidgetBackground() const
		{
			return FSlateColor( FLinearColor( 1.f, 1.f, 1.f, IsHovered() ? 0.8f : 0.4f ) );
		}

	private:
		TWeakObjectPtr<UMovieSceneSequence> LastSequence;
		bool bNeedsUpdate;
	};

	return SNew(SGraphNodeGetSequenceBinding, this);
}

#endif

#undef LOCTEXT_NAMESPACE
