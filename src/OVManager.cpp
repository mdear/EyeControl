#include "OVManager.h"
using namespace Gdiplus;

//----------
//TOBII FUNCTIONS, adapted from stream engine example code
//----------

static void gaze_callback(tobii_gaze_point_t const* gaze_point, void* user_data)
{
	// Store the latest gaze point data in the supplied storage
	tobii_gaze_point_t* gaze_point_storage = (tobii_gaze_point_t*)user_data;
	*gaze_point_storage = *gaze_point;
}

struct url_receiver_context_t
{
	char** urls;
	int capacity;
	int count;
};

static void url_receiver(char const* url, void* user_data)
{
	// The memory context is passed through the user_data void pointer
	struct url_receiver_context_t* context = (struct url_receiver_context_t*) user_data;
	// Allocate more memory if maximum capacity has been reached
	if (context->count >= context->capacity)
	{
		context->capacity *= 2;
		char** urls = (char**)realloc(context->urls, sizeof(char*) * context->capacity);
		if (!urls)
		{
			fprintf(stderr, "Allocation failed\n");
			return;
		}
		context->urls = urls;
	}
	// Copy the url string input parameter to allocated memory
	size_t url_length = strlen(url) + 1;
	context->urls[context->count] = (char*)malloc(url_length);	//TODO: verify functionality
	if (!context->urls[context->count])
	{
		fprintf(stderr, "Allocation failed\n");
		return;
	}
	memcpy(context->urls[context->count++], url, url_length);
}


struct device_list_t
{
	char** urls;
	int count;
};

static struct device_list_t list_devices(tobii_api_t* api)
{
	struct device_list_t list = { NULL, 0 };
	// Create a memory context that can be used by the url receiver callback
	struct url_receiver_context_t url_receiver_context;
	url_receiver_context.count = 0;
	url_receiver_context.capacity = 16;
	url_receiver_context.urls = (char**)malloc(sizeof(char*) * url_receiver_context.capacity);
	if (!url_receiver_context.urls)
	{
		fprintf(stderr, "Allocation failed\n");
		return list;
	}

	// Enumerate the connected devices, connected devices will be stored in the supplied memory context
	tobii_error_t error = tobii_enumerate_local_device_urls(api, url_receiver, &url_receiver_context);
	if (error != TOBII_ERROR_NO_ERROR) fprintf(stderr, "Failed to enumerate devices.\n");

	list.urls = url_receiver_context.urls;
	list.count = url_receiver_context.count;
	return list;
}


static void free_device_list(struct device_list_t* list)
{
	for (int i = 0; i < list->count; ++i)
		free(list->urls[i]);

	free(list->urls);
}


static char const* select_device(struct device_list_t* devices)
{
	int tmp = 0, selection = 0;

	// Present the available devices and loop until user has selected a valid device
	while (selection < 1 || selection > devices->count)
	{
		printf("\nSelect a device\n\n");
		for (int i = 0; i < devices->count; ++i) printf("%d. %s\n", i + 1, devices->urls[i]);
		if (scanf("%d", &selection) <= 0) while ((tmp = getchar()) != '\n' && tmp != EOF);
	}

	return devices->urls[selection - 1];
}


struct thread_context_t
{
	tobii_device_t* device;
	HANDLE reconnect_event; // Used to signal that a reconnect is needed
	HANDLE timesync_event; // Timer event used to signal that time synchronization is needed
	HANDLE exit_event; // Used to signal that the background thead should exit
	volatile LONG is_reconnecting;
};


static DWORD WINAPI reconnect_and_timesync_thread(LPVOID param)
{
	struct thread_context_t* context = (struct thread_context_t*) param;

	HANDLE objects[3];
	objects[0] = context->reconnect_event;
	objects[1] = context->timesync_event;
	objects[2] = context->exit_event;

	for (; ; )
	{
		// Block here, waiting for one of the three events.
		DWORD result = WaitForMultipleObjects(3, objects, FALSE, INFINITE);
		if (result == WAIT_OBJECT_0) // Handle reconnect event
		{
			tobii_error_t error;
			// Run reconnect loop until connection succeeds or the exit event occurs
			while ((error = tobii_device_reconnect(context->device)) != TOBII_ERROR_NO_ERROR)
			{
				if (error != TOBII_ERROR_CONNECTION_FAILED)
					fprintf(stderr, "Reconnection failed: %s.\n", tobii_error_message(error));
				// Blocking waiting for exit event, timeout after 250 ms and retry reconnect
				result = WaitForSingleObject(context->exit_event, 250); // Retry every quarter of a second
				if (result == WAIT_OBJECT_0)
					return 0; // exit thread
			}
			InterlockedExchange(&context->is_reconnecting, 0L);
		}
		else if (result == WAIT_OBJECT_0 + 1) // Handle timesync event
		{
			tobii_error_t error = tobii_update_timesync(context->device);
			LARGE_INTEGER due_time;
			LONGLONG const timesync_update_interval = 300000000LL; // Time sync every 30 s
			LONGLONG const timesync_retry_interval = 1000000LL; // Retry time sync every 100 ms
			due_time.QuadPart = (error == TOBII_ERROR_NO_ERROR) ? -timesync_update_interval : -timesync_retry_interval;
			// Re-schedule timesync event
			SetWaitableTimer(context->timesync_event, &due_time, 0, NULL, NULL, FALSE);
		}
		else if (result == WAIT_OBJECT_0 + 2) // Handle exit event
		{
			// Exit requested, exiting the thread
			return 0;
		}
	}
}


static void log_func(void* log_context, tobii_log_level_t level, char const* text)
{
	CRITICAL_SECTION* log_mutex = (CRITICAL_SECTION*)log_context;

	EnterCriticalSection(log_mutex);
	if (level == TOBII_LOG_LEVEL_ERROR)
		fprintf(stderr, "Logged error: %s\n", text);
	LeaveCriticalSection(log_mutex);
}

//global variables to pass through game loop

static CRITICAL_SECTION log_mutex;
static tobii_api_t* api;
static tobii_device_t* device;
static tobii_gaze_point_t latest_gaze_point;
static HANDLE reconnect_event;
static HANDLE timesync_event;
static HANDLE exit_event;
static struct thread_context_t thread_context;
static HANDLE thread_handle;

//----------
//END TOBII FUNCTIONS
//----------

Color OVManager::inactive_color = Color(255, 254, 255, 255);
Color OVManager::focus_color = Color(255, 0, 255, 255);
Color OVManager::active_color = Color(255, 255, 255, 0);
int OVManager::gazepos_x = 0;
int OVManager::gazepos_y = 0;
bool OVManager::gaze_valid = false;
int OVManager::gazeDecay = 0;

OVManager::OVManager(HWND* hWnd)
{
	this->hWnd = hWnd;
	this->b_quit = false;
	this->frametime = 0;
	this->tobii_failure = false;
	this->deadframes = 0;
	this->last_x = 0;
	this->last_y = 0;
	numframes = 0;
	curr_panel = 0;
	//get screen resolution
	RECT desktop;
	const HWND hDesktop = GetDesktopWindow();
	GetWindowRect(hDesktop, &desktop);
	resolution_x = desktop.right;
	resolution_y = desktop.bottom;
	this->load();

	//Initialize Tobii Eyetracking system
	InitializeCriticalSection(&log_mutex);
	tobii_custom_log_t custom_log = { &log_mutex, log_func };
	tobii_error_t error = tobii_api_create(&api, NULL, &custom_log);
	if (error != TOBII_ERROR_NO_ERROR)
	{
		MessageBox(NULL, "Tobii API not installed.", "Tobii Failure", MB_OK);
		tobii_failure = true;
		return;
	}
	struct device_list_t devices = list_devices(api);
	if (devices.count == 0)
	{
		free_device_list(&devices);
		tobii_api_destroy(api);
		MessageBox(NULL, "Device not found.", "Tobii Failure", MB_OK);
		tobii_failure = true;
		return;
	}
	char const* selected_device = devices.count == 1 ? devices.urls[0] : select_device(&devices);
	error = tobii_device_create(api, selected_device, &device);
	free_device_list(&devices);
	if (error != TOBII_ERROR_NO_ERROR)
	{
		tobii_api_destroy(api);
		MessageBox(NULL, "Failed to initialize device.", "Tobii Failure", MB_OK);
		tobii_failure = true;
		return;
	}
	latest_gaze_point.timestamp_us = 0LL;
	latest_gaze_point.validity = TOBII_VALIDITY_INVALID;
	//start subscribing to gaze point data
	error = tobii_gaze_point_subscribe(device, gaze_callback, &latest_gaze_point);
	if (error != TOBII_ERROR_NO_ERROR)
	{
		tobii_device_destroy(device);
		tobii_api_destroy(api);
		MessageBox(NULL, "Failed to subscribe to gaze stream.", "Tobii Failure", MB_OK);
		tobii_failure = true;
		return;
	}
	//create events for inter-thread signaling
	reconnect_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	timesync_event = CreateWaitableTimer(NULL, TRUE, NULL);
	exit_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (reconnect_event == NULL || timesync_event == NULL || exit_event == NULL)
	{
		tobii_device_destroy(device);
		tobii_api_destroy(api);
		MessageBox(NULL, "Failed to create event objects.", "Tobii Failure", MB_OK);
		tobii_failure = true;
		return;
	}
	thread_context.device = device;
	thread_context.reconnect_event = reconnect_event;
	thread_context.timesync_event = timesync_event;
	thread_context.exit_event = exit_event;
	thread_context.is_reconnecting = 0;
	thread_handle = CreateThread(NULL, 0U, reconnect_and_timesync_thread, &thread_context, 0U, NULL);
	if(thread_handle == NULL)
	{
		tobii_device_destroy(device);
		tobii_api_destroy(api);
		MessageBox(NULL, "Failed to create thread.", "Tobii Failure", MB_OK);
		tobii_failure = true;
		return;
	}
	LARGE_INTEGER due_time;
	LONGLONG const timesync_update_interval = 300000000LL;
	due_time.QuadPart = -timesync_update_interval;
	//schedule time synchronization event
	BOOL timer_error = SetWaitableTimer(thread_context.timesync_event, &due_time, 0, NULL, NULL, FALSE);
	if (timer_error == FALSE)
	{
		tobii_device_destroy(device);
		tobii_api_destroy(api);
		MessageBox(NULL, "Failed to schedule timer event.", "Tobii Failure", MB_OK);
		tobii_failure = true;
		return;
	}
}

OVManager::~OVManager()
{
	for (size_t i = 0; i < panels.size(); ++i)
	{
		delete panels[i];
	}
	//destroy tobii objects
	tobii_error_t error;
	if (!tobii_failure)
	{
		SetEvent(thread_context.exit_event);
		WaitForSingleObject(thread_handle, INFINITE);
		CloseHandle(thread_handle);
		CloseHandle(timesync_event);
		CloseHandle(exit_event);
		CloseHandle(reconnect_event);
		error = tobii_gaze_point_unsubscribe(device);
		if (error != TOBII_ERROR_NO_ERROR)
			tobii_failure = true;
		error = tobii_device_destroy(device);
		if (error != TOBII_ERROR_NO_ERROR)
			tobii_failure = true;
		error = tobii_api_destroy(api);
		if (error != TOBII_ERROR_NO_ERROR)
			tobii_failure = true;
	}
	DeleteCriticalSection(&log_mutex);
}

bool OVManager::loop()
{
	if (tobii_failure) return false;	//quits out on a tobii failure
	if (clock() > frametime && !InterlockedCompareExchange(&thread_context.is_reconnecting, 0L, 0L))	//checks to make sure it isn't reconnecting and is within refresh rate
	{
		frametime = clock() + 1000 / framerate;	//set next time a frame should run
		this->gazeDecay = (1 + decayRate) * (clock() - pframe);
		tobii_error_t error = tobii_device_process_callbacks(device);
		if (error == TOBII_ERROR_CONNECTION_FAILED)
		{
			//change state and signal reconnect
			InterlockedExchange(&thread_context.is_reconnecting, 1L);
			SetEvent(thread_context.reconnect_event);
		}
		else if (error != TOBII_ERROR_NO_ERROR)
		{
			MessageBox(NULL, "Tobii callback failed.", "Tobii Failure", MB_OK);
			return false;
		}
		//use the gaze point data


		//TODO: if invalid, pass old data and update counter, if counter passes 5 or so go to negative values.
		//if valid, check if under speed, if it is, give accurate data, if it isn't, give previous point data.

		if (latest_gaze_point.validity == TOBII_VALIDITY_VALID)	//checks if data is valid, otherwise skips a frame (may want to change so updates without valid data)
		{
			deadframes = 0;
			int newframe_x = latest_gaze_point.position_xy[0] * resolution_x - offset_x;	//finds gaze point
			int newframe_y = latest_gaze_point.position_xy[1] * resolution_y - offset_y;
			int dx = newframe_x - last_x;
			int dy = newframe_y - last_y;
			if (maxspeed * maxspeed > dx * dx + dy * dy)	//checks that frame is under the 'speed limit'
			{
				gazepos_x = newframe_x;
				gazepos_y = newframe_y;
			}
			last_x = newframe_x;	//updates previous position
			last_y = newframe_y;
			gaze_valid = true;
		}
		else
		{
			deadframes++;
			if (deadframes > 20)	//takes small amt of time to register 'not actually looking at screen'
			{
				gazepos_x = -1;
				gazepos_y = -1;
			}
			gaze_valid = false;
		}
		if (panels[curr_panel]->update()) RedrawWindow(*hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE);	//updates all controls, if a redraw is needed forces the paint function
		ControlDriver::Update();
		this->pframe = clock();
	}
	return !OVManager::b_quit;
}

void OVManager::draw(HDC hdc)
{
	Graphics    graphics(hdc);

	panels[curr_panel]->draw(&graphics);

	Pen pen(Color(255, 255, 0, 0));	//debug code for eye position
	pen.SetWidth(3);
	//graphics.DrawLine(&pen, latest_gaze_point.position_xy[0] * resolution_x - 10, latest_gaze_point.position_xy[1] * resolution_y - 10, latest_gaze_point.position_xy[0] * resolution_x + 10, latest_gaze_point.position_xy[1] * resolution_y + 10);
	//graphics.DrawLine(&pen, latest_gaze_point.position_xy[0] * resolution_x + 10, latest_gaze_point.position_xy[1] * resolution_y - 10, latest_gaze_point.position_xy[0] * resolution_x - 10, latest_gaze_point.position_xy[1] * resolution_y + 10);
}

void OVManager::changePanel(std::string name)
{
	for (int i = 0; i < panels.size(); ++i)
	{
		if (name == panels[i]->name)
		{
			panels[i]->reset();
			curr_panel = i;
			return;
		}
	}
}

using namespace rapidjson;

void OVManager::load()
{
	std::ifstream file;	//read entire file into a char* for processing by RapidJSON
	int length;
	file.open("..\\Configuration.json");
	file.seekg(0, std::ios::end);
	length = file.tellg();
	file.seekg(0, std::ios::beg);
	char* json = new char[length];
	file.read(json, length);
	file.close();

	Document doc;
	if (doc.Parse<kParseStopWhenDoneFlag>(json).HasParseError())
	{
		fprintf(stderr, "\nError(offset %u): %s\n", (unsigned)doc.GetErrorOffset(), doc.GetParseError());
		MessageBox(NULL, "Something went wrong in loading the configuration, please check your formatting.", "Load Failure", MB_OK);
		return;
	}
	float screen_width = doc["screen_width"].GetFloat();	//get parameters for screen size in user-definable dimensions, calculate multipliers to use for control dimensions
	float screen_height = doc["screen_height"].GetFloat();
	this->maxspeed = doc["max_speed"].GetInt();
	this->decayRate = doc["decay_rate"].GetFloat();
	float mult_x = (float)resolution_x / screen_width;
	float mult_y = (float)resolution_y / screen_height;
	offset_x = doc["offset_x"].GetInt();
	offset_y = doc["offset_y"].GetInt();
	for (Value::ConstValueIterator panel = doc["panels"].Begin(); panel != doc["panels"].End(); ++panel)	//iterate through each panel
	{
		OVPanel* pn = new OVPanel();
		pn->name = (*panel)["name"].GetString();
		panels.push_back(pn);
		for (Value::ConstValueIterator control = (*panel)["controls"].Begin(); control != (*panel)["controls"].End(); ++control)	//iterate through each control in a panel
		{
			int top = mult_y * (*control)["top"].GetFloat() + offset_y;
			int bottom = mult_y * (*control)["bottom"].GetFloat() + offset_y;
			int left = mult_x * (*control)["left"].GetFloat() + offset_x;
			int right = mult_x * (*control)["right"].GetFloat() + offset_x;
			std::string ctrl_type = (*control)["type"].GetString();
			if (ctrl_type == "DIAL")
			{
				int stick;
				std::string st = (*control)["stick"].GetString();
				if (st == "LEFT") stick = 0;
				else if (st == "RIGHT") stick = 1;
				else
				{
					//TODO: failure point
					stick = 5;
				}
				OVDial* dial = new OVDial(this, top, bottom, left, right, stick);
				pn->add(dial);
			}
			else if (ctrl_type == "BUTTON")
			{
				std::string btn_name = (*control)["name"].GetString();
				OVButton* btn = new OVButton(this, btn_name, top, bottom, left, right);
				pn->add(btn);
				std::string pswitch = "";
				if ((*control)["pswitch"].IsString()) pswitch = (*control)["pswitch"].GetString();
				int delay = (*control)["delay"].GetInt();
				bool hold = (*control)["hold"].GetBool();
				bool toggle = (*control)["toggle"].GetBool();
				std::vector<CKey> keys;
				for (Value::ConstValueIterator key = (*control)["keys"].Begin(); key != (*control)["keys"].End(); ++key)
				{
					CKey k;
					std::string ky = key->GetString();
					if (ky == "A") k = CK_A;
					else if (ky == "B") k = CK_B;
					else if (ky == "X") k = CK_X;
					else if (ky == "Y") k = CK_Y;
					else if (ky == "D_up") k = CK_D_up;
					else if (ky == "D_down") k = CK_D_down;
					else if (ky == "D_left") k = CK_D_left;
					else if (ky == "D_right") k = CK_D_right;
					else if (ky == "Start") k = CK_Start;
					else if (ky == "Select") k = CK_Select;
					else if (ky == "L1") k = CK_L1;
					else if (ky == "L2") k = CK_L2;
					else if (ky == "L3") k = CK_L3;
					else if (ky == "R1") k = CK_R1;
					else if (ky == "R2") k = CK_R2;
					else if (ky == "R3") k = CK_R3;
					else if (ky == "Home") k = CK_Home;
					else if (ky == "L_up") k = CK_L_up;
					else if (ky == "L_down") k = CK_L_down;
					else if (ky == "L_left") k = CK_L_left;
					else if (ky == "L_right") k = CK_L_right;
					else if (ky == "R_up") k = CK_R_up;
					else if (ky == "R_down") k = CK_R_down;
					else if (ky == "R_left") k = CK_R_left;
					else if (ky == "R_right") k = CK_R_right;
					else if (ky == "0") k = CK_0;
					else if (ky == "1") k = CK_1;
					else if (ky == "2") k = CK_2;
					else if (ky == "3") k = CK_3;
					else if (ky == "4") k = CK_4;
					else if (ky == "5") k = CK_5;
					else if (ky == "6") k = CK_6;
					else if (ky == "7") k = CK_7;
					else if (ky == "8") k = CK_8;
					else if (ky == "9") k = CK_9;
					else if (ky == "QUIT") k = CK_QUIT;

					keys.push_back(k);
				}
				btn->setActions(delay, hold, toggle, pswitch, keys);
			}
		}
	}
}