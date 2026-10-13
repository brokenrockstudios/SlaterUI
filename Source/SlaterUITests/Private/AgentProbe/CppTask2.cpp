// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "AgentProbeCpp.h"
#include "Widgets/SNullWidget.h"

TSharedRef<SWidget> SlaterProbeCpp::Task2(TFunction<void(int32, int32)>& OutSetCount)
{
	OutSetCount = [](int32, int32) {};
	return SNullWidget::NullWidget;
}

#endif
