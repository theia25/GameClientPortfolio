// Fill out your copyright notice in the Description page of Project Settings.


#include "ChaseComponent.h"
#include "Net/UnrealNetwork.h"     
#include "Components/SphereComponent.h" 
#include "../BaseCharacter.h"

UChaseComponent::UChaseComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	mChaseSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ChaseSphere"));

	mChaseSphere->SetSphereRadius(mChaseSphereRadius);

	// 1. 콜리전 활성화: 쿼리(Overlap, Trace)에만 반응하도록 설정 (물리 시뮬레이션 X)
	mChaseSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// 2. 오브젝트 타입 설정: 이 스피어 자체는 동적인 오브젝트로 설정 
	mChaseSphere->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);

	// 3. 모든 채널에 대한 기본 반응을 무시(Ignore)로 설정
	mChaseSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);

	// 4. Pawn 채널에 대해서만 Overlap으로 반응하도록 설정
	mChaseSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);

	// 생존자 피격 이후에 변경되는 오브젝트 채널 Immortal과도 Overlap 설정
	mChaseSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_GameTraceChannel4, ECollisionResponse::ECR_Overlap);

}

void UChaseComponent::BeginPlay()
{
	Super::BeginPlay();

	// BeginPlay 시점에서는 Owner가 확실하게 존재하므로 초기화 가능.
	mOwner = Cast<ABaseCharacter>(GetOwner());

	// 오버랩 이벤트는 서버에서만 감지하고 처리
	if (mOwner->HasAuthority())
	{
		// ChaseShape(Sphere)의 오버랩 이벤트에 만든 함수를 연결(바인딩)
		mChaseSphere->OnComponentBeginOverlap.AddDynamic(this, &UChaseComponent::OnChaseBeginOverlap);
		mChaseSphere->OnComponentEndOverlap.AddDynamic(this, &UChaseComponent::OnChaseEndOverlap);		
	}

}

void UChaseComponent::OnChaseBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 서버에서만 탐지 진행
	if (mOwner->HasAuthority())
	{
		ABaseCharacter* TargetCharacter = Cast<ABaseCharacter>(OtherActor);
		if (nullptr == TargetCharacter && mOwner == TargetCharacter)
		{
			return;
		}

		// 죽은 생존자는 Overlap대상에서 제외시킴
		if (mDeadSurvivorList.Contains(TargetCharacter))
		{
			return;
		}

		// 이미 추격이 시작된 대상인 경우 return
		if (mChasedCharacterList.Contains(TargetCharacter))
		{
			return;
		}

		// Overlap된 캐릭터 리스트에 등록 (AddUnique()로 중복 방지)
		mOverlappedCharacterList.AddUnique(TargetCharacter);

		// Overlap된 생존자 캐릭터는 심박동 사운드 재생
		TargetCharacter->Client_PlayOverlapAlarm(ESoundTypes::SOUND_OVERLAP_ALARM);

		// 타이머 상태 업데이트
		UpdateChaseTimer();

	}

}

void UChaseComponent::OnChaseEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// 서버에서만 리스트에 접근
	if (mOwner->HasAuthority())
	{
		ABaseCharacter* TargetCharacter = Cast<ABaseCharacter>(OtherActor);
		if (TargetCharacter == nullptr || TargetCharacter == mOwner)
		{
			return;
		}

		// Overlap만 되고 추적은 안된 경우
		if (mOverlappedCharacterList.Contains(TargetCharacter))
		{
			mOverlappedCharacterList.RemoveSwap(TargetCharacter);

			// 추격이 성공하지 않은 경우 따로 삭제시켜줘야 한다.
			mStareTimeMap.Remove(TargetCharacter);

			// 심박동 사운드 종료
			TargetCharacter->Client_StopOverlapAlarm(ESoundTypes::SOUND_OVERLAP_ALARM);
		}

		// 추격 대상이 EndOverlap된 경우
		else if (mChasedCharacterList.Contains(TargetCharacter))
		{
			mChasedCharacterList.RemoveSwap(TargetCharacter);

			// 추격 사운드 종료
			TargetCharacter->Client_StopChaseSound(ESoundTypes::SOUND_CHASE);

			// 심박동 사운드 종료
			TargetCharacter->Client_StopOverlapAlarm(ESoundTypes::SOUND_OVERLAP_ALARM);
		}

		// Overlap대상이 없을 때 타이머 클리어하는 용도
		UpdateChaseTimer();

		// mChasedCharacterList 목록이 비었을 때 킬러의 추격 사운드 종료 
		if (0 == mChasedCharacterList.Num())
		{
			mOwner->Client_StopChaseSound(ESoundTypes::SOUND_CHASE);
		}

	}

}

// ## 추격 성공 시 로직 ##
void UChaseComponent::StartChase(ABaseCharacter* _Target)
{
	if (nullptr == _Target)
	{
		return;
	}

	// 주시 시간 맵에서 제거
	mStareTimeMap.Remove(_Target);

	// 추격 대상은 Overlap 목록에서 제외
	mOverlappedCharacterList.RemoveSwap(_Target);

	// 타이머 관리 함수를 호출하여 Overlap 대상이 있는지 상태를 다시 검사 
	UpdateChaseTimer();

	// 킬러 시야에 보인 경우 킬러와 생존자 모두 추격 bgm 재생
	mChasedCharacterList.AddUnique(_Target);

	// 탐지 된 생존자에게 놀라는 소리 재생 명령 (글로벌 재생에서 범위 재생으로 변경)
	_Target->Multicast_PlayChaseStartSound(ESoundTypes::SOUND_CHASESTART);

	// 추격 대상 생존자에게 추격 bgm재생 명령
	_Target->Client_PlayChaseSound(ESoundTypes::SOUND_CHASE);

	if (1 == mChasedCharacterList.Num())
	{
		// 킬러 캐릭터도 추격음 재생
		mOwner->Client_PlayChaseSound(ESoundTypes::SOUND_CHASE);
	}

}

// ## 시야각 검증 로직 ##
bool UChaseComponent::IsInFieldOfView(AActor* _Target)
{
	AActor* Owner = Cast<AActor>(mOwner);

	if (Owner && _Target)
	{
		// GetActorForwardVector()는 단위 벡터 반환
		FVector ForwardVector = Owner->GetActorForwardVector();

		// 타겟을 향한 벡터 계산
		FVector DirectionToTarget = _Target->GetActorLocation() - Owner->GetActorLocation();

		// 높이(Z축) 문제 해결 : 평면 벡터 계산
		ForwardVector.Z = 0;
		DirectionToTarget.Z = 0;

		// 방향 벡터 정규화, Z값을 바꿨으므로 벡터의 길이가 1이 아닐 수도 있다.
		ForwardVector = ForwardVector.GetSafeNormal();
		DirectionToTarget = DirectionToTarget.GetSafeNormal();

		// 정규화 된 벡터 사이의 내적 계산, 내적 결과는 두 벡터 사이의 각도, Cos값
		const float DotProduct = FVector::DotProduct(ForwardVector, DirectionToTarget);

		// 시야각의 절반에 해당하는 Cos값을 구한다.
		const float AngleCosThreshold = FMath::Cos(FMath::DegreesToRadians(mHalfAngle));

		// 내적 결과가 시야각의 Cos값보다 큰 경우 Target은 킬러의 시야각 내부에 존재한다.
		return DotProduct > AngleCosThreshold;
	}

	return false;
}

// ## 가시성 검증 ##
bool UChaseComponent::HasLineOfSight(AActor* _Target)
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(mOwner);
	ACharacter* TargetCharacter = Cast<ACharacter>(_Target);

	if (OwnerCharacter)
	{
		// 캐릭터 머리 위치에서 계산한 방향으로 Ray발사 
		FVector StartLocation = OwnerCharacter->GetMesh()->GetSocketLocation(FName("head"));
		FVector EndLocation = TargetCharacter->GetMesh()->GetSocketLocation(FName("head"));

		// 충돌 파라미터를 설정
		FCollisionQueryParams CollisionParams;

		// Ray 발사 대상과 타겟은 무시
		CollisionParams.AddIgnoredActor(OwnerCharacter);
		CollisionParams.AddIgnoredActor(TargetCharacter);

		// 라인 트레이스 결과 저장
		FHitResult HitResult;

		bool bHit = GetWorld()->LineTraceSingleByChannel(
			HitResult,
			StartLocation,
			EndLocation,
			ECC_GameTraceChannel5, // <-- ECC_Visibility에서 장애물 탐지 전용 트레이스 채널로 변경
			CollisionParams
		);

		// Ray와 충돌대상 없는 경우
		if (false == bHit)
		{
			// 킬러 시야 확보상태
			return true;
		}

	}

	// 시야 확보 실패
	return false;
}


// ## Broad-Narrow 파이프라인을 모방한 시야 판정 로직 ##
void UChaseComponent::CheckStaringTargets()
{
	// 서버가 아니면 즉시 종료
	if (!mOwner->HasAuthority())
	{
		return;
	}

	// 생존자 목록 순회 중 배열 변경을 막기 위해 복사해서 사용
	TArray<TWeakObjectPtr<ABaseCharacter>> OverlappedTargets = mOverlappedCharacterList;

	for (const TWeakObjectPtr<ABaseCharacter>& Target : OverlappedTargets)
	{
		// Target이 유효하지 않으면 건너뜀 (안전 장치)
		if (!Target.IsValid())
			continue;

		ABaseCharacter* TargetCharacter = Target.Get();

		// 시야각과 장애물을 검사하는 함수 (핵심 코드)
		bool bIsInSight = IsInFieldOfView(TargetCharacter) && HasLineOfSight(TargetCharacter);

		if (bIsInSight)
		{
			// 시야 내에 있으면: 주시 시간을 누적
			float& StareTime = mStareTimeMap.FindOrAdd(Target, 0.0f);
			StareTime += mChaseTimerRate; // 타이머 간격만큼 시간 추가

			// 누적 시간이 1.4초를 넘었는지 확인
			if (StareTime >= 1.4f)
			{
				// 탐지 성공! 추격 시작
				StartChase(TargetCharacter);
			}
		}
		else
		{
			// 시야 밖에 있으면 주시 시간을 리셋
			mStareTimeMap.Remove(Target);
		}
	}

}

// ## 추격 타이머 시작/중지를 관리 핵심 함수 ##
void UChaseComponent::UpdateChaseTimer()
{
	// 오버랩 목록에 한 명이라도 있으면 타이머를 (재)시작하거나 계속 실행 
	if (mOverlappedCharacterList.Num() > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(
			mChaseTimerHandle,
			this,
			&UChaseComponent::CheckStaringTargets,
			mChaseTimerRate,
			true
		);
	}
	// 목록에 아무도 없으면 타이머를 확실히 정지
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(mChaseTimerHandle);
	}
}

// ## 사망 처리된 생존자의 오디오 종료 로직 ##
void UChaseComponent::SetRemoveSurvivorList(AActor* _Survivor)
{
	if (mOwner->HasAuthority())
	{
		ABaseCharacter* DeadSurvivor = Cast<ABaseCharacter>(_Survivor);

		if (mOverlappedCharacterList.Contains(DeadSurvivor))
		{
			mOverlappedCharacterList.RemoveSwap(DeadSurvivor);

			if (mStareTimeMap.Contains(DeadSurvivor))
			{
				mStareTimeMap.Remove(DeadSurvivor);
			}

			DeadSurvivor->Client_StopOverlapAlarm(ESoundTypes::SOUND_OVERLAP_ALARM);
		}

		else if (mChasedCharacterList.Contains(DeadSurvivor))
		{
			mChasedCharacterList.RemoveSwap(DeadSurvivor);

			// 추격 사운드 종료
			DeadSurvivor->Client_StopChaseSound(ESoundTypes::SOUND_CHASE);

			// 심박동 사운드 종료
			DeadSurvivor->Client_StopOverlapAlarm(ESoundTypes::SOUND_OVERLAP_ALARM);

			if (0 == mChasedCharacterList.Num())
			{
				mOwner->Client_StopChaseSound(ESoundTypes::SOUND_CHASE);
			}
		}

		UpdateChaseTimer();

		mDeadSurvivorList.AddUnique(DeadSurvivor);

	}

}
