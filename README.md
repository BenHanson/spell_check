# spell_check
Simple spell checker

## Building
- There is a `Makefile` for Linux
- There is a VS `.sln` for Windows

## Usage
```
spell_check [pathname...]
  -r, --recurse
      --include <wildcard>{;<wildcard>}
      --exclude <wildcard>{;<wildcard>}
      --exclude-dir <wildcard>{;<wildcard>}
  -w, --word-regex <regex>
  -f, --filter <pathname to flex style lexer spec>
  -d, --dictionary <pathname to whitespace separated word list>
```

* `--dictionary` can be specified multiple times
* `--recurse` means you specify paths to search rather than pathnames
* If no pathnames are supplied, input is taken from stdin.

## Dictionaries
Google: `dictionary word list text file` for sample dictionaries.
If you want case insensitive matching make all words lower case.
