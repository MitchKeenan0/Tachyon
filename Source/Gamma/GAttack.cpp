// Fill out your copyright notice in the Description page of Project Settings.

#include "GAttack.h"
#include "GMatch.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ParticleDefinitions.h"
#include "Particles/ParticleSystem.h"
#include "PaperSpriteComponent.h"

AGAttack::AGAttack()
{
 	PrimaryActorTick.bCanEverTick = true;

	AttackScene = CreateDefaultSubobject<USceneComponent>(TEXT("AttackScene"));
	SetRootComponent(AttackScene);

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

	SetLifeSpan(DurationTime);

	DetectHit(GetActorForwardVector());
}


void AGAttack::InitAttack(AActor* Shooter, float Magnitude, float YScale)
{
	// Set local variables
	OwningShooter = Shooter;
	AttackMagnitude = Magnitude;
	ShotDirection = YScale;

	// set sounds
	AttackSound->SetPitchMultiplier(AttackSound->PitchMultiplier - (AttackMagnitude / 1.5f));
	AttackSound->SetVolumeMultiplier(1.0f + (AttackMagnitude * 1.25f));
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("Pitch After: %f"), AttackSound->PitchMultiplier));

	// Lifespan
	if (AttackMagnitude > 0.15f)
	{
		SetLifeSpan(DurationTime * (1.0f + AttackMagnitude));
	}

	// Projectile movement
	if (ProjectileSpeed > 0.0f)
	{
		ProjectileComponent->Velocity = GetActorForwardVector() * ProjectileSpeed * AttackMagnitude * ProjectileMaxSpeed;
	}

	//// Last-second update to direction after fire
	float DirRecalc = ShotDirection;
	if (AngleSweep != 0.0f)
	{
		DirRecalc *= -3.0f;
	}
	FVector LocalForward = GetActorForwardVector().ProjectOnToNormal(FVector::ForwardVector);
	FRotator FireRotation = LocalForward.Rotation() + FRotator(21.0f * DirRecalc, 0.0f, 0.0f);
	SetActorRotation(FireRotation);
	
	// Get match obj
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGMatch::StaticClass(), Actors);
	CurrentMatch = Cast<AGMatch>(Actors[0]);

	// Color & cosmetics
	/*FlashMesh->SetMaterial(0, MainMaterial);
	AttackMesh->SetMaterial(0, MainMaterial);*/

	DetectHit(GetActorForwardVector());
}


// Called every frame
void AGAttack::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Healthy attack activities
	if (bLethal)
	{
		UpdateAttack(DeltaTime);
	}
}


void AGAttack::UpdateAttack(float DeltaTime)
{
	LifeTimer += DeltaTime;
	HitTimer += DeltaTime;

	/*if (!bHit)
	{
		DetectHit(GetActorForwardVector());
	}*/

	DetectHit(GetActorForwardVector());
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
	
	if (bRaycastOnMesh)
	{
		float DistX = (AttackSprite->Bounds.BoxExtent.X) * 2.f;
		Start	= GetActorLocation();
		End		= Start + (RaycastVector * DistX);
	}
	
	// Pew pew
	bool HitResult = UKismetSystemLibrary::LineTraceSingleForObjects(
											this,
											Start,
											End,
											TraceObjects,
											false,
											IgnoredActors,
											EDrawDebugTrace::ForDuration,
											Hit,
											true,
											FLinearColor::Red, FLinearColor::White, 0.15f);
	
	if (Hit.Actor.Get())
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

	// finally shooting
	if (bHit && (HitTimer >= (1.0f / HitsPerSecond)))
	{
		SpawnDamage(HitActor, HitActor->GetActorLocation());
		ApplyKnockback(HitActor);
		TakeGG();
		HitTimer = 0.0f;
		bHit = false;
	}
}


void AGAttack::SpawnDamage(AActor* HitActor, FVector HitPoint)
{
	if (DamageClass)
	{
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
	ACharacter* Chara = Cast<ACharacter>(HitActor);
	if (Chara)
	{
		FVector AwayFromShooter = (HitActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		Chara->GetCharacterMovement()->AddImpulse(AwayFromShooter * KineticForce);
	}
}


void AGAttack::TakeGG()
{
	if (CurrentMatch)
	{
		CurrentMatch->ClaimGG(OwningShooter);
	}
}


// NETWORK VARIABLES
void AGAttack::GetLifetimeReplicatedProps(TArray <FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGAttack, bHit);
	DOREPLIFETIME(AGAttack, bStillHitting);
	DOREPLIFETIME(AGAttack, bDoneHitting);
	DOREPLIFETIME(AGAttack, OwningShooter);
	DOREPLIFETIME(AGAttack, HitActor);
	DOREPLIFETIME(AGAttack, AttackMagnitude);
	DOREPLIFETIME(AGAttack, ShotDirection);
	DOREPLIFETIME(AGAttack, AttackDamage);
}
