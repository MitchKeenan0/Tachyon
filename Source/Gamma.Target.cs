// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class GammaTarget : TargetRules
{
	public GammaTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;

        bUsesSteam = true;

        ExtraModuleNames.Add("Gamma");
	}
}
