// Copyright 2026 kokage. All Rights Reserved.

#include "SubtitleSection.h"

UMovieSceneSeqSubtitleSection::UMovieSceneSeqSubtitleSection()
	: BarColor(FLinearColor(0.2f, 0.6f, 0.9f, 1.0f))
{
	SetRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(90)));
}
