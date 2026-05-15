# sniffercomit
make sure works very well before push it with C++20 standard.

## Depedency

- [`tomlplusplus`](https://github.com/marzer/tomlplusplus.git)

    Header only TOML config file parser and serializer

- [`fmt`](https://github.com/fmtlib/fmt.git)

    Modern formatting library

## Feature

- parallel check execution
    run linter concurrencyvia bash background jobs
- pattern aware filtering
    apply checks only matching file extensions (`.cpp`)
- zero runtime Depedency
    single static binary; no python / Node require
- auto generate CI workflow mirroring local hooks

## Build and Test it

```bash
mkdir build && cd build
# release type
cmake .. -DCMAKE_BUILD_TYPE=Release
# active build with parallel options
cmake --build . --parallel

# verify
./sniffercommit --version

./sniffercommit init

# install into pre-commit hooks and github actions as CI
./sniffercommit install -c .sniffercommit.toml
```

```bash
# initialize project into project

cd /path/project
sniffercommit init

# optional formatter style (.clang-formats)
# avaiable: llvm, google, microsoft, gnu, mozilla, webkit, default are google
sniffercomit init --style llvm
```

this will be create `.sniffercommit.toml` with sensible defaults

```bash
# install local pre-commit hooks
sniffercommit install

# (optional) generate github action workflows
sniffercommit generate-gha
```

```bash
# commit and test verify
echo "int main(){return 0;}" > test.cpp
git add test.cpp
git commit -m "chore: test sniffercommit"
```

## Debugging tips

```
# enable verbose config output
cmake .. -DSNIFFERCOMMIT_VERBOSE_CONFIG=ON

# build with sanitizers (catch memory bugs)
cmake .. -DCMAKE_BUILD_TYPE=Debug -DSNIFFERCOMMIT_ENABLE_SANITIZERS=ON

# inspect generated hook
cat .git/hooks/pre-commit | less

# test hook manually (without git)
bash .git/hooks/pre-commit 
```

## Acknowledge

- [precommit](https://pre-commit.com/) - inspiration for hook management workflow

This project was inspired by an internal tool developed at a previous company for enforcing C/C++ naming conventions. Due to licensing constraints, sniffercommit was built from scratch with the same functionality but a distinct architecture and open-source ethos.
