// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class liquid_project : ModuleRules
{
	public liquid_project(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "LevelInstanceEditor" });
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
	}
}
