# `lyn::unique_multilock`
A class which is a combination of `std::unique_lock` and `std::scoped_lock` for C++20 and above.

## `lyn::try_lock_for` and `lyn::try_lock_until`

This repo also contains a header, `try_lock_for_until.hpp`, with two freestanding functions where two or more timed mutexes can be locked using an algorithm similar to that which is used in `std::lock`:
```
template<class Clock, class Duration, class L1, class L2, class... Ls>
[[nodiscard]] bool try_lock_until(const std::chrono::time_point<Clock, Duration>& tp,
                                  L1& l1, L2& l2, Ls&... ls);
```

```
template<class Rep, class Period, class L1, class L2, class... Ls>
[[nodiscard]] bool try_lock_for(const std::chrono::duration<Rep, Period>& dur,
                                L1& l1, L2& l2, Ls&... ls);
```
