// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine.h"
#include "GameFramework/Actor.h"
#include "GammaCharacter.h"
#include "GMatch.generated.h"


UCLASS()
class GAMMA_API AGMatch : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AGMatch();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Charge reporters for HUD display
	UFUNCTION(BlueprintCallable)
	float GetLocalChargePercent();

	UFUNCTION(BlueprintCallable)
	float GetOpponentChargePercent();

	// End-game slow-time functions
	void ClaimGG(AActor* Winner);
	UFUNCTION(Server, BlueprintCallable, reliable, WithValidation)
	void ServerClaimGG(AActor* Winner);

	void HandleTimeScale(bool Gg, float Delta);

	void SetTimeScale(float Time);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GGTimescale = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimescaleDropSpeed = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimescaleRecoverySpeed = 0.33f;

	// Network-replicated variables
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly)
	bool bGG = false;

	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly)
	float GGTimer = 0.0f;


private:
	int LocalScore = 0;
	int OpponentScore = 0;

	bool bPlayersIn = false;
	void GetPlayers();
	TArray<AActor*> TempPlayers;
	AGammaCharacter* LocalPlayer = nullptr;
	AGammaCharacter* OpponentPlayer = nullptr;
	
};
