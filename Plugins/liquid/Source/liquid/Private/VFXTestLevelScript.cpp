// Fill out your copyright notice in the Description page of Project Settings.


#include "VFXTestLevelScript.h"
#include "PostProcessCallSubsystem.h"

void AVFXTestLevelScript::ExecutePostProcessInitVectorParameter(const FName EffectID, const FName ParameterName)
{
	auto CallSystem = GetWorld()->GetSubsystem<UPostProcessCallSubsystem>();
	if (!CallSystem)
		return;
	float X = FMath::FRand();
	float Y = FMath::FRand();
	float Z = .0;

	FLinearColor Val(X,Y,Z,.0f);
	CallSystem->PlayPostEffect(EffectID,[ParameterName, Val](UMaterialInstanceDynamic* DynamicInstance)
	{
		DynamicInstance->SetVectorParameterValue(ParameterName,Val);
	});
}

bool AVFXTestLevelScript::IsExecuteAdditionalPostEffect(const FName EffectID)
{
	auto CallSystem = GetWorld()->GetSubsystem<UPostProcessCallSubsystem>();
	if (!CallSystem)
		return false;
	return CallSystem->IsPlayingOverrideEffect(EffectID);
}
