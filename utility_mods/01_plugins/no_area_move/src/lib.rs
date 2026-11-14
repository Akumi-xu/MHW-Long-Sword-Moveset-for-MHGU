//! 阻止"普通走到地图边缘触发区域传送"，保留若干合法换图方式：
//!   蹲下 / 蹲走、攀爬、跳下、长按/点按 L 或 ZL（道具轮盘）。
//!
//! 机制（GU v1.4.0 main 偏移）：
//!   - 0x50005C = `sStage::checkAreaMove()`：任务场景任务 sub_6069F9FC 每帧调用，
//!     遍历当前区域的 areaMove 触发点；玩家在触发点内就置 `area+74=下一区域`、
//!     `area+81=出现点`、`sStage+420 |= 2`，随后任务切到过渡状态开始换区。
//!   - 函数开头门控 `!sub_603A5ED0(sQuest,5,1) && sQuest+85 <= 0xC`，
//!     任务完成结算时阶段 9 会让它直接返回 0 —— 这就是"结算时边缘不传送"的原因。
//!
//! 实测确认（2026-10-31，Eden）：
//!   - 蹲下/蹲走：act = 1/23、1/24、1/25；
//!   - 攀爬：act = 810 上爬 / 880 下爬 / 1240 攀爬跳跃（固定开启，不配置）；
//!   - 跳下：act_type = 4（固定开启，不配置）；
//!   - 切道具：不改变 act（恒为 1/0），只能按输入判定；长按 L/ZL 开轮盘，
//!     点按 L/ZL 是否放行由 ini 开关控制；
//!   - 普通走/跑（1/0、1/7、2/x）仍然拦。
//!
//! 判定参数都在 savedata 的 `no_area_move.ini` 里，改完重启游戏生效。

#![no_std]
#![allow(unsafe_op_in_unsafe_fn)]
#![allow(non_snake_case)]

extern crate alloc;

use alloc::string::String;
use core::sync::atomic::{AtomicU32, Ordering};

use forge::input::{Button, Input};
use forge::log::prelude::*;
use forge::{IniDeserialize, IniSerialize};

const MAX_LIST: usize = 32;

/// savedata 根目录 `no_area_move.ini`。字段都可缺省；缺省（空串）时用默认值。
#[derive(Default, IniDeserialize, IniSerialize)]
struct Cfg {
    enabled: String,
    log_act: String,
    allow_crouch: String,
    allow_item: String,
    item_by_tap: String,
    item_by_hold: String,
    item_by_tap_zl: String,
    item_by_hold_zl: String,
    item_hold_frames: String,
    crouch_acts: String,
    crouch_walk_types: String,
    crouch_walk_acts: String,
    item_acts: String,
    item_types: String,
}

const DEF_CROUCH: &str = "23,24,25,27,28,31,32,35,37";
const DEF_CROUCH_WALK_TYPES: &str = "2";
const DEF_CROUCH_WALK_ACTS: &str = "7";
const DEF_HOLD_FRAMES: u32 = 18;

/// 攀爬/跳下：固定开启，不放进配置。
const CLIMB_ACTS: [u32; 3] = [810, 880, 1240];
const JUMP_TYPE: u32 = 4;

/// sStage::checkAreaMove()，插件偏移（main .text 基址 + 该值）。
const FN_AREA_CHECK: u32 = 0x0050_005C;
/// 函数头 `push {r4-r11, lr}`，用来核对版本/偏移。
const PROLOGUE: u32 = 0xE92D_4FF0;

/// `dword_61885B58`：sPlayer 单例指针（.data 里的全局）。
const G_SPLAYER: u32 = 0x0188_5B58;
/// `sub_60276278(sPlayer)`：取本地玩家。
const FN_LOCAL_PLAYER: u32 = 0x0027_6278;

const OFF_ACT_TYPE: u32 = 9478; // +0x2506 u16
const OFF_ACT_NO: u32 = 9482; // +0x250A u16

static BASE: AtomicU32 = AtomicU32::new(0);
static ENABLED: AtomicU32 = AtomicU32::new(1);
static LOG_ACT: AtomicU32 = AtomicU32::new(0);
static ALLOW_CROUCH: AtomicU32 = AtomicU32::new(1);
static ALLOW_ITEM: AtomicU32 = AtomicU32::new(1);
static ITEM_BY_TAP: AtomicU32 = AtomicU32::new(0);
static ITEM_BY_HOLD: AtomicU32 = AtomicU32::new(1);
static ITEM_BY_TAP_ZL: AtomicU32 = AtomicU32::new(0);
static ITEM_BY_HOLD_ZL: AtomicU32 = AtomicU32::new(0);
static ITEM_HOLD_FRAMES: AtomicU32 = AtomicU32::new(DEF_HOLD_FRAMES);
static CROUCH_MODE: AtomicU32 = AtomicU32::new(0);
static L_HELD: AtomicU32 = AtomicU32::new(0);
static ZL_HELD: AtomicU32 = AtomicU32::new(0);

static CROUCH_V: [AtomicU32; MAX_LIST] = [const { AtomicU32::new(0) }; MAX_LIST];
static CROUCH_N: AtomicU32 = AtomicU32::new(0);
static CWALK_T: [AtomicU32; MAX_LIST] = [const { AtomicU32::new(0) }; MAX_LIST];
static CWALK_TN: AtomicU32 = AtomicU32::new(0);
static CWALK_A: [AtomicU32; MAX_LIST] = [const { AtomicU32::new(0) }; MAX_LIST];
static CWALK_AN: AtomicU32 = AtomicU32::new(0);
static ITEM_V: [AtomicU32; MAX_LIST] = [const { AtomicU32::new(0) }; MAX_LIST];
static ITEM_N: AtomicU32 = AtomicU32::new(0);
static ITEM_T: [AtomicU32; MAX_LIST] = [const { AtomicU32::new(0) }; MAX_LIST];
static ITEM_TN: AtomicU32 = AtomicU32::new(0);

static TICK: AtomicU32 = AtomicU32::new(0);
static LAST_TYPE: AtomicU32 = AtomicU32::new(0xFFFF_FFFF);
static LAST_NO: AtomicU32 = AtomicU32::new(0xFFFF_FFFF);
static N_ALLOW: AtomicU32 = AtomicU32::new(0);
static N_BLOCK: AtomicU32 = AtomicU32::new(0);

#[inline]
unsafe fn rd16(a: u32) -> u32 {
    core::ptr::read_volatile(a as *const u16) as u32
}

#[inline]
unsafe fn rd32(a: u32) -> u32 {
    core::ptr::read_volatile(a as *const u32)
}

unsafe fn local_player(base: u32) -> u32 {
    let sp = rd32(base.wrapping_add(G_SPLAYER));
    if sp == 0 {
        return 0;
    }
    let f: unsafe extern "C" fn(u32) -> u32 =
        core::mem::transmute(base.wrapping_add(FN_LOCAL_PLAYER));
    f(sp)
}

fn parse_bool(s: &str, def: bool) -> bool {
    match s.trim().to_ascii_lowercase().as_str() {
        "true" | "1" | "yes" | "on" => true,
        "false" | "0" | "no" | "off" => false,
        _ => def,
    }
}

fn parse_u32(s: &str, def: u32) -> u32 {
    s.trim().parse::<u32>().unwrap_or(def)
}

fn parse_into(s: &str, vals: &[AtomicU32; MAX_LIST], n: &AtomicU32) {
    let mut count = 0usize;
    for part in s.split(',') {
        let t = part.trim();
        if t.is_empty() {
            continue;
        }
        if let Ok(v) = t.parse::<u32>() {
            if count < MAX_LIST {
                vals[count].store(v, Ordering::Relaxed);
                count += 1;
            }
        }
    }
    n.store(count as u32, Ordering::Relaxed);
}

fn in_list(v: u32, vals: &[AtomicU32; MAX_LIST], n: &AtomicU32) -> bool {
    let count = n.load(Ordering::Relaxed) as usize;
    for i in 0..count {
        if vals[i].load(Ordering::Relaxed) == v {
            return true;
        }
    }
    false
}

fn is_climb(no: u32) -> bool {
    CLIMB_ACTS.contains(&no)
}

/// 替换 `checkAreaMove`：命中配置里的任一状态才跑原函数。
#[forge::hook(offset = 0x50005C)]
fn area_check(stage: u32) -> i32 {
    if ENABLED.load(Ordering::Relaxed) == 0 {
        return original!(stage);
    }
    let base = BASE.load(Ordering::Relaxed);
    let p = unsafe { local_player(base) };
    if p == 0 {
        return 0;
    }

    let (ty, no) = unsafe {
        (
            rd16(p.wrapping_add(OFF_ACT_TYPE)),
            rd16(p.wrapping_add(OFF_ACT_NO)),
        )
    };

    if LOG_ACT.load(Ordering::Relaxed) != 0 {
        let lt = LAST_TYPE.load(Ordering::Relaxed);
        let ln = LAST_NO.load(Ordering::Relaxed);
        if lt != ty || ln != no {
            LAST_TYPE.store(ty, Ordering::Relaxed);
            LAST_NO.store(no, Ordering::Relaxed);
            info!("[noarea] act={}/{}", ty, no);
        }
        // 排障：每 15 帧打一次 act + 左摇杆 + 坐标，用来分辨蹲走/普通走。
        let t = TICK.fetch_add(1, Ordering::Relaxed);
        if t % 15 == 0 {
            let (sx, sy) = Input::get_stick_l();
            unsafe {
                info!(
                    "[noarea] tick act={}/{} stick=({:.2},{:.2}) x={:.1} z={:.1}",
                    ty,
                    no,
                    sx,
                    sy,
                    core::ptr::read_volatile(p.wrapping_add(64) as *const f32),
                    core::ptr::read_volatile(p.wrapping_add(72) as *const f32),
                );
            }
        }
    }

    let crouch_static = in_list(no, &CROUCH_V, &CROUCH_N);
    // 蹲着推摇杆走：type 2 + act 7（实测），单独按类型匹配，避免误伤 1/7 普通走。
    let crouch_walk = in_list(ty, &CWALK_T, &CWALK_TN) && in_list(no, &CWALK_A, &CWALK_AN);
    let crouch_act = crouch_static || crouch_walk;

    // 蹲模式：蹲下/蹲走时进入；满推变 2/6（和普通跑同号）后继续保持，
    // 直到停下（act 1/0 或左摇杆回中）或做出非移动动作才退出。
    let (sx, sy) = Input::get_stick_l();
    let stick_sq = sx * sx + sy * sy;
    let locomotion = (ty == 1 && no == 0) || (ty == 2 && no <= 9);
    let mode = if crouch_act {
        CROUCH_MODE.store(1, Ordering::Relaxed);
        1
    } else if !locomotion || stick_sq < 0.0625 {
        CROUCH_MODE.store(0, Ordering::Relaxed);
        0
    } else {
        CROUCH_MODE.load(Ordering::Relaxed)
    };
    let crouch = ALLOW_CROUCH.load(Ordering::Relaxed) != 0 && (crouch_act || mode != 0);
    // 攀爬、跳下：固定开启，不受 ini 控制。
    let climb = is_climb(no);
    let jump = ty == JUMP_TYPE;

    // 切道具：实测不改变 act，只能按输入判定。
    let item_act = in_list(no, &ITEM_V, &ITEM_N) || in_list(ty, &ITEM_T, &ITEM_TN);
    let hold_need = ITEM_HOLD_FRAMES.load(Ordering::Relaxed);

    let l_down = Input::is_button_down(Button::L);
    let held_l = if l_down {
        L_HELD.fetch_add(1, Ordering::Relaxed) + 1
    } else {
        L_HELD.store(0, Ordering::Relaxed);
        0
    };
    let zl_down = Input::is_button_down(Button::ZL);
    let held_zl = if zl_down {
        ZL_HELD.fetch_add(1, Ordering::Relaxed) + 1
    } else {
        ZL_HELD.store(0, Ordering::Relaxed);
        0
    };

    let item_tap = ITEM_BY_TAP.load(Ordering::Relaxed) != 0
        && Input::is_button_pressed(Button::L);
    let item_tap_zl = ITEM_BY_TAP_ZL.load(Ordering::Relaxed) != 0
        && Input::is_button_pressed(Button::ZL);
    let item_hold_l = ITEM_BY_HOLD.load(Ordering::Relaxed) != 0 && held_l >= hold_need;
    let item_hold_zl = ITEM_BY_HOLD_ZL.load(Ordering::Relaxed) != 0 && held_zl >= hold_need;

    let item = ALLOW_ITEM.load(Ordering::Relaxed) != 0
        && (item_act || item_tap || item_tap_zl || item_hold_l || item_hold_zl);

    if crouch || climb || jump || item {
        let n = N_ALLOW.fetch_add(1, Ordering::Relaxed);
        if n == 0 {
            info!(
                "[noarea] allow first: act={}/{} (crouch={} climb={} jump={} item={})",
                ty, no, crouch, climb, jump, item
            );
        }
        return original!(stage);
    }

    let n = N_BLOCK.fetch_add(1, Ordering::Relaxed);
    if n == 0 {
        info!("[noarea] block first: act={}/{}", ty, no);
    }
    0
}

#[forge::entry]
fn main() {
    let base = forge::mem::text_addr();

    // 装之前核对函数头，版本/偏移不对就不动游戏。
    unsafe {
        let got = core::ptr::read_volatile(base.wrapping_add(FN_AREA_CHECK) as *const u32);
        if got != PROLOGUE {
            error!(
                "[noarea] bad prologue at 0x{:08X}: got 0x{:08X} want 0x{:08X}, skipped",
                FN_AREA_CHECK, got, PROLOGUE
            );
            return;
        }
    }

    let cfg: Cfg = forge::config::load("no_area_move.ini").unwrap_or_default();

    let enabled = parse_bool(&cfg.enabled, true);
    let log_act = parse_bool(&cfg.log_act, false);
    let allow_crouch = parse_bool(&cfg.allow_crouch, true);
    let allow_item = parse_bool(&cfg.allow_item, true);
    let item_by_tap = parse_bool(&cfg.item_by_tap, false);
    let item_by_hold = parse_bool(&cfg.item_by_hold, true);
    let item_by_tap_zl = parse_bool(&cfg.item_by_tap_zl, false);
    let item_by_hold_zl = parse_bool(&cfg.item_by_hold_zl, false);
    let hold_need = parse_u32(&cfg.item_hold_frames, DEF_HOLD_FRAMES);

    ENABLED.store(enabled as u32, Ordering::Relaxed);
    LOG_ACT.store(log_act as u32, Ordering::Relaxed);
    ALLOW_CROUCH.store(allow_crouch as u32, Ordering::Relaxed);
    ALLOW_ITEM.store(allow_item as u32, Ordering::Relaxed);
    ITEM_BY_TAP.store(item_by_tap as u32, Ordering::Relaxed);
    ITEM_BY_HOLD.store(item_by_hold as u32, Ordering::Relaxed);
    ITEM_BY_TAP_ZL.store(item_by_tap_zl as u32, Ordering::Relaxed);
    ITEM_BY_HOLD_ZL.store(item_by_hold_zl as u32, Ordering::Relaxed);
    ITEM_HOLD_FRAMES.store(hold_need.max(1), Ordering::Relaxed);

    let crouch_s = if cfg.crouch_acts.trim().is_empty() { DEF_CROUCH } else { cfg.crouch_acts.as_str() };
    let cwalk_t = if cfg.crouch_walk_types.trim().is_empty() { DEF_CROUCH_WALK_TYPES } else { cfg.crouch_walk_types.as_str() };
    let cwalk_a = if cfg.crouch_walk_acts.trim().is_empty() { DEF_CROUCH_WALK_ACTS } else { cfg.crouch_walk_acts.as_str() };
    parse_into(crouch_s, &CROUCH_V, &CROUCH_N);
    parse_into(cwalk_t, &CWALK_T, &CWALK_TN);
    parse_into(cwalk_a, &CWALK_A, &CWALK_AN);
    parse_into(&cfg.item_acts, &ITEM_V, &ITEM_N);
    parse_into(&cfg.item_types, &ITEM_T, &ITEM_TN);

    BASE.store(base, Ordering::Relaxed);
    forge::install_hook!(base, area_check);
    info!(
        "[noarea] loaded base={:#010x} crouch={} item={} tapL={} holdL={} tapZL={} holdZL={}({}f) log_act={}",
        base,
        allow_crouch,
        allow_item,
        item_by_tap,
        item_by_hold,
        item_by_tap_zl,
        item_by_hold_zl,
        hold_need,
        log_act
    );
}
