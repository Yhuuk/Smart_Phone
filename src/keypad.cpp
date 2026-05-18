#include "keypad.h"

static const int DEFAULT_ROW_PINS[4] = {4,17,25,26};
static const int DEFAULT_COL_PINS[4] = {34,35,36,39};

static const char DEFAULT_KEYMAP[4][4] = 
{
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'},
};

/*****************************************
 * 构造函数判断：
 * 1它的名字和类名完全一样
 * 2没有返回值类型(没有void/int...)
 * 3-参数类型const代表只读，不去修改传进去的数组
 ****************************************/
MatrixKeypad4x4_CR::MatrixKeypad4x4_CR(const int rowPins[4],
                                       const int colPins[4],
                                       const char keymap[4][4]) {
  for (int i = 0; i < 4; i++) {
    _rowPins[i] = rowPins[i];
    _colPins[i] = colPins[i];
  }
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++)
      _map[r][c] = keymap[r][c];
}

// MatrixKeypad4x4_CR::MatrixKeypad4x4_CR()
//     : MatrixKeypad4x4_CR(DEFAULT_ROW_PINS, DEFAULT_COL_PINS, DEFAULT_KEYMAP) {}

/*****************************************
 *默认构造函数：
 *1-第三个MatrixKeypad4x4_CR：当前这个无参构造函数
 *去调用带3个参数的那个构造函数(避免重复初始化代码)
 * 
 * 2- :符号是调用别的构造函数
 * 3- {}是空的，把初始化工作交给了那个带参的构造函数了
 ****************************************/
MatrixKeypad4x4_CR::MatrixKeypad4x4_CR() : MatrixKeypad4x4_CR
(DEFAULT_ROW_PINS,DEFAULT_COL_PINS,DEFAULT_KEYMAP) {}


void MatrixKeypad4x4_CR::begin(uint32_t debounceMs) {
  _debounceMs = debounceMs;

  // 行：输出，默认拉高
  for (int r = 0; r < 4; r++) {
    pinMode(_rowPins[r], OUTPUT);
    digitalWrite(_rowPins[r], HIGH);
  }

  // 列：输入（你已经外接10k上拉到3.3V，所以用 INPUT 即可）
  // 注意：GPIO34~39 没有内部上拉，这就是你外接电阻的原因
  for (int c = 0; c < 4; c++) {
    pinMode(_colPins[c], INPUT);
  }
}

void MatrixKeypad4x4_CR::enableRepeat(char repeatKey,
                                      uint32_t firstDelayMs,
                                      uint32_t intervalMs) {
  int8_t idx = repeatConfigIndex(repeatKey);
  if (idx < 0) {
    if (_repeatKeyCount >= REPEAT_KEY_MAX) return;
    idx = _repeatKeyCount++;
    _repeatKeys[idx] = repeatKey;
  }

  _repeatFirstDelay[idx] = firstDelayMs;
  _repeatInterval[idx] = intervalMs;
}

int8_t MatrixKeypad4x4_CR::repeatConfigIndex(char k) const {
  for (uint8_t i = 0; i < _repeatKeyCount; i++) {
    if (_repeatKeys[i] == k) return (int8_t)i;
  }
  return -1;
}

inline void MatrixKeypad4x4_CR::allRowsHigh() {
  for (int r = 0; r < 4; r++) digitalWrite(_rowPins[r], HIGH);
}

inline void MatrixKeypad4x4_CR::driveOneRowLow(int r) {
  digitalWrite(_rowPins[r], LOW);
}

void MatrixKeypad4x4_CR::pushEvent(char k, KeyEventType t, uint32_t now) {
  uint8_t next = (uint8_t)((_qt + 1) % QSIZE);
  if (next == _qh) {
    // 队列满了：丢最旧的
    _qh = (uint8_t)((_qh + 1) % QSIZE);
  }
  _q[_qt] = {k, t, now};
  _qt = next;
}

/*************************************************
 * 
 * 
 * 
 ***********************************************/
bool MatrixKeypad4x4_CR::poll(KeyEvent &out) {
  uint32_t now = millis();
  scanOnce(now);

  if (_qh == _qt) return false;
  out = _q[_qh];
  _qh = (uint8_t)((_qh + 1) % QSIZE);
  return true;
}

bool MatrixKeypad4x4_CR::getKey(char &k) {
  KeyEvent e;
  while (poll(e)) {
    if (e.type == KeyEventType::Pressed || e.type == KeyEventType::Repeat) {
      k = e.key;
      return true;
    }
  }
  return false;
}

void MatrixKeypad4x4_CR::scanOnce(uint32_t now) {
  for (int r = 0; r < 4; r++) {
    allRowsHigh();
    driveOneRowLow(r);
    delayMicroseconds(20); // 电平稳定

    for (int c = 0; c < 4; c++) {
      bool rawPressed = (digitalRead(_colPins[c]) == LOW);

      if (rawPressed != _rawLast[r][c]) {
        _rawLast[r][c] = rawPressed;
        _lastChange[r][c] = now;
      }

      if ((now - _lastChange[r][c]) >= _debounceMs) {
        if (_stable[r][c] != rawPressed) {
          _stable[r][c] = rawPressed;

          char k = _map[r][c];
          if (rawPressed) {
            _pressedAt[r][c] = now;
            _lastRepeatAt[r][c] = now;
            pushEvent(k, KeyEventType::Pressed, now);
          } else {
            pushEvent(k, KeyEventType::Released, now);
          }
        }
      }

      // 长按连发（适合 '*' 退格）
      if (_stable[r][c]) {
        char k = _map[r][c];
        int8_t repeatIdx = repeatConfigIndex(k);
        if (repeatIdx >= 0) {
          if ((now - _pressedAt[r][c]) >= _repeatFirstDelay[repeatIdx] &&
              (now - _lastRepeatAt[r][c]) >= _repeatInterval[repeatIdx]) {
            _lastRepeatAt[r][c] = now;
            pushEvent(k, KeyEventType::Repeat, now);
          }
        }
      }
    }
  }

  allRowsHigh();
}
