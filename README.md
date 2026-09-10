# spell_check
Simple spell checker

## Building

A C++20 compatible compiler is required.

```shell
git clone https://github.com/BenHanson/lexertl17
git clone https://github.com/BenHanson/parsertl17
git clone https://github.com/BenHanson/wildcardtl
git clone https://github.com/BenHanson/spell_check
```

* Use the `Makefile` when building on Linux
* Use the `.sln` file when building with Visual Studio

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
