// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/ActorPickerTrackEditor.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleManager.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "ActorPickerMode.h"
#include "SceneOutlinerPublicTypes.h"
#include "SceneOutlinerModule.h"
#include "Private/SSocketChooser.h"
#include "LevelEditor.h"

#define LOCTEXT_NAMESPACE "FActorPickerTrackEditor"

FActorPickerTrackEditor::FActorPickerTrackEditor(TSharedRef<ISequencer> InSequencer) 
	: FMovieSceneTrackEditor( InSequencer ) 

{
}

void FActorPickerTrackEditor::PickActorInteractive(FGuid ObjectBinding, UMovieSceneSection* Section)
{
	if(GUnrealEd->GetSelectedActorCount())
	{
		FActorPickerModeModule& ActorPickerMode = FModuleManager::Get().GetModuleChecked<FActorPickerModeModule>("ActorPickerMode");

		ActorPickerMode.BeginActorPickingMode(
			FOnGetAllowedClasses(), 
			FOnShouldFilterActor::CreateSP(this, &FActorPickerTrackEditor::IsActorPickable, ObjectBinding, Section), 
			FOnActorSelected::CreateSP(this, &FActorPickerTrackEditor::ActorPicked, ObjectBinding, Section)
			);
	}
}

void FActorPickerTrackEditor::ShowActorSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, UMovieSceneSection* Section)
{
	struct Local
	{
		static FReply OnInteractiveActorPickerClicked(FActorPickerTrackEditor* ActorPickerTrackEditor, FGuid TheObjectBinding, UMovieSceneSection* TheSection)
		{
			FSlateApplication::Get().DismissAllMenus();
			ActorPickerTrackEditor->PickActorInteractive(TheObjectBinding, TheSection);
			return FReply::Handled();
		}
	};

	using namespace SceneOutliner;

	SceneOutliner::FInitializationOptions InitOptions;
	{
		InitOptions.Mode = ESceneOutlinerMode::ActorPicker;			
		InitOptions.bShowHeaderRow = false;
		InitOptions.bFocusSearchBoxWhenOpened = true;
		InitOptions.bShowTransient = true;
		InitOptions.bShowCreateNewFolder = false;
		// Only want the actor label column
		InitOptions.ColumnMap.Add(FBuiltInColumnTypes::Label(), FColumnInfo(EColumnVisibility::Visible, 0));

		// Only display Actors that we can attach too
		InitOptions.Filters->AddFilterPredicate( SceneOutliner::FActorFilterPredicate::CreateSP(this, &FActorPickerTrackEditor::IsActorPickable, ObjectBinding, Section) );
	}		

	// Actor selector to allow the user to choose a parent actor
	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>( "SceneOutliner" );

	TSharedRef< SWidget > MenuWidget = 
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.MaxDesiredHeight(400.0f)
			.WidthOverride(300.0f)
			[
				SceneOutlinerModule.CreateSceneOutliner(
					InitOptions,
					FOnActorPicked::CreateSP(this, &FActorPickerTrackEditor::ActorPicked, ObjectBinding, Section )
					)
			]
		]
	
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Top)
		.AutoWidth()
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ToolTipText( LOCTEXT( "PickButtonLabel", "Pick a parent actor to attach to") )
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.OnClicked(FOnClicked::CreateStatic(&Local::OnInteractiveActorPickerClicked, this, ObjectBinding, Section))
				.ContentPadding(4.0f)
				.ForegroundColor(FSlateColor::UseForeground())
				.IsFocusable(false)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.Button_PickActorInteractive"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		];

	MenuBuilder.AddWidget(MenuWidget, FText::GetEmpty(), false);
}

class SComponentChooserPopup : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam( FOnComponentChosen, FString );

	SLATE_BEGIN_ARGS( SComponentChooserPopup )
		: _Actor(NULL)
		{}

		/** An actor with components */
		SLATE_ARGUMENT( AActor*, Actor )

		/** Called when the text is chosen. */
		SLATE_EVENT( FOnComponentChosen, OnComponentChosen )

	SLATE_END_ARGS()

	/** Delegate to call when component is selected */
	FOnComponentChosen OnComponentChosen;

	/** List of tag names selected in the tag containers*/
	TArray< TSharedPtr<FString> > ComponentNames;

private:
	TSharedRef<ITableRow> MakeListViewWidget(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew( STableRow< TSharedPtr<FString> >, OwnerTable )
				[
					SNew(STextBlock) .Text( FText::FromString(*InItem.Get()) )
				];
	}

	void OnComponentSelected(TSharedPtr<FString> InItem, ESelectInfo::Type InSelectInfo)
	{
		FSlateApplication::Get().DismissAllMenus();

		if(OnComponentChosen.IsBound())
		{
			OnComponentChosen.Execute(*InItem.Get());
		}
	}

public:
	void Construct( const FArguments& InArgs )
	{
		OnComponentChosen = InArgs._OnComponentChosen;
		AActor* Actor = InArgs._Actor;

		TInlineComponentArray<USceneComponent*> Components(Actor);

		ComponentNames.Empty();
		for(USceneComponent* Component : Components)
		{
			if (Component->HasAnySockets())
			{
				ComponentNames.Add(MakeShareable(new FString(Component->GetName())));
			}
		}

		// Then make widget
		this->ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush(TEXT("Menu.Background")))
			.Padding(5)
			.Content()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 1.0f)
				[
					SNew(STextBlock)
					.Font( FEditorStyle::GetFontStyle(TEXT("SocketChooser.TitleFont")) )
					.Text( NSLOCTEXT("ComponentChooser", "ChooseComponentLabel", "Choose Component") )
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.MaxHeight(512)
				[
					SNew(SBox)
					.WidthOverride(256)
					.Content()
					[
						SNew(SListView< TSharedPtr<FString> >)
						.ListItemsSource( &ComponentNames)
						.OnGenerateRow( this, &SComponentChooserPopup::MakeListViewWidget )
						.OnSelectionChanged( this, &SComponentChooserPopup::OnComponentSelected )
					]
				]
			]
		];
	}
};

void FActorPickerTrackEditor::ActorPicked(AActor* ParentActor, FGuid ObjectGuid, UMovieSceneSection* Section)
{
	TArray<USceneComponent*> ComponentsWithSockets;
	if (ParentActor != NULL)
	{
		TInlineComponentArray<USceneComponent*> Components(ParentActor);

		for(USceneComponent* Component : Components)
		{
			if (Component->HasAnySockets())
			{
				ComponentsWithSockets.Add(Component);
			}
		}
	}

	if (ComponentsWithSockets.Num() == 0)
	{
		FSlateApplication::Get().DismissAllMenus();
		ActorSocketPicked( NAME_None, nullptr, ParentActor, ObjectGuid, Section );
		return;
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( "LevelEditor");
	TSharedPtr< ILevelEditor > LevelEditor = LevelEditorModule.GetFirstLevelEditor();

	TSharedPtr<SWidget> MenuWidget;

	if (ComponentsWithSockets.Num() > 1)
	{			
		MenuWidget = 
			SNew(SComponentChooserPopup)
			.Actor(ParentActor)
			.OnComponentChosen(this, &FActorPickerTrackEditor::ActorComponentPicked, ParentActor, ObjectGuid, Section);		

		// Create as context menu
		FSlateApplication::Get().PushMenu(
			LevelEditor.ToSharedRef(),
			FWidgetPath(),
			MenuWidget.ToSharedRef(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
			);
	}
	else
	{
		ActorComponentPicked(ComponentsWithSockets[0]->GetName(), ParentActor, ObjectGuid, Section);
	}
}


void FActorPickerTrackEditor::ActorComponentPicked(FString ComponentName, AActor* ParentActor, FGuid ObjectGuid, UMovieSceneSection* Section)
{
	USceneComponent* ComponentWithSockets = nullptr;
	TInlineComponentArray<USceneComponent*> Components(ParentActor);

	for(USceneComponent* Component : Components)
	{
		if (Component->GetName() == ComponentName)
		{
			ComponentWithSockets = Component;
			break;
		}
	}

	if (ComponentWithSockets == nullptr)
	{
		return;
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( "LevelEditor");
	TSharedPtr< ILevelEditor > LevelEditor = LevelEditorModule.GetFirstLevelEditor();

	TSharedPtr<SWidget> MenuWidget;

	MenuWidget = 
		SNew(SSocketChooserPopup)
		.SceneComponent(ComponentWithSockets)
		.OnSocketChosen(this, &FActorPickerTrackEditor::ActorSocketPicked, ComponentWithSockets, ParentActor, ObjectGuid, Section);		

	// Create as context menu
	FSlateApplication::Get().PushMenu(
		LevelEditor.ToSharedRef(),
		FWidgetPath(),
		MenuWidget.ToSharedRef(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
		);
}

#undef LOCTEXT_NAMESPACE
