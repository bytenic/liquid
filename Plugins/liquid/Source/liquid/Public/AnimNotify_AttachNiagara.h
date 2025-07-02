// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_AttachNiagara.generated.h"


class UNiagaraSystem;
class UNiagaraComponent;

UENUM(BlueprintType)
enum class EAttachNiagaraLocationType: uint8
{
	MeshLocation,
	SocketLocation,
};

/**
 * 
 */
UCLASS()
class LIQUID_API UAnimNotify_AttachNiagara : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_AttachNiagara() = default;
	virtual ~UAnimNotify_AttachNiagara()override = default;
	
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	UNiagaraComponent* SpawnNiagara(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation);
private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> NiagaraSystem{};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> NiagaraComponent{}; //note: 変数として保持しなくていいかもしれない

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters", meta = (AllowPrivateAccess = "true"))
	EAttachNiagaraLocationType AttachType{EAttachNiagaraLocationType::SocketLocation};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters", meta = (AllowPrivateAccess = "true"))
	FName AttachSocketName{};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters", meta = (AllowPrivateAccess = "true"))
	FVector LocationOffset;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters", meta = (AllowPrivateAccess = "true"))
	TMap<FName, float> InitialFloatParameters{};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters", meta = (AllowPrivateAccess = "true"))
	float DelayActivateTime{.0f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters", meta = (AllowPrivateAccess = "true"))
	float DestroyTime{1.0f};
	
	//UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters", meta = (AllowPrivateAccess = "true"))
	//TMap<FName, FVector4> InitialVectorParameters{};
	
};
