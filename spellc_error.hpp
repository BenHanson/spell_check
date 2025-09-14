#pragma once

#include <stdexcept>

class spellc_error : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};
