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
	if (!bSpawned)
	{
		RunTimer += DeltaTime;
		if (RunTimer >= FirstEncounterTime)
		{
			SpawnDenizen();
		}
	}
}


// Spawn Denizen
void AGammaSoloSequence::SpawnDenizen()
{
	FActorSpawnParameters SpawnParams;
	FVector PlayerLocation = Player->GetActorLocation() + Player->GetActorForwardVector() * 1000.0f;
	FVector PlayerVelocity = Player->GetCharacterMovement()->Velocity;
	FVector AheadOfPlayer = PlayerLocation + PlayerVelocity;
	AGammaCharacter* NewDenizen = Cast<AGammaCharacter>(GetWorld()->SpawnActor<AActor>(DenizenClass, AheadOfPlayer, GetActorRotation(), SpawnParams));
	if (NewDenizen)
	{
		bSpawned = true;
		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("Spawned Denizen"));
	}
}

