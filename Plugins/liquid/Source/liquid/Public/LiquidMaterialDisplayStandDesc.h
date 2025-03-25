// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Materials/MaterialInstance.h"
#include "LiquidMaterialDisplayStandDesc.generated.h"


class UMaterialInstance;

UENUM(BlueprintType,DisplayName = "DP Type")
enum class ELiquidDisplayStandDynamicParameterType : uint8
{
	NotSim,
	DynamicParameterSlot0,
	DynamicParameterSlot1,
	DynamicParameterSlot2,
	DynamicParameterSlot3,
};

UENUM(BlueprintType,DisplayName = "Shape Type")
enum class ELiquidDisplayStandShapeType: uint8
{
	Quad,
	Sphere,
};


USTRUCT(BlueprintType,DisplayName = "Element")
struct FLiquidMaterialDisplayStandElement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TSoftObjectPtr<UMaterialInstance> MaterialInstance;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	ELiquidDisplayStandDynamicParameterType DynamicParameterType{};
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	ELiquidDisplayStandShapeType ShapeType{};
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText Title{};
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText Desc{};
};

/**
 * 
 */
UCLASS(BlueprintType)
class LIQUID_API ULiquidMaterialDisplayStandDesc : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FLiquidMaterialDisplayStandElement> DescArray{};
	
};
