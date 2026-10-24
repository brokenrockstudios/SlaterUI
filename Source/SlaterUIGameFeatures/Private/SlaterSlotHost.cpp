// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterSlotHost.h"

namespace SlaterSlotHostPrivate
{
	TWeakPtr<ISlaterSlotHost>& HostSlot()
	{
		static TWeakPtr<ISlaterSlotHost> Host;
		return Host;
	}
}
using namespace SlaterSlotHostPrivate;

void Slater::SetSlotHost(const TSharedPtr<ISlaterSlotHost>& Host)
{
	HostSlot() = Host;
}

TSharedPtr<ISlaterSlotHost> Slater::GetSlotHost()
{
	return HostSlot().Pin();
}
