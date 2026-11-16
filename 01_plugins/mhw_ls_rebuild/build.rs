use std::{env,path::PathBuf,process::Command};
fn main(){
    let dir=env::var("CARGO_MANIFEST_DIR").unwrap();
    let out=PathBuf::from(env::var("OUT_DIR").unwrap());
    let gcc=env::var("ARM_GCC").unwrap_or_else(|_|"arm-none-eabi-gcc".into());
    let preview=env::var("ML_ANIMATION_PREVIEW").as_deref()==Ok("1");
    let attachments=env::var("ML_ATTACHMENT_PREVIEW").as_deref()==Ok("1");
    let combat=env::var("ML_WORLD_COMBAT").as_deref()==Ok("1");
    let input_vm=env::var("ML_WORLD_INPUT_VM").as_deref()==Ok("1");
    assert!(!input_vm||combat,"World input VM requires the combat build");
    println!("cargo:rerun-if-env-changed=ML_WORLD_INPUT_VM");
    let audio_probe=env::var("ML_WORLD_AUDIO_PROBE").as_deref()==Ok("1");
    let fx_probe=env::var("ML_WORLD_FX_PROBE").as_deref()==Ok("1");
    let presentation=env::var("ML_WORLD_PRESENTATION").as_deref()==Ok("1");
    assert!(!presentation||(combat&&!fx_probe&&!audio_probe),"Presentation build is separate from probes");
    println!("cargo:rerun-if-env-changed=ML_WORLD_PRESENTATION");
    println!("cargo:rustc-check-cfg=cfg(ml_world_presentation)");
    if presentation {println!("cargo:rustc-cfg=ml_world_presentation");}
    assert!(!fx_probe||combat,"Effect probe requires World controller");
    assert!(!audio_probe||combat,"Audio probe requires World controller");
    assert!(!combat||!preview,"World controller and sequential preview are separate builds");
    assert!(!attachments||preview,"Attachment probe requires body preview");
    println!("cargo:rerun-if-env-changed=ML_ANIMATION_PREVIEW");
    println!("cargo:rerun-if-env-changed=ML_ATTACHMENT_PREVIEW");
    println!("cargo:rerun-if-env-changed=ML_WORLD_COMBAT");
    println!("cargo:rerun-if-env-changed=ML_WORLD_AUDIO_PROBE");
    println!("cargo:rerun-if-env-changed=ML_WORLD_FX_PROBE");
    println!("cargo:rustc-check-cfg=cfg(ml_world_fx_probe)");
    if fx_probe { println!("cargo:rustc-cfg=ml_world_fx_probe"); }
    println!("cargo:rustc-check-cfg=cfg(ml_world_combat)");
    println!("cargo:rustc-check-cfg=cfg(ml_world_audio_probe)");
    if combat { println!("cargo:rustc-cfg=ml_world_combat"); }
    if audio_probe { println!("cargo:rustc-cfg=ml_world_audio_probe"); }
    println!("cargo:rustc-check-cfg=cfg(ml_animation_preview)");
    println!("cargo:rustc-check-cfg=cfg(ml_attachment_preview)");
    if preview { println!("cargo:rustc-cfg=ml_animation_preview"); }
    if attachments { println!("cargo:rustc-cfg=ml_attachment_preview"); }
    for name in ["moveset","world_core","gu_adapter"]{
        let obj=out.join(format!("{name}.o"));
        let status=Command::new(&gcc).arg(if preview {"-DML_ANIMATION_PREVIEW=1"} else {"-DML_ANIMATION_PREVIEW=0"})
            .arg(if attachments {"-DML_ATTACHMENT_PREVIEW=1"} else {"-DML_ATTACHMENT_PREVIEW=0"})
            .arg(if combat {"-DML_WORLD_COMBAT=1"} else {"-DML_WORLD_COMBAT=0"})
            .arg(if input_vm {"-DML_WORLD_INPUT_VM=1"} else {"-DML_WORLD_INPUT_VM=0"})
            .arg(if audio_probe {"-DML_WORLD_AUDIO_PROBE=1"} else {"-DML_WORLD_AUDIO_PROBE=0"})
            .arg(if fx_probe {"-DML_WORLD_FX_PROBE=1"} else {"-DML_WORLD_FX_PROBE=0"})
            .arg(if presentation {"-DML_WORLD_PRESENTATION=1"} else {"-DML_WORLD_PRESENTATION=0"})
            .arg(format!("-I{dir}/../../.cache/mhw_ls_animation_port"))
            .args(["-mcpu=cortex-a8","-marm","-mfloat-abi=softfp","-mfpu=vfpv3-d16","-O2","-fPIC","-ffreestanding","-fno-builtin","-fno-common","-Wall","-Wextra","-I"])
            .arg(format!("{dir}/c")).arg("-c").arg(format!("{dir}/c/{name}.c")).arg("-o").arg(&obj).status().unwrap();
        assert!(status.success(),"C compilation failed: {name}");
        println!("cargo:rustc-link-arg={}",obj.display());
    }
    println!("cargo:rustc-link-arg=-T{dir}/link/switch32.ld");
    println!("cargo:rustc-link-arg=-Wl,--build-id=sha1");
    println!("cargo:rerun-if-changed=c");
    if attachments || combat {println!("cargo:rerun-if-changed={dir}/../../.cache/mhw_ls_animation_port/attachment_data.h");}
}
