// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_AttachNiagara.generated.h"


class UNiagaraSystem;
class UNiagaraComponent;

USTRUCT(BlueprintType)
struct FSocketAttachDesc
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SocketTransform", meta=(AllowPrivateAccess="true"))
	FName SocketName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SocketTransform", meta=(AllowPrivateAccess="true"))
	bool bIsLocalSpace{false};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SocketTransform", meta=(AllowPrivateAccess="true"))
	bool bIsRequiareUpdate{false};
};

UCLASS()
class LIQUID_API UAnimNotify_AttachNiagara : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_AttachNiagara() = default;
	virtual ~UAnimNotify_AttachNiagara()override = default;
	
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual void BeginDestroy() override;
	virtual FString GetNotifyName_Implementation() const override;
	
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

	bool IsNeedUpdateSocketLocation()const;

	void ScheduleUpdateSocketLocation(UNiagaraComponent* NiagaraComponent, USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation);
	
	
private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset", meta = (AllowPrivateAccess = "true", ToolTip="スポーンするNiagaraシステム"))
	TObjectPtr<UNiagaraSystem> NiagaraSystem{};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true", ToolTip="メッシュにアタッチするかどうか"))
	bool bIsAttach{false};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true", ToolTip="アタッチするソケットの名前"))
	FName AttachSocketName{};
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true", ToolTip="シミュレーションされた物理ボディを溶接するかどうか"))
	bool bInWeldSimulatedBodies{false};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true", ToolTip="位置オフセット"))
	FVector LocationOffset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true", ToolTip="回転オフセット"))
	FRotator RotationOffset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "User Parameters", meta = (AllowPrivateAccess = "true", ToolTip="Niagaraに設定する初期float型パラメータ"))
	TMap<FName, float> InitialFloatParameters{};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "User Parameters", meta = (AllowPrivateAccess = "true", ToolTip="Niagaraに設定するソケット位置パラメータ"))
	TMap<FName, FSocketAttachDesc> SocketLocationParameters{};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Parameters", meta = (AllowPrivateAccess = "true", ToolTip="非アクティブ化するまでの時間"))
	float DeactivateTime{.0f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Parameters", meta = (AllowPrivateAccess = "true", ClampMin="0.0", UIMin="0.0", ToolTip="破棄するまでの時間"))
	float DestroyTime {1.0f};
};