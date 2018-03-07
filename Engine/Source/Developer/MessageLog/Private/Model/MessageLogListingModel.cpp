// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Model/MessageLogListingModel.h"
#include "Logging/MessageLog.h"

#define LOCTEXT_NAMESPACE "Developer.MessageLog"


MessageContainer::TConstIterator FMessageLogListingModel::GetMessageIterator(const uint32 PageIndex) const
{
	return PageAtIndex(PageIndex)->Messages.CreateConstIterator();
}

const TSharedPtr< FTokenizedMessage >  FMessageLogListingModel::GetMessageAtIndex( const uint32 PageIndex, const int32 MessageIndex ) const
{
	TSharedPtr<FTokenizedMessage> FoundMessage = NULL;
	FPage* Page = PageAtIndex(PageIndex);
	if( Page->Messages.IsValidIndex( MessageIndex ) )
	{
		FoundMessage = Page->Messages[MessageIndex];
	}
	return FoundMessage;
}

const TSharedPtr< FTokenizedMessage > FMessageLogListingModel::GetMessageFromData( const FTokenizedMiscData& MessageData ) const
{
	// Go through all the messages looking for a data match
	for(uint32 PageIndex = 0; PageIndex < NumPages(); PageIndex++)
	{
		for( MessageContainer::TConstIterator It( GetMessageIterator(PageIndex) ); It; ++It )
		{
			const TSharedPtr< FTokenizedMessage > Message = *It;

			if( Message->GetMessageData() == &MessageData )
			{
				return Message;
			}
		}
	}
	return NULL;
}

FText FMessageLogListingModel::GetAllMessagesAsText( const uint32 PageIndex ) const
{
	FText CompiledText;

	// Go through all the messages and add it to the compiled one.
	FPage* Page = PageAtIndex(PageIndex);
	for( int32 MessageID = 0; MessageID < Page->Messages.Num(); MessageID++ )
	{
		const TSharedPtr< FTokenizedMessage > Message = Page->Messages[ MessageID ];
		FFormatNamedArguments Args;
		Args.Add( TEXT("PreviousMessage"), CompiledText );
		Args.Add( TEXT("NewMessage"), Message.Get()->ToText() );
		CompiledText = FText::Format( LOCTEXT("AggregateMessagesFormatter", "{PreviousMessage}{NewMessage}\n"), Args );
	}

	return CompiledText;
}

void FMessageLogListingModel::AddMessageInternal( const TSharedRef<FTokenizedMessage>& NewMessage, bool bMirrorToOutputLog )
{
	if (bIsPrintingToOutputLog)
	{
		return;
	}

	CurrentPage().Messages.Add( NewMessage );

	if (bMirrorToOutputLog)
	{
		// Prevent re-entrancy from the output log to message log mirroring code
		TGuardValue<bool> PrintToOutputLogBool(bIsPrintingToOutputLog, true);

		const TCHAR* const LogColor = FMessageLog::GetLogColor(NewMessage->GetSeverity());
		if (LogColor)
		{
			SET_WARN_COLOR(LogColor);
		}

		FMsg::Logf(__FILE__, __LINE__, *LogName.ToString(), FMessageLog::GetLogVerbosity(NewMessage->GetSeverity()), TEXT("%s"), *NewMessage->ToText().ToString());

		CLEAR_WARN_COLOR();
	}
}

void FMessageLogListingModel::AddMessage( const TSharedRef<FTokenizedMessage>& NewMessage, bool bMirrorToOutputLog )
{
	CreateNewPageIfRequired();

	AddMessageInternal( NewMessage, bMirrorToOutputLog );

	Notify();
}

void FMessageLogListingModel::AddMessages( const TArray< TSharedRef<class FTokenizedMessage> >& NewMessages, bool bMirrorToOutputLog )
{
	CreateNewPageIfRequired();

	for( int32 MessageIdx = 0; MessageIdx < NewMessages.Num(); MessageIdx++ )
	{
		AddMessageInternal( NewMessages[MessageIdx], bMirrorToOutputLog );
	}
	Notify();
}

void FMessageLogListingModel::ClearMessages()
{
	CurrentPage().Messages.Empty();
	Notify();
}

void FMessageLogListingModel::NewPage( const FText& InTitle, uint32 InMaxPages )
{
	FMsg::Logf(__FILE__, __LINE__, *LogName.ToString(), ELogVerbosity::Log, TEXT("New page: %s"), *InTitle.ToString());

	// store the name of the page we will add if a new message is pushed
	PendingPageName = InTitle;
	MaxPages = InMaxPages;

	// if the current output page has messages, we create a new page now
	if( CurrentPage().Messages.Num() > 0 )
	{
		CreateNewPageIfRequired();
	}
}

uint32 FMessageLogListingModel::NumPages() const
{
	return Pages.Num();
}

uint32 FMessageLogListingModel::NumMessages( uint32 PageIndex ) const
{
	return PageAtIndex(PageIndex)->Messages.Num();
}

FMessageLogListingModel::FPage& FMessageLogListingModel::CurrentPage() const
{
	check(Pages.Num() > 0);
	return Pages.GetHead()->GetValue();
}

FMessageLogListingModel::FPage* FMessageLogListingModel::PageAtIndex(const uint32 PageIndex) const
{
	check(PageIndex < (uint32)Pages.Num());
	if(CachedPage != NULL && CachedPageIndex == PageIndex)
	{
		return CachedPage;
	}
	else
	{
		uint32 CurrentIndex = 0;
		for(auto Node = Pages.GetHead(); Node; Node = Node->GetNextNode())
		{
			if(CurrentIndex == PageIndex)
			{
				CachedPage = &Node->GetValue();
				CachedPageIndex = CurrentIndex;
				return CachedPage;
			}
			CurrentIndex++;
		}
	}

	check(false);	// Should never get here!
	return NULL;
}

const FText& FMessageLogListingModel::GetPageTitle( const uint32 PageIndex ) const
{
	return PageAtIndex(PageIndex)->Title;
}

void FMessageLogListingModel::CreateNewPageIfRequired()
{
	if(!PendingPageName.IsEmpty())
	{
		// dont create a new page if the current is empty, just change its name
		if(CurrentPage().Messages.Num() != 0)
		{
			// remove any pages that exceed the max page count
			while(Pages.Num() >= (int32)MaxPages)
			{
				Pages.RemoveNode(Pages.GetTail());
			}
			Pages.AddHead(FPage(PendingPageName));

			// invalidate cache as all indices will change
			CachedPage = NULL;
		}
		else
		{
			CurrentPage().Title = PendingPageName;
		}

		PendingPageName = FText::GetEmpty();
	}
}

bool FMessageLogListingModel::AreMessagesEqual(const TSharedRef< FTokenizedMessage >& MessageA, const TSharedRef< FTokenizedMessage >& MessageB)
{
	if(MessageA->GetMessageTokens().Num() == MessageB->GetMessageTokens().Num())
	{
		auto TokenItA(MessageA->GetMessageTokens().CreateConstIterator());
		auto TokenItB(MessageB->GetMessageTokens().CreateConstIterator());
		for( ; TokenItA && TokenItB; TokenItA++, TokenItB++)
		{
			TSharedRef<IMessageToken> TokenA = *TokenItA;
			TSharedRef<IMessageToken> TokenB = *TokenItB;
			if ( TokenA->GetType() != TokenB->GetType() || !TokenA->ToText().EqualTo( TokenB->ToText() ) )
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

void FMessageLogListingModel::RemoveDuplicates(uint32 PageIndex)
{
	FPage* Page = PageAtIndex(PageIndex);
	if(Page != NULL)
	{
		for(int IndexOuter = Page->Messages.Num() - 1; IndexOuter >= 0; --IndexOuter)
		{
			for(int IndexInner = Page->Messages.Num() - 1; IndexInner >= 0; --IndexInner)
			{
				if(IndexInner != IndexOuter && AreMessagesEqual(Page->Messages[IndexInner], Page->Messages[IndexOuter]))
				{
					Page->Messages.RemoveAt(IndexOuter);
					break;
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
