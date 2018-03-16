// Fill out your copyright notice in the Description page of Project Settings.

#include "GAttack.h"
#include "GMatch.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ParticleDefinitions.h"
#include "Particles/ParticleSystem.h"
#include "PaperSpriteComponent.h"
//#include "PaperFlipbookComponent.h"

AGAttack::AGAttack()
{
 	PrimaryActorTick.bCanEverTick = true;

	AttackScene = CreateDefaultSubobject<USceneComponent>(TEXT("AttackScene"));
	SetRootComponent(AttackScene);

	CapsuleRoot = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleRoot"));
	CapsuleRoot->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	CapsuleRoot->OnComponentBeginOverlap.AddDynamic(this, &AGAttack::OnAttackBeginOverlap);
	//OuterTouchCollider->OnComponentBeginOverlap.AddDynamic(this, &AGammaCharacter::OnShieldBeginOverlap);

	

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

	// Add natural deviancy to sound
	if (AttackSound != nullptr)
	{
		float Current = AttackSound->PitchMultiplier;
		float Spinoff = Current + FMath::FRandRange(-0.3f, 0.0f);
		AttackSound->SetPitchMultiplier(Spinoff);
	}
}


void AGAttack::InitAttack(AActor* Shooter, float Magnitude, float YScale)
{
	// Set local variables
	OwningShooter = Shooter;
	AttackMagnitude = Magnitude;
	ShotDirection = YScale;

	// set sounds
	AttackSound->SetPitchMultiplier(AttackSound->PitchMultiplier - (AttackMagnitude / 1.5f));
	//AttackSound->SetVolumeMultiplier(FMath::Clamp(1.0f + (AttackMagnitude * 1.25f), 0.1f, 1.5f));
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("Pitch After: %f"), AttackSound->PitchMultiplier));

	// Lifespan
	/*if (MagnitudeTimeScalar != 0.0f)
	{
		DynamicLifetime = DurationTime * (1.0f + (AttackMagnitude * MagnitudeTimeScalar));
	}
	else 
	{ 
		DynamicLifetime = DurationTime;
	}*/
	SetLifeSpan(DurationTime);

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

	// Init success & recoil
	if (OwningShooter != nullptr)
	{
		ACharacter* Chara = Cast<ACharacter>(OwningShooter);
		if (Chara)
		{
			float RecoilScalar = KineticForce * -0.5f;
			FRotator RecoilRotator = LocalForward.Rotation() + FRotator(ShotDirection * ShootingAngle, 0.0f, 0.0f);
			Chara->GetCharacterMovement()->AddImpulse(RecoilRotator.Vector() * RecoilScalar);

		}

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

	// Neutralize before smooth visual exit
	if (LifeTimer >= LethalTime)
	{
		bLethal = false;
	}

	// End-of-life activities
	float CloseEnough = DurationTime * 0.97f;
	if (LifeTimer >= CloseEnough || this->IsPendingKillOrUnreachable())
	{
		AttackParticles->bSuppressSpawning = true;

		AGammaCharacter* PotentialGamma = Cast<AGammaCharacter>(OwningShooter);
		if (PotentialGamma)
		{
			bool Input = bSecondary;
			Nullify(Input);
		}
	}

	// Healthy attack activities
	if (bLethal && HasAuthority())
	{
		UpdateAttack(DeltaTime);
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
	if (bRaycastOnMesh && AttackSprite->GetSprite() != nullptr)
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
	
	if (!bHit && HitResult)
	{
		HitActor = Hit.Actor.Get();
		if (HitActor != nullptr)
		{
			HitEffects(HitActor, Hit.ImpactPoint);

			// Clean-up for next frame
			HitTimer = 0.0f;
		}
	}
}


void AGAttack::SpawnDamage(AActor* HitActor, FVector HitPoint)
{
	if (DamageClass!= nullptr)
	{
		// Spawning damage fx
		FActorSpawnParameters SpawnParams;
		FRotator Forward = HitActor->GetActorRotation();
		AGDamage* DmgObj = Cast<AGDamage>(GetWorld()->SpawnActor<AGDamage>(DamageClass, HitPoint, Forward, SpawnParams));
		DmgObj->AttachToActor(HitActor, FAttachmentTransformRules::KeepWorldTransform);
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("No Dmg class to spawn"));
	}

	ForceNetUpdate();
}


void AGAttack::ApplyKnockback(AActor* HitActor)
{
	// The knock itself
	FVector AwayFromShooter = (HitActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	float KnockScalar = FMath::Abs(KineticForce);

	// Get character movement to kick on
	ACharacter* Chara = Cast<ACharacter>(HitActor);
	if (Chara != nullptr)
	{
		Chara->GetCharacterMovement()->AddImpulse(AwayFromShooter * KnockScalar);
	}
	else
	{
		// Or get general static mesh
		UStaticMeshComponent* HitMeshComponent = nullptr;
		TArray<UStaticMeshComponent*> Components;
		HitActor->GetComponents<UStaticMeshComponent>(Components);
		for (int32 i = 0; i < Components.Num(); ++i)
		{
			HitMeshComponent = Components[i];
		}

		// Apply force to it
		if (HitMeshComponent != nullptr)
		{
			HitMeshComponent->AddImpulse(AwayFromShooter * KnockScalar);
		}
	}
}


void AGAttack::ReportHit(AActor* HitActor)
{
	/*if ((!HitActor->ActorHasTag("Player"))
		&& (!HitActor->ActorHasTag("Bot")))
	{
		HitActor->Tags.Add("Doomed");
	}*/

	// Damage
	AGammaCharacter* PotentialPlayer = Cast<AGammaCharacter>(HitActor);
	if (PotentialPlayer != nullptr)
	{
		PotentialPlayer->ModifyHealth(-20);
	}

	// Grow maze cubes
	if (HitActor->ActorHasTag("Grow"))
	{
		FVector HitActorScale = HitActor->GetActorScale3D();
		HitActor->SetActorScale3D(HitActorScale * 1.15f);
		/*TSubclassOf<UStaticMeshComponent> MeshCompTest;
		UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(HitActor->GetComponentByClass(MeshCompTest));
		if (MeshComp != nullptr)
		{
			float MeshMass = MeshComp->GetMass();
			MeshComp->SetMassScale(NAME_None, MeshMass * 0.9f);
		}*/
	}

	if (CurrentMatch != nullptr)
	{
		// if multi-hit
		bLethal = false;
		CurrentMatch->ClaimHit(HitActor, OwningShooter);
	}
}


void AGAttack::Nullify(int AttackType)
{
	AGammaCharacter* PossibleCharacter = Cast<AGammaCharacter>(OwningShooter);
	if (PossibleCharacter != nullptr)
	{
		// All
		if (AttackType == -1)
		{
			PossibleCharacter->NullifyAttack();
			PossibleCharacter->NullifySecondary();
		}

		// Attack
		if (AttackType == 0)
		{
			PossibleCharacter->NullifyAttack();
		}
		// Secondary
		else if (AttackType == 1)
		{
			PossibleCharacter->NullifySecondary();
		}
		
		this->Destroy();
	}
}


void AGAttack::HitEffects(AActor* HitActor, FVector HitPoint)
{
	bHit = true;


	// Hit another attack?
	AGAttack* Atk = Cast<AGAttack>(HitActor);
	if (Atk != nullptr)
	{
		if (Atk->OwningShooter != nullptr
			&& this->OwningShooter != nullptr
			&& Atk->OwningShooter != this->OwningShooter)
		{

			// Collide off shield
			if (HitActor->ActorHasTag("Shield"))
			{
				SpawnDamage(HitActor, HitPoint);
				ApplyKnockback(HitActor);

				return;
			}
		}
	}

	// Real hit Consequences
	else if (!HitActor->ActorHasTag("Attack")
		&& !HitActor->ActorHasTag("Doomed")) /// && (HitTimer >= (1.0f / HitsPerSecond)))
	{
		SpawnDamage(HitActor, HitPoint);
		ApplyKnockback(HitActor);
		ReportHit(HitActor);

		HitTimer = 0.0f;
	}
	else
	{
		// Hit another attack..
	}

	if (HitActor->ActorHasTag("Solid")
		&& this->ActorHasTag("Solid")
		&& LifeTimer > 0.1f)
	{
		bool Input = bSecondary;
		Nullify(Input);
	}
}


// COLLISION BEGIN
void AGAttack::OnAttackBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bHit && bLethal && (OtherActor != OwningShooter))
	{
		FVector DamageLocation = GetActorLocation() + (OwningShooter->GetActorForwardVector() * RaycastHitRange);

		// Got'em
		HitEffects(OtherActor, DamageLocation);
	}
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
