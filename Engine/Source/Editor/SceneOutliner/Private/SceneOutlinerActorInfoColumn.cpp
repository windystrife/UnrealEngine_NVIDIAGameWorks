// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerActorInfoColumn.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "EditorStyleSet.h"
#include "ISceneOutliner.h"


#include "EditorClassUtils.h"
#include "SortHelper.h"

#define LOCTEXT_NAMESPACE "SceneOutlinerActorInfoColumn"

namespace SceneOutliner
{

struct FGetInfo : TTreeItemGetter<FString>
{
	ECustomColumnMode::Type CurrentMode;

	FGetInfo(ECustomColumnMode::Type InCurrentMode) : CurrentMode(InCurrentMode) {}

	virtual FString Get(const FActorTreeItem& ActorItem) const override
	{
		AActor* Actor = ActorItem.Actor.Get();
		if (!Actor)
		{
			return FString();
		}

		switch(CurrentMode)
		{
		case ECustomColumnMode::Class:
			return Actor->GetClass()->GetName();

		case ECustomColumnMode::Level:
			return FPackageName::GetShortName(Actor->GetOutermost()->GetName());

		case ECustomColumnMode::Socket:
			return Actor->GetAttachParentSocketName().ToString();

		case ECustomColumnMode::InternalName:
			return Actor->GetFName().ToString();

		case ECustomColumnMode::UncachedLights:
			return FString::Printf(TEXT("%7d"), Actor->GetNumUncachedStaticLightingInteractions());

		case ECustomColumnMode::Layer:
			{
				FString Result;
				for (const auto& Layer : Actor->Layers)
				{
					if (Result.Len())
					{
						Result += TEXT(", ");
					}

					Result += Layer.ToString();
				}
				return Result;
			}

		default:
			return FString();
		}
	}

	virtual FString Get(const FFolderTreeItem&) const override
	{
		switch(CurrentMode)
		{
		case ECustomColumnMode::Class:
			return LOCTEXT("FolderTypeName", "Folder").ToString();

		default:
			return FString();
		}
	}

	virtual FString Get(const FWorldTreeItem&) const override
	{
		switch(CurrentMode)
		{
		case ECustomColumnMode::Class:
			return LOCTEXT("WorldTypeName", "World").ToString();

		default:
			return FString();
		}
	}
};


TArray< TSharedPtr< ECustomColumnMode::Type > > FActorInfoColumn::ModeOptions;

FActorInfoColumn::FActorInfoColumn( ISceneOutliner& Outliner, ECustomColumnMode::Type InDefaultCustomColumnMode )
	: CurrentMode( InDefaultCustomColumnMode )
	, SceneOutlinerWeak( StaticCastSharedRef<ISceneOutliner>(Outliner.AsShared()) )
{

}


FName FActorInfoColumn::GetColumnID()
{
	return GetID();
}


SHeaderRow::FColumn::FArguments FActorInfoColumn::ConstructHeaderRowColumn()
{
	if( ModeOptions.Num() == 0 )
	{
		for( ECustomColumnMode::Type CurMode : TEnumRange<ECustomColumnMode::Type>() )
		{
			ModeOptions.Add( MakeShareable( new ECustomColumnMode::Type( CurMode ) ) );
		}
	}

	/** Customizable actor data column */
	return SHeaderRow::Column( GetColumnID() )
		.FillWidth(2)
		.HeaderComboVisibility(EHeaderComboVisibility::Ghosted)
		.MenuContent()
		[
			SNew(SBorder)
			.Padding(FMargin(5))
			.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
			[
				SNew(SListView<TSharedPtr<ECustomColumnMode::Type>>)
				.ListItemsSource(&ModeOptions)
				.OnGenerateRow( this, &FActorInfoColumn::MakeComboButtonItemWidget )
				.OnSelectionChanged( this, &FActorInfoColumn::OnModeChanged )
			]
		]
		.HeaderContent()
		[
			SNew( SHorizontalBox )

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text( this, &FActorInfoColumn::GetSelectedMode )
			]
		];
}

const TSharedRef< SWidget > FActorInfoColumn::ConstructRowWidget( FTreeItemRef TreeItem, const STableRow<FTreeItemPtr>& Row )
{
	auto SceneOutliner = SceneOutlinerWeak.Pin();
	check(SceneOutliner.IsValid());

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	TSharedRef<STextBlock> MainText = SNew( STextBlock )
		.Text( this, &FActorInfoColumn::GetTextForItem, TWeakPtr<ITreeItem>(TreeItem) ) 
		.HighlightText( SceneOutliner->GetFilterHighlightText() )
		.ColorAndOpacity( FSlateColor::UseSubduedForeground() );

	HorizontalBox->AddSlot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		MainText
	];

	auto Hyperlink = ConstructClassHyperlink(*TreeItem);
	if (Hyperlink.IsValid())
	{
		// If we got a hyperlink, disable hide default text, and show the hyperlink
		MainText->SetVisibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FActorInfoColumn::GetColumnDataVisibility, false)));
		Hyperlink->SetVisibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FActorInfoColumn::GetColumnDataVisibility, true)));

		HorizontalBox->AddSlot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			// Make sure that the hyperlink shows as black (by multiplying black * desired color) when selected so it is readable against the orange background even if blue/green/etc... normally
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.ColorAndOpacity_Static([](TWeakPtr<const STableRow<FTreeItemPtr>> WeakRow)->FLinearColor{
				auto TableRow = WeakRow.Pin();
				return TableRow.IsValid() && TableRow->IsSelected() ? FLinearColor::Black : FLinearColor::White;
			}, TWeakPtr<const STableRow<FTreeItemPtr>>(StaticCastSharedRef<const STableRow<FTreeItemPtr>>(Row.AsShared())))
			[
				Hyperlink.ToSharedRef()
			]
		];
	}

	return HorizontalBox;
}

TSharedPtr<SWidget> FActorInfoColumn::ConstructClassHyperlink( ITreeItem& TreeItem )
{
	struct FConstructHyperlink : TTreeItemGetter<TSharedPtr<SWidget>>
	{
		TSharedPtr<SWidget> Get(const FActorTreeItem& ActorItem) const override
		{
			if (AActor* Actor = ActorItem.Actor.Get())
			{
				if (UClass* ActorClass = Actor->GetClass())
				{
					// Always show blueprints
					const bool bIsBlueprintClass = UBlueprint::GetBlueprintFromClass(ActorClass) != nullptr;

					// Also show game or game plugin native classes (but not engine classes as that makes the scene outliner pretty noisy)
					bool bIsGameClass = false;
					if (!bIsBlueprintClass)
					{
						UPackage* Package = ActorClass->GetOutermost();
						const FString ModuleName = FPackageName::GetShortName(Package->GetFName());

						FModuleStatus PackageModuleStatus;
						if (FModuleManager::Get().QueryModule(*ModuleName, /*out*/ PackageModuleStatus))
						{
							bIsGameClass = PackageModuleStatus.bIsGameModule;
						}
					}

					if (bIsBlueprintClass || bIsGameClass)
					{
						return FEditorClassUtils::GetSourceLink(ActorClass, Actor);
					}
				}
			}

			return nullptr;
		}
	};

	return TreeItem.Get(FConstructHyperlink());
}

void FActorInfoColumn::PopulateSearchStrings( const ITreeItem& Item, TArray< FString >& OutSearchStrings ) const
{
	{
		FString String = Item.Get(FGetInfo(CurrentMode));
		if (String.Len())
		{
			OutSearchStrings.Add(String);
		}
	}

	// We always add the class
	if (CurrentMode != ECustomColumnMode::Class)
	{
		FString String = Item.Get(FGetInfo(ECustomColumnMode::Class));
		if (String.Len())
		{
			OutSearchStrings.Add(String);
		}
	}
}

bool FActorInfoColumn::SupportsSorting() const
{
	return CurrentMode != ECustomColumnMode::None;
}

void FActorInfoColumn::SortItems(TArray<FTreeItemPtr>& RootItems, const EColumnSortMode::Type SortMode) const
{
	FSortHelper<FString>()
		.Primary(FGetInfo(CurrentMode), SortMode)
		.Sort(RootItems);
}

void FActorInfoColumn::OnModeChanged( TSharedPtr< ECustomColumnMode::Type > NewSelection, ESelectInfo::Type /*SelectInfo*/ )
{
	CurrentMode = *NewSelection;

	// Refresh and refilter the list
	SceneOutlinerWeak.Pin()->Refresh();
	FSlateApplication::Get().DismissAllMenus();
}

EVisibility FActorInfoColumn::GetColumnDataVisibility( bool bIsClassHyperlink ) const
{
	if ( CurrentMode == ECustomColumnMode::Class )
	{
		return bIsClassHyperlink ? EVisibility::Visible : EVisibility::Collapsed;
	}
	else
	{
		return bIsClassHyperlink ? EVisibility::Collapsed : EVisibility::Visible;
	}
}

FText FActorInfoColumn::GetTextForItem( TWeakPtr<ITreeItem> TreeItem ) const
{
	auto Item = TreeItem.Pin();
	return Item.IsValid() ? FText::FromString(Item->Get(FGetInfo(CurrentMode))) : FText::GetEmpty();
}

FText FActorInfoColumn::GetSelectedMode() const
{
	if (CurrentMode == ECustomColumnMode::None)
	{
		return FText();
	}

	return MakeComboText(CurrentMode);
}

FText FActorInfoColumn::MakeComboText( const ECustomColumnMode::Type& Mode ) const
{
	FText ModeName;

	switch( Mode )
	{
	case ECustomColumnMode::None:
		ModeName = LOCTEXT("CustomColumnMode_None", "None");
		break;

	case ECustomColumnMode::Class:
		ModeName = LOCTEXT("CustomColumnMode_Class", "Type");
		break;

	case ECustomColumnMode::Level:
		ModeName = LOCTEXT("CustomColumnMode_Level", "Level");
		break;

	case ECustomColumnMode::Layer:
		ModeName = LOCTEXT("CustomColumnMode_Layer", "Layer");
		break;

	case ECustomColumnMode::Socket:
		ModeName = LOCTEXT("CustomColumnMode_Socket", "Socket");
		break;

	case ECustomColumnMode::InternalName:
		ModeName = LOCTEXT("CustomColumnMode_InternalName", "ID Name");
		break;

	case ECustomColumnMode::UncachedLights:
		ModeName = LOCTEXT("CustomColumnMode_UncachedLights", "# Uncached Lights");
		break;

	default:
		ensure(0);
		break;
	}

	return ModeName;
}


FText FActorInfoColumn::MakeComboToolTipText( const ECustomColumnMode::Type& Mode )
{
	FText ToolTipText;

	switch( Mode )
	{
	case ECustomColumnMode::None:
		ToolTipText = LOCTEXT("CustomColumnModeToolTip_None", "Hides all extra actor info");
		break;

	case ECustomColumnMode::Class:
		ToolTipText = LOCTEXT("CustomColumnModeToolTip_Class", "Displays the name of each actor's type");
		break;

	case ECustomColumnMode::Level:
		ToolTipText = LOCTEXT("CustomColumnModeToolTip_Level", "Displays the level each actor is in, and allows you to search by level name");
		break;

	case ECustomColumnMode::Layer:
		ToolTipText = LOCTEXT("CustomColumnModeToolTip_Layer", "Displays the layer each actor is in, and allows you to search by layer name");
		break;

	case ECustomColumnMode::Socket:
		ToolTipText = LOCTEXT("CustomColumnModeToolTip_Socket", "Shows the socket the actor is attached to, and allows you to search by socket name");
		break;

	case ECustomColumnMode::InternalName:
		ToolTipText = LOCTEXT("CustomColumnModeToolTip_InternalName", "Shows the internal name of the actor (for diagnostics)");
		break;

	case ECustomColumnMode::UncachedLights:
		ToolTipText = LOCTEXT("CustomColumnModeToolTip_UncachedLights", "Shows the number of uncached static lights (missing in lightmap)");
		break;

	default:
		ensure(0);
		break;
	}

	return ToolTipText;
}


TSharedRef< ITableRow > FActorInfoColumn::MakeComboButtonItemWidget( TSharedPtr< ECustomColumnMode::Type > Mode, const TSharedRef<STableViewBase>& Owner )
{
	return
		SNew( STableRow< TSharedPtr<ECustomColumnMode::Type> >, Owner )
		[
			SNew( STextBlock )
			.Text( MakeComboText( *Mode ) )
			.ToolTipText( MakeComboToolTipText( *Mode ) )
		];
}

}		// namespace SceneOutliner

#undef LOCTEXT_NAMESPACE
