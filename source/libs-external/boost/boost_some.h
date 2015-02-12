#pragma once

/* 
    this file contains some boost useful templates
    this is just copy-paste
*/

namespace boost
{

    template <int n> struct choose_initial_n
    {
        enum
        {
            c = (unsigned int(1) << n << n) != 0,
            value = !c*n + choose_initial_n<2 * c*n>::value
        };
    };
    template <> struct choose_initial_n < 0 > 
    {
        enum
        {
            value = 0
        };
    };


    const int n_zero = 16;
    const int initial_n = choose_initial_n<n_zero>::value;

    template <unsigned int x, int n = initial_n> struct static_log2_impl
    {
        enum 
        {
            c = (x >> n) > 0,
            value = c*n + (static_log2_impl< (x >> c*n), n / 2 >::value)
        };
    };

    template <> struct static_log2_impl < 1, 0 >
    {
        enum
        {
            value = 0
        };
    };

    template <int x> struct static_log2
    {
        enum
        {
            value = static_log2_impl<x>::value
        };
    };


    template <> struct static_log2<0> {};

} // namespace boost