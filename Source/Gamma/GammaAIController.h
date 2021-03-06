// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AIController.h"
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

	UFUNCTION(BlueprintCallable)
	void SetReactionTime(float Value) { ReactionTime = Value; }

	UFUNCTION(BlueprintCallable)
	float GetReactionTime() { return ReactionTime; }

	UFUNCTION(BlueprintCallable)
	void AimAtTarget(AActor* TargetActor);

	UFUNCTION(BlueprintCallable)
	bool HasViewToTarget();

	/*UFUNCTION(BlueprintCallable)
	void InitAttack();

	UFUNCTION(BlueprintCallable)
	void FireAttack();*/

	FRandomStream* RandStream;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	TArray<AActor*> PlayersArray;

	// Attributes
	UPROPERTY(EditDefaultsOnly)
	float PrimaryRange = 1500.0f;

	UPROPERTY(EditDefaultsOnly)
	float SecondaryRange = 4500.0f;

	UPROPERTY(EditDefaultsOnly)
	float MoveSpeed = -1.0f;

	UPROPERTY(EditDefaultsOnly)
	float TurnSpeed = 300.0f;

	UPROPERTY(EditDefaultsOnly)
	float MovesPerSec = 5.0f;

	UPROPERTY(EditDefaultsOnly)
	float PrefireTime = 0.0f;

	UPROPERTY(EditDefaultsOnly)
	float MoveRange = 750.0f;

	UPROPERTY(EditDefaultsOnly)
	float Aggression = 0.5f;

	UPROPERTY(EditDefaultsOnly)
	float ReactionTime = 0.2f;

	UPROPERTY(EditDefaultsOnly)
	float ShootingAngle = 45.0f;


	// Quiet variables
	UPROPERTY(EditDefaultsOnly)
	APawn* MyPawn = nullptr;

	UPROPERTY(EditDefaultsOnly)
	AGammaCharacter* MyCharacter = nullptr;

	UPROPERTY(EditDefaultsOnly)
	float ReactionTimer = 0.0f;

	UPROPERTY(EditDefaultsOnly)
	bool bCourseLayedIn = false;

	UPROPERTY(EditDefaultsOnly)
	FVector LocationTarget = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly)
	FVector MoveInput = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly)
	bool bViewToTarget = false;

private:
	UPROPERTY()
	float MoveTimer = 0.0f;

	UPROPERTY()
	float TravelTimer = 0.0f;

	UPROPERTY()
	float PrefireTimer = 0.0f;

	UPROPERTY()
	bool bShootingSoon = false; /// used to reject moving; would cause a miss
};
