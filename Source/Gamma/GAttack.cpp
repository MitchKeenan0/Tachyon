// Fill out your copyright notice in the Description page of Project Settings.

#include "GAttack.h"
#include "GMatch.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ParticleDefinitions.h"
#include "Particles/ParticleSystem.h"
#include "PaperSpriteComponent.h"

AGAttack::AGAttack()
{
 	PrimaryActorTick.bCanEverTick = true;

	AttackScene = CreateDefaultSubobject<USceneComponent>(TEXT("AttackScene"));
	SetRootComponent(AttackScene);

	CapsuleRoot = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleRoot"));
	CapsuleRoot->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	CapsuleRoot->OnComponentBeginOverlap.AddDynamic(this, &AGAttack::OnAttackBeginOverlap);
	//ShieldCollider->OnComponentBeginOverlap.AddDynamic(this, &AGammaCharacter::OnShieldBeginOverlap);

	

	AttackSound = CreateDefaultSubobject<UAudioComponent>(TEXT("AttackSound"));
	AttackSound->SetIsReplicated(true);

	ProjectileComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComponent"));

	AttackParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ChargeParticles"));
	AttackParticles->SetupAttachment(RootComponent);

	AttackSprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("AttackSprite"));
	AttackSprite->SetupAttachment(RootComponent);

	bReplicates = true;
	bReplicateMovement = true;
	AttackSprite->SetIsReplicated(true);
	ProjectileComponent->SetIsReplicated(true);
}



void AGAttack::BeginPlay()
{
	Super::BeginPlay();
	bHit = false;
	bLethal = false;
	SetLifeSpan(DurationTime);
}


void AGAttack::InitAttack(AActor* Shooter, float Magnitude, float YScale)
{
	// Set local variables
	OwningShooter = Shooter;
	AttackMagnitude = Magnitude;
	ShotDirection = YScale;

	// set sounds
	AttackSound->SetPitchMultiplier(AttackSound->PitchMultiplier - (AttackMagnitude / 1.5f));
	AttackSound->SetVolumeMultiplier(FMath::Clamp(1.0f + (AttackMagnitude * 1.25f), 0.1f, 1.5f));
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("Pitch After: %f"), AttackSound->PitchMultiplier));

	// Lifespan
	if (MagnitudeTimeScalar != 0.0f)
	{
		DynamicLifetime = DurationTime * (1.0f + (AttackMagnitude * MagnitudeTimeScalar));
		SetLifeSpan(DynamicLifetime);
	}
	else { DynamicLifetime = DurationTime; }

	//// Last-second update to direction after fire
	float DirRecalc = ShotDirection * ShootingAngle;
	if (AngleSweep != 0.0f)
	{
		DirRecalc *= -1.68f;
	}
	FVector LocalForward = GetActorForwardVector().ProjectOnToNormal(FVector::ForwardVector);
	FRotator FireRotation = LocalForward.Rotation() + FRotator(DirRecalc, 0.0f, 0.0f);
	SetActorRotation(FireRotation);

	// Projectile movement
	if (ProjectileSpeed > 0.0f)
	{
		ProjectileComponent->Velocity = GetActorForwardVector() * ProjectileSpeed;

		if (ProjectileComponent)
		{
			if (bScaleProjectileSpeed)
			{
				ProjectileComponent->Velocity = GetActorForwardVector() * ProjectileSpeed * AttackMagnitude * ProjectileMaxSpeed;
			}
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("no projectile comp"));
		}
	}

	// Get match obj
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGMatch::StaticClass(), Actors);
	CurrentMatch = Cast<AGMatch>(Actors[0]);

	// Color & cosmetics
	/*FlashMesh->SetMaterial(0, MainMaterial);
	AttackMesh->SetMaterial(0, MainMaterial);*/

	// Init success
	if (OwningShooter != nullptr)
	{
		bLethal = true;
		DetectHit(GetActorForwardVector());
	}
}


// Called every frame
void AGAttack::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	LifeTimer += DeltaTime;
	HitTimer += DeltaTime;

	// Healthy attack activities
	if (bLethal)
	{
		UpdateAttack(DeltaTime);
	}

	// End-of-life activities
	float CloseEnough = DynamicLifetime * 0.95f;
	if (LifeTimer >= CloseEnough)
	{
		AttackParticles->bSuppressSpawning = true;
		
		if (LifeTimer > CloseEnough + 0.03f)
		{
			AGammaCharacter* PotentialGamma = Cast<AGammaCharacter>(OwningShooter);
			if (PotentialGamma)
			{
				PotentialGamma->NullifyAttack();
				Destroy();
			}
		}
	}
}


void AGAttack::UpdateAttack(float DeltaTime)
{
	if (!bHit && OwningShooter != nullptr)
	{
		DetectHit(GetActorForwardVector());
	}
}


void AGAttack::DetectHit(FVector RaycastVector)
{
	// Linecast ingredients
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjects;
	TraceObjects.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	TraceObjects.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	TraceObjects.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	TraceObjects.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
	
	TArray<AActor*> IgnoredActors;
	IgnoredActors.Add(OwningShooter);
	
	FVector Start = GetActorLocation() + GetActorForwardVector();
	FVector End = Start + (RaycastVector * RaycastHitRange);
	End.Y = 0.0f; /// strange y-axis drift
	
	FHitResult Hit;
	
	// Swords, etc, get tangible ray space
	if (bRaycastOnMesh)
	{
		float AttackBodyLength = (AttackSprite->Bounds.BoxExtent.X) * RaycastHitRange;
		Start	= GetActorLocation() + (GetActorForwardVector());
		End		= Start + (RaycastVector * AttackBodyLength);
	}
	
	// Pew pew
	bool HitResult = UKismetSystemLibrary::LineTraceSingleForObjects(
											this,
											Start,
											End,
											TraceObjects,
											false,
											IgnoredActors,
											EDrawDebugTrace::None,
											Hit,
											true,
											FLinearColor::White, FLinearColor::Red, 0.15f);
	
	if (HitResult)
	{
		HitActor = Hit.Actor.Get();
		if (HitActor)
		{
			// do some checks to make sure its a player
			if (!HitActor->ActorHasTag("Attack"))
			{
				// good hit as they say
				bHit = true;
			}
		}
	}
	else
	{
		bHit = false;
	}

	// Consequences
	if (bHit)// && (HitTimer >= (1.0f / HitsPerSecond)))
	{
		// Damage vfx
		SpawnDamage(HitActor, HitActor->GetActorLocation());

		// Player killer
		ApplyKnockback(HitActor);
		TakeGG();

		// Clean-up for next frame
		HitTimer = 0.0f;
	}
}


void AGAttack::SpawnDamage(AActor* HitActor, FVector HitPoint)
{
	if (DamageClass)
	{
		// Spawning actor
		FActorSpawnParameters SpawnParams;
		FRotator Forward = (HitActor->GetActorLocation() - OwningShooter->GetActorLocation()).Rotation();
		AGDamage* DmgObj = Cast<AGDamage>(GetWorld()->SpawnActor<AGDamage>(DamageClass, HitPoint, Forward, SpawnParams));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("No Dmg class to spawn"));
	}
}


void AGAttack::ApplyKnockback(AActor* HitActor)
{
	// Get character movement to kick on
	ACharacter* Chara = Cast<ACharacter>(HitActor);
	if (Chara)
	{
		FVector AwayFromShooter = (HitActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		Chara->GetCharacterMovement()->AddImpulse(AwayFromShooter * KineticForce);
	}// !! IMPORTANT !! clamp this, future me. oh hai
}


void AGAttack::TakeGG()
{
	if (CurrentMatch)
	{
		CurrentMatch->ClaimGG(OwningShooter);
	}
}


// COLLISION BEGIN
void AGAttack::OnAttackBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bLethal && (OtherActor != OwningShooter))
	{
		if (OtherComp->ComponentHasTag("Player"))
		{
			///GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, TEXT("oh look a bridge"));
		}

		// Consequences
		if (!bHit)// && (HitTimer >= (1.0f / HitsPerSecond)))
		{
			bHit = true;

			// Damage vfx
			SpawnDamage(OtherActor, OtherActor->GetActorLocation());

			// Player killer
			ApplyKnockback(OtherActor);
			TakeGG();
		}
	}

	// attack should check for shield to make reflect fx
}


// NETWORK VARIABLES
void AGAttack::GetLifetimeReplicatedProps(TArray <FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGAttack, DynamicLifetime);
	DOREPLIFETIME(AGAttack, bHit);
	DOREPLIFETIME(AGAttack, bStillHitting);
	DOREPLIFETIME(AGAttack, bDoneHitting);
	DOREPLIFETIME(AGAttack, OwningShooter);
	DOREPLIFETIME(AGAttack, HitActor);
	DOREPLIFETIME(AGAttack, AttackMagnitude);
	DOREPLIFETIME(AGAttack, ShotDirection);
	DOREPLIFETIME(AGAttack, AttackDamage);
}
