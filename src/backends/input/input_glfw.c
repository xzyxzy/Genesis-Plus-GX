#include "input_base.h"
#include "video_base.h"

#include <GLFW/glfw3.h> // GLFW helper library

#include "shared.h"

#ifndef max
    #define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
    #define min(a,b) ((a) < (b) ? (a) : (b))
#endif

int Backend_Input_Update() {
	glfwPollEvents();
	return 1;
}

int joynum = 0;

unsigned short input_kb = 0;
unsigned short input_j1 = 0;

uint64 frametimer = 0; // used for analog movement via PWM

void Update_KB() {
	input.pad[joynum] |= input_kb;
}

void Update_Joystick() {
	// int count = 0;
	// input_j1 = 0;

	// const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &count);

	// if ((count >= 1) && buttons[0]) input.pad[joynum] |= INPUT_A;
	// if ((count >= 2) && buttons[1]) input.pad[joynum] |= INPUT_B;
	// if ((count >= 3) && buttons[2]) input.pad[joynum] |= INPUT_C;
	// if ((count >= 8) && buttons[7]) input.pad[joynum] |= INPUT_START;

	// const unsigned char* hats = glfwGetJoystickHats(GLFW_JOYSTICK_1, &count);
	
	// if ((count >= 1) && (hats[0] & GLFW_HAT_UP))      input_j1 |= INPUT_UP;
	// if ((count >= 1) && (hats[0] & GLFW_HAT_DOWN))    input_j1 |= INPUT_DOWN;
	// if ((count >= 1) && (hats[0] & GLFW_HAT_LEFT))    input_j1 |= INPUT_LEFT;
	// if ((count >= 1) && (hats[0] & GLFW_HAT_RIGHT))   input_j1 |= INPUT_RIGHT;

	// const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &count);

	// int axis_lx = 0;
	// int axis_ly = 0;

	// if (count >= 1) {
	// 	axis_lx = axes[0] * 32767;
	// 	axis_ly = axes[1] * 32767;
	// }

	// double deadzone_l = 10000;
	// double intensity_l = sqrt(pow(axis_lx, 2) + pow(axis_ly, 2));
	// // printf("%f\n",intensity_l);
	// double angle_l = atan2((double)axis_ly, (double)axis_lx) + M_PI;

	// double angle_overlap_x = M_PI_4 / 8.0;
	// double angle_overlap_y = M_PI_4 / 8.0;

	// double deadzone_lx_analog_upper = 10000;
	// double intensity_lx_adj = min(1, max(0,
	// 	(abs(axis_lx) - deadzone_l) /
	// 	(32767.0 - deadzone_lx_analog_upper - deadzone_l)
	// ));


	// int allow_horiz = 1;
	// if (intensity_lx_adj < 1.0) {
	// 	double frametimer_period = 3.0;
	// 	allow_horiz = (
	// 		(int)(frametimer / frametimer_period) %
	// 		(
	// 			(int)(60.0 / frametimer_period) -
	// 			(int)(intensity_lx_adj * (60.0 / frametimer_period))
	// 		)
	// 	) == 0;
	// }

	// if (intensity_l > deadzone_l) {
	// 	if (
	// 		(
	// 		(angle_l < M_PI_4 + angle_overlap_x) ||
	// 		(angle_l > ((2*M_PI) - M_PI_4 - angle_overlap_x))
	// 		) && (allow_horiz)
	// 	) input.pad[joynum] |= INPUT_LEFT;

	// 	if (
	// 		(
	// 		(angle_l < (M_PI + M_PI_4 + angle_overlap_x)) &&
	// 		(angle_l > (M_PI - M_PI_4 - angle_overlap_x))
	// 		) && (allow_horiz)
	// 	) input.pad[joynum] |= INPUT_RIGHT;

	// 	if (
	// 		(angle_l < (M_PI_2 + M_PI_4 + angle_overlap_y)) &&
	// 		(angle_l > (M_PI_2 - M_PI_4 - angle_overlap_y))
	// 	) input.pad[joynum] |= INPUT_UP;

	// 	if (
	// 		(angle_l < (M_PI + M_PI_2 + M_PI_4 + angle_overlap_y)) &&
	// 		(angle_l > (M_PI + M_PI_2 - M_PI_4 - angle_overlap_y))
	// 	) input.pad[joynum] |= INPUT_DOWN;
	// }

	// input.pad[joynum] |= input_j1;
}

int Backend_Input_MainLoop() {
	input.pad[joynum] = 0;

	Update_KB();
	Update_Joystick();

	frametimer++;
	
	return 1;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	unsigned short keymask;

		 if(key == GLFW_KEY_A)      keymask = INPUT_A;
	else if(key == GLFW_KEY_S)      keymask = INPUT_B;
	else if(key == GLFW_KEY_D)      keymask = INPUT_C;
	else if(key == GLFW_KEY_F)      keymask = INPUT_START;
	else if(key == GLFW_KEY_Z)      keymask = INPUT_X;
	else if(key == GLFW_KEY_X)      keymask = INPUT_Y;
	else if(key == GLFW_KEY_C)      keymask = INPUT_Z;
	else if(key == GLFW_KEY_V)      keymask = INPUT_MODE;

	else if(key == GLFW_KEY_UP)     keymask = INPUT_UP;
	else if(key == GLFW_KEY_DOWN)   keymask = INPUT_DOWN;
	else if(key == GLFW_KEY_LEFT)   keymask = INPUT_LEFT;
	else if(key == GLFW_KEY_RIGHT)  keymask = INPUT_RIGHT;

	else if(action == GLFW_PRESS) {
			 if(key == GLFW_KEY_F11)	Backend_Video_ToggleFullscreen();
		else if(key == GLFW_KEY_TAB)	system_reset();
	}

	if (action == GLFW_PRESS) {
		input_kb |= keymask;
	} else if (action == GLFW_RELEASE) {
		input_kb &= ~keymask;
	}
}

int Backend_Input_Init() {
	GLFWwindow *window = (GLFWwindow *)window_shared;
	glfwSetKeyCallback(window, key_callback);

	return 1;
}

int Backend_Input_Close() {
	glfwTerminate();
	return 1;
}