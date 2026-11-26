// Dragon Rider - AIController de los dragones

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DragonAIController.generated.h"

class UStateTreeAIComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UAISenseConfig_Damage;

// El controlador NO decide nada
UCLASS()
class AI_GAME_API ADragonAIController : public AAIController
{
	GENERATED_BODY()

public:
	ADragonAIController();

	void OnDragonDied();

	UStateTreeAIComponent* GetStateTreeComponent() const { return StateTreeAI; }

protected:
	virtual void OnPossess(APawn* InPawn) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UStateTreeAIComponent> StateTreeAI;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UAIPerceptionComponent> Perception;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Damage> DamageConfig;
};
