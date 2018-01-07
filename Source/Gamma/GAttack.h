// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine.h"
#include "UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "GDamage.h"
#include "GAttack.generated.h"

UCLASS()
class GAMMA_API AGAttack : public AActor
{
	GENERATED_BODY()


private:
	float Lifetime = 0.0f;

public:	
	// Sets default values for this actor's properties
	AGAttack();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void InitAttack(AActor* Shooter, float Magnitude, float YScale);
	
	void TakeGG();

	bool GetHit() { return bHit; }
	bool GetLethal() { return bLethal; }

	// Public gets
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool LockedEmitPoint = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DurationTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bLethal = true;


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Attack functions
	void UpdateAttack(float DeltaTime);

	void DetectHit(FVector RaycastVector);

	void SpawnDamage(FVector HitPoint, AActor* HitActor);

	void ApplyKnockback(AActor* HitActor);


	// Basic Variables
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ShootingAngle = 21.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ProjectileSpeed = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ProjectileMaxSpeed = 12000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AngleSweep = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RefireTime = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ChargeCost = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUpdateAttack = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FireDelay = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FlashTiming = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FlashTimingFluctuation = 0.025f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RaycastHitRange = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRaycastOnMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float KineticForce = 1.0f;


	// Local Variables
	UPROPERTY(BlueprintReadWrite)
	float LifeTimer = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class USceneComponent* AttackScene = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UAudioComponent* AttackSound = nullptr;

	UPROPERTY(EditDefaultsOnly)
	class UProjectileMovementComponent* ProjectileComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class UParticleSystemComponent* AttackParticles = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class UPaperSpriteComponent* AttackSprite = nullptr;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AGDamage> DamageClass = nullptr;

	/*UPROPERTY(BlueprintReadWrite)
	class AMatch* CurrentMatch = nullptr;*/

	

	// Replicated Variables
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadWrite)
	bool bHit = false;

	UPROPERTY(BlueprintReadWrite, Replicated)
	class AActor* OwningShooter = nullptr;

	UPROPERTY(BlueprintReadWrite, Replicated, BlueprintReadWrite)
	float AttackMagnitude = 0.0f;

	UPROPERTY(BlueprintReadWrite, Replicated, BlueprintReadWrite)
	float ShotDirection = 0.0f;


	
};
