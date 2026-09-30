// Copyright 2026 kokage. All Rights Reserved.

#include "SeqImageSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#if WITH_EDITOR
#include "Channels/MovieSceneChannelEditorData.h"
#endif

#define LOCTEXT_NAMESPACE "MovieSceneSeqImageSection"

UMovieSceneSeqImageSection::UMovieSceneSeqImageSection()
{
	SetRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(90)));

	// Neutral values when a channel has no keys
	OffsetXCurve.SetDefault(0.f);
	OffsetYCurve.SetDefault(0.f);
	ScaleCurve.SetDefault(1.f);
	RotationCurve.SetDefault(0.f);
	OpacityCurve.SetDefault(1.f);
}

EMovieSceneChannelProxyType UMovieSceneSeqImageSection::CacheChannelProxy()
{
	FMovieSceneChannelProxyData Channels;

#if WITH_EDITOR
	const FText Group = LOCTEXT("KeysGroup", "Image Keys");

	FMovieSceneChannelMetaData OffsetXMeta(TEXT("OffsetX"), LOCTEXT("OffsetX", "Offset X"), Group);
	OffsetXMeta.SortOrder = 0;
	FMovieSceneChannelMetaData OffsetYMeta(TEXT("OffsetY"), LOCTEXT("OffsetY", "Offset Y"), Group);
	OffsetYMeta.SortOrder = 1;
	FMovieSceneChannelMetaData ScaleMeta(TEXT("Scale"), LOCTEXT("Scale", "Scale"), Group);
	ScaleMeta.SortOrder = 2;
	FMovieSceneChannelMetaData RotationMeta(TEXT("Rotation"), LOCTEXT("Rotation", "Rotation"), Group);
	RotationMeta.SortOrder = 3;
	FMovieSceneChannelMetaData OpacityMeta(TEXT("Opacity"), LOCTEXT("Opacity", "Opacity"), Group);
	OpacityMeta.SortOrder = 4;

	Channels.Add(OffsetXCurve,  OffsetXMeta,  TMovieSceneExternalValue<float>());
	Channels.Add(OffsetYCurve,  OffsetYMeta,  TMovieSceneExternalValue<float>());
	Channels.Add(ScaleCurve,    ScaleMeta,    TMovieSceneExternalValue<float>());
	Channels.Add(RotationCurve, RotationMeta, TMovieSceneExternalValue<float>());
	Channels.Add(OpacityCurve,  OpacityMeta,  TMovieSceneExternalValue<float>());
#else
	Channels.Add(OffsetXCurve);
	Channels.Add(OffsetYCurve);
	Channels.Add(ScaleCurve);
	Channels.Add(RotationCurve);
	Channels.Add(OpacityCurve);
#endif

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
	return EMovieSceneChannelProxyType::Static;
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

#undef LOCTEXT_NAMESPACE
