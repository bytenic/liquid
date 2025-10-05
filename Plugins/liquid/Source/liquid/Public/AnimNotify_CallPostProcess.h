// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CallPostProcess.generated.h"

class UMaterialInstanceDynamic;

UENUM(Blueprintable)
enum class EPostProcessCallParamType : uint8
{
	ActorLocation,
	SocketLocation
};

USTRUCT(BlueprintType)
struct FPostProcessCallInitSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Params", meta=(AllowPrivateAccess="true"))
	FName ParamName{};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Params", meta=(AllowPrivateAccess="true"))
	EPostProcessCallParamType ParamType{EPostProcessCallParamType::ActorLocation};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Params", meta=(AllowPrivateAccess="true"))
	FName SocketName{};
	
};

/**
 * 
 */
UCLASS(DisplayName="Call PostProcess")
class LIQUID_API UAnimNotify_CallPostProcess : public UAnimNotify
{
	GENERATED_BODY()
	UAnimNotify_CallPostProcess();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	void InitializeMaterialParameters(USkeletalMeshComponent* MeshComp, UMaterialInstanceDynamic* MID);

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Params", meta=(AllowPrivateAccess="true"))
	FName PostProcessRowName{};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Params", meta=(AllowPrivateAccess="true"))
	TArray<FPostProcessCallInitSettings> ParamSettings;

};
