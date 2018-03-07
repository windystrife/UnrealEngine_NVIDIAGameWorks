// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"

const FName FFriendsAndChatStyle::TypeName( TEXT("FFriendsAndChatStyle") );

FFriendsAndChatStyle& FFriendsAndChatStyle::SetSmallFriendsFontStyle(const FFriendsFontStyle& FontStyle)
{
	FriendsSmallFontStyle = FontStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetNormalFriendsFontStyle(const FFriendsFontStyle& FontStyle)
{
	FriendsNormalFontStyle = FontStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetLargeFriendsFontStyle(const FFriendsFontStyle& FontStyle)
{
	FriendsLargeFontStyle = FontStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatFontStyle(const FFriendsFontStyle& FontStyle)
{
	ChatFontStyle = FontStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsListStyle(const FFriendsListStyle& InFriendsListStyle)
{
	FriendsListStyle = InFriendsListStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetCheckBoxStyle(const FCheckBoxStyle& InCheckBoxStyle)
{
	CheckBoxStyle = InCheckBoxStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetRadioBoxStyle(const FCheckBoxStyle& InRadioBoxStyle)
{
	RadioBoxStyle = InRadioBoxStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsChatStyle(const FFriendsChatStyle& InFriendsChatStyle)
{
	FriendsChatStyle = InFriendsChatStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsChatChromeStyle(const FFriendsChatChromeStyle& InFriendsChatChromeStyle)
{
	FriendsChatChromeStyle = InFriendsChatChromeStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsMarkupStyle(const FFriendsMarkupStyle& InFriendsMarkupStyle)
{
	FriendsMarkupStyle = InFriendsMarkupStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetScrollbarStyle(const FScrollBarStyle& InScrollBarStyle)
{
	ScrollBarStyle = InScrollBarStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetActionButtonStyle(const FButtonStyle& ButtonStyle)
{
	ActionButtonStyle = ButtonStyle;
	return *this;
}

const FFriendsAndChatStyle& FFriendsAndChatStyle::GetDefault()
{
	static FFriendsAndChatStyle Default;
	return Default;
}

/**
	Module style set
*/
TSharedPtr< FSlateStyleSet > FFriendsAndChatModuleStyle::FriendsAndChatModuleStyleInstance = NULL;

void FFriendsAndChatModuleStyle::Initialize(FFriendsAndChatStyle FriendStyle)
{
	if ( !FriendsAndChatModuleStyleInstance.IsValid() )
	{
		FriendsAndChatModuleStyleInstance = Create(FriendStyle);
		FSlateStyleRegistry::RegisterSlateStyle( *FriendsAndChatModuleStyleInstance );
	}
}

void FFriendsAndChatModuleStyle::Shutdown()
{
	if ( FriendsAndChatModuleStyleInstance.IsValid() )
	{
		FSlateStyleRegistry::UnRegisterSlateStyle( *FriendsAndChatModuleStyleInstance );
		ensure( FriendsAndChatModuleStyleInstance.IsUnique() );
		FriendsAndChatModuleStyleInstance.Reset();
	}
}

FName FFriendsAndChatModuleStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("FriendsAndChat"));
	return StyleSetName;
}

TSharedRef< FSlateStyleSet > FFriendsAndChatModuleStyle::Create(FFriendsAndChatStyle FriendStyle)
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("FriendsAndChatStyle"));

	const FButtonStyle UserNameButton = FButtonStyle()
		.SetNormal(FSlateNoResource())
		.SetPressed(FSlateNoResource())
		.SetHovered(FSlateNoResource());

	// Small
	{
		const FTextBlockStyle DefaultTextSmall = FTextBlockStyle(FriendStyle.FriendsChatStyle.TextStyle)
			.SetFont(FriendStyle.ChatFontStyle.FriendsFontSmallBold);

		const FTextBlockStyle GlobalChatFontSmall = FTextBlockStyle(DefaultTextSmall)
			.SetColorAndOpacity(FriendStyle.FriendsChatStyle.GlobalHyperlinkChatColor);

		const FTextBlockStyle GameChatFontSmall = FTextBlockStyle(DefaultTextSmall)
			.SetColorAndOpacity(FriendStyle.FriendsChatStyle.GameHyperlinkChatColor);

		const FTextBlockStyle TeamChatFontSmall = FTextBlockStyle(DefaultTextSmall)
			.SetColorAndOpacity(FriendStyle.FriendsChatStyle.TeamHyperlinkChatColor);

		const FTextBlockStyle PartyChatFontSmall = FTextBlockStyle(DefaultTextSmall)
			.SetColorAndOpacity(FriendStyle.FriendsChatStyle.PartyHyperlinkChatColor);

		const FTextBlockStyle WhisperChatFontSmall = FTextBlockStyle(DefaultTextSmall)
			.SetColorAndOpacity(FriendStyle.FriendsChatStyle.WhisperHyperlinkChatColor);

		const FHyperlinkStyle GlobalChatHyperlinkSmall = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(GlobalChatFontSmall)
			.SetPadding(FMargin(0.0f));

		const FHyperlinkStyle GameChatHyperlinkSmall = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(GameChatFontSmall)
			.SetPadding(FMargin(0.0f));

		const FHyperlinkStyle TeamChatHyperlinkSmall = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(TeamChatFontSmall)
			.SetPadding(FMargin(0.0f));

		const FHyperlinkStyle PartyChatHyperlinkSmall = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(PartyChatFontSmall)
			.SetPadding(FMargin(0.0f));

		const FHyperlinkStyle WhisperChatHyperlinkSmall = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(WhisperChatFontSmall)
			.SetPadding(FMargin(0.0f));

		const FHyperlinkStyle DefaultChatHyperlinkSmall = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(DefaultTextSmall)
			.SetPadding(FMargin(0.0f));

		Style->Set("UserNameTextStyle.DefaultSmall", DefaultTextSmall);
		Style->Set("UserNameTextStyle.GlobalHyperlinkSmall", GlobalChatHyperlinkSmall);
		Style->Set("UserNameTextStyle.GameHyperlinkSmall", GameChatHyperlinkSmall);
		Style->Set("UserNameTextStyle.TeamHyperlinkSmall", TeamChatHyperlinkSmall);
		Style->Set("UserNameTextStyle.PartyHyperlinkSmall", PartyChatHyperlinkSmall);
		Style->Set("UserNameTextStyle.WhisperlinkSmall", WhisperChatHyperlinkSmall);
		Style->Set("UserNameTextStyle.DefaultHyperlinkSmall", DefaultChatHyperlinkSmall);
		Style->Set("UserNameTextStyle.GlobalTextStyleSmall", GlobalChatFontSmall);
		Style->Set("UserNameTextStyle.GameTextStyleSmall", GameChatFontSmall);
		Style->Set("UserNameTextStyle.TeamTextStyleSmall", TeamChatFontSmall);
		Style->Set("UserNameTextStyle.PartyTextStyleSmall", PartyChatFontSmall);
		Style->Set("UserNameTextStyle.WhisperTextStyleSmall", WhisperChatFontSmall);

		Style->Set("MessageBreak", FTextBlockStyle(DefaultTextSmall)
			.SetFont(FSlateFontInfo(
			FriendStyle.FriendsNormalFontStyle.FriendsFontSmall.FontObject,
			6,
			FriendStyle.FriendsNormalFontStyle.FriendsFontSmall.TypefaceFontName
			)));
	}


	// Normal
	{
		const FTextBlockStyle DefaultText = FTextBlockStyle(FriendStyle.FriendsChatStyle.TextStyle)
			.SetFont(FriendStyle.ChatFontStyle.FriendsFontNormalBold);

		const FTextBlockStyle GlobalChatFont = FTextBlockStyle(DefaultText)
			.SetColorAndOpacity(FriendStyle.FriendsChatStyle.GlobalHyperlinkChatColor);

		const FTextBlockStyle GameChatFont = FTextBlockStyle(DefaultText)
			.SetColorAndOpacity(FriendStyle.FriendsChatStyle.GameHyperlinkChatColor);

		const FTextBlockStyle TeamChatFont = FTextBlockStyle(DefaultText)
			.SetColorAndOpacity(FriendStyle.FriendsChatStyle.TeamHyperlinkChatColor);

		const FTextBlockStyle PartyChatFont = FTextBlockStyle(DefaultText)
			.SetColorAndOpacity(FriendStyle.FriendsChatStyle.PartyHyperlinkChatColor);

		const FTextBlockStyle WhisperChatFont = FTextBlockStyle(DefaultText)
			.SetColorAndOpacity(FriendStyle.FriendsChatStyle.WhisperHyperlinkChatColor);

		const FHyperlinkStyle GlobalChatHyperlink = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(GlobalChatFont)
			.SetPadding(FMargin(0.0f));

		const FHyperlinkStyle GameChatHyperlink = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(GameChatFont)
			.SetPadding(FMargin(0.0f));

		const FHyperlinkStyle TeamChatHyperlink = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(TeamChatFont)
			.SetPadding(FMargin(0.0f));

		const FHyperlinkStyle PartyChatHyperlink = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(PartyChatFont)
			.SetPadding(FMargin(0.0f));

		const FHyperlinkStyle WhisperChatHyperlink = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(WhisperChatFont)
			.SetPadding(FMargin(0.0f));

		const FHyperlinkStyle DefaultChatHyperlink = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(DefaultText)
			.SetPadding(FMargin(0.0f));

		Style->Set("UserNameTextStyle.Default", DefaultText);
		Style->Set("UserNameTextStyle.GlobalHyperlink", GlobalChatHyperlink);
		Style->Set("UserNameTextStyle.GameHyperlink", GameChatHyperlink);
		Style->Set("UserNameTextStyle.TeamHyperlink", TeamChatHyperlink);
		Style->Set("UserNameTextStyle.PartyHyperlink", PartyChatHyperlink);
		Style->Set("UserNameTextStyle.Whisperlink", WhisperChatHyperlink);
		Style->Set("UserNameTextStyle.DefaultHyperlink", DefaultChatHyperlink);
		Style->Set("UserNameTextStyle.GlobalTextStyle", GlobalChatFont);
		Style->Set("UserNameTextStyle.GameTextStyle", GameChatFont);
		Style->Set("UserNameTextStyle.TeamTextStyle", TeamChatFont);
		Style->Set("UserNameTextStyle.PartyTextStyle", PartyChatFont);
		Style->Set("UserNameTextStyle.WhisperTextStyle", WhisperChatFont);
	}

	// Large
	{
		const FTextBlockStyle DefaultTextLarge = FTextBlockStyle(FriendStyle.FriendsChatStyle.TextStyle)
			.SetFont(FriendStyle.ChatFontStyle.FriendsFontLargeBold);

		const FTextBlockStyle GlobalChatFontLarge = FTextBlockStyle(DefaultTextLarge)
			.SetColorAndOpacity(FriendStyle.FriendsChatStyle.GlobalHyperlinkChatColor);

		const FTextBlockStyle GameChatFontLarge = FTextBlockStyle(DefaultTextLarge)
			.SetColorAndOpacity(FriendStyle.FriendsChatStyle.GameHyperlinkChatColor);

		const FTextBlockStyle TeamChatFontLarge = FTextBlockStyle(DefaultTextLarge)
			.SetColorAndOpacity(FriendStyle.FriendsChatStyle.TeamHyperlinkChatColor);

		const FTextBlockStyle PartyChatFontLarge = FTextBlockStyle(DefaultTextLarge)
			.SetColorAndOpacity(FriendStyle.FriendsChatStyle.PartyHyperlinkChatColor);

		const FTextBlockStyle WhisperChatFontLarge = FTextBlockStyle(DefaultTextLarge)
			.SetColorAndOpacity(FriendStyle.FriendsChatStyle.WhisperHyperlinkChatColor);

		const FHyperlinkStyle GlobalChatHyperlinkLarge = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(GlobalChatFontLarge)
			.SetPadding(FMargin(0.0f));

		const FHyperlinkStyle GameChatHyperlinkLarge = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(GameChatFontLarge)
			.SetPadding(FMargin(0.0f));

		const FHyperlinkStyle TeamChatHyperlinkLarge = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(TeamChatFontLarge)
			.SetPadding(FMargin(0.0f));

		const FHyperlinkStyle PartyChatHyperlinkLarge = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(PartyChatFontLarge)
			.SetPadding(FMargin(0.0f));

		const FHyperlinkStyle WhisperChatHyperlinkLarge = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(WhisperChatFontLarge)
			.SetPadding(FMargin(0.0f));

		const FHyperlinkStyle DefaultChatHyperlinkLarge = FHyperlinkStyle()
			.SetUnderlineStyle(UserNameButton)
			.SetTextStyle(DefaultTextLarge)
			.SetPadding(FMargin(0.0f));

		Style->Set("UserNameTextStyle.DefaultLarge", DefaultTextLarge);
		Style->Set("UserNameTextStyle.GlobalHyperlinkLarge", GlobalChatHyperlinkLarge);
		Style->Set("UserNameTextStyle.GameHyperlinkLarge", GameChatHyperlinkLarge);
		Style->Set("UserNameTextStyle.TeamHyperlinkLarge", TeamChatHyperlinkLarge);
		Style->Set("UserNameTextStyle.PartyHyperlinkLarge", PartyChatHyperlinkLarge);
		Style->Set("UserNameTextStyle.WhisperlinkLarge", WhisperChatHyperlinkLarge);
		Style->Set("UserNameTextStyle.DefaultHyperlinkLarge", DefaultChatHyperlinkLarge);
		Style->Set("UserNameTextStyle.GlobalTextStyleLarge", GlobalChatFontLarge);
		Style->Set("UserNameTextStyle.GameTextStyleLarge", GameChatFontLarge);
		Style->Set("UserNameTextStyle.TeamTextStyleLarge", TeamChatFontLarge);
		Style->Set("UserNameTextStyle.PartyTextStyleLarge", PartyChatFontLarge);
		Style->Set("UserNameTextStyle.WhisperTextStyleLarge", WhisperChatFontLarge);
	}

	return Style;
}

void FFriendsAndChatModuleStyle::ReloadTextures()
{
	FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

const ISlateStyle& FFriendsAndChatModuleStyle::Get()
{
	return *FriendsAndChatModuleStyleInstance;
}
