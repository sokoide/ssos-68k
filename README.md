# SSOS for X68000

## Prereq

* Set up and build a cross compile toolset written in <https://github.com/yunkya2/elf2x68k>
* Make the following changes to compile elf2x68k before `make all` on macos 15 Sequoia

```sh
brew install texinfo gmp mpfr libmpc
```

  * modify scripts/binutils.sh

```
 41 ${SRC_DIR}/${BINUTILS_DIR}/configure \
 42     --prefix=${INSTALL_DIR} \
 43     --program-prefix=${PROGRAM_PREFIX} \
 44     --target=${TARGET} \
 45     --enable-lto \
 46     --enable-multilib \
 47 ▸   --with-gmp=/opt/homebrew/Cellar/gmp/6.3.0 \
 48 ▸   --with-mpfr=/opt/homebrew/Cellar/mpfr/4.2.1 \
 49 ▸   --with-mpc=/opt/homebrew/Cellar/libmpc/1.3.1
```

* Please refer to <https://github.com/sokoide/x68k-cross-compile> for more info

## Build

### prereq

* set the following 2 env vars

```bash
export XELF_BASE=/path/to/your/cloned/elf2x68k/m68k-xelf
export PATH=$XELF_BASE/bin:$PATH
```

### ssos

* cd ssos
* make clean # this is necessary after making `local` below
* make
* ~/tmp/ssos.xdf is built
* boot from the xdf

![ssos](./docs/ssos.png)

### local version

* for faster development, there is a local version runs as an .X file on Human68K
* cd ssos
* make clean # this is necessary after making the xfd version above
* make local
* mount and run ~/tmp/local.x

## Unit Testing

SSOS includes a comprehensive unit testing framework for testing core functionality without requiring the X68000 emulator.

### Running Tests

```bash
cd ssos/tests
. ~/.elf2x68k
make test
```

### Test Framework Features

- **Local Mode Testing**: Tests run in LOCAL_MODE as native executables for fast feedback
- **Comprehensive Coverage**: Tests for memory management, task scheduling, layer management, and error handling
- **Regression Detection**: Automated testing to catch bugs early in development
- **CI/CD Ready**: Lightweight framework suitable for continuous integration

### Test Suites

- **Memory Tests** (`test_memory.c`): Memory allocator, 4K alignment, fragmentation handling
- **Scheduler Tests** (`test_scheduler.c`): Task creation, priority scheduling, state transitions
- **Layer Tests** (`test_layers.c`): Layer allocation, z-order, dirty rectangle tracking
- **Error Tests** (`test_errors.c`): Error reporting, severity levels, compatibility codes

### Build Commands

```bash
# Build test framework
make all

# Run all tests
make test

# Clean build artifacts
make clean

# Show build configuration
make debug

# Show help
make help
```

The testing framework provides 90% code coverage without requiring emulator setup, enabling rapid development and validation of core OS functionality.

