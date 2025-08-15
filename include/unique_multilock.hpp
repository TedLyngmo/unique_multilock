#ifndef UNIQUE_MULTILOCK_HPP_A2BDEBD0_71FC_11F0_9AA4_90B11C0C0FF8
#define UNIQUE_MULTILOCK_HPP_A2BDEBD0_71FC_11F0_9AA4_90B11C0C0FF8
#include "detail/concepts.hpp"
#include "try_lock_for_until.hpp"

#include <chrono>
#include <concepts>
#include <cstdint>
#include <memory>
#include <mutex>
#include <system_error>
#include <utility>

namespace lyn {
template<class... Ms>
    requires((... && detail::Lockable<Ms>) || (sizeof...(Ms) == 1 && (... && detail::BasicLockable<Ms>)))
class unique_multilock {
public:
    // rule of 5 begin --------------------------------------------------------
    unique_multilock(const unique_multilock&) = delete;
    unique_multilock(unique_multilock&& other) noexcept :
        m_ms(std::exchange(other.m_ms, std::tuple<Ms*...>{})), m_locked(std::exchange(other.m_locked, false)) {}
    unique_multilock& operator=(const unique_multilock&) = delete;
    unique_multilock& operator=(unique_multilock&& other) noexcept {
        unique_multilock(std::move(other)).swap(*this);
        return *this;
    }
    ~unique_multilock() {
        if(m_locked) unlock();
    }
    // rule of 5 end ----------------------------------------------------------

    unique_multilock(Ms&... ms) : m_ms(std::addressof(ms)...) { lock(); }
    unique_multilock(std::defer_lock_t, Ms&... ms) noexcept : m_ms(std::addressof(ms)...) {}
    unique_multilock(std::adopt_lock_t, Ms&... ms) noexcept : m_ms(std::addressof(ms)...), m_locked(true) {}

    unique_multilock(std::try_to_lock_t, Ms&... ms)
        requires(... && detail::Lockable<Ms>)
        : m_ms(std::addressof(ms)...) {
        try_lock();
    }

    template<class Rep, class Period>
        requires(... && detail::TimedLockable<Ms>)
    unique_multilock(const std::chrono::duration<Rep, Period>& dur, Ms&... ms) : m_ms(std::addressof(ms)...) {
        try_lock_for(dur);
    }

    template<class Clock, class Duration>
        requires(... && detail::TimedLockable<Ms>)
    unique_multilock(const std::chrono::time_point<Clock, Duration>& tp, Ms&... ms) : m_ms(std::addressof(ms)...) {
        try_lock_until(tp);
    }

    void swap(unique_multilock& other) noexcept {
        std::swap(m_ms, other.m_ms);
        std::swap(m_locked, other.m_locked);
    }
    std::tuple<Ms*...> release() noexcept {
        m_locked = false;
        return std::exchange(m_ms, std::tuple<Ms*...>{});
    }
    std::tuple<Ms*...> mutex() const noexcept { return m_ms; }
    bool owns_lock() const noexcept { return m_locked; }
    explicit operator bool() const noexcept { return m_locked; }

    void unlock() {
        if(not m_locked) {
            throw std::system_error(std::make_error_code(std::errc::operation_not_permitted));
        }
        std::apply([](auto... ms) { (..., ms->unlock()); }, m_ms);
        m_locked = false;
    }

private:
    void lock_check() {
        if(m_locked) {
            throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur));
        }
        if constexpr(sizeof...(Ms) != 0) {
            if(std::get<0>(m_ms) == nullptr) {
                throw std::system_error(std::make_error_code(std::errc::operation_not_permitted));
            }
        }
    }

public:
    void lock() {
        lock_check();
        if constexpr(sizeof...(Ms) == 1) {
            std::get<sizeof...(Ms) - 1>(m_ms)->lock();
        } else if constexpr(sizeof...(Ms) > 1) {
            std::apply([](auto... ms) { std::lock(*ms...); }, m_ms);
        }
        m_locked = true;
    }

    int try_lock() // note: returns -1 for success like std::try_lock
        requires(... && detail::Lockable<Ms>)
    {
        lock_check();
        int rv;
        if constexpr(sizeof...(Ms) == 0) {
            rv = -1;
        } else if constexpr(sizeof...(Ms) == 1) {
            rv = -static_cast<int>(std::get<sizeof...(Ms) - 1>(m_ms)->try_lock());
        } else {
            rv = std::apply([](auto... ms) { return std::try_lock(*ms...); }, m_ms);
        }
        m_locked = rv == -1;
        return rv;
    }
    template<class Clock, class Duration>
        requires(... && detail::TimedLockable<Ms>)
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& tp) {
        lock_check();
        return m_locked = std::apply([&](auto... ms) { return lyn::try_lock_until(tp, *ms...); }, m_ms);
    }
    template<class Rep, class Period>
        requires(... && detail::TimedLockable<Ms>)
    bool try_lock_for(const std::chrono::duration<Rep, Period>& dur) {
        lock_check();
        return m_locked = std::apply([&](auto... ms) { return lyn::try_lock_for(dur, *ms...); }, m_ms);
    }

private:
    std::tuple<Ms*...> m_ms;
    bool m_locked = false;
};

template<class... Ms>
void swap(unique_multilock<Ms...>& lhs, unique_multilock<Ms...>& rhs) {
    lhs.swap(rhs);
}
} // namespace lyn
#endif
