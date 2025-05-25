// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EffectDisplayActor.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

UCLASS()
class LIQUID_API AEffectDisplayActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEffectDisplayActor();

public:
	virtual void BeginPlay() override;
	UFUNCTION(BlueprintCallable)
	void PlayEffect(TArray<UNiagaraSystem*> PlaySystem);

	UFUNCTION(BlueprintImplementableEvent)
	void ExecuteAllEffectSequential();
	
protected:

	UPROPERTY(EditAnywhere)
	FVector EffectPlaceOffset{};
	//private に移せるかも:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TObjectPtr<UNiagaraComponent> NiagaraComponent{};

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FAssetData> NiagaraAssetArray{};

	UPROPERTY(EditAnywhere)
	FString NiagaraRootPath{};

	UPROPERTY(EditAnywhere)
	bool IsAutoPlay{true};
	
private:
	UFUNCTION()
	void OnNiagaraSystemFinished(UNiagaraComponent* InComp);

	void StopCurrentPlayEffect();
	void ClearPlaylistQueue();
	bool PlayNext();
	bool IsPlaying() const;

private:
	static constexpr int32 QueueCapacity = 64;
	static constexpr int32 InvalidPlayIndex = -1;
	TArray<TObjectPtr<UNiagaraSystem>> PlaylistArray{};
	int32 CurrentPlayIndex{-1};

	
};
