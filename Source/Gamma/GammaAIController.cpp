// Fill out your copyright notice in the Description page of Project Settings.

#include "GammaAIController.h"


void AGammaAIController::BeginPlay()
{
	Super::BeginPlay();

	// Get our Gamma Character
	MyPawn = GetPawn();
	if (MyPawn != nullptr)
	{
		MyCharacter = Cast<AGammaCharacter>(MyPawn);
	}
	if (MyCharacter != nullptr)
	{
		MyCharacter->Tags.Add("Bot");
	}

	RandStream = new FRandomStream;
	if (RandStream != nullptr)
	{
		RandStream->GenerateNewSeed();
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::Blue, TEXT("NO RAND STREAM"));
	}

	Player = nullptr;
}


void AGammaAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!MyCharacter)
	{
		MyPawn = GetPawn();
		if (MyPawn != nullptr)
		{
			MyCharacter = Cast<AGammaCharacter>(MyPawn);
		}
	}
	else if ((MyCharacter != nullptr)
		&& (GetGameTimeSinceCreation() > 1.0f))
	{

		// Set move values by character's flavour
		if (MoveSpeed == -1.0f)
		{
			MoveSpeed = MyCharacter->GetMoveSpeed();
			TurnSpeed = MyCharacter->GetTurnSpeed();
			MovesPerSec = MyCharacter->GetMovesPerSec();
			PrefireTime = MyCharacter->GetPrefireTime();
		}

		// Got a player - stunt on'em
		if (HasAuthority() && (Player != nullptr) && IsValid(Player)
			&& (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.5f))
		{
			// Reation time
			if (ReactionTiming(DeltaSeconds))
			{
				Tactical(FVector::ZeroVector);
			}

			// Update attacks to be
			if (PrefireTimer >= PrefireTime
				&& MyCharacter->GetActiveFlash() != nullptr)
			{
				MyCharacter->ReleaseAttack();
			}


			// Finial line draw to Player
			/*DrawDebugLine(GetWorld(), GetPawn()->GetActorLocation(), Player->GetActorLocation(),
			FColor::White, false, -1.0f, 0, 3.0f);*/


			// Movement
			if (TravelTimer < 2.0f)
			{
				// Get some moves
				if ((TravelTimer >= 1.0f)
					|| (LocationTarget == FVector::ZeroVector))
				{
					GetNewLocationTarget();
					TravelTimer = 0.0f;
				}

				if ((LocationTarget != FVector::ZeroVector) && !bShootingSoon
					&& UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.3f)
				{
					NavigateTo(LocationTarget);
					MoveTimer += DeltaSeconds;
					TravelTimer += DeltaSeconds;
				}
			}
		}
		else
		{
			// Locate "player" target
			bool bPlayerFound = false;
			UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), PlayersArray); // FramingActor for brawl
			if (PlayersArray.Num() > 0)
			{
				int LoopCount = PlayersArray.Num();
				for (int i = 0; i < LoopCount; ++i)
				{
					if (PlayersArray[i] != nullptr)
					{
						AActor* TempActor = PlayersArray[i];
						if (TempActor != nullptr
							&& TempActor != MyPawn
							&& TempActor != MyCharacter
							&& !TempActor->ActorHasTag("Spectator")
							&& !TempActor->ActorHasTag("Bot"))
						{
							AGammaCharacter* PotentialPlayer = Cast<AGammaCharacter>(TempActor);
							if ((PotentialPlayer != nullptr)
								&& PotentialPlayer->ActorHasTag("Player"))
							{
								Player = PotentialPlayer;
								bPlayerFound = true;
								///GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::White, FString::Printf(TEXT("p %s   targeting %s"), *MyCharacter->GetName(), *Player->GetName()));
								break;
							}
						}
					}
				}
			}
			if (!bPlayerFound)
			{
				// Edge case for spectator match with no players
				UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Bot"), PlayersArray);
				if (PlayersArray.Num() > 0)
				{
					for (int i = 0; i < PlayersArray.Num(); ++i)
					{
						if (PlayersArray[i] != nullptr)
						{
							AActor* TempActor = PlayersArray[i];
							if (TempActor != nullptr
								&& TempActor != MyPawn
								&& TempActor != MyCharacter)
							{
								AGammaCharacter* PotentialPlayer = Cast<AGammaCharacter>(TempActor);
								if (PotentialPlayer != nullptr)
								{
									Player = PotentialPlayer;
									bPlayerFound = true;
									break;
								}
							}
						}
					}
				}
			}
		}
	}
}


bool AGammaAIController::ReactionTiming(float DeltaTime)
{
	bool Result = false;
	float TimeDilat = MyCharacter->CustomTimeDilation;
	ReactionTimer += (DeltaTime * TimeDilat);
	
	if (ReactionTimer >= ReactionTime)
	{
		Result = true;
		float RandomOffset = FMath::FRandRange(ReactionTime * -0.5f, ReactionTime * 0.25f);
		ReactionTimer = RandomOffset;
	}

	return Result;
}


void AGammaAIController::AimAtTarget(AActor* TargetActor)
{
	FVector LocalForward = MyCharacter->GetAttackScene()->GetForwardVector();
	FVector ToPlayer = Player->GetActorLocation() - MyCharacter->GetActorLocation();
	
	// AIM
	// X
	float LateralDistance = ToPlayer.X;
	if (LateralDistance < 0.0f) {
		MyCharacter->SetX(-1.0f, 1.0f);
	} else {
		MyCharacter->SetX(1.0f, 1.0f);
	}

	// Z
	float VerticalDistance = ToPlayer.Z;
	if (VerticalDistance > 0.0f) {
		MyCharacter->SetZ(1.0f, 1.0f);
	} else {
		MyCharacter->SetZ(-1.0f, 1.0f);
	}

	// LOS
	FVector ForwardNorm = LocalForward.GetSafeNormal();
	FVector ToPlayerNorm = ToPlayer.GetSafeNormal();
	float DotToPlayer = FVector::DotProduct(ForwardNorm, ToPlayerNorm);
	float RangeToPlayer = ToPlayer.Size();
	if (RangeToPlayer <= PrimaryRange)
	{

		float AngleToPlayer = FMath::RadiansToDegrees(FMath::Acos(DotToPlayer));
		float AbsAngle = FMath::Abs(AngleToPlayer);
		if (AbsAngle <= ShootingAngle) {
			bViewToTarget = true;
		} else {
			bViewToTarget = false;
		}
	}
}


bool AGammaAIController::HasViewToTarget()
{
	return bViewToTarget;
}


// TACTICAL ///////////////////////////////////////////////////////
void AGammaAIController::Tactical(FVector Target)
{
	// Random number to evoke choice
	float RandomDc = FMath::FRandRange(0.0f, 100.0f);
	float HandBrakeVal = 2.0f;
	float ChargeVal = 50.0f;
	float SecoVal = 70.0f;


	// POINT-BLANK stunts
	FVector LocalForward = MyCharacter->GetAttackScene()->GetForwardVector();
	FVector ToPlayer = Player->GetActorLocation() - MyCharacter->GetActorLocation();
	float DistToPlayer = ToPlayer.Size();
	if (DistToPlayer < 1000.0f)
	{
		if (HasViewToTarget())
		{
			if (MyCharacter->GetActiveFlash() != nullptr)
			{
				float Prefire = MyCharacter->GetPrefireTime();
				float MagnitudePreference = 0.2f + FMath::FRand();
				if (Prefire >= MagnitudePreference)
				{
					MyCharacter->CheckAttackOff();
				}
			}
			else
			{
				MyCharacter->CheckAttackOn();
			}
		}
		else
		{
			AimAtTarget(Player);
		}
	}


	// HANDBRAKE
	if (RandomDc <= HandBrakeVal)
	{
		// Hand brake
		FVector CurrentHeading = MyCharacter->GetCharacterMovement()->Velocity.GetSafeNormal();
		FVector TargetHeading = (LocationTarget - MyPawn->GetActorLocation()).GetSafeNormal();
		float DotToTarget = FVector::DotProduct(CurrentHeading, TargetHeading);
		if (DotToTarget < -0.15f) /// or, just over 90 degrees away
		{
			MyCharacter->CheckPowerSlideOn();
		}
		else
		{
			MyCharacter->CheckPowerSlideOff();
		}
	}
	
	// CHARGE
	else if (RandomDc <= ChargeVal)
	{
		if ((MyCharacter != nullptr) && IsValid(MyCharacter))
		{
			if (MyCharacter->GetActiveBoost() != nullptr)
			{
				// Decide whether to keep or lose the boost
				FVector MyVelNorm = MyCharacter->GetCharacterMovement()->Velocity.GetSafeNormal();
				FVector ToLocNorm = (LocationTarget - MyPawn->GetActorLocation()).GetSafeNormal();
				float DotToTarget = FVector::DotProduct(MyVelNorm, ToLocNorm);
				
				// Call off existing boost if we're going off-course
				if (DotToTarget < 0.777f) /// roughly 39 degrees
				{
					MyCharacter->DisengageKick();
				}
			}
			else
			{
				// Charge
				if (MyCharacter->GetCharge() <= 3.0f) /// dirty hardcode!
				{
					MyCharacter->CheckPowerSlideOff();
					MyCharacter->RaiseCharge();
				}
			}
		}
	}

	// COMBAT
	else if (MyCharacter != nullptr)
	{
		bShootingSoon = true; /// trivial?
		
		// Aim - leads to attacks and secondaries
		FVector ForwardNorm = LocalForward.GetSafeNormal();
		FVector ToPlayerNorm = ToPlayer.GetSafeNormal();
		float VerticalNorm = FMath::FloorToFloat(FMath::Clamp((ToPlayer.GetSafeNormal()).Z, -1.0f, 1.0f));
		float DotToPlayer = FVector::DotProduct(ForwardNorm, ToPlayerNorm);
		float RangeToPlayer = ToPlayer.Size();
		if (RangeToPlayer <= PrimaryRange)
		{
			
			// Only fire if a) we're on screen & b) angle looks good
			float AngleToPlayer = FMath::RadiansToDegrees(FMath::Acos(DotToPlayer));

			if (AngleToPlayer <= ShootingAngle)  // && MyCharacter->WasRecentlyRendered(0.15f)
			{
				

				// Line up a shot with player Z input
				if (FMath::Abs(AngleToPlayer) <= 0.25f)
				{
					MyCharacter->SetZ(0.0f, 1.0f);
				}
				else
				{
					MyCharacter->SetZ(VerticalNorm, 1.0f);
				}

				// Aim input
				float XTarget = FMath::Clamp(ToPlayer.X, -1.0f, 1.0f);
				MyCharacter->SetX(XTarget, 1.0f);
				
				// Fire Secondary
				if (RandomDc < SecoVal
					&& (ToPlayer.Size() <= SecondaryRange)
					&& !MyCharacter->GetActiveSecondary())
				{
					MyCharacter->CheckSecondaryOn();
				}
				else
				{

					// Init Attack
					if (ToPlayer.Size() <= PrimaryRange)
					{
						if (MyCharacter->GetActiveFlash() == nullptr)
						{
							MyCharacter->CheckAttackOn();
						}
						else if ((MyCharacter->GetActiveFlash() != nullptr) && (MyCharacter->GetPrefireTime() >= 0.1f))
						{
							MyCharacter->CheckAttackOff(); /// will trigger an attack if prefire-timer > 0
						}
					}
					else if (MyCharacter->GetPrefireTime() >= 0.1f)
					{
						MyCharacter->CheckAttackOff();
					}
				}
			}
		}

		bShootingSoon = false;
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::White, TEXT("MyCharacter not valid"));
	}
}



FVector AGammaAIController::GetNewLocationTarget()
{
	// Basic ingredients
	FVector Result = FVector::ZeroVector;
	if (Player != nullptr)
	{
		FVector PlayerLocation = Player->GetActorLocation();
		//FVector AILocation = GetPawn()->GetActorLocation();

		// Getting spicy
		float MyVelocity = MyCharacter->GetCharacterMovement()->Velocity.Size();
		float VelocityScalar = FMath::Clamp((1 / (1 / MyVelocity)), 1.0f, MoveRange);
		float DynamicMoveRange = MoveRange + VelocityScalar; /// usually 100 :P
		FVector PlayerVelocity = Player->GetCharacterMovement()->Velocity;
		PlayerVelocity *= 0.55f;
		FVector PlayerAtSpeed = PlayerLocation + (PlayerVelocity * 0.5f * Aggression);
		
		// Randomness in movement
		FVector RandomOffset = (FMath::VRand() * DynamicMoveRange) * (1 / Aggression);
		RandomOffset.Y = 0.0f;
		RandomOffset.Z *= 0.25f;

		// Take care to avoid crossing Player's line of fire
		float VerticalDistToPlayer = FMath::Abs((Result - MyPawn->GetActorLocation()).Z);
		/// Currently this is really just shaving AI's intended location vertically
		/// Depending on whether we're closer to world centre. Roookie!
		if ((VerticalDistToPlayer >= 500.0f)
			&& ( FMath::Abs(RandomOffset.Z) > FMath::Abs(PlayerVelocity.Z) ))
		{
			RandomOffset.Z *= (-0.15f);
		}

		// And serve
		Result = (PlayerAtSpeed + RandomOffset);
		bCourseLayedIn = true;
		LocationTarget = Result;

		// Debug basin
		//float DistToNewTarget = (LocationTarget - MyPawn->GetActorLocation()).Size();
		//GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::White, FString::Printf(TEXT("DynamicMoveRange: %f"), DynamicMoveRange));
		//GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::White, FString::Printf(TEXT("DistToNewTarget: %f"), DistToNewTarget));
	}///else player is nullptr

	return Result;
}



void AGammaAIController::NavigateTo(FVector Target)
{

	// Basics and line to Target
	FVector MyLocation = MyCharacter->GetActorLocation();
	FVector ToTarget = (Target - MyLocation);

	/*DrawDebugLine(GetWorld(), MyLocation, Target,
		FColor::Green, false, -1.0f, 0, 5.f);*/

	// Have we reached target?
	/// Or are we too far out vertically?
	if (ToTarget.Size() < 500.0f) /// || (ToTarget.Z > (ToTarget.X * 1.5f))
	{
		LocationTarget = FVector::ZeroVector;
		bCourseLayedIn = false;
		return;
	}
	else if (GetWorld()) //  && MoveTimer >= (1 / MovesPerSec)
	{
		// Use the handbrake if we're off target
		float ValueX = 0.0f;
		float ValueZ = 0.0f;
		
		// Compare movement priorites by distance
		float VerticalDistance = FMath::Abs(ToTarget.Z);
		float LateralDistance = FMath::Abs(ToTarget.X);
		float LongitudeDistance = FMath::Abs(ToTarget.Y);

		//bool bMoved = false;

		// Simulating decision between vertical and lateral
		if (FMath::Rand() >= 0.1f)
		{
			ValueX = FMath::Clamp((ToTarget.X * 0.01f), -1.0f, 1.0f);
			MyCharacter->SetX(ValueX, 1.0f);
		}
		if (FMath::Rand() >= 0.11f)
		{
			ValueZ = FMath::Clamp((ToTarget.Z * 0.01f), -1.0f, 1.0f);

			MyCharacter->SetZ(ValueZ, 1.0f);
		}


		//MoveInput = FVector(ValueX, 0.0f, ValueZ).GetSafeNormal();
		//FVector CurrentV = MyCharacter->GetMovementComponent()->Velocity.GetSafeNormal();

		//// Move by dot product for skating effect
		//if (MoveInput != FVector::ZeroVector)
		//{
		//	float MoveByDot = 0.0f;
		//	float DotToInput = FVector::DotProduct(MoveInput, CurrentV);
		//	float AngleToInput = TurnSpeed * FMath::Abs(FMath::Clamp(FMath::Acos(DotToInput), -90.0f, 90.0f));
		//	MoveByDot = MoveSpeed + (AngleToInput * MoveSpeed);
		//	MyCharacter->GetCharacterMovement()->MaxFlySpeed = MoveByDot / 3.0f;
		//	MyCharacter->GetCharacterMovement()->MaxAcceleration = MoveByDot;
		//	MyPawn->AddMovementInput(MoveInput * MoveByDot);

		//	MoveTimer = 0.0f;
		//}


		// Sprite flipping
		if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.25f)
		{
			if (ValueX < 0.0f)
			{
				FRotator Fint = FMath::RInterpTo(GetControlRotation(), FRotator(0.0, 180.0f, 0.0f), GetWorld()->DeltaTimeSeconds, 15.0f);
				SetControlRotation(Fint);
			}
			else if (ValueX > 0.0f)
			{
				FRotator Fint = FMath::RInterpTo(GetControlRotation(), FRotator(0.0, 0.0f, 0.0f), GetWorld()->DeltaTimeSeconds, 15.0f);
				SetControlRotation(Fint);
			}
			else
			{
				// No Input - finish rotation
				if (GetControlRotation().Yaw > 90.0f)
				{
					FRotator Fint = FMath::RInterpTo(GetControlRotation(), FRotator(0.0, 180.0f, 0.0f), GetWorld()->DeltaTimeSeconds, 5.0f);
					SetControlRotation(Fint);
				}
				else
				{
					FRotator Fint = FMath::RInterpTo(GetControlRotation(), FRotator(0.0f, 0.0f, 0.0f), GetWorld()->DeltaTimeSeconds, 5.0f);
					SetControlRotation(Fint);
				}
			}
		}
	}
}


