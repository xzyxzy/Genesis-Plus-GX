#ifndef __INPUTACT_H__
#define __INPUTACT_H__

#ifdef __cplusplus
extern "C" {
#endif

void input_run_action(char *str, int press);
int input_process_keycode(char *keystr, int press);
void inputact_init();
void input_process_joystick(int joynum, int button, int press);


#ifdef __cplusplus
}
#endif

#endif