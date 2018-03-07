// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Input/Reply.h"

FReply& FReply::SetMousePos(const FIntPoint& NewMousePos)
{
	this->RequestedMousePos = NewMousePos;
	return Me();
}

FReply& FReply::SetUserFocus(TSharedRef<SWidget> GiveMeFocus, EFocusCause ReasonFocusIsChanging, bool bInAllUsers)
{
	this->bSetUserFocus = true;
	this->FocusRecipient = GiveMeFocus;
	this->FocusChangeReason = ReasonFocusIsChanging;
	this->bReleaseUserFocus = false;
	this->bAllUsers = bInAllUsers;
	return Me();
}

FReply& FReply::ClearUserFocus(EFocusCause ReasonFocusIsChanging, bool bInAllUsers)
{
	this->FocusRecipient = nullptr;
	this->FocusChangeReason = ReasonFocusIsChanging;
	this->bReleaseUserFocus = true;
	this->bSetUserFocus = false;
	this->bAllUsers = bInAllUsers;
	return Me();
}
