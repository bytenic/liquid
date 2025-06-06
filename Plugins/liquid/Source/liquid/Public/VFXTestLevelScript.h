// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/LevelScriptActor.h"
#include "VFXTestLevelScript.generated.h"

/**
 * 
 */
UCLASS()
class LIQUID_API AVFXTestLevelScript : public ALevelScriptActor
{
	GENERATED_BODY()


public:
	UFUNCTION(BlueprintCallable)
	void ExecutePostProcessInitVectorParameter(const FName EffectID, const FName ParameterName);
};
