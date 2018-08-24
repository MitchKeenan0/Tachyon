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
	GetCharacterMovement()->GravityScale = 1.0f;
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
	///MoveParticles->bAutoActivate = false;

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
	bReplicateMovement = true;

	// TextRender->SetNetAddressable();

	//GetCameraBoom()->TargetArmLength = 1000.0f;
}


//////////////////////////////////////////////////////////////////////////
// Input
void AGammaCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(InputComponent);
	PlayerInputComponent->BindAction("Attack", IE_Pressed, this, &AGammaCharacter::CheckAttackOn);
	PlayerInputComponent->BindAction("Attack", IE_Released, this, &AGammaCharacter::CheckAttackOff);
	PlayerInputComponent->BindAction("PowerSlide", IE_Pressed, this, &AGammaCharacter::CheckPowerSlideOn);
	PlayerInputComponent->BindAction("PowerSlide", IE_Released, this, &AGammaCharacter::CheckPowerSlideOff);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AGammaCharacter::RaiseCharge);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AGammaCharacter::DisengageKick);
	PlayerInputComponent->BindAction("Secondary", IE_Pressed, this, &AGammaCharacter::CheckSecondaryOn);
	PlayerInputComponent->BindAction("Secondary", IE_Released, this, &AGammaCharacter::CheckSecondaryOff);
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
	//TextComponent->SetText(FText::FromString(CharacterName));

	// Init charge & health
	Charge = FMath::FloorToFloat(ChargeMax / 2.0f);
	Health = MaxHealth;

	// Init camera
	FVector PlayerLocation = GetActorLocation();
	PlayerLocation.Y = 0.0f;
	CameraBoom->SetRelativeLocation(PlayerLocation);
	PositionOne = PlayerLocation;
	PositionTwo = PlayerLocation;
	CameraBoom->TargetArmLength = 9000.0f;

	// Sprite Scaling
	float ClampedCharge = FMath::Clamp(Charge * 0.7f, 1.0f, ChargeMax);
	float SCharge = 0.5f * (FMath::Square(ClampedCharge));
	FVector ChargeSize = FVector(SCharge, SCharge, SCharge);
	GetCapsuleComponent()->SetWorldScale3D(ChargeSize);

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
		ResetLocator();
	}
	
	

	if ((Controller != nullptr))
	{
		float VelocitySize = GetCharacterMovement()->Velocity.Size();
		float AccelSpeed = FMath::Clamp((100.0f / VelocitySize) * MaxMoveSpeed, 1.0f, MaxMoveSpeed * 100.0f);
		GetCharacterMovement()->MaxFlySpeed = AccelSpeed;
		//GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::White, FString::Printf(TEXT("max fly speed: %f"), GetCharacterMovement()->MaxFlySpeed));

		// Surface place
		/*FVector PlayerPosition = GetActorLocation();
		PlayerPosition.Z = FMath::Clamp(PlayerPosition.Z, -3000.0f, 10000.0f);
		SetActorLocation(PlayerPosition);*/

		//float GlobalTimeDil = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
		//if (GlobalTimeDil > 0.3f)
		//{
		//	float t = (FMath::Square(MyTimeDilation) * 100.0f) * DeltaTime;
		//	float ReturnTime = FMath::FInterpConstantTo(MyTimeDilation, 1.0f, DeltaTime, 2.6f); // t or DeltaTime
		//	CustomTimeDilation = FMath::Clamp(ReturnTime, 0.01f, 1.0f);
		//	///GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::White, FString::Printf(TEXT("t: %f"), t));
		//}


		// Set rotation so character faces direction of travel
		float TravelDirection = FMath::Clamp(InputX, -1.0f, 1.0f);
		float ClimbDirection = FMath::Clamp(InputZ, -1.0f, 1.0f) * 5.0f;
		float Roll = FMath::Clamp(InputZ, -1.0f, 1.0f) * 15.0f;

		if (TravelDirection < 0.0f)
		{
			FRotator Fint = FMath::RInterpTo(Controller->GetControlRotation(), FRotator(ClimbDirection, 180.0f, Roll), DeltaTime, 15.0f);
			Controller->SetControlRotation(Fint);
		}
		else if (TravelDirection > 0.0f)
		{
			FRotator Fint = FMath::RInterpTo(Controller->GetControlRotation(), FRotator(ClimbDirection, 0.0f, -Roll), DeltaTime, 15.0f);
			Controller->SetControlRotation(Fint);
		}

		// No lateral Input - finish rotation
		else
		{
			//// Incorporate velocity into rotation
			//FVector PlayerVelocity = GetCharacterMovement()->Velocity * GetActorForwardVector();
			//ClimbDirection = (PlayerVelocity.X * DeltaTime);

			if (Controller->GetControlRotation().Yaw > 90.0f)
			{
				FRotator Fint = FMath::RInterpTo(Controller->GetControlRotation(), FRotator(ClimbDirection, 180.0f, -Roll), DeltaTime, 5.0f);
				Controller->SetControlRotation(Fint);
			}
			else
			{
				FRotator Fint = FMath::RInterpTo(Controller->GetControlRotation(), FRotator(ClimbDirection, 0.0f, Roll), DeltaTime, 5.0f);
				Controller->SetControlRotation(Fint);
			}
		}


		// Locator scaling
		if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.01f)
		{
			LocatorScaling();
		}

		// AfterImage rotation
		
	}

	//// Move timing and clamp
	//if (GetCharacterMovement() && !bMoved) //  HasAuthority() && 
	//{
	//	// Clamp velocity
	//	/*float TimeDilat = UGameplayStatics::GetGlobalTimeDilation(this->GetWorld());
	//	float TimeScalar = FMath::Abs(SlowmoMoveBoost * (1 / TimeDilat));
	//	GetCharacterMovement()->Velocity = 
	//		GetCharacterMovement()->Velocity.GetClampedToSize(0.0f, MaxMoveSpeed * MoveFreshMultiplier * TimeScalar);*/

	//	MoveTimer += GetWorld()->DeltaTimeSeconds;
	//}
}



// UPDATE CAMERA
void AGammaCharacter::UpdateCamera(float DeltaTime)
{
	// Start by checking valid actor
	AActor* Actor1 = nullptr;
	AActor* Actor2 = nullptr;

	// Poll for framing actors
	if ((Actor1 == nullptr) && (Actor2 == nullptr))
	{
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("FramingActor"), FramingActors);
	}
	///GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, FString::Printf(TEXT("FramingActors Num:  %i"), FramingActors.Num()));
	/// TODO - add defense on running get?
	
	// Spectator ez life
	if (!this->ActorHasTag("Spectator"))
	{
		Actor1 = this;
	}
	
	if ((Controller != nullptr) && (FramingActors.Num() >= 1))
	{
		// Edge case: player is spectator, find a sub
		if (Actor1 == nullptr)
		{
			int LoopSize = FramingActors.Num();
			for (int i = 0; i < LoopSize; ++i)
			{
				AActor* Actore = FramingActors[i];
				if (Actore != nullptr
					&& !Actore->ActorHasTag("Spectator")
					&& Actore->ActorHasTag("Player"))
				{
					Actor1 = Actore;
					break;
				}
			}
		}

		// Let's go
		if ((Actor1 != nullptr)) ///  && IsValid(Actor1) && !Actor1->IsUnreachable()
		{

			float VelocityCameraSpeed = CameraMoveSpeed;
			float CameraMaxSpeed = 10000.0f;
			float ConsideredDistanceScalar = CameraDistanceScalar;
			
			// Position by another actor
			bool bAlone = true;
			// Find closest best candidate for Actor 2
			if (Actor2 == nullptr)
			{

				// Find Actor2 by nominating best actor
				float DistToActor2 = 99999.0f;
				int LoopCount = FramingActors.Num();
				AActor* CurrentActor = nullptr;
				AActor* BestCandidate = nullptr;
				float BestDistance = 0.0f;
				for (int i = 0; i < LoopCount; ++i)
				{
					CurrentActor = FramingActors[i];
					if (CurrentActor != nullptr
						&& CurrentActor != Actor1
						&& !CurrentActor->ActorHasTag("Spectator")
						&& !CurrentActor->ActorHasTag("Obstacle"))
					{
						float DistToTemp = FVector::Dist(CurrentActor->GetActorLocation(), GetActorLocation());
						if (DistToTemp < DistToActor2)
						{
							
							// Players get veto importance
							if ((BestCandidate == nullptr)
								|| (!BestCandidate->ActorHasTag("Player")))
							{
								BestCandidate = CurrentActor;
								DistToActor2 = DistToTemp;
							}
						}
					}
				}

				// Got your boye
				Actor2 = BestCandidate;
			}

			

			// Framing up first actor with their own velocity
			FVector Actor1Velocity = (Actor1->GetVelocity()) * CustomTimeDilation;
			Actor1Velocity.Z *= 0.5f;
			float SafeVelocitySize = FMath::Clamp(Actor1Velocity.Size(), 0.01f, MaxMoveSpeed); // Prev. 350
			VelocityCameraSpeed = CameraMoveSpeed * SafeVelocitySize * DeltaTime;
			VelocityCameraSpeed = FMath::Clamp(VelocityCameraSpeed, 0.1f, CameraMaxSpeed);

			FVector LocalPos = Actor1->GetActorLocation(); // +(Actor1Velocity * CameraVelocityChase); // *GTimeScale); // * TimeDilationScalarClamped
			PositionOne = FMath::VInterpTo(PositionOne, LocalPos, DeltaTime, VelocityCameraSpeed);

			// Setting up distance and speed dynamics
			float GTimeScale = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
			float ChargeScalar = FMath::Clamp((FMath::Sqrt(Charge - 0.9f)), 0.1f, ChargeMax);
			float SizeScalar = 1.0f; /// GetCapsuleComponent()->GetComponentScale().Size()
			float SpeedScalar = FMath::Sqrt(Actor1Velocity.Size() + 0.01f) * 0.39f;
			float PersonalScalar = (36.0f * SizeScalar * ChargeScalar * SpeedScalar) * (FMath::Sqrt(SafeVelocitySize) * 0.06f);
			float CameraMinimumDistance = (1100.0f + PersonalScalar) * CameraDistanceScalar;
			float CameraMaxDistance = 1551000.0f;


			// If Actor2 is valid, make Pair Framing
			if (Actor2 != nullptr)
			{
				
				// Distance check i.e pair bounds
				float PairDistanceThreshold = FMath::Clamp(Actor1->GetVelocity().Size(), 1000.0f, 3000.0f); /// formerly 3000.0f
				if (this->ActorHasTag("Spectator"))
				{
					PairDistanceThreshold *= 3.3f;
				}
				
				// Special care taken for vertical as we are probably widescreen
				float Vertical = FMath::Abs((Actor2->GetActorLocation() - Actor1->GetActorLocation()).Z);
				bool bInRange = (FVector::Dist(Actor1->GetActorLocation(), Actor2->GetActorLocation()) <= PairDistanceThreshold)
					&& (Vertical <= (PairDistanceThreshold * 0.55f));
				bool TargetVisible = Actor2->WasRecentlyRendered(0.2f);
				
				if (bInRange && TargetVisible)
				{
					bAlone = false;

					//// Framing up with second actor
					//FVector Actor2Velocity = (Actor2->GetVelocity()) * CustomTimeDilation;
					//float SafeVelocitySize = FMath::Clamp(Actor2Velocity.Size(), 1.0f, MaxMoveSpeed);
					//VelocityCameraSpeed = CameraMoveSpeed * (FMath::Sqrt(SafeVelocitySize)) * DeltaTime;
					//VelocityCameraSpeed = FMath::Clamp(VelocityCameraSpeed, 1.0f, CameraMaxSpeed);
					//
					//
					//Actor2Velocity = Actor2Velocity.GetClampedToSize(1.0f, 1500.0f) * 0.5f; // *CustomTimeDilation);
					/////Actor2Velocity = Actor2Velocity.GetClampedToMaxSize(15000.0f * (CustomTimeDilation + 0.5f));
					//Actor2Velocity.Z *= 0.85f;

					// Declare Position Two
					FVector PairFraming = Actor2->GetActorLocation(); //  +(Actor2Velocity * CameraVelocityChase * CustomTimeDilation);
					PositionTwo = FMath::VInterpTo(PositionTwo, PairFraming, DeltaTime, VelocityCameraSpeed);
				}
			}
			
			// Lone player gets Velocity Framing
			if (bAlone || (FramingActors.Num() == 1))
			{
				
				// Framing lone player by their velocity
				//Actor1Velocity = (Actor1->GetVelocity()) * CustomTimeDilation;

				// Clamp to max size
				//Actor1Velocity = Actor1Velocity.GetClampedToSize(105.0f, 1500.0f); // *CustomTimeDilation);
				//Actor1Velocity.Z *= 0.85f;

				// Smooth-over low velocities with a standard framing
				/*if (Actor1Velocity.Size() < 300.0f)
				{
					Actor1Velocity = GetActorForwardVector() * 521.0f;
				}*/

				// Declare Position Two
				FVector VelocityFraming = Actor1->GetActorLocation(); // +(Actor1Velocity * CameraSoloVelocityChase);
				PositionTwo = FMath::VInterpTo(PositionTwo, VelocityFraming, DeltaTime, (VelocityCameraSpeed * 0.5f)); // UnDilatedDeltaTime
				
				// Distance controls
				ConsideredDistanceScalar *= 1.5f;
				CameraMaxDistance = 150000.0f;

				Actor2 = nullptr;
			}


			// Positions done
			// Find the midpoint
			FVector TargetMidpoint = PositionOne + ((PositionTwo - PositionOne) * 0.5f);
			float DistanceToTargetScalar = FVector::Dist(TargetMidpoint, CameraBoom->GetComponentLocation()) * 0.1f;
			
			Midpoint = TargetMidpoint;
				///FMath::VInterpTo(Midpoint, TargetMidpoint, DeltaTime, DistanceToTargetScalar);
			if (Midpoint.Size() > 0.0f)
			{

				// Distance
				float DistBetweenActors = FVector::Dist(PositionOne, PositionTwo);
				float ProcessedDist = (FMath::Sqrt(DistBetweenActors) * 1500.0f);
				float VerticalDist = FMath::Abs((PositionTwo - PositionOne).Z);
				// If paired, widescreen edges are vulnerable to overshoot
				if (!bAlone)
				{
					VerticalDist *= 1.2f;
				}

				// Handle horizontal bias
				float DistancePreClamp = ProcessedDist + FMath::Sqrt(VerticalDist);
				///GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::White, FString::Printf(TEXT("DistancePreClamp: %f"), DistancePreClamp));
				float TargetLength = FMath::Clamp(DistancePreClamp, CameraMinimumDistance, CameraMaxDistance);

				// Modifier for prefire timing
				if (PrefireTimer > 0.0f)
				{
					float PrefireScalarRaw = PrefireTimer; //  FMath::Sqrt(PrefireTimer * 0.618f);
					float PrefireScalarClamped = FMath::Clamp(PrefireScalarRaw, 0.1f, 0.99f);
					float PrefireProduct = (-PrefireScalarClamped);
					TargetLength += PrefireProduct;
				}

				// Last modifier for global time dilation
				float GlobalTimeScale = UGameplayStatics::GetGlobalTimeDilation(GetWorld()) * 2.1f;
				float RefinedGScalar = FMath::Clamp(GlobalTimeScale, 0.5f, 1.0f);
				TargetLength *= RefinedGScalar;

				// Clamp useable distance
				float TargetLengthClamped = FMath::Clamp(FMath::Sqrt(TargetLength * 150.0f) * ConsideredDistanceScalar,
					CameraMinimumDistance * RefinedGScalar,
					CameraMaxDistance);

				// Set Camera Distance
				float DesiredCameraDistance = FMath::FInterpTo(GetCameraBoom()->TargetArmLength,
					TargetLengthClamped, DeltaTime, (VelocityCameraSpeed * 0.215f));
					
				// Camera tilt
				FRotator FTarget = FRotator::ZeroRotator;
				if (!this->ActorHasTag("Spectator"))
				{
					//float TimeSensitiveTiltValue = CameraTiltValue * CustomTimeDilation;
					float TiltDistanceScalar = FMath::Clamp((1.0f / DistBetweenActors) * 99.9f, 0.1f, 0.3f);
					float DistScalar = (TargetLengthClamped * 0.1f);
					float ClampedTargetTiltX = FMath::Clamp((InputZ*CameraTiltValue*DistScalar), -CameraTiltClamp, CameraTiltClamp);
					float ClampedTargetTiltZ = FMath::Clamp((InputX*CameraTiltValue*DistScalar) * (PrefireTimer + 0.1f), -CameraTiltClamp, CameraTiltClamp * 2.0f);
					CameraTiltX = FMath::FInterpTo(CameraTiltX, ClampedTargetTiltX, DeltaTime, CameraTiltSpeed * TiltDistanceScalar); // pitch
					CameraTiltZ = FMath::FInterpTo(CameraTiltZ, ClampedTargetTiltZ, DeltaTime, CameraTiltSpeed * TiltDistanceScalar); // yaw
					
				}
				FTarget = FRotator((CameraTiltX, CameraTiltZ, 0.0f) * CameraTiltValue);
				FTarget.Roll = 0.0f;
				
				// Adjust position to work angle
				//Midpoint.X -= (CameraTiltZ * DesiredCameraDistance) * DeltaTime;
				//Midpoint.Z -= (CameraTiltX * DesiredCameraDistance) * DeltaTime;

				// Make it so
				CameraBoom->SetWorldLocation(Midpoint);
				CameraBoom->TargetArmLength = DesiredCameraDistance * FMath::Clamp(GTimeScale, 0.999f, 1.0f);
				SideViewCameraComponent->SetRelativeRotation(FTarget);
				SideViewCameraComponent->OrthoWidth = (DesiredCameraDistance);

				/// debug Velocity size
				/*GEngine->AddOnScreenDebugMessage(-1, 0.f,
					FColor::White, FString::Printf(TEXT("DesiredCameraDistance: %f"), DesiredCameraDistance));*/
			}
		}
	}
}


void AGammaCharacter::ResetLocator()
{
	Locator->SetRelativeScale3D(FVector(25.0f, 25.0f, 25.0f));
	TextComponent->SetText(FText::FromString(CharacterName));
	ClearFlash();
}


void AGammaCharacter::LocatorScaling()
{
	// Player locator
	if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.01f)
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
	if (Charge > 0)
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
	if ((Controller != nullptr))
	{
		UpdateCharacter(DeltaSeconds);

		///GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, FString::Printf(TEXT("timescale  %f"), CustomTimeDilation));

		// Timescaling
		/*if (CustomTimeDilation < 1.0f)
		{
			RecoverTimescale(DeltaSeconds);
		}*/
		// experimental hotness -- bigger characters are slower
		//float SizeBasedTimescale = 1.0f / GetChargePercentage();
		//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, FString::Printf(TEXT("SizeBasedTimescale  %f"), SizeBasedTimescale));
		
		// Inner lighting representing charge
		TArray<UPointLightComponent*> PointLights;
		GetComponents<UPointLightComponent>(PointLights);
		int LightsNum = PointLights.Num();
		if (LightsNum > 0)
		{
			for (int i = 0; i < LightsNum; ++i)
			{
				UPointLightComponent* CurrentLight = PointLights[i];
				float NewLightIntensity = (1000.0f * Charge) + 100.0f;
				CurrentLight->Intensity = NewLightIntensity;
				///GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, FString::Printf(TEXT("NewLightIntensity  %f"), NewLightIntensity));
			}
		}
		

		if (bShooting)
		{
			if ((ActiveAttack == nullptr) || !(IsValid(ActiveAttack)))
			{
				InitAttack();
			}
		}
		
		
		if (!bShooting && (PrefireTimer > 0.0f))
		{
			if (ActiveAttack == nullptr)
			{
				ReleaseAttack();
			}
		}

		if (bShielding)
		{
			FireSecondary();
			//bShielding = false;
		}

		// Update prefire
		if ((GetActiveFlash() != nullptr) && IsValid(GetActiveFlash()))
		{
			PrefireTiming();
		}

		// Update boosting
		if (bBoosting)
		{
			KickPropulsion();
		}
		else if (GetActiveBoost() != nullptr)
		{
			if (GetActiveBoost()->GetGameTimeSinceCreation() > MoveFreshMultiplier)
			{
				DisengageKick();
			}
		}

		// Update charge to catch lost input
		if (bCharging)
		{
			RaiseCharge();
		}

		// Update smooth health value
		if (MaxHealth < Health)
		{
			float InterpSpeed = FMath::Abs(Health - MaxHealth) * 5.1f;
			Health = FMath::FInterpConstantTo(Health, MaxHealth, UGameplayStatics::GetWorldDeltaSeconds(GetWorld()), InterpSpeed);
			///GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::White, FString::Printf(TEXT("InterpSpeed: %f"), InterpSpeed));

			// Player killed fx
			if ((KilledFX != nullptr)
				&& (ActiveKilledFX == nullptr)
				&& (Health <= 1.0f))
			{
				FActorSpawnParameters SpawnParams;
				ActiveKilledFX = GetWorld()->SpawnActor<AActor>(KilledFX, GetActorLocation(), GetActorRotation(), SpawnParams);
				if (ActiveKilledFX != nullptr)
				{
					ActiveKilledFX->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
				}
				
				// Particle coloring (for later)
				/*if (ActiveKilledFX != nullptr)
				{
					TArray<UParticleSystemComponent*> ParticleComps;
					ActiveKilledFX->GetComponents<UParticleSystemComponent>(ParticleComps);
					float NumParticles = ParticleComps.Num();
					if (NumParticles > 0)
					{
						UParticleSystemComponent* Parti = ParticleComps[0];
						if (Parti != nullptr)
						{
							FLinearColor PlayerColor = GetCharacterColor().ReinterpretAsLinear();
							Parti->SetColorParameter(FName("InitialColor"), PlayerColor);
						}
					}
				}*/
			}
		}

		// Update player pitch
		if (PlayerSound != nullptr)
		{
			float VectorScale = FMath::Sqrt(GetCharacterMovement()->Velocity.Size()) * 0.033f;
			float Scalar = FMath::Clamp(VectorScale, 0.2f, 4.0f) * 0.5f;
			PlayerSound->SetPitchMultiplier(Scalar);
		}
	}
}



//////////////////////////////////////////////////////////////////////
// MOVEMENT & FIGHTING

// MOVE	LEFT / RIGHT
void AGammaCharacter::MoveRight(float Value)
{
	float MoveByDot = 0.0f;

	if (!bSliding && (Timescale > 0.215f))
	{
		FVector MoveInput = FVector(InputX, 0.0f, InputZ).GetSafeNormal();
		FVector CurrentV = GetMovementComponent()->Velocity;
		FVector VNorm = CurrentV.GetSafeNormal();

		// Move by dot product for skating effect
		if ((MoveInput != FVector::ZeroVector)
			&& (Controller != nullptr))
		{
			
			float DotToInput = (FVector::DotProduct(MoveInput, VNorm));
			float AngleToInput = FMath::Acos(DotToInput);
			AngleToInput = FMath::Clamp(AngleToInput, 1.0f, 1000.0f);

			// Effect Move
			float TurnScalar = MoveSpeed + FMath::Square(TurnSpeed * AngleToInput);
			float DeltaTime = GetWorld()->DeltaTimeSeconds;
			MoveByDot = MoveSpeed * TurnScalar;
			//AddMovementInput(FVector(1.0f, 0.0f, 0.0f), InputX * MoveByDot, true);
			AddMovementInput(FVector(1.0f, 0.0f, 0.0f), InputX * DeltaTime * MoveSpeed * MoveByDot);
		}
	}

	if (!ActorHasTag("Bot"))
	{
		SetX(Value, MoveByDot);
	}
}

// MOVE UP / DOWN
void AGammaCharacter::MoveUp(float Value)
{
	float MoveByDot = 0.0f;

	if (!bSliding && (Timescale > 0.215f))
	{
		FVector MoveInput = FVector(InputX, 0.0f, InputZ).GetSafeNormal();
		FVector CurrentV = GetMovementComponent()->Velocity;
		FVector VNorm = CurrentV.GetSafeNormal();

		// Move by dot product for skating effect
		if ((MoveInput != FVector::ZeroVector)
			&& (Controller != nullptr))
		{
			
			float DotToInput = (FVector::DotProduct(MoveInput, VNorm));
			float AngleToInput = FMath::Acos(DotToInput);
			AngleToInput = FMath::Clamp(AngleToInput, 1.0f, 1000.0f);

			// Effect move
			float TurnScalar = MoveSpeed + FMath::Square(TurnSpeed * AngleToInput);
			float DeltaTime = GetWorld()->DeltaTimeSeconds;
			MoveByDot = MoveSpeed * TurnScalar;
			//AddMovementInput(FVector(0.0f, 0.0f, 1.0f), InputZ * MoveByDot, true);
			AddMovementInput(FVector(0.0f, 0.0f, 1.0f), InputZ * DeltaTime * MoveSpeed * MoveByDot);
		}
	}

	if (!ActorHasTag("Bot"))
	{
		SetZ(Value, MoveByDot);
	}
}

// SET X & Z SERVERSIDE
void AGammaCharacter::SetX(float Value, float MoveScalar)
{
	if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.01f)
	{
		InputX = Value;
		if (Value != 0.0f)
		{
			x = Value;
		}
		
		/*if (GetCharacterMovement() != nullptr)
		{
			GetCharacterMovement()->MaxAcceleration = MoveScalar * Timescale;
		}*/

		///GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, FString::Printf(TEXT("x: %f"), InputX));

		if (ActorHasTag("Bot"))
		{
			MoveRight(InputX);
		}
	}

	if (Role < ROLE_Authority)
	{
		ServerSetX(Value, MoveScalar);
	}
}
void AGammaCharacter::ServerSetX_Implementation(float Value, float MoveScalar)
{
	SetX(Value, MoveScalar);
}
bool AGammaCharacter::ServerSetX_Validate(float Value, float MoveScalar)
{
	return true;
}


void AGammaCharacter::SetZ(float Value, float MoveScalar)
{
	if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.01f)
	{
		InputZ = Value;
		if (Value != 0.0f)
		{
			z = Value;
		}
		
		
		/*if (GetCharacterMovement() != nullptr)
		{
			GetCharacterMovement()->MaxAcceleration = MoveScalar * Timescale;
		}*/

		///GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, FString::Printf(TEXT("z: %f"), InputZ));

		if (ActorHasTag("Bot"))
		{
			MoveUp(InputZ);
		}
	}

	if (Role < ROLE_Authority)
	{
		ServerSetZ(Value, MoveScalar);
	}
}
void AGammaCharacter::ServerSetZ_Implementation(float Value, float MoveScalar)
{
	SetZ(Value, MoveScalar);
}
bool AGammaCharacter::ServerSetZ_Validate(float Value, float MoveScalar)
{
	return true;
}

// BOOST KICK HUSTLE DASH
void AGammaCharacter::NewMoveKick()
{
	if (Role < ROLE_Authority)
	{
		ServerNewMoveKick();
	}
	else if (!bBoosting)
	{
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
	if (Role < ROLE_Authority)
	{
		ServerDisengageKick();
	}
	
	// Clear existing boost object
	if (GetActiveBoost() != nullptr)
	{
		if (bSliding || (GetActiveBoost()->GetGameTimeSinceCreation() >= MoveFreshMultiplier))
		{
			ActiveBoost->Destroy();
			ActiveBoost = nullptr;

			bBoosting = false;
			bCharging = false;

			GetCharacterMovement()->MaxFlySpeed = MaxMoveSpeed;

			PlayerSound->Stop();

			// Clear existing charge object
			if (ActiveChargeParticles != nullptr)
			{
				ActiveChargeParticles->Destroy();
				ActiveChargeParticles = nullptr;
			}
		}
	}
	if (ActiveChargeParticles != nullptr)
	{
		if (bSliding || (ActiveChargeParticles->GetGameTimeSinceCreation() >= MoveFreshMultiplier))
		{
			ActiveChargeParticles->Destroy();
			ActiveChargeParticles = nullptr;
		}
	}
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
	// Attack cancel
	if (ActiveAttack != nullptr)
	{
		float AttackLiveTime = ActiveAttack->GetGameTimeSinceCreation();
		if (AttackLiveTime >= 1.0f)
		{
			ActiveAttack->Destroy();
			ActiveAttack = nullptr;
		}
	}

	// Basic propulsive ingredients
	FVector CurrentVelocity = GetCharacterMovement()->Velocity;
	FVector MoveInputVector = FVector(InputX, 0.0f, InputZ * 0.75f).GetSafeNormal();
	FVector KickVector = MoveInputVector;

	// Basic thrust if no input
	if ((MoveInputVector == FVector::ZeroVector)
		|| (MoveInputVector.X == 0.0f))
	{
		MoveInputVector.X = GetActorForwardVector().X;
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, FString::Printf(TEXT("Wild boosting  %f"), 1.0f));
	}

	// Air-dodge if handbraking
	if (bSliding)
	{
		GetCharacterMovement()->AddImpulse(KickVector * 500000.0f);
		DisengageKick();
		bBoosting = false;
		bCharging = false;
		return;
	}
	
	// Force, clamp, & effect chara movement
	float DeltaTime = GetWorld()->DeltaTimeSeconds;
	float FreshKickSpeed = 1250000.0f;
	KickVector = MoveInputVector * FreshKickSpeed;
	KickVector.Z *= 0.9f;
	KickVector.Y = 0.0f;
	KickVector -= (CurrentVelocity * 0.5f);
	KickVector *= DeltaTime;

	///GEngine->AddOnScreenDebugMessage(-1, 0.1f, FColor::Cyan, FString::Printf(TEXT("kick   %f"), KickVector.Size()));

	// Initial kick for good feels
	if ((GetActiveBoost() == nullptr) && (BoostClass != nullptr))
	{
		GetCharacterMovement()->AddImpulse(KickVector);

		if (HasAuthority())
		{
			// Set up Kick Visuals direction
			FActorSpawnParameters SpawnParams;
			FRotator InputRotator = MoveInputVector.Rotation();
			FVector SpawnLocation = GetActorLocation() + (FVector::UpVector * 10.0f); /// + (PlayerVelocity / 3);

			ActiveBoost = GetWorld()->SpawnActor<AActor>
				(BoostClass, SpawnLocation, InputRotator, SpawnParams); /// PlayerVelRotator
		}
	}

	// Sustained Propulsion
	if ((GetActiveBoost() != nullptr) && (ActiveChargeParticles != nullptr)
		&& (GetCharacterMovement() != nullptr))
	{
		GetCharacterMovement()->AddImpulse(KickVector * 0.5f);

		//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, FString::Printf(TEXT("MoveInputVector  %f"), MoveInputVector.Size()));
		//GEngine->AddOnScreenDebugMessage(-1, 10.5f, FColor::Cyan, FString::Printf(TEXT("dot  %f"), DotScalar));
		//GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan, FString::Printf(TEXT("mass  %f"), GetCharacterMovement()->Mass));
		///GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, FString::Printf(TEXT("RelativityToMaxSpeed  %f"), RelativityToMaxSpeed));
		
	}

	if (GetActiveBoost() != nullptr)
	{
		if (GetActiveBoost()->GetGameTimeSinceCreation() > MoveFreshMultiplier)
		{
			DisengageKick();
		}
	}
	
	if (Role < ROLE_Authority)
	{
		ServerKickPropulsion();
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


// RECEIVING DAMAGE
void AGammaCharacter::ReceiveDamage(float Dmg)
{
	// damage flashout etc other fx

	if (Role < ROLE_Authority)
	{
		ServerReceiveDamage(Dmg);
	}
}
void AGammaCharacter::ServerReceiveDamage_Implementation(float Dmg)
{
	ReceiveDamage(Dmg);
}
bool AGammaCharacter::ServerReceiveDamage_Validate(float Dmg)
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
	if (Role < ROLE_Authority)
	{
		ServerRaiseCharge();
	}
	else
	{
		if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.01f)
		{
			// Noobish recovery from empty-case -1 charge
			if (Charge < 0.0f) {
				Charge = 0.0f;
			}

			// Charge growth
			if ((Charge <= (ChargeMax))
				&& ((ActiveAttack == nullptr) || ActiveAttack->IsPendingKillOrUnreachable()))
			{
				Charge += (Charge / 100.0f) + (ChargeGain * GetWorld()->DeltaTimeSeconds);

				if (Charge < ChargeMax)
				{
					bCharging = true;
				}
				else
				{
					bCharging = false;
					Charge = ChargeMax;
				}
			}

			// Sprite Scaling
			float ClampedCharge = FMath::Clamp(Charge * 0.7f, 1.0f, ChargeMax);
			float SCharge = FMath::Sqrt(ClampedCharge);
			FVector ChargeSize = FVector(SCharge, SCharge, SCharge);
			GetCapsuleComponent()->SetWorldScale3D(ChargeSize);

			// visual charge vfx
			if (HasAuthority() && (ChargeParticles != nullptr) && (ActiveChargeParticles == nullptr))
			{
				FActorSpawnParameters SpawnParams;
				ActiveChargeParticles = Cast<AActor>(GetWorld()->SpawnActor<AActor>(ChargeParticles, GetActorLocation(), GetActorRotation(), SpawnParams));
				if (ActiveChargeParticles != nullptr)
				{
					ActiveChargeParticles->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
					ActiveChargeParticles->SetActorScale3D(ChargeSize);
				}
			}

			// Move boost - activate if no boost yet
			if ((GetActiveBoost() == nullptr)) // (FVector(InputX, 0.0f, InputZ) != FVector::ZeroVector) &&
			{
				NewMoveKick();
			}

			// Sound fx
			if ((PlayerSound != nullptr)
				&& !PlayerSound->IsPlaying())
			{
				//PlayerSound->SetPitchMultiplier(Charge * 0.3f);
				PlayerSound->Play();
			}
		}

		// Catch misfire - if we were shooting, etc
		/*if ((ActiveChargeParticles == nullptr) || (!IsValid(ActiveChargeParticles)))
		{
			bCharging = true;
		}*/
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
	if (Role < ROLE_Authority)
	{
		ServerInitAttack();
	}
	else
	{
		// If we're shooting dry, trigger ChargeBar warning by going sub-zero
		if (Charge <= 0.0f)
		{
			Charge = (-1.0f);
		}

		bool bCancelling = false;
		// PROHIBITIVE Attack cancel for single shooters already shooting
		if ((ActiveAttack != nullptr) && !bMultipleAttacks)
		{
			bCancelling = true;
			return;
		}

		// Conditions for shooting
		bool bWeaponAllowed = !bCancelling 
			&& ((ActiveAttack == nullptr) || bMultipleAttacks) 
			&& (ActiveSecondary == nullptr);
		bool bFireAllowed = bWeaponAllowed 
			&& (GetActiveFlash() == nullptr)
			&& (FlashClass != nullptr);
		/// Extra bools for action heirarchy, currently moving away from this...
		/// (GetActiveBoost() == nullptr) &&  && (!IsValid(GetActiveFlash()))

		if (bFireAllowed)
		{
			// Direction & setting up
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
			if (HasAuthority() && (FlashClass != nullptr))
			{
				// Spawn eye heat
				FActorSpawnParameters SpawnParams;
				ActiveFlash = Cast<AGFlash>(GetWorld()->SpawnActor<AGFlash>(FlashClass, FirePosition, FireRotation, SpawnParams));
				ActiveFlash->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
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
	if (Role < ROLE_Authority)
	{
		ServerReleaseAttack();
	}
	else
	{
		if ((GetActiveFlash() != nullptr)
			&& (AttackClass != nullptr)
			&& ((ActiveAttack == nullptr) || bMultipleAttacks)
			&& (GetActiveSecondary() == nullptr)
			&& (PrefireTimer > 0.0f)
			&& (UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) > 0.01f))
		{

			// Aim by InputY
			float AimClampedInputZ = FMath::Clamp((InputZ * 10.0f), -1.0f, 1.0f);
			FVector FirePosition = AttackScene->GetComponentLocation();
			FVector LocalForward = AttackScene->GetForwardVector();
			LocalForward.Y = 0.0f;
			FRotator FireRotation = LocalForward.Rotation() + FRotator(InputZ * 21.0f, 0.0f, 0.0f); /// AimClampedInputZ
			FireRotation.Yaw = GetActorRotation().Yaw;
			if (FMath::Abs(FireRotation.Yaw) >= 90.0f)
			{
				FireRotation.Yaw = 180.0f;
			}
			else
			{
				FireRotation.Yaw = 0.0f;
			}

			// Scale prefire output's minimum by missing HP
			float MissingLife = FMath::Clamp((MaxHealth - Health), 0.1f, MaxHealth);
			float PrefireVal = PrefireTimer * (GetChargePercentage() + 0.05f);
			float PrefireMin = (MissingLife / 100.0f) + 0.1f;
			PrefireVal = FMath::Clamp(PrefireVal, PrefireMin, 1.0f);
			///GEngine->AddOnScreenDebugMessage(-1, 5.5f, FColor::White, FString::Printf(TEXT("PrefireMin: %f"), PrefireMin));

			// Spawning
			if (HasAuthority() && (ActiveAttack == nullptr))
			{
				if ((AttackClass != nullptr) || bMultipleAttacks)
				{
					FActorSpawnParameters SpawnParams;
					ActiveAttack = Cast<AGAttack>(GetWorld()->SpawnActor<AGAttack>(AttackClass, FirePosition, FireRotation, SpawnParams));
					if (ActiveAttack != nullptr)
					{
						
						// The attack is born
						if (ActiveAttack != nullptr)
						{
							ActiveAttack->InitAttack(this, PrefireVal, AimClampedInputZ);
						}

						// Position lock, or naw
						if ((ActiveAttack != nullptr) && ActiveAttack->LockedEmitPoint)
						{
							ActiveAttack->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
						}

						// Clean up previous flash
						if ((GetActiveFlash() != nullptr))
						{
							ActiveFlash->Destroy();
							ActiveFlash = nullptr;
						}

						// Slowing the character on fire
						if (GetCharacterMovement() != nullptr)
						{
							GetCharacterMovement()->Velocity *= SlowmoMoveBoost;
						}

						// Break handbrake
						CheckPowerSlideOff();
					}
				}
				else
				{
					GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("No attack class to spawn"));
				}

				// Spend it!
				float ChargeSpend = 1.0f;
				/// More magnitude -> higher cost
				/*if (PrefireTimer >= 0.33f)
				{
					float BigSpend = FMath::FloorToFloat(ChargeMax * PrefireTimer);
					ChargeSpend = BigSpend;
				}*/
				Charge -= ChargeSpend;


				// Sprite Scaling
				float ClampedCharge = FMath::Clamp(Charge * 0.7f, 1.0f, ChargeMax);
				float SCharge = FMath::Sqrt(ClampedCharge);
				FVector ChargeSize = FVector(SCharge, SCharge, SCharge);
				GetCapsuleComponent()->SetWorldScale3D(ChargeSize);
			}

			PrefireTimer = 0.0f;
		}

		// If we're 'shooting dry', notify the chargebar to flash
		if (Charge <= 0.0f)
		{
			Charge = -1.0f;
			return;
		}
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
		&& (UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) > 0.01f))
	{
		// Cancel the Flash and Attack
		if (GetActiveFlash() != nullptr)
		{
			ActiveFlash->Destroy();
			ActiveFlash = nullptr;
		}
		// Attack cancel is prohibitve
		if (ActiveAttack != nullptr)
		{
			ActiveAttack->Destroy();
			ActiveAttack = nullptr;
		}

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
					PotentialAttack->InitAttack(this, 1.0f, InputZ);

					if (PotentialAttack != nullptr && PotentialAttack->LockedEmitPoint)
					{
						PotentialAttack->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
					}

					bShielding = false;
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


// TIMESCALE RECOVERY
void AGammaCharacter::RecoverTimescale(float DeltaTime)
{
	float Global = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
	if (Global < 1.0f)
	{
		Timescale = 1.0f;
		CustomTimeDilation = 1.0f;
	}

	if (CustomTimeDilation != 1.0f)
	{
		// Personal Timescale
		Timescale = CustomTimeDilation;

		/*if (GetActiveFlash() != nullptr)
		{
			GetActiveFlash()->CustomTimeDilation = Timescale;
			float NewLife = GetActiveFlash()->GetLifeSpan() * (1.0f / Timescale);
			GetActiveFlash()->SetLifeSpan(NewLife);
		}
		if (ActiveAttack != nullptr)
		{
			ActiveAttack->CustomTimeDilation = Timescale;
			float NewLife = ActiveAttack->GetLifeSpan() * (1.0f / Timescale);
			ActiveAttack->SetLifeSpan(NewLife);
			ActiveAttack->ForceNetUpdate();
		}
		if (ActiveSecondary != nullptr)
		{
			ActiveSecondary->CustomTimeDilation = Timescale;
			float NewLife = ActiveSecondary->GetLifeSpan() * (1.0f / Timescale);
			ActiveSecondary->SetLifeSpan(NewLife);
		}*/

		// Personal Recovery
		float ReturnTime = FMath::FInterpTo(Timescale, 1.0f, DeltaTime, Timescale * MoveSpeed);
		CustomTimeDilation = FMath::Clamp(ReturnTime, 0.1f, 1.0f);

		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, FString::Printf(TEXT("RECOVERING: %f"), CustomTimeDilation));
	}

	if (Role < ROLE_Authority)
	{
		ServerRecoverTimescale(DeltaTime);
	}
}
void AGammaCharacter::ServerRecoverTimescale_Implementation(float DeltaTime)
{
	RecoverTimescale(DeltaTime);

	if (CustomTimeDilation != 1.0f)
	{
		// Personal Timescale
		Timescale = CustomTimeDilation;

		/*if (GetActiveFlash() != nullptr)
		{
			GetActiveFlash()->CustomTimeDilation = Timescale;
			float NewLife = GetActiveFlash()->GetLifeSpan() * (1.0f / Timescale);
			GetActiveFlash()->SetLifeSpan(NewLife);
		}
		if (ActiveAttack != nullptr)
		{
			ActiveAttack->CustomTimeDilation = Timescale;
			float NewLife = ActiveAttack->GetLifeSpan() * (1.0f / Timescale);
			ActiveAttack->SetLifeSpan(NewLife);
			ActiveAttack->ForceNetUpdate();
		}
		if (ActiveSecondary != nullptr)
		{
			ActiveSecondary->CustomTimeDilation = Timescale;
			float NewLife = ActiveSecondary->GetLifeSpan() * (1.0f / Timescale);
			ActiveSecondary->SetLifeSpan(NewLife);
		}*/

		// Personal Recovery
		float ReturnTime = FMath::FInterpTo(Timescale, 1.0f, DeltaTime, Timescale * MoveSpeed);
		CustomTimeDilation = FMath::Clamp(ReturnTime, 0.1f, 1.0f);

		ForceNetUpdate();
	}
}
bool AGammaCharacter::ServerRecoverTimescale_Validate(float DeltaTime)
{
	return true;
}


// Setter for Timescale from remote places i.e Match
void AGammaCharacter::NewTimescale(float Value)
{
	GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, FString::Printf(TEXT("NEW TIMESCALE WAS CALLED: %f"), CustomTimeDilation));

	if (Role < ROLE_Authority)
	{
		ServerNewTimescale(Value);
		ForceNetUpdate();
	}
	else
	{
		CustomTimeDilation = Value;

		if ((ActiveAttack != nullptr) && HasAuthority())
		{
			ActiveAttack->CustomTimeDilation = Value;
			UParticleSystemComponent* ParticleComp = ActiveAttack->GetParticles();
			if (ParticleComp != nullptr)
			{
				ParticleComp->CustomTimeDilation = Value;
			}
			float NewLife = ActiveAttack->GetLifeSpan() * (1.0f / CustomTimeDilation);
			ActiveAttack->SetLifeSpan(NewLife);

			ActiveAttack->ForceNetUpdate();
		}
		if (ActiveSecondary != nullptr)
		{
			ActiveSecondary->CustomTimeDilation = Value;
			UParticleSystemComponent* ParticleComp = ActiveSecondary->GetParticles();
			if (ParticleComp != nullptr)
			{
				ParticleComp->CustomTimeDilation = (Value);
			}
			float NewLife = ActiveSecondary->GetLifeSpan() * (1.0f / CustomTimeDilation);
			ActiveSecondary->SetLifeSpan(NewLife);
		}
		if (GetActiveBoost() != nullptr)
		{
			GetActiveBoost()->CustomTimeDilation = Value;
			float NewLife = GetActiveBoost()->GetLifeSpan() * (1.0f / CustomTimeDilation);
			GetActiveBoost()->SetLifeSpan(NewLife);
		}
		if (GetActiveFlash() != nullptr)
		{
			GetActiveFlash()->CustomTimeDilation = Value;
			float NewLife = GetActiveFlash()->GetLifeSpan() * (1.0f / CustomTimeDilation);
			GetActiveFlash()->SetLifeSpan(NewLife);
		}

		if (GetMovementComponent() != nullptr)
		{
			float VelocityTimeScalar = FMath::Clamp(Value, 0.8f, 1.0f);
			GetMovementComponent()->Velocity *= VelocityTimeScalar;
		}

		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::White, FString::Printf(TEXT("set Timescale:  %f"), Value));
	}
}
void AGammaCharacter::ServerNewTimescale_Implementation(float Value)
{
	NewTimescale(Value);
}
bool AGammaCharacter::ServerNewTimescale_Validate(float Value)
{
	return true;
}


// PREFIRE TIMING
void AGammaCharacter::PrefireTiming()
{
	if ((GetActiveFlash() != nullptr)
		&& (PrefireTimer < PrefireTime))
	{
		PrefireTimer += GetWorld()->GetDeltaSeconds();
		///GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::White, FString::Printf(TEXT("Prefire: %f"), PrefireTimer));
	}
	else if ((PrefireTimer >= PrefireTime)
		&& (Charge > 0)
		&& (ActiveAttack == nullptr)
		&& (ActiveFlash != nullptr))
	{
		ReleaseAttack();
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


// ATTACK PRIMING
void AGammaCharacter::CheckAttackOn()
{
	ArmAttack();
}

void AGammaCharacter::CheckAttackOff()
{
	if ((PrefireTimer > 0.0f) || (GetActiveFlash() != nullptr))
	{
		ReleaseAttack();
	}

	DisarmAttack();
}


void AGammaCharacter::ArmAttack()
{
	if (!bShooting)
	{
		bShooting = true;
	}

	if (Role < ROLE_Authority)
	{
		ServerArmAttack();
	}
}
void AGammaCharacter::ServerArmAttack_Implementation()
{
	ArmAttack();
}
bool AGammaCharacter::ServerArmAttack_Validate()
{
	return true;
}

void AGammaCharacter::DisarmAttack()
{
	if (bShooting)
	{
		bShooting = false;
	}

	if (Role < ROLE_Authority)
	{
		ServerDisarmAttack();
	}
}
void AGammaCharacter::ServerDisarmAttack_Implementation()
{
	DisarmAttack();
}
bool AGammaCharacter::ServerDisarmAttack_Validate()
{
	return true;
}


// SECONDARY PRIMING
void AGammaCharacter::CheckSecondaryOn()
{
	if (!bShielding)
	{
		if (ActiveSecondary == nullptr)
		{
			ArmSecondary();
		}
		else
		{
			CheckSecondaryOff();
		}
	}
}
void AGammaCharacter::CheckSecondaryOff()
{
	if (bShielding)
	{
		DisarmSecondary();
	}
}

void AGammaCharacter::ArmSecondary()
{
	if (!bShielding)
	{
		bShielding = true;
	}

	if (Role < ROLE_Authority)
	{
		ServerArmSecondary();
	}
}
void AGammaCharacter::ServerArmSecondary_Implementation()
{
	ArmSecondary();
}
bool AGammaCharacter::ServerArmSecondary_Validate()
{
	return true;
}

void AGammaCharacter::DisarmSecondary()
{
	if (bShielding)
	{
		bShielding = false;
	}

	if (Role < ROLE_Authority)
	{
		ServerDisarmSecondary();
	}
}
void AGammaCharacter::ServerDisarmSecondary_Implementation()
{
	DisarmSecondary();
}
bool AGammaCharacter::ServerDisarmSecondary_Validate()
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
		DisengageKick();
		
		// Neutralizing charge
		// Armed player goes to -1, triggering a warning
		/*if (Charge > 0.0f)
		{
			Charge = -1.0f;
		}
		else
		{
			Charge = 0.0f;
		}*/

		//// Attack cancel
		if (GetActiveFlash() != nullptr)
		{
			ClearFlash();
		}
		if (ActiveAttack != nullptr)
		{
			ActiveAttack->Destroy();
			ActiveAttack = nullptr;
		}
		PrefireTimer = 0.0f;
		bShooting = false;

		// Sprite Scaling
		float ClampedCharge = FMath::Clamp(Charge * 0.7f, 1.0f, ChargeMax);
		float SCharge = FMath::Sqrt(ClampedCharge);
		FVector ChargeSize = FVector(SCharge, SCharge, SCharge);
		GetCapsuleComponent()->SetWorldScale3D(ChargeSize);

		// Zoom camera
		// Disengage resets to 5x to restore
		CameraMoveSpeed *= 0.2f;

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

		// Engage sets to 0.2, so we restore
		CameraMoveSpeed *= 5.0f;

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
	// Taking damage
	//Health = FMath::Clamp(Health += Value, -1.0f, 100.0f);

	if (Value >= 100.0f)
	{
		Health = 100.0f;
		MaxHealth = 100.0f;
	}
	else
	{
		MaxHealth = FMath::Clamp(Health + Value, -1.0f, 100.0f);
	}

	
	///GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Red, FString::Printf(TEXT("MaxHealth: %f"), MaxHealth));

	//float InterpSpeed = FMath::Abs(Value);
	//Health = FMath::FInterpTo(Health, -1.0f, UGameplayStatics::GetWorldDeltaSeconds(GetWorld()), InterpSpeed);

	//// Player killed fx
	//if ((Health <= 0.0f)
	//	&& (KilledFX != nullptr))
	//{
	//	FActorSpawnParameters SpawnParams;
	//	AActor* NewKilledFX = GetWorld()->SpawnActor<AActor>(KilledFX, GetActorLocation(), GetActorRotation(), SpawnParams);
	//	if (NewKilledFX != nullptr)
	//	{
	//		TArray<UParticleSystemComponent*> ParticleComps;
	//		NewKilledFX->GetComponents<UParticleSystemComponent>(ParticleComps);
	//		float NumParticles = ParticleComps.Num();
	//		if (NumParticles > 0)
	//		{
	//			UParticleSystemComponent* Parti = ParticleComps[0];
	//			if (Parti != nullptr)
	//			{
	//				FLinearColor PlayerColor = GetCharacterColor().ReinterpretAsLinear();
	//				Parti->SetColorParameter(FName("InitialColor"), PlayerColor);
	//			}
	//		}
	//	}
	//}

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
	CameraBoom->TargetArmLength = 2000.0f;
}


////////////////////////////////////////////////////////////////////////
// NETWORKED PROPERTY REPLICATION
void AGammaCharacter::GetLifetimeReplicatedProps(TArray <FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGammaCharacter, Health);
	DOREPLIFETIME(AGammaCharacter, Score);
	DOREPLIFETIME(AGammaCharacter, Timescale);

	DOREPLIFETIME(AGammaCharacter, InputX);
	DOREPLIFETIME(AGammaCharacter, InputZ);
	DOREPLIFETIME(AGammaCharacter, x);
	DOREPLIFETIME(AGammaCharacter, z);
	DOREPLIFETIME(AGammaCharacter, MoveTimer);
	DOREPLIFETIME(AGammaCharacter, bSliding);
	DOREPLIFETIME(AGammaCharacter, bBoosting);
	DOREPLIFETIME(AGammaCharacter, bShooting);
	DOREPLIFETIME(AGammaCharacter, bShielding);

	DOREPLIFETIME(AGammaCharacter, Charge);
	DOREPLIFETIME(AGammaCharacter, bCharging);
}


// Print to screen
// GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::White, TEXT("sizing"));
// GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::White, FString::Printf(TEXT("Charge: %f"), Charge));