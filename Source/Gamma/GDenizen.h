// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine.h"
#include "GameFramework/Actor.h"
#include "GDenizen.generated.h"

UCLASS()
class GAMMA_API AGDenizen : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AGDenizen();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	TArray<AActor*> TheCrowd;
	TArray<AActor*> ActorsOfInterest;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MoveSpeed = 1.0f;

	UFUNCTION()
	void FindActorsOfInterest();
	
	UFUNCTION()
	void ApproachActorOfInterest(AActor* InterestActor);
};
