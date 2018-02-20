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

	UFUNCTION(BlueprintCallable)
	AGammaCharacter* GetOpponentPlayer() { return OpponentPlayer; }

	UFUNCTION(BlueprintCallable)
	void ClearPlayers() { LocalPlayer = OpponentPlayer = nullptr; }

	UFUNCTION(BlueprintCallable)
	float GetNaturalTimeScale() { return NaturalTimeScale; }

	// End-game slow-time functions
	void ClaimHit(AActor* HitActor, AActor* Winner);

	void HandleTimeScale(bool Gg, float Delta);

	void SetTimeScale(float Time);

	bool PlayersAccountedFor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NaturalTimeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GGTimescale = 0.001f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GGDelayTime = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HitFreezeTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimescaleDropSpeed = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimescaleRecoverySpeed = 0.33f;

	// Network-replicated variables
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadWrite)
	bool bGG = false;

	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly)
	float GGTimer = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float GGDelayTimer = 0.0f;

	UPROPERTY(BlueprintReadWrite)
	bool bReturn = false;


private:
	int LocalScore = 0;
	int OpponentScore = 0;

	//float GGTimer = 0.0f;

	bool bPlayersIn = false;
	void GetPlayers();
	TArray<AActor*> TempPlayers;
	AGammaCharacter* LocalPlayer = nullptr;
	AGammaCharacter* OpponentPlayer = nullptr;
	AActor* Winner = nullptr;
	AActor* DoomedBoye = nullptr;
	
};
