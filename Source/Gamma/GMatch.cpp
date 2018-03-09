// Fill out your copyright notice in the Description page of Project Settings.

#include "GMatch.h"
#include "PaperFlipbookComponent.h"


// Sets default values
AGMatch::AGMatch()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AGMatch::BeginPlay()
{
	Super::BeginPlay();
	
	SetTimeScale(NaturalTimeScale);
}

// Called every frame
void AGMatch::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// On gg wait for gg delay before freezing time
	if ((bGG || bMinorGG) && GGDelayTimer <= GGDelayTime)
	{
		GGDelayTimer += DeltaTime;
	}

	if (!PlayersAccountedFor())
	{
		//GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::White, TEXT("Looking for players..."));
		GetPlayers();
	}
	
	HandleTimeScale(DeltaTime);
}


bool AGMatch::PlayersAccountedFor()
{
	bool Result = false;
	
	if ((LocalPlayer != nullptr && LocalPlayer->GetOwner())) // && LocalPlayer->IsValidLowLevel()
	{
		//GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::White, TEXT("got local player"));
		
		if (OpponentPlayer != nullptr && OpponentPlayer->GetOwner()) // && OpponentPlayer->IsValidLowLevel()
		{
			//GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::White, TEXT("got opponent player"));
			Result = true;
		}
	}

	return Result;
}


void AGMatch::ClaimHit(AActor* HitActor, AActor* Winner)
{
	// Player killer
	if (HitActor->ActorHasTag("Player")
		|| (HitActor->ActorHasTag("Bot")))
	{
		AGammaCharacter* Reciever = Cast<AGammaCharacter>(HitActor);
		if (Reciever != nullptr)
		{

			/*FTimerHandle UnusedHandle;
			GetWorldTimerManager().SetTimer(UnusedHandle, 1.0f, false, 0.1f);*/

			// End of game?
			if (Reciever->GetHealth() <= 0.0f)
			{
				bGG = true;
				bReturn = false;

				//Calcify HitActor
				UPaperFlipbookComponent* ActorFlipbook = Cast<UPaperFlipbookComponent>
					(HitActor->FindComponentByClass<UPaperFlipbookComponent>());
				if (ActorFlipbook != nullptr)
				{
					float CurrentPosition = FMath::FloorToInt(ActorFlipbook->GetPlaybackPosition());
					//ActorFlipbook->SetPlayRate(1);
					ActorFlipbook->SetPlaybackPositionInFrames(1, true);
					///GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("Got Flipbook"));
				}

				if (HitActor->ActorHasTag("Bot"))
				{
					HitActor->Tags.Add("Doomed");
				}
			}
			else
			{
				bMinorGG = true;
				bReturn = true;
			}
		}
	}
	/*else
	{
		bMinorGG = true;
		bReturn = true;
	}
	*/

	// Mob killer
	if (HitActor->ActorHasTag("NoKill"))
	{
		bMinorGG = true;
		bReturn = true;
	}

	
}


void AGMatch::HandleTimeScale(float Delta)
{
	// Handle gameover scenario - timing and score handouts
	if ((bGG && (GGDelayTimer >= GGDelayTime))
		|| bMinorGG)
	{
		// Drop timescale to glacial..
		if (UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) > GGTimescale)
		{
			float DeltaT = UGameplayStatics::GetGlobalTimeDilation(this->GetWorld());
			float TimeT = FMath::FInterpConstantTo(DeltaT, GGTimescale, Delta, TimescaleDropSpeed);
			SetTimeScale(TimeT);
		}
		else
		{
			GGDelayTimer = 0.0f;
			bGG = false;
			bMinorGG = false;
		}
	}
	else
	{
		//GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("reading ground"));

		if (bReturn && UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) < 1.0f)
		{
			// ..Rise timescale back to 1
			float TimeDilat = UGameplayStatics::GetGlobalTimeDilation(this->GetWorld());
			float TimeT = FMath::FInterpConstantTo(TimeDilat, 1.0f, Delta, TimescaleRecoverySpeed);
			SetTimeScale(TimeT);
		}
		else
		{
			//// Clear doomed actors
			//TArray<AActor*> DoomedActors;
			//UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Doomed"), DoomedActors);
			//if (DoomedActors.Num() > 0)
			//{
			//	for (int i = 0; i < DoomedActors.Num(); ++i)
			//	{
			//		if (DoomedActors[i] != nullptr)
			//		{
			//			DoomedActors[i]->Destroy();
			//		}
			//	}
			//}
		}
	}
}

void AGMatch::SetTimeScale(float Time)
{
	UGameplayStatics::SetGlobalTimeDilation(this->GetWorld(), Time);
}


//
// Charge Gets called by ScoreboardWidget BP
float AGMatch::GetLocalChargePercent()
{
	if (LocalPlayer != nullptr)
	{
		float LocalCharge = LocalPlayer->GetChargePercentage();
		///GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Red, FString::Printf(TEXT("Charge: %f"), LocalCharge));
		return LocalCharge;
	}
	else
	{
		//GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::White, TEXT("match sees no local player"));
		return -1.0f;
	}
}
float AGMatch::GetOpponentChargePercent()
{
	if (OpponentPlayer != nullptr)
	{
		return OpponentPlayer->GetChargePercentage();
	}
	else
	{
		//GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::White, TEXT("match sees no opponent player"));
		return -1.0f;
	}
}


float AGMatch::GetLocalHealth()
{
	if (LocalPlayer != nullptr)
	{
		float LocalHealth = LocalPlayer->GetHealth();
		return LocalHealth;
	}
	else
	{
		return 0.0f;
	}
}
float AGMatch::GetOpponentHealth()
{
	if (OpponentPlayer != nullptr)
	{
		float OpponentHealth = OpponentPlayer->GetHealth();
		return OpponentHealth;
	}
	else
	{
		return 0.0f;
	}
}


void AGMatch::GetPlayers()
{
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("FramingActor"), TempPlayers);
	if (GetWorld() && TempPlayers.Num() > 0)
	{
		// Loop through to deliberate local and opponent
		for (int i = 0; i < TempPlayers.Num(); ++i)
		{
			TWeakObjectPtr<ACharacter> TChar = nullptr;
			ACharacter* TempChar = Cast<ACharacter>(TempPlayers[i]);
			if (TempChar != nullptr) //  && TempChar->IsValidLowLevel()
			{
				// Check if controller is local
				APlayerController* TempCont = Cast<APlayerController>(TempChar->GetController());
				if (TempCont && TempCont->IsLocalController())
				{
					LocalPlayer = Cast<AGammaCharacter>(TempChar);
				}
				else // or opponent
				{
					OpponentPlayer = Cast<AGammaCharacter>(TempChar);
				}
				
			}
			//else // ...or client's opponent
			//{
			//	OpponentPlayer = Cast<AGammaCharacter>(TempChar);
			//}
		}
	}
}


//
// NETWORKED PROPERTY REPLICATION
void AGMatch::GetLifetimeReplicatedProps(TArray <FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGMatch, bGG);
	DOREPLIFETIME(AGMatch, bMinorGG);
	DOREPLIFETIME(AGMatch, GGTimer);
}
