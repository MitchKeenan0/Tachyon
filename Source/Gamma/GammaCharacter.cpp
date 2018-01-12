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

	MoveParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("MoveParticles"));
	MoveParticles->SetupAttachment(RootComponent);
	MoveParticles->bAutoActivate = false;

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
}


//////////////////////////////////////////////////////////////////////////
// Input

void AGammaCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(InputComponent);
	PlayerInputComponent->BindAction("Attack", IE_Pressed, this, &AGammaCharacter::InitAttack);
	PlayerInputComponent->BindAction("Attack", IE_Released, this, &AGammaCharacter::ReleaseAttack);

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
}


/////////////////////////////////////////////////////////////////////////////////////
// MAIN CHARACTER UPDATE
void AGammaCharacter::UpdateCharacter(float DeltaTime)
{
	// Update animation to match the motion
	//UpdateAnimation();

	// Camera update
	UpdateCamera(DeltaTime);

	// Now setup the rotation of the controller based on the direction we are travelling
	const FVector PlayerVelocity = GetVelocity();
	float TravelDirection = InputX;
	
	// Set the rotation so that the character faces his direction of travel.
	if (Controller != nullptr)
	{
		if (TravelDirection < 0.0f)
		{
			Controller->SetControlRotation(FRotator(0.0, 180.0f, 0.0f));
		}
		else if (TravelDirection > 0.0f)
		{
			Controller->SetControlRotation(FRotator(0.0f, 0.0f, 0.0f));
		}

		// Aiming
		/* TRY THIS, FRAVE -- Check if Local
		if (GEngine->GetGamePlayer(GetWorld(), 0)->PlayerController
					== Cast<APlayerController>(TempChar->GetController()))
		*/
		/*if (InputComponent && HasAuthority())
		{
			float HorizontalInput = InputComponent->GetAxisValue(TEXT("MoveRight"));
			float VerticalInput = InputComponent->GetAxisValue(TEXT("MoveUp"));
			SetAim(HorizontalInput, VerticalInput);
		}*/
		
	}

	// Attack stuff
	if (ActiveAttack != nullptr && ActiveAttack->GetLethal())
	{
		// Wipe pointer if attack is gone or duration elapsed
		AttackTimer += DeltaTime;
		if (!ActiveAttack->IsValidLowLevel() || (AttackTimer > AttackTimeout))
		{
			ActiveAttack = nullptr;
			AttackTimer = 0.0f;
			//SweepingYScale = 0.0f;
		}
	}
}


void AGammaCharacter::UpdateCamera(float DeltaTime)
{
	// Framing for 2
	if (FramingActors.Num() < 2)
	{
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("FramingActor"), FramingActors);
	}
	if (FramingActors[0] != nullptr)
	{
		AActor* Actor1 = FramingActors[0];
		FVector Actor1Velocity = Actor1->GetVelocity();
		Actor1Velocity.Z *= 0.5f;
		AActor* Actor2 = nullptr; /// no guarantee
		FVector LocalPos = Actor1->GetActorLocation() + (Actor1Velocity * CameraVelocityChase);
		PositionOne = FMath::VInterpTo(PositionOne, LocalPos, DeltaTime, CameraMoveSpeed);
		float CameraTilt = 0.0f;
		float CameraMaxDistance = 5555.5f;
		float CameraDistance = CameraDistanceScalar;
		
		// Prepare to locate centre of either 2 scenarios:
		if (FramingActors.Num() > 1)
		{
			Actor2 = FramingActors[1];
			FVector Actor2Velocity = Actor2->GetVelocity();
			Actor2Velocity.Z *= 0.5f;
			FVector PairFraming = Actor2->GetActorLocation() + (Actor2Velocity * CameraVelocityChase);
			PositionTwo = FMath::VInterpTo(PositionTwo, PairFraming, DeltaTime, CameraMoveSpeed);

			// Compare velocities for cam tilting
			FVector Vel1 = Actor1->GetVelocity();
			FVector Vel2 = Actor2->GetVelocity();
			float Difference = Vel1.Size() - Vel2.Size();

			// Clamp and set camera tilt
			CameraTilt = FMath::Clamp(Difference, -CameraTiltClamp, CameraTiltClamp);
			CameraTilt = FMath::FInterpTo(SideViewCameraComponent->GetComponentRotation().Roll, CameraTilt, DeltaTime, 3.0f);
			SideViewCameraComponent->SetRelativeRotation(FRotator(0.0f, 0.0f, CameraTilt));

			// TO DO: ^^ camera seems to tilt one direction for each player...
		}
		else if (FramingActors.Num() == 1)
		{
			FVector VelocityFraming = Actor1->GetActorLocation() + (Actor1->GetVelocity() * CameraSoloVelocityChase);
			PositionTwo = FMath::VInterpTo(PositionTwo, VelocityFraming, DeltaTime, CameraMoveSpeed);
			CameraDistance *= (GetVelocity().Size() / 500.0f);
			CameraMaxDistance = 10111.0f;
		}

		// Set the midpoint
		Midpoint = (PositionOne + PositionTwo) / 2.0f;
		if (Midpoint.Size() > 1.0f)
		{
			// Back away to accommodate distance
			float DistBetweenActors = FVector::Dist(PositionOne, PositionTwo) + (300 / FramingActors.Num());
			float DesiredCameraDistance = FMath::Clamp(FMath::Sqrt(DistBetweenActors * 300.f) * CameraDistance,
														300.0f,
														CameraMaxDistance);

			// Make it so
			CameraBoom->SetWorldLocation(Midpoint);
			CameraBoom->TargetArmLength = DesiredCameraDistance;
		}
	}
}


void AGammaCharacter::UpdateAnimation()
{
	const FVector PlayerVelocity = GetVelocity();
	const float PlayerSpeedSqr = PlayerVelocity.SizeSquared();

	//// Are we moving or standing still?
	//UPaperFlipbook* DesiredAnimation = (PlayerSpeedSqr > 0.0f) ? RunningAnimation : IdleAnimation;
	//if( GetSprite()->GetFlipbook() != DesiredAnimation 	)
	//{
	//	GetSprite()->SetFlipbook(DesiredAnimation);
	//}
}


/////////////////////////////////////////////////////////////////////////
// TICK
void AGammaCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateCharacter(DeltaSeconds);

	// Aiming
	SetAim(0.f, 0.f);

	// Charging
	if (ActiveFlash != nullptr)
	{
		RaiseCharge(DeltaSeconds);
	}
	else if (Charge != 0.0f)
	{
		Charge = 0.0f;
	}

	// Tempo
	if (bOnTempo)
	{
		BPMTimer += DeltaSeconds;
	}

}



//////////////////////////////////////////////////////////////////////
// MOVEMENT & FIGHTING

// MOVE
void AGammaCharacter::MoveRight(float Value)
{
	AddMovementInput(FVector(1.0f, 0.0f, 0.0f), Value);
	if (InputX != Value)
	{
		SetX(Value);
	}
}
void AGammaCharacter::MoveUp(float Value)
{
	AddMovementInput(FVector(0.0f, 0.0f, 1.0f), Value);
	if (InputZ != Value)
	{
		SetZ(Value);
	}
}

// SET X & Z
void AGammaCharacter::SetX(float Value)
{
	/*InputX = Value;

	if (Role < ROLE_Authority)
	{
		ServerSetX(Value);
	}*/
	ServerSetX(Value);
}
void AGammaCharacter::ServerSetX_Implementation(float Value)
{
	//SetX(Value);
	InputX = Value;
}
bool AGammaCharacter::ServerSetX_Validate(float Value)
{
	return true;
}

void AGammaCharacter::SetZ(float Value)
{
	/*InputZ = Value;
	
	if (Role < ROLE_Authority)
	{
		ServerSetZ(Value);
	}*/
	ServerSetZ(Value);
}
void AGammaCharacter::ServerSetZ_Implementation(float Value)
{
	//SetZ(Value);
	InputZ = Value;
}
bool AGammaCharacter::ServerSetZ_Validate(float Value)
{
	return true;
}

// MOVE-KICK on fresh input
void AGammaCharacter::NewMoveKick()
{
	// Add fresh velocity and clamp to max move speed
	GetCharacterMovement()->AddForce(FVector(InputX, 0.0f, InputZ) * MoveFreshMultiplier);
	GetCharacterMovement()->Velocity = GetCharacterMovement()->Velocity.GetClampedToMaxSize(MaxMoveSpeed);

	if (Role == ROLE_AutonomousProxy) // (Role < ROLE_Authority)
	{
		ServerNewMoveKick();
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

// KICK-PARTICLES
void AGammaCharacter::UpdateMoveParticles(FVector Move)
{
	// Direct particles away from vector of movement
	//Move.X = GetActorScale().X * -1;
	FRotator RecoilRotation = (Move).Rotation();
	MoveParticles->SetWorldRotation(RecoilRotation);
	MoveParticles->Activate();

	if (Role == ROLE_AutonomousProxy) // (Role < ROLE_Authority)
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

// AIM replicated
void AGammaCharacter::SetAim(float x, float z)
{
	// Fresh move 'kick it'
	FVector CurrentMove = FVector(InputX, 0.0f, InputZ);
	if ((CurrentMove != PrevMoveInput) 
		&& (CurrentMove != FVector::ZeroVector))
	{
		// Respect tempo
		CurrentMoveTime = GetWorld()->TimeSeconds;
		float TimeDelta = (CurrentMoveTime - LastMoveTime);

		if ((TimeDelta <= 0.314f) && (TimeDelta >= 0.11f))
		{
			NewMoveKick();
			UpdateMoveParticles(CurrentMove);
			PlayerSound->SetPitchMultiplier(TimeDelta);
			PlayerSound->Play();
			///GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::White, TEXT("O N  T E M P O"));
		}
	}

	LastMoveTime = CurrentMoveTime;
	PrevMoveInput = CurrentMove;

	if (Role == ROLE_AutonomousProxy) // (Role < ROLE_Authority)
	{
		ServerSetAim(x, z);
	}
}
void AGammaCharacter::ServerSetAim_Implementation(float x, float z)
{
	SetAim(x, z);
}
bool AGammaCharacter::ServerSetAim_Validate(float x, float z)
{
	return true;
}

// RAISE CHARGE for attack 
void AGammaCharacter::RaiseCharge(float DeltaTime)
{
	if (Charge < ChargeMax)
	{
		Charge += (ChargeGainSpeed * DeltaTime);
		if (bFullCharge)
		{
			bFullCharge = false;
		}
	}
	else if (!bFullCharge)
	{
		bFullCharge = true;
		//PlayerSound->Play();
	}

	if (Role < ROLE_Authority)
	{
		ServerRaiseCharge(DeltaTime);
	}
}
void AGammaCharacter::ServerRaiseCharge_Implementation(float DeltaTime)
{
	RaiseCharge(DeltaTime);
}
bool AGammaCharacter::ServerRaiseCharge_Validate(float DeltaTime)
{
	return true;
}

// PRE-ATTACK flash spawning
void AGammaCharacter::InitAttack()
{
	if (FlashClass && ActiveFlash == nullptr)
	{
		// Clean up previous attack
		if (ActiveAttack)
		{
			ActiveAttack->Destroy();
			ActiveAttack = nullptr;
		}

		// Direction & setting up
		FVector FirePosition = AttackScene->GetComponentLocation();
		FVector FireDirection = AttackScene->GetForwardVector();
		FRotator FireRotation = FireDirection.Rotation() + FRotator(21.0f * InputZ, 0.0f, 0.0f);
		FActorSpawnParameters SpawnParams;

		// Spawning
		if (HasAuthority())
		{
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
	if (Charge > 0.0f && AttackClass && ActiveAttack == nullptr)
	{
		// Clean up previous flash
		if (ActiveFlash)
		{
			ActiveFlash->Destroy();
			ActiveFlash = nullptr;
		}

		// Direction & setting up
		FVector FirePosition = AttackScene->GetComponentLocation();
		FVector LocalForward = AttackScene->GetForwardVector();
		FRotator FireRotation = LocalForward.Rotation() + FRotator(21.0f * InputZ, 0.0f, 0.0f);
		FActorSpawnParameters SpawnParams;

		// Spawning
		if (HasAuthority())
		{
			if (AttackClass != nullptr)
			{
				ActiveAttack = Cast<AGAttack>(GetWorld()->SpawnActor<AGAttack>(AttackClass, FirePosition, FireRotation, SpawnParams));
				ActiveAttack->InitAttack(this, Charge, InputZ);
				
				if (ActiveAttack->LockedEmitPoint)
				{
					ActiveAttack->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform); // World
				}
			}
			else
			{
				GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("No attack class to spawn"));
			}
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


// Shield Collision
void AGammaCharacter::OnShieldBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherComp->ComponentHasTag("Bridge"))
	{
		///GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, TEXT("oh look a bridge"));
	}
	
	// attack should check for shield to make reflect fx
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

	DOREPLIFETIME(AGammaCharacter, Charge);
	DOREPLIFETIME(AGammaCharacter, bCharging);
}


// Print to screen
// GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("sizing"));
// GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Charge: %f"), Charge));