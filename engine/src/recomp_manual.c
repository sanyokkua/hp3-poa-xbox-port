/**
 * Manual function overrides and ICALL diagnostics -- Harry Potter and the
 * Prisoner of Azkaban (Xbox, NTSC-US).
 *
 * Started from the Chamber of Secrets port's file with every hook that belongs to
 * that executable removed (their addresses mean nothing in this one). The list of
 * what they did is docs/PORTING.md, "CoS hooks to re-find for HP3": each is re-found
 * by signature and added here in phase 2.
 *
 * A hook is a strong definition of a lifted function's name (void sub_XXXXXXXX(void))
 * in this file: tools.recomp --exclude-manual reads it and skips the generated body,
 * and the hook calls the original through `extern void sub_XXXXXXXX_gen(void)`.
 * Optional hooks the runtime calls have weak no-op defaults in the runtime itself
 * (Mach-O cannot link an undefined weak reference): xbox_after_vblank_dpc in
 * engine/xboxrecomp/src/kernel/kernel_bridge.c, nv2a_mtl_movie_tick in
 * xbox_memory_layout.c. Define the name here to use one.
 *
 * This file provides:
 *   - recomp_lookup_manual()  : intercept specific Xbox VAs with hand-written code
 *   - recomp_icall_fail_log() : log when an indirect call target can't be resolved
 *   - ICALL trace ring buffer  : globals used by the RECOMP_ICALL macro
 *
 * The recomp pipeline generates an auto-dispatch table (recomp_lookup) that
 * resolves most function addresses. recomp_lookup_manual() is called FIRST,
 * giving you a chance to override any function with a custom implementation.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

/* The generated register model (g_eax/g_esp are thread-local there) and the
 * XBOX_PTR/MEM32 accessors; the register names below are its macros. */
#define RECOMP_GENERATED_CODE
#include "recomp_funcs.h"

/* ── ICALL trace ring buffer ───────────────────────────────── */

/*
 * These globals are written by the RECOMP_ICALL macro (defined in
 * recomp_types.h) every time an indirect call is dispatched. When a
 * crash occurs, the VEH handler or recomp_icall_fail_log() can dump
 * the last 16 call targets to help you trace what happened.
 *
 * The runtime owns them: xbox_kernel defines all three in
 * src/kernel/xbox_memory_layout.c, and recomp_types.h declares them extern.
 * Declare, do not define -- a definition here as well is a duplicate symbol,
 * and a project copied from this template failed to link on all three:
 *
 *   xbox_memory_layout.obj : error LNK2005: g_icall_count already defined
 *                            in recomp_manual.obj
 */
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;
extern volatile uint64_t g_icall_count;

typedef void (*recomp_func_t)(void);

/* ── Register state (defined in xbox_memory_layout.c) ──────── */

extern ptrdiff_t g_xbox_mem_offset;

/* ── XDK 5849 glue ────────────────────────────────────────────
 *
 * The same XDK as NFS Underground 2: every address below was found with
 * tools/sigmatch.py from NFSU2's hook and checked instruction by instruction
 * (tools/xdis.py, branch targets aside, identical): docs/PORTING.md, step 2. */

/* ── CRT memmove/memcpy (0x0013E2F0, 0x0013FF20) ──────────────
 *
 * Two copies of the MSVC CRT memmove (memcpy is the same body). Their tail and
 * backward-copy paths dispatch through vectors indexed with a negative
 * register, which the lifter cannot follow: copies lost their tail. Replaced by
 * the host memmove. cdecl (dst, src, count), returns dst. */
static void crt_memmove(void)
{
    uint32_t dst = MEM32(esp + 4);
    uint32_t src = MEM32(esp + 8);
    uint32_t n   = MEM32(esp + 12);

    if (n)
        memmove((void *)XBOX_PTR(dst), (const void *)XBOX_PTR(src), n);
    eax = dst;
    esp += 4;   /* return address; cdecl, the caller pops the arguments */
}

void sub_0013E2F0(void) { crt_memmove(); }
void sub_0013FF20(void) { crt_memmove(); }

/* ── D3D fence wait, D3D_BlockOnTime (0x0026C540) ────────────
 *
 * stdcall (fence, flags), ret 8. Device through the global at 0x00275FE8:
 *   +0x2C  write sequence, +0x30  pointer to the GPU's semaphore.
 * With the pushbuffer executor (RECOMP_PB_EXEC) the real semaphore is handed to
 * it once and the original wait runs; without it the GPU is reported caught up. */
#define HPPOA_D3D_DEVICE_PTR 0x00275FE8u
extern void nv2a_pb_set_semaphore_target(uint32_t guest_va);
extern void sub_0026C540_gen(void);

void sub_0026C540(void)
{
    static int exec = -1;
    uint32_t dev = MEM32(HPPOA_D3D_DEVICE_PTR);
    uint32_t sem = dev ? MEM32(dev + 0x30) : 0;

    if (exec < 0)
        exec = getenv("RECOMP_PB_EXEC") != NULL;
    if (exec) {
        static uint32_t registered;
        if (sem && sem != registered) {
            nv2a_pb_set_semaphore_target(sem);
            fprintf(stderr, "[D3D] GPU semaphore at 0x%08X\n", sem);
            registered = sem;
        }
        sub_0026C540_gen();
        return;
    }
    if (sem)
        MEM32(sem) = MEM32(dev + 0x2C);     /* "GPU" is always caught up */
    eax = 0;
    esp += 4 + 8;                           /* return address, 2 stdcall args */
}

/* ── D3D interrupt handlers at DISPATCH_LEVEL ────────────────
 *
 * D3D services the GPU from its DPC (0x0026E6B0) and, with the interrupt
 * masked, from its own busy-waits on the game thread: both call the vblank
 * handler (0x0026DF00) and the PGRAPH handler (0x0026E470). On the Xbox the two
 * never interleave; here the DPC runs on the kernel timer thread, and a flip
 * whose target vblank moved past while it was computed stayed pending forever
 * (NFSU2's hang). Run both at DISPATCH_LEVEL, letting the guest lock go for the
 * raise as a kernel call would. */
extern uint8_t xbox_KfRaiseIrql(uint8_t irql);
extern void xbox_KfLowerIrql(uint8_t irql);
extern uint8_t xbox_CurrentIrql(void);
extern int xbox_gil_suspend(void);
extern void xbox_gil_resume(int depth);
extern void xbox_Nv2aVblankTaken(void);
extern void nv2a_pb_flip_retired(unsigned n);

static uint8_t d3d_isr_enter(void)
{
    uint8_t old = xbox_CurrentIrql();

    if (old < 2) {
        int d = xbox_gil_suspend();
        old = xbox_KfRaiseIrql(2);
        xbox_gil_resume(d);
    }
    return old;
}

static void d3d_isr_leave(uint8_t old)
{
    if (old < 2)
        xbox_KfLowerIrql(old);
}

/* Vblank handler, thiscall; flips retired = D3D's retire index at +0x1BC of the
 * block in ecx (read at DISPATCH only, so a retire is never counted twice). */
extern void sub_0026DF00_gen(void);

void sub_0026DF00(void)
{
    uint32_t blk = ecx;
    uint8_t old = d3d_isr_enter();
    uint32_t head = MEM32(blk + 0x1BC);

    xbox_Nv2aVblankTaken();
    sub_0026DF00_gen();
    nv2a_pb_flip_retired(MEM32(blk + 0x1BC) - head);
    d3d_isr_leave(old);
}

/* PGRAPH handler, thiscall, ecx = the device's hardware block. Takes a
 * software-method trap (NOP(5) sets BlockOnTime's event, a flip trap queues the
 * flip); the executor posts traps under a lock held here, and a call that found
 * one reports it taken (nv2a_pb_exec.c). The title's flip interval is kept. */
extern void sub_0026E470_gen(void);
extern void nv2a_pb_trap_lock(void);
extern void nv2a_pb_trap_unlock(void);
extern void nv2a_pb_trap_taken(void);

static int fps60(void);
static uint32_t s_ff_cs;    /* Cutscenes: the one being fast-forwarded (flips on every vblank meanwhile) */

void sub_0026E470(void)
{
    volatile uint32_t *nv = (volatile uint32_t *)XBOX_PTR(0xFD000000u);
    uint32_t blk = ecx;
    uint8_t old = d3d_isr_enter();
    uint32_t nsource, head;

    nv2a_pb_trap_lock();
    head = MEM32(blk + 0x1BC);
    nsource = nv[0x400108 / 4];
    /* 60 fps: Present's NOP trap word is (interval bits << 5) | 1, bits 0..2 = 1/2/3 for 1/2/4 vblanks,
     * bit 3 = flip at once when late (same D3D code and format as CoS, NFSU1, NFSU2). The title asks for 2
     * vblanks (D3DPRESENT_INTERVAL_TWO | IMMEDIATE, 0x80000002); HPPOA_FPS=60 rewrites it to 1 as the trap
     * is taken. */
    if (nsource && (nv[0x400704 / 4] & 0xFFFFu) == 0x0100u && (fps60() || s_ff_cs)) {
        uint32_t data = nv[0x400708 / 4];
        if ((data & 0x1Fu) == 1u && ((data >> 5) & 7u) > 1u) {
            static int told;
            nv[0x400708 / 4] = (data & ~(7u << 5)) | (1u << 5);
            if (!told++)
                fprintf(stderr, "[fps] flip interval %u -> 1 vblank (trap word %08X)\n", (data >> 5) & 7u, data);
        }
    }
    {   /* HPPOA_TRAP_LOG=1: each PGRAPH handler call */
        static int lg = -1;
        if (lg < 0) { const char *e = getenv("HPPOA_TRAP_LOG"); lg = e && *e == '1'; }
        if (lg) fprintf(stderr, "[trap] PGRAPH handler: NSOURCE %08X INTR %08X ADDR %08X DATA %08X\n",
                        nsource, nv[0x400100 / 4], nv[0x400704 / 4], nv[0x400708 / 4]);
    }
    sub_0026E470_gen();
    nv2a_pb_flip_retired(MEM32(blk + 0x1BC) - head);
    if (nsource)
        nv2a_pb_trap_taken();
    nv2a_pb_trap_unlock();
    d3d_isr_leave(old);
}

/* ── DirectSound DSP command post (0x002B8B0D) ───────────────
 *
 * thiscall. Stores a command code at scratch + 0x810 (scratch = ***(this+8+0x10))
 * and spins until the DSP zeroes it. There is no DSP: register the word with
 * the runtime's completion list, then run the original. */
extern int xbox_ApuDspAckWord(uint32_t va);
extern void sub_002B8B0D_gen(void);

void sub_002B8B0D(void)
{
    uint32_t obj = MEM32(ecx + 8);
    uint32_t blk = obj ? MEM32(obj + 0x10) : 0;
    uint32_t scratch = blk ? MEM32(blk) : 0;

    if (scratch)
        xbox_ApuDspAckWord(scratch + 0x810);
    sub_002B8B0D_gen();
}

/* ── DirectSound AC'97 channel reset (0x002BE412) ────────────
 *
 * thiscall, ecx = channel object. Sets RR (bit 1) in the channel's NABM control
 * byte (NABM base table 0x002BF060, byte at base + 0xFEC0010B), then waits for it
 * to clear -- but the load is hoisted out of the loop (0x2BE43F, loop 0x2BE448),
 * so it spins on a register copy forever. Same function with the reset done at
 * once; the lock (0x2B6710/0x2B6732), the descriptor base store, the 0xFEC0017C
 * write for channel 1 and the re-arm (0x2BE21C) are as the original. */
void sub_002BE412(void)
{
    uint32_t ebp_saved = g_ebp, frame, chan, nabm;

    PUSH32(esp, ebp_saved);
    frame = esp;
    g_ebp = frame;
    g_seh_ebp = frame;
    esp -= 8;
    MEM32(frame - 4) = 0;
    PUSH32(esp, esi);
    esi = ecx;
    chan = esi;

    ecx = frame - 8;
    PUSH32(esp, 0x002BE426u); RECOMP_ABI_CALL(0x002B6710u, sub_002B6710);

    nabm = MEM32(MEM32(chan) * 4 + 0x002BF060u);
    MEM8(nabm + 0xFEC0010Bu) = 2;
    MEM8(nabm + 0xFEC0010Bu) &= (uint8_t)~2u;   /* reset complete */
    eax = nabm;
    SET_LO8(ecx, 0);

    MEM32(nabm + 0xFEC00100u) = MEM32(chan + 0x1C);
    if (MEM32(chan) == 1)
        MEM32(0xFEC0017Cu) = MEM32(chan + 0x28);

    PUSH32(esp, 1);
    PUSH32(esp, 1);
    ecx = chan;
    PUSH32(esp, MEM8(chan + 0x24));
    PUSH32(esp, MEM8(chan + 0x25));
    eax = MEM8(chan + 0x25);
    g_ebp = frame;
    g_seh_ebp = frame;
    PUSH32(esp, 0x002BE479u); RECOMP_ABI_CALL(0x002BE21Cu, sub_002BE21C);

    ecx = frame - 8;
    g_ebp = frame;
    g_seh_ebp = frame;
    PUSH32(esp, 0x002BE481u); RECOMP_ABI_CALL(0x002B6732u, sub_002B6732);

    POP32(esp, esi);
    esp = frame;
    POP32(esp, ebp_saved);
    g_ebp = ebp_saved;
    esp += 4;
}


/* ── The game clock (0x0038BA6C) and the frame rate ──────────────────────
 *
 * The frame timer sub_001A7450 (thiscall, ecx = the clock, ticked once a frame): +0 frames, +4 game time (ms),
 * +8 the last step, +0xC QPC ticks a ms, +0x10 origin, +0x14 fixed step (ms a frame, 0 = real time, above 10 s
 * = paused), +0x18 minimum step. Real time since the last tick is added, at least +0x18 (renderer init at
 * 0x34A26 sets it through sub_00096520 to 1000/30 on NTSC) and at most 100 ms. Cutscene scripts set the fixed
 * step through sub_00213E40 (via sub_000676A0) and read it back with sub_001E7640.
 * HPPOA_CLOCK_LOG=1: [CLOCK] every 5 s (2: every second), game ms against real ms. */
#define GAME_CLOCK 0x0038BA6Cu

static uint64_t mono_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

/* HPPOA_FPS=60 (overlay: Display > Frame Rate, from the next start): every frame on the next vblank. */
static int fps60(void)
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("HPPOA_FPS");
        v = e && atoi(e) == 60;
        fprintf(stderr, "[fps] frame rate: %s\n", v ? "60 (HPPOA_FPS=60)" : "the title's 30");
    }
    return v;
}

/* 60 fps and the clock (after HP3-Recompiled's hle_video.c and HP-POA's hle_clock.c, MIT; facts only):
 * - the minimum step: at 30 fps a frame is 33 ms and the title's floor (33) is real time; at 60 every frame
 *   is shorter than it and the clock would gain 33 ms a 16.7 ms frame (double speed). The floor becomes 1 ms
 *   and the clock follows real time (HP3-Recompiled measured a 10 ms floor running 1.1-1.3x: a short frame
 *   puts the clock ahead and the next tick's negative step reads as a 100 ms hitch).
 * - the fixed step: a cutscene's script sets ms a frame (33) so the scene is locked to the frame count; at
 *   60 the step is split over two frames (16, 17, the odd ms carried) and the getter answers the script's
 *   own value, which the title saves and puts back. */
static uint32_t s_fixed;    /* the fixed step as the title set it (0 = real time) */
static uint32_t s_carry;

extern void sub_00213E40_gen(void);
void sub_00213E40(void)     /* a class's +0x14 setter, thiscall ([esp+4] = value) */
{
    if (ecx == GAME_CLOCK) {
        uint32_t v = MEM32(esp + 4);
        static int log = -1;
        if (log < 0) { const char *e = getenv("HPPOA_CLOCK_LOG"); log = e && *e == '1'; }
        if (log && v != s_fixed) fprintf(stderr, "[CLOCK] fixed step %u ms (from %08X)\n", v, MEM32(esp));
        s_fixed = v, s_carry = 0;
    }
    sub_00213E40_gen();
}

extern void sub_001E7640_gen(void);
void sub_001E7640(void)     /* a class's +0x14 getter, thiscall */
{
    uint32_t obj = ecx;
    sub_001E7640_gen();
    if (obj == GAME_CLOCK && (fps60() || s_ff_cs))
        eax = s_fixed;
}

static void clock_before_tick(uint32_t clk)
{
    if (!fps60()) return;
    MEM32(clk + 0x18) = 1;
    if (s_fixed && s_fixed <= 10000u) {
        uint32_t total = s_fixed + s_carry, step = total / 2;
        s_carry = total % 2;
        MEM32(clk + 0x14) = step ? step : 1;    /* 0 would mean real time */
    }
}

/* ── Cutscenes ──────────────────────────────────────────────────────────
 *
 * The cutscene manager (singleton 0x365EA8, made by sub_000616A0): +0x8C the cutscene playing (0 none),
 * +0x99 it may be skipped. The Apt (Flash) native SkipCutscene (sub_0001FB90) runs sub_00061520(this,
 * force): with a cutscene playing that is skippable (or forced) it ends it (sub_00062170) and answers
 * CutsceneSkipped true to the menu. HPPOA_CUTSCENE_LOG=1 logs both. */
#define CUTSCENES 0x00365EA8u
static int cutscene_log(void)
{
    static int v = -1;
    if (v < 0) { const char *e = getenv("HPPOA_CUTSCENE_LOG"); v = e && *e == '1'; }
    return v;
}
/* The menus' screen (SetTraceControlMode, sub_00020220, cdecl: a name): "ig" in play, "widescreen" while a scripted
 * scene of the level runs with the black bars (no orchestrator cutscene: +0x8C stays 0; the Marauder's Map
 * challenge's opening, Night 0's corridor with the twins), "fe" the front end. HP-POA-Xbox-Recomp's hle_quit.c
 * (MIT; facts only). */
static char s_screen[24];
extern void sub_00020220_gen(void);
void sub_00020220(void)
{
    uint32_t a = MEM32(esp + 4);
    size_t i = 0;
    for (; a && i + 1 < sizeof s_screen && MEM8(a + i); i++) s_screen[i] = (char)MEM8(a + i);
    s_screen[i] = 0;
    if (cutscene_log()) fprintf(stderr, "[cutscene] screen %s (game ms %u)\n", s_screen, MEM32(GAME_CLOCK + 4));
    sub_00020220_gen();
}
static int letterbox_scene(void) { return !strcmp(s_screen, "widescreen"); }

/* Skip Cutscenes (overlay: Tweaks, HPPOA_SKIP_CUTSCENES, on unless 0): Start or Back in any cutscene.
 * - Skippable ones (+0x99 != 0) end the game's own way, sub_00061520(this, force = 1) -> sub_00062170, the path
 *   the menu's SkipCutscene takes.
 * - Unskippable ones are fast-forwarded (HPPOA_CUTSCENE_FF, on unless 0): the game clock's fixed step (+0x14) is
 *   HPPOA_FF_STEP ms a tick (100, the step the timer allows itself after a hitch) and every frame flips on the
 *   next vblank, until the manager's cutscene changes, with the sound muted: about 6x (a 15 s scene in 2.5 s).
 *   The level script's scripted scenes with the black bars (screen "widescreen", no orchestrator cutscene) are
 *   fast-forwarded the same way until the screen changes.
 *   Why not end them: the level script plays a cutscene with PlayScene(orc, done, skip) (sub_00061F10 keeps the
 *   two script functions at +0x1C and +0x20 of the cutscene). The natural end (sub_00062100 -> sub_00062090)
 *   runs the timeline to its last event, then calls `done`. A skip (sub_00062170) puts the orchestrator in skip
 *   mode (sub_001A50D0(1)), so events after the press are never fired, calls `done`, then also `skip`. The title
 *   only offers that for skippable scenes; for the others the script was never written to be cut short (the
 *   dementor's Cutscene_EnterTheDementor queues inhogexcam_01 right after inhogex_07, its `skip` only starts
 *   music), and play test 3 stuck in a looping inhogexcam_01 after a forced end of inhogex_07 (a staged run with
 *   the forced end, build/t/forceA, went on instead: the loop's exact trigger is not reproduced).
 *   Fast-forwarded, the scene plays out: every timeline event fires and `done` runs as on the console.
 *   HPPOA_CUTSCENE_FF=force keeps the forced end (for A/B tests only).
 * The press is asked for in XInputGetState and kept from the game while a cutscene plays (Start would pause
 * it); the skip runs on the game's thread at the next clock tick, before the frame's tasks. */
#define FF_LETTERBOX 1u     /* s_ff_cs while a scripted "widescreen" scene is fast-forwarded */
static volatile int s_skip_req;
extern void xa2_set_mute(int on);
static int skip_cutscenes_on(void)
{
    const char *e = getenv("HPPOA_SKIP_CUTSCENES");
    return !(e && *e == '0');
}
enum { FF_OFF, FF_ON, FF_FORCE };
static int cutscene_ff_mode(void)   /* HPPOA_CUTSCENE_FF: unset/1 fast-forward, 0 off, force: the old forced end */
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("HPPOA_CUTSCENE_FF");
        v = !e || !*e || *e == '1' ? FF_ON : !strcmp(e, "force") ? FF_FORCE : FF_OFF;
    }
    return v;
}
static uint32_t ff_step(void)
{
    static uint32_t v;
    if (!v) { const char *e = getenv("HPPOA_FF_STEP"); v = e && atoi(e) > 0 ? (uint32_t)atoi(e) : 100u; }
    return v;
}
static int cutscene_skip_press_taken(void)   /* the game is not to see Start/Back now */
{
    if (MEM32(CUTSCENES + 0x8C))
        return MEM8(CUTSCENES + 0x99) != 0 || cutscene_ff_mode() != FF_OFF;
    return letterbox_scene() && cutscene_ff_mode() == FF_ON;
}
extern void sub_00061520(void);
static void cutscene_watch(void)
{
    static uint32_t last_cs, last_sk;
    static uint64_t ff_t0;
    static uint32_t ff_g0;
    uint32_t cs = MEM32(CUTSCENES + 0x8C), sk = MEM8(CUTSCENES + 0x99);
    if (cutscene_log() && (cs != last_cs || sk != last_sk))
        fprintf(stderr, "[cutscene] %08X skippable %u (game ms %u, wall %llu ms)\n", cs, sk, MEM32(GAME_CLOCK + 4),
                (unsigned long long)mono_ms());
    last_cs = cs, last_sk = sk;
    {   /* dev: HPPOA_FF_AUTO=<n>: the next n unskippable cutscenes are fast-forwarded without a press */
        static int left = -1; static uint32_t done;
        if (left < 0) { const char *e = getenv("HPPOA_FF_AUTO"); left = e ? atoi(e) : 0; }
        if (cs && cs != done && cs != s_ff_cs && !sk && left > 0) { left--; done = cs; s_skip_req = 2; }
        if (!cs) done = 0;
    }
    if (s_ff_cs && (s_ff_cs == FF_LETTERBOX ? cs || !letterbox_scene() : cs != s_ff_cs)) {   /* it ended */
        MEM32(GAME_CLOCK + 0x14) = s_fixed;     /* the title's own step (60 fps: split again by the next tick) */
        xa2_set_mute(0);
        fprintf(stderr, "[cutscene] %08X fast-forward done: %u game ms in %llu wall ms (next %08X, screen %s)\n",
                s_ff_cs, MEM32(GAME_CLOCK + 4) - ff_g0, (unsigned long long)(mono_ms() - ff_t0), cs, s_screen);
        s_ff_cs = 0;
    }
    if (s_skip_req) {
        int autoreq = s_skip_req == 2;
        s_skip_req = 0;
        if (cs && (sk || (cutscene_ff_mode() == FF_FORCE && !autoreq))) {
            uint32_t s_esp = esp, s_eax = eax, s_ebx = ebx, s_ecx = ecx, s_edx = edx, s_esi = esi, s_edi = edi,
                     s_ebp = g_ebp;
            if (cutscene_log())
                fprintf(stderr, "[cutscene] %08X script functions: done %08X skip %08X\n", cs, MEM32(cs + 0x1C),
                        MEM32(cs + 0x20));
            esp -= 4; MEM32(esp) = 1;           /* force */
            esp -= 4; MEM32(esp) = 0;           /* return address, unused */
            ecx = CUTSCENES;
            sub_00061520();
            fprintf(stderr, "[cutscene] %08X skipped (skippable %u): %s\n", cs, sk, (eax & 0xFF) ? "ended" : "refused");
            esp = s_esp; eax = s_eax; ebx = s_ebx; ecx = s_ecx; edx = s_edx; esi = s_esi; edi = s_edi; g_ebp = s_ebp;
        } else if (!s_ff_cs && (cutscene_ff_mode() == FF_ON || autoreq) && (cs || letterbox_scene())) {
            s_ff_cs = cs ? cs : FF_LETTERBOX;
            ff_t0 = mono_ms(), ff_g0 = MEM32(GAME_CLOCK + 4);
            xa2_set_mute(1);
            fprintf(stderr, "[cutscene] %08X fast-forward (%s), %u ms a tick\n", s_ff_cs,
                    cs ? "not skippable" : "scripted scene, screen widescreen", ff_step());
        }
    }
    if (s_ff_cs && s_fixed <= 10000u)           /* above 10 s the clock is paused: left alone */
        MEM32(GAME_CLOCK + 0x14) = ff_step();
}
extern void sub_0001FB90_gen(void);
void sub_0001FB90(void)
{
    uint32_t cs = MEM32(CUTSCENES + 0x8C), sk = MEM8(CUTSCENES + 0x99);
    sub_0001FB90_gen();
    if (cutscene_log()) fprintf(stderr, "[cutscene] SkipCutscene native: cutscene %08X skippable %u\n", cs, sk);
}

/* The chapter start sub_00115390 (cdecl: chapter, room or -1 for the chapter's own, start point name, flag),
 * logged with HPPOA_CUTSCENE_LOG=1. New Game (StartGame, 0x21A60, return 0x21C6B) passes chapter 0, room 42 (the
 * train). Dev: HPPOA_START_CHAPTER=<chapter>[:<room>] makes the first New Game start there instead (room -1 when
 * not given). For tests only: no menu row. */
extern void sub_00115390_gen(void);
void sub_00115390(void)
{
    static int done;
    const char *e = getenv("HPPOA_START_CHAPTER");
    if (!done && e && *e && MEM32(esp) == 0x00021C6Bu) {
        done = 1;
        MEM32(esp + 4) = (uint32_t)atoi(e);
        MEM32(esp + 8) = strchr(e, ':') ? (uint32_t)atoi(strchr(e, ':') + 1) : 0xFFFFFFFFu;
        fprintf(stderr, "[dev] New Game starts at chapter %d room %d\n", (int)MEM32(esp + 4), (int)MEM32(esp + 8));
    }
    if (cutscene_log())
        fprintf(stderr, "[chapter] start chapter %d room %d point %08X flag %u (from %08X)\n", (int)MEM32(esp + 4),
                (int)MEM32(esp + 8), MEM32(esp + 12), MEM32(esp + 16), MEM32(esp));
    sub_00115390_gen();
}

/* Dev: HPPOA_MOVE_LOG=scan lists the player object's floats (+0..+0x800) that changed in the last second;
 * HPPOA_MOVE_LOG=1 logs a [move] line a tick: game ms, step, the left stick as the game sees it (pad 0's
 * floats at 0x3C5BC0/4) and the player's position (+0x4EC x, +0x4F0 y, +0x4F4 z; HPPOA_MOVE_POS=<hex> another). The player: the character table [0x3660C8], player 0's
 * handle at +0xE6, the object at [0x3BDCF0 + handle*4] (HP-POA's hle_trainer.c, MIT; facts only). */
#define CHAR_TABLE  0x003660C8u
#define OBJ_HANDLES 0x003BDCF0u
static uint32_t player_obj(void)
{
    uint32_t tab = MEM32(CHAR_TABLE), h, o;
    if ((tab & 0x7FFFFFFFu) < 0x10000u || (tab & 0x7FFFFFFFu) > 0x3FF0000u) return 0;
    h = MEM16(tab + 0xE6);
    if (!h || h >= 0x1000u) return 0;
    o = MEM32(OBJ_HANDLES + h * 4u);
    return (o & 0x7FFFFFFFu) >= 0x10000u && (o & 0x7FFFFFFFu) < 0x3FF0000u ? o : 0;   /* also 0x8xxxxxxx */
}
static void move_log(uint32_t clk, uint32_t before)
{
    static int mode = -1;
    if (mode < 0) {
        const char *e = getenv("HPPOA_MOVE_LOG");
        mode = !e || !*e ? 0 : strcmp(e, "scan") == 0 ? 2 : 1;
    }
    if (!mode) return;
    uint32_t o = player_obj();
    if (!o) return;
    if (mode == 2) {
        static float last[512]; static uint32_t last_o; static uint64_t t0;
        uint64_t now = mono_ms();
        if (now - t0 < 1000u) return;
        t0 = now;
        fprintf(stderr, "[move] obj %08X game %u:", o, MEM32(clk + 4));
        for (uint32_t i = 0; i < 512; i++) {
            float f; uint32_t u = MEM32(o + i * 4u); memcpy(&f, &u, 4);
            if (o == last_o && f != last[i] && f == f && fabsf(f) > 1e-3f && fabsf(f) < 1e5f)
                fprintf(stderr, " +%X=%.2f", i * 4u, f);
            last[i] = f;
        }
        last_o = o;
        fprintf(stderr, "\n");
        return;
    }
    {
        static int off = -1;
        float v[6]; uint32_t u;
        if (off < 0) { const char *e = getenv("HPPOA_MOVE_POS"); off = e ? (int)strtol(e, NULL, 16) : 0x4EC; }
        for (int k = 0; k < 2; k++) { u = MEM32(0x3C5BC0u + 4u * k); memcpy(&v[k], &u, 4); }
        for (int k = 0; k < 3; k++) { u = MEM32(o + (uint32_t)off + 4u * k); memcpy(&v[2 + k], &u, 4); }
        fprintf(stderr, "[move] game %u step %u wall %llu stick %.3f %.3f pos %.3f %.3f %.3f\n", MEM32(clk + 4),
                MEM32(clk + 4) - before, (unsigned long long)mono_ms(), v[0], v[1], v[2], v[3], v[4]);
    }
}

/* Dev: HPPOA_POKE8="<hex va>=<value>,...": bytes written every game tick (the title's tuning switches, e.g. NoClip
 * 0x36A93C, DebugMove 0x36AC30; HP-POA-Xbox-Recomp's trainer_vars.h, MIT, facts only). For scripted routes.
 * HPPOA_POKE8_AT=<ms>: only from that many ms of wall time after the first game tick (e.g. TimeFactor 0x3676D8 =
 * 0.0 freezes a scene for A/B shots of one frame). */
static void dev_poke(void)
{
    static int n = -1; static uint32_t va[16]; static uint8_t val[16]; static uint64_t at;
    if (n < 0) {
        const char *e = getenv("HPPOA_POKE8");
        if (getenv("HPPOA_POKE8_AT")) at = mono_ms() + strtoull(getenv("HPPOA_POKE8_AT"), NULL, 10);
        n = 0;
        while (e && *e && n < 16) {
            char *q;
            va[n] = (uint32_t)strtoul(e, &q, 16);
            if (*q != '=') break;
            val[n++] = (uint8_t)strtoul(q + 1, &q, 0);
            e = *q == ',' ? q + 1 : q;
        }
    }
    if (!at || mono_ms() >= at) {
        if (at && n) { fprintf(stderr, "[dev] pokes on\n"); at = 0; }
        for (int i = 0; i < n; i++) MEM8(va[i]) = val[i];
    }
    /* HPPOA_SETPOS="<game ms>,x,y,z": once the game clock passes that time, the player is put at x y z (player
     * +0x4EC, as HPPOA_MOVE_LOG prints it): a scripted route's start that does not depend on the camera */
    {
        static int st = -1; static uint32_t at; static float p[3];
        if (st < 0) {
            const char *e = getenv("HPPOA_SETPOS");
            st = e && sscanf(e, "%u,%f,%f,%f", &at, &p[0], &p[1], &p[2]) == 4 ? 1 : 0;
        }
        if (st == 1 && MEM32(GAME_CLOCK + 4) >= at) {
            uint32_t o = player_obj();
            if (o) {
                float cx; memcpy(&cx, (void *)XBOX_PTR(o + 0x4ECu), 4);
                for (uint32_t i = 0; i < 0x2000u; i += 4) {   /* other copies of the position */
                    float f; memcpy(&f, (void *)XBOX_PTR(o + i), 4);
                    if (f == cx) {   /* each copy is x y z (the body's, the controller's): all written */
                        for (int k = 0; k < 3; k++) memcpy((void *)XBOX_PTR(o + i + 4u * k), &p[k], 4);
                        fprintf(stderr, "[dev] position copy at player +%X\n", i);
                    }
                }
                fprintf(stderr, "[dev] player %08X put at %.1f %.1f %.1f\n", o, p[0], p[1], p[2]);
                st = 2;
            }
        }
    }
}

/* Dev: HPPOA_CALL_SCRIPT="<s>:<function>,...": <s> seconds after the first clock tick, run that function of the
 * level script by name, as PlayScene does for its callbacks: the level's script module (current level [0x30E2B4]
 * +0xC4, the module at [+0xA8] + index * 0x50 + 0x1C), its lookup (vtable +8 through sub_0006A300) and the script
 * VM's call (sub_001B8C70, sub_001B88D0). For tests only (the train's dementor scene without its fights:
 * Cutscene_MBOM_Defeated, CutScene_DinerCarDoTauntRon, Cutscene_DinerCarMalfoyAttacks, Cutscene_DinerCarDuelWon,
 * Cutscene_EnterTheDementor). */
extern void sub_0006A300(void);
extern void sub_001B8C70(void);
extern void sub_001B88D0(void);
static void dev_call_script(void)
{
    static int n = -1, i; static double at[8]; static char fn[8][64]; static uint64_t t0;
    if (n < 0) {
        const char *e = getenv("HPPOA_CALL_SCRIPT");
        n = 0; t0 = mono_ms();
        while (e && *e && n < 8) {
            at[n] = atof(e); const char *c = strchr(e, ':'); if (!c) break;
            size_t l = strcspn(c + 1, ","); if (l > 63) l = 63;
            memcpy(fn[n], c + 1, l); fn[n][l] = 0; n++;
            e = c + 1 + l; if (*e == ',') e++;
        }
    }
    if (i >= n || (double)(mono_ms() - t0) < at[i] * 1000.0) return;
    uint32_t s_esp = esp, s_eax = eax, s_ebx = ebx, s_ecx = ecx, s_edx = edx, s_esi = esi, s_edi = edi, s_ebp = g_ebp;
    uint32_t lv = MEM32(0x0030E2B4u), idx = lv ? MEM32(lv + 0xC4) : 0, mod = 0, h = 0, ok = 0;
    if (lv && (int)idx > 0) mod = MEM32(MEM32(lv + 0xA8) + idx * 0x50u + 0x1Cu);
    if (mod) {
        uint32_t name = (esp - 256) & ~15u, outp;
        strcpy((char *)XBOX_PTR(name), fn[i]);
        esp = name - 16; outp = esp; MEM32(outp) = 0;
        esp -= 4; MEM32(esp) = outp;
        esp -= 4; MEM32(esp) = name;
        esp -= 4; MEM32(esp) = 0;
        ecx = mod;
        sub_0006A300();
        ok = eax & 0xFF; h = MEM32(outp);
        if (ok) {
            esp = outp - 16;
            esp -= 4; MEM32(esp) = 0;
            sub_001B8C70();
            ecx = eax;
            esp -= 4; MEM32(esp) = 0;
            esp -= 4; MEM32(esp) = h;
            esp -= 4; MEM32(esp) = 0;
            sub_001B88D0();
        }
    }
    fprintf(stderr, "[dev] script %s: module %08X handle %08X %s (game ms %u)\n", fn[i], mod, h, ok ? "called" : "not found",
            MEM32(GAME_CLOCK + 4));
    esp = s_esp; eax = s_eax; ebx = s_ebx; ecx = s_ecx; edx = s_edx; esi = s_esi; edi = s_edi; g_ebp = s_ebp;
    i++;
}
extern void sub_001A7450_gen(void);
void sub_001A7450(void)
{
    if (ecx == GAME_CLOCK) dev_call_script();
    static int log = -1;
    static uint64_t real0;
    static uint32_t game0, frames0, clamps, floors, callers[4], ticks[4];
    uint32_t clk = ecx, before, caller = MEM32(esp);
    int k;

    if (clk != GAME_CLOCK) { sub_001A7450_gen(); return; }
    clock_before_tick(clk);
    cutscene_watch();
    dev_poke();
    before = MEM32(clk + 4);
    sub_001A7450_gen();
    move_log(clk, before);
    if (log < 0) { const char *e = getenv("HPPOA_CLOCK_LOG"); log = e ? atoi(e) : 0; }
    if (!log) return;
    for (k = 0; k < 4 && callers[k] && callers[k] != caller; k++) {}
    if (k < 4) callers[k] = caller, ticks[k]++;
    if (MEM32(clk + 4) - before == 100u) clamps++;
    if (MEM32(clk + 4) - before == MEM32(clk + 0x18) && !MEM32(clk + 0x14)) floors++;
    {
        static uint64_t last; static int shown;
        uint64_t t = mono_ms();
        if (last && t - last < 3 && shown < 3) {
            uint32_t i, n = 0;
            shown++;
            fprintf(stderr, "[CLOCK] tick %llu ms after the last, stack:", (unsigned long long)(t - last));
            for (i = 0; i < 128 && n < 12; i++) {
                uint32_t v = MEM32(esp + i * 4);
                if (v > 0x11000u && v < 0x264C80u) { fprintf(stderr, " %06X", v); n++; }
            }
            fprintf(stderr, "\n");
        }
        last = t;
    }
    {
        uint64_t now = mono_ms();
        if (!real0) { real0 = now; game0 = MEM32(clk + 4); frames0 = MEM32(clk); clamps = floors = 0; return; }
        if (now - real0 < (log == 2 ? 1000u : 5000u)) return;
        fprintf(stderr, "[CLOCK] wall %llu: %u ticks, game %u ms in %llu real ms (x%.3f), floor %u fixed %u, "
                        "%u ticks at the floor, %u of 100 ms; from %08X x%u %08X x%u %08X x%u\n",
                (unsigned long long)now, MEM32(clk) - frames0, MEM32(clk + 4) - game0, (unsigned long long)(now - real0),
                (double)(MEM32(clk + 4) - game0) / (double)(now - real0), MEM32(clk + 0x18), MEM32(clk + 0x14),
                floors, clamps, callers[0], ticks[0], callers[1], ticks[1], callers[2], ticks[2]);
        memset(callers, 0, sizeof callers); memset(ticks, 0, sizeof ticks);
        real0 = now; game0 = MEM32(clk + 4); frames0 = MEM32(clk); clamps = floors = 0;
    }
}

/* ── Skip Logo Movies (overlay: Tweaks, HPPOA_SKIP_LOGOS=1) ───────────────
 *
 * The boot script plays its movies through PlayMovie (sub_001AC0A0, thiscall, ret 8: the name as
 * "<Movie>%s.mad" spells it, and a flag): eac_usa (EA), wb, then intro. PlayMovie marks the movie object
 * playing and makes the player ([0x364120], sub_0005DBE0); each frame the main loop asks the player whether it
 * is still going (sub_0005DE00) and, once it is not, runs StopMovie (sub_001AC100), which is what the script
 * waits for. So a logo still goes through PlayMovie and its player is stopped at once with the player's own
 * stop (sub_0005DC90, cdecl, no argument): the next frame ends it the game's way. Returning without PlayMovie
 * would leave the script waiting on a movie that never ends (HP-POA-Xbox-Recomp's hle_video.c, MIT; facts
 * only). intro (the story's opening) still plays. */
#define MOVIE_PLAYER 0x00364120u
extern void sub_001AC0A0_gen(void);
extern void sub_0005DC90(void);
void sub_001AC0A0(void)
{
    uint32_t name = MEM32(esp + 4);
    const char *e = getenv("HPPOA_SKIP_LOGOS");
    int skip = 0;
    if (name && e && *e == '1') {
        const char *n = (const char *)XBOX_PTR(name);
        skip = strcasecmp(n, "wb") == 0 || strncasecmp(n, "eac", 3) == 0;
    }
    sub_001AC0A0_gen();
    if (skip && MEM32(MOVIE_PLAYER)) {
        uint32_t s_esp = esp, s_eax = eax, s_ebx = ebx, s_ecx = ecx, s_edx = edx, s_esi = esi, s_edi = edi;
        esp -= 4; MEM32(esp) = 0;       /* return address, unused */
        sub_0005DC90();
        esp = s_esp; eax = s_eax; ebx = s_ebx; ecx = s_ecx; edx = s_edx; esi = s_esi; edi = s_edi;
        fprintf(stderr, "[movie] logo %s skipped (HPPOA_SKIP_LOGOS=1)\n", (const char *)XBOX_PTR(name));
    }
}

/* ── Controllers: XAPI's XInput answered from the host pad ─────────────
 *
 * CoS's hook (from MiddleDucky/HP-PS-Xbox-Recomp's hle_input.c, MIT), addresses
 * from HP-POA's hle_input.c and checked here: XInputOpen/Close/GetCapabilities/
 * GetState match CoS's bodies by signature, SetState is the same code, and
 * XInitDevices walks the driver table 0x2BF934..0x2BF944 as CoS's walks its own.
 * The emulated OHCI pad does work for PoA's menus (HPPOA_XINPUT_USB=1 reaches the
 * main menu as well); the direct path is kept as in CoS because XInputSetState
 * there waits on the pad's interrupt OUT pipe, which never answers (CoS hung the
 * first time Harry was hit), and HP-PS saw sticks arrive late. XGetDevices and
 * XGetDeviceChanges stay XAPI's and read the device-type record kept below.
 * HPPOA_XINPUT_USB=1 keeps XAPI's own bodies on the emulated USB. */
#include <pthread.h>
#define XPP_GAMEPAD_TYPE 0x002BF9F4u  /* g_DeviceType_Gamepad (class 0x401 record 0x2BFA2C): Current, Change, Previous */
#define XPP_TYPES_BEGIN  0x002BF924u  /* the linked device type records, begin .. end */
#define XPP_TYPES_END    0x002BF92Cu
#define XI_HANDLE_BASE   0x5ADE0010u  /* opaque handles, never dereferenced by the title */
#define XI_MAX_HANDLES   32
#define XI_PORTS         2
#define ARG(i)           MEM32(esp + 4u + 4u * (i))

extern int usb_gamepad_connected(int dev);
extern int usb_gamepad_report(int dev, uint8_t *out, int max);
extern void usb_gamepad_output(int dev, const uint8_t *data, int len);
extern void xbox_GuestSetEvent(uint32_t token);

static pthread_mutex_t s_xi_lock = PTHREAD_MUTEX_INITIALIZER;
static struct { int used; uint32_t port, packet; uint8_t last[16]; } s_xi[XI_MAX_HANDLES];

static int xi_hle(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("HPPOA_XINPUT_USB");
        on = !(e && *e == '1');
        fprintf(stderr, "[input] XInput %s\n", on ? "from the host pad (direct)" : "through the emulated USB");
    }
    return on;
}

static void xi_update_ports(void)
{
    uint32_t cur = 0, old;
    int p;
    for (p = 0; p < XI_PORTS; p++)
        if (usb_gamepad_connected(p))
            cur |= 1u << p;
    pthread_mutex_lock(&s_xi_lock);
    old = MEM32(XPP_GAMEPAD_TYPE);
    if (cur != old) {
        __atomic_fetch_or((uint32_t *)XBOX_PTR(XPP_GAMEPAD_TYPE + 4), cur ^ old, __ATOMIC_SEQ_CST);
        MEM32(XPP_GAMEPAD_TYPE) = cur;
        fprintf(stderr, "[input] controller ports now %X (was %X)\n", cur, old);
    }
    pthread_mutex_unlock(&s_xi_lock);
}

static void *xi_poller(void *unused)
{
    (void)unused;
    for (;;) {
        usleep(250000);
        xi_update_ports();
    }
    return NULL;
}

static int xi_index(uint32_t h)
{
    uint32_t i = (h - XI_HANDLE_BASE) / 16u;
    if (h < XI_HANDLE_BASE || (h - XI_HANDLE_BASE) % 16u || i >= XI_MAX_HANDLES || !s_xi[i].used)
        return -1;
    return (int)i;
}

/* XInitDevices(dwPreallocTypeCount, PreallocTypes), ret 8. The pads arrive from
 * the poller 250 ms in, as insertions, as on the console. */
extern void sub_002BFC0C_gen(void);
void sub_002BFC0C(void)
{
    static int poller;
    uint32_t t;
    int i;
    if (!xi_hle()) { sub_002BFC0C_gen(); return; }
    for (t = XPP_TYPES_BEGIN; t < XPP_TYPES_END; t += 4) {
        uint32_t info = MEM32(t), type = info ? MEM32(info + 4) : 0;
        if (type)
            for (i = 0; i < 3; i++) MEM32(type + 4u * i) = 0;
    }
    for (i = 0; i < 3; i++) MEM32(XPP_GAMEPAD_TYPE + 4u * i) = 0;
    if (!poller) {
        pthread_t th;
        poller = 1;
        pthread_create(&th, NULL, xi_poller, NULL);
        pthread_detach(th);
    }
    esp += 4 + 8;
}

/* XInputOpen(DeviceType, dwPort, dwSlot, pPollingParameters) -> HANDLE */
extern void sub_002C096C_gen(void);
void sub_002C096C(void)
{
    uint32_t type = ARG(0), port = ARG(1);
    int i;
    if (!xi_hle()) { sub_002C096C_gen(); return; }
    eax = 0;
    if (type == XPP_GAMEPAD_TYPE && port < XI_PORTS)
        for (i = 0; i < XI_MAX_HANDLES; i++)
            if (!s_xi[i].used) {
                memset(&s_xi[i], 0, sizeof s_xi[i]);
                s_xi[i].used = 1;
                s_xi[i].port = port;
                eax = XI_HANDLE_BASE + 16u * (uint32_t)i;
                break;
            }
    fprintf(stderr, "[input] XInputOpen(port %u) = %08X\n", port, eax);
    esp += 4 + 16;
}

/* XInputClose(hDevice) */
extern void sub_002C09C2_gen(void);
void sub_002C09C2(void)
{
    int i;
    if (!xi_hle()) { sub_002C09C2_gen(); return; }
    i = xi_index(ARG(0));
    if (i >= 0) s_xi[i].used = 0;
    esp += 4 + 4;
}

/* XInputGetCapabilities(hDevice, pCapabilities) */
extern void sub_002C09CE_gen(void);
void sub_002C09CE(void)
{
    uint32_t caps = ARG(1);
    int i = xi_hle() ? xi_index(ARG(0)) : -2, k;
    if (i == -2) { sub_002C09CE_gen(); return; }
    if (i < 0) {
        eax = 6;                                    /* ERROR_INVALID_HANDLE */
    } else if (!usb_gamepad_connected((int)s_xi[i].port)) {
        eax = 1167;                                 /* ERROR_DEVICE_NOT_CONNECTED */
    } else {
        MEM8(caps) = 1;                             /* XINPUT_DEVSUBTYPE_GC_GAMEPAD */
        MEM8(caps + 1) = 0;
        MEM16(caps + 2) = 0;
        MEM16(caps + 4) = 0x00FF;
        for (k = 0; k < 8; k++) MEM8(caps + 6 + k) = 0xFF;
        for (k = 0; k < 4; k++) MEM16(caps + 14 + 2 * k) = 0xFFFF;
        MEM16(caps + 22) = 0xFFFF;
        MEM16(caps + 24) = 0xFFFF;
        eax = 0;
    }
    esp += 4 + 8;
}

/* XInputGetState(hDevice, pState): packet number bumped when the report changes.
 * HPPOA_PAD_LOG=1 logs every change. */
extern void sub_002C0BA6_gen(void);
void sub_002C0BA6(void)
{
    uint32_t st = ARG(1);
    uint8_t rep[20];
    int i = xi_hle() ? xi_index(ARG(0)) : -2, k;
    if (i == -2) { sub_002C0BA6_gen(); return; }
    int have = i >= 0 && usb_gamepad_connected((int)s_xi[i].port)
            && usb_gamepad_report((int)s_xi[i].port, rep, (int)sizeof rep) >= 20;
    if (i < 0) {
        eax = 6;
    } else if (!have) {
        eax = 1167;
    } else {
        if (memcmp(rep + 2, s_xi[i].last, 16)) {
            static int log = -1;
            s_xi[i].packet++;
            if (log < 0) { const char *e = getenv("HPPOA_PAD_LOG"); log = e && *e == '1'; }
            if (log)
                fprintf(stderr, "[pad] port %u buttons %02X analog %02X %02X %02X %02X %02X %02X %02X %02X"
                        " LX %d LY %d RX %d RY %d\n", s_xi[i].port, rep[2], rep[4], rep[5], rep[6], rep[7],
                        rep[8], rep[9], rep[10], rep[11], (int16_t)(rep[12] | rep[13] << 8),
                        (int16_t)(rep[14] | rep[15] << 8), (int16_t)(rep[16] | rep[17] << 8),
                        (int16_t)(rep[18] | rep[19] << 8));
            memcpy(s_xi[i].last, rep + 2, 16);
        }
        MEM32(st) = s_xi[i].packet;
        MEM16(st + 4) = rep[2];
        if (s_xi[i].port == 0) {
            static uint8_t was;
            uint8_t now = rep[2] & 0x30u;       /* Start, Back */
            if (skip_cutscenes_on() && cutscene_skip_press_taken()) {
                if (now & ~was) s_skip_req = 1;
                MEM16(st + 4) = rep[2] & ~0x30u;    /* kept from the game */
            }
            was = now;
        }
        for (k = 0; k < 8; k++) MEM8(st + 6 + k) = rep[4 + k];
        for (k = 0; k < 8; k++) MEM8(st + 14 + k) = rep[12 + k];
        eax = 0;
    }
    esp += 4 + 8;
}

/* XInputSetState(hDevice, pFeedback): rumble to the host pad, completes at once
 * (XAPI would complete it from the pad's interrupt OUT pipe, which never answers
 * here). XINPUT_FEEDBACK: { dwStatus, hEvent, ... }, XINPUT_RUMBLE at 0x42. */
void sub_002C0C19(void)
{
    uint32_t fb = ARG(1);
    int i = xi_index(ARG(0));
    if (fb) {
        uint8_t out[6] = { 0x00, 0x06, MEM8(fb + 0x42), MEM8(fb + 0x43), MEM8(fb + 0x44), MEM8(fb + 0x45) };
        uint32_t event = MEM32(fb + 4);
        usb_gamepad_output(i >= 0 ? (int)s_xi[i].port : 0, out, 6);
        MEM32(fb) = 0;                              /* ERROR_SUCCESS */
        if (event)
            xbox_GuestSetEvent(event);
    }
    eax = 997;                                      /* ERROR_IO_PENDING, as XAPI returns */
    esp += 4 + 8;
}
#undef ARG

/*
 * Return a function pointer to override the given Xbox VA, or NULL
 * to fall through to the auto-generated dispatch table.
 *
 * This is called on every indirect call (RECOMP_ICALL) and every
 * direct call through the dispatch table, so keep it fast. A chain
 * of if-statements on uint32_t compiles to a simple comparison
 * sequence; for large override tables, consider a sorted array
 * with binary search.
 *
 * Examples of common override patterns:
 *
 *   // Trace wrapper: log entry/exit around the generated function
 *   extern void sub_00012345(void);
 *   static void traced_sub_00012345(void) {
 *       fprintf(stderr, "[TRACE] sub_00012345 entered, eax=0x%08X\n", g_eax);
 *       sub_00012345();
 *       fprintf(stderr, "[TRACE] sub_00012345 returned, eax=0x%08X\n", g_eax);
 *   }
 *
 *   // Stub: skip a function entirely (return 0 in eax)
 *   static void stub_00067890(void) {
 *       g_eax = 0;
 *   }
 *
 *   // Fix: replace a broken lifted function with correct C
 *   static void fixed_sub_000ABCDE(void) {
 *       // Read arguments from stack/registers per calling convention
 *       uint32_t arg1 = g_ecx;
 *       uint32_t arg2 = MEM32(g_esp + 4);
 *       // ... correct implementation ...
 *       g_eax = result;
 *   }
 */
recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    /*
     * TODO: Add your overrides here. Examples:
     *
     * if (xbox_va == 0x00012345) return traced_sub_00012345;
     * if (xbox_va == 0x00067890) return stub_00067890;
     * if (xbox_va == 0x000ABCDE) return fixed_sub_000ABCDE;
     */

    return (recomp_func_t)0;
}

/* ── ICALL failure logging ─────────────────────────────────── */

#include <execinfo.h>

/* The first wild calls name their caller. A burst of them means a list or object
 * holds garbage; the lifted functions on the host stack say which one walked it (the
 * ring of targets alone does not). Title-specific context (an entity pool, say) goes
 * here when a hook knows one. */
static void icall_wild_context(uint32_t va)
{
    static int printed;
    (void)va;
    if (printed >= 4) return;
    printed++;
    fprintf(stderr, "  guest call site: return to %08X; eax %08X ecx %08X edx %08X ebx %08X esi %08X edi %08X\n",
            MEM32(g_esp), g_eax, g_ecx, g_edx, g_ebx, g_esi, g_edi);
    fprintf(stderr, "  last indirect targets (oldest first):");
    for (int i = 0; i < 16; i++) {
        uint32_t t = g_icall_trace[(g_icall_trace_idx - 16 + i) & (ICALL_TRACE_SIZE - 1)];
        fprintf(stderr, " %08X", t);
    }
    fprintf(stderr, "\n");
}

static void icall_fail_backtrace(void)
{
    static int printed;
    void *frames[10];
    int n;
    if (printed >= 6)
        return;
    printed++;
    n = backtrace(frames, 10);
    fprintf(stderr, "  called from:\n");
    backtrace_symbols_fd(frames + 2, n > 2 ? n - 2 : 0, 2);
}

/*
 * Called when RECOMP_ICALL cannot resolve a target address.
 * This usually means one of:
 *   - A vtable dispatch to an address not in the dispatch table
 *   - A function pointer loaded from uninitialized or corrupt memory
 *   - A kernel thunk address that the bridge doesn't handle
 *
 * During early bring-up you will see many of these. Most are harmless
 * (the ICALL macro pops the dummy return address and continues).
 * Focus on the ones that cause crashes or incorrect behavior.
 */
void recomp_icall_fail_log(uint32_t va)
{
    fprintf(stderr, "[ICALL] Failed to resolve VA 0x%08X (total calls: %llu)\n",
            va, (unsigned long long)g_icall_count);

    /* Dump last 16 call targets from the ring buffer */
    fprintf(stderr, "  Recent ICALL targets:\n");
    for (int i = 0; i < 16; i++) {
        int idx = (g_icall_trace_idx - 16 + i) & 15;
        if (g_icall_trace[idx])
            fprintf(stderr, "    [%2d] 0x%08X\n", i, g_icall_trace[idx]);
    }
    icall_wild_context(va);
    icall_fail_backtrace();
    fflush(stderr);
}

/* An indirect call whose target is not code: a null or wild function pointer.
 *
 * Skipping these is right -- calling a data address is worse -- but skipping
 * them *silently* is not. They almost always arrive inside a loop, so the
 * symptom is a hang with no output rather than a diagnosable null vtable call.
 *
 * Rate-limited per address: a spin can produce millions of these, and the
 * useful information is which addresses occur, not how often.
 */
void recomp_icall_not_code_log(uint32_t va)
{
    enum { SLOTS = 16 };
    static uint32_t seen[SLOTS];
    static uint64_t hits[SLOTS];
    static int count;
    int i;

    for (i = 0; i < count; i++)
        if (seen[i] == va)
            break;
    if (i == count) {
        if (count == SLOTS)
            return;
        seen[count] = va;
        hits[count] = 0;
        count++;
    }
    hits[i]++;
    /* Report at 1, 10, 100, 1000 ... rather than once. A single line says a
     * wild pointer was skipped; the progression says it is being skipped in a
     * loop, which is the difference between a curiosity and the reason the
     * title is hung. */
    {
        uint64_t n = hits[i];
        while (n >= 10 && n % 10 == 0)
            n /= 10;
        if (n != 1)
            return;
    }
    fprintf(stderr, "[ICALL] target 0x%08X is not code -- skipped %llu time(s) "
                    "(null or wild function pointer, at call #%llu)\n",
            va, (unsigned long long)hits[i],
            (unsigned long long)g_icall_count);
    icall_wild_context(va);
    icall_fail_backtrace();
    fflush(stderr);
}

/* ── Untranslated instructions ───────────────────────────────────────────
 *
 * The lifter emits RECOMP_UNIMPL(text, va) at every instruction it has no
 * translation for, in place of the bare comment it used to leave. The
 * instruction is still a no-op; this only stops the omission being silent.
 * RECOMP_UNIMPL_TRAP=1 aborts at the first hit, at the guest address of the
 * cause rather than wherever the damage surfaces. */
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

void recomp_unimpl(const char *text, uint32_t va)
{
    static int printed;
    const char *trap = getenv("RECOMP_UNIMPL_TRAP");
    int stop = trap && *trap && *trap != '0';

    if (printed < 50 || stop) {
        printed++;
        fprintf(stderr,
                "[UNIMPL] untranslated instruction REACHED: `%s` at 0x%08X"
                " (a no-op; set RECOMP_UNIMPL_TRAP=1 to stop here)\n",
                text, va);
        fflush(stderr);
    }
    if (stop) abort();
}

/* Dev: HPPOA_TM_LOG=1 logs D3DDevice_SetTransform (0x2672C0, stdcall type, matrix; XDK types: view 0,
 * projection 1, texture 2-5, world 6-9) for the texture matrices, with the caller (shadow fade mask, PORTING 1d). */
extern void sub_002672C0_gen(void);
void sub_002672C0(void)
{
    static int lg = -1;
    if (lg < 0) { const char *e = getenv("HPPOA_TM_LOG"); lg = e && *e == '1'; }
    if (lg) {
        uint32_t t = MEM32(esp + 4), m = MEM32(esp + 8);
        static int n;
        static const float id[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        extern volatile int nv2a_mtl_capture;
        if (t >= 2 && t <= 5 && n < 4000 && nv2a_mtl_capture > 0 && memcmp((const void *)XBOX_PTR(m), id, sizeof id)) {
            int i;
            n++;
            fprintf(stderr, "[tm] type %u ret %08X caller %08X", t, MEM32(esp), MEM32(esp) == 0x0002F62Au ? MEM32(esp + 24) : 0);
            for (i = 0; i < 16; i++) { uint32_t u = MEM32(m + 4 * i); float f; memcpy(&f, &u, 4); fprintf(stderr, " %.5g", f); }
            fputc('\n', stderr);
            if (MEM32(esp) == 0x0002F62Au && MEM32(esp + 24) == 0x0004AD63u) {
                uint32_t bp = MEM32(esp + 20), r60 = ebx - 0x60, v48 = bp - 0x48;
                fprintf(stderr, "[tm] rec60");
                for (i = 0; i < 16; i++) { uint32_t u = MEM32(r60 + 4 * i); float f; memcpy(&f, &u, 4); fprintf(stderr, " %.5g", f); }
                fprintf(stderr, "\n[tm] m48");
                for (i = 0; i < 16; i++) { uint32_t u = MEM32(v48 + 4 * i); float f; memcpy(&f, &u, 4); fprintf(stderr, " %.5g", f); }
                fputc('\n', stderr);
            }
        }
    }
    sub_002672C0_gen();
}

/* Dev: differential test of the lifted code (tools/difftest.py), as hpcos-xbox-port's. HPPOA_DIFF_DIR=<dir> and
 * HPPOA_DIFF_AT="n1,n2,..." (calls of the shadow matrix builder sub_00048DC0, counted from the first) write
 * <dir>/<n>_pre.bin and <n>_post.bin around those calls: registers, x87 state, guest memory (main RAM from
 * 0x10000, then the 64 MB contiguous window). */
extern size_t xbox_GetMappedSize(void);
static unsigned s_diff_n;
static int diff_due(void)
{
    static int on = -1, nat; static unsigned at[64];
    if (on < 0) {
        const char *e = getenv("HPPOA_DIFF_AT");
        on = getenv("HPPOA_DIFF_DIR") && e;
        while (on && e && *e && nat < 64) { at[nat++] = (unsigned)strtoul(e, NULL, 10); e = strchr(e, ','); if (e) e++; }
    }
    if (!on) return 0;
    s_diff_n++;
    for (int i = 0; i < nat; i++) if (at[i] == s_diff_n) return 1;
    return 0;
}
static void diff_capture(const char *tag)
{
    char fn[600]; FILE *f;
    struct {
        char magic[4]; uint32_t ver; uint32_t r[8]; int32_t fp_top; uint32_t fp_cw; double fp[8];
        uint32_t main_lo, main_size, contig_base, contig_size;
    } h;
    memcpy(h.magic, "HDIF", 4); h.ver = 1;
    h.r[0] = eax; h.r[1] = ecx; h.r[2] = edx; h.r[3] = ebx; h.r[4] = esp; h.r[5] = g_ebp; h.r[6] = esi; h.r[7] = edi;
    h.fp_top = g_fp_top; h.fp_cw = g_fp_control_word;
    memcpy(h.fp, g_fp_stack, sizeof h.fp);
    h.main_lo = 0x10000u; h.main_size = (uint32_t)xbox_GetMappedSize() - 0x10000u;
    h.contig_base = 0x80000000u; h.contig_size = 64u << 20;
    snprintf(fn, sizeof fn, "%s/%u_%s.bin", getenv("HPPOA_DIFF_DIR"), s_diff_n, tag);
    if (!(f = fopen(fn, "wb"))) return;
    fwrite(&h, sizeof h, 1, f);
    fwrite((const void *)XBOX_PTR(h.main_lo), 1, h.main_size, f);
    fwrite((const void *)XBOX_PTR(h.contig_base), 1, h.contig_size, f);
    fclose(f);
    fprintf(stderr, "[diff] %s\n", fn);
}
extern void sub_00048DC0_gen(void);
void sub_00048DC0(void)
{
    static int fn = -1;
    if (fn < 0) { const char *e = getenv("HPPOA_DIFF_FN"); fn = !e || strtoul(e, NULL, 16) == 0x48DC0u; }
    int now = fn && diff_due();
    if (now) diff_capture("pre");
    sub_00048DC0_gen();
    if (now) diff_capture("post");
}

/* Dev: HPPOA_WATCH_FMT=<hex dword>,<seconds>: at that time, find every dword of that value in main RAM (a texture
 * header's Format, e.g. 06710B29 for a 128x64 P8 text texture), write-protect their pages and log the lifted
 * function (dladdr of the host pc) that writes within 16 bytes of one (Options "(" glyph, PORTING 1d). Pages are
 * protected again at each vblank. */
#include <signal.h>
#include <dlfcn.h>
#include <sys/mman.h>
#define WATCH_MAX 64
static uint32_t s_wva[WATCH_MAX]; static int s_nw;
static volatile int s_w_armed;
static struct sigaction s_w_prev[2];
static void w_protect(int prot)
{
    for (int i = 0; i < s_nw; i++)
        mprotect((void *)((uintptr_t)XBOX_PTR(s_wva[i]) & ~(uintptr_t)0x3FFF), 0x4000, prot);
}
static void w_handler(int sig, siginfo_t *si, void *uc)
{
    uintptr_t a = (uintptr_t)si->si_addr;
    int hit = -1, mine = 0;
    for (int i = 0; i < s_nw; i++) {
        uintptr_t h = (uintptr_t)XBOX_PTR(s_wva[i]);
        if ((a & ~(uintptr_t)0x3FFF) == (h & ~(uintptr_t)0x3FFF)) mine = 1;
        if (a + 16 >= h && a <= h + 16) hit = i;
    }
    if (mine) {
        if (hit >= 0) {
            Dl_info di; void *pc = NULL;
#if defined(__aarch64__)
            pc = (void *)((ucontext_t *)uc)->uc_mcontext->__ss.__pc;
#endif
            memset(&di, 0, sizeof di);
            dladdr(pc, &di);
            fprintf(stderr, "[watch] write %08X (header %08X %+d) by %s\n",
                    (unsigned)(a - (uintptr_t)g_xbox_mem_offset), s_wva[hit],
                    (int)(a - (uintptr_t)XBOX_PTR(s_wva[hit])), di.dli_sname ? di.dli_sname : "?");
        }
        w_protect(PROT_READ | PROT_WRITE);
        s_w_armed = 0;
        return;
    }
    {
        struct sigaction *p = &s_w_prev[sig == SIGBUS];
        if (p->sa_flags & SA_SIGINFO) p->sa_sigaction(sig, si, uc);
        else if (p->sa_handler != SIG_DFL && p->sa_handler != SIG_IGN) p->sa_handler(sig);
        else { signal(sig, SIG_DFL); raise(sig); }
    }
}
void xbox_after_vblank_dpc(void)
{
    static int on = -1; static uint32_t fmt; static double at; static struct timespec t0;
    struct timespec now;
    if (on < 0) {
        const char *e = getenv("HPPOA_WATCH_FMT");
        on = e && strchr(e, ',');
        if (on) { fmt = (uint32_t)strtoul(e, NULL, 16); at = atof(strchr(e, ',') + 1); clock_gettime(CLOCK_MONOTONIC, &t0); }
    }
    if (!on) return;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (on == 1 && (now.tv_sec - t0.tv_sec) + (now.tv_nsec - t0.tv_nsec) / 1e9 >= at) {
        uint32_t lim = (uint32_t)xbox_GetMappedSize(), a;
        struct sigaction sa;
        if (getenv("HPPOA_WATCH_ADDR")) { s_wva[s_nw++] = (uint32_t)strtoul(getenv("HPPOA_WATCH_ADDR"), NULL, 16); lim = 0; }
        for (a = 0x10000; a + 4 <= lim && s_nw < WATCH_MAX; a += 4)
            if (MEM32(a) == fmt) {
                s_wva[s_nw++] = a;
                fprintf(stderr, "[watch] %08X at %08X: %08X %08X %08X %08X [%08X] %08X %08X %08X\n", fmt, a, MEM32(a - 16),
                        MEM32(a - 12), MEM32(a - 8), MEM32(a - 4), MEM32(a), MEM32(a + 4), MEM32(a + 8), MEM32(a + 12));
            }
        memset(&sa, 0, sizeof sa);
        sa.sa_sigaction = w_handler; sa.sa_flags = SA_SIGINFO;
        sigaction(SIGBUS, &sa, &s_w_prev[1]);
        sigaction(SIGSEGV, &sa, &s_w_prev[0]);
        on = 2;
    }
    if (on == 2 && !s_w_armed && s_nw) { s_w_armed = 1; w_protect(PROT_READ); }
}

/* Dev: HPPOA_TEX_LOG=1 logs D3DXCreateTexture (0x279064, stdcall device, width, height, levels, usage, format,
 * pool, out) with its caller (Apt text textures: Options "(" glyph, PORTING 1d). */
extern void sub_00279064_gen(void);
void sub_00279064(void)
{
    static int lg = -1;
    if (lg < 0) { const char *e = getenv("HPPOA_TEX_LOG"); lg = e && *e == '1'; }
    if (lg)
        fprintf(stderr, "[d3dx] CreateTexture %u x %u levels %u usage %X format %X from %08X (caller %08X asked %u x %u)\n",
                MEM32(esp + 8), MEM32(esp + 12), MEM32(esp + 16), MEM32(esp + 20), MEM32(esp + 24), MEM32(esp),
                MEM32(g_ebp + 4), MEM32(g_ebp + 12), MEM32(g_ebp + 16));
    if (lg && MEM32(g_ebp + 4) == 0x001AE5DBu) {   /* the text field record (caller's ebx): +0x1C width, +0x20 height */
        uint32_t r = MEM32(g_ebp - 0x20), k;
        fprintf(stderr, "[d3dx]   field");
        for (k = 0; k < 0x58; k += 4) { uint32_t u = MEM32(r + k); float f; memcpy(&f, &u, 4); fprintf(stderr, " +%X=%g", k, f); }
        fputc('\n', stderr);
    }
    sub_00279064_gen();
}

/* Apt text texture width (Options "(" glyph, PORTING 1d item 4). The text field builder sub_001ADFC0 sizes a
 * field's text texture as 2 << _ftol(log2(W + 7)) (0x1AE2E9..0x1AE323; W = the field's width, [ebp-0x18]) but draws
 * the field's quad W + 24 wide (W + 13..14 for a field of mode 2, field +0x18; measured over 38 fields) with
 * u = quad width / texture width. For W in [2^k - 24, 2^k - 8] the quad reaches
 * past the texture and the wrapped u samples the left of the text: the "Off" field (W 115: texture 128, quad 139,
 * u 1.086) shows the left of its "O" as a "(" at its right end. The original x86 makes the same choice (difftest of
 * the call: same +0x1C), so this is the game's own arithmetic, not a lifting difference. Here the exponent from that
 * one _ftol call (return 0x1AE31C) is raised until the texture is at least the quad's width (at most 512, the
 * width the builder already caps fields at). HPPOA_APT_TEXT_FIT=0 keeps the game's size. */
extern void sub_0013E9F8_gen(void);
void sub_0013E9F8(void)
{
    static int on = -1;
    uint32_t ra = MEM32(esp), sp = esp;
    sub_0013E9F8_gen();
    if (ra != 0x001AE31Cu) return;
    if (on < 0) { const char *e = getenv("HPPOA_APT_TEXT_FIT"); on = !e || *e != '0'; }
    {
        /* at the call, esp = ebp - 0x2084 (frame) - 12 (edi, ebx, esi) - 4 (return address) */
        int32_t w = (int32_t)MEM32(sp + 0x2084u + 16u - 0x18u);
        uint32_t mode = MEM32(MEM32(sp + 0x2084u + 16u + 8u) + 0x18u), n = eax;   /* field +0x18 */
        int32_t quad = w + (mode == 2 ? 14 : 24);
        while (on && eax < 8u && (int32_t)(2u << eax) < quad) eax++;
        if (getenv("HPPOA_TEX_LOG"))
            fprintf(stderr, "[d3dx] text field W %d mode %u: texture %u -> %u\n", w, mode, 2u << n, 2u << eax);
    }
}

/* The Apt text field builder (sub_001ADFC0: sizes a field's text texture, calls D3DXCreateTexture through
 * sub_0004F280); HPPOA_DIFF_FN=1ADFC0 with HPPOA_DIFF_DIR/HPPOA_DIFF_AT captures its calls for tools/difftest.py. */
extern void sub_001ADFC0_gen(void);
void sub_001ADFC0(void)
{
    static int fn = -1;
    if (fn < 0) { const char *e = getenv("HPPOA_DIFF_FN"); fn = e && strtoul(e, NULL, 16) == 0x1ADFC0u; }
    int now = fn && diff_due();
    if (fn && getenv("HPPOA_TEX_LOG")) fprintf(stderr, "[d3dx] field builder call %u\n", s_diff_n);
    if (now) diff_capture("pre");
    sub_001ADFC0_gen();
    if (now) diff_capture("post");
}
