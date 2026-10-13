// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * A named place in a layout that C++ fills with its own widget (`<outlet id="Content">` and `SSlaterView::SetOutlet`).
 * The element's child is the default content: it shows until something is set, and again when it is cleared. The
 * content is owned by whoever set it, so it survives a reload of the layout (the view puts it into the new tree).
 */
class SLATERUI_API SSlaterOutlet : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSlaterOutlet) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, FName InId, TSharedRef<SWidget> InDefaultContent);

	FName GetOutletId() const { return Id; }

	/** Shows Content in place of the default content; null brings the default content back. */
	void SetContent(TSharedPtr<SWidget> InContent);
	TSharedPtr<SWidget> GetContent() const { return Content; }
	bool HasContent() const { return Content.IsValid(); }

private:
	FName Id;
	TSharedPtr<SWidget> DefaultContent;
	TSharedPtr<SWidget> Content;
};
