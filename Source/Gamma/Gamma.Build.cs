// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Gamma : ModuleRules
{
	public Gamma(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "Paper2D",
            "OnlineSubsystem",
            "OnlineSubsystemUtils"
        });

        DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
    }
}
