#pragma once
#include <vector>

enum CKey
{
	CK_A,
	CK_B,
	CK_X,
	CK_Y,
	CK_D_up,
	CK_D_down,
	CK_D_left,
	CK_D_right,
	CK_Start,
	CK_Select,
	CK_L1,
	CK_L2,
	CK_L3,
	CK_R1,
	CK_R2,
	CK_R3,
	CK_Home,
	CK_L_up,
	CK_L_down,
	CK_L_left,
	CK_L_right,
	CK_R_up,
	CK_R_down,
	CK_R_left,
	CK_R_right,
	CK_0,
	CK_1,
	CK_2,
	CK_3,
	CK_4,
	CK_5,
	CK_6,
	CK_7,
	CK_8,
	CK_9,
	CK_PLUS,
	CK_MINUS,
	CK_QUIT
};

class ControlDriver
{
public:
	ControlDriver();
	~ControlDriver();	//performs exit code
	static bool Init();	//initializes driver, returns false if failure
	static void Update();	//updates at end of frame
	static void pressKey(CKey key);	//presses a given key
	static void moveStick(int stick, float x, float y);	//moves the given stick to a position
private:
	static std::vector<bool> keys;	//all keys pressed in a frame
	//static long keys;
	static std::vector<std::pair<float, float>> sticks;	//where the sticks are in a frame
};