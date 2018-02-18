// Fill out your copyright notice in the Description page of Project Settings.

#include "GammaSoloSequence.h"


// Sets default values
AGammaSoloSequence::AGammaSoloSequence()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AGammaSoloSequence::BeginPlay()
{
	Super::BeginPlay();
	
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), PlayersArray);
	if (PlayersArray.Num() > 0)
	{
		Player = Cast<AGammaCharacter>(PlayersArray[0]);
	}
}

// Called every frame
void AGammaSoloSequence::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	MainSequence(DeltaTime);
}


// Main Sequence
void AGammaSoloSequence::MainSequence(float DeltaTime)
{
	RunTimer += DeltaTime;
	if (RunTimer >= FirstEncounterTime)
	{
		RunTimer = 0.0f;
		if (NumDenizens() < MaxLiveUnits)
		{
			SpawnDenizen();
		}
	}
}


// Spawn Denizen
void AGammaSoloSequence::SpawnDenizen()
{
	FActorSpawnParameters SpawnParams;
	
	FVector PlayerWiseLocation = FVector::ZeroVector;
	if (Player && Player->IsValidLowLevel()
		&& Player->GetCharacterMovement())
	{
		FVector PlayerLocation = Player->GetActorLocation() + Player->GetActorForwardVector() * 1000.0f;
		FVector PlayerVelocity = Player->GetCharacterMovement()->Velocity;
		PlayerWiseLocation = PlayerLocation + PlayerVelocity;
	}

	FVector RandomOffset = (FMath::VRand() * 1000);
	RandomOffset.Y = 0.0f;
	FVector SpawnLoc = PlayerWiseLocation + RandomOffset;
	AGammaCharacter* NewDenizen = Cast<AGammaCharacter>(GetWorld()->SpawnActor<AActor>(DenizenClass, SpawnLoc, GetActorRotation(), SpawnParams));

	++Spawns;
}


int AGammaSoloSequence::NumDenizens()
{
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Bot"), DenizenArray);
	int Result = DenizenArray.Num();
	if ((Result == 0 && (Spawns > 0)))
	{
		RunTimer = 0;
	}
	return Result;
}

