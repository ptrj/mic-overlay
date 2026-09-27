#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

// Frames pw-dump's consecutive JSON arrays; never derives mute from their contents.
class PipeWireEvents
{
public:
    struct Result { bool valid = true; bool frame = false; bool relevant = false; };
    Result feed(const QByteArray &bytes);
    void reset();

private:
    bool relevant(const QJsonArray &objects);
    QByteArray m_buffer;
    qsizetype m_scan = 0;
    qsizetype m_start = 0;
    int m_depth = 0;
    bool m_string = false;
    bool m_escape = false;
    QSet<int> m_sources;
    QHash<int, int> m_sourceDevices;
    QHash<int, QJsonObject> m_defaults;
};
