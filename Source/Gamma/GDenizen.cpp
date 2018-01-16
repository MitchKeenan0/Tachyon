// Fill out your copyright notice in the Description page of Project Settings.

#include "GDenizen.h"


// Sets default values
AGDenizen::AGDenizen()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AGDenizen::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AGDenizen::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ActorsOfInterest.Num() == 0)
	{
		FindActorsOfInterest();
	}
	else
	{
		AActor* FirstBoye = ActorsOfInterest[0];
		ApproachActorOfInterest(FirstBoye);
	}
}

// Find interesting actors
void AGDenizen::FindActorsOfInterest()
{
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("FramingActor"), TheCrowd);
	if (TheCrowd.Num() >= 1)
	{
		// Loop to do background checks on each actor
		for (int i = 0; i < TheCrowd.Num(); ++i)
		{
			AActor* CurrentActor = TheCrowd[i];
			if (CurrentActor != this)
			{
				// If everything looks good add it to the array
				ActorsOfInterest.Push(CurrentActor);
			}
		}
	}
}

// Approach interesting actor
void AGDenizen::ApproachActorOfInterest(AActor* InterestActor)
{
	FVector ToActor = InterestActor->GetActorLocation() - GetActorLocation();
	FVector MoveVector = ToActor * MoveSpeed * GetWorld()->DeltaTimeSeconds;
	SetActorLocation(GetActorLocation() + MoveVector);
}