// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/LevelScriptActor.h"
#include "EffectUnitTestLevel.generated.h"

/**
 * 
 */
UCLASS()
class LIQUID_API AEffectUnitTestLevel : public ALevelScriptActor
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	UFUNCTION(BlueprintImplementableEvent)
	void ExecuteTestSequential();
	

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FAssetData> NiagaraAssetArray{};
	
private:
	UPROPERTY(EditDefaultsOnly)
	FString NiagaraRootPath{};
	
	
};
