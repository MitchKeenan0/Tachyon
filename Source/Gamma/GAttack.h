// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

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
	UParticleSystemComponent* GetParticles() { return AttackParticles; }

	UFUNCTION(BlueprintCallable)
	void ReportHit(AActor* HitActor);

	UFUNCTION(BlueprintCallable)
	void Nullify(int AttackType);


	// Public variables
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bSecondary = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool LockedEmitPoint = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float DurationTime = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float MagnitudeTimeScalar = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float HitsPerSecond = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bLethal = false;


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Attack functions
	void UpdateAttack(float DeltaTime);

	void DetectHit(FVector RaycastVector);

	UFUNCTION(BlueprintCallable)
	void HitEffects(AActor* HitActor, FVector HitPoint);

	UFUNCTION(BlueprintCallable)
	void SpawnDamage(AActor* HitActor, FVector HitPoint);

	UFUNCTION(BlueprintCallable)
	void ApplyKnockback(AActor* HitActor, FVector HitPoint);


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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ShootingAngle = 21.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ProjectileSpeed = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bScaleProjectileSpeed = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ProjectileMaxSpeed = 12000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float AngleSweep = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float RefireTime = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ChargeCost = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bUpdateAttack = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float FireDelay = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float RaycastHitRange = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bRaycastOnMesh = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float KineticForce = 30000.0f;


	// Local Variables
	UPROPERTY(BlueprintReadWrite)
	float LifeTimer = 0.0f;

	UPROPERTY(BlueprintReadWrite)
	float HitTimer = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class UCapsuleComponent* CapsuleRoot = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class USceneComponent* AttackScene = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class UAudioComponent* AttackSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class UProjectileMovementComponent* ProjectileComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class UParticleSystemComponent* AttackParticles = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class UPaperSpriteComponent* AttackSprite = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<AGDamage> DamageClass = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<AGDamage> BlockedClass = nullptr;


	// Replicated Variables /////////////////////////////////////////
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadWrite)
	float DynamicLifetime = 0.0f;

	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadWrite)
	bool bHit = false;

	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadWrite)
	int numHits = 1;

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

	UPROPERTY(BlueprintReadWrite, Replicated, BlueprintReadWrite)
	float LethalTime = 0.2f;


	
};
