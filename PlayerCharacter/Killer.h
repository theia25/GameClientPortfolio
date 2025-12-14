#pragma once

#include "EngineMinimal.h"
#include "BaseCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Interface/AttackInterface.h"
#include "Killer.generated.h"

UCLASS()
class PROJECTD_API AKiller : public ABaseCharacter, public IAttackInterface
{
	GENERATED_BODY()

#pragma region Variable
protected:
	// Killer용 Input
	UPROPERTY(VisibleAnywhere, Category = "Input Killer")
	TObjectPtr<class UKillerInputSystem> mInput;

	// Killer용 Animation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim Killer")
	TObjectPtr<class UKillerAnimInstance> mAnim;

	// Killer 무기 정보
	UPROPERTY(Replicated)
	TObjectPtr<UStaticMeshComponent> mWeapon;

	// 킬러 전용 상호작용 컴포넌트 (창틀넘기)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UKillerInteractionComponent> mKillerInteract;

	// 킬러 전용 ChaseComponent (오디오 트리거 시스템)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UChaseComponent> mChaseComponent;

#pragma endregion Variable

#pragma region CustomFuction
protected:
	//공격 
	UFUNCTION()
	void AttackAction(const FInputActionValue& _InputActionValue);

	//공격 스윙
	UFUNCTION()
	void AttackSwingAction(const FInputActionValue& _InputActionValue);

	//스킬 1번 광폭화
	UFUNCTION()
	void Skill1Action(const FInputActionValue& _InputActionValue);

	//스킬 2번 투시
	UFUNCTION()
	void Skill2Action(const FInputActionValue& _InputActionValue);

	// 창틀넘기
	UFUNCTION()
	void VaultAction(const FInputActionValue& _InputActionValue);

	// 킬러 우클릭 능력
	UFUNCTION()
	void AbilityAction(const FInputActionValue& _InputActionValue);

#pragma region Attack Interface Override
public:
	// Attack 몽타주 Notify에서 호출되는 함수 (공격 타이밍에 호출)
	UFUNCTION(BlueprintCallable)
	virtual void AttackHitCheck() override;

	// 공격이 몽타주 종료 시 공격 딜레이 타이머와 기본 속도로 되돌리기 
	UFUNCTION(BlueprintCallable)
	virtual void AttackEnd() override;

	UFUNCTION(BlueprintCallable)
	virtual void AttackStart() override;

	// AnimNotifyState 공격 탐지 용도
	UFUNCTION(BlueprintCallable)
	virtual bool AttackHitDetect() override;

#pragma endregion Attack Interface Override

#pragma region Replicate
protected:
	// 서버에게 공격 요청
	UFUNCTION(Server, Reliable)
	void Server_RequestAttack();
	void Server_RequestAttack_Implementation();

	// ## 공격 상태 관리 ##
	UFUNCTION()
	void OnRep_AttackStateChanged();

	// 클라이언트가 스윙을 요청하는 함수 (입력 키를 뗐을 때)
	UFUNCTION(Server, Reliable)
	void Server_RequestSwing();
	void Server_RequestSwing_Implementation();

	// 타이머가 만료되어 강제로 스윙하게 하는 함수 (서버에서만 호출됨)
	UFUNCTION()
	void ForceSwing();

#pragma endregion Replicate

#pragma endregion CustomFuction

#pragma region GetSet
public:
	// 캐릭터 정보 초기화
	virtual void SetDefaultStatus() override;

#pragma endregion GetSet
};
