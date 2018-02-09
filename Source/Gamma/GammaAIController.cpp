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
	if (!Player)
	{
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), PlayersArray);
		if (PlayersArray.Num() > 0)
		{
			Player = Cast<AGammaCharacter>(PlayersArray[0]);
		}
	}
	else
	{
		// Get some moves
		if (LocationTarget == FVector::ZeroVector)
		{
			GetNewLocationTarget();
			GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("new move target"));
		}
		else
		{
			NavigateTo(LocationTarget);
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
	FVector RandomOffset = (FMath::VRand() * MoveSpeed) * Aggression * 2;
	Result = PlayerAtSpeed + RandomOffset;

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

	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Red, FString::Printf(TEXT("LocationTarget: %f"), ToTarget.Size()));

	// Have we reached target?
	if (ToTarget.Size() < 150.0f)
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
		
		//if (FVector::DotProduct())

		// Simulating decision between vertical and lateral
		if (LateralDistance > VerticalDistance)
		{
			float ValueX = FMath::Clamp(ToTarget.X, -1.0f, 1.0f);
			MoveInput = FVector::ForwardVector;
			GetPawn()->AddMovementInput(MoveInput, MoveSpeed * ValueX);
		}
		else
		{
			float ValueZ = FMath::Clamp(ToTarget.Z, -1.0f, 1.0f);
			MoveInput = FVector::UpVector;
			GetPawn()->AddMovementInput(MoveInput, MoveSpeed * ValueZ);
		}
	}
}


