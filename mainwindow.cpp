#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QKeyEvent>
#include <QPushButton>
#include <QStyle>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setFocusPolicy(Qt::StrongFocus);   // 让窗口本身能接收键盘事件

    setupButtonMap();
    connectButtons();
    refreshDisplay();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ---------------------------------------------------------------- 初始化

void MainWindow::setupButtonMap()
{
    using KA = KeyAction;

    // 数字键：objectName 统一采用 btnDigit0 ~ btnDigit9
    for (int d = 0; d <= 9; ++d) {
        const QString name = QStringLiteral("btnDigit%1").arg(d);
        m_buttonActions.insert(name, KA::Digit);
        m_buttonDigits.insert(name, d);
    }

    // 其余功能键
    m_buttonActions.insert(QStringLiteral("btnDot"),        KA::Dot);
    m_buttonActions.insert(QStringLiteral("btnBackspace"),  KA::Backspace);
    m_buttonActions.insert(QStringLiteral("btnClearAll"),   KA::ClearAll);
    m_buttonActions.insert(QStringLiteral("btnClearEntry"), KA::ClearEntry);
    m_buttonActions.insert(QStringLiteral("btnSign"),       KA::ToggleSign);
    m_buttonActions.insert(QStringLiteral("btnPercent"),    KA::Percent);
    m_buttonActions.insert(QStringLiteral("btnEquals"),     KA::Equals);

    // 运算符键：objectName 采用 opAdd / opSub / opMul / opDiv，便于在样式表里统一配色
    struct { const char *name; QChar op; } ops[] = {
        { "opAdd", QLatin1Char('+') },
        { "opSub", QLatin1Char('-') },
        { "opMul", QChar(0x00D7) },   // ×
        { "opDiv", QChar(0x00F7) },   // ÷
    };
    for (const auto &item : ops) {
        m_buttonActions.insert(QString::fromLatin1(item.name), KA::Operator);
        m_buttonOperators.insert(QString::fromLatin1(item.name), item.op);
    }
}

void MainWindow::connectButtons()
{
    // 所有按键共用一个槽：靠 sender()->objectName() 分派，避免 19 个 connect 重复代码
    const QList<QPushButton *> buttons = findChildren<QPushButton *>();
    for (QPushButton *btn : buttons) {
        if (!m_buttonActions.contains(btn->objectName()))
            continue;
        connect(btn, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
        btn->setFocusPolicy(Qt::NoFocus);   // 焦点留在窗口上，避免按键被按钮吃掉
    }
}

// ---------------------------------------------------------------- 鼠标输入

void MainWindow::onButtonClicked()
{
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn)
        return;

    const QString name = btn->objectName();
    const KeyAction action = m_buttonActions.value(name, KeyAction::None);

    handleKeyInternal(action, m_buttonDigits.value(name, -1),
                      m_buttonOperators.value(name, QChar(0)));
}

// ---------------------------------------------------------------- 键盘输入

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    int digit = -1;
    QChar op = QChar(0);
    KeyAction action = KeyAction::None;
    QString highlightKey;   // 与本次按键等价的按钮 objectName，用于视觉联动

    const int key = event->key();

    // —— 数字：主键盘 0~9 与小键盘 0~9 ——
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        digit = key - Qt::Key_0;
        action = KeyAction::Digit;
        highlightKey = QStringLiteral("btnDigit%1").arg(digit);
    }
    // —— 四则运算符 ——
    else if (key == Qt::Key_Plus) {
        op = QLatin1Char('+');
        action = KeyAction::Operator;
        highlightKey = QStringLiteral("opAdd");
    } else if (key == Qt::Key_Minus) {
        op = QLatin1Char('-');
        action = KeyAction::Operator;
        highlightKey = QStringLiteral("opSub");
    } else if (key == Qt::Key_Asterisk || key == Qt::Key_X) {
        // 小键盘 * 与字母 x 都视为乘
        op = QChar(0x00D7);
        action = KeyAction::Operator;
        highlightKey = QStringLiteral("opMul");
    } else if (key == Qt::Key_Slash) {
        op = QChar(0x00F7);
        action = KeyAction::Operator;
        highlightKey = QStringLiteral("opDiv");
    } else {
        switch (key) {
        case Qt::Key_Period:                          // 主键盘 .
        case Qt::Key_Comma:
            action = KeyAction::Dot;
            highlightKey = QStringLiteral("btnDot");
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Equal:
            action = KeyAction::Equals;
            highlightKey = QStringLiteral("btnEquals");
            break;
        case Qt::Key_Backspace:
            action = KeyAction::Backspace;
            highlightKey = QStringLiteral("btnBackspace");
            break;
        case Qt::Key_Delete:
        case Qt::Key_Escape:                          // Esc / Delete → C
            action = KeyAction::ClearAll;
            highlightKey = QStringLiteral("btnClearAll");
            break;
        case Qt::Key_C:
            // C = 全部清除，Ctrl+C 不拦截
            if (!(event->modifiers() & Qt::ControlModifier)) {
                action = KeyAction::ClearAll;
                highlightKey = QStringLiteral("btnClearAll");
            }
            break;
        case Qt::Key_E:
            action = KeyAction::ClearEntry;
            highlightKey = QStringLiteral("btnClearEntry");
            break;
        case Qt::Key_F9:
            action = KeyAction::ToggleSign;
            highlightKey = QStringLiteral("btnSign");
            break;
        case Qt::Key_Percent:
            action = KeyAction::Percent;
            highlightKey = QStringLiteral("btnPercent");
            break;
        default:
            QMainWindow::keyPressEvent(event);        // 未识别的键交给父类，保持默认行为
            return;
        }
    }

    // 键盘与鼠标走完全相同的入口
    handleKeyInternal(action, digit, op);

    // 视觉反馈：按键按下时高亮对应按钮，松开后恢复（模拟真实点击）
    if (!highlightKey.isEmpty())
        setButtonHighlight(highlightKey, true);

    event->accept();
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    // 键盘抬起时取消所有按钮的"按下"外观，保持与鼠标点击一致的瞬时反馈
    const QList<QPushButton *> buttons = findChildren<QPushButton *>();
    for (QPushButton *btn : buttons) {
        if (btn->property("keyPressed").toBool()) {
            btn->setProperty("keyPressed", false);
            btn->style()->unpolish(btn);
            btn->style()->polish(btn);
        }
    }
    QMainWindow::keyReleaseEvent(event);
}

// ---------------------------------------------------------------- 统一逻辑入口
void MainWindow::handleKeyInternal(KeyAction action, int digit, QChar op)
{
    switch (action) {
    case KeyAction::Digit:
        m_engine.enterDigit(digit);
        break;
    case KeyAction::Dot:
        m_engine.enterDot();
        break;
    case KeyAction::Operator:
        m_engine.enterOperator(op);
        break;
    case KeyAction::Equals:
        m_engine.equals();
        break;
    case KeyAction::Backspace:
        m_engine.backspace();
        break;
    case KeyAction::ClearAll:
        m_engine.clearAll();
        break;
    case KeyAction::ClearEntry:
        m_engine.clearEntry();
        break;
    case KeyAction::ToggleSign:
        m_engine.toggleSign();
        break;
    case KeyAction::Percent:
        m_engine.percent();
        break;
    default:
        return;
    }
    refreshDisplay();
}

// ---------------------------------------------------------------- 界面刷新

void MainWindow::refreshDisplay()
{
    // 显示屏文字随位数自动缩放，避免超长数字被截断
    const QString text = m_engine.displayText();
    ui->lcdDisplay->setText(text);

    int fontSize = 34;
    if (text.length() > 10) fontSize = 28;
    if (text.length() > 14) fontSize = 22;
    if (text.length() > 18) fontSize = 18;
    ui->lcdDisplay->setStyleSheet(
        QStringLiteral("font-size: %1px; color: %2;")
            .arg(fontSize)
            .arg(m_engine.isError() ? QStringLiteral("#C0392B")
                                    : QStringLiteral("#1A1A1A")));

    ui->lblExpression->setText(m_engine.expressionText());

    // 运算符按下后高亮当前运算符，提示"它正在挂起"
    const QChar pending = m_engine.pendingOperator();
    const QList<QPair<QString, QChar>> opButtons = {
        { QStringLiteral("opAdd"), QLatin1Char('+') },
        { QStringLiteral("opSub"), QLatin1Char('-') },
        { QStringLiteral("opMul"), QChar(0x00D7) },
        { QStringLiteral("opDiv"), QChar(0x00F7) },
    };
    for (const auto &item : opButtons) {
        QPushButton *btn = findChild<QPushButton *>(item.first);
        if (!btn)
            continue;
        const bool active = (pending != QChar(0) && pending == item.second);
        btn->setProperty("active", active);
        // 重新应用样式表，使 [active="true"] 选择器生效
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

void MainWindow::setButtonHighlight(const QString &objectName, bool on)
{
    QPushButton *btn = findChild<QPushButton *>(objectName);
    if (!btn)
        return;
    btn->setProperty("keyPressed", on);
    btn->style()->unpolish(btn);
    btn->style()->polish(btn);
}
