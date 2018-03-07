// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "KismetPins/SGraphPinVector2D.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "VectorTextBox"

//Class implementation to create 2 editable text boxes to represent vector2D graph pin
class SVector2DTextBox : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SVector2DTextBox ){}	
		SLATE_ATTRIBUTE( FString, VisibleText_X )
		SLATE_ATTRIBUTE( FString, VisibleText_Y )
		SLATE_EVENT( FOnFloatValueCommitted, OnFloatCommitted_Box_X )
		SLATE_EVENT( FOnFloatValueCommitted, OnFloatCommitted_Box_Y )
	SLATE_END_ARGS()

	//Construct editable text boxes with the appropriate getter & setter functions along with tool tip text
	void Construct( const FArguments& InArgs )
	{
		VisibleText_X = InArgs._VisibleText_X;
		VisibleText_Y = InArgs._VisibleText_Y;
		const FLinearColor LabelClr = FLinearColor( 1.f, 1.f, 1.f, 0.4f );

		this->ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth().Padding(2) .HAlign(HAlign_Fill)
				[
					//Create Text box 0 
					SNew( SNumericEntryBox<float> )
					.LabelVAlign(VAlign_Center)
					.Label()
					[
						SNew( STextBlock )
						.Font( FEditorStyle::GetFontStyle( "Graph.VectorEditableTextBox" ) )
						.Text( LOCTEXT("VectorNodeXAxisValueLabel", "X") )
						.ColorAndOpacity( LabelClr )
					]
					.Value( this, &SVector2DTextBox::GetTypeInValue_X )
					.OnValueCommitted( InArgs._OnFloatCommitted_Box_X )
					.Font( FEditorStyle::GetFontStyle( "Graph.VectorEditableTextBox" ) )
					.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
					.ToolTipText(LOCTEXT("VectorNodeXAxisValueLabel_ToolTip", "X value") )
					.EditableTextBoxStyle( &FEditorStyle::GetWidgetStyle<FEditableTextBoxStyle>( "Graph.VectorEditableTextBox" ))
					.BorderForegroundColor( FLinearColor::White )
					.BorderBackgroundColor( FLinearColor::White )
				]
				+SHorizontalBox::Slot()
				.AutoWidth().Padding(2) .HAlign(HAlign_Fill)
				[
					//Create Text box 1
					SNew( SNumericEntryBox<float> )
					.LabelVAlign(VAlign_Center)
					.Label()
					[
						SNew( STextBlock )
						.Font( FEditorStyle::GetFontStyle( "Graph.VectorEditableTextBox" ) )
						.Text( LOCTEXT("VectorNodeYAxisValueLabel", "Y") )
						.ColorAndOpacity( LabelClr )
					]
					.Value( this, &SVector2DTextBox::GetTypeInValue_Y )
					.OnValueCommitted( InArgs._OnFloatCommitted_Box_Y )
					.Font( FEditorStyle::GetFontStyle( "Graph.VectorEditableTextBox" ) )
					.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
					.ToolTipText(LOCTEXT("VectorNodeYAxisValueLabel_ToolTip", "Y value"))
					.EditableTextBoxStyle( &FEditorStyle::GetWidgetStyle<FEditableTextBoxStyle>( "Graph.VectorEditableTextBox" ))
					.BorderForegroundColor( FLinearColor::White )
					.BorderBackgroundColor( FLinearColor::White )
				]
			]
		];
	}

private:

	//Get value for X text box
	TOptional<float> GetTypeInValue_X() const
	{
		return FCString::Atof(*(VisibleText_X.Get()));
	}

	//Get value for Y text box
	TOptional<float> GetTypeInValue_Y() const
	{
		return FCString::Atof(*(VisibleText_Y.Get()));
	}

	TAttribute< FString >	VisibleText_X;
	TAttribute< FString >	VisibleText_Y;
};


/************************************************************************/
/*                    SGraphPinVector class implementation              */
/************************************************************************/
 
void SGraphPinVector2D::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SGraphPinVector2D::GetDefaultValueWidget()
{
	//Create widget
	return	SNew( SVector2DTextBox )
			.VisibleText_X( this, &SGraphPinVector2D::GetCurrentValue_X )
			.VisibleText_Y( this, &SGraphPinVector2D::GetCurrentValue_Y )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			.OnFloatCommitted_Box_X( this, &SGraphPinVector2D::OnChangedValueTextBox_X )
			.OnFloatCommitted_Box_Y( this, &SGraphPinVector2D::OnChangedValueTextBox_Y );
}

FString SGraphPinVector2D::GetCurrentValue_X() const
{
	return GetValue( TextBox_X );
}

FString SGraphPinVector2D::GetCurrentValue_Y() const
{
	return GetValue( TextBox_Y );
}

FString SGraphPinVector2D::GetValue( ETextBoxIndex Index ) const
{
	FString DefaultString = GraphPinObj->GetDefaultAsString();
	TArray<FString> ResultString;

	FVector2D Value;
	Value.InitFromString(DefaultString);

	if (Index == TextBox_X)
	{
		return FString::Printf(TEXT("%f"), Value.X);
	}
	else
	{
		return FString::Printf(TEXT("%f"), Value.Y);
	}
}

FString MakeVector2DString(const FString& X, const FString& Y)
{
	return FString(TEXT("(X=")) + X + FString(TEXT(",Y=")) + Y + FString(TEXT(")"));
}

void SGraphPinVector2D::OnChangedValueTextBox_X(float NewValue, ETextCommit::Type CommitInfo)
{
	const FString ValueStr = FString::Printf( TEXT("%f"), NewValue );
	const FString Vector2DString = MakeVector2DString(ValueStr, GetValue(TextBox_Y));

	if(GraphPinObj->GetDefaultAsString() != Vector2DString)
	{
		const FScopedTransaction Transaction( NSLOCTEXT("GraphEditor", "ChangeVectorPinValue", "Change Vector Pin Value" ) );
		GraphPinObj->Modify();

		//Set new default value
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, Vector2DString);
	}
}

void SGraphPinVector2D::OnChangedValueTextBox_Y(float NewValue, ETextCommit::Type CommitInfo)
{
	const FString ValueStr = FString::Printf( TEXT("%f"), NewValue );
	const FString Vector2DString =MakeVector2DString(GetValue(TextBox_X), ValueStr);

	if(GraphPinObj->GetDefaultAsString() != Vector2DString)
	{
		const FScopedTransaction Transaction( NSLOCTEXT("GraphEditor", "ChangeVectorPinValue", "Change Vector Pin Value" ) );
		GraphPinObj->Modify();

		//Set new default value
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, Vector2DString);
	}
}

#undef LOCTEXT_NAMESPACE
