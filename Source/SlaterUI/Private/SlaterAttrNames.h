// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Attribute names that more than one source file reads. A name used by a single file stays a function-local
 * `static const FName` beside its use: function scope cannot collide in a unity build, while two file-scope
 * `const FName` of the same name in different files do (the namespace is shared once the files are merged).
 * Move a name here when a second file needs it.
 */
namespace Slater::Attr
{
	inline const FName OpenWhen(TEXT("open-when"));
	inline const FName CloseState(TEXT("close-state"));
	inline const FName OnChange(TEXT("on-change"));
}