// Copyright 2026 kokage. All Rights Reserved.

#include "SeqImageSection.h"

UMovieSceneSeqImageSection::UMovieSceneSeqImageSection()
{
	SetRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(90)));
}

FSeqImageParams UMovieSceneSeqImageSection::MakeParams() const
{
	FSeqImageParams Params;
	Params.Image         = Image;
	Params.Layout        = Layout;
	Params.Entrance      = Entrance;
	Params.bOverrideExit = bOverrideExit;
	Params.Exit          = Exit;
	Params.Continuous    = Continuous;
	Params.Motion        = Motion;
	return Params;
}
