#pragma once
#include <Arduino.h>

/*********************************
 * 枚举出按键的状态
 * 
 ********************************/
enum class KeyEventType : uint8_t {
  Pressed,    // 消抖后的按下沿
  Released,   // 消抖后的松开沿（你现在可以不用，但后面做长按会用）
  Repeat      // 长按连发（比如 * 退格连删）
};


/*********************************
 * 结构体：存放各种不同类型的变量
 * 
 ********************************/
struct KeyEvent {
  char key;            // '0'~'9' '*' '#' 'A'~'D'
  KeyEventType type;
  uint32_t ms;         // 事件时间戳 millis()
};


/*********************************
 * 类的声明
 * 
 ********************************/
class MatrixKeypad4x4_CR { // CR = Column input, Row output
public:
  MatrixKeypad4x4_CR();   //默认构造函数
  MatrixKeypad4x4_CR(const int rowPins[4],
                     const int colPins[4],
                     const char keymap[4][4]);   //带参构造函数

  // debounceMs：消抖时间，建议 20~30ms
  void begin(uint32_t debounceMs = 25);

  // 扫描并弹出一个事件（有事件返回 true）
  bool poll(KeyEvent &out);

  // 只取“按下”事件（你后面接 TextArea 最常用）
  bool getKey(char &k);

  // 开启某个键的长按连发（适合 '*' 退格）
  void enableRepeat(char repeatKey,
                    uint32_t firstDelayMs = 500,
                    uint32_t intervalMs   = 80);

private:
  void scanOnce(uint32_t now);
  void pushEvent(char k, KeyEventType t, uint32_t now);
  int8_t repeatConfigIndex(char k) const;

  inline void allRowsHigh();
  inline void driveOneRowLow(int r);

private:
  int _rowPins[4];
  int _colPins[4];
  char _map[4][4];

  uint32_t _debounceMs = 25;

  bool     _rawLast[4][4]    = {{0}};
  bool     _stable[4][4]     = {{0}};
  uint32_t _lastChange[4][4] = {{0}};

  // repeat
  static constexpr uint8_t REPEAT_KEY_MAX = 4;
  uint8_t  _repeatKeyCount = 0;
  char     _repeatKeys[REPEAT_KEY_MAX] = {0};
  uint32_t _repeatFirstDelay[REPEAT_KEY_MAX] = {0};
  uint32_t _repeatInterval[REPEAT_KEY_MAX] = {0};
  uint32_t _pressedAt[4][4] = {{0}};
  uint32_t _lastRepeatAt[4][4] = {{0}};

  // 简单事件队列（环形缓冲）
  static constexpr uint8_t QSIZE = 16;
  KeyEvent _q[QSIZE];
  uint8_t  _qh = 0, _qt = 0;
};
