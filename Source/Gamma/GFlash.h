// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine.h"
#include "UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "GFlash.generated.h"

UCLASS()
class GAMMA_API AGFlash : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AGFlash();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FlashGrowthIntensity = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FlashMaxScale = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FlashMaxPitch = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FlashMaxVolume = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class USceneComponent* FlashScene = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UAudioComponent* FlashSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UParticleSystemComponent* FlashParticles = nullptr;

	UFUNCTION()
	void UpdateFlash(float DeltaTime);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	
	
};
