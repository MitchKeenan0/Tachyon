// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine.h"
#include "AIController.h"
#include "GAttack.h"
#include "GammaCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GammaAIController.generated.h"

/**
 * 
 */
UCLASS()
class GAMMA_API AGammaAIController : public AAIController
{
	GENERATED_BODY()
	
	virtual void Tick(float DeltaSeconds) override;

public:
	// Pointers of interest
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class AGammaCharacter* Player = nullptr;

	// Behaviours
	UFUNCTION(BlueprintCallable)
	FVector GetNewLocationTarget();

	UFUNCTION(BlueprintCallable)
	void NavigateTo(FVector Target);

	UFUNCTION(BlueprintCallable)
	void Tactical(FVector Target);

	UFUNCTION(BlueprintCallable)
	bool ReactionTiming(float DeltaTime);

	/*UFUNCTION(BlueprintCallable)
	void InitAttack();

	UFUNCTION(BlueprintCallable)
	void FireAttack();*/
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	TArray<AActor*> PlayersArray;

	// Attributes
	UPROPERTY(EditDefaultsOnly)
	float MoveSpeed = 300.0f;

	UPROPERTY(EditDefaultsOnly)
	float MoveRange = 750.0f;

	UPROPERTY(EditDefaultsOnly)
	float Aggression = 0.5f;

	UPROPERTY(EditDefaultsOnly)
	float ReactionTime = 0.2f;

	UPROPERTY(EditDefaultsOnly)
	float ShootingAngle = 45.0f;

private:
	// Quiet variables
	float ReactionTimer = 0.0f;
	bool bCourseLayedIn = false;
	FVector LocationTarget = FVector::ZeroVector;
	FVector MoveInput = FVector::ZeroVector;
};
