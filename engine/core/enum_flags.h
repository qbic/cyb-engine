#pragma once
#include <type_traits>

// Derived from Anthony Williams "Using Enum Classes as Bitfields"
// https://www.justsoftwaresolutions.co.uk/cplusplus/using-enum-classes-as-bitfields.html

#define CYB_ENABLE_BITMASK_OPERATORS(E) template <> struct EnableBitmaskOperators<E> : std::true_type {};

template<typename E>
struct EnableBitmaskOperators : std::false_type {};

template<typename E>
constexpr bool EnableBitmaskOperators_v = EnableBitmaskOperators<E>::value;

template<typename E>
[[nodiscard]] constexpr typename std::enable_if_t<EnableBitmaskOperators_v<E>, E> operator|(E lhs, E rhs) noexcept
{
    typedef typename std::underlying_type<E>::type underlying;
    return static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template<typename E>
constexpr typename std::enable_if_t<EnableBitmaskOperators_v<E>, E&> operator|=(E& lhs, E rhs) noexcept
{
    typedef typename std::underlying_type<E>::type underlying;
    lhs = static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
    return lhs;
}

template<typename E>
[[nodiscard]] constexpr typename std::enable_if_t<EnableBitmaskOperators_v<E>, E> operator&(E lhs, E rhs) noexcept
{
    typedef typename std::underlying_type<E>::type underlying;
    return static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template<typename E>
constexpr typename std::enable_if_t<EnableBitmaskOperators_v<E>, E&> operator&=(E& lhs, E rhs) noexcept
{
    typedef typename std::underlying_type<E>::type underlying;
    lhs = static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
    return lhs;
}

template<typename E>
[[nodiscard]] constexpr typename std::enable_if_t<EnableBitmaskOperators_v<E>, E> operator~(E rhs) noexcept
{
    typedef typename std::underlying_type<E>::type underlying;
    rhs = static_cast<E>(~static_cast<underlying>(rhs));
    return rhs;
}

template<typename E>
[[nodiscard]] constexpr bool HasFlag(E lhs, E rhs) noexcept
{
    return (lhs & rhs) == rhs;
}

template<typename E>
constexpr void SetFlag(E& lhs, E rhs, bool value) noexcept
{
    typedef typename std::underlying_type<E>::type underlying;
    lhs &= ~rhs;
    lhs |= (E)(static_cast<underlying>(value) * static_cast<underlying>(rhs));
}

template<typename E>
constexpr std::underlying_type<E>::type Numerical(E e) noexcept
{
    return (static_cast<std::underlying_type<E>::type>(e));
}
