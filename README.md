# spell_check
Simple spell checker

## Building
- There is a Makefile for Linux
- There is a VS .sln for Windows

## Usage
```
spell_check [pathname...]
[(--word-regex|-w) <regex>]
[(--filter|-f) <pathname to flex style lexer spec>]
((--dictionary|-d) <pathname to whitespace separated word list>)+

```

Note that if no pathnames are supplied, input is taken from stdin.

## Dictionaries
Google: `dictionary word list text file` for sample dictionaries. Be aware that spell_check expects all words to be lower case.
