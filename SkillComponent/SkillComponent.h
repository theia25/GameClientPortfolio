#pragma once

#include "../../Common/GameDatas.h"
#include "Components/ActorComponent.h"
#include "SkillComponent.generated.h"

// 스킬 사용시 UI 업데이트용 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillUse, int32, _SlotIndex, float, _SkillCoolDown);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuffSkillUse, const FSkillData&, _SkillData);


// 스킬 활성화 체크
USTRUCT(BlueprintType)
struct FSkillOnCheck
{
	GENERATED_BODY()

	FSkillOnCheck() {}
	~FSkillOnCheck() {}

	UPROPERTY(BlueprintType)
	bool bSkillOn = true;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTD_API USkillComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region Variable
protected:
	// 캐릭터 직업 정보
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPlayerJob mPlayerJob;

	// 스킬 데이터 정보
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, FSkillData> mSkillDataList;

	// 스킬 객체 리스트
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, TObjectPtr<class ASKillBase>> mSkillList;

	// Stat에 영향을 주는 스킬 효과 저장, Attach되는 Actor에서 해당 정보를 이용해 SkillStatus 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FStatusData> mSkillStatData;

	// 보유 스킬 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> mSkillNameList;

	// 스킬 상태 관리
	TMap<FName, FTimerHandle> mSkillCoolTimer;

	// 스킬 사용 가능 여부 반환
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, FSkillOnCheck> bSkillOnList;

public:
	// 스킬 사용시 UI 업데이트용 델리게이트
	FOnSkillUse OnSkillUse;
	FOnBuffSkillUse OnBuffSkillUse;

#pragma endregion Variable

#pragma region Init Function
public:
	// 스킬 목록과 이펙트 바인딩
	UFUNCTION()
	void SkillEffectBinding();

#pragma endregion Init Function

#pragma region RPC Function
public:
	// 스킬 재사용 대기시간 지정
	UFUNCTION(Server, Reliable)
	void Server_SetSkillTimer(const FName& _SkillName, float _CoolTime);
	void Server_SetSkillTimer_Implementation(const FName& _Skill, float _CoolTime);

	// 스킬 이름으로 해당 스킬 사용
	UFUNCTION(Server, Reliable)
	void Server_UseSkill(int32 _SlotIndex, const FName& _SkillName);
	void Server_UseSkill_Implementation(int32 _SlotIndex, const FName& _SkillName);

	// 플레이어가 요청한 스킬 인덱스 정보
	UFUNCTION(Server, Reliable)
	void Server_UseSkill_Slot(int32 _SlotIndex);
	void Server_UseSkill_Slot_Implementation(int32 _SlotIndex);

	// 스킬 쿨타임 종료 로직
	void OnCoolTimeFinished(FName _SkillName);

	// 스킬 UI 위젯 업데이트
	UFUNCTION(Client, Reliable)
	void Client_UseSkill(int32 _SlotIndex, float _SkillCoolTime);
	void Client_UseSkill_Implementation(int32 _SlotIndex, float _SkillCoolTime);

	UFUNCTION(Client, Reliable)
	void Client_UseBuffSkill(const FSkillData& _SkillData);
	void Client_UseBuffSkill_Implementation(const FSkillData& _SkillData);

	// 스킬 이펙트 On / Off 로직
	UFUNCTION(NetMulticast, Reliable)
	void Multi_EffectOnName(const FName& _SkillName);
	void Multi_EffectOnName_Implementation(const FName& _SkillName);

	UFUNCTION()
	void SkillEffectOff(const FName& _SkillName);

	UFUNCTION(NetMulticast, Reliable)
	void Multi_EffectOffName(const FName& _SkillName);
	void Multi_EffectOffName_Implementation(const FName& _SkillName);

#pragma endregion RPC Function

#pragma region GetSet
public:
	void SetPlayerJob(EPlayerJob _PlayerJob);

	// ## 스킬 테이블 조회, 스킬 생성 로직 ##
	UFUNCTION()
	void SetSkillDataList();

	// 사망한 생존자 Mesh정보 갱신
	UFUNCTION()
	void UpdateSurvivorInfo(AActor* _Survivor);

#pragma endregion GetSet
};
