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
	if (PlayersArray.Num() > 0)
	{
		AActor* PlayerActor = PlayersArray[0];
		if (PlayerActor != nullptr)
		{
			Player = Cast<AGammaCharacter>(PlayerActor);
		}
	}
	if (Player == nullptr)
	{
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("FramingActor"), PlayersArray);
		if (PlayersArray.Num() > 0)
		{
			AActor* PlayerActor = PlayersArray[0];
			if (PlayerActor != nullptr)
			{
				Player = Cast<AGammaCharacter>(PlayerActor);
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

		if (Player == nullptr)
		{
			AquirePlayer();
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


	// OBSTACLE SPAWNING ////////////////////////////////////////////////
	if (bSpawnObstacles && ObstacleArray.Num() < MaxLiveUnits)
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
			SpawnLoc += (FMath::VRand() * 1000.0f);
			SpawnLoc.Y = 0.0f;
			
			float RandF = FMath::FRandRange(-1.0f, 1.0f);
			float RandG = FMath::FRandRange(-1.0f, 1.0f);
			float RandH = FMath::FRandRange(-1.0f, 1.0f);
			FRotator SpawnRotation = FRotator(RandF * 45.0f, RandG * 45.0f, RandH * 45.0f);

			AActor* NewObstacle = GetWorld()->SpawnActor<AActor>(ObstacleSpawning, SpawnLoc * 5.5f, SpawnRotation, SpawnParams);
			
			if (NewObstacle != nullptr)
			{
				ObstacleArray.Add(NewObstacle);
			}
		}
	}


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
	
	return Result;
}

