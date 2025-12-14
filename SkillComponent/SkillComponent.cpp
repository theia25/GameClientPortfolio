
#include "SkillComponent.h"
#include "../../Subsystem/TableSubsystem.h"
#include "../Skill/SKillBase.h"

// ## 스킬 이펙트 On / Off 로직 ##
void USkillComponent::Multi_EffectOnName_Implementation(const FName& _SkillName)
{
	ASKillBase& RequestSkill = **mSkillList.Find(_SkillName);

	if (GetOwner()->HasAuthority())
	{
		RequestSkill.ActivateSkillEffect();
	}
	else
	{
		RequestSkill.ActivateSkillEffect();
	}
}

void USkillComponent::SkillEffectOff(const FName& _SkillName)
{
	Multi_EffectOffName(_SkillName);
}

void USkillComponent::Multi_EffectOffName_Implementation(const FName& _SkillName)
{
	ASKillBase& RequestSkill = **mSkillList.Find(_SkillName);

	if (GetOwner()->HasAuthority())
	{
		RequestSkill.DeactivateSkillEffect();
	}
	else
	{
		RequestSkill.DeactivateSkillEffect();
	}
}

// ## 스킬 이펙트 바인딩 ##
void USkillComponent::SkillEffectBinding()
{
	// Elem => TPair<FName, TObjectPtr<ASkillBase>> 타입
	// TMap의 키와 밸류 함께 접근 가능
	for (auto& Elem : mSkillList)
	{
		FName SkillName = Elem.Key;
		ASKillBase* Skill = Elem.Value;

		Skill->mSkillEffectEnded.AddDynamic(this, &USkillComponent::SkillEffectOff);
	}
}

// ## 스킬 재사용 대기시간 지정 ##
void USkillComponent::Server_SetSkillTimer_Implementation(const FName& _SkillName, float _CoolTime)
{
	FTimerHandle& SkillTimer = *mSkillCoolTimer.Find(_SkillName);

	// 델리게이트 생성 + 함수 인자 바인딩
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &USkillComponent::OnCoolTimeFinished, _SkillName);

	GetWorld()->GetTimerManager().SetTimer(
		SkillTimer,
		TimerDelegate,
		_CoolTime,
		false
	);

}

// ## 스킬 사용 로직 ##
void USkillComponent::Server_UseSkill_Implementation(int32 _SlotIndex, const FName& _SkillName)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ASKillBase& RequestSkill = **mSkillList.Find(_SkillName);

		// 스킬 효과 적용
		RequestSkill.UseSkill(_SkillName);

		// 이펙트 켜고 끄는 작업을 스킬 컴포넌트에서 스킬에 접근해서 수행
		Multi_EffectOnName(_SkillName);

		FSkillOnCheck* SkillOnCheck = bSkillOnList.Find(_SkillName);

		if (SkillOnCheck)
		{
			// 복제를 위해 구조체를 덮어쓰기
			FSkillOnCheck CopyFSkillOnCheck = *SkillOnCheck;

			CopyFSkillOnCheck.bSkillOn = false;

			bSkillOnList[_SkillName] = CopyFSkillOnCheck;
		}

		// 스킬 재사용 타이머 세팅
		// 버프 스킬일 경우 버프 지속시간 UI 업데이트
		if (RequestSkill.GetSkillData().skillType == ESkillType::SKILL_BUFF)
		{
			Client_UseBuffSkill(RequestSkill.GetSkillData());
		}

		// 스킬 쿨타임 UI 로직 호출
		Client_UseSkill(_SlotIndex, RequestSkill.GetSkillData().skillCoolTime);
		Server_SetSkillTimer(_SkillName, RequestSkill.GetSkillData().skillCoolTime);
	}

}

// ## 플레이어가 요청한 스킬 인덱스 정보 ##
void USkillComponent::Server_UseSkill_Slot_Implementation(int32 _SlotIndex)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		const FName& SkillName = GetSkillName(_SlotIndex);

		FSkillOnCheck* SkillOnCheck = bSkillOnList.Find(SkillName);

		// bSkillOnList에서 해당 스킬이 쿨타임 상태인지 체크
		if (SkillOnCheck)
		{
			if (false == SkillOnCheck->bSkillOn)
			{
				return;
			}
			else
			{
				Server_UseSkill(_SlotIndex, SkillName);
			}
		}
	}

}

// ## 스킬 쿨타임 종료 로직 ##
void USkillComponent::OnCoolTimeFinished(FName _SkillName)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		FSkillOnCheck* SkillOnCheck = bSkillOnList.Find(_SkillName);
		if (SkillOnCheck)
		{
			// 복제를 위해 구조체를 덮어쓰기
			FSkillOnCheck CopyFSkillOnCheck = *SkillOnCheck;

			CopyFSkillOnCheck.bSkillOn = true;

			bSkillOnList[_SkillName] = CopyFSkillOnCheck;
		}
	}

}

// ## 스킬 UI 위젯 업데이트 ##
void USkillComponent::Client_UseSkill_Implementation(int32 _SlotIndex, float _SkillCoolTime)
{
	OnSkillUse.Broadcast(_SlotIndex, _SkillCoolTime);
}

void USkillComponent::Client_UseBuffSkill_Implementation(const FSkillData& _SkillData)
{
	OnBuffSkillUse.Broadcast(_SkillData);
}

void USkillComponent::SetPlayerJob(EPlayerJob _PlayerJob)
{
	mPlayerJob = _PlayerJob;
}

// ## 스킬 테이블 조회, 스킬 생성 ##
void USkillComponent::SetSkillDataList()
{
	// 테이블 서브시스템
	UTableSubsystem* TableSub = UTableSubsystem::Get(this);
	if (nullptr == TableSub)
		return;

	ABaseCharacter* OwnerPlayer = Cast<ABaseCharacter>(GetOwner());
	if (nullptr == OwnerPlayer)
		return;

	AInGamePlayerState* InGamePlayerState = OwnerPlayer->GetPlayerState<AInGamePlayerState>();
	if (nullptr == InGamePlayerState)
		return;

	FName CharacterTID = InGamePlayerState->GetCharacterTID();

	FPlayerSkillTableRow* PlayerSkillRow = TableSub->FindTableRow<FPlayerSkillTableRow>(UTableData::TableName::PLAYERSKILL, CharacterTID);
	if (nullptr == PlayerSkillRow)
		return;

	SetPlayerJob(PlayerSkillRow->PlayerJob);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 스킬 추가 시 사용한 이름으로 목록 생성, 조회
	TArray<FName> ConvertedFNameList = {};
	Algo::Transform(PlayerSkillRow->SkillNameList, ConvertedFNameList,
		[](ESkillName _SkillName)
		{
			return UGameDefines::ConvertSkillEnumToFName(_SkillName);
		});

	for (const FName& SkillName : ConvertedFNameList)
	{
		FSkillDataTableRow* SkillDataRow = TableSub->FindTableRow<FSkillDataTableRow>(UTableData::TableName::SKILLDATA, SkillName);
		if (nullptr == SkillDataRow)
			return;

		ASKillBase* SpawnedSkill = GetWorld()->SpawnActor<ASKillBase>(SkillDataRow->SkillClass, SpawnParams);
		if (nullptr == SpawnedSkill)
			return;

		SpawnedSkill->InitializeSkillData(*SkillDataRow);

		// BuffSkill 생성 시 필수 로직
		SpawnedSkill->SetOwner(OwnerPlayer);  // 반드시 소유자 설정
		SpawnedSkill->SetReplicates(true);
		SpawnedSkill->SetReplicateMovement(false);  // 필요시 설정

		// 투시 스킬을 보유한 경우 카메라에 Outline, Occlusion 머티리얼 적용
		if ("SKILL_XRAYVISION" == SkillDataRow->SkillName)
		{
			SpawnedSkill->InitXrayMaterial(OwnerPlayer->GetCamera());
		}

		// 스킬 보유 목록 갱신
		mSkillDataList.Add(SkillName, SpawnedSkill->GetSkillData());
		mSkillList.Add(SkillName, SpawnedSkill);

		// 스킬 활성화 여부 체크
		FSkillOnCheck NewSkill;
		NewSkill.bSkillOn = true;
		bSkillOnList.Add(SkillName, NewSkill);

		// 패시브 스킬: 캐릭터 스탯 시스템과 연동
		if (ESkillType::SKILL_PASSIVE == SkillDataRow->SkillType)
		{
			SetSkillStatData(SkillDataRow->SkillStatBoost);
		}
		// 버프 스킬은 타이머 로직 적용
		else if (ESkillType::SKILL_BUFF == SkillDataRow->SkillType)
		{
			// 버프형 스킬은 배열로 받아서 숫자 키 1번, 2번으로 바인딩
			mSkillNameList.Add(SkillName);

			FTimerHandle NesSkillTimer;

			mSkillCoolTimer.Add(SkillName, NesSkillTimer);
		}
	}

}

void USkillComponent::UpdateSurvivorInfo(AActor* _Survivor)
{
	ASKillBase* XraySkill = *mSkillList.Find(FName("SKILL_XRAYVISION"));
	if (XraySkill)
	{
		XraySkill->Client_SetSurvivorMeshUpdate(_Survivor);
	}
}

