// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TestPackedActorGenerator.generated.h"

/**
 * 
 */
UCLASS()
class LIQUID_PROJECT_API UTestPackedActorGenerator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	UFUNCTION(BlueprintCallable)
	static bool GenerateActor();
	
};
