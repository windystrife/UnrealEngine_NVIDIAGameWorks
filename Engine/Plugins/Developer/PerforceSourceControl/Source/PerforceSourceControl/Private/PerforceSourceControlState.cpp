// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PerforceSourceControlState.h"
#include "PerforceSourceControlRevision.h"

#define LOCTEXT_NAMESPACE "PerforceSourceControl.State"

int32 FPerforceSourceControlState::GetHistorySize() const
{
	return History.Num();
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FPerforceSourceControlState::GetHistoryItem( int32 HistoryIndex ) const
{
	check(History.IsValidIndex(HistoryIndex));
	return History[HistoryIndex];
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FPerforceSourceControlState::FindHistoryRevision( int32 RevisionNumber ) const
{
	for(auto Iter(History.CreateConstIterator()); Iter; Iter++)
	{
		if((*Iter)->GetRevisionNumber() == RevisionNumber)
		{
			return *Iter;
		}
	}

	return NULL;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FPerforceSourceControlState::FindHistoryRevision(const FString& InRevision) const
{
	for(const auto& Revision : History)
	{
		if(Revision->GetRevision() == InRevision)
		{
			return Revision;
		}
	}

	return NULL;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FPerforceSourceControlState::GetBaseRevForMerge() const
{
	if( PendingResolveRevNumber == INVALID_REVISION )
	{
		return NULL;
	}

	return FindHistoryRevision(PendingResolveRevNumber);
}

FName FPerforceSourceControlState::GetIconName() const
{
	if( !IsCurrent() )
	{
		return FName("Perforce.NotAtHeadRevision");
	}

	switch(State)
	{
	default:
	case EPerforceState::DontCare:
		return NAME_None;
	case EPerforceState::CheckedOut:
		return FName("Perforce.CheckedOut");
	case EPerforceState::ReadOnly:
		return NAME_None;
	case EPerforceState::NotInDepot:
		return FName("Perforce.NotInDepot");
	case EPerforceState::CheckedOutOther:
		return FName("Perforce.CheckedOutByOtherUser");
	case EPerforceState::Ignore:
		return NAME_None;
	case EPerforceState::OpenForAdd:
		return FName("Perforce.OpenForAdd");
	case EPerforceState::MarkedForDelete:
		return FName("Perforce.MarkedForDelete");
	case EPerforceState::Branched:
		return FName("Perforce.Branched");
	}
}

FName FPerforceSourceControlState::GetSmallIconName() const
{
	if( !IsCurrent() )
	{
		return FName("Perforce.NotAtHeadRevision_Small");
	}

	switch(State)
	{
	default:
	case EPerforceState::DontCare:
		return NAME_None;
	case EPerforceState::CheckedOut:
		return FName("Perforce.CheckedOut_Small");
	case EPerforceState::ReadOnly:
		return NAME_None;
	case EPerforceState::NotInDepot:
		return FName("Perforce.NotInDepot_Small");
	case EPerforceState::CheckedOutOther:
		return FName("Perforce.CheckedOutByOtherUser_Small");
	case EPerforceState::Ignore:
		return NAME_None;
	case EPerforceState::OpenForAdd:
		return FName("Perforce.OpenForAdd_Small");
	case EPerforceState::MarkedForDelete:
		return FName("Perforce.MarkedForDelete_Small");
	case EPerforceState::Branched:
		return FName("Perforce.Branched_Small");
	}
}

FText FPerforceSourceControlState::GetDisplayName() const
{
	if( IsConflicted() )
	{
		return LOCTEXT("Conflicted", "Conflicted");
	}
	else if( !IsCurrent() )
	{
		return LOCTEXT("NotCurrent", "Not current");
	}

	switch(State)
	{
	default:
	case EPerforceState::DontCare:
		return LOCTEXT("Unknown", "Unknown");
	case EPerforceState::CheckedOut:
		return LOCTEXT("CheckedOut", "Checked out");
	case EPerforceState::ReadOnly:
		return LOCTEXT("ReadOnly", "Read only");
	case EPerforceState::NotInDepot:
		return LOCTEXT("NotInDepot", "Not in depot");
	case EPerforceState::CheckedOutOther:
		return FText::Format( LOCTEXT("CheckedOutOther", "Checked out by: {0}"), FText::FromString(OtherUserCheckedOut) );
	case EPerforceState::Ignore:
		return LOCTEXT("Ignore", "Ignore");
	case EPerforceState::OpenForAdd:
		return LOCTEXT("OpenedForAdd", "Opened for add");
	case EPerforceState::MarkedForDelete:
		return LOCTEXT("MarkedForDelete", "Marked for delete");
	case EPerforceState::Branched:
		return LOCTEXT("Branched", "Branched");
	}
}

FText FPerforceSourceControlState::GetDisplayTooltip() const
{
	if (IsConflicted())
	{
		return LOCTEXT("Conflicted_Tooltip", "The files(s) have local changes that need to be resolved with changes submitted to the Perforce depot");
	}
	else if( !IsCurrent() )
	{
		return LOCTEXT("NotCurrent_Tooltip", "The file(s) are not at the head revision");
	}

	switch(State)
	{
	default:
	case EPerforceState::DontCare:
		return LOCTEXT("Unknown_Tooltip", "The file(s) status is unknown");
	case EPerforceState::CheckedOut:
		return LOCTEXT("CheckedOut_Tooltip", "The file(s) are checked out");
	case EPerforceState::ReadOnly:
		return LOCTEXT("ReadOnly_Tooltip", "The file(s) are marked locally as read-only");
	case EPerforceState::NotInDepot:
		return LOCTEXT("NotInDepot_Tooltip", "The file(s) are not present in the Perforce depot");
	case EPerforceState::CheckedOutOther:
		return FText::Format( LOCTEXT("CheckedOutOther_Tooltip", "Checked out by: {0}"), FText::FromString(OtherUserCheckedOut) );
	case EPerforceState::Ignore:
		return LOCTEXT("Ignore_Tooltip", "The file(s) are ignored by Perforce");
	case EPerforceState::OpenForAdd:
		return LOCTEXT("OpenedForAdd_Tooltip", "The file(s) are opened for add");
	case EPerforceState::MarkedForDelete:
		return LOCTEXT("MarkedForDelete_Tooltip", "The file(s) are marked for delete");
	case EPerforceState::Branched:
		return LOCTEXT("Branched_Tooltip", "The file(s) are opened for branching");
	}
}

const FString& FPerforceSourceControlState::GetFilename() const
{
	return LocalFilename;
}

const FDateTime& FPerforceSourceControlState::GetTimeStamp() const
{
	return TimeStamp;
}

bool FPerforceSourceControlState::CanCheckIn() const
{
	return ( (State == EPerforceState::CheckedOut) || (State == EPerforceState::OpenForAdd) || (State == EPerforceState::Branched) ) && !IsConflicted() && IsCurrent();
}

bool FPerforceSourceControlState::CanCheckout() const
{
	bool bCanDoCheckout = false;

	const bool bIsInP4NotCheckedOut = State == EPerforceState::ReadOnly;
	if (!bBinary && !bExclusiveCheckout)
	{
		// Notice that we don't care whether we're up to date. User can perform textual
		// merge via P4V:
		const bool bIsCheckedOutElseWhere = State == EPerforceState::CheckedOutOther;
		bCanDoCheckout =	bIsInP4NotCheckedOut ||
							bIsCheckedOutElseWhere;
	}
	else
	{
		// For assets that are either binary or textual but marked for exclusive checkout
		// we only want to permit check out when we are at head:
		bCanDoCheckout = bIsInP4NotCheckedOut && IsCurrent();
	}

	return bCanDoCheckout;
}

bool FPerforceSourceControlState::IsCheckedOut() const
{
	return State == EPerforceState::CheckedOut;
}

bool FPerforceSourceControlState::IsCheckedOutOther(FString* Who) const
{
	if(Who != NULL)
	{
		*Who = OtherUserCheckedOut;
	}
	return State == EPerforceState::CheckedOutOther;
}

bool FPerforceSourceControlState::IsCurrent() const
{
	return LocalRevNumber == DepotRevNumber;
}

bool FPerforceSourceControlState::IsSourceControlled() const
{
	return State != EPerforceState::NotInDepot && State != EPerforceState::NotUnderClientRoot;
}

bool FPerforceSourceControlState::IsAdded() const
{
	return State == EPerforceState::OpenForAdd;
}

bool FPerforceSourceControlState::IsDeleted() const
{
	return State == EPerforceState::MarkedForDelete;
}

bool FPerforceSourceControlState::IsIgnored() const
{
	return State == EPerforceState::Ignore;
}

bool FPerforceSourceControlState::CanEdit() const
{
	return State == EPerforceState::CheckedOut || State == EPerforceState::OpenForAdd || State == EPerforceState::Branched;
}

bool FPerforceSourceControlState::CanDelete() const
{
	return !IsCheckedOutOther() && IsSourceControlled() && IsCurrent();
}

bool FPerforceSourceControlState::IsUnknown() const
{
	return State == EPerforceState::DontCare;
}

bool FPerforceSourceControlState::IsModified() const
{
	return bModifed;
}

bool FPerforceSourceControlState::CanAdd() const
{
	return State == EPerforceState::NotInDepot;
}

bool FPerforceSourceControlState::IsConflicted() const
{
	return PendingResolveRevNumber != INVALID_REVISION;
}

#undef LOCTEXT_NAMESPACE
