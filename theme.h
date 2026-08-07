#pragma once
#include <QApplication>
#include <QPalette>
#include <QColor>
#include <QStyleFactory>

inline void applyDarkTheme(QApplication& app)
{
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette p;
    p.setColor(QPalette::Window,          QColor(0x1e, 0x1e, 0x1e));
    p.setColor(QPalette::WindowText,      QColor(0xe0, 0xe0, 0xe0));
    p.setColor(QPalette::Base,            QColor(0x25, 0x25, 0x25));
    p.setColor(QPalette::AlternateBase,   QColor(0x2d, 0x2d, 0x2d));
    p.setColor(QPalette::Text,            QColor(0xe0, 0xe0, 0xe0));
    p.setColor(QPalette::Button,          QColor(0x2d, 0x2d, 0x2d));
    p.setColor(QPalette::ButtonText,      QColor(0xe0, 0xe0, 0xe0));
    p.setColor(QPalette::ToolTipBase,     QColor(0x25, 0x25, 0x25));
    p.setColor(QPalette::ToolTipText,     QColor(0xe0, 0xe0, 0xe0));
    p.setColor(QPalette::PlaceholderText, QColor(0x9e, 0x9e, 0x9e));
    p.setColor(QPalette::Highlight,       QColor(0x37, 0x79, 0xc2));
    p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Disabled, QPalette::Text,       QColor(0x7a, 0x7a, 0x7a));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x7a, 0x7a, 0x7a));
    app.setPalette(p);

    app.setStyleSheet(R"(
        #currentCh        { color:#87CEFA; font-weight:bold; background:#1b1b1b; border:1px solid #2c2c2c; border-radius:8px; padding:6px 12px; }
        #emptyLabel       { color: #888888; font-size: 14pt; }
        /* 공격 배너 (다크 톤 뮤트 레드) */
        #attackBanner     { background:#241618; border:1px solid #4f2a2d; border-radius:10px; }
        #attackLabel      { background:transparent; color:#e6cfcf; font-size:13px; padding:2px 6px; }
        #attackStopButton { background:#8f3a3a; color:#f0dede; font-weight:600; padding:8px 18px; border:none; border-radius:8px; }
        #attackStopButton:hover   { background:#9c4444; }
        #attackStopButton:pressed { background:#7c3232; }

        QPushButton          { padding:6px 12px; border:1px solid #555; border-radius:4px; background:#2d2d2d; }
        QPushButton:hover    { background:#3a3a3a; }
        QPushButton:checked  { background:#3779c2; color:white; border-color:#3779c2; }
        QPushButton:disabled { color:#777; }

        /* Start/Stop 토글 버튼 (다크 톤에 맞춘 뮤트 초록/빨강) */
        #scanButton                        { background:#2c5233; color:#dfe7e0; font-weight:600; border:1px solid #34613c; border-radius:6px; }
        #scanButton:hover                  { background:#33603c; }
        #scanButton[running="true"]        { background:#5e2b2b; color:#f0dede; border-color:#743636; }
        #scanButton[running="true"]:hover  { background:#6e3232; }

        QLineEdit { padding:6px; border:1px solid #555; border-radius:4px; background:#252525; color:#e0e0e0; }
        QComboBox { padding:6px 10px; border:1px solid #555; border-radius:4px; background:#252525; color:#e0e0e0; }
        QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right; width:36px; border:none; }
        QComboBox QAbstractItemView {
            background:#252525; color:#e0e0e0; border:1px solid #555;
            selection-background-color:#3779c2; selection-color:#ffffff; outline:0;
        }
        QComboBox QAbstractItemView::item { min-height:44px; padding:2px 8px; }
        QListView { background:#1e1e1e; border:1px solid #333; }

        QStatusBar { color:#d0d0d0; }
        QStatusBar::item { border:none; }

        /* 세그먼티드 컨트롤 (AP | STATION) */
        #segToggle { background:#202020; border:1px solid #303030; border-radius:8px; }
        #segToggle QPushButton { border:none; background:transparent; color:#9a9a9a; padding:4px 0; border-radius:6px; font-weight:600; }
        #segToggle QPushButton:hover:!checked { color:#dddddd; }
        #segToggle QPushButton:checked { background:#3779c2; color:#ffffff; }

        /* 스캔 대역 선택 다이얼로그 */
        #bandDialog            { background:#1e1e1e; }
        #bandCard              { background:#2a2a2a; border:1px solid #454545; border-radius:12px; }
        #bandTitle             { color:#e8e8e8; font-size:14px; font-weight:600; padding:2px 0 10px 0; }
        #bandCard QRadioButton { color:#e0e0e0; font-size:13px; font-weight:600; padding:4px 2px; }
    
    )");
}
