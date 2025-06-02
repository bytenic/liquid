// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/EngineTypes.h"
#include "Engine/Scene.h"
#include "PostProcessCallSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FPostProcessConfig : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UMaterialInstance> Material{nullptr}; //TSoftObjectPtrの方がいいかも

	UPROPERTY(EditAnywhere)
	float Duration = 1.0f;
};

/**
 * 
 */
UCLASS(Config=Game)
class LIQUID_API UPostProcessCallSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	
	UFUNCTION(BlueprintCallable,Category="PostProcess")
	void PlayPostEffect(const FName EffectID);
	void StopCurrentEffect();

private:
	void BeginEffect(const FPostProcessConfig& Config);
	void EndEffect();
	void OnWorldPostActorTick(UWorld* InWorld, ELevelTick InType,float DeltaTime);

	float ElapsedTime = .0f;
	bool IsPlaying{false};

	FPostProcessConfig CurrentSettings{};

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CurrentPlayMID = nullptr;
	UPROPERTY()
	TObjectPtr<UDataTable> PostProcessTable{nullptr};
	FPostProcessSettings OverrideSettings;
	FWeightedBlendable Blendable{};
	
	FDelegateHandle PostActorTickHandle;
};
