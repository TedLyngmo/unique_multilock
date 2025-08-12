#ifndef TRY_LOCK_FOR_UNTIL_HPP_6FE470F4_744E_11F0_BE3A_90B11C0C0FF8
#define TRY_LOCK_FOR_UNTIL_HPP_6FE470F4_744E_11F0_BE3A_90B11C0C0FF8
#include "detail/concepts.hpp"

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

        template<std::size_t I0> // extra terminator for MSVC
        struct rot_help<std::index_sequence<I0>> {
            using type = std::tuple<std::index_sequence<I0>>;
        };

        template<size_t I0, size_t... Is, class... Seqs>
        struct rot_help<std::index_sequence<I0, Is...>, std::index_sequence<I0, Is...>, Seqs...> {
            using type = std::tuple<Seqs...>;
        };

        template<size_t I0, size_t... Is>
        struct rot_help<std::index_sequence<I0, Is...>> :
            rot_help<std::index_sequence<Is..., I0>, std::index_sequence<I0, Is...>, std::index_sequence<I0, Is...>> {};

        template<size_t I0, size_t... Is, class End, class... Seqs>
        struct rot_help<std::index_sequence<I0, Is...>, End, Seqs...> :
            rot_help<std::index_sequence<Is..., I0>, End, Seqs..., std::index_sequence<I0, Is...>> {};
    } // namespace rot_detail

    template<size_t N>
    using make_tuple_of_rotating_index_sequences = rot_detail::rot_help<std::make_index_sequence<N>>::type;
    //-------------------------------------------------------------------------
    namespace result {
        inline constexpr int fail = -2;
        inline constexpr int success = -1;
        template<std::size_t I>
        constexpr int timeout = static_cast<int>(I);
    } // namespace result
    //-------------------------------------------------------------------------
    template<class Timepoint, class Locks, class... Seqs>
    int try_lock_until_impl(const Timepoint& tp, Locks locks, std::tuple<Seqs...>) {
        if constexpr(sizeof...(Seqs) == 0) {
            return result::success;
        } else if constexpr(sizeof...(Seqs) == 1) {
            if(std::get<0>(locks).try_lock()) {
                return result::success;
            }
            return result::timeout<0>;
        } else {
            auto try_seq = [&]<std::size_t I0, size_t... Is>(std::index_sequence<I0, Is...>) {
                if(std::unique_lock first{std::get<I0>(locks), tp}) {
                    auto lock_the_rest = [&] {
                        if constexpr(sizeof...(Is) >= 2) {
                            return std::try_lock(std::get<Is>(locks)...) == -1;
                        } else {
                            return std::get<0>(locks).try_lock();
                        }
                    };
                    if(lock_the_rest()) {
                        first.release();
                        return result::success;
                    }
                    return result::fail;
                }
                return result::timeout<I0>;
            };

            // try all the rotations in Seqs until one doesn't return result::fail
            int res;
            while(not(... || ((res = try_seq(Seqs{})) != result::fail)));
            return res;
        }
    }
} // namespace detail

template<class Clock, class Duration, detail::TimedLockable... Ls>
[[nodiscard]] int try_lock_until(const std::chrono::time_point<Clock, Duration>& tp, Ls&... ls) {
    return detail::try_lock_until_impl(tp, std::tie(ls...), detail::make_tuple_of_rotating_index_sequences<sizeof...(Ls)>{});
}

template<class Rep, class Period, detail::TimedLockable... Ls>
[[nodiscard]] int try_lock_for(const std::chrono::duration<Rep, Period>& dur, Ls&... ls) {
    return try_lock_until(std::chrono::steady_clock::now() + dur, ls...);
}
} // namespace lyn
#endif
