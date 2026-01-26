#pragma once

#include "CoreMinimal.h"
#include "AssetActionUtility.h"
#include "NiagaraModuleSnapShotAction.generated.h"

UCLASS()
class UNiagaraModuleSnapshotAction : public UAssetActionUtility
{
	GENERATED_BODY()

public:
	UFUNCTION(CallInEditor, Category = "Niagara")
	void CreateNiagaraModuleSnapshot();
};
