// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterExamples.h"

FSlaterExampleRegistry& FSlaterExampleRegistry::Get()
{
	static FSlaterExampleRegistry Registry;
	return Registry;
}

bool FSlaterExampleRegistry::Register(FSlaterExample Example)
{
	if (Example.Name.IsNone() || Find(Example.Name))
	{
		return false;
	}
	Examples.Add(MoveTemp(Example));
	return true;
}

bool FSlaterExampleRegistry::Unregister(FName Name)
{
	return Examples.RemoveAll([Name](const FSlaterExample& Example) { return Example.Name == Name; }) > 0;
}

const FSlaterExample* FSlaterExampleRegistry::Find(FName Name) const
{
	return Examples.FindByPredicate([Name](const FSlaterExample& Example) { return Example.Name == Name; });
}

void FSlaterExampleRegistry::GetNames(TArray<FName>& OutNames) const
{
	OutNames.Reset();
	for (const FSlaterExample& Example : Examples)
	{
		OutNames.Add(Example.Name);
	}
}
