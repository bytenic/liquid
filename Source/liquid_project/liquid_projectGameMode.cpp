// Copyright Epic Games, Inc. All Rights Reserved.

#include "liquid_projectGameMode.h"
#include "liquid_projectCharacter.h"
#include "UObject/ConstructorHelpers.h"

Aliquid_projectGameMode::Aliquid_projectGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
