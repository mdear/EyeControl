#pragma once

#include <time.h>
#include "OVControls.h"
#include <vector>
#include "tobii\tobii.h"
#include "tobii\tobii_streams.h"
#include "rapidjson\document.h"
#include <fstream>
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

class OVManager
{
public:
	OVManager() {};
	OVManager(HWND* hWnd);
	~OVManager();
	bool loop();	//runs an iteration of the software loop (after checking framerate), returns true if continuing, false if not
	void draw(HDC hdc);
	void changePanel(std::string name);	//change to the specified panel
	void load();	//loads the configuration file into the controls
	void quit() { this->b_quit = true; }	//trigger for buttons to signal quit
	static Gdiplus::Color inactive_color;
	static Gdiplus::Color focus_color;
	static Gdiplus::Color active_color;
	static int gazepos_x;
	static int gazepos_y;
	static bool gaze_valid;
private:
	bool tobii_failure;	//flips to true if tobii screws up, causes program to immediately proceed to end
	std::vector<OVPanel*> panels;
	int curr_panel;
	int resolution_x;
	int resolution_y;
	int offset_x;	//in case of screen issues, offset calculations can be made
	int offset_y;
	bool b_quit;	//signals to quit at end of loop
	HWND* hWnd;	//window instance
	static const int framerate = 60;	//set this to desired framerate of system
	int frametime;	//time value for next frame
	int numframes;
};