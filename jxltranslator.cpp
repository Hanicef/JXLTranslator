/*
 * Copyright 2021, Craig Watson <watsoncraigjohn@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "jxltranslator.h"

#include <Alignment.h>
#include <Catalog.h>
#include <Translator.h>
#include <TranslatorFormats.h>
#include <TranslationDefs.h>
#include <syslog.h>

#include <jxl/decode.h>
#include <jxl/encode.h>
#include <jxl/thread_parallel_runner.h>

#include "configview.h"
#include "TranslatorSettings.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "JXLTranslator"

static const translation_format sInputFormats[] = {
	{ B_TRANSLATOR_BITMAP, B_TRANSLATOR_BITMAP, BBT_IN_QUALITY, BBT_IN_CAPABILITY,
		"image/x-be-bitmap", "Be Bitmap Format (JXLTranslator)" },
	{ JXL_FORMAT, B_TRANSLATOR_BITMAP, JXL_IN_QUALITY, JXL_IN_CAPABILITY,
		"image/jxl", "JPEG-XL Image" }
};

static const translation_format sOutputFormats[] = {
	{ B_TRANSLATOR_BITMAP, B_TRANSLATOR_BITMAP, BBT_OUT_QUALITY, BBT_OUT_CAPABILITY,
		"image/x-be-bitmap", "Be Bitmap Format (JXLTranslator)" },
	{ JXL_FORMAT, B_TRANSLATOR_BITMAP, JXL_OUT_QUALITY, JXL_OUT_CAPABILITY,
		"image/jxl", "JPEG-XL Image" }
};

static const TranSetting sDefaultSettings[] = {
	{JXL_SETTING_DISTANCE, TRAN_SETTING_INT32, JXL_DEFAULT_DISTANCE},
	{JXL_SETTING_EFFORT, TRAN_SETTING_INT32, JXL_DEFAULT_EFFORT}	
};

const uint32 kNumInputFormats = sizeof(sInputFormats) / sizeof(translation_format);
const uint32 kNumOutputFormats = sizeof(sOutputFormats) / sizeof(translation_format);
const uint32 kNumDefaultSettings = sizeof(sDefaultSettings) / sizeof(TranSetting);


JXLTranslator::JXLTranslator()
	: BaseTranslator(B_TRANSLATE("JPEG-XL images"),
		B_TRANSLATE("JPEG-XL image translator"),
		JXL_TRANSLATOR_VERSION,
		sInputFormats, kNumInputFormats,
		sOutputFormats, kNumOutputFormats,
		JXL_TRANSLATOR_SETTINGS,
		sDefaultSettings, kNumDefaultSettings,
		B_TRANSLATOR_BITMAP, JXL_FORMAT)
{
}

JXLTranslator::~JXLTranslator()
{
}

status_t
JXLTranslator::IdentifyJXL(BPositionIO *inSource, translator_info *outInfo)
{
	off_t position;
	position = inSource->Position();
	char header[sizeof(TranslatorBitmap)];
	status_t err = inSource->Read(header, sizeof(TranslatorBitmap));
	inSource->Seek(position, SEEK_SET);
	if (err < B_OK)
		return err;
		
	if (B_BENDIAN_TO_HOST_INT32(((TranslatorBitmap *)header)->magic) == B_TRANSLATOR_BITMAP)
	{
		if (PopulateInfoFromFormat(outInfo, B_TRANSLATOR_BITMAP) != B_OK)
			return B_NO_TRANSLATOR;	
	}
	else
	{
		// First two bytes should be 255 and 10
		if (header[0] == (char)0xff && header[1] == char(0x0a))
		{
			if (PopulateInfoFromFormat(outInfo, JXL_FORMAT) != B_OK)
			{
				syslog(LOG_ERR, "Couldn't populate info\n");
				return B_NO_TRANSLATOR;
			}
		}
		else {
			return B_NO_TRANSLATOR;
		}		
	}
	return B_OK;
}

status_t
JXLTranslator::DerivedIdentify(BPositionIO* inSource, const translation_format* inFormat, BMessage* ioExtension, translator_info * outInfo, uint32 outType)
{
	return IdentifyJXL(inSource, outInfo);	
}

status_t
JxlMemoryToPixels(const uint8_t *next_in, size_t size, size_t *stride,
                           size_t *xsize, size_t *ysize, int *has_alpha, uint8 *& pixels) {
  JxlDecoder *dec = JxlDecoderCreate(NULL);
  if (!dec) {
    syslog(LOG_ERR, "JxlDecoderCreate failed\n");
    return B_ERROR;
  }
  void * runner = JxlThreadParallelRunnerCreate(NULL, JxlThreadParallelRunnerDefaultNumWorkerThreads());
  if (JXL_DEC_SUCCESS !=
      JxlDecoderSetParallelRunner(dec, JxlThreadParallelRunner, runner)) {
    syslog(LOG_ERR, "JxlDecoderSetParallelRunner failed\n");
    JxlThreadParallelRunnerDestroy(runner);
    JxlDecoderDestroy(dec);
    return B_ERROR;
  }
  *has_alpha = 1; //we always create RGBA32 currently, see format below
  if (JXL_DEC_SUCCESS !=
      JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE)) {
    syslog(LOG_ERR, "JxlDecoderSubscribeEvents failed\n");
    JxlThreadParallelRunnerDestroy(runner);
    JxlDecoderDestroy(dec);
    return B_ERROR;
  }

  JxlBasicInfo info;
  int success = 0;
  JxlPixelFormat format = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
  JxlDecoderSetInput(dec, next_in, size);

  for (;;) {
    JxlDecoderStatus status = JxlDecoderProcessInput(dec);

    if (status == JXL_DEC_ERROR) {
      syslog(LOG_ERR, "Decoder error\n");
      break;
    } else if (status == JXL_DEC_NEED_MORE_INPUT) {
      syslog(LOG_ERR, "Error, already provided all input\n");
      break;
    } else if (status == JXL_DEC_BASIC_INFO) {
      if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec, &info)) {
        syslog(LOG_ERR, "JxlDecoderGetBasicInfo failed\n");
        break;
      }
      *xsize = info.xsize;
      *ysize = info.ysize;
      *stride = info.xsize * 4;
    } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
      size_t buffer_size;
      if (JXL_DEC_SUCCESS !=
          JxlDecoderImageOutBufferSize(dec, &format, &buffer_size)) {
        syslog(LOG_ERR, "JxlDecoderImageOutBufferSize failed\n");
        break;
      }
      if (buffer_size != *stride * *ysize) {
        syslog(LOG_ERR, "Invalid out buffer size %zu %zu\n", buffer_size, *stride * *ysize);
        break;
      }
      size_t pixels_buffer_size = buffer_size * sizeof(uint8_t);
      pixels = (uint8*)malloc(pixels_buffer_size);
      void *pixels_buffer = (void *)pixels;
      if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec, &format,
                                                         pixels_buffer,
                                                         pixels_buffer_size)) {
        syslog(LOG_ERR, "JxlDecoderSetImageOutBuffer failed\n");
        break;
      }
    } else if (status == JXL_DEC_FULL_IMAGE) {
      // This means the decoder has decoded all pixels into the buffer.
      success = 1;
      break;
    } else if (status == JXL_DEC_SUCCESS) {
      syslog(LOG_ERR, "Decoding finished before receiving pixel data\n");
      break;
    } else {
      syslog(LOG_ERR, "Unexpected decoder status: %d\n", status);
      break;
    }
  }
  JxlThreadParallelRunnerDestroy(runner);
  JxlDecoderDestroy(dec);

  if (success){
    return B_OK;
  } else {
    free(pixels);
    return B_ERROR;
  }
}

status_t
JXLTranslator::BitmapPixelsToJxl(uint8* pixels, size_t size, int xsize, int ysize, uint32 bpp, int alphabits, uint32 align, BPositionIO* out)
{
	JxlEncoder *enc = JxlEncoderCreate(NULL);
	void * runner = JxlThreadParallelRunnerCreate(NULL, JxlThreadParallelRunnerDefaultNumWorkerThreads());
	
	if (JXL_ENC_SUCCESS != JxlEncoderSetParallelRunner(enc, JxlThreadParallelRunner, runner))
	{
		syslog(LOG_ERR, "JxlEncoderSetParallelRunner failed\n");
		JxlThreadParallelRunnerDestroy(runner);
		JxlEncoderDestroy(enc);
		return B_ERROR;	
	}
	JxlPixelFormat pixel_format = {bpp, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, align};
	
	JxlBasicInfo basic_info;
	basic_info.xsize = xsize;
	basic_info.ysize = ysize;
	basic_info.bits_per_sample = 8;
	basic_info.exponent_bits_per_sample = 0; //not floating point
	basic_info.num_color_channels = bpp == 1 ? 1 : 3;
	basic_info.alpha_bits = alphabits;
	basic_info.alpha_exponent_bits = 0;
	basic_info.uses_original_profile = JXL_FALSE;
	
	if (JXL_ENC_SUCCESS != JxlEncoderSetBasicInfo(enc, &basic_info))
	{
		syslog(LOG_ERR, "JxlEncoderSetBasicInfo failed\n");	
		JxlThreadParallelRunnerDestroy(runner);
		JxlEncoderDestroy(enc);
		return B_ERROR;
	}
	
	JxlEncoderOptions * options =JxlEncoderOptionsCreate(enc, NULL);
	JxlEncoderOptionsSetEffort(options, fSettings->SetGetInt32(JXL_SETTING_EFFORT));
	JxlEncoderOptionsSetDistance(options, (float)fSettings->SetGetInt32(JXL_SETTING_DISTANCE));
	JxlColorEncoding color_encoding;
	JxlColorEncodingSetToSRGB(&color_encoding, bpp == 1);
	
	if (JXL_ENC_SUCCESS != JxlEncoderSetColorEncoding(enc, &color_encoding))
	{
		syslog(LOG_ERR, "JxlEncoderSetColorEncoding failed\n");	
		JxlThreadParallelRunnerDestroy(runner);
		JxlEncoderDestroy(enc);
		return B_ERROR;			
	}
	
	if (JXL_ENC_SUCCESS != 
		JxlEncoderAddImageFrame(options, &pixel_format, (void*)pixels, size))
	{
		syslog(LOG_ERR, "JxlEncoderAddImageFrame failed\n");
		JxlThreadParallelRunnerDestroy(runner);
		JxlEncoderDestroy(enc);
		return B_ERROR;			
	}
	size_t written;
	size_t outSize = 64 * sizeof(uint8);
	uint8 *output = (uint8*)malloc(outSize);

	uint8 * next_output = output;
	size_t availOut = outSize - (next_output - output);
	JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
	while (process_result == JXL_ENC_NEED_MORE_OUTPUT)
	{
		process_result = JxlEncoderProcessOutput(enc, &next_output, &availOut);
		if (process_result == JXL_ENC_NEED_MORE_OUTPUT)
		{
			next_output = output;
			availOut = outSize;
			written = out->Write(output, outSize);
			if (written < B_OK) 
			{
				syslog(LOG_ERR, "Data write failed %d\n", written);
				free(output);
				return written;
			}
			if (written != outSize)
			{
				syslog(LOG_ERR, "Data write IO Error\n");					
				free(output);
				return B_IO_ERROR;
			}
		}
	}
	outSize = next_output - output;
	written = out->Write(output, outSize);
	if (written < B_OK) 
	{
		syslog(LOG_ERR, "Data write failed %d\n", written);
		free(output);
		return written;
	}
	if (written != outSize)
	{
		syslog(LOG_ERR, "Data write IO Error\n");					
		free(output);
		return B_IO_ERROR;
	}
	free(output);
	if (JXL_ENC_SUCCESS != process_result)
	{
		JxlThreadParallelRunnerDestroy(runner);
		JxlEncoderDestroy(enc);
	    syslog(LOG_ERR,"JxlEncoderProcessOutput failed\n");
	    return B_ERROR;			
	}
	JxlThreadParallelRunnerDestroy(runner);
	JxlEncoderDestroy(enc);
	return B_OK;
}

status_t 
JXLTranslator::Decompress(BPositionIO* in, BPositionIO* out)
{
	uint8_t * convertedData = NULL;
	size_t xsize, ysize, stride;
 	int has_alpha;
 	off_t inSize = in->Seek(0, SEEK_END);
 	in->Seek(0, SEEK_SET);
	
	void * inData = malloc(inSize);
	if (inData == NULL) {
		syslog(LOG_ERR, "Couldn't malloc in space\n");
		return B_NO_MEMORY;
	}
		
	if (in->Read(inData, inSize) != inSize)
	{
		syslog(LOG_ERR, "Couldn't read in data\n");
		free(inData);
		return B_IO_ERROR;	
	}
	
	status_t err = JxlMemoryToPixels((uint8_t*)inData, inSize, &stride, &xsize, &ysize, &has_alpha, convertedData);
	free(inData); // not needed now
	if (err != B_OK) return err;
	if (convertedData == NULL)
	{
		syslog(LOG_ERR, "Invalid pointer returned\n");
		return B_ILLEGAL_DATA;	
	}
	BRect bounds(0, 0, xsize - 1, ysize - 1);
	color_space outColorSpace = B_RGBA32;
	size_t outSize = stride * ysize;

	TranslatorBitmap header;
	header.magic = B_HOST_TO_BENDIAN_INT32(B_TRANSLATOR_BITMAP);
	header.bounds.left = B_HOST_TO_BENDIAN_FLOAT(bounds.left);
	header.bounds.top = B_HOST_TO_BENDIAN_FLOAT(bounds.top);
	header.bounds.right = B_HOST_TO_BENDIAN_FLOAT(bounds.right);
	header.bounds.bottom = B_HOST_TO_BENDIAN_FLOAT(bounds.bottom);
	header.colors = (color_space)B_HOST_TO_BENDIAN_INT32(outColorSpace);
	header.rowBytes = B_HOST_TO_BENDIAN_INT32(stride);
	header.dataSize = B_HOST_TO_BENDIAN_INT32(outSize);
	err = out->Write(&header, sizeof(TranslatorBitmap));
	if (err < B_OK || err < (int)sizeof(TranslatorBitmap))
		return B_IO_ERROR;

	//write data from convertedData
	size_t written;
	written = out->Write(convertedData, outSize);
	if (written < B_OK) 
	{
		syslog(LOG_ERR, "Data write failed %d\n", err);
		free(convertedData);
		return err;
	}
	if (written != outSize)
	{
		syslog(LOG_ERR, "Data write IO Error\n");					
		free(convertedData);
		return B_IO_ERROR;
	}
	
	free(convertedData);
	return B_OK;
}


status_t 
JXLTranslator::Compress(BPositionIO* in, BPositionIO* out)
{
 	off_t inSize = in->Seek(0, SEEK_END);
 	in->Seek(0, SEEK_SET);
	
	inSize -= sizeof(TranslatorBitmap);
	void * inData = malloc(inSize);
	if (inData == NULL) {
		syslog(LOG_ERR, "Couldn't malloc in space\n");
		return B_NO_MEMORY;
	}
		
	in->Seek(sizeof(TranslatorBitmap), SEEK_SET);
	if (in->Read(inData, inSize) != inSize)
	{
		syslog(LOG_ERR, "Couldn't read in data\n");
		free(inData);
		return B_IO_ERROR;	
	}
	in->Seek(0, SEEK_SET);
	TranslatorBitmap bmpHeader;
	status_t err = identify_bits_header(in, NULL, &bmpHeader);
	if (err != B_OK)
	{
		syslog(LOG_ERR, "Error identifying bitmap: %d\n", err);	
	}
	//get bpp
	uint32 bytesPerPixel = 0;
	int32 alphaBits = 0;
	switch (bmpHeader.colors) {
		case B_RGB32:
		case B_RGB32_BIG:
		case B_RGBA32:
		case B_RGBA32_BIG:
			bytesPerPixel = 4;
			alphaBits = 8;
			break;
		case B_RGB24:
		case B_RGB24_BIG:
			bytesPerPixel = 3;
			break;
		case B_RGB16:
		case B_RGB16_BIG:
		case B_RGB15:
		case B_RGB15_BIG:
			bytesPerPixel = 2;
			break;
		case B_RGBA15: // need alpha
		case B_RGBA15_BIG:
			bytesPerPixel = 2;
			alphaBits = 1;
			break;
		case B_GRAY8:
			bytesPerPixel = 1;
			break;
		default:
			return B_NO_TRANSLATOR;	
	}
	
	//encoding now
	err = BitmapPixelsToJxl((uint8*)inData, inSize, bmpHeader.bounds.IntegerWidth()+1, bmpHeader.bounds.IntegerHeight()+1, bytesPerPixel, alphaBits, 0, out);
	free(inData);
	return err;
}


status_t
JXLTranslator::DerivedTranslate(BPositionIO* inSource,
	const translator_info* inInfo, BMessage* ioExtension, uint32 outType,
	BPositionIO* outDestination, int32 baseType)
{
	if (outType == inInfo->type)
	{
		translate_direct_copy(inSource, outDestination);	
		return B_OK;
	} else if (outType == JXL_FORMAT && inInfo->type == B_TRANSLATOR_BITMAP)
	{
		return Compress(inSource, outDestination);	
	}
	else if (outType == B_TRANSLATOR_BITMAP && inInfo->type == JXL_FORMAT)
	{
		return Decompress(inSource, outDestination);	
	}
	return B_NO_TRANSLATOR;
}

BView *
JXLTranslator::NewConfigView(TranslatorSettings *settings)
{
	return new ConfigView(settings);	
}

/*! have the other PopulateInfoFromFormat() check both inputFormats & outputFormats */
status_t
JXLTranslator::PopulateInfoFromFormat(translator_info* info,
	uint32 formatType, translator_id id)
{
	int32 formatCount;
	const translation_format* formats = OutputFormats(&formatCount);
	for (int i = 0; i <= 1 ;formats = InputFormats(&formatCount), i++) {
		if (PopulateInfoFromFormat(info, formatType,
			formats, formatCount) == B_OK) {
			info->translator = id;
			return B_OK;
		}
	}

	return B_ERROR;
}


status_t
JXLTranslator::PopulateInfoFromFormat(translator_info* info,
	uint32 formatType, const translation_format* formats, int32 formatCount)
{
	for (int i = 0; i < formatCount; i++) {
		if (formats[i].type == formatType) {
			info->type = formatType;
			info->group = formats[i].group;
			info->quality = formats[i].quality;
			info->capability = formats[i].capability;
			BString str1(formats[i].name);
			str1.ReplaceFirst("Be Bitmap Format (JXLTranslator)", 
				B_TRANSLATE("Be Bitmap Format (JXLTranslator)"));
			strlcpy(info->name, str1.String(), sizeof(info->name));
			strcpy(info->MIME,  formats[i].MIME);
			return B_OK;
		}
	}

	return B_ERROR;
}

BTranslator*
make_nth_translator(int32 n, image_id you, uint32 flags, ...)
{
	if (n == 0)
		return new JXLTranslator();

	return NULL;
}
