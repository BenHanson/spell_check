# spell_check
Simple spell checker

## Building
`g++ -o spell_check main.cpp -std=c++20 -I ../lexertl17/include -I ../../boost_1_77_0`

## Usage
`spell_check [pathname...] ((--dictionary|-d) <pathname to whitespace separated word list>)+`

Note that if no pathnames are supplied, input is taken from stdin.

## Dictionaries
Google: `dictionary word list text file` for sample dictionaries. Be aware that spell_check expects all words to be lower case.
