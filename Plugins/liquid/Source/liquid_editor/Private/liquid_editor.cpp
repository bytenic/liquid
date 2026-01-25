// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#define LOCTEXT_NAMESPACE "Fliquid_editorModule"



class Fliquid_editorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};



void Fliquid_editorModule::StartupModule()
{

}

void Fliquid_editorModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(Fliquid_editorModule, liquid)