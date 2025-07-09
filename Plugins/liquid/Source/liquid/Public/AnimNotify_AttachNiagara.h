// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_AttachNiagara.generated.h"


class UNiagaraSystem;
class UNiagaraComponent;

/**
 * @brief Animation notify that spawns and attaches a Niagara particle system to a skeletal mesh during an animation sequence.
 *
 * This class facilitates the creation and management of a Niagara particle system during animation playback. It allows for
 * configuring attachment parameters such as location, rotation, and scale rules, and supports delay activation, destruction timing,
 * and initial particle system parameters.
 *
 * @note This class assumes the use of the Niagara particle system framework in Unreal Engine.
 */
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
	UNiagaraComponent* SpawnNiagara(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation) const;
	void SetFloatParametersToNiagara(UNiagaraComponent* InNiagaraComponent);
	void SetSocketLocationToNiagara(UNiagaraComponent* InNiagaraComponent, USkeletalMeshComponent* MeshComp);

	void InitializeNiagara(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation);
	void ActivateNiagara(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation);
	void SetUpDeactivate(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation);
	void SetUpDestroy(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation);

	
private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> NiagaraSystem{};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	bool IsAttach{false};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	FName AttachSocketName{};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	EAttachmentRule LocationRule{EAttachmentRule::KeepRelative};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	EAttachmentRule RotationRule{EAttachmentRule::KeepRelative};
	//UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	//EAttachmentRule ScaleRule{EAttachmentRule::KeepRelative};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	bool InWeldSimulatedBodies{false};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	FVector LocationOffset;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "User Parameters", meta = (AllowPrivateAccess = "true"))
	TMap<FName, float> InitialFloatParameters{};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "User Parameters", meta = (AllowPrivateAccess = "true"))
	TMap<FName, FName> InitialSocketLocationParameters{};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Parameters", meta = (AllowPrivateAccess = "true"))
	float DelayActivateTime{.0f};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Parameters", meta = (AllowPrivateAccess = "true"))
	float DeactivateTime{.0f};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Parameters", meta = (AllowPrivateAccess = "true"))
	float DestroyAfterDeactivateTime {1.0f};

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> NiagaraComponent{}; //note: 変数として保持しなくていいかもしれない
};
