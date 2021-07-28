/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file null_v.cpp The video driver that doesn't blit. */

#include "../stdafx.h"
#include "../console_func.h"
#include "../date_func.h"
#include "../gfx_func.h"
#include "../blitter/factory.hpp"
#include "../saveload/saveload.h"
#include "../window_func.h"
#include "null_v.h"

#include "../safeguards.h"

/** Factory for the null video driver. */
static FVideoDriver_Null iFVideoDriver_Null;

const char *VideoDriver_Null::Start(const StringList &parm)
{
#ifdef _MSC_VER
	/* Disable the MSVC assertion message box. */
	_set_error_mode(_OUT_TO_STDERR);
#endif

	this->UpdateAutoResolution();

	this->ticks = GetDriverParamInt(parm, "ticks", 1000);
	this->savefile = GetDriverParam(parm, "save");
	_screen.width  = _screen.pitch = _cur_resolution.width;
	_screen.height = _cur_resolution.height;
	_screen.dst_ptr = nullptr;
	ScreenSizeChanged();

	/* Do not render, nor blit */
	DEBUG(misc, 1, "Forcing blitter 'null'...");
	BlitterFactory::SelectBlitter("null");
	return nullptr;
}

void VideoDriver_Null::Stop() { }

void VideoDriver_Null::MakeDirty(int left, int top, int width, int height) {}

void VideoDriver_Null::MainLoop()
{
	uint i;

	uint16 old_tick;
	for (i = 0; i < this->ticks; ) {
		old_tick = _tick_counter;
		::GameLoop();
		::InputLoop();
		::UpdateWindows();
		if (old_tick != _tick_counter) i++;
		else _pause_mode = PM_UNPAUSED;
	}
	fprintf(stderr, "Null driver ran for %u tics, save: %s\n", this->ticks, this->savefile.c_str());
	if (!this->savefile.empty()) {
	    if (SaveOrLoad(this->savefile.c_str(), SLO_SAVE, DFT_GAME_FILE, SAVE_DIR) != SL_OK) {
	        IConsolePrintF(CC_ERROR, "Error saving the final game state.");
	    } else {
	        IConsolePrintF(CC_DEFAULT, "Saved the final game state to %s", this->savefile.c_str());
	    }
	}
}

bool VideoDriver_Null::ChangeResolution(int w, int h) { return false; }

bool VideoDriver_Null::ToggleFullscreen(bool fs) { return false; }
