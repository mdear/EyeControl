#include "OVControls.h"
#include "OVManager.h"
using namespace Gdiplus;

const int MIN_DELAY = 10;
const int DEC_DELAY = 10;

OVButton::OVButton(OVManager* mgr, std::string name, int top, int bottom, int left, int right)
{
	this->mgr = mgr;
	this->active = 0;
	this->name = name;
	this->corner_x = left;
	this->corner_y = top;
	this->center_x = (left + right) / 2;
	this->center_y = (top + bottom) / 2;
	this->rect = RECT();
	this->rect.bottom = bottom + 5;
	this->rect.top = top - 5;
	this->rect.left = left - 5;
	this->rect.right = right + 5;
	this->height = bottom - top;
	this->width = right - left;
	this->mode = Tap;
	this->toggle = false;
	this->pchange = "";
	this->odelay = 0;
	this->delay = 0;
	this->prevTime = 0;
	this->timeFocus = 0;
	this->timeToggle = 0;
	this->timeFlash = -1;
	this->flipToggle = 0;
	this->dblTimer = -1;
}

void OVButton::setActions(int delay, OVMode mode, bool toggle, std::string pchange, std::vector<CKey> keys)
{
	if (mode == TapAccel)
		int x = 5;
	this->delay = delay;
	this->odelay = delay;
	this->mode = mode;
	this->toggle = toggle;
	this->pchange = pchange;
	this->keys = keys;
}

void OVButton::update()
{
	int time = clock();
	bool draw = false;
	if (timeFlash >= 0)
	{
		timeFlash--;
		runActions();
	}
	if (mode == Double)
	{
		if (dblTimer >= 0) dblTimer--;
		if (dblTimer == 0) timeFlash = 5;
	}
	if (timeFlash == 0) draw = true;
	if (toggle)	//toggle has special behavior
	{
		if (OVManager::gazepos_x > corner_x && OVManager::gazepos_x < corner_x + width && OVManager::gazepos_y > corner_y && OVManager::gazepos_y < corner_y + height)
		{
			if (flipToggle <= 0)	//you have to look away for some frames before it will flip again after switching states
			{
				switch (active)
				{
				case 0:
					active = 1;
					draw = true;
					timeFocus = time + delay;
					break;
				case 1:
					if (time > timeFocus)
					{
						active = 2;
						draw = true;
						flipToggle = 10;
					}
					break;
				case 2:
					active = 3;
					draw = true;
					timeFocus = time + delay;
					break;
				case 3:
					if (time > timeFocus)
					{
						active = 0;
						draw = true;
						flipToggle = 10;
					}
					break;
				}
			}
		}
		else
		{
			switch (active)
			{
			case 0:
				break;
			case 1:
				timeFocus += OVManager::gazeDecay;
				if (timeFocus > time + delay)
				{
					active = 0;	//reverse back to inactive
					draw = true;
				}
				break;
			case 2:
				break;
			case 3:
				timeFocus += OVManager::gazeDecay;
				if (timeFocus > time + delay)
				{
					active = 2;	//reverse back to active
					draw = true;
				}
				break;
			}
			if (flipToggle > 0) flipToggle--;	//reduce timer until flipping enabled again
		}

		if (active > 1 && (time > timeToggle || mode == Hold))	//2 and 3 are both active states
		{
			timeToggle = time + delay;
			runActions();
			timeFlash = 5;
		}
	}
	else	//non-toggle behavior
	{
		if (active == 2 && mode != Hold)	//if not held down active phase ends after 1 frame
		{
			active = 0;
			if (this->mode == TapAccel && delay > MIN_DELAY)
				delay -= DEC_DELAY;	//reduces delay every firing
		}
		if (OVManager::gazepos_x > corner_x && OVManager::gazepos_x < corner_x + width && OVManager::gazepos_y > corner_y && OVManager::gazepos_y < corner_y + height)
		{
			switch (active)
			{
			case 0:
				active = 1;
				draw = true;
				timeFocus = time + delay;
				break;
			case 1:
				if (time > timeFocus)
				{
					active = 2;
					draw = true;
				}
				break;
			case 2:
				break;
			}
		}
		else
		{
			switch (active)
			{
			case 0:
				break;
			case 1:
				timeFocus += OVManager::gazeDecay;
				if (timeFocus > time + delay)
				{
					active = 0;
					if (this->mode == TapAccel) delay = odelay;
					draw = true;
				}
				break;
			case 2:
				timeFocus += OVManager::gazeDecay;
				if (timeFocus > time + delay)
				{
					active = 0;
				}
				break;
			}
		}

		if (active == 2)
		{
			timeFlash = 5;
			if (mode == Double) dblTimer = 12;
		}
	}
	if (draw) OVManager::drawRect(this->rect);
}

void OVButton::runActions()
{
	for (size_t i = 0; i < keys.size(); i++)
	{
		if (keys[i] == CK_QUIT) mgr->quit();	//quit function
		else ControlDriver::pressKey(keys[i]);	//push buttons
	}
	if (this->pchange != "") mgr->changePanel(this->pchange);
}

void OVButton::draw(Graphics* g)
{
	Color clr;
	if (timeFlash > 0)
	{
		clr = OVManager::active_color;
	}
	else if (active > 0) clr = OVManager::focus_color;
	else clr = OVManager::inactive_color;
	SolidBrush  brush(clr);
	FontFamily  fontFamily(L"Times New Roman");
	Font        font(&fontFamily, 24, FontStyleRegular, UnitPixel);
	PointF      pointF(center_x, center_y);
	RectF rectF = RectF(corner_x, corner_y, width, height);

	StringFormat format = new StringFormat();
	format.SetLineAlignment(StringAlignmentCenter);
	format.SetAlignment(StringAlignmentCenter);
	std::wstring widename = std::wstring(name.begin(), name.end());
	const WCHAR * wname = widename.c_str();
	g->DrawString(wname, -1, &font, rectF, &format, &brush);

	Pen pen(clr);
	pen.SetWidth(5);
	g->DrawRectangle(&pen, corner_x, corner_y, width, height);
	g->DrawRectangle(&pen, rectF);
}

OVDial::OVDial(OVManager* mgr, int top, int bottom, int left, int right, int stick)
{
	this->mgr = mgr;
	this->active = 0;
	this->corner_x = left;
	this->corner_y = top;
	this->center_x = (left + right) / 2;
	this->center_y = (top + bottom) / 2;
	this->rect = RECT();
	this->rect.bottom = bottom + 5;
	this->rect.top = top - 5;
	this->rect.left = left - 5;
	this->rect.right = right + 5;
	this->height = bottom - top;
	this->width = right - left;
	this->stick = stick;
}

void OVDial::update()
{
	bool draw = false;
	if (OVManager::gazepos_x > corner_x && OVManager::gazepos_x < corner_x + width && OVManager::gazepos_y > corner_y && OVManager::gazepos_y < corner_y + height)	//basic rectangle check before more expensive distance checks
	{
		float vec_x = (OVManager::gazepos_x - center_x) * 2.0 / (float)width;	//calculate position relative to unit circle version of dial
		float vec_y = (OVManager::gazepos_y - center_y) * 2.0 / (float)height;
		if (sqrtf(vec_x * vec_x + vec_y * vec_y) <= 1.0f)
		{
			if (active == 0) draw = true;
			active = 1;
			ControlDriver::moveStick(stick, vec_x, vec_y);
		}
		else
		{
			if (active == 1) draw = true;
			active = 0;
			ControlDriver::moveStick(stick, 0.0f, 0.0f);
		}
	}
	else
	{
		if (active == 1) draw = true;
		active = 0;
		ControlDriver::moveStick(stick, 0.0f, 0.0f);
	}
	if (draw) OVManager::drawRect(this->rect);
}

void OVDial::draw(Graphics* g)
{
	Color clr;
	switch (active)
	{
	case 0: clr = OVManager::inactive_color; break;
	case 1: clr = OVManager::focus_color; break;
	case 2: clr = OVManager::active_color; break;
	}
	Pen pen(clr);
	pen.SetWidth(5);
	g->DrawEllipse(&pen, corner_x, corner_y, width, height);
	pen.SetWidth(2);
	g->DrawEllipse(&pen, corner_x + width / 8, corner_y + height / 8, width * 3/ 4, height * 3 / 4);
	g->DrawEllipse(&pen, corner_x + width / 4, corner_y + height / 4, width / 2, height / 2);
	g->DrawEllipse(&pen, corner_x + width * 3 / 8, corner_y + height * 3 / 8, width / 4, height / 4);
	pen.SetWidth(1);
	g->DrawLine(&pen, corner_x, center_y, corner_x + width, center_y);
	g->DrawLine(&pen, center_x, corner_y, center_x, corner_y + height);
	g->DrawLine(&pen, center_x - width * 0.3536f, center_y - height * 0.3536f, center_x + width * 0.3536f, center_y + height * 0.3536f);
	g->DrawLine(&pen, center_x - width * 0.3536f, center_y + height * 0.3536f, center_x + width * 0.3536f, center_y - height * 0.3536f);
}

OVPanel::OVPanel()
{
	controls = std::vector<OVControl*>();
	drawNow = true;
}

OVPanel::~OVPanel()
{
	for (size_t i = 0; i < controls.size(); ++i)
	{
		delete controls[i];
	}
}

void OVPanel::add(OVControl* ctrl)
{
	controls.push_back(ctrl);
}

void OVPanel::update()
{
	if (drawNow)	//flag for redrawing frame immediately once, used on panel load
	{
		OVManager::redrawAll = true;
		drawNow = false;
	}
	for (size_t i = 0; i < controls.size(); ++i)
	{
		controls[i]->update();
	}
}

void OVPanel::draw(Graphics* g)
{
	for (size_t i = 0; i < controls.size(); ++i)
	{
		controls[i]->draw(g);
	}
}

void OVPanel::reset()
{
	drawNow = true;
	for (size_t i = 0; i < controls.size(); ++i)
	{
		controls[i]->reset();
	}
}
