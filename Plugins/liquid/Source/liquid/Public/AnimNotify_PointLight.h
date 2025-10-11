// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "TransientPointLightComponent.h"
#include "AnimNotify_PointLight.generated.h"

/**
 * 
 */
UCLASS()
class LIQUID_API UAnimNotify_PointLight : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	UTransientPointLightComponent* SpawnPointLight(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation);
	
private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true", ToolTip="メッシュにアタッチするかどうか"))
	bool bIsAttach{false};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true", ToolTip="アタッチするソケットの名前"))
	FName SocketName{};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transforms", meta = (AllowPrivateAccess = "true", ToolTip="位置オフセット"))
	FVector LocationOffset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true", ToolTip="パラメータ", ShowOnlyInnerProperties))
	FTransientPointLightDesc Parameters;
	
};
