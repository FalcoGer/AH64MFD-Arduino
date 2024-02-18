#pragma once

#include <Arduino.h>

#include "error.h"

template <typename T, size_t n>
class Array
{
  private:
    // NOLINTNEXTLINE(modernize-avoid-c-arrays) // This is the array class to get rid of arrays. No choice.
    T m_data[n];

  public:
    Array() { static_assert(n > 0); }

    template <typename... Args>
    Array(Args... args) : m_data {args...}    // NOLINT(google-explicit-constructor)
    {
        static_assert(sizeof...(args) == n, "Incorrect number of arguments");
        static_assert(n > 0);
    }

    using ValueType     = T;
    using SizeType      = size_t;
    using Iterator      = T*;
    using ConstIterator = const T*;

    [[nodiscard]]
    constexpr auto size() const -> SizeType
    {
        return n;
    }
    [[nodiscard]]
    auto begin() -> Iterator
    {
        return static_cast<Iterator>(m_data);
    }
    [[nodiscard]]
    auto end() -> Iterator
    {
        return static_cast<Iterator>(m_data) + n;
    }
    [[nodiscard]]
    auto begin() const -> ConstIterator
    {
        return static_cast<ConstIterator>(m_data);
    }
    [[nodiscard]]
    auto end() const -> ConstIterator
    {
        return static_cast<ConstIterator>(m_data) + n;
    }

    auto operator[] (const SizeType INDEX) -> T&
    {
        if (INDEX < 0 || INDEX >= n)
        {
            // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay) // Requires the use of c functions like sprintf. Using static_cast everywhere is annoying
            // NOLINTBEGIN(cppcoreguidelines-pro-type-vararg) // No alternatives to c-style function since no standard library

            const uint8_t BUFFER_SIZE = 64;
            // NOLINTNEXTLINE(modernize-avoid-c-arrays) // This is the array class to get rid of arrays. No choice.
            char buffer[BUFFER_SIZE];
            (void)sprintf(buffer, "Index out of range. Valid: 0 .. %d. Actual: %d\n", n - 1, INDEX);
            error(buffer, false);
            // NOLINTEND(cppcoreguidelines-pro-type-vararg)
            // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        }
        // // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index) // No choice
        return m_data[INDEX];
    }
    auto operator[] (const SizeType INDEX) const -> const T&
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast) // Using const cast to avoid code duplication. Non const version doesn't modify.
        return const_cast<Array*>(this)->operator[] (INDEX);
    }
};
