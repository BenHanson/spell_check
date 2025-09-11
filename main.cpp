#include "filter.hpp"
#include "spellc_error.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <lexertl/generator.hpp>
#include <lexertl/iterator.hpp>
#include <lexertl/memory_file.hpp>
#include <lexertl/rules.hpp>
#include <lexertl/state_machine.hpp>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using mf_vector = std::vector<lexertl::memory_file>;
using str_vector = std::vector<std::string>;
using sv_vector = std::vector<std::string_view>;

static void build_word_lexer(const char* word_rx, lexertl::state_machine& sm)
{
	lexertl::rules rules;

	rules.push(word_rx, 1);
	rules.push("(?s:.)", lexertl::rules::skip());
	lexertl::generator::build(rules, sm);
}

static bool build_indexes(const mf_vector& dictionaries, sv_vector& indexes)
{
	using namespace lexertl;
	bool icase = true;
	std::size_t count = 0;
	rules index_rules;
	state_machine sm;
	enum class token { lower = 1, upper };

	index_rules.push("[a-z]([-']?[a-z])*", *token::lower);
	index_rules.push("[A-Z]([-']?[A-Za-z])*", *token::upper);
	index_rules.push(R"(\s+)", lexertl::rules::skip());
	generator::build(index_rules, sm);

	for (const auto& dict : dictionaries)
	{
		count += std::count(dict.data(), dict.data() + dict.size(), '\n');
	}

	indexes.reserve(count);

	for (const auto& dict : dictionaries)
	{
		lexertl::citerator iter(dict.data(), dict.data() + dict.size(), sm);

		for (; iter->id; ++iter)
		{
			using namespace lexertl;

			switch (iter->id)
			{
			case +token::lower:
				// As expected
				break;
			case +token::upper:
				// Seen capital letters, so make case sensitive
				icase = false;
				break;
			default:
				throw spellc_error(std::format("Unexpected char '{}' "
					"in dictionaries", *iter->first));
				break;
			}

			indexes.push_back(iter->view());
		}
	}

	std::ranges::sort(indexes);
	return icase;
}

static void read_args(const std::span<const char*>& params,
	str_vector& input_pathnames, mf_vector& inputs,
	mf_vector& dictionaries, std::string& lexer_pathname,
	const char*& word_rx)
{
	str_vector dictionary_pathnames;

	for (std::size_t i = 1, size = params.size(); i < size; ++i)
	{
		const std::string_view param(params[i]);

		if (param[0] == '-')
		{
			if (param == "-d" || param == "--dictionary")
			{
				++i;

				if (i == size)
					throw spellc_error("--dictionary is not "
						"followed by pathname");

				// Dictionary to load
				dictionary_pathnames.emplace_back(params[i]);
			}
			else if (param == "-f" || param == "--filter")
			{
				++i;

				if (i == size)
					throw spellc_error("--filter is not "
						"followed by pathname");

				if (!lexer_pathname.empty())
					throw spellc_error("Cannot set "
						"--filter more than once");

				// Lexer to load
				lexer_pathname = params[i];
			}
			else if (param == "-w" || param == "--word-regex")
			{
				++i;

				if (i == size)
					throw spellc_error("--word-regex is not "
						"followed by pathname");

				if (word_rx != nullptr)
					throw spellc_error("Cannot set "
						"--word_regex more than once");

				word_rx = params[i];
			}
			else
				throw spellc_error(std::format("Unknown switch {}", param));
		}
		else
			// Input file to load
			input_pathnames.emplace_back(params[i]);
	}

	namespace fs = std::filesystem;

	if (dictionary_pathnames.empty())
		throw spellc_error("No dictionaries specified!");

	// Move construct a number of lexertl::memory file objects
	inputs = mf_vector(input_pathnames.size());
	dictionaries = mf_vector(dictionary_pathnames.size());

	for (std::size_t idx = 0, size = inputs.size(); idx < size; ++idx)
	{
		// Load an input file
		inputs[idx].open(input_pathnames[idx].c_str());

		if (inputs[idx].data() == nullptr)
			std::cerr << "Failed to open " << input_pathnames[idx] << '\n';
	}

	for (std::size_t idx = 0, size = dictionaries.size(); idx < size; ++idx)
	{
		// Load a dictionary
		dictionaries[idx].open(dictionary_pathnames[idx].c_str());

		if (dictionaries[idx].data() == nullptr)
			throw spellc_error(std::format("Failed to open {}",
				dictionary_pathnames[idx]));
	}
}

static void check_range(const char* first, const char* second,
	const sv_vector& indexes, const lexertl::state_machine& word_sm,
	const std::size_t input_idx, const str_vector& input_pathnames,
	const bool icase)
{
	// Lex a range
	lexertl::citerator iter(first, second, word_sm);
	// Re-use memory of temporary string
	std::string lhs;

	for (; iter->id != 0; ++iter)
	{
		lhs = icase ?
			boost::to_lower_copy(iter->str()) :
			iter->str();

		if (const auto hit_iter =
			std::ranges::lower_bound(indexes, lhs);
			hit_iter == indexes.end() || *hit_iter != lhs)
		{
			// Word not found in dictionaries
			if (!input_pathnames.empty())
				std::cout << input_pathnames[input_idx] << '(' <<
				1 + std::count(first, iter->first, '\n') << "): ";

			std::cout << iter->view() << '\n';
		}
	}
}

static void check_spell(const char* first, const char* second,
	const sv_vector &indexes, const lexertl::state_machine& filter_sm,
	const lexertl::state_machine& word_sm, const std::size_t input_idx,
	const str_vector& input_pathnames, const bool icase)
{
	if (filter_sm.empty())
	{
		check_range(first, second, indexes, word_sm, input_idx,
			input_pathnames, icase);
	}
	else
	{
		// Lex the file
		lexertl::citerator iter(first, second, filter_sm);

		for (; iter->id; ++iter)
		{
			check_range(iter->first, iter->second, indexes, word_sm, input_idx,
				input_pathnames, icase);
		}
	}
}

int main(int argc, const char* argv[])
{
	if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help"))
	{
		std::cout << "Usage: spell_check [pathname...]\n"
			"[(--word-regex|-w) <regex>]\n"
			"[(--filter|-f) <pathname to flex style lexer spec>]\n"
			"((--dictionary|-d) <pathname to whitespace separated word list>)+\n";
		return argc == 1;
	}

	try
	{
		// Word can be capitalised, all lower case or all upper case
		const char* word_rx = nullptr;
		str_vector input_pathnames;
		mf_vector inputs;
		mf_vector dictionaries;
		std::string filter_pathname;
		sv_vector indexes;
		lexertl::state_machine word_sm;
		lexertl::state_machine filter_sm;
		std::size_t input_idx = 0;

		read_args(std::span<const char*>(argv, argc), input_pathnames,
			inputs, dictionaries, filter_pathname, word_rx);

		if (word_rx == nullptr)
			word_rx = "[A-Za-z]([-']?[a-z])*|[A-Z]([-']?[A-Z])*";

		const bool icase = build_indexes(dictionaries, indexes);

		build_word_lexer(word_rx, word_sm);

		if (!filter_pathname.empty())
			filter_sm = build_filter_lexer(filter_pathname);

		for (const auto& in : inputs)
		{
			check_spell(in.data(), in.data() + in.size(), indexes,
				filter_sm, word_sm, input_idx, input_pathnames, icase);
			++input_idx;
		}

		if (inputs.empty())
		{
			// Read from cin
			std::ostringstream ss;
			std::string cin;

			ss << std::cin.rdbuf();
			cin = ss.str();
			check_spell(cin.c_str(), cin.c_str() + cin.size(), indexes,
				filter_sm, word_sm, input_idx, input_pathnames, icase);
		}

		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << '\n';
		return 1;
	}
}
