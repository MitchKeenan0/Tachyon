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
	else
	{
		// Locating player
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), PlayersArray);
		if (PlayersArray.Num() > 0)
		{
			for (int i = 0; i < PlayersArray.Num(); ++i)
			{
				if (PlayersArray[i] != GetPawn())
				{
					AGammaCharacter* PotentialPlayer = Cast<AGammaCharacter>(PlayersArray[i]);
					if (PotentialPlayer && PotentialPlayer->IsValidLowLevel())
					{
						Player = PotentialPlayer;
						break;
					}
				}
			}
		}
		if (Player->IsValidLowLevel())
		{
			// Got a player - stunt on'em
			// Update prefire
			if ((MyCharacter->GetActiveFlash()->IsValidLowLevel()))
			{
				MyCharacter->PrefireTiming();
			}

			// Reation time
			if (ReactionTiming(DeltaSeconds)
				&& MyCharacter->GetRootComponent()->bVisible)
			{
				Tactical(FVector::ZeroVector);
			}

			// Get some moves
			if (LocationTarget == FVector::ZeroVector)
			{
				GetNewLocationTarget();
			}
			else
			{
				NavigateTo(LocationTarget);
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

	// Charge
	if (RandomDc < 3.9f)
	{
		if (MyCharacter->GetCharge() < 4)
		{
			MyCharacter->RaiseCharge();
		}
	}

	// Shooting
	else
	{
		// Considerations for shooting
		FVector LocalForward = MyCharacter->GetActorForwardVector();
		FVector ToPlayer = Player->GetActorLocation() - MyCharacter->GetActorLocation();
		float VerticalDir = FMath::FloorToFloat(FMath::Clamp(ToPlayer.Z, -1.0f, 1.0f));
		//float LateralDir = FMath::FloorToFloat(FMath::Clamp(ToPlayer.X, -1.0f, 1.0f));
		
		// Aim
		float DotToPlayer = FVector::DotProduct(LocalForward, ToPlayer);
		if (DotToPlayer > 0.0f)
		{
			float AngleToPlayer = FMath::Acos(DotToPlayer);
			if (AngleToPlayer <= ShootingAngle)
			{

				// Firing
				if (!MyCharacter->GetActiveFlash()->IsValidLowLevel())
				{
					MyCharacter->SetZ(VerticalDir);
					MyCharacter->InitAttack();
				}
			}
		}
	}
}


FVector AGammaAIController::GetNewLocationTarget()
{
	// Basic ingredients
	FVector Result = FVector::ZeroVector;
	FVector PlayerLocation = Player->GetActorLocation();
	/// not used	FVector AILocation = GetPawn()->GetActorLocation();
	
	// Getting spicy
	FVector PlayerAtSpeed = PlayerLocation + (Player->GetCharacterMovement()->Velocity * Aggression);
	FVector RandomOffset = (FMath::VRand() * MoveRange) * (1 / Aggression);
	RandomOffset.Y = 0.0f;
	Result = PlayerAtSpeed + RandomOffset;
	Result.Z *= 0.8f;

	// And serve
	bCourseLayedIn = true;
	LocationTarget = Result;
	return Result;
}


void AGammaAIController::NavigateTo(FVector Target)
{
	// Basics and line to Target
	FVector MyLocation = GetPawn()->GetActorLocation();
	FVector ToTarget = (Target - MyLocation);

	// Have we reached target?
	if (ToTarget.Size() < 550.0f)
	{
		LocationTarget = FVector::ZeroVector;
		bCourseLayedIn = false;
		return;
	}
	else
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
			MoveInput = FVector::ForwardVector;
			GetPawn()->AddMovementInput(MoveInput, MoveSpeed * ValueX);
			bMoved = true;
		}
		if (VerticalDistance > 100)
		{
			ValueZ = FMath::Clamp(ToTarget.Z, -1.0f, 1.0f);
			MoveInput = FVector::UpVector;
			GetPawn()->AddMovementInput(MoveInput, MoveSpeed * ValueZ);
			bMoved = true;
		}

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


