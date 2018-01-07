// Fill out your copyright notice in the Description page of Project Settings.

#include "GFlash.h"
#include "ParticleDefinitions.h"
#include "Particles/ParticleSystem.h"


// Sets default values
AGFlash::AGFlash()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	FlashScene = CreateDefaultSubobject<USceneComponent>(TEXT("FlashScene"));
	SetRootComponent(FlashScene);

	FlashSound = CreateDefaultSubobject<UAudioComponent>(TEXT("FlashSound"));

	FlashParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("FlashParticles"));
	FlashParticles->SetupAttachment(RootComponent);

	bReplicates = true;
	bReplicateMovement = true;
}

// Called when the game starts or when spawned
void AGFlash::BeginPlay()
{
	Super::BeginPlay();
	FlashParticles->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	float InitialPitch = FlashSound->PitchMultiplier;
	FlashSound->SetPitchMultiplier(FMath::FRandRange(InitialPitch, InitialPitch + 1.f));
	FlashSound->SetVolumeMultiplier(0.125f);
}

// Called every frame
void AGFlash::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateFlash(DeltaTime);
}



void AGFlash::UpdateFlash(float DeltaTime)
{
	float GrowthAlgo = (1 + FMath::Sqrt(FlashGrowthIntensity * DeltaTime) / 2.0f);
	
	FVector FScale = FlashParticles->GetComponentScale() * GrowthAlgo;
	if (FScale.Size() < FlashMaxScale)
	{
		FlashParticles->SetRelativeScale3D(FScale);
	}
	
	float FPitch = FlashSound->PitchMultiplier * GrowthAlgo;
	if (FPitch < FlashMaxPitch)
	{
		FlashSound->SetPitchMultiplier(FlashSound->PitchMultiplier * GrowthAlgo);
	}
	
	float FVol = FlashSound->VolumeMultiplier * GrowthAlgo;
	if (FVol < FlashMaxVolume)
	{
		FlashSound->SetVolumeMultiplier(FlashSound->VolumeMultiplier * GrowthAlgo);
	}
}

