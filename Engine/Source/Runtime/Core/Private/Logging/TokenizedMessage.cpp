// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Logging/TokenizedMessage.h"
#include "HAL/PlatformProcess.h"
#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE "Core.MessageLog"

TSharedRef<FTokenizedMessage> FTokenizedMessage::Create(EMessageSeverity::Type InSeverity, const FText& InMessageText)
{
	TSharedRef<FTokenizedMessage> Message = MakeShareable(new FTokenizedMessage());
	Message->SetSeverity( InSeverity );
	Message->AddToken( FSeverityToken::Create(InSeverity) );
	if(!InMessageText.IsEmpty())
	{
		Message->AddToken( FTextToken::Create(InMessageText) );
	}
	return Message;
}

FText FTokenizedMessage::ToText() const
{
	FText OutMessage;
	int32 TokenIndex = 0;

	// Don't duplicate starting severity when displaying as string - but display it if it differs (for whatever reason)
	if(MessageTokens.Num() > 0 && MessageTokens[0]->GetType() == EMessageToken::Severity)
	{
		TSharedRef<FSeverityToken> SeverityToken = StaticCastSharedRef<FSeverityToken>(MessageTokens[0]);
		if(SeverityToken->GetSeverity() != Severity)
		{
			OutMessage = FText::Format(LOCTEXT("SeverityMessageTokenFormatter", "{0}:"), FTokenizedMessage::GetSeverityText(Severity));
		}

		// Skip the first token message as the Severity gets added again in FMsg::Logf
		TokenIndex = 1;
	}
	else
	{
		OutMessage = FTokenizedMessage::GetSeverityText( Severity );
	}

	//@todo This is bad and not safe for localization, this needs to be refactored once rich text is implemented [9/24/2013 justin.sargent]
	for(; TokenIndex < MessageTokens.Num(); TokenIndex++)
	{
		if ( !OutMessage.IsEmpty() )
		{
			OutMessage = FText::Format( LOCTEXT("AggregateMessageTokenFormatter", "{0} {1}"), OutMessage, MessageTokens[TokenIndex]->ToText() );
		}
		else
		{
			OutMessage = MessageTokens[TokenIndex]->ToText();
		}
	}

	return OutMessage;
}

FText FTokenizedMessage::GetSeverityText( EMessageSeverity::Type InSeverity )
{
	switch (InSeverity)
	{
	case EMessageSeverity::CriticalError:		
		return LOCTEXT("CritError", "Critical Error");			
	case EMessageSeverity::Error:				
		return LOCTEXT("Error", "Error");						
	case EMessageSeverity::PerformanceWarning:	
		return LOCTEXT("PerfWarning", "Performance Warning");	
	case EMessageSeverity::Warning:				
		return LOCTEXT("Warning", "Warning");					
	case EMessageSeverity::Info:				
		return LOCTEXT("Info", "Info");							
	default:		
		return FText::GetEmpty();					
	}
}

FName FTokenizedMessage::GetSeverityIconName(EMessageSeverity::Type InSeverity)
{
	FName SeverityIconName;
	switch (InSeverity)
	{
	case EMessageSeverity::CriticalError:		
		SeverityIconName = "MessageLog.Error";		
		break;
	case EMessageSeverity::Error:				
		SeverityIconName = "MessageLog.Error";		
		break;
	case EMessageSeverity::PerformanceWarning:	
		SeverityIconName = "MessageLog.Warning";	
		break;
	case EMessageSeverity::Warning:				
		SeverityIconName = "MessageLog.Warning";	
		break;
	case EMessageSeverity::Info:				
		SeverityIconName = "MessageLog.Note";		
		break;
	default:		
		/* No icon for this type */						
		break;
	}
	return SeverityIconName;
}

TSharedRef<FTokenizedMessage> FTokenizedMessage::AddToken( const TSharedRef<IMessageToken>& InToken )
{
	MessageTokens.Add( InToken );
	return AsShared();
}


void FTokenizedMessage::SetMessageLink(const TSharedRef<IMessageToken>& InToken)
{
	MessageLink = InToken;
}

void FTokenizedMessage::SetSeverity( const EMessageSeverity::Type InSeverity )
{
	Severity = InSeverity;
}

EMessageSeverity::Type FTokenizedMessage::GetSeverity() const
{
	return Severity;
}

TSharedRef<FTokenizedMessage> FTokenizedMessage::SetMessageData( FTokenizedMiscData* InMessageData )
{
	MessageData = InMessageData;
	return AsShared();
}

FTokenizedMiscData* FTokenizedMessage::GetMessageData() const
{
	return MessageData;
}

const TArray<TSharedRef<IMessageToken> >& FTokenizedMessage::GetMessageTokens() const
{
	return MessageTokens;
}

TSharedPtr<IMessageToken> FTokenizedMessage::GetMessageLink() const
{
	return MessageLink;
}

FURLToken::FGenerateURL FURLToken::GenerateURL;

void FURLToken::VisitURL(const TSharedRef<IMessageToken>& Token, FString InURL)
{	
	FPlatformProcess::LaunchURL(*InURL, NULL, NULL);
}

FURLToken::FURLToken( const FString& InURL, const FText& InMessage )
{
	if(GenerateURL.IsBound())
	{
		URL = GenerateURL.Execute(InURL);
	}
	else
	{
		URL = InURL;
	}
	
	if ( !InMessage.IsEmpty() )
	{
		CachedText = InMessage;
	}
	else
	{
		CachedText = NSLOCTEXT("Core.MessageLog", "DefaultHelpURLLabel", "Help");
	}

	MessageTokenActivated = FOnMessageTokenActivated::CreateStatic(&FURLToken::VisitURL, URL);
}

FAssetNameToken::FOnGotoAsset FAssetNameToken::GotoAsset;

void FAssetNameToken::FindAsset(const TSharedRef<IMessageToken>& Token, FString InAssetName)
{
	if(GotoAsset.IsBound())
	{
		GotoAsset.Execute(InAssetName);
	}
}

TSharedRef<FAssetNameToken> FAssetNameToken::Create(const FString& InAssetName, const FText& InMessage)
{
	return MakeShareable(new FAssetNameToken(InAssetName, InMessage));
}

FAssetNameToken::FAssetNameToken(const FString& InAssetName, const FText& InMessage)
	: AssetName(InAssetName)
{
	if ( !InMessage.IsEmpty() )
	{
		CachedText = InMessage;
	}
	else
	{
		CachedText = FText::FromString( AssetName );
	}

	MessageTokenActivated = FOnMessageTokenActivated::CreateStatic(&FAssetNameToken::FindAsset, AssetName);
}

FDocumentationToken::FDocumentationToken( const FString& InDocumentationLink, const FString& InPreviewExcerptLink, const FString& InPreviewExcerptName )
	: DocumentationLink(InDocumentationLink)
	, PreviewExcerptLink(InPreviewExcerptLink)
	, PreviewExcerptName(InPreviewExcerptName)
{ }

TSharedRef<FDocumentationToken> FDocumentationToken::Create(const FString& InDocumentationLink, const FString& InPreviewExcerptLink, const FString& InPreviewExcerptName)
{
	return MakeShareable(new FDocumentationToken(InDocumentationLink, InPreviewExcerptLink, InPreviewExcerptName));
}

#undef LOCTEXT_NAMESPACE
