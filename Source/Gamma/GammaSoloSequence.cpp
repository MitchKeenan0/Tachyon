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

	/*if (Player != nullptr)
	{
		MainSequence(DeltaTime);
	}
	else*/
	if (Player == nullptr)
	{
		//GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Blue, TEXT("no player brahhh"));
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), PlayersArray);
		if (PlayersArray.Num() > 0)
		{
			Player = Cast<AGammaCharacter>(PlayersArray[0]);
		}
	}
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
	
	FVector PlayerWiseLocation = SpawnLocation;
	if (Player != nullptr
		&& Player->GetCharacterMovement() != nullptr)
	{
		FVector PlayerLocation = Player->GetActorLocation();
		FVector PlayerVelocity = Player->GetCharacterMovement()->Velocity;
		PlayerWiseLocation = PlayerLocation + PlayerVelocity;
	}
	else
	{
		float Rando = FMath::FRandRange(0.0f, 1000.0f);
		PlayerWiseLocation = FVector(Rando, 0.0f, Rando);
	}

	FVector RandomOffset = (FMath::VRand() * 2200);
	RandomOffset.Y = 0.0f;
	FVector SpawnLoc = PlayerWiseLocation + RandomOffset;

	// Random character each spawn
	TSubclassOf<AGammaCharacter> PlayerSpawning = nullptr;
	int Rando = FMath::FloorToInt(FMath::FRand() * 3);
	
	// Ensure different enemy each time
	if (Rando == PreviousSpawn)
	{
		return;
	}

	switch (Rando)
	{
		case 0: PlayerSpawning = KaraokeClass;
			break;
		case 1: PlayerSpawning = PeaceGiantClass;
			break;
		case 2: PlayerSpawning = BaetylusClass;
			break;
		default:
			break;
	}
	AGammaCharacter* NewDenizen = Cast<AGammaCharacter>(GetWorld()->SpawnActor<AActor>(PlayerSpawning, SpawnLoc, GetActorRotation(), SpawnParams));

	if (NewDenizen != nullptr)
	{
		++Spawns;
		PreviousSpawn = Rando;

		// GameInst = GetGameInstance();
	}
	
}


int AGammaSoloSequence::NumDenizens()
{
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Bot"), DenizenArray);
	int Result = 0;

	// Loop to ensure validity of denizens
	for (int i = 0; i < DenizenArray.Num(); ++i)
	{
		if (DenizenArray[i] != nullptr)
		{
			Result += 1;
		}
	}

	// No denizens?
	if ((Result == 0 && (Spawns > 0)))
	{
		RunTimer = 0;
	}
	
	return Result;
}

