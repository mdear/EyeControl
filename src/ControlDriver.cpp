#include "ControlDriver.h"
#include "stdafx.h"
#include "public.h"
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include "vjoyinterface.h"
#include "Math.h"

#define DEV_ID		1

// Prototypes
void  CALLBACK FfbFunction(PVOID data);
void  CALLBACK FfbFunction1(PVOID cb, PVOID data);

BOOL PacketType2Str(FFBPType Type, LPTSTR Str);
BOOL EffectType2Str(FFBEType Ctrl, LPTSTR Str);
BOOL DevCtrl2Str(FFB_CTRL Type, LPTSTR Str);
BOOL EffectOpStr(FFBOP Op, LPTSTR Str);
int  Polar2Deg(BYTE Polar);
int  Byte2Percent(BYTE InByte);
int TwosCompByte2Int(BYTE in);


static int ffb_direction = 0;
static int ffb_strenght = 0;
static int serial_result = 0;

static JOYSTICK_POSITION_V2 iReport; // The structure that holds the full position data

static UINT DevID;
static PVOID pPositionMessage;

std::vector<bool> ControlDriver::keys;
//LONG ControlDriver::keys;
std::vector<std::pair<float, float>> ControlDriver::sticks;

ControlDriver::ControlDriver()
{
}

ControlDriver::~ControlDriver()
{
}

bool ControlDriver::Init()
{
	int stat = 0;
	DevID = DEV_ID;
	
	ControlDriver::keys = std::vector<bool>(37);	//array to hold one of each input
	//ControlDriver::keys = 0;
	ControlDriver::sticks = std::vector<std::pair<float, float>>();
	ControlDriver::sticks.push_back(std::pair<float, float>(0.0f, 0.0f));
	ControlDriver::sticks.push_back(std::pair<float, float>(0.0f, 0.0f));


	UINT	IoCode = LOAD_POSITIONS;
	UINT	IoSize = sizeof(JOYSTICK_POSITION);
	// HID_DEVICE_ATTRIBUTES attrib;
	BYTE id = 1;
	UINT iInterface = 1;

	// Define the effect names
	static FFBEType FfbEffect = (FFBEType)-1;
	LPCTSTR FfbEffectName[] =
	{ "NONE", "Constant Force", "Ramp", "Square", "Sine", "Triangle", "Sawtooth Up",\
		"Sawtooth Down", "Spring", "Damper", "Inertia", "Friction", "Custom Force" };

	// Get the driver attributes (Vendor ID, Product ID, Version Number)
	if (!vJoyEnabled())
	{
		_tprintf("Function vJoyEnabled Failed - make sure that vJoy is installed and enabled\n");
		int dummy = getchar();
		stat = -2;
		return false;
	}
	else
	{
		wprintf(L"Vendor: %s\nProduct :%s\nVersion Number:%s\n", static_cast<TCHAR *> (GetvJoyManufacturerString()), static_cast<TCHAR *>(GetvJoyProductString()), static_cast<TCHAR *>(GetvJoySerialNumberString()));
	};

	// Get the status of the vJoy device before trying to acquire it
	VjdStat status = GetVJDStatus(DevID);

	switch (status)
	{
	case VJD_STAT_OWN:
		_tprintf("vJoy device %d is already owned by this feeder\n", DevID);
		break;
	case VJD_STAT_FREE:
		_tprintf("vJoy device %d is free\n", DevID);
		break;
	case VJD_STAT_BUSY:
		_tprintf("vJoy device %d is already owned by another feeder\nCannot continue\n", DevID);
		MessageBox(NULL, "Device already owned by another feeder.", "vJoy Failure", MB_OK);
		return false;
	case VJD_STAT_MISS:
		_tprintf("vJoy device %d is not installed or disabled\nCannot continue\n", DevID);
		MessageBox(NULL, "Device not installed or disabled.", "vJoy Failure", MB_OK);
		return false;
	default:
		_tprintf("vJoy device %d general error\nCannot continue\n", DevID);
		MessageBox(NULL, "General error.", "vJoy Failure", MB_OK);
		return false;
	};

	// Acquire the vJoy device
	if (!AcquireVJD(DevID))
	{
		_tprintf("Failed to acquire vJoy device number %d.\n", DevID);
		MessageBox(NULL, "Failed to acquire device.", "vJoy Failure", MB_OK);
		int dummy = getchar();
		stat = -1;
		return false;
	}
	else
		_tprintf("Acquired device number %d - OK\n", DevID);



	// Start FFB
#if 1
	BOOL Ffbstarted = FfbStart(DevID);
	if (!Ffbstarted)
	{
		_tprintf("Failed to start FFB on vJoy device number %d.\n", DevID);
		MessageBox(NULL, "Failed to start FFB.", "vJoy Failure", MB_OK);
		int dummy = getchar();
		stat = -3;
		return false;
	}
	else
		_tprintf("Started FFB on vJoy device number %d - OK\n", DevID);

#endif // 1

	// Register Generic callback function
	// At this point you instruct the Receptor which callback function to call with every FFB packet it receives
	// It is the role of the designer to register the right FFB callback function
	FfbRegisterGenCB(FfbFunction1, NULL);
	return true;
}

void ControlDriver::Update()
{
	BYTE id = (BYTE)DevID;
	iReport.bDevice = id;
	iReport.wAxisX = 16384 + 16000 * sticks[0].first;
	iReport.wAxisY = 16384 + 16000 * sticks[0].second;
	iReport.wAxisXRot = 16384 + 16000 * sticks[1].first;
	iReport.wAxisYRot = 16384 + 16000 * sticks[1].second;
	sticks[0] = std::pair<float, float>(0.0f, 0.0f);
	sticks[1] = std::pair<float, float>(0.0f, 0.0f);

	
	LONG btn = 0;
	for (size_t i = 0; i < ControlDriver::keys.size(); ++i)
	{
		if (keys[i])
		{
			keys[i] = false;
			if (i < CK_L_up)	btn = btn | (1 << i);
			else if (i < CK_0)
			{
				switch (i)	//directional key overrides
				{
				case CK_L_up: iReport.wAxisY = 384; break;
				case CK_L_down: iReport.wAxisY = 32384; break;
				case CK_L_left: iReport.wAxisX = 384; break;
				case CK_L_right: iReport.wAxisX = 32384; break;
				case CK_R_up: iReport.wAxisYRot = 384; break;
				case CK_R_down: iReport.wAxisYRot = 32384; break;
				case CK_R_left: iReport.wAxisXRot = 384; break;
				case CK_R_right: iReport.wAxisXRot = 32384; break;
				}
			}
			else if(i < CK_QUIT)
			{
				INPUT ip;
				ip.type = INPUT_KEYBOARD;

				switch (i)
				{
				case CK_0: ip.ki.wVk = 0x30; break;
				case CK_1: ip.ki.wVk = 0x31; break;
				case CK_2: ip.ki.wVk = 0x32; break;
				case CK_3: ip.ki.wVk = 0x33; break;
				case CK_4: ip.ki.wVk = 0x34; break;
				case CK_5: ip.ki.wVk = 0x35; break;
				case CK_6: ip.ki.wVk = 0x36; break;
				case CK_7: ip.ki.wVk = 0x37; break;
				case CK_8: ip.ki.wVk = 0x38; break;
				case CK_9: ip.ki.wVk = 0x39; break;
				case CK_ZOOM_IN: {
					ip.type = INPUT_MOUSE;
					ip.mi.dwFlags = MOUSEEVENTF_WHEEL;
					ip.mi.mouseData = WHEEL_DELTA;
					break;
					}
				case CK_ZOOM_OUT: {
					ip.type = INPUT_MOUSE;
					ip.mi.dwFlags = MOUSEEVENTF_WHEEL;
					ip.mi.mouseData = -WHEEL_DELTA;
					break;
					}
				}
				if (ip.type == INPUT_KEYBOARD) {
					ip.ki.wScan = 0;
					ip.ki.time = 0;
					ip.ki.dwExtraInfo = 0;
					ip.ki.dwFlags = 0;
					UINT s1 = SendInput(1, &ip, sizeof(INPUT));
					DWORD err1 = GetLastError();
					UINT s2 = ip.ki.dwFlags = KEYEVENTF_KEYUP;
					SendInput(1, &ip, sizeof(INPUT));
					DWORD err2 = GetLastError();
					ip.ki.dwFlags = 0;
				}
				if (ip.type == INPUT_MOUSE) {
					ip.mi.dx = 0;
					ip.mi.dy = 0;
					ip.mi.time = 0;
					ip.mi.dwExtraInfo = 0;
					UINT s1 = SendInput(1, &ip, sizeof(INPUT));
					if (s1 != 1) {
						DWORD err1 = GetLastError();
						_tprintf("Failed to send mouse event %d, return %d, err %d.\n", ip.mi.mouseData, s1, err1);
						/*MessageBox(NULL, "Failed to send mouse wheel event.", "Mouse wheel Failure", MB_OK);*/
					}
				}
			}
		}
	} 
	iReport.lButtons = btn;
	//iReport.lButtons = ControlDriver::keys;
	//ControlDriver::keys = 0;

	pPositionMessage = (PVOID)(&iReport);
	if (!UpdateVJD(DevID, pPositionMessage))
	{
		printf("Feeding vJoy device number %d failed - try to enable device then press enter\n", DevID);
		getchar();
		AcquireVJD(DevID);
	}
}

void ControlDriver::pressKey(CKey key)
{
	ControlDriver::keys[key] = true;	//each button press held for several frames
	//ControlDriver::keys = ControlDriver::keys | (1 << key);
}

void ControlDriver::moveStick(int stick, float x, float y)
{
	ControlDriver::sticks[stick] = std::pair<float, float>(x, y);
}





// Generic callback function
void CALLBACK FfbFunction(PVOID data)
{
	FFB_DATA * FfbData = (FFB_DATA *)data;
	int size = FfbData->size;
	_tprintf("\nFFB Size %d\n", size);

	_tprintf("Cmd:%08.8X ", FfbData->cmd);
	_tprintf("ID:%02.2X ", FfbData->data[0]);
	_tprintf("Size:%02.2d ", static_cast<int>(FfbData->size - 8));
	_tprintf(" - ");
	for (UINT i = 0; i < FfbData->size - 8; i++)
		_tprintf(" %02.2X", (UINT)FfbData->data);
	_tprintf("\n");
}

void CALLBACK FfbFunction1(PVOID data, PVOID userdata)
{
	// Packet Header
	_tprintf("\n ============= FFB Packet size Size %d =============\n", static_cast<int>(((FFB_DATA *)data)->size));

	/////// Packet Device ID, and Type Block Index (if exists)
#pragma region Packet Device ID, and Type Block Index
	int DeviceID, BlockIndex;
	FFBPType	Type;
	TCHAR	TypeStr[100];

	if (ERROR_SUCCESS == Ffb_h_DeviceID((FFB_DATA *)data, &DeviceID))
		_tprintf("\n > Device ID: %d", DeviceID);
	if (ERROR_SUCCESS == Ffb_h_Type((FFB_DATA *)data, &Type))
	{
		if (!PacketType2Str(Type, TypeStr))
			_tprintf("\n > Packet Type: %d", Type);
		else
			_tprintf("\n > Packet Type: %s", TypeStr);

	}
	if (ERROR_SUCCESS == Ffb_h_EBI((FFB_DATA *)data, &BlockIndex))
		_tprintf("\n > Effect Block Index: %d", BlockIndex);
#pragma endregion


	/////// Effect Report
#pragma region Effect Report
	FFB_EFF_CONST Effect;
	if (ERROR_SUCCESS == Ffb_h_Eff_Report((FFB_DATA *)data, &Effect))
	{
		if (!EffectType2Str(Effect.EffectType, TypeStr))
			_tprintf("\n >> Effect Report: %02x", Effect.EffectType);
		else
			_tprintf("\n >> Effect Report: %s", TypeStr);

		if (Effect.Polar)
		{
			_tprintf("\n >> Direction: %d deg (%02x)", Polar2Deg(Effect.Direction), Effect.Direction);


		}
		else
		{
			_tprintf("\n >> X Direction: %02x", Effect.DirX);
			_tprintf("\n >> Y Direction: %02x", Effect.DirY);
		};

		if (Effect.Duration == 0xFFFF)
			_tprintf("\n >> Duration: Infinit");
		else
			_tprintf("\n >> Duration: %d MilliSec", static_cast<int>(Effect.Duration));

		if (Effect.TrigerRpt == 0xFFFF)
			_tprintf("\n >> Trigger Repeat: Infinit");
		else
			_tprintf("\n >> Trigger Repeat: %d", static_cast<int>(Effect.TrigerRpt));

		if (Effect.SamplePrd == 0xFFFF)
			_tprintf("\n >> Sample Period: Infinit");
		else
			_tprintf("\n >> Sample Period: %d", static_cast<int>(Effect.SamplePrd));


		_tprintf("\n >> Gain: %d%%", Byte2Percent(Effect.Gain));

	};
#pragma endregion
#pragma region PID Device Control
	FFB_CTRL	Control;
	TCHAR	CtrlStr[100];
	if (ERROR_SUCCESS == Ffb_h_DevCtrl((FFB_DATA *)data, &Control) && DevCtrl2Str(Control, CtrlStr))
		_tprintf("\n >> PID Device Control: %s", CtrlStr);

#pragma endregion
#pragma region Effect Operation
	FFB_EFF_OP	Operation;
	TCHAR	EffOpStr[100];
	if (ERROR_SUCCESS == Ffb_h_EffOp((FFB_DATA *)data, &Operation) && EffectOpStr(Operation.EffectOp, EffOpStr))
	{
		_tprintf("\n >> Effect Operation: %s", EffOpStr);
		if (Operation.LoopCount == 0xFF)
			_tprintf("\n >> Loop until stopped");
		else
			_tprintf("\n >> Loop %d times", static_cast<int>(Operation.LoopCount));

	};
#pragma endregion
#pragma region Global Device Gain
	BYTE Gain;
	if (ERROR_SUCCESS == Ffb_h_DevGain((FFB_DATA *)data, &Gain))
		_tprintf("\n >> Global Device Gain: %d", Byte2Percent(Gain));

#pragma endregion
#pragma region Condition
	FFB_EFF_COND Condition;
	if (ERROR_SUCCESS == Ffb_h_Eff_Cond((FFB_DATA *)data, &Condition))
	{
		if (Condition.isY)
			_tprintf("\n >> Y Axis");
		else
			_tprintf("\n >> X Axis");
		_tprintf("\n >> Center Point Offset: %d", TwosCompByte2Int(Condition.CenterPointOffset) * 10000 / 127);
		_tprintf("\n >> Positive Coefficient: %d", TwosCompByte2Int(Condition.PosCoeff) * 10000 / 127);
		_tprintf("\n >> Negative Coefficient: %d", TwosCompByte2Int(Condition.NegCoeff) * 10000 / 127);
		_tprintf("\n >> Positive Saturation: %d", Condition.PosSatur * 10000 / 255);
		_tprintf("\n >> Negative Saturation: %d", Condition.NegSatur * 10000 / 255);
		_tprintf("\n >> Dead Band: %d", Condition.DeadBand * 10000 / 255);
	}
#pragma endregion
#pragma region Envelope
	FFB_EFF_ENVLP Envelope;
	if (ERROR_SUCCESS == Ffb_h_Eff_Envlp((FFB_DATA *)data, &Envelope))
	{
		_tprintf("\n >> Attack Level: %d", Envelope.AttackLevel * 10000 / 255);
		_tprintf("\n >> Fade Level: %d", Envelope.FadeLevel * 10000 / 255);
		_tprintf("\n >> Attack Time: %d", static_cast<int>(Envelope.AttackTime));
		_tprintf("\n >> Fade Time: %d", static_cast<int>(Envelope.FadeTime));
	};

#pragma endregion
#pragma region Periodic
	FFB_EFF_PERIOD EffPrd;
	if (ERROR_SUCCESS == Ffb_h_Eff_Period((FFB_DATA *)data, &EffPrd))
	{
		_tprintf("\n >> Magnitude: %d", EffPrd.Magnitude * 10000 / 255);
		_tprintf("\n >> Offset: %d", TwosCompByte2Int(EffPrd.Offset) * 10000 / 127);
		_tprintf("\n >> Phase: %d", EffPrd.Phase * 3600 / 255);
		_tprintf("\n >> Period: %d", static_cast<int>(EffPrd.Period));
	};
#pragma endregion

#pragma region Effect Type
	FFBEType EffectType;
	if (ERROR_SUCCESS == Ffb_h_EffNew((FFB_DATA *)data, &EffectType))
	{
		if (EffectType2Str(EffectType, TypeStr))
			_tprintf("\n >> Effect Type: %s", TypeStr);
		else
			_tprintf("\n >> Effect Type: Unknown");
	}

#pragma endregion

#pragma region Ramp Effect
	FFB_EFF_RAMP RampEffect;
	if (ERROR_SUCCESS == Ffb_h_Eff_Ramp((FFB_DATA *)data, &RampEffect))
	{
		_tprintf("\n >> Ramp Start: %d", TwosCompByte2Int(RampEffect.Start) * 10000 / 127);
		_tprintf("\n >> Ramp End: %d", TwosCompByte2Int(RampEffect.End) * 10000 / 127);
	};

#pragma endregion

	_tprintf("\n");
	FfbFunction(data);
	_tprintf("\n ====================================================\n");

}


// Convert Packet type to String
BOOL PacketType2Str(FFBPType Type, LPTSTR OutStr)
{
	BOOL stat = TRUE;
	LPTSTR Str = "";

	switch (Type)
	{
	case PT_EFFREP:
		Str = "Effect Report";
		break;
	case PT_ENVREP:
		Str = "Envelope Report";
		break;
	case PT_CONDREP:
		Str = "Condition Report";
		break;
	case PT_PRIDREP:
		Str = "Periodic Report";
		break;
	case PT_CONSTREP:
		Str = "Constant Force Report";
		break;
	case PT_RAMPREP:
		Str = "Ramp Force Report";
		break;
	case PT_CSTMREP:
		Str = "Custom Force Data Report";
		break;
	case PT_SMPLREP:
		Str = "Download Force Sample";
		break;
	case PT_EFOPREP:
		Str = "Effect Operation Report";
		break;
	case PT_BLKFRREP:
		Str = "PID Block Free Report";
		break;
	case PT_CTRLREP:
		Str = "PID Device Contro";
		break;
	case PT_GAINREP:
		Str = "Device Gain Report";
		break;
	case PT_SETCREP:
		Str = "Set Custom Force Report";
		break;
	case PT_NEWEFREP:
		Str = "Create New Effect Report";
		break;
	case PT_BLKLDREP:
		Str = "Block Load Report";
		break;
	case PT_POOLREP:
		Str = "PID Pool Report";
		break;
	default:
		stat = FALSE;
		break;
	}

	if (stat)
		_tcscpy_s(OutStr, 100, Str);

	return stat;
}

// Convert Effect type to String
BOOL EffectType2Str(FFBEType Type, LPTSTR OutStr)
{
	BOOL stat = TRUE;
	LPTSTR Str = "";

	switch (Type)
	{
	case ET_NONE:
		stat = FALSE;
		break;
	case ET_CONST:
		Str = "Constant Force";
		break;
	case ET_RAMP:
		Str = "Ramp";
		break;
	case ET_SQR:
		Str = "Square";
		break;
	case ET_SINE:
		Str = "Sine";
		break;
	case ET_TRNGL:
		Str = "Triangle";
		break;
	case ET_STUP:
		Str = "Sawtooth Up";
		break;
	case ET_STDN:
		Str = "Sawtooth Down";
		break;
	case ET_SPRNG:
		Str = "Spring";
		break;
	case ET_DMPR:
		Str = "Damper";
		break;
	case ET_INRT:
		Str = "Inertia";
		break;
	case ET_FRCTN:
		Str = "Friction";
		break;
	case ET_CSTM:
		Str = "Custom Force";
		break;
	default:
		stat = FALSE;
		break;
	};

	if (stat)
		_tcscpy_s(OutStr, 100, Str);

	return stat;
}

// Convert PID Device Control to String
BOOL DevCtrl2Str(FFB_CTRL Ctrl, LPTSTR OutStr)
{
	BOOL stat = TRUE;
	LPTSTR Str = "";

	switch (Ctrl)
	{
	case CTRL_ENACT:
		Str = "Enable Actuators";
		break;
	case CTRL_DISACT:
		Str = "Disable Actuators";
		break;
	case CTRL_STOPALL:
		Str = "Stop All Effects";
		break;
	case CTRL_DEVRST:
		Str = "Device Reset";
		break;
	case CTRL_DEVPAUSE:
		Str = "Device Pause";
		break;
	case CTRL_DEVCONT:
		Str = "Device Continue";
		break;
	default:
		stat = FALSE;
		break;
	}
	if (stat)
		_tcscpy_s(OutStr, 100, Str);

	return stat;
}

// Convert Effect operation to string
BOOL EffectOpStr(FFBOP Op, LPTSTR OutStr)
{
	BOOL stat = TRUE;
	LPTSTR Str = "";

	switch (Op)
	{
	case EFF_START:
		Str = "Effect Start";
		break;
	case EFF_SOLO:
		Str = "Effect Solo Start";
		break;
	case EFF_STOP:
		Str = "Effect Stop";
		break;
	default:
		stat = FALSE;
		break;
	}

	if (stat)
		_tcscpy_s(OutStr, 100, Str);

	return stat;
}

// Polar values (0x00-0xFF) to Degrees (0-360)
int Polar2Deg(BYTE Polar)
{
	return ((UINT)Polar * 360) / 255;
}

// Convert range 0x00-0xFF to 0%-100%
int Byte2Percent(BYTE InByte)
{
	return ((UINT)InByte * 100) / 255;
}

// Convert One-Byte 2's complement input to integer
int TwosCompByte2Int(BYTE in)
{
	int tmp;
	BYTE inv = ~in;
	BOOL isNeg = in >> 7;
	if (isNeg)
	{
		tmp = (int)(inv);
		tmp = -1 * tmp;
		return tmp;
	}
	else
		return (int)in;
}
