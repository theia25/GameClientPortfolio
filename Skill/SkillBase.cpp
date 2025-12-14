#include "SkillBase.h"
#include "NiagaraFunctionLibrary.h"

ASKillBase::ASKillBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// 지역 변수처럼 보이지만 UObject 시스템에 등록되어 this->RootComponent에 할당된다. (GC 대상도 아님)
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent = SceneRoot;

	// 좌, 우 이펙트 부착을 위해 선언한 컴포넌트
	mNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SkillEffectComponent"));
	mNiagaraComponent->SetupAttachment(RootComponent);
	mNiagaraComponent->SetAutoActivate(false);
	mNiagaraComponent->SetIsReplicated(true);

	mNiagaraComponent2 = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SkillEffectComponent2"));
	mNiagaraComponent2->SetupAttachment(RootComponent);
	mNiagaraComponent2->SetAutoActivate(false);
	mNiagaraComponent2->SetIsReplicated(true);
}

void ASKillBase::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASKillBase, mSkillData);
	DOREPLIFETIME(ASKillBase, mNiagaraSkillEffect);
}

void ASKillBase::UseSkill_Implementation(const FName& _SkillName)
{
	LOG_TIME(5.f, TEXT("[ASKillBase::UseSkill()] - Base Skill"));
}

void ASKillBase::InitializeSkillData(const FSkillDataTableRow& _InRow)
{
	mSkillData.skillName = _InRow.SkillName;
	mSkillData.skillType = _InRow.SkillType;
	mSkillData.skillStatBoost = _InRow.SkillStatBoost;
	mSkillData.skillCoolTime = _InRow.SkillCoolTime;
	mSkillData.skillDuration = _InRow.SkillDuration;
	mSkillData.skillviewTID = _InRow.SkillViewTID;
	mSkillData.skillClass = _InRow.SkillClass;
	mSkillData.mSkillEffect = _InRow.SkillEffect;

	// 스킬 이펙트가 있는 경우 초기화 
	if (mSkillData.mSkillEffect)
	{
		mNiagaraSkillEffect = mSkillData.mSkillEffect;

		ABaseCharacter* Player = Cast<ABaseCharacter>(GetOwner());
		if (nullptr == Player)
			return;

		USkeletalMeshComponent* MeshComp = Player ? Player->GetMesh() : nullptr;

		// 테이블에서 이펙트 부착 위치를 설정하지 않았을 경우 디폴트 위치 (pelvis)
		if (_InRow.AttachSocketNames.IsEmpty())	
		{
			mNiagaraComponent->SetAsset(mNiagaraSkillEffect);

			mNiagaraComponent->AttachToComponent(
				Player->GetMesh(),
				FAttachmentTransformRules(EAttachmentRule::SnapToTarget,  // 위치 소켓 기준
					EAttachmentRule::KeepRelative,  // 회전 로컬 기준 유지
					EAttachmentRule::KeepRelative,
					false),
				FName("pelvis")
			);

			// 상대 위치 및 회전 초기화
			mNiagaraComponent->SetRelativeLocation(FVector::ZeroVector);
			mNiagaraComponent->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
		}
		else if (1 == _InRow.AttachSocketNames.Num())
		{
			mNiagaraComponent->SetAsset(mNiagaraSkillEffect);

			FName EffectSocketName = _InRow.AttachSocketNames[0];

			// 스킬 이펙트를 부착할 소켓 위치가 메쉬에 존재하지 않는 경우 pelvis로 변경
			if (false == MeshComp->DoesSocketExist(EffectSocketName))
			{
				EffectSocketName = FName("pelvis");
			}

			mNiagaraComponent->AttachToComponent(
				Player->GetMesh(),
				FAttachmentTransformRules(EAttachmentRule::SnapToTarget,  // 위치 소켓 기준
					EAttachmentRule::KeepRelative,  // 회전 로컬 기준 유지
					EAttachmentRule::KeepRelative,
					false),
				EffectSocketName // FName("spine_01")
			);

			// 상대 위치 및 회전 초기화
			mNiagaraComponent->SetRelativeLocation(FVector::ZeroVector);
			mNiagaraComponent->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
		}

		// 2개 이상인 경우에도 앞 2개 위치에만 소켓 부착함 
		// - 동적으로 나이아가라 컴포넌트 생성 후 배열로 관리하면 이펙트 On/Off마다 메모리에 생성/제거 해야됨
		else
		{
			// Overdrive 버프 스킬 이펙트를 좌, 우에 부착하기 위해 만든 코드 (부착 위치 2개이상 확장 고려 X)
			mNiagaraComponent->SetAsset(mNiagaraSkillEffect);
			FName EffectSocketName = _InRow.AttachSocketNames[0];

			// 스킬 이펙트를 부착할 소켓 위치가 메쉬에 존재하지 않는 경우 pelvis로 변경
			if (false == MeshComp->DoesSocketExist(EffectSocketName))
			{
				EffectSocketName = FName("pelvis");
			}

			mNiagaraComponent->AttachToComponent(
				Player->GetMesh(),
				FAttachmentTransformRules(EAttachmentRule::SnapToTarget,  // 위치 소켓 기준
					EAttachmentRule::KeepRelative,  // 회전 로컬 기준 유지
					EAttachmentRule::KeepRelative,
					false),
				EffectSocketName // FName("spine_01")
			);
			mNiagaraComponent->SetRelativeLocation(FVector::ZeroVector);
			mNiagaraComponent->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));


			mNiagaraComponent2->SetAsset(mNiagaraSkillEffect);
			EffectSocketName = _InRow.AttachSocketNames[1];

			// 스킬 이펙트를 부착할 소켓 위치가 메쉬에 존재하지 않는 경우 pelvis로 변경
			if (false == MeshComp->DoesSocketExist(EffectSocketName))
			{
				EffectSocketName = FName("pelvis");
			}

			mNiagaraComponent2->AttachToComponent(
				Player->GetMesh(),
				FAttachmentTransformRules(EAttachmentRule::SnapToTarget,  // 위치 소켓 기준
					EAttachmentRule::KeepRelative,  // 회전 로컬 기준 유지
					EAttachmentRule::KeepRelative,
					false),
				EffectSocketName // FName("spine_01")
			);
			mNiagaraComponent2->SetRelativeLocation(FVector::ZeroVector);
			mNiagaraComponent2->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));

		}

	}

}

void ASKillBase::InitXrayMaterial(UCameraComponent* _Camera)
{
	// PassiveSkill, BuffSkill에서 재정의해서 사용
}

void ASKillBase::ActivateSkillEffect()
{
	mNiagaraComponent->Activate(true);

	if (mNiagaraComponent2->GetAsset())
	{
		mNiagaraComponent2->Activate(true);
	}
}

void ASKillBase::DeactivateSkillEffect()
{
	mNiagaraComponent->DeactivateImmediate();

	if (mNiagaraComponent2->GetAsset())
	{
		mNiagaraComponent2->DeactivateImmediate();
	}
}

void ASKillBase::Client_SetSurvivorMeshUpdate_Implementation(AActor* _Survivor)
{
	// 사망한 생존자 Mesh정보 갱신이 필요한 경우 구현
}
