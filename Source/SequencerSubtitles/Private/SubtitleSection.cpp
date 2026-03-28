// Copyright 2026 kokage. All Rights Reserved.

#include "SubtitleSection.h"

UMovieSceneSeqSubtitleSection::UMovieSceneSeqSubtitleSection()
	: BarColor(FLinearColor(0.2f, 0.6f, 0.9f, 1.0f))
{
	SetRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(90)));
}

#if WITH_EDITOR
void UMovieSceneSeqSubtitleSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = PropertyChangedEvent.GetPropertyName();
	if (PropName == GET_MEMBER_NAME_CHECKED(UMovieSceneSeqSubtitleSection, SubtitleText))
	{
		TArray<FString> Lines;
		SubtitleText.ToString().ParseIntoArrayLines(Lines, false);
		const int32 LineCount = FMath::Max(1, Lines.Num());
		bOverrideAppearance = true;
		AppearanceOverride.MaxLinesPerPage = LineCount;
		AppearanceOverride.MessageWindowHeight = 0.0f;
	}
}
#endif
