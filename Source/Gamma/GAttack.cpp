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
	bLethal = false;
	SetLifeSpan(DurationTime);
	AttackDamage = 10.0f;
	numHits = 0;

	// Add natural deviancy to sound
	if (AttackSound != nullptr)
	{
		float Current = AttackSound->PitchMultiplier;
		float Spinoff = Current + FMath::FRandRange(0.2f, 0.3f);
		AttackSound->SetPitchMultiplier(Spinoff);
	}

	// Visual trim
	if (AttackParticles != nullptr)
	{
		AttackParticles->DeactivateSystem();
	}
	if (AttackSprite != nullptr)
	{
		AttackSprite->SetVisibility(false);
		AttackSprite->bHiddenInGame = true;
		AttackSprite->Deactivate();
		//GEngine->AddOnScreenDebugMessage(-1, 20.0f, FColor::White, FString::Printf(TEXT("Deactivated Sprite! %f"), 0.0f));
	}

	if (ActorHasTag("Obstacle") || ActorHasTag("Swarm"))
	{
		InitAttack(this, 1.0f, 0.0f);
		AttackParticles->ActivateSystem();
		numHits = 1;
	}
}


void AGAttack::InitAttack(AActor* Shooter, float Magnitude, float YScale)
{
	if (HasAuthority())
	{
		// Set local variables
		OwningShooter = Shooter;
		AttackMagnitude = Magnitude;
		ShotDirection = FMath::Clamp(YScale, -1.0f, 1.0f);

		/*if (!bSecondary)
		{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::White, FString::Printf(TEXT("AttackMagnitude: %f"), AttackMagnitude));
		}*/

		// set sounds
		AttackSound->SetPitchMultiplier(AttackSound->PitchMultiplier * AttackMagnitude);
		//AttackSound->SetVolumeMultiplier(FMath::Clamp(1.0f + (AttackMagnitude * 1.25f), 0.1f, 1.5f));
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("Pitch After: %f"), AttackSound->PitchMultiplier));

		// Lifespan
		if (MagnitudeTimeScalar != 1.0f)
		{
			DurationTime = DurationTime + (AttackMagnitude * MagnitudeTimeScalar);
		}
		LethalTime = DurationTime * 0.97f;
		DynamicLifetime = DurationTime;
		SetLifeSpan(DurationTime);

		// Scale HitsPerSecond by Magnitude
		HitsPerSecond = FMath::Clamp(HitsPerSecond * AttackMagnitude, 1.0f, 1000.0f);


		// Update to direction after fire
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
			FireRotation.Pitch = FMath::Clamp(FireRotation.Pitch, -ShootingAngle, ShootingAngle);
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
		TArray<AActor*> PotentialMatches;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGMatch::StaticClass(), PotentialMatches);
		if (PotentialMatches.Num() >= 1)
		{
			CurrentMatch = Cast<AGMatch>(PotentialMatches[0]);
		}

		// Burst visual
		if ((OwningShooter != nullptr) && (BurstClass != nullptr))
		{
			FActorSpawnParameters SpawnParams;
			AActor* NewBurst = GetWorld()->SpawnActor<AActor>(BurstClass, GetActorLocation(), GetActorRotation(), SpawnParams);
			if (NewBurst != nullptr)
			{
				NewBurst->AttachToActor(OwningShooter, FAttachmentTransformRules::KeepWorldTransform);
				float MagnitudeScalar = FMath::Clamp(AttackMagnitude * 2.1f, 0.5f, 1.1f);
				FVector NewBurstScale = NewBurst->GetActorRelativeScale3D() * MagnitudeScalar;
				NewBurst->SetActorRelativeScale3D(NewBurstScale);
			}

			// Inherit some velocity from owning shooter
			if (ProjectileSpeed != 0.0f)
			{
				FVector ShooterVelocity = OwningShooter->GetVelocity();
				FVector ScalarVelocity = FVector::ZeroVector;
				float VelFloat = ShooterVelocity.Size();
				if (VelFloat != 0.0f)
				{
					ScalarVelocity = ShooterVelocity.GetSafeNormal() * FMath::Sqrt(VelFloat);
					ProjectileComponent->Velocity += ScalarVelocity;
				}
			}
		}


		// Color & cosmetics
		/*FlashMesh->SetMaterial(0, MainMaterial);
		AttackMesh->SetMaterial(0, MainMaterial);*/

		//GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::White, FString::Printf(TEXT("Magnitude:  %f"), AttackMagnitude));
	}
}


// Called every frame
void AGAttack::Tick(float DeltaTime)
{
	if (!bLethal && (GetGameTimeSinceCreation() > RefireTime)
		&& (GetGameTimeSinceCreation() < 0.215f)) /// for attacks being blocked later
	{
		// Init One-Timer
		if (!bHit && (OwningShooter != nullptr) && (CurrentMatch != nullptr))
		{
			
			// Last-second redirection
			float ShooterYaw = FMath::Abs(OwningShooter->GetActorRotation().Yaw);

			float Yaw = 0.0f;
			if ((ShooterYaw > 50.0f))
			{
				Yaw = 180.0f;
			}

			float Pitch = FMath::Clamp(GetActorRotation().Pitch, -ShootingAngle, ShootingAngle);
			float Roll = GetActorRotation().Roll;
			FRotator NewRotation = FRotator(Pitch, Yaw, Roll);
			SetActorRotation(NewRotation);

			// Recoil
			ACharacter* Chara = Cast<ACharacter>(OwningShooter);
			if (Chara != nullptr)
			{
				float RecoilScalar = KineticForce * (-500.0f * FMath::Clamp(AttackMagnitude, 0.1f, 0.5f)); // (FMath::Abs(KineticForce)
				FVector LocalForward = GetActorForwardVector(); /// .ProjectOnToNormal(FVector::ForwardVector);
				FRotator RecoilRotator = LocalForward.Rotation() + FRotator(ShotDirection * ShootingAngle, 0.0f, 0.0f);
				Chara->GetCharacterMovement()->AddImpulse(LocalForward * RecoilScalar); /// RecoilRotator.Vector()

				// Take the first shot
				HitTimer = (1.0f / HitsPerSecond);
				bLethal = true;
				DetectHit(GetActorForwardVector());

				// Visual trim
				if (AttackParticles != nullptr)
				{
					AttackParticles->ActivateSystem();
				}
				if (AttackSprite != nullptr)
				{
					AttackSprite->bHiddenInGame = false;
					AttackSprite->Activate();
					AttackSprite->SetVisibility(true);
					//GEngine->AddOnScreenDebugMessage(-1, 20.5f, FColor::White, FString::Printf(TEXT("Activated Sprite!  %f"), 1.0f));
				}
			}
			else if (ActorHasTag("Obstacle") || ActorHasTag("Swarm"))
			{
				// Take the first shot
				HitTimer = (1.0f / HitsPerSecond);
				bLethal = true;
				
				
				if (HitsPerSecond > 0.0f)
				{
					DetectHit(GetActorForwardVector());
				}

				// Visual trim
				if (AttackParticles != nullptr)
				{
					AttackParticles->ActivateSystem();
				}
				if (AttackSprite != nullptr)
				{
					AttackSprite->bHiddenInGame = false;
					AttackSprite->Activate();
					AttackSprite->SetVisibility(true);
					//GEngine->AddOnScreenDebugMessage(-1, 20.5f, FColor::White, FString::Printf(TEXT("Activated Sprite!  %f"), 1.0f));
				}
			}
		}
	}

	if (bHit && (!ActorHasTag("Swarm")) && (!ActorHasTag("Obstacle")))
	{
		float AttackTimescale = FMath::FInterpConstantTo(AttackParticles->CustomTimeDilation, 0.0f, DeltaTime, 1.0f);
		AttackParticles->CustomTimeDilation = AttackTimescale;
	}


	// Main update
	if (HasAuthority())
	{
		Super::Tick(DeltaTime);

		// Check for end of game
		float Timespeed = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
		if (Timespeed < 0.15f)
		{
			return;
		}

		LifeTimer += DeltaTime;
		HitTimer += (DeltaTime * (1.0f / Timespeed));
		
		// Healthy attack activities
		if (bLethal)
		{
			UpdateAttack(DeltaTime);
		}
		else if (GetGameTimeSinceCreation() > 0.22f)
		{
			if (AttackSprite != nullptr)
			{
				AttackSprite->SetVisibility(false);
			}
			else
			{
				AttackParticles->CustomTimeDilation = 0.0f;
			}
		}

		// Life-tracking activities
		float GlobalTimeScale = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
		if (GlobalTimeScale > 0.3f)
		{
			float CloseEnough = GetLifeSpan() * 0.97f;
			if ((LifeTimer >= CloseEnough)
				&& (GetGameTimeSinceCreation() >= DurationTime))
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
		TraceObjects.Add(UEngineTypes::ConvertToObjectType(ECC_Destructible));

		TArray<AActor*> IgnoredActors;
		IgnoredActors.Add(OwningShooter);

		FVector Start = GetActorLocation() + (GetActorForwardVector() * -100.0f);
		Start.Y = 0.0f;
		FVector End = Start + (RaycastVector * RaycastHitRange);
		End.Y = 0.0f; /// strange y-axis drift

		

		// Swords, etc, get tangible ray space
		if (bRaycastOnMesh)
		{
			if (AttackSprite->GetSprite() != nullptr)
			{
				float SpriteLength = (AttackSprite->Bounds.BoxExtent.X) * (1.0f + AttackMagnitude);
				float AttackBodyLength = SpriteLength * RaycastHitRange;
				Start = AttackSprite->GetComponentLocation() + (GetActorForwardVector() * (-SpriteLength / 2.1f));
				Start.Y = 0.0f;
				End = Start + (RaycastVector * AttackBodyLength);
				End.Y = 0.0f;
			}
			else if (AttackParticles != nullptr)
			{
				float AttackBodyLength = RaycastHitRange;
				Start = AttackParticles->GetComponentLocation() + (GetActorForwardVector() * (-AttackBodyLength / 2.1f));
				Start.Y = 0.0f;
				End = Start + (RaycastVector * AttackBodyLength);
				End.Y = 0.0f;
			}
		}

		TArray<FHitResult> Hits;

		// Pew pew
		bool HitResult = UKismetSystemLibrary::LineTraceMultiForObjects(
			this,
			Start,
			End,
			TraceObjects,
			false,
			IgnoredActors,
			EDrawDebugTrace::None,
			Hits,
			true,
			FLinearColor::Gray, FLinearColor::Red, 5.0f);

		if (HitResult)
		{
			int NumHits = Hits.Num();
			for (int i = 0; i < NumHits; ++i)
			{
				HitActor = Hits[i].Actor.Get();
				if ((HitActor != nullptr)
					&& (HitActor != OwningShooter)
					&& (HitActor->WasRecentlyRendered(0.2f)))
				{
					HitEffects(HitActor, Hits[i].ImpactPoint);
				}
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
	FVector AwayFromAttack = (HitPoint - OwningShooter->GetActorLocation()).GetSafeNormal(); // previously this actor's location
	FVector AttackForward = GetActorForwardVector().GetSafeNormal();
	

	//FVector AwayFromAttack = (HitActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	//float TimeDilat = //UGameplayStatics::GetGlobalTimeDilation(GetWorld());
	//TimeDilat = FMath::Clamp(TimeDilat, 0.01f, 0.15f);
	float SafeHits = FMath::Clamp(numHits, 1, 5);
	float HitsScalar = (1.0f / SafeHits); /// 1.0f + (2.0f / SafeHits);
	float MagnitudeScalar = FMath::Square(1.0f + AttackMagnitude);
	float KnockScalar = FMath::Abs(KineticForce) * HitsScalar * MagnitudeScalar;

	// Special care for obstacles
	if (ActorHasTag("Obstacle"))
	{
		AttackForward = FVector::UpVector;
		AwayFromAttack = (HitActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		KnockScalar = KineticForce;
	}

	// Got the knockback
	FVector KnockVector = (AwayFromAttack + (AttackForward * 0.5f)).GetSafeNormal();
	float DirRecalc = ShotDirection * ShootingAngle;
	FRotator FireRotation = KnockVector.Rotation() + FRotator(DirRecalc, 0.0f, 0.0f);
	KnockVector = FireRotation.Vector();
	KnockVector.Z *= 0.8f;

	/// Vector done -> Effect the force
	/// Get character movement to kick on
	ACharacter* Chara = Cast<ACharacter>(HitActor);
	if (Chara != nullptr)
	{
		bool bVelocityOverride = bHit;
		Chara->GetCharacterMovement()->AddImpulse(KnockVector * KnockScalar * 3.3f, bVelocityOverride);
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
			float MassScalar = ((HitMeshComponent->GetMass() * HitMeshComponent->GetMassScale())) * 0.05f;
			HitMeshComponent->AddImpulseAtLocation(KnockVector * (KnockScalar * 0.5f) * MassScalar, HitPoint);
		}
	}
}


void AGAttack::ReportHit(AActor* HitActor)
{
	if (HasAuthority())
	{
		/// Track hitscale curvature for increasing knockback and damage
		if (!ActorHasTag("Obstacle"))
		{
			numHits += 1;
		}

		/// Damage
		AGammaCharacter* PotentialPlayer = Cast<AGammaCharacter>(HitActor);
		if (PotentialPlayer != nullptr)
		{
			float Dmg = -1 * FMath::Clamp((AttackDamage * numHits), 1.0f, 99.0f);
			PotentialPlayer->ModifyHealth(Dmg);
			///GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::White, FString::Printf(TEXT("Hit for %f!"), Dmg));

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
			FVector TargetScale = HitActorScale * 1.0521f;
			FVector GrowScale = FMath::VInterpTo(HitActorScale, TargetScale, GetWorld()->DeltaTimeSeconds, 2.1f);
			HitActor->SetActorScale3D(GrowScale);
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
	// Check for forwardness
	FVector ShooterForward = OwningShooter->GetActorForwardVector().GetSafeNormal();
	FVector ToHitPoint = (HitPoint - OwningShooter->GetActorLocation()).GetSafeNormal();
	FVector AttackForward = GetActorForwardVector().GetSafeNormal();
	float DotToHit = FVector::DotProduct(ShooterForward, ToHitPoint);
	float HitDist = FVector::Dist(ShooterForward, HitPoint);
	if ((!ActorHasTag("Solid")) && (DotToHit < 0.0f) && (HitDist <= 99.9f))
	{
		return;
	}

	bool bSuccessfulHit = false;


	// Hit-slow on characters
	AGammaCharacter* HitCharacter = Cast<AGammaCharacter>(HitActor);
	if ((HitCharacter != nullptr))
	{
		HitCharacter->GetMovementComponent()->Velocity *= FireDelay;
	}
	// Self-slow for solid attacks
	if ((ProjectileSpeed != 0.0f)
		&& (ActorHasTag("Solid")))
	{
		float HitSpeedScalarSafe = FMath::Clamp(FireDelay, 0.21f, 1.0f);
		ProjectileComponent->Velocity *= HitSpeedScalarSafe;
	}


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
				return;
			}

			if (ActorHasTag("Swarm") && HitActor->ActorHasTag("Swarm"))
			{
				HitActor->Destroy();
				return;
			}

			// Get countered on shield
			if (OtherAttack->ActorHasTag("Shield")
				|| OtherAttack->ActorHasTag("Obstacle"))
			{
				if (!ActorHasTag("Obstacle"))
				{
					bLethal = false;
					HitsPerSecond = 0.0f;
					bHit = true;
				}

				// Spawn blocked fx
				if (BlockedClass != nullptr)
				{
					FActorSpawnParameters SpawnParams;
					AActor* BlockedFX = GetWorld()->SpawnActor<AActor>(
						BlockedClass, GetActorLocation(), GetActorRotation(), SpawnParams);
					BlockedFX->SetLifeSpan(0.15f);
				}
			}
			
			// Locked weapons' wielders are pushed away
			if ((OtherAttack->LockedEmitPoint == true)
				&& !OtherAttack->ActorHasTag("Obstacle"))
			{
				AGammaCharacter* PotentialPlayer = Cast<AGammaCharacter>(OtherAttack->OwningShooter);
				if (PotentialPlayer != nullptr)
				{
					ApplyKnockback(PotentialPlayer, HitPoint); // Pending Refactor - add scalar argument to ApKnk!
				}
			}
		}
	}


	// All's good if we got here
	bSuccessfulHit = true;

	if (bSuccessfulHit)
	{
		// Real hit Consequences
		if (bLethal && !HitActor->ActorHasTag("Attack")
			&& !HitActor->ActorHasTag("Doomed")) /// && (HitTimer >= (1.0f / HitsPerSecond)))
		{
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

			ApplyKnockback(HitActor, HitPoint);

			if (!ActorHasTag("Obstacle") && (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.3f))
			{
				ReportHit(HitActor);
			}
			else
			{
				return;
			}
		}

		// Update hit status
		if (!ActorHasTag("Obstacle"))
		{
			bHit = true;
		}
		HitTimer = 0.0f;
		if (!bSecondary)
		{
			if (AngleSweep != 0.0f)
			{
				HitsPerSecond *= 0.7f;
			}
			else
			{
				HitsPerSecond *= 0.5f;
			}
		}
		

		// Extend lifetime
		if (GetLifeSpan() < DurationTime)
		{
			float CurrentLifespan = GetLifeSpan();
			float LifeAddition = (1.0f / AttackMagnitude) * 0.2f;
			LifeAddition = FMath::Clamp(LifeAddition, 0.05f, 0.15f);
			float NewLifespan = CurrentLifespan + LifeAddition;
			SetLifeSpan(NewLifespan);
			LethalTime += (NewLifespan * 0.99f);
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

				ProjectileComponent->Deactivate();
			}

			//AttachToActor(HitActor, FAttachmentTransformRules::KeepRelativeTransform);
			AttachToComponent(HitActor->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		}
	}
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
	DOREPLIFETIME(AGAttack, LethalTime);
}
