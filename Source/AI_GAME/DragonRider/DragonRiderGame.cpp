// Dragon Rider - Jugador, proyectil, PlayerController y GameMode

#include "DragonRiderGame.h"
#include "DragonArena.h"
#include "DragonNest.h"
#include "DragonCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

// JUGADOR

ADragonRiderPlayer::ADragonRiderPlayer()
{
	// Acciones de Enhanced Input del template First Person
	static ConstructorHelpers::FObjectFinder<UInputAction> IA_Jump(TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));
	static ConstructorHelpers::FObjectFinder<UInputAction> IA_Move(TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
	static ConstructorHelpers::FObjectFinder<UInputAction> IA_Look(TEXT("/Game/Input/Actions/IA_Look.IA_Look"));
	static ConstructorHelpers::FObjectFinder<UInputAction> IA_MouseLook(TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook"));
	JumpAction = IA_Jump.Object;
	MoveAction = IA_Move.Object;
	LookAction = IA_Look.Object;
	MouseLookAction = IA_MouseLook.Object;

	// Camara a la altura de los ojos
	UCameraComponent* Cam = GetFirstPersonCameraComponent();
	Cam->SetupAttachment(GetCapsuleComponent());
	Cam->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 64.f), FRotator::ZeroRotator);
	Cam->bEnableFirstPersonScale = false;

	GetCharacterMovement()->MaxWalkSpeed = 750.f;
	GetCharacterMovement()->JumpZVelocity = 650.f;

	// Ballesta greybox
	Crossbow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crossbow"));
	Crossbow->SetupAttachment(Cam);
	Crossbow->SetRelativeLocation(FVector(45.f, 22.f, -20.f));
	Crossbow->SetRelativeScale3D(FVector(0.45f, 0.06f, 0.06f));
	Crossbow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Crossbow->CastShadow = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Cube.Succeeded()) Crossbow->SetStaticMesh(Cube.Object);
	if (Mat.Succeeded()) Crossbow->SetMaterial(0, Mat.Object);
}

void ADragonRiderPlayer::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	SpawnTransform = GetActorTransform();
	if (UMaterialInstanceDynamic* MID = Crossbow->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.3f, 0.18f, 0.08f));
	}
}

void ADragonRiderPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ADragonRiderPlayer::Fire);
	PlayerInputComponent->BindKey(EKeys::Gamepad_RightTrigger, IE_Pressed, this, &ADragonRiderPlayer::Fire);
}

void ADragonRiderPlayer::Fire()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastFireTime < FireCooldown) return;
	LastFireTime = Now;

	UCameraComponent* Cam = GetFirstPersonCameraComponent();
	const FVector Dir = Cam->GetForwardVector();
	const FVector Start = Cam->GetComponentLocation() + Dir * 80.f + Cam->GetRightVector() * 15.f - FVector(0, 0, 10.f);

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ADragonPlayerBolt* Bolt = GetWorld()->SpawnActor<ADragonPlayerBolt>(Start, Dir.Rotation(), Params))
	{
		Bolt->Damage = BoltDamage;
	}

	// El disparo hace ruido
	UAISense_Hearing::ReportNoiseEvent(this, GetActorLocation(), 1.f, this, 2500.f);
}

float ADragonRiderPlayer::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	Health = FMath::Max(0.f, Health - DamageAmount);

	if (Health <= 0.f)
	{
		Respawn();
	}
	return DamageAmount;
}

void ADragonRiderPlayer::Respawn()
{
	++Deaths;
	// Si llevaba un huevo, vuelve a su nido
	if (UDragonRiderSubsystem* Sub = GetWorld()->GetSubsystem<UDragonRiderSubsystem>())
	{
		for (ADragonEgg* Egg : Sub->GetEggsCarriedBy(this))
		{
			Egg->ReturnToNest();
		}
	}
	GetCharacterMovement()->StopMovementImmediately();
	TeleportTo(SpawnTransform.GetLocation(), SpawnTransform.Rotator());
	Health = MaxHealth;
}

// DARDO

ADragonPlayerBolt::ADragonPlayerBolt()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 3.f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(10.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetNotifyRigidBodyCollision(true);
	SetRootComponent(Collision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetRelativeScale3D(FVector(0.4f, 0.05f, 0.05f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Cube.Succeeded()) Mesh->SetStaticMesh(Cube.Object);
	if (Mat.Succeeded()) Mesh->SetMaterial(0, Mat.Object);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 6000.f;
	Movement->MaxSpeed = 6000.f;
	Movement->ProjectileGravityScale = 0.15f;
	Movement->bRotationFollowsVelocity = true;
}

void ADragonPlayerBolt::BeginPlay()
{
	Super::BeginPlay();
	if (GetInstigator()) Collision->IgnoreActorWhenMoving(GetInstigator(), true);
	Collision->OnComponentHit.AddDynamic(this, &ADragonPlayerBolt::OnHit);
	if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.2f, 0.9f, 1.f));
	}
}

void ADragonPlayerBolt::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor && OtherActor != GetInstigator())
	{
		APawn* InstigatorPawn = GetInstigator();
		UGameplayStatics::ApplyDamage(OtherActor, Damage, InstigatorPawn ? InstigatorPawn->GetController() : nullptr, this, UDamageType::StaticClass());
	}
	// Ruido del impacto
	UAISense_Hearing::ReportNoiseEvent(this, GetActorLocation(), 0.6f, GetInstigator(), 1500.f);
	Destroy();
}

// PLAYER CONTROLLER

ADragonRiderPlayerController::ADragonRiderPlayerController()
{
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC_Default(TEXT("/Game/Input/IMC_Default.IMC_Default"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC_Mouse(TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));
	if (IMC_Default.Succeeded()) MappingContexts.Add(IMC_Default.Object);
	if (IMC_Mouse.Succeeded()) MappingContexts.Add(IMC_Mouse.Object);
}

void ADragonRiderPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (UInputMappingContext* IMC : MappingContexts)
		{
			if (IMC) Sub->AddMappingContext(IMC, 0);
		}
	}
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;

	// Para pruebas automaticas
	int32 StartNest = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("DRStartNest="), StartNest))
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &ADragonRiderPlayerController::DRNido, StartNest), 1.5f, false);
	}
}

void ADragonRiderPlayerController::DRNido(int32 Index)
{
	APawn* P = GetPawn();
	if (!P) return;
	TArray<AActor*> Nests;
	UGameplayStatics::GetAllActorsOfClass(this, ADragonNest::StaticClass(), Nests);
	Nests.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });
	if (!Nests.IsValidIndex(Index)) return;

	const FVector NestLoc = Nests[Index]->GetActorLocation();
	const FVector Dir = (FVector::ZeroVector - NestLoc).GetSafeNormal2D();
	const FVector Dest = NestLoc + Dir * 2200.f + FVector(0, 0, 150.f);
	P->TeleportTo(Dest, (-Dir).Rotation());
	SetControlRotation((-Dir).Rotation());
}

// GAME MODE

ADragonRiderGameMode::ADragonRiderGameMode()
{
	DefaultPawnClass = ADragonRiderPlayer::StaticClass();
	PlayerControllerClass = ADragonRiderPlayerController::StaticClass();
	HUDClass = AHUD::StaticClass();
}
