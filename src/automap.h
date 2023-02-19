#pragma once

#include <QObject>

#include <concepts>

namespace QNVim::Internal {

class AutoMapBase : public QObject {
    Q_OBJECT
  protected:
    explicit AutoMapBase(QObject *parent = nullptr) : QObject(parent){};
};

template<typename T>
concept QObjectBasedPointer =
    std::is_pointer_v<T> && std::is_base_of_v<QObject, std::remove_pointer_t<T>>;

/**
 * Map, that automatically deletes pairs, when key or value is deleted,
 * given that either a key or a value is a pointer to QObject.
 */
template <typename K, typename V>
requires (QObjectBasedPointer<K> || QObjectBasedPointer<V>)
class AutoMap : public AutoMapBase {
  public:
    explicit AutoMap(QObject *parent = nullptr) : AutoMapBase(parent){};

    V &at(const K &key) {
        return m_map.at(key);
    }

    auto begin() { return m_map.begin(); };
    auto end() { return m_map.end(); };
    auto cbegin() const { return m_map.cbegin(); };
    auto cend() const { return m_map.cend(); };

    auto contains(const K& key) const {
        return m_map.contains(key);
    }

    auto find(const K &key) {
        return m_map.find(key);
    }

    auto find(const K &key) const {
        return m_map.find(key);
    }

    auto insert(const std::pair<K, V> &v) {
        auto result = m_map.insert(v);

        if (!result.second)
            return result;

        if constexpr (std::is_base_of_v<QObject, std::remove_pointer<K>>)
            connect(v.first, &QObject::destroyed, this, [=]() {
                m_map.erase(v.first);
            });

        if constexpr (std::is_base_of_v<QObject, std::remove_pointer<V>>)
            connect(v.second, &QObject::destroyed, this, [=]() {
                m_map.erase(v.first);
            });

        return result;
    }

    auto insert_or_assign(const K &k, V &&v) {
        auto it = m_map.find(k);

        if (it == m_map.end()) {
            auto [it, _] = this->insert({k, v});
            return std::make_pair(it, true);
        } else {
            if constexpr (std::is_base_of_v<QObject, std::remove_pointer<V>>) {
                it->second->disconnect(this);
                connect(v, &QObject::destroyed, this, [=]() {
                    m_map.erase(k);
                });
            }

            it->second = v;
            return std::make_pair(it, false);
        }
    }

  private:
    std::unordered_map<K, V> m_map;
};

} // namespace QNVim::Internal
