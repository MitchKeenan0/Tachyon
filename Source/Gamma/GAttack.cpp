// Fill out your copyright notice in the Description page of Project Settings.

#include "GAttack.h"
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
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("Pitch After: %f"), AttackSound->PitchMultiplier));

	// Lifespan
	if (AttackMagnitude > 0.3f)
	{
		SetLifeSpan(DurationTime * (1.0f + AttackMagnitude));
	}

	// Projectile movement
	if (ProjectileSpeed > 0.0f)
	{
		ProjectileComponent->Velocity = GetActorForwardVector() * ProjectileSpeed * AttackMagnitude * ProjectileMaxSpeed;
	}

	// Last-second update to direction after fire
	float DirRecalc = ShotDirection;
	if (AngleSweep != 0.0f)
	{
		DirRecalc *= -3.14f;
	}
	FVector LocalForward = GetActorForwardVector().ProjectOnToNormal(FVector::ForwardVector);
	FRotator FireRotation = LocalForward.Rotation() + FRotator(21.0f * DirRecalc, 0.0f, 0.0f);
	SetActorRotation(FireRotation);
	
	// Get match obj
	/*TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMatch::StaticClass(), Actors);
	CurrentMatch = Cast<AMatch>(Actors[0]);*/

	// Color & cosmetics
	/*FlashMesh->SetMaterial(0, MainMaterial);
	AttackMesh->SetMaterial(0, MainMaterial);*/
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

	if (!bHit)
	{
		DetectHit(GetActorForwardVector());
	}

}


void AGAttack::DetectHit(FVector RaycastVector)
{
	// Linecast ingredients
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjects;
	TraceObjects.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	TraceObjects.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	TraceObjects.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	TraceObjects.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
	TArray<AActor*> IgnoredActors;
	IgnoredActors.Add(OwningShooter);
	FVector Start = GetActorLocation() + GetActorForwardVector();
	FVector End = Start + (RaycastVector * RaycastHitRange);
	FHitResult Hit;
	
	if (bRaycastOnMesh)
	{
		float DistX = (AttackSprite->Bounds.BoxExtent.X) * 2.f;
		Start	= GetActorLocation();
		End		= Start + (RaycastVector * DistX);
	}
	else
	{
		Start	= GetActorLocation() + GetActorForwardVector();
		End		= Start + (RaycastVector * RaycastHitRange);
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
											FLinearColor::Red, FLinearColor::White, 0.15f);
	if (HitResult)
	{
		// do some checks to make sure its a player
		AActor* HitActor = Hit.Actor.Get();
		if (!HitActor->ActorHasTag("Attack"))
		{
			bHit = true;
			SpawnDamage(Hit.ImpactPoint, HitActor);
			ApplyKnockback(HitActor);
			TakeGG();
		}
	}
}


void AGAttack::SpawnDamage(FVector HitPoint, AActor* HitActor)
{
	if (DamageClass)
	{
		FActorSpawnParameters SpawnParams;
		FRotator Forward = GetActorForwardVector().Rotation();
		AGDamage* DmgObj = Cast<AGDamage>(GetWorld()->SpawnActor<AGDamage>(DamageClass, HitPoint, Forward, SpawnParams));
		DmgObj->AttachToActor(HitActor, FAttachmentTransformRules::KeepWorldTransform);
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
	//GEngine->AddOnScreenDebugMessage(-1, 2.2f, FColor::Blue, TEXT("Attack Hit!"));
	
}


// NETWORK VARIABLES
void AGAttack::GetLifetimeReplicatedProps(TArray <FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGAttack, bHit);
	DOREPLIFETIME(AGAttack, OwningShooter);
	DOREPLIFETIME(AGAttack, AttackMagnitude);
	DOREPLIFETIME(AGAttack, ShotDirection);
}
