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

	UPROPERTY(EditDefaultsOnly)
	UTextRenderComponent* TextComponent;
	
	virtual void Tick(float DeltaSeconds) override;

	float AttackTimer = 0.0f;	/// stopwatch situation
	float AttackTimeout = 0.0f; /// generated at fire time
	float SecondaryTimer = 0.0f;
	float FrictionTimer = 0.0f;
	float LastMoveTime = 0.0f;
	float CurrentMoveTime = 0.0f;
	float ChargeFXTimer = 0.0f;
	float CameraTiltX = 0.0f;
	float CameraTiltZ = 0.0f;

	bool bOnTempo = false;
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

	UFUNCTION(BlueprintCallable)
	void NullifySecondary() { ActiveSecondary = nullptr; MoveParticles->bSuppressSpawning = false; }
	
	UFUNCTION(BlueprintCallable)
	void NullifyAttack() { ActiveAttack = nullptr; MoveParticles->bSuppressSpawning = false; }

	UFUNCTION(BlueprintCallable)
	void EraseCharge() { Charge = 0.0f; }

	UFUNCTION(BlueprintCallable)
	float GetCharge() { return Charge; }

	UFUNCTION(BlueprintCallable)
	float GetHealth() { return Health; }

	UFUNCTION(BlueprintCallable)
	float GetChargePercentage();

	UFUNCTION(BlueprintCallable)
	void ResetFlipbook();

	UFUNCTION(BlueprintCallable)
	void ClearFlash() 
	{ 
		if (ActiveFlash != nullptr)
		{
			ActiveFlash->Destroy();
			ActiveFlash = nullptr;
		}
	}

	// AI HELPER FUNCTIONS
	float GetMoveSpeed() { return MoveSpeed; }
	float GetTurnSpeed() { return TurnSpeed; }
	float GetMovesPerSec() { return MovesPerSecond; }
	float GetPrefireTime() { return PrefireTime; }
	USceneComponent* GetAttackScene() { return AttackScene; }
	FString GetCharacterName() { return CharacterName; }


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

	// Locator handling
	UFUNCTION(BlueprintCallable)
	void ResetLocator();

	void LocatorScaling();

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<AGFlash> FlashClass;
	class AGFlash* ActiveFlash = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<AGAttack> AttackClass;
	class AGAttack* ActiveAttack = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bMultipleAttacks = false;

	// Secondary object
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<AGAttack> SecondaryClass;
	class AGAttack* ActiveSecondary = nullptr;

	// Charge object
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<AActor> ChargeParticles;
	class AActor* ActiveChargeParticles = nullptr;

	// Boost object
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<AActor> BoostClass;

	// Replicated functions
	void Rematch();
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerRematch();

	public:
	void SetX(float Value);
	protected:
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerSetX(float Value);
	
	public:
	void SetZ(float Value);
	protected:
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerSetZ(float Value);

	void NewMoveKick();
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation) // see chrome
	void ServerNewMoveKick();

	void UpdateMoveParticles(FVector Move);
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerUpdateMoveParticles(FVector Move);

	void SetAim();
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerSetAim();

public:
	AActor* GetActiveFlash() { return ActiveFlash; }
	AActor* GetActiveSecondary() { return ActiveSecondary; }



	// REPLICATED COMBAT FUNCTIONS ////////////////////////////////
	void InitAttack();
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerInitAttack();

	void RaiseCharge();
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerRaiseCharge();

	void ReleaseAttack();
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerReleaseAttack();

	void FireSecondary();
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerFireSecondary();

	void PrefireTiming();
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerPrefireTiming();

	// Handoff functions to avoid net saturation
	void CheckPowerSlideOn();
	void CheckPowerSlideOff();

	void PowerSlideEngage();
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerPowerSlideEngage();

	void PowerSlideDisengage();
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerPowerSlideDisengage();

	UFUNCTION(BlueprintCallable)
	void ModifyHealth(float Value);
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerModifyHealth(float Value);


protected:
	// BLUEPRINT COMPONENTS ///////////////////////////////////////
	//
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class USceneComponent* AttackScene = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UPaperSpriteComponent* Locator = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UParticleSystemComponent* MoveParticles = nullptr;

	/*UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UParticleSystemComponent* AmbientParticles = nullptr;*/

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UAudioComponent* PlayerSound = nullptr;

	// Shield type
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class USphereComponent* OuterTouchCollider;



	// VARIABLES //////////////////////////////////////////////	
	//

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
	float MoveTimer = 0.0f;
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly)
	bool bMoved = false;
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly)
	bool bSliding = false;

	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly)
	float Charge = 0.0f;
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly)
	bool bCharging = false;
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly)
	float PrefireTimer = 0.0f;

	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadWrite)
	int Score = 0;

	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly)
	bool bSecondaryActive = false;

	// ATTRIBUTES //////////////////////////////////////////	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FString CharacterName = "Name";

	// Player movement
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float MoveSpeed = 1000.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float MovesPerSecond = 30.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float TurnSpeed = -0.1f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float MoveFreshMultiplier = 100.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float SlowmoMoveBoost = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float MaxMoveSpeed = 5000.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float MoveAccelerationSpeed = 1000.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float DecelerationSpeed = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float PowerSlideSpeed = 0.0f;

	// Camera movement
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float CameraMoveSpeed = 0.6f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float CameraVelocityChase = 1.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float CameraSoloVelocityChase = 1.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float CameraDistanceScalar = 1.3f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float CameraTiltValue = 10.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float CameraTiltSpeed = 0.36f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float CameraTiltClamp = 1.0f;

	// Charge properties
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float PrefireTime = 0.1f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ChargeMax = 4.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ChargeGain = 1.0f;

	

};
