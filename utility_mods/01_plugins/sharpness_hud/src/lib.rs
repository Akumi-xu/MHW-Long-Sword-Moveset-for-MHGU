#![no_std]

//! MHGU 斩味条 HUD
//!
//! 每帧从游戏内存里读当前武器的斩味值，用 ImGui 前景绘制列表直接画在画面上：
//!
//! * 7 段颜色（红→橙→黄→绿→蓝→白→紫），用掉的部分变暗，当前值画一条白色刻度；
//! * 远程武器（重弩 W04、轻弩 W06、弓 W10）和猎猫（W15）不显示；
//! * 位置/大小按画面分辨率算，**同一份插件同时适配底座模式和掌机模式**，
//!   不用再像原来那样编两个 NRO。
//!
//! ## 数据来源（全部是 GU main 模块的偏移）
//!
//! ```text
//! base        = forge::mem::text_addr()
//! sPlayer     = *(base + 0x1885B58)
//! player      = fn_0x276278(sPlayer)          // 取本地玩家对象
//! vtable      = *player                        // 用来判断武器种类
//! kire_cur    = *(player + 0x2774)             // 当前斩味值
//! kire_max    = *(player + 0x2778)             // 斩味总长（最大斩味值）
//! color_of(i) = fn_0x29CDFC(player, i)         // 第 i 点斩味的颜色 0..6
//! ```
//!
//! `fn_0x29CDFC` 就是游戏里的 `sub_6029CDFC`（见反编译 `main.c`），它按武器当前
//! 斩味等级返回色阶；`player+0x13F0` 是游戏自己缓存的结果（原插件拿它当有效性判断）。
//!
//! 偏移对应的游戏版本：MHGU 1.4.0。游戏更新后这些偏移要重新核对。

extern crate alloc;

use core::sync::atomic::{AtomicU32, Ordering};

use forge::imgui::color::color;
use forge::imgui::context;
use forge::imgui::draw::DrawList;
use forge::log::prelude::*;
#[cfg(sharp_dbg)]
use forge::mt::object::MtObject;
#[cfg(sharp_dbg)]
use forge::singleton::SingletonManager;
use forge::imgui::widgets::separator_text;
#[cfg(sharpness_adj)]
use forge::input::{Button, Input};

#[cfg(sharpness_adj)]
use forge::imgui::widgets::{button, text};
use forge::sys::imgui::ImVec2;
#[cfg(sharpness_adj)]
use forge::config::{IniDeserialize, IniSerialize};
#[cfg(sharpness_adj)]
use alloc::format;

// ---------------------------------------------------------------- 游戏偏移
const OFF_S_PLAYER: u32 = 0x1885B58;
const FN_LOCAL_PLAYER: u32 = 0x276278;
const FN_KIRE_COLOR: u32 = 0x29CDFC;
const OFF_KIRE_CUR: u32 = 0x2774;
const OFF_KIRE_MAX: u32 = 0x2778;
/// 游戏缓存的当前斩味色阶（`sub_6029CDFC` 的结果）
const OFF_KIRE_LEVEL: u32 = 0x13F0;

/// 玩家对象 vtable → 武器编号（w00..w15）
const WEAPONS: [(u32, u8); 15] = [
    (0x1749A88, 0),  // w00 大剑
    (0x174A354, 1),  // w01 片手剑
    (0x174AC20, 2),  // w02 大锤
    (0x174B4F0, 3),  // w03 长枪
    (0x174BDC4, 4),  // w04 重弩
    (0x174C694, 6),  // w06 轻弩
    (0x174CF64, 7),  // w07 太刀
    (0x174D838, 8),  // w08 斩击斧
    (0x174E110, 9),  // w09 铳枪
    (0x174E9E0, 10), // w10 弓
    (0x174F2B4, 11), // w11 双剑
    (0x174FB80, 12), // w12 狩猎笛
    (0x1750450, 13), // w13 操虫棍
    (0x1750D20, 14), // w14 盾斧
    (0x17515EC, 15), // w15 猎猫
];

/// 没有斩味条的武器：重弩 / 轻弩 / 弓 / 猎猫
const HIDDEN: [u8; 4] = [4, 6, 10, 15];

/// 斩味配色（取自旧插件的颜色表，索引 0..6 = 红 橙 黄 绿 蓝 白 紫）
const PALETTE: [(u8, u8, u8); 7] = [
    (0xE6, 0x36, 0x2E),
    (0xF2, 0x8C, 0x1E),
    (0xF4, 0xD4, 0x3D),
    (0x4E, 0xD6, 0x4A),
    (0x2F, 0x7B, 0xFF),
    (0xF0, 0xF0, 0xF0),
    (0xB0, 0x6C, 0xFF),
];

// ---------------------------------------------------------------- 版面
// 两套基准坐标来自旧插件的两个成品。构建时用 SHARPNESS_LAYOUT 选：
//   SHARPNESS_LAYOUT=handheld  → 掌机版（720p 基准）
//   不设 / 其他               → 底座版（1080p 基准）
// 字段：x0, x1, y0, y1, 基准宽, 基准高
/// (x, y, 宽, 高)，单位是屏幕像素
///
/// 这个矩形按实测的原生斩味条"内框"放大过：原生条是贴图精灵，代码关不掉，
/// 所以用我们的条（不透明）把它整个盖住。实测（1080p 截图）：
///   原生银框上沿 y=57..59，下沿 y=87..89，填充区约 x 182..406, y 59..87
#[cfg(sharpness_handheld)]
const PROFILE: (i32, i32, i32, i32) = (128, 41, 109, 17);
#[cfg(not(sharpness_handheld))]
const PROFILE: (i32, i32, i32, i32) = (192, 62, 164, 27);

/// 构建出来的版本名（写进日志，方便确认装的是哪一个）
#[cfg(sharpness_handheld)]
const LAYOUT_NAME: &str = "掌机 Handheld 720p";
#[cfg(not(sharpness_handheld))]
const LAYOUT_NAME: &str = "底座 Docked 1080p";

// ---------------------------------------------------------------- 状态
// 游戏线程写、渲染线程读。SEQ 为奇数表示正在写，读者跳过这一帧。
static SEQ: AtomicU32 = AtomicU32::new(0);
static VALID: AtomicU32 = AtomicU32::new(0);
static TOTAL: AtomicU32 = AtomicU32::new(0);
static CUR: AtomicU32 = AtomicU32::new(0);
static ENDS: [AtomicU32; 7] = [
    AtomicU32::new(0),
    AtomicU32::new(0),
    AtomicU32::new(0),
    AtomicU32::new(0),
    AtomicU32::new(0),
    AtomicU32::new(0),
    AtomicU32::new(0),
];

// 重算缓存：玩家 / 武器 / 斩味总长没变就不重新算颜色表
static CACHE_PLAYER: AtomicU32 = AtomicU32::new(0);
static CACHE_WEAPON: AtomicU32 = AtomicU32::new(0);
static CACHE_TOTAL: AtomicU32 = AtomicU32::new(0);

#[inline]
unsafe fn rd32(addr: u32) -> u32 {
    unsafe { core::ptr::read_volatile(addr as *const u32) }
}

/// 运行时覆盖（只有调整版会写；固定版恒为 UNSET）
#[cfg(sharpness_adj)]
static OV_X: core::sync::atomic::AtomicI32 = core::sync::atomic::AtomicI32::new(i32::MIN);
#[cfg(sharpness_adj)]
static OV_Y: core::sync::atomic::AtomicI32 = core::sync::atomic::AtomicI32::new(i32::MIN);
#[cfg(sharpness_adj)]
static OV_W: core::sync::atomic::AtomicI32 = core::sync::atomic::AtomicI32::new(i32::MIN);
#[cfg(sharpness_adj)]
static OV_H: core::sync::atomic::AtomicI32 = core::sync::atomic::AtomicI32::new(i32::MIN);

/// 固定版面（固定版写死；调整版读运行时覆盖）
fn layout() -> (i32, i32, i32, i32) {
    #[cfg(sharpness_adj)]
    {
        use core::sync::atomic::Ordering as O;
        let f = |a: &core::sync::atomic::AtomicI32, d: i32| {
            let v = a.load(O::Relaxed);
            if v == i32::MIN { d } else { v }
        };
        return (f(&OV_X, PROFILE.0), f(&OV_Y, PROFILE.1), f(&OV_W, PROFILE.2), f(&OV_H, PROFILE.3));
    }
    #[cfg(not(sharpness_adj))]
    {
        PROFILE
    }
}

/// 按比例压暗颜色（num/100）
#[inline]
fn shade(c: (u8, u8, u8), num: u32) -> (u8, u8, u8) {
    (
        ((c.0 as u32 * num) / 100) as u8,
        ((c.1 as u32 * num) / 100) as u8,
        ((c.2 as u32 * num) / 100) as u8,
    )
}

/// 从 vtable 反查武器编号
fn weapon_from_vtable(base: u32, vt: u32) -> Option<u8> {
    for &(off, id) in WEAPONS.iter() {
        if vt == base.wrapping_add(off) {
            return Some(id);
        }
    }
    None
}

#[cfg(sharp_dbg)]
static LAST_REASON: AtomicU32 = AtomicU32::new(0);

fn hide_reason(_why: &str) {
    VALID.store(0, Ordering::Release);
    CACHE_PLAYER.store(0, Ordering::Relaxed);
    CACHE_WEAPON.store(0, Ordering::Relaxed);
    CACHE_TOTAL.store(0, Ordering::Relaxed);
    #[cfg(sharp_dbg)]
    {
        // 原因字符串哈希，变了才打日志（免得刷屏）
        let mut h: u32 = 2166136261;
        for b in _why.as_bytes() {
            h = (h ^ (*b as u32)).wrapping_mul(16777619);
        }
        if LAST_REASON.swap(h, Ordering::Relaxed) != h {
            info!("[qs] hide: {}", _why);
        }
    }
}


// ================================================================
// 探针：找"任务中 / 结算 / 回城"三种状态的判别字段
// ================================================================
// 每秒最多打一次，且只在数值变化时打，避免刷屏。
#[cfg(sharp_dbg)]
static PROBE_TICK: AtomicU32 = AtomicU32::new(0);
#[cfg(sharp_dbg)]
static PROBE_LAST: AtomicU32 = AtomicU32::new(0);

#[cfg(sharp_dbg)]
unsafe fn probe_state(player: u32, cur: u32, total: u32) {
    let t = PROBE_TICK.fetch_add(1, Ordering::Relaxed);
    if t % 60 != 0 {
        return;
    }
    let q = match SingletonManager::get_by_name("sQuest") {
        Some(o) => o as *const MtObject as u32,
        None => 0,
    };
    let p24 = unsafe { rd32(player.wrapping_add(0x2844)) };
    let lvl = unsafe { rd32(player.wrapping_add(OFF_KIRE_LEVEL)) };
    let mut q0 = [0u32; 16];
    if q != 0 {
        for (i, v) in q0.iter_mut().enumerate() {
            *v = unsafe { rd32(q.wrapping_add((i as u32) * 4)) };
        }
    }
    // 用异或做变化检测
    let mut sig = cur ^ total.rotate_left(3) ^ p24.rotate_left(7) ^ lvl.rotate_left(11);
    for (i, v) in q0.iter().enumerate() {
        sig ^= v.rotate_left((i as u32 % 16) + 1);
    }
    if sig == PROBE_LAST.load(Ordering::Relaxed) {
        return;
    }
    PROBE_LAST.store(sig, Ordering::Relaxed);
    info!(
        "[qs] t={} cur={} max={} p2844={:08X} lvl={:08X} qaddr={:08X}",
        t, cur, total, p24, lvl, q
    );
    for row in 0..4 {
        let a = q0[row * 4];
        let b = q0[row * 4 + 1];
        let c = q0[row * 4 + 2];
        let d = q0[row * 4 + 3];
        info!(
            "[qs] q+0x{:02X}: {:08X} {:08X} {:08X} {:08X}",
            row * 16, a, b, c, d
        );
    }
}

// ---------------------------------------------------------------- 任务结束门控
// sQuest+0x1C = 任务倒计时（每帧 -1），sQuest+0x24 = 已用时间（float，每帧递增）。
// 两个独立计数器同时停住才算任务结束 —— 阈值 5 帧（约 0.08 秒 @60fps），
// 又不会因为单帧掉帧而误判。
const OFF_QUEST_TIMER: u32 = 0x1C;
const OFF_QUEST_ELAPSED: u32 = 0x24;
const STUCK_FRAMES: u32 = 5;

static QUEST_ADDR: AtomicU32 = AtomicU32::new(0);
static LAST_TIMER: AtomicU32 = AtomicU32::new(0);
static LAST_ELAPSED: AtomicU32 = AtomicU32::new(0);
static TIMER_STUCK: AtomicU32 = AtomicU32::new(0);

/// 返回 true 表示任务还在进行
unsafe fn quest_running() -> bool {
    let mut q = QUEST_ADDR.load(Ordering::Relaxed);
    if q == 0 {
        q = match forge::singleton::SingletonManager::get_by_name("sQuest") {
            Some(o) => o as *const forge::mt::object::MtObject as u32,
            None => return true, // 查不到就别误杀
        };
        if q == 0 {
            return true;
        }
        QUEST_ADDR.store(q, Ordering::Relaxed);
    }
    let t = unsafe { rd32(q.wrapping_add(OFF_QUEST_TIMER)) };
    let e = unsafe { rd32(q.wrapping_add(OFF_QUEST_ELAPSED)) };
    if t == LAST_TIMER.load(Ordering::Relaxed) && e == LAST_ELAPSED.load(Ordering::Relaxed) {
        let n = TIMER_STUCK.load(Ordering::Relaxed).saturating_add(1);
        TIMER_STUCK.store(n, Ordering::Relaxed);
        n < STUCK_FRAMES
    } else {
        LAST_TIMER.store(t, Ordering::Relaxed);
        LAST_ELAPSED.store(e, Ordering::Relaxed);
        TIMER_STUCK.store(0, Ordering::Relaxed);
        true
    }
}

unsafe fn update() {
    let base = forge::mem::text_addr();
    if base == 0 {
        hide_reason("base=0");
        return;
    }

    // sPlayer 单例 → 本地玩家对象
    let singleton = unsafe { rd32(base.wrapping_add(OFF_S_PLAYER)) };
    if singleton == 0 {
        hide_reason("sPlayer=0");
        return;
    }
    let get_player: unsafe extern "C" fn(u32) -> u32 =
        unsafe { core::mem::transmute(base.wrapping_add(FN_LOCAL_PLAYER)) };
    let player = unsafe { get_player(singleton) };
    if player < 0x10000 {
        hide_reason("player=0");
        return;
    }

    // 武器判断：远程 / 猎猫不显示
    let vt = unsafe { rd32(player) };
    let weapon = match weapon_from_vtable(base, vt) {
        Some(w) => w,
        None => {
            hide_reason("vtable 不在任务玩家表");
            return;
        }
    };
    if HIDDEN.contains(&weapon) {
        hide_reason("远程/猎猫");
        return;
    }

    // 斩味值
    let cur = unsafe { rd32(player.wrapping_add(OFF_KIRE_CUR)) };
    let total = unsafe { rd32(player.wrapping_add(OFF_KIRE_MAX)) };
    if total == 0 || total > 1024 || cur > total {
        hide_reason("斩味值异常");
        return;
    }

    // 游戏自己缓存的“当前斩味色阶”，原插件也拿它做有效性判断（>7 说明数据还没就绪）
    if unsafe { rd32(player.wrapping_add(OFF_KIRE_LEVEL)) } > 7 {
        hide_reason("色阶未就绪");
        return;
    }

    #[cfg(sharp_dbg)]
    unsafe {
        probe_state(player, cur, total);
    }

    // 任务结束（两个计数器同时停住 5 帧）→ 立刻停画
    if !unsafe { quest_running() } {
        hide_reason("任务已结束");
        return;
    }

    // 颜色表没变就只刷新当前值
    let cached = CACHE_PLAYER.load(Ordering::Relaxed) == player
        && CACHE_WEAPON.load(Ordering::Relaxed) == weapon as u32
        && CACHE_TOTAL.load(Ordering::Relaxed) == total;
    if cached {
        CUR.store(cur, Ordering::Relaxed);
        VALID.store(1, Ordering::Release);
        return;
    }

    // 重新算每一点斩味的颜色，得到每一色的结束位置
    let color_of: unsafe extern "C" fn(u32, u32) -> i32 =
        unsafe { core::mem::transmute(base.wrapping_add(FN_KIRE_COLOR)) };
    let mut ends = [0u32; 7];
    for i in 1..=total {
        let c = unsafe { color_of(player, i) };
        let idx = if c < 0 {
            0
        } else if c > 6 {
            6
        } else {
            c as usize
        };
        ends[idx] = i;
    }

    // 单调化：某个颜色不存在时用前一段的结束位置顶上
    let mut run = 0u32;
    for e in ends.iter_mut() {
        if *e < run {
            *e = run;
        } else {
            run = *e;
        }
    }
    if ends[6] < total {
        ends[6] = total;
    }

    // 发布快照
    SEQ.fetch_add(1, Ordering::AcqRel);
    TOTAL.store(total, Ordering::Relaxed);
    CUR.store(cur, Ordering::Relaxed);
    for (i, e) in ends.iter().enumerate() {
        ENDS[i].store(*e, Ordering::Relaxed);
    }
    CACHE_PLAYER.store(player, Ordering::Relaxed);
    CACHE_WEAPON.store(weapon as u32, Ordering::Relaxed);
    CACHE_TOTAL.store(total, Ordering::Relaxed);
    VALID.store(1, Ordering::Release);
    SEQ.fetch_add(1, Ordering::AcqRel);


    // 换武器/首次读到数据时打一条日志，方便实机排查偏移是否正确
    if LOGGED_WEAPON.swap(weapon as u32 + 1, Ordering::Relaxed) != weapon as u32 + 1 {
        info!(
            "[sharp] W{} cur={} max={} bands=[{},{},{},{},{},{},{}]",
            weapon, cur, total,
            ends[0], ends[1], ends[2], ends[3], ends[4], ends[5], ends[6]
        );
    }
}

static LOGGED_WEAPON: AtomicU32 = AtomicU32::new(0);

#[forge::entry]
fn main() {
    info!("[sharp] 已加载：{} 版（实机 + 模拟器）", LAYOUT_NAME);

    // 调整版：把上次保存的模式和坐标读回来，避免重启后回到默认位置
    #[cfg(sharpness_adj)]
    adj_init();
}

/// 游戏线程每帧调用
#[unsafe(no_mangle)]
pub extern "C" fn forge_onUpdate() {
    UPDATE_COUNT.fetch_add(1, Ordering::Relaxed);
    unsafe { update() }
}

static UPDATE_COUNT: AtomicU32 = AtomicU32::new(0);



/// 只打一次版面日志，避免刷屏
static LOG_ONCE: AtomicU32 = AtomicU32::new(0);


/// 画一把剑的剪影：柄头三角 + 握把 + 护手 + 刀身 + 斜切刀尖
/// rect = (x0, y0, x1, y1)，grow = 向外扩张量（做成描边/遮罩）
fn sword_silhouette(dl: &DrawList, rect: (f32, f32, f32, f32), col: u32, grow: f32) {
    let (x0, y0, x1, y1) = rect;
    let h = y1 - y0;
    let mid = (y0 + y1) * 0.5;
    let ft = if h * 0.08 > 2.0 { h * 0.08 } else { 2.0 };
    let tip = h * 0.95;
    let guard_w = if h * 0.20 > 3.0 { h * 0.20 } else { 3.0 };
    let guard_pad = h * 0.10;
    let grip_w = h * 0.50;
    let grip_h = h * 0.28;
    let pommel_w = h * 0.42;
    let pommel_h = h * 0.46;

    let ox0 = x0 - ft;
    let oy0 = y0 - ft;
    let oy1 = y1 + ft;
    let gx1 = ox0;
    let gx0 = ox0 - guard_w;
    let px1 = gx0;
    let px0 = px1 - grip_w;
    let pommel_x = px0;
    let pommel_tip_x = pommel_x - pommel_w;

    let g = grow;
    #[cfg(sharpness_adj)]
    {
        // 刀身 + 斜切刀尖（只改可调整版）。
        // 不用 add_convex_poly_filled：gu-native 安卓运行时里数组版多边形填充不可靠，
        // 会把整块刀身画丢；改成矩形 + 三角形，和原来的四边形形状一致。
        dl.add_rect_filled(
            ImVec2::new(ox0 - g, oy0 - g),
            ImVec2::new(x1 - tip * 0.35 + g, oy1 + g),
            col,
            0.0,
        );
        dl.add_triangle_filled(
            ImVec2::new(x1 - tip * 0.35 + g, oy0 - g),
            ImVec2::new(x1 + tip + g, oy0 - g),
            ImVec2::new(x1 - tip * 0.35 + g, oy1 + g),
            col,
        );
    }
    #[cfg(not(sharpness_adj))]
    {
        let blade = [
            ImVec2::new(ox0 - g, oy0 - g),
            ImVec2::new(x1 + tip + g, oy0 - g),
            ImVec2::new(x1 - tip * 0.35 + g, oy1 + g),
            ImVec2::new(ox0 - g, oy1 + g),
        ];
        dl.add_convex_poly_filled(&blade, col);
    }
    // 护手
    dl.add_rect_filled(
        ImVec2::new(gx0 - g, oy0 - guard_pad - g),
        ImVec2::new(gx1 + g, oy1 + guard_pad + g),
        col,
        1.0,
    );
    // 握把
    dl.add_rect_filled(
        ImVec2::new(px0 - g, mid - grip_h * 0.5 - g),
        ImVec2::new(px1 + g, mid + grip_h * 0.5 + g),
        col,
        0.0,
    );
    // 柄头（朝左三角）
    #[cfg(sharpness_adj)]
    dl.add_triangle_filled(
        ImVec2::new(pommel_tip_x - g, mid),
        ImVec2::new(pommel_x + g, mid - pommel_h * 0.5 - g),
        ImVec2::new(pommel_x + g, mid + pommel_h * 0.5 + g),
        col,
    );
    #[cfg(not(sharpness_adj))]
    {
        let pommel = [
            ImVec2::new(pommel_tip_x - g, mid),
            ImVec2::new(pommel_x + g, mid - pommel_h * 0.5 - g),
            ImVec2::new(pommel_x + g, mid + pommel_h * 0.5 + g),
        ];
        dl.add_convex_poly_filled(&pommel, col);
    }
}


// ================================================================
// 调整版（SHARPNESS_ADJ=1 构建时编入）
// ================================================================
// 主机 / 掌机各存一份 ini，进游戏选对应模式调，调完自动保存。
// 界面用大号按钮而不是滑块：手机上拖滑块很难点，按钮一下一下按更稳。
#[cfg(sharpness_adj)]
#[derive(forge::IniDeserialize, forge::IniSerialize)]
struct LayoutCfg {
    x: i32,
    y: i32,
    w: i32,
    h: i32,
}

#[cfg(sharpness_adj)]
impl Default for LayoutCfg {
    fn default() -> Self {
        Self { x: PROFILE.0, y: PROFILE.1, w: PROFILE.2, h: PROFILE.3 }
    }
}

/// 0 = 主机, 1 = 掌机
#[cfg(sharpness_adj)]
static MODE: AtomicU32 = AtomicU32::new(0);

#[cfg(sharpness_adj)]
fn cfg_file(mode: u32) -> &'static str {
    if mode == 1 { "sharpness_hud_handheld.ini" } else { "sharpness_hud_docked.ini" }
}

#[cfg(sharpness_adj)]
fn cfg_path(name: &str) -> alloc::string::String {
    format!("{}:/{}", forge::fs::savedata::mount_point(), name)
}

/// 主机 / 掌机各自的首次默认值（调整版固定按这两套，和构建时的 SHARPNESS_LAYOUT 无关）
#[cfg(sharpness_adj)]
fn default_cfg(mode: u32) -> LayoutCfg {
    if mode == 1 {
        LayoutCfg { x: 128, y: 41, w: 109, h: 17 }
    } else {
        LayoutCfg { x: 192, y: 62, w: 164, h: 27 }
    }
}

#[cfg(sharpness_adj)]
fn cfg_load(mode: u32) -> LayoutCfg {
    let _m = forge::fs::savedata::ScopedMount::new();
    let path = cfg_path(cfg_file(mode));
    if let Ok(ini) = forge::config::ini::Reader::read(&path) {
        let c = LayoutCfg::from_ini(&ini);
        info!("[sharp] 读配置 {} x={} y={} w={} h={}", cfg_file(mode), c.x, c.y, c.w, c.h);
        return c;
    }
    // 没有该模式的配置：写一份该模式自己的默认值（不是另一模式的坐标）
    let c = default_cfg(mode);
    if let Err(e) = forge::config::ini::Writer::write(&path, &c.to_ini()) {
        info!("[sharp] 写默认配置失败: {}", e);
    }
    c
}

#[cfg(sharpness_adj)]
fn cfg_save(mode: u32, c: &LayoutCfg) {
    let _m = forge::fs::savedata::ScopedMount::new();
    let path = format!("{}:/{}", forge::fs::savedata::mount_point(), cfg_file(mode));
    match forge::config::ini::Writer::write(&path, &c.to_ini()) {
        Ok(_) => info!("[sharp] 已保存 {} x={} y={} w={} h={}", cfg_file(mode), c.x, c.y, c.w, c.h),
        Err(e) => info!("[sharp] 保存失败: {}", e),
    }
}

#[cfg(sharpness_adj)]
fn set_layout(x: i32, y: i32, w: i32, h: i32) {
    use core::sync::atomic::Ordering;
    OV_X.store(x, Ordering::Relaxed);
    OV_Y.store(y, Ordering::Relaxed);
    OV_W.store(w, Ordering::Relaxed);
    OV_H.store(h, Ordering::Relaxed);
}

#[cfg(sharpness_adj)]
fn apply_cfg(c: &LayoutCfg) {
    set_layout(c.x, c.y, c.w, c.h);
}

/// 记住上次用的是主机还是掌机模式
#[cfg(sharpness_adj)]
#[derive(Default, forge::IniDeserialize, forge::IniSerialize)]
struct AdjState {
    mode: u32,
}

#[cfg(sharpness_adj)]
const STATE_FILE: &str = "sharpness_hud_adj.ini";

#[cfg(sharpness_adj)]
fn load_mode() -> Option<u32> {
    let _m = forge::fs::savedata::ScopedMount::new();
    let path = cfg_path(STATE_FILE);
    match forge::config::ini::Reader::read(&path) {
        Ok(ini) => {
            let st = AdjState::from_ini(&ini);
            let m = if st.mode != 0 { 1 } else { 0 };
            info!("[sharp] 上次的模式 = {}（0=主机 1=掌机）", m);
            Some(m)
        }
        Err(_) => None,
    }
}

#[cfg(sharpness_adj)]
fn save_mode(mode: u32) {
    let _m = forge::fs::savedata::ScopedMount::new();
    let path = format!("{}:/{}", forge::fs::savedata::mount_point(), STATE_FILE);
    if let Err(e) = forge::config::ini::Writer::write(&path, &AdjState { mode }.to_ini()) {
        info!("[sharp] 模式保存失败: {}", e);
    }
}

/// 切模式：写回原子量 + 加载该模式的坐标 + 记住模式
#[cfg(sharpness_adj)]
fn switch_mode(mode: u32) {
    let m = if mode != 0 { 1 } else { 0 };
    MODE.store(m, Ordering::Relaxed);
    let c = cfg_load(m);
    apply_cfg(&c);
    save_mode(m);
    info!("[sharp] 切到 {} 模式 ({},{},{},{})", if m == 0 { "主机" } else { "掌机" }, c.x, c.y, c.w, c.h);
}

/// 启动时恢复上次的模式和坐标（这就是"重启后不再回默认"的关键）
#[cfg(sharpness_adj)]
static AUTO_MODE: AtomicU32 = AtomicU32::new(0);

#[cfg(sharpness_adj)]
fn adj_init() {
    match load_mode() {
        Some(m) => {
            MODE.store(m, Ordering::Relaxed);
            let c = cfg_load(m);
            apply_cfg(&c);
            info!(
                "[sharp] 调整版启动：{} 模式，位置 ({},{},{},{})",
                if m == 0 { "主机" } else { "掌机" },
                c.x, c.y, c.w, c.h
            );
        }
        None => {
            // 首次启动：先不写磁盘，等第一帧拿到实际画面高度再选。
            AUTO_MODE.store(1, Ordering::Relaxed);
            info!("[sharp] 调整版首次启动：按画面分辨率自动选主机/掌机");
        }
    }
}

#[cfg(sharpness_adj)]
#[forge::imgui_render]
fn settings_ui() {
    let mode = MODE.load(Ordering::Relaxed);
    let (mut x, mut y, mut w, mut h) = layout();

    // 模式切换（按了才切换，并把该模式的配置读进来）
    if button(if mode == 0 { c"> DOCKED <" } else { c"DOCKED" }, ImVec2::new(150.0, 44.0)) {
        switch_mode(0);
        return;
    }
    if button(if mode == 1 { c"> HANDHELD <" } else { c"HANDHELD" }, ImVec2::new(150.0, 44.0)) {
        switch_mode(1);
        return;
    }

    let big = ImVec2::new(64.0, 44.0);
    let mut changed = false;

    text("X");
    if button(c"X -10", big) { x -= 10; changed = true; }
    if button(c"X  -1", big) { x -= 1; changed = true; }
    if button(c"X  +1", big) { x += 1; changed = true; }
    if button(c"X +10", big) { x += 10; changed = true; }

    text("Y");
    if button(c"Y -10", big) { y -= 10; changed = true; }
    if button(c"Y  -1", big) { y -= 1; changed = true; }
    if button(c"Y  +1", big) { y += 1; changed = true; }
    if button(c"Y +10", big) { y += 10; changed = true; }

    text("Width");
    if button(c"W -10", big) { w -= 10; changed = true; }
    if button(c"W  -1", big) { w -= 1; changed = true; }
    if button(c"W  +1", big) { w += 1; changed = true; }
    if button(c"W +10", big) { w += 10; changed = true; }

    text("Height");
    if button(c"H -5", big) { h -= 5; changed = true; }
    if button(c"H -1", big) { h -= 1; changed = true; }
    if button(c"H +1", big) { h += 1; changed = true; }
    if button(c"H +5", big) { h += 5; changed = true; }

    // 夹到合理范围
    if x < 0 { x = 0; }
    if y < 0 { y = 0; }
    if w < 40 { w = 40; }
    if h < 6 { h = 6; }

    if changed {
        set_layout(x, y, w, h);
        let c = LayoutCfg { x, y, w, h };
        cfg_save(mode, &c);
        info!("[sharp] layout x={} y={} w={} h={} （{}）", x, y, w, h, cfg_file(mode));
    }

    separator_text(c"current");
    text(&format!("x={} y={} w={} h={}", x, y, w, h));
}


// ================================================================
// 自带菜单（调整版）：完全手柄控制，不依赖 forge 的菜单键
// ================================================================
// 开关：同时按住 L + 十字键下 + Y
// 面板：十字键 上/下 选行，左/右 调值，X 切步长(1/10)，A 切 主机/掌机，B 关闭
#[cfg(sharpness_adj)]
mod panel {
    use super::*;

    pub static OPEN: AtomicU32 = AtomicU32::new(0);
    static ROW: AtomicU32 = AtomicU32::new(2); // 0=MODE 1=STEP 2=X 3=Y 4=W 5=H
    static STEP: AtomicU32 = AtomicU32::new(1);
    static COMBO_LATCH: AtomicU32 = AtomicU32::new(0);

    const ROWS: u32 = 6;

    fn txt(dl: &DrawList, x: f32, y: f32, size: f32, col: u32, s: &str) {
        let font = forge::imgui::ImFont::current();
        dl.add_text_with_font(&font, size, ImVec2::new(x, y), col, s);
    }

    pub fn toggle() {
        let v = OPEN.load(Ordering::Relaxed);
        OPEN.store(v ^ 1, Ordering::Relaxed);
    }

    /// 每帧调用：处理组合键开关
    pub fn poll() {
        let combo = Input::is_button_down(Button::L)
            && Input::is_button_down(Button::Down)
            && Input::is_button_down(Button::Y);
        if combo {
            if COMBO_LATCH.swap(1, Ordering::Relaxed) == 0 {
                toggle();
            }
        } else {
            COMBO_LATCH.store(0, Ordering::Relaxed);
        }
    }

    pub fn is_open() -> bool {
        OPEN.load(Ordering::Relaxed) != 0
    }

    /// 画面板并处理手柄输入
    pub fn draw(dl: &DrawList, dw: f32, dh: f32) {
        let s = (dh / 1080.0).clamp(0.6, 1.2);
        let w = 620.0 * s;
        let row_h = 86.0 * s;
        let pad = 18.0 * s;
        let x0 = (dw - w) * 0.5;
        let y0 = dh - (row_h * (ROWS as f32) + pad * 3.0) - 40.0 * s;
        let text_size = 26.0 * s;

        // 背板 + 标题
        dl.add_rect_filled(
            ImVec2::new(x0 - pad, y0 - pad - 34.0 * s),
            ImVec2::new(x0 + w + pad, y0 + row_h * (ROWS as f32) + pad),
            color(0x10, 0x12, 0x16, 0xE6),
            8.0,
        );
        txt(dl, x0, y0 - 30.0 * s, 24.0 * s, color(0xFF, 0xD8, 0x60, 0xFF), "SHARPNESS HUD");
        txt(
            dl,
            x0 + 250.0 * s,
            y0 - 30.0 * s,
            18.0 * s,
            color(0x90, 0x98, 0xA8, 0xFF),
            "L+Down+Y / B",
        );

        let mut sel = ROW.load(Ordering::Relaxed);
        let mut step = STEP.load(Ordering::Relaxed);
        let mut mode = MODE.load(Ordering::Relaxed);
        let (mut x, mut y, mut ww, mut hh) = layout();

        // ---- 手柄导航
        if Input::is_button_pressed(Button::Up) {
            sel = (sel + ROWS - 1) % ROWS;
            ROW.store(sel, Ordering::Relaxed);
        }
        if Input::is_button_pressed(Button::Down) {
            sel = (sel + 1) % ROWS;
            ROW.store(sel, Ordering::Relaxed);
        }

        let mut dir = 0i32;
        if Input::is_button_pressed(Button::Right) { dir = 1; }
        if Input::is_button_pressed(Button::Left) { dir = -1; }

        if Input::is_button_pressed(Button::X) {
            step = if step == 1 { 10 } else { 1 };
            STEP.store(step, Ordering::Relaxed);
        }
        if Input::is_button_pressed(Button::A) {
            mode ^= 1;
            super::switch_mode(mode);
        }
        if Input::is_button_pressed(Button::B) {
            OPEN.store(0, Ordering::Relaxed);
            return;
        }

        // ---- 行内容
        let labels = ["MODE", "STEP", "X", "Y", "WIDTH", "HEIGHT"];
        let mut changed = false;
        for row in 0..ROWS {
            let ry = y0 + (row as f32) * row_h;
            let hl = row == sel;

            if hl {
                dl.add_rect_filled(
                    ImVec2::new(x0 - 8.0 * s, ry),
                    ImVec2::new(x0 + w + 8.0 * s, ry + row_h),
                    color(0x2A, 0x3A, 0x52, 0x80),
                    4.0,
                );
            }

            txt(
                dl,
                x0,
                ry + (row_h - text_size) * 0.5,
                text_size,
                if hl { color(0xFF, 0xD8, 0x60, 0xFF) } else { color(0xC8, 0xC8, 0xC8, 0xFF) },
                labels[row as usize],
            );

            // 左/右提示
            txt(dl, x0 + 220.0 * s, ry + (row_h - text_size) * 0.5, text_size, color(0x70, 0x80, 0x98, 0xFF), "<");
            txt(dl, x0 + 520.0 * s, ry + (row_h - text_size) * 0.5, text_size, color(0x70, 0x80, 0x98, 0xFF), ">");

            let value = match row {
                0 => format!("{}", if mode == 0 { "DOCKED" } else { "HANDHELD" }),
                1 => format!("x{}", step),
                2 => format!("{}", x),
                3 => format!("{}", y),
                4 => format!("{}", ww),
                _ => format!("{}", hh),
            };
            txt(
                dl,
                x0 + 265.0 * s,
                ry + (row_h - text_size) * 0.5,
                text_size,
                color(0x9E, 0xE0, 0xFF, 0xFF),
                &value,
            );

            // 只有选中的行响应左右键
            if hl && dir != 0 {
                let k = step as i32;
                match row {
                    0 => {
                        mode ^= 1;
                        super::switch_mode(mode);
                    }
                    1 => {
                        step = if step == 1 { 10 } else { 1 };
                        STEP.store(step, Ordering::Relaxed);
                    }
                    2 => { x += dir * k; changed = true; }
                    3 => { y += dir * k; changed = true; }
                    4 => { ww += dir * k; changed = true; }
                    _ => { hh += dir * k; changed = true; }
                }
            }
        }

        if changed {
            if x < 0 { x = 0; }
            if y < 0 { y = 0; }
            if ww < 40 { ww = 40; }
            if hh < 6 { hh = 6; }
            super::set_layout(x, y, ww, hh);
            super::cfg_save(
                MODE.load(Ordering::Relaxed),
                &super::LayoutCfg { x, y, w: ww, h: hh },
            );
        }
    }
}

#[forge::imgui_free_render]
fn render_hud() {
    // 兜底：如果加载器没有调用 forge_onUpdate（或还没调过），就在渲染回调里自己算一次
    if UPDATE_COUNT.load(Ordering::Relaxed) == 0 {
        unsafe { update() }
    }
    if VALID.load(Ordering::Acquire) == 0 {
        return;
    }

    let seq = SEQ.load(Ordering::Acquire);
    if seq & 1 == 1 {
        return; // 正在写，跳过这一帧
    }
    let total = TOTAL.load(Ordering::Relaxed);
    let cur = CUR.load(Ordering::Relaxed);
    let mut ends = [0u32; 7];
    for (i, e) in ENDS.iter().enumerate() {
        ends[i] = e.load(Ordering::Relaxed);
    }
    if SEQ.load(Ordering::Acquire) != seq || total == 0 {
        return;
    }

    let io = context::get_io();
    let dw = io.DisplaySize.x;
    let dh = io.DisplaySize.y;
    if dw <= 1.0 || dh <= 1.0 {
        return;
    }

    #[cfg(sharpness_adj)]
    {
        if AUTO_MODE.swap(0, Ordering::Relaxed) != 0 {
            // 720p 类画面用掌机基准，1080p 类画面用主机基准；之后可以随时在面板里切。
            let m = if dh < 900.0 { 1 } else { 0 };
            switch_mode(m);
        }
    }

    // 坐标是原始像素，不缩放（旧插件也是写死的）。可以在 forge 菜单里临时调。
    let (lx, ly, lw, lh) = layout();
    let x0 = lx as f32;
    let y0 = ly as f32;
    let x1 = (lx + lw) as f32;
    let y1 = (ly + lh) as f32;

    // 把分辨率/矩形打进日志，方便核对位置（打一次）
    if LOG_ONCE.swap(1, Ordering::Relaxed) == 0 {
        info!(
            "[sharp] {} 版 disp={}x{} bar=({},{})-({},{})",
            LAYOUT_NAME, dw as u32, dh as u32, lx, ly, lx + lw, ly + lh
        );
    }

    let dl = DrawList::foreground();
    // 其他斩味条 MOD（例如 world-sharpness-gauge）可能往同一个前景绘制列表里塞
    // scissor/clip，且不恢复；先重置成全屏，画完再恢复，避免我们的剑形被裁掉。
    #[cfg(sharpness_adj)]
    dl.push_clip_rect_full_screen();

    #[cfg(sharpness_adj)]
    {
        panel::poll();
        if panel::is_open() {
            panel::draw(&dl, dw, dh);
        }
    }


    // ---- 崛起样式：整把剑 = 当前斩味色，中间挖一个小窗口放彩色条 ----
    let h = y1 - y0;
    let ft = if h * 0.08 > 2.0 { h * 0.08 } else { 2.0 };
    let tip = h * 0.95; // 刀尖横向长度（几何在 sword_silhouette 里）

    // 当前斩味档位
    let mut tier = 0usize;
    for (i, &e) in ends.iter().enumerate() {
        if e == 0 {
            continue;
        }
        tier = i;
        if cur <= e {
            break;
        }
    }
    let (cr, cg, cb) = PALETTE[tier];
    let body = color(cr, cg, cb, 0xFF);
    let dark = color(0x18, 0x18, 0x18, 0xFF);

    // 我们的条：黑描边 + 当前斩味色剑身
    let rect = (x0, y0, x1, y1);
    sword_silhouette(&dl, rect, dark, 1.2);
    sword_silhouette(&dl, rect, body, 0.0);

    // 中间的小窗口（比剑身窄一圈、矮一圈）
    let wx0 = x0 + ft * 1.5;
    let wx1 = x1 - tip * 0.50;
    let ins = h * 0.22;
    let wy0 = y0 + ins;
    let wy1 = y1 - ins;

    // 窗口黑边 + 黑底
    dl.add_rect_filled(
        ImVec2::new(wx0 - 1.5, wy0 - 1.5),
        ImVec2::new(wx1 + 1.5, wy1 + 1.5),
        dark,
        0.0,
    );
    dl.add_rect_filled(
        ImVec2::new(wx0, wy0),
        ImVec2::new(wx1, wy1),
        color(0, 0, 0, 0xFF),
        0.0,
    );

    // 窗口里的彩色斩味条（再内缩一圈，让外圈和内圈之间的黑边更粗）
    let bw = if h * 0.07 > 2.0 { h * 0.07 } else { 2.0 };
    let sx0 = wx0 + bw;
    let sx1 = wx1 - bw;
    let sy0 = wy0 + bw;
    let sy1 = wy1 - bw;
    let inv = 1.0 / total as f32;
    let cx = sx0 + (sx1 - sx0) * (cur as f32 * inv);
    let mut prev = 0u32;
    for (i, &end) in ends.iter().enumerate() {
        if end <= prev {
            continue;
        }
        let bx1 = sx0 + (sx1 - sx0) * (prev as f32 * inv);
        let bx2 = sx0 + (sx1 - sx0) * (end as f32 * inv);
        let (r, g, b) = PALETTE[i];
        let split = if cx <= bx1 {
            bx1
        } else if cx >= bx2 {
            bx2
        } else {
            cx
        };
        if split > bx1 {
            dl.add_rect_filled(
                ImVec2::new(bx1, sy0),
                ImVec2::new(split, sy1),
                color(r, g, b, 0xFF),
                0.0,
            );
        }
        if bx2 > split {
            let (dr, dg, db) = shade((r, g, b), 26);
            dl.add_rect_filled(
                ImVec2::new(split, sy0),
                ImVec2::new(bx2, sy1),
                color(dr, dg, db, 0xFF),
                0.0,
            );
        }
        prev = end;
    }

    #[cfg(sharpness_adj)]
    dl.pop_clip_rect();
}
