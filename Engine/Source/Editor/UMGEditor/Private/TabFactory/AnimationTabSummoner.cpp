// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TabFactory/AnimationTabSummoner.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MovieScene.h"
#include "Animation/WidgetAnimation.h"
#include "WidgetBlueprint.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"

#if WITH_EDITOR
	#include "EditorStyleSet.h"
#endif // WITH_EDITOR
#include "Blueprint/WidgetTree.h"

#include "UMGStyle.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Framework/Commands/GenericCommands.h"
#include "ScopedTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/TextFilter.h"
#include "Kismet2/Kismet2NameValidators.h"

#define LOCTEXT_NAMESPACE "UMG"

const FName FAnimationTabSummoner::TabID(TEXT("Animations"));

FAnimationTabSummoner::FAnimationTabSummoner(TSharedPtr<class FWidgetBlueprintEditor> InBlueprintEditor)
		: FWorkflowTabFactory(TabID, InBlueprintEditor)
		, BlueprintEditor(InBlueprintEditor)
{
	TabLabel = LOCTEXT("AnimationsTabLabel", "Animations");
	TabIcon = FSlateIcon(FUMGStyle::GetStyleSetName(), "Animations.TabIcon");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("Animations_ViewMenu_Desc", "Animations");
	ViewMenuTooltip = LOCTEXT("Animations_ViewMenu_ToolTip", "Opens a tab to manage animations");
}


bool VerifyAnimationRename( FWidgetBlueprintEditor& BlueprintEditor, UWidgetAnimation* Animation, FString NewAnimationName, FText& OutErrorMessage )
{
	UWidgetBlueprint* Blueprint = BlueprintEditor.GetWidgetBlueprintObj();
	if (Blueprint && FindObject<UWidgetAnimation>( Blueprint, *NewAnimationName, true ) )
	{
		OutErrorMessage = LOCTEXT( "NameInUseByAnimation", "An animation with this name already exists" );
		return false;
	}

	FName NewAnimationNameAsName( *NewAnimationName );
	if ( Blueprint && Blueprint->WidgetTree->FindWidget<UWidget>( NewAnimationNameAsName ) != nullptr )
	{
		OutErrorMessage = LOCTEXT( "NameInUseByWidget", "A widget with this name already exists" );
		return false;
	}

	UUserWidget* PreviewWidget = BlueprintEditor.GetPreview();
	FName FunctionName(*NewAnimationName);
	if (PreviewWidget && PreviewWidget->FindFunction(FunctionName))
	{
		OutErrorMessage = LOCTEXT("NameInUseByFunction", "A function with this name already exists");
		return false;
	}

	FKismetNameValidator Validator( Blueprint );
	EValidatorResult ValidationResult = Validator.IsValid( NewAnimationName );

	if ( ValidationResult != EValidatorResult::Ok )
	{
		FString ErrorString = FKismetNameValidator::GetErrorString( NewAnimationName, ValidationResult );
		OutErrorMessage = FText::FromString( ErrorString );
		return false;
	}

	return true;
}


struct FWidgetAnimationListItem
{
	FWidgetAnimationListItem(UWidgetAnimation* InAnimation, bool bInRenameRequestPending = false, bool bInNewAnimation = false )
		: Animation(InAnimation)
		, bRenameRequestPending(bInRenameRequestPending)
		, bNewAnimation( bInNewAnimation )
	{}

	UWidgetAnimation* Animation;
	bool bRenameRequestPending;
	bool bNewAnimation;
};


typedef SListView<TSharedPtr<FWidgetAnimationListItem> > SWidgetAnimationListView;

class SWidgetAnimationListItem : public STableRow<TSharedPtr<FWidgetAnimationListItem> >
{
public:
	SLATE_BEGIN_ARGS( SWidgetAnimationListItem ){}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor, TSharedPtr<FWidgetAnimationListItem> InListItem )
	{
		ListItem = InListItem;
		BlueprintEditor = InBlueprintEditor;

		STableRow<TSharedPtr<FWidgetAnimationListItem>>::Construct(
			STableRow<TSharedPtr<FWidgetAnimationListItem>>::FArguments()
			.Padding( FMargin( 3.0f, 2.0f) )
			.Content()
			[
				SAssignNew(InlineTextBlock, SInlineEditableTextBlock)
				.Font(FCoreStyle::Get().GetFontStyle("NormalFont"))
				.Text(this, &SWidgetAnimationListItem::GetMovieSceneText)
				//.HighlightText(InArgs._HighlightText)
				.OnVerifyTextChanged(this, &SWidgetAnimationListItem::OnVerifyNameTextChanged)
				.OnTextCommitted(this, &SWidgetAnimationListItem::OnNameTextCommited)
				.IsSelected(this, &SWidgetAnimationListItem::IsSelectedExclusively)
			],
			InOwnerTableView);
	}

	void BeginRename()
	{
		InlineTextBlock->EnterEditingMode();
	}

private:
	FText GetMovieSceneText() const
	{
		if( ListItem.IsValid() )
		{
			return FText::FromString( ListItem.Pin()->Animation->GetName() );
		}

		return FText::GetEmpty();
	}

	bool OnVerifyNameTextChanged(const FText& InText, FText& OutErrorMessage)
	{
		UWidgetAnimation* Animation = ListItem.Pin()->Animation;

		FString NewName = InText.ToString();

		if ( Animation->GetName() != NewName )
		{
			TSharedPtr<FWidgetBlueprintEditor> Editor = BlueprintEditor.Pin();
			return Editor.IsValid() && VerifyAnimationRename( *Editor, Animation, NewName, OutErrorMessage );
		}

		return true;
	}

	void OnNameTextCommited(const FText& InText, ETextCommit::Type CommitInfo)
	{
		UWidgetAnimation* WidgetAnimation = ListItem.Pin()->Animation;
		UWidgetBlueprint* Blueprint = BlueprintEditor.Pin()->GetWidgetBlueprintObj();

		FName CurrentName = WidgetAnimation->GetFName();

		bool bNewAnimation = ListItem.Pin()->bNewAnimation;

		if(!CurrentName.ToString().Equals(InText.ToString()) && !InText.IsEmpty())
		{
			FText TransactionName = bNewAnimation ? LOCTEXT("NewAnimation", "New Animation") : LOCTEXT("RenameAnimation", "Rename Animation");

			{
				const FScopedTransaction Transaction(TransactionName);
				WidgetAnimation->Modify();
				WidgetAnimation->GetMovieScene()->Modify();

				WidgetAnimation->Rename(*InText.ToString());
				WidgetAnimation->GetMovieScene()->Rename(*InText.ToString());

				if(bNewAnimation)
				{
					Blueprint->Modify();
					Blueprint->Animations.Add(WidgetAnimation);
					ListItem.Pin()->bNewAnimation = false;
				}
			}

			FBlueprintEditorUtils::ReplaceVariableReferences(Blueprint, CurrentName, FName(*InText.ToString()));

			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
		else if( bNewAnimation )
		{
			const FScopedTransaction Transaction(LOCTEXT("NewAnimation", "New Animation"));
			Blueprint->Modify();
			Blueprint->Animations.Add(WidgetAnimation);
			ListItem.Pin()->bNewAnimation = false;
		}
	}
private:
	TWeakPtr<FWidgetAnimationListItem> ListItem;
	TWeakPtr<FWidgetBlueprintEditor> BlueprintEditor;
	TSharedPtr<SInlineEditableTextBlock> InlineTextBlock;
};


class SUMGAnimationList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SUMGAnimationList	) {}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor )
	{
		BlueprintEditor = InBlueprintEditor;

		InBlueprintEditor->GetOnWidgetBlueprintTransaction().AddSP( this, &SUMGAnimationList::OnWidgetBlueprintTransaction );
		InBlueprintEditor->OnEnterWidgetDesigner.AddSP(this, &SUMGAnimationList::OnEnteringDesignerMode);

		SAssignNew(AnimationListView, SWidgetAnimationListView)
			.ItemHeight(20.0f)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SUMGAnimationList::OnGenerateWidgetForMovieScene)
			.OnItemScrolledIntoView(this, &SUMGAnimationList::OnItemScrolledIntoView)
			.OnSelectionChanged(this, &SUMGAnimationList::OnSelectionChanged)
			.OnContextMenuOpening(this, &SUMGAnimationList::OnContextMenuOpening)
			.ListItemsSource(&Animations);

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding( 2 )
				.AutoHeight()
				[
					SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					.Padding(0)
					.VAlign( VAlign_Center )
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
						.ForegroundColor(FEditorStyle::Get().GetSlateColor("Foreground"))
						.ContentPadding(FMargin(2.0f, 1.0f))
						.OnClicked( this, &SUMGAnimationList::OnNewAnimationClicked )
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew( SHorizontalBox )
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.AutoWidth()
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "NormalText.Important")
								.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
								.Text(FText::FromString(FString(TEXT("\xf067"))) /*fa-plus*/)
							]

							+ SHorizontalBox::Slot()
							.Padding( 2.0f, 0.0f )
							[
								SNew( STextBlock )
								.TextStyle(FEditorStyle::Get(), "NormalText.Important")
								.Text( LOCTEXT("NewAnimationButtonText", "Animation") )
							]
						]
					]
					+ SHorizontalBox::Slot()
					.Padding(2.0f, 0.0f)
					.VAlign( VAlign_Center )
					[
						SAssignNew(SearchBoxPtr, SSearchBox)
						.HintText(LOCTEXT("Search Animations", "Search Animations"))
						.OnTextChanged(this, &SUMGAnimationList::OnSearchChanged)
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SScrollBorder, AnimationListView.ToSharedRef())
					[
						AnimationListView.ToSharedRef()
					]
				]
			]
		];

		UpdateAnimationList();

		CreateCommandList();
	}

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
	{
		FReply Reply = FReply::Unhandled();
		if( CommandList->ProcessCommandBindings( InKeyEvent ) )
		{
			Reply = FReply::Handled();
		}

		return Reply;
	}

private:

	void UpdateAnimationList()
	{
		Animations.Empty();

		const TArray<UWidgetAnimation*>& WidgetAnimations = BlueprintEditor.Pin()->GetWidgetBlueprintObj()->Animations;

		for( UWidgetAnimation* Animation : WidgetAnimations )
		{
			Animations.Add( MakeShareable( new FWidgetAnimationListItem( Animation ) ) );
		}
		
		AnimationListView->RequestListRefresh();
	}

	void OnEnteringDesignerMode()
	{
		UpdateAnimationList();


		TSharedPtr<FWidgetBlueprintEditor> WidgetBlueprintEditorPin = BlueprintEditor.Pin();
		const UWidgetAnimation* ViewedAnim = WidgetBlueprintEditorPin->RefreshCurrentAnimation();

		if (ViewedAnim)
		{
			const TSharedPtr<FWidgetAnimationListItem>* FoundListItemPtr = Animations.FindByPredicate([&](const TSharedPtr<FWidgetAnimationListItem>& ListItem) { return ListItem->Animation == ViewedAnim; });

			if (FoundListItemPtr != nullptr)
			{
				AnimationListView->SetSelection(*FoundListItemPtr);
			}
		}

		UWidgetAnimation* CurrentAnim = WidgetBlueprintEditorPin->GetCurrentAnimation();
		WidgetBlueprintEditorPin->ChangeViewedAnimation(*CurrentAnim);
	}

	void OnWidgetBlueprintTransaction()
	{
		UpdateAnimationList();

		
		TSharedPtr<FWidgetBlueprintEditor> WidgetBlueprintEditorPin = BlueprintEditor.Pin();
		const UWidgetAnimation* ViewedAnim = WidgetBlueprintEditorPin->RefreshCurrentAnimation();

		if(ViewedAnim)
		{
			const TSharedPtr<FWidgetAnimationListItem>* FoundListItemPtr = Animations.FindByPredicate( [&](const TSharedPtr<FWidgetAnimationListItem>& ListItem ) { return ListItem->Animation == ViewedAnim; } );

			if (FoundListItemPtr != nullptr)
			{
				AnimationListView->SetSelection(*FoundListItemPtr);
			}
		}

	}

	void OnItemScrolledIntoView( TSharedPtr<FWidgetAnimationListItem> InListItem, const TSharedPtr<ITableRow>& InWidget ) const
	{
		if( InListItem->bRenameRequestPending )
		{
			StaticCastSharedPtr<SWidgetAnimationListItem>( InWidget )->BeginRename();
			InListItem->bRenameRequestPending = false;
		}
	}

	FReply OnNewAnimationClicked()
	{
		const float InTime = 0.f;
		const float OutTime = 5.0f;

		TSharedPtr<FWidgetBlueprintEditor> Editor = BlueprintEditor.Pin();
		if (!Editor.IsValid())
		{
			return FReply::Handled();
		}

		UWidgetBlueprint* WidgetBlueprint = Editor->GetWidgetBlueprintObj();

		FString BaseName = "NewAnimation";
		UWidgetAnimation* NewAnimation = NewObject<UWidgetAnimation>(WidgetBlueprint, *BaseName, RF_Transactional);

		FString UniqueName = BaseName;
		int32 NameIndex = 1;
		FText Unused;
		while ( VerifyAnimationRename( *Editor, NewAnimation, UniqueName, Unused ) == false )
		{
			UniqueName = FString::Printf( TEXT( "%s_%i" ), *BaseName, NameIndex );
			NameIndex++;
		}
		NewAnimation->Rename( *UniqueName );

		NewAnimation->MovieScene = NewObject<UMovieScene>(NewAnimation, NewAnimation->GetFName(), RF_Transactional);

		NewAnimation->MovieScene->SetPlaybackRange(InTime, OutTime);
		NewAnimation->MovieScene->GetEditorData().WorkingRange = TRange<float>(InTime, OutTime);

		bool bRequestRename = true;
		bool bNewAnimation = true;

		int32 NewIndex = Animations.Add( MakeShareable( new FWidgetAnimationListItem( NewAnimation, bRequestRename, bNewAnimation ) ) );

		AnimationListView->RequestScrollIntoView( Animations[NewIndex] );

		return FReply::Handled();
	}

	void OnSearchChanged( const FText& InSearchText )
	{
		const TArray<UWidgetAnimation*>& WidgetAnimations = BlueprintEditor.Pin()->GetWidgetBlueprintObj()->Animations;

		if( !InSearchText.IsEmpty() )
		{
			struct Local
			{
				static void UpdateFilterStrings(UWidgetAnimation* InAnimation, OUT TArray< FString >& OutFilterStrings)
				{
					OutFilterStrings.Add(InAnimation->GetName());
				}
			};

			TTextFilter<UWidgetAnimation*> TextFilter(TTextFilter<UWidgetAnimation*>::FItemToStringArray::CreateStatic(&Local::UpdateFilterStrings));

			TextFilter.SetRawFilterText(InSearchText);
			SearchBoxPtr->SetError(TextFilter.GetFilterErrorText());

			Animations.Reset();

			for(UWidgetAnimation* Animation : WidgetAnimations)
			{
				if(TextFilter.PassesFilter(Animation))
				{
					Animations.Add(MakeShareable(new FWidgetAnimationListItem(Animation)));
				}
			}

			AnimationListView->RequestListRefresh();
		}
		else
		{
			SearchBoxPtr->SetError(FText::GetEmpty());

			// Just regenerate the whole list
			UpdateAnimationList();
		}
	}

	void OnSelectionChanged( TSharedPtr<FWidgetAnimationListItem> InSelectedItem, ESelectInfo::Type SelectionInfo )
	{
		UWidgetAnimation* WidgetAnimation;
		if( InSelectedItem.IsValid() )
		{
			WidgetAnimation = InSelectedItem->Animation;
		}
		else
		{
			WidgetAnimation = UWidgetAnimation::GetNullAnimation();
		}

		const UWidgetAnimation* CurrentWidgetAnimation = BlueprintEditor.Pin()->RefreshCurrentAnimation();
		if (WidgetAnimation != CurrentWidgetAnimation)
		{
			BlueprintEditor.Pin()->ChangeViewedAnimation( *WidgetAnimation );
		}
	}

	TSharedPtr<SWidget> OnContextMenuOpening() const
	{
		FMenuBuilder MenuBuilder( true, CommandList.ToSharedRef() );

		MenuBuilder.BeginSection("Edit", LOCTEXT("Edit", "Edit"));
		{
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename);
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Duplicate);
			MenuBuilder.AddMenuSeparator();

			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	TSharedRef<ITableRow> OnGenerateWidgetForMovieScene( TSharedPtr<FWidgetAnimationListItem> InListItem, const TSharedRef< STableViewBase >& InOwnerTableView )
	{
		return SNew( SWidgetAnimationListItem, InOwnerTableView, BlueprintEditor.Pin(), InListItem );
	}

	void CreateCommandList()
	{
		CommandList = MakeShareable(new FUICommandList);

		CommandList->MapAction(
			FGenericCommands::Get().Duplicate,
			FExecuteAction::CreateSP(this, &SUMGAnimationList::OnDuplicateAnimation),
			FCanExecuteAction::CreateSP(this, &SUMGAnimationList::CanExecuteContextMenuAction)
			);

		CommandList->MapAction(
			FGenericCommands::Get().Delete,
			FExecuteAction::CreateSP(this, &SUMGAnimationList::OnDeleteAnimation),
			FCanExecuteAction::CreateSP(this, &SUMGAnimationList::CanExecuteContextMenuAction)
			);

		CommandList->MapAction(
			FGenericCommands::Get().Rename,
			FExecuteAction::CreateSP(this, &SUMGAnimationList::OnRenameAnimation),
			FCanExecuteAction::CreateSP(this, &SUMGAnimationList::CanExecuteContextMenuAction)
			);
	}

	bool CanExecuteContextMenuAction() const
	{
		return AnimationListView->GetNumItemsSelected() == 1 && !BlueprintEditor.Pin()->InDebuggingMode();
	}

	void OnDuplicateAnimation()
	{
		TArray< TSharedPtr<FWidgetAnimationListItem> > SelectedAnimations = AnimationListView->GetSelectedItems();
		check(SelectedAnimations.Num() == 1);

		TSharedPtr<FWidgetAnimationListItem> SelectedAnimation = SelectedAnimations[0];

		UWidgetBlueprint* WidgetBlueprint = BlueprintEditor.Pin()->GetWidgetBlueprintObj();

		UWidgetAnimation* NewAnimation = 
			DuplicateObject<UWidgetAnimation>
			(
				SelectedAnimation->Animation,
				WidgetBlueprint, 
				MakeUniqueObjectName( WidgetBlueprint, UWidgetAnimation::StaticClass(), SelectedAnimation->Animation->GetFName() )
			);
	
		NewAnimation->MovieScene->Rename(*NewAnimation->GetName(), nullptr, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);


		bool bRenameRequestPending = true;
		bool bNewAnimation = true;
		int32 NewIndex = Animations.Add(MakeShareable(new FWidgetAnimationListItem(NewAnimation,bRenameRequestPending,bNewAnimation)));

		AnimationListView->RequestScrollIntoView(Animations[NewIndex]);
	}


	void OnDeleteAnimation()
	{
		TArray< TSharedPtr<FWidgetAnimationListItem> > SelectedAnimations = AnimationListView->GetSelectedItems();
		check(SelectedAnimations.Num() == 1);

		TSharedPtr<FWidgetAnimationListItem> SelectedAnimation = SelectedAnimations[0];

		TSharedPtr<FWidgetBlueprintEditor> WidgetBlueprintEditorPin = BlueprintEditor.Pin();

		UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintEditorPin->GetWidgetBlueprintObj();

		TArray<UWidgetAnimation*>& WidgetAnimations = WidgetBlueprint->Animations;

		{
			const FScopedTransaction Transaction(LOCTEXT("DeleteAnimationTransaction", "Delete Animation"));
			WidgetBlueprint->Modify();
			// Rename the animation and move it to the transient package to avoid collisions.
			SelectedAnimation->Animation->Rename( NULL, GetTransientPackage() );
			WidgetAnimations.Remove(SelectedAnimation->Animation);

			UpdateAnimationList();
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

		WidgetBlueprintEditorPin->ChangeViewedAnimation(*UWidgetAnimation::GetNullAnimation());
	}

	void OnRenameAnimation()
	{
		TArray< TSharedPtr<FWidgetAnimationListItem> > SelectedAnimations = AnimationListView->GetSelectedItems();
		check( SelectedAnimations.Num() == 1 );

		TSharedPtr<FWidgetAnimationListItem> SelectedAnimation = SelectedAnimations[0];
		SelectedAnimation->bRenameRequestPending = true;

		AnimationListView->RequestScrollIntoView( SelectedAnimation );
	}

private:
	TSharedPtr<FUICommandList> CommandList;
	TWeakPtr<FWidgetBlueprintEditor> BlueprintEditor;
	TSharedPtr<SWidgetAnimationListView> AnimationListView;
	TArray< TSharedPtr<FWidgetAnimationListItem> > Animations;
	TSharedPtr<SSearchBox> SearchBoxPtr;
};


TSharedRef<SWidget> FAnimationTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FWidgetBlueprintEditor> BlueprintEditorPinned = BlueprintEditor.Pin();

	return SNew( SUMGAnimationList, BlueprintEditorPinned );
	
}

#undef LOCTEXT_NAMESPACE 
