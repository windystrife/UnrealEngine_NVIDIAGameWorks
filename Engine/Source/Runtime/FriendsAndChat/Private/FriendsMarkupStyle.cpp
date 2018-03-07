// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FriendsMarkupStyle.h"

const FName FFriendsMarkupStyle::TypeName( TEXT("FFriendsMarkupStyle") );

const FFriendsMarkupStyle& FFriendsMarkupStyle::GetDefault()
{
	static FFriendsMarkupStyle Default;
	return Default;
}

FFriendsMarkupStyle& FFriendsMarkupStyle::SetMarkupButtonStyle(const FButtonStyle& ButtonStyle)
{
	MarkupButtonStyle = ButtonStyle;
	return *this;
}

FFriendsMarkupStyle& FFriendsMarkupStyle::SetMarkupTextStyle(const FTextBlockStyle& InTextStyle)
{
	MarkupTextStyle = InTextStyle;
	return *this;
}

FFriendsMarkupStyle& FFriendsMarkupStyle::SetMarkupBackgroundBrush(const FSlateBrush& Value)
{
	MarkupBackground = Value;
	return *this;
}

FFriendsMarkupStyle& FFriendsMarkupStyle::SetButtonColor(const FSlateColor& Value)
{
	ButtonColor = Value;
	return *this;
}

FFriendsMarkupStyle& FFriendsMarkupStyle::SetButtonHoverColor(const FSlateColor& Value)
{
	ButtonHoverColor = Value;
	return *this;
}

FFriendsMarkupStyle& FFriendsMarkupStyle::SetTipColor(const FSlateColor& Value)
{
	TipColor = Value;
	return *this;
}

FFriendsMarkupStyle& FFriendsMarkupStyle::SetSeperatorBrush(const FSlateBrush& Value)
{
	SeperatorBrush = Value;
	return *this;
}
