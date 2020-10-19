#ifndef __INPUTACT_H__
#define __INPUTACT_H__

void input_run_action(char *str, int press);
void input_process_keycode(int keycode, int press);
void inputact_init();
void input_process_joystick(int joynum, int button, int press);

#endif