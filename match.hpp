#ifndef __match_hpp
#define __match_hpp

template<class... Ts> struct match : Ts... { using Ts::operator()...; };
template<class... Ts> match(Ts...) -> match<Ts...>;

#endif // __match_hpp
