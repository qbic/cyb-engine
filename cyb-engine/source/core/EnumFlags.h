/************************************************************************************
 Derived from:
 https://www.justsoftwaresolutions.co.uk/cplusplus/using-enum-classes-as-bitfields.html
************************************************************************************/
#pragma once
#include <type_traits>

#define CYB_ENABLE_BITMASK_OPERATORS(type)                  \
    template<>                                              \
    struct EnableBitmaskOperators<type> {                   \
        static const bool enable = true;                    \
    }

template<typename E>
struct EnableBitmaskOperators {
    static constexpr bool enable = false;
};
template<typename E>
constexpr typename std::enable_if<EnableBitmaskOperators<E>::enable, E>::type operator|(E lhs, E rhs)
{
    typedef typename std::underlying_type<E>::type underlying;
    return static_cast<E>(
        static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}
template<typename E>
constexpr typename std::enable_if<EnableBitmaskOperators<E>::enable, E&>::type operator|=(E& lhs, E rhs)
{
    typedef typename std::underlying_type<E>::type underlying;
    lhs = static_cast<E>(
        static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
    return lhs;
}
template<typename E>
constexpr typename std::enable_if<EnableBitmaskOperators<E>::enable, E>::type operator&(E lhs, E rhs)
{
    typedef typename std::underlying_type<E>::type underlying;
    return static_cast<E>(
        static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}
template<typename E>
constexpr typename std::enable_if<EnableBitmaskOperators<E>::enable, E&>::type operator&=(E& lhs, E rhs)
{
    typedef typename std::underlying_type<E>::type underlying;
    lhs = static_cast<E>(
        static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
    return lhs;
}
template<typename E>
constexpr typename std::enable_if<EnableBitmaskOperators<E>::enable, E>::type operator~(E rhs)
{
    typedef typename std::underlying_type<E>::type underlying;
    rhs = static_cast<E>(
        ~static_cast<underlying>(rhs));
    return rhs;
}
template<typename E>
constexpr bool HasFlag(E lhs, E rhs)
{
    return (lhs & rhs) == rhs;
}
