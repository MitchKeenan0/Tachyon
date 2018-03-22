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
		GetPlayers();
	}
	
	if (Role == ROLE_Authority)
	{
		HandleTimeScale(DeltaTime);
	}
}


bool AGMatch::PlayersAccountedFor()
{
	bool Result = false;
	
	if ((LocalPlayer != nullptr && LocalPlayer->GetOwner()))
	{
		if (OpponentPlayer != nullptr && OpponentPlayer->GetOwner())
		{
			Result = true;
		}
	}

	return Result;
}


void AGMatch::ClaimHit(AActor* HitActor, AActor* Winner)
{
	// Player killer
	if (!bGG && (HitActor->ActorHasTag("Player") || (HitActor->ActorHasTag("Bot")))) // || UGameplayStatics::GetGlobalTimeDilation(GetWorld()) >= 0.9f
	{
		AGammaCharacter* Reciever = Cast<AGammaCharacter>(HitActor);
		if (Reciever != nullptr)
		{
			/*FTimerHandle UnusedHandle;
			GetWorldTimerManager().SetTimer(UnusedHandle, 1.0f, false, 0.1f);*/

			// End of game
			if (Reciever->GetHealth() <= 0.0f)
			{
				bGG = true;
				bReturn = false;
				SetTimeScale(GGTimescale);

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

				/*if (HitActor->ActorHasTag("Bot"))
				{
					HitActor->Tags.Add("Doomed");
				}*/
			}
			else if (!bGG)
			{

				// Just a hit
				bMinorGG = true;
				SetTimeScale((1 - GGTimescale) * 0.33f);
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
	if (bGG)
	{
		bReturn = false;
	}

	if (bReturn && UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) < 1.0f)
	{
		// ..Rise timescale back to 1
		float TimeDilat = UGameplayStatics::GetGlobalTimeDilation(this->GetWorld());
		float TimeT = FMath::FInterpConstantTo(TimeDilat, 1.0f, Delta, TimescaleRecoverySpeed);
		SetTimeScale(TimeT);
	}

	//// Handle gameover scenario - timing and score handouts
	//if ( (bGG && (GGDelayTimer >= GGDelayTime) )
	//	|| bMinorGG)
	//{

	//	// Drop timescale to glacial..
	//	if (UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) > GGTimescale)
	//	{
	//		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("TIMING DOWN ..."));

	//		float DeltaT = UGameplayStatics::GetGlobalTimeDilation(this->GetWorld());
	//		float TimeT = FMath::FInterpConstantTo(DeltaT, GGTimescale, Delta, TimescaleDropSpeed);
	//		SetTimeScale(TimeT);
	//	}
	//	else
	//	{
	//		GGDelayTimer = 0.0f;
	//		
	//		if (bMinorGG)
	//		{
	//			bMinorGG = false;
	//		}
	//	}
	//}
	//else if (!bGG)
	//{
	//	//GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("reading ground"));

	//	if (bReturn && UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) < 1.0f)
	//	{
	//		// ..Rise timescale back to 1
	//		float TimeDilat = UGameplayStatics::GetGlobalTimeDilation(this->GetWorld());
	//		float TimeT = FMath::FInterpConstantTo(TimeDilat, 1.0f, Delta, TimescaleRecoverySpeed);
	//		SetTimeScale(TimeT);
	//	}
	//	else
	//	{
	//		

	//		//// Clear doomed actors
	//		//TArray<AActor*> DoomedActors;
	//		//UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Doomed"), DoomedActors);
	//		//if (DoomedActors.Num() > 0)
	//		//{
	//		//	for (int i = 0; i < DoomedActors.Num(); ++i)
	//		//	{
	//		//		if (DoomedActors[i] != nullptr)
	//		//		{
	//		//			DoomedActors[i]->Destroy();
	//		//		}
	//		//	}
	//		//}
	//	}
	//}
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
	if (LocalPlayer != nullptr
		&& LocalPlayer->IsValidLowLevel())
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
	if (OpponentPlayer != nullptr
		&& OpponentPlayer->IsValidLowLevel())
	{
		float OpponentHealth = OpponentPlayer->GetHealth();
		return OpponentHealth;
	}
	else
	{
		return 0.0f;
	}
}


FText AGMatch::GetLocalCharacterName()
{
	if (LocalPlayer != nullptr
		&& LocalPlayer->IsValidLowLevel())
	{
		return FText::FromString(LocalPlayer->GetCharacterName());
	}
	else
	{
		return FText::FromString("-");
	}
}


FText AGMatch::GetOpponentCharacterName()
{
	if (OpponentPlayer != nullptr
		&& OpponentPlayer->IsValidLowLevel())
	{
		return FText::FromString(OpponentPlayer->GetCharacterName());
	}
	else
	{
		return FText::FromString("-");
	}
}


void AGMatch::GetPlayers()
{
	// Spectator case
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Spectator"), TempPlayers);
	if (TempPlayers.Num() > 0)
	{
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("FramingActor"), TempPlayers);
		if (TempPlayers.Num() == 2)
		{
			LocalPlayer = Cast<AGammaCharacter>(TempPlayers[0]);
			OpponentPlayer = Cast<AGammaCharacter>(TempPlayers[1]);
		}
	}
	else
	{

		// Proper game
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
