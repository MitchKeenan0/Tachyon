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
	GetCharacterMovement()->bUseFlatBaseForFloorChecks = false;

    TextComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TextComponent"));
    TextComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
    TextComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -75.0f));
    TextComponent->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
    TextComponent->SetupAttachment(RootComponent);
	TextComponent->Text = FText::FromString(CharacterName);
	TextComponent->SetAbsolute(false, true, false);

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

	OuterTouchCollider = CreateDefaultSubobject<USphereComponent>(TEXT("WarheadCollider"));
	//WarheadCollider->SetSphereRadius(10.0f);
	OuterTouchCollider->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	OuterTouchCollider->OnComponentBeginOverlap.AddDynamic(this, &AGammaCharacter::OnShieldBeginOverlap);
	

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
	PlayerInputComponent->BindAction("Attack", IE_Released, this, &AGammaCharacter::ReleaseAttack);
	PlayerInputComponent->BindAction("PowerSlide", IE_Pressed, this, &AGammaCharacter::CheckPowerSlideOn);
	PlayerInputComponent->BindAction("PowerSlide", IE_Released, this, &AGammaCharacter::CheckPowerSlideOff);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AGammaCharacter::RaiseCharge);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AGammaCharacter::DisengageKick);
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
	DecelerationSpeed = GetCharacterMovement()->BrakingFrictionFactor;

	// To reduce jitter
	UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(), TEXT("p.NetEnableMoveCombining 0")); /// is this needed?

	// Camera needs some movement to wake up
	if (Controller && Controller->IsLocalController())
	{
		AddMovementInput(FVector(0.0f, 0.0f, 1.0f), 100.0f);
		GetCharacterMovement()->AddImpulse(FVector::UpVector * 100.0f);
	}
	else
	{
		Locator->SetActive(false);
	}

	ResetLocator();
	TextComponent->SetText(FText::FromString(CharacterName));

	// Init charge
	Charge = FMath::FloorToFloat(ChargeMax / 2.0f);
	// Init health
	Health = MaxHealth;

	// Init camera
	FVector PlayerLocation = GetActorLocation();
	PlayerLocation.Y = 0.0f;
	CameraBoom->SetRelativeLocation(PlayerLocation);
	PositionOne = PlayerLocation;
	PositionTwo = PlayerLocation;
	CameraBoom->TargetArmLength = 30000.0f;

	// Init location (obstacles can currently deset players)
	FVector ActorLoc = GetActorLocation();
	ActorLoc.Y = 0.0f;
	SetActorLocation(ActorLoc);
}


/////////////////////////////////////////////////////////////////////////////////////
// MAIN CHARACTER UPDATE
void AGammaCharacter::UpdateCharacter(float DeltaTime)
{

	// Update animation to match the motion
	//UpdateAnimation();

	// CAMERA UPDATE
	if (!ActorHasTag("Bot"))
	{
		UpdateCamera(DeltaTime);
	}
	// One-time disable camera on Bots
	if (ActorHasTag("Bot") && GetCameraBoom()->IsActive())
	{
		GetCameraBoom()->Deactivate();
	}
	
	

	if (Controller != nullptr)
	{

		// Personal Timescale
		float MyTimeDilation = CustomTimeDilation;
		
		// Attacks and Secondary Recovery
		if (MyTimeDilation < 1.0f)
		{
			if (GetActiveFlash() != nullptr)
			{
				GetActiveFlash()->CustomTimeDilation = MyTimeDilation;
				GetActiveFlash()->SetLifeSpan(GetActiveFlash()->GetLifeSpan() / MyTimeDilation);
			}
			if (ActiveAttack != nullptr)
			{
				ActiveAttack->CustomTimeDilation = MyTimeDilation;
				ActiveAttack->SetLifeSpan(ActiveAttack->GetLifeSpan() / CustomTimeDilation);
			}
			if (ActiveSecondary != nullptr)
			{
				ActiveSecondary->CustomTimeDilation = MyTimeDilation;
				ActiveSecondary->SetLifeSpan(ActiveSecondary->GetLifeSpan() / CustomTimeDilation);
			}
		}

		// Personal Recovery
		float GlobalTimeDil = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
		if (GlobalTimeDil > 0.3f)
		{
			float t = (FMath::Square(MyTimeDilation) * 100.0f) * DeltaTime;
			float ReturnTime = FMath::FInterpConstantTo(MyTimeDilation, 1.0f, DeltaTime, 2.6f); // t or DeltaTime
			CustomTimeDilation = FMath::Clamp(ReturnTime, 0.01f, 1.0f);
			///GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::White, FString::Printf(TEXT("t: %f"), t));
		}


		// Set rotation so character faces direction of travel
		float TravelDirection = FMath::Clamp(InputX, -1.0f, 1.0f);
		if (TravelDirection < 0.0f)
		{
			FRotator Fint = FMath::RInterpTo(Controller->GetControlRotation(), FRotator(0.0, 180.0f, 0.0f), DeltaTime, 20.0f);
			Controller->SetControlRotation(Fint);
		}
		else if (TravelDirection > 0.0f)
		{
			FRotator Fint = FMath::RInterpTo(Controller->GetControlRotation(), FRotator(0.0f, 0.0f, 0.0f), DeltaTime, 20.0f);
			Controller->SetControlRotation(Fint);
		}
		else
		{
			// No Input - finish rotation
			if (Controller->GetControlRotation().Yaw > 90.0f)
			{
				FRotator Fint = FMath::RInterpTo(Controller->GetControlRotation(), FRotator(0.0, 180.0f, 0.0f), DeltaTime, 20.0f);
				Controller->SetControlRotation(Fint);
			}
			else
			{
				FRotator Fint = FMath::RInterpTo(Controller->GetControlRotation(), FRotator(0.0f, 0.0f, 0.0f), DeltaTime, 20.0f);
				Controller->SetControlRotation(Fint);
			}
		}


		// Locator scaling
		if (Controller->IsLocalController()
			&& UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.3f)
		{
			LocatorScaling();
		}

		// AfterImage rotation
		
	}

	// Move timing and clamp
	if (GetCharacterMovement() && !bMoved) //  HasAuthority() && 
	{
		// Clamp velocity
		/*float TimeDilat = UGameplayStatics::GetGlobalTimeDilation(this->GetWorld());
		float TimeScalar = FMath::Abs(SlowmoMoveBoost * (1 / TimeDilat));
		GetCharacterMovement()->Velocity = 
			GetCharacterMovement()->Velocity.GetClampedToSize(0.0f, MaxMoveSpeed * MoveFreshMultiplier * TimeScalar);*/

		MoveTimer += GetWorld()->DeltaTimeSeconds;
	}
}



// UPDATE CAMERA
void AGammaCharacter::UpdateCamera(float DeltaTime)
{
	// Start by checking valid actor
	AActor* Actor1 = nullptr;
	AActor* Actor2 = nullptr;

	float VelocityCameraSpeed = CameraMoveSpeed;
	
	if (!this->ActorHasTag("Spectator"))
	{
		Actor1 = this;
	}


	// Poll for framing actors
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("FramingActor"), FramingActors);
	/// TODO - add defense on running get?
	if (FramingActors.Num() > 1)
	{
		// Edge case: player is spectator, find a sub
		if (Actor1 == nullptr)
		{
			for (int i = 0; i < FramingActors.Num(); ++i)
			{
				if (FramingActors[i] != nullptr
					&& FramingActors[i] != this
					&& !FramingActors[i]->ActorHasTag("Spectator")
					&& !FramingActors[i]->ActorHasTag("Land"))
				{
					Actor1 = FramingActors[i];
					break;
				}
			}
		}

		// Let's go
		if (Actor1 != nullptr && Actor1->IsValidLowLevelFast() && !Actor1->IsUnreachable())
		{
			float UnDilatedDeltaTime = (DeltaTime / CustomTimeDilation) * CameraMoveSpeed; /// UGameplayStatics::GetGlobalTimeDilation(GetWorld())
			float TimeDilationScalar = (1.0f / CustomTimeDilation) + 0.01f; /// UGameplayStatics::GetGlobalTimeDilation(GetWorld())
			float TimeDilationScalarClamped = FMath::Clamp(TimeDilationScalar, 0.5f, 1.5f);

			// Framing up first actor
			FVector Actor1Velocity = Actor1->GetVelocity();

			// Set Velocity Camera Move Speed
			VelocityCameraSpeed *= (1 + (Actor1Velocity.Size() / 30000.0f));
			
			FVector LocalPos = Actor1->GetActorLocation() + (Actor1Velocity * CameraVelocityChase); // * TimeDilationScalarClamped
			PositionOne = FMath::VInterpTo(PositionOne, LocalPos, DeltaTime, VelocityCameraSpeed);
			float CameraMinimumDistance = 1000.0f;
			float CameraMaxDistance = 50000.0f;

			// Position two by another actor
			bool bAlone = false;
			if (FramingActors.Num() > 1)
			{
				// Find closest best candidate for Actor 2
				if (Actor2 == nullptr
					|| FVector::Dist(Actor2->GetActorLocation(), GetActorLocation()) > 5000.0f)
				{

					// Try player targets first
					UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), FramingActors);
					if (FramingActors.Num() > 1)
					{
						for (int i = 0; i < FramingActors.Num(); ++i)
						{
							if (FramingActors[i] != nullptr
								&& FramingActors[i] != this
								&& FramingActors[i] != Actor1
								&& !FramingActors[i]->ActorHasTag("Spectator"))
							{
								Actor2 = FramingActors[i];
							}
						}
					}
					else
					{
						UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("FramingActor"), FramingActors);
					}
				}

				// Update target Actor by nominating closest
				if (FramingActors.Num() > 1)
				{
					float DistToActor2 = 99999.0f;
					for (int i = 0; i < FramingActors.Num(); ++i)
					{
						if (FramingActors[i] != nullptr
							&& FramingActors[i] != this
							&& FramingActors[i] != Actor1
							&& !FramingActors[i]->ActorHasTag("Spectator"))
						{
							float DistToTemp = FVector::Dist(FramingActors[i]->GetActorLocation(), GetActorLocation());
							if (DistToTemp < DistToActor2)
							{
								Actor2 = FramingActors[i];
								DistToActor2 = DistToTemp;
							}
						}
					}
				}

				// If Actor2 isn't too far away, make 'Pair Framing'
				if (Actor2 != nullptr && !Actor2->IsUnreachable()
					&& FVector::Dist(Actor1->GetActorLocation(), Actor2->GetActorLocation()) <= 4200.0f)
				{

					// Framing up with second actor
					FVector Actor2Velocity = Actor2->GetVelocity();
					Actor2Velocity = Actor2Velocity.GetClampedToMaxSize(5000.0f * (CustomTimeDilation + 0.5f));
					Actor2Velocity.Z *= 0.15f;

					// Declare Position Two
					FVector PairFraming = Actor2->GetActorLocation() + (Actor2Velocity * CameraVelocityChase); // * TimeDilationScalarClamped
					PositionTwo = FMath::VInterpTo(PositionTwo, PairFraming, UnDilatedDeltaTime, VelocityCameraSpeed);
				}
				else
				{
					bAlone = true;
				}
			}
			
			// Lone player gets 'Velocity Framing'
			if (bAlone || FramingActors.Num() == 1)
			{
				
				// Framing lone player by their velocity
				FVector Actor1Velocity = (Actor1->GetVelocity() * CameraSoloVelocityChase) * 3.0f;

				// Clamp to max size
				Actor1Velocity = Actor1Velocity.GetClampedToMaxSize(3000.0f * (CustomTimeDilation + 0.5f));
				Actor1Velocity.Z *= 0.75f;

				// Declare Position Two
				FVector VelocityFraming = Actor1->GetActorLocation() + (Actor1Velocity);
				PositionTwo = FMath::VInterpTo(PositionTwo, VelocityFraming, UnDilatedDeltaTime, VelocityCameraSpeed); // UnDilatedDeltaTime
				
				// Distance controls
				CameraMaxDistance = 11000.0f;

				/// debug Velocity size
				/*GEngine->AddOnScreenDebugMessage(-1, 0.f,
					FColor::White, FString::Printf(TEXT("Actor1 Vel Size: %f"), (Actor1Velocity * 3.0f).Size()));*/
			}


			// Positions done
			// Find the midpoint, leaning to actor one
			FVector TargetMidpoint = PositionOne + ((PositionTwo - PositionOne) * FMath::Clamp(Actor1->CustomTimeDilation, 0.1f, 0.4f));
			Midpoint = FMath::VInterpTo(Midpoint, TargetMidpoint, UnDilatedDeltaTime, VelocityCameraSpeed);
			if (Midpoint.Size() > 0.001f)
			{

				// Distance
				float DistBetweenActors = FVector::Dist(PositionOne, PositionTwo);
				float VerticalDist = FMath::Abs((PositionTwo - PositionOne).Z);
				// If paired, widescreen edges are vulnerable to overshoot
				if (!bAlone)
				{
					VerticalDist *= 9.0f;
				}

				// Handle horizontal bias
				float DistancePreClamp = (DistBetweenActors + (FMath::Sqrt(VerticalDist) * 155.0f)) * 1.15f;
				///GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::White, FString::Printf(TEXT("DistancePreClamp: %f"), DistancePreClamp));
				float TargetLength = FMath::Clamp(DistancePreClamp, CameraMinimumDistance, CameraMaxDistance);

				// Modifier for prefire timing
				if (PrefireTimer > 0.0f)
				{
					float PrefireScalarRaw = FMath::Sqrt(PrefireTimer * 0.618f) / (PrefireTime * 2.0f);
					float PrefireScalarClamped = FMath::Clamp(PrefireScalarRaw, 0.01f, 0.99f);
					TargetLength *= (1 - PrefireScalarClamped);
				}

				// Modifier for hit/gg
				float Timescale = (Actor1->CustomTimeDilation + Actor2->CustomTimeDilation) / 2.0f;
				if (Timescale < 1.0f)
				{
					float HitTimeScalar = FMath::Clamp(FMath::Square(Timescale), 0.55f, 1.0f);
					TargetLength *= HitTimeScalar;
				}

				// Last modifier for global time dilation
				float GlobalTimeScale = UGameplayStatics::GetGlobalTimeDilation(GetWorld()) * 2.1f;
				float RefinedGScalar = FMath::Clamp(GlobalTimeScale, 0.21f, 1.0f);
				TargetLength *= RefinedGScalar;

				// Clamp useable distance
				float TargetLengthClamped = FMath::Clamp(FMath::Sqrt(TargetLength * 1200.0f) * CameraDistanceScalar,
					CameraMinimumDistance * RefinedGScalar,
					CameraMaxDistance);

				// Set Camera Distance
				float DesiredCameraDistance = FMath::FInterpTo(GetCameraBoom()->TargetArmLength,
					TargetLengthClamped, DeltaTime, (VelocityCameraSpeed * 1.15f));
					
				// Camera tilt
				float TiltDistanceScalar = FMath::Clamp((1.0f / DistBetweenActors) * 100.0f, 0.1f, 0.5f);
				float DistScalar = TargetLengthClamped * 0.0001f;
				float ClampedTargetTiltX = FMath::Clamp((InputZ*CameraTiltValue*DistScalar), -CameraTiltClamp, CameraTiltClamp);
				float ClampedTargetTiltZ = FMath::Clamp((InputX*CameraTiltValue*DistScalar), -CameraTiltClamp, CameraTiltClamp);
				CameraTiltX = FMath::FInterpTo(CameraTiltX, ClampedTargetTiltX, DeltaTime, CameraTiltSpeed * TiltDistanceScalar); // pitch
				CameraTiltZ = FMath::FInterpTo(CameraTiltZ, ClampedTargetTiltZ, DeltaTime, CameraTiltSpeed * TiltDistanceScalar); // yaw
				FRotator FTarget = FRotator(CameraTiltX, CameraTiltZ, 0.0f) * CameraTiltValue;
				FTarget.Roll = 0.0f;
				SideViewCameraComponent->SetRelativeRotation(FTarget);

				// Adjust position to work angle
				Midpoint.X -= ((CameraTiltZ * (DesiredCameraDistance / 15.0f))) * DeltaTime;
				Midpoint.Z -= ((CameraTiltX * (DesiredCameraDistance / 15.0f))) * DeltaTime;

				// Make it so
				CameraBoom->SetWorldLocation(Midpoint);
				CameraBoom->TargetArmLength = DesiredCameraDistance;

				///GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::White, FString::Printf(TEXT("DesiredCameraDistance: %f"), DesiredCameraDistance));

				/*
				Out_Parallel = VectorToSplit.ProjectOnTo(ReferenceVector);
				//vectorApar + vectorAper = vectorA
				Out_Perpendicular = VectorToSplit - Out_Parallel;
				*/
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
	if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.3f)
	{
		// Scaling down
		if (Locator->RelativeScale3D.Size() >= 0.01f)
		{
			float DeltaTime = GetWorld()->DeltaTimeSeconds;
			FVector BottomSize = FVector(0.01f, 0.01f, 0.01f);
			FVector ShrinkingSize = FMath::VInterpTo(Locator->RelativeScale3D, BottomSize, DeltaTime, 3.0f); //Locator->RelativeScale3D * 0.93f;
			Locator->SetRelativeScale3D(ShrinkingSize);

			float SpinScalar = 1 + (1000 / Locator->RelativeScale3D.Size());
			Locator->AddLocalRotation(FRotator(SpinScalar, 0.0f, 0.0f) * DeltaTime);
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


// Helper for ChargeBar UI
float AGammaCharacter::GetChargePercentage()
{
	float Percentage = 0.0f;
	if (Charge >= 0)
	{
		Percentage = Charge / ChargeMax;
	}
	else
	{
		// Subzero charge is absolute
		Percentage = Charge;
	}
	return Percentage;
}


/////////////////////////////////////////////////////////////////////////
// TICK
void AGammaCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	// Main update
	if (CustomTimeDilation > 0.1f) /// UGameplayStatics::GetGlobalTimeDilation(GetWorld())
	{
		UpdateCharacter(DeltaSeconds);
	}

	// Update prefire
	if ((GetActiveFlash() != nullptr) && GetActiveFlash()->IsValidLowLevel())
	{
		PrefireTiming();
	}

	// Update boosting
	if (bBoosting)
	{
		KickPropulsion();
	}


	// Update player pitch
	if (PlayerSound != nullptr)
	{
		float VectorScale = FMath::Sqrt(GetCharacterMovement()->Velocity.Size()) * 0.033f;
		float Scalar = FMath::Clamp(VectorScale, 0.2f, 4.0f) * 0.5f;
		PlayerSound->SetPitchMultiplier(Scalar);
	}
}



//////////////////////////////////////////////////////////////////////
// MOVEMENT & FIGHTING

// MOVE	LEFT / RIGHT
void AGammaCharacter::MoveRight(float Value)
{
	float ValClamped = FMath::Clamp(Value, -1.0f, 1.0f);

	// Adjust aim to reflect move
	/// Ignoring no inputs and bots
	if (InputX != ValClamped && !ActorHasTag("Bot")
		&& !(InputX == 0.0f && ValClamped == 0.0f))
	{
		SetX(ValClamped);
	}

	if ((MoveTimer >= (1 / MovesPerSecond))
		&& !bSliding
		&& CustomTimeDilation > 0.1f) /// UGameplayStatics::GetGlobalTimeDilation(GetWorld()
	{
		FVector MoveInput = FVector(InputX, 0.0f, InputZ).GetSafeNormal();
		FVector CurrentV = GetMovementComponent()->Velocity.GetSafeNormal();
		
		// Move by dot product for skating effect
		if (InputX != 0.0f)
		{
			float MoveByDot = 0.0f;
			float DotToInput = FVector::DotProduct(MoveInput, CurrentV);
			float AngleToInput = TurnSpeed * FMath::Abs(FMath::Clamp(FMath::Acos(DotToInput), -90.0f, 90.0f));
			MoveByDot = MoveSpeed + (AngleToInput * MoveSpeed);
			//GetCharacterMovement()->MaxFlySpeed = MoveByDot / 3.0f;
			//GetCharacterMovement()->MaxAcceleration = MoveByDot;
			AddMovementInput(FVector(1.0f, 0.0f, 0.0f), InputX * MoveByDot); // ValClamped
		}
	}

	//GEngine->AddOnScreenDebugMessage(-1, 0.018f, FColor::White, FString::Printf(TEXT("Input x: %f"), InputX));
	ForceNetUpdate();
}

// MOVE UP / DOWN
void AGammaCharacter::MoveUp(float Value)
{
	float ValClamped = FMath::Clamp(Value, -1.0f, 1.0f);

	// Adjust aim to reflect move
	/// Ignoring no inputs and bots
	if (InputZ != ValClamped && !ActorHasTag("Bot")
		&& !(InputZ == 0.0f && ValClamped == 0.0f))
	{
		SetZ(ValClamped);
	}

	// Abide moves per second
	if ((MoveTimer >= (1 / MovesPerSecond))
		&& !bSliding
		&& CustomTimeDilation > 0.1f) /// UGameplayStatics::GetGlobalTimeDilation(GetWorld()) 
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
			//GetCharacterMovement()->MaxFlySpeed = MoveByDot / 3.0f;
			//GetCharacterMovement()->MaxAcceleration = MoveByDot;
			AddMovementInput(FVector(0.0f, 0.0f, 1.0f), InputZ * MoveByDot);
		}
	}

	//GEngine->AddOnScreenDebugMessage(-1, 0.018f, FColor::White, FString::Printf(TEXT("Input z: %f"), InputZ));
	ForceNetUpdate();
}

// SET X & Z SERVERSIDE
void AGammaCharacter::SetX(float Value)
{
	if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.3f)
	{
		ServerSetX(Value);
	}
}
void AGammaCharacter::ServerSetX_Implementation(float Value)
{
	// Get delta move value
	float DeltaVal = FMath::Abs(FMath::Abs(Value) - FMath::Abs(x));
	float ValClamped = FMath::Clamp(DeltaVal * 0.68f, 0.1f, 1.0f);
	float TimeScaleInfluence = 1 + FMath::Abs(1 - CustomTimeDilation); /// UGameplayStatics::GetGlobalTimeDilation(GetWorld())
	InputX = Value + (Value * ValClamped) * TimeScaleInfluence;

	// Speed and Acceleration
	float Scalar = FMath::Abs(InputX);
	GetCharacterMovement()->MaxFlySpeed = MaxMoveSpeed * FMath::Square(Scalar);
	GetCharacterMovement()->MaxAcceleration = MoveSpeed * FMath::Square(Scalar) + TurnSpeed;

	///GEngine->AddOnScreenDebugMessage(-1, DeltaVal, FColor::Green, FString::Printf(TEXT("Scalar: %f"), Scalar));

	// Update delta
	x = Value;

	if (ActorHasTag("Bot"))
	{
		MoveRight(InputX);
	}
}
bool AGammaCharacter::ServerSetX_Validate(float Value)
{
	return true;
}


void AGammaCharacter::SetZ(float Value)
{
	if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.3f)
	{
		ServerSetZ(Value);
	}
}
void AGammaCharacter::ServerSetZ_Implementation(float Value)
{
	// Get move input Delta
	float DeltaVal = FMath::Abs(FMath::Abs(Value) - FMath::Abs(z));
	float ValClamped = FMath::Clamp(DeltaVal * 0.68f, 0.1f, 1.0f);
	float TimeScaleInfluence = 1 + FMath::Abs(1 - CustomTimeDilation); /// UGameplayStatics::GetGlobalTimeDilation(GetWorld())
	InputZ = Value + (Value * ValClamped) * TimeScaleInfluence;

	// Speed and Acceleration
	float Scalar = FMath::Abs(InputZ);
	GetCharacterMovement()->MaxFlySpeed = MaxMoveSpeed * FMath::Square(Scalar);
	GetCharacterMovement()->MaxAcceleration = MoveSpeed * FMath::Square(Scalar) + TurnSpeed;

	///GEngine->AddOnScreenDebugMessage(-1, DeltaVal, FColor::Green, FString::Printf(TEXT("Scalar: %f"), Scalar));

	// Update delta
	z = Value;
	
	if (ActorHasTag("Bot"))
	{
		MoveUp(InputZ);
	}
}
bool AGammaCharacter::ServerSetZ_Validate(float Value)
{
	return true;
}

// BOOST KICK HUSTLE DASH
void AGammaCharacter::NewMoveKick()
{
	if (!bBoosting)
	{
		if (Role == ROLE_AutonomousProxy) // < ROLE_Authority
		{
			ServerNewMoveKick();
			return;
		}

		bBoosting = true;
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


// DISENGAGE LE KICK
void AGammaCharacter::DisengageKick()
{
	if (Role == ROLE_AutonomousProxy)
	{
		ServerDisengageKick();
		return;
	}

	// Clear existing boost
	if (ActiveBoost != nullptr)
	{
		ActiveBoost->Destroy();
		ActiveBoost = nullptr;
		///GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("KICK DISENGAGED.."));
	}
	if (ActiveChargeParticles != nullptr)
	{
		ActiveChargeParticles->Destroy();
		ActiveChargeParticles = nullptr;
	}

	//bBoosting = false;
}
void AGammaCharacter::ServerDisengageKick_Implementation()
{
	DisengageKick();
}
bool AGammaCharacter::ServerDisengageKick_Validate()
{
	return true;
}


// LE KICK PROPULSION
void AGammaCharacter::KickPropulsion()
{
	if (Role == ROLE_AutonomousProxy)
	{
		ServerKickPropulsion();
		return;
	}

	if (UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) > 0.3f)
	{

		// Conditions met - (Charge FX are active) and (We're within acceptable speed)
		if (((ActiveChargeParticles != nullptr) && ActiveChargeParticles->IsValidLowLevel() && !ActiveChargeParticles->IsPendingKillOrUnreachable()) 
			&& GetCharacterMovement()->Velocity.Size() < (MaxMoveSpeed * 5.0f))
		{
			// Algo scaling for timescale & max velocity
			FVector MoveInputVector = FVector(InputX + x, 0.0f, InputZ + z);
			FVector CurrentVelocity = GetCharacterMovement()->Velocity;
			float TimeDelta = (GetWorld()->DeltaTimeSeconds / CustomTimeDilation);
			//float RelativityToMaxSpeed = (MaxMoveSpeed) - CurrentVelocity.Size();
			float DotScalar = 1 / FMath::Abs(FVector::DotProduct(CurrentVelocity.GetSafeNormal(), MoveInputVector));

			// Force, clamp, & effect chara movement
			FVector KickVector = MoveInputVector
				* MoveFreshMultiplier
				* 1000.0f ///previously RelativityToMaxSpeed
				* (1.0f + FMath::Abs(x))
				* TimeDelta;
			
			// Trimming
			KickVector.Z *= 0.7f;
			KickVector.Y = 0.0f;
			GetCharacterMovement()->AddImpulse(KickVector);

			bMoved = false;
			MoveTimer = 0.0f;

			// Spawn visuals
			if (BoostClass != nullptr && ActiveBoost == nullptr)
			{
				FActorSpawnParameters SpawnParams;
				FVector PlayerVelocity = GetCharacterMovement()->Velocity;
				FRotator PlayerVelRotator = PlayerVelocity.Rotation();
				FRotator InputRotator = MoveInputVector.Rotation();

				PlayerVelocity.Z *= 0.01f;
				FVector SpawnLocation = GetActorLocation(); /// + (PlayerVelocity / 3);

				ActiveBoost = GetWorld()->SpawnActor<AActor>
					(BoostClass, SpawnLocation, InputRotator, SpawnParams); /// PlayerVelRotator

				//GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::White, TEXT("SPAWNING KICK..."));
			}

			ForceNetUpdate();

			GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, FString::Printf(TEXT("MoveInputVector  %f"), MoveInputVector.Size()));
			//GEngine->AddOnScreenDebugMessage(-1, 10.5f, FColor::Cyan, FString::Printf(TEXT("dot  %f"), DotScalar));
			//GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan, FString::Printf(TEXT("mass  %f"), GetCharacterMovement()->Mass));
			//GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan, FString::Printf(TEXT("vel  %f"), (GetCharacterMovement()->Velocity.Size())));
			//GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan, FString::Printf(TEXT("time scalar   %f"), TimeScalar));
		}
		else
		{
			DisengageKick();
		}
		
	}
}
void AGammaCharacter::ServerKickPropulsion_Implementation()
{
	KickPropulsion();
}
bool AGammaCharacter::ServerKickPropulsion_Validate()
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


// RAISE CHARGE
void AGammaCharacter::RaiseCharge()
{
	if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.3f
		&& Charge <= (ChargeMax - ChargeGain)
		&& ActiveAttack == nullptr)
	{
		// Noobish recovery from empty-case -1 charge
		if (Charge < 0.0f) {
			Charge = 0.0f;
		}

		// Charge growth
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

		// Sound fx
		if (PlayerSound != nullptr)
		{
			//PlayerSound->SetPitchMultiplier(Charge * 0.3f);
			PlayerSound->Play();
		}

		// visual charge vfx
		if (ChargeParticles != nullptr)
		{
			FActorSpawnParameters SpawnParams;
			ActiveChargeParticles = Cast<AActor>(GetWorld()->SpawnActor<AActor>(ChargeParticles, GetActorLocation(), GetActorRotation(), SpawnParams));
			ActiveChargeParticles->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		}

		//GEngine->AddOnScreenDebugMessage(-1, 0.68f, FColor::White, FString::Printf(TEXT("%f % CHARGE"), Charge), true, FVector2D::UnitVector * 2);


		//// Scale up charge vfx
		//float Scalar = Charge / ChargeMax;
		//ActiveChargeParticles->SetActorRelativeScale3D(ActiveChargeParticles->GetActorRelativeScale3D() * Scalar);
	}

	//// Catch edgecase 'dead key' and refire
	//if ((ActiveChargeParticles == nullptr && GetActiveBoost() == nullptr) || ((GetActiveBoost() != nullptr) && GetActiveBoost()->IsPendingKillOrUnreachable()))
	//{
	//	///GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("N E W  K I C K I N G"));
	//	NewMoveKick();
	//}
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
	if (Charge < ChargeGain)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, FString::Printf(TEXT("CHARGE [SPACEBAR], [DOWN] %f"), Charge));
		Charge = -1.0f;
		return;
	}

	// Clean burn
	MoveParticles->bSuppressSpawning = true;

	if (!bMultipleAttacks && ActiveAttack != nullptr)
	{
		ActiveAttack->Nullify(0);
	}

	bool bFireAllowed = (bMultipleAttacks || (!bMultipleAttacks && ActiveAttack == nullptr)) && GetActiveFlash() == nullptr;
	if (bFireAllowed && (Charge > 0.0f) && (FlashClass != nullptr)
		&& (UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) > 0.3f))
	{
		//// Clean up previous attack
		//if (ActiveAttack && !bMultipleAttacks)
		//{
		//	ActiveAttack->Destroy();
		//	ActiveAttack = nullptr;
		//}

		// Direction & setting up
		FVector FirePosition = AttackScene->GetComponentLocation();
		FVector FireDirection = AttackScene->GetForwardVector();
		FireDirection.Y = 0.0f;
		FRotator FireRotation = FireDirection.Rotation() + FRotator(InputZ, 0.0f, 0.0f); // InputZ * 21.0f
		FireRotation.Yaw = GetActorRotation().Yaw;
		if (FMath::Abs(FireRotation.Yaw) > 90.0f)
		{
			FireRotation.Yaw = 180.0f;
		}
		else
		{
			FireRotation.Yaw = 0.0f;
		}
		//FVector MoveInputVector = FVector(InputX, 0.0f, InputZ);

		// Spawning
		if (HasAuthority() && (FlashClass != nullptr))
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

	if (!bMultipleAttacks && ActiveAttack != nullptr)
	{
		NullifyAttack();
		return;
	}

	if (	AttackClass != nullptr
		&& (ActiveAttack == nullptr || bMultipleAttacks) 
		&& (Charge > 0.0f)
		&& (UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) > 0.3f)
		&& ((PrefireTimer >= 0.001f && ActiveFlash != nullptr)))
	{
		// Clean up previous flash
		if ((GetActiveFlash() != nullptr))
		{
			ActiveFlash->Destroy();
			ActiveFlash = nullptr;
			//ClearFlash();
		}

		// Aim by InputY
		float AimClampedInputZ = FMath::Clamp((InputZ * 10.0f), -1.0f, 1.0f);
		FVector FirePosition = AttackScene->GetComponentLocation();
		FVector LocalForward = AttackScene->GetForwardVector();
		LocalForward.Y = 0.0f;
		FRotator FireRotation = LocalForward.Rotation() + FRotator(InputZ * 21.0f, 0.0f, 0.0f); /// AimClampedInputZ
		FireRotation.Yaw = GetActorRotation().Yaw;
		if (FMath::Abs(FireRotation.Yaw) > 90.0f)
		{
			FireRotation.Yaw = 180.0f;
		}
		else
		{
			FireRotation.Yaw = 0.0f;
		}


		// Spawning
		if (HasAuthority())
		{
			if (AttackClass != nullptr || bMultipleAttacks)
			{
				FActorSpawnParameters SpawnParams;
				ActiveAttack = Cast<AGAttack>(GetWorld()->SpawnActor<AGAttack>(AttackClass, FirePosition, FireRotation, SpawnParams));
				if (ActiveAttack != nullptr)
				{

					// Imbue with magnitude by PrefireTimer
					float PrefireClamped = FMath::Clamp(PrefireTimer, 0.1f, 1.0f);
					float PrefireCurve = FMath::Square(PrefireClamped) * 2.1f; /// curves out to max ~1
					PrefireCurve = FMath::Clamp(PrefireCurve, 0.1f, 1.0f);

					///GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::White, FString::Printf(TEXT("PrefireClamped: %f"), PrefireClamped));
					///GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::White, FString::Printf(TEXT("PrefireCurve: %f"), PrefireCurve));
					
					ActiveAttack->InitAttack(this, PrefireCurve, AimClampedInputZ);

					// Positional lock or naw
					if (ActiveAttack->LockedEmitPoint)
					{
						ActiveAttack->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform); // World
					}
					//else
					//{
					//	// Non-locked attacks must align to fighting plane
					//	FRotator AttackRotation = ActiveAttack->GetActorRotation();
					//	if (AttackRotation.Yaw > 90.0f)
					//	{
					//		ActiveAttack->SetActorRotation(FRotator(AttackRotation.Pitch, 180.0f, AttackRotation.Roll));
					//	}
					//	else
					//	{
					//		ActiveAttack->SetActorRotation(FRotator(AttackRotation.Pitch, 0.0f, AttackRotation.Roll));
					//	}
					//}
				}
			}
			else
			{
				GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("No attack class to spawn"));
			}

			// Exit claus'
			Charge -= ChargeGain;
		}

		PrefireTimer = 0.0f;
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
		&& (UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) > 0.3f))
	{
		// Clean burn
		MoveParticles->bSuppressSpawning = true;

		// Direction & setting up
		FVector FirePosition = GetActorLocation();
		FVector LocalForward = AttackScene->GetForwardVector();
		FRotator FireRotation = LocalForward.Rotation() + FRotator(21.0f * InputZ, 0.0f, 0.0f);
		FActorSpawnParameters SpawnParams;

		// Spawning
		if (HasAuthority() && (SecondaryClass != nullptr))
		{
			ActiveSecondary = GetWorld()->SpawnActor<AGAttack>(SecondaryClass, FirePosition, FireRotation, SpawnParams); //  Cast<AActor>());

			if (ActiveSecondary != nullptr)
			{
				//bSecondaryActive = true;

				AGAttack* PotentialAttack = Cast<AGAttack>(ActiveSecondary);
				if (PotentialAttack != nullptr)
				{
					PotentialAttack->InitAttack(this, 1, InputZ);

					if (PotentialAttack != nullptr && PotentialAttack->LockedEmitPoint)
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
	if (Role < ROLE_Authority)
	{
		ServerPrefireTiming();
	}
	else
	{
		if (GetActiveFlash() != nullptr
			&& PrefireTimer < PrefireTime)
		{
			PrefireTimer += GetWorld()->GetDeltaSeconds() * CustomTimeDilation;
			//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, FString::Printf(TEXT("PREFIRE T: %f"), PrefireTimer));
		}
		else if ((PrefireTimer >= PrefireTime)
			&& (Charge > 0)
			&& ActiveAttack == nullptr
			&& ActiveFlash != nullptr)
		{
			ReleaseAttack();
		}
		
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


// POWER SLIDE

// Using these hand-off functions to avoid network saturation
void AGammaCharacter::CheckPowerSlideOn()
{
	if (!bSliding)
	{
		PowerSlideEngage();
	}
}
void AGammaCharacter::CheckPowerSlideOff()
{
	if (bSliding)
	{
		PowerSlideDisengage();
	}
}


void AGammaCharacter::PowerSlideEngage()
{
	if (!bSliding)
	{
		GetCharacterMovement()->BrakingFrictionFactor = PowerSlideSpeed;
		bSliding = true;
		
		// Neutralizing charge
		// Armed player goes to -1, triggering a warning
		if (Charge > 0.0f)
		{
			Charge = -1.0f;
		}
		else
		{
			Charge = 0.0f;
		}

		// Attack cancel
		if (GetActiveFlash() != nullptr)
		{
			ClearFlash();
			PrefireTimer = 0.0f;
		}

		// Netcode emissary
		if (Role < ROLE_Authority)
		{
			ServerPowerSlideEngage();
		}
	}
}
void AGammaCharacter::ServerPowerSlideEngage_Implementation()
{
	PowerSlideEngage();
}
bool AGammaCharacter::ServerPowerSlideEngage_Validate()
{
	return true;
}

void AGammaCharacter::PowerSlideDisengage()
{
	if (bSliding)
	{
		GetCharacterMovement()->BrakingFrictionFactor = DecelerationSpeed;
		bSliding = false;

		if (Role < ROLE_Authority)
		{
			ServerPowerSlideDisengage();
		}
	}
}
void AGammaCharacter::ServerPowerSlideDisengage_Implementation()
{
	PowerSlideDisengage();
}
bool AGammaCharacter::ServerPowerSlideDisengage_Validate()
{
	return true;
}


// MODIFY HEALTH
void AGammaCharacter::ModifyHealth(float Value)
{
	Health = FMath::Clamp(Health += Value, -1.0f, 100.0f);

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

	// Init camera
	FVector PlayerLocation = GetActorLocation();
	PlayerLocation.Y = 0.0f;
	CameraBoom->SetRelativeLocation(PlayerLocation);
	PositionOne = PlayerLocation;
	PositionTwo = PlayerLocation;
	CameraBoom->TargetArmLength = 9000.0f;
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
	DOREPLIFETIME(AGammaCharacter, bSliding);
	DOREPLIFETIME(AGammaCharacter, bBoosting);

	DOREPLIFETIME(AGammaCharacter, Charge);
	DOREPLIFETIME(AGammaCharacter, bCharging);
}


// Print to screen
// GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("sizing"));
// GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Charge: %f"), Charge));