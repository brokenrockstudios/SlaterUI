// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterOutlet.h"

void SSlaterOutlet::Construct(const FArguments&, FName InId, TSharedRef<SWidget> InDefaultContent)
{
	Id = InId;
	DefaultContent = InDefaultContent;
	// An empty outlet takes no clicks; what is put in it decides for itself.
	SetVisibility(EVisibility::SelfHitTestInvisible);
	ChildSlot
	[
		InDefaultContent
	];
}

void SSlaterOutlet::SetContent(TSharedPtr<SWidget> InContent)
{
	if (Content == InContent)
	{
		return;
	}
	Content = MoveTemp(InContent);
	ChildSlot
	[
		Content.IsValid() ? Content.ToSharedRef() : DefaultContent.ToSharedRef()
	];
}
