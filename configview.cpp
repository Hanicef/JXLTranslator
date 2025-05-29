/*
 * Copyright 2021, Craig Watson <watsoncraigjohn@gmail.com>
 * Copyright 2025, Gustaf "Hanicef" Alhäll <gustaf@hanicef.me>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "configview.h"

#include <Catalog.h>
#include <LayoutBuilder.h>
#include <stdio.h>
#include <StringView.h>

#include <jxl/encode.h>
#include "jxltranslator.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ConfigView"

#define BMSG_DISTANCE 'jdst'
#define BMSG_EFFORT 'jeff'


ConfigView::ConfigView(TranslatorSettings *settings)
	: BGroupView(B_TRANSLATE("JPEG-XL Translator Settings"), B_VERTICAL),
	fSettings(settings)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	
	BStringView * title = new BStringView("Title", "JPEG-XL Images");
	title->SetFont(be_bold_font);
	title->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_TOP));
	
	char versionString[255];
	sprintf(versionString, "v%d.%d.%d, %s",
		B_TRANSLATION_MAJOR_VERSION(JXL_TRANSLATOR_VERSION),
		B_TRANSLATION_MINOR_VERSION(JXL_TRANSLATOR_VERSION),
		B_TRANSLATION_REVISION_VERSION(JXL_TRANSLATOR_VERSION),
		__DATE__);
	
	BStringView * version = new BStringView("version", versionString);
	
	BStringView * copyright = new BStringView("copyright", "©2021, Craig Watson");
	
	char libjxlVersionString[255];
	uint32 jxlver = JxlEncoderVersion();
	sprintf(libjxlVersionString, "libjxl v%d.%d.%d",
		jxlver / 1000000, 
		(jxlver/1000) %1000,
		jxlver %1000);

	BStringView * basedon = new BStringView("based on", "Based on JXL Library © The JPEG XL Project Authors");
	BStringView * jxlversion = new BStringView("jxlversion", libjxlVersionString);
	
	fDistanceSlider = new BSlider("distance", B_TRANSLATE("Max Butteraugli distance:"),
		new BMessage(BMSG_DISTANCE), 0, 15, B_HORIZONTAL, B_BLOCK_THUMB);
	fDistanceSlider->SetHashMarks(B_HASH_MARKS_BOTTOM);
	fDistanceSlider->SetHashMarkCount(15);
	fDistanceSlider->SetLimitLabels(B_TRANSLATE("Lossless"),B_TRANSLATE("Lossy"));
	fDistanceSlider->SetValue(fSettings->SetGetInt32(JXL_SETTING_DISTANCE));
	
	fEffortSlider = new BSlider("effort", B_TRANSLATE("Encoding effort:"),
		new BMessage(BMSG_EFFORT), 3, 9, B_HORIZONTAL, B_BLOCK_THUMB);
	fEffortSlider->SetHashMarks(B_HASH_MARKS_BOTTOM);
	fEffortSlider->SetHashMarkCount(7);
	fEffortSlider->SetLimitLabels(B_TRANSLATE("Faster"),B_TRANSLATE("Slower"));
	fEffortSlider->SetValue(fSettings->SetGetInt32(JXL_SETTING_EFFORT));
	
	
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.Add(title)
		.Add(version)
		.Add(copyright)
		.AddGlue()
		.Add(fDistanceSlider)
		.Add(fEffortSlider)
		.AddGlue()
		.Add(basedon)
		.Add(jxlversion);
	
	BFont font;
	GetFont(&font);
	SetExplicitPreferredSize(BSize((font.Size() * 300) / 12, (font.Size()) * 350 / 12));
}


ConfigView::~ConfigView()
{
	fSettings->Release();
}

void
ConfigView::AttachedToWindow()
{
	BGroupView::AttachedToWindow();
	fDistanceSlider->SetTarget(this);
	fEffortSlider->SetTarget(this);
	
	if (Parent() == NULL && Window()->GetLayout() == NULL)
	{
		Window()->SetLayout(new BGroupLayout(B_VERTICAL));
		Window()->ResizeTo(PreferredSize().Width(), PreferredSize().Height());	
	}	
}


void
ConfigView::MessageReceived(BMessage* message)
{
	switch(message->what)
	{
		case BMSG_DISTANCE:
		{
			int32 value;
			if (message->FindInt32("be:value", &value) == B_OK)
			{
				fSettings->SetGetInt32(JXL_SETTING_DISTANCE, &value);
				fSettings->SaveSettings();
			}
			break;
		}
		case BMSG_EFFORT:
		{
			int32 value;
			if (message->FindInt32("be:value", &value) == B_OK)
			{
				fSettings->SetGetInt32(JXL_SETTING_EFFORT, &value);
				fSettings->SaveSettings();
			}
			break;
		}
		default:
			BGroupView::MessageReceived(message);
	}	
}
