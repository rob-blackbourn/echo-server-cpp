#ifndef JETBLACK_NET_MATCH_HPP
#define JETBLACK_NET_MATCH_HPP

// For rust style matching with variants.

namespace jetblack::net
{
  
  template<class... Ts> struct match : Ts... { using Ts::operator()...; };
  template<class... Ts> match(Ts...) -> match<Ts...>;

}

#endif // JETBLACK_NET_MATCH_HPP
