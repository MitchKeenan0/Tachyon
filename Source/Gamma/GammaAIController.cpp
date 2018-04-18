// Fill out your copyright notice in the Description page of Project Settings.

#include "GammaAIController.h"


void AGammaAIController::BeginPlay()
{
	Super::BeginPlay();

	// Get our Gamma Character
	MyPawn = GetPawn();
	MyCharacter = Cast<AGammaCharacter>(MyPawn);

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
}


void AGammaAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!MyCharacter)
	{
		MyPawn = GetPawn();
		MyCharacter = Cast<AGammaCharacter>(MyPawn);
	}
	else if (MyCharacter != nullptr)
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
		if (Player != nullptr && IsValid(Player))
		{
			
			// Finial line draw to Player
			/*DrawDebugLine(GetWorld(), GetPawn()->GetActorLocation(), Player->GetActorLocation(),
				FColor::White, false, -1.0f, 0, 3.0f);*/

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

			// Get some moves
			if (TravelTimer >= (10.0f * ReactionTime)
				|| (LocationTarget == FVector::ZeroVector))
			{
				GetNewLocationTarget();
				TravelTimer = 0.0f;
			}
			else if ((LocationTarget != FVector::ZeroVector) && !bShootingSoon
				&& UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.5f)
			{
				NavigateTo(LocationTarget);
				MoveTimer += DeltaSeconds;
				TravelTimer += DeltaSeconds;
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
					AActor* TempActor = PlayersArray[i];
					if (TempActor != nullptr
						&& TempActor != MyPawn
						&& TempActor != MyCharacter
						&& !TempActor->ActorHasTag("Spectator")
						&& !TempActor->ActorHasTag("Bot"))
					{
						AGammaCharacter* PotentialPlayer = Cast<AGammaCharacter>(TempActor);
						if (PotentialPlayer != nullptr)
						{
							Player = PotentialPlayer;
							bPlayerFound = true;
							/*GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::White,
							FString::Printf(TEXT("p %s   targeting %s"), *MyCharacter->GetName(), *Player->GetName()));*/
							break;
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


bool AGammaAIController::ReactionTiming(float DeltaTime)
{
	bool Result = false;
	float TimeDilat = MyCharacter->CustomTimeDilation;
	ReactionTimer += (DeltaTime * TimeDilat);
	if (ReactionTimer >= ReactionTime)
	{
		Result = true;
		float RandomOffset = FMath::FRandRange(-ReactionTime, ReactionTime * 0.5f);
		ReactionTimer = RandomOffset;
	}

	return Result;
}


// TACTICAL ///////////////////////////////////////////////////////
void AGammaAIController::Tactical(FVector Target)
{
	// Random number to evoke choice
	float RandomDc = FMath::FRandRange(0.0f, 100.0f);
	float HandBrakeVal = 5.5f;
	float ChargeVal = 69.0f;
	float SecoVal = 81.0f;


	// HANDBRAKE
	if (RandomDc <= HandBrakeVal)
	{
		if (MyCharacter->GetCharge() > 0.0f)
		{
			// Hand brake
			FVector CurrentHeading = MyCharacter->GetCharacterMovement()->Velocity.GetSafeNormal();
			FVector TargetHeading = (LocationTarget - MyPawn->GetActorLocation()).GetSafeNormal();
			float DotToTarget = FVector::DotProduct(CurrentHeading, TargetHeading);
			if (DotToTarget < -0.15f)
			{
				MyCharacter->CheckPowerSlideOn();
			}
			else
			{
				MyCharacter->CheckPowerSlideOff();
			}
		}
		else
		{
			// Moving on to charge...
		}
	}
	
	// CHARGE
	if (RandomDc <= ChargeVal)
	{
		// Call-off existing charge boost if there is one
		if ((MyCharacter != nullptr) && IsValid(MyCharacter))
		{
			if (MyCharacter->GetActiveBoost() != nullptr) // && IsValid(MyCharacter->GetActiveBoost()) && !MyCharacter->GetActiveBoost()->IsPendingKillPending())
			{
				MyCharacter->DisengageKick();
			}
			else
			{
				// Charge
				if (MyCharacter->GetCharge() <= 3.0f) /// dirty hardcode! ChargeMax unreachable until Epic fix
				{
					MyCharacter->CheckPowerSlideOff();
					MyCharacter->RaiseCharge();
				}
				else
				{
					MyCharacter->CheckPowerSlideOn();
				}
			}
		}
	}

	// COMBAT
	else if (MyCharacter != nullptr)
	{
		bShootingSoon = true; /// trivial?
		
		// Attack and Secondary
		FVector LocalForward = MyCharacter->GetAttackScene()->GetForwardVector();
		FVector ToPlayer = Player->GetActorLocation() - MyCharacter->GetActorLocation();
		float VerticalDist = FMath::FloorToFloat(FMath::Clamp((ToPlayer.GetSafeNormal()).Z, -1.0f, 1.0f));

		// Aim - leads to attacks and secondaries
		float DotToPlayer = FVector::DotProduct(LocalForward.GetSafeNormal(), ToPlayer.GetSafeNormal());
		float RangeToPlayer = ToPlayer.Size();
		if (DotToPlayer > 0.0f
			&& RangeToPlayer <= PrimaryRange)
		{

			// Only fire if a) we're on screen & b) angle looks good
			float AngleToPlayer = FMath::Acos(DotToPlayer);
			if (AngleToPlayer <= ShootingAngle
				&& MyCharacter->WasRecentlyRendered(0.75f))
			{
				// Line up a shot with player Z input
				if (FMath::Abs(AngleToPlayer) <= 0.25f)
				{
					MyCharacter->SetZ(0.0f);
				}
				else
				{
					MyCharacter->SetZ(VerticalDist);
				}

				// Aim input
				float XTarget = FMath::Clamp(ToPlayer.X, -1.0f, 1.0f);
				MyCharacter->SetX(XTarget);
				
				// Fire Secondary
				if (RandomDc < SecoVal
					&& (ToPlayer.Size() <= SecondaryRange)
					&& !MyCharacter->GetActiveSecondary())
				{
					MyCharacter->FireSecondary();
				}
				else
				{
					// Init Attack
					if ((MyCharacter->GetActiveFlash() == nullptr)
						&& ToPlayer.Size() <= PrimaryRange)
					{
						MyCharacter->InitAttack();
					}
					else
					{
						MyCharacter->ReleaseAttack();
					}
				}
			}
		}

		bShootingSoon = false;
	}
}



FVector AGammaAIController::GetNewLocationTarget()
{
	// Basic ingredients
	FVector Result = FVector::ZeroVector;
	if (Player != nullptr)
	{
		FVector PlayerLocation = Player->GetActorLocation();
		//FVector AILocation = GetPawn()->GetActorLocation(); // not used

		// Getting spicy
		float MyVelocity = MyCharacter->GetCharacterMovement()->Velocity.Size();
		float DynamicMoveRange = MoveRange + (1 / (1 / MyVelocity)); /// woah
		///GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("DMR: %f"), DynamicMoveRange));
		FVector PlayerAtSpeed = PlayerLocation + (Player->GetCharacterMovement()->Velocity * Aggression);
		
		// Randomness in movement
		FVector RandomOffset = (FMath::VRand() * DynamicMoveRange) * (1 / Aggression);
		RandomOffset.Y = 0.0f;
		FVector NextRand = FMath::VRandCone(MyCharacter->GetActorForwardVector(), ShootingAngle) * DynamicMoveRange * -1.0f;
		NextRand.Y = 0.0f;
		FVector CombinedRand = RandomOffset + NextRand;
		CombinedRand = CombinedRand.GetClampedToMaxSize(MoveRange);
		CombinedRand.Z *= 0.25f;

		// And serve
		Result = PlayerAtSpeed + CombinedRand;
		
		bCourseLayedIn = true;
		LocationTarget = Result;
	}
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
	if (ToTarget.Size() < 550.0f) /// || (ToTarget.Z > (ToTarget.X * 1.5f))
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
		if (LateralDistance != 0.0f)
		{
			ValueX = FMath::Clamp((ToTarget.X * 0.01f), -1.0f, 1.0f);
			MyCharacter->SetX(ValueX);
		}
		if (VerticalDistance != 0.0f
			|| LongitudeDistance != 0.0f)
		{
			ValueZ = FMath::Clamp((ToTarget.Z * 0.01f), -1.0f, 1.0f);

			MyCharacter->SetZ(ValueZ);
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
				FRotator Fint = FMath::RInterpTo(GetControlRotation(), FRotator(0.0, 180.0f, 0.0f), GetWorld()->DeltaTimeSeconds, 20.0f);
				SetControlRotation(Fint);
			}
			else if (ValueX > 0.0f)
			{
				FRotator Fint = FMath::RInterpTo(GetControlRotation(), FRotator(0.0, 0.0f, 0.0f), GetWorld()->DeltaTimeSeconds, 20.0f);
				SetControlRotation(Fint);
			}
			else
			{
				// No Input - finish rotation
				if (GetControlRotation().Yaw > 90.0f)
				{
					FRotator Fint = FMath::RInterpTo(GetControlRotation(), FRotator(0.0, 180.0f, 0.0f), GetWorld()->DeltaTimeSeconds, 20.0f);
					SetControlRotation(Fint);
				}
				else
				{
					FRotator Fint = FMath::RInterpTo(GetControlRotation(), FRotator(0.0f, 0.0f, 0.0f), GetWorld()->DeltaTimeSeconds, 20.0f);
					SetControlRotation(Fint);
				}
			}
		}
	}
}


