#pragma once

#include "NiagaraScript.h"
#include "InputChord.h"
#include "NiagaraSpawnShortcut.generated.h"

USTRUCT()
struct FNiagaraSpawnShortcut
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY(EditAnywhere, Category = Shortcut)
	FString Name;
	UPROPERTY(EditAnywhere, Category = Shortcut)
	FInputChord Input;
};