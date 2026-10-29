#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>

#include "calculatorengine.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @brief 计算器主窗口
 *
 * 职责划分：
 *  - MainWindow：只负责"把按键/按键事件翻译成引擎调用"，再"把引擎状态刷到界面"。
 *  - CalculatorEngine：承担全部运算逻辑与输入状态管理。
 * 这样鼠标与键盘共用同一条路径：onButtonClicked() 与 keyPressEvent() 都只调用
 * handleKeyInternal()，真正做到一套逻辑两种输入。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;   ///< 键盘事件处理
    void keyReleaseEvent(QKeyEvent *event) override; ///< 键盘抬起：恢复按钮外观

private slots:
    void onButtonClicked();                          ///< 鼠标点击（信号自动连接，按 objectName 分派）

private:
    enum class KeyAction {
        Digit, Dot, Operator, Equals, Backspace, ClearAll, ClearEntry,
        ToggleSign, Percent, None
    };

    void setupButtonMap();                           ///< 建立 按钮 objectName → 按键语义 的映射表
    void connectButtons();                           ///< 批量绑定 clicked 信号（避免逐个手写）
    void handleKeyInternal(KeyAction action, int digit, QChar op); ///< 统一入口
    void refreshDisplay();                           ///< 引擎状态 → 界面
    void setButtonHighlight(const QString &objectName, bool on); ///< 键盘联动高亮

    Ui::MainWindow *ui;
    CalculatorEngine m_engine;

    QMap<QString, KeyAction> m_buttonActions;        ///< 按钮 → 动作
    QMap<QString, int>       m_buttonDigits;         ///< 数字按钮 → 数字值
    QMap<QString, QChar>     m_buttonOperators;      ///< 运算符按钮 → 运算符
    QMap<int, QString>       m_keyToButton;          ///< 键盘按键 → 按钮 objectName（高亮用）
    QString m_lastHighlighted;
};
#endif // MAINWINDOW_H
