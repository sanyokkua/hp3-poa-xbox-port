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

void sub_0026E470(void)
{
    volatile uint32_t *nv = (volatile uint32_t *)XBOX_PTR(0xFD000000u);
    uint32_t blk = ecx;
    uint8_t old = d3d_isr_enter();
    uint32_t nsource, head;

    nv2a_pb_trap_lock();
    head = MEM32(blk + 0x1BC);
    nsource = nv[0x400108 / 4];
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
