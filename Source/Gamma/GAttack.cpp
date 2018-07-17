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
	AttackParticles->SetIsReplicated(true);
	ProjectileComponent->SetIsReplicated(true);
}



void AGAttack::BeginPlay()
{
	Super::BeginPlay();
	bHit = false;
	//bLethal = false;
	SetLifeSpan(DurationTime);
	AttackDamage = 3.0f;

	// Add natural deviancy to sound
	if (AttackSound != nullptr)
	{
		float Current = AttackSound->PitchMultiplier;
		float Spinoff = Current + FMath::FRandRange(-0.3f, 0.0f);
		AttackSound->SetPitchMultiplier(Spinoff);
	}

	if (ActorHasTag("Obstacle"))
	{
		InitAttack(this, 1.0f, 0.0f);
	}
}


void AGAttack::InitAttack(AActor* Shooter, float Magnitude, float YScale)
{
	if (HasAuthority())
	{
		// Set local variables
		OwningShooter = Shooter;
		AttackMagnitude = Magnitude;
		ShotDirection = YScale;

		/*if (!bSecondary)
		{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::White, FString::Printf(TEXT("AttackMagnitude: %f"), AttackMagnitude));
		}*/

		// set sounds
		AttackSound->SetPitchMultiplier(AttackSound->PitchMultiplier + AttackMagnitude);
		//AttackSound->SetVolumeMultiplier(FMath::Clamp(1.0f + (AttackMagnitude * 1.25f), 0.1f, 1.5f));
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("Pitch After: %f"), AttackSound->PitchMultiplier));

		// Lifespan
		if (MagnitudeTimeScalar != 1.0f)
		{
			DurationTime = (DurationTime + AttackMagnitude) * MagnitudeTimeScalar;
			LethalTime = DurationTime * 0.5f;
		}
		else
		{
			DynamicLifetime = DurationTime;
		}
		SetLifeSpan(DurationTime);

		// Scale HitsPerSecond by Magnitude
		HitsPerSecond = FMath::Clamp(HitsPerSecond * AttackMagnitude, 1.0f, 100.0f);

		// Adjust lethal time by magnitude
		if (DurationTime < 1.0f)
		{
			float NewLethalTime = LethalTime * AttackMagnitude;
			LethalTime = NewLethalTime;
		}

		// Last-second update to direction after fire
		if (AngleSweep != 0.0f)
		{
			float DirRecalc = ShotDirection * ShootingAngle;
			DirRecalc *= (-2.0f * AttackMagnitude);
			FVector LocalForward = GetActorForwardVector();
			FRotator FireRotation = LocalForward.Rotation() + FRotator(DirRecalc, 0.0f, 0.0f);

			// Correct rotation -> Character may be spinning
			float YawAbs = FMath::Abs(FireRotation.Yaw);
			///GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::White, FString::Printf(TEXT("YawAbs:  %f"), YawAbs));
			if ((YawAbs < 90.0f) || (YawAbs > 270.0f))
			{
				FireRotation.Yaw = 0.0f;
			}
			else
			{
				FireRotation.Yaw = 180.0f;
			}
			SetActorRotation(FireRotation);
			///GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::White, FString::Printf(TEXT("FireRotation.Yaw:  %f"), FireRotation.Yaw));
		}


		// Projectile movement
		if (ProjectileSpeed > 0.0f)
		{
			ProjectileComponent->Velocity = GetActorForwardVector() * ProjectileSpeed;

			if (ProjectileComponent)
			{
				/*if (bScaleProjectileSpeed)
				{
				ProjectileComponent->Velocity = GetActorForwardVector() * ProjectileSpeed * AttackMagnitude * ProjectileMaxSpeed;
				}*/
			}
			else
			{
				GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("no projectile comp"));
			}
		}

		// Get match obj
		TArray<AActor*> Actors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGMatch::StaticClass(), Actors);
		if (Actors.Num() >= 1)
		{
			CurrentMatch = Cast<AGMatch>(Actors[0]);
		}


		// Color & cosmetics
		/*FlashMesh->SetMaterial(0, MainMaterial);
		AttackMesh->SetMaterial(0, MainMaterial);*/


		// Recoil &
		// Init Success
		if ((OwningShooter != nullptr) && (CurrentMatch != nullptr))
		{
			ACharacter* Chara = Cast<ACharacter>(OwningShooter);
			if (Chara != nullptr)
			{
				float RecoilScalar = FMath::Abs(KineticForce) * FMath::Clamp((10.0f * AttackMagnitude), 0.5f, 2.1f);
				FVector LocalForward = GetActorForwardVector().ProjectOnToNormal(FVector::ForwardVector);
				FRotator RecoilRotator = LocalForward.Rotation() + FRotator(ShotDirection * ShootingAngle, 0.0f, 0.0f);
				Chara->GetCharacterMovement()->AddImpulse(RecoilRotator.Vector() * RecoilScalar);

				// Take the first shot
				HitTimer = (1.0f / HitsPerSecond);
				bLethal = true;
				DetectHit(GetActorForwardVector());

				///GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::White, FString::Printf(TEXT("LethalTime:  %f"), LethalTime));
			}
		}

		//GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::White, FString::Printf(TEXT("Magnitude:  %f"), AttackMagnitude));
	}
}


// Called every frame
void AGAttack::Tick(float DeltaTime)
{
	if (bHit)
	{
		AttackParticles->CustomTimeDilation *= 0.9f; // Interp this with deltatime
	}

	if (HasAuthority())
	{
		Super::Tick(DeltaTime);

		LifeTimer += DeltaTime;
		HitTimer += (DeltaTime * CustomTimeDilation);
		
		// Healthy attack activities
		if (bLethal)
		{
			UpdateAttack(DeltaTime);
		}

		// Life-tracking activities
		float GlobalTimeScale = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
		if (GlobalTimeScale > 0.3f)
		{
			float CloseEnough = DurationTime * 0.97f;
			if (LifeTimer >= CloseEnough)
			{
				AttackParticles->bSuppressSpawning = true;

				AGammaCharacter* PotentialGamma = Cast<AGammaCharacter>(OwningShooter);
				if (PotentialGamma != nullptr)
				{
					bool Input = bSecondary;
					Nullify(Input);
				}
			}

			// Neutralize before smooth visual exit
			if (LifeTimer >= LethalTime)
			{
				bLethal = false;
			}
		}
	}

	// Timescaling
	//if (OwningShooter != nullptr)
	//{
	//	if (CustomTimeDilation != OwningShooter->CustomTimeDilation)
	//	{
	//		CustomTimeDilation = OwningShooter->CustomTimeDilation;
	//		AttackParticles->CustomTimeDilation = OwningShooter->CustomTimeDilation * 0.5f;
	//		float NewLife = GetLifeSpan() * (1.0f / CustomTimeDilation);
	//		SetLifeSpan(NewLife);
	//		HitsPerSecond *= CustomTimeDilation;

	//		ForceNetUpdate();

	//		//GEngine->AddOnScreenDebugMessage(-1, 10.5f, FColor::White, FString::Printf(TEXT("Attack update timescale to  : %f"), CustomTimeDilation));
	//	}
	//}

	
}


void AGAttack::UpdateAttack(float DeltaTime)
{
	if (OwningShooter != nullptr)
	{
		bool bTime = (HitTimer >= (1.0f / HitsPerSecond));
		float TimeSc = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
		if (bTime && (TimeSc > 0.5f))
		{
			DetectHit(GetActorForwardVector());
		}
	}
}
	


void AGAttack::DetectHit(FVector RaycastVector)
{
	if (HasAuthority())
	{
		// Linecast ingredients
		TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjects;
		TraceObjects.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
		TraceObjects.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
		TraceObjects.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
		TraceObjects.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));

		TArray<AActor*> IgnoredActors;
		IgnoredActors.Add(OwningShooter);

		FVector Start = GetActorLocation() + (GetActorForwardVector() * -10.0f);
		Start.Y = 0.0f;
		FVector End = Start + (RaycastVector * RaycastHitRange);
		End.Y = 0.0f; /// strange y-axis drift

		FHitResult Hit;

		// Swords, etc, get tangible ray space
		if (bRaycastOnMesh && (AttackSprite->GetSprite() != nullptr))
		{
			float SpriteLength = (AttackSprite->Bounds.BoxExtent.X) * (1.0f + AttackMagnitude);
			float AttackBodyLength = SpriteLength * RaycastHitRange;
			Start = AttackSprite->GetComponentLocation() + (GetActorForwardVector() * (-SpriteLength / 2.1f));
			Start.Y = 0.0f;
			End = Start + (RaycastVector * AttackBodyLength);
			End.Y = 0.0f;
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
			FLinearColor::White, FLinearColor::Red, 5.0f);

		if (HitResult)
		{
			//FVector ClosestHit = Hit.Component.Get()->GetClosestPointOnCollision(Hit.ImpactPoint, Hit.ImpactPoint);
			HitActor = Hit.Actor.Get();
			if (HitActor != nullptr)
			{
				HitEffects(HitActor, Hit.ImpactPoint);
			}
		}
	}
}


void AGAttack::SpawnDamage(AActor* HitActor, FVector HitPoint)
{
	if (HasAuthority() && (DamageClass != nullptr) && (HitActor != nullptr))
	{
		/// Spawning damage fx
		FActorSpawnParameters SpawnParams;
		FVector ToHit = (HitActor->GetActorLocation() - HitPoint).GetSafeNormal();
		FRotator ToHitRotation = ToHit.Rotation();

		//if (ActorHasTag("Obstacle"))
		//{
		//	ToHit = (HitActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		//	//ToHitRotation = ToHit.Rotation();
		//	ToHitRotation = ToHit.ToOrientationRotator();
		//}

		/// Get closest point on bounds of HitActor
		FVector SpawnLocation = FVector::ZeroVector;
		if (HitActor != this)
		{
			UPrimitiveComponent* HitPrimitive = Cast<UPrimitiveComponent>(HitActor->GetRootComponent());
			if (HitPrimitive != nullptr)
			{
				/// Root capsules are easy
				float ClosestPointDist = HitPrimitive->GetClosestPointOnCollision(HitPoint, SpawnLocation);
			}
			else
			{
				/// else do it the old fashioned way
				SpawnLocation = HitPoint;
			}
		}
		else
		{
			SpawnLocation = HitPoint;
		}
		

		/// Spawn!
		AGDamage* DmgObj = Cast<AGDamage>(GetWorld()->SpawnActor<AGDamage>(DamageClass, SpawnLocation, ToHitRotation, SpawnParams)); /// HitPoint
		DmgObj->AttachToActor(HitActor, FAttachmentTransformRules::KeepWorldTransform);
																																	 
		/*if (!ActorHasTag("Obstacle"))
		{
			DmgObj->AttachToActor(HitActor, FAttachmentTransformRules::KeepWorldTransform);
		}*/

		ForceNetUpdate();
	}
}


void AGAttack::ApplyKnockback(AActor* HitActor, FVector HitPoint)
{
	/// The knock itself
	FVector AwayFromShooter = (HitPoint - GetActorLocation()).GetSafeNormal();
	if (ActorHasTag("Obstacle"))
	{
		AwayFromShooter = (HitActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	}

	//FVector AwayFromShooter = (HitActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	//float TimeDilat = //UGameplayStatics::GetGlobalTimeDilation(GetWorld());
	//TimeDilat = FMath::Clamp(TimeDilat, 0.01f, 0.15f);
	float HitsScalar = 1.0f + (2.0f / numHits);
	float MagnitudeScalar = FMath::Square(1.0f + AttackMagnitude);
	float KnockScalar = FMath::Abs(KineticForce) * HitsScalar * MagnitudeScalar;

	/// Trim the vector to favor widescreen
	AwayFromShooter.Z *= 0.55f;
	AwayFromShooter.X *= 1.25f;

	/// Get character movement to kick on
	ACharacter* Chara = Cast<ACharacter>(HitActor);
	if (Chara != nullptr)
	{
		Chara->GetCharacterMovement()->AddImpulse(AwayFromShooter * KnockScalar);
	}
	else
	{
		/// Or get general static mesh
		UStaticMeshComponent* HitMeshComponent = nullptr;
		TArray<UStaticMeshComponent*> Components;
		HitActor->GetComponents<UStaticMeshComponent>(Components);
		int LoopSize = Components.Num();
		for (int i = 0; i < LoopSize; ++i)
		{
			HitMeshComponent = Components[i];
			if (HitMeshComponent != nullptr)
			{
				break;
			}
		}

		/// Apply force to it
		if ((HitMeshComponent != nullptr)
			&& HitMeshComponent->IsSimulatingPhysics())
		{
			HitMeshComponent->AddImpulseAtLocation(AwayFromShooter * KnockScalar * 1000.0f, HitPoint);
		}
	}
}


void AGAttack::ReportHit(AActor* HitActor)
{
	if (HasAuthority())
	{
		/// Track hitscale curvature for increasing knockback and damage
		numHits = FMath::Clamp((numHits += 1), 1, 10); /// (numHits += (numHits - 1)), 2, 9

		/// Damage
		AGammaCharacter* PotentialPlayer = Cast<AGammaCharacter>(HitActor);
		if (PotentialPlayer != nullptr)
		{
			PotentialPlayer->ModifyHealth((-AttackDamage) * numHits);

			/// Marked killed AI for the reset sweep
			if (PotentialPlayer->GetHealth() <= 0.0f
				&& PotentialPlayer->ActorHasTag("Bot"))
			{
				PotentialPlayer->Tags.Add("Doomed");
			}
		}

		/// Call for slowtime
		if (CurrentMatch != nullptr)
		{
			CurrentMatch->ClaimHit(HitActor, OwningShooter);
		}

		/// Grow maze cubes
		if (HitActor->ActorHasTag("Grow"))
		{
			FVector HitActorScale = HitActor->GetActorScale3D();
			HitActor->SetActorScale3D(HitActorScale * 1.0521f);

			/// Affect object's mass
			/*TSubclassOf<UStaticMeshComponent> MeshCompTest;
			UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(HitActor->GetComponentByClass(MeshCompTest));
			if (MeshComp != nullptr)
			{
			float MeshMass = MeshComp->GetMass();
			MeshComp->SetMassScale(NAME_None, MeshMass * 0.9f);
			}*/
		}

		ForceNetUpdate();
	}
}


void AGAttack::Nullify(int AttackType)
{
	if ((OwningShooter != nullptr)
		&& (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.5f))
	{
		AGammaCharacter* PossibleCharacter = Cast<AGammaCharacter>(OwningShooter);
		if (PossibleCharacter != nullptr)
		{
			/// All
			if (AttackType == -1)
			{
				PossibleCharacter->NullifyAttack();
				PossibleCharacter->NullifySecondary();
			}

			/// Attack
			if (AttackType == 0)
			{
				PossibleCharacter->NullifyAttack();
			}
			/// Secondary
			else if (AttackType == 1)
			{
				PossibleCharacter->NullifySecondary();
			}

			Destroy();
		}
	}
}


void AGAttack::HitEffects(AActor* HitActor, FVector HitPoint)
{
	HitTimer = 0.0f;
	bool bSpawnBlock = false;

	// Hit another attack?
	AGAttack* OtherAttack = Cast<AGAttack>(HitActor);
	if (OtherAttack != nullptr)
	{
		// Qualify that it's not one of ours
		if ((OwningShooter != nullptr)
			&& (OtherAttack->OwningShooter != nullptr))
		{
			
			// Return if we hit another of us
			if ((OwningShooter == OtherAttack->OwningShooter)
				&& !(ActorHasTag("Obstacle")))
			{
				///GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Green, FString::Printf(TEXT("Hit A Mirror:  %f"), 1.0f));
				return;
			}

			// Collide off shield
			if (OtherAttack->ActorHasTag("Shield")
				&& !OtherAttack->ActorHasTag("Obstacle"))
			{
				// Spawn blocked fx
				if (BlockedClass != nullptr)
				{
					FActorSpawnParameters SpawnParams;
					AActor* BlockedFX = GetWorld()->SpawnActor<AActor>(
						BlockedClass, GetActorLocation(), GetActorRotation(), SpawnParams);
				}

				//ApplyKnockback(OtherAttack, HitPoint);
				ApplyKnockback(OwningShooter, GetActorLocation());

				if (!ActorHasTag("Obstacle"))
				{
					bLethal = false;
				}
			}
			
			// Locked weapons' wielders are pushed away
			if ((OtherAttack->LockedEmitPoint == true)
				&& !OtherAttack->ActorHasTag("Obstacle"))
			{
				AGammaCharacter* PotentialPlayer = Cast<AGammaCharacter>(OtherAttack->OwningShooter);
				if (PotentialPlayer != nullptr)
				{
					ApplyKnockback(PotentialPlayer, HitPoint);
					ApplyKnockback(PotentialPlayer, HitPoint); // Pending Refactor - add scalar argument to ApKnk!
				}
			}
		}
	}

	// Spawn the basic damage smoke
	// 'Freefire' attacks always attach to the hitactor
	if (!LockedEmitPoint)
	{
		SpawnDamage(HitActor, HitPoint);
	}
	else // 'Locked' attacks ie. sword
	{
		float Rando = FMath::FRand();
		if ((Rando >= 0.5f) || ActorHasTag("Obstacle"))
		{
			SpawnDamage(HitActor, HitPoint);
		}
		else
		{
			SpawnDamage(this, HitPoint);
		}
	}

	// Real hit Consequences
	if (bLethal && !HitActor->ActorHasTag("Attack")
		&& !HitActor->ActorHasTag("Doomed")) /// && (HitTimer >= (1.0f / HitsPerSecond)))
	{
		ApplyKnockback(HitActor, HitPoint);

		if (!ActorHasTag("Obstacle") && (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.3f))
		{
			ReportHit(HitActor);
		}
	}

	// Stick-in for 'solid' attacks
	if (HitActor->ActorHasTag("Wall")
		&& this->ActorHasTag("Solid"))
	{
		// Spawn blocked fx
		if (BlockedClass != nullptr)
		{
			FActorSpawnParameters SpawnParams;
			AActor* BlockedFX = GetWorld()->SpawnActor<AActor>(
				BlockedClass, GetActorLocation(), GetActorRotation(), SpawnParams);
			if (BlockedFX != nullptr)
			{
				BlockedFX->SetLifeSpan(0.221f);
			}
		}

		bLethal = false;
		if (ProjectileComponent != nullptr)
		{
			ProjectileComponent->Velocity *= 0.01f;
			ProjectileSpeed = 0.0f;
			ProjectileMaxSpeed = 0.0f;
		}

	}

	bHit = true;
}


// COLLISION BEGIN
void AGAttack::OnAttackBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (HasAuthority())
	{
		bool bTime = (HitTimer >= (1 / HitsPerSecond));
		bool bActors = (OwningShooter != nullptr) && (OtherActor != nullptr);
		float TimeSc = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
		if (bTime && bActors && bLethal && (OtherActor != OwningShooter) && (TimeSc > 0.5f))
		{
			FVector DamageLocation = GetActorLocation() + (OwningShooter->GetActorForwardVector() * RaycastHitRange);
			if (ActorHasTag("Obstacle"))
			{
				DamageLocation = GetActorLocation() + SweepResult.ImpactPoint;
			}

			// Got'em
			HitEffects(OtherActor, DamageLocation);
		}
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
