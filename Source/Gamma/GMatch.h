// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

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

	// Charge & Health reporters for HUD display
	UFUNCTION(BlueprintCallable)
	float GetLocalChargePercent();

	UFUNCTION(BlueprintCallable)
	float GetOpponentChargePercent();

	UFUNCTION(BlueprintCallable)
	float GetLocalHealth();

	UFUNCTION(BlueprintCallable)
	float GetOpponentHealth();

	UFUNCTION(BlueprintCallable)
	FText GetLocalCharacterName();

	UFUNCTION(BlueprintCallable)
	FText GetOpponentCharacterName();

	UFUNCTION(BlueprintCallable)
	AGammaCharacter* GetLocalPlayer() { return LocalPlayer; }

	UFUNCTION(BlueprintCallable)
	AGammaCharacter* GetOpponentPlayer() { return OpponentPlayer; }

	UFUNCTION(BlueprintCallable)
	void ClearPlayers() { LocalPlayer = OpponentPlayer = nullptr; }

	UFUNCTION(BlueprintCallable)
	float GetNaturalTimeScale() { return NaturalTimeScale; }

	// End-game slow-time functions
	void ClaimHit(AActor* HitActor, AActor* Winner);

	void HandleTimeScale(float Delta);

	void SetTimeScale(float Time);

	bool PlayersAccountedFor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float NaturalTimeScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float GGTimescale = 0.01f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float GGDelayTime = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float HitFreezeTime = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TimescaleDropSpeed = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TimescaleRecoverySpeed = 11.0f;

	// Network-replicated variables
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadWrite)
	bool bGG = false;

	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadWrite)
	bool bMinorGG = false;

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
