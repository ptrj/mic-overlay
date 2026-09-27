#include "pipewireevents.h"

#include <QJsonDocument>
#include <QJsonParseError>

PipeWireEvents::Result PipeWireEvents::feed(const QByteArray &bytes)
{
    Result result;
    m_buffer += bytes;
    if (m_buffer.size() > 8 * 1024 * 1024) {
        result.valid = false;
        return result;
    }
    for (; m_scan < m_buffer.size(); ++m_scan) {
        const char ch = m_buffer.at(m_scan);
        if (m_depth == 0) {
            if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') {
                continue;
            }
            if (ch != '[') {
                result.valid = false;
                return result;
            }
            m_start = m_scan;
            m_depth = 1;
            continue;
        }
        if (m_string) {
            if (m_escape) {
                m_escape = false;
            } else if (ch == '\\') {
                m_escape = true;
            } else if (ch == '"') {
                m_string = false;
            }
            continue;
        }
        if (ch == '"') {
            m_string = true;
        } else if (ch == '[' || ch == '{') {
            if (++m_depth > 128) {
                result.valid = false;
                return result;
            }
        } else if (ch == ']' || ch == '}') {
            --m_depth;
            if (m_depth == 0) {
                QJsonParseError error;
                const auto document = QJsonDocument::fromJson(
                    m_buffer.mid(m_start, m_scan - m_start + 1), &error);
                if (error.error != QJsonParseError::NoError || !document.isArray()) {
                    result.valid = false;
                    return result;
                }
                for (const auto &entry : document.array()) {
                    if (!entry.isObject() || entry.toObject().value("id").toInt(-1) < 0) {
                        result.valid = false;
                        return result;
                    }
                }
                result.frame = true;
                // Always process every frame, even if a prior one was relevant.
                const bool changed = relevant(document.array());
                result.relevant = result.relevant || changed;
                m_buffer.remove(0, m_scan + 1);
                m_scan = -1;
            }
        }
    }
    if (m_depth == 0) {
        m_buffer.clear();
        m_scan = 0;
    }
    return result;
}

bool PipeWireEvents::relevant(const QJsonArray &objects)
{
    bool changed = false;
    for (const auto &entry : objects) {
        const auto object = entry.toObject();
        const int id = object.value("id").toInt();
        // Hardware mute/route changes may arrive only on the backing Device,
        // without an accompanying update to the capture Node.
        const bool captureDevice = m_sourceDevices.values().contains(id);
        if (object.value("info").isNull() || object.value("props").isNull()) {
            const bool source = m_sources.remove(id);
            m_sourceDevices.remove(id);
            const bool metadata = m_defaults.remove(id) > 0;
            changed = changed || source || metadata || captureDevice;
            continue;
        }
        const auto info = object.value("info").toObject();
        const auto props = info.value("props").toObject();
        const QString mediaClass = props.value("media.class").toString();
        const bool wasSource = m_sources.contains(id);
        if (mediaClass.startsWith("Audio/Source")) {
            m_sources.insert(id);
            if (props.contains("device.id")) {
                bool valid = false;
                const int device = props.value("device.id").toVariant().toInt(&valid);
                if (valid && device >= 0) {
                    m_sourceDevices.insert(id, device);
                } else {
                    m_sourceDevices.remove(id);
                }
            }
            changed = true;
        } else if (wasSource) {
            if (!mediaClass.isEmpty()) {
                m_sources.remove(id);
                m_sourceDevices.remove(id);
            }
            changed = true;
        }

        changed = changed || captureDevice;

        const auto metadata = object.value("metadata").toArray();
        QJsonObject captureDefaults;
        for (const auto &item : metadata) {
            const auto property = item.toObject();
            const QString key = property.value("key").toString();
            if (property.value("subject").toInt(-1) == 0
                && (key == "default.audio.source" || key == "default.configured.audio.source")) {
                captureDefaults.insert(key, property.value("value"));
            }
        }
        if (object.contains("metadata")
            && (m_defaults.contains(id) || !captureDefaults.isEmpty()
                || object.value("props").toObject().value("metadata.name").toString() == "default")) {
            if (!m_defaults.contains(id) || m_defaults.value(id) != captureDefaults) {
                changed = true;
            }
            m_defaults.insert(id, captureDefaults);
        }
        // Client, port, playback-only device/node and unrelated metadata events are ignored.
    }
    return changed;
}

void PipeWireEvents::reset()
{
    *this = PipeWireEvents();
}
