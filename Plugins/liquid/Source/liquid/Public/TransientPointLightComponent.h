// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PointLightComponent.h"
#include "TransientPointLightComponent.generated.h"


USTRUCT(BlueprintType)
struct FTransientPointLightDesc
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Parameters", meta = (AllowPrivateAccess = "true", ToolTip="ライトの強さ"))
	float DestroyTime{1.0f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light Parameters", meta = (AllowPrivateAccess = "true", ToolTip="ライトの強さ"))
	float Intensity{1.0f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light Parameters", meta = (AllowPrivateAccess = "true", ToolTip="ライトの色"))
	FLinearColor LightColor{FLinearColor::White};
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light Parameters", meta = (AllowPrivateAccess = "true", ToolTip="ライトの強さカーブ"))
	TObjectPtr<UCurveFloat> IntensityCurve{};
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light Parameters", meta = (AllowPrivateAccess = "true", ToolTip="ライトの色カーブ"))
	TObjectPtr<UCurveLinearColor> ColorCurve{};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light Parameters", meta = (AllowPrivateAccess = "true", ToolTip="ライトの強さカーブスケール"))
	float IntensityCurveScale{1.0f};
	
};

/**
 * 
 */
UCLASS()
class LIQUID_API UTransientPointLightComponent : public UPointLightComponent
{
	GENERATED_BODY()

public:
	UTransientPointLightComponent();
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetParameters(const FTransientPointLightDesc& Desc);
private:
	void InitParameters();
private:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true", ToolTip="パラメータ", ShowOnlyInnerProperties))
	FTransientPointLightDesc Parameters;

	UPROPERTY(Transient)
	bool bIsInitialized = false;
	
	UPROPERTY(Transient)
	float CurrentDuration = .0f;
};
