#ifndef TRY_LOCK_FOR_UNTIL_HPP_6FE470F4_744E_11F0_BE3A_90B11C0C0FF8
#define TRY_LOCK_FOR_UNTIL_HPP_6FE470F4_744E_11F0_BE3A_90B11C0C0FF8
#include "detail/concepts.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <tuple>
#include <utility>

namespace lyn {
namespace detail {
    namespace rot_detail {
        template<class...>
        struct rot_help;

        template<>
        struct rot_help<std::index_sequence<>> {
            using type = std::tuple<>;
        };

        template<size_t I0, size_t... Is>
        struct rot_help<std::index_sequence<I0, Is...>> :
            rot_help<std::index_sequence<Is..., I0>, std::index_sequence<I0, Is...>, std::index_sequence<I0, Is...>> {};

        template<size_t I0, size_t... Is, class End, class... Seqs>
        struct rot_help<std::index_sequence<I0, Is...>, End, Seqs...> :
            rot_help<std::index_sequence<Is..., I0>, End, Seqs..., std::index_sequence<I0, Is...>> {};

        template<size_t I0, size_t... Is, class... Seqs>
        struct rot_help<std::index_sequence<I0, Is...>, std::index_sequence<I0, Is...>, Seqs...> {
            using type = std::tuple<Seqs...>;
        };
    } // namespace rot_detail

    template<size_t N>
    using make_tuple_of_rotating_index_sequences = rot_detail::rot_help<std::make_index_sequence<N>>::type;
    //-------------------------------------------------------------------------
    namespace rev_detail {
        template<template<class A, A...> class C, class T, T... I, std::size_t... J>
        constexpr auto rev_help(C<T, I...> const&, std::index_sequence<J...>)
        // -> C<T, std::get<sizeof...(J) - J - 1>(std::tuple{I...})...>;
        { // workaround for MSVC:
            constexpr std::array<T, sizeof...(I)> arr{I...};
            return C<T, arr[sizeof...(J) - J - 1]...>{};
        }
    } // namespace rev_detail
    template<class T>
    using reverse_sequence = decltype(rev_detail::rev_help(std::declval<T>(), std::make_index_sequence<T::size()>{}));
    //-------------------------------------------------------------------------
    enum class rec_res { fail = -2, success = -1 };
    template<std::size_t I>
    constexpr rec_res timeout = static_cast<rec_res>(static_cast<int>(I));
    //-------------------------------------------------------------------------
    template<class Timepoint, class Locks, class... Seqs>
    int try_lock_until_impl(const Timepoint& tp, Locks locks, std::tuple<Seqs...>) {
        auto try_once = []<class TUL, size_t... RIs>(TUL ul, std::index_sequence<RIs...>) {
            // lock from last to first in the tuple of unique_locks
            if((... && std::get<RIs>(ul).try_lock())) {
                (..., std::get<RIs>(ul).release());
                return true;
            }
            return false; // all unlocked in the reverse locking order
        };
        auto try_until = [&]<std::size_t I0, size_t... Is>(std::index_sequence<I0, Is...>) {
            if(std::unique_lock first{std::get<I0>(locks), tp}) {
                return [&]<std::size_t... RIs>(std::index_sequence<RIs...>) {
                    // create a tuple of unique_locks in reverse order
                    if(try_once(std::tuple{std::unique_lock{std::get<RIs>(locks), std::defer_lock}...},
                                reverse_sequence<std::make_index_sequence<sizeof...(RIs)>>{})) {
                        first.release();
                        return rec_res::success;
                    }
                    return rec_res::fail;
                }(reverse_sequence<std::index_sequence<Is...>>{});
            }
            return timeout<I0>;
        };

        // try all the rotations in Seqs until one doesn't return rec_res::fail
        rec_res res;
        while(not(... || ((res = try_until(Seqs{})) != rec_res::fail)));
        return static_cast<int>(res);
    }
} // namespace detail

template<class Clock, class Duration, class L1, class L2, class... Ls>
    requires(detail::TimedLockable<L1> && detail::TimedLockable<L2> && (... && detail::TimedLockable<Ls>))
[[nodiscard]] int try_lock_until(const std::chrono::time_point<Clock, Duration>& tp, L1& l1, L2& l2, Ls&... ls) {
    return detail::try_lock_until_impl(tp, std::tie(l1, l2, ls...),
                                       detail::make_tuple_of_rotating_index_sequences<sizeof...(Ls) + 2>{});
}

template<class Rep, class Period, class L1, class L2, class... Ls>
    requires(detail::TimedLockable<L1> && detail::TimedLockable<L2> && (... && detail::TimedLockable<Ls>))
[[nodiscard]] int try_lock_for(const std::chrono::duration<Rep, Period>& dur, L1& l1, L2& l2, Ls&... ls) {
    return try_lock_until(std::chrono::steady_clock::now() + dur, l1, l2, ls...);
}
} // namespace lyn
#endif
