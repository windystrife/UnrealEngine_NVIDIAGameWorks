// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "AnimationCompressionPanel.generated.h"

class SAnimationCompressionPanel;
class SWindow;
class UAnimSequence;

UCLASS()
class UCompressionHolder : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Instanced, Category = Compression, EditAnywhere, NoClear)
	class UAnimCompress* Compression;
};

/** 
* FDlgAnimCompression
* 
* Wrapper class for SAnimationCompressionPanel. This class creates and launches a dialog then awaits the
* result to return to the user. 
*/
class UNREALED_API FDlgAnimCompression
{
public:
	FDlgAnimCompression( TArray<TWeakObjectPtr<UAnimSequence>>& AnimSequences );

	/**  Shows the dialog box and waits for the user to respond. */
	void ShowModal();

private:

	/** Cached pointer to the modal window */
	TSharedPtr<SWindow> DialogWindow;

	/** Cached pointer to the animation compression widget */
	TSharedPtr<class SAnimationCompressionPanel> DialogWidget;
};

/** 
 * Slate panel for Choosing animation compression
 */
class SAnimationCompressionPanel : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SAnimationCompressionPanel)
		{
		}
		/** The anim sequences to compress */
		SLATE_ARGUMENT( TArray<TWeakObjectPtr<UAnimSequence>>, AnimSequences )

		/** Window in which this widget resides */
		SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)

	SLATE_END_ARGS()	

	/**
	 * Constructs this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	virtual ~SAnimationCompressionPanel();

	/**
	 * Handler for the panels "Apply" button
	 */
	FReply					ApplyClicked();

private:
	/**
	 * Apply the selected compression Algorithm to animation sequences supplied to the panel.
	 *
	 * @param	Algorithm	The type of compression to apply to the animation sequences
	 */
	void					ApplyAlgorithm(class UAnimCompress* Algorithm);

	TArray< TWeakObjectPtr<UAnimSequence> >	AnimSequences;

	/** Pointer to the window which holds this Widget, required for modal control */
	TWeakPtr<SWindow> ParentWindow;

	/** Our compression display object */
	UCompressionHolder* CompressionHolder;
};
