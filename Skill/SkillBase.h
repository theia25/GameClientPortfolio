#pragma once

#include "GameFramework/Actor.h"
#include "../../Common/GameDatas.h"
#include "../BaseCharacter.h"
#include "../../Table/TableData.h"
#include "NiagaraComponent.h"
#include "SKillBase.generated.h"

// 스킬 이펙트 종료 시 호출할 함수 바인딩
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSkillEffectEndedDelegate, const FName&, _SkillName);

UCLASS(Blueprintable, BlueprintType)
class PROJECTD_API ASKillBase : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FSkillEffectEndedDelegate mSkillEffectEnded;

#pragma region Variable
protected:
	// 스킬 속성(이름, 타입, 이펙트 등) 정보 저장
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite)
	FSkillData mSkillData;

#pragma endregion Variable

#pragma region CustomFuction
public:
	// 스킬 기본 동작 구현
	UFUNCTION(Server, Reliable, BlueprintCallable)
	virtual void UseSkill(const FName& _SkillName);
	virtual void UseSkill_Implementation(const FName& _SkillName);

	// 데이터 기반 스킬 초기화
	UFUNCTION(BlueprintCallable)
	void InitializeSkillData(const FSkillDataTableRow& _InRow);

	// 투시 스킬 전용 초기화 함수
	UFUNCTION(BlueprintCallable)
	virtual void InitXrayMaterial(UCameraComponent* _Camera);

#pragma region Niagara Effect
	// 좌, 우 이펙트 부착을 위해 선언한 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<class UNiagaraComponent> mNiagaraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<class UNiagaraComponent> mNiagaraComponent2;

	UPROPERTY(ReplicatedUsing = OnRep_NiagaraSkillEffect)
	TObjectPtr<class UNiagaraSystem> mNiagaraSkillEffect;

	// 스킬에 등록된 이펙트 On/Off 함수
	UFUNCTION()
	virtual void ActivateSkillEffect();

	UFUNCTION()
	virtual void DeactivateSkillEffect();

#pragma endregion Niagara Effect

public:
	// 사망한 생존자 Mesh정보 갱신
	UFUNCTION(Client, Reliable)
	virtual void Client_SetSurvivorMeshUpdate(AActor* _Survivor);
	virtual void Client_SetSurvivorMeshUpdate_Implementation(AActor* _Survivor);

#pragma endregion CustomFuction

};
