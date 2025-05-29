/*
 * Copyright 2021, Craig Watson <watsoncraigjohn@gmail.com>
 * Copyright 2025, Gustaf "Hanicef" Alhäll <gustaf@hanicef.me>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef JXLTRANSLATOR_H
#define JXLTRANSLATOR_H

#include "BaseTranslator.h"
#include <TranslationKit.h>
#include <TranslatorAddOn.h>

#define JXL_TRANSLATOR_VERSION B_TRANSLATION_MAKE_VERSION(0,1,0)
#define JXL_FORMAT 'JXL '
#define JXL_TRANSLATOR_SETTINGS "JXLTranslatorSettings"

#define JXL_IN_QUALITY 0.7
#define JXL_IN_CAPABILITY 0.8
#define JXL_OUT_QUALITY 0.7
#define JXL_OUT_CAPABILITY 0.6

#define BBT_IN_QUALITY 0.7
#define BBT_IN_CAPABILITY 0.6
#define BBT_OUT_QUALITY 0.7
#define BBT_OUT_CAPABILITY 0.6

#define JXL_SETTING_DISTANCE "JXL_SETTING_DISTANCE"
#define JXL_SETTING_EFFORT "JXL_SETTING_EFFORT"
#define JXL_DEFAULT_DISTANCE 1 // visually lossless, 0-15 higher = worse
#define JXL_DEFAULT_EFFORT 7 // 3-9 higher = slower

class JXLTranslator : public BaseTranslator {
public:
						JXLTranslator(void);
	virtual status_t	DerivedIdentify(BPositionIO* inSource, const translation_format* inFormat, BMessage* ioExtension, translator_info * outInfo, uint32 outType);
	virtual status_t	DerivedTranslate(BPositionIO* inSource, const translator_info *inInfo, BMessage* ioExtension, uint32 outType, BPositionIO* outDestination, int32 baseType);
	virtual BView*		NewConfigView(TranslatorSettings* settings);

protected:
	virtual ~JXLTranslator(void);

private:
	status_t IdentifyJXL(BPositionIO *inSource, translator_info *outInfo);
	status_t Decompress(BPositionIO* in, BPositionIO* out);
	status_t Compress(BPositionIO* in, BPositionIO* out);
	status_t BitmapPixelsToJxl(uint8* pixels, size_t size, int xsize, int ysize, uint32 bpp,
				int alphabits, uint32 align, BPositionIO* out);
};


#endif
