# 构建 / Build

需要 Windows、Python 3.10+、Rust nightly 和 `rust-src`、`cargo-mhgu-forge` 1.5.0 以及 ARM GNU Embedded 工具链。将 ARM 工具链的 `bin` 加入 `PATH`，或额外设置 `ARM_GCC` 指向编译器。构建路径使用 ASCII 字符。

Use Windows, Python 3.10+, Rust nightly with `rust-src`, `cargo-mhgu-forge` 1.5.0, and the ARM GNU Embedded toolchain. Put its `bin` directory on `PATH`. You can also set `ARM_GCC` to the compiler path. Build from an ASCII-only path.

```powershell
py -3 tools/restore_build_inputs.py "C:\path\to\MHGU-World-Long-Sword-Rise-Enhanced_v1.2-Recomp.zip"
./build.ps1
```

第一条命令从本版独立安装包的 NRO 还原构建所需的挂点表，输出到本地 `.cache`。第二条命令构建加强版并检查重定位。构建产物为 `01_plugins/mhw_ls_rebuild/mhw_ls_rebuild.nro`。使用安装包解压得到的 MOD 目录，将新 NRO 放入其中的 `romfs/nativeNX/plugins`。

The first command restores the generated attachment tables from the matching standalone package's NRO into the local `.cache`. The second builds the Enhanced plugin and checks relocations. The output is `01_plugins/mhw_ls_rebuild/mhw_ls_rebuild.nro`. Place it in `romfs/nativeNX/plugins` in the extracted mod directory.
