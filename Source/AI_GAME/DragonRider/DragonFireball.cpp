// Dragon Rider - Bola de fuego del Dragon Volador

#include "DragonFireball.h"
#include "DragonCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ADragonFireball::ADragonFireball()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 5.f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(28.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	SetRootComponent(Collision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetRelativeScale3D(FVector(0.55f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (SphereMesh.Succeeded()) Mesh->SetStaticMesh(SphereMesh.Object);
	if (Mat.Succeeded()) Mesh->SetMaterial(0, Mat.Object);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 0.f;
	Movement->MaxSpeed = 4000.f;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bAutoActivate = true;
}

void ADragonFireball::BeginPlay()
{
	Super::BeginPlay();
	Collision->OnComponentBeginOverlap.AddDynamic(this, &ADragonFireball::OnOverlap);
	Movement->OnProjectileStop.AddDynamic(this, &ADragonFireball::OnStop);
	if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.45f, 0.f));
	}
}

void ADragonFireball::Launch(const FVector& Direction, float Speed)
{
	Movement->Velocity = Direction.GetSafeNormal() * Speed;
}

void ADragonFireball::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == GetOwner() || OtherActor->IsA<ADragonCharacter>()) return;
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		APawn* InstigatorPawn = GetInstigator();
		UGameplayStatics::ApplyDamage(Pawn, Damage, InstigatorPawn ? InstigatorPawn->GetController() : nullptr, this, UDamageType::StaticClass());
		Destroy();
	}
}

void ADragonFireball::OnStop(const FHitResult& ImpactResult)
{
	Destroy();
}
