// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Widgets/Input/IVirtualKeyboardEntry.h"
#include "SlateWrapperTypes.generated.h"


#define BIND_UOBJECT_ATTRIBUTE(Type, Function) \
	TAttribute<Type>::Create( TAttribute<Type>::FGetter::CreateUObject( this, &ThisClass::Function ) )

#define BIND_UOBJECT_DELEGATE(Type, Function) \
	Type::CreateUObject( this, &ThisClass::Function )

/** Is an entity visible? */
UENUM(BlueprintType)
enum class ESlateVisibility : uint8
{
	/** Default widget visibility - visible and can interact with the cursor */
	Visible,
	/** Not visible and takes up no space in the layout; can never be clicked on because it takes up no space. */
	Collapsed,
	/** Not visible, but occupies layout space. Not interactive for obvious reasons. */
	Hidden,
	/** Visible to the user, but only as art. The cursors hit tests will never see this widget. */
	HitTestInvisible,
	/** Same as HitTestInvisible, but doesn't apply to child widgets. */
	SelfHitTestInvisible
};

/** The sizing options of UWidgets */
UENUM(BlueprintType)
namespace ESlateSizeRule
{
	enum Type
	{
		/** Only requests as much room as it needs based on the widgets desired size. */
		Automatic,
		/** Greedily attempts to fill all available room based on the percentage value 0..1 */
		Fill
	};
}

/**
 * Allows users to handle events and return information to the underlying UI layer.
 */
USTRUCT(BlueprintType)
struct FEventReply
{
	GENERATED_USTRUCT_BODY()

public:

	FEventReply(bool IsHandled = false)
	: NativeReply(IsHandled ? FReply::Handled() : FReply::Unhandled())
	{
	}

	FReply NativeReply;
};

/** A struct exposing size param related properties to UMG. */
USTRUCT(BlueprintType)
struct FSlateChildSize
{
	GENERATED_USTRUCT_BODY()

	/** The parameter of the size rule. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance, meta=( UIMin="0", UIMax="1" ))
	float Value;

	/** The sizing rule of the content. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	TEnumAsByte<ESlateSizeRule::Type> SizeRule;

	FSlateChildSize()
		: Value(1.0f)
		, SizeRule(ESlateSizeRule::Fill)
	{
	}

	FSlateChildSize(ESlateSizeRule::Type InSizeRule)
		: Value(1.0f)
		, SizeRule(InSizeRule)
	{
	}
};


UENUM( BlueprintType )
namespace EVirtualKeyboardType
{
	enum Type
	{
		Default,
		Number,
		Web,
		Email,
		Password,
		AlphaNumeric
	};
}

namespace EVirtualKeyboardType
{
	static EKeyboardType AsKeyboardType( Type InType )
	{
		switch ( InType )
		{
		case Type::Default:
			return EKeyboardType::Keyboard_Default;
		case Type::Number:
			return EKeyboardType::Keyboard_Number;
		case Type::Web:
			return EKeyboardType::Keyboard_Web;
		case Type::Email:
			return EKeyboardType::Keyboard_Email;
		case Type::Password:
			return EKeyboardType::Keyboard_Password;
		case Type::AlphaNumeric:
			return EKeyboardType::Keyboard_AlphaNumeric;
		}

		return EKeyboardType::Keyboard_Default;
	}
}
