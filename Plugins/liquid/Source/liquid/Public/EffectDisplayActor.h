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
	AEffectDisplayActor();
	
	virtual void BeginPlay() override;
	UFUNCTION(BlueprintCallable)
	void PlayEffect();
protected:
	virtual void Tick(float DeltaTime)override;
protected:
	UPROPERTY(EditAnywhere, meta=(Tooltip = "再生位置のオフセット"))
	FVector EffectPlaceOffset{200.0,.0,100.0};
	
	UPROPERTY(EditAnywhere, meta=(Tooltip = "登録対象となるエフェクトのルートパス"))
	FString NiagaraRootPath{};

	UPROPERTY(EditAnywhere, meta=(Tooltip = "ゲーム再生時に自動で登録したエフェクトを再生する"))
	bool IsAutoPlay{true};
	UPROPERTY(EditAnywhere, meta=(Tooltip = "ループ再生を行うかどうか"))
	bool IsLoop{true};

	UPROPERTY(EditAnywhere, meta=(Tooltip = "一秒あたりの回転角度(度) ※NiagaraSystemのLocalSpaceをONにしないと回転しません"))
	float RotateSpeed{.0f};
	
	UPROPERTY(EditAnywhere, meta=(Tooltip = "1つあたりのエフェクトの再生時間"))
	float PlayInterval{5.0f};
	
private:
	UFUNCTION()
	void OnNiagaraSystemFinished(UNiagaraComponent* InComp);

	void StopCurrentPlayEffect();
	void ClearPlaylistQueue();
	bool PlayNext();
	bool IsPlaying() const;
	void RotationNiagaraSystem(float DeltaTime)const ;
	bool LoadNiagaraSystems();

private:
	static constexpr int32 QueueCapacity = 64;
	static constexpr int32 InvalidPlayIndex = -1;

	UPROPERTY()
	TObjectPtr<USceneComponent> RotationRoot{};	//NiagaraComponent自身を回転させてもSystemが回らなかったので親子関係で回転させる
	
	UPROPERTY()
	TArray<TObjectPtr<UNiagaraSystem>> PlaylistArray{}; //再生するNiagaraSystemのコンテナ
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> NiagaraComponent{}; //再生中のNiagara
	int32 CurrentPlayIndex{-1}; //再生中のPlaylistArray Index
};
