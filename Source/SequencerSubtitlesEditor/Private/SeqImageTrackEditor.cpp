// Copyright 2026 kokage. All Rights Reserved.

#include "SeqImageTrackEditor.h"
#include "SeqImageTrack.h"
#include "SeqImageSection.h"
#include "SubtitleSettings.h"

#include "ISequencer.h"
#include "SequencerSectionPainter.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "MovieScene.h"
#include "ScopedTransaction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#if ENGINE_MINOR_VERSION >= 7
#include "MVVM/Views/ViewUtilities.h"
#endif

#define LOCTEXT_NAMESPACE "SeqImageTrackEditor"

// --- FSeqImageSectionUI ---

FSeqImageSectionUI::FSeqImageSectionUI(UMovieSceneSection& InSection)
	: FSequencerSection(InSection)
{
}

int32 FSeqImageSectionUI::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	int32 LayerId = Painter.PaintSectionBackground();

	UMovieSceneSeqImageSection* ImageSection = Cast<UMovieSceneSeqImageSection>(
		const_cast<FSeqImageSectionUI*>(this)->GetSectionObject());
	if (!ImageSection || ImageSection->Image.IsNull())
	{
		return LayerId;
	}

	// Load once so the thumbnail shows before the section is first evaluated
	UTexture2D* Texture = ImageSection->Image.Get();
	if (!Texture)
	{
		Texture = ImageSection->Image.LoadSynchronous();
	}
	if (!Texture)
	{
		return LayerId;
	}

	// Thumbnail at the right end of the bar (skipped when the section is too short)
	const FVector2f SectionSize = FVector2f(Painter.SectionGeometry.GetLocalSize());
	const float     ThumbH      = FMath::Min(SectionSize.Y - 4.f, 36.f);
	const FIntPoint Imported    = Texture->GetImportedSize();
	const float     Aspect      = (Imported.X > 0 && Imported.Y > 0) ? static_cast<float>(Imported.X) / Imported.Y : 1.f;
	const float     ThumbW      = ThumbH * Aspect;
	if (ThumbH < 4.f || ThumbW + 8.f > SectionSize.X * 0.5f)
	{
		return LayerId;
	}

	ThumbnailBrush = FSlateBrush();
	ThumbnailBrush.SetResourceObject(Texture);
	ThumbnailBrush.ImageSize = FVector2D(ThumbW, ThumbH);
	ThumbnailBrush.DrawAs    = ESlateBrushDrawType::Image;

	FSlateDrawElement::MakeBox(
		Painter.DrawElements,
		++LayerId,
		Painter.SectionGeometry.ToPaintGeometry(
			FVector2f(ThumbW, ThumbH),
			FSlateLayoutTransform(FVector2f(SectionSize.X - ThumbW - 4.f, (SectionSize.Y - ThumbH) * 0.5f))),
		&ThumbnailBrush,
		ESlateDrawEffect::None,
		FLinearColor(1.f, 1.f, 1.f, Painter.GhostAlpha));

	return LayerId;
}

#if ENGINE_MINOR_VERSION >= 7
float FSeqImageSectionUI::GetSectionHeight(const UE::Sequencer::FViewDensityInfo& ViewDensity) const
#else
float FSeqImageSectionUI::GetSectionHeight() const
#endif
{
	// Taller than the subtitle bar so the thumbnail is readable
	return 40.0f;
}

FText FSeqImageSectionUI::GetSectionTitle() const
{
	const UMovieSceneSeqImageSection* ImageSection = Cast<UMovieSceneSeqImageSection>(
		const_cast<FSeqImageSectionUI*>(this)->GetSectionObject());
	if (ImageSection && !ImageSection->Image.IsNull())
	{
		return FText::FromString(ImageSection->Image.GetAssetName());
	}
	return LOCTEXT("NoImage", "(No Image)");
}

// --- FSeqImageTrackEditor ---

TSharedRef<ISequencerTrackEditor> FSeqImageTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer)
{
	return MakeShareable(new FSeqImageTrackEditor(OwningSequencer));
}

FSeqImageTrackEditor::FSeqImageTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{
}

#if ENGINE_MINOR_VERSION >= 6
FText FSeqImageTrackEditor::GetDisplayName() const
{
	return LOCTEXT("TrackDisplayName", "Image Track");
}
#endif

bool FSeqImageTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const
{
	return TrackClass == UMovieSceneSeqImageTrack::StaticClass();
}

TSharedRef<ISequencerSection> FSeqImageTrackEditor::MakeSectionInterface(
	UMovieSceneSection& SectionObject,
	UMovieSceneTrack& Track,
	FGuid ObjectBinding)
{
	return MakeShared<FSeqImageSectionUI>(SectionObject);
}

void FSeqImageTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddImageTrack", "Image Track"),
		LOCTEXT("AddImageTrackTooltip", "Add a track that shows images on screen with entrance / exit effects"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"),
		FUIAction(FExecuteAction::CreateSP(this, &FSeqImageTrackEditor::HandleAddImageTrack))
	);
}

TSharedPtr<SWidget> FSeqImageTrackEditor::BuildOutlinerEditWidget(
	const FGuid& ObjectBinding,
	UMovieSceneTrack* Track,
	const FBuildEditWidgetParams& Params)
{
	if (!Cast<UMovieSceneSeqImageTrack>(Track))
	{
		return TSharedPtr<SWidget>();
	}

	TWeakObjectPtr<UMovieSceneTrack> WeakTrack = Track;

	// --- "+" Add Section button (same style as the subtitle track) ---
#if ENGINE_MINOR_VERSION >= 7
	return UE::Sequencer::MakeAddButton(
		LOCTEXT("AddImageSection", "Image"),
		FOnClicked::CreateLambda([this, WeakTrack]() -> FReply
		{
			if (UMovieSceneTrack* TrackPtr = WeakTrack.Get())
			{
				AddNewSectionToTrack(TrackPtr);
			}
			return FReply::Handled();
		}),
		Params.ViewModel);
#else
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "FlatButton")
		.ContentPadding(FMargin(2, 0))
		.OnClicked_Lambda([this, WeakTrack]() -> FReply
		{
			if (UMovieSceneTrack* TrackPtr = WeakTrack.Get())
			{
				AddNewSectionToTrack(TrackPtr);
			}
			return FReply::Handled();
		})
		.ToolTipText(LOCTEXT("AddImageSectionTooltip", "Add a new image section"))
		[
			SNew(STextBlock)
			.TextStyle(FAppStyle::Get(), "NormalText.Important")
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			.Text(LOCTEXT("AddImageSectionButton", "+ Section"))
		];
#endif
}

UMovieSceneSeqImageSection* FSeqImageTrackEditor::AddNewSectionToTrack(UMovieSceneTrack* Track, UTexture2D* Texture)
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (!SequencerPtr.IsValid() || !Track || !FocusedMovieScene || FocusedMovieScene->IsReadOnly())
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddImageSection_Transaction", "Add Image Section"));
	FocusedMovieScene->Modify();
	Track->Modify();

	UMovieSceneSeqImageSection* NewSection = Cast<UMovieSceneSeqImageSection>(Track->CreateNewSection());
	if (!NewSection)
	{
		return nullptr;
	}
	if (Texture)
	{
		NewSection->Image = Texture;
	}

	// Start at the playhead with the default subtitle section duration
	const FFrameNumber CurrentTime    = SequencerPtr->GetLocalTime().Time.GetFrame();
	const FFrameRate   TickResolution = FocusedMovieScene->GetTickResolution();
	const USubtitleSettings* Settings = GetDefault<USubtitleSettings>();
	const float DefaultDuration = Settings ? Settings->DefaultSectionDuration : 3.0f;
	const FFrameNumber Duration = (static_cast<double>(DefaultDuration) * TickResolution).FloorToFrame();
	NewSection->SetRange(TRange<FFrameNumber>(CurrentTime, CurrentTime + Duration));

	int32 OverlapPriority = 0;
	for (UMovieSceneSection* Section : Track->GetAllSections())
	{
		OverlapPriority = FMath::Max(Section->GetOverlapPriority() + 1, OverlapPriority);
	}
	NewSection->SetOverlapPriority(OverlapPriority);

	Track->AddSection(*NewSection);

	SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	SequencerPtr->EmptySelection();
	SequencerPtr->SelectSection(NewSection);
	SequencerPtr->ThrobSectionSelection();
	return NewSection;
}

bool FSeqImageTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	// Only textures dropped on the sequence itself (not on an actor binding)
	UTexture2D* Texture = Cast<UTexture2D>(Asset);
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (!Texture || TargetObjectGuid.IsValid() || !FocusedMovieScene || FocusedMovieScene->IsReadOnly())
	{
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("DropImage_Transaction", "Add Image Section"));
	FocusedMovieScene->Modify();

	// Use the first Image Track, or create one
	UMovieSceneSeqImageTrack* ImageTrack = nullptr;
	for (UMovieSceneTrack* Track : FocusedMovieScene->GetTracks())
	{
		ImageTrack = Cast<UMovieSceneSeqImageTrack>(Track);
		if (ImageTrack) { break; }
	}
	if (!ImageTrack)
	{
		ImageTrack = FocusedMovieScene->AddTrack<UMovieSceneSeqImageTrack>();
	}

	return AddNewSectionToTrack(ImageTrack, Texture) != nullptr;
}

void FSeqImageTrackEditor::HandleAddImageTrack()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (!FocusedMovieScene || FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddImageTrack_Transaction", "Add Image Track"));
	FocusedMovieScene->Modify();

	UMovieSceneSeqImageTrack* NewTrack = FocusedMovieScene->AddTrack<UMovieSceneSeqImageTrack>();
	if (!NewTrack)
	{
		return;
	}

	// One section at the start of the playback range
	if (UMovieSceneSection* NewSection = NewTrack->CreateNewSection())
	{
		if (FocusedMovieScene->GetPlaybackRange().HasLowerBound())
		{
			const FFrameNumber Start = FocusedMovieScene->GetPlaybackRange().GetLowerBoundValue();
			const FFrameRate TickResolution = FocusedMovieScene->GetTickResolution();
			const USubtitleSettings* Settings = GetDefault<USubtitleSettings>();
			const float DefaultDuration = Settings ? Settings->DefaultSectionDuration : 3.0f;
			const FFrameNumber Duration = (static_cast<double>(DefaultDuration) * TickResolution).FloorToFrame();
			NewSection->SetRange(TRange<FFrameNumber>(Start, Start + Duration));
		}
		NewTrack->AddSection(*NewSection);
	}

	if (TSharedPtr<ISequencer> SequencerPtr = GetSequencer())
	{
		SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
}

#undef LOCTEXT_NAMESPACE
