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
        template<std::size_t I, class IndexSeq>
        struct rotate;

        template<std::size_t I, std::size_t... J>
        struct rotate<I, std::index_sequence<J...>> {
            using type = std::index_sequence<((J + I) % sizeof...(J))...>;
        };

        template<class IndexSeq>
        struct make_pack_impl;

        template<std::size_t... I>
        struct make_pack_impl<std::index_sequence<I...>> {
            using type = type_pack<typename rotate<I, std::index_sequence<I...>>::type...>;
        };
    } // namespace rot_detail

    template<std::size_t N>
    using make_pack_of_rotating_index_sequences = typename rot_detail::make_pack_impl<std::make_index_sequence<N>>::type;
    //-------------------------------------------------------------------------
    namespace result {
        inline constexpr auto timeout = static_cast<std::size_t>(-2);
        inline constexpr auto success = static_cast<std::size_t>(-1);
        inline bool is_index(std::size_t x) {
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
    template<class Timepoint, class Locks, class... Seqs>
    bool try_lock_until_impl(const Timepoint& end_time, Locks locks, type_pack<Seqs...>) {
        // an array with one function per lockable/sequence to try:
        std::array<std::size_t (*)(const Timepoint&, Locks&), sizeof...(Seqs)> seqs{{+[](const Timepoint& tp, Locks& lks) {
            return []<class Clock, class Duration, std::size_t I0, std::size_t... Is>(
                       const std::chrono::time_point<Clock, Duration>& itp, Locks& lcks, std::index_sequence<I0, Is...>) {
                do {
                    if constexpr(sizeof...(Is) == 0) {
                        if(std::get<I0>(lcks).try_lock_until(itp)) {
                            return result::success;
                        }
                    } else if(std::unique_lock first{std::get<I0>(lcks), itp}) {
                        int res = friendly_try_lock(std::get<Is>(lcks)...);
                        if(res == -1) {
                            first.release();
                            return result::success;
                        }
                        // return the index to try_lock_until next round
                        return std::array{Is...}[static_cast<std::size_t>(res)];
                    }
                } while(Clock::now() < itp); // retry if spurious return
                return result::timeout;
            }(tp, lks, Seqs{});
        }...}};

        // try rotations in seqs while they return an index sequence to try next
        std::size_t ret = 0;
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
