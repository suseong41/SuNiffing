#pragma once
#include <QApplication>
#include <QPalette>
#include <QColor>
#include <QStyleFactory>

// 전역 다크 테마 (B-6): 흩어진 인라인 QSS 를 한곳으로 중앙화.
// 위젯별 스타일은 objectName 셀렉터로 지정 -> 코드/ .ui 에서 setStyleSheet 제거.
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
        #currentCh        { color: #87CEFA; font-weight: bold; }
        #emptyLabel       { color: #888888; font-size: 14pt; }
        /* 공격 배너 (모던 alert) */
        #attackBanner     { background:#2b1719; border:1px solid #6e2b30; border-radius:10px; }
        #attackLabel      { background:transparent; color:#f2dada; font-size:13px; padding:2px 6px; }
        #attackStopButton { background:#e5484d; color:white; font-weight:600; padding:8px 18px; border:none; border-radius:8px; }
        #attackStopButton:hover   { background:#f0575b; }
        #attackStopButton:pressed { background:#cf3d42; }

        QPushButton          { padding:6px 12px; border:1px solid #555; border-radius:4px; background:#2d2d2d; }
        QPushButton:hover    { background:#3a3a3a; }
        QPushButton:checked  { background:#3779c2; color:white; border-color:#3779c2; }
        QPushButton:disabled { color:#777; }

        QLineEdit { padding:6px; border:1px solid #555; border-radius:4px; background:#252525; }
        QComboBox { padding:6px; border:1px solid #555; border-radius:4px; background:#252525; }
        QListView { background:#1e1e1e; border:1px solid #333; }

        /* 세그먼티드 컨트롤 (AP | STATION) — 풀폭 얇은 탭바 */
        #segToggle { background:#202020; border:1px solid #303030; border-radius:8px; }
        #segToggle QPushButton { border:none; background:transparent; color:#9a9a9a; padding:4px 0; border-radius:6px; font-weight:600; }
        #segToggle QPushButton:hover:!checked { color:#dddddd; }
        #segToggle QPushButton:checked { background:#3779c2; color:#ffffff; }
    
    )");
}
