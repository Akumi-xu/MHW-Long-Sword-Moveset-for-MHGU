fn main() {
    let dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    println!("cargo:rustc-link-arg=-T{dir}/link/switch32.ld");
    println!("cargo:rustc-link-arg=-Wl,--build-id=sha1");
    println!("cargo:rerun-if-changed=link/switch32.ld");

    // 版式选择：构建时设 SHARPNESS_LAYOUT=handheld 出掌机版，其他/不设出底座版。
    // 两套坐标是旧插件两个成品里实测出来的，写死比运行时猜模式可靠。
    println!("cargo:rerun-if-env-changed=SHARPNESS_LAYOUT");
    println!("cargo:rerun-if-env-changed=SHARPNESS_DEBUG");
    println!("cargo:rerun-if-env-changed=SHARPNESS_ADJ");
    println!("cargo:rustc-check-cfg=cfg(sharpness_adj)");
    if std::env::var("SHARPNESS_ADJ").as_deref() == Ok("1") {
        println!("cargo:rustc-cfg=sharpness_adj");
    }

    println!("cargo:rustc-check-cfg=cfg(sharp_dbg)");
    if std::env::var("SHARPNESS_DEBUG").as_deref() == Ok("1") {
        println!("cargo:rustc-cfg=sharp_dbg");
    }
    println!("cargo:rustc-check-cfg=cfg(sharpness_handheld)");
    println!("cargo:rustc-check-cfg=cfg(sharpness_docked)");
    match std::env::var("SHARPNESS_LAYOUT").as_deref() {
        Ok("handheld") => println!("cargo:rustc-cfg=sharpness_handheld"),
        _ => println!("cargo:rustc-cfg=sharpness_docked"),
    }
}
