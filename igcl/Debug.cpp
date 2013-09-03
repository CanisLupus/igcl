#include "Debug.hpp"

#include <iostream>

#ifdef DEBUG_MODE

void dbg_() {
	std::cout << ']' << std::endl;
}

void dbg_nnl_() {
	std::cout << ']';
}

#endif
