#pragma once
#include <Arduino.h>

//先删 _candPy[MAX_CAND  ..去找一下

//candidate，候选人
// 输入法当前所处的模式
// 中文模式：数字键用于九键拼音
// 英文模式：当前版本只是占位，后续可扩展为多击英文/T9
// 数字模式：数字键直接上屏
// : uint8_t 表示这个枚举底层用 1 字节无符号整数来存储，更省内存
// 这在 ESP32 这种嵌入式平台上是很常见的写法

enum InputMode : uint8_t {
    INPUT_CN = 0,
    INPUT_EN,
    INPUT_NUM
};

//拼音和汉字候选的枚举
enum CnSelectStage : uint8_t {
    CN_STAGE_PY = 0,   // 当前在拼音候选行
    CN_STAGE_HZ        // 当前在汉字候选行
};

// NineKeyIME：九键输入法核心类
// 这个类不直接负责 LVGL 界面，也不直接负责 MQTT 发送。
// 它只负责两件事：
// 1. 接收按键
// 2. 维护输入法内部状态（当前模式、当前数字串、候选、最终文本等）
class NineKeyIME {
public:
    // 初始化输入法到默认状态
    void begin();

    // 输入法统一按键入口
    // 外部只要把键值（如 '2'、'A'、'#'）交给它即可
    void onKey(char key);

    // 清空已经确认上屏的正文文本
    void clearText();

    // 清空“当前正在拼”的内容：数字串 + 候选
    // 注意：不会清掉已经确认的正文 _text
    void clearComposition();

    // 返回已经确认好的正文文本
    const String& text() const { return _text; }

    //_text开机时读取Flash中保存的
    void setText(const String& s);

    // 返回当前正在输入的九键数字串，例如 "64" / "426"
    const String& digits() const { return _digits; }

    // 返回当前模式名称，给界面显示用
    String modeName() const;

    // 返回当前选中的候选字/词
    String currentCandidate() const;

    // 返回一个适合直接显示在界面上的候选预览串
    // 例如："[你] 呢 泥 拟"
    String candidatesPreview(uint8_t maxShow = 6) const;

    // 新增：拼音候选预览、汉字候选预览
    String pyCandidatesPreview(uint8_t maxShow = 4) const;
    String hzCandidatesPreview(uint8_t maxShow = 6) const;

    // 当前是否在拼音候选行
    bool selectingPinyin() const { return _cnStage == CN_STAGE_PY; }

    // 取走“发送请求”标志。
    // 这是一次性事件：取走后会自动清零。
    bool takeSendFlag();

    // 取走“返回请求”标志。
    // 同样是一次性事件：取走后会自动清零。
    bool takeBackFlag();

    uint16_t textCursor() const { return _textCursor; }

private:
    // 当前候选列表最多缓存多少个候选项
    // 不是词库总量，而是一次查找后内部最多存多少候选
    static const uint8_t MAX_HZ_CAND = 48;
    static const uint8_t MAX_PY_CAND = 8;
    static const uint8_t MAX_USER_FREQ = 48;

    //自学习频次表
    struct UserFreqEntry {
    String text;
    uint16_t freq;
    };


    //谁出现的频次多，谁就往前靠
    UserFreqEntry _userFreq[MAX_USER_FREQ];
    uint8_t _userFreqCount = 0;

    //记录上一个已经上屏的字或词，用于做简单的上下文联想
    String _lastCommitted;

    // 当前输入模式，默认中文
    InputMode _mode = INPUT_CN;

    // 已经确认上屏的最终文本
    // 比如用户依次确认了“你”“好”，那这里就会是“你好”
    String _text;

    // 当前还没确认的九键数字串
    // 例如输入 n-i 对应的是 6-4，那么这里会暂存 "64"
    //String是一个类，所以可以有_digits.length()和_digits.remove()这种，length()和remove()是String的类成员函数
    String _digits;    //这个就是TTF上编码上显示的数字数组

    // 当前候选文本列表
    // 例如“64”可能得到：你 / 呢 / 泥 / 拟 / 妮
    String _candText[MAX_HZ_CAND];   //候选数组

    // 当前候选对应的拼音来源
    // 当前版本主要是保存起来，后面你想显示“候选来自哪个拼音”时能用到
    String _candPy[MAX_PY_CAND];     //候选数组


    int16_t _pyScore[MAX_PY_CAND] = {0};
    int16_t _hzScore[MAX_HZ_CAND] = {0};

    // // 当前候选个数
    // uint8_t _candCount = 0;

    // // 当前选中的候选索引
    // // 例如 _sel = 0 表示当前选中第一个候选
    // uint8_t _sel = 0;

    //拼音个数和索引
    uint8_t _pyCount = 0;
    uint8_t _pySel   = 0;

    //汉字的个数和索引
    uint8_t _hzCount = 0;
    uint8_t _hzSel   = 0;

    //_cnStage = CN_STAGE_HZ;或_cnStage = CN_STAGE_PY;
    //_cnStage就是一个状态标志，表示当前操作阶段是拼音候选阶段还是汉字候选阶段
    CnSelectStage _cnStage = CN_STAGE_PY;  //这个作用是什么？

    //这是文本区的光标索引
    uint16_t _textCursor = 0;

    // 是否触发了发送请求（由 # 键触发）
    bool _sendFlag = false;

    // 是否触发了返回请求（由 0 键在“无当前拼音”时触发）
    bool _backFlag = false;

    // 切换到下一个输入模式：中文 -> 英文 -> 数字 -> 中文 ...
    void nextMode();

    // 不同模式下的按键处理函数
    void handleChineseKey(char key);
    void handleEnglishKey(char key); // 当前只是占位实现
    void handleNumberKey(char key);  // 简单数字输入

    void addUtf8CandidatesFromGroup(const char* py, const char* cands);

    // void addPhraseCandidatesFromGroup(const char* py, const char* cands);
    void addPhraseCandidatesFromGroup(const char* py, const char* cands, int16_t baseScore);

    // 根据当前 _digits 重新生成候选列表
    void rebuildCandidates();

    //这4个函数是通过拼音锁定汉字的函数
    void rebuildHanziFromSelectedPinyin();
    void clearHanziCandidates();

    // void addPyCandidate(const char* py);
    bool pyCandidateExists(const char* py) const;

    // 清空当前候选缓存
    void clearCandidates();

    
    // // 向候选列表中加入一个候选（如果未满且未重复）
    // void addCandidate(const char* py, const char* hz);
    int8_t findPyCandidateIndex(const char* py) const;
    int8_t  findCandidateIndex(const char* hz) const;
    //默认参数只写在声明里，不写在定义里
    void addCandidate(const char* py, const char* hz, int16_t score = 0);  //这里的第三个参数默认给了0
    void addPyCandidate(const char* py, int16_t score);
    // 检查某个候选是否已经存在，用于去重
    bool candidateExists(const char* hz) const;

    // 把当前选中的候选确认上屏，追加到 _text 里
    void commitCurrentCandidate();

    // 把一个拼音字母映射到九键数字
    // 例如 a/b/c -> 2, d/e/f -> 3 ...
    char letterToDigit(char c) const;

    // 把整段拼音（如 "nihao"）转换成九键数字串（如 "64426"）
    String pinyinToDigits(const char* py) const;

    // 删除 String 中最后一个 UTF-8 字符
    // 这个函数很重要，因为汉字不是单字节，不能简单删掉最后一个字节
    void deleteLastUtf8Char(String &s);

    uint16_t utf8CharCount(const String &s) const;
    uint16_t utf8ByteOffset(const String &s, uint16_t charPos) const;

    void moveTextCursorLeft();
    void moveTextCursorRight();

    void insertAtTextCursor(const String &ins);
    void deleteBeforeTextCursor();

    String textCursorPreview() const;

    void sortPyCandidates();                    //按分数排序拼音候选
    void sortHanziCandidates();                 //按分数排序汉字/词组候选

    int16_t userBonusFor(const String& text) const;         //查询这个候选的用户频次加分
    int16_t contextBonusFor(const String& text) const;      //查询这个候选的上下文加分
    void touchUserFrequency(const String& text);            //某候选被确认后，给它频次加1
    void rememberCommittedText(const String& text);         //记住这个刚被确认的词


    /********************
     * 掉电不丢失:把自学习频次表 + 上一次已上屏词保存到NVS
     * 这个自学习是放在IME内部完成，外部的main.cpp不需要手动管理这部分数据
     * 
     * 注意：
     * void saveLearningToNvs() const; 这里的const ->这个成员函数在调用时，不允许修改当前对象的成员状态
     * 也就是说这个类里面的_textCursor这些参数，这个函数是没权修改的，就是它有个const在修饰了。
     * 如果没有const修饰，那这个函数就有权去修改这个类对象的变量参数，比如void moveTextCursorLeft();这个函数就可以修改_textCursor
     * ************** */
    void loadLearningFromNvs();                     //开机恢复学习数据
    void saveLearningToNvs() const;                 //把学习数据写入NVS
    String packLearningData() const;                //把内存里的学习状态把包成字符串
    void unpackLearningData(const String& blob);    //把字符串还原成内存结构

};
