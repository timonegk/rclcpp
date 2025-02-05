#pragma once

#include <cstddef>

namespace rclcpp
{

namespace detail
{

struct EnumClassHash
{
    template <typename T>
    std::size_t operator()(T t) const
    {
        return static_cast<std::size_t>(t);
    }
};

}
}
