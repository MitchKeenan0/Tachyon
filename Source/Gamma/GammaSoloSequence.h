// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "GammaAIController.h"
#include "GammaSoloSequence.generated.h"

UCLASS()
class GAMMA_API AGammaSoloSequence : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AGammaSoloSequence();

	FRandomStream* RandStream;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void MainSequence(float DeltaTime);

	int NumDenizens();

	void SpawnDenizen();

	void AquirePlayer();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AActor> ObstacleClass1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AActor> ObstacleClass2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AGammaCharacter> KaraokeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AGammaCharacter> PeaceGiantClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AGammaCharacter> BaetylusClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AGammaAIController> AIControllerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FirstEncounterTime = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSpawnCharacters = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSpawnObstacles = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int MaxLiveUnits = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int MaxObstacles = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector SpawnLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite)
	AGammaCharacter* Player = nullptr;

	UPROPERTY(BlueprintReadWrite)
	TArray<AActor*> PlayersArray;
	
	UPROPERTY(BlueprintReadWrite)
	TArray<AActor*> DenizenArray;

	UPROPERTY(BlueprintReadWrite)
	TArray<AActor*> ObstacleArray;

	UPROPERTY(BlueprintReadWrite)
	float RunTimer = 0.0f;

	UPROPERTY(BlueprintReadWrite)
	int Spawns = 0;

	UPROPERTY(BlueprintReadWrite)
	bool bSpawned = false;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	
	UPROPERTY()
	int PreviousSpawn = 0;

	UPROPERTY()
	int NumObstacles = 0;
	
	
};
