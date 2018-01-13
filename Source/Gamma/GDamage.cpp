// Fill out your copyright notice in the Description page of Project Settings.

#include "GDamage.h"
#include "ParticleDefinitions.h"
#include "Particles/ParticleSystem.h"


// Sets default values
AGDamage::AGDamage()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	DamageScene = CreateDefaultSubobject<USceneComponent>(TEXT("DamageScene"));
	SetRootComponent(DamageScene);

	DamageParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("DamageParticles"));
	DamageParticles->SetupAttachment(RootComponent);

	bReplicates = true;
}

// Called when the game starts or when spawned
void AGDamage::BeginPlay()
{
	Super::BeginPlay();
	DamageParticles->RegisterComponent();
	SetLifeSpan(Lifetime);
}

// Called every frame
void AGDamage::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

