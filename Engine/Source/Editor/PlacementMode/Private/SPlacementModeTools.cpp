// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SPlacementModeTools.h"
#include "Application/SlateApplicationBase.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "AssetThumbnail.h"
#include "LevelEditor.h"
#include "PlacementMode.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "EditorClassUtils.h"
#include "Widgets/Input/SSearchBox.h"


struct FSortPlaceableItems
{
	static bool SortItemsByOrderThenName(const TSharedPtr<FPlaceableItem>& A, const TSharedPtr<FPlaceableItem>& B)
	{
		if (A->SortOrder.IsSet())
		{
			if (B->SortOrder.IsSet())
			{
				return A->SortOrder.GetValue() < B->SortOrder.GetValue();
			}
			else
			{
				return true;
			}
		}
		else if (B->SortOrder.IsSet())
		{
			return false;
		}
		else
		{
			return SortItemsByName(A, B);
		}
	}

	static bool SortItemsByName(const TSharedPtr<FPlaceableItem>& A, const TSharedPtr<FPlaceableItem>& B)
	{
		return A->DisplayName.CompareTo(B->DisplayName) < 0;
	}
};

namespace PlacementViewFilter
{
	void GetBasicStrings(const FPlaceableItem& InPlaceableItem, TArray<FString>& OutBasicStrings)
	{
		OutBasicStrings.Add(InPlaceableItem.DisplayName.ToString());

		const FString* SourceString = FTextInspector::GetSourceString(InPlaceableItem.DisplayName);
		if (SourceString)
		{
			OutBasicStrings.Add(*SourceString);
		}
	}
} // namespace PlacementViewFilter

/**
 * These are the asset thumbnails.
 */
class SPlacementAssetThumbnail : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SPlacementAssetThumbnail )
		: _Width( 32 )
		, _Height( 32 )
		, _AlwaysUseGenericThumbnail( false )
		, _AssetTypeColorOverride()
	{}

	SLATE_ARGUMENT( uint32, Width )

	SLATE_ARGUMENT( uint32, Height )

	SLATE_ARGUMENT( FName, ClassThumbnailBrushOverride )

	SLATE_ARGUMENT( bool, AlwaysUseGenericThumbnail )

	SLATE_ARGUMENT( TOptional<FLinearColor>, AssetTypeColorOverride )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const FAssetData& InAsset)
	{
		Asset = InAsset;

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>( "LevelEditor" );
		TSharedPtr<FAssetThumbnailPool> ThumbnailPool = LevelEditorModule.GetFirstLevelEditor()->GetThumbnailPool();

		Thumbnail = MakeShareable(new FAssetThumbnail(Asset, InArgs._Width, InArgs._Height, ThumbnailPool));

		FAssetThumbnailConfig Config;
		Config.bForceGenericThumbnail = InArgs._AlwaysUseGenericThumbnail;
		Config.ClassThumbnailBrushOverride = InArgs._ClassThumbnailBrushOverride;
		Config.AssetTypeColorOverride = InArgs._AssetTypeColorOverride;
		ChildSlot
		[
			Thumbnail->MakeThumbnailWidget( Config )
		];
	}

private:

	FAssetData Asset;
	TSharedPtr< FAssetThumbnail > Thumbnail;
};

void SPlacementAssetEntry::Construct(const FArguments& InArgs, const TSharedPtr<const FPlaceableItem>& InItem)
{	
	bIsPressed = false;

	Item = InItem;

	TSharedPtr< SHorizontalBox > ActorType = SNew( SHorizontalBox );

	const bool bIsClass = Item->AssetData.GetClass() == UClass::StaticClass();
	const bool bIsActor = bIsClass ? CastChecked<UClass>(Item->AssetData.GetAsset())->IsChildOf(AActor::StaticClass()) : false;

	AActor* DefaultActor = nullptr;
	if (Item->Factory != nullptr)
	{
		DefaultActor = Item->Factory->GetDefaultActor(Item->AssetData);
	}
	else if (bIsActor)
	{
		DefaultActor = CastChecked<AActor>(CastChecked<UClass>(Item->AssetData.GetAsset())->ClassDefaultObject);
	}

	UClass* DocClass = nullptr;
	TSharedPtr<IToolTip> AssetEntryToolTip;
	if(DefaultActor != nullptr)
	{
		DocClass = DefaultActor->GetClass();
		AssetEntryToolTip = FEditorClassUtils::GetTooltip(DefaultActor->GetClass());
	}

	if (!AssetEntryToolTip.IsValid())
	{
		AssetEntryToolTip = FSlateApplicationBase::Get().MakeToolTip(Item->DisplayName);
	}
	
	const FButtonStyle& ButtonStyle = FEditorStyle::GetWidgetStyle<FButtonStyle>( "PlacementBrowser.Asset" );

	NormalImage = &ButtonStyle.Normal;
	HoverImage = &ButtonStyle.Hovered;
	PressedImage = &ButtonStyle.Pressed; 

	// Create doc link widget if there is a class to link to
	TSharedRef<SWidget> DocWidget = SNew(SSpacer);
	if(DocClass != NULL)
	{
		DocWidget = FEditorClassUtils::GetDocumentationLinkWidget(DocClass);
		DocWidget->SetCursor( EMouseCursor::Default );
	}

	ChildSlot
	[
		SNew( SBorder )
		.BorderImage( this, &SPlacementAssetEntry::GetBorder )
		.Cursor( EMouseCursor::GrabHand )
		.ToolTip( AssetEntryToolTip )
		[
			SNew( SHorizontalBox )

			+ SHorizontalBox::Slot()
			.Padding( 0 )
			.AutoWidth()
			[
				// Drop shadow border
				SNew( SBorder )
				.Padding( 4 )
				.BorderImage( FEditorStyle::GetBrush( "ContentBrowser.ThumbnailShadow" ) )
				[
					SNew( SBox )
					.WidthOverride( 35 )
					.HeightOverride( 35 )
					[
						SNew( SPlacementAssetThumbnail, Item->AssetData )
						.ClassThumbnailBrushOverride( Item->ClassThumbnailBrushOverride )
						.AlwaysUseGenericThumbnail( Item->bAlwaysUseGenericThumbnail )
						.AssetTypeColorOverride( Item->AssetTypeColorOverride )
					]
				]
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(2, 0, 4, 0)
			[
				SNew( SVerticalBox )
				+SVerticalBox::Slot()
				.Padding(0, 0, 0, 1)
				.AutoHeight()
				[
					SNew( STextBlock )
					.TextStyle( FEditorStyle::Get(), "PlacementBrowser.Asset.Name" )
					.Text( Item->DisplayName )
					.HighlightText(InArgs._HighlightText)
				]
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				DocWidget
			]
		]
	];
}

FReply SPlacementAssetEntry::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		bIsPressed = true;

		return FReply::Handled().DetectDrag( SharedThis( this ), MouseEvent.GetEffectingButton() );
	}

	return FReply::Unhandled();
}

FReply SPlacementAssetEntry::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		bIsPressed = false;
	}

	return FReply::Unhandled();
}

FReply SPlacementAssetEntry::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bIsPressed = false;

	if (FEditorDelegates::OnAssetDragStarted.IsBound())
	{
		TArray<FAssetData> DraggedAssetDatas;
		DraggedAssetDatas.Add( Item->AssetData );
		FEditorDelegates::OnAssetDragStarted.Broadcast( DraggedAssetDatas, Item->Factory );
		return FReply::Handled();
	}

	if( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
	{
		return FReply::Handled().BeginDragDrop( FAssetDragDropOp::New( Item->AssetData, Item->Factory ) );
	}
	else
	{
		return FReply::Handled();
	}
}

bool SPlacementAssetEntry::IsPressed() const
{
	return bIsPressed;
}

const FSlateBrush* SPlacementAssetEntry::GetBorder() const
{
	if ( IsPressed() )
	{
		return PressedImage;
	}
	else if ( IsHovered() )
	{
		return HoverImage;
	}
	else
	{
		return NormalImage;
	}
}

SPlacementModeTools::~SPlacementModeTools()
{
	if ( IPlacementModeModule::IsAvailable() )
	{
		IPlacementModeModule::Get().OnRecentlyPlacedChanged().RemoveAll( this );
		IPlacementModeModule::Get().OnAllPlaceableAssetsChanged().RemoveAll( this );
	}
}

void SPlacementModeTools::Construct( const FArguments& InArgs )
{
	bPlaceablesFullRefreshRequested = false;
	bRecentlyPlacedRefreshRequested = false;
	bNeedsUpdate = true;

	FPlacementMode* PlacementEditMode = (FPlacementMode*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_Placement );
	PlacementEditMode->AddValidFocusTargetForPlacement( SharedThis( this ) );

	SearchTextFilter = MakeShareable(new FPlacementAssetEntryTextFilter(
		FPlacementAssetEntryTextFilter::FItemToStringArray::CreateStatic(&PlacementViewFilter::GetBasicStrings)
		));

	TSharedRef<SVerticalBox> Tabs = SNew(SVerticalBox).Visibility(this, &SPlacementModeTools::GetTabsVisibility);

	// Populate the tabs and body from the defined placeable items
	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();

	TArray<FPlacementCategoryInfo> Categories;
	PlacementModeModule.GetSortedCategories(Categories);
	for (const FPlacementCategoryInfo& Category : Categories)
	{
		Tabs->AddSlot()
		.AutoHeight()
		[
			CreatePlacementGroupTab(Category)
		];
	}

	TSharedRef<SScrollBar> ScrollBar = SNew(SScrollBar)
		.Thickness(FVector2D(5, 5));

	ChildSlot
	[
		SNew( SVerticalBox )

		+ SVerticalBox::Slot()
		.Padding(4)
		.AutoHeight()
		[
			SAssignNew( SearchBoxPtr, SSearchBox )
			.HintText(NSLOCTEXT("PlacementMode", "SearchPlaceables", "Search Classes"))
			.OnTextChanged(this, &SPlacementModeTools::OnSearchChanged)
			.OnTextCommitted(this, &SPlacementModeTools::OnSearchCommitted)
		]

		+ SVerticalBox::Slot()
		.Padding( 0 )
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				Tabs
			]

			+ SHorizontalBox::Slot()
			[
				SNew(SBorder)
				.Padding(FMargin(3))
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				[
					SNew(SOverlay)

					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Fill)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("PlacementMode", "NoResultsFound", "No Results Found"))
						.Visibility(this, &SPlacementModeTools::GetFailedSearchVisibility)
					]

					+ SOverlay::Slot()
					[
						SAssignNew(CustomContent, SBox)
					]

					+ SOverlay::Slot()
					[
						SAssignNew(DataDrivenContent, SBox)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							[
								SAssignNew(ListView, SListView<TSharedPtr<FPlaceableItem>>)
								.ListItemsSource(&FilteredItems)
								.OnGenerateRow(this, &SPlacementModeTools::OnGenerateWidgetForItem)
								.ExternalScrollbar(ScrollBar)
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								ScrollBar
							]
						]
					]
				]
			]
		]
	];

	ActiveTabName = FBuiltInPlacementCategories::Basic();
	bNeedsUpdate = true;

	PlacementModeModule.OnRecentlyPlacedChanged().AddSP( this, &SPlacementModeTools::UpdateRecentlyPlacedAssets );
	PlacementModeModule.OnAllPlaceableAssetsChanged().AddSP( this, &SPlacementModeTools::UpdatePlaceableAssets );
}

TSharedRef< SWidget > SPlacementModeTools::CreatePlacementGroupTab( const FPlacementCategoryInfo& Info )
{
	return SNew( SCheckBox )
	.Style( FEditorStyle::Get(), "PlacementBrowser.Tab" )
	.OnCheckStateChanged( this, &SPlacementModeTools::OnPlacementTabChanged, Info.UniqueHandle )
	.IsChecked( this, &SPlacementModeTools::GetPlacementTabCheckedState, Info.UniqueHandle )
	[
		SNew( SOverlay )

		+ SOverlay::Slot()
		.VAlign( VAlign_Center )
		[
			SNew(SSpacer)
			.Size( FVector2D( 1, 30 ) )
		]

		+ SOverlay::Slot()
		.Padding( FMargin(6, 0, 15, 0) )
		.VAlign( VAlign_Center )
		[
			SNew( STextBlock )
			.TextStyle( FEditorStyle::Get(), "PlacementBrowser.Tab.Text" )
			.Text( Info.DisplayName )
		]

		+ SOverlay::Slot()
		.VAlign( VAlign_Fill )
		.HAlign( HAlign_Left )
		[
			SNew(SImage)
			.Image( this, &SPlacementModeTools::PlacementGroupBorderImage, Info.UniqueHandle )
		]
	];
}

FName SPlacementModeTools::GetActiveTab() const
{
	return IsSearchActive() ? FBuiltInPlacementCategories::AllClasses() : ActiveTabName;
}

void SPlacementModeTools::UpdateFilteredItems()
{
	bNeedsUpdate = false;

	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();

	const FPlacementCategoryInfo* Category = PlacementModeModule.GetRegisteredPlacementCategory(GetActiveTab());
	if (!Category)
	{
		return;
	}
	else if (Category->CustomGenerator)
	{
		CustomContent->SetContent(Category->CustomGenerator());

		CustomContent->SetVisibility(EVisibility::Visible);
		DataDrivenContent->SetVisibility(EVisibility::Collapsed);
	}
	else
	{
		FilteredItems.Reset();

		const FPlacementCategoryInfo* CategoryInfo = PlacementModeModule.GetRegisteredPlacementCategory(GetActiveTab());
		if (!ensure(CategoryInfo))
		{
			return;
		}
		
		if (IsSearchActive())
		{
			auto Filter = [&](const TSharedPtr<FPlaceableItem>& Item) { return SearchTextFilter->PassesFilter(*Item); };
			PlacementModeModule.GetFilteredItemsForCategory(CategoryInfo->UniqueHandle, FilteredItems, Filter);
			
			if (CategoryInfo->bSortable)
			{
				FilteredItems.Sort(&FSortPlaceableItems::SortItemsByName);
			}
		}
		else
		{
			PlacementModeModule.GetItemsForCategory(CategoryInfo->UniqueHandle, FilteredItems);

			if (CategoryInfo->bSortable)
			{
				FilteredItems.Sort(&FSortPlaceableItems::SortItemsByOrderThenName);
			}
		}

		CustomContent->SetVisibility(EVisibility::Collapsed);
		DataDrivenContent->SetVisibility(EVisibility::Visible);
		ListView->RequestListRefresh();
	}
}

bool SPlacementModeTools::IsSearchActive() const
{
	return !SearchTextFilter->GetRawFilterText().IsEmpty();
}

ECheckBoxState SPlacementModeTools::GetPlacementTabCheckedState( FName CategoryName ) const
{
	return ActiveTabName == CategoryName ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

EVisibility SPlacementModeTools::GetFailedSearchVisibility() const
{
	if (!IsSearchActive() || FilteredItems.Num())
	{
		return EVisibility::Collapsed;
	}
	return EVisibility::Visible;
}

EVisibility SPlacementModeTools::GetTabsVisibility() const
{
	return IsSearchActive() ? EVisibility::Collapsed : EVisibility::Visible;
}

TSharedRef<ITableRow> SPlacementModeTools::OnGenerateWidgetForItem(TSharedPtr<FPlaceableItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FPlaceableItem>>, OwnerTable)
		[
			SNew(SPlacementAssetEntry, InItem.ToSharedRef())
			.HighlightText(this, &SPlacementModeTools::GetHighlightText)
		];
}

void SPlacementModeTools::OnPlacementTabChanged( ECheckBoxState NewState, FName CategoryName )
{
	if ( NewState == ECheckBoxState::Checked )
	{
		ActiveTabName = CategoryName;
		IPlacementModeModule::Get().RegenerateItemsForCategory(ActiveTabName);

		bNeedsUpdate = true;
	}
}

const FSlateBrush* SPlacementModeTools::PlacementGroupBorderImage( FName CategoryName ) const
{
	if ( ActiveTabName == CategoryName )
	{
		static FName PlacementBrowserActiveTabBarBrush( "PlacementBrowser.ActiveTabBar" );
		return FEditorStyle::GetBrush( PlacementBrowserActiveTabBarBrush );
	}
	else
	{
		return nullptr;
	}
}

void SPlacementModeTools::UpdateRecentlyPlacedAssets( const TArray< FActorPlacementInfo >& RecentlyPlaced )
{
	if (GetActiveTab() == FBuiltInPlacementCategories::RecentlyPlaced())
	{
		bRecentlyPlacedRefreshRequested = true;
	}
}

void SPlacementModeTools::UpdatePlaceableAssets()
{
	if (GetActiveTab() == FBuiltInPlacementCategories::AllClasses())
	{
		bPlaceablesFullRefreshRequested = true;
	}
}

void SPlacementModeTools::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if ( bPlaceablesFullRefreshRequested )
	{
		IPlacementModeModule::Get().RegenerateItemsForCategory(FBuiltInPlacementCategories::AllClasses());
		bPlaceablesFullRefreshRequested = false;
		bNeedsUpdate = true;
	}

	if ( bRecentlyPlacedRefreshRequested )
	{
		IPlacementModeModule::Get().RegenerateItemsForCategory(FBuiltInPlacementCategories::RecentlyPlaced());
		bRecentlyPlacedRefreshRequested = false;
		bNeedsUpdate = true;
	}

	if ( bNeedsUpdate )
	{
		UpdateFilteredItems();
	}
}

FReply SPlacementModeTools::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	FReply Reply = FReply::Unhandled();

	if ( InKeyEvent.GetKey() == EKeys::Escape )
	{
		FPlacementMode* PlacementEditMode = (FPlacementMode*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_Placement );
		PlacementEditMode->StopPlacing();
		Reply = FReply::Handled();
	}

	return Reply;
}

void SPlacementModeTools::OnSearchChanged(const FText& InFilterText)
{
	// If the search text was previously empty we do a full rebuild of our cached widgets
	// for the placeable widgets.
	if ( !IsSearchActive() )
	{
		bPlaceablesFullRefreshRequested = true;
	}
	else
	{
		bNeedsUpdate = true;
	}

	SearchTextFilter->SetRawFilterText( InFilterText );
	SearchBoxPtr->SetError( SearchTextFilter->GetFilterErrorText() );
}

void SPlacementModeTools::OnSearchCommitted(const FText& InFilterText, ETextCommit::Type InCommitType)
{
	OnSearchChanged(InFilterText);
}

FText SPlacementModeTools::GetHighlightText() const
{
	return SearchTextFilter->GetRawFilterText();
}
