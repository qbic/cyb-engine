/************************************************************************************
 Derived from:
 https://www.justsoftwaresolutions.co.uk/cplusplus/using-enum-classes-as-bitfields.html
************************************************************************************/
#pragma once
#include <type_traits>

#define CYB_ENABLE_BITMASK_OPERATORS(E) constexpr bool EnableBitmaskOperators(E) noexcept { return true; }

template<typename E>
constexpr bool EnableBitmaskOperators(E) noexcept
{
    return false;
}

template<typename E>
[[nodiscard]] constexpr typename std::enable_if<EnableBitmaskOperators(E()), E>::type operator|(E lhs, E rhs) noexcept
{
    typedef typename std::underlying_type<E>::type underlying;
    return static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template<typename E>
constexpr typename std::enable_if<EnableBitmaskOperators(E()), E&>::type operator|=(E& lhs, E rhs) noexcept
{
    typedef typename std::underlying_type<E>::type underlying;
    lhs = static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
    return lhs;
}

template<typename E>
[[nodiscard]] constexpr typename std::enable_if<EnableBitmaskOperators(E()), E>::type operator&(E lhs, E rhs) noexcept
{
    typedef typename std::underlying_type<E>::type underlying;
    return static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template<typename E>
constexpr typename std::enable_if<EnableBitmaskOperators(E()), E&>::type operator&=(E& lhs, E rhs) noexcept
{
    typedef typename std::underlying_type<E>::type underlying;
    lhs = static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
    return lhs;
}

template<typename E>
[[nodiscard]] constexpr typename std::enable_if<EnableBitmaskOperators(E()), E>::type operator~(E rhs) noexcept
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
