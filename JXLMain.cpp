/*
 * Copyright 2021, Craig Watson <watsoncraigjohn@gmail.com>
 * Copyright 2025, Gustaf "Hanicef" Alhäll <gustaf@hanicef.me>
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include <Application.h>
#include <Alert.h>
#include <Catalog.h>
#include <GroupLayout.h>
#include <Screen.h>

#include "jxltranslator.h"
#include "configview.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "JXLTranslator"


int
main()
{
	BApplication app("application/x-vnd.Haiku-JXLTranslator");
	status_t result;
	JXLTranslator *translator = new JXLTranslator();
	BRect rect(0, 0, JXL_VIEW_WIDTH, JXL_VIEW_HEIGHT);
	BView *view = NULL;
	if (translator->MakeConfigurationView(NULL, &view, &rect)) {
		BAlert *err = new BAlert(B_TRANSLATE("Error"),
			B_TRANSLATE("Unable to create the view."), B_TRANSLATE("OK"));
		err->SetFlags(err->Flags() | B_CLOSE_ON_ESCAPE);
		err->Go();
		return 1;
	}
	// release the translator even though I never really used it anyway
	translator->Release();
	translator = NULL;

	BWindow *wnd = new BWindow(rect, B_TRANSLATE("JPEG-XL Settings"),
		B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS);
	wnd->SetLayout(new BGroupLayout(B_HORIZONTAL));
	wnd->AddChild(view);
	BPoint wndpt = B_ORIGIN;
	{
		BScreen scrn;
		BRect frame = scrn.Frame();
		frame.InsetBy(10, 23);
		// if the point is outside of the screen frame,
		// use the mouse location to find a better point
		if (!frame.Contains(wndpt)) {
			uint32 dummy;
			view->GetMouse(&wndpt, &dummy, false);
			wndpt.x -= rect.Width() / 2;
			wndpt.y -= rect.Height() / 2;
			// clamp location to screen
			if (wndpt.x < frame.left)
				wndpt.x = frame.left;
			if (wndpt.y < frame.top)
				wndpt.y = frame.top;
			if (wndpt.x > frame.right)
				wndpt.x = frame.right;
			if (wndpt.y > frame.bottom)
				wndpt.y = frame.bottom;
		}
	}
	wnd->MoveTo(wndpt);
	wnd->Show();
	
	app.Run();
	return 0;
}
