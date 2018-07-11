#pragma once
#include "ControlDriver.h"
#include <string>
#include <vector>
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

class OVManager;

enum OVMode
{
	Tap,
	Hold,
	Double,
	TapAccel
};

class OVControl	//main function for all control objects
{
public:
	virtual void update() = 0;	//update function, returns true if a redraw is needed
	virtual void draw(Graphics* g) = 0;	//draw function, passed down from GDI paint
	void reset() { active = 0; timeFlash = -1; }	//used to prevent panel switch from leaving buttons on and such
protected:
	OVManager* mgr;
	int center_x;
	int center_y;
	int corner_x;
	int corner_y;
	RECT rect;
	int width;
	int height;
	int active;
	int timeFlash;//number of frames to draw activation 'lit up' for button
};

class OVButton : public OVControl
{
public:
	OVButton(OVManager* mgr, std::string name, int top, int bottom, int left, int right);
	void setActions(int delay, OVMode mode, bool toggle, std::string pchange, std::vector<CKey> keys);
	void update();
	void runActions();
	void draw(Graphics* g);
private:
	std::string name;
	OVMode mode;
	bool toggle;
	std::string pchange;
	std::vector<CKey> keys;
	int delay;
	int prevTime;	//amount of time since last update
	int timeFocus;	//amount of time eye is focused
	int timeToggle;	//used for recurring signals in toggle
	int flipToggle;	//makes it so you don't go back and forth by just staring
};

class OVDial : public OVControl
{
public:
	OVDial(OVManager* mgr, int top, int bottom, int left, int right, int stick);
	void update();
	void draw(Graphics* g);
private:
	int stick;
};

class OVPanel
{
public:
	OVPanel();
	~OVPanel();
	void add(OVControl* ctrl);
	void update();
	void draw(Graphics* g);
	void reset();
	std::string name;
private:
	bool drawNow;
	std::vector<OVControl*> controls;
};