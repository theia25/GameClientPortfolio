#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ChaseComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTD_API UChaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UChaseComponent();

protected:
	virtual void BeginPlay() override;

#pragma region Variable
protected:
	UPROPERTY(VisibleAnywhere, Category = "Owner")
	TObjectPtr<class ABaseCharacter> mOwner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chase")
	TObjectPtr<class USphereComponent> mChaseSphere;

	// Overlap된 대상들의 목록 저장, 가리키던 대상이 다른 이유로 메모리에서 제거되면 nullptr이 되어 안전한 상태 유지
	UPROPERTY()
	TArray<TWeakObjectPtr<class ABaseCharacter>> mOverlappedCharacterList;

	// 추격 대상 목록 : 추격 대상의 수를 확인해서 킬러의 추격 BGM On/Off 결정함
	UPROPERTY()
	TArray<TWeakObjectPtr<class ABaseCharacter>> mChasedCharacterList;

	// 각 생존자를 주시한 시간 기록
	UPROPERTY()
	TMap<TWeakObjectPtr<class ABaseCharacter>, float> mStareTimeMap;

	// 죽은 생존자 목록 기록 : Overlap 대상과 오클루전 대상에서 제외
	UPROPERTY()
	TArray<TObjectPtr<class ABaseCharacter>> mDeadSurvivorList;

	// 탐지 범위 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float mChaseSphereRadius = 1200.f;

	// 기본 추적 시야각 90도
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float mHalfAngle = 45.f;

	// Overlap된 대상이 있는 경우 시야각 탐지를 위한 타이머
	FTimerHandle mChaseTimerHandle;

	// 탐지 간격 0.2초
	float mChaseTimerRate = 0.2f;

#pragma endregion Variable		


#pragma region Custom Function

#pragma region Overlap Binding Func
protected:
	// Sphere Collision의 이벤트 감지
	UFUNCTION()
	void OnChaseBeginOverlap(
		UPrimitiveComponent* OverlappedComponent, // 이 컴포넌트(UChaseComponent)가 가지고 있는 콜리전 컴포넌트(USphereComponent) 의미
		AActor* OtherActor,						  // 내 콜리전 컴포넌트와 충돌한 상대방 액터
		UPrimitiveComponent* OtherComp,			  // 충돌한 상대방 액터가 가진 콜리전 컴포넌트 (생존자 캐릭터의 캡슐 컴포넌트)
		int32 OtherBodyIndex,					  // 상대방 컴포넌트가 물리 시뮬레이션을 하는 복잡한 메쉬(예: 스켈레탈 메쉬)일 경우, 어떤 부위(뼈)와 충돌했는지에 대한 인덱스 
		bool bFromSweep,						  // 이 오버랩이 스윕(Sweep)에 의해 발생했는지를 나타내는 bool 값
		const FHitResult& SweepResult			  // 만약 bFromSweep이 true일 경우, 스윕에 대한 상세한 충돌 정보(FHitResult)를 담고 있다. (충돌 위치, 노멀 벡터 등의 정보를 알 수 있다.)
	);

	UFUNCTION()
	void OnChaseEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);

#pragma endregion Overlap Binding Func

protected:
	// 탐지 성공 후 호출하는 함수
	UFUNCTION()
	void StartChase(class ABaseCharacter* _Target);

protected:
	// 대상이 시야각 안에 있는지 확인하는 함수, 시야각 검증(Field of View)
	bool IsInFieldOfView(AActor* _Target);

	// 라인 트레이스 검사 함수, 가시성 확보 검증(Line of Sight)
	bool HasLineOfSight(AActor* _Target);

	// 주시 시간 도입
	void CheckStaringTargets();

	// 추격 타이머 시작/중지를 관리 핵심 함수
	void UpdateChaseTimer();

#pragma endregion Custom Function


#pragma region GetSet
public:
	// 사망 처리된 생존자의 오디오 종료 로직 
	UFUNCTION()
	void SetRemoveSurvivorList(AActor* _Survivor);

#pragma endregion GetSet

};
