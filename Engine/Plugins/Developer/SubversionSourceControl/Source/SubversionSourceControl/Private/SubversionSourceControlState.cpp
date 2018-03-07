// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SubversionSourceControlState.h"

#define LOCTEXT_NAMESPACE "SubversionSourceControl.State"

int32 FSubversionSourceControlState::GetHistorySize() const
{
	return History.Num();
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FSubversionSourceControlState::GetHistoryItem( int32 HistoryIndex ) const
{
	check(History.IsValidIndex(HistoryIndex));
	return History[HistoryIndex];
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FSubversionSourceControlState::FindHistoryRevision( int32 RevisionNumber ) const
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

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FSubversionSourceControlState::FindHistoryRevision(const FString& InRevision) const
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

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FSubversionSourceControlState::GetBaseRevForMerge() const
{
	return FindHistoryRevision(PendingMergeBaseFileRevNumber);
}

FName FSubversionSourceControlState::GetIconName() const
{
	if(LockState == ELockState::Locked)
	{
		return FName("Subversion.CheckedOut");
	}
	else if(LockState == ELockState::LockedOther)
	{
		return FName("Subversion.CheckedOutByOtherUser");
	}
	else if (!IsCurrent())
	{
		return FName("Subversion.NotAtHeadRevision");
	}

	switch(WorkingCopyState)
	{
	case EWorkingCopyState::Added:
		if(bCopied)
		{
			return FName("Subversion.Branched");
		}
		else
		{
			return FName("Subversion.OpenForAdd");
		}
	case EWorkingCopyState::NotControlled:
		return FName("Subversion.NotInDepot");
	case EWorkingCopyState::Deleted:
		return FName("Subversion.MarkedForDelete");
	}

	return NAME_None;
}

FName FSubversionSourceControlState::GetSmallIconName() const
{
	if(LockState == ELockState::Locked)
	{
		return FName("Subversion.CheckedOut_Small");
	}
	else if(LockState == ELockState::LockedOther)
	{
		return FName("Subversion.CheckedOutByOtherUser_Small");
	}
	else if (!IsCurrent())
	{
		return FName("Subversion.NotAtHeadRevision_Small");
	}

	switch(WorkingCopyState)
	{
	case EWorkingCopyState::Added:
		if(bCopied)
		{
			return FName("Subversion.Branched_Small");
		}
		else
		{
			return FName("Subversion.OpenForAdd_Small");
		}
	case EWorkingCopyState::NotControlled:
		return FName("Subversion.NotInDepot_Small");
	case EWorkingCopyState::Deleted:
		return FName("Subversion.MarkedForDelete_Small");
	}

	return NAME_None;
}

FText FSubversionSourceControlState::GetDisplayName() const
{
	if(LockState == ELockState::Locked)
	{
		return LOCTEXT("Locked", "Locked For Editing");
	}
	else if(LockState == ELockState::LockedOther)
	{
		return FText::Format( LOCTEXT("LockedOther", "Locked by "), FText::FromString(LockUser) );
	}

	switch(WorkingCopyState) //-V719
	{
	case EWorkingCopyState::Unknown:
		return LOCTEXT("Unknown", "Unknown");
	case EWorkingCopyState::Pristine:
		return LOCTEXT("Pristine", "Pristine");
	case EWorkingCopyState::Added:
		if(bCopied)
		{
			return LOCTEXT("Added", "Added With History");
		}
		else
		{
			return LOCTEXT("Added", "Added");
		}
	case EWorkingCopyState::Deleted:
		return LOCTEXT("Deleted", "Deleted");
	case EWorkingCopyState::Modified:
		return LOCTEXT("Modified", "Modified");
	case EWorkingCopyState::Replaced:
		return LOCTEXT("Replaced", "Replaced");
	case EWorkingCopyState::Conflicted:
		return LOCTEXT("ContentsConflict", "Contents Conflict");
	case EWorkingCopyState::External:
		return LOCTEXT("External", "External");
	case EWorkingCopyState::Ignored:
		return LOCTEXT("Ignored", "Ignored");
	case EWorkingCopyState::Incomplete:
		return LOCTEXT("Incomplete", "Incomplete");
	case EWorkingCopyState::Merged:
		return LOCTEXT("Merged", "Merged");
	case EWorkingCopyState::NotControlled:
		return LOCTEXT("NotControlled", "Not Under Source Control");
	case EWorkingCopyState::Obstructed:
		return LOCTEXT("Obstructed", "Obstructed By Other Type");
	case EWorkingCopyState::Missing:
		return LOCTEXT("Missing", "Missing");
	}

	return FText();
}

FText FSubversionSourceControlState::GetDisplayTooltip() const
{
	if(LockState == ELockState::Locked)
	{
		return LOCTEXT("Locked_Tooltip", "Locked for editing by current user");
	}
	else if(LockState == ELockState::LockedOther)
	{
		return FText::Format( LOCTEXT("LockedOther_Tooltip", "Locked for editing by: {0}"), FText::FromString(LockUser) );
	}

	switch(WorkingCopyState) //-V719
	{
	case EWorkingCopyState::Unknown:
		return LOCTEXT("Unknown_Tooltip", "Unknown source control state");
	case EWorkingCopyState::Pristine:
		return LOCTEXT("Pristine_Tooltip", "There are no modifications");
	case EWorkingCopyState::Added:
		if(bCopied)
		{
			return LOCTEXT("Added_Tooltip", "Item is scheduled for addition with history");
		}
		else
		{
			return LOCTEXT("Added_Tooltip", "Item is scheduled for addition");
		}
	case EWorkingCopyState::Deleted:
		return LOCTEXT("Deleted_Tooltip", "Item is scheduled for deletion");
	case EWorkingCopyState::Modified:
		return LOCTEXT("Modified_Tooltip", "Item has been modified");
	case EWorkingCopyState::Replaced:
		return LOCTEXT("Replaced_Tooltip", "Item has been replaced in this working copy. This means the file was scheduled for deletion, and then a new file with the same name was scheduled for addition in its place.");
	case EWorkingCopyState::Conflicted:
		return LOCTEXT("ContentsConflict_Tooltip", "The contents (as opposed to the properties) of the item conflict with updates received from the repository.");
	case EWorkingCopyState::External:
		return LOCTEXT("External_Tooltip", "Item is present because of an externals definition.");
	case EWorkingCopyState::Ignored:
		return LOCTEXT("Ignored_Tooltip", "Item is being ignored.");
	case EWorkingCopyState::Merged:
		return LOCTEXT("Merged_Tooltip", "Item has been merged.");
	case EWorkingCopyState::NotControlled:
		return LOCTEXT("NotControlled_Tooltip", "Item is not under version control.");
	case EWorkingCopyState::Obstructed:
		return LOCTEXT("ReplacedOther_Tooltip", "Item is versioned as one kind of object (file, directory, link), but has been replaced by a different kind of object.");
	case EWorkingCopyState::Missing:
		return LOCTEXT("Missing_Tooltip", "Item is missing (e.g., you moved or deleted it without using svn). This also indicates that a directory is incomplete (a checkout or update was interrupted).");
	}

	return FText();
}

const FString& FSubversionSourceControlState::GetFilename() const
{
	return LocalFilename;
}

const FDateTime& FSubversionSourceControlState::GetTimeStamp() const
{
	return TimeStamp;
}

bool FSubversionSourceControlState::CanCheckIn() const
{
	return ( (LockState == ELockState::Locked) || (WorkingCopyState == EWorkingCopyState::Added) ) && !IsConflicted() && IsCurrent();
}

bool FSubversionSourceControlState::CanCheckout() const
{
	return (WorkingCopyState == EWorkingCopyState::Pristine || WorkingCopyState == EWorkingCopyState::Modified) && LockState == ELockState::NotLocked;
}

bool FSubversionSourceControlState::IsCheckedOut() const
{
	return LockState == ELockState::Locked;
}

bool FSubversionSourceControlState::IsCheckedOutOther(FString* Who) const
{
	if(Who != NULL)
	{
		*Who = LockUser;
	}
	return LockState == ELockState::LockedOther;
}

bool FSubversionSourceControlState::IsCurrent() const
{
	return !bNewerVersionOnServer;
}

bool FSubversionSourceControlState::IsSourceControlled() const
{
	return WorkingCopyState != EWorkingCopyState::NotControlled && WorkingCopyState != EWorkingCopyState::Unknown && WorkingCopyState != EWorkingCopyState::NotAWorkingCopy;
}

bool FSubversionSourceControlState::IsAdded() const
{
	return WorkingCopyState == EWorkingCopyState::Added;
}

bool FSubversionSourceControlState::IsDeleted() const
{
	return WorkingCopyState == EWorkingCopyState::Deleted;
}

bool FSubversionSourceControlState::IsIgnored() const
{
	return WorkingCopyState == EWorkingCopyState::Ignored;
}

bool FSubversionSourceControlState::CanEdit() const
{
	return LockState == ELockState::Locked || WorkingCopyState == EWorkingCopyState::Added;
}

bool FSubversionSourceControlState::CanDelete() const
{
	return !IsCheckedOutOther() && IsSourceControlled() && IsCurrent();
}

bool FSubversionSourceControlState::IsUnknown() const
{
	return WorkingCopyState == EWorkingCopyState::Unknown;
}

bool FSubversionSourceControlState::IsModified() const
{
	return WorkingCopyState == EWorkingCopyState::Modified || WorkingCopyState == EWorkingCopyState::Merged || WorkingCopyState == EWorkingCopyState::Obstructed || WorkingCopyState == EWorkingCopyState::Conflicted;
}

bool FSubversionSourceControlState::CanAdd() const
{
	return WorkingCopyState == EWorkingCopyState::NotControlled;
}

bool FSubversionSourceControlState::IsConflicted() const
{
	return PendingMergeBaseFileRevNumber != INVALID_REVISION;
}

#undef LOCTEXT_NAMESPACE
