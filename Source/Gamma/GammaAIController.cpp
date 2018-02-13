// Fill out your copyright notice in the Description page of Project Settings.

#include "GammaAIController.h"


void AGammaAIController::BeginPlay()
{
	Super::BeginPlay();
}


void AGammaAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Locating player
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), PlayersArray);
	if (PlayersArray.Num() > 0)
	{
		Player = Cast<AGammaCharacter>(PlayersArray[0]);
	}
	if (Player->IsValidLowLevel())
	{
		if (ReactionTiming(DeltaSeconds))
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
	// Get our Gamma Character
	APawn* MyPawn = GetPawn();
	AGammaCharacter* MyCharacter = Cast<AGammaCharacter>(MyPawn);

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
		FVector LocalForward = MyCharacter->GetActorForwardVector();
		FVector ToPlayer = Player->GetActorLocation() - MyCharacter->GetActorLocation();
		float VerticalDir = FMath::FloorToFloat(FMath::Clamp(ToPlayer.Z, -1.0f, 1.0f));
		float DotToPlayer = FVector::DotProduct(LocalForward, ToPlayer);
		float AngleToPlayer = FMath::Acos(DotToPlayer);
		if (AngleToPlayer <= ShootingAngle)
		{
			if (MyCharacter->GetActiveFlash() != nullptr)
			{
				MyCharacter->SetZ(VerticalDir);
				MyCharacter->InitAttack();
			}
			else
			{
				MyCharacter->SetZ(VerticalDir);
				MyCharacter->ReleaseAttack();
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::White, FString::Printf(TEXT("AI Z: %f"), VerticalDir));
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
	FVector RandomOffset = (FMath::VRand() * MoveRange) * Aggression;
	Result = PlayerAtSpeed + RandomOffset;
	Result.Z *= 0.68f;

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
	if (ToTarget.Size() < 50.0f)
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


