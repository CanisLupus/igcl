#ifndef DEBUG_HPP_
#define DEBUG_HPP_

#include "CommonDefines.hpp"

#include <iostream>

#ifndef DEBUG_MODE

	#define dbg(...)
	#define dbg_f(...)

#else

	// Must be macro (instead of function) to print the correct call place.
	#define dbg_f() dbg(__func__, "(", __FILE__, __LINE__, ")")


	// Since variadic templates are recursive, must have a base case
	void dbg_();
	void dbg_nnl_();

	template <typename T, typename ...P>
	void dbg_(T t, P ...p)
	{
		std::cout << ' ' << t;
		dbg_(p...);
	}

	template <typename T, typename ...P>
	void dbg_nnl_(T t, P ...p)
	{
		std::cout << ' ' << t;
		dbg_nnl_(p...);
	}

	// ------------------- actually called

	template <class P>
	void dbg_a(unsigned char * t, P size)
	{
		std::cout << "[DBG:";
		for (P i=0; i<size; i++) {
			if (t[i] != '\0')
				std::cout << t[i];
		}
		dbg_();
	}

	template <class T, class P>
	void dbg_a(T * t, P size)
	{
		std::cout << "[DBG:";
		for (P i=0; i<size; i++)
			std::cout << ' ' << t[i];
		dbg_();
	}

	template <typename T, typename ...P>
	void dbg(T t, P ...p)
	{
		std::cout << "[DBG: " << t;
		dbg_(p...);
	}

	template <typename T, typename ...P>
	void dbg_nnl(T t, P ...p)
	{
		std::cout << "[DBG: " << t;
		dbg_nnl_(p...);
	}

#endif

#endif /* DEBUG_HPP_ */
