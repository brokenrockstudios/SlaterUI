// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterMotion.h"

#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<float> CVarMotionScale(
		TEXT("Slater.MotionScale"),
		1.f,
		TEXT("Multiplies the duration of every SlaterUI transition. 0 makes them all instant."),
		ECVF_Default);
}

bool Slater::EaseFromWord(FName Word, ECurveEaseFunction& OutEase)
{
	static const TPair<const TCHAR*, ECurveEaseFunction> Words[] = {
		{TEXT("Linear"), ECurveEaseFunction::Linear},
		{TEXT("QuadIn"), ECurveEaseFunction::QuadIn},
		{TEXT("QuadOut"), ECurveEaseFunction::QuadOut},
		{TEXT("QuadInOut"), ECurveEaseFunction::QuadInOut},
		{TEXT("CubicIn"), ECurveEaseFunction::CubicIn},
		{TEXT("CubicOut"), ECurveEaseFunction::CubicOut},
		{TEXT("CubicInOut"), ECurveEaseFunction::CubicInOut},
	};
	for (const TPair<const TCHAR*, ECurveEaseFunction>& Entry : Words)
	{
		if (Word == FName(Entry.Key))
		{
			OutEase = Entry.Value;
			return true;
		}
	}
	return false;
}

float Slater::GetMotionScale()
{
	return FMath::Max(0.f, CVarMotionScale.GetValueOnGameThread());
}

void FSlaterAnimator::Snap(float Value)
{
	From = To = Value;
	Elapsed = Length = 0.f;
	bActive = false;
	bHasValue = true;
}

void FSlaterAnimator::Start(float Target, float Duration, ECurveEaseFunction InEase)
{
	if (!bHasValue || Duration <= 0.f)
	{
		Snap(Target);
		return;
	}
	From = GetValue();
	To = Target;
	Elapsed = 0.f;
	Length = Duration;
	Ease = InEase;
	bActive = true;
}

bool FSlaterAnimator::Advance(float DeltaTime)
{
	if (!bActive)
	{
		return false;
	}
	Elapsed += FMath::Max(0.f, DeltaTime);
	if (Elapsed >= Length)
	{
		Snap(To);
	}
	return bActive;
}

float FSlaterAnimator::GetValue() const
{
	if (!bActive || Length <= 0.f)
	{
		return To;
	}
	const float Alpha = FCurveHandle::ApplyEasing(FMath::Clamp(Elapsed / Length, 0.f, 1.f), Ease);
	return FMath::Lerp(From, To, Alpha);
}

void FSlaterAnimator::Reset()
{
	Snap(0.f);
	bHasValue = false;
}
