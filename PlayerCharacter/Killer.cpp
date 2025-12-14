
#include "Killer.h"

void AKiller::SetDefaultStatus()
{
	Super::SetDefaultStatus();

	// @@ 테이블 로드, 캐릭터 기본 정보 초기화 코드 생략

	// SurvivorTable에 설정한 Status를 불러와 mStatus를 초기화
	for (auto& it : playerRow->Status)
		SetStatusData(EStatusCategoryType::STAT_CATEGORY_DEFAULT, it);


	// ### Skill Status 적용 로직 ###

	// 스킬 데이터 테이블 참조, 스킬 생성
	mSkillComponent->SetSkillDataList();

	// 패시브 스킬 스탯 정보 참조
	TArray<FStatusData>& SkillStatData = mSkillComponent->GetSkillStatData();

	// 패시브 스킬로인한 SkillStatus 정보 설정
	for (FStatusData& SkillStatus : SkillStatData)
	{
		mStatus->SetStatusData(EStatusCategoryType::STAT_CATEGORY_SKILL, SkillStatus);
	}

	// 캐릭터 스탯 초기화
	if (mStatus)
	{
		// 기본속도 초기화
		mBaseSpeed = GetStatusValue(EStatusType::STATTYPE_SPEED);
		GetCharacterMovement()->MaxWalkSpeed = mBaseSpeed;

		UCharacterMovementComponent* MoveComp = GetCharacterMovement();

		// 캐릭터 공중 가속도 감속
		if (MoveComp)
		{
			MoveComp->bUseSeparateBrakingFriction = true;   //낙하 중 제동 마찰력 사용 여부
			MoveComp->BrakingFriction = 2.f;              //제동 시 마찰력(높을수록 빠른 감속)
			MoveComp->BrakingDecelerationFalling = 1024.f;  //낙하 중 감속 가속도
		}

		mAttackDelaySpeed = GetStatusValue(EStatusType::STATTYPE_ATTACKDELAYSPEED);
		mAttackDamage = GetStatusValue(EStatusType::STATTYPE_DAMAGE);

	}

	// 스킬 컴포넌트가 들고있는 스킬 목록 이펙트 바인딩
	mSkillComponent->SkillEffectBinding();
}

void AKiller::AttackAction(const FInputActionValue& _InputActionValue)
{
	// 로컬에서 먼저 공격 가능한지 체크, 서버 부하 줄임 (마우스 좌, 우클릭 동시입력 방지 트리거)
	if (false == bCanMouseInput)
	{
		return;
	}

	// 리플리케이트 변수, 서버에서 공격 쿨타임 관리
	if (bCanAttack)
	{
		bCanMouseInput = false;
		Server_RequestAttack();
	}
}

void AKiller::Server_RequestAttack_Implementation()
{
	// 공격 상태가 아닐 때만 수행
	if (EAttackState::NONE != mCurrentAttackState)
	{
		return;
	}

	bAttackReleased = true;
	mCurrentAttackState = EAttackState::ATTACK_WINDUPHOLD;

	OnRep_AttackStateChanged();
}

void AKiller::OnRep_AttackStateChanged()
{
	if (false == IsValid(mAnim))
		return;

	switch (mCurrentAttackState)
	{
	case EAttackState::ATTACK_WINDUPHOLD:
		if (false == bPlayingMontage)
		{
			bPlayingMontage = true;
			mAnim->PlayWindupHoldMontage(false);
			PlayAttackSpeedCurve();
		}
		break;

		// 반응성은 Multicast가 더 좋으나 타임라인을 통한 이동속도 조정하는 로직때문에 OnRep로 상태 동기화 수행
	case EAttackState::ABILITY:
		if (false == bPlayingMontage)
		{
			bPlayingMontage = true;
			PlayAbilitySpeedCurve();
		}
		break;

	case EAttackState::NONE:
	default:
		bPlayingMontage = false;
		bCanMouseInput = true;
		break;
	}
}

void AKiller::AttackSwingAction(const FInputActionValue& _InputActionValue)
{
	// 서버에 스윙을 요청
	// - 서버는 현재 상태가 HOLDING일 때만 처리
	Server_RequestSwing();
}

void AKiller::Server_RequestSwing_Implementation()
{
	bAttackReleased = false;

	if (EAttackState::ATTACK_WINDUPHOLD == mCurrentAttackState)
	{
		ForceSwing();
	}
}

void AKiller::ForceSwing()
{
	// 스윙 중복 실행 방지
	if (EAttackState::ATTACK_SWING == mCurrentAttackState)
	{
		return;
	}

	// 진행 중이던 타임아웃 타이머를 반드시 초기화
	GetWorld()->GetTimerManager().ClearTimer(mHoldTimerHandle);

	mCurrentAttackState = EAttackState::ATTACK_SWING;

	Multi_PlaySwingAnimation();
}

// ## 킬러 공격 판정 로직 : 
void AKiller::AttackHitCheck()
{
	// 서버가 아닌 경우 로직 실행 방지
	if (!HasAuthority())
		return;

	// 중복 공격 방지
	if (bAttackSuccess)
		return;

	mCachedHitResults.Reset();

	// Box 위치 계산
	const FVector Forward = GetActorForwardVector();
	const FVector CharacterLocation = GetActorLocation();

	// 캐릭터 중심에서 100.f 전방, 30.f 위에 Box 중심 위치 계산
	const FVector BoxCenter = CharacterLocation + Forward * 100.f + FVector(0.f, 0.f, 30.f);

	// Box 크기: 160.f x 100.f x 100.f → 절반 크기를 extent로 지정
	const FVector& BoxHalfExtent = mAttackBoxExtent;

	// 박스 회전: 캐릭터 Forward 방향을 박스의 X축으로 삼음
	const FQuat BoxRotation = GetActorQuat();

	GetWorld()->SweepMultiByChannel(
		mCachedHitResults,
		BoxCenter,
		BoxCenter,
		BoxRotation,
		ECC_GameTraceChannel6, // <-- ECC_Pawn에서 Pawn만 Block되어 있는 트레이스 채널로 교체
		FCollisionShape::MakeBox(BoxHalfExtent),
		mAttackCollisionParams
	);

	// 거리가 가깝고 Forward Vector와 가장 가까운 대상 탐색
	AActor* BestTarget = nullptr;
	float BestScore = -FLT_MAX;

	// 가까운 적을 우선적으로 탐색
	const float DotWeight = 10.f;       // 정면성 중요도
	const float DistanceWeight = 100.f;      // 거리 중요도 

	for (const FHitResult& Hit : mCachedHitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (nullptr == HitActor || this == HitActor)
			continue;

		const FVector ToTarget = (HitActor->GetActorLocation() - CharacterLocation);
		const float DistanceSquare = ToTarget.SizeSquared(); // 거리 제곱 사용
		const float Dot = FVector::DotProduct(Forward, ToTarget.GetSafeNormal());

		// 점수 계산 
		// Dot 클수록 점수 증가 -> 정면에 가까울수록 우선
		// Distance가 작을수록 점수 증가 -> 가까울수록 우선
		float Score = Dot * DotWeight - DistanceSquare * DistanceWeight;

		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = HitActor;
		}
	}

	ACharacter* TargetCharacter = Cast<ACharacter>(BestTarget);

	// 결과 처리
	if (TargetCharacter)
	{
		// 타겟과 킬러 사이의 장애물이 있는지 Line Trace로 확인
		FHitResult ObstacleHitResult;
		FCollisionQueryParams LineTraceParams;

		// 킬러와 선정된 타겟은 Line Trace에서 제외해야 정확한 장애물 판별 가능
		LineTraceParams.AddIgnoredActor(this);
		LineTraceParams.AddIgnoredActor(TargetCharacter);

		FVector StartLocation = this->GetMesh()->GetSocketLocation(FName("head"));
		FVector EndLocation = TargetCharacter->GetMesh()->GetSocketLocation(FName("head"));

		// 킬러 머리위치에서 타겟 머리위치 사이의 장애물을 검사 (울타리 사이나 창틀 사이에서는 공격 맞음)
		bool bHasObstacle = GetWorld()->LineTraceSingleByChannel(
			ObstacleHitResult,
			StartLocation,
			EndLocation,
			ECC_GameTraceChannel5, // <-- ECC_Visibility에서 장애물 탐지 전용 레이스 채널 생성(캐릭터, 관전자, 트리거 제외)
			LineTraceParams
		);

		// 중간에 장애물이 없다면 공격 성공
		if (false == bHasObstacle)
		{
			mSpeedDecreasedValue = -200.f;

			bAttackSuccess = true;

			// 광폭화 버프로 인한 데미지 합산 적용
			float totalDamage = mAttackDamage + GetBuffStatusValue(EStatusType::STATTYPE_BUFFSKILL_OVERDRIVE);

			FDamageEvent DamageEvent;
			BestTarget->TakeDamage(totalDamage, DamageEvent, GetController(), this);

			// Killer : 이펙트 출력
			Server_BleedEffect(BestTarget);

			// 히트 사운드 출력
			mSoundComponent->Multicast_PlayOneShotSound(ESoundTypes::SOUND_ATTACKHIT);
		}
	}

}

void AKiller::AttackEnd()
{
	// 서버만 들어오는 로직

	mCurrentAttackState = EAttackState::NONE;
	bAttackReleased = false;

	OnRep_AttackStateChanged();

	// 중복 공격 방지 트리거 Off
	bAttackSuccess = false;
	bCanVault = true;
}

void AKiller::AttackStart()
{
	// 서버만 호출되는 Notify
	bCanAttack = false;
	bCanVault = false;

	// 어택 타이머 On
	bOnAttackTimer = true;

	// 타이머로 공격 쿨타임 설정
	GetWorld()->GetTimerManager().SetTimer(
		mAttackCoolTimerHandle,
		this,
		&AKiller::ResetAttackCool,
		mAttackCoolTime,
		false // 반복 X	
	);

}