#ifndef CALCULATORENGINE_H
#define CALCULATORENGINE_H

#include <QString>

/**
 * @brief 计算器核心状态机（与界面完全解耦，便于单元测试）
 *
 * 设计要点：
 *  1. 所有输入（鼠标点击、键盘按键）最终都调用 enterDigit()/enterDot()/
 *     enterOperator()/backspace()/clearAll() 这同一组接口，因此两条输入通道
 *     天然复用同一套逻辑，不会出现行为差异。
 *  2. 用 m_pendingOperand 区分"正在录入的操作数"与"刚算出的结果"：
 *     刚算完结果时再次输入数字 → 开启新表达式；输入运算符 → 用结果继续算。
 *  3. 用 m_error 标记错误态（如除数为 0），错误态下除 C/AC 外的所有输入被丢弃。
 */
class CalculatorEngine
{
public:
    /// 运算状态
    enum class State {
        EnteringFirst,   ///< 正在录入第一个操作数
        EnteringSecond,  ///< 正在录入第二个操作数（已有挂起运算符）
        ResultShown,     ///< 已显示计算结果（= 之后）
        Error            ///< 错误态（如除数为 0）
    };

    CalculatorEngine();

    // ---- 输入接口（鼠标与键盘共用）----
    bool enterDigit(int digit);            ///< 输入数字 0~9
    bool enterDot();                       ///< 输入小数点
    bool enterOperator(QChar op);          ///< 输入四则运算符 + - × ÷
    bool equals();                         ///< 计算，二次按下重复上次运算
    bool backspace();                      ///< 退格（删除末位）
    bool clearEntry();                     ///< CE：仅清除当前操作数
    bool clearAll();                       ///< C / AC：清除全部状态
    bool toggleSign();                     ///< ±：正负号取反
    bool percent();                        ///< %：把当前操作数转成百分比

    // ---- 查询接口 ----
    QString displayText() const;           ///< 显示屏应显示的文本
    QString expressionText() const;        ///< 表达式提示行（如 "12 × 3"）
    State state() const { return m_state; }
    bool isError() const { return m_state == State::Error; }

    /// 内部数值，仅供单元测试断言使用
    double currentValue() const { return m_currentValue; }
    double accumulator() const { return m_accumulator; }
    QChar pendingOperator() const { return m_pendingOperator; }

    static constexpr int MaxDigits = 15;   ///< 操作数最大有效位数（防溢出）

private:
    void resetAll();
    void setError(const QString &message);
    QString formatNumber(double value) const;
    /** 计算 a op b，除数为 0 时返回 false */
    static bool apply(QChar op, double a, double b, double *out);

    State   m_state{State::EnteringFirst};
    QString m_input;             ///< 正在录入的数字串（保证 "0.10" 这类输入的显示保真）
    bool    m_pendingOperand{true}; ///< true = 下次输入数字要开启新操作数
    bool    m_hasDot{false};     ///< 当前操作数中是否已含小数点
    bool    m_justPressedOperator{false}; ///< 关联缺陷 BD-03 修复标记

    double  m_currentValue{0.0}; ///< 当前操作数的数值
    double  m_accumulator{0.0};  ///< 第一个操作数 / 中间累计结果
    QChar   m_pendingOperator{0};///< 挂起的运算符；为 0 表示没有
    QString m_errorMessage;      ///< 错误提示文本

    // 二次按 "=" 重复上次运算
    double  m_lastRightOperand{0.0};
    QChar   m_lastOperator{0};
    bool    m_repeatEnabled{false};
};

#endif // CALCULATORENGINE_H
