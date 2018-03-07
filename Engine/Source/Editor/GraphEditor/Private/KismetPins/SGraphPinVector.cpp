// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "KismetPins/SGraphPinVector.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "VectorTextBox"


//Class implementation to create 3 editable text boxes to represent vector/rotator graph pin
class SVectorTextBox : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SVectorTextBox ) {}	
		SLATE_ATTRIBUTE( FString, VisibleText_0 )
		SLATE_ATTRIBUTE( FString, VisibleText_1 )
		SLATE_ATTRIBUTE( FString, VisibleText_2 )
		SLATE_EVENT( FOnFloatValueCommitted, OnFloatCommitted_Box_0 )
		SLATE_EVENT( FOnFloatValueCommitted, OnFloatCommitted_Box_1 )
		SLATE_EVENT( FOnFloatValueCommitted, OnFloatCommitted_Box_2 )
	SLATE_END_ARGS()

	//Construct editable text boxes with the appropriate getter & setter functions along with tool tip text
	void Construct( const FArguments& InArgs, const bool bInIsRotator )
	{
		bIsRotator = bInIsRotator;
		const bool bUseRPY = false;

		VisibleText_0 = InArgs._VisibleText_0;
		VisibleText_1 = InArgs._VisibleText_1;
		VisibleText_2 = InArgs._VisibleText_2;
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
						.Text( bIsRotator && bUseRPY ? LOCTEXT("VectorNodeRollValueLabel", "R") : LOCTEXT("VectorNodeXAxisValueLabel", "X") )
						.ColorAndOpacity( LabelClr )
					]
					.Value( this, &SVectorTextBox::GetTypeInValue_0 )
					.OnValueCommitted( InArgs._OnFloatCommitted_Box_0 )
					.Font( FEditorStyle::GetFontStyle( "Graph.VectorEditableTextBox" ) )
					.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
					.ToolTipText( bIsRotator ? LOCTEXT("VectorNodeRollValueLabel_ToolTip", "Roll value (around X)") : LOCTEXT("VectorNodeXAxisValueLabel_ToolTip", "X value"))
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
						.Text( bIsRotator && bUseRPY ? LOCTEXT("VectorNodePitchValueLabel", "P") : LOCTEXT("VectorNodeYAxisValueLabel", "Y") )
						.ColorAndOpacity( LabelClr )
					]
					.Value( this, &SVectorTextBox::GetTypeInValue_1 )
					.OnValueCommitted( InArgs._OnFloatCommitted_Box_1 )
					.Font( FEditorStyle::GetFontStyle( "Graph.VectorEditableTextBox" ) )
					.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
					.ToolTipText(bIsRotator ? LOCTEXT("VectorNodePitchValueLabel_ToolTip", "Pitch value (around Y)") : LOCTEXT("VectorNodeYAxisValueLabel_ToolTip", "Y value"))
					.EditableTextBoxStyle( &FEditorStyle::GetWidgetStyle<FEditableTextBoxStyle>( "Graph.VectorEditableTextBox" ))
					.BorderForegroundColor( FLinearColor::White )
					.BorderBackgroundColor( FLinearColor::White )
				]
				+SHorizontalBox::Slot()
				.AutoWidth().Padding(2) .HAlign(HAlign_Fill)
				[
					//Create Text box 2
					SNew( SNumericEntryBox<float> )
					.LabelVAlign(VAlign_Center)
					.Label()
					[
						SNew( STextBlock )
						.Font( FEditorStyle::GetFontStyle( "Graph.VectorEditableTextBox" ) )
						.Text( bIsRotator && bUseRPY ? LOCTEXT("VectorNodeYawValueLabel", "Y") : LOCTEXT("VectorNodeZAxisValueLabel", "Z") )
						.ColorAndOpacity( LabelClr )
					]
					.Value( this, &SVectorTextBox::GetTypeInValue_2 )
					.OnValueCommitted( InArgs._OnFloatCommitted_Box_2 )
					.Font( FEditorStyle::GetFontStyle( "Graph.VectorEditableTextBox" ) )
					.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
					.ToolTipText(bIsRotator ? LOCTEXT("VectorNodeYawValueLabel_Tooltip", "Yaw value (around Z)") : LOCTEXT("VectorNodeZAxisValueLabel_ToolTip", "Z value"))
					.EditableTextBoxStyle( &FEditorStyle::GetWidgetStyle<FEditableTextBoxStyle>( "Graph.VectorEditableTextBox" ))
					.BorderForegroundColor( FLinearColor::White )
					.BorderBackgroundColor( FLinearColor::White )
				]
			]
		];
	}

private:

	//Get value for text box 0
	TOptional<float> GetTypeInValue_0() const
	{
		return FCString::Atof(*(VisibleText_0.Get()));
	}

	//Get value for text box 1
	TOptional<float> GetTypeInValue_1() const
	{
		return FCString::Atof(*(VisibleText_1.Get()));
	}

	//Get value for text box 2
	TOptional<float> GetTypeInValue_2() const
	{
		return FCString::Atof(*(VisibleText_2.Get()));
	}

	TAttribute< FString >	VisibleText_0;
	TAttribute< FString >	VisibleText_1;
	TAttribute< FString >	VisibleText_2;

	bool bIsRotator;
};




/************************************************************************/
/*                    SGraphPinVector class implementation              */
/************************************************************************/
 
void SGraphPinVector::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SGraphPinVector::GetDefaultValueWidget()
{
	UScriptStruct* RotatorStruct = TBaseStructure<FRotator>::Get();
	bIsRotator = (GraphPinObj->PinType.PinSubCategoryObject == RotatorStruct) ? true : false;
	
	//Create widget
	return	SNew( SVectorTextBox, bIsRotator )
			.VisibleText_0( this, &SGraphPinVector::GetCurrentValue_0 )
			.VisibleText_1( this, &SGraphPinVector::GetCurrentValue_1 )
			.VisibleText_2( this, &SGraphPinVector::GetCurrentValue_2 )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			.OnFloatCommitted_Box_0( this, &SGraphPinVector::OnChangedValueTextBox_0 )
			.OnFloatCommitted_Box_1( this, &SGraphPinVector::OnChangedValueTextBox_1 )
			.OnFloatCommitted_Box_2( this, &SGraphPinVector::OnChangedValueTextBox_2 );
}

//Rotator is represented as X->Roll, Y->Pitch, Z->Yaw

FString SGraphPinVector::GetCurrentValue_0() const
{
	//Text box 0: Rotator->Roll, Vector->X
	return GetValue( bIsRotator ? TextBox_2 : TextBox_0 );
}

FString SGraphPinVector::GetCurrentValue_1() const
{
	//Text box 1: Rotator->Pitch, Vector->Y
	return GetValue( bIsRotator ? TextBox_0 : TextBox_1 );
}

FString SGraphPinVector::GetCurrentValue_2() const
{
	//Text box 2: Rotator->Yaw, Vector->Z
	return GetValue( bIsRotator ? TextBox_1 : TextBox_2 );
}

FString SGraphPinVector::GetValue( ETextBoxIndex Index ) const
{
	FString DefaultString = GraphPinObj->GetDefaultAsString();
	TArray<FString> ResultString;

	//Parse string to split its contents separated by ','
	DefaultString.TrimStartInline();
	DefaultString.TrimEndInline();
	DefaultString.ParseIntoArray(ResultString, TEXT(","), true);

	if(Index < ResultString.Num())
	{
		return ResultString[ Index ];
	}
	else
	{
		return FString(TEXT("0"));
	}
}


void SGraphPinVector::OnChangedValueTextBox_0(float NewValue, ETextCommit::Type CommitInfo)
{
	const FString ValueStr = FString::Printf( TEXT("%f"), NewValue );

	FString DefaultValue;
	if(bIsRotator)
	{
		//Update Roll value
		DefaultValue = GetValue(TextBox_0) + FString(TEXT(",")) + GetValue(TextBox_1) + FString(TEXT(",")) + ValueStr;
	}
	else
	{
		//Update X value
		DefaultValue = ValueStr + FString(TEXT(",")) + GetValue(TextBox_1) + FString(TEXT(",")) + GetValue(TextBox_2);
	}

	if(GraphPinObj->GetDefaultAsString() != DefaultValue)
	{
		const FScopedTransaction Transaction( NSLOCTEXT("GraphEditor", "ChangeVectorPinValue", "Change Vector Pin Value" ) );
		GraphPinObj->Modify();

		//Set new default value
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, DefaultValue);
	}
}

void SGraphPinVector::OnChangedValueTextBox_1(float NewValue, ETextCommit::Type CommitInfo)
{
	const FString ValueStr = FString::Printf( TEXT("%f"), NewValue );

	FString DefaultValue;
	if(bIsRotator)
	{
		//Update Pitch value
		DefaultValue =  ValueStr + FString(TEXT(",")) + GetValue(TextBox_1) + FString(TEXT(",")) + GetValue(TextBox_2);
	}
	else
	{
		//Update Y value
		DefaultValue = GetValue(TextBox_0) + FString(TEXT(",")) + ValueStr + FString(TEXT(",")) + GetValue(TextBox_2);
	}

	if(GraphPinObj->GetDefaultAsString() != DefaultValue)
	{
		const FScopedTransaction Transaction( NSLOCTEXT("GraphEditor", "ChangeVectorPinValue", "Change Vector Pin Value" ) );
		GraphPinObj->Modify();

		//Set new default value
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, DefaultValue);
	}
}

void SGraphPinVector::OnChangedValueTextBox_2(float NewValue, ETextCommit::Type CommitInfo)
{
	const FString ValueStr = FString::Printf( TEXT("%f"), NewValue );

	FString DefaultValue;
	if(bIsRotator)
	{
		//Update Yaw value
		DefaultValue = GetValue(TextBox_0) + FString(TEXT(",")) + ValueStr + FString(TEXT(",")) + GetValue(TextBox_2);
	}
	else
	{
		//Update Z value
		DefaultValue = GetValue(TextBox_0) + FString(TEXT(",")) + GetValue(TextBox_1) + FString(TEXT(",")) + ValueStr;
	}

	if(GraphPinObj->GetDefaultAsString() != DefaultValue)
	{
		const FScopedTransaction Transaction( NSLOCTEXT("GraphEditor", "ChangeVectorPinValue", "Change Vector Pin Value" ) );
		GraphPinObj->Modify();

		//Set new default value
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, DefaultValue);
	}
}

#undef LOCTEXT_NAMESPACE
