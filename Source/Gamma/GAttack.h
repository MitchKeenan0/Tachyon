// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine.h"
#include "UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "GDamage.h"
#include "GAttack.generated.h"

class AGammaCharacter;

UCLASS()
class GAMMA_API AGAttack : public AActor
{
	GENERATED_BODY()



public:	
	// Sets default values for this actor's properties
	AGAttack();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Attack functions
	UFUNCTION()
	void InitAttack(AActor* Shooter, float Magnitude, float YScale);

	bool GetHit() { return bHit; }
	bool GetLethal() { return bLethal; }

	void TakeGG();

	UFUNCTION(BlueprintCallable)
	void Nullify();


	// Public variables
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool LockedEmitPoint = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DurationTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MagnitudeTimeScalar = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HitsPerSecond = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bLethal = false;


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Attack functions
	void UpdateAttack(float DeltaTime);

	void DetectHit(FVector RaycastVector);

	void SpawnDamage(AActor* HitActor, FVector HitPoint);

	void ApplyKnockback(AActor* HitActor);


	// Collision hit detec
	// Shield collision
	UFUNCTION()
	void OnAttackBeginOverlap
	(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);


	// Ref to match
	UPROPERTY(BlueprintReadWrite)
	class AGMatch* CurrentMatch = nullptr;


	// Basic Variables
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ShootingAngle = 21.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ProjectileSpeed = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bScaleProjectileSpeed = true;

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

	UPROPERTY(BlueprintReadWrite)
	float HitTimer = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class UCapsuleComponent* CapsuleRoot = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class USceneComponent* AttackScene = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UAudioComponent* AttackSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class UProjectileMovementComponent* ProjectileComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class UParticleSystemComponent* AttackParticles = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class UPaperSpriteComponent* AttackSprite = nullptr;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AGDamage> DamageClass = nullptr;


	// Replicated Variables /////////////////////////////////////////
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadWrite)
	float DynamicLifetime = 0.0f;

	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadWrite)
	bool bHit = false;

	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadWrite)
	bool bStillHitting = false;

	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadWrite)
	bool bDoneHitting = false;

	UPROPERTY(BlueprintReadWrite, Replicated)
	class AActor* OwningShooter = nullptr;

	UPROPERTY(BlueprintReadWrite, Replicated)
	class AActor* HitActor = nullptr;

	UPROPERTY(BlueprintReadWrite, Replicated, BlueprintReadWrite)
	float AttackMagnitude = 0.0f;

	UPROPERTY(BlueprintReadWrite, Replicated, BlueprintReadWrite)
	float ShotDirection = 0.0f;

	UPROPERTY(BlueprintReadWrite, Replicated, BlueprintReadWrite)
	float AttackDamage = 1.0f;


	
};
