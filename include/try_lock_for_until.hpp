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
    template<class...>
    struct type_pack {}; // seems to compile slightly faster than using a tuple
    namespace rot_detail {
        template<class...>
        struct rot_help;

        template<>
        struct rot_help<std::index_sequence<>> {
            using type = type_pack<>;
        };

        template<size_t I0, size_t... Is, class... Seqs> // term
        struct rot_help<std::index_sequence<I0, Is...>, std::index_sequence<I0, Is...>, Seqs...> {
            using type = type_pack<Seqs...>;
        };

        template<std::size_t I0> // extra terminator for MSVC
        struct rot_help<std::index_sequence<I0>> {
            using type = type_pack<std::index_sequence<I0>>;
        };

        template<size_t I0, size_t... Is>
        struct rot_help<std::index_sequence<I0, Is...>> :
            rot_help<std::index_sequence<Is..., I0>, std::index_sequence<I0, Is...>, std::index_sequence<I0, Is...>> {};

        template<size_t I0, size_t... Is, class End, class... Seqs>
        struct rot_help<std::index_sequence<I0, Is...>, End, Seqs...> :
            rot_help<std::index_sequence<Is..., I0>, End, Seqs..., std::index_sequence<I0, Is...>> {};
    } // namespace rot_detail

    template<size_t N>
    using make_pack_of_rotating_index_sequences = rot_detail::rot_help<std::make_index_sequence<N>>::type;
    //-------------------------------------------------------------------------
    namespace result {
        inline constexpr auto timeout = static_cast<std::size_t>(-2);
        inline constexpr auto success = static_cast<std::size_t>(-1);
        inline bool is_index(size_t x) {
            return x < timeout;
        }
    } // namespace result
    //-------------------------------------------------------------------------
    int friendly_try_lock(auto& l1) {
        return -static_cast<int>(l1.try_lock());
    }
    int friendly_try_lock(auto&... ls) {
        return std::try_lock(ls...);
    }
    //-------------------------------------------------------------------------
    template<class Clock, class Duration, class Locks, std::size_t I0, size_t... Is>
    std::size_t try_sequence(const std::chrono::time_point<Clock, Duration>& tp, Locks& locks, std::index_sequence<I0, Is...>) {
        do {
            if constexpr(sizeof...(Is) == 0) {
                if(std::get<I0>(locks).try_lock_until(tp)) {
                    return result::success;
                }
            } else if(std::unique_lock first{std::get<I0>(locks), tp}) {
                int res = friendly_try_lock(std::get<Is>(locks)...);
                if(res == -1) {
                    first.release();
                    return result::success;
                }
                // return index to try_lock_until next round
                return std::array{Is...}[static_cast<std::size_t>(res)];
            }
        } while(Clock::now() < tp); // retry if spurious return
        return result::timeout;
    }
    //-------------------------------------------------------------------------
    template<class Timepoint, class Locks, class... Seqs>
    bool try_lock_until_impl(const Timepoint& end_time, Locks locks, type_pack<Seqs...>) {
        using func_sig = std::size_t (*)(const Timepoint&, Locks&);

        std::array<func_sig, sizeof...(Seqs)> seqs{
            {{+[](const Timepoint& tp, Locks& lks) { return try_sequence(tp, lks, Seqs{}); }}...}};

        // try rotations in seqs while they return an index sequence to try next
        size_t ret = 0;
        while(result::is_index(ret = seqs[ret](end_time, locks)));
        return ret == result::success;
    }
} // namespace detail

template<class Clock, class Duration, detail::TimedLockable... Ls>
[[nodiscard]] bool try_lock_until(const std::chrono::time_point<Clock, Duration>& tp, Ls&... ls) {
    if constexpr(sizeof...(Ls) == 0) {
        return detail::result::success;
    } else {
        return detail::try_lock_until_impl(tp, std::tie(ls...), detail::make_pack_of_rotating_index_sequences<sizeof...(Ls)>{});
    }
}

template<class Rep, class Period, detail::TimedLockable... Ls>
[[nodiscard]] bool try_lock_for(const std::chrono::duration<Rep, Period>& dur, Ls&... ls) {
    return try_lock_until(std::chrono::steady_clock::now() + dur, ls...);
}
} // namespace lyn
#endif
