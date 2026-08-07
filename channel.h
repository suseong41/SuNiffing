#pragma once
#include <QString>
#include <QList>

class Channel
{
public:
    enum class Band { Only24, Only5, Dual };
    
    void probe(const QString& dev, Band band = Band::Dual);
    int next();
    void setChannel(int ch);
    void reset() { m_hopIdx = 0; }

private:
    static const QList<int> candidates;
    static const QList<int> defaultHops;
    static bool inBand(int ch, Band band);
    static QList<int> filterByBand(const QList<int>& in, Band band);
    
    QList<int> m_hopSeq = defaultHops;
    int m_hopIdx = 0;
};
