#ifndef JETBLACK_NET_MATCH_HPP
#define JETBLACK_NET_MATCH_HPP

namespace jetblack::net
{
  
  template<class... Ts> struct match : Ts... { using Ts::operator()...; };
  template<class... Ts> match(Ts...) -> match<Ts...>;

}

#endif // JETBLACK_NET_MATCH_HPP
