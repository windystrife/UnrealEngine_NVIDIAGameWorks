// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Templates/SharedPointer.h"
#include "Delegates/Delegate.h"
#include "Internationalization/Text.h"

class Error;

/** The severity of the message type */
namespace EMessageSeverity
{
	/** Ordered according to their severity */
	enum Type
	{
		CriticalError		= 0,
		Error				= 1,
		PerformanceWarning	= 2,
		Warning				= 3,
		Info				= 4,	// Should be last
	};
}

/** Delgate used when clocking a message token */
DECLARE_DELEGATE_OneParam(FOnMessageTokenActivated, const TSharedRef<class IMessageToken>&);

namespace EMessageToken
{
	enum Type
	{
		Action,
		AssetName,
		Documentation,
		Image,
		Object,
		Severity,
		Text,
		Tutorial,
		URL,
		EdGraph,
	};
}

/** A single message token for a FTokenizedMessage instance */
class IMessageToken : public TSharedFromThis<IMessageToken>
{
public:

	/**
	 * Virtual destructor
	 */
	virtual ~IMessageToken() {}

	/** 
	 * Get the type of this message token
	 * 
	 * @returns the type of the token
	 */
	virtual EMessageToken::Type GetType() const = 0;

	/** 
	 * Get a string representation of this token
	 * 
	 * @returns a string representing this token
	 */
	virtual const FText& ToText() const
	{
		return CachedText;
	}

	/** 
	 * Get the activated delegate associated with this token, if any
	 * 
	 * @returns a reference to the delegate
	 */
	virtual const FOnMessageTokenActivated& GetOnMessageTokenActivated() const
	{
		return MessageTokenActivated;
	}

	/** 
	 * Set the activated delegate associated with this token
	 * 
	 * @returns a reference to this token, for chaining
	 */
	virtual TSharedRef<IMessageToken> OnMessageTokenActivated( FOnMessageTokenActivated InMessageTokenActivated )
	{
		MessageTokenActivated = InMessageTokenActivated;
		return AsShared();
	}

protected:
	/** A delegate for when this token is activated (e.g. hyperlink clicked) */
	FOnMessageTokenActivated MessageTokenActivated;

	/** Cached string representation of this token */
	FText CachedText;
};


/** Dummy struct that other structs need to inherit from if they wish to assign to MessageData */
struct FTokenizedMiscData {};

/** This class represents a rich tokenized message, such as would be used for compiler output with 'hyperlinks' to source file locations */
class FTokenizedMessage : public TSharedFromThis<FTokenizedMessage>
{
public:

	/** 
	 * Creates a new FTokenizedMessage
	 * 
	 * @param	InSeverity			The severity of the message.
	 * @param	InMessageString		The string to display for this message. If this is not empty, then a string token will be added to this message.
	 * @returns	the generated message for temporary storage or so other tokens can be added to it if required.
	 */
	CORE_API static TSharedRef<FTokenizedMessage> Create(EMessageSeverity::Type InSeverity, const FText& InMessageText = FText());

	/** 
	 * Get this tokenized message as a string
	 * 
	 * @returns a string representation of this message
	 */
	CORE_API FText ToText() const;
	
	/** 
	 * Adds a token to a message.
	 * @param	InMessage	The message to insert a token into
	 * @param	InToken		The token to insert
	 * @returns this message, for chaining calls.
	 */
	CORE_API TSharedRef<FTokenizedMessage> AddToken( const TSharedRef<IMessageToken>& InToken );

	/** 
	 * Sets the severity of this message
	 * 
	 * @param	InSeverity	The severity to set this message to

	 */
	CORE_API void SetSeverity( const EMessageSeverity::Type InSeverity );

	/** 
	 * Gets the severity of this message
	 * 
	 * @returns the severity of this message
	 */
	CORE_API EMessageSeverity::Type GetSeverity() const;

	/** 
	 * Sets the custom message data of this message
	 * 
	 * @param	InMessageData	The custom message data to set
	 * @returns this message, so calls can be chained together
	 */
	CORE_API TSharedRef<FTokenizedMessage> SetMessageData( FTokenizedMiscData* InMessageData );

	/** 
	 * Gets the custom message data of this message
	 * 
	 * @returns the custom message data contained in this message
	 */
	CORE_API FTokenizedMiscData* GetMessageData() const;

	/**
	 * Get the tokens in this message
	 * 
	 * @returns an array of tokens
	 */
	CORE_API const TArray< TSharedRef<IMessageToken> >& GetMessageTokens() const;

	/**
	 * Sets up a token action for the message as a whole (not to be displayed...
	 * intended to be invoked from a double click).
	 * 
	 * @param  InToken	A token for the entire message to link to.
	 */
	CORE_API void SetMessageLink(const TSharedRef<IMessageToken>& InToken);

	/**
	 * Gets the token action associated with this messages as a whole. Should
	 * link to an associated element.
	 * 
	 * @return A token action that was set via SetMessageLink().
	 */
	CORE_API TSharedPtr<IMessageToken> GetMessageLink() const;

	/** 
	 * Helper function for getting a severity as text
	 *
	 * @param	InSeverity	the severity to use
	 * @returns a string representation of the severity
	 */
	CORE_API static FText GetSeverityText(EMessageSeverity::Type InSeverity);

	/** 
	 * Helper function for getting a severity as an icon name
	 *
	 * @param	InSeverity	the severity to use
	 * @returns the name of the icon for the severity
	 */
	CORE_API static FName GetSeverityIconName(EMessageSeverity::Type InSeverity);

private:
	/** Private constructor - we want to only create these structures as shared references via Create() */
	FTokenizedMessage()
		: Severity( EMessageSeverity::Info )
		, MessageData( nullptr )
	{ }

protected:

	/** the array of message tokens this message contains */
	TArray< TSharedRef<IMessageToken> > MessageTokens;

	/** A token associated with the entire message (doesn't display) */
	TSharedPtr<IMessageToken> MessageLink;

private:

	/** The severity of this message */
	EMessageSeverity::Type Severity;

	/** A payload for this message */
	FTokenizedMiscData* MessageData;	// @todo Don't use TSharedPtr as it'll get free'd mid-sort! (refcount == 0)
};

/** Basic message token with a localized text payload */
class FTextToken : public IMessageToken
{
public:
	/** Factory method, tokens can only be constructed as shared refs */
	CORE_API static TSharedRef<FTextToken> Create( const FText& InMessage )
	{
		return MakeShareable(new FTextToken(InMessage));
	}

	/** Begin IMessageToken interface */
	virtual EMessageToken::Type GetType() const override
	{
		return EMessageToken::Text;
	}
	/** End IMessageToken interface */

private:
	/** Private constructor */
	FTextToken( const FText& InMessage )
	{
		CachedText = InMessage;
	}
};

/** Basic message token with an icon/image payload */
class FImageToken : public IMessageToken
{
public:
	/** Factory method, tokens can only be constructed as shared refs */
	CORE_API static TSharedRef<FImageToken> Create( const FName& InImageName )
	{
		return MakeShareable(new FImageToken(InImageName));
	}

	/** Begin IMessageToken interface */
	virtual EMessageToken::Type GetType() const override
	{
		return EMessageToken::Image;
	}
	/** End IMessageToken interface */

	/** Get the name of the image for this token */
	const FName& GetImageName() const
	{
		return ImageName;
	}

private:
	/** Private constructor */
	FImageToken( const FName& InImageName )
		: ImageName(InImageName)
	{
		CachedText = FText::FromName( InImageName );
	}

	/** A name to be used as a brush in this message */
	FName ImageName;
};

/** Basic message token with a severity payload */
class FSeverityToken : public IMessageToken
{
public:
	/** Factory method, tokens can only be constructed as shared refs */
	CORE_API static TSharedRef<FSeverityToken> Create( EMessageSeverity::Type InSeverity )
	{
		return MakeShareable(new FSeverityToken(InSeverity));
	}

	/** Begin IMessageToken interface */
	virtual EMessageToken::Type GetType() const override
	{
		return EMessageToken::Severity;
	}
	/** End IMessageToken interface */

	/** Get the severity of this token */
	EMessageSeverity::Type GetSeverity() const
	{
		return Severity;
	}

private:
	/** Private constructor */
	FSeverityToken( EMessageSeverity::Type InSeverity )
		: Severity(InSeverity)
	{
		CachedText = FTokenizedMessage::GetSeverityText( InSeverity );
	}

	/** A severity for this token */
	EMessageSeverity::Type Severity;
};

/** Basic message token that defaults is activated method to traverse a URL */
class FURLToken : public IMessageToken
{
public:
	/** Factory method, tokens can only be constructed as shared refs */
	CORE_API static TSharedRef<FURLToken> Create( const FString& InURL, const FText& InMessage = FText() )
	{
		return MakeShareable(new FURLToken(InURL, InMessage));
	}

	/** Begin IMessageToken interface */
	virtual EMessageToken::Type GetType() const override
	{
		return EMessageToken::URL;
	}
	/** End IMessageToken interface */

	/** Get the URL used by this token */
	const FString& GetURL() const
	{
		return URL;
	}

	DECLARE_DELEGATE_RetVal_OneParam(FString, FGenerateURL, const FString&);
	CORE_API static FGenerateURL& OnGenerateURL()
	{
		return GenerateURL;
	}

private:
	/** Private constructor */
	FURLToken( const FString& InURL, const FText& InMessage );

	/**
	 * Delegate used to visit a URL
	 * @param	Token		The token that was clicked
	 * @param	InURL		The URL to visit
	 */
	static void VisitURL(const TSharedRef<IMessageToken>& Token, FString InURL);

	/** The URL we will follow */
	FString URL;

	/** The delegate we will use to generate our URL */
	CORE_API static FGenerateURL GenerateURL;
};

/** 
 * Basic message token that defaults its activated method to find a file
 * Intended to hook into things like the content browser.
 */
class FAssetNameToken : public IMessageToken
{
public:
	/** Factory method, tokens can only be constructed as shared refs */
	CORE_API static TSharedRef<FAssetNameToken> Create(const FString& InAssetName, const FText& InMessage = FText());

	/** Begin IMessageToken interface */
	virtual EMessageToken::Type GetType() const override
	{
		return EMessageToken::AssetName;
	}
	/** End IMessageToken interface */

	/** Get the filename used by this token */
	const FString& GetAssetName() const
	{
		return AssetName;
	}

	DECLARE_DELEGATE_OneParam(FOnGotoAsset, const FString&);
	CORE_API static FOnGotoAsset& OnGotoAsset()
	{
		return GotoAsset;
	}

private:
	/** Private constructor */
	FAssetNameToken( const FString& InAssetName, const FText& InMessage );

	/**
	 * Delegate used to find a file
	 * @param	Token		The token that was clicked
	 * @param	InURL		The file to find
	 */
	static void FindAsset(const TSharedRef<IMessageToken>& Token, FString InAssetName);

	/** The asset name we will find */
	FString AssetName;

	/** The delegate we will use to go to our file */
	CORE_API static FOnGotoAsset GotoAsset;
};

/** 
 * Basic message token that defaults is activated method to access UDN documentation.
 */
class FDocumentationToken : public IMessageToken
{
public:
	/** Factory method, tokens can only be constructed as shared refs */
	CORE_API static TSharedRef<FDocumentationToken> Create(const FString& InDocumentationLink, const FString& InPreviewExcerptLink = FString(), const FString& InPreviewExcerptName = FString());

	/** Begin IMessageToken interface */
	virtual EMessageToken::Type GetType() const override
	{
		return EMessageToken::Documentation;
	}
	/** End IMessageToken interface */

	/** Get the documentation link used by this token */
	const FString& GetDocumentationLink() const
	{
		return DocumentationLink;
	}

	/** Get the documentation excerpt link used by this token */
	const FString& GetPreviewExcerptLink() const
	{
		return PreviewExcerptLink;
	}

	/** Get the documentation excerpt name used by this token */
	const FString& GetPreviewExcerptName() const
	{
		return PreviewExcerptName;
	}

protected:
	/** Protected constructor */
	FDocumentationToken( const FString& InDocumentationLink, const FString& InPreviewExcerptLink, const FString& InPreviewExcerptName );

private:
	/** The documentation path we link to when clicked */
	FString DocumentationLink;

	/** The link we display an excerpt from */
	FString PreviewExcerptLink;

	/** The excerpt to display */
	FString PreviewExcerptName;
};


DECLARE_DELEGATE(FOnActionTokenExecuted);

/**
 * Message token that performs an action when activated.
 */
class FActionToken
	: public IMessageToken
{
public:

	/** Factory method, tokens can only be constructed as shared refs */
	CORE_API static TSharedRef<FActionToken> Create( const FText& ActionName, const FText& ActionDescription, const FOnActionTokenExecuted& Action, bool bInSingleUse=false )
	{
		return MakeShareable(new FActionToken(ActionName, ActionDescription, Action, bInSingleUse));
	}

	/** Executes the assigned action delegate. */
	void ExecuteAction()
	{
		Action.ExecuteIfBound();
		bActionExecuted = true;
	}

	/** Gets the action's description text. */
	const FText& GetActionDescription()
	{
		return ActionDescription;
	}

	/** Returns true if the action can be activated */
	bool CanExecuteAction() const
	{
		return Action.IsBound() && (!bSingleUse || !bActionExecuted);
	}

public:

	// IMessageToken interface
	virtual EMessageToken::Type GetType() const override
	{
		return EMessageToken::Action;
	}

protected:

	/** Hidden constructor. */
	FActionToken(const FText& InActionName, const FText& InActionDescription, const FOnActionTokenExecuted& InAction, bool bInSingleUse)
		: Action(InAction)
		, ActionDescription(InActionDescription)
		, bSingleUse(bInSingleUse)
		, bActionExecuted(false)
	{
		CachedText = InActionName;
	}

private:

	/** Holds a delegate that is executed when this token is activated. */
	FOnActionTokenExecuted Action;

	/** The action's description text. */
	const FText ActionDescription;
	
	/** If true, the action can only be performed once. */
	bool bSingleUse;

	/** If true, the action has been executed already. */
	bool bActionExecuted;
};


class FTutorialToken
	: public IMessageToken
{
public:

	/** Factory method, tokens can only be constructed as shared refs */
	CORE_API static TSharedRef<FTutorialToken> Create( const FString& TutorialAssetName )
	{
		return MakeShareable(new FTutorialToken(TutorialAssetName));
	}

public:

	// IMessageToken interface

	virtual EMessageToken::Type GetType() const override
	{
		return EMessageToken::Tutorial;
	}

	/** Get the tutorial asset name stored in this token. */
	const FString& GetTutorialAssetName() const
	{
		return TutorialAssetName;
	}

protected:
	/** Protected constructor */
	FTutorialToken( const FString& InTutorialAssetName )
		: TutorialAssetName(InTutorialAssetName)
	{ }

private:

	/** The name of the tutorial asset. */
	FString TutorialAssetName;
};

