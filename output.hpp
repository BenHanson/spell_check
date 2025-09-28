#pragma once

#include "colours.hpp"
#include "types.hpp"

#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>

bool is_a_tty(FILE* fd);

const char* sc_text();

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>& output_gg(std::basic_ostream<CharT, Traits>& os)
{
	os << sc_text();
	return os;
}

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>& output_nl(std::basic_ostream<CharT, Traits>& os)
{
	os << '\n';
	return os;
}

template<class CharT, class Traits>
void output_text(std::basic_ostream<CharT, Traits>& os, const bool tty,
	const CharT* szColour, const std::basic_string_view<CharT> text)
{
	if (*szColour && tty)
	{
		os << szColour << szEraseEOL;
	}

	os << text;

	if (*szColour && tty)
	{
		os << szDefaultText << szEraseEOL;
	}
}

template<class CharT, class Traits>
void output_text(std::basic_ostream<CharT, Traits>& os, const bool tty,
	const CharT* szColour, const std::basic_string<CharT>& text)
{
	output_text(os, tty, szColour, std::string_view(text));
}

template<class CharT, class Traits>
void output_text(std::basic_ostream<CharT, Traits>& os, const bool tty,
	const CharT* szColour, const CharT* text)
{
	output_text(os, tty, szColour, std::string_view(text));
}

template<class CharT, class Traits>
void output_text_nl(std::basic_ostream<CharT, Traits>& os, const bool tty,
	const CharT* szColour, const std::basic_string_view<CharT> text)
{
	output_text(os, tty, szColour, text);
	os << output_nl;
}

template<class CharT, class Traits>
void output_text_nl(std::basic_ostream<CharT, Traits>& os, const bool tty,
	const CharT* szColour, const CharT* text)
{
	output_text_nl(os, tty, szColour, std::string_view(text));
}

template<class CharT, class Traits>
void output_text_nl(std::basic_ostream<CharT, Traits>& os, const bool tty,
	const CharT* szColour, const std::basic_string<CharT>& text)
{
	output_text_nl(os, tty, szColour, std::string_view(text));
}
