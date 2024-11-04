// Copyright Epic Games, Inc. All Rights Reserved.

#include "liquid.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FliquidModule"

void FliquidModule::StartupModule()
{
	const FString ShaderDirectory = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("liquid"), TEXT("Shaders"));
	if(!AllShaderSourceDirectoryMappings().Contains("/liquid/Shaders"))
		AddShaderSourceDirectoryMapping("/liquid/Shaders", ShaderDirectory);
}

void FliquidModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FliquidModule, liquid)