// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/Commands/UIAction.h"
#include "Framework/Application/SlateApplication.h"


FUIAction::FUIAction()
	: ExecuteAction()
	, CanExecuteAction()
	, GetActionCheckState()
	, IsActionVisibleDelegate()
	, RepeatMode(EUIActionRepeatMode::RepeatDisabled)
{ 
}

FUIAction::FUIAction(FExecuteAction InitExecuteAction, EUIActionRepeatMode InitRepeatMode)
	: ExecuteAction(InitExecuteAction)
	, CanExecuteAction(FCanExecuteAction::CreateRaw(&FSlateApplication::Get(), &FSlateApplication::IsNormalExecution))
	, GetActionCheckState()
	, IsActionVisibleDelegate()
	, RepeatMode(InitRepeatMode)
{ 
}

FUIAction::FUIAction(FExecuteAction InitExecuteAction, FCanExecuteAction InitCanExecuteAction, EUIActionRepeatMode InitRepeatMode)
	: ExecuteAction(InitExecuteAction)
	, CanExecuteAction(InitCanExecuteAction)
	, GetActionCheckState()
	, IsActionVisibleDelegate()
	, RepeatMode(InitRepeatMode)
{ 
}

FUIAction::FUIAction(FExecuteAction InitExecuteAction, FCanExecuteAction InitCanExecuteAction, FIsActionChecked InitIsCheckedDelegate, EUIActionRepeatMode InitRepeatMode)
	: ExecuteAction(InitExecuteAction)
	, CanExecuteAction(InitCanExecuteAction)
	, GetActionCheckState(FGetActionCheckState::CreateStatic(&FUIAction::IsActionCheckedPassthrough, InitIsCheckedDelegate))
	, IsActionVisibleDelegate()
	, RepeatMode(InitRepeatMode)
{ 
}

FUIAction::FUIAction(FExecuteAction InitExecuteAction, FCanExecuteAction InitCanExecuteAction, FGetActionCheckState InitGetActionCheckStateDelegate, EUIActionRepeatMode InitRepeatMode)
	: ExecuteAction(InitExecuteAction)
	, CanExecuteAction(InitCanExecuteAction)
	, GetActionCheckState(InitGetActionCheckStateDelegate)
	, IsActionVisibleDelegate()
	, RepeatMode(InitRepeatMode)
{ 
}

FUIAction::FUIAction(FExecuteAction InitExecuteAction, FCanExecuteAction InitCanExecuteAction, FIsActionChecked InitIsCheckedDelegate, FIsActionButtonVisible InitIsActionVisibleDelegate, EUIActionRepeatMode InitRepeatMode)
	: ExecuteAction(InitExecuteAction)
	, CanExecuteAction(InitCanExecuteAction)
	, GetActionCheckState(FGetActionCheckState::CreateStatic(&FUIAction::IsActionCheckedPassthrough, InitIsCheckedDelegate))
	, IsActionVisibleDelegate(InitIsActionVisibleDelegate)
	, RepeatMode(InitRepeatMode)
{ 
}

FUIAction::FUIAction(FExecuteAction InitExecuteAction, FCanExecuteAction InitCanExecuteAction, FGetActionCheckState InitGetActionCheckStateDelegate, FIsActionButtonVisible InitIsActionVisibleDelegate, EUIActionRepeatMode InitRepeatMode)
	: ExecuteAction(InitExecuteAction)
	, CanExecuteAction(InitCanExecuteAction)
	, GetActionCheckState(InitGetActionCheckStateDelegate)
	, IsActionVisibleDelegate(InitIsActionVisibleDelegate)
	, RepeatMode(InitRepeatMode)
{ 
}
