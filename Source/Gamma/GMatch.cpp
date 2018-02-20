// Fill out your copyright notice in the Description page of Project Settings.

#include "GMatch.h"


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
	if (bGG && GGDelayTimer < GGDelayTime)
	{
		GGDelayTimer += DeltaTime;
	}

	if (!PlayersAccountedFor())
	{
		//GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::White, TEXT("Looking for players..."));
		GetPlayers();
	}
	
	HandleTimeScale(bGG, DeltaTime);
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
	if (HitActor->ActorHasTag("Player"))
	{
		AGammaCharacter* Reciever = Cast<AGammaCharacter>(Winner);
		if (Reciever)
		{
			bGG = true;
			bReturn = false;
			///Reciever->RaiseScore(1);
		}
	}

	// Bot killer
	if (HitActor->ActorHasTag("Bot")
		&& !HitActor->ActorHasTag("Attack"))
	{
		bGG = true;
		bReturn = true;
		HitActor->Tags.Add("Doomed");
	}
}


void AGMatch::HandleTimeScale(bool Gg, float Delta)
{
	// Handle gameover scenario - timing and score handouts
	if (Gg && GGDelayTimer >= GGDelayTime)
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
		}
	}
	else if (bReturn && UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) < 1.0f)
	{
		// ..Rise timescale back to 1
		float TimeDilat = UGameplayStatics::GetGlobalTimeDilation(this->GetWorld());
		float TimeT = FMath::FInterpConstantTo(TimeDilat, 1.0f, Delta, TimescaleRecoverySpeed * TimeDilat);
		SetTimeScale(TimeT);
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
	if (LocalPlayer)
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
	if (OpponentPlayer)
	{
		return OpponentPlayer->GetChargePercentage();
	}
	else
	{
		//GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::White, TEXT("match sees no opponent player"));
		return -1.0f;
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
	DOREPLIFETIME(AGMatch, GGTimer);
}
