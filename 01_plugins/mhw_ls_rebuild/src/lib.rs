//! Clean host shim: no legacy controller, private motion data or hit rewrites.
#![no_std]
use core::sync::atomic::{AtomicU32,Ordering};
use forge::log::prelude::*;
static BASE:AtomicU32=AtomicU32::new(0);
unsafe extern "C" {fn ml_install(base:u32)->u32;fn ml_pad_refresh();}

#[forge::hook(offset=0x003D_E064)]
fn sample_pad(object:u32)->u32{
    let value=original!(object);
    if BASE.load(Ordering::Relaxed)!=0 {unsafe{ml_pad_refresh();}}
    value
}
#[forge::entry]
fn main(){
    let base=forge::mem::text_addr();
    #[cfg(all(not(ml_animation_preview),not(ml_world_combat)))]
    info!("[ml-rebuild] build=rebuild-b4 native_thrust_transitions=1 legacy_assets=0 scope=fade_step");
    #[cfg(ml_world_combat)]
    info!("[ml-rebuild] build=world-controller-dev1 scope=six_moves integration_validation_pending=1");
    #[cfg(ml_world_audio_probe)]
    info!("[ml-rebuild] audio-probe-only=1 independent_entry_deploy=0");
    #[cfg(ml_world_fx_probe)]
    info!("[ml-rebuild] fx-probe-only=1 independent_entry_deploy=0");
    #[cfg(ml_world_presentation)]
    info!("[ml-rebuild] presentation=world-layers-v102 world_audio=1 probe_mailbox=0");
    #[cfg(all(ml_animation_preview,not(ml_attachment_preview)))]
    info!("[ml-rebuild] build=body-preview-1 scope=animation_probe combat_ready=0");
    #[cfg(ml_attachment_preview)]
    info!("[ml-rebuild] build=attachment-preview-1 scope=attachment_probe combat_ready=0");
    let installed=unsafe{ml_install(base)};
    if installed==0 {error!("[ml-rebuild] native guards rejected; controller remains inactive");return;}
    BASE.store(base,Ordering::Relaxed);
    forge::install_hook!(base,sample_pad);
    info!("[ml-rebuild] installed; Guild only; R/ZR separate; native collision retained");
}

#[cfg(ml_world_combat)]
static GAUGE_GLOW: AtomicU32 = AtomicU32::new(0);

#[cfg(ml_world_combat)]
#[unsafe(no_mangle)]
extern "C" fn ml_gauge_glow(value: u32) {
    GAUGE_GLOW.store(value, Ordering::Relaxed);
}

#[cfg(ml_world_combat)]
#[forge::imgui_free_render]
fn draw_gauge_glow() {
    use forge::imgui::{color::color, context, draw::DrawList};
    use forge::sys::imgui::ImVec2;
    let value = GAUGE_GLOW.swap(0, Ordering::Relaxed);
    if value == 0 { return; }
    let size = context::get_io().DisplaySize;
    let scale = (size.x / 640.0).min(size.y / 360.0);
    let left = (size.x - 640.0 * scale) * 0.5;
    let top = (size.y - 360.0 * scale) * 0.5;
    let width = 48.0 * (value >> 8) as f32 / 100.0;
    let dl = DrawList::foreground();
    dl.add_rect_filled(ImVec2::new(left+64.0*scale, top+38.0*scale),
        ImVec2::new(left+(64.0+width)*scale, top+40.0*scale),
        color(248,235,223,value as u8), 0.0);
}
