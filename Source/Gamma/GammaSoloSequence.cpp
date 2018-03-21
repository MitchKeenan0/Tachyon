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
	
	AquirePlayer();

	// Init Rand
	RandStream = new FRandomStream;
	if (RandStream != nullptr)
	{
		RandStream->GenerateNewSeed();
	}

	RunTimer = FirstEncounterTime / 5.0f;
}


void AGammaSoloSequence::AquirePlayer()
{
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), PlayersArray);
	if (PlayersArray.Num() > 0)
	{
		if (PlayersArray[0] != nullptr)
		{
			Player = Cast<AGammaCharacter>(PlayersArray[0]);
		}
	}
	if (Player == nullptr)
	{
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("FramingActor"), PlayersArray);
		if (PlayersArray.Num() > 0)
		{
			if (PlayersArray[0] != nullptr)
			{
				Player = Cast<AGammaCharacter>(PlayersArray[0]);
			}
		}
	}
}

// Called every frame
void AGammaSoloSequence::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	MainSequence(DeltaTime);

	if (Player == nullptr)
	{
		AquirePlayer();
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
	
	// Set up spawn location
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
		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("No Player, Extrapolating..."));
		float Rando = FMath::FRandRange(0.0f, 1000.0f);
		PlayerWiseLocation += FVector(Rando, 0.0f, Rando);
	}

	FVector RandomOffset = (FMath::VRand() * 1000);
	RandomOffset.Y = 0.0f;
	FVector SpawnLoc = PlayerWiseLocation + RandomOffset;


	// OBSTACLE SPAWNING ////////////////////////////////////////////////
	if (bSpawnObstacles && ObstacleArray.Num() < 10)
	{

		// Random spawning object
		int Rando = FMath::FloorToInt(FMath::FRand() * 2); /// filthy hard-coded value!
		TSubclassOf<AActor> ObstacleSpawning = nullptr;
		switch (Rando)
		{
			case 0: ObstacleSpawning = ObstacleClass1;
				break;
			case 1: ObstacleSpawning = ObstacleClass2;
				break;
			default:
				break;
		}
		if (ObstacleSpawning != nullptr)
		{

			// Position and Rotation
			SpawnLoc.X += FMath::FRandRange(-10000.0f, 10000.0f);
			SpawnLoc.Y = 0.0f; //+= FMath::FRandRange(-500.0f, 500.0f);
			SpawnLoc.Z += FMath::FRandRange(-7000.0f, 7000.0f);

			float RandF = FMath::FRandRange(-0.8f, 1.2f);
			FRotator SpawnRotation = FRotator(RandF * 90.0f, RandF * 3.0f, RandF * 9.0f);

			AActor* NewObstacle = GetWorld()->SpawnActor<AActor>(ObstacleSpawning, SpawnLoc, SpawnRotation, SpawnParams);
			
			if (NewObstacle != nullptr)
			{
				NewObstacle->SetActorScale3D(FMath::VRand() * 2.0f);
				ObstacleArray.Add(NewObstacle);
			}
		}
	}


	// CHARACTER SPAWNING ///////////////////////////////////////////////
	if (bSpawnCharacters)
	{
		// Random character each spawn
		TSubclassOf<AGammaCharacter> PlayerSpawning = nullptr;
		int Rando = PreviousSpawn;

		// Ensure different enemy each time
		while (Rando == PreviousSpawn)
		{
			Rando = FMath::FloorToInt(FMath::FRand() * 3); /// filthy hard-coded value!
		}

		// Delineate a random Character to spawn
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

		// Spawn is go!
		if (PlayerSpawning != nullptr)
		{
			AGammaCharacter* NewDenizen = Cast<AGammaCharacter>(
				GetWorld()->SpawnActor<AActor>(PlayerSpawning, SpawnLoc, GetActorRotation(), SpawnParams));
			AGammaAIController* DenizenController = Cast<AGammaAIController>(
				GetWorld()->SpawnActor<AActor>(AIControllerClass, SpawnLoc, GetActorRotation(), SpawnParams));

			if (NewDenizen != nullptr
				&& DenizenController != nullptr)
			{
				// Install AI Controller
				NewDenizen->Controller = DenizenController;
				DenizenController->Possess(NewDenizen);

				// "Alert the media"
				NewDenizen->Tags.Add("Bot");

				++Spawns;
				PreviousSpawn = Rando;
			}
		}
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
			AActor* Act = DenizenArray[i];
			if (Act != Cast<AActor>(KaraokeClass->GetOwnerClass())
				&& Act != Cast<AActor>(PeaceGiantClass->GetOwnerClass())
				&& Act != Cast<AActor>(BaetylusClass->GetOwnerClass()))
			{
				Result += 1;
			}
		}
	}

	// No denizens?
	if ((Result == 0 && (Spawns > 0)))
	{
		RunTimer = 0;
	}
	
	return Result;
}

