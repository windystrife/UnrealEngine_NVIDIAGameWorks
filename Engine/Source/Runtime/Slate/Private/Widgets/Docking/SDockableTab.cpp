// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Docking/SDockableTab.h"


void SDockableTab::SetContent( TSharedRef<SWidget> InContent )
{

}

bool SDockableTab::IsActive() const
{
	return false;
}

bool SDockableTab::IsForeground() const
{
	return false;
}

TSharedRef<SWidget> SDockableTab::GetContent()
{
	return SNullWidget::NullWidget;
}

TSharedPtr<SDockTabStack> SDockableTab::GetParentDockTabStack() const
{
	return TSharedPtr<SDockTabStack>();
}

void SDockableTab::BringToFrontInParent()
{

}

void SDockableTab::RemoveTabFromParent()
{

}

void SDockableTab::ActivateInParent(ETabActivationCause InActivationCause)
{
}

void SDockableTab::Construct( const FArguments& InArgs )
{

}

bool SDockableTab::ShouldAutosize() const
{
	return false;
}

void SDockableTab::SetOnTabStackMenuOpening( const FOnTabStackMenuOpening& InHandler )
{
}

void SDockableTab::RequestCloseTab()
{	

}

