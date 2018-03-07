// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Layout/Clipping.h"
#include "Rendering/SlateRenderTransform.h"
#include "GenericPlatform/ICursor.h"
#include "Types/ISlateMetaData.h"
#include "Widgets/SNullWidget.h"

class IToolTip;
class SUserWidget;
class SWidget;
template<typename WidgetType> struct TSlateBaseNamedArgs;

/**
 * Slate widgets are constructed through SNew and SAssignNew.
 * e.g.
 *      
 *     TSharedRef<SButton> MyButton = SNew(SButton);
 *        or
 *     TSharedPtr<SButton> MyButton;
 *     SAssignNew( MyButton, SButton );
 *
 * Using SNew and SAssignNew ensures that widgets are populated
 */

#define SNew( WidgetType, ... ) \
	MakeTDecl<WidgetType>( #WidgetType, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__) ) <<= TYPENAME_OUTSIDE_TEMPLATE WidgetType::FArguments()


#define SAssignNew( ExposeAs, WidgetType, ... ) \
	MakeTDecl<WidgetType>( #WidgetType, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__) ) . Expose( ExposeAs ) <<= TYPENAME_OUTSIDE_TEMPLATE WidgetType::FArguments()


/**
 * Widget authors can use SLATE_BEGIN_ARGS and SLATE_END_ARS to add support
 * for widget construction via SNew and SAssignNew.
 * e.g.
 * 
 *    SLATE_BEGIN_ARGS( SMyWidget )
 *         , _PreferredWidth( 150.0f )
 *         , _ForegroundColor( FLinearColor::White )
 *         {}
 *
 *         SLATE_ATTRIBUTE(float, PreferredWidth)
 *         SLATE_ATTRIBUTE(FSlateColor, ForegroundColor)
 *    SLATE_END_ARGS()
 */

#define SLATE_BEGIN_ARGS( WidgetType ) \
	public: \
	struct FArguments : public TSlateBaseNamedArgs<WidgetType> \
	{ \
		typedef FArguments WidgetArgsType; \
		FORCENOINLINE FArguments()

/**
 * Just like SLATE_BEGIN_ARGS but requires the user to implement the New() method in the .CPP.
 * Allows for widget implementation details to be entirely reside in the .CPP file.
 * e.g.
 *   SMyWidget.h:
 *   ----------------------------------------------------------------
 *    SLATE_USER_ARGS( SMyWidget )
 *    {}
 *    SLATE_END_ARGS()
 *    
 *    virtual void DoStuff() = nullptr;
 *
 *   SMyWidget.cpp:
 *   ----------------------------------------------------------------
 *   namespace Implementation{
 *   class SMyWidget : public SMyWidget{
 *     void Construct( const FArguments& InArgs )
 *     {
 *         SUserWidget::Construct( SUSerWidget::FArguments()
 *         [
 *             SNew(STextBlock) .Text( "Implementation Details!" )
 *         ] );
 *     }
 *
 *     virtual void DoStuff() override {}
 *     
 *     // Truly private. All handlers can be inlined (less boilerplate).
 *     // All private members are truly private.
 *     private:
 *     FReply OnButtonClicked
 *     {
 *     }
 *     TSharedPtr<SButton> MyButton;
 *   }
 *   }
 */
#define SLATE_USER_ARGS( WidgetType ) \
	public: \
	static TSharedRef<WidgetType> New(); \
	struct FArguments; \
	struct FArguments : public TSlateBaseNamedArgs<WidgetType> \
	{ \
		typedef FArguments WidgetArgsType; \
		FORCENOINLINE FArguments()


// @todo UMG: Probably remove this?
#define HACK_SLATE_SLOT_ARGS( WidgetType ) \
	public: \
	struct FArguments : public TSlateBaseNamedArgs<WidgetType> \
	{ \
		typedef FArguments WidgetArgsType; \
		FORCENOINLINE FArguments()



#define SLATE_END_ARGS() \
	};


/**
 * Use this macro to add a attribute to the declaration of your widget.
 * An attribute can be a value or a function.
 */
#define SLATE_ATTRIBUTE( AttrType, AttrName ) \
		TAttribute< AttrType > _##AttrName; \
		WidgetArgsType& AttrName( const TAttribute< AttrType >& InAttribute ) \
		{ \
			_##AttrName = InAttribute; \
			return this->Me(); \
		} \
	\
		/* Bind attribute with delegate to a global function
		 * NOTE: We use a template here to avoid 'typename' issues when hosting attributes inside templated classes */ \
		template< typename StaticFuncPtr > \
		WidgetArgsType& AttrName##_Static( StaticFuncPtr InFunc )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateStatic( InFunc ) ); \
			return this->Me(); \
		} \
		template< typename Var1Type > \
		WidgetArgsType& AttrName##_Static( typename TAttribute< AttrType >::FGetter::template TStaticDelegate_OneVar< Var1Type >::FFuncPtr InFunc, Var1Type Var1 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateStatic( InFunc, Var1 ) ); \
			return this->Me(); \
		} \
		template< typename Var1Type, typename Var2Type > \
		WidgetArgsType& AttrName##_Static( typename TAttribute< AttrType >::FGetter::template TStaticDelegate_TwoVars< Var1Type, Var2Type >::FFuncPtr InFunc, Var1Type Var1, Var2Type Var2 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateStatic( InFunc, Var1, Var2 ) ); \
			return this->Me(); \
		} \
		template< typename Var1Type, typename Var2Type, typename Var3Type > \
		WidgetArgsType& AttrName##_Static( typename TAttribute< AttrType >::FGetter::template TStaticDelegate_ThreeVars< Var1Type, Var2Type, Var3Type >::FFuncPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateStatic( InFunc, Var1, Var2, Var3 ) ); \
			return this->Me(); \
		} \
		template< typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type > \
		WidgetArgsType& AttrName##_Static( typename TAttribute< AttrType >::FGetter::template TStaticDelegate_FourVars< Var1Type, Var2Type, Var3Type, Var4Type >::FFuncPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateStatic( InFunc, Var1, Var2, Var3, Var4 ) ); \
			return this->Me(); \
		} \
	\
		/* Bind attribute with delegate to a lambda
		 * technically this works for any functor types, but lambdas are the primary use case */ \
		WidgetArgsType& AttrName##_Lambda(TFunction< AttrType(void) >&& InFunctor) \
		{ \
			_##AttrName = TAttribute< AttrType >::Create(Forward<TFunction< AttrType(void) >>(InFunctor)); \
			return this->Me(); \
		} \
	\
		/* Bind attribute with delegate to a raw C++ class method */ \
		template< class UserClass >	\
		WidgetArgsType& AttrName##_Raw( UserClass* InUserObject, typename TAttribute< AttrType >::FGetter::template TRawMethodDelegate_Const< UserClass >::FMethodPtr InFunc )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateRaw( InUserObject, InFunc ) ); \
			return this->Me(); \
		} \
		template< class UserClass, typename Var1Type >	\
		WidgetArgsType& AttrName##_Raw( UserClass* InUserObject, typename TAttribute< AttrType >::FGetter::template TRawMethodDelegate_OneVar_Const< UserClass, Var1Type  >::FMethodPtr InFunc, Var1Type Var1 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateRaw( InUserObject, InFunc, Var1 ) ); \
			return this->Me(); \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type >	\
		WidgetArgsType& AttrName##_Raw( UserClass* InUserObject, typename TAttribute< AttrType >::FGetter::template TRawMethodDelegate_TwoVars_Const< UserClass, Var1Type, Var2Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateRaw( InUserObject, InFunc, Var1, Var2 ) ); \
			return this->Me(); \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type >	\
		WidgetArgsType& AttrName##_Raw( UserClass* InUserObject, typename TAttribute< AttrType >::FGetter::template TRawMethodDelegate_ThreeVars_Const< UserClass, Var1Type, Var2Type, Var3Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateRaw( InUserObject, InFunc, Var1, Var2, Var3 ) ); \
			return this->Me(); \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type >	\
		WidgetArgsType& AttrName##_Raw( UserClass* InUserObject, typename TAttribute< AttrType >::FGetter::template TRawMethodDelegate_FourVars_Const< UserClass, Var1Type, Var2Type, Var3Type, Var4Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateRaw( InUserObject, InFunc, Var1, Var2, Var3, Var4 ) ); \
			return this->Me(); \
		} \
	\
		/* Bind attribute with delegate to a shared pointer-based class method.  Slate mostly uses shared pointers so we use an overload for this type of binding. */ \
		template< class UserClass >	\
		WidgetArgsType& AttrName( TSharedRef< UserClass > InUserObjectRef, typename TAttribute< AttrType >::FGetter::template TSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )	\
		{ \
			_##AttrName = TAttribute< AttrType >( InUserObjectRef, InFunc ); \
			return this->Me(); \
		} \
		template< class UserClass, typename Var1Type >	\
		WidgetArgsType& AttrName( TSharedRef< UserClass > InUserObjectRef, typename TAttribute< AttrType >::FGetter::template TSPMethodDelegate_OneVar_Const< UserClass, Var1Type >::FMethodPtr InFunc, Var1Type Var1 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateSP( InUserObjectRef, InFunc, Var1 ) ); \
			return this->Me(); \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type >	\
		WidgetArgsType& AttrName( TSharedRef< UserClass > InUserObjectRef, typename TAttribute< AttrType >::FGetter::template TSPMethodDelegate_TwoVars_Const< UserClass, Var1Type, Var2Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateSP( InUserObjectRef, InFunc, Var1, Var2 ) ); \
			return this->Me(); \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type >	\
		WidgetArgsType& AttrName( TSharedRef< UserClass > InUserObjectRef, typename TAttribute< AttrType >::FGetter::template TSPMethodDelegate_ThreeVars_Const< UserClass, Var1Type, Var2Type, Var3Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateSP( InUserObjectRef, InFunc, Var1, Var2, Var3 ) ); \
			return this->Me(); \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type >	\
		WidgetArgsType& AttrName( TSharedRef< UserClass > InUserObjectRef, typename TAttribute< AttrType >::FGetter::template TSPMethodDelegate_FourVars_Const< UserClass, Var1Type, Var2Type, Var3Type, Var4Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateSP( InUserObjectRef, InFunc, Var1, Var2, Var3, Var4 ) ); \
			return this->Me(); \
		} \
	\
		/* Bind attribute with delegate to a shared pointer-based class method.  Slate mostly uses shared pointers so we use an overload for this type of binding. */ \
		template< class UserClass >	\
		WidgetArgsType& AttrName( UserClass* InUserObject, typename TAttribute< AttrType >::FGetter::template TSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateSP( InUserObject, InFunc ) ); \
			return this->Me(); \
		} \
		template< class UserClass, typename Var1Type >	\
		WidgetArgsType& AttrName( UserClass* InUserObject, typename TAttribute< AttrType >::FGetter::template TSPMethodDelegate_OneVar_Const< UserClass, Var1Type >::FMethodPtr InFunc, Var1Type Var1 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateSP( InUserObject, InFunc, Var1 ) ); \
			return this->Me(); \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type >	\
		WidgetArgsType& AttrName( UserClass* InUserObject, typename TAttribute< AttrType >::FGetter::template TSPMethodDelegate_TwoVars_Const< UserClass, Var1Type, Var2Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateSP( InUserObject, InFunc, Var1, Var2 ) ); \
			return this->Me(); \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type >	\
		WidgetArgsType& AttrName( UserClass* InUserObject, typename TAttribute< AttrType >::FGetter::template TSPMethodDelegate_ThreeVars_Const< UserClass, Var1Type, Var2Type, Var3Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateSP( InUserObject, InFunc, Var1, Var2, Var3 ) ); \
			return this->Me(); \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type >	\
		WidgetArgsType& AttrName( UserClass* InUserObject, typename TAttribute< AttrType >::FGetter::template TSPMethodDelegate_FourVars_Const< UserClass, Var1Type, Var2Type, Var3Type, Var4Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateSP( InUserObject, InFunc, Var1, Var2, Var3, Var4 ) ); \
			return this->Me(); \
		} \
	\
		/* Bind attribute with delegate to a UObject-based class method */ \
		template< class UserClass >	\
		WidgetArgsType& AttrName##_UObject( UserClass* InUserObject, typename TAttribute< AttrType >::FGetter::template TUObjectMethodDelegate_Const< UserClass >::FMethodPtr InFunc )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateUObject( InUserObject, InFunc ) ); \
			return this->Me(); \
		} \
		template< class UserClass, typename Var1Type >	\
		WidgetArgsType& AttrName##_UObject( UserClass* InUserObject, typename TAttribute< AttrType >::FGetter::template TUObjectMethodDelegate_OneVar_Const< UserClass, Var1Type >::FMethodPtr InFunc, Var1Type Var1 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateUObject( InUserObject, InFunc, Var1 ) ); \
			return this->Me(); \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type  >	\
		WidgetArgsType& AttrName##_UObject( UserClass* InUserObject, typename TAttribute< AttrType >::FGetter::template TUObjectMethodDelegate_TwoVars_Const< UserClass, Var1Type, Var2Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateUObject( InUserObject, InFunc, Var1, Var2 ) ); \
			return this->Me(); \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type >	\
		WidgetArgsType& AttrName##_UObject( UserClass* InUserObject, typename TAttribute< AttrType >::FGetter::template TUObjectMethodDelegate_ThreeVars_Const< UserClass, Var1Type, Var2Type, Var3Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateUObject( InUserObject, InFunc, Var1, Var2, Var3 ) ); \
			return this->Me(); \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type >	\
		WidgetArgsType& AttrName##_UObject( UserClass* InUserObject, typename TAttribute< AttrType >::FGetter::template TUObjectMethodDelegate_FourVars_Const< UserClass, Var1Type, Var2Type, Var3Type, Var4Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4 )	\
		{ \
			_##AttrName = TAttribute< AttrType >::Create( TAttribute< AttrType >::FGetter::CreateUObject( InUserObject, InFunc, Var1, Var2, Var3, Var4 ) ); \
			return this->Me(); \
		}


/**
 * Use this macro to declare a slate argument.
 * Arguments differ from attributes in that they can only be values
 */
#define SLATE_ARGUMENT( ArgType, ArgName ) \
		ArgType _##ArgName; \
		WidgetArgsType& ArgName( ArgType InArg ) \
		{ \
			_##ArgName = InArg; \
			return this->Me(); \
		}


/**
 * Use this macro to declare a slate argument.
 * Arguments differ from attributes in that they can only be values
 */
#define SLATE_STYLE_ARGUMENT( ArgType, ArgName ) \
		const ArgType* _##ArgName; \
		WidgetArgsType& ArgName( const ArgType* InArg ) \
		{ \
			_##ArgName = InArg; \
			return this->Me(); \
		}\
		\
		WidgetArgsType& ArgName( const class USlateWidgetStyleAsset* const InSlateStyleAsset ) \
		{ \
			_##ArgName = InSlateStyleAsset->GetStyleChecked< ArgType >(); \
			return this->Me(); \
		}\
		\
		WidgetArgsType& ArgName( const TWeakObjectPtr< const class USlateWidgetStyleAsset >& InSlateStyleAsset ) \
		{ \
			_##ArgName = InSlateStyleAsset->GetStyleChecked< ArgType >(); \
			return this->Me(); \
		}\
		\
		WidgetArgsType& ArgName( const class ISlateStyle* InSlateStyle, const FName& StyleName, const ANSICHAR* Specifier = nullptr ) \
		{ \
			check( InSlateStyle != nullptr ); \
			_##ArgName = &InSlateStyle->GetWidgetStyle< ArgType >( StyleName, Specifier ); \
			return this->Me(); \
		}\
		\
		WidgetArgsType& ArgName( const class ISlateStyle& InSlateStyle, const FName& StyleName, const ANSICHAR* Specifier = nullptr ) \
		{ \
			_##ArgName = &InSlateStyle.GetWidgetStyle< ArgType >( StyleName, Specifier ); \
			return this->Me(); \
		}\
		\
		WidgetArgsType& ArgName( const TWeakObjectPtr< const class ISlateStyle >& InSlateStyle, const FName& StyleName, const ANSICHAR* Specifier = nullptr ) \
		{ \
			_##ArgName = &InSlateStyle->GetWidgetStyle< ArgType >( StyleName, Specifier ); \
			return this->Me(); \
		} \
		\
		WidgetArgsType& ArgName( const TSharedPtr< const class ISlateStyle >& InSlateStyle, const FName& StyleName, const ANSICHAR* Specifier = nullptr ) \
		{ \
			_##ArgName = &InSlateStyle->GetWidgetStyle< ArgType >( StyleName, Specifier ); \
			return this->Me(); \
		} 


/**
 * Use this macro between SLATE_BEGIN_ARGS and SLATE_END_ARGS
 * in order to add support for slots.
 */
#define SLATE_SUPPORTS_SLOT( SlotType ) \
		TArray< SlotType* > Slots; \
		WidgetArgsType& operator + (SlotType& SlotToAdd) \
		{ \
			Slots.Add( &SlotToAdd ); \
			return *this; \
		}


 /**
 * Use this macro between SLATE_BEGIN_ARGS and SLATE_END_ARGS
 * in order to add support for slots that have slate args.
 */
#define SLATE_SUPPORTS_SLOT_WITH_ARGS( SlotType ) \
	TArray< SlotType* > Slots; \
	WidgetArgsType& operator + (const SlotType::FArguments& ArgumentsForNewSlot) \
		{ \
			Slots.Add( new SlotType( ArgumentsForNewSlot ) ); \
			return *this; \
		}


/** A widget reference that is always a valid pointer; defaults to SNullWidget */
struct TAlwaysValidWidget
{
	TAlwaysValidWidget()
	: Widget(SNullWidget::NullWidget)
	{
	}

	TSharedRef<SWidget> Widget;
};


/**
 * We want to be able to do:
 * SNew( ContainerWidget )
 * .SomeContentArea()
 * [
 *   // Child widgets go here
 * ]
 *
 * NamedSlotProperty is a helper that will be returned by SomeContentArea().
 */
template<class DeclarationType>
struct NamedSlotProperty
{
	NamedSlotProperty( DeclarationType& InOwnerDeclaration, TAlwaysValidWidget& ContentToSet )
		: OwnerDeclaration( InOwnerDeclaration )
		, SlotContent(ContentToSet)
	{}

	DeclarationType & operator[]( const TSharedRef<SWidget>& InChild )
	{
		SlotContent.Widget = InChild;
		return OwnerDeclaration;
	}

	DeclarationType & OwnerDeclaration;
	TAlwaysValidWidget & SlotContent;
};


/**
 * Use this macro to add support for named slot properties such as Content and Header. See NamedSlotProperty for more details.
 *
 * NOTE: If you're using this within a widget class that is templated, then you might have to specify a full name for the declaration.
 *       For example: SLATE_NAMED_SLOT( typename SSuperWidget<T>::Declaration, Content )
 */
#define SLATE_NAMED_SLOT( DeclarationType, SlotName ) \
		NamedSlotProperty< DeclarationType > SlotName() \
		{ \
			return NamedSlotProperty< DeclarationType >( *this, _##SlotName ); \
		} \
		TAlwaysValidWidget _##SlotName; \

#define SLATE_DEFAULT_SLOT( DeclarationType, SlotName ) \
		SLATE_NAMED_SLOT(DeclarationType, SlotName) ; \
		DeclarationType & operator[]( const TSharedRef<SWidget> InChild ) \
		{ \
			_##SlotName.Widget = InChild; \
			return *this; \
		}


/**
 * Use this macro to add event handler support to the declarative syntax of your widget.
 * It is expected that the widget has a delegate called of type 'EventDelegateType' that is
 * named 'EventName'.
 */	
#define SLATE_EVENT( DelegateName, EventName ) \
		WidgetArgsType& EventName( const DelegateName& InDelegate ) \
		{ \
			_##EventName = InDelegate; \
			return *this; \
		} \
		\
		/* Set event delegate to a global function */ \
		/* NOTE: We use a template here to avoid 'typename' issues when hosting attributes inside templated classes */ \
		template< typename StaticFuncPtr > \
		WidgetArgsType& EventName##_Static( StaticFuncPtr InFunc )	\
		{ \
			_##EventName = DelegateName::CreateStatic( InFunc ); \
			return *this; \
		} \
		template< typename StaticFuncPtr, typename Var1Type > \
		WidgetArgsType& EventName##_Static( StaticFuncPtr InFunc, Var1Type Var1 )	\
		{ \
			_##EventName = DelegateName::CreateStatic( InFunc, Var1 ); \
			return *this; \
		} \
		template< typename StaticFuncPtr, typename Var1Type, typename Var2Type > \
		WidgetArgsType& EventName##_Static( StaticFuncPtr InFunc, Var1Type Var1, Var2Type Var2 )	\
		{ \
			_##EventName = DelegateName::CreateStatic( InFunc, Var1, Var2 ); \
			return *this; \
		} \
		template< typename StaticFuncPtr, typename Var1Type, typename Var2Type, typename Var3Type > \
		WidgetArgsType& EventName##_Static( StaticFuncPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3 )	\
		{ \
			_##EventName = DelegateName::CreateStatic( InFunc, Var1, Var2, Var3 ); \
			return *this; \
		} \
		template< typename StaticFuncPtr, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type > \
		WidgetArgsType& EventName##_Static( StaticFuncPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4 )	\
		{ \
			_##EventName = DelegateName::CreateStatic( InFunc, Var1, Var2, Var3, Var4 ); \
			return *this; \
		} \
		\
		/* Set event delegate to a lambda
		 * technically this works for any functor types, but lambdas are the primary use case */ \
		template<typename FunctorType> \
		WidgetArgsType& EventName##_Lambda(FunctorType&& InFunctor) \
		{ \
			_##EventName = DelegateName::CreateLambda(Forward<FunctorType>(InFunctor)); \
			return this->Me(); \
		} \
		\
		/* Set event delegate to a raw C++ class method */ \
		template< class UserClass >	\
		WidgetArgsType& EventName##_Raw( UserClass* InUserObject, typename DelegateName::template TRawMethodDelegate< UserClass >::FMethodPtr InFunc )	\
		{ \
			_##EventName = DelegateName::CreateRaw( InUserObject, InFunc ); \
			return *this; \
		} \
		template< class UserClass >	\
		WidgetArgsType& EventName##_Raw( UserClass* InUserObject, typename DelegateName::template TRawMethodDelegate_Const< UserClass >::FMethodPtr InFunc )	\
		{ \
			_##EventName = DelegateName::CreateRaw( InUserObject, InFunc ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type >	\
		WidgetArgsType& EventName##_Raw( UserClass* InUserObject, typename DelegateName::template TRawMethodDelegate_OneVar< UserClass, Var1Type >::FMethodPtr InFunc, Var1Type Var1 )	\
		{ \
			_##EventName = DelegateName::CreateRaw( InUserObject, InFunc, Var1 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type >	\
		WidgetArgsType& EventName##_Raw( UserClass* InUserObject, typename DelegateName::template TRawMethodDelegate_OneVar_Const< UserClass, Var1Type >::FMethodPtr InFunc, Var1Type Var1 )	\
		{ \
			_##EventName = DelegateName::CreateRaw( InUserObject, InFunc, Var1 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type >	\
		WidgetArgsType& EventName##_Raw( UserClass* InUserObject, typename DelegateName::template TRawMethodDelegate_TwoVars< UserClass, Var1Type, Var2Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2 )	\
		{ \
			_##EventName = DelegateName::CreateRaw( InUserObject, InFunc, Var1, Var2 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type >	\
		WidgetArgsType& EventName##_Raw( UserClass* InUserObject, typename DelegateName::template TRawMethodDelegate_TwoVars_Const< UserClass, Var1Type, Var2Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2 )	\
		{ \
			_##EventName = DelegateName::CreateRaw( InUserObject, InFunc, Var1, Var2 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type >	\
		WidgetArgsType& EventName##_Raw( UserClass* InUserObject, typename DelegateName::template TRawMethodDelegate_ThreeVars< UserClass, Var1Type, Var2Type, Var3Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3 )	\
		{ \
			_##EventName = DelegateName::CreateRaw( InUserObject, InFunc, Var1, Var2, Var3 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type >	\
		WidgetArgsType& EventName##_Raw( UserClass* InUserObject, typename DelegateName::template TRawMethodDelegate_ThreeVars_Const< UserClass, Var1Type, Var2Type, Var3Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3 )	\
		{ \
			_##EventName = DelegateName::CreateRaw( InUserObject, InFunc, Var1, Var2, Var3 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type >	\
		WidgetArgsType& EventName##_Raw( UserClass* InUserObject, typename DelegateName::template TRawMethodDelegate_FourVars< UserClass, Var1Type, Var2Type, Var3Type, Var4Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4 )	\
		{ \
			_##EventName = DelegateName::CreateRaw( InUserObject, InFunc, Var1, Var2, Var3, Var4 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type >	\
		WidgetArgsType& EventName##_Raw( UserClass* InUserObject, typename DelegateName::template TRawMethodDelegate_FourVars_Const< UserClass, Var1Type, Var2Type, Var3Type, Var4Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4 )	\
		{ \
			_##EventName = DelegateName::CreateRaw( InUserObject, InFunc, Var1, Var2, Var3, Var4 ); \
			return *this; \
		} \
		\
		/* Set event delegate to a shared pointer-based class method.  Slate mostly uses shared pointers so we use an overload for this type of binding. */ \
		template< class UserClass >	\
		WidgetArgsType& EventName( TSharedRef< UserClass > InUserObjectRef, typename DelegateName::template TSPMethodDelegate< UserClass >::FMethodPtr InFunc )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObjectRef, InFunc ); \
			return *this; \
		} \
		template< class UserClass >	\
		WidgetArgsType& EventName( TSharedRef< UserClass > InUserObjectRef, typename DelegateName::template TSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObjectRef, InFunc ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type >	\
		WidgetArgsType& EventName( TSharedRef< UserClass > InUserObjectRef, typename DelegateName::template TSPMethodDelegate_OneVar< UserClass, Var1Type >::FMethodPtr InFunc, Var1Type Var1 )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObjectRef, InFunc, Var1 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type >	\
		WidgetArgsType& EventName( TSharedRef< UserClass > InUserObjectRef, typename DelegateName::template TSPMethodDelegate_OneVar_Const< UserClass, Var1Type >::FMethodPtr InFunc, Var1Type Var1 )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObjectRef, InFunc, Var1 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type >	\
		WidgetArgsType& EventName( TSharedRef< UserClass > InUserObjectRef, typename DelegateName::template TSPMethodDelegate_TwoVars< UserClass, Var1Type, Var2Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2 )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObjectRef, InFunc, Var1, Var2 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type >	\
		WidgetArgsType& EventName( TSharedRef< UserClass > InUserObjectRef, typename DelegateName::template TSPMethodDelegate_TwoVars_Const< UserClass, Var1Type, Var2Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2 )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObjectRef, InFunc, Var1, Var2 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type >	\
		WidgetArgsType& EventName( TSharedRef< UserClass > InUserObjectRef, typename DelegateName::template TSPMethodDelegate_ThreeVars< UserClass, Var1Type, Var2Type, Var3Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3 )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObjectRef, InFunc, Var1, Var2, Var3 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type >	\
		WidgetArgsType& EventName( TSharedRef< UserClass > InUserObjectRef, typename DelegateName::template TSPMethodDelegate_ThreeVars_Const< UserClass, Var1Type, Var2Type, Var3Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3 )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObjectRef, InFunc, Var1, Var2, Var3 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type >	\
		WidgetArgsType& EventName( TSharedRef< UserClass > InUserObjectRef, typename DelegateName::template TSPMethodDelegate_FourVars< UserClass, Var1Type, Var2Type, Var3Type, Var4Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4 )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObjectRef, InFunc, Var1, Var2, Var3, Var4 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type >	\
		WidgetArgsType& EventName( TSharedRef< UserClass > InUserObjectRef, typename DelegateName::template TSPMethodDelegate_FourVars_Const< UserClass, Var1Type, Var2Type, Var3Type, Var4Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4 )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObjectRef, InFunc, Var1, Var2, Var3, Var4 ); \
			return *this; \
		} \
		\
		/* Set event delegate to a shared pointer-based class method.  Slate mostly uses shared pointers so we use an overload for this type of binding. */ \
		template< class UserClass >	\
		WidgetArgsType& EventName( UserClass* InUserObject, typename DelegateName::template TSPMethodDelegate< UserClass >::FMethodPtr InFunc )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObject, InFunc ); \
			return *this; \
		} \
		template< class UserClass >	\
		WidgetArgsType& EventName( UserClass* InUserObject, typename DelegateName::template TSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObject, InFunc ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type >	\
		WidgetArgsType& EventName( UserClass* InUserObject, typename DelegateName::template TSPMethodDelegate_OneVar< UserClass, Var1Type >::FMethodPtr InFunc, Var1Type Var1 )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObject, InFunc, Var1 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type >	\
		WidgetArgsType& EventName( UserClass* InUserObject, typename DelegateName::template TSPMethodDelegate_OneVar_Const< UserClass, Var1Type >::FMethodPtr InFunc, Var1Type Var1 )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObject, InFunc, Var1 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type >	\
		WidgetArgsType& EventName( UserClass* InUserObject, typename DelegateName::template TSPMethodDelegate_TwoVars< UserClass, Var1Type, Var2Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2 )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObject, InFunc, Var1, Var2 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type >	\
		WidgetArgsType& EventName( UserClass* InUserObject, typename DelegateName::template TSPMethodDelegate_TwoVars_Const< UserClass, Var1Type, Var2Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2 )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObject, InFunc, Var1, Var2 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type >	\
		WidgetArgsType& EventName( UserClass* InUserObject, typename DelegateName::template TSPMethodDelegate_ThreeVars< UserClass, Var1Type, Var2Type, Var3Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3 )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObject, InFunc, Var1, Var2, Var3 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type >	\
		WidgetArgsType& EventName( UserClass* InUserObject, typename DelegateName::template TSPMethodDelegate_ThreeVars_Const< UserClass, Var1Type, Var2Type, Var3Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3 )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObject, InFunc, Var1, Var2, Var3 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type >	\
		WidgetArgsType& EventName( UserClass* InUserObject, typename DelegateName::template TSPMethodDelegate_FourVars< UserClass, Var1Type, Var2Type, Var3Type, Var4Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4 )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObject, InFunc, Var1, Var2, Var3, Var4 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type >	\
		WidgetArgsType& EventName( UserClass* InUserObject, typename DelegateName::template TSPMethodDelegate_FourVars_Const< UserClass, Var1Type, Var2Type, Var3Type, Var4Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4 )	\
		{ \
			_##EventName = DelegateName::CreateSP( InUserObject, InFunc, Var1, Var2, Var3, Var4 ); \
			return *this; \
		} \
		\
		/* Set event delegate to a UObject-based class method */ \
		template< class UserClass >	\
		WidgetArgsType& EventName##_UObject( UserClass* InUserObject, typename DelegateName::template TUObjectMethodDelegate< UserClass >::FMethodPtr InFunc )	\
		{ \
			_##EventName = DelegateName::CreateUObject( InUserObject, InFunc ); \
			return *this; \
		} \
		template< class UserClass >	\
		WidgetArgsType& EventName##_UObject( UserClass* InUserObject, typename DelegateName::template TUObjectMethodDelegate_Const< UserClass >::FMethodPtr InFunc )	\
		{ \
			_##EventName = DelegateName::CreateUObject( InUserObject, InFunc ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type >	\
		WidgetArgsType& EventName##_UObject( UserClass* InUserObject, typename DelegateName::template TUObjectMethodDelegate_OneVar< UserClass, Var1Type >::FMethodPtr InFunc, Var1Type Var1 )	\
		{ \
			_##EventName = DelegateName::CreateUObject( InUserObject, InFunc, Var1 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type >	\
		WidgetArgsType& EventName##_UObject( UserClass* InUserObject, typename DelegateName::template TUObjectMethodDelegate_OneVar_Const< UserClass, Var1Type >::FMethodPtr InFunc, Var1Type Var1 )	\
		{ \
			_##EventName = DelegateName::CreateUObject( InUserObject, InFunc, Var1 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type >	\
		WidgetArgsType& EventName##_UObject( UserClass* InUserObject, typename DelegateName::template TUObjectMethodDelegate_TwoVars< UserClass, Var1Type, Var2Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2 )	\
		{ \
			_##EventName = DelegateName::CreateUObject( InUserObject, InFunc, Var1, Var2 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type >	\
		WidgetArgsType& EventName##_UObject( UserClass* InUserObject, typename DelegateName::template TUObjectMethodDelegate_TwoVars_Const< UserClass, Var1Type, Var2Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2 )	\
		{ \
			_##EventName = DelegateName::CreateUObject( InUserObject, InFunc, Var1, Var2 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type >	\
		WidgetArgsType& EventName##_UObject( UserClass* InUserObject, typename DelegateName::template TUObjectMethodDelegate_ThreeVars< UserClass, Var1Type, Var2Type, Var3Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3 )	\
		{ \
			_##EventName = DelegateName::CreateUObject( InUserObject, InFunc, Var1, Var2, Var3 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type >	\
		WidgetArgsType& EventName##_UObject( UserClass* InUserObject, typename DelegateName::template TUObjectMethodDelegate_ThreeVars_Const< UserClass, Var1Type, Var2Type, Var3Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3 )	\
		{ \
			_##EventName = DelegateName::CreateUObject( InUserObject, InFunc, Var1, Var2, Var3 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type >	\
		WidgetArgsType& EventName##_UObject( UserClass* InUserObject, typename DelegateName::template TUObjectMethodDelegate_FourVars< UserClass, Var1Type, Var2Type, Var3Type, Var4Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4 )	\
		{ \
			_##EventName = DelegateName::CreateUObject( InUserObject, InFunc, Var1, Var2, Var3, Var4 ); \
			return *this; \
		} \
		template< class UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type >	\
		WidgetArgsType& EventName##_UObject( UserClass* InUserObject, typename DelegateName::template TUObjectMethodDelegate_FourVars_Const< UserClass, Var1Type, Var2Type, Var3Type, Var4Type >::FMethodPtr InFunc, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4 )	\
		{ \
			_##EventName = DelegateName::CreateUObject( InUserObject, InFunc, Var1, Var2, Var3, Var4 ); \
			return *this; \
		} \
		\
		DelegateName _##EventName; \


/** Base class for named arguments. Provides settings necessary for all widgets. */
template<typename WidgetType>
struct TSlateBaseNamedArgs
{
	typedef typename WidgetType::FArguments WidgetArgsType;
	
	TSlateBaseNamedArgs()
	: _ToolTipText()
	, _ToolTip()
	, _Cursor( TOptional<EMouseCursor::Type>() )
	, _IsEnabled( true )
	, _Visibility( EVisibility::Visible )
	, _RenderTransform( )
	, _RenderTransformPivot( FVector2D::ZeroVector )
	, _ForceVolatile( false )
	, _Clipping( EWidgetClipping::Inherit )
	{
	}

	/** Used by the named argument pattern as a safe way to 'return *this' for call-chaining purposes. */
	WidgetArgsType& Me()
	{
		return *(static_cast<WidgetArgsType*>(this));
	}

	/** Add metadata to this widget. */
	WidgetArgsType& AddMetaData(TSharedRef<ISlateMetaData> InMetaData)
	{
		MetaData.Add(InMetaData);
		return Me();
	}

	/** Add metadata to this widget - convenience method - 1 argument */
	template<typename MetaDataType, typename Arg0Type>
	WidgetArgsType& AddMetaData(Arg0Type InArg0)
	{
		MetaData.Add(MakeShared<MetaDataType>(InArg0));
		return Me();
	}

	/** Add metadata to this widget - convenience method - 2 arguments */
	template<typename MetaDataType, typename Arg0Type, typename Arg1Type>
	WidgetArgsType& AddMetaData(Arg0Type InArg0, Arg1Type InArg1)
	{
		MetaData.Add(MakeShared<MetaDataType>(InArg0, InArg1));
		return Me();
	}

	SLATE_ATTRIBUTE( FText, ToolTipText )
	SLATE_ARGUMENT( TSharedPtr<IToolTip>, ToolTip )
	SLATE_ATTRIBUTE( TOptional<EMouseCursor::Type>, Cursor )
	SLATE_ATTRIBUTE( bool, IsEnabled )
	SLATE_ATTRIBUTE( EVisibility, Visibility )
	SLATE_ATTRIBUTE( TOptional<FSlateRenderTransform>, RenderTransform )
	SLATE_ATTRIBUTE( FVector2D, RenderTransformPivot )
	SLATE_ARGUMENT( FName, Tag )
	SLATE_ARGUMENT( bool, ForceVolatile )
	SLATE_ARGUMENT( EWidgetClipping, Clipping )

	TArray<TSharedRef<ISlateMetaData>> MetaData;
};

namespace RequiredArgs
{
	struct T0RequiredArgs
	{
		T0RequiredArgs()
		{
		}

		template<class WidgetType>
		void CallConstruct(const TSharedRef<WidgetType>& OnWidget, const typename WidgetType::FArguments& WithNamedArgs) const
		{
			// YOUR WIDGET MUST IMPLEMENT void Construct(const FArguments& InArgs)
			OnWidget->Construct(WithNamedArgs);
			OnWidget->CacheVolatility();
		}
	};

	template<typename Arg0Type>
	struct T1RequiredArgs
	{
		T1RequiredArgs(Arg0Type&& InArg0)
			: Arg0(InArg0)
		{
		}

		template<class WidgetType>
		void CallConstruct(const TSharedRef<WidgetType>& OnWidget, const typename WidgetType::FArguments& WithNamedArgs) const
		{
			// YOUR WIDGET MUST IMPLEMENT void Construct(const FArguments& InArgs)
			OnWidget->Construct(WithNamedArgs, Forward<Arg0Type>(Arg0));
			OnWidget->CacheVolatility();
		}

		Arg0Type& Arg0;
	};

	template<typename Arg0Type, typename Arg1Type>
	struct T2RequiredArgs
	{
		T2RequiredArgs(Arg0Type&& InArg0, Arg1Type&& InArg1)
			: Arg0(InArg0)
			, Arg1(InArg1)
		{
		}

		template<class WidgetType>
		void CallConstruct(const TSharedRef<WidgetType>& OnWidget, const typename WidgetType::FArguments& WithNamedArgs) const
		{
			// YOUR WIDGET MUST IMPLEMENT Construct(const FArguments& InArgs)
			OnWidget->Construct(WithNamedArgs, Forward<Arg0Type>(Arg0), Forward<Arg1Type>(Arg1));
			OnWidget->CacheVolatility();
		}

		Arg0Type& Arg0;
		Arg1Type& Arg1;
	};

	template<typename Arg0Type, typename Arg1Type, typename Arg2Type>
	struct T3RequiredArgs
	{
		T3RequiredArgs(Arg0Type&& InArg0, Arg1Type&& InArg1, Arg2Type&& InArg2)
			: Arg0(InArg0)
			, Arg1(InArg1)
			, Arg2(InArg2)
		{
		}

		template<class WidgetType>
		void CallConstruct(const TSharedRef<WidgetType>& OnWidget, const typename WidgetType::FArguments& WithNamedArgs) const
		{
			// YOUR WIDGET MUST IMPLEMENT Construct(const FArguments& InArgs)
			OnWidget->Construct(WithNamedArgs, Forward<Arg0Type>(Arg0), Forward<Arg1Type>(Arg1), Forward<Arg2Type>(Arg2));
			OnWidget->CacheVolatility();
		}

		Arg0Type& Arg0;
		Arg1Type& Arg1;
		Arg2Type& Arg2;
	};

	template<typename Arg0Type, typename Arg1Type, typename Arg2Type, typename Arg3Type>
	struct T4RequiredArgs
	{
		T4RequiredArgs(Arg0Type&& InArg0, Arg1Type&& InArg1, Arg2Type&& InArg2, Arg3Type&& InArg3)
			: Arg0(InArg0)
			, Arg1(InArg1)
			, Arg2(InArg2)
			, Arg3(InArg3)
		{
		}

		template<class WidgetType>
		void CallConstruct(const TSharedRef<WidgetType>& OnWidget, const typename WidgetType::FArguments& WithNamedArgs) const
		{
			// YOUR WIDGET MUST IMPLEMENT Construct(const FArguments& InArgs)
			OnWidget->Construct(WithNamedArgs, Forward<Arg0Type>(Arg0), Forward<Arg1Type>(Arg1), Forward<Arg2Type>(Arg2), Forward<Arg3Type>(Arg3));
			OnWidget->CacheVolatility();
		}

		Arg0Type& Arg0;
		Arg1Type& Arg1;
		Arg2Type& Arg2;
		Arg3Type& Arg3;
	};

	template<typename Arg0Type, typename Arg1Type, typename Arg2Type, typename Arg3Type, typename Arg4Type>
	struct T5RequiredArgs
	{
		T5RequiredArgs(Arg0Type&& InArg0, Arg1Type&& InArg1, Arg2Type&& InArg2, Arg3Type&& InArg3, Arg4Type&& InArg4)
			: Arg0(InArg0)
			, Arg1(InArg1)
			, Arg2(InArg2)
			, Arg3(InArg3)
			, Arg4(InArg4)
		{
		}

		template<class WidgetType>
		void CallConstruct(const TSharedRef<WidgetType>& OnWidget, const typename WidgetType::FArguments& WithNamedArgs) const
		{
			// YOUR WIDGET MUST IMPLEMENT Construct(const FArguments& InArgs)
			OnWidget->Construct(WithNamedArgs, Forward<Arg0Type>(Arg0), Forward<Arg1Type>(Arg1), Forward<Arg2Type>(Arg2), Forward<Arg3Type>(Arg3), Forward<Arg4Type>(Arg4));
			OnWidget->CacheVolatility();
		}

		Arg0Type& Arg0;
		Arg1Type& Arg1;
		Arg2Type& Arg2;
		Arg3Type& Arg3;
		Arg4Type& Arg4;
	};

	FORCEINLINE T0RequiredArgs MakeRequiredArgs()
	{
		return T0RequiredArgs();
	}

	template<typename Arg0Type>
	T1RequiredArgs<Arg0Type&&> MakeRequiredArgs(Arg0Type&& InArg0)
	{
		return T1RequiredArgs<Arg0Type&&>(Forward<Arg0Type>(InArg0));
	}

	template<typename Arg0Type, typename Arg1Type>
	T2RequiredArgs<Arg0Type&&, Arg1Type&&> MakeRequiredArgs(Arg0Type&& InArg0, Arg1Type&& InArg1)
	{
		return T2RequiredArgs<Arg0Type&&, Arg1Type&&>(Forward<Arg0Type>(InArg0), Forward<Arg1Type>(InArg1));
	}

	template<typename Arg0Type, typename Arg1Type, typename Arg2Type>
	T3RequiredArgs<Arg0Type&&, Arg1Type&&, Arg2Type&&> MakeRequiredArgs(Arg0Type&& InArg0, Arg1Type&& InArg1, Arg2Type&& InArg2)
	{
		return T3RequiredArgs<Arg0Type&&, Arg1Type&&, Arg2Type&&>(Forward<Arg0Type>(InArg0), Forward<Arg1Type>(InArg1), Forward<Arg2Type>(InArg2));
	}

	template<typename Arg0Type, typename Arg1Type, typename Arg2Type, typename Arg3Type>
	T4RequiredArgs<Arg0Type&&, Arg1Type&&, Arg2Type&&, Arg3Type&&> MakeRequiredArgs(Arg0Type&& InArg0, Arg1Type&& InArg1, Arg2Type&& InArg2, Arg3Type&& InArg3)
	{
		return T4RequiredArgs<Arg0Type&&, Arg1Type&&, Arg2Type&&, Arg3Type&&>(Forward<Arg0Type>(InArg0), Forward<Arg1Type>(InArg1), Forward<Arg2Type>(InArg2), Forward<Arg3Type>(InArg3));
	}

	template<typename Arg0Type, typename Arg1Type, typename Arg2Type, typename Arg3Type, typename Arg4Type>
	T5RequiredArgs<Arg0Type&&, Arg1Type&&, Arg2Type&&, Arg3Type&&, Arg4Type&&> MakeRequiredArgs(Arg0Type&& InArg0, Arg1Type&& InArg1, Arg2Type&& InArg2, Arg3Type&& InArg3, Arg4Type&& InArg4)
	{
		return T5RequiredArgs<Arg0Type&&, Arg1Type&&, Arg2Type&&, Arg3Type&&, Arg4Type&&>(Forward<Arg0Type>(InArg0), Forward<Arg1Type>(InArg1), Forward<Arg2Type>(InArg2), Forward<Arg3Type>(InArg3), Forward<Arg4Type>(InArg4));
	}
}

/** Normal widgets are allocated directly by the TDecl. */
template<typename WidgetType, bool IsDerived>
struct TWidgetAllocator
{
	static TSharedRef<WidgetType> PrivateAllocateWidget()
	{
		return MakeShared<WidgetType>();
	}
};

/**
 * SUserWidgets are allocated in the corresponding CPP file, so that
 * the implementer can return an implementation that differs from the
 * public interface. @see SUserWidgetExample
 */
template<typename WidgetType>
struct TWidgetAllocator< WidgetType, true >
{
	static TSharedRef<WidgetType> PrivateAllocateWidget()
	{
		return WidgetType::New();
	}
};

class SUserWidget;

/**
 * Utility class used during widget instantiation.
 * Performs widget allocation and construction.
 * Ensures that debug info is set correctly.
 * Returns TSharedRef to widget.
 *
 * @see SNew
 * @see SAssignNew
 */
template<class WidgetType, typename RequiredArgsPayloadType>
struct TDecl
{
	TDecl( const ANSICHAR* InType, const ANSICHAR* InFile, int32 OnLine, RequiredArgsPayloadType&& InRequiredArgs )
		: _Widget( TWidgetAllocator<WidgetType, TIsDerivedFrom<WidgetType, SUserWidget>::IsDerived >::PrivateAllocateWidget() )
		, _RequiredArgs(InRequiredArgs)
	{
		_Widget->SetDebugInfo( InType, InFile, OnLine );
	}

	/**
	 * Initialize OutVarToInit with the widget that is being constructed.
	 * @see SAssignNew
	 */
	template<class ExposeAsWidgetType>
	TDecl& Expose( TSharedPtr<ExposeAsWidgetType>& OutVarToInit )
	{
		OutVarToInit = _Widget;
		return *this;
	}

	/**
	 * Initialize OutVarToInit with the widget that is being constructed.
	 * @see SAssignNew
	 */
	template<class ExposeAsWidgetType>
	TDecl& Expose( TSharedRef<ExposeAsWidgetType>& OutVarToInit )
	{
		OutVarToInit = _Widget;
		return *this;
	}

	/**
	 * Initialize a WEAK OutVarToInit with the widget that is being constructed.
	 * @see SAssignNew
	 */
	template<class ExposeAsWidgetType>
	TDecl& Expose( TWeakPtr<ExposeAsWidgetType>& OutVarToInit )
	{
		OutVarToInit = _Widget;
		return *this;
	}

	/**
	 * Complete widget construction from InArgs.
	 *
	 * @param InArgs  NamedArguments from which to construct the widget.
	 *
	 * @return A reference to the widget that we constructed.
	 */
	TSharedRef<WidgetType> operator<<=( const typename WidgetType::FArguments& InArgs ) const
	{
		//@todo UMG: This should be removed in favor of all widgets calling their superclass construct.
		_Widget->SWidgetConstruct(
			InArgs._ToolTipText,
			InArgs._ToolTip ,
			InArgs._Cursor ,
			InArgs._IsEnabled ,
			InArgs._Visibility,
			InArgs._RenderTransform,
			InArgs._RenderTransformPivot,
			InArgs._Tag,
			InArgs._ForceVolatile,
			InArgs._Clipping,
			InArgs.MetaData );

		_RequiredArgs.CallConstruct(_Widget, InArgs);

		return _Widget;
	}

	const TSharedRef<WidgetType> _Widget;
	RequiredArgsPayloadType& _RequiredArgs;
};


template<typename WidgetType, typename RequiredArgsPayloadType>
TDecl<WidgetType, RequiredArgsPayloadType> MakeTDecl( const ANSICHAR* InType, const ANSICHAR* InFile, int32 OnLine, RequiredArgsPayloadType&& InRequiredArgs )
{
	return TDecl<WidgetType, RequiredArgsPayloadType>(InType, InFile, OnLine, Forward<RequiredArgsPayloadType>(InRequiredArgs));
}
