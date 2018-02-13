// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine.h"
#include "GameFramework/Actor.h"
#include "GammaCharacter.h"
#include "GammaSoloSequence.generated.h"

UCLASS()
class GAMMA_API AGammaSoloSequence : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AGammaSoloSequence();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void MainSequence(float DeltaTime);

	void SpawnDenizen();

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AGammaCharacter> DenizenClass;

	UPROPERTY(EditDefaultsOnly)
	float FirstEncounterTime = 3.0f;

	UPROPERTY(EditDefaultsOnly)
	FVector SpawnLocation = FVector::ZeroVector;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	float RunTimer = 0.0f;
	bool bSpawned = false;
	AGammaCharacter* Player = nullptr;
	TArray<AActor*> PlayersArray;
	
};
