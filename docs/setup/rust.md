# install rust

```
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

# VS Code

Install extensions:

rust-analyzer (mandatory)

CodeLLDB (debugging)

Rust does not work well without rust-analyzer.

# C + Rust interop tools

```
cargo install cbindgen
cargo install cargo-expand
```

* cbindgen → generate C headers
* cargo-expand → inspect macro-expanded code

# Install Rust ESP tools

```
cargo install espup
espup install
```

This installs:

* esp32 Rust toolchains
* LLVM + Xtensa patches
* Cargo runners

```
source ~/export-esp.sh
```