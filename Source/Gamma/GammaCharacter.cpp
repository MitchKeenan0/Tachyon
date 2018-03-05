// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GammaCharacter.h"
#include "PaperFlipbookComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "PaperSpriteComponent.h"
#include "ParticleDefinitions.h"
#include "Particles/ParticleSystem.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Camera/CameraComponent.h"

DEFINE_LOG_CATEGORY_STATIC(SideScrollerCharacter, Log, All);

//////////////////////////////////////////////////////////////////////////
// AGammaCharacter

AGammaCharacter::AGammaCharacter()
{
	// Use only Yaw from the controller and ignore the rest of the rotation.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Set the size of our collision capsule.
	GetCapsuleComponent()->SetCapsuleHalfHeight(96.0f);
	GetCapsuleComponent()->SetCapsuleRadius(40.0f);

	// Create a camera boom attached to the root (capsule)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 500.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 75.0f);
	CameraBoom->bAbsoluteRotation = true;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->RelativeRotation = FRotator(0.0f, -90.0f, 0.0f);

	// Create an orthographic camera (no perspective) and attach it to the boom
	SideViewCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("SideViewCamera"));
	SideViewCameraComponent->ProjectionMode = ECameraProjectionMode::Orthographic;
	SideViewCameraComponent->OrthoWidth = 2048.0f;
	SideViewCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	// Prevent all automatic rotation behavior on the camera, character, and camera component
	CameraBoom->bAbsoluteRotation = true;
	SideViewCameraComponent->bUsePawnControlRotation = false;
	SideViewCameraComponent->bAutoActivate = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// Configure character movement
	GetCharacterMovement()->GravityScale = 0.0f;
	GetCharacterMovement()->AirControl = 0.80f;
	GetCharacterMovement()->JumpZVelocity = 1000.f;
	GetCharacterMovement()->GroundFriction = 3.0f;
	GetCharacterMovement()->MaxWalkSpeed = 600.0f;
	GetCharacterMovement()->MaxFlySpeed = 600.0f;

	// Lock character motion onto the XZ plane, so the character can't move in or out of the screen
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, -1.0f, 0.0f));

	// Behave like a traditional 2D platformer character, with a flat bottom instead of a curved capsule bottom
	// Note: This can cause a little floating when going up inclines; you can choose the tradeoff between better
	// behavior on the edge of a ledge versus inclines by setting this to true or false
	GetCharacterMovement()->bUseFlatBaseForFloorChecks = true;

    // 	TextComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IncarGear"));
    // 	TextComponent->SetRelativeScale3D(FVector(3.0f, 3.0f, 3.0f));
    // 	TextComponent->SetRelativeLocation(FVector(35.0f, 5.0f, 20.0f));
    // 	TextComponent->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
    // 	TextComponent->SetupAttachment(RootComponent);

	// Set up attack
	AttackScene = CreateDefaultSubobject<USceneComponent>(TEXT("AttackScene"));
	AttackScene->SetupAttachment(RootComponent);

	Locator = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Locator"));
	Locator->SetupAttachment(RootComponent);
	Locator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Locator->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
	Locator->bGenerateOverlapEvents = 0;

	MoveParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("MoveParticles"));
	MoveParticles->SetupAttachment(RootComponent);
	MoveParticles->bAutoActivate = false;

	/*AmbientParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("AmbientParticles"));
	AmbientParticles->SetupAttachment(RootComponent);*/

	PlayerSound = CreateDefaultSubobject<UAudioComponent>(TEXT("PlayerSound"));
	PlayerSound->SetIsReplicated(true);

	ShieldCollider = CreateDefaultSubobject<USphereComponent>(TEXT("WarheadCollider"));
	//WarheadCollider->SetSphereRadius(10.0f);
	ShieldCollider->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	ShieldCollider->OnComponentBeginOverlap.AddDynamic(this, &AGammaCharacter::OnShieldBeginOverlap);
	

	// Enable replication on the Sprite component so animations show up when networked
	GetSprite()->SetIsReplicated(true);
	bReplicates = true;

	// TextRender->SetNetAddressable();

	//GetCameraBoom()->TargetArmLength = 1000.0f;
}


//////////////////////////////////////////////////////////////////////////
// Input
void AGammaCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(InputComponent);
	PlayerInputComponent->BindAction("Attack", IE_Pressed, this, &AGammaCharacter::InitAttack);
	//PlayerInputComponent->BindAction("Attack", IE_Released, this, &AGammaCharacter::ReleaseAttack);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AGammaCharacter::RaiseCharge);
	PlayerInputComponent->BindAction("Secondary", IE_Pressed, this, &AGammaCharacter::FireSecondary);
	PlayerInputComponent->BindAction("Rematch", IE_Pressed, this, &AGammaCharacter::Rematch);

	PlayerInputComponent->BindAxis("MoveRight", this, &AGammaCharacter::MoveRight);
	PlayerInputComponent->BindAxis("MoveUp", this, &AGammaCharacter::MoveUp);
}


//////////////////////////////////////////////////////////////////////////
// Begin Play
void AGammaCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->MaxAcceleration = MoveAccelerationSpeed;
	GetCharacterMovement()->MaxFlySpeed = MaxMoveSpeed;

	// To reduce jitter
	UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(), TEXT("p.NetEnableMoveCombining 0")); /// is this needed?

	// Camera needs some movement to wake up
	if (Controller && Controller->IsLocalController())
	{
		AddMovementInput(FVector(0.0f, 0.0f, 1.0f), 1.0f);
		GetCharacterMovement()->AddImpulse(FVector::UpVector * 100.0f);
	}
	else
	{
		Locator->SetActive(false);
	}

	ResetLocator();

	// Init charge
	Charge = FMath::FloorToFloat(ChargeMax / 2);

	//GetCameraBoom()->TargetArmLength = 10000.0f;
}


/////////////////////////////////////////////////////////////////////////////////////
// MAIN CHARACTER UPDATE
void AGammaCharacter::UpdateCharacter(float DeltaTime)
{
	// Update prefire
	if ((GetActiveFlash() != nullptr) && GetActiveFlash()->IsValidLowLevel())
	{
		PrefireTiming();
	}

	// Update animation to match the motion
	//UpdateAnimation();

	UpdateCamera(DeltaTime);

	// Now setup the rotation of the controller based on the direction we are travelling
	const FVector PlayerVelocity = GetVelocity();
	float TravelDirection = InputX;
	
	// Set rotation so character faces direction of travel
	if (Controller != nullptr
		&& UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.5f)
	{
		if (TravelDirection < 0.0f)
		{
			Controller->SetControlRotation(FRotator(0.0, 180.0f, 0.0f));
		}
		else if (TravelDirection > 0.0f)
		{
			Controller->SetControlRotation(FRotator(0.0f, 0.0f, 0.0f));
		}

		// Locator scaling
		if (Controller->IsLocalController())
		{
			LocatorScaling();
		}
	}

	// Move timing and clamp
	if (GetCharacterMovement() && !bMoved) //  HasAuthority() && 
	{
		// Clamp velocity
		float TimeDilat = UGameplayStatics::GetGlobalTimeDilation(this->GetWorld());
		float TimeScalar = FMath::Abs(SlowmoMoveBoost * (1 / TimeDilat));
		GetCharacterMovement()->Velocity = 
			GetCharacterMovement()->Velocity.GetClampedToSize(0.0f, MaxMoveSpeed * MoveFreshMultiplier * TimeScalar);

		MoveTimer += GetWorld()->DeltaTimeSeconds;
	}
}


void AGammaCharacter::UpdateCamera(float DeltaTime)
{
	// Interpolating camera position to the centre of 2 framing points.
	// TODO - add defense on running get?
	
	// Poll for framing actors
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("FramingActor"), FramingActors);

	if (FramingActors.Num() > 0)
	{

		// Start by checking valid actor
		AActor* Actor1 = FramingActors[0];
		if (Actor1 && Actor1->IsValidLowLevelFast() && !Actor1->IsUnreachable())
		{

			// Framing up first actor
			FVector Actor1Velocity = Actor1->GetVelocity();
			Actor1Velocity.Z *= 0.9f; /// vertical kerning
			Actor1Velocity.X *= 0.9f; /// lateral kerning
			
			FVector LocalPos = Actor1->GetActorLocation() + (Actor1Velocity * CameraVelocityChase);
			PositionOne = FMath::VInterpTo(PositionOne, LocalPos, DeltaTime, CameraMoveSpeed);
			float CameraMaxDistance = 15555.5f;
			float CameraDistance = CameraDistanceScalar * 1.25f;

			// Position two may be another actor
			if (FramingActors.Num() > 1 && FramingActors[1])
			{
				AActor* Actor2 = FramingActors[1];
				if (Actor2 && !Actor2->IsUnreachable())
				{

					// Framing up with second actor
					FVector Actor2Velocity = Actor2->GetVelocity();
					Actor2Velocity.Z *= 0.9f;
					FVector PairFraming = Actor2->GetActorLocation() + (Actor2Velocity * CameraVelocityChase);
					PositionTwo = FMath::VInterpTo(PositionTwo, PairFraming, DeltaTime, CameraMoveSpeed);
				}
			}
			else if (FramingActors.Num() == 1)
			{

				// Framing lone player by their velocity
				FVector VelocityFraming = Actor1->GetActorLocation() + (Actor1->GetVelocity() * CameraSoloVelocityChase);
				PositionTwo = FMath::VInterpTo(PositionTwo, VelocityFraming, DeltaTime, CameraMoveSpeed);
				CameraMaxDistance = 100111.0f;
			}


			// Positions done
			// Find the midpoint
			Midpoint = (PositionOne + PositionTwo) / 2.0f;
			if (Midpoint.Size() > 0.001f)
			{
				// Back away to accommodate distance
				float DistBetweenActors = FVector::Dist(PositionOne, PositionTwo) + (300 / FramingActors.Num());
				float TargetLengthClamped = FMath::Clamp(FMath::Sqrt(DistBetweenActors * 300.f) * CameraDistance,
					1300.0f,
					CameraMaxDistance);
				float DesiredCameraDistance = FMath::FInterpTo(GetCameraBoom()->TargetArmLength, 
					TargetLengthClamped, DeltaTime, CameraMoveSpeed * 3.0f);
					
				// Camera tilt
				CameraTiltX = FMath::FInterpTo(CameraTiltX, InputZ * CameraTiltValue, DeltaTime, CameraTiltSpeed); // pitch
				CameraTiltZ = FMath::FInterpTo(CameraTiltZ, InputX * CameraTiltValue, DeltaTime, CameraTiltSpeed); // yaw
				SideViewCameraComponent->SetRelativeRotation(FRotator(CameraTiltX, CameraTiltZ, 0.0f) * CameraTiltValue);

				// Adjust position to work angle
				Midpoint.X -= (CameraTiltZ * (DesiredCameraDistance / 6));
				Midpoint.Z -= (CameraTiltX * (DesiredCameraDistance / 6));

				// Make it so
				CameraBoom->SetWorldLocation(Midpoint);
				CameraBoom->TargetArmLength = DesiredCameraDistance;
			}
		}
	}
}


void AGammaCharacter::ResetLocator()
{
	Locator->SetRelativeScale3D(FVector(25.0f, 25.0f, 25.0f));

	ClearFlash();
}


void AGammaCharacter::LocatorScaling()
{
	// Player locator
	if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.2f)
	{
		// Scaling down
		if (Locator->RelativeScale3D.Size() >= 0.01f)
		{
			FVector ShrinkingSize = Locator->RelativeScale3D * 0.93f;
			Locator->SetRelativeScale3D(ShrinkingSize);
		}
		else if (Locator->RelativeScale3D == FVector(25.0f, 25.0f, 25.0f)
			&& Locator->IsVisible())
		{
			Locator->SetVisibility(false);
		}
	}
	else
	{
		if (Locator->RelativeScale3D.Size() < 1.0f)
		{
			ResetLocator();
		}
		if (!Locator->IsVisible())
		{
			Locator->SetVisibility(true);
		}
	}
}


void AGammaCharacter::UpdateAnimation()
{
	//const FVector PlayerVelocity = GetVelocity();
	//const float PlayerSpeedSqr = PlayerVelocity.SizeSquared();

	//// Are we moving or standing still?
	//UPaperFlipbook* DesiredAnimation = (PlayerSpeedSqr > 0.0f) ? RunningAnimation : IdleAnimation;
	//if( GetSprite()->GetFlipbook() != DesiredAnimation 	)
	//{
	//	GetSprite()->SetFlipbook(DesiredAnimation);
	//}
}


float AGammaCharacter::GetChargePercentage()
{
	float Percentage = 0.0f;
	if (Charge > 0)
	{
		Percentage = Charge / ChargeMax;
	}
	return Percentage;
}


/////////////////////////////////////////////////////////////////////////
// TICK
void AGammaCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	// Main update
	UpdateCharacter(DeltaSeconds);


	// Update player pitch
	if (PlayerSound != nullptr)
	{
		float VectorScale = GetCharacterMovement()->Velocity.Size() * 0.001f;
		float Scalar = FMath::Clamp(VectorScale, 0.2f, 4.0f) * 0.5f;
		PlayerSound->SetPitchMultiplier(Scalar);
	}
}



//////////////////////////////////////////////////////////////////////
// MOVEMENT & FIGHTING

// MOVE	LEFT / RIGHT
void AGammaCharacter::MoveRight(float Value)
{
	// Adjust aim to reflect move
	if (InputX != Value 
		/// Ignoring no inputs
		&& !(InputX == 0.0f && Value == 0.0f))
	{
		SetX(Value);
	}

	if (MoveTimer >= (1 / MovesPerSecond))
	{
		FVector MoveInput = FVector(InputX, 0.0f, InputZ).GetSafeNormal();
		FVector CurrentV = GetMovementComponent()->Velocity.GetSafeNormal();
		
		// Move by dot product for skating effect
		if (MoveInput != FVector::ZeroVector)
		{
			float MoveByDot = 0.0f;
			float DotToInput = FVector::DotProduct(MoveInput, CurrentV);
			float AngleToInput = TurnSpeed * FMath::Abs(FMath::Clamp(FMath::Acos(DotToInput), -90.0f, 90.0f));
			MoveByDot = MoveSpeed + (AngleToInput * MoveSpeed);
			GetCharacterMovement()->MaxFlySpeed = MoveByDot / 3.0f;
			GetCharacterMovement()->MaxAcceleration = MoveByDot;
			AddMovementInput(FVector(1.0f, 0.0f, 0.0f), Value * MoveByDot);
		}
	}

	ForceNetUpdate();
}

// MOVE UP / DOWN
void AGammaCharacter::MoveUp(float Value)
{
	// Adjust aim to reflect move
	if (InputZ != Value 
		/// Ignoring no inputs
		&& !(InputZ == 0.0f && Value == 0.0f))
	{
		SetZ(Value);
	}

	// Abide moves per second
	if (MoveTimer >= (1 / MovesPerSecond))
	{
		FVector MoveInput = FVector(InputX, 0.0f, InputZ).GetSafeNormal();
		FVector CurrentV = (GetMovementComponent()->Velocity).GetSafeNormal();

		// Move by dot product for skating effect
		if (MoveInput != FVector::ZeroVector)
		{
			float MoveByDot = 0.0f;
			float DotToInput = FVector::DotProduct(CurrentV, MoveInput);
			float AngleToInput = TurnSpeed * FMath::Abs(FMath::Clamp(FMath::Acos(DotToInput), -90.0f, 90.0f));
			MoveByDot = MoveSpeed + (AngleToInput * MoveSpeed);
			GetCharacterMovement()->MaxFlySpeed = MoveByDot / 3.0f;
			GetCharacterMovement()->MaxAcceleration = MoveByDot;
			AddMovementInput(FVector(0.0f, 0.0f, 1.0f), Value * MoveByDot);
		}
	}

	ForceNetUpdate();
}

// SET X & Z SERVERSIDE
void AGammaCharacter::SetX(float Value)
{
	ServerSetX(Value);
}
void AGammaCharacter::ServerSetX_Implementation(float Value)
{
	InputX = Value;
	//bMoved = true;
}
bool AGammaCharacter::ServerSetX_Validate(float Value)
{
	return true;
}

void AGammaCharacter::SetZ(float Value)
{
	ServerSetZ(Value);
}
void AGammaCharacter::ServerSetZ_Implementation(float Value)
{
	InputZ = Value;
	//bMoved = true;
}
bool AGammaCharacter::ServerSetZ_Validate(float Value)
{
	return true;
}

// BOOST KICK HUSTLE DASH
void AGammaCharacter::NewMoveKick()
{
	if (Role == ROLE_AutonomousProxy) // < ROLE_Authority
	{
		ServerNewMoveKick();
		return;
	}

	if (UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) > 0.2f)
	{
		// Algo scaling for timescale & max velocity
		FVector MoveInputVector = FVector(InputX, 0.0f, InputZ);
		FVector CurrentVelocity = GetCharacterMovement()->Velocity;
		float TimeDelta = GetWorld()->DeltaTimeSeconds;
		float RelativityToMaxSpeed = (MaxMoveSpeed) - CurrentVelocity.Size();
		//float DotScalar = 1 / FMath::Abs(FVector::DotProduct(CurrentVelocity.GetSafeNormal(), MoveInputVector));

		// Force, clamp, & effect chara movement
		FVector KickVector = MoveInputVector
			* MoveFreshMultiplier
			* RelativityToMaxSpeed;
			//* DotScalar;
		//KickVector = KickVector.GetClampedToSize(0.0f, MaxMoveSpeed);
		GetCharacterMovement()->AddImpulse(KickVector * TimeDelta);

		bMoved = false;
		MoveTimer = 0.0f;

		// Spawn visuals
		FActorSpawnParameters SpawnParams;
		FVector PlayerVelocity = GetCharacterMovement()->Velocity;
		FRotator PlayerVelRotator = PlayerVelocity.Rotation();
		FRotator InputRotator = MoveInputVector.Rotation();

		PlayerVelocity.Z *= 0.01f;
		FVector SpawnLocation = GetActorLocation(); /// + (PlayerVelocity / 3);
		
		GetWorld()->SpawnActor<AActor>
			(BoostClass, SpawnLocation, InputRotator, SpawnParams); /// PlayerVelRotator
		
		ForceNetUpdate();

		//GEngine->AddOnScreenDebugMessage(-1, 10.5f, FColor::Cyan, FString::Printf(TEXT("dot  %f"), DotScalar));
		//GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan, FString::Printf(TEXT("mass  %f"), GetCharacterMovement()->Mass));
		//GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan, FString::Printf(TEXT("vel  %f"), (GetCharacterMovement()->Velocity.Size())));
		//GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan, FString::Printf(TEXT("time scalar   %f"), TimeScalar));
	}
}
void AGammaCharacter::ServerNewMoveKick_Implementation()
{
	NewMoveKick();
}
bool AGammaCharacter::ServerNewMoveKick_Validate()
{
	return true;
}

// BOOST PARTICLES
void AGammaCharacter::UpdateMoveParticles(FVector Move)
{
	// Direct particles on vector of movement
	FVector TempVec = Move;
	FRotator RecoilRotation = TempVec.Rotation();
	if (MoveParticles)
	{
		MoveParticles->SetWorldRotation(RecoilRotation);
		MoveParticles->Activate();
	}

	if (Role < ROLE_Authority) // (Role < ROLE_Authority) (Role == ROLE_AutonomousProxy)
	{
		ServerUpdateMoveParticles(Move);
	}
}
void AGammaCharacter::ServerUpdateMoveParticles_Implementation(FVector Move)
{
	UpdateMoveParticles(Move);
}
bool AGammaCharacter::ServerUpdateMoveParticles_Validate(FVector Move)
{
	return true;
}

// AIM
void AGammaCharacter::SetAim()
{
	// Fresh move 'kick it'
	FVector CurrentMove = FVector(InputX, 0.0f, InputZ);
	if ((CurrentMove != PrevMoveInput) 
		&& (CurrentMove != FVector::ZeroVector))
	{
		// Tempo timing
		CurrentMoveTime = GetWorld()->TimeSeconds;
		float TimeDelta = (CurrentMoveTime - LastMoveTime);

		// Respect tempo
		if (TimeDelta > (0.1f * UGameplayStatics::GetGlobalTimeDilation(GetWorld())))
		{
			//NewMoveKick();
			//UpdateMoveParticles(CurrentMove);

			if (PlayerSound != nullptr)
			{
				PlayerSound->SetPitchMultiplier(Charge);
				PlayerSound->Play();
			}
		}
	}

	LastMoveTime = CurrentMoveTime;
	PrevMoveInput = CurrentMove;
	//bNewMove = false;

	if (Role == ROLE_AutonomousProxy) //< ROLE_Authority
	{
		ServerSetAim();
	}
}
void AGammaCharacter::ServerSetAim_Implementation()
{
	SetAim();
}
bool AGammaCharacter::ServerSetAim_Validate()
{
	return true;
}

// RAISE CHARGE for attack 
void AGammaCharacter::RaiseCharge()
{
	if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.2f)
	{
		if (Charge < ChargeMax)
		{
			Charge += ChargeGain;
		}

		if (Role < ROLE_Authority)
		{
			ServerRaiseCharge();
			return;
		}

		// Move boost o.o
		if (FVector(InputX, 0.0f, InputZ) != FVector::ZeroVector)
		{
			NewMoveKick();
		}

		// Sound fx -.-
		if (PlayerSound != nullptr)
		{
			//PlayerSound->SetPitchMultiplier(Charge * 0.3f);
			PlayerSound->Play();
		}

		// Spawn charge vfx
		FActorSpawnParameters SpawnParams;
		ActiveChargeParticles = Cast<AActor>(GetWorld()->SpawnActor<AActor>(ChargeParticles, GetActorLocation(), GetActorRotation(), SpawnParams));
		ActiveChargeParticles->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

		//GEngine->AddOnScreenDebugMessage(-1, 0.68f, FColor::White, FString::Printf(TEXT("%f % CHARGE"), Charge), true, FVector2D::UnitVector * 2);


		//// Scale up charge vfx
		//float Scalar = Charge / ChargeMax;
		//ActiveChargeParticles->SetActorRelativeScale3D(ActiveChargeParticles->GetActorRelativeScale3D() * Scalar);
	}
}
void AGammaCharacter::ServerRaiseCharge_Implementation()
{
	RaiseCharge();
}
bool AGammaCharacter::ServerRaiseCharge_Validate()
{
	return true;
}

// PRE-ATTACK flash spawning
void AGammaCharacter::InitAttack()
{
	// Clean burn
	MoveParticles->bSuppressSpawning = true;

	if ((Charge > 0.0f) && FlashClass && (ActiveFlash == nullptr)
		&& (UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) > 0.2f))
	{
		// Clean up previous attack
		if (ActiveAttack && !bMultipleAttacks)
		{
			ActiveAttack->Destroy();
			ActiveAttack = nullptr;
		}

		// Direction & setting up
		FVector FirePosition = AttackScene->GetComponentLocation();
		FVector FireDirection = AttackScene->GetForwardVector();
		FRotator FireRotation = FireDirection.Rotation() + FRotator(21.0f * InputZ, 0.0f, 0.0f);

		// Spawning
		if (HasAuthority())
		{
			// Spawn eye heat
			FActorSpawnParameters SpawnParams;
			ActiveFlash = Cast<AGFlash>(GetWorld()->SpawnActor<AGFlash>(FlashClass, FirePosition, FireRotation, SpawnParams));
			ActiveFlash->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	if (Role < ROLE_Authority)
	{
		ServerInitAttack();
	}
}
void AGammaCharacter::ServerInitAttack_Implementation()
{
	InitAttack();
}
bool AGammaCharacter::ServerInitAttack_Validate()
{
	return true;
}

// ATTACK
void AGammaCharacter::ReleaseAttack()
{
	// Less-clean burn
	MoveParticles->bSuppressSpawning = false;

	if (AttackClass && (ActiveAttack == nullptr || bMultipleAttacks) && (Charge > 0.0f)
		&& (UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) > 0.2f))
	{
		// Clean up previous flash
		if (ActiveFlash != nullptr && ActiveFlash->IsValidLowLevel())
		{
			ActiveFlash->Destroy();
			ActiveFlash = nullptr;
		}
		if (ActiveChargeParticles != nullptr && ActiveChargeParticles->IsValidLowLevel())
		{
			ActiveChargeParticles->Destroy();
			ActiveChargeParticles = nullptr;
		}

		// Direction & setting up
		FVector FirePosition = AttackScene->GetComponentLocation();
		FVector LocalForward = AttackScene->GetForwardVector();
		FRotator FireRotation = LocalForward.Rotation() + FRotator(21.0f * InputZ, 0.0f, 0.0f);
		FActorSpawnParameters SpawnParams;

		// Spawning
		if (HasAuthority())
		{
			if (AttackClass != nullptr || bMultipleAttacks)
			{
				ActiveAttack = Cast<AGAttack>(GetWorld()->SpawnActor<AGAttack>(AttackClass, FirePosition, FireRotation, SpawnParams));
				if (ActiveAttack != nullptr)
				{
					ActiveAttack->InitAttack(this, 1, InputZ);

					if (ActiveAttack->LockedEmitPoint)
					{
						ActiveAttack->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform); // World
					}
				}
			}
			else
			{
				GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("No attack class to spawn"));
			}

			Charge -= ChargeGain;
		}
	}

	if (Role < ROLE_Authority)
	{
		ServerReleaseAttack();
	}
}
void AGammaCharacter::ServerReleaseAttack_Implementation()
{
	ReleaseAttack();
}
bool AGammaCharacter::ServerReleaseAttack_Validate()
{
	return true;
}

// SECONDARY
void AGammaCharacter::FireSecondary()
{
	if (SecondaryClass && (ActiveSecondary == nullptr)
		&& (UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) > 0.2f))
	{
		// Clean burn
		MoveParticles->bSuppressSpawning = true;

		// Direction & setting up
		FVector FirePosition = GetActorLocation();
		FVector LocalForward = AttackScene->GetForwardVector();
		FRotator FireRotation = LocalForward.Rotation() + FRotator(21.0f * InputZ, 0.0f, 0.0f);
		FActorSpawnParameters SpawnParams;

		// Spawning
		if (HasAuthority())
		{
			ActiveSecondary = GetWorld()->SpawnActor<AGAttack>(SecondaryClass, FirePosition, FireRotation, SpawnParams); //  Cast<AActor>());

			if (ActiveSecondary != nullptr)
			{
				//bSecondaryActive = true;

				AGAttack* PotentialAttack = Cast<AGAttack>(ActiveSecondary);
				if (PotentialAttack != nullptr)
				{
					PotentialAttack->InitAttack(this, 1, InputZ);

					if (PotentialAttack->LockedEmitPoint)
					{
						PotentialAttack->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
					}
				}
			}
		}
	}

	if (Role < ROLE_Authority)
	{
		ServerFireSecondary();
	}
}
void AGammaCharacter::ServerFireSecondary_Implementation()
{
	FireSecondary();
}
bool AGammaCharacter::ServerFireSecondary_Validate()
{
	return true;
}


// PREFIRE TIMING
void AGammaCharacter::PrefireTiming()
{
	if ((PrefireTimer >= PrefireTime)
		&& (Charge > 0))
	{
		PrefireTimer = 0.0f;
		ReleaseAttack();
	}
	else
	{
		PrefireTimer += GetWorld()->GetDeltaSeconds();
	}

	if (Role < ROLE_Authority)
	{
		ServerPrefireTiming();
	}
}
void AGammaCharacter::ServerPrefireTiming_Implementation()
{
	PrefireTiming();
}
bool AGammaCharacter::ServerPrefireTiming_Validate()
{
	return true;
}


// MODIFY HEALTH
void AGammaCharacter::ModifyHealth(float Value)
{
	Health = FMath::Clamp(Health + Value, 0.0f, 100.0f);

	if (Role < ROLE_Authority)
	{
		ServerModifyHealth(Value);
	}
}
void AGammaCharacter::ServerModifyHealth_Implementation(float Value)
{
	ModifyHealth(Value);
}
bool AGammaCharacter::ServerModifyHealth_Validate(float Value)
{
	return true;
}


// REMATCH
void AGammaCharacter::Rematch()
{
	if (Role < ROLE_Authority)
	{
		ServerRematch();
	}
}
void AGammaCharacter::ServerRematch_Implementation()
{
	Rematch();
}
bool AGammaCharacter::ServerRematch_Validate()
{
	return true;
}

// Shield Collision
void AGammaCharacter::OnShieldBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherComp->ComponentHasTag("Bridge"))
	{
		///GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, TEXT("oh look a bridge"));
	}
	
	// attack should check for shield to make reflect fx
}


void AGammaCharacter::ResetFlipbook()
{
	// Calcify HitActor
	UPaperFlipbookComponent* ActorFlipbook = Cast<UPaperFlipbookComponent>
		(this->FindComponentByClass<UPaperFlipbookComponent>());
	if (ActorFlipbook)
	{
		ActorFlipbook->SetPlaybackPositionInFrames(0, true);
		ActorFlipbook->SetPlayRate(1);
	}
}


////////////////////////////////////////////////////////////////////////
// NETWORKED PROPERTY REPLICATION
void AGammaCharacter::GetLifetimeReplicatedProps(TArray <FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGammaCharacter, Health);
	DOREPLIFETIME(AGammaCharacter, Score);

	DOREPLIFETIME(AGammaCharacter, InputX);
	DOREPLIFETIME(AGammaCharacter, InputZ);
	DOREPLIFETIME(AGammaCharacter, x);
	DOREPLIFETIME(AGammaCharacter, z);
	DOREPLIFETIME(AGammaCharacter, MoveTimer);

	DOREPLIFETIME(AGammaCharacter, Charge);
	DOREPLIFETIME(AGammaCharacter, bCharging);
}


// Print to screen
// GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("sizing"));
// GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Charge: %f"), Charge));