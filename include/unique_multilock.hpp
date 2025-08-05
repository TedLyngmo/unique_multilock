#ifndef UNIQUE_MULTILOCK_HPP_A2BDEBD0_71FC_11F0_9AA4_90B11C0C0FF8
#define UNIQUE_MULTILOCK_HPP_A2BDEBD0_71FC_11F0_9AA4_90B11C0C0FF8

#include <chrono>
#include <concepts>
#include <cstdint>
#include <mutex>
#include <utility>

namespace lyn {
template<class T>
concept BasicLockable = requires(T t) {
    t.lock();
    t.unlock();
};

template<class T>
concept Lockable = BasicLockable<T> && requires(T t) {
    { t.try_lock() } -> std::same_as<bool>;
};

template<class T>
concept TimedLockable = Lockable<T> && requires(T t) {
    { // using an unusal ratio to avoid false positives for hardcoded duration types
        t.try_lock_for(std::chrono::duration<std::int_least32_t, std::ratio<1, 999999999>>{})
    } -> std::same_as<bool>;
    { t.try_lock_until(std::chrono::steady_clock{}) } -> std::same_as<bool>;
};

template<class... Ms>
    requires((... && Lockable<Ms>) || (sizeof...(Ms) == 1 && (... && BasicLockable<Ms>)))
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

    unique_multilock(Ms&... ms) : m_ms(&ms...) { lock(); }
    unique_multilock(std::defer_lock_t, Ms&... ms) noexcept : m_ms(&ms...) {}
    unique_multilock(std::adopt_lock_t, Ms&... ms) noexcept : m_ms(&ms...), m_locked(true) {}

    unique_multilock(std::try_to_lock_t, Ms&... ms)
        requires(... && Lockable<Ms>)
        : m_ms(&ms...), m_locked(try_lock() == -1) {}

    template<class Rep, class Period>
        requires(... && Lockable<Ms>) // TimedLockable?
    unique_multilock(const std::chrono::duration<Rep, Period>& dur, Ms&... ms) : m_ms(&ms...), m_locked(try_lock_for(dur) == -1) {}

    template<class Clock, class Duration>
        requires(... && Lockable<Ms>) // TimedLockable?
    unique_multilock(const std::chrono::time_point<Clock, Duration>& tp, Ms&... ms) :
        m_ms(&ms...), m_locked(try_lock_until(tp) == -1) {}

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

    void lock() {
        if constexpr(sizeof...(Ms) == 1) {
            std::get<0>(m_ms)->lock();
        } else if constexpr(sizeof...(Ms) > 1) {
            std::apply([](auto... ms) { std::lock(*ms...); }, m_ms);
        }
        m_locked = true;
    }
    void unlock() noexcept {
        std::apply([](auto... ms) { (..., ms->unlock()); }, m_ms);
        m_locked = false;
    }
    int try_lock() // note: returns -1 for success like std::try_lock
        requires(... && Lockable<Ms>)
    {
        int res = -1;
        if constexpr(sizeof...(Ms) == 1) {
            res = std::get<0>(m_ms)->try_lock() ? -1 : 0;
        } else if constexpr(sizeof...(Ms) > 1) {
            res = std::apply([](auto... ms) { return std::try_lock(*ms...); }, m_ms);
        }
        if(res == -1) m_locked = true;
        return res;
    }
    template<class Clock, class Duration>
        requires(... && Lockable<Ms>) // TimedLockable?
    int try_lock_until(const std::chrono::time_point<Clock, Duration>& tp) {
        int res;
        while((res = try_lock()) != -1 && tp < Clock::now()) {
        }
        return res;
    }
    template<class Rep, class Period>
        requires(... && Lockable<Ms>) // TimedLockable?
    int try_lock_for(const std::chrono::duration<Rep, Period>& dur) {
        return try_lock_until(std::chrono::steady_clock::now() + dur);
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
