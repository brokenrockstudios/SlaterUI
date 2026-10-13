// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterSlotHost.h"

namespace
{
	TWeakPtr<ISlaterSlotHost>& HostSlot()
	{
		static TWeakPtr<ISlaterSlotHost> Host;
		return Host;
	}
}

void Slater::SetSlotHost(const TSharedPtr<ISlaterSlotHost>& Host)
{
	HostSlot() = Host;
}

TSharedPtr<ISlaterSlotHost> Slater::GetSlotHost()
{
	return HostSlot().Pin();
}
