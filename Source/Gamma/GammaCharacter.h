// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GAttack.h"
#include "GFlash.h"
#include "PaperCharacter.h"
#include "GammaCharacter.generated.h"

class UTextRenderComponent;

/**
 * This class is the default character for Gamma, and it is responsible for all
 * physical interaction between the player and the world.
 *
 * The capsule component (inherited from ACharacter) handles collision with the world
 * The CharacterMovementComponent (inherited from ACharacter) handles movement of the collision capsule
 * The Sprite component (inherited from APaperCharacter) handles the visuals
 */
UCLASS(config=Game)
class AGammaCharacter : public APaperCharacter
{
	GENERATED_BODY()

	// Side view camera
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera, meta=(AllowPrivateAccess="true"))
	class UCameraComponent* SideViewCameraComponent;

	// Camera boom positioning the camera beside the character
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	UTextRenderComponent* TextComponent;
	virtual void Tick(float DeltaSeconds) override;

	float AttackTimer = 0.0f;	/// stopwatch situation
	float AttackTimeout = 0.0f; /// generated at fire time
	float FrictionTimer = 0.0f;
	float BPM = 0.0f;
	float BPMTimer = 0.0f;
	float LastMoveTime = 0.0f;
	float CurrentMoveTime = 0.0f;
	float ChargeFXTimer = 0.0f;

	bool bOnTempo = false;
	//float x = 0.0f;
	//float z = 0.0f;				/// allows replicatiuon only 1pf for both
	//float MoveFriction = 0.0f;
	bool bFullCharge = false;

	FVector PositionOne = FVector::ZeroVector;
	FVector PositionTwo = FVector::ZeroVector;
	FVector Midpoint = FVector::ZeroVector;
	FVector PrevMoveInput = FVector::ZeroVector;



public:
	AGammaCharacter();

	// Returns SideViewCameraComponent subobject
	FORCEINLINE class UCameraComponent* GetSideViewCameraComponent() const { return SideViewCameraComponent; }
	// Returns CameraBoom subobject
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Called for scoreboard */
	UFUNCTION()
	int GetScore() { return Score; }
	void RaiseScore(int Val) { Score += Val; }

	UFUNCTION()
	float GetChargePercentage() { return Charge / ChargeMax; } // (Charge / ChargeMax)


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Character-wide updates ////////////////////////////////////////////////////////////
	void UpdateCharacter(float DeltaTime);
	void UpdateCamera(float DeltaTime);
	void UpdateAnimation();

	// Input funcs
	void MoveRight(float Value);
	void MoveUp(float Value);

	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* InputComponent) override;

	// Shield collision
	UFUNCTION()
	void OnShieldBeginOverlap
	(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	// ARRAYS ///////////////////////////////////////////////////////////////
	// Camera partners
	TArray<AActor*> FramingActors;


	// ATTACK STUFF ////////////////////////////////////////////////
	// Attack objects
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AGFlash> FlashClass = nullptr;
	class AGFlash* ActiveFlash = nullptr;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AGAttack> AttackClass = nullptr;
	class AGAttack* ActiveAttack = nullptr;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AActor> ChargeParticles = nullptr;
	class AActor* ActiveChargeParticles = nullptr;

	// Replicated functions
	void SetX(float Value);
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerSetX(float Value);
	
	void SetZ(float Value);
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerSetZ(float Value);

	void NewMoveKick();
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerNewMoveKick();

	void UpdateMoveParticles(FVector Move);
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerUpdateMoveParticles(FVector Move);

	void SetAim(float x, float z);
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerSetAim(float x, float z);

	void InitAttack();
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerInitAttack();

	void RaiseCharge(float DeltaTime);
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerRaiseCharge(float DeltaTime);

	void ReleaseAttack();
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerReleaseAttack();


	// VARIABLES //////////////////////////////////////////////	

	// Replicated variables
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly)
	float Health = 100.0f;

	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly)
	float InputX = 0.0f;
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly)
	float InputZ = 0.0f;
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly)
	float x = 0.0f;
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly)
	float z = 0.0f;

	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly)
	float Charge = 0.0f;
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly)
	bool bCharging = false;

	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadWrite)
	int Score = 0;


	// BLUEPRINT COMPONENTS ///////////////////////////////////////
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class USceneComponent* AttackScene = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UParticleSystemComponent* MoveParticles = nullptr;

	/*UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UParticleSystemComponent* AmbientParticles = nullptr;*/

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UAudioComponent* PlayerSound = nullptr;

	// Shield type
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class USphereComponent* ShieldCollider;

	// Player movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MoveSpeed = 1000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TurnSpeed = 1000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MoveFreshMultiplier = 250.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxMoveSpeed = 1000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MoveAccelerationSpeed = 1500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MoveFrictionGain = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RestFriction = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxFriction = 5.0f;

	// Camera movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraMoveSpeed = 0.68f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraVelocityChase = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraSoloVelocityChase = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraDistanceScalar = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraTiltClamp = 1.0f;

	// Charge properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ChargeMax = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ChargeGainSpeed = 1.0f;

	

};
