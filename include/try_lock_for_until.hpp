#ifndef TRY_LOCK_FOR_UNTIL_HPP_6FE470F4_744E_11F0_BE3A_90B11C0C0FF8
#define TRY_LOCK_FOR_UNTIL_HPP_6FE470F4_744E_11F0_BE3A_90B11C0C0FF8
#include "detail/concepts.hpp"

#include <chrono>
#include <concepts>
#include <cstdint>
#include <tuple>
#include <utility>
namespace lyn {
namespace detail {

    enum class rec_res { no, yes, timeout };

    template<class Tup, std::size_t... Is>
    void unlock(Tup& locks, std::index_sequence<Is...>, std::size_t lock_co) {
        // lock_co must be at least 1
        (... && ((std::get<Is>(locks).unlock(), true) && --lock_co));
    }

    template<class Clock, class Dur, class Tup, std::size_t I0, std::size_t... Is, class End>
    rec_res try_lock_until_rec_rot(const std::chrono::time_point<Clock, Dur>& tp, Tup& locks, std::index_sequence<I0, Is...>,
                                   const End end) {
        // try locking the first until tp
        if(not std::get<I0>(locks).try_lock_until(tp)) return rec_res::timeout;

        // try locking the rest using a plain try_lock:
        try {
            if(std::try_lock(std::get<Is>(locks)...)) return rec_res::yes;
        } catch(...) { // locking threw, unlock and rethrow
            std::get<I0>(locks).unlock();
            throw;
        }
        // locking failed gracefully
        std::get<I0>(locks).unlock();

        // no lock held, do we have a timeout?
        if(Clock::now() >= tp) return rec_res::timeout;

        // rotate and recurse or have we already tried all rotations?
        constexpr std::index_sequence<Is..., I0> next;
        if constexpr(std::same_as<decltype(next), decltype(end)>) {
            // all rotations exhausted, back out to try again
            return rec_res::no;
        } else {
            return try_lock_until_rec_rot(tp, locks, next, end);
        }
    }

    template<class Clock, class Dur, std::size_t... Is, class... Ls>
    bool try_lock_until_impl(const std::chrono::time_point<Clock, Dur>& tp, std::index_sequence<Is...> is, Ls&... ls) {
        std::tuple<Ls&...> locks{ls...};
        while(true) {
            const auto res = try_lock_until_rec_rot(tp, locks, is, is);
            if(res == rec_res::yes) return true;
            if(res == rec_res::timeout) return false;
        }
    }
} // namespace detail

template<class Clock, class Duration, class L1, class L2, class... Ls>
    requires(detail::TimedLockable<L1> && detail::TimedLockable<L2> && (... && detail::TimedLockable<Ls>))
[[nodiscard]] bool try_lock_until(const std::chrono::time_point<Clock, Duration>& tp, L1& l1, L2& l2, Ls&... ls) {
    return detail::try_lock_until_impl(tp, std::make_index_sequence<sizeof...(Ls) + 2>{}, l1, l2, ls...);
}

template<class Rep, class Period, class L1, class L2, class... Ls>
    requires(detail::TimedLockable<L1> && detail::TimedLockable<L2> && (... && detail::TimedLockable<Ls>))
[[nodiscard]] bool try_lock_for(const std::chrono::duration<Rep, Period>& dur, L1& l1, L2& l2, Ls&... ls) {
    return try_lock_until(std::chrono::steady_clock::now() + dur, l1, l2, ls...);
}
} // namespace lyn
#endif
