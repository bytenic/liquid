#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "AssetActionUtility.h"
#include "NiagaraModuleSnapShot.generated.h"

UCLASS()
class UNiagaraModuleSnapshotAction : public UAssetActionUtility
{
	GENERATED_BODY()

public:
	//virtual bool CanExecuteOnAssets(const TArray<FAssetData>& InAssets) const override;

	
	UFUNCTION(CallInEditor, Category = "Niagara")
	void CreateNiagaraModuleSnapshot();
};
