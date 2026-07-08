#pragma once
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>

namespace dev
{
enum Role
{
    MacRole = Qt::UserRole + 100, // QString (원본 MAC, 복사/공격용)
    EssidRole,                    // QString (ESSID 또는 "(Not Associated)"/"To: ..")
    PwrRole,                      // int (dBm, 정렬 키)
    ChRole,                       // int (채널, 0 = 미상/없음)
    TypeRole,                     // int (0=AP, 1=STATION)
    FadedRole,                    // bool (에이징)
    LastSeenRole,                 // qint64 (ms epoch)
    KeyRole,                      // QString ("type_mac")
    AttackingRole                 // bool (현재 공격 대상)
};
}

// SSID/MAC 검색 필터
class DeviceProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit DeviceProxy(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

    void setTypeFilter(int t) { m_type = t; beginFilterChange(); endFilterChange(Direction::Rows); }
    void setSearch(const QString& s) { m_search = s; beginFilterChange(); endFilterChange(Direction::Rows); }

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override;

private:
    int m_type = 0;
    QString m_search;
};

// ESSID | MAC | PWR | CH
class DeviceDelegate : public QStyledItemDelegate
{
public:
    explicit DeviceDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
