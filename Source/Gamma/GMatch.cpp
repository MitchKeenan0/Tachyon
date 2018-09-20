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
	bReturn = true;
	
	UGameplayStatics::SetGlobalTimeDilation(this->GetWorld(), NaturalTimeScale);
}

// Called every frame
void AGMatch::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// On gg wait for gg delay before freezing time
	float GlTimeScale = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
	float TimeToMatch = GGDelayTime;
	float ScaledDeltaTime = DeltaTime * (1.0f / GlTimeScale);
	if (GlTimeScale == GGTimescale)
	{
		GGDelayTimer += ScaledDeltaTime;
		if (GGDelayTimer >= TimeToMatch)
		{
			bGG = true;
			GGDelayTimer = 0.0f;
			bReturn = true;
		}
	}
	/// Edge case - catching stalled timescale interp
	else if (!bReturn)
	{
		SetTimeScale(GGTimescale);
	}

	// Maintain fix on players
	if (!bGG && bReturn)
	{
		GetPlayers();
		HandleTimeScale(DeltaTime);
	}
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
	if (HasAuthority() && !bGG && (HitActor->ActorHasTag("Player") || (HitActor->ActorHasTag("Bot"))))
	{
		AGammaCharacter* Reciever = Cast<AGammaCharacter>(HitActor);
		AGammaCharacter* Shooter = Cast<AGammaCharacter>(Winner);
		if ((Reciever != nullptr) && (Shooter != nullptr))
		{
			// End of game?
			if (Reciever->GetHealth() <= 1.0f)
			{

				//Calcify killed HitActor
				UPaperFlipbookComponent* ActorFlipbook = Cast<UPaperFlipbookComponent>
					(Reciever->FindComponentByClass<UPaperFlipbookComponent>());
				if (ActorFlipbook != nullptr)
				{
					float CurrentPosition = FMath::FloorToInt(ActorFlipbook->GetPlaybackPosition());
					ActorFlipbook->SetPlaybackPositionInFrames(1, true);
				}

				// Freezing time to glacial
				SetTimeScale(GGTimescale);
				bReturn = false;

				// Erase Bots
				if (Reciever->ActorHasTag("Bot"))
				{
					Reciever->Tags.Add("Doomed");
				}
			}

			// Just a hit?
			else
			{
				// Timescale slowering
				float GlobalTime = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
				float HitTime = GGTimescale * 10.0f;
				if (GlobalTime != HitTime)
				{
					SetTimeScale(HitTime);
				}
			}
		}

		// Basic GG to catch enviro kills
		if ((Reciever != nullptr) 
			&& (!Reciever->ActorHasTag("Swarm")) 
			&& (Reciever->GetHealth() <= 0.0f))
		{
			SetTimeScale(GGTimescale);
			bReturn = false;

			if (Reciever->ActorHasTag("Bot"))
			{
				Reciever->Tags.Add("Doomed");
			}
		}
	}
	if (HitActor->ActorHasTag("Shield"))
	{
		float GlobalTime = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
		float HitTime = GGTimescale * 10.0f;
		if (GlobalTime != HitTime)
		{
			SetTimeScale(HitTime);
		}
	}
}


void AGMatch::HandleTimeScale(float Delta)
{
	float TimeDilat = UGameplayStatics::GetGlobalTimeDilation(this->GetWorld());
	if ((TimeDilat < 1.0f)
		&& (TimeDilat > GGTimescale))
	{
		float ReturnTime = FMath::FInterpConstantTo(TimeDilat, NaturalTimeScale, Delta, TimescaleRecoverySpeed);
		SetTimeScale(ReturnTime);
	}
}

void AGMatch::SetTimeScale(float Time)
{
	float TargetTime = Time;
	float TimeDilat = UGameplayStatics::GetGlobalTimeDilation(this->GetWorld());
	float ScaledDelta = GetWorld()->DeltaTimeSeconds;// *(1.0f / TimeDilat);
	float InterpSpeed = TimescaleRecoverySpeed * 1.5f;
	if (TargetTime != GGTimescale)
	{
		InterpSpeed *= 2.15f;
	}
	else
	{
		InterpSpeed *= 0.6f;
	}
	TargetTime = FMath::FInterpConstantTo(TimeDilat, Time, ScaledDelta, InterpSpeed);
	// doesnt get called enough = may just need to set it instantly unless more var added
	UGameplayStatics::SetGlobalTimeDilation(this->GetWorld(), TargetTime);
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
		return FText::FromString("NO TARGET");
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
