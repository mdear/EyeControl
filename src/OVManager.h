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
	static void drawRect(RECT r) { OVManager::rdraw.push_back(r); }
	static Gdiplus::Color inactive_color;
	static Gdiplus::Color focus_color;
	static Gdiplus::Color active_color;
	static int gazepos_x;
	static int gazepos_y;
	static bool gaze_valid;
	static bool redrawAll;
	static int gazeDecay;	//set to amount of decay each frame
private:
	bool tobii_failure;	//flips to true if tobii screws up, causes program to immediately proceed to end
	std::vector<OVPanel*> panels;
	int curr_panel;
	int resolution_x;
	int resolution_y;
	float decayRate;
	int offset_x;	//in case of screen issues, offset calculations can be made
	int offset_y;
	bool b_quit;	//signals to quit at end of loop
	HWND* hWnd;	//window instance
	static const int framerate = 60;	//set this to desired framerate of system
	static std::vector<RECT> rdraw;
	int frametime;	//time value for next frame
	int pframe;	//previous frame value
	int numframes;
	int last_x;
	int last_y;
	int maxspeed;	//maximum distance eye can cover in pixels for data to be valid
	int deadframes;	//number of frames invalid data has been read for
};