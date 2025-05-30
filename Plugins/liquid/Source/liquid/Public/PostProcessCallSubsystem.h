// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/EngineTypes.h"
#include "Engine/Scene.h"
//#include "PostProcess/confi"
#include "PostProcessCallSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FPostProcessConfig : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	TSoftObjectPtr<UMaterialInstance> Material{nullptr};

	UPROPERTY(BlueprintReadOnly)
	float Duration = 1.0f;
};

/**
 * 
 */
UCLASS(Config=Game)
class LIQUID_API UPostProcessCallSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override{RETURN_QUICK_DECLARE_CYCLE_STAT(UPostProcessCallSubsystem, STATGROUP_Tickables);}

	UFUNCTION(BlueprintCallable,Category="PostProcess")
	void PlayPostEffect(const FName EffectID);
	void StopCurrentEffect();

	UPROPERTY(EditDefaultsOnly, Category="Config")
	TObjectPtr<UDataTable> PostProcessTable{nullptr};
	
private:
	void BeginPlayEffect(const FPostProcessConfig& Config);
	void EndEffect();

	float ElapsedTime = .0f;
	bool IsPlaying{false};

	FPostProcessConfig CurrentSettings{};

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CurrentPlayMID = nullptr;

	FPostProcessSettings OverrideSettings;
	FWeightedBlendable Blendable{};
	
};
