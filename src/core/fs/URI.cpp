#include "URI.h"
#include <algorithm>

namespace vrutti::core::fs {

    URI::URI(std::string rawUri) : m_rawUri(std::move(rawUri)) {}

    void URI::parseIfNeeded() const {
        if (m_isParsed) return;
        m_isParsed = true;

        size_t len = m_rawUri.length();
        if (len == 0) return;

        // Simplified RFC 3986 parsing
        // format: scheme:[//authority]path[?query][#fragment]
        
        size_t pos = 0;
        
        // 1. Find Scheme (ends with ':')
        size_t schemeColon = m_rawUri.find(':');
        if (schemeColon != std::string::npos) {
            m_schemeEnd = static_cast<uint16_t>(schemeColon);
            pos = schemeColon + 1;
        }

        // 2. Find Authority (starts with "//")
        if (pos + 1 < len && m_rawUri[pos] == '/' && m_rawUri[pos + 1] == '/') {
            pos += 2;
            m_authorityStart = static_cast<uint16_t>(pos);
            
            // Authority ends at next '/', '?', or '#'
            size_t authEnd = m_rawUri.find_first_of("/?#", pos);
            if (authEnd == std::string::npos) {
                m_authorityEnd = static_cast<uint16_t>(len);
                pos = len;
            } else {
                m_authorityEnd = static_cast<uint16_t>(authEnd);
                pos = authEnd;
            }
        }

        // 3. Find Path (from end of authority/scheme to '?' or '#')
        m_pathStart = static_cast<uint16_t>(pos);
        size_t pathEnd = m_rawUri.find_first_of("?#", pos);
        if (pathEnd == std::string::npos) {
            m_pathEnd = static_cast<uint16_t>(len);
            pos = len;
        } else {
            m_pathEnd = static_cast<uint16_t>(pathEnd);
            pos = pathEnd;
        }

        // 4. Find Query (starts with '?')
        if (pos < len && m_rawUri[pos] == '?') {
            m_queryStart = static_cast<uint16_t>(pos + 1);
            size_t queryEnd = m_rawUri.find('#', pos);
            if (queryEnd == std::string::npos) {
                m_queryEnd = static_cast<uint16_t>(len);
                pos = len;
            } else {
                m_queryEnd = static_cast<uint16_t>(queryEnd);
                pos = queryEnd;
            }
        }

        // 5. Find Fragment (starts with '#')
        if (pos < len && m_rawUri[pos] == '#') {
            m_fragmentStart = static_cast<uint16_t>(pos + 1);
        }
    }

    std::string_view URI::scheme() const {
        parseIfNeeded();
        return m_schemeEnd > 0 ? std::string_view(m_rawUri.data(), m_schemeEnd) : std::string_view();
    }

    std::string_view URI::authority() const {
        parseIfNeeded();
        return m_authorityEnd > m_authorityStart 
            ? std::string_view(m_rawUri.data() + m_authorityStart, m_authorityEnd - m_authorityStart) 
            : std::string_view();
    }

    std::string_view URI::path() const {
        parseIfNeeded();
        return m_pathEnd > m_pathStart 
            ? std::string_view(m_rawUri.data() + m_pathStart, m_pathEnd - m_pathStart) 
            : std::string_view();
    }

    std::string_view URI::query() const {
        parseIfNeeded();
        return m_queryEnd > m_queryStart 
            ? std::string_view(m_rawUri.data() + m_queryStart, m_queryEnd - m_queryStart) 
            : std::string_view();
    }

    std::string_view URI::fragment() const {
        parseIfNeeded();
        return m_fragmentStart > 0 
            ? std::string_view(m_rawUri.data() + m_fragmentStart, m_rawUri.length() - m_fragmentStart) 
            : std::string_view();
    }

    const std::string& URI::toString() const {
        return m_rawUri;
    }

} // namespace vrutti::core::fs
