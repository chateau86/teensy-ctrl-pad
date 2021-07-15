#include <Metro.h>
typedef int PIN_NUM;

typedef struct actions {
  int action_type;
  /*
    0: nop
    1: keyboard
    2: joystick
  */
  int action_code; // Button number or keycode
} ACTION;

typedef struct knobs {
  
  PIN_NUM enc_id;

  PIN_NUM pin_A;
  PIN_NUM pin_B;
  PIN_NUM pin_DN;

  int knob_threshold_multiplier;
  bool knob_updown;

  ACTION act_down;
  ACTION act_push;
  ACTION act_up;
    
  //---internal states---
  int knob_pos_raw; // [0...3]
  int knob_pos;  // Reset this to zero when event is handled
  int btn_pos_raw; // [0,1]
  int btn_pos;  // Reset this to zero when event is handled
  int _abs_knob_pos;  // abs pos starting from 0; may overflow; for debug only

} KNOB;

Metro updCtrl = Metro(1000/200); //200Hz

int KNOB_COUNT = 10;
KNOB knob_list[10] = {
  {
    0,
    18, 20, 19,
    1, false,
    {2, 1},
    {2, 2},
    {2, 3},
    0, 0, 0, 0, 0,
  }, {
    1,
    18, 20, 19,
    1, false,
    {2, 4},
    {2, 5},
    {2, 6},
    0, 0, 0, 0, 0,
  }, {
    2,
    18, 20, 19,
    1, false,
    {2, 7},
    {2, 8},
    {2, 9},
    0, 0, 0, 0, 0,
  }, {
    3,
    18, 20, 19,
    1, false,
    {2, 10},
    {2, 11},
    {2, 12},
    0, 0, 0, 0, 0,
  }, {
    4,
    18, 20, 19,
    1, false,
    {2, 13},
    {2, 14},
    {2, 15},
    0, 0, 0, 0, 0,
  }, {
    5,
    18, 20, 19,
    1, false,
    {2, 16},
    {2, 17},
    {2, 18},
    0, 0, 0, 0, 0,
  }, {
    6,
    18, 20, 19,
    1, true,
    {1, KEY_MEDIA_VOLUME_DEC},
    {1, KEY_MEDIA_MUTE},
    {1, KEY_MEDIA_VOLUME_INC},
    0, 0, 0, 0, 0,
  }, {
    7,
    18, 20, 19,
    3, true,
    {1, KEY_MEDIA_PREV_TRACK},
    {1, KEY_MEDIA_PLAY_PAUSE},
    {1, KEY_MEDIA_NEXT_TRACK},
    0, 0, 0, 0, 0,
  }, {
    8,
    18, 20, 19,
    1, false,
    {2, 19},
    {2, 20},
    {2, 21},
    0, 0, 0, 0, 0,
  }, {
    9,
    18, 20, 19,
    1, false,
    {2, 22},
    {2, 23},
    {2, 24},
    0, 0, 0, 0, 0,
  },
};

int BTN_ROW_COUNT = 3;
int BTN_ROW_PIN_LIST[3] = {7, 8, 9};
int BTN_COL_COUNT = 4;
int BTN_COL_PIN_LIST[4] = {0, 1, 2, 3};

int* BTN_STATUS = NULL;

int JS_BTN_COUNT = 35;

ACTION btn_actions[12] = {
  {2, 32},
  {2, 29},
  {2, 31},
  {2, 30},
  {2, 34},
  {2, 33},
  {0, 106},
  {0, 107},
  {2, 28},
  {2, 27},
  {2, 26},
  {2, 25},
};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(38400);

  // Encoder select
  pinMode(14, OUTPUT);
  pinMode(15, OUTPUT);
  pinMode(16, OUTPUT);
  pinMode(17, OUTPUT);

  // Encoder return
  pinMode(18, INPUT_PULLUP);
  pinMode(19, INPUT_PULLUP);
  pinMode(20, INPUT_PULLUP);

  // Btn select
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);

  // Btn return
  pinMode(0, INPUT_PULLUP);
  pinMode(1, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);

  BTN_STATUS = (int*) calloc(BTN_ROW_COUNT*BTN_COL_COUNT, sizeof(int));

  Joystick.useManualSend(true);

  Serial.println("Init OK");
}


void loop() {
  //Read encoders

  for (int i = 0; i < KNOB_COUNT; i++) {
    // pull down the right knobs (after decoder chip is wired in)

    select_enc(knob_list[i].enc_id);
    // sleep a bit after knob is pulled down (just under ~100ns needed at 3.3v per decoder datasheet)
    //delay(1);
    delayMicroseconds(100);
    track_knob(&knob_list[i]);
  }
  select_enc(-1);

  // Only wake at 200 Hz
  if(updCtrl.check() == 0) {
    return;
  }
  clear_js_buffer();
  // Handle the knobs
  for (int i = 0; i < KNOB_COUNT; i++) {
    service_knob(&knob_list[i]);
  }

  // Handle the button
  service_buttons();
  Joystick.send_now();
}

void clear_js_buffer(){
  for(int i = 0; i < JS_BTN_COUNT; i++){
    Joystick.button(i, 0); 
  }
}

void select_enc(int id) {
  int dac_bits[4] = {17,16,15,14};
  if (id < 0 || id > 9) {
    id = 15;
  }
  for(int b = 0; b < 4; b++){
    digitalWrite(dac_bits[b], id & 1 << b);
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
  if (knob->knob_updown) {
    if (knob->btn_pos_raw != knob->btn_pos) {
      perform_action(knob->act_push, knob->btn_pos_raw - knob->btn_pos);
      knob->btn_pos = knob->btn_pos_raw;
    }
  } else {
    if (knob->btn_pos_raw == 0) {
      perform_action(knob->act_push, 0);
    }
  }
}

void service_buttons() {
  for(int row = 0; row < BTN_ROW_COUNT; row++) {
    select_btn_row(row);
    delayMicroseconds(50);
    for(int col = 0; col < BTN_COL_COUNT; col++){
      if (digitalRead(BTN_COL_PIN_LIST[col]) == 0) {
          int btn_id = row * BTN_COL_COUNT + col;
          perform_action(btn_actions[btn_id], 0);
      }
    }
  }
  select_btn_row(-1);
}

void select_btn_row(int row) {
  for(int i = 0; i < BTN_ROW_COUNT; i++){
    if (row == i){
      digitalWrite(BTN_ROW_PIN_LIST[i], 0);
    } else {
      digitalWrite(BTN_ROW_PIN_LIST[i], 1);
    }
  }
}

void perform_action(ACTION act, int state) {
  // State:
  //   -1: Release
  //    0: Pulse
  //   +1: Push and hold
  if (act.action_type == 0){
    Serial.print("a: ");
    Serial.print(act.action_code);
    Serial.print(" st: ");
    Serial.println(state);
  }else if (act.action_type == 1){
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
  } else if (act.action_type == 2) {
    // Set joystick button
    Joystick.button(act.action_code, 1); 
  }
}
