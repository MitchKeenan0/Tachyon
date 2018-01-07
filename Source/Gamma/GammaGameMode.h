// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GammaGameMode.generated.h"

/**
 * The GameMode defines the game being played. It governs the game rules, scoring, what actors
 * are allowed to exist in this game type, and who may enter the game.
 */
UCLASS(minimalapi)
class AGammaGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AGammaGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<APawn> PlayerCharacter;
};
