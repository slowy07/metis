# sniffercomit
make sure works very well before push it with C++20 standard.

## Depedency

- [`tomlplusplus`](https://github.com/marzer/tomlplusplus.git)

    Header only TOML config file parser and serializer

- [`fmt`](https://github.com/fmtlib/fmt.git)

    Modern formatting library

## Build and Test it

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel

./sniffercommit init

# install into pre-commit hooks and github actions as CI
./sniffercommit install -c .sniffercommit.toml
```
