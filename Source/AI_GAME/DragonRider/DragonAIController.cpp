// Dragon Rider - AIController de los dragones

#include "DragonAIController.h"
#include "DragonCharacter.h"
#include "Components/StateTreeAIComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTree.h"

ADragonAIController::ADragonAIController()
{
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
	StateTreeAI->SetStartLogicAutomatically(false);

	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));

	// --- Vista ---
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 3500.f;
	SightConfig->LoseSightRadius = 4000.f;
	SightConfig->PeripheralVisionAngleDegrees = 70.f;
	SightConfig->SetMaxAge(4.f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	Perception->ConfigureSense(*SightConfig);

	// --- Oido (disparos del jugador hacen ruido) ---
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 3500.f;
	HearingConfig->SetMaxAge(6.f);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	Perception->ConfigureSense(*HearingConfig);

	// --- Dano ---
	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageConfig"));
	DamageConfig->SetMaxAge(5.f);
	Perception->ConfigureSense(*DamageConfig);

	Perception->SetDominantSense(SightConfig->GetSenseImplementation());
}

void ADragonAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ADragonCharacter* Dragon = Cast<ADragonCharacter>(InPawn);
	if (!Dragon) return;

	// Los voladores ven mas lejos
	if (Dragon->bCanFly)
	{
		SightConfig->SightRadius = 4500.f;
		SightConfig->LoseSightRadius = 5200.f;
		SightConfig->PeripheralVisionAngleDegrees = 85.f;
		Perception->ConfigureSense(*SightConfig);
	}

	if (UStateTree* Tree = Dragon->StateTreeAsset.LoadSynchronous())
	{
		StateTreeAI->SetStateTree(Tree);
		StateTreeAI->StartLogic();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[DragonRider] %s no tiene StateTree (%s). Ejecuta el generador DragonSetup."),
			*Dragon->GetName(), *Dragon->StateTreeAsset.ToString());
		Dragon->SetStatus(TEXT("SIN STATETREE"), EDragonMood::Stunned);
	}
}

void ADragonAIController::OnDragonDied()
{
	if (UPathFollowingComponent* PF = GetPathFollowingComponent())
	{
		PF->AbortMove(*this, FPathFollowingResultFlags::UserAbort);
	}
	StateTreeAI->StopLogic(TEXT("Dead"));
	ClearFocus(EAIFocusPriority::Gameplay);
}
