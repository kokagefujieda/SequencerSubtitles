// Copyright 2026 kokage. All Rights Reserved.

#include "SeqImageTrack.h"
#include "SeqImageSection.h"
#include "SeqImageEvalTemplate.h"

#define LOCTEXT_NAMESPACE "MovieSceneSeqImageTrack"

UMovieSceneSeqImageTrack::UMovieSceneSeqImageTrack()
{
	// Same as the subtitle track: required for legacy template evaluation to work in SubSequences
	EvalOptions.bCanEvaluateNearestSection = EvalOptions.bEvaluateNearestSection_DEPRECATED = true;

#if WITH_EDITORONLY_DATA
	TrackTint = FColor(200, 120, 60, 200);
#endif
}

bool UMovieSceneSeqImageTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}

bool UMovieSceneSeqImageTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneSeqImageSection::StaticClass();
}

UMovieSceneSection* UMovieSceneSeqImageTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSeqImageSection>(this, NAME_None, RF_Transactional);
}

const TArray<UMovieSceneSection*>& UMovieSceneSeqImageTrack::GetAllSections() const
{
	return Sections;
}

bool UMovieSceneSeqImageTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}

void UMovieSceneSeqImageTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}

void UMovieSceneSeqImageTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}

void UMovieSceneSeqImageTrack::RemoveSectionAt(int32 SectionIndex)
{
	Sections.RemoveAt(SectionIndex);
}

void UMovieSceneSeqImageTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}

FMovieSceneEvalTemplatePtr UMovieSceneSeqImageTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	if (const UMovieSceneSeqImageSection* ImageSection = Cast<const UMovieSceneSeqImageSection>(&InSection))
	{
		return FSeqImageEvalTemplate(*ImageSection);
	}
	return FMovieSceneEvalTemplatePtr();
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneSeqImageTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Image");
}
#endif

#undef LOCTEXT_NAMESPACE
