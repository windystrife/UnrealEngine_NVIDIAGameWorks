// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkMessageBusSourceEditor.h"
#include "LiveLinkMessages.h"
#include "SBox.h"

#include "MessageEndpointBuilder.h"

#define LOCTEXT_NAMESPACE "LiveLinkMessageBusSourceEditor"

namespace ProviderPollUI
{
	const FName TypeColumnName(TEXT("Type"));
	const FName MachineColumnName(TEXT("Machine"));
};

class SProviderPollRow : public SMultiColumnTableRow<FProviderPollResultPtr>
{
public:
	SLATE_BEGIN_ARGS(SProviderPollRow) {}

		/** The list item for this row */
		SLATE_ARGUMENT(FProviderPollResultPtr, PollResultPtr)

	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		PollResultPtr = Args._PollResultPtr;

		SMultiColumnTableRow<FProviderPollResultPtr>::Construct(
			FSuperRowType::FArguments()
			.Padding(1.0f),
			OwnerTableView
		);
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == ProviderPollUI::TypeColumnName)
		{
			return	SNew(STextBlock)
					.Text(FText::FromString(PollResultPtr->Name));
		}
		else if (ColumnName == ProviderPollUI::MachineColumnName)
		{
			return	SNew(STextBlock)
				.Text(FText::FromString(PollResultPtr->MachineName));
		}

		return SNullWidget::NullWidget;
	}

private:

	FProviderPollResultPtr PollResultPtr;
};


SLiveLinkMessageBusSourceEditor::~SLiveLinkMessageBusSourceEditor()
{
	FMessageEndpoint::SafeRelease(MessageEndpoint);
}

void SLiveLinkMessageBusSourceEditor::Construct(const FArguments& Args)
{
	MessageEndpoint =	FMessageEndpoint::Builder(TEXT("LiveLinkMessageBusSource"))
						.Handling<FLiveLinkPongMessage>(this, &SLiveLinkMessageBusSourceEditor::HandlePongMessage);

	CurrentPollRequest = FGuid::NewGuid();

	//Simple each for connections till UI comes along
	MessageEndpoint->Publish(new FLiveLinkPingMessage(CurrentPollRequest));

	ChildSlot
	[
		SNew(SBox)
		.HeightOverride(200)
		.WidthOverride(200)
		[
			SAssignNew(ListView, SListView<FProviderPollResultPtr>)
			.ListItemsSource(&PollData)
			.SelectionMode(ESelectionMode::SingleToggle)
			.OnGenerateRow(this, &SLiveLinkMessageBusSourceEditor::MakeSourceListViewWidget)
			.OnSelectionChanged(this, &SLiveLinkMessageBusSourceEditor::OnSourceListSelectionChanged)
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(ProviderPollUI::TypeColumnName)
				.FillWidth(43.0f)
				.DefaultLabel(LOCTEXT("TypeColumnHeaderName", "Source Type"))
				+ SHeaderRow::Column(ProviderPollUI::MachineColumnName)
				.FillWidth(43.0f)
				.DefaultLabel(LOCTEXT("MachineColumnHeaderName", "Source Machine"))
			)
		]
	];
}

TSharedRef<ITableRow> SLiveLinkMessageBusSourceEditor::MakeSourceListViewWidget(FProviderPollResultPtr PollResult, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(SProviderPollRow, OwnerTable).PollResultPtr(PollResult);
}

void SLiveLinkMessageBusSourceEditor::OnSourceListSelectionChanged(FProviderPollResultPtr PollResult, ESelectInfo::Type SelectionType)
{
	SelectedResult = PollResult;
}

void SLiveLinkMessageBusSourceEditor::HandlePongMessage(const FLiveLinkPongMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if(Message.PollRequest == CurrentPollRequest)
	{
		PollData.Add(MakeShareable(new FProviderPollResult(Context->GetSender(), Message.ProviderName, Message.MachineName)));

		ListView->RequestListRefresh();
	}
}

#undef LOCTEXT_NAMESPACE