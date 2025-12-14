
#include "BuffSkill.h"
#include "../Component/StatusComponent.h"


void ABuffSkill::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 투시 스킬에 사용될 Material 정보
	DOREPLIFETIME(ABuffSkill, XrayOcclusionMaterial);
	DOREPLIFETIME(ABuffSkill, XrayOutlineMaterial);
}

// ## TODO [캐릭터] : BuffSkill 타입의 스킬 2가지로 분리해서 로직 실행 ##
void ABuffSkill::UseSkill_Implementation(const FName& _SkillName)
{
	// 스킬 이름으로 로직 분리해서 사용중 (변경 예정)
	if ("SKILL_OVERDRIVE" == _SkillName)
	{
		ABaseCharacter* Player = Cast<ABaseCharacter>(GetOwner());
		if (nullptr == Player)
			return;

		UStatusComponent* PlayerStatusComp = Player->GetStatus();

		for (FStatusData& BuffStatus : mSkillData.skillStatBoost)
		{
			PlayerStatusComp->SetBuffStatus(BuffStatus);
		}

		// 버프 지속시간 후 해제
		GetWorld()->GetTimerManager().SetTimer(
			BuffDurationHandle_Overdrive,
			this,
			&ABuffSkill::EndBuff_Overdrive,
			mSkillData.skillDuration,
			false
		);
	}
	else
	{
		Client_SetPostProcessWeight(1.f);  // 로컬에서만 처리

		GetWorld()->GetTimerManager().SetTimer(
			BuffDurationHandle_XrayVision,
			this,
			&ABuffSkill::EndBuff_XrayVision,
			mSkillData.skillDuration,
			false
		);

	}

}

// ## 버프 지속시간 종료 시 델리게이트 호출 ##
void ABuffSkill::EndBuff_Overdrive()
{
	if (HasAuthority())
	{
		ABaseCharacter* Player = Cast<ABaseCharacter>(GetOwner());
		if (nullptr == Player)
			return;

		UStatusComponent* PlayerStatusComp = Player->GetStatus();

		for (FStatusData& BuffStatus : mSkillData.skillStatBoost)
		{
			PlayerStatusComp->SetBuffInit(BuffStatus);
		}


		mSkillEffectEnded.Broadcast(this->GetSkillName());
	}

}

void ABuffSkill::EndBuff_XrayVision()
{
	if (HasAuthority())
	{
		Client_SetPostProcessWeight(0.f);

		mSkillEffectEnded.Broadcast(this->GetSkillName());
	}

}

// ## 투시스킬을 위한 포스트 프로세스 적용 로직 ##
void ABuffSkill::Client_SetPostProcessWeight_Implementation(float _Weight)
{
	// 투시 스킬 사용 시 처음 한번만 수행, 맵에 소환된 생존자의 메쉬 정보를 저장
	if (bInitMeshTrigger)
	{
		InitSurvivorMesh();

		// 카메라 초기화 부분도 한번만 진행
		APlayerController* PC = Cast<APlayerController>(UGameplayStatics::GetPlayerController(this, 0));
		if (nullptr == PC || !(PC->IsLocalController()))
			return;

		AActor* ViewTarget = PC->GetViewTarget();
		if (nullptr == ViewTarget)
			return;

		mOwnerCameraCached = ViewTarget->FindComponentByClass<UCameraComponent>();
		if (nullptr == mOwnerCameraCached)
			return;

	}

	if (_Weight > 0.f)
	{
		for (TObjectPtr<USkeletalMeshComponent> SurvivorMesh : mSurvivorsMeshes)
		{
			if (USkeletalMeshComponent* MeshComp = SurvivorMesh.Get())
			{
				// 원하는 스텐실 값 설정 (0 ~ 255)
				MeshComp->SetCustomDepthStencilValue(32);
			}
		}
	}
	else
	{
		for (TObjectPtr<USkeletalMeshComponent> SurvivorMesh : mSurvivorsMeshes)
		{
			if (USkeletalMeshComponent* MeshComp = SurvivorMesh.Get())
			{
				// 원하는 스텐실 값 설정 (0 ~ 255)
				MeshComp->SetCustomDepthStencilValue(0);
			}
		}
	}

	if (XrayOutlineMaterial)
	{
		mOwnerCameraCached->AddOrUpdateBlendable(XrayOutlineMaterial, _Weight);
	}
	if (XrayOcclusionMaterial)
	{
		mOwnerCameraCached->AddOrUpdateBlendable(XrayOcclusionMaterial, _Weight);
	}

}

// ## 생존자 스켈레탈 메쉬 정보 커스텀 뎁스 설정 ##
void ABuffSkill::InitSurvivorMesh()
{
	if (bInitMeshTrigger)
	{
		// 호스트가 킬러와 거리가 먼 생존자의 정보를 관련성이 없다고 판다하여 찾지 않음 
		// ==> 강제로 Survivor 클래스의 생성자에게 항상 네트워크 관련이 있도록 만들기
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASurvivor::StaticClass(), mSurvivors);

		for (AActor* Actor : mSurvivors)
		{
			ASurvivor* Survivor = Cast<ASurvivor>(Actor);
			if (Survivor)
			{
				USkeletalMeshComponent* Mesh = Survivor->GetMesh();
				if (Mesh)
				{
					mSurvivorsMeshes.Add(Mesh);
				}
			}
		}

		for (TObjectPtr<USkeletalMeshComponent> SurvivorMesh : mSurvivorsMeshes)
		{
			if (USkeletalMeshComponent* MeshComp = SurvivorMesh.Get())
			{
				// 커스텀 뎁스 렌더 사용 여부
				MeshComp->SetRenderCustomDepth(true);

				// 원하는 스텐실 값 설정 (0 ~ 255)
				MeshComp->SetCustomDepthStencilValue(0);

				// 커스텀 뎁스 스텐실 버퍼 마스크 설정 (엔진에서 필요 시)
				MeshComp->CustomDepthStencilWriteMask = ERendererStencilMask::ERSM_Default;

			}
		}

		bInitMeshTrigger = false;
	}

}

// ## 사망한 생존자의 메쉬 정보 갱신 ##
void ABuffSkill::Client_SetSurvivorMeshUpdate_Implementation(AActor* _Survivor)
{
	ASurvivor* DeadSurvivor = Cast<ASurvivor>(_Survivor);
	if (DeadSurvivor)
	{
		USkeletalMeshComponent* TargetMesh = DeadSurvivor->GetMesh();
		if (TargetMesh)
		{
			TargetMesh->SetRenderCustomDepth(false);
			mSurvivorsMeshes.Remove(TargetMesh);
		}
	}
}
