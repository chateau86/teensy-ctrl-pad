#include <Metro.h>
typedef int PIN_NUM;

typedef struct actions {
  bool is_keyboard;
  int action_code; // Button number or keycode
} ACTION;

typedef struct knobs {
  PIN_NUM pin_A;
  PIN_NUM pin_B;
  PIN_NUM pin_DN;
  int knob_threshold_multiplier;
  ACTION act_up;
  ACTION act_down;
  ACTION act_push;
  //---internal states---
  int knob_pos_raw; // [0...3]
  int knob_pos;  // Reset this to zero when event is handled
  int btn_pos_raw; // [0,1]
  int btn_pos;  // Reset this to zero when event is handled
  int _abs_knob_pos;  // abs pos starting from 0; may overflow; for debug only

} KNOB;

Metro updCtrl = Metro(1000/200); //200Hz

int KNOB_COUNT = 1;
KNOB knob_list[1] = {
  {
    0, 1, 2,
    1,
    {true, KEY_MEDIA_VOLUME_INC},
    {true, KEY_MEDIA_VOLUME_DEC},
    {true, KEY_MEDIA_MUTE},
    0, 0, 0, 0, 0,
  },
};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(38400);
  pinMode(0, INPUT_PULLUP);
  pinMode(1, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  Serial.println("Init OK");
}



void loop() {
  //Read encoders
  for (int i = 0; i < KNOB_COUNT; i++) {
    // TODO: pull down the right knobs (after decoder chip is wired in)
    // TODO: sleep a bit after knob is pulled down
    track_knob(&knob_list[i]);
  }

  // Only wake at 200 Hz
  if(updCtrl.check() == 0) {
    return;
  }

  // Handle the button
  for (int i = 0; i < KNOB_COUNT; i++) {
    service_knob(&knob_list[i]);
  }
}

int remap_enc(PIN_NUM A , PIN_NUM B) {
  // Going up: 3->2->0->1
  const int actual[4] = {2,3,1,0};
  bool a_val = digitalRead(A);
  bool b_val = digitalRead(B);
  int val = (int)(b_val) << 1 | (int)(a_val);
  return actual[val];
}

void track_knob(KNOB *knob) {
  int knob_val = remap_enc(knob->pin_A , knob->pin_B);
  if (knob_val != knob->knob_pos_raw) {
    // knob was moved
    if ((knob_val - knob->knob_pos_raw == 1) || (knob_val == 0 && knob->knob_pos_raw == 3)) {
      //Serial.print("+ ");
      //Serial.println(knob->knob_pos);
      knob->knob_pos += 1;
      knob->_abs_knob_pos += 1;
    } else if ((knob_val - knob->knob_pos_raw == -1) || (knob_val == 3 && knob->knob_pos_raw == 0)) {
      //Serial.print("- ");
      //Serial.println(knob->knob_pos);
      knob->knob_pos -= 1;
      knob->_abs_knob_pos -= 1;
    } else {
      //Serial.print("? ");
      //Serial.println(knob->knob_pos);
    }
  }

  // reset KNOB object state for next refresh
  knob->knob_pos_raw = knob_val;
  knob->btn_pos_raw = digitalRead(knob->pin_DN);
}

void service_knob(KNOB *knob) {
  int KNOB_THRESHOLD = 4 * knob->knob_threshold_multiplier;
  if (knob->knob_pos >= KNOB_THRESHOLD) {
    // Do knob+ action
    Serial.print("+ ");
    Serial.print(knob->knob_pos);
    Serial.print(" : ");
    Serial.println(knob->_abs_knob_pos);
    perform_action(knob->act_up, 0);
    knob->knob_pos = 0;
  } else if (knob->knob_pos <= -KNOB_THRESHOLD){
    // Do knob- action
    Serial.print("- ");
    Serial.print(-(knob->knob_pos));
    Serial.print(" : ");
    Serial.println(knob->_abs_knob_pos);
    perform_action(knob->act_down, 0);
    knob->knob_pos = 0;
  }
  if (knob->btn_pos_raw != knob->btn_pos) {
    perform_action(knob->act_push, knob->btn_pos_raw - knob->btn_pos);
    knob->btn_pos = knob->btn_pos_raw;
  }
}

void perform_action(ACTION act, int state) {
  // State:
  //   -1: Release
  //    0: Pulse
  //   +1: Push and hold
  if (act.is_keyboard){
    if (state == -1) {
      Keyboard.press(act.action_code);
      Serial.print("v");
    } else if (state == 1) {
      Keyboard.release(act.action_code);
      Serial.print("^");
    } else {
      // Do a keypress
      Keyboard.press(act.action_code);
      Keyboard.release(act.action_code);
      Serial.print("v^");
    }
  } else {
    // Set joystick button
    // TODO
  }
}
