// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_AttachNiagara.generated.h"


class UNiagaraSystem;
class UNiagaraComponent;

UCLASS()
class LIQUID_API UAnimNotify_AttachNiagara : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_AttachNiagara() = default;
	virtual ~UAnimNotify_AttachNiagara()override = default;
	
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual void BeginDestroy() override;
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void ValidateAssociatedAssets() override;
#endif
	
private:
	/**
	* @brief Niagaraをスポーンする
	* @param MeshComp スケルタルメッシュコンポーネント
	* @param Animation アニメーションシーケンス
	* @return スポーンされたNiagaraコンポーネント
	*/
	UNiagaraComponent* SpawnNiagara(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation) const;

	/**
	* @brief Niagaraにfloatパラメータを設定する
	* @param InNiagaraComponent 対象のNiagaraコンポーネント
	*/
	void SetFloatParametersToNiagara(UNiagaraComponent* InNiagaraComponent);

	/**
	* @brief Niagaraにソケット位置を設定する
	* @param InNiagaraComponent 対象のNiagaraコンポーネント
	* @param MeshComp スケルタルメッシュコンポーネント
	*/
	void SetSocketLocationToNiagara(UNiagaraComponent* InNiagaraComponent, USkeletalMeshComponent* MeshComp);

	/**
	* @brief Niagaraを初期化する
	* @param MeshComp スケルタルメッシュコンポーネント
	* @param Animation アニメーションシーケンス
	*/
	void InitializeNiagara(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation);

	/**
	* @brief Niagaraをアクティブ化する
	* @param MeshComp スケルタルメッシュコンポーネント 
	* @param Animation アニメーションシーケンス
	*/
	void ActivateNiagara(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation);

	/**
	* @brief Niagaraの非アクティブ化をスケジュールする
	* @param NiagaraComponent 対象のNiagaraコンポーネント
	* @param MeshComp スケルタルメッシュコンポーネント
	* @param Animation アニメーションシーケンス
	*/
	void ScheduleDeactivate(UNiagaraComponent* NiagaraComponent, USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation);

	/**
	* @brief Niagaraの破棄をスケジュールする
	* @param NiagaraComponent 対象のNiagaraコンポーネント
	* @param MeshComp スケルタルメッシュコンポーネント
	* @param Animation アニメーションシーケンス
	*/
	void ScheduleDestroy(UNiagaraComponent* NiagaraComponent, USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation);

	
private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset", meta = (AllowPrivateAccess = "true", ToolTip="スポーンするNiagaraシステム"))
	TObjectPtr<UNiagaraSystem> NiagaraSystem{};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true", ToolTip="メッシュにアタッチするかどうか"))
	bool IsAttach{false};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true", ToolTip="アタッチするソケットの名前"))
	FName AttachSocketName{};
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true", ToolTip="シミュレーションされた物理ボディを溶接するかどうか"))
	bool InWeldSimulatedBodies{false};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true", ToolTip="位置オフセット"))
	FVector LocationOffset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true", ToolTip="回転オフセット"))
	FRotator RotationOffset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "User Parameters", meta = (AllowPrivateAccess = "true", ToolTip="Niagaraに設定する初期float型パラメータ"))
	TMap<FName, float> InitialFloatParameters{};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "User Parameters", meta = (AllowPrivateAccess = "true", ToolTip="Niagaraに設定する初期ソケット位置パラメータ"))
	TMap<FName, FName> InitialSocketLocationParameters{};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Parameters", meta = (AllowPrivateAccess = "true", ToolTip="アクティブ化までの遅延時間"))
	float DelayActivateTime{.0f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Parameters", meta = (AllowPrivateAccess = "true", ToolTip="非アクティブ化するまでの時間"))
	float DeactivateTime{.0f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Parameters", meta = (AllowPrivateAccess = "true", ClampMin="0.0", UIMin="0.0", ToolTip="非アクティブ化後に破棄するまでの時間"))
	float DestroyAfterDeactivateTime {1.0f};
};