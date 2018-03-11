// Fill out your copyright notice in the Description page of Project Settings.

#include "GammaAIController.h"


void AGammaAIController::BeginPlay()
{
	Super::BeginPlay();

	// Get our Gamma Character
	MyPawn = GetPawn();
	MyCharacter = Cast<AGammaCharacter>(MyPawn);
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

		if (Player != nullptr)
		{
			// Got a player - stunt on'em
			// Update prefire
			if ((MyCharacter->GetActiveFlash() != nullptr))
			{
				MyCharacter->PrefireTiming();
			}

			// Reation time
			PrefireTimer += DeltaSeconds;
			if (ReactionTiming(DeltaSeconds)
				&& MyCharacter->GetRootComponent()
				&& MyCharacter->GetRootComponent()->bVisible)
			{
				Tactical(FVector::ZeroVector);
			}

			// Get some moves
			if (TravelTimer >= 3.0f || 
				LocationTarget == FVector::ZeroVector)
			{
				GetNewLocationTarget();
				TravelTimer = 0.0f;
			}
			else
			{
				NavigateTo(LocationTarget);
				MoveTimer += DeltaSeconds;
				TravelTimer += DeltaSeconds;
			}
		}
		else
		{
			// Locating actual player
			UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("FramingActor"), PlayersArray);
			if (PlayersArray.Num() > 0)
			{
				for (int i = 0; i < PlayersArray.Num(); ++i)
				{
					if (PlayersArray[i] != nullptr
						&& PlayersArray[i] != MyPawn
						&& !PlayersArray[i]->ActorHasTag("Spectator"))
					{
						AGammaCharacter* PotentialPlayer = Cast<AGammaCharacter>(PlayersArray[i]);
						if (PotentialPlayer != nullptr)
						{
							Player = PotentialPlayer;
							break;
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
	ReactionTimer += DeltaTime;
	if (ReactionTimer >= ReactionTime)
	{
		Result = true;
		float RandomOffset = FMath::FRandRange(-ReactionTime, 0.0f);
		ReactionTimer = RandomOffset;
	}

	return Result;
}


void AGammaAIController::Tactical(FVector Target)
{
	// Random number to evoke choice
	float RandomDc = FMath::FRandRange(0.0f, 10.0f);

	if (RandomDc <= 0.09f)
	{
		// Chance to do nothing
		return;
	}
	if (RandomDc < 3.68f)
	{
		// Charge
		MyCharacter->RaiseCharge();
	}
	else if (MyCharacter != nullptr)
	{
		// Attack and Secondary
		// 
		FVector LocalForward = MyCharacter->GetActorForwardVector();
		FVector ToPlayer = Player->GetActorLocation() - MyCharacter->GetActorLocation();
		float VerticalDist = FMath::FloorToFloat(FMath::Clamp(ToPlayer.Z, -1.0f, 1.0f));
		//float LateralDist = FMath::FloorToFloat(FMath::Clamp(ToPlayer.X, -1.0f, 1.0f));
		
		// Aim
		float DotToPlayer = FVector::DotProduct(LocalForward.GetSafeNormal(), ToPlayer.GetSafeNormal());
		float RangeToPlayer = ToPlayer.Size();
		if (DotToPlayer > 0.0f
			&& RangeToPlayer <= PrimaryRange)
		{
			float AngleToPlayer = FMath::Acos(DotToPlayer);
			if (AngleToPlayer <= ShootingAngle)
			{
				// Forward vs angle aiming
				if (FMath::Abs(AngleToPlayer) <= 0.25f)
				{
					MyCharacter->SetZ(0.0f);
				}
				else
				{
					MyCharacter->SetZ(VerticalDist);
				}

				// Chance to secondary..
				if (RandomDc < 6.0f
					&& (ToPlayer.Size() <= SecondaryRange))
				{
					MyCharacter->FireSecondary();
				}
				// .. Or fire attack
				else if (!MyCharacter->GetActiveFlash()
					&& ToPlayer.Size() <= PrimaryRange
					&& PrefireTimer >= PrefireTime)
				{
					MyCharacter->InitAttack();
					PrefireTimer = 0.0f;
				}
			}
		}
	}
}


FVector AGammaAIController::GetNewLocationTarget()
{
	// Basic ingredients
	FVector Result = FVector::ZeroVector;
	if (Player != nullptr)
	{
		FVector PlayerLocation = Player->GetActorLocation();
		FVector AILocation = GetPawn()->GetActorLocation();

		// Getting spicy
		FVector PlayerAtSpeed = PlayerLocation + (Player->GetCharacterMovement()->Velocity * Aggression);
		FVector RandomOffset = (FMath::VRand() * MoveRange) * (1 / Aggression);
		RandomOffset.Y = 0.0f;
		Result = (PlayerAtSpeed - AILocation) + RandomOffset;
		Result.Z *= 0.5f;
		

		// And serve
		bCourseLayedIn = true;
		LocationTarget = Result;
	}
	return Result;
}


void AGammaAIController::NavigateTo(FVector Target)
{

	// Basics and line to Target
	FVector MyLocation = MyPawn->GetActorLocation();
	FVector ToTarget = (Target - MyLocation);

	// Have we reached target?
	if (ToTarget.Size() < 550.0f)
	{
		LocationTarget = FVector::ZeroVector;
		bCourseLayedIn = false;
		return;
	}
	else if (GetWorld() && MoveTimer >= (1 / MovesPerSec))
	{
		// Compare movement priorites by distance
		float VerticalDistance = FMath::Abs(ToTarget.Z);
		float LateralDistance = FMath::Abs(ToTarget.X);
		float ValueX = 0.0f;
		float ValueZ = 0.0f;
		bool bMoved = false;

		// Simulating decision between vertical and lateral
		if (LateralDistance > 100)
		{
			ValueX = FMath::Clamp(ToTarget.X, -1.0f, 1.0f);
		}
		if (VerticalDistance > 100)
		{
			ValueZ = FMath::Clamp(ToTarget.Z, -1.0f, 1.0f);
		}

		MoveInput = FVector(ValueX, 0.0f, ValueZ).GetSafeNormal();
		FVector CurrentV = MyCharacter->GetMovementComponent()->Velocity.GetSafeNormal();

		// Move by dot product for skating effect
		if (MoveInput != FVector::ZeroVector)
		{
			float MoveByDot = 0.0f;
			float DotToInput = FVector::DotProduct(MoveInput, CurrentV);
			float AngleToInput = TurnSpeed * FMath::Abs(FMath::Clamp(FMath::Acos(DotToInput), -90.0f, 90.0f));
			MoveByDot = MoveSpeed + (AngleToInput * MoveSpeed);
			MyCharacter->GetCharacterMovement()->MaxFlySpeed = MoveByDot / 3.0f;
			MyCharacter->GetCharacterMovement()->MaxAcceleration = MoveByDot;
			MyPawn->AddMovementInput(MoveInput * MoveByDot);
			bMoved = true;
			MoveTimer = 0.0f;
		}

		// We've arrived
		if (!bMoved)
		{
			// Cancel move
			LocationTarget = FVector::ZeroVector;
			bCourseLayedIn = false;
		}

		// Sprite flipping
		if (ValueX < 0.0f)
		{
			SetControlRotation(FRotator(0.0, 180.0f, 0.0f));
		}
		else if (ValueX > 0.0f)
		{
			SetControlRotation(FRotator(0.0f, 0.0f, 0.0f));
		}
	}
}


