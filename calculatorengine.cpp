#include "calculatorengine.h"

#include <QLocale>
#include <QStringList>
#include <cmath>

namespace {
constexpr int kMaxDecimals = 10;   ///< 显示时最多保留的小数位
constexpr double kEpsilon = 1e-12; ///< 判断"是否为整数"的容差
} // namespace

CalculatorEngine::CalculatorEngine()
{
    resetAll();
}

// ---------------------------------------------------------------- 私有工具

void CalculatorEngine::resetAll()
{
    m_state = State::EnteringFirst;
    m_input = QStringLiteral("0");
    m_pendingOperand = true;
    m_hasDot = false;
    m_justPressedOperator = false;
    m_currentValue = 0.0;
    m_accumulator = 0.0;
    m_pendingOperator = QChar(0);
    m_errorMessage.clear();
    m_lastRightOperand = 0.0;
    m_lastOperator = QChar(0);
    m_repeatEnabled = false;
}

void CalculatorEngine::setError(const QString &message)
{
    m_state = State::Error;
    m_errorMessage = message;
    m_pendingOperand = true;
    m_pendingOperator = QChar(0);
    m_repeatEnabled = false;
}

QString CalculatorEngine::formatNumber(double value) const
{
    if (!std::isfinite(value))
        return QStringLiteral("Error");

    // 去掉浮点尾差：0.1+0.2 应为 0.3 而不是 0.30000000000000004
    double rounded = std::round(value * 1e10) / 1e10;
    if (std::fabs(rounded) < kEpsilon)
        rounded = 0.0;   // 避免出现 "-0"

    // 整数直接输出，不显示 ".0"
    if (std::fabs(rounded - std::llround(rounded)) < kEpsilon
        && std::fabs(rounded) < 1e15) {
        return QString::number(static_cast<qlonglong>(std::llround(rounded)));
    }

    QString text = QString::number(rounded, 'f', kMaxDecimals);
    while (text.endsWith(QLatin1Char('0')))
        text.chop(1);
    if (text.endsWith(QLatin1Char('.')))
        text.chop(1);
    return text;
}

bool CalculatorEngine::apply(QChar op, double a, double b, double *out)
{
    if (op == QChar(0x00D7) || op == QLatin1Char('*')) {        // ×
        *out = a * b;
    } else if (op == QChar(0x00F7) || op == QLatin1Char('/')) { // ÷
        if (std::fabs(b) < kEpsilon)
            return false;                                       // 除数为 0
        *out = a / b;
    } else if (op == QLatin1Char('+')) {
        *out = a + b;
    } else if (op == QLatin1Char('-') || op == QChar(0x2212)) {  // - / −
        *out = a - b;
    } else {
        *out = b;   // 无运算符时直接返回第二个操作数
    }
    return std::isfinite(*out);
}

// ---------------------------------------------------------------- 数字与小���点

bool CalculatorEngine::enterDigit(int digit)
{
    if (digit < 0 || digit > 9)
        return false;

    if (m_state == State::Error)
        return false;                 // 错误态：除 C 外一律忽略

    // 结果状态下输入数字 → 丢弃旧结果，开始一条全新表达式
    if (m_state == State::ResultShown) {
        resetAll();
    }

    if (m_pendingOperand) {           // 新操作数：用本次按键覆盖显示
        m_input = QString::number(digit);
        m_hasDot = false;
        m_pendingOperand = false;
        m_justPressedOperator = false;
    } else {
        // 防御：允许重新点击运算符后继续补位（BD-03 修复点）
        if (m_input.size() >= MaxDigits)
            return false;             // 达到位数上限，忽略多余输入
        if (m_input == QLatin1String("0"))
            m_input = QString::number(digit);   // 去掉前导 0
        else
            m_input.append(QString::number(digit));
    }

    m_currentValue = m_input.toDouble();
    // 状态推导不能写成 m_state != ResultShown：刚 resetAll 时 state 已是
    // EnteringFirst，会把 ResultShown 状态提前覆盖，导致错误提示/表达式行丢失
    m_state = (m_pendingOperator == QChar(0)) ? State::EnteringFirst
                                              : State::EnteringSecond;
    m_justPressedOperator = false;
    return true;
}

bool CalculatorEngine::enterDot()
{
    if (m_state == State::Error)
        return false;                 // 错误态：小数点同样被忽略

    if (m_state == State::ResultShown) {
        resetAll();                   // 结果状态下按小数点：从 0. 开始新输入
    }

    if (m_pendingOperand) {           // 新操作数直接以 "0." 开头
        m_input = QStringLiteral("0.");
        m_pendingOperand = false;
        m_hasDot = true;
        m_currentValue = 0.0;
        m_justPressedOperator = false;
        m_state = (m_pendingOperator == QChar(0)) ? State::EnteringFirst
                                                  : State::EnteringSecond;
        return true;
    }

    if (m_hasDot)
        return false;                 // 同一操作数已有小数点 → 回落：忽略本次输入，
                                      // 不再追加，防止出现 "1..2" 这种非法数字串

    m_input.append(QLatin1Char('.'));
    m_hasDot = true;
    return true;
}

// ---------------------------------------------------------------- 运算符与等号

bool CalculatorEngine::enterOperator(QChar op)
{
    // 统一运算符写法：* → ×，/ → ÷，− → -
    if (op == QLatin1Char('*'))
        op = QChar(0x00D7);
    else if (op == QLatin1Char('/'))
        op = QChar(0x00F7);
    else if (op == QChar(0x2212))
        op = QLatin1Char('-');

    if (m_state == State::Error)
        return false;

    if (m_justPressedOperator) {
        // 连续点击运算符：仅替换运算符，不触发计算（不当作第二个操作数）
        m_pendingOperator = op;
        m_state = State::EnteringSecond;
        return true;
    }

    if (m_state == State::ResultShown) {
        // 结果后直接接运算符：以上一次结果作为第一个操作数继续运算
        m_accumulator = m_currentValue;
        m_pendingOperator = op;
        m_pendingOperand = true;
        m_hasDot = false;
        m_repeatEnabled = false;
        m_state = State::EnteringSecond;
        m_justPressedOperator = true;
        return true;
    }

    if (m_pendingOperator != QChar(0) && !m_pendingOperand) {
        // 已有挂起运算符且第二个操作数已录入 → 先算出中间结果（连续运算 2+3+4）
        double out = 0.0;
        if (!apply(m_pendingOperator, m_accumulator, m_currentValue, &out)) {
            setError(QStringLiteral("除数不能为 0"));
            return false;
        }
        m_accumulator = out;
        m_currentValue = out;         // 中间结果立即显示，便于用户确认
    } else {
        m_accumulator = m_currentValue;
    }

    m_pendingOperator = op;
    m_pendingOperand = true;
    m_hasDot = false;
    m_repeatEnabled = false;
    m_state = State::EnteringSecond;
    m_justPressedOperator = true;
    return true;
}

bool CalculatorEngine::equals()
{
    if (m_state == State::Error)
        return false;

    // 二次按 "=" → 用上次的右操作数与运算符重复计算
    if (m_state == State::ResultShown && m_repeatEnabled) {
        double out = 0.0;
        if (!apply(m_lastOperator, m_currentValue, m_lastRightOperand, &out)) {
            setError(QStringLiteral("除数不能为 0"));
            return false;
        }
        m_currentValue = out;
        return true;
    }

    if (m_pendingOperator == QChar(0)) {
        // 没有运算符：纯数字按 "=" 保持原值
        m_accumulator = m_currentValue;
        m_state = State::ResultShown;
        m_pendingOperand = true;
        return true;
    }

    // "5 + =" 这类缺右侧操作数的情形：把第一个操作数当作第二个操作数
    double right = m_pendingOperand ? m_accumulator : m_currentValue;

    double out = 0.0;
    if (!apply(m_pendingOperator, m_accumulator, right, &out)) {
        setError(QStringLiteral("除数不能为 0"));
        return false;
    }

    m_lastOperator = m_pendingOperator;
    m_lastRightOperand = right;
    m_repeatEnabled = true;

    m_currentValue = out;
    m_accumulator = out;
    m_input = formatNumber(out);
    m_pendingOperator = QChar(0);
    m_pendingOperand = true;
    m_hasDot = m_input.contains(QLatin1Char('.'));
    m_state = State::ResultShown;
    m_justPressedOperator = false;
    return true;
}

// ---------------------------------------------------------------- 退格与清除

bool CalculatorEngine::backspace()
{
    if (m_state == State::Error)
        return false;

    if (m_state == State::ResultShown) {
        // 结果状态退格：先回到可编辑状态，再从结果末尾删一位
        m_state = State::EnteringFirst;
        m_pendingOperand = false;
        m_input = formatNumber(m_currentValue);
        m_repeatEnabled = false;
    }

    if (m_pendingOperand)
        return false;                 // 还没开始录入，退格无效

    m_input.chop(1);
    if (m_input.isEmpty() || m_input == QLatin1String("-")) {
        m_input = QStringLiteral("0");
        m_pendingOperand = false;
    }
    // 关键：退格可能删掉小数点（"1.5" → "1."），也可能删回整数，
    // 因此 m_hasDot 必须由当前字符串重新推导，而不是沿用旧值——
    // 旧值残留会让后续 "." 被误判为"重复小数点"而拒绝输入（缺陷 BD-04）
    m_hasDot = m_input.contains(QLatin1Char('.'));
    m_currentValue = m_input.toDouble();
    m_justPressedOperator = false;
    // 状态必须按"是否已有挂起运算符"重算，而不是只看 m_pendingOperand，
    // 否则在第二个操作数里退格会让状态回退到 EnteringFirst，后续数字丢失
    m_state = (m_pendingOperator == QChar(0)) ? State::EnteringFirst
                                              : State::EnteringSecond;
    return true;
}

bool CalculatorEngine::clearEntry()
{
    if (m_state == State::Error) {
        resetAll();
        return true;
    }
    m_input = QStringLiteral("0");
    m_currentValue = 0.0;
    m_pendingOperand = true;
    m_hasDot = false;
    m_justPressedOperator = false;
    if (m_state == State::ResultShown) {
        resetAll();
        return true;
    }
    m_state = (m_pendingOperator == QChar(0)) ? State::EnteringFirst
                                              : State::EnteringSecond;
    return true;
}

bool CalculatorEngine::clearAll()
{
    resetAll();
    return true;
}

// ---------------------------------------------------------------- 其他运算

bool CalculatorEngine::toggleSign()
{
    if (m_state == State::Error)
        return false;

    m_currentValue = -m_currentValue;
    if (m_pendingOperand) {
        // 尚未录入操作数时按 ±：以 -0 作为输入起点
        m_input = formatNumber(m_currentValue);
        m_pendingOperand = false;
        m_state = State::EnteringFirst;
    } else if (m_input.startsWith(QLatin1Char('-'))) {
        m_input.remove(0, 1);
    } else {
        m_input.prepend(QLatin1Char('-'));
    }
    m_hasDot = m_input.contains(QLatin1Char('.'));
    return true;
}

bool CalculatorEngine::percent()
{
    if (m_state == State::Error)
        return false;

    // 语义：百分比作用于当前操作数；处于 "a + b%" 时按 a 的百分比计算，符合手机计算器习惯
    double base = (m_pendingOperator != QChar(0) && !m_pendingOperand) ? m_accumulator
                                                                       : 1.0;
    if (m_pendingOperator != QChar(0) && m_pendingOperator != QChar(0x00D7)
        && m_pendingOperator != QChar(0x00F7) && !m_pendingOperand) {
        base = m_accumulator;                       // a + b% → a + a*b/100
        m_currentValue = m_accumulator * m_currentValue / 100.0;
    } else {
        m_currentValue = m_currentValue / 100.0;
        Q_UNUSED(base);
    }

    m_input = formatNumber(m_currentValue);
    m_pendingOperand = false;
    m_hasDot = m_input.contains(QLatin1Char('.'));
    m_state = (m_pendingOperator == QChar(0)) ? State::EnteringFirst
                                              : State::EnteringSecond;
    return true;
}

// ---------------------------------------------------------------- 显示

QString CalculatorEngine::displayText() const
{
    if (m_state == State::Error)
        return m_errorMessage;
    if (m_pendingOperand && m_pendingOperator != QChar(0))
        return formatNumber(m_accumulator);   // 刚按下运算符：显示第一个操作数
    return m_input;
}

QString CalculatorEngine::expressionText() const
{
    if (m_state == State::Error)
        return QString();

    if (m_state == State::ResultShown)
        return QStringLiteral("%1 %2 %3 =")
            .arg(formatNumber(m_accumulator), QString(m_lastOperator),
                 formatNumber(m_lastRightOperand));

    if (m_pendingOperator == QChar(0))
        return QString();

    return QStringLiteral("%1 %2").arg(formatNumber(m_accumulator),
                                       QString(m_pendingOperator));
}
