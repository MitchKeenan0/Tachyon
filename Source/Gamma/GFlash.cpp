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

	// Init scale for rapid growth
	FlashParticles->SetRelativeScale3D(FVector(InitialScale, InitialScale, InitialScale));
	
	// Init sound
	float InitialPitch = FlashSound->PitchMultiplier;
	FlashSound->SetPitchMultiplier(FMath::FRandRange(InitialPitch * 0.5f, InitialPitch * 0.75f));
	FlashSound->SetVolumeMultiplier(0.125f);
}

// Called every frame
void AGFlash::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CustomTimeDilation >= 0.9f)
	{
		UpdateFlash(DeltaTime);
	}
}



void AGFlash::UpdateFlash(float DeltaTime)
{
	// Scaling size and sound
	FVector MaxV = FVector(FlashMaxScale, FlashMaxScale, FlashMaxScale);
	
	if (FlashParticles != nullptr)
	{
		FVector FScale = FMath::VInterpConstantTo(FlashParticles->GetComponentScale(), MaxV, DeltaTime, FlashGrowthIntensity);
		FScale = FScale.GetClampedToMaxSize(5.0f);
		FlashParticles->SetRelativeScale3D(FScale);
	}
	
	if (FlashSound != nullptr)
	{
		float FPitch = FMath::FInterpConstantTo(FlashSound->PitchMultiplier, FlashMaxPitch, DeltaTime, FlashGrowthIntensity);
		float FVolum = FMath::FInterpConstantTo(FlashSound->VolumeMultiplier, FlashMaxVolume, DeltaTime, FlashGrowthIntensity);
		FlashSound->SetPitchMultiplier(FPitch);
		FlashSound->SetVolumeMultiplier(FVolum);
	}
}


