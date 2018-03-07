// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "LiveLinkMessageBusSourceFactory.h"
#include "LiveLinkMessageBusSource.h"
#include "LiveLinkMessageBusSourceEditor.h"

#define LOCTEXT_NAMESPACE "LiveLinkMessageBusSourceFactory"

FText ULiveLinkMessageBusSourceFactory::GetSourceDisplayName() const
{
	return LOCTEXT("SourceDisplayName", "Message Bus Source");
}

FText ULiveLinkMessageBusSourceFactory::GetSourceTooltip() const
{
	return LOCTEXT("SourceTooltip", "Creates a connection to a Message Bus based Live Link Source");
}

TSharedPtr<SWidget> ULiveLinkMessageBusSourceFactory::CreateSourceCreationPanel()
{
	if (!ActiveSourceEditor.IsValid())
	{
		SAssignNew(ActiveSourceEditor, SLiveLinkMessageBusSourceEditor);
	}
	return ActiveSourceEditor;
}

TSharedPtr<ILiveLinkSource> ULiveLinkMessageBusSourceFactory::OnSourceCreationPanelClosed(bool bMakeSource)
{
	//Clean up
	TSharedPtr<FLiveLinkMessageBusSource> NewSource = nullptr;

	if (bMakeSource && ActiveSourceEditor.IsValid())
	{
		FProviderPollResultPtr Result = ActiveSourceEditor->GetSelectedSource();
		if(Result.IsValid())
		{
			NewSource = MakeShareable( new FLiveLinkMessageBusSource(FText::FromString(Result->Name), FText::FromString(Result->MachineName), Result->Address));
		}
	}
	ActiveSourceEditor = nullptr;
	return NewSource;
}

#undef LOCTEXT_NAMESPACE