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
	
}

// Called every frame
void AGMatch::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!LocalPlayer || !OpponentPlayer)
	{
		if (GetWorld()->GetTimeSeconds() > 1.0f)
		{
			GetPlayers();
		}
	}
	
	HandleTimeScale(bGG, DeltaTime);
}


void AGMatch::ClaimGG(AActor* Winner)
{
	bGG = true;

	AGammaCharacter* Reciever = Cast<AGammaCharacter>(Winner);
	if (Reciever)
	{
		Reciever->RaiseScore(1);
	}
}


void AGMatch::HandleTimeScale(bool Gg, float Delta)
{
	// Handle gameover scenario - timing and score handouts
	if (Gg)
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
			bGG = false;
		}
	}
	else if (UGameplayStatics::GetGlobalTimeDilation(this->GetWorld()) < 1.0f)
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
		return LocalPlayer->GetChargePercentage();
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::White, TEXT("match sees no local player"));
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
		GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::White, TEXT("match sees no opponent player"));
		return -1.0f;
	}
}


void AGMatch::GetPlayers()
{
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), TempPlayers);
	if (GetWorld() && TempPlayers.Num() > 1)
	{
		// Loop through to deliberate local and opponent
		for (int i = 0; i < TempPlayers.Num(); ++i)
		{

			ACharacter* TempChar = Cast<ACharacter>(TempPlayers[i]);
			if (TempChar && TempChar->IsValidLowLevel() && TempChar->GetController())
			{
				APlayerController* TempCont = Cast<APlayerController>(TempChar->GetController());
				// Check if controller is local
				if (TempCont && TempCont->NetPlayerIndex == 0)
				{
					LocalPlayer = Cast<AGammaCharacter>(TempChar);
					//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, FString::Printf(TEXT("GotLocalPlayer")));
				}
				else /// or opponent
				{
					OpponentPlayer = Cast<AGammaCharacter>(TempChar);
					//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, FString::Printf(TEXT("GotOpponentPlayer")));
				}
			}
			else /// ...or client's opponent
			{
				OpponentPlayer = Cast<AGammaCharacter>(TempChar);
				//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, FString::Printf(TEXT("GotOpponentPlayer")));
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
	DOREPLIFETIME(AGMatch, GGTimer);
}
