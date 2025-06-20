// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#if UE_BUILD_DEVELOPMENT
	#define EFFECT_DISPLAY_ENABLED 1
#else
	#define EFFECT_DISPLAY_ENABLED 0
#endif

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/StreamableManager.h"
#include "EffectDisplayActor.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

UCLASS()
class LIQUID_API AEffectDisplayActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AEffectDisplayActor();

#if	EFFECT_DISPLAY_ENABLED
	
	virtual void BeginPlay() override;
	virtual void Destroyed() override;
protected:
	virtual void Tick(float DeltaTime)override;
	
private:
	void StopCurrentPlayEffect();
	bool PlayNext();
	void RotationNiagaraSystem(float DeltaTime)const ;
	void BeginLoadAsync();
	void LoadNiagaraSystemAsync();
	bool ShouldStartNextEffect();
	
#endif //UPROPERTYマクロ関連はビルドから除外できないかったのでここまで
private:
	UPROPERTY(EditAnywhere, meta=(ToolTip="再生するエフェクトリスト"))
	TArray<TSoftObjectPtr<UNiagaraSystem>> Playlist{};
	UPROPERTY(EditAnywhere, meta=(Tooltip = "PlayListに追加するNiagaraフォルダのパス"))
	FString AdditionalNiagaraFolderPath{};
	UPROPERTY(EditAnywhere, meta=(Tooltip = "再生位置のオフセット"))
	FVector PlaceOffset{200.0,.0,100.0};
	UPROPERTY(EditAnywhere, meta=(Tooltip = "回転時の半径"))
	float RotationRadius{.0f};
	UPROPERTY(EditAnywhere, meta=(Tooltip = "1秒あたりの回転角度(度) ※NiagaraSystemのLocalSpaceをONにしないと回転しません"))
	float RotateSpeed{.0f};
	UPROPERTY(EditAnywhere, meta=(Tooltip = "1つあたりのエフェクトの再生時間"))
	float PlayInterval{5.0f};
	//UPROPERTY(EditAnywhere, meta=(Tooltip = "ゲーム再生時に自動で登録したエフェクトを再生する"))
	//bool IsAutoPlay{true};
	UPROPERTY(EditAnywhere, meta=(Tooltip = "ループ再生を行うかどうか"))
	bool IsLoop{true};
	
	UPROPERTY()
	TObjectPtr<USceneComponent> RotationRoot{};	//NiagaraComponent自身を回転させてもSystemが回らなかったので親子関係で回転させる
	UPROPERTY()
	TObjectPtr<USceneComponent> PlaceRoot{};	//実際のNiagaraComponent配置位置(RotationRadius)
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> NiagaraComponent{}; //再生中のNiagara
	TSharedPtr<FStreamableHandle> CurrentLoadingHandle{};
	UPROPERTY()
	TArray<TObjectPtr<UNiagaraSystem>> LoadedPlayList{};

	int32 CurrentPlayIndex{-1}; //再生中のPlaylistArray Index
	int32 CurrentLoadingIndex{0}; //ロード中のIndex
	
	static constexpr int32 PlaylistReserveCapacity = 64;
	static constexpr int32 InvalidPlayIndex = -1;
};
