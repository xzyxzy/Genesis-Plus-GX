#include "portable-file-dialogs.h"

#include "inputact.h"
#include "shared.h"
#include "video_base.h" 
#include "config.h"

void input_run_action(char *str, int press) {
    if (press == 2) return;

    // ELSE IF ELSE IF ELSE IF YEAH I GOT IT
    if (strncmp(str, "pad", 3) == 0) {
        if (strlen(str) <= 6) return;
    
        int padnum;
        sscanf(str, "pad_%i", &padnum);
        padnum--;
        
        char *buttonname = str + 6;
        // printf("%i, %s\n",padnum, buttonname);

	    unsigned short keymask;
             if(strcmp(buttonname, "a")     == 0)   keymask = INPUT_A;
        else if(strcmp(buttonname, "b")     == 0)   keymask = INPUT_B;
        else if(strcmp(buttonname, "c")     == 0)   keymask = INPUT_C;
        else if(strcmp(buttonname, "start") == 0)   keymask = INPUT_START;
        else if(strcmp(buttonname, "x")     == 0)   keymask = INPUT_X;
        else if(strcmp(buttonname, "y")     == 0)   keymask = INPUT_Y;
        else if(strcmp(buttonname, "z")     == 0)   keymask = INPUT_Z;
        else if(strcmp(buttonname, "mode")  == 0)   keymask = INPUT_MODE;
        else if(strcmp(buttonname, "up")    == 0)   keymask = INPUT_UP;
        else if(strcmp(buttonname, "down")  == 0)   keymask = INPUT_DOWN;
        else if(strcmp(buttonname, "left")  == 0)   keymask = INPUT_LEFT;
        else if(strcmp(buttonname, "right") == 0)   keymask = INPUT_RIGHT;

        if (press) input.pad[padnum] |= keymask;
        else input.pad[padnum] &= ~keymask;
        // input.pad[padnum] |= input_kb;

    } else if (strcmp(str, "reset") == 0) {
        if (!press) return;
        json_t *config_system = json_object_get(config_json, "system");
        json_t *config_confirm_reset = json_object_get(config_system, "confirm_reset");
        if (
            (config_confirm_reset != NULL) &&
            json_boolean_value(config_confirm_reset)
        ) {
            pfd::button quit_result = pfd::message(
                "Reset",
                "Are you sure you want to reset?",
                pfd::choice::yes_no,
                pfd::icon::warning
            ).result();
            if (quit_result != pfd::button::yes) return;
        }
        system_reset();
    } else if (strcmp(str, "fullscreen") == 0) {
        if (press) Backend_Video_ToggleFullscreen();
    } else if (strcmp(str, "quit") == 0) {
        if (!press) return;
        json_t *config_system = json_object_get(config_json, "system");
        json_t *config_confirm_quit = json_object_get(config_system, "confirm_quit");
        if (
            (config_confirm_quit != NULL) &&
            json_boolean_value(config_confirm_quit)
        ) {
            pfd::button quit_result = pfd::message(
                "Quit",
                "Are you sure you want to quit?",
                pfd::choice::yes_no,
                pfd::icon::question
            ).result();
            if (quit_result != pfd::button::yes) return;
        }
        running = 0;
    }
}

json_t *binds_keyboard;
json_t *binds_controller;

void input_process_joystick(int joynum, int button, int press) {
    // printf("Joynum: %i, Button: %i, Pressed: %i\n", joynum, button, press);

    if (binds_controller == NULL) return;

    char buffer[3];

    if (joynum + 1 >= 100) return;
    sprintf(buffer, "%i", joynum + 1);
    json_t *binds_controllernum = json_object_get(binds_controller, buffer);
    if (binds_controllernum == NULL) return;

    if (button + 1 >= 100) return;
    sprintf(buffer, "%i", button + 1);
    json_t *bind_actions = json_object_get(binds_controllernum, buffer);

    if (bind_actions == NULL) return;

    if (!json_is_array(bind_actions)) return;

    for (int i = 0; i < json_array_size(bind_actions); i++) {
        json_t *element = json_array_get(bind_actions, i);
        if (!json_is_string(element)) continue;
        input_run_action((char *)json_string_value(element), press);
    }
}

int input_process_keycode(char *keystr, int press) {
    // printf("Keycode: %i, Pressed: %i, Name: %s\n", keycode, press, keystr);

    if (keystr == NULL) return FALSE;
    if (binds_keyboard == NULL) return FALSE;

    json_t *bind_actions = json_object_get(binds_keyboard, keystr);
    if (bind_actions == NULL) return FALSE;
    if (!json_is_array(bind_actions)) return FALSE;

    for (int i = 0; i < json_array_size(bind_actions); i++) {
        json_t *element = json_array_get(bind_actions, i);
        if (!json_is_string(element)) continue;
        input_run_action((char *)json_string_value(element), press);
    }

    return TRUE;
}

void inputact_init() {
    json_t *config_binds = json_object_get(config_json, "binds");
    if (config_binds == NULL) return;
    binds_keyboard = json_object_get(config_binds, "keyboard");
    binds_controller = json_object_get(config_binds, "controller");
};