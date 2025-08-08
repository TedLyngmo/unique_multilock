#ifndef CONCEPTS_HPP_EC431704_744E_11F0_9190_90B11C0C0FF8
#define CONCEPTS_HPP_EC431704_744E_11F0_9190_90B11C0C0FF8
#include <chrono>
#include <concepts>

namespace lyn {
namespace detail {
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
        { t.try_lock_for(std::chrono::nanoseconds{}) } -> std::same_as<bool>;
        { t.try_lock_for(std::chrono::microseconds{}) } -> std::same_as<bool>;
        { t.try_lock_for(std::chrono::milliseconds{}) } -> std::same_as<bool>;
        { t.try_lock_until(std::chrono::time_point<std::chrono::steady_clock>{}) } -> std::same_as<bool>;
        { t.try_lock_until(std::chrono::time_point<std::chrono::system_clock>{}) } -> std::same_as<bool>;
    };
} // namespace detail
} // namespace lyn
#endif
