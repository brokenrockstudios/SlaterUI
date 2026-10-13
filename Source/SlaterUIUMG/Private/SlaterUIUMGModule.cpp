// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "SlaterElements.h"
#include "SlaterUmg.h"

class FSlaterUIUMGModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Slater::RegisterUmgElements(Slater::Elements());
	}

	virtual void ShutdownModule() override
	{
		Slater::Elements().Unregister(TEXT("umg"));
	}
};

IMPLEMENT_MODULE(FSlaterUIUMGModule, SlaterUIUMG)
