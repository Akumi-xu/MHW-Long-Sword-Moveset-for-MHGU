
fn main() {
    let dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    println!("cargo:rustc-link-arg=-T{dir}/link/switch32.ld");
    println!("cargo:rustc-link-arg=-Wl,--build-id=sha1");
    println!("cargo:rerun-if-changed=link/switch32.ld");
}
