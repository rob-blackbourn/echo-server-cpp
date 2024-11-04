#ifndef SQUAWKBUS_IO_MATCH_HPP
#define SQUAWKBUS_IO_MATCH_HPP

// For rust style matching with variants.

namespace jetblack::utils
{
  
  template<class... Ts> struct match : Ts... { using Ts::operator()...; };
  template<class... Ts> match(Ts...) -> match<Ts...>;

}

#endif // SQUAWKBUS_IO_MATCH_HPP
