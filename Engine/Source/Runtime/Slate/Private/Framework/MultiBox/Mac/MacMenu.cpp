// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MacMenu.h"
#include "Framework/Application/SlateApplication.h"
#include "CocoaThread.h"
#include "MacApplication.h"
#include "Misc/ScopeLock.h"
#include "HAL/PlatformApplicationMisc.h"

struct FMacMenuItemState
{
	TSharedPtr<const FMenuEntryBlock> Block;
	EMultiBlockType::Type Type;
	NSString* Title;
	NSString* KeyEquivalent;
	uint32 KeyModifiers;
	NSImage* Icon;
	bool IsSubMenu;
	bool IsEnabled;
	uint32 State;

	FMacMenuItemState() : Type(EMultiBlockType::None), Title(nil), KeyEquivalent(nil), KeyModifiers(0), Icon(nil), IsSubMenu(false), IsEnabled(false), State(0) {}
	~FMacMenuItemState()
	{
		if (Title) [Title release];
		if (KeyEquivalent) [KeyEquivalent release];
		if (Icon) [Icon release];
	}
};

static TMap<FMacMenu*, TSharedPtr<TArray<FMacMenuItemState>>> GCachedMenuState;
static FCriticalSection GCachedMenuStateCS;

@interface FMacMenuItem : NSMenuItem
@property (assign) TSharedPtr<const FMenuEntryBlock> MenuEntryBlock;
- (void)performAction;
@end

@implementation FMacMenuItem

- (id)initWithMenuEntryBlock:(TSharedPtr< const FMenuEntryBlock >)Block
{
	self = [super initWithTitle:@"" action:nil keyEquivalent:@""];
	self.MenuEntryBlock = Block;
	return self;
}

- (void)performAction
{
	FCocoaMenu* CocoaMenu = [[self menu] isKindOfClass:[FCocoaMenu class]] ? (FCocoaMenu*)[self menu] : nil;
	if ( !CocoaMenu || ![CocoaMenu isHighlightingKeyEquivalent] )
	{
		FSlateMacMenu::ExecuteMenuItemAction(self.MenuEntryBlock.ToSharedRef());
	}
}

@end

@implementation FMacMenu

- (id)initWithMenuEntryBlock:(TSharedPtr< const FMenuEntryBlock >)Block
{
	self = [super initWithTitle:@""];
	[self setDelegate:self];
	self.MenuEntryBlock = Block;
	FScopeLock Lock(&GCachedMenuStateCS);
	GCachedMenuState.Add(self, TSharedPtr<TArray<FMacMenuItemState>>(new TArray<FMacMenuItemState>()));
	return self;
}

- (void)menuNeedsUpdate:(NSMenu*)Menu
{
	FSlateMacMenu::UpdateMenu(self);
}

- (void)menuWillOpen:(NSMenu*)Menu
{
	FPlatformApplicationMisc::bChachedMacMenuStateNeedsUpdate = true;
	
	GameThreadCall(^{
		FSlateApplication::Get().ClearKeyboardFocus( EFocusCause::WindowActivate );
	}, @[ NSDefaultRunLoopMode ], false);

}

@end

void FSlateMacMenu::UpdateWithMultiBox(const TSharedPtr< FMultiBox > MultiBox)
{
	// The dispatch block can't handle TSharedPtr correctly, so we use a small trick to pass MultiBox safely
	struct FSafeMultiBoxPass
	{
		TSharedPtr<FMultiBox> MultiBox;
	};
	FSafeMultiBoxPass* SafeMultiBoxPtr = new FSafeMultiBoxPass;
	SafeMultiBoxPtr->MultiBox = MultiBox;

	MainThreadCall(^{
		FScopeLock Lock(&GCachedMenuStateCS);

		if (!FPlatformApplicationMisc::UpdateCachedMacMenuState)
		{
			FPlatformApplicationMisc::UpdateCachedMacMenuState = UpdateCachedState;
		}

		int32 NumItems = [[NSApp mainMenu] numberOfItems];
		FText HelpTitle = NSLOCTEXT("MainMenu", "HelpMenu", "Help");
		FText WindowLabel = NSLOCTEXT("MainMenu", "WindowMenu", "Window");
		for (int32 Index = NumItems - 1; Index > 0; Index--)
		{
			[[NSApp mainMenu] removeItemAtIndex:Index];
		}
		GCachedMenuState.Reset();
		
		if( SafeMultiBoxPtr->MultiBox.IsValid() )
		{
			const TArray<TSharedRef<const FMultiBlock> >& MenuBlocks = SafeMultiBoxPtr->MultiBox->GetBlocks();

			for (int32 Index = 0; Index < MenuBlocks.Num(); Index++)
			{
				TSharedRef<const FMenuEntryBlock> Block = StaticCastSharedRef<const FMenuEntryBlock>(MenuBlocks[Index]);
				FMacMenu* Menu = [[FMacMenu alloc] initWithMenuEntryBlock:Block];
				NSString* Title = FSlateMacMenu::GetMenuItemTitle(Block);
				[Menu setTitle:Title];

				NSMenuItem* MenuItem = [[NSMenuItem new] autorelease];
				[MenuItem setTitle:Title];
				[[NSApp mainMenu] addItem:MenuItem];
				[MenuItem setSubmenu:Menu];

				const bool bIsWindowMenu = (WindowLabel.ToString().Compare(FString(Title)) == 0);
				if (bIsWindowMenu)
				{
					[NSApp setWindowsMenu:nil];

					[Menu removeAllItems];

					NSMenuItem* MinimizeItem = [[[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(miniaturize:) keyEquivalent:@"m"] autorelease];
					NSMenuItem* ZoomItem = [[[NSMenuItem alloc] initWithTitle:@"Zoom" action:@selector(zoom:) keyEquivalent:@""] autorelease];
					NSMenuItem* CloseItem = [[[NSMenuItem alloc] initWithTitle:@"Close" action:@selector(performClose:) keyEquivalent:@"w"] autorelease];
					NSMenuItem* BringAllToFrontItem = [[[NSMenuItem alloc] initWithTitle:@"Bring All to Front" action:@selector(arrangeInFront:) keyEquivalent:@""] autorelease];

					[Menu addItem:MinimizeItem];
					[Menu addItem:ZoomItem];
					[Menu addItem:CloseItem];
					[Menu addItem:[NSMenuItem separatorItem]];
					[Menu addItem:BringAllToFrontItem];
					[Menu addItem:[NSMenuItem separatorItem]];

					[NSApp setWindowsMenu:Menu];
					[Menu addItem:[NSMenuItem separatorItem]];
				}
			}

			delete SafeMultiBoxPtr;
		}

		FPlatformApplicationMisc::bChachedMacMenuStateNeedsUpdate = true;
	}, NSDefaultRunLoopMode, false);
}

void FSlateMacMenu::UpdateMenu(FMacMenu* Menu)
{
	MainThreadCall(^{
		FScopeLock Lock(&GCachedMenuStateCS);

		FText WindowLabel = NSLOCTEXT("MainMenu", "WindowMenu", "Window");
		const bool bIsWindowMenu = (WindowLabel.ToString().Compare(FString([Menu title])) == 0);
		int32 ItemIndexOffset = 0;
		if (bIsWindowMenu)
		{
			int32 SeparatorIndex = 0;
			for (NSMenuItem* Item in [Menu itemArray])
			{
				SeparatorIndex += [Item isSeparatorItem] ? 1 : 0;
				ItemIndexOffset++;
				if (SeparatorIndex == 3)
				{
					break;
				}
			}
		}

		TSharedPtr<TArray<FMacMenuItemState>> MenuState = GCachedMenuState[Menu];
		int32 ItemIndexAdjust = 0;
		for (int32 Index = 0; Index < MenuState->Num(); Index++)
		{
			FMacMenuItemState& MenuItemState = (*MenuState)[Index];
			const int32 ItemIndex = (bIsWindowMenu ? Index + ItemIndexOffset : Index) - ItemIndexAdjust;
			NSMenuItem* MenuItem = [Menu numberOfItems] > ItemIndex ? [Menu itemAtIndex:ItemIndex] : nil;

			if (MenuItemState.Type == EMultiBlockType::MenuEntry)
			{
				if (MenuItem && (![MenuItem isKindOfClass:[FMacMenuItem class]] || (MenuItemState.IsSubMenu && [MenuItem submenu] == nil) || (!MenuItemState.IsSubMenu && [MenuItem submenu] != nil)))
				{
					[Menu removeItem:MenuItem];
					MenuItem = nil;
				}
				if (!MenuItem)
				{
					MenuItem = [[[FMacMenuItem alloc] initWithMenuEntryBlock:MenuItemState.Block] autorelease];

					if (MenuItemState.IsSubMenu)
					{
						FMacMenu* SubMenu = [[FMacMenu alloc] initWithMenuEntryBlock:MenuItemState.Block];
						[MenuItem setSubmenu:SubMenu];
					}

					if ([Menu numberOfItems] > ItemIndex)
					{
						[Menu insertItem:MenuItem atIndex:ItemIndex];
					}
					else
					{
						[Menu addItem:MenuItem];
					}
				}

				[MenuItem setTitle:MenuItemState.Title];

				[MenuItem setKeyEquivalent:MenuItemState.KeyEquivalent];
				[MenuItem setKeyEquivalentModifierMask:MenuItemState.KeyModifiers];

				if (bIsWindowMenu)
				{
					NSImage* MenuImage = MenuItemState.Icon;
					if(MenuImage)
					{
						[MenuItem setImage:MenuImage];
					}
				}
				else
				{
					[MenuItem setImage:nil];
				}

                [MenuItem setTarget:MenuItem];
                if(!MenuItemState.IsSubMenu)
                {
                   if(MenuItemState.IsEnabled)
                    {
                        [MenuItem setAction:@selector(performAction)];
                    }
                    else
                    {
                        [MenuItem setAction:nil];
                    }
                }
				
				if (!MenuItemState.IsSubMenu)
				{
					[MenuItem setState:MenuItemState.State];
				}
			}
			else if (MenuItemState.Type == EMultiBlockType::MenuSeparator)
			{
				if (MenuItem && ![MenuItem isSeparatorItem])
				{
					[Menu removeItem:MenuItem];
				}
				else if (!MenuItem)
				{
					if ([Menu numberOfItems] > ItemIndex)
					{
						[Menu insertItem:[NSMenuItem separatorItem] atIndex:ItemIndex];
					}
					else
					{
						[Menu addItem:[NSMenuItem separatorItem]];
					}
				}
			}
			else
			{
				// If it's a type we skip, update ItemIndexAdjust so we can properly calculate item's index in NSMenu
				ItemIndexAdjust++;
			}
		}
	});
}

void FSlateMacMenu::UpdateCachedState()
{
	bool bShouldUpdate = false;

	// @todo: Ideally this would ask global tab manager if there's any active tab, but that cannot be done reliably at the moment
	// so instead we assume that as long as there's any visible, regular window open, we do have some menu to show/update.
	{
		MacApplication->GetWindowsArrayMutex().Lock();
		const TArray<TSharedRef<FMacWindow>>&AllWindows = MacApplication->GetAllWindows();
		for (auto Window : AllWindows)
		{
			if (Window->IsRegularWindow() && Window->IsVisible())
			{
				bShouldUpdate = true;
				break;
			}
		}
		MacApplication->GetWindowsArrayMutex().Unlock();
    }
	

	if (bShouldUpdate)
	{
		FScopeLock Lock(&GCachedMenuStateCS);

		for (TMap<FMacMenu*, TSharedPtr<TArray<FMacMenuItemState>>>::TIterator It(GCachedMenuState); It; ++It)
		{
			FMacMenu* Menu = It.Key();
			TSharedPtr<TArray<FMacMenuItemState>> MenuState = It.Value();

			TSharedRef<SWidget> Widget = SNullWidget::NullWidget;
			if (!Menu.MultiBox.IsValid())
			{
				if (Menu.MenuEntryBlock->MenuBuilder.IsBound())
				{
					Widget = Menu.MenuEntryBlock->MenuBuilder.Execute();
				}
				else
				{
					const bool bShouldCloseWindowAfterMenuSelection = true;
					FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, Menu.MenuEntryBlock->GetActionList(), Menu.MenuEntryBlock->Extender);
					{
						// Have the menu fill its contents
						Menu.MenuEntryBlock->EntryBuilder.ExecuteIfBound(MenuBuilder);
					}
					Widget = MenuBuilder.MakeWidget();
				}

				if (Widget->GetType() == FName(TEXT("SMultiBoxWidget")))
				{
					Menu.MultiBox = TSharedPtr<const FMultiBox>(StaticCastSharedRef<SMultiBoxWidget>(Widget)->GetMultiBox());
				}
				else
				{
					UE_LOG(LogMac, Warning, TEXT("Unsupported type of menu widget in FSlateMacMenu::UpdateCachedState(): %s"), *Widget->GetType().ToString());
				}
			}

			if (Menu.MultiBox.IsValid())
			{
				const TArray<TSharedRef<const FMultiBlock>>& MenuBlocks = Menu.MultiBox->GetBlocks();
				for (int32 Index = MenuState->Num(); MenuBlocks.Num() > MenuState->Num(); Index++)
				{
					MenuState->Add(FMacMenuItemState());
				}
				for (int32 Index = 0; Index < MenuBlocks.Num(); Index++)
				{
					FMacMenuItemState& ItemState = (*MenuState)[Index];
					ItemState.Type = MenuBlocks[Index]->GetType();

					if (ItemState.Type == EMultiBlockType::MenuEntry)
					{
						TSharedRef<const FMenuEntryBlock> Block = StaticCastSharedRef<const FMenuEntryBlock>(MenuBlocks[Index]);
						ItemState.Block = Block;
						ItemState.Title = [FSlateMacMenu::GetMenuItemTitle(Block) retain];
						ItemState.KeyEquivalent = [FSlateMacMenu::GetMenuItemKeyEquivalent(Block, &ItemState.KeyModifiers) retain];
						if (!ItemState.Icon)
						{
							ItemState.Icon = [FSlateMacMenu::GetMenuItemIcon(Block) retain];
						}
						ItemState.IsSubMenu = Block->bIsSubMenu;
						ItemState.IsEnabled = FSlateMacMenu::IsMenuItemEnabled(Block);
						ItemState.State = ItemState.IsSubMenu ? 0 : FSlateMacMenu::GetMenuItemState(Block);
					}
				}
			}
		}
	}
}

void FSlateMacMenu::ExecuteMenuItemAction(const TSharedRef< const class FMenuEntryBlock >& Block)
{
    TSharedPtr< const class FMenuEntryBlock>* MenuBlock = new TSharedPtr< const class FMenuEntryBlock>(Block);
	if (!FPlatformApplicationMisc::bMacApplicationModalMode)
	{
		GameThreadCall(^{
			TSharedPtr< const FUICommandList > ActionList = (*MenuBlock)->GetActionList();
			if (ActionList.IsValid() && (*MenuBlock)->GetAction().IsValid())
			{
				ActionList->ExecuteAction((*MenuBlock)->GetAction().ToSharedRef());
			}
			else
			{
				// There is no action list or action associated with this block via a UI command.  Execute any direct action we have
				(*MenuBlock)->GetDirectActions().Execute();
			}
			delete MenuBlock;
		}, @[ NSDefaultRunLoopMode ], false);
	}
}

static const TSharedRef<SWidget> FindTextBlockWidget(TSharedRef<SWidget> Content)
{
	if (Content->GetType() == FName(TEXT("STextBlock")))
	{
		return Content;
	}

	FChildren* Children = Content->GetChildren();
	int32 NumChildren = Children->Num();

	for (int32 Index=0; Index < NumChildren; ++Index)
	{
		const TSharedRef<SWidget> Found = FindTextBlockWidget(Children->GetChildAt(Index));
		if (Found != SNullWidget::NullWidget)
		{
			return Found;
		}
	}
	return SNullWidget::NullWidget;
}

NSString* FSlateMacMenu::GetMenuItemTitle(const TSharedRef<const FMenuEntryBlock>& Block)
{
	TAttribute<FText> Label;
	if (!Block->LabelOverride.IsBound() && Block->LabelOverride.Get().IsEmpty() && Block->GetAction().IsValid())
	{
		Label = Block->GetAction()->GetLabel();
	}
	else if (!Block->LabelOverride.Get().IsEmpty())
	{
		Label = Block->LabelOverride;
	}
	else if (Block->EntryWidget.IsValid())
	{
		const TSharedRef<SWidget>& TextBlockWidget = FindTextBlockWidget(Block->EntryWidget.ToSharedRef());
		if (TextBlockWidget != SNullWidget::NullWidget)
		{
			Label = StaticCastSharedRef<STextBlock>(TextBlockWidget)->GetText();
		}
	}

	return Label.Get().ToString().GetNSString();
}

NSImage* FSlateMacMenu::GetMenuItemIcon(const TSharedRef<const FMenuEntryBlock>& Block)
{
	NSImage* MenuImage = nil;
	FSlateIcon Icon;
	if (Block->IconOverride.IsSet())
	{
		Icon = Block->IconOverride;
	}
	else if (Block->GetAction().IsValid() && Block->GetAction()->GetIcon().IsSet())
	{
		Icon = Block->GetAction()->GetIcon();
	}
	if (Icon.IsSet())
	{
		if (Icon.GetIcon())
		{
			FSlateBrush const* IconBrush = Icon.GetIcon();
			FName ResourceName = IconBrush->GetResourceName();
			MenuImage = [[NSImage alloc] initWithContentsOfFile:ResourceName.ToString().GetNSString()];
			if (MenuImage)
			{
				[MenuImage setSize:NSMakeSize(16.0f, 16.0f)];
			}
		}
	}
	return MenuImage;
}

NSString* FSlateMacMenu::GetMenuItemKeyEquivalent(const TSharedRef<const class FMenuEntryBlock>& Block, uint32* OutModifiers)
{
	*OutModifiers = 0;

	if (Block->GetAction().IsValid())
	{
		const TSharedRef<const FInputChord> Chord = Block->GetAction()->GetFirstValidChord();
		if (Chord->IsValidChord())
		{
			if (Chord->NeedsControl())
			{
				*OutModifiers |= NSControlKeyMask;
			}
			if (Chord->NeedsShift())
			{
				*OutModifiers |= NSShiftKeyMask;
			}
			if (Chord->NeedsAlt())
			{
				*OutModifiers |= NSAlternateKeyMask;
			}
			if (Chord->NeedsCommand())
			{
				*OutModifiers |= NSCommandKeyMask;
			}

			FString KeyString = Chord->GetKeyText().ToString().ToLower();
			return KeyString.GetNSString();
		}
	}

	return @"";
}

bool FSlateMacMenu::IsMenuItemEnabled(const TSharedRef<const class FMenuEntryBlock>& Block)
{
	TSharedPtr<const FUICommandList> ActionList = Block->GetActionList();
	TSharedPtr<const FUICommandInfo> Action = Block->GetAction();
	const FUIAction& DirectActions = Block->GetDirectActions();

	bool bEnabled = true;
	if (ActionList.IsValid() && Action.IsValid())
	{
		bEnabled = ActionList->CanExecuteAction(Action.ToSharedRef());
	}
	else
	{
		// There is no action list or action associated with this block via a UI command.  Execute any direct action we have
		bEnabled = DirectActions.CanExecute();
	}
    
    if(FPlatformApplicationMisc::bMacApplicationModalMode)
    {
        bEnabled = false;
    }

	return bEnabled;
}

int32 FSlateMacMenu::GetMenuItemState(const TSharedRef<const class FMenuEntryBlock>& Block)
{
	TSharedPtr<const FUICommandList> ActionList = Block->GetActionList();
	TSharedPtr<const FUICommandInfo> Action = Block->GetAction();
	const FUIAction& DirectActions = Block->GetDirectActions();

	ECheckBoxState CheckState = ECheckBoxState::Unchecked;
	if (ActionList.IsValid() && Action.IsValid())
	{
		CheckState = ActionList->GetCheckState(Action.ToSharedRef());
	}
	else
	{
		// There is no action list or action associated with this block via a UI command.  Execute any direct action we have
		CheckState = DirectActions.GetCheckState();
	}

	switch(CheckState)
	{
	case ECheckBoxState::Checked:
		return NSOnState;
	case ECheckBoxState::Undetermined:
		return NSMixedState;
	default:
		break;
	}
	return NSOffState;
}
