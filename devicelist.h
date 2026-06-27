#pragma once
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>

// 모델 행에 저장하는 커스텀 role (itemRole 과 겹치지 않게 +100 부터)
namespace dev
{
enum Role
{
    MacRole = Qt::UserRole + 100, // QString (원본 MAC, 복사/공격용)
    EssidRole,                    // QString (ESSID 또는 "(Not Associated)"/"To: ..")
    PwrRole,                      // int (dBm, 정렬 키)
    ChRole,                       // int (채널, 0 = 미상/없음)
    TypeRole,                     // int (0=AP, 1=STATION)
    FadedRole,                    // bool (에이징 흐림)
    LastSeenRole,                 // qint64 (ms epoch)
    KeyRole,                      // QString ("type_mac")
    AttackingRole                 // bool (현재 공격 대상 = 배지)
};
}

// 타입(AP/STATION) + SSID 검색 필터. 정렬은 setSortRole(PwrRole) 로 위임.
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

// 카드형 행 렌더링: ESSID(굵게) + MAC(회색) / PWR(신호색) + CH
class DeviceDelegate : public QStyledItemDelegate
{
public:
    explicit DeviceDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
};
