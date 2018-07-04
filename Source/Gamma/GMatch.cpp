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
	/*if (bGG && (GGDelayTimer <= GGDelayTime))
	{
		GGDelayTimer += DeltaTime;
	}*/

	// Maintain fix on players
	if (!bGG)
	{
		GetPlayers();
	}
	

	// Timescaling
	/*if (Role == ROLE_Authority)
	{
		HandleTimeScale(DeltaTime);
	}*/
}


bool AGMatch::PlayersAccountedFor()
{
	bool Result = false;
	if (LocalPlayer != nullptr)
	{
		if (OpponentPlayer != nullptr)
		{
			Result = true;
		}
	}

	return Result;
}


void AGMatch::ClaimHit(AActor* HitActor, AActor* Winner)
{
	// Player killer
	if (!bGG && (HitActor->ActorHasTag("Player") || (HitActor->ActorHasTag("Bot"))))
	{
		AGammaCharacter* Reciever = Cast<AGammaCharacter>(HitActor);
		AGammaCharacter* Shooter = Cast<AGammaCharacter>(Winner);
		if ((Reciever != nullptr) && (Shooter != nullptr))
		{
			// End of game?
			if (Reciever->GetHealth() <= 0.0f)
			{
				if ((!Reciever->ActorHasTag("Bot"))
					|| (Shooter->ActorHasTag("Bot")))
				{
					bGG = true;

					//Calcify killed HitActor
					UPaperFlipbookComponent* ActorFlipbook = Cast<UPaperFlipbookComponent>
						(Reciever->FindComponentByClass<UPaperFlipbookComponent>());
					if (ActorFlipbook != nullptr)
					{
						float CurrentPosition = FMath::FloorToInt(ActorFlipbook->GetPlaybackPosition());
						ActorFlipbook->SetPlaybackPositionInFrames(1, true);
					}

					// Transfer timescaling to global
					//HitActor->CustomTimeDilation = 1.0f;
					//Winner->CustomTimeDilation = 1.0f;
					SetTimeScale(GGTimescale);

					// Award winner with a star ;P
					FString DecoratedName = FString(Shooter->GetCharacterName().Append(" *"));
					Shooter->SetCharacterName(DecoratedName);


					// Clear shot-out NPCs
					/*if (HitActor->ActorHasTag("Bot"))
					{
					HitActor->Tags.Add("Doomed");
					}*/
				}

				// Erase Bots
				if (Reciever->ActorHasTag("Bot"))
				{
					Reciever->NullifyAttack();
					Reciever->NullifySecondary();
					Reciever->ClearFlash();

					Reciever->Destroy();
					if (Reciever == LocalPlayer)
					{
						LocalPlayer = nullptr;
					}
					else if (Reciever == OpponentPlayer)
					{
						OpponentPlayer = nullptr;
					}
					Reciever = nullptr;
				}
			}
			else
			{
				// Hit-confirm timescaling
				float CurrentTScale = Shooter->CustomTimeDilation;
				float NewTScale = CurrentTScale * 0.11f;
				NewTScale = FMath::Clamp(NewTScale, GGTimescale * 0.1f, 1.0f);

				Shooter->NewTimescale(NewTScale);
				Reciever->NewTimescale(NewTScale);
				
				//Reciever->CustomTimeDilation = NewTScale;
				//Shooter->CustomTimeDilation = NewTScale;
				//Reciever->ForceNetUpdate();
				//Shooter->ForceNetUpdate();

				//GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Blue, TEXT("Set new timescale.."));
			}
		}

		ForceNetUpdate();
	}
}


void AGMatch::HandleTimeScale(float Delta)
{
	float TimeToSet = 1.0f;
	if (bGG && (GGDelayTimer >= GGDelayTime))
	{
		SetTimeScale(GGTimescale);
		//TimeToSet = GGTimescale;
		GGDelayTimer = 0.0f;
	}

	//else
	//{
	//	// Recovery timescale interpolation
	//	bool bRecovering = false;
	//	float TimeDilat = UGameplayStatics::GetGlobalTimeDilation(this->GetWorld());
	//	if (!bGG && bReturn && TimeDilat < 1.0f)
	//	{
	//		// ..Rise timescale back to 1
	//		float TimeT = FMath::FInterpConstantTo(TimeDilat, 1.0f, Delta, TimescaleRecoverySpeed);
	//		TimeToSet = TimeT;
	//		bRecovering = true;
	//		///GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Blue, TEXT("recovering.."));
	//	}

	//	// Return to 1nnocence
	//	if (UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) >= 1.0f)
	//	{
	//		TimeToSet = 1.0f;
	//		bReturn = false;
	//		bRecovering = false;
	//	}
	//}


	//SetTimeScale(TimeToSet);

	
	// OLD SYSTEM
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
	ForceNetUpdate();
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
		return 0.0f;
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
		return 0.0f;
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
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), TempPlayers);
		if (TempPlayers.Num() == 2)
		{
			if ((TempPlayers[0] != nullptr))
			{
				LocalPlayer = Cast<AGammaCharacter>(TempPlayers[0]);
			}
			if ((TempPlayers[1] != nullptr))
			{
				OpponentPlayer = Cast<AGammaCharacter>(TempPlayers[1]);
			}
		}

		if (PlayersAccountedFor())
		{
			return;
		}
	}
	else
	{
		// Proper game
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), TempPlayers);
		if (GetWorld() && TempPlayers.Num() > 0)
		{
			float BestDistance = 999999.0f;
			
			// Loop through to deliberate local player
			int LoopSize = TempPlayers.Num();
			for (int i = 0; i < LoopSize; ++i)
			{
				if (TempPlayers[i] != nullptr)
				{
					// Qualify valid player object
					ACharacter* TempChar = Cast<ACharacter>(TempPlayers[i]);
					if (TempChar != nullptr
						&& !TempChar->ActorHasTag("Spectator"))
					{

						// Check if controller is local
						APlayerController* TempCont = Cast<APlayerController>(TempChar->GetController());
						if ((TempCont != nullptr) && (TempCont->IsLocalController() || TempCont->ActorHasTag("Bot")))
						{
							LocalPlayer = Cast<AGammaCharacter>(TempChar);
						}

						// Get closest opponent
						else if ((TempChar != nullptr) && (TempChar != LocalPlayer) && (!TempChar->ActorHasTag("Spectator"))
							&& (LocalPlayer != nullptr))
						{
							float DistToTemp = FVector::Dist(TempChar->GetActorLocation(), LocalPlayer->GetActorLocation());
							if (DistToTemp < BestDistance)
							{
								OpponentPlayer = Cast<AGammaCharacter>(TempChar);
								BestDistance = DistToTemp;
							}
						}
					}
				}
			}
		}
	}


	//// Spectator case
	//UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Spectator"), TempPlayers);
	//if (TempPlayers.Num() > 0)
	//{
	//	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), TempPlayers);
	//	if (TempPlayers.Num() == 2)
	//	{
	//		LocalPlayer = Cast<AGammaCharacter>(TempPlayers[0]);
	//		OpponentPlayer = Cast<AGammaCharacter>(TempPlayers[1]);
	//	}
	//}
	//else
	//{

	//	// Proper game
	//	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), TempPlayers);
	//	if (GetWorld() && TempPlayers.Num() > 0)
	//	{
	//		// Loop through to deliberate local and opponent
	//		for (int i = 0; i < TempPlayers.Num(); ++i)
	//		{
	//			TWeakObjectPtr<ACharacter> TChar = nullptr;
	//			ACharacter* TempChar = Cast<ACharacter>(TempPlayers[i]);
	//			if (TempChar != nullptr) //  && TempChar->IsValidLowLevel()
	//			{
	//				// Check if controller is local
	//				APlayerController* TempCont = Cast<APlayerController>(TempChar->GetController());
	//				if (TempCont != nullptr && TempCont->IsLocalController())
	//				{
	//					LocalPlayer = Cast<AGammaCharacter>(TempChar);
	//				}
	//				else // or opponent
	//				{
	//					OpponentPlayer = Cast<AGammaCharacter>(TempChar);
	//				}
	//			}
	//			//else // ...or client's opponent
	//			//{
	//			//	OpponentPlayer = Cast<AGammaCharacter>(TempChar);
	//			//}
	//		}
	//	}
	//}
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
