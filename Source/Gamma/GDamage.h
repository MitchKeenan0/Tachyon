// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine.h"
#include "GameFramework/Actor.h"
#include "GDamage.generated.h"

UCLASS()
class GAMMA_API AGDamage : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AGDamage();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Lifetime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class USceneComponent* DamageScene = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UParticleSystemComponent* DamageParticles = nullptr;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	
	
};
