#![no_std]

//! MHGU 大型怪物手动显隐 MOD
//!
//! `L + 十字键左/右` 分别隐藏 / 恢复两只大型怪物。
//!
//! 隐藏时：
//! * 体型倍率 +0x15F0 写 0.001，模型不可见；
//! * 把 sFestaUnit + 0x88 / + 0x8C 的 unit 列表重建为“只包含没隐藏的怪”的
//!   连续前缀，游戏的 unit 更新循环扫不到隐藏的怪，AI/动作/特效/音效全部停止。
//!
//! 恢复时写回体型，并重新把怪排进 unit 列表；不碰动作和帧步长，
//! 让 AI 从原来的状态自然继续（清零动作会让怪恢复后僵住，要被打一下才动）。
//!
//! 大怪/小怪用 DTI 类名区分：uEm###_00 是大怪，uEms###_00 是小怪。
//! 偏移对应 MHGU 1.4.0（Title 0100770008DD8000）。

extern crate alloc;

use core::sync::atomic::{AtomicU32, Ordering};

use forge::input::{Button, Input};
use forge::log::prelude::*;
use forge::mt::object::MtObject;
use forge::singleton::SingletonManager;

const OFF_ENEMY0: u32 = 0x1C;
const ENEMY_COUNT: u32 = 10;
const OFF_STATUS: u32 = 0x1428;
const OFF_NET_ID: u32 = 0x1B6;

const OFF_SIZE: u32 = 0x15F0;

const OFF_UNIT0: u32 = 0x88;
const OFF_UNIT1: u32 = 0x8C;

const HIDE_SIZE: f32 = 0.001;

static ENEMY: AtomicU32 = AtomicU32::new(0);
static OBJ: [AtomicU32; 2] = [AtomicU32::new(0), AtomicU32::new(0)];
static ORIG: [AtomicU32; 2] = [AtomicU32::new(0), AtomicU32::new(0)];
static HIDDEN: [AtomicU32; 2] = [AtomicU32::new(0), AtomicU32::new(0)];
static FRAME: AtomicU32 = AtomicU32::new(0);

#[inline]
unsafe fn rd32(a: u32) -> u32 {
    unsafe { core::ptr::read_volatile(a as *const u32) }
}

#[inline]
unsafe fn rd16(a: u32) -> u16 {
    unsafe { core::ptr::read_volatile(a as *const u16) }
}

#[inline]
unsafe fn rdf(a: u32) -> f32 {
    unsafe { core::ptr::read_volatile(a as *const f32) }
}

#[inline]
unsafe fn wr32(a: u32, v: u32) {
    unsafe { core::ptr::write_volatile(a as *mut u32, v) }
}

#[inline]
unsafe fn wrf(a: u32, v: f32) {
    unsafe { core::ptr::write_volatile(a as *mut f32, v) }
}

fn singleton(name: &str) -> u32 {
    match SingletonManager::get_by_name(name) {
        Some(o) => o as *const MtObject as u32,
        None => 0,
    }
}

fn plausible_obj(p: u32) -> bool {
    // 实机/eden 的堆在 0x40000000 以上，重编译版（PC/安卓）的 guest 堆在 0x0Cxxxxxx 一带，
    // 所以下限不能写死 0x40000000，否则重编译版永远找不到怪（并把 unit 列表清空 → 怪直接消失）。
    p >= 0x0020_0000 && p < 0x8000_0000 && (p & 3) == 0
}

fn plausible_vt(v: u32) -> bool {
    let b = forge::mem::text_addr();
    let lo = b.wrapping_add(0x172F000);
    let hi = b.wrapping_add(0x184AD78 + 0x1000);
    v >= lo && v < hi && (v & 3) == 0
}

unsafe fn live(p: u32) -> bool {
    plausible_obj(p) && plausible_vt(unsafe { rd32(p) })
}

unsafe fn net_id(en: u32) -> u16 {
    if !unsafe { live(en) } {
        return 0xFFFF;
    }
    let st = unsafe { rd32(en.wrapping_add(OFF_STATUS)) };
    if !plausible_obj(st) {
        return 0xFFFF;
    }
    unsafe { rd16(st.wrapping_add(OFF_NET_ID)) }
}

unsafe fn is_large(en: u32) -> bool {
    if !unsafe { live(en) } {
        return false;
    }
    let obj = unsafe { &*(en as *const MtObject) };
    let name = obj.dti().name();
    let b = name.as_bytes();
    b.len() > 3 && b[0] == b'u' && b[1] == b'E' && b[2] == b'm' && b[3].is_ascii_digit()
}

unsafe fn collect_large(e: u32) -> ([u32; 2], [u16; 2]) {
    let mut out = [0u32; 2];
    let mut ids = [0u16; 2];
    let mut n = 0usize;
    for k in 0..ENEMY_COUNT {
        let en = unsafe { rd32(e.wrapping_add(OFF_ENEMY0 + 4 * k)) };
        if !unsafe { is_large(en) } {
            continue;
        }
        if n < 2 {
            out[n] = en;
            ids[n] = unsafe { net_id(en) };
            n += 1;
        }
        if n == 2 {
            break;
        }
    }
    (out, ids)
}

unsafe fn toggle(i: usize) {
    let p = OBJ[i].load(Ordering::Relaxed);
    if !unsafe { live(p) } {
        info!("[hide] slot {} toggle ignored", i);
        return;
    }
    if HIDDEN[i].load(Ordering::Relaxed) == 0 {
        let raw = unsafe { rdf(p.wrapping_add(OFF_SIZE)) };
        let size = if (0.05..=20.0).contains(&raw) { raw } else { 1.0 };
        ORIG[i].store(size.to_bits(), Ordering::Relaxed);
        HIDDEN[i].store(1, Ordering::Relaxed);
        unsafe { wrf(p.wrapping_add(OFF_SIZE), HIDE_SIZE) };
        info!("[hide] slot {} HIDDEN (size {})", i, size);
    } else {
        let bits = ORIG[i].load(Ordering::Relaxed);
        let size = if bits != 0 { f32::from_bits(bits) } else { 1.0 };
        unsafe { wrf(p.wrapping_add(OFF_SIZE), size) };
        HIDDEN[i].store(0, Ordering::Relaxed);
        info!("[hide] slot {} SHOWN (size {})", i, size);
    }
}

unsafe fn update() {
    let mut e = ENEMY.load(Ordering::Relaxed);
    if e == 0 || !plausible_obj(e) {
        e = singleton("sEnemy");
        ENEMY.store(e, Ordering::Relaxed);
    }
    if e == 0 {
        return;
    }

    let (mut ptrs, _) = unsafe { collect_large(e) };
    if ptrs[0] == 0 || ptrs[1] == 0 {
        let u = singleton("sFestaUnit");
        if u != 0 {
            let up = [
                unsafe { rd32(u.wrapping_add(OFF_UNIT0)) },
                unsafe { rd32(u.wrapping_add(OFF_UNIT1)) },
            ];
            for i in 0..2 {
                if ptrs[i] == 0 && unsafe { live(up[i]) } && !ptrs.contains(&up[i]) {
                    ptrs[i] = up[i];
                }
            }
        }
    }

    for i in 0..2 {
        if ptrs[i] != OBJ[i].load(Ordering::Relaxed) {
            // 旧对象若还藏着，先恢复体型
            let old = OBJ[i].load(Ordering::Relaxed);
            if old != 0 && HIDDEN[i].swap(0, Ordering::AcqRel) != 0 {
                if plausible_vt(unsafe { rd32(old) }) {
                    let bits = ORIG[i].load(Ordering::Relaxed);
                    if bits != 0 {
                        unsafe { wrf(old.wrapping_add(OFF_SIZE), f32::from_bits(bits)) };
                    }
                }
            }
            OBJ[i].store(ptrs[i], Ordering::Relaxed);
            ORIG[i].store(0, Ordering::Relaxed);
            if ptrs[i] != 0 {
                info!(
                    "[hide] slot {} monster={:08X} id={}",
                    i,
                    ptrs[i],
                    unsafe { net_id(ptrs[i]) }
                );
            }
        }
    }

    if Input::is_button_down(Button::L) {
        if Input::is_button_pressed(Button::Left) {
            unsafe { toggle(0) };
        }
        if Input::is_button_pressed(Button::Right) {
            unsafe { toggle(1) };
        }
    }

    // 隐藏的怪每帧压一次体型
    for i in 0..2 {
        if HIDDEN[i].load(Ordering::Relaxed) != 0 {
            let p = OBJ[i].load(Ordering::Relaxed);
            if unsafe { live(p) } {
                unsafe { wrf(p.wrapping_add(OFF_SIZE), HIDE_SIZE) };
            }
        }
    }

    // 重建 sFestaUnit 的 unit 列表：只保留没隐藏的怪，连续排在前缀。
    // 游戏更新循环遇到 0 就停；直接清 slot0 会把 slot1 的怪也一起冻住。
    // 只有确实认出了怪才动这个列表——认不出时写成 [0,0] 会让怪直接消失（重编译版的旧毛病）。
    let u = singleton("sFestaUnit");
    if u != 0
        && (OBJ[0].load(Ordering::Relaxed) != 0 || OBJ[1].load(Ordering::Relaxed) != 0)
    {
        let mut out = [0u32; 2];
        let mut n = 0usize;
        for i in 0..2 {
            if HIDDEN[i].load(Ordering::Relaxed) == 0 {
                let p = OBJ[i].load(Ordering::Relaxed);
                if unsafe { live(p) } {
                    out[n] = p;
                    n += 1;
                }
            }
        }
        unsafe {
            wr32(u.wrapping_add(OFF_UNIT0), out[0]);
            wr32(u.wrapping_add(OFF_UNIT1), out[1]);
        }
    }
}

#[forge::entry]
fn main() {
    info!("[hide] monster_hide loaded: L+Left / L+Right toggles large monster 1 / 2");
}

#[unsafe(no_mangle)]
pub extern "C" fn forge_onUpdate() {
    FRAME.fetch_add(1, Ordering::Relaxed);
    unsafe { update() };
}
