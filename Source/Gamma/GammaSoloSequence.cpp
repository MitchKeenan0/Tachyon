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
	
	if (HasAuthority())
	{
		AquirePlayer();
	}

	//// Init Rand
	//RandStream = new FRandomStream;
	//if (RandStream != nullptr)
	//{
	//	RandStream->GenerateNewSeed();
	//}

	//RunTimer = FirstEncounterTime / 5.0f;


	//// Initial burst
	//for (int i = 0; i < 50; ++i)
	//{
	//	SpawnDenizen();
	//}
}


void AGammaSoloSequence::AquirePlayer()
{
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), PlayersArray);
	int NumPlayers = PlayersArray.Num();
	if (NumPlayers > 0)
	{
		for (int i = 0; i < NumPlayers; ++i)
		{
			AActor* PlayerActor = PlayersArray[0];
			if (PlayerActor != nullptr)
			{
				Player = Cast<AGammaCharacter>(PlayerActor);
				if (Player != nullptr)
				{
					return;
				}
			}
		}
	}
	if (Player == nullptr)
	{
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Spectator"), PlayersArray);
		int NumSpectators = PlayersArray.Num();
		if (NumSpectators > 0)
		{
			for (int i = 0; i < NumSpectators; ++i)
			{
				AActor* PlayerActor = PlayersArray[0];
				if (PlayerActor != nullptr)
				{
					Player = Cast<AGammaCharacter>(PlayerActor);
					if (Player != nullptr)
					{
						return;
					}
				}
			}
		}
	}
}

// Called every frame
void AGammaSoloSequence::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority())
	{
		MainSequence(DeltaTime);
	}
}


// Main Sequence
void AGammaSoloSequence::MainSequence(float DeltaTime)
{
	RunTimer += DeltaTime;
	if (RunTimer >= FirstEncounterTime)
	{
		RunTimer = 0.0f;

		if (Player == nullptr
			|| Player->ActorHasTag("Bot"))
		{
			AquirePlayer();
		}

		SpawnDenizen();
	}
}


// Spawn Denizen handles whether to spawn or not & where
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
		//GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("No Player, Extrapolating..."));
		float RandoX = FMath::FRandRange(0.0f, 1000.0f);
		float RandoZ = FMath::FRandRange(0.0f, 1000.0f);
		PlayerWiseLocation += FVector(RandoX, 0.0f, RandoZ);
	}

	FVector RandomOffset = (FMath::VRand() * 1000);
	RandomOffset.Y = 0.0f;
	FVector SpawnLoc = PlayerWiseLocation + RandomOffset;


	// CHARACTER SPAWNING ///////////////////////////////////////////////
	if (bSpawnCharacters && (NumDenizens() < MaxLiveUnits))
	{
		// Random character each spawn
		TSubclassOf<AGammaCharacter> PlayerSpawning = nullptr;
		int Rando = PreviousSpawn;

		// Ensure different enemy each time
		while (Rando == PreviousSpawn)
		{
			Rando = FMath::FloorToInt(FMath::FRand() * 3); /// filthy hard-coded value!
		}

		// Ez track for naming the character later
		int CharacterID = 0;

		// Delineate a random Character to spawn
		switch (Rando)
		{
			case 0: PlayerSpawning = KaraokeClass;
				CharacterID = 0;
				break;
			case 1: PlayerSpawning = PeaceGiantClass;
				CharacterID = 1;
				break;
			case 2: PlayerSpawning = BaetylusClass;
				CharacterID = 2;
				break;
			default:
				break;
		}

		// Spawn is go!
		if (PlayerSpawning != nullptr)
		{

			// Spawn Character body
			AGammaCharacter* NewDenizen = Cast<AGammaCharacter>(
				GetWorld()->SpawnActor<AActor>(
					PlayerSpawning, SpawnLoc, GetActorRotation(), SpawnParams));

			if (NewDenizen != nullptr)
			{
				// Rename the character to avoid duplicate bois
				FString FreshName = NewDenizen->GetCharacterName();
				TArray<AActor*> Players;
				UGameplayStatics::GetAllActorsOfClass(GetWorld(), PlayerSpawning, Players);
				if (Players.Num() > 0)
				{
					int NumPlayers = Players.Num();
					for (int i = 0; i < NumPlayers; ++i)
					{
						AActor* CurrentActor = Players[i];
						if ((CurrentActor != nullptr) && (CurrentActor != NewDenizen))
						{
							AGammaCharacter* CurrentCharacter = Cast<AGammaCharacter>(CurrentActor);
							if (CurrentCharacter != nullptr)
							{
								FString CurrentName = CurrentCharacter->GetCharacterName();
								FString DenizenName = NewDenizen->GetCharacterName();
								if (CurrentName.Equals(DenizenName))
								{
									switch (CharacterID)
									{
									case 0: FreshName = "AJIEL";
										break;
									case 1: FreshName = "ELTIGAL";
										break;
									case 2: FreshName = "CASSIS";
										break;
									}
								}
							}
						}
					}
				}

				NewDenizen->SetCharacterName(FreshName);

				// Spawn AI Controller for body
				AGammaAIController* DenizenController = Cast<AGammaAIController>(
					GetWorld()->SpawnActor<AActor>(
						AIControllerClass, FVector::ZeroVector, GetActorRotation(), SpawnParams));

				if ((NewDenizen != nullptr)
					&& (DenizenController != nullptr))
				{
					// Install AI Controller
					NewDenizen->Controller = DenizenController;
					DenizenController->Possess(NewDenizen);

					// "Alert the media"
					NewDenizen->Tags.Add("Bot");
					DenizenArray.Add(NewDenizen);

					++Spawns;
					PreviousSpawn = Rando;
				}
			}
		}
	}

	// OBSTACLE SPAWNING ////////////////////////////////////////////////
	if (bSpawnObstacles && (ObstacleArray.Num() < MaxObstacles))
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
			// Re-Using SpawnLoc but get new position
			FVector Offset = (FMath::VRand() * 3000.0f);
			Offset.X *= 2.0f;
			Offset.Y = 0.0f;
			SpawnLoc = PlayerWiseLocation + Offset;

			float RandF = FMath::FRandRange(-1.0f, 1.0f);
			float RandG = FMath::FRandRange(-1.0f, 1.0f);
			float RandH = FMath::FRandRange(-1.0f, 1.0f);
			FRotator SpawnRotation = FRotator(RandF * 45.0f, RandG * 45.0f, RandH * 45.0f);

			AActor* NewObstacle = GetWorld()->SpawnActor<AActor>(ObstacleSpawning, SpawnLoc, SpawnRotation, SpawnParams);
			if (NewObstacle != nullptr)
			{
				//ObstacleArray.Add(NewObstacle);
				ObstacleArray.Emplace(NewObstacle);
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
		AActor* Act = DenizenArray[i];
		if (Act != nullptr)
		{
			Result += 1;

			/*if (Act != Cast<AActor>(KaraokeClass->GetOwnerClass())
				&& Act != Cast<AActor>(PeaceGiantClass->GetOwnerClass())
				&& Act != Cast<AActor>(BaetylusClass->GetOwnerClass()))
			{
				Result += 1;
			}*/
		}
	}

	// No denizens?
	if ((Result == 0 && (Spawns > 0)))
	{
		RunTimer = 0;
	}

	// Obstacle trimming based on dist
	if (Player != nullptr)
	{
		int NumObstacles = ObstacleArray.Num();
		if (NumObstacles > 0)
		{

			for (int i = NumObstacles - 1; i > 0; --i)
			{
				if (ObstacleArray[i] != nullptr)
				{
					
					AActor* ThisObs = ObstacleArray[i];
					if (ThisObs != nullptr)
					{
						float DistToObs = FVector::Dist(ThisObs->GetActorLocation(), Player->GetActorLocation());
						if (DistToObs >= 3500.0f)
						{
							ThisObs->Destroy();
							ThisObs = nullptr;

							ObstacleArray.RemoveAt(i);
						}
					}
				}
			}
		}
	}
	
	/// End of NumDenizen
	return Result;
}

