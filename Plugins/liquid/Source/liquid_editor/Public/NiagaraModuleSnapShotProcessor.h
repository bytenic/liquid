 #pragma once

#include "CoreMinimal.h"

class UNiagaraScript;

class FNiagaraModuleSnapshotProcessor
{
public:
	static bool IsModuleScript(const UNiagaraScript* Script);
	static FString NormalizeGamePath(const FString& Path);
	static bool IsValidGamePath(const FString& Path);
	static bool CreateSnapshot(UNiagaraScript* ModuleScript, const FString& SearchPath, FString& OutErrorMessage, FString& OutOutputPath);
};
