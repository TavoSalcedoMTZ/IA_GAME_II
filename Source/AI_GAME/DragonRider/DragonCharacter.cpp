// Dragon Rider - Personaje base de los dragones

#include "DragonCharacter.h"
#include "DragonNest.h"
#include "DragonAIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Perception/AISense_Damage.h"
#include "StateTree.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

namespace DragonMeshes
{
	static const TCHAR* Sphere   = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	static const TCHAR* Cube     = TEXT("/Engine/BasicShapes/Cube.Cube");
	static const TCHAR* Cone     = TEXT("/Engine/BasicShapes/Cone.Cone");
	static const TCHAR* Cylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	static const TCHAR* Material = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
}

ADragonCharacter::ADragonCharacter()
{
	// El Tick del actor SOLO se usa para orientar el texto hacia la camara
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	AIControllerClass = ADragonAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->MaxWalkSpeed = BaseSpeed;
	GetCharacterMovement()->AirControl = 0.4f;

	BodyRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BodyRoot"));
	BodyRoot->SetupAttachment(GetCapsuleComponent());

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusText->SetupAttachment(GetCapsuleComponent());
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextBottom);
	StatusText->SetWorldSize(26.f);
	StatusText->SetTextRenderColor(FColor::White);
	StatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

UStaticMeshComponent* ADragonCharacter::AddShape(FName Name, const TCHAR* MeshPath, const FVector& RelLoc, const FVector& Scale, const FRotator& Rot, bool bAccent)
{
	UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(MeshPath);
	if (MeshFinder.Succeeded())
	{
		Comp->SetStaticMesh(MeshFinder.Object);
	}
	ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(DragonMeshes::Material);
	if (MatFinder.Succeeded())
	{
		Comp->SetMaterial(0, MatFinder.Object);
	}
	Comp->SetupAttachment(BodyRoot);
	Comp->SetRelativeLocation(RelLoc);
	Comp->SetRelativeScale3D(Scale);
	Comp->SetRelativeRotation(Rot);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetCanEverAffectNavigation(false);
	Comp->CastShadow = true;
	(bAccent ? AccentParts : BodyParts).Add(Comp);
	return Comp;
}

void ADragonCharacter::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	Energy = MaxEnergy;
	GetCharacterMovement()->MaxWalkSpeed = BaseSpeed;
	GetCharacterMovement()->MaxFlySpeed = BaseSpeed;

	if (bCanFly)
	{
		GetCharacterMovement()->DefaultLandMovementMode = MOVE_Flying;
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		GetCharacterMovement()->BrakingDecelerationFlying = 1200.f;
	}

	StatusText->SetRelativeLocation(FVector(0.f, 0.f, TextHeight));
	ApplyColors();
	RefreshText();

	// Actualizacion lenta
	GetWorldTimerManager().SetTimer(SlowTimer, this, &ADragonCharacter::SlowUpdate, 0.25f, true, FMath::FRandRange(0.f, 0.25f));
}

void ADragonCharacter::ApplyColors()
{
	auto Paint = [this](UStaticMeshComponent* C, const FLinearColor& Color)
	{
		if (!C) return;
		if (UMaterialInstanceDynamic* MID = C->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			BodyMIDs.Add(MID);
		}
	};
	BodyMIDs.Reset();
	for (UStaticMeshComponent* C : BodyParts)   Paint(C, BodyColor);
	for (UStaticMeshComponent* C : AccentParts) Paint(C, AccentColor);
}

void ADragonCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// --- SOLO VISUAL
	if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCam = Cam->GetCameraLocation() - StatusText->GetComponentLocation();
		StatusText->SetWorldRotation(FRotator(0.f, ToCam.Rotation().Yaw, 0.f));
	}

	// --- SOLO VISUAL
	const float Now = GetWorld()->GetTimeSeconds();
	if (PulseEndTime > Now && PulseDuration > 0.f)
	{
		const float Alpha = 1.f - (PulseEndTime - Now) / PulseDuration;
		const float S = FMath::Lerp(1.f, PulseScale, FMath::Sin(Alpha * PI));
		BodyRoot->SetRelativeScale3D(FVector(S));
	}
	else if (!BodyRoot->GetRelativeScale3D().Equals(FVector::OneVector))
	{
		BodyRoot->SetRelativeScale3D(FVector::OneVector);
	}
}

void ADragonCharacter::SlowUpdate()
{
	if (bIsDead) return;
	const float Dt = 0.25f;
	if (EnergyRegenPerSecond > 0.f)
	{
		Energy = FMath::Min(MaxEnergy, Energy + EnergyRegenPerSecond * Dt);
	}
	Fury = FMath::Max(0.f, Fury - FuryDecayPerSecond * Dt);
	StunAccum = FMath::Max(0.f, StunAccum - 6.f * Dt);
	RefreshText();
}

void ADragonCharacter::SetHomeNest(ADragonNest* Nest)
{
	HomeNest = Nest;
	PatrolPoints.Reset();
	if (!Nest) return;

	const FVector Center = Nest->GetActorLocation();
	const int32 NumPoints = 5;
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const float StartAngle = FMath::FRandRange(0.f, 360.f);
	for (int32 i = 0; i < NumPoints; ++i)
	{
		const float Ang = FMath::DegreesToRadians(StartAngle + i * 360.f / NumPoints);
		FVector P = Center + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * PatrolRadius;
		if (!bCanFly && Nav)
		{
			FNavLocation NavLoc;
			if (Nav->ProjectPointToNavigation(P, NavLoc, FVector(400.f, 400.f, 1000.f)))
			{
				P = NavLoc.Location;
			}
		}
		PatrolPoints.Add(P);
	}
}

FVector ADragonCharacter::GetNestLocation() const
{
	return HomeNest ? HomeNest->GetNestTopLocation() : GetActorLocation();
}

FVector ADragonCharacter::GetNextPatrolPoint()
{
	if (PatrolPoints.Num() == 0)
	{
		return GetNestLocation() + FMath::VRand().GetSafeNormal2D() * PatrolRadius;
	}
	PatrolIndex = (PatrolIndex + 1) % PatrolPoints.Num();
	return PatrolPoints[PatrolIndex];
}

bool ADragonCharacter::GetRandomPerchPoint(FVector& OutPoint) const
{
	if (HomeNest && HomeNest->PerchPoints.Num() > 0)
	{
		OutPoint = HomeNest->PerchPoints[FMath::RandRange(0, HomeNest->PerchPoints.Num() - 1)];
		return true;
	}
	return false;
}

bool ADragonCharacter::IsStunned() const
{
	return GetWorld() && StunEndTime > GetWorld()->GetTimeSeconds();
}

bool ADragonCharacter::ConsumeEnergy(float Amount)
{
	if (Energy + KINDA_SMALL_NUMBER < Amount) return false;
	Energy = FMath::Max(0.f, Energy - Amount);
	RefreshText();
	return true;
}

void ADragonCharacter::AddEnergy(float Amount)
{
	Energy = FMath::Clamp(Energy + Amount, 0.f, MaxEnergy);
	RefreshText();
}

void ADragonCharacter::AddFury(float Amount)
{
	Fury = FMath::Clamp(Fury + Amount, 0.f, 1.f);
}

void ADragonCharacter::NotifyEggStolen(AActor* Thief)
{
	if (bIsDead) return;
	Blackboard.bEggStolen = true;
	Blackboard.EggSearchCount = 0;
	Blackboard.EggThiefLocation = Thief ? Thief->GetActorLocation() : GetNestLocation();
	AddFury(0.6f);
	SetDebugDetail(TEXT("¡HUEVO ROBADO!"));
}

void ADragonCharacter::SetStatus(const FString& InStatus, EDragonMood Mood)
{
	StatusString = InStatus;
	CurrentMood = Mood;
	RefreshText();
}

void ADragonCharacter::SetDebugDetail(const FString& InDetail)
{
	DetailString = InDetail;
	RefreshText();
}

void ADragonCharacter::PlayPulse(float Scale, float Duration)
{
	PulseScale = Scale;
	PulseDuration = FMath::Max(0.05f, Duration);
	PulseEndTime = GetWorld()->GetTimeSeconds() + PulseDuration;
}

void ADragonCharacter::RefreshText()
{
	if (!StatusText) return;

	FColor Color = FColor::White;
	switch (CurrentMood)
	{
	case EDragonMood::Alert:   Color = FColor::Yellow; break;
	case EDragonMood::Combat:  Color = FColor(255, 60, 60); break;
	case EDragonMood::Stunned: Color = FColor(80, 160, 255); break;
	case EDragonMood::Special: Color = FColor(255, 150, 0); break;
	default: break;
	}
	StatusText->SetTextRenderColor(Color);

	const FString EnergyLabel = bCanFly ? TEXT("Fuego") : TEXT("Estamina");
	const FString EnergyValue = bCanFly ? FString::Printf(TEXT("%d/%d"), FMath::FloorToInt(Energy), FMath::FloorToInt(MaxEnergy))
	                                    : FString::Printf(TEXT("%d%%"), FMath::RoundToInt(100.f * Energy / FMath::Max(1.f, MaxEnergy)));

	const FString Text = FString::Printf(TEXT("[%s]\n%s\nHP %d%%  %s %s  Furia %.2f%s%s"),
		DragonRider::SpeciesName(Species),
		*StatusString,
		FMath::RoundToInt(GetHealthPct() * 100.f),
		*EnergyLabel, *EnergyValue,
		Fury,
		DetailString.IsEmpty() ? TEXT("") : TEXT("\n"),
		*DetailString);

	StatusText->SetText(FText::FromString(Text));
}

FVector ADragonCharacter::GetMouthLocation() const
{
	return GetActorLocation() + GetActorForwardVector() * (GetCapsuleComponent()->GetScaledCapsuleRadius() + 40.f) + FVector(0.f, 0.f, 30.f);
}

void ADragonCharacter::SetAttackTell(bool bOn)
{
	if (bAttackTell == bOn) return;
	bAttackTell = bOn;
	const int32 NumBody = BodyParts.Num();
	for (int32 i = 0; i < BodyMIDs.Num(); ++i)
	{
		if (!BodyMIDs[i]) continue;
		const FLinearColor Base = i < NumBody ? BodyColor : AccentColor;
		const FLinearColor Tell = i < NumBody ? FLinearColor(1.f, 0.95f, 0.85f) : FLinearColor(1.f, 0.35f, 0.f);
		BodyMIDs[i]->SetVectorParameterValue(TEXT("Color"), bOn ? Tell : Base);
	}
}

bool ADragonCharacter::IsInMeleeReach(const AActor* Target, float Reach) const
{
	if (!Target) return false;
	float TargetRadius = 40.f, TargetHalfHeight = 90.f;
	if (const ACharacter* C = Cast<ACharacter>(Target))
	{
		TargetRadius = C->GetCapsuleComponent()->GetScaledCapsuleRadius();
		TargetHalfHeight = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	const UCapsuleComponent* Mine = GetCapsuleComponent();
	const FVector Delta = Target->GetActorLocation() - GetActorLocation();
	const float Gap2D = Delta.Size2D() - Mine->GetScaledCapsuleRadius() - TargetRadius;
	const float MaxDZ = Mine->GetScaledCapsuleHalfHeight() + TargetHalfHeight + Reach * 0.5f;
	return Gap2D <= Reach && FMath::Abs(Delta.Z) <= MaxDZ;
}

void ADragonCharacter::DealDamageTo(AActor* Victim, float Amount, const FVector& Knockback)
{
	if (!Victim || Victim->IsA<ADragonCharacter>()) return;
	UGameplayStatics::ApplyDamage(Victim, Amount, GetController(), this, UDamageType::StaticClass());
	if (ACharacter* Char = Cast<ACharacter>(Victim))
	{
		if (!Knockback.IsNearlyZero())
		{
			Char->LaunchCharacter(Knockback, true, true);
		}
	}
}

float ADragonCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead) return 0.f;

	// Los dragones no se hacen dano entre si
	if (DamageCauser && DamageCauser->IsA<ADragonCharacter>()) return 0.f;
	if (EventInstigator && EventInstigator->GetPawn() && EventInstigator->GetPawn()->IsA<ADragonCharacter>()) return 0.f;

	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	const float Dmg = Applied > 0.f ? Applied : DamageAmount;

	Health = FMath::Max(0.f, Health - Dmg);
	LastDamageTime = GetWorld()->GetTimeSeconds();
	AddFury(0.08f);

	// Avisamos al sistema de percepcion
	APawn* InstigatorPawn = EventInstigator ? EventInstigator->GetPawn() : nullptr;
	if (InstigatorPawn)
	{
		UAISense_Damage::ReportDamageEvent(this, this, InstigatorPawn, Dmg, InstigatorPawn->GetActorLocation(), GetActorLocation());
	}

	// Reaccion a dano
	StunAccum += Dmg;
	if (StunAccum >= StunThreshold && !IsStunned())
	{
		StunAccum = 0.f;
		StunEndTime = GetWorld()->GetTimeSeconds() + StunDuration;
		PlayPulse(0.75f, 0.4f);
	}

	if (Health <= 0.f)
	{
		Die();
	}
	RefreshText();
	return Dmg;
}

void ADragonCharacter::Die()
{
	if (bIsDead) return;
	bIsDead = true;
	SetStatus(TEXT("MUERTO"), EDragonMood::Stunned);
	SetDebugDetail(TEXT(""));

	if (ADragonAIController* AIC = Cast<ADragonAIController>(GetController()))
	{
		AIC->OnDragonDied();
	}
	if (HomeNest)
	{
		HomeNest->RemoveGuardian(this);
	}

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyRoot->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
	GetWorldTimerManager().ClearTimer(SlowTimer);
	SetLifeSpan(5.f);
}

// ARQUETIPOS

ADragonVolador::ADragonVolador()
{
	Species = EDragonSpecies::Volador;
	StateTreeAsset = TSoftObjectPtr<UStateTree>(FSoftObjectPath(TEXT("/Game/DragonRider/AI/ST_DragonVolador.ST_DragonVolador")));
	MaxHealth = 80.f;
	MaxEnergy = 4.f; // 4 bolas de fuego
	EnergyRegenPerSecond = 0.f; // SOLO recarga en el nido
	BaseSpeed = 700.f;
	bCanFly = true;
	LeashRadius = 5200.f;
	PatrolRadius = 2300.f;
	StunThreshold = 30.f;
	BodyColor = DragonRider::SpeciesColor(EDragonSpecies::Volador);
	AccentColor = FLinearColor(1.f, 0.55f, 0.05f);
	TextHeight = 150.f;
	GetCapsuleComponent()->InitCapsuleSize(55.f, 60.f);
	BuildBody();
}

void ADragonVolador::BuildBody()
{
	AddShape(TEXT("Body"),  DragonMeshes::Sphere, FVector(0, 0, 0),     FVector(1.3f, 0.8f, 0.7f));
	AddShape(TEXT("Neck"),  DragonMeshes::Cylinder, FVector(60, 0, 30), FVector(0.3f, 0.3f, 0.6f), FRotator(-50, 0, 0));
	AddShape(TEXT("Head"),  DragonMeshes::Sphere, FVector(95, 0, 55),   FVector(0.5f, 0.4f, 0.4f));
	AddShape(TEXT("Snout"), DragonMeshes::Cone,   FVector(130, 0, 52),  FVector(0.25f, 0.25f, 0.4f), FRotator(-90, 0, 0), true);
	AddShape(TEXT("Tail"),  DragonMeshes::Cone,   FVector(-110, 0, 0),  FVector(0.35f, 0.35f, 1.1f), FRotator(90, 0, 0));
	AddShape(TEXT("WingL"), DragonMeshes::Cube,   FVector(0, -110, 20), FVector(0.9f, 1.6f, 0.05f), FRotator(0, 0, -15), true);
	AddShape(TEXT("WingR"), DragonMeshes::Cube,   FVector(0, 110, 20),  FVector(0.9f, 1.6f, 0.05f), FRotator(0, 0, 15), true);
}

ADragonSaltarin::ADragonSaltarin()
{
	Species = EDragonSpecies::Saltarin;
	StateTreeAsset = TSoftObjectPtr<UStateTree>(FSoftObjectPath(TEXT("/Game/DragonRider/AI/ST_DragonSaltarin.ST_DragonSaltarin")));
	MaxHealth = 70.f;
	MaxEnergy = 100.f;
	EnergyRegenPerSecond = 14.f;
	BaseSpeed = 520.f;
	LeashRadius = 4200.f;
	PatrolRadius = 2000.f;
	StunThreshold = 25.f;
	BodyColor = DragonRider::SpeciesColor(EDragonSpecies::Saltarin);
	AccentColor = FLinearColor(0.9f, 0.95f, 0.2f);
	TextHeight = 140.f;
	GetCapsuleComponent()->InitCapsuleSize(40.f, 55.f);
	GetCharacterMovement()->JumpZVelocity = 900.f;
	GetCharacterMovement()->AirControl = 0.8f;
	BuildBody();
}

void ADragonSaltarin::BuildBody()
{
	AddShape(TEXT("Body"),  DragonMeshes::Sphere, FVector(0, 0, -5),     FVector(0.9f, 0.6f, 0.6f));
	AddShape(TEXT("Head"),  DragonMeshes::Sphere, FVector(55, 0, 30),    FVector(0.45f, 0.38f, 0.38f));
	AddShape(TEXT("Snout"), DragonMeshes::Cone,   FVector(85, 0, 28),    FVector(0.2f, 0.2f, 0.3f), FRotator(-90, 0, 0), true);
	AddShape(TEXT("LegL"),  DragonMeshes::Cylinder, FVector(-25, -30, -40), FVector(0.18f, 0.18f, 0.35f), FRotator(0, 0, 0), true);
	AddShape(TEXT("LegR"),  DragonMeshes::Cylinder, FVector(-25, 30, -40),  FVector(0.18f, 0.18f, 0.35f), FRotator(0, 0, 0), true);
	AddShape(TEXT("Tail"),  DragonMeshes::Cone,   FVector(-75, 0, 5),    FVector(0.25f, 0.25f, 0.9f), FRotator(80, 0, 0));
	AddShape(TEXT("GlideL"), DragonMeshes::Cube,  FVector(-5, -45, 5),   FVector(0.6f, 0.4f, 0.04f), FRotator(0, 0, -25), true);
	AddShape(TEXT("GlideR"), DragonMeshes::Cube,  FVector(-5, 45, 5),    FVector(0.6f, 0.4f, 0.04f), FRotator(0, 0, 25), true);
}

ADragonImpulsivo::ADragonImpulsivo()
{
	Species = EDragonSpecies::Impulsivo;
	StateTreeAsset = TSoftObjectPtr<UStateTree>(FSoftObjectPath(TEXT("/Game/DragonRider/AI/ST_DragonImpulsivo.ST_DragonImpulsivo")));
	MaxHealth = 180.f;
	MaxEnergy = 100.f;
	EnergyRegenPerSecond = 10.f;
	BaseSpeed = 380.f;
	LeashRadius = 3800.f;
	PatrolRadius = 1800.f;
	StunThreshold = 60.f;
	StunDuration = 2.0f;
	FuryDecayPerSecond = 0.03f;
	BodyColor = DragonRider::SpeciesColor(EDragonSpecies::Impulsivo);
	AccentColor = FLinearColor(0.95f, 0.95f, 0.85f);
	TextHeight = 200.f;
	GetCapsuleComponent()->InitCapsuleSize(70.f, 90.f);
	GetCharacterMovement()->JumpZVelocity = 700.f;
	BuildBody();
}

void ADragonImpulsivo::BuildBody()
{
	AddShape(TEXT("Body"),  DragonMeshes::Sphere, FVector(0, 0, -10),     FVector(1.6f, 1.2f, 1.1f));
	AddShape(TEXT("Head"),  DragonMeshes::Sphere, FVector(95, 0, 30),     FVector(0.75f, 0.65f, 0.6f));
	AddShape(TEXT("HornL"), DragonMeshes::Cone,   FVector(105, -25, 75),  FVector(0.15f, 0.15f, 0.4f), FRotator(0, 0, -20), true);
	AddShape(TEXT("HornR"), DragonMeshes::Cone,   FVector(105, 25, 75),   FVector(0.15f, 0.15f, 0.4f), FRotator(0, 0, 20), true);
	AddShape(TEXT("LegFL"), DragonMeshes::Cylinder, FVector(45, -45, -70),  FVector(0.3f, 0.3f, 0.4f));
	AddShape(TEXT("LegFR"), DragonMeshes::Cylinder, FVector(45, 45, -70),   FVector(0.3f, 0.3f, 0.4f));
	AddShape(TEXT("LegBL"), DragonMeshes::Cylinder, FVector(-45, -45, -70), FVector(0.3f, 0.3f, 0.4f));
	AddShape(TEXT("LegBR"), DragonMeshes::Cylinder, FVector(-45, 45, -70),  FVector(0.3f, 0.3f, 0.4f));
	AddShape(TEXT("Tail"),  DragonMeshes::Cone,   FVector(-130, 0, -10),  FVector(0.45f, 0.45f, 1.0f), FRotator(90, 0, 0));
	AddShape(TEXT("Spikes"), DragonMeshes::Cube,  FVector(0, 0, 50),      FVector(1.2f, 0.1f, 0.35f), FRotator::ZeroRotator, true);
}
