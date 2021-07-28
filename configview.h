/*
 * Copyright 2021, Craig Watson <watsoncraigjohn@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef CONFIGVIEW_H
#define CONFIGVIEW_H

#include <GroupView.h>
#include <Slider.h>

#include "TranslatorSettings.h"


class ConfigView : public BGroupView {
public:
					ConfigView(TranslatorSettings *settings);
	virtual 		~ConfigView();
	virtual void	AttachedToWindow(void);	
	virtual void	MessageReceived(BMessage *message);
	
private:
	TranslatorSettings *fSettings;
	BSlider * fDistanceSlider;
	BSlider * fEffortSlider;
};


#endif // CONFIGVIEW_H
