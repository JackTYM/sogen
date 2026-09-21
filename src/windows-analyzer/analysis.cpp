#include "std_include.hpp"

#include "analysis.hpp"
#include "analysis_reporter.hpp"
#include "disassembler.hpp"
#include "windows_emulator.hpp"
#include <devices/named_pipe.hpp>
#include <utils/lazy_object.hpp>

#if defined(OS_EMSCRIPTEN) && !defined(SOGEN_EMSCRIPTEN_SUPPORT_NODEJS)
#include <event_handler.hpp>
#endif

#define STR_VIEW_VA(str) static_cast<int>((str).size()), (str).data()

namespace sogen
{

    extern uint64_t g_sldim_dispatch_watch_va;

    namespace
    {
        constexpr size_t MAX_INSTRUCTION_BYTES = 15;
        constexpr uint64_t SYSCALL_INSTRUCTION_SIZE = 2;

        // mojo::IncomingInvitation::AcceptIsolated's RVA in msedge.dll 150.0.7871.187,
        // resolved from Microsoft's own public PDB (see project_solidworks_bringup.md #269).
        constexpr uint64_t ACCEPT_ISOLATED_RVA = 0xa6bef06;
        uint64_t g_accept_isolated_trace_va = 0;

        struct traced_symbol
        {
            const char* name;
            uint64_t rva;
        };

        // ipcz node-connection/transport-activation entry points in msedge.dll 150.0.7871.187,
        // resolved from Microsoft's own public PDB (see project_solidworks_bringup.md #270, #272, #277).
        constexpr std::array<traced_symbol, 16> NODE_CONNECT_TARGETS{{
            {"ipcz::NodeConnector::ConnectNode", 0x1f2e8fc},
            {"ipcz::NodeConnectorForNonBrokerToBroker::Connect", 0x1f2f2d0},
            {"ipcz::NodeConnectorForBrokerToNonBroker::Connect", 0x1534cf0},
            {"ipcz::NodeConnectorForBrokerToBroker::Connect", 0x53acf80},
            {"ipcz::DriverTransport::Activate", 0x1b62548},
            {"mojo::core::ipcz_driver::Transport::Activate", 0x1daba10},
            {"ipcz::NodeConnector::OnTransportError", 0x53acc00},
            {"ipcz::DriverTransport::NotifyError", 0x3458680},
            {"ipcz::DriverTransport::Transmit", 0x21b816c},
            {"mojo::core::ipcz_driver::Transmit(trampoline)", 0x13b4b60},
            {"mojo::core::Channel::WriteNextIpczMessage", 0x1450130},
            {"mojo::core::ChannelWin::Write", 0xab58e0},
            {"mojo::core::Channel::CreateForIpczDriver", 0x1dabd30},
            {"mojo::core::ipcz_driver::Transport::GetIOTaskRunner", 0x53a6b10},
            {"mojo::core::ChannelWin::OnIOCompleted", 0x11a9eb0},
            {"mojo::core::ChannelWin::Start", 0x252d760},
        }};
        std::array<uint64_t, NODE_CONNECT_TARGETS.size()> g_node_connect_trace_vas{};

        // ipcz::NodeConnector::EstablishWaitingRouters's own `cmp rcx, r8` instruction in msedge.dll
        // 150.0.7871.187 (statically disassembled this cycle from the shared root's own msedge.dll,
        // cross-checked against the cached PDB's publics dump; see project_solidworks_bringup.md
        // #335): at this exact point rcx already holds `waiting_routers_.size()` (computed just above
        // via `(end-begin)>>3` on the vector at `this+0x28`/`this+0x30`) and r8 still holds the
        // untouched `max_valid_portals` argument (the wire's `num_initial_portals`, passed through
        // unmodified from `AcceptConnection`) -- the two operands of `std::min` that decide whether
        // the portal-linking loop (the real, non-inlined `AddRemoteRouterLink` call further down this
        // same function) ever runs. `EstablishWaitingRouters`/`AcceptConnection` have no public PDB
        // symbol of their own (ICF-folded away); this RVA was found by walking the direct call site
        // inside `OnConnectFromBrokerToBroker` (RVA 0x53acc10) one hop deeper.
        constexpr uint64_t ESTABLISH_WAITING_ROUTERS_CMP_RVA = 0x1dffc99;
        uint64_t g_establish_waiting_routers_trace_va = 0;

        // The portal-linking loop's own two consequence calls, one hop further down the same
        // EstablishWaitingRouters body (see project_solidworks_bringup.md #335): confirms live
        // whether a nonzero min-comparison (above) actually goes on to call these, closing the loop
        // on the register-level evidence rather than relying on it alone.
        constexpr std::array<traced_symbol, 2> ROUTER_LINK_TARGETS{{
            {"ipcz::NodeLink::AddRemoteRouterLink", 0x109aec0},
            {"ipcz::Router::SetOutwardLink", 0x860658},
        }};
        std::array<uint64_t, ROUTER_LINK_TARGETS.size()> g_router_link_trace_vas{};

        // HandleDelayLoadFailureCommon in msedge.dll 150.0.7871.187, resolved from Microsoft's
        // own public PDB (see project_solidworks_bringup.md #274; #270's RVA for this was wrong,
        // resolving to an unrelated BluetoothAdapterWinrt::CreateDevice offset).
        constexpr uint64_t DELAYLOAD_FAILURE_RVA = 0x8682e93;
        uint64_t g_delayload_failure_trace_va = 0;

        // CreateNamedPipeW's own export RVA in the shared root's kernelbase.dll (a plain PE export,
        // resolved directly via its export table -- no PDB needed; see project_solidworks_bringup.md #279).
        constexpr uint64_t CREATE_NAMED_PIPE_W_RVA = 0x87f20;
        uint64_t g_create_named_pipe_trace_va = 0;

        // mojo::PlatformChannel::PlatformChannel()'s own RVA in msedge.dll 150.0.7871.187, resolved
        // from Microsoft's own public PDB by walking one CreateNamedPipeW caller back (see
        // project_solidworks_bringup.md #279); its constructor body inlines the anonymous-namespace
        // CreateChannel() helper that issues the CreateNamedPipeW/CreateFileW/ConnectNamedPipe
        // self-connect sequence.
        constexpr uint64_t PLATFORM_CHANNEL_CTOR_RVA = 0x1c94708;
        uint64_t g_platform_channel_ctor_trace_va = 0;

        // Entry point of the unexported helper that is PLATFORM_CHANNEL_CTOR_RVA's sole caller,
        // walked one frame further back to identify what repeatedly allocates and invokes it (see
        // project_solidworks_bringup.md #279 point 8-9, #280). No PDB symbol resolves exactly to
        // this address; the nearest preceding public symbol lands on int3 padding, not real code.
        constexpr uint64_t PLATFORM_CHANNEL_TRACKER_ENTRY_RVA = 0x13ead50;
        uint64_t g_platform_channel_tracker_entry_trace_va = 0;

        // The CFG-dispatched virtual delegate call the tracker helper makes right after
        // constructing its PlatformChannel (`call qword ptr [rax+0x80]` where
        // `rax = *(this->[rsi+0x28])`), and the instruction immediately after it -- its
        // return point. See project_solidworks_bringup.md #280's own recommended next step.
        constexpr uint64_t PLATFORM_CHANNEL_DELEGATE_CALL_RVA = 0x13eae53;
        constexpr uint64_t PLATFORM_CHANNEL_DELEGATE_RETURN_RVA = 0x13eae59;
        uint64_t g_platform_channel_delegate_call_trace_va = 0;
        uint64_t g_platform_channel_delegate_return_trace_va = 0;

        // sldim.exe's own inter-thread command-relay busy-spin, disassembled from a live memory
        // dump (see project_solidworks_bringup.md #256): the two `test eax, eax` checks right
        // after each `get_pending_command()` call, where a non-zero eax means the queue held an
        // item.
        constexpr uint64_t SLDIM_QUEUE_CHECK_RVA_1 = 0x666325;
        constexpr uint64_t SLDIM_QUEUE_CHECK_RVA_2 = 0x666339;
        uint64_t g_sldim_queue_check_trace_va_1 = 0;
        uint64_t g_sldim_queue_check_trace_va_2 = 0;
        bool g_sldim_queue_check_checked = false;

        // The real CWnd::OnCommand virtual-dispatch call site inside sldim.exe's own CWnd::OnWndMsg
        // (statically disassembled from the direct-launch cache's own sldim.exe copy this cycle, see
        // project_solidworks_bringup.md #293): OnWndMsg's WM_COMMAND early branch reads
        // pWnd's vtable slot +0xf4 into esi, then `call ecx=pWnd; call esi`. Resolving esi live tells
        // us which module/function actually owns the WM_COMMAND/0x464 handler this whole investigation
        // has been chasing.
        constexpr uint64_t SLDIM_ONCOMMAND_CALL_RVA = 0xb86698;
        uint64_t g_sldim_oncommand_trace_va = 0;

        // sldim.exe's own real OnCommand override (statically resolved this cycle from the call
        // target of the site above, see project_solidworks_bringup.md #293): right after its own
        // per-window state lookup (`call 0x406e06`), it branches on `[state+0x94]` and the command ID
        // to decide whether to relay wParam 0x464 as a private `ON_MESSAGE(0x365, ...)` notification
        // (carrying wParam = original_id + 0x10000) back to the same window, or to fall through to the
        // base `CWnd::OnCommand`. This watch captures the live state pointer and the `[state+0x94]`
        // flag at the exact branch point to determine which path actually executes.
        constexpr uint64_t SLDIM_ONCOMMAND_BRANCH_RVA = 0xb96f6e;
        uint64_t g_sldim_oncommand_branch_trace_va = 0;

        // The real base `CWnd::OnCommand`'s own `OnCmdMsg(nID, CN_COMMAND, &info, nullptr)` virtual
        // dispatch (`call [esi+0xc]` where esi is pWnd's own vtable, resolved this cycle by following
        // the base-OnCommand fallback path from SLDIM_ONCOMMAND_BRANCH_RVA -- see
        // project_solidworks_bringup.md #293). This is MFC's real message-map command-routing engine;
        // resolving its live target identifies which concrete window/document/frame/app object's
        // vtable actually owns `OnCmdMsg` for this window, one hop before the real ON_COMMAND(0x464,
        // ...) handler itself.
        constexpr uint64_t SLDIM_ONCMDMSG_CALL_RVA = 0xb85589;
        uint64_t g_sldim_oncmdmsg_trace_va = 0;

        // Inside the real `_AfxDispatchCmdMsg`-equivalent message-map walker resolved this cycle
        // (sldim.exe+0xb80799, see project_solidworks_bringup.md #293): right after its own
        // `AfxFindMessageEntry`-equivalent lookup call (`call 0x415cc6`) for msg=WM_COMMAND,
        // nID=wParam, this watch captures the live `eax` result to determine whether a real
        // ON_COMMAND(0x464, ...) message-map entry is ever actually found for this window/class chain.
        constexpr uint64_t SLDIM_FINDENTRY_RESULT_RVA = 0xb808c4;
        uint64_t g_sldim_findentry_trace_va = 0;

        // The real, statically-resolved ON_COMMAND(0x464, ...) handler (found this cycle by reading
        // the matched AFX_MSGMAP_ENTRY struct directly out of sldim.exe's own .rdata, see
        // project_solidworks_bringup.md #293): a tiny 5-instruction stub that reads a vtable pointer
        // out of an embedded sub-object at `this+0xcc` and makes one further virtual call
        // (`(*[this+0xcc])[1](0)`). This watch resolves that final virtual call's live target -- the
        // real innermost command-processing code this whole investigation has been chasing.
        constexpr uint64_t SLDIM_HANDLER_DELEGATE_CALL_RVA = 0x66b76e;
        uint64_t g_sldim_handler_delegate_trace_va = 0;

        // sldim.exe's own real "try-pop one work item" primitive (statically disassembled this
        // cycle from the direct-launch cache's own sldim.exe copy, see project_solidworks_bringup.md
        // #294; it is the real body behind the `call sldim.exe+0x418df4` sub-check #293 point 7 left
        // unresolved). Called with ecx = the registered listener object (read from the outer
        // notification-dispatcher's own `this+0xc`), it early-returns null if the listener's
        // `this+0x28` work-queue container is itself null; otherwise it takes a lock (`call
        // 0x41d8c2` on `this+8`) and, for the arg=false variant polled every iteration, checks the
        // queue's own item count at `[queue+4]`: <=0 means empty (returns null, which is what makes
        // the caller `Sleep(1)`), >0 means it pops the head node off a doubly-linked list at
        // `[queue+0]` and dispatches it via the same `0x421bd4` listener-notify virtual call the
        // outer function itself uses. This watch fires right after `this` (edi) and the queue
        // container pointer `[this+0x28]` are both resolved.
        constexpr uint64_t SLDIM_TRYPOP_ENTRY_RVA = 0x6a318c;
        uint64_t g_sldim_trypop_entry_trace_va = 0;

        // The queue's own real item-count check inside the try-pop primitive above (`mov
        // ebx,[eax+4]; test ebx,ebx; jle <empty-path>`, where eax is the queue container read from
        // `[listener+0x28]`). This is the actual live value the outer WM_COMMAND handler chain's
        // Sleep(1) polling loop is blocked on: it needs to be seen going above 0 for the self-repost
        // loop to ever process something instead of sleeping.
        constexpr uint64_t SLDIM_TRYPOP_COUNT_CHECK_RVA = 0x6a32d5;
        uint64_t g_sldim_trypop_count_check_trace_va = 0;

        uint64_t g_sldim_trypop_total_hits = 0;
        uint64_t g_sldim_trypop_count_nonzero_hits = 0;
        uint64_t g_sldim_trypop_last_queue_ptr = 0;
        uint64_t g_sldim_trypop_last_count = 0xffffffff;
        bool g_sldim_trypop_write_watch_armed = false;

        struct sldim_queue_check_state
        {
            uint64_t total{0};
            uint64_t nonzero{0};
        };

        sldim_queue_check_state g_sldim_queue_check_state{};

        // CMessagingThread's own real constructors and destructor (statically disassembled this
        // cycle from the direct-launch cache's own sldim.exe copy, see
        // project_solidworks_bringup.md #295). state_obj+4 -- the "pending command" field
        // get_pending_command() reads and the self-repost relay polls -- is a raw CMessagingThread*
        // (confirmed via RTTI: the vtable written at the destructor's own `mov [esi],0x184a734`
        // resolves to a `.?AVCMessagingThread@@` type descriptor, itself referencing CWinThread as
        // a base). The 2-arg constructor (`sldim.exe+0xb8abf6`) stores its two arguments into
        // `this+0x38`/`this+0x34`; the 0-arg constructor (`sldim.exe+0xb8ac42`) zero-initializes the
        // same two fields. Both watches fire right after the shared SEH-prolog helper returns, at
        // the `mov esi,ecx` that first captures `this`.
        constexpr uint64_t SLDIM_CMSGTHREAD_CTOR2_RVA = 0xb8ac02;
        constexpr uint64_t SLDIM_CMSGTHREAD_CTOR0_RVA = 0xb8ac4e;
        uint64_t g_sldim_cmsgthread_ctor2_trace_va = 0;
        uint64_t g_sldim_cmsgthread_ctor0_trace_va = 0;

        // CMessagingThread's own real destructor (`sldim.exe+0xb8ac85`, reached from its scalar
        // deleting destructor at `sldim.exe+0xb8ad3d` -- itself the class's real vtable slot 1,
        // `0x4181b0` -- via a thin `jmp` thunk at `0x41d07a`, plus two further direct call sites at
        // `sldim.exe+0x8c8bed`/`0xb9d430` found this cycle, both inside OTHER classes' own
        // destructors that call this one as a base-class subobject destructor). This watch fires
        // right after the destructor's own inline SEH prolog, at the `mov esi,ecx` that first
        // captures `this` -- i.e. every single invocation of this destructor, regardless of which
        // caller reached it or whether the field-4 clear below ends up matching.
        constexpr uint64_t SLDIM_CMSGTHREAD_DTOR_ENTRY_RVA = 0xb8aca8;
        uint64_t g_sldim_cmsgthread_dtor_entry_trace_va = 0;

        // The destructor's own real "am I the currently-pending command" check (`call
        // sldim.exe+0x413958` to resolve `state_obj`, then `cmp [eax+4],esi; jne skip; and
        // [eax+4],0` -- the exact clear site prior findings quoted as `sldim.exe+0xb89cbf`/`f8acbf`,
        // which is this same instruction's own address under the objdump `.text`-section-relative
        // labeling convention, 0x1000 off from the image-base-relative RVA this file's own
        // `exe->image_base + RVA` convention uses everywhere else). This watch fires at the `cmp`
        // itself, capturing `state_obj` (eax), `this` (esi), and the live `state_obj->field_4` value
        // to determine whether the match -- and therefore the clear -- actually happens.
        constexpr uint64_t SLDIM_CMSGTHREAD_DTOR_CHECK_RVA = 0xb8acc4;
        uint64_t g_sldim_cmsgthread_dtor_check_trace_va = 0;

        uint64_t g_sldim_cmsgthread_ctor_hits = 0;
        uint64_t g_sldim_cmsgthread_dtor_entry_hits = 0;
        uint64_t g_sldim_cmsgthread_dtor_match_hits = 0;

        // get_pending_command()'s own real body (`sldim.exe+0xb8af2f`, see project_solidworks_bringup.md
        // #294): `call sldim.exe+0x413958` resolves `state_obj`, then `mov eax,[eax+4]; ret` reads
        // field-4. This watch fires at the `mov eax,[eax+4]` itself, where `eax` still holds the
        // un-dereferenced `state_obj` pointer -- the first time it is seen ON TID 40 (the producer
        // busy-loop thread #293/#294 already identified; `state_obj` is itself thread-local, so arming
        // on any other thread's own call would watch a different, unrelated slot), a `hook_memory_write`
        // is dynamically armed on `state_obj+4` (the same technique #294 used for the trypop queue's own
        // count field), catching every single write to the "pending command" field regardless of which
        // code performs it, however rare -- the definitive way to answer where it is actually set/cleared.
        constexpr uint64_t SLDIM_GET_PENDING_COMMAND_STATE_RVA = 0xb8af34;
        uint64_t g_sldim_get_pending_command_state_trace_va = 0;
        bool g_sldim_pending_command_write_watch_armed = false;

        // Arms the moment ANY thread's own FSCTL_PIPE_LISTEN targets a "mojo."-prefixed pipe (the real
        // cross-process bootstrap pipe's own naming convention; see project_solidworks_bringup.md #279)
        // rather than watching a hardcoded tid: #296 found the accepting thread (tid=28 that cycle, not
        // assumed stable run-to-run) is observed exactly once, at that same FSCTL_PIPE_LISTEN call, and
        // never again -- this captures whichever tid actually issues it this run and traces everything
        // that thread does (or fails to do) afterward.
        bool g_thread_activity_armed = false;
        uint32_t g_thread_activity_target_tid = 0;

        // Extends the above: once armed, any subsequently created worker-factory thread (see
        // src/windows-emulator/syscalls/worker_factory.cpp's ensure_worker_factory_threads) also gets
        // traced -- the pipe listen's own completion is delivered to it via a wait-completion-packet
        // rather than the FSCTL_PIPE_LISTEN thread's own NtRemoveIoCompletion polling, so tracing that
        // thread too is required to see what it does with the delivery.
        std::unordered_set<uint32_t> g_thread_activity_extra_tids{};

        bool is_thread_activity_traced_tid(const uint32_t tid)
        {
            return tid == g_thread_activity_target_tid || g_thread_activity_extra_tids.contains(tid);
        }

        // Extends the thread-activity trace (which only ever logs syscalls) with the first hit of
        // every address a traced thread executes inside EmbeddedBrowserWebView.dll/msedge.dll/
        // sldim.exe -- used to determine whether a traced thread's post-wake activity (see
        // project_solidworks_bringup.md #303 point 9) ever re-enters one of these modules' own
        // code, or stays inside ntdll/kernel32 the whole time (sldim.exe added in #326, to check
        // whether the environment-creation-completion chain ever crosses back into the embedder's
        // own module). dxgi.dll/d3d11.dll/libglesv2.dll/libegl.dll added in #332 to determine
        // whether the gpu-process's own DXGK-issuing code runs through the ANGLE/D3D11 graphics
        // stack instead of msedge.dll directly. Dedup'd by exact address so repeated execution of
        // the same code (loops, re-entry) only logs once; capped to bound log growth over a
        // long-running thread.
        //
        // user32.dll/ntdll.dll are also watched (added in #328), but only for a thread already in
        // the traced set (see is_thread_activity_traced_tid below): unlike the three modules above,
        // this function is called for every executed instruction of every thread system-wide (#327),
        // so watching these two ubiquitous system modules unconditionally would explode the trace.
        // They get their own separate dedup set/cap so the always-on core-module trace (dominated by
        // msedge.dll, which alone reaches tens of thousands of unique addresses within seconds) can't
        // exhaust the budget before a traced thread's narrow window of interest is even reached.
        constexpr size_t MODULE_ENTRY_TRACE_CAP = 1200000;
        std::unordered_set<uint64_t> g_module_entry_traced_addresses{};
        size_t g_module_entry_trace_hits = 0;
        bool g_module_entry_trace_cap_logged = false;

        constexpr size_t MODULE_ENTRY_TRACE_EXTENDED_CAP = 60000;
        std::unordered_set<uint64_t> g_module_entry_traced_extended_addresses{};
        size_t g_module_entry_trace_extended_hits = 0;
        bool g_module_entry_trace_extended_cap_logged = false;

        constexpr size_t MODULE_CENSUS_CAP = 64;
        std::unordered_set<std::string> g_module_census_seen_names{};
        size_t g_module_census_unmapped_hits = 0;

        void trace_module_census_if_new(const analysis_context& c, const uint32_t tid, const uint64_t address, const mapped_module* mod)
        {
            if (g_module_census_seen_names.size() >= MODULE_CENSUS_CAP)
            {
                return;
            }

            if (!mod)
            {
                if (g_module_census_unmapped_hits < 5)
                {
                    ++g_module_census_unmapped_hits;
                    c.win_emu->log.error("[module-census] tid=%u executed unmapped address 0x%llx\n", tid,
                                         static_cast<unsigned long long>(address));
                }
                return;
            }

            if (g_module_census_seen_names.insert(mod->name).second)
            {
                c.win_emu->log.error("[module-census] tid=%u first execution inside %s (0x%llx+0x%llx)\n", tid, mod->name.c_str(),
                                     static_cast<unsigned long long>(mod->image_base),
                                     static_cast<unsigned long long>(address - mod->image_base));
            }
        }

        void trace_module_entry_if_new(const analysis_context& c, const uint32_t tid, const uint64_t address)
        {
            const auto* mod = c.win_emu->mod_manager.find_by_address(address);

            if (std::getenv("SOGEN_TRACE_MODULE_CENSUS"))
            {
                trace_module_census_if_new(c, tid, address, mod);
            }

            if (!mod)
            {
                return;
            }

            if (mod->name == "embeddedbrowserwebview.dll" || mod->name == "msedge.dll" || mod->name == "sldim.exe" ||
                mod->name == "dxgi.dll" || mod->name == "d3d11.dll" || mod->name == "libglesv2.dll" || mod->name == "libegl.dll")
            {
                if (!g_module_entry_traced_addresses.insert(address).second)
                {
                    return;
                }

                if (g_module_entry_trace_hits >= MODULE_ENTRY_TRACE_CAP)
                {
                    if (!g_module_entry_trace_cap_logged)
                    {
                        g_module_entry_trace_cap_logged = true;
                        c.win_emu->log.error("[module-entry-trace] cap of %zu unique addresses reached, suppressing further hits\n",
                                             MODULE_ENTRY_TRACE_CAP);
                    }
                    return;
                }

                ++g_module_entry_trace_hits;
                c.win_emu->log.error("[module-entry-trace] tid=%u %s+0x%llx\n", tid, mod->name.c_str(),
                                     static_cast<unsigned long long>(address - mod->image_base));
                return;
            }

            if ((mod->name == "user32.dll" || mod->name == "ntdll.dll") && is_thread_activity_traced_tid(tid))
            {
                if (!g_module_entry_traced_extended_addresses.insert(address).second)
                {
                    return;
                }

                if (g_module_entry_trace_extended_hits >= MODULE_ENTRY_TRACE_EXTENDED_CAP)
                {
                    if (!g_module_entry_trace_extended_cap_logged)
                    {
                        g_module_entry_trace_extended_cap_logged = true;
                        c.win_emu->log.error(
                            "[module-entry-trace] extended cap of %zu unique addresses reached, suppressing further hits\n",
                            MODULE_ENTRY_TRACE_EXTENDED_CAP);
                    }
                    return;
                }

                ++g_module_entry_trace_extended_hits;
                c.win_emu->log.error("[module-entry-trace] tid=%u %s+0x%llx\n", tid, mod->name.c_str(),
                                     static_cast<unsigned long long>(address - mod->image_base));
            }
        }

        // TppWorkerThread's own real body inside the 32-bit ntdll.dll (RVAs against that module's
        // declared image base, confirmed unrelocated live this cycle -- see
        // project_solidworks_bringup.md #300 for the static disassembly this is based on).
        // WAIT_RETURN_RVA: right after `call ZwWaitForWorkViaWorkerFactory` returns (eax=status);
        // KEY_CHECK_RVA: `mov esi, [ebp-0x3c]` -- esi becomes the delivered miniPacket's own
        // KeyContext, and dispatch branches on whether it is zero; GENERIC_CALL_RVA: the real,
        // CFG-checked indirect `call ebx` that invokes an application-registered TP callback when
        // KeyContext is a live TP-object pointer.
        constexpr uint64_t TPP_WORKER_WAIT_RETURN_RVA = 0x3d008;
        constexpr uint64_t TPP_WORKER_KEY_CHECK_RVA = 0x3d195;
        // `mov ebx, [esi+0x20]` -- watch fires on the NEXT instruction (0x3d1df) so ebx is already
        // loaded, matching this file's own established "hook fires before the watched instruction
        // executes" convention (see SLDIM_GET_PENDING_COMMAND_STATE_RVA's own comment above).
        constexpr uint64_t TPP_WORKER_EBX_LOADED_RVA = 0x3d1df;
        constexpr uint64_t TPP_WORKER_GENERIC_CALL_RVA = 0x3d22c;
        constexpr uint64_t TPP_WORKER_NO_LOCAL_WORK_HELPER_RETURN_RVA = 0x3d277;
        constexpr uint64_t TPP_WORKER_RELEASE_OR_LOOP_RVA = 0x3d0bb;
        // The TP_WAIT-specific sentinel thunk (found live this cycle: `EBX_LOADED` resolves here
        // for the bootstrap pipe's own delivery) itself calls a second internal helper
        // (0x3baf9f-0x3b055 RVA chain) that loads the real, application-registered WaitCallback
        // from the TP_WAIT object at `[edi+0x30]` and CFG-checks/invokes it via `call esi` --
        // watch fires on the next instruction (0x3b013) so esi already holds the real target.
        // Corrected this cycle (see project_solidworks_bringup.md #301): the original RVA here
        // (0x2b013) was off by exactly 0x10000 against the real, disassembly-confirmed address,
        // so this watch never actually fired in any prior cycle that relied on it.
        constexpr uint64_t TPP_WAIT_CALLBACK_LOADED_RVA = 0x3b013;

        // The TP_WAIT sentinel thunk's own real gate (see project_solidworks_bringup.md #301):
        // `mov cl, byte ptr [edi+0x124]` ... `test cl, 1` ... `jne <rearm-instead-of-invoke path>`.
        // This is a genuinely different field than #300's own "+0xDE" writeup (a transcription slip
        // against a nested, unrelated helper) -- ground-truth static disassembly this cycle traced
        // every real access to +0x124 in the module and found exactly three: this gate's own two
        // reads plus its own unconditional post-dispatch clear (both paths converge on clearing it),
        // and TpSetWaitEx's own finalization clear. GATE_TEST_RVA watches the `test cl,1` itself (cl
        // already loaded by the preceding `mov`); REARM_TAKEN_RVA watches the out-of-line re-arm
        // path's own entry, reached only when bit 0 was set.
        constexpr uint64_t TP_WAIT_GATE_TEST_RVA = 0x3ae91;
        constexpr uint64_t TP_WAIT_GATE_REARM_TAKEN_RVA = 0xa13e7;

        // TpAllocWait's own real body (RtlAllocateHeap(..., HEAP_ZERO_MEMORY, 0x128)) is the only
        // place the TP_WAIT object's storage is ever brought into existence -- watching the
        // instruction right after the allocation returns (esi = the new object, or 0 on failure)
        // reads back the +0x124 gate byte directly, to determine whether the real ntdll allocation
        // path actually hands back zeroed memory the way HEAP_ZERO_MEMORY promises.
        constexpr uint64_t TP_ALLOC_WAIT_ZERO_CHECK_RVA = 0x36c0d;

        // TpSetWaitEx's own entry (edi = the TP_WAIT object, its first argument) and its own
        // finalization clear (`and byte ptr [esi+0x124], -4`, reached by both the "nothing to
        // cancel" fast path and the "cancel completed synchronously" path) -- watching both pins
        // down exactly when/how often the real re-arm API runs against this specific object, and
        // what the gate byte held immediately before each finalization clears it.
        constexpr uint64_t TPSETWAITEX_ENTRY_RVA = 0x3c8cf;
        constexpr uint64_t TPSETWAITEX_FINALIZE_CLEAR_RVA = 0x3c9ed;

        // The generic legacy-API bridge invoked as the real callback above (ntdll.dll+0x26990,
        // live-confirmed this cycle) is itself just a shim: it loads the TRUE, deepest
        // application-supplied `WAITORTIMERCALLBACK` from its own context object's `[edi+0x10]`
        // and CFG-checks/invokes it via `call esi` -- watching the instruction right after that
        // load (0x26a19) resolves whether execution genuinely reaches into mojo/Chromium's own
        // code, or stalls inside ntdll's own bridge.
        constexpr uint64_t TP_WAIT_LEGACY_BRIDGE_CALLBACK_LOADED_RVA = 0x26a19;

        // EmbeddedBrowserWebView.dll's own real callback (ntdll.dll+0x26990's `[edi+0x10]` target,
        // live-confirmed in project_solidworks_bringup.md #301 as embeddedbrowserwebview.dll+0x211c40)
        // is itself a small dispatcher, not the real work: static disassembly this cycle (see #302)
        // against the real 32-bit DLL in the shared root shows it moves a closure-shaped two-word
        // object out of its context (`[context+0x14]`, either moving the live value or default-
        // constructing a fresh one depending on a flag at `[context+0x24]`), then calls a
        // PostTask-shaped wrapper (RVA 0x211caa: `push edi(=context+4); push eax(=extracted value);
        // call <wrapper>`) passing `ecx=[context+0x20]` (a "task runner"-shaped object) as `this`.
        // EBWV_POST_TASK_CALL_RVA watches that call site with all three already loaded.
        constexpr uint64_t EBWV_CALLBACK_ENTRY_RVA = 0x211c40;
        constexpr uint64_t EBWV_POST_TASK_CALL_RVA = 0x211caa;

        // Inside the PostTask-shaped wrapper (RVA 0x243ed0), the real work is a single virtual
        // dispatch through the task-runner's own vtable slot 0 (`mov eax,[ecx]; mov edi,[eax];
        // ...CFG-check edi...; call edi`) with a zeroed 8-byte value in the delay-shaped argument
        // slot -- structurally a `base::TaskRunner`-style `PostDelayedTask(location, closure, delay=0)`
        // virtual call. EBWV_POST_TASK_VIRTUAL_CALL_RVA watches the `call edi` itself (edi already
        // loaded, unmodified since) to read the real, dynamically-resolved target live.
        constexpr uint64_t EBWV_POST_TASK_VIRTUAL_CALL_RVA = 0x243f2f;

        // The posted closure's own real content, found by dumping the BindStateBase-shaped object
        // EBWV_POST_TASK_CALL_RVA's own `eax` points to (see project_solidworks_bringup.md's cycle-77
        // finding): `[eax+0]`=refcount(1), `[eax+4]`=polymorphic_invoke_, `[eax+8]`=destructor_,
        // `[eax+0xc]`=is_cancelled_ -- all three function-pointer fields live-confirmed (via
        // SOGEN_TRACE_MODULE_ENTRY) to actually execute on tid=28 shortly after the PostTask call,
        // in exactly that order (is_cancelled_ check, then polymorphic_invoke_, then destructor_
        // once the task finishes) -- proving the posted closure genuinely does get popped and run,
        // not just posted-and-abandoned as #306 could only speculate. `[eax+0x10]`=embeddedbrowser
        // webview.dll+0x211ce0, the bound functor itself: a thiscall sibling of the WaitCallback
        // (same `[this+0x24]` gate byte, same class), also live-confirmed executed by tid=28
        // immediately after polymorphic_invoke_. Its own body loads a delegate object from
        // `[this+0]`, reads that delegate's own vtable slot 1, and calls it with `[this+0x18]` as
        // the sole argument -- EBWV_SIGNALER_DELEGATE_DISPATCH_RVA watches that final `call edi`
        // (edi already loaded with the delegate's real vtable[1] target) to read what it resolves
        // to live.
        constexpr uint64_t EBWV_SIGNALER_DELEGATE_DISPATCH_RVA = 0x211d06;

        // EBWV_SIGNALER_DELEGATE_DISPATCH_RVA's own resolved target (see project_solidworks_bringup.md
        // cycle-77/#311 finding) is embeddedbrowserwebview.dll+0x366620: it validates its second argument
        // against a stored id at `this+0x1c` (`cmp eax,[esi+0x1c]`; mismatches fastfail via int3/ud2),
        // performs a status-shaped indirect call through `[this+4]`, builds outcome arguments, and -- on
        // both resulting sub-paths -- invokes a `base::OnceCallback`-shaped member stored at `this+0x50`
        // via the "run and clear stored callback" helper at +0x366bfe. EBWV_ENV_ARG_VALIDATE_RVA watches
        // the compare itself to confirm live whether the two ever actually mismatch.
        constexpr uint64_t EBWV_ENV_ARG_VALIDATE_RVA = 0x366639;

        // +0x366bfe's own body: a thiscall that moves a BindStateBase-shaped closure out of `[this+0]`
        // (nulling the source, the same std::move tell as EBWV_CALLBACK_ENTRY_RVA), reads its own
        // `polymorphic_invoke_` field from `[closure+4]`, CFG-checks it, and calls it SYNCHRONOUSLY (no
        // PostTask wrapper involved this time, unlike EBWV_CALLBACK_ENTRY_RVA's own chain) with
        // (`closure`, `&arg`) -- structurally a `base::OnceCallback<...>::Run()` invocation, not a repost.
        // EBWV_ENV_COMPLETION_INVOKE_RVA watches the `call ecx` itself (ecx already resolved) to read what
        // the stored completion callback's real target is live.
        constexpr uint64_t EBWV_ENV_COMPLETION_INVOKE_RVA = 0x366c30;

        // +0x366620's own body beyond the argument-validation check (project_solidworks_bringup.md #316):
        // it calls the real Win32 GetOverlappedResult(HANDLE hFile=[this+4], LPOVERLAPPED lpOverlapped=this+8
        // (a genuine 0x14-byte OVERLAPPED-shaped region embedded inline, not a separate pointer),
        // LPDWORD lpNumberOfBytesTransferred=&local, BOOL bWait=FALSE) through the KERNEL32 IAT slot at
        // 0x1051dfe8, confirmed by `pefile` import-table lookup, not by name-guessing. Its BOOL return in eax
        // (`test eax,eax; je <fail-path>` immediately after) is the concrete success/failure status this whole
        // causal chain propagates onward: both the success and failure branches build a small outcome struct and
        // converge on the exact same `call ecx` to +0x366bfe already watched above (EBWV_ENV_COMPLETION_INVOKE_RVA).
        // Disassembly of the resolved target that call reaches (+0x366b10's real body, +0x366b2a) shows it does
        // NOT terminate there: it forwards the outcome struct into ANOTHER call into +0x366bfe (the same shared
        // helper, nested/recursive, not a second sibling call from +0x366620) which is what actually resolves to
        // +0x365640 next -- correcting #312's "BOTH ... environment-creation-completion code" framing, which
        // described the two resolved targets as if they were parallel alternatives from a single call site; they
        // are in fact a two-level chain (+0x366620 -> +0x366bfe -> +0x366b10 -> (nested) +0x366bfe -> +0x365640).
        // EBWV_ENV_GETOVERLAPPEDRESULT_CALL_RVA watches the call site itself (esi=`this`, unmodified since entry)
        // to read the raw handle/overlapped inputs; EBWV_ENV_GETOVERLAPPEDRESULT_RETURN_RVA watches the very next
        // instruction (eax=the real BOOL result; ebp unmodified by the call, so the bytes-transferred out-param
        // is still readable at [ebp-0x14]).
        constexpr uint64_t EBWV_ENV_GETOVERLAPPEDRESULT_CALL_RVA = 0x366652;
        constexpr uint64_t EBWV_ENV_GETOVERLAPPEDRESULT_RETURN_RVA = 0x366658;

        // Re-disassembly of +0x366620 with fresh eyes (project_solidworks_bringup.md #318) shows the
        // GetOverlappedResult result DOES change what's delivered onward, just not by skipping the call to
        // +0x366bfe: both branches converge on the identical `mov ecx,esi; call 0x10366bfe` at +0x3666b2, but
        // each branch leaves a DIFFERENT 8-byte outcome struct sitting at the top of the stack right before that
        // call (the ABI +0x366bfe itself expects, per its own `lea esi,[ebp+8]` prologue): the success branch
        // (+0x36665c..+0x36669f) moves the real environment-pointer field out of `this+4` into it via
        // CreateWebViewEnvironmentWithOptionsInternal-adjacent helpers (+0x785d0/+0x366dfe/+0x367078); the
        // failure branch (+0x3666a4..+0x3666b0) just pushes two zero dwords and calls the trivial zeroing ctor
        // at +0x367064. EBWV_ENV_OUTCOME_INVOKE_ARG_RVA watches the call site itself (esp unmodified since
        // either branch finished, so [esp]/[esp+4] are exactly the two outcome dwords about to be forwarded).
        constexpr uint64_t EBWV_ENV_OUTCOME_INVOKE_ARG_RVA = 0x3666b4;

        // project_solidworks_bringup.md #319: the 3 real call sites (found via a `pefile` KERNEL32 IAT lookup
        // for ConnectNamedPipe, address 0x1051deac, then grepping every `call dword ptr [0x1051deac]` in a full
        // disassembly of the same-MD5 (bd0f2fc3be1e50d311af62801361d5ef) DLL) that issue the ORIGINAL async
        // FSCTL_PIPE_LISTEN-backing Win32 call, all in the same +0x364000-0x368000 region as the already-known
        // WaitCallback/GetOverlappedResult chain. Each watches the call site itself (args not yet popped, so
        // [esp]=hNamedPipe, [esp+4]=lpOverlapped) to read live what OVERLAPPED address is actually submitted.
        constexpr uint64_t EBWV_CONNECT_NAMED_PIPE_CALL_RVAS[] = {0x3664bd, 0x3673bb, 0x367648};
        constexpr size_t EBWV_CONNECT_NAMED_PIPE_CALL_COUNT = std::size(EBWV_CONNECT_NAMED_PIPE_CALL_RVAS);

        // ipcz::Node::ConnectNode's own real entry point (see project_solidworks_bringup.md #305, address
        // corrected by #309): located via an RTTI-string cross-reference walk starting from the local lambda's own
        // TypeDescriptor (`.?AV<lambda_0>@?0??ConnectNode@Node@ipcz@@...`, surfaced by #304's own `strings` output)
        // through the static typeid table it is embedded in, to the code that constructs that table's address.
        // WRAPPER_RVA is a tiny public thunk (`push ebp; mov ebp,esp; call REAL_BODY_RVA; xor eax,eax; pop ebp; ret`)
        // embedded exactly once as a raw function-pointer value at embeddedbrowserwebview.dll+0x43b4ec (#305
        // reported +0x43a0ec, off by exactly 0x1400 -- the delta between .rdata's own PointerToRawData and
        // VirtualAddress -- a raw-file-offset-vs-RVA transcription bug, corrected by #309 against direct PE
        // section-table arithmetic). +0x43b4ec is written as an immediate exactly twice anywhere in the module,
        // both times as `movl $0x1043b4ec, (%reg)` at an object's offset 0 in a constructor -- the standard MSVC
        // vtable-pointer-store pattern -- so it is a C++ vtable slot 0, not a manually-indexed message dispatch
        // table; no code anywhere in the module performs a computed-index load off this address. REAL_BODY_RVA is
        // the actual thiscall implementation (stack-cookie prologue, processes an `absl::Span<uint32_t>` argument,
        // matching the mangled signature `ConnectNode(unsigned int, unsigned int, absl::Span<unsigned int>)`
        // exactly) and is also called directly (devirtualized, never through the vtable) from one other internal
        // site (embeddedbrowserwebview.dll+0x85188, inside IPCZ_CONNECT_NODE_INTERNAL_CALLER_RVA below).
        constexpr uint64_t IPCZ_CONNECT_NODE_WRAPPER_RVA = 0x847b0;
        constexpr uint64_t IPCZ_CONNECT_NODE_REAL_BODY_RVA = 0x847bc;

        // The sole other call site that reaches ConnectNode's real body, found by #309 by re-locating the vtable
        // at +0x43b4ec and searching the whole module for its own address as an operand. Entry point of the
        // enclosing function: thiscall, locks a mutex at this+0x38, looks up an entry in a container at this+0x58
        // keyed by its own second argument (EQUALITY_CHECK_NOT_TAKEN_RVA is the early-exit path when that lookup
        // finds nothing; unrelated to the watches below). If the lookup succeeds, it does a second, separate
        // equality check (`cmpl %ecx, 0x10(%eax); je EQUALITY_CHECK_TAKEN_RVA`) against the caller-supplied key
        // (arg at [ebp+8]) -- #309's own "unconditionally ends in a call to ConnectNode" framing is corrected by
        // this cycle's re-disassembly: EQUALITY_CHECK_TAKEN_RVA's own path branches a THIRD time (this+0x44 vs. a
        // loop cursor) into either a direct call to REAL_BODY_RVA (ecx=this, no loop iterations pending) or a
        // stride-0x10 loop over per-entry work (call +0x861c8 per entry) that never itself calls ConnectNode at
        // all once the loop runs. Reached from two call sites inside a "flush pending completions" loop at
        // +0x84c52/+0x84c96 (one firing with a real dequeued item, one firing with a literal 0/null argument) and,
        // recursively, from two more call sites (+0x8e41a/+0x8e44f) inside the function at +0x8e120 (itself called
        // from this function's own inner loop at +0x85157/+0x85177) -- structurally consistent with being the
        // async completion path that resolves a previously-registered pending operation (matching #308's
        // NodeLink::ReferNonBroker inserting a completion callback into pending_referrals_, keyed by
        // referral_id), but not confirmed as that specific callback by name.
        constexpr uint64_t IPCZ_CONNECT_NODE_INTERNAL_CALLER_RVA = 0x84f76;
        constexpr uint64_t IPCZ_CONNECT_NODE_INTERNAL_CALLER_EQUALITY_CHECK_TAKEN_RVA = 0x85029;
        constexpr uint64_t IPCZ_CONNECT_NODE_INTERNAL_CALLER_EQUALITY_CHECK_NOT_TAKEN_RVA = 0x84ff7;

        // The gate function's 4 known direct callers, reconfirmed this cycle by a fresh whole-module grep for
        // `calll ...0x10084f76` (exactly 4 hits, matching #309's own count with no 5th caller found).
        constexpr uint64_t IPCZ_CONNECT_NODE_INTERNAL_CALLER_SITE_FLUSH_REAL_RVA = 0x84c52;
        constexpr uint64_t IPCZ_CONNECT_NODE_INTERNAL_CALLER_SITE_FLUSH_EMPTY_RVA = 0x84c96;
        constexpr uint64_t IPCZ_CONNECT_NODE_MUTUAL_RECURSION_ENTRY_RVA = 0x8e120;
        constexpr uint64_t IPCZ_CONNECT_NODE_MUTUAL_RECURSION_SITE_1_RVA = 0x8e41a;
        constexpr uint64_t IPCZ_CONNECT_NODE_MUTUAL_RECURSION_SITE_2_RVA = 0x8e44f;

        // The object whose vtable pointer is +0x43b4ec (WRAPPER_RVA at slot 0) is not IpczAPI, IpczDriver, or
        // ipcz::Node's own vtable (none of the three, fetched fresh from real ipcz.h/node.h/api_object.h, has
        // ConnectNode as a virtual method at slot 0 -- IpczAPI declares it at slot 3 behind ~IpczAPI/Close/CreateNode,
        // IpczDriver never declares it at all, and Node's own vtable is only [~Node, Close, CanSendFrom]). Reading the
        // table itself (embeddedbrowserwebview.dll+0x43b4ec) shows only 5 live dwords before the next confirmed
        // vtable anchor at +0x43b500 (also empirically confirmed via its own two `movl $0x1043b500, (%reg)`
        // constructor stores): [WRAPPER_RVA, a trivial `xor eax,eax; ret 4` stub also reused as a shared default
        // across many unrelated tables in this module, an MSVC scalar-deleting-destructor thunk, two null/reserved
        // slots]. The real (non-thunk) constructor is CTOR_RVA (thiscall; stores the vtable pointer, a bool flag
        // arg, a second arg pointer at this+0x10, and an optional-arg-sourced 24-byte bounded copy at this+0x18,
        // then zero-initializes a large tail including a mutex at this+0x38). #309's second `movl $0x1043b4ec`
        // site (formerly assumed to be a second constructor) is DTOR_RVA: it re-stores the same vtable pointer as
        // its first action, the standard MSVC destructor-entry codegen to un-poison the vtable pointer before
        // teardown, not a second construction path; SCALAR_DELETING_DTOR_RVA (the table's own slot-2 destructor
        // thunk) calls DTOR_RVA and then conditionally calls a sized `operator delete`, matching that reading.
        // CTOR_RVA's *only* static caller in the whole module is FACTORY_RVA, which validates its second argument
        // as an IpczDriver-shaped struct (`size>=0x38`, all 13 following dwords non-null -- exactly IpczDriver's
        // real 13-method shape) before constructing. FACTORY_RVA itself has *zero* static callers anywhere in the
        // module, but its address is embedded as data (function-pointer table index 1 of 16) immediately after
        // debug strings reading `..\..\mojo\core\channel_win.cc` / `ChannelWin` -- i.e. FACTORY_RVA is one of real
        // Chromium's `mojo::core::ChannelWin`'s own virtual methods, reachable only via a virtual call through a
        // live ChannelWin vtable pointer, never a direct call -- explaining the missing static callers and
        // overturning every prior cycle's IpczAPI/NodeMessageListener framing for this specific table. No
        // `movl $<ChannelWin's own vtable base>, (%reg)` construction site was found anywhere in the module (a
        // whole-module search for the table's own base address as an immediate came up empty), so it remains
        // unconfirmed whether any live ChannelWin object in this build ever actually carries this specific vtable.
        constexpr uint64_t IPCZ_CHANNEL_WIN_FACTORY_RVA = 0x7d9d0;
        constexpr uint64_t IPCZ_CONNECT_WRAPPER_CTOR_RVA = 0x845f0;
        constexpr uint64_t IPCZ_CONNECT_WRAPPER_DTOR_RVA = 0x84724;
        constexpr uint64_t IPCZ_CONNECT_WRAPPER_SCALAR_DELETING_DTOR_RVA = 0x89580;

        // ipcz::(anonymous namespace)::NodeConnectorForReferrer::Connect's own entry point (see
        // project_solidworks_bringup.md #305): located the same way, via the local lambda
        // `<lambda_1>@?0??Connect@NodeConnectorForReferrer@...`'s TypeDescriptor. The resolved function is a genuine
        // thiscall, zero-argument, stack-cookie-protected method (matching the mangled signature's `Connect(void)`
        // exactly) that is itself embedded exactly once as a raw function-pointer value inside a vtable-shaped array
        // at embeddedbrowserwebview.dll+0x43a3c8, and is also reached via one direct (devirtualized) call site at
        // embeddedbrowserwebview.dll+0x8b6cf (`mov ecx, [edi]; call`).
        constexpr uint64_t IPCZ_NODE_CONNECTOR_FOR_REFERRER_CONNECT_RVA = 0x8b570;

        // ipcz::(anonymous namespace)::NodeConnectorForReferrer::Connect's own `broker_link_` (this+0x38)
        // null-check (see project_solidworks_bringup.md #308): disassembly of the already-located Connect
        // body shows `movl 0x38(%ecx), %eax; testl %eax, %eax; je <no-broker-link branch>` as its very first
        // real work, matching real ipcz source's `if (!broker_link_) { node_->WaitForBrokerLinkAsync(...); ... }`
        // exactly. NO_BROKER_LINK_BRANCH_RVA is the branch target taken when broker_link_ is still null (which
        // constructs a WaitForBrokerLinkAsync callback and returns without ever calling ReferNonBroker) --
        // watching it (in addition to Connect itself) tells us which of the two branches actually executes,
        // if Connect is ever reached at all.
        constexpr uint64_t IPCZ_NODE_CONNECTOR_FOR_REFERRER_NO_BROKER_LINK_BRANCH_RVA = 0x8b5f8;

        // ipcz::NodeLink::ReferNonBroker's own real implementation (see project_solidworks_bringup.md #308):
        // located by disassembling NodeConnectorForReferrer::Connect's own non-null-broker-link branch, which
        // ends with a direct (non-virtual, matching real ipcz source's plain member function) call to this
        // address with ecx=broker_link_. Real ipcz source (node_link.cc) shows it locks a mutex, allocates a
        // referral_id, stores the completion callback in `pending_referrals_`, builds a `msg::ReferNonBroker`
        // with the new transport's driver object appended, and calls `Transmit()` on ITS OWN transport (the
        // already-established broker_link_'s own transport) -- it never activates/reads the new transport
        // being referred at all, matching the disassembly's own mutex-lock/map-insert/message-build shape.
        constexpr uint64_t IPCZ_REFER_NON_BROKER_RVA = 0x8d992;

        // Lazily-resolved DirectComposition wrapper embeddedbrowserwebview.dll builds around
        // DCompositionCreateDevice2 (x86 EmbeddedBrowserWebView.dll 150.0.4078.105, MD5
        // bd0f2fc3be1e50d311af62801361d5ef -- statically disassembled this cycle from the shared
        // root's own copy by finding the sole static xref to the "DCompositionCreateDevice2"
        // ASCII string; see project_solidworks_bringup.md #338). WRAPPER_ENTRY_RVA is the whole
        // function's own entry, a double-checked-locking "once" accessor. If the cached function
        // pointer at +0x1053b684 is unset, it takes the RESOLVE_CALL_RVA branch, which calls a
        // helper with (L"dcomp.dll", "DCompositionCreateDevice2", &cache) -- real Chromium's own
        // `ui/gl/direct_composition_support.cc` pattern of a runtime-optional dcomp.dll import. It
        // then always forwards its own three stack arguments (renderingDevice, iid, ppv -- the
        // real DCompositionCreateDevice2 signature) into the resolved pointer via an indirect
        // `call ecx` at INVOKE_CALL_RVA if that pointer is non-null; INVOKE_RETURN_RVA is the
        // instruction immediately after, where eax holds the real HRESULT. No RTTI/PDB symbol
        // exists for any of this (this DLL's own RTTI, confirmed present via 262 real MSVC type
        // descriptors, covers only ICU/perfetto/std -- none of ui/views/aura/compositor/viz).
        constexpr uint64_t EBWV_DCOMP_DEVICE2_WRAPPER_ENTRY_RVA = 0x17b21a;
        constexpr uint64_t EBWV_DCOMP_DEVICE2_RESOLVE_CALL_RVA = 0x17b298;
        constexpr uint64_t EBWV_DCOMP_DEVICE2_INVOKE_CALL_RVA = 0x17b26c;
        constexpr uint64_t EBWV_DCOMP_DEVICE2_INVOKE_RETURN_RVA = 0x17b26e;
        uint64_t g_ebwv_dcomp_device2_wrapper_entry_va = 0;
        uint64_t g_ebwv_dcomp_device2_resolve_call_va = 0;
        uint64_t g_ebwv_dcomp_device2_invoke_call_va = 0;
        uint64_t g_ebwv_dcomp_device2_invoke_return_va = 0;

        // The 3 DComp accessors' own single callers (see project_solidworks_bringup.md #338/#339),
        // each a per-object method gating device creation on a small "readiness" object that is
        // freshly allocated and constructed on EVERY call -- not a persistent, asynchronously-updated
        // field. Its gate byte (checked by 0x100f791a/0x101b96e2/0x101d5c9a, each an identical
        // `(val - 1) < 2` test) is computed synchronously, in the same call, purely from the caller's
        // own first stack argument ("other", an options/snapshot struct): caller 1 (whose accessor is
        // at 0x10113022) computes `other[0x56] != 0 ? 2 : (this[0x58] == 2 ? 1 : 0)`; callers 2 and 3
        // (accessors at 0x101aab4e/0x101ca9e8) both compute `other[0x35] == 1 ? 2 : 0`. ENTRY_RVA is
        // each caller's real prologue (the task-given mid-function addresses are not function
        // boundaries; found via a static call-target scan of the whole .text section). ARGS_RVA is
        // the first instruction after both `this` and `other` are loaded into registers.
        // RESULT_RVA is the instruction immediately after the gate-check call returns, where ecx
        // still holds the freshly-constructed gate object (the check function never clobbers ecx)
        // and al holds the real bool result.
        constexpr uint64_t EBWV_DCOMP_GATE1_ENTRY_RVA = 0x112832;
        constexpr uint64_t EBWV_DCOMP_GATE1_ARGS_RVA = 0x11284d;
        constexpr uint64_t EBWV_DCOMP_GATE1_RESULT_RVA = 0x112b55;
        constexpr uint64_t EBWV_DCOMP_GATE1_OFFSET = 0x100;

        constexpr uint64_t EBWV_DCOMP_GATE2_ENTRY_RVA = 0x1aa168;
        constexpr uint64_t EBWV_DCOMP_GATE2_ARGS_RVA = 0x1aa185;
        constexpr uint64_t EBWV_DCOMP_GATE2_RESULT_RVA = 0x1aa2b9;
        constexpr uint64_t EBWV_DCOMP_GATE2_OFFSET = 0xd0;

        constexpr uint64_t EBWV_DCOMP_GATE3_ENTRY_RVA = 0x1ca23a;
        constexpr uint64_t EBWV_DCOMP_GATE3_ARGS_RVA = 0x1ca257;
        constexpr uint64_t EBWV_DCOMP_GATE3_RESULT_RVA = 0x1ca382;
        constexpr uint64_t EBWV_DCOMP_GATE3_OFFSET = 0xb0;

        uint64_t g_ebwv_dcomp_gate1_entry_va = 0;
        uint64_t g_ebwv_dcomp_gate1_args_va = 0;
        uint64_t g_ebwv_dcomp_gate1_result_va = 0;
        uint64_t g_ebwv_dcomp_gate2_entry_va = 0;
        uint64_t g_ebwv_dcomp_gate2_args_va = 0;
        uint64_t g_ebwv_dcomp_gate2_result_va = 0;
        uint64_t g_ebwv_dcomp_gate3_entry_va = 0;
        uint64_t g_ebwv_dcomp_gate3_args_va = 0;
        uint64_t g_ebwv_dcomp_gate3_result_va = 0;

        // The 3 gate functions' own real static callers (see project_solidworks_bringup.md #339
        // point 9 / #340): each gate function has exactly 3 static call sites. The task-given
        // addresses are call-site addresses inside enclosing functions, not function boundaries --
        // their true entries were found this cycle via a padding-boundary scan (each candidate sits
        // immediately after a run of 0xCC alignment bytes, disassembles as a textbook
        // `push ebp; mov ebp, esp` prologue, and disassembles cleanly, on an instruction boundary,
        // all the way through to the already-known call site). Index order matches #339's own
        // listing: [0..2] call gate1, [3..5] call gate2, [6..8] call gate3.
        constexpr size_t EBWV_DCOMP_GATE_CALLER_COUNT = 9;
        constexpr std::array<uint64_t, EBWV_DCOMP_GATE_CALLER_COUNT> EBWV_DCOMP_GATE_CALLER_ENTRY_RVAS = {
            0x112de0, 0x1156f0, 0x115d20, 0x1aa020, 0x1aa8a0, 0x1aa9e0, 0x1ca120, 0x1ca750, 0x1ca880};

        bool g_ebwv_dcomp_gate_caller_watches_armed = false;
        std::array<uint64_t, EBWV_DCOMP_GATE_CALLER_COUNT> g_ebwv_dcomp_gate_caller_vas{};

        // Gate1's own object-construction function (0x112832, see EBWV_DCOMP_GATE1_ENTRY_RVA above) calls
        // this lazy-init/cache-lookup helper (RVA 0x2292b0) with (holder-unrelated object, flag=1) right
        // before one CANDIDATE write to the retry-status object's [0xc0] field, via
        // GetLastError()+HRESULT_FROM_WIN32 (RVA 0x112a3a-0x112a50, `call dword ptr [GetLastError]` whose
        // low 16 bits get OR'd with FACILITY_WIN32), taken only when the helper's own `al` return is
        // false. Live-traced this cycle: the helper always returned true with zero syscalls in 3/3
        // observations, so this is NOT the write site that produces the real ERROR_TIMEOUT HRESULT --
        // see EBWV_GATE1_ACCESSOR_RESULT_RVA below for the one that is. This window records every syscall
        // issued by the same thread between entering the helper and returning, kept for completeness/
        // future runs where the helper's own al may come back false (see project_solidworks_bringup.md
        // #340-#341).
        constexpr uint64_t EBWV_GATE1_LAZY_HELPER_ENTRY_RVA = 0x2292b0;
        constexpr uint64_t EBWV_GATE1_LAZY_HELPER_RETURN_RVA = 0x112933;

        uint64_t g_ebwv_gate1_lazy_helper_entry_va = 0;
        uint64_t g_ebwv_gate1_lazy_helper_return_va = 0;
        bool g_ebwv_gate1_lazy_helper_window_active = false;
        uint32_t g_ebwv_gate1_lazy_helper_window_tid = 0;

        // The REAL write site for the retry-status object's [0xc0] field (found this cycle, tracing
        // onward from gate1's own readiness check at 0x100f791a): if the readiness gate passes, gate1's
        // construction function calls its own real accessor (RVA 0x113022, already named in
        // EBWV_DCOMP_GATE1_ENTRY_RVA's own comment) on `this` (esi), and stores the accessor's raw HRESULT
        // return value directly into newobj[0xc0] (RVA 0x112b64: `mov dword ptr [edi+0xc0], eax`
        // immediately after `call 0x10113022`). The accessor itself (0x10113022) is a once-cached
        // lookup: on a cache miss it calls a 3-arg trampoline at RVA 0x17b21a with
        // (0, &IID_c37ea93a-e7aa-450d-b16f-9746cb0407f3, &out) -- that trampoline is a lazily-resolved
        // (magic-static, `_Init_thread_header`/`_Init_thread_footer`-guarded) LoadLibraryW(L"dcomp.dll")
        // + GetProcAddress(L"DCompositionCreateDevice2") delegate call, i.e. this whole chain IS a real
        // call to the actual Win32 `DCompositionCreateDevice2` API -- a different call site than the one
        // #338/#340 already instrumented (SOGEN_TRACE_DCOMP_DEVICE2, which showed zero hits even after
        // being un-gated in #340).
        constexpr uint64_t EBWV_GATE1_ACCESSOR_ENTRY_RVA = 0x113022;
        constexpr uint64_t EBWV_GATE1_ACCESSOR_RESULT_RVA = 0x112b64;
        constexpr uint64_t EBWV_GATE1_ACCESSOR_DCOMP_INVOKE_RVA = 0x17b26c;
        constexpr uint64_t EBWV_GATE1_ACCESSOR_DCOMP_INVOKE_RETURN_RVA = 0x17b26e;
        constexpr uint64_t EBWV_GATE1_ACCESSOR_DCOMP_RESOLVED_FN_RVA = 0x53b684;

        uint64_t g_ebwv_gate1_accessor_entry_va = 0;
        uint64_t g_ebwv_gate1_accessor_result_va = 0;
        uint64_t g_ebwv_gate1_accessor_dcomp_invoke_va = 0;
        uint64_t g_ebwv_gate1_accessor_dcomp_invoke_return_va = 0;
        uint64_t g_ebwv_gate1_accessor_dcomp_resolved_fn_va = 0;

        // Caller-of-gate1's own retry-on-failure gate (RVA 0x112e00-0x112e34, disassembled this cycle):
        // `edi = ecx[0xc0]` (an HRESULT-shaped status field on the "device-kind holder" object passed
        // in as the caller's own second argument), then `jns` skips the whole retry path if edi >= 0
        // (SUCCEEDED), else checks `ecx[0xc4] == 0`, then `edi == E_ABORT (0x80004004)`, else decrements
        // a retry-budget counter at `ecx[0x88]` and forwards the holder object into gate1 again. This VA
        // is the instruction immediately after the HRESULT load, so ecx/edi are the real live status object
        // and status code at the point the retry decision is made.
        constexpr uint64_t EBWV_DCOMP_GATE1_UP1_RETRY_STATUS_RVA = 0x112e06;
        uint64_t g_ebwv_dcomp_gate1_up1_retry_status_va = 0;

        // The holder-object async-submit helper (RVA 0x11f68a, disassembled this cycle -- see
        // project_solidworks_bringup.md #342): called from gate1_construct_fn's "readiness gate failed"
        // branch (RVA 0x112cc9-0x112ccb) with (this=newobj[0x38], holder=[ebp+8], &task=[ebp+0xc]).
        // ENTRY_RVA is past its own prologue where [ebp+8]/[ebp+0xc] are already valid. It makes two
        // early synchronous sub-calls whose failure return values are each independently stored straight
        // into holder[0xc0] (RVA 0x11f78d and 0x11f7b9): the first (0x1011e255, a feature/policy-gate
        // check) returns a hardcoded 0x8007064e on failure; the second (0x1002d9a7, an adapter-preference
        // check) can return `HRESULT_FROM_WIN32(GetLastError())` on failure, via the SAME reusable
        // SRW-lock-guarded cache-lookup helper (RVA 0x2292b0) #341 already showed returns true 3/3 times
        // at a DIFFERENT call site -- this cycle's hooks check whether it also succeeds at THIS call site.
        constexpr uint64_t EBWV_HOLDER_SUBMIT_ENTRY_RVA = 0x11f6ab;
        constexpr uint64_t EBWV_HOLDER_SUBMIT_CHECK1_RETURN_RVA = 0x11f6e9;
        constexpr uint64_t EBWV_HOLDER_SUBMIT_CHECK2_RETURN_RVA = 0x11f715;
        constexpr uint64_t EBWV_HOLDER_SUBMIT_WRITE1_RVA = 0x11f78d;
        constexpr uint64_t EBWV_HOLDER_SUBMIT_WRITE2_RVA = 0x11f7b9;
        constexpr uint64_t EBWV_HOLDER_CHECK2_CACHE_RESULT_RVA = 0x2dab0;
        constexpr uint64_t EBWV_HOLDER_CHECK2_GETLASTERROR_RETURN_RVA = 0x2db08;

        // A whole-.text literal scan for the exact DWORD 0x800705B4 found exactly 2 occurrences in the
        // entire 5.6MB module, both inside a generic, reusable "arm a software deadline timer" utility
        // (RVA 0x171a9a): it builds a delayed-task continuation object with a HARDCODED
        // `edi[0x18] = 0x800705B4` (RVA 0x172746) and a real function-pointer dispatcher
        // (`edi[0x10] = 0x10172b60`), then schedules it via what looks like Chromium's own
        // `PostDelayedTask`-style mechanism, gated on a non-negative timeout duration field. This is NOT
        // derived from GetLastError()/any Win32 return value anywhere in this construction -- it is Chromium's
        // own internal software-watchdog timeout policy constant. WATCHDOG_DISPATCH_ENTRY_RVA watches the
        // callback's own entry (0x172b60) to determine live whether this mechanism ever actually fires
        // during a real run.
        constexpr uint64_t EBWV_HOLDER_WATCHDOG_DISPATCH_ENTRY_RVA = 0x172b60;
        uint64_t g_ebwv_holder_watchdog_dispatch_entry_va = 0;

        uint64_t g_ebwv_holder_submit_entry_va = 0;
        uint64_t g_ebwv_holder_submit_check1_return_va = 0;
        uint64_t g_ebwv_holder_submit_check2_return_va = 0;
        uint64_t g_ebwv_holder_submit_write1_va = 0;
        uint64_t g_ebwv_holder_submit_write2_va = 0;
        uint64_t g_ebwv_holder_check2_cache_result_va = 0;
        uint64_t g_ebwv_holder_check2_getlasterror_return_va = 0;

        void arm_ebwv_holder_submit_watches(const analysis_context& c)
        {
            if (g_ebwv_holder_submit_entry_va != 0)
            {
                return;
            }

            if (!std::getenv("SOGEN_TRACE_HOLDER_SUBMIT"))
            {
                return;
            }

            const auto* ebwv = c.win_emu->mod_manager.find_by_name("embeddedbrowserwebview.dll");
            if (!ebwv || ebwv->machine != IMAGE_FILE_MACHINE_I386)
            {
                return;
            }

            g_ebwv_holder_submit_entry_va = ebwv->image_base + EBWV_HOLDER_SUBMIT_ENTRY_RVA;
            g_ebwv_holder_submit_check1_return_va = ebwv->image_base + EBWV_HOLDER_SUBMIT_CHECK1_RETURN_RVA;
            g_ebwv_holder_submit_check2_return_va = ebwv->image_base + EBWV_HOLDER_SUBMIT_CHECK2_RETURN_RVA;
            g_ebwv_holder_submit_write1_va = ebwv->image_base + EBWV_HOLDER_SUBMIT_WRITE1_RVA;
            g_ebwv_holder_submit_write2_va = ebwv->image_base + EBWV_HOLDER_SUBMIT_WRITE2_RVA;
            g_ebwv_holder_check2_cache_result_va = ebwv->image_base + EBWV_HOLDER_CHECK2_CACHE_RESULT_RVA;
            g_ebwv_holder_check2_getlasterror_return_va = ebwv->image_base + EBWV_HOLDER_CHECK2_GETLASTERROR_RETURN_RVA;
            g_ebwv_holder_watchdog_dispatch_entry_va = ebwv->image_base + EBWV_HOLDER_WATCHDOG_DISPATCH_ENTRY_RVA;

            c.win_emu->log.error("[holder-submit-trace] armed against embeddedbrowserwebview.dll image_base=0x%llx: "
                                 "entry=0x%llx check1_return=0x%llx check2_return=0x%llx write1=0x%llx write2=0x%llx "
                                 "check2_cache_result=0x%llx check2_getlasterror_return=0x%llx watchdog_dispatch_entry=0x%llx\n",
                                 static_cast<unsigned long long>(ebwv->image_base),
                                 static_cast<unsigned long long>(g_ebwv_holder_submit_entry_va),
                                 static_cast<unsigned long long>(g_ebwv_holder_submit_check1_return_va),
                                 static_cast<unsigned long long>(g_ebwv_holder_submit_check2_return_va),
                                 static_cast<unsigned long long>(g_ebwv_holder_submit_write1_va),
                                 static_cast<unsigned long long>(g_ebwv_holder_submit_write2_va),
                                 static_cast<unsigned long long>(g_ebwv_holder_check2_cache_result_va),
                                 static_cast<unsigned long long>(g_ebwv_holder_check2_getlasterror_return_va),
                                 static_cast<unsigned long long>(g_ebwv_holder_watchdog_dispatch_entry_va));
        }

        void trace_ebwv_holder_watchdog_dispatch_entry_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto continuation = emu.reg<uint32_t>(x86_register::ecx);
            uint32_t stored_hresult = 0;
            const bool ok = emu.try_read_memory(continuation + 0x18, &stored_hresult, sizeof(stored_hresult));
            c.win_emu->log.error("[holder-submit-trace] tid=%u WATCHDOG FIRED: continuation=0x%x stored_hresult=0x%x (read_ok=%d)\n", tid,
                                 continuation, stored_hresult, ok ? 1 : 0);
        }

        void trace_ebwv_holder_submit_entry_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto ebp = emu.reg<uint32_t>(x86_register::ebp);
            uint32_t holder = 0;
            uint32_t task = 0;
            emu.try_read_memory(ebp + 8, &holder, sizeof(holder));
            emu.try_read_memory(ebp + 0xc, &task, sizeof(task));
            uint32_t holder_c0 = 0;
            const bool c0_ok = emu.try_read_memory(holder + 0xc0, &holder_c0, sizeof(holder_c0));
            c.win_emu->log.error("[holder-submit-trace] tid=%u ENTERED async-submit helper holder=0x%x task=0x%x "
                                 "holder[0xc0]=0x%x (read_ok=%d)\n",
                                 tid, holder, task, holder_c0, c0_ok ? 1 : 0);
        }

        void trace_ebwv_holder_submit_check1_return_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto eax = emu.reg<uint32_t>(x86_register::eax);
            c.win_emu->log.error("[holder-submit-trace] tid=%u check1 (0x1011e255) returned 0x%x (failed=%d)\n", tid, eax,
                                 static_cast<int32_t>(eax) < 0 ? 1 : 0);
        }

        void trace_ebwv_holder_submit_check2_return_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto eax = emu.reg<uint32_t>(x86_register::eax);
            c.win_emu->log.error("[holder-submit-trace] tid=%u check2 (0x1002d9a7) returned 0x%x (failed=%d)\n", tid, eax,
                                 static_cast<int32_t>(eax) < 0 ? 1 : 0);
        }

        void trace_ebwv_holder_submit_write1_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto holder = emu.reg<uint32_t>(x86_register::eax);
            const auto value = emu.reg<uint32_t>(x86_register::edi);
            c.win_emu->log.error("[holder-submit-trace] tid=%u WRITE1: holder=0x%x holder[0xc0] <- 0x%x (from check1)\n", tid, holder,
                                 value);
        }

        void trace_ebwv_holder_submit_write2_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto holder = emu.reg<uint32_t>(x86_register::ebx);
            const auto value = emu.reg<uint32_t>(x86_register::eax);
            c.win_emu->log.error("[holder-submit-trace] tid=%u WRITE2: holder=0x%x holder[0xc0] <- 0x%x (from check2)\n", tid, holder,
                                 value);
        }

        void trace_ebwv_holder_check2_cache_result_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto al = emu.reg<uint8_t>(x86_register::al);
            c.win_emu->log.error("[holder-submit-trace] tid=%u check2's own cache-lookup helper (0x2292b0) returned al=%d\n", tid, al);
        }

        void trace_ebwv_holder_check2_getlasterror_return_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto eax = emu.reg<uint32_t>(x86_register::eax);
            c.win_emu->log.error("[holder-submit-trace] tid=%u check2 fell back to GetLastError()=%u (0x%x)\n", tid, eax, eax);
        }

        // this[0x58]'s owning class (vtable at embeddedbrowserwebview.dll+0x1043d520, see
        // project_solidworks_bringup.md #342) exposes it as a genuine external-facing property, not an
        // internally-computed value: disassembling the vtable's remaining ~22 methods (cycle 109) found a
        // plain, unconditional getter (RVA 0x112540: `*out = this[0x58]; return S_OK`, or E_POINTER
        // 0x80004003 if out==nullptr) and setter (RVA 0x112560: `this[0x58] = value; return S_OK`,
        // unconditionally, no validation). Both take `this` as an explicit first stack argument (not via
        // ECX) and `ret 8`, confirming real __stdcall/COM calling convention -- i.e. reachable only via
        // indirect vtable dispatch, not a direct call (a whole-.text E8 scan found zero direct callers of
        // either). SETTER_ENTRY_RVA/GETTER_ENTRY_RVA watch each method's own entry, before `push ebp` runs,
        // so `this`/`value`/the return address are still readable directly off the stack.
        constexpr uint64_t EBWV_PROP58_SETTER_ENTRY_RVA = 0x112560;
        constexpr uint64_t EBWV_PROP58_GETTER_ENTRY_RVA = 0x112540;

        uint64_t g_ebwv_prop58_setter_entry_va = 0;
        uint64_t g_ebwv_prop58_getter_entry_va = 0;

        void arm_ebwv_prop58_accessor_watches(const analysis_context& c)
        {
            if (g_ebwv_prop58_setter_entry_va != 0)
            {
                return;
            }

            if (!std::getenv("SOGEN_TRACE_PROP58_ACCESSORS"))
            {
                return;
            }

            const auto* ebwv = c.win_emu->mod_manager.find_by_name("embeddedbrowserwebview.dll");
            if (!ebwv || ebwv->machine != IMAGE_FILE_MACHINE_I386)
            {
                return;
            }

            g_ebwv_prop58_setter_entry_va = ebwv->image_base + EBWV_PROP58_SETTER_ENTRY_RVA;
            g_ebwv_prop58_getter_entry_va = ebwv->image_base + EBWV_PROP58_GETTER_ENTRY_RVA;

            c.win_emu->log.error("[prop58-accessor-trace] armed against embeddedbrowserwebview.dll image_base=0x%llx: "
                                 "setter_entry=0x%llx getter_entry=0x%llx\n",
                                 static_cast<unsigned long long>(ebwv->image_base),
                                 static_cast<unsigned long long>(g_ebwv_prop58_setter_entry_va),
                                 static_cast<unsigned long long>(g_ebwv_prop58_getter_entry_va));
        }

        void trace_ebwv_prop58_setter_entry_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto esp = emu.read_stack_pointer();

            uint32_t return_address = 0;
            uint32_t obj = 0;
            uint32_t value = 0;
            emu.try_read_memory(esp, &return_address, sizeof(return_address));
            emu.try_read_memory(esp + 4, &obj, sizeof(obj));
            emu.try_read_memory(esp + 8, &value, sizeof(value));

            const auto* caller_mod_name = c.win_emu->mod_manager.find_name(return_address);
            const auto* caller_mod = c.win_emu->mod_manager.find_by_address(return_address);
            const auto caller_offset = caller_mod ? return_address - caller_mod->image_base : return_address;

            c.win_emu->log.error("[prop58-accessor-trace] tid=%u SETTER this=0x%x value=0x%x return=0x%x (%s+0x%llx)\n", tid, obj, value,
                                 return_address, caller_mod_name, static_cast<unsigned long long>(caller_offset));
        }

        void trace_ebwv_prop58_getter_entry_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto esp = emu.read_stack_pointer();

            uint32_t return_address = 0;
            uint32_t obj = 0;
            uint32_t out_ptr = 0;
            emu.try_read_memory(esp, &return_address, sizeof(return_address));
            emu.try_read_memory(esp + 4, &obj, sizeof(obj));
            emu.try_read_memory(esp + 8, &out_ptr, sizeof(out_ptr));

            uint32_t current_value = 0;
            const bool value_ok = emu.try_read_memory(static_cast<uint64_t>(obj) + 0x58, &current_value, sizeof(current_value));

            const auto* caller_mod_name = c.win_emu->mod_manager.find_name(return_address);
            const auto* caller_mod = c.win_emu->mod_manager.find_by_address(return_address);
            const auto caller_offset = caller_mod ? return_address - caller_mod->image_base : return_address;

            c.win_emu->log.error("[prop58-accessor-trace] tid=%u GETTER this=0x%x out=0x%x current_value=0x%x "
                                 "(read_ok=%d) return=0x%x (%s+0x%llx)\n",
                                 tid, obj, out_ptr, current_value, value_ok ? 1 : 0, return_address, caller_mod_name,
                                 static_cast<unsigned long long>(caller_offset));
        }

        // Detects a same-thread code-flow boundary crossing between msedge.dll and
        // embeddedbrowserwebview.dll (either architecture): tracks, per tid, the last of these
        // two modules that thread's own instruction stream actually entered, and logs any
        // change. Unlike trace_module_entry_if_new's shared MODULE_ENTRY_TRACE_CAP (which
        // sldim.exe's own code alone can exhaust before this pair ever gets a turn), this has
        // its own small, dedicated budget since real crossings are expected to be rare, not
        // per-instruction-volume.
        constexpr size_t EBWV_MSEDGE_XMODULE_TRACE_CAP = 500;
        std::unordered_map<uint32_t, std::string> g_ebwv_msedge_xmodule_last{};
        size_t g_ebwv_msedge_xmodule_hits = 0;

        void trace_ebwv_msedge_xmodule_crossing(const analysis_context& c, const uint32_t tid, const uint64_t address)
        {
            const auto* mod = c.win_emu->mod_manager.find_by_address(address);
            if (!mod || (mod->name != "msedge.dll" && mod->name != "embeddedbrowserwebview.dll"))
            {
                return;
            }

            const std::string label = mod->name + (mod->machine == IMAGE_FILE_MACHINE_I386 ? " (x86)" : " (x64)");

            auto it = g_ebwv_msedge_xmodule_last.find(tid);
            if (g_ebwv_msedge_xmodule_hits < EBWV_MSEDGE_XMODULE_TRACE_CAP)
            {
                if (it == g_ebwv_msedge_xmodule_last.end())
                {
                    ++g_ebwv_msedge_xmodule_hits;
                    c.win_emu->log.error("[ebwv-msedge-xmodule-trace] tid=%u FIRST ENTRY into %s at %s+0x%llx\n", tid, label.c_str(),
                                         mod->name.c_str(), static_cast<unsigned long long>(address - mod->image_base));
                }
                else if (it->second != label)
                {
                    ++g_ebwv_msedge_xmodule_hits;
                    c.win_emu->log.error("[ebwv-msedge-xmodule-trace] tid=%u CROSSING: %s -> %s at %s+0x%llx\n", tid, it->second.c_str(),
                                         label.c_str(), mod->name.c_str(), static_cast<unsigned long long>(address - mod->image_base));
                }
            }

            g_ebwv_msedge_xmodule_last[tid] = label;
        }

        void arm_ebwv_dcomp_gate_caller_watches(const analysis_context& c)
        {
            if (g_ebwv_dcomp_gate_caller_watches_armed)
            {
                return;
            }

            if (!std::getenv("SOGEN_TRACE_DCOMP_GATE_CALLERS"))
            {
                return;
            }

            const auto* ebwv = c.win_emu->mod_manager.find_by_name("embeddedbrowserwebview.dll");
            if (!ebwv || ebwv->machine != IMAGE_FILE_MACHINE_I386)
            {
                return;
            }

            g_ebwv_dcomp_gate_caller_watches_armed = true;
            for (size_t i = 0; i < EBWV_DCOMP_GATE_CALLER_COUNT; ++i)
            {
                g_ebwv_dcomp_gate_caller_vas[i] = ebwv->image_base + EBWV_DCOMP_GATE_CALLER_ENTRY_RVAS[i];
            }
            g_ebwv_dcomp_gate1_up1_retry_status_va = ebwv->image_base + EBWV_DCOMP_GATE1_UP1_RETRY_STATUS_RVA;

            c.win_emu->log.error("[dcomp-gate-caller-trace] armed against embeddedbrowserwebview.dll image_base=0x%llx, %zu "
                                 "candidate caller-of-gate entries, retry_status_va=0x%llx\n",
                                 static_cast<unsigned long long>(ebwv->image_base), EBWV_DCOMP_GATE_CALLER_COUNT,
                                 static_cast<unsigned long long>(g_ebwv_dcomp_gate1_up1_retry_status_va));
        }

        // EBWV_CALLBACK_ENTRY_RVA's own context object (see project_solidworks_bringup.md #302) is
        // constructed by a generic `RegisterWaitForSingleObject`-wrapping helper (RVA 0x211990,
        // reached only through a thin thiscall thunk at RVA 0x211970 -- found this cycle via the
        // only static xref to 0x211c40 itself, confirming the callback is registered exactly once
        // per watch, with the handle passed as the thunk's own first stack argument). The thunk has
        // exactly 8 static callers across the whole module -- confirming this is generic,
        // reused-everywhere infrastructure (matching `base::win::ObjectWatcher::StartWatchingOnce`),
        // not something exclusive to the bootstrap pipe's own connect/read flow. These watches
        // record which of the 8 call sites actually arms the wait that this investigation has
        // already proven fires (see #299-#306), to determine whether it is even related to pipe I/O.
        constexpr size_t START_WATCHING_ONCE_CALLER_COUNT = 8;
        constexpr std::array<uint64_t, START_WATCHING_ONCE_CALLER_COUNT> START_WATCHING_ONCE_CALLER_RVAS = {
            0x11510d, 0x15d539, 0x15dd1b, 0x1725d8, 0x172a58, 0x174239, 0x21109e, 0x36647d};

        bool g_start_watching_once_watches_armed = false;
        std::array<uint64_t, START_WATCHING_ONCE_CALLER_COUNT> g_start_watching_once_caller_vas{};

        void arm_start_watching_once_watches(const analysis_context& c)
        {
            if (g_start_watching_once_watches_armed)
            {
                return;
            }

            const auto* ebwv = c.win_emu->mod_manager.find_by_name("embeddedbrowserwebview.dll");
            if (!ebwv)
            {
                return;
            }

            g_start_watching_once_watches_armed = true;
            for (size_t i = 0; i < START_WATCHING_ONCE_CALLER_COUNT; ++i)
            {
                g_start_watching_once_caller_vas[i] = ebwv->image_base + START_WATCHING_ONCE_CALLER_RVAS[i];
            }

            c.win_emu->log.error("[start-watching-once-trace] armed against embeddedbrowserwebview.dll image_base=0x%llx, %zu "
                                 "candidate caller sites\n",
                                 static_cast<unsigned long long>(ebwv->image_base), START_WATCHING_ONCE_CALLER_COUNT);
        }

        bool g_ebwv_connect_named_pipe_watches_armed = false;
        std::array<uint64_t, EBWV_CONNECT_NAMED_PIPE_CALL_COUNT> g_ebwv_connect_named_pipe_call_vas{};

        void arm_ebwv_connect_named_pipe_watches(const analysis_context& c)
        {
            if (g_ebwv_connect_named_pipe_watches_armed)
            {
                return;
            }

            const auto* ebwv = c.win_emu->mod_manager.find_by_name("embeddedbrowserwebview.dll");
            if (!ebwv)
            {
                return;
            }

            g_ebwv_connect_named_pipe_watches_armed = true;
            for (size_t i = 0; i < EBWV_CONNECT_NAMED_PIPE_CALL_COUNT; ++i)
            {
                g_ebwv_connect_named_pipe_call_vas[i] = ebwv->image_base + EBWV_CONNECT_NAMED_PIPE_CALL_RVAS[i];
            }

            c.win_emu->log.error("[connect-named-pipe-trace] armed against embeddedbrowserwebview.dll image_base=0x%llx, %zu "
                                 "candidate caller sites\n",
                                 static_cast<unsigned long long>(ebwv->image_base), EBWV_CONNECT_NAMED_PIPE_CALL_COUNT);
        }

        void trace_ebwv_connect_named_pipe_hit(const analysis_context& c, const uint32_t tid, const size_t caller_index)
        {
            auto& emu = c.win_emu->emu();
            const auto esp = emu.reg<uint32_t>(x86_register::esp);

            std::array<uint32_t, 2> args{};
            const bool args_read_ok = emu.try_read_memory(esp, args.data(), sizeof(args));

            uint32_t overlapped_contents_low = 0;
            const bool overlapped_read_ok = emu.try_read_memory(args[1], &overlapped_contents_low, sizeof(overlapped_contents_low));

            c.win_emu->log.error("[connect-named-pipe-trace] tid=%u caller_index=%zu ConnectNamedPipe(hNamedPipe=0x%x, "
                                 "lpOverlapped=0x%x) (args_read_ok=%d) lpOverlapped->Internal=0x%x (read_ok=%d)\n",
                                 tid, caller_index, args[0], args[1], args_read_ok ? 1 : 0, overlapped_contents_low,
                                 overlapped_read_ok ? 1 : 0);
        }

        void trace_ebwv_dcomp_device2_wrapper_entry_hit(const analysis_context& c, const uint32_t tid)
        {
            c.win_emu->log.error("[dcomp-device2-trace] tid=%u REACHED embeddedbrowserwebview.dll's "
                                 "DCompositionCreateDevice2 wrapper entry\n",
                                 tid);
        }

        void trace_ebwv_dcomp_device2_resolve_call_hit(const analysis_context& c, const uint32_t tid)
        {
            c.win_emu->log.error("[dcomp-device2-trace] tid=%u attempting to resolve DCompositionCreateDevice2 via "
                                 "LoadLibraryW(L\"dcomp.dll\")+GetProcAddress\n",
                                 tid);
        }

        void trace_ebwv_dcomp_device2_invoke_call_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto resolved_fn = emu.reg<uint32_t>(x86_register::ecx);
            const auto rendering_device = emu.reg<uint32_t>(x86_register::ebx);
            const auto iid_ptr = emu.reg<uint32_t>(x86_register::edi);
            const auto out_ptr = emu.reg<uint32_t>(x86_register::esi);

            c.win_emu->log.error("[dcomp-device2-trace] tid=%u INVOKING resolved DCompositionCreateDevice2=0x%x "
                                 "renderingDevice=0x%x iid=0x%x ppv=0x%x\n",
                                 tid, resolved_fn, rendering_device, iid_ptr, out_ptr);
        }

        void trace_ebwv_dcomp_device2_invoke_return_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto hresult = emu.reg<uint32_t>(x86_register::eax);
            c.win_emu->log.error("[dcomp-device2-trace] tid=%u DCompositionCreateDevice2 returned HRESULT=0x%x\n", tid, hresult);
        }

        void trace_ebwv_dcomp_gate_entry_hit(const analysis_context& c, const uint32_t tid, const int gate_index)
        {
            c.win_emu->log.error("[dcomp-gate-trace] tid=%u gate=%d REACHED the true entry of the accessor's caller\n", tid, gate_index);
        }

        void trace_ebwv_dcomp_gate1_args_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto this_ptr = emu.reg<uint32_t>(x86_register::esi);
            const auto other_ptr = emu.reg<uint32_t>(x86_register::edi);

            uint8_t other_flag = 0;
            const bool other_read_ok = emu.try_read_memory(other_ptr + 0x56, &other_flag, sizeof(other_flag));
            uint32_t this_state = 0;
            const bool this_read_ok = emu.try_read_memory(this_ptr + 0x58, &this_state, sizeof(this_state));

            c.win_emu->log.error("[dcomp-gate-trace] tid=%u gate=1 this=0x%x other=0x%x other[0x56]=0x%x (read_ok=%d) "
                                 "this[0x58]=0x%x (read_ok=%d)\n",
                                 tid, this_ptr, other_ptr, other_flag, other_read_ok ? 1 : 0, this_state, this_read_ok ? 1 : 0);
        }

        void trace_ebwv_dcomp_gate23_args_hit(const analysis_context& c, const uint32_t tid, const int gate_index)
        {
            auto& emu = c.win_emu->emu();
            const auto this_ptr = emu.reg<uint32_t>(x86_register::ebx);
            const auto other_ptr = emu.reg<uint32_t>(x86_register::edi);

            uint8_t other_flag = 0;
            const bool other_read_ok = emu.try_read_memory(other_ptr + 0x35, &other_flag, sizeof(other_flag));

            c.win_emu->log.error("[dcomp-gate-trace] tid=%u gate=%d this=0x%x other=0x%x other[0x35]=0x%x (read_ok=%d)\n", tid, gate_index,
                                 this_ptr, other_ptr, other_flag, other_read_ok ? 1 : 0);
        }

        void trace_ebwv_dcomp_gate_result_hit(const analysis_context& c, const uint32_t tid, const int gate_index,
                                              const uint32_t field_offset)
        {
            auto& emu = c.win_emu->emu();
            const auto newobj = emu.reg<uint32_t>(x86_register::ecx);
            const auto eax = emu.reg<uint32_t>(x86_register::eax);

            uint32_t field_value = 0;
            const bool read_ok = emu.try_read_memory(newobj + field_offset, &field_value, sizeof(field_value));

            c.win_emu->log.error("[dcomp-gate-trace] tid=%u gate=%d newobj=0x%x newobj[0x%x]=0x%x (read_ok=%d) gate_pass=%d\n", tid,
                                 gate_index, newobj, field_offset, field_value, read_ok ? 1 : 0, (eax & 0xff) != 0 ? 1 : 0);
        }

        void trace_ebwv_dcomp_gate_caller_hit(const analysis_context& c, const uint32_t tid, const size_t caller_index)
        {
            const auto gate_index = (caller_index / 3) + 1;
            c.win_emu->log.error(
                "[dcomp-gate-caller-trace] tid=%u caller_index=%zu (caller-of-gate%zu) REACHED the true entry of one of gate%zu's "
                "3 own static callers\n",
                tid, caller_index, gate_index, gate_index);
        }

        void trace_ebwv_dcomp_gate1_up1_retry_status_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto holder = emu.reg<uint32_t>(x86_register::ecx);
            const auto status = emu.reg<uint32_t>(x86_register::edi);

            uint8_t flag_c4 = 0;
            const bool flag_c4_ok = emu.try_read_memory(holder + 0xc4, &flag_c4, sizeof(flag_c4));
            int32_t retry_budget = 0;
            const bool retry_budget_ok = emu.try_read_memory(holder + 0x88, &retry_budget, sizeof(retry_budget));
            uint8_t flag_c5 = 0;
            const bool flag_c5_ok = emu.try_read_memory(holder + 0xc5, &flag_c5, sizeof(flag_c5));

            c.win_emu->log.error("[dcomp-gate-caller-trace] tid=%u caller-of-gate1's own retry gate: holder=0x%x "
                                 "status(holder[0xc0])=0x%x (SUCCEEDED=%d) holder[0xc4]=0x%x (read_ok=%d) "
                                 "holder[0x88] (retry_budget)=%d (read_ok=%d) holder[0xc5]=0x%x (read_ok=%d)\n",
                                 tid, holder, status, static_cast<int32_t>(status) >= 0 ? 1 : 0, flag_c4, flag_c4_ok ? 1 : 0, retry_budget,
                                 retry_budget_ok ? 1 : 0, flag_c5, flag_c5_ok ? 1 : 0);
        }

        void trace_ebwv_gate1_lazy_helper_entry_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto esp = emu.reg<uint32_t>(x86_register::esp);
            uint32_t return_addr = 0;
            const bool read_ok = emu.try_read_memory(esp, &return_addr, sizeof(return_addr));

            if (!read_ok || return_addr != static_cast<uint32_t>(g_ebwv_gate1_lazy_helper_return_va))
            {
                return;
            }

            g_ebwv_gate1_lazy_helper_window_active = true;
            g_ebwv_gate1_lazy_helper_window_tid = tid;
            c.win_emu->log.error("[gate1-lazy-helper-trace] tid=%u ENTERED the lazy-init/cache-lookup helper FROM gate1's own "
                                 "construction function, watching its syscalls until return\n",
                                 tid);
        }

        void trace_ebwv_gate1_lazy_helper_return_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto al_result = emu.reg<uint8_t>(x86_register::al);
            c.win_emu->log.error("[gate1-lazy-helper-trace] tid=%u RETURNED from the lazy-init/cache-lookup helper, al(result)=%u\n", tid,
                                 al_result);
            g_ebwv_gate1_lazy_helper_window_active = false;
        }

        void trace_ebwv_gate1_accessor_entry_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto this_ptr = emu.reg<uint32_t>(x86_register::ecx);

            uint32_t cached_state = 0;
            const bool read_ok = emu.try_read_memory(this_ptr + 0xa4, &cached_state, sizeof(cached_state));

            c.win_emu->log.error("[gate1-accessor-trace] tid=%u ENTERED gate1's own real accessor, this=0x%x this[0xa4] "
                                 "(cached-state)=0x%x (read_ok=%d)\n",
                                 tid, this_ptr, cached_state, read_ok ? 1 : 0);
        }

        void trace_ebwv_gate1_accessor_dcomp_invoke_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto resolved_fn = emu.reg<uint32_t>(x86_register::ecx);
            const auto arg0_rendering_device = emu.reg<uint32_t>(x86_register::ebx);
            const auto arg1_riid = emu.reg<uint32_t>(x86_register::edi);
            const auto arg2_ppv = emu.reg<uint32_t>(x86_register::esi);

            c.win_emu->log.error("[gate1-accessor-trace] tid=%u INVOKING the resolved DCompositionCreateDevice2=0x%x "
                                 "renderingDevice=0x%x riid=0x%x ppv=0x%x\n",
                                 tid, resolved_fn, arg0_rendering_device, arg1_riid, arg2_ppv);
        }

        void trace_ebwv_gate1_accessor_dcomp_invoke_return_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto hresult = emu.reg<uint32_t>(x86_register::eax);
            c.win_emu->log.error("[gate1-accessor-trace] tid=%u DCompositionCreateDevice2 returned HRESULT=0x%x\n", tid, hresult);
        }

        void trace_ebwv_gate1_accessor_result_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto holder = emu.reg<uint32_t>(x86_register::edi);
            const auto hresult = emu.reg<uint32_t>(x86_register::eax);

            uint32_t resolved_fn = 0;
            const bool resolved_fn_ok = g_ebwv_gate1_accessor_dcomp_resolved_fn_va != 0 &&
                                        emu.try_read_memory(g_ebwv_gate1_accessor_dcomp_resolved_fn_va, &resolved_fn, sizeof(resolved_fn));

            c.win_emu->log.error("[gate1-accessor-trace] tid=%u gate1's real accessor RETURNED, about to store holder[0xc0]: "
                                 "holder=0x%x hresult=0x%x resolved_dcomp_fn=0x%x (read_ok=%d)\n",
                                 tid, holder, hresult, resolved_fn, resolved_fn_ok ? 1 : 0);
        }

        void trace_start_watching_once_hit(const analysis_context& c, const uint32_t tid, const size_t caller_index)
        {
            auto& emu = c.win_emu->emu();
            const auto ecx = emu.reg<uint32_t>(x86_register::ecx);
            const auto esp = emu.reg<uint32_t>(x86_register::esp);

            std::array<uint32_t, 3> args{};
            const bool read_ok = emu.try_read_memory(esp, args.data(), sizeof(args));

            c.win_emu->log.error(
                "[start-watching-once-trace] tid=%u caller_index=%zu this=0x%x arg0=0x%x arg1=0x%x arg2=0x%x (read_ok=%d)\n", tid,
                caller_index, ecx, args[0], args[1], args[2], read_ok ? 1 : 0);
        }

        uint64_t g_tpp_worker_wait_return_va = 0;
        uint64_t g_tpp_worker_key_check_va = 0;
        uint64_t g_tpp_worker_ebx_loaded_va = 0;
        uint64_t g_tpp_worker_generic_call_va = 0;
        uint64_t g_tpp_worker_no_local_work_helper_return_va = 0;
        uint64_t g_tpp_worker_release_or_loop_va = 0;
        uint64_t g_tpp_wait_callback_loaded_va = 0;
        uint64_t g_tp_wait_gate_test_va = 0;
        uint64_t g_tp_wait_gate_rearm_taken_va = 0;
        uint64_t g_tp_alloc_wait_zero_check_va = 0;
        uint64_t g_tpsetwaitex_entry_va = 0;
        uint64_t g_tpsetwaitex_finalize_clear_va = 0;
        uint64_t g_tp_wait_legacy_bridge_callback_loaded_va = 0;
        uint64_t g_ebwv_image_base = 0;
        uint64_t g_ebwv_post_task_call_va = 0;
        uint64_t g_ebwv_post_task_virtual_call_va = 0;
        uint64_t g_ebwv_signaler_delegate_dispatch_va = 0;
        uint64_t g_ebwv_env_arg_validate_va = 0;
        uint64_t g_ebwv_env_completion_invoke_va = 0;
        uint64_t g_ebwv_env_getoverlappedresult_call_va = 0;
        uint64_t g_ebwv_env_getoverlappedresult_return_va = 0;
        uint64_t g_ebwv_env_outcome_invoke_arg_va = 0;

        bool g_ipcz_connect_watches_armed = false;
        uint64_t g_ipcz_connect_node_wrapper_va = 0;
        uint64_t g_ipcz_connect_node_real_body_va = 0;
        uint64_t g_ipcz_node_connector_for_referrer_connect_va = 0;
        uint64_t g_ipcz_node_connector_for_referrer_no_broker_link_branch_va = 0;
        uint64_t g_ipcz_refer_non_broker_va = 0;
        uint64_t g_ipcz_connect_node_internal_caller_va = 0;
        uint64_t g_ipcz_connect_node_internal_caller_equality_check_taken_va = 0;
        uint64_t g_ipcz_connect_node_internal_caller_equality_check_not_taken_va = 0;
        uint64_t g_ipcz_connect_node_internal_caller_site_flush_real_va = 0;
        uint64_t g_ipcz_connect_node_internal_caller_site_flush_empty_va = 0;
        uint64_t g_ipcz_connect_node_mutual_recursion_entry_va = 0;
        uint64_t g_ipcz_connect_node_mutual_recursion_site_1_va = 0;
        uint64_t g_ipcz_connect_node_mutual_recursion_site_2_va = 0;
        uint64_t g_ipcz_channel_win_factory_va = 0;
        uint64_t g_ipcz_connect_wrapper_ctor_va = 0;
        uint64_t g_ipcz_connect_wrapper_dtor_va = 0;
        uint64_t g_ipcz_connect_wrapper_scalar_deleting_dtor_va = 0;

        void arm_ipcz_connect_watches(const analysis_context& c)
        {
            if (g_ipcz_connect_watches_armed)
            {
                return;
            }

            const auto* ebwv = c.win_emu->mod_manager.find_by_name("embeddedbrowserwebview.dll");
            if (!ebwv)
            {
                return;
            }

            g_ipcz_connect_watches_armed = true;
            g_ipcz_connect_node_wrapper_va = ebwv->image_base + IPCZ_CONNECT_NODE_WRAPPER_RVA;
            g_ipcz_connect_node_real_body_va = ebwv->image_base + IPCZ_CONNECT_NODE_REAL_BODY_RVA;
            g_ipcz_node_connector_for_referrer_connect_va = ebwv->image_base + IPCZ_NODE_CONNECTOR_FOR_REFERRER_CONNECT_RVA;
            g_ipcz_node_connector_for_referrer_no_broker_link_branch_va =
                ebwv->image_base + IPCZ_NODE_CONNECTOR_FOR_REFERRER_NO_BROKER_LINK_BRANCH_RVA;
            g_ipcz_refer_non_broker_va = ebwv->image_base + IPCZ_REFER_NON_BROKER_RVA;
            g_ipcz_connect_node_internal_caller_va = ebwv->image_base + IPCZ_CONNECT_NODE_INTERNAL_CALLER_RVA;
            g_ipcz_connect_node_internal_caller_equality_check_taken_va =
                ebwv->image_base + IPCZ_CONNECT_NODE_INTERNAL_CALLER_EQUALITY_CHECK_TAKEN_RVA;
            g_ipcz_connect_node_internal_caller_equality_check_not_taken_va =
                ebwv->image_base + IPCZ_CONNECT_NODE_INTERNAL_CALLER_EQUALITY_CHECK_NOT_TAKEN_RVA;
            g_ipcz_connect_node_internal_caller_site_flush_real_va =
                ebwv->image_base + IPCZ_CONNECT_NODE_INTERNAL_CALLER_SITE_FLUSH_REAL_RVA;
            g_ipcz_connect_node_internal_caller_site_flush_empty_va =
                ebwv->image_base + IPCZ_CONNECT_NODE_INTERNAL_CALLER_SITE_FLUSH_EMPTY_RVA;
            g_ipcz_connect_node_mutual_recursion_entry_va = ebwv->image_base + IPCZ_CONNECT_NODE_MUTUAL_RECURSION_ENTRY_RVA;
            g_ipcz_connect_node_mutual_recursion_site_1_va = ebwv->image_base + IPCZ_CONNECT_NODE_MUTUAL_RECURSION_SITE_1_RVA;
            g_ipcz_connect_node_mutual_recursion_site_2_va = ebwv->image_base + IPCZ_CONNECT_NODE_MUTUAL_RECURSION_SITE_2_RVA;
            g_ipcz_channel_win_factory_va = ebwv->image_base + IPCZ_CHANNEL_WIN_FACTORY_RVA;
            g_ipcz_connect_wrapper_ctor_va = ebwv->image_base + IPCZ_CONNECT_WRAPPER_CTOR_RVA;
            g_ipcz_connect_wrapper_dtor_va = ebwv->image_base + IPCZ_CONNECT_WRAPPER_DTOR_RVA;
            g_ipcz_connect_wrapper_scalar_deleting_dtor_va = ebwv->image_base + IPCZ_CONNECT_WRAPPER_SCALAR_DELETING_DTOR_RVA;

            c.win_emu->log.error("[ipcz-connect-trace] armed against embeddedbrowserwebview.dll image_base=0x%llx: "
                                 "ConnectNode_wrapper=0x%llx ConnectNode_real_body=0x%llx NodeConnectorForReferrer::Connect=0x%llx "
                                 "NodeConnectorForReferrer::Connect(no-broker-link-branch)=0x%llx NodeLink::ReferNonBroker=0x%llx "
                                 "ConnectNode_internal_caller=0x%llx ConnectNode_internal_caller(gate-taken)=0x%llx "
                                 "ConnectNode_internal_caller(gate-not-taken)=0x%llx flush_site_real=0x%llx flush_site_empty=0x%llx "
                                 "mutual_recursion_entry=0x%llx mutual_recursion_site_1=0x%llx mutual_recursion_site_2=0x%llx "
                                 "ChannelWin_factory=0x%llx ConnectWrapper_ctor=0x%llx ConnectWrapper_dtor=0x%llx "
                                 "ConnectWrapper_scalar_deleting_dtor=0x%llx\n",
                                 static_cast<unsigned long long>(ebwv->image_base),
                                 static_cast<unsigned long long>(g_ipcz_connect_node_wrapper_va),
                                 static_cast<unsigned long long>(g_ipcz_connect_node_real_body_va),
                                 static_cast<unsigned long long>(g_ipcz_node_connector_for_referrer_connect_va),
                                 static_cast<unsigned long long>(g_ipcz_node_connector_for_referrer_no_broker_link_branch_va),
                                 static_cast<unsigned long long>(g_ipcz_refer_non_broker_va),
                                 static_cast<unsigned long long>(g_ipcz_connect_node_internal_caller_va),
                                 static_cast<unsigned long long>(g_ipcz_connect_node_internal_caller_equality_check_taken_va),
                                 static_cast<unsigned long long>(g_ipcz_connect_node_internal_caller_equality_check_not_taken_va),
                                 static_cast<unsigned long long>(g_ipcz_connect_node_internal_caller_site_flush_real_va),
                                 static_cast<unsigned long long>(g_ipcz_connect_node_internal_caller_site_flush_empty_va),
                                 static_cast<unsigned long long>(g_ipcz_connect_node_mutual_recursion_entry_va),
                                 static_cast<unsigned long long>(g_ipcz_connect_node_mutual_recursion_site_1_va),
                                 static_cast<unsigned long long>(g_ipcz_connect_node_mutual_recursion_site_2_va),
                                 static_cast<unsigned long long>(g_ipcz_channel_win_factory_va),
                                 static_cast<unsigned long long>(g_ipcz_connect_wrapper_ctor_va),
                                 static_cast<unsigned long long>(g_ipcz_connect_wrapper_dtor_va),
                                 static_cast<unsigned long long>(g_ipcz_connect_wrapper_scalar_deleting_dtor_va));
        }

        void trace_ipcz_connect_hit(const analysis_context& c, const uint32_t tid, const char* label)
        {
            c.win_emu->log.error("[ipcz-connect-trace] tid=%u REACHED %s\n", tid, label);
        }

        void arm_tpp_worker_thread_watches(const analysis_context& c)
        {
            if (g_tpp_worker_wait_return_va != 0)
            {
                return;
            }

            const auto* ntdll32 = c.win_emu->mod_manager.wow64_modules_.ntdll32;
            if (!ntdll32)
            {
                return;
            }

            g_tpp_worker_wait_return_va = ntdll32->image_base + TPP_WORKER_WAIT_RETURN_RVA;
            g_tpp_worker_key_check_va = ntdll32->image_base + TPP_WORKER_KEY_CHECK_RVA;
            g_tpp_worker_ebx_loaded_va = ntdll32->image_base + TPP_WORKER_EBX_LOADED_RVA;
            g_tpp_worker_generic_call_va = ntdll32->image_base + TPP_WORKER_GENERIC_CALL_RVA;
            g_tpp_worker_no_local_work_helper_return_va = ntdll32->image_base + TPP_WORKER_NO_LOCAL_WORK_HELPER_RETURN_RVA;
            g_tpp_worker_release_or_loop_va = ntdll32->image_base + TPP_WORKER_RELEASE_OR_LOOP_RVA;
            g_tpp_wait_callback_loaded_va = ntdll32->image_base + TPP_WAIT_CALLBACK_LOADED_RVA;
            g_tp_wait_gate_test_va = ntdll32->image_base + TP_WAIT_GATE_TEST_RVA;
            g_tp_wait_gate_rearm_taken_va = ntdll32->image_base + TP_WAIT_GATE_REARM_TAKEN_RVA;
            g_tp_alloc_wait_zero_check_va = ntdll32->image_base + TP_ALLOC_WAIT_ZERO_CHECK_RVA;
            g_tpsetwaitex_entry_va = ntdll32->image_base + TPSETWAITEX_ENTRY_RVA;
            g_tpsetwaitex_finalize_clear_va = ntdll32->image_base + TPSETWAITEX_FINALIZE_CLEAR_RVA;
            g_tp_wait_legacy_bridge_callback_loaded_va = ntdll32->image_base + TP_WAIT_LEGACY_BRIDGE_CALLBACK_LOADED_RVA;

            c.win_emu->log.error(
                "[tpp-worker-trace] armed against ntdll.dll (32-bit) image_base=0x%llx: wait_return=0x%llx "
                "key_check=0x%llx ebx_loaded=0x%llx generic_call=0x%llx helper_return=0x%llx release_or_loop=0x%llx "
                "wait_callback_loaded=0x%llx\n",
                static_cast<unsigned long long>(ntdll32->image_base), static_cast<unsigned long long>(g_tpp_worker_wait_return_va),
                static_cast<unsigned long long>(g_tpp_worker_key_check_va), static_cast<unsigned long long>(g_tpp_worker_ebx_loaded_va),
                static_cast<unsigned long long>(g_tpp_worker_generic_call_va),
                static_cast<unsigned long long>(g_tpp_worker_no_local_work_helper_return_va),
                static_cast<unsigned long long>(g_tpp_worker_release_or_loop_va),
                static_cast<unsigned long long>(g_tpp_wait_callback_loaded_va));

            c.win_emu->log.error(
                "[tp-wait-gate-trace] armed: gate_test=0x%llx rearm_taken=0x%llx alloc_zero_check=0x%llx "
                "tpsetwaitex_entry=0x%llx tpsetwaitex_finalize_clear=0x%llx legacy_bridge_callback_loaded=0x%llx\n",
                static_cast<unsigned long long>(g_tp_wait_gate_test_va), static_cast<unsigned long long>(g_tp_wait_gate_rearm_taken_va),
                static_cast<unsigned long long>(g_tp_alloc_wait_zero_check_va), static_cast<unsigned long long>(g_tpsetwaitex_entry_va),
                static_cast<unsigned long long>(g_tpsetwaitex_finalize_clear_va),
                static_cast<unsigned long long>(g_tp_wait_legacy_bridge_callback_loaded_va));
        }

        void trace_tpp_worker_wait_return_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto status = emu.reg<uint32_t>(x86_register::eax);
            const auto ebp = emu.reg<uint32_t>(x86_register::ebp);

            uint32_t mini_packets_ptr = 0;
            emu.try_read_memory(ebp - 0x11c, &mini_packets_ptr, sizeof(mini_packets_ptr));

            uint32_t packet[4]{};
            const bool read_ok = mini_packets_ptr != 0 && emu.try_read_memory(mini_packets_ptr, &packet, sizeof(packet));

            c.win_emu->log.error(
                "[tpp-worker-trace] tid=%u WAIT_RETURN status=0x%x mini_packets_ptr=0x%x key_context=0x%x apc_context=0x%x "
                "io_status=0x%x io_information=0x%x (read_ok=%d)\n",
                tid, status, mini_packets_ptr, packet[0], packet[1], packet[2], packet[3], read_ok ? 1 : 0);
        }

        void trace_tpp_worker_key_check_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto esi = emu.reg<uint32_t>(x86_register::esi);

            const auto* mod_name = c.win_emu->mod_manager.find_name(esi);
            const auto* mod = c.win_emu->mod_manager.find_by_address(esi);
            const auto offset = mod ? esi - mod->image_base : esi;

            c.win_emu->log.error("[tpp-worker-trace] tid=%u KEY_CHECK esi(key_context)=0x%x -> %s (%s+0x%llx)\n", tid, esi,
                                 esi == 0 ? "TAKING ESI==0 (no-local-work-helper) PATH" : "TAKING ESI!=0 (fast dispatch) PATH", mod_name,
                                 static_cast<unsigned long long>(offset));
        }

        void trace_tpp_worker_ebx_loaded_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto ebx = emu.reg<uint32_t>(x86_register::ebx);
            const auto esi = emu.reg<uint32_t>(x86_register::esi);

            const char* classification = "UNKNOWN (generic/CFG-checked call)";
            if (ebx == 0x4b2bc180)
            {
                classification = "SENTINEL #1 (direct call, no CFG check)";
            }
            else if (ebx == 0x4b2bae40)
            {
                classification = "SENTINEL #2 (direct call, no CFG check)";
            }
            else if (ebx == 0x4b2b8fc0)
            {
                classification = "SENTINEL #3 (direct call, no CFG check)";
            }

            c.win_emu->log.error("[tpp-worker-trace] tid=%u EBX_LOADED tp_object(esi)=0x%x callback_ptr(ebx)=0x%x -> %s\n", tid, esi, ebx,
                                 classification);
        }

        void trace_tpp_worker_generic_call_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto ebx = emu.reg<uint32_t>(x86_register::ebx);
            const auto esi = emu.reg<uint32_t>(x86_register::esi);
            const auto esp = emu.reg<uint32_t>(x86_register::esp);

            uint32_t args[4]{};
            emu.try_read_memory(esp, &args, sizeof(args));

            const auto* mod_name = c.win_emu->mod_manager.find_name(ebx);
            const auto* mod = c.win_emu->mod_manager.find_by_address(ebx);
            const auto offset = mod ? ebx - mod->image_base : ebx;

            c.win_emu->log.error("[tpp-worker-trace] tid=%u GENERIC_CALLBACK_DISPATCH target=0x%x (%s+0x%llx) tp_object(esi)=0x%x "
                                 "args=[0x%x,0x%x,0x%x,0x%x]\n",
                                 tid, ebx, mod_name, static_cast<unsigned long long>(offset), esi, args[0], args[1], args[2], args[3]);
        }

        void trace_tpp_worker_no_local_work_helper_return_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto eax = emu.reg<uint32_t>(x86_register::eax);

            c.win_emu->log.error("[tpp-worker-trace] tid=%u NO_LOCAL_WORK_HELPER_RETURN eax=0x%x -> %s\n", tid, eax,
                                 eax == 0 ? "NO WORK FOUND, taking RELEASE_OR_LOOP branch" : "WORK FOUND, continuing dispatch");
        }

        void trace_tpp_worker_release_or_loop_hit(const analysis_context& c, const uint32_t tid)
        {
            c.win_emu->log.error("[tpp-worker-trace] tid=%u RELEASE_OR_LOOP reached (no callback invoked this iteration)\n", tid);
        }

        void trace_tpp_wait_callback_loaded_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto esi = emu.reg<uint32_t>(x86_register::esi);
            const auto edi = emu.reg<uint32_t>(x86_register::edi);

            const auto* mod_name = c.win_emu->mod_manager.find_name(esi);
            const auto* mod = c.win_emu->mod_manager.find_by_address(esi);
            const auto offset = mod ? esi - mod->image_base : esi;

            c.win_emu->log.error(
                "[tpp-worker-trace] tid=%u REAL_WAIT_CALLBACK tp_wait_object=0x%x callback=0x%x (%s+0x%llx) about to be invoked\n", tid,
                edi, esi, mod_name, static_cast<unsigned long long>(offset));
        }

        void trace_tp_wait_gate_test_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto edi = emu.reg<uint32_t>(x86_register::edi);
            const auto ecx = emu.reg<uint32_t>(x86_register::ecx);
            const auto cl = static_cast<uint8_t>(ecx & 0xff);

            c.win_emu->log.error("[tp-wait-gate-trace] tid=%u GATE_TEST tp_wait_object=0x%x gate_byte(+0x124)=0x%x bit0=%d bit2=%d -> %s\n",
                                 tid, edi, cl, cl & 1, (cl >> 2) & 1,
                                 (cl & 1) ? "REARM (real callback SKIPPED)" : "PROCEED (real callback invoked)");
        }

        void trace_tp_wait_gate_rearm_taken_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto edi = emu.reg<uint32_t>(x86_register::edi);

            c.win_emu->log.error("[tp-wait-gate-trace] tid=%u REARM_TAKEN tp_wait_object=0x%x (re-arming via TpSetWaitEx-equivalent "
                                 "instead of invoking the real callback this cycle)\n",
                                 tid, edi);
        }

        void trace_tp_alloc_wait_zero_check_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto esi = emu.reg<uint32_t>(x86_register::esi);

            if (esi == 0)
            {
                c.win_emu->log.error("[tp-wait-gate-trace] tid=%u TP_ALLOC_WAIT_ZERO_CHECK allocation failed\n", tid);
                return;
            }

            uint8_t gate_byte = 0;
            const bool read_ok = emu.try_read_memory(esi + 0x124, &gate_byte, sizeof(gate_byte));

            c.win_emu->log.error("[tp-wait-gate-trace] tid=%u TP_ALLOC_WAIT_ZERO_CHECK tp_wait_object=0x%x gate_byte(+0x124)=0x%x "
                                 "(read_ok=%d) -> %s\n",
                                 tid, esi, gate_byte, read_ok ? 1 : 0,
                                 gate_byte == 0 ? "ZEROED as expected" : "NON-ZERO at allocation time");
        }

        void trace_tpsetwaitex_entry_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto edi = emu.reg<uint32_t>(x86_register::edi);

            c.win_emu->log.error("[tp-wait-gate-trace] tid=%u TPSETWAITEX_ENTRY tp_wait_object=0x%x\n", tid, edi);
        }

        void trace_tpsetwaitex_finalize_clear_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto esi = emu.reg<uint32_t>(x86_register::esi);

            uint8_t gate_byte_before = 0;
            const bool read_ok = esi != 0 && emu.try_read_memory(esi + 0x124, &gate_byte_before, sizeof(gate_byte_before));

            c.win_emu->log.error(
                "[tp-wait-gate-trace] tid=%u TPSETWAITEX_FINALIZE_CLEAR tp_wait_object=0x%x gate_byte(+0x124)_before_clear=0x%x "
                "(read_ok=%d)\n",
                tid, esi, gate_byte_before, read_ok ? 1 : 0);
        }

        void trace_tp_wait_legacy_bridge_callback_loaded_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto esi = emu.reg<uint32_t>(x86_register::esi);

            const auto* mod_name = c.win_emu->mod_manager.find_name(esi);
            const auto* mod = c.win_emu->mod_manager.find_by_address(esi);
            const auto offset = mod ? esi - mod->image_base : esi;

            c.win_emu->log.error("[tp-wait-gate-trace] tid=%u LEGACY_BRIDGE_CALLBACK_LOADED target=0x%x (%s+0x%llx) about to be invoked\n",
                                 tid, esi, mod_name, static_cast<unsigned long long>(offset));

            if (g_ebwv_image_base == 0 && offset == EBWV_CALLBACK_ENTRY_RVA)
            {
                g_ebwv_image_base = esi - EBWV_CALLBACK_ENTRY_RVA;
                g_ebwv_post_task_call_va = g_ebwv_image_base + EBWV_POST_TASK_CALL_RVA;
                g_ebwv_post_task_virtual_call_va = g_ebwv_image_base + EBWV_POST_TASK_VIRTUAL_CALL_RVA;
                g_ebwv_signaler_delegate_dispatch_va = g_ebwv_image_base + EBWV_SIGNALER_DELEGATE_DISPATCH_RVA;
                g_ebwv_env_arg_validate_va = g_ebwv_image_base + EBWV_ENV_ARG_VALIDATE_RVA;
                g_ebwv_env_completion_invoke_va = g_ebwv_image_base + EBWV_ENV_COMPLETION_INVOKE_RVA;
                g_ebwv_env_getoverlappedresult_call_va = g_ebwv_image_base + EBWV_ENV_GETOVERLAPPEDRESULT_CALL_RVA;
                g_ebwv_env_getoverlappedresult_return_va = g_ebwv_image_base + EBWV_ENV_GETOVERLAPPEDRESULT_RETURN_RVA;
                g_ebwv_env_outcome_invoke_arg_va = g_ebwv_image_base + EBWV_ENV_OUTCOME_INVOKE_ARG_RVA;

                c.win_emu->log.error("[ebwv-posttask-trace] armed against %s image_base=0x%llx: post_task_call=0x%llx "
                                     "post_task_virtual_call=0x%llx signaler_delegate_dispatch=0x%llx env_arg_validate=0x%llx "
                                     "env_completion_invoke=0x%llx env_getoverlappedresult_call=0x%llx "
                                     "env_getoverlappedresult_return=0x%llx env_outcome_invoke_arg=0x%llx\n",
                                     mod_name, static_cast<unsigned long long>(g_ebwv_image_base),
                                     static_cast<unsigned long long>(g_ebwv_post_task_call_va),
                                     static_cast<unsigned long long>(g_ebwv_post_task_virtual_call_va),
                                     static_cast<unsigned long long>(g_ebwv_signaler_delegate_dispatch_va),
                                     static_cast<unsigned long long>(g_ebwv_env_arg_validate_va),
                                     static_cast<unsigned long long>(g_ebwv_env_completion_invoke_va),
                                     static_cast<unsigned long long>(g_ebwv_env_getoverlappedresult_call_va),
                                     static_cast<unsigned long long>(g_ebwv_env_getoverlappedresult_return_va),
                                     static_cast<unsigned long long>(g_ebwv_env_outcome_invoke_arg_va));
            }
        }

        // Shared by both hooks below: dumps the first 8 dwords of a BindStateBase-shaped closure
        // (see project_solidworks_bringup.md's cycle-77 finding for the confirmed layout:
        // [+0]=refcount, [+4]=polymorphic_invoke_, [+8]=destructor_, [+0xc]=is_cancelled_) so the
        // closure's real, dynamically-resolved invoke target is visible without a second pass.
        void dump_bind_state_words(const analysis_context& c, const uint32_t tid, const uint32_t closure)
        {
            auto& emu = c.win_emu->emu();

            std::array<uint32_t, 8> bind_state_words{};
            const bool bind_state_read_ok = closure != 0 && emu.try_read_memory(closure, bind_state_words.data(), sizeof(bind_state_words));

            for (size_t i = 0; bind_state_read_ok && i < bind_state_words.size(); ++i)
            {
                const auto word = bind_state_words.at(i);
                const auto* word_mod_name = c.win_emu->mod_manager.find_name(word);
                const auto* word_mod = c.win_emu->mod_manager.find_by_address(word);
                const auto word_offset = word_mod ? word - word_mod->image_base : word;

                c.win_emu->log.error("[ebwv-posttask-trace] tid=%u   bind_state[+0x%zx]=0x%x (%s+0x%llx)\n", tid, i * sizeof(uint32_t),
                                     word, word_mod_name, static_cast<unsigned long long>(word_offset));
            }
        }

        void trace_ebwv_post_task_call_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto esi = emu.reg<uint32_t>(x86_register::esi);
            const auto edi = emu.reg<uint32_t>(x86_register::edi);
            const auto eax = emu.reg<uint32_t>(x86_register::eax);

            const auto* mod_name = c.win_emu->mod_manager.find_name(esi);
            const auto* mod = c.win_emu->mod_manager.find_by_address(esi);
            const auto offset = mod ? esi - mod->image_base : esi;

            c.win_emu->log.error(
                "[ebwv-posttask-trace] tid=%u POST_TASK_CALL task_runner(esi/ecx)=0x%x (%s+0x%llx) context_plus_4(edi)=0x%x "
                "extracted_value(eax)=0x%x\n",
                tid, esi, mod_name, static_cast<unsigned long long>(offset), edi, eax);

            // `eax` is the closure's own single BindStateBase*-shaped field (the "two-word object"
            // #302 described is really one scoped_refptr-shaped field being extracted, plus an
            // unrelated, always-null local temp whose destructor call is just ordinary RAII
            // cleanup -- see EBWV_SIGNALER_DELEGATE_DISPATCH_RVA's own comment above for the layout this dump
            // confirmed). Dumping the first few dwords of the object it points to identifies that
            // layout live.
            dump_bind_state_words(c, tid, eax);
        }

        // `ebx` is set once at the PostTask wrapper's own entry (embeddedbrowserwebview.dll+0x243ed0:
        // `mov ebx, dword ptr [ebp+8]`, the first stack argument at ANY call site into this shared
        // wrapper, not just EBWV_POST_TASK_CALL_RVA's own specific one) and is never overwritten before
        // the virtual dispatch at +0x243f2f -- confirmed by re-disassembling the wrapper's full body this
        // cycle (project_solidworks_bringup.md #329). It therefore holds the SAME BindStateBase-shaped
        // closure pointer trace_ebwv_post_task_call_hit already knows how to dump, generalized to every
        // call site that ever reaches this generic dispatcher (the 23x/4-thread `target=+0x2496d0`
        // firings #328 found included) rather than only the one specific environment-creation call site.
        void trace_ebwv_post_task_virtual_call_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto edi = emu.reg<uint32_t>(x86_register::edi);
            const auto ebx = emu.reg<uint32_t>(x86_register::ebx);

            const auto* mod_name = c.win_emu->mod_manager.find_name(edi);
            const auto* mod = c.win_emu->mod_manager.find_by_address(edi);
            const auto offset = mod ? edi - mod->image_base : edi;

            c.win_emu->log.error(
                "[ebwv-posttask-trace] tid=%u POST_TASK_VIRTUAL_CALL target=0x%x (%s+0x%llx) closure(ebx)=0x%x about to be invoked\n", tid,
                edi, mod_name, static_cast<unsigned long long>(offset), ebx);

            dump_bind_state_words(c, tid, ebx);
        }

        void trace_ebwv_signaler_delegate_dispatch_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto esi = emu.reg<uint32_t>(x86_register::esi);
            const auto ebx = emu.reg<uint32_t>(x86_register::ebx);
            const auto edi = emu.reg<uint32_t>(x86_register::edi);

            const auto* mod_name = c.win_emu->mod_manager.find_name(edi);
            const auto* mod = c.win_emu->mod_manager.find_by_address(edi);
            const auto offset = mod ? edi - mod->image_base : edi;

            c.win_emu->log.error("[ebwv-posttask-trace] tid=%u SIGNALER_DELEGATE_DISPATCH delegate(esi)=0x%x arg(ebx)=0x%x target=0x%x "
                                 "(%s+0x%llx) about to be invoked\n",
                                 tid, esi, ebx, edi, mod_name, static_cast<unsigned long long>(offset));
        }

        void trace_ebwv_env_arg_validate_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto eax = emu.reg<uint32_t>(x86_register::eax);
            const auto esi = emu.reg<uint32_t>(x86_register::esi);

            uint32_t expected = 0;
            const bool read_ok = emu.try_read_memory(esi + 0x1c, &expected, sizeof(expected));

            c.win_emu->log.error(
                "[ebwv-posttask-trace] tid=%u ENV_ARG_VALIDATE arg(eax)=0x%x expected(this+0x1c)=0x%x (read_ok=%d) -> %s\n", tid, eax,
                expected, read_ok ? 1 : 0, (read_ok && eax == expected) ? "MATCH (happy path continues)" : "MISMATCH (fastfail path)");
        }

        void trace_ebwv_env_completion_invoke_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto ecx = emu.reg<uint32_t>(x86_register::ecx);
            const auto ebx = emu.reg<uint32_t>(x86_register::ebx);
            const auto esi = emu.reg<uint32_t>(x86_register::esi);
            const auto esp = emu.reg<uint32_t>(x86_register::esp);

            const auto* mod_name = c.win_emu->mod_manager.find_name(ecx);
            const auto* mod = c.win_emu->mod_manager.find_by_address(ecx);
            const auto offset = mod ? ecx - mod->image_base : ecx;

            c.win_emu->log.error("[ebwv-posttask-trace] tid=%u ENV_COMPLETION_INVOKE target=0x%x (%s+0x%llx) closure(ebx)=0x%x "
                                 "arg(esi)=0x%x esp=0x%x about to be invoked\n",
                                 tid, ecx, mod_name, static_cast<unsigned long long>(offset), ebx, esi, esp);
        }

        void trace_ebwv_env_getoverlappedresult_call_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto esi = emu.reg<uint32_t>(x86_register::esi);

            uint32_t handle = 0;
            const bool handle_read_ok = emu.try_read_memory(esi + 0x4, &handle, sizeof(handle));

            std::array<uint32_t, 5> overlapped{};
            const bool overlapped_read_ok = emu.try_read_memory(esi + 0x8, overlapped.data(), sizeof(overlapped));

            c.win_emu->log.error("[ebwv-posttask-trace] tid=%u ENV_GETOVERLAPPEDRESULT_CALL this=0x%x handle(this+4)=0x%x (read_ok=%d) "
                                 "overlapped(this+8)=[0x%x,0x%x,0x%x,0x%x,0x%x] (read_ok=%d)\n",
                                 tid, esi, handle, handle_read_ok ? 1 : 0, overlapped[0], overlapped[1], overlapped[2], overlapped[3],
                                 overlapped[4], overlapped_read_ok ? 1 : 0);
        }

        void trace_ebwv_env_getoverlappedresult_return_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto eax = emu.reg<uint32_t>(x86_register::eax);
            const auto ebp = emu.reg<uint32_t>(x86_register::ebp);

            uint32_t bytes_transferred = 0;
            const bool bytes_read_ok = emu.try_read_memory(ebp - 0x14, &bytes_transferred, sizeof(bytes_transferred));

            uint32_t last_error = 0;
            bool last_error_read_ok = false;
            auto& thread = c.win_emu->current_thread();
            if (thread.teb32.has_value())
            {
                const auto teb = thread.teb32->try_read();
                if (teb.has_value())
                {
                    last_error = teb->LastErrorValue;
                    last_error_read_ok = true;
                }
            }

            c.win_emu->log.error("[ebwv-posttask-trace] tid=%u ENV_GETOVERLAPPEDRESULT_RETURN result(eax)=0x%x (%s) bytes_transferred=0x%x "
                                 "(read_ok=%d) last_error=%u (read_ok=%d)\n",
                                 tid, eax, eax != 0 ? "TRUE/success" : "FALSE/failure", bytes_transferred, bytes_read_ok ? 1 : 0,
                                 last_error, last_error_read_ok ? 1 : 0);
        }

        void trace_ebwv_env_outcome_invoke_arg_hit(const analysis_context& c, const uint32_t tid)
        {
            auto& emu = c.win_emu->emu();
            const auto esp = emu.reg<uint32_t>(x86_register::esp);
            const auto ecx = emu.reg<uint32_t>(x86_register::ecx);

            std::array<uint32_t, 2> outcome{};
            const bool outcome_read_ok = emu.try_read_memory(esp, outcome.data(), sizeof(outcome));

            c.win_emu->log.error("[ebwv-posttask-trace] tid=%u ENV_OUTCOME_INVOKE_ARG closure(ecx)=0x%x outcome@esp=[0x%x,0x%x] "
                                 "(read_ok=%d) -> %s\n",
                                 tid, ecx, outcome[0], outcome[1], outcome_read_ok ? 1 : 0,
                                 (outcome[0] == 0 && outcome[1] == 0) ? "ZEROED (failure-path outcome)"
                                                                      : "NON-ZERO (success-path outcome)");
        }

        void trace_worker_factory_thread(const analysis_context& c, const uint32_t tid, const handle io_completion_handle,
                                         const uint64_t start_routine)
        {
            if (g_thread_activity_extra_tids.insert(tid).second)
            {
                const auto* start_mod_name = c.win_emu->mod_manager.find_name(start_routine);
                const auto* start_mod = c.win_emu->mod_manager.find_by_address(start_routine);
                const auto start_offset = start_mod ? start_routine - start_mod->image_base : start_routine;

                c.win_emu->log.error(
                    "[thread-activity-trace] extending trace to worker-factory thread tid=%u (factory io_completion=0x%llx) "
                    "start_routine=0x%llx (%s+0x%llx)\n",
                    tid, static_cast<unsigned long long>(io_completion_handle.bits), static_cast<unsigned long long>(start_routine),
                    start_mod_name, static_cast<unsigned long long>(start_offset));

                arm_tpp_worker_thread_watches(c);
            }
        }

        void trace_worker_factory_thread_if_new(const analysis_context& c, const handle thread_handle, const uint32_t tid)
        {
            for (const auto& [factory_id, factory] : c.win_emu->process.worker_factories)
            {
                (void)factory_id;
                if (std::ranges::find(factory.worker_threads, thread_handle) != factory.worker_threads.end())
                {
                    trace_worker_factory_thread(c, tid, factory.io_completion_handle, factory.start_routine);
                    return;
                }
            }
        }

        // Dynamically extends the trace to whichever thread owns a real `Chrome_WidgetWin_0` window
        // (the Chromium/WebView2 content-widget class, see project_solidworks_bringup.md #327) --
        // deliberately not a hardcoded tid, since #327 found tids are not stable run-to-run. Scanning
        // proc.windows every event-pump tick (rather than hooking NtUserCreateWindowEx itself) keeps
        // this entirely within windows-analyzer, matching this file's existing layering.
        void trace_widget_window_threads(const analysis_context& c)
        {
            if (!g_thread_activity_armed)
            {
                return;
            }

            for (const auto& win : c.win_emu->process.windows | std::views::values)
            {
                if (win.class_name != u"Chrome_WidgetWin_0")
                {
                    continue;
                }

                if (g_thread_activity_extra_tids.insert(win.thread_id).second)
                {
                    c.win_emu->log.error(
                        "[thread-activity-trace] extending trace to Chrome_WidgetWin_0-owning thread tid=%u (hwnd=0x%llx)\n", win.thread_id,
                        static_cast<unsigned long long>(win.handle));
                }
            }
        }

        // Called once at arming time (see the FSCTL_PIPE_LISTEN check below): a worker factory's thread
        // can be created before the pipe listen that arms this whole tracer, so relying solely on
        // handle_thread_create's own forward-looking check would miss it -- this catches every
        // worker-factory thread that already exists by the time arming happens.
        void trace_all_existing_worker_factory_threads(const analysis_context& c)
        {
            for (const auto& [factory_id, factory] : c.win_emu->process.worker_factories)
            {
                (void)factory_id;
                for (const auto thread_handle : factory.worker_threads)
                {
                    if (const auto* thread = c.win_emu->process.threads.get(thread_handle))
                    {
                        trace_worker_factory_thread(c, thread->id, factory.io_completion_handle, factory.start_routine);
                    }
                }
            }
        }

        // Live localization of the missing DispatchMessage call site for sldim.exe's WM_COMMAND
        // relay (see project_solidworks_bringup.md #284): g_sldim_dispatch_watch_va is armed by
        // handle_NtUserGetMessage (src/windows-emulator/syscalls/user.cpp) at the guest's real
        // post-syscall return RIP the first time it dequeues WM_COMMAND/0x464 on tid 8. Once
        // execution actually reaches that address, a handful of instructions are disassembled
        // forward; the first indirect call/jmp found there is armed as a second watch so its real
        // resolved target can be read live off the CPU once execution reaches it.
        uint64_t g_sldim_dispatch_indirect_call_va = 0;
        uint32_t g_sldim_dispatch_indirect_call_hits = 0;
        bool g_sldim_dispatch_watch_for_exe_entry = false;
        bool g_sldim_dispatch_indirect_is_jmp = false;
        uint32_t g_sldim_dispatch_hop_count = 0;
        // 300 was enough to reach and disassemble the real message-dispatch logic in every prior
        // cycle's chase target, but following INTO user32.dll+0x27ac0 (this cycle's own target, see
        // project_solidworks_bringup.md #289) reveals a genuine, real linear scan of the per-thread
        // class-info table (a ~0x248-byte-stride walk comparing a fixed class atom) before it ever
        // reaches the actual WNDPROC-invoking call -- 300 hops is exhausted mid-scan every time.
        constexpr uint32_t SLDIM_DISPATCH_MAX_HOPS = 20000;

        // user32.dll's real internal message-delivery helper that DispatchMessageW's own internal
        // worker calls to invoke a window's WNDPROC (`mov ecx,[esi+0xe0]; call user32.dll+0x27ac0`,
        // live-disassembled in project_solidworks_bringup.md #288 point 8). The generic chase below
        // always skips over a direct CALL like any other; this one is followed INTO instead. Live
        // disassembly this cycle (see #289) found 0x27ac0's own real body, after its class-info-table
        // scan, ends in `mov ecx,esi; call user32.dll+0x47dac` then
        // `push [ebp+0x18]; push [ebp+0x14]; push edi; push ebx; push esi; call user32.dll+0x47c78` --
        // 0x47c78's 5-argument shape (hwnd/msg/wParam/lParam-sized) is the real remaining candidate
        // for the actual per-window WNDPROC-invoking call, so it is followed into as well. Once
        // inside this chain, any indirect call reached is also followed live (register-resolved),
        // hop by hop, until execution genuinely lands inside sldim.exe's own image -- the real WNDPROC.
        constexpr uint64_t WNDPROC_INVOKE_RVA = 0x27ac0;
        constexpr uint64_t WNDPROC_INVOKE_CALLEE_RVA = 0x47c78;

        // sldim.exe's own real, live-confirmed WNDPROC (found this cycle by following the chain
        // above): `CWnd::FromHandlePermanent`-style handle-map lookup (`call sldim.exe+0x2257`), a
        // `pWnd->m_hWnd == hWnd` self-consistency check (`cmp [eax+0x20],esi`), then
        // `push lParam,wParam,msg,hWnd,pWnd; call sldim.exe+0x153e3` -- the real MFC-shaped
        // AfxCallWndProc-equivalent dispatcher, i.e. the actual message-map routing this
        // investigation cares about. Followed into for one more level, same as the user32.dll chain.
        constexpr uint64_t AFX_CALL_WND_PROC_RVA = 0x153e3;

        // AfxCallWndProc's own real body (`sldim.exe+0xb81996`, reached via the RVA above) saves the
        // thread state's `m_lastSentMsg` (a 7-dword/28-byte MSG copy at state_obj+0x58, via `rep
        // movsd`) to a local buffer, overwrites it with the message actually being dispatched, then
        // does `mov ecx,ebx; call esi` where `esi = [[ebx]+0x114]` -- a real C++ virtual call through
        // pWnd's own vtable (CFG-checked via `call [_guard_check_icall_fptr]` first) into the real
        // `CWnd::WindowProc`. This is the one specific call site (not a general call target, since its
        // target address varies by vtable slot contents) forced to be followed regardless of the
        // general follow-mode state, so the chase can see what WindowProc itself does with wParam
        // 0x464 rather than stopping the moment sldim.exe's own image is reached.
        constexpr uint64_t CWND_WINDOWPROC_VIRTUAL_CALL_SITE_RVA = 0xb81a52;
        bool g_sldim_dispatch_follow_next_indirect_call = false;

        // DispatchMessageW's own RVA in the 32-bit (syswow64) user32.dll loaded by sldim.exe under
        // WOW64, re-resolved and cross-checked against the shared root's own COFF export table this
        // cycle (see project_solidworks_bringup.md #285/#288). Arms the same generic hop-chase engine
        // used for the WOW64-return trace, but starting directly at DispatchMessageW's entry instead
        // of at a post-syscall return address, to trace its real internal control flow. sldim.exe's
        // own process was observed this cycle to map a 32-bit user32.dll TWICE at two different
        // addresses (a real NtMapViewOfSection STATUS_IMAGE_NOT_AT_BASE retry dance, not a guess) --
        // every I386 user32.dll load this process performs is watched, not just the first, so real
        // execution (not assumption) determines which copy is actually called.
        constexpr uint64_t DISPATCH_MESSAGE_W_RVA = 0x26510;
        constexpr size_t DISPATCH_MESSAGE_W_MAX_CANDIDATES = 8;
        std::array<uint64_t, DISPATCH_MESSAGE_W_MAX_CANDIDATES> g_dispatch_message_w_entry_vas{};
        size_t g_dispatch_message_w_entry_count = 0;
        bool g_dispatch_message_w_entry_armed = false;

        // DispatchMessageW's own body (see the SOGEN_TRACE_DISPATCHMESSAGE trace above) is a thin,
        // unexported hot-patchable wrapper: `mov edi,edi; push ebp; mov ebp,esp; push ecx;
        // mov ecx,[ebp+8]; xor edx,edx; call user32.dll+0x26530; pop ecx; pop ebp; ret 4` -- its sole
        // internal call, live-disassembled and confirmed this cycle (project_solidworks_bringup.md
        // #288), is the real dispatch worker. Hooking this RVA directly (instead of following it via
        // the generic "skip over calls" chase) is what actually traces DispatchMessageW's real internal
        // dispatch logic rather than just its own thin prologue/epilogue.
        constexpr uint64_t DISPATCH_MESSAGE_W_INTERNAL_RVA = 0x26530;
        std::array<uint64_t, DISPATCH_MESSAGE_W_MAX_CANDIDATES> g_dispatch_message_w_internal_vas{};
        size_t g_dispatch_message_w_internal_count = 0;
        bool g_dispatch_message_w_internal_armed = false;

        template <typename Return, typename... Args>
        std::function<Return(Args...)> make_callback(analysis_context& c, Return (*callback)(analysis_context&, Args...))
        {
            return [&c, callback](Args... args) {
                return callback(c, std::forward<Args>(args)...); //
            };
        }

        template <typename Return, typename... Args>
        std::function<Return(Args...)> make_callback(analysis_context& c, Return (*callback)(const analysis_context&, Args...))
        {
            return [&c, callback](Args... args) {
                return callback(c, std::forward<Args>(args)...); //
            };
        }

        std::string get_instruction_string(const disassembler& d, x86_64_cpu& emu, const uint64_t address)
        {
            std::array<uint8_t, MAX_INSTRUCTION_BYTES> instruction_bytes{};
            const auto result = emu.try_read_memory(address, instruction_bytes.data(), instruction_bytes.size());
            if (!result)
            {
                return {};
            }

            const auto reg_cs = emu.reg<uint16_t>(x86_register::cs);
            const auto instructions = d.disassemble(emu, reg_cs, instruction_bytes, 1, address);
            if (instructions.empty())
            {
                return {};
            }

            const auto& inst = instructions[0];
            return std::string(inst.mnemonic) + (strlen(inst.op_str) ? " "s + inst.op_str : "");
        }

        bool is_int_resource(const uint64_t address)
        {
            return (address >> 0x10) == 0;
        }

        template <typename CharType = char>
        std::string read_arg_as_string(windows_emulator& win_emu, const size_t index)
        {
            const auto var_ptr = get_function_argument(win_emu.emu(), index);
            if (!var_ptr || is_int_resource(var_ptr))
            {
                return {};
            }

            try
            {
                auto str = read_string<CharType>(win_emu.memory, var_ptr);
                if constexpr (std::is_same_v<CharType, char16_t>)
                {
                    return u16_to_u8(str);
                }
                else
                {
                    return str;
                }
            }
            catch (...)
            {
                return "[failed to read]";
            }
        }

        std::string read_module_name(windows_emulator& win_emu, const size_t index)
        {
            const auto var_ptr = get_function_argument(win_emu.emu(), index);
            if (!var_ptr)
            {
                return {};
            }

            return win_emu.mod_manager.find_name(var_ptr);
        }

        std::vector<function_execution_detail> collect_function_details(const analysis_context& c, const std::string_view function)
        {
            std::vector<function_execution_detail> details{};

            const auto push_detail = [&](std::string value, std::string label = {}) {
                if (!value.empty())
                {
                    details.emplace_back(function_execution_detail{.label = std::move(label), .value = std::move(value)});
                }
            };

            if (function == "GetEnvironmentVariableA"      //
                || function == "ExpandEnvironmentStringsA" //
                || function == "LoadLibraryA")
            {
                push_detail(read_arg_as_string(*c.win_emu, 0));
            }
            else if (function == "LoadLibraryW")
            {
                push_detail(read_arg_as_string<char16_t>(*c.win_emu, 0));
            }
            else if (function == "MessageBoxA")
            {
                push_detail(read_arg_as_string(*c.win_emu, 2));
                push_detail(read_arg_as_string(*c.win_emu, 1));
            }
            else if (function == "MessageBoxW")
            {
                push_detail(read_arg_as_string<char16_t>(*c.win_emu, 2));
                push_detail(read_arg_as_string<char16_t>(*c.win_emu, 1));
            }
            else if (function == "GetProcAddress")
            {
                push_detail(read_module_name(*c.win_emu, 0));
                push_detail(read_arg_as_string(*c.win_emu, 1));
            }
            else if (function == "WinVerifyTrust")
            {
                auto& emu = c.win_emu->emu();
                emu.reg(x86_register::rip, emu.read_stack(0));
                emu.reg(x86_register::rsp, emu.reg(x86_register::rsp) + 8);
                emu.reg(x86_register::rax, 0);
            }
            else if (function == "lstrcmp" || function == "lstrcmpi")
            {
                push_detail(read_arg_as_string(*c.win_emu, 0));
                push_detail(read_arg_as_string(*c.win_emu, 1));
            }

            return details;
        }

        void handle_suspicious_activity(const analysis_context& c, const std::string_view details)
        {
            std::string decoded_instruction{};
            const auto rip = c.win_emu->emu().read_instruction_pointer();

            if (details == "Illegal instruction")
            {
                decoded_instruction = get_instruction_string(c.d, c.win_emu->emu(), rip);
            }

            c.emit_observation<suspicious_activity_event>([&](auto& event) {
                event.details = std::string(details);
                event.decoded_instruction = std::move(decoded_instruction);
            });
        }

        void handle_debug_string(const analysis_context& c, const std::string_view details)
        {
            c.emit_observation<debug_string_event>([&](auto& event) { event.details = std::string(details); });
        }

        void handle_generic_activity(const analysis_context& c, const std::string_view details)
        {
            if (!c.settings->skip_generic_activity)
            {
                c.emit_observation<generic_activity_event>([&](auto& event) { event.details = std::string(details); });
            }
        }

        void handle_generic_access(const analysis_context& c, const std::string_view type, const std::u16string_view name)
        {
            if (!c.settings->skip_generic_activity)
            {
                c.emit_observation<generic_access_event>([&](auto& event) {
                    event.type = std::string(type);
                    event.name = u16_to_u8(name);
                });
            }
        }

        void handle_memory_allocate(const analysis_context& c, const uint64_t address, const uint64_t length,
                                    const memory_permission permission, const bool commit)
        {
            if (!c.settings->skip_generic_activity)
            {
                c.emit_observation<memory_allocate_event>([&](auto& event) {
                    event.address = address;
                    event.length = length;
                    event.permissions = get_permission_string(permission);
                    event.commit = commit;
                });
            }
        }

        void handle_memory_protect(const analysis_context& c, const uint64_t address, const uint64_t length,
                                   const memory_permission permission)
        {
            if (!c.settings->skip_generic_activity)
            {
                c.emit_observation<memory_protect_event>([&](auto& event) {
                    event.address = address;
                    event.length = length;
                    event.permissions = get_permission_string(permission);
                });
            }
        }

        void handle_memory_violate(const analysis_context& c, const uint64_t address, const uint64_t size, const memory_operation operation,
                                   const memory_violation_type type)
        {
            c.emit_observation<memory_violation_event>([&](auto& event) {
                event.address = address;
                event.size = size;
                event.operation = get_permission_string(operation);
                event.violation_type = type == memory_violation_type::protection ? "protection"s : "unmapped"s;
            });

            if (type == memory_violation_type::unmapped)
            {
                if (c.mapping_violation.first == address)
                {
                    if (++c.mapping_violation.second > 5)
                    {
                        throw std::runtime_error("Too many identical violations. Aborting...");
                    }
                }
                else
                {
                    c.mapping_violation.first = address;
                    c.mapping_violation.second = 1;
                }
            }
        }

        void handle_ioctrl(const analysis_context& c, const io_device& device, const std::u16string_view device_name, const ULONG code)
        {
            if (!c.settings->skip_generic_activity)
            {
                c.emit_observation<io_control_event>([&](auto& event) {
                    event.device_name = u16_to_u8(device_name);
                    event.code = static_cast<uint32_t>(code);
                });
            }

            if (!g_thread_activity_armed && std::getenv("SOGEN_TRACE_THREAD_ACTIVITY") && code == FSCTL_PIPE_LISTEN)
            {
                const auto* pipe = dynamic_cast<const named_pipe*>(&device);
                if (pipe && pipe->name.find(u"mojo.") != std::u16string::npos)
                {
                    g_thread_activity_armed = true;
                    g_thread_activity_target_tid = c.win_emu->current_thread().id;
                    c.win_emu->log.error("[thread-activity-trace] armed on tid=%u after its own FSCTL_PIPE_LISTEN on pipe='%s'\n",
                                         g_thread_activity_target_tid, u16_to_u8(pipe->name).c_str());
                    trace_all_existing_worker_factory_threads(c);
                }
            }
        }

        void handle_thread_create(const analysis_context& c, const handle thread_handle, emulator_thread& t)
        {
            if (g_thread_activity_armed)
            {
                trace_worker_factory_thread_if_new(c, thread_handle, t.id);
            }

            if (c.settings->skip_generic_activity)
            {
                return;
            }

            std::vector<std::string> flags{};

            if (t.create_flags & THREAD_CREATE_FLAGS_CREATE_SUSPENDED)
            {
                flags.emplace_back("suspended");
            }
            if (t.create_flags & THREAD_CREATE_FLAGS_SKIP_THREAD_ATTACH)
            {
                flags.emplace_back("skip thread attach");
            }
            if (t.create_flags & THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER)
            {
                flags.emplace_back("hide from debugger");
            }
            if (t.create_flags & THREAD_CREATE_FLAGS_LOADER_WORKER)
            {
                flags.emplace_back("loader worker");
            }
            if (t.create_flags & THREAD_CREATE_FLAGS_SKIP_LOADER_INIT)
            {
                flags.emplace_back("skip loader init");
            }
            if (t.create_flags & THREAD_CREATE_FLAGS_BYPASS_PROCESS_FREEZE)
            {
                flags.emplace_back("bypass process freeze");
            }

            c.emit_observation<thread_create_event>([&](auto& event) {
                event.created_thread_id = t.id;
                event.start_address = t.start_address;
                event.argument = t.argument;
                event.flags = std::move(flags);
            });
        }

        void handle_thread_terminated(const analysis_context& c, handle, emulator_thread& t)
        {
            if (!c.settings->skip_generic_activity)
            {
                c.emit_observation<thread_terminated_event>([&](auto& event) { event.terminated_thread_id = t.id; });
            }

            if (g_thread_activity_armed && is_thread_activity_traced_tid(t.id))
            {
                c.win_emu->log.error("[thread-activity-trace] tid=%u TERMINATED (exit_status=0x%08X)\n", t.id,
                                     static_cast<uint32_t>(t.exit_status.value_or(0)));
            }
        }

        void handle_thread_set_name(const analysis_context& c, const emulator_thread& t)
        {
            c.emit_observation<thread_set_name_event>([&](auto& event) {
                event.renamed_thread_id = t.id;
                event.name = u16_to_u8(t.name);
            });
        }

        void handle_thread_switch(const analysis_context& c, const emulator_thread& current_thread, const emulator_thread& new_thread)
        {
            if (!c.settings->skip_generic_activity)
            {
                c.emit_observation<thread_switch_event>([&](auto& event) {
                    event.previous_thread_id = current_thread.id;
                    event.next_thread_id = new_thread.id;
                });
            }

            if (g_thread_activity_armed &&
                (is_thread_activity_traced_tid(current_thread.id) || is_thread_activity_traced_tid(new_thread.id)))
            {
                c.win_emu->log.error("[thread-activity-trace] scheduler switch %u -> %u\n", current_thread.id, new_thread.id);
            }
        }

        void handle_module_load(const analysis_context& c, const mapped_module& mod)
        {
            c.emit_observation<module_load_event>([&](auto& event) {
                event.path = mod.module_path.string();
                event.image_base = mod.image_base;
            });

            if (std::getenv("SOGEN_TRACE_MODULE_LOAD"))
            {
                c.win_emu->log.error("[module-load-trace] %s loaded at 0x%llx (machine=0x%x, entry=0x%llx, "
                                     "instruction_precision=%d)\n",
                                     mod.name.c_str(), static_cast<unsigned long long>(mod.image_base), mod.machine,
                                     static_cast<unsigned long long>(mod.entry_point), c.win_emu->uses_instruction_precision());
            }

            if (mod.name == "msedge.dll" && std::getenv("SOGEN_TRACE_ACCEPT_ISOLATED"))
            {
                g_accept_isolated_trace_va = mod.image_base + ACCEPT_ISOLATED_RVA;
                c.win_emu->log.error("[accept-isolated-trace] msedge.dll loaded at 0x%llx, watching 0x%llx\n",
                                     static_cast<unsigned long long>(mod.image_base),
                                     static_cast<unsigned long long>(g_accept_isolated_trace_va));
            }

            if (mod.name == "msedge.dll" && std::getenv("SOGEN_TRACE_NODE_CONNECT"))
            {
                for (size_t i = 0; i < NODE_CONNECT_TARGETS.size(); ++i)
                {
                    g_node_connect_trace_vas[i] = mod.image_base + NODE_CONNECT_TARGETS[i].rva;
                    c.win_emu->log.error("[node-connect-trace] watching %s at 0x%llx\n", NODE_CONNECT_TARGETS[i].name,
                                         static_cast<unsigned long long>(g_node_connect_trace_vas[i]));
                }
            }

            if (mod.name == "msedge.dll" && std::getenv("SOGEN_TRACE_ESTABLISH_WAITING_ROUTERS"))
            {
                g_establish_waiting_routers_trace_va = mod.image_base + ESTABLISH_WAITING_ROUTERS_CMP_RVA;
                c.win_emu->log.error("[establish-waiting-routers-trace] watching NodeConnector::EstablishWaitingRouters's "
                                     "min-comparison at 0x%llx\n",
                                     static_cast<unsigned long long>(g_establish_waiting_routers_trace_va));

                for (size_t i = 0; i < ROUTER_LINK_TARGETS.size(); ++i)
                {
                    g_router_link_trace_vas[i] = mod.image_base + ROUTER_LINK_TARGETS[i].rva;
                    c.win_emu->log.error("[establish-waiting-routers-trace] watching %s at 0x%llx\n", ROUTER_LINK_TARGETS[i].name,
                                         static_cast<unsigned long long>(g_router_link_trace_vas[i]));
                }
            }

            if (mod.name == "msedge.dll" && std::getenv("SOGEN_TRACE_DELAYLOAD_FAILURE"))
            {
                g_delayload_failure_trace_va = mod.image_base + DELAYLOAD_FAILURE_RVA;
                c.win_emu->log.error("[delayload-failure-trace] watching HandleDelayLoadFailureCommon at 0x%llx\n",
                                     static_cast<unsigned long long>(g_delayload_failure_trace_va));
            }

            if (mod.name == "embeddedbrowserwebview.dll" && mod.machine == IMAGE_FILE_MACHINE_I386 &&
                std::getenv("SOGEN_TRACE_DCOMP_DEVICE2"))
            {
                g_ebwv_dcomp_device2_wrapper_entry_va = mod.image_base + EBWV_DCOMP_DEVICE2_WRAPPER_ENTRY_RVA;
                g_ebwv_dcomp_device2_resolve_call_va = mod.image_base + EBWV_DCOMP_DEVICE2_RESOLVE_CALL_RVA;
                g_ebwv_dcomp_device2_invoke_call_va = mod.image_base + EBWV_DCOMP_DEVICE2_INVOKE_CALL_RVA;
                g_ebwv_dcomp_device2_invoke_return_va = mod.image_base + EBWV_DCOMP_DEVICE2_INVOKE_RETURN_RVA;
                c.win_emu->log.error("[dcomp-device2-trace] watching embeddedbrowserwebview.dll's DCompositionCreateDevice2 wrapper: "
                                     "entry=0x%llx resolve_call=0x%llx invoke_call=0x%llx invoke_return=0x%llx\n",
                                     static_cast<unsigned long long>(g_ebwv_dcomp_device2_wrapper_entry_va),
                                     static_cast<unsigned long long>(g_ebwv_dcomp_device2_resolve_call_va),
                                     static_cast<unsigned long long>(g_ebwv_dcomp_device2_invoke_call_va),
                                     static_cast<unsigned long long>(g_ebwv_dcomp_device2_invoke_return_va));
            }

            if (mod.name == "embeddedbrowserwebview.dll" && mod.machine == IMAGE_FILE_MACHINE_I386 && std::getenv("SOGEN_TRACE_DCOMP_GATE"))
            {
                g_ebwv_dcomp_gate1_entry_va = mod.image_base + EBWV_DCOMP_GATE1_ENTRY_RVA;
                g_ebwv_dcomp_gate1_args_va = mod.image_base + EBWV_DCOMP_GATE1_ARGS_RVA;
                g_ebwv_dcomp_gate1_result_va = mod.image_base + EBWV_DCOMP_GATE1_RESULT_RVA;
                g_ebwv_dcomp_gate2_entry_va = mod.image_base + EBWV_DCOMP_GATE2_ENTRY_RVA;
                g_ebwv_dcomp_gate2_args_va = mod.image_base + EBWV_DCOMP_GATE2_ARGS_RVA;
                g_ebwv_dcomp_gate2_result_va = mod.image_base + EBWV_DCOMP_GATE2_RESULT_RVA;
                g_ebwv_dcomp_gate3_entry_va = mod.image_base + EBWV_DCOMP_GATE3_ENTRY_RVA;
                g_ebwv_dcomp_gate3_args_va = mod.image_base + EBWV_DCOMP_GATE3_ARGS_RVA;
                g_ebwv_dcomp_gate3_result_va = mod.image_base + EBWV_DCOMP_GATE3_RESULT_RVA;
                c.win_emu->log.error("[dcomp-gate-trace] watching all 3 accessor-caller functions' true entries: gate1_entry=0x%llx "
                                     "gate1_args=0x%llx gate1_result=0x%llx gate2_entry=0x%llx gate2_args=0x%llx gate2_result=0x%llx "
                                     "gate3_entry=0x%llx gate3_args=0x%llx gate3_result=0x%llx\n",
                                     static_cast<unsigned long long>(g_ebwv_dcomp_gate1_entry_va),
                                     static_cast<unsigned long long>(g_ebwv_dcomp_gate1_args_va),
                                     static_cast<unsigned long long>(g_ebwv_dcomp_gate1_result_va),
                                     static_cast<unsigned long long>(g_ebwv_dcomp_gate2_entry_va),
                                     static_cast<unsigned long long>(g_ebwv_dcomp_gate2_args_va),
                                     static_cast<unsigned long long>(g_ebwv_dcomp_gate2_result_va),
                                     static_cast<unsigned long long>(g_ebwv_dcomp_gate3_entry_va),
                                     static_cast<unsigned long long>(g_ebwv_dcomp_gate3_args_va),
                                     static_cast<unsigned long long>(g_ebwv_dcomp_gate3_result_va));
            }

            if (mod.name == "embeddedbrowserwebview.dll" && mod.machine == IMAGE_FILE_MACHINE_I386 &&
                std::getenv("SOGEN_TRACE_GATE1_LAZY_HELPER"))
            {
                g_ebwv_gate1_lazy_helper_entry_va = mod.image_base + EBWV_GATE1_LAZY_HELPER_ENTRY_RVA;
                g_ebwv_gate1_lazy_helper_return_va = mod.image_base + EBWV_GATE1_LAZY_HELPER_RETURN_RVA;
                c.win_emu->log.error("[gate1-lazy-helper-trace] watching lazy-init/cache-lookup helper: entry=0x%llx return=0x%llx\n",
                                     static_cast<unsigned long long>(g_ebwv_gate1_lazy_helper_entry_va),
                                     static_cast<unsigned long long>(g_ebwv_gate1_lazy_helper_return_va));
            }

            if (mod.name == "embeddedbrowserwebview.dll" && mod.machine == IMAGE_FILE_MACHINE_I386 &&
                std::getenv("SOGEN_TRACE_GATE1_ACCESSOR"))
            {
                g_ebwv_gate1_accessor_entry_va = mod.image_base + EBWV_GATE1_ACCESSOR_ENTRY_RVA;
                g_ebwv_gate1_accessor_result_va = mod.image_base + EBWV_GATE1_ACCESSOR_RESULT_RVA;
                g_ebwv_gate1_accessor_dcomp_invoke_va = mod.image_base + EBWV_GATE1_ACCESSOR_DCOMP_INVOKE_RVA;
                g_ebwv_gate1_accessor_dcomp_invoke_return_va = mod.image_base + EBWV_GATE1_ACCESSOR_DCOMP_INVOKE_RETURN_RVA;
                g_ebwv_gate1_accessor_dcomp_resolved_fn_va = mod.image_base + EBWV_GATE1_ACCESSOR_DCOMP_RESOLVED_FN_RVA;
                c.win_emu->log.error("[gate1-accessor-trace] watching gate1's real accessor and its DCompositionCreateDevice2 call chain: "
                                     "entry=0x%llx result=0x%llx dcomp_invoke=0x%llx dcomp_invoke_return=0x%llx resolved_fn_slot=0x%llx\n",
                                     static_cast<unsigned long long>(g_ebwv_gate1_accessor_entry_va),
                                     static_cast<unsigned long long>(g_ebwv_gate1_accessor_result_va),
                                     static_cast<unsigned long long>(g_ebwv_gate1_accessor_dcomp_invoke_va),
                                     static_cast<unsigned long long>(g_ebwv_gate1_accessor_dcomp_invoke_return_va),
                                     static_cast<unsigned long long>(g_ebwv_gate1_accessor_dcomp_resolved_fn_va));
            }

            if (mod.name == "user32.dll" && mod.machine == IMAGE_FILE_MACHINE_I386 &&
                (std::getenv("SOGEN_TRACE_DISPATCHMESSAGE") || std::getenv("SOGEN_TRACE_DISPATCHMESSAGE_INTERNAL")))
            {
                const auto* exe = c.win_emu->mod_manager.executable;
                if (exe != nullptr && exe->name == "sldim.exe")
                {
                    if (std::getenv("SOGEN_TRACE_DISPATCHMESSAGE") &&
                        g_dispatch_message_w_entry_count < g_dispatch_message_w_entry_vas.size())
                    {
                        const auto va = mod.image_base + DISPATCH_MESSAGE_W_RVA;
                        g_dispatch_message_w_entry_vas[g_dispatch_message_w_entry_count++] = va;
                        c.win_emu->log.error("[dispatchmessage-trace] I386 user32.dll #%zu loaded at 0x%llx, watching "
                                             "DispatchMessageW entry at 0x%llx\n",
                                             g_dispatch_message_w_entry_count, static_cast<unsigned long long>(mod.image_base),
                                             static_cast<unsigned long long>(va));
                    }

                    if (std::getenv("SOGEN_TRACE_DISPATCHMESSAGE_INTERNAL") &&
                        g_dispatch_message_w_internal_count < g_dispatch_message_w_internal_vas.size())
                    {
                        const auto va = mod.image_base + DISPATCH_MESSAGE_W_INTERNAL_RVA;
                        g_dispatch_message_w_internal_vas[g_dispatch_message_w_internal_count++] = va;
                        c.win_emu->log.error("[dispatchmessage-trace] I386 user32.dll #%zu loaded at 0x%llx, watching "
                                             "DispatchMessageW's internal worker at 0x%llx\n",
                                             g_dispatch_message_w_internal_count, static_cast<unsigned long long>(mod.image_base),
                                             static_cast<unsigned long long>(va));
                    }
                }
            }

            if (mod.name == "kernelbase.dll" && std::getenv("SOGEN_TRACE_NAMED_PIPE_CREATE"))
            {
                g_create_named_pipe_trace_va = mod.image_base + CREATE_NAMED_PIPE_W_RVA;
                c.win_emu->log.error("[named-pipe-create-trace] watching CreateNamedPipeW at 0x%llx\n",
                                     static_cast<unsigned long long>(g_create_named_pipe_trace_va));
            }

            if (mod.name == "msedge.dll" && std::getenv("SOGEN_TRACE_NAMED_PIPE_CREATE"))
            {
                g_platform_channel_ctor_trace_va = mod.image_base + PLATFORM_CHANNEL_CTOR_RVA;
                c.win_emu->log.error("[named-pipe-create-trace] watching PlatformChannel::PlatformChannel at 0x%llx\n",
                                     static_cast<unsigned long long>(g_platform_channel_ctor_trace_va));

                g_platform_channel_tracker_entry_trace_va = mod.image_base + PLATFORM_CHANNEL_TRACKER_ENTRY_RVA;
                c.win_emu->log.error("[named-pipe-create-trace] watching unexported tracker helper at 0x%llx\n",
                                     static_cast<unsigned long long>(g_platform_channel_tracker_entry_trace_va));

                g_platform_channel_delegate_call_trace_va = mod.image_base + PLATFORM_CHANNEL_DELEGATE_CALL_RVA;
                g_platform_channel_delegate_return_trace_va = mod.image_base + PLATFORM_CHANNEL_DELEGATE_RETURN_RVA;
                c.win_emu->log.error("[named-pipe-create-trace] watching tracker helper's post-construction delegate call at "
                                     "0x%llx (return point 0x%llx)\n",
                                     static_cast<unsigned long long>(g_platform_channel_delegate_call_trace_va),
                                     static_cast<unsigned long long>(g_platform_channel_delegate_return_trace_va));
            }
        }

        void trace_accept_isolated_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            const auto rsp = emu.read_stack_pointer();

            uint64_t return_address{};
            emu.try_read_memory(rsp, &return_address, sizeof(return_address));

            const auto rcx = emu.reg<uint64_t>(x86_register::rcx);
            const auto rdx = emu.reg<uint64_t>(x86_register::rdx);
            const auto r8 = emu.reg<uint64_t>(x86_register::r8);
            const auto r9 = emu.reg<uint64_t>(x86_register::r9);

            uint64_t rdx_pointee[2]{};
            emu.try_read_memory(rdx, &rdx_pointee, sizeof(rdx_pointee));

            const auto* caller_mod_name = c.win_emu->mod_manager.find_name(return_address);
            const auto* caller_mod = c.win_emu->mod_manager.find_by_address(return_address);
            const auto caller_offset = caller_mod ? return_address - caller_mod->image_base : return_address;

            c.win_emu->log.error("[accept-isolated-trace] hit at 0x%llx, rcx=0x%llx rdx=0x%llx r8=0x%llx r9=0x%llx "
                                 "*rdx=[0x%llx, 0x%llx] return=0x%llx (%s+0x%llx)\n",
                                 static_cast<unsigned long long>(address), static_cast<unsigned long long>(rcx),
                                 static_cast<unsigned long long>(rdx), static_cast<unsigned long long>(r8),
                                 static_cast<unsigned long long>(r9), static_cast<unsigned long long>(rdx_pointee[0]),
                                 static_cast<unsigned long long>(rdx_pointee[1]), static_cast<unsigned long long>(return_address),
                                 caller_mod_name, static_cast<unsigned long long>(caller_offset));
        }

        void trace_node_connect_hit(const analysis_context& c, const uint64_t address, const char* name)
        {
            auto& emu = c.win_emu->emu();
            const auto rsp = emu.read_stack_pointer();

            uint64_t return_address{};
            emu.try_read_memory(rsp, &return_address, sizeof(return_address));

            const auto rcx = emu.reg<uint64_t>(x86_register::rcx);
            const auto rdx = emu.reg<uint64_t>(x86_register::rdx);
            const auto r8 = emu.reg<uint64_t>(x86_register::r8);
            const auto r9 = emu.reg<uint64_t>(x86_register::r9);

            const auto* caller_mod_name = c.win_emu->mod_manager.find_name(return_address);
            const auto* caller_mod = c.win_emu->mod_manager.find_by_address(return_address);
            const auto caller_offset = caller_mod ? return_address - caller_mod->image_base : return_address;

            c.win_emu->log.error("[node-connect-trace] hit %s at 0x%llx, rcx=0x%llx rdx=0x%llx r8=0x%llx r9=0x%llx "
                                 "return=0x%llx (%s+0x%llx)\n",
                                 name, static_cast<unsigned long long>(address), static_cast<unsigned long long>(rcx),
                                 static_cast<unsigned long long>(rdx), static_cast<unsigned long long>(r8),
                                 static_cast<unsigned long long>(r9), static_cast<unsigned long long>(return_address), caller_mod_name,
                                 static_cast<unsigned long long>(caller_offset));
        }

        void trace_establish_waiting_routers_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            const auto rsp = emu.read_stack_pointer();

            uint64_t return_address{};
            emu.try_read_memory(rsp, &return_address, sizeof(return_address));

            const auto this_ptr = emu.reg<uint64_t>(x86_register::rbx);
            const auto waiting_routers_size = emu.reg<uint64_t>(x86_register::rcx);
            const auto max_valid_portals = emu.reg<uint64_t>(x86_register::r8);

            const auto* caller_mod_name = c.win_emu->mod_manager.find_name(return_address);
            const auto* caller_mod = c.win_emu->mod_manager.find_by_address(return_address);
            const auto caller_offset = caller_mod ? return_address - caller_mod->image_base : return_address;

            c.win_emu->log.error("[establish-waiting-routers-trace] hit at 0x%llx, this=0x%llx "
                                 "waiting_routers_.size()=%llu max_valid_portals=%llu return=0x%llx (%s+0x%llx)\n",
                                 static_cast<unsigned long long>(address), static_cast<unsigned long long>(this_ptr),
                                 static_cast<unsigned long long>(waiting_routers_size), static_cast<unsigned long long>(max_valid_portals),
                                 static_cast<unsigned long long>(return_address), caller_mod_name,
                                 static_cast<unsigned long long>(caller_offset));
        }

        void trace_delayload_failure_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            const auto rsp = emu.read_stack_pointer();

            uint64_t return_address{};
            emu.try_read_memory(rsp, &return_address, sizeof(return_address));

            const auto dli_notify = emu.reg<uint32_t>(x86_register::ecx);
            const auto pdli = emu.reg<uint64_t>(x86_register::rdx);

            uint64_t sz_dll_ptr{};
            uint32_t import_by_name{};
            uint64_t proc_union{};
            uint32_t dw_last_error{};
            emu.try_read_memory(pdli + 0x18, &sz_dll_ptr, sizeof(sz_dll_ptr));
            emu.try_read_memory(pdli + 0x20, &import_by_name, sizeof(import_by_name));
            emu.try_read_memory(pdli + 0x28, &proc_union, sizeof(proc_union));
            emu.try_read_memory(pdli + 0x40, &dw_last_error, sizeof(dw_last_error));

            const auto sz_proc_name_ptr = import_by_name ? proc_union : 0;
            const auto dw_ordinal = import_by_name ? 0u : static_cast<uint32_t>(proc_union);

            std::string dll_name;
            std::string proc_name;
            try
            {
                if (sz_dll_ptr)
                {
                    dll_name = read_string<char>(c.win_emu->memory, sz_dll_ptr);
                }
            }
            catch (...)
            {
            }
            try
            {
                if (sz_proc_name_ptr)
                {
                    proc_name = read_string<char>(c.win_emu->memory, sz_proc_name_ptr);
                }
            }
            catch (...)
            {
            }

            const auto* caller_mod_name = c.win_emu->mod_manager.find_name(return_address);
            const auto* caller_mod = c.win_emu->mod_manager.find_by_address(return_address);
            const auto caller_offset = caller_mod ? return_address - caller_mod->image_base : return_address;

            c.win_emu->log.error("[delayload-failure-trace] hit at 0x%llx, dliNotify=%u pdli=0x%llx dll=\"%s\" "
                                 "proc=\"%s\" ordinal=%u lastError=0x%x return=0x%llx (%s+0x%llx)\n",
                                 static_cast<unsigned long long>(address), dli_notify, static_cast<unsigned long long>(pdli),
                                 dll_name.c_str(), proc_name.c_str(), dw_ordinal, dw_last_error,
                                 static_cast<unsigned long long>(return_address), caller_mod_name,
                                 static_cast<unsigned long long>(caller_offset));
        }

        void trace_create_named_pipe_hit(const analysis_context& c, const uint64_t address)
        {
            constexpr uint32_t FILE_FLAG_OVERLAPPED = 0x40000000;

            auto& emu = c.win_emu->emu();
            const auto rsp = emu.read_stack_pointer();

            uint64_t return_address{};
            emu.try_read_memory(rsp, &return_address, sizeof(return_address));

            const auto lp_name = emu.reg<uint64_t>(x86_register::rcx);
            const auto dw_open_mode = emu.reg<uint32_t>(x86_register::edx);
            const auto dw_pipe_mode = emu.reg<uint32_t>(x86_register::r8d);
            const auto n_max_instances = emu.reg<uint32_t>(x86_register::r9d);

            std::string name;
            try
            {
                if (lp_name)
                {
                    name = u16_to_u8(read_string<char16_t>(c.win_emu->memory, lp_name));
                }
            }
            catch (...)
            {
            }

            const auto* caller_mod_name = c.win_emu->mod_manager.find_name(return_address);
            const auto* caller_mod = c.win_emu->mod_manager.find_by_address(return_address);
            const auto caller_offset = caller_mod ? return_address - caller_mod->image_base : return_address;

            c.win_emu->log.error("[named-pipe-create-trace] hit at 0x%llx, name=\"%s\" dwOpenMode=0x%x overlapped=%d dwPipeMode=0x%x "
                                 "nMaxInstances=%u tid=%u return=0x%llx (%s+0x%llx)\n",
                                 static_cast<unsigned long long>(address), name.c_str(), dw_open_mode,
                                 (dw_open_mode & FILE_FLAG_OVERLAPPED) != 0, dw_pipe_mode, n_max_instances, c.win_emu->current_thread().id,
                                 static_cast<unsigned long long>(return_address), caller_mod_name,
                                 static_cast<unsigned long long>(caller_offset));
        }

        void trace_platform_channel_ctor_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            const auto rsp = emu.read_stack_pointer();

            uint64_t return_address{};
            emu.try_read_memory(rsp, &return_address, sizeof(return_address));

            const auto* caller_mod_name = c.win_emu->mod_manager.find_name(return_address);
            const auto* caller_mod = c.win_emu->mod_manager.find_by_address(return_address);
            const auto caller_offset = caller_mod ? return_address - caller_mod->image_base : return_address;

            c.win_emu->log.error("[named-pipe-create-trace] PlatformChannel ctor hit at 0x%llx, tid=%u return=0x%llx (%s+0x%llx)\n",
                                 static_cast<unsigned long long>(address), c.win_emu->current_thread().id,
                                 static_cast<unsigned long long>(return_address), caller_mod_name,
                                 static_cast<unsigned long long>(caller_offset));
        }

        void trace_platform_channel_tracker_entry_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            const auto rsp = emu.read_stack_pointer();

            uint64_t return_address{};
            emu.try_read_memory(rsp, &return_address, sizeof(return_address));

            const auto rcx = emu.reg<uint64_t>(x86_register::rcx);

            const auto* caller_mod_name = c.win_emu->mod_manager.find_name(return_address);
            const auto* caller_mod = c.win_emu->mod_manager.find_by_address(return_address);
            const auto caller_offset = caller_mod ? return_address - caller_mod->image_base : return_address;

            c.win_emu->log.error("[named-pipe-create-trace] tracker-entry hit at 0x%llx, this=0x%llx tid=%u return=0x%llx (%s+0x%llx)\n",
                                 static_cast<unsigned long long>(address), static_cast<unsigned long long>(rcx),
                                 c.win_emu->current_thread().id, static_cast<unsigned long long>(return_address), caller_mod_name,
                                 static_cast<unsigned long long>(caller_offset));
        }

        // Decodes the MSVC x64 RTTI chain (vtable[-1] -> RTTICompleteObjectLocator -> TypeDescriptor)
        // for a polymorphic object's vtable pointer, exactly the technique project_solidworks_bringup.md
        // #295 already used successfully to identify CMessagingThread from a raw vtable address -- applied
        // here to whatever real C++ class the delegate-call's target object (project_solidworks_bringup.md
        // #279-281) actually is, since its vtable slot resolving to a no-op `ret` left that unresolved.
        struct rtti_decode_result
        {
            std::string type_name;
            uint64_t vtable_ptr{};
            uint64_t locator_ptr{};
            uint32_t type_descriptor_rva{};
            const char* failed_at = nullptr;
        };

        rtti_decode_result decode_rtti_type_name(const analysis_context& c, const uint64_t object_ptr)
        {
            auto& emu = c.win_emu->emu();
            rtti_decode_result result{};

            if (!emu.try_read_memory(object_ptr, &result.vtable_ptr, sizeof(result.vtable_ptr)) || result.vtable_ptr == 0)
            {
                result.failed_at = "read-vtable-ptr";
                return result;
            }

            if (!emu.try_read_memory(result.vtable_ptr - sizeof(uint64_t), &result.locator_ptr, sizeof(result.locator_ptr)) ||
                result.locator_ptr == 0)
            {
                result.failed_at = "read-locator-ptr";
                return result;
            }

            if (!emu.try_read_memory(result.locator_ptr + 12, &result.type_descriptor_rva, sizeof(result.type_descriptor_rva)) ||
                result.type_descriptor_rva == 0)
            {
                result.failed_at = "read-type-descriptor-rva";
                return result;
            }

            const auto* vtable_mod = c.win_emu->mod_manager.find_by_address(result.vtable_ptr);
            if (!vtable_mod)
            {
                result.failed_at = "resolve-vtable-module";
                return result;
            }

            try
            {
                result.type_name = read_string<char>(c.win_emu->memory, vtable_mod->image_base + result.type_descriptor_rva + 16);
                if (result.type_name.empty())
                {
                    result.failed_at = "empty-type-name";
                }
            }
            catch (...)
            {
                result.failed_at = "read-type-name-threw";
            }

            return result;
        }

        void trace_platform_channel_delegate_call_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();

            const auto rsi = emu.reg<uint64_t>(x86_register::rsi);
            const auto rcx = emu.reg<uint64_t>(x86_register::rcx);
            const auto rax = emu.reg<uint64_t>(x86_register::rax);

            const auto* target_mod_name = c.win_emu->mod_manager.find_name(rax);
            const auto* target_mod = c.win_emu->mod_manager.find_by_address(rax);
            const auto target_offset = target_mod ? rax - target_mod->image_base : rax;

            const auto rtti = decode_rtti_type_name(c, rcx);

            c.win_emu->log.error("[named-pipe-create-trace] delegate-call hit at 0x%llx, tracker_this=0x%llx delegate_this=0x%llx "
                                 "delegate_type=\"%s\" rtti_failed_at=\"%s\" delegate_vtable=0x%llx locator=0x%llx type_desc_rva=0x%x "
                                 "target=0x%llx (%s+0x%llx) tid=%u\n",
                                 static_cast<unsigned long long>(address), static_cast<unsigned long long>(rsi),
                                 static_cast<unsigned long long>(rcx), rtti.type_name.c_str(), rtti.failed_at ? rtti.failed_at : "",
                                 static_cast<unsigned long long>(rtti.vtable_ptr), static_cast<unsigned long long>(rtti.locator_ptr),
                                 rtti.type_descriptor_rva, static_cast<unsigned long long>(rax), target_mod_name,
                                 static_cast<unsigned long long>(target_offset), c.win_emu->current_thread().id);
        }

        void trace_platform_channel_delegate_return_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();

            const auto rsi = emu.reg<uint64_t>(x86_register::rsi);
            const auto rax = emu.reg<uint64_t>(x86_register::rax);

            uint8_t constructed_flag{};
            uint8_t gate_flag{};
            emu.try_read_memory(rsi + 0x60, &constructed_flag, sizeof(constructed_flag));
            emu.try_read_memory(rsi + 0x90, &gate_flag, sizeof(gate_flag));

            c.win_emu->log.error("[named-pipe-create-trace] delegate-call return at 0x%llx, tracker_this=0x%llx return_rax=0x%llx "
                                 "this+0x60=0x%x this+0x90=0x%x tid=%u\n",
                                 static_cast<unsigned long long>(address), static_cast<unsigned long long>(rsi),
                                 static_cast<unsigned long long>(rax), constructed_flag, gate_flag, c.win_emu->current_thread().id);
        }

        void trace_sldim_queue_check_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            const auto eax = emu.reg<uint32_t>(x86_register::eax);

            ++g_sldim_queue_check_state.total;

            if (eax != 0)
            {
                ++g_sldim_queue_check_state.nonzero;
                c.win_emu->log.error("[sldim-queue-trace] QUEUE POPULATED: hit at 0x%llx eax=0x%x tid=%u total=%llu nonzero=%llu\n",
                                     static_cast<unsigned long long>(address), eax, c.win_emu->current_thread().id,
                                     static_cast<unsigned long long>(g_sldim_queue_check_state.total),
                                     static_cast<unsigned long long>(g_sldim_queue_check_state.nonzero));
            }
            else if ((g_sldim_queue_check_state.total % 2000000) == 0)
            {
                c.win_emu->log.error("[sldim-queue-trace] checkpoint: total=%llu nonzero=%llu\n",
                                     static_cast<unsigned long long>(g_sldim_queue_check_state.total),
                                     static_cast<unsigned long long>(g_sldim_queue_check_state.nonzero));
            }
        }

        void trace_sldim_oncommand_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            const auto edi = emu.reg<uint32_t>(x86_register::edi);
            const auto esi = emu.reg<uint32_t>(x86_register::esi);
            const auto ebp = emu.reg<uint32_t>(x86_register::ebp);

            uint32_t w_param{};
            uint32_t l_param{};
            emu.try_read_memory(ebp + 0xc, &w_param, sizeof(w_param));
            emu.try_read_memory(ebp + 0x10, &l_param, sizeof(l_param));

            const auto* target_mod_name = c.win_emu->mod_manager.find_name(esi);
            const auto* target_mod = c.win_emu->mod_manager.find_by_address(esi);
            const auto target_offset = target_mod ? esi - target_mod->image_base : esi;

            c.win_emu->log.error(
                "[sldim-oncommand-trace] hit at 0x%llx tid=%u pWnd=0x%x wParam=0x%x lParam=0x%x OnCommand target=0x%x (%s+0x%llx)\n",
                static_cast<unsigned long long>(address), c.win_emu->current_thread().id, edi, w_param, l_param, esi, target_mod_name,
                static_cast<unsigned long long>(target_offset));
        }

        void trace_sldim_oncommand_branch_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            const auto state_ptr = emu.reg<uint32_t>(x86_register::eax);

            uint32_t flag_94{};
            const auto flag_read_ok = emu.try_read_memory(state_ptr + 0x94, &flag_94, sizeof(flag_94));

            c.win_emu->log.error("[sldim-oncommand-trace] branch check at 0x%llx tid=%u state=0x%x flag@0x94=0x%x (read_ok=%d)\n",
                                 static_cast<unsigned long long>(address), c.win_emu->current_thread().id, state_ptr, flag_94,
                                 flag_read_ok ? 1 : 0);
        }

        void trace_sldim_oncmdmsg_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            const auto esi = emu.reg<uint32_t>(x86_register::esi);
            const auto ecx = emu.reg<uint32_t>(x86_register::ecx);

            uint32_t oncmdmsg_target{};
            const auto read_ok = emu.try_read_memory(esi + 0xc, &oncmdmsg_target, sizeof(oncmdmsg_target));

            const auto* target_mod_name = read_ok ? c.win_emu->mod_manager.find_name(oncmdmsg_target) : "?";
            const auto* target_mod = read_ok ? c.win_emu->mod_manager.find_by_address(oncmdmsg_target) : nullptr;
            const auto target_offset = target_mod ? oncmdmsg_target - target_mod->image_base : oncmdmsg_target;

            c.win_emu->log.error(
                "[sldim-oncommand-trace] OnCmdMsg dispatch at 0x%llx tid=%u this=0x%x vtbl=0x%x OnCmdMsg=0x%x (%s+0x%llx)\n",
                static_cast<unsigned long long>(address), c.win_emu->current_thread().id, ecx, esi, oncmdmsg_target, target_mod_name,
                static_cast<unsigned long long>(target_offset));
        }

        void trace_sldim_findentry_result_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            const auto eax = emu.reg<uint32_t>(x86_register::eax);

            c.win_emu->log.error("[sldim-oncommand-trace] message-map lookup result at 0x%llx tid=%u found=%d (entry=0x%x)\n",
                                 static_cast<unsigned long long>(address), c.win_emu->current_thread().id, eax != 0, eax);
        }

        void trace_sldim_handler_delegate_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            const auto ecx = emu.reg<uint32_t>(x86_register::ecx);
            const auto eax = emu.reg<uint32_t>(x86_register::eax);

            uint32_t target{};
            const auto read_ok = emu.try_read_memory(eax + 4, &target, sizeof(target));

            const auto* target_mod_name = read_ok ? c.win_emu->mod_manager.find_name(target) : "?";
            const auto* target_mod = read_ok ? c.win_emu->mod_manager.find_by_address(target) : nullptr;
            const auto target_offset = target_mod ? target - target_mod->image_base : target;

            c.win_emu->log.error(
                "[sldim-oncommand-trace] handler delegate call at 0x%llx tid=%u subobj=0x%x vtbl=0x%x target=0x%x (%s+0x%llx)\n",
                static_cast<unsigned long long>(address), c.win_emu->current_thread().id, ecx, eax, target, target_mod_name,
                static_cast<unsigned long long>(target_offset));
        }

        void trace_sldim_trypop_entry_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            const auto edi = emu.reg<uint32_t>(x86_register::edi);

            uint32_t queue_ptr{};
            const auto read_ok = emu.try_read_memory(edi + 0x28, &queue_ptr, sizeof(queue_ptr));

            if (queue_ptr != g_sldim_trypop_last_queue_ptr)
            {
                c.win_emu->log.error(
                    "[sldim-trypop-trace] hit at 0x%llx listener=0x%x queue container [listener+0x28]=0x%x (read_ok=%d) changed "
                    "from 0x%llx, tid=%u\n",
                    static_cast<unsigned long long>(address), edi, queue_ptr, read_ok ? 1 : 0,
                    static_cast<unsigned long long>(g_sldim_trypop_last_queue_ptr), c.win_emu->current_thread().id);
                g_sldim_trypop_last_queue_ptr = queue_ptr;
            }

            if (read_ok && queue_ptr != 0 && !g_sldim_trypop_write_watch_armed)
            {
                g_sldim_trypop_write_watch_armed = true;
                const uint64_t count_field_addr = queue_ptr + 4;
                c.win_emu->log.error("[sldim-trypop-trace] arming a write-watch on the queue count field at 0x%llx\n",
                                     static_cast<unsigned long long>(count_field_addr));

                emu.hook_memory_write(
                    count_field_addr, sizeof(uint32_t),
                    [&c, count_field_addr](cpu_interface&, const uint64_t write_address, const void* value, const size_t size) {
                        uint32_t new_value{};
                        memcpy(&new_value, value, std::min(size, sizeof(new_value)));

                        const auto rip = c.win_emu->emu().read_instruction_pointer();
                        const auto* writer_mod_name = c.win_emu->mod_manager.find_name(rip);
                        const auto* writer_mod = c.win_emu->mod_manager.find_by_address(rip);
                        const auto writer_offset = writer_mod ? rip - writer_mod->image_base : rip;

                        c.win_emu->log.error("[sldim-trypop-trace] WRITE to queue count field 0x%llx (at 0x%llx): new_value=%u "
                                             "size=%zu writer_rip=0x%llx (%s+0x%llx) tid=%u\n",
                                             static_cast<unsigned long long>(count_field_addr),
                                             static_cast<unsigned long long>(write_address), new_value, size,
                                             static_cast<unsigned long long>(rip), writer_mod_name,
                                             static_cast<unsigned long long>(writer_offset), c.win_emu->current_thread().id);
                    });
            }
        }

        void trace_sldim_trypop_count_check_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            const auto eax = emu.reg<uint32_t>(x86_register::eax);
            const auto ebx = emu.reg<uint32_t>(x86_register::ebx);

            ++g_sldim_trypop_total_hits;

            if (static_cast<uint64_t>(ebx) != g_sldim_trypop_last_count)
            {
                c.win_emu->log.error("[sldim-trypop-trace] hit at 0x%llx queue=0x%x count=%u changed from %llu at hit#%llu, tid=%u\n",
                                     static_cast<unsigned long long>(address), eax, ebx,
                                     static_cast<unsigned long long>(g_sldim_trypop_last_count),
                                     static_cast<unsigned long long>(g_sldim_trypop_total_hits), c.win_emu->current_thread().id);
                g_sldim_trypop_last_count = ebx;
            }

            if (static_cast<int32_t>(ebx) > 0)
            {
                ++g_sldim_trypop_count_nonzero_hits;
            }
            else if ((g_sldim_trypop_total_hits % 20000) == 0)
            {
                c.win_emu->log.error("[sldim-trypop-trace] checkpoint: total=%llu nonzero_count_hits=%llu last_count=%llu\n",
                                     static_cast<unsigned long long>(g_sldim_trypop_total_hits),
                                     static_cast<unsigned long long>(g_sldim_trypop_count_nonzero_hits),
                                     static_cast<unsigned long long>(g_sldim_trypop_last_count));
            }
        }

        void trace_sldim_cmsgthread_ctor_hit(const analysis_context& c, const uint64_t address, const bool two_arg)
        {
            auto& emu = c.win_emu->emu();
            const auto this_ptr = emu.reg<uint32_t>(x86_register::ecx);

            ++g_sldim_cmsgthread_ctor_hits;

            if (two_arg)
            {
                const auto ebp = emu.reg<uint32_t>(x86_register::ebp);
                uint32_t arg1{};
                uint32_t arg2{};
                emu.try_read_memory(ebp + 0x8, &arg1, sizeof(arg1));
                emu.try_read_memory(ebp + 0xc, &arg2, sizeof(arg2));

                c.win_emu->log.error("[sldim-cmsgthread-trace] 2-arg ctor hit at 0x%llx this=0x%x arg1(->+0x38)=0x%x "
                                     "arg2(->+0x34)=0x%x tid=%u ctor_hits=%llu\n",
                                     static_cast<unsigned long long>(address), this_ptr, arg1, arg2, c.win_emu->current_thread().id,
                                     static_cast<unsigned long long>(g_sldim_cmsgthread_ctor_hits));
            }
            else
            {
                c.win_emu->log.error("[sldim-cmsgthread-trace] 0-arg ctor hit at 0x%llx this=0x%x tid=%u ctor_hits=%llu\n",
                                     static_cast<unsigned long long>(address), this_ptr, c.win_emu->current_thread().id,
                                     static_cast<unsigned long long>(g_sldim_cmsgthread_ctor_hits));
            }
        }

        void trace_sldim_cmsgthread_dtor_entry_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            const auto this_ptr = emu.reg<uint32_t>(x86_register::ecx);

            ++g_sldim_cmsgthread_dtor_entry_hits;

            c.win_emu->log.error("[sldim-cmsgthread-trace] DESTRUCTOR ENTRY hit at 0x%llx this=0x%x tid=%u dtor_entry_hits=%llu\n",
                                 static_cast<unsigned long long>(address), this_ptr, c.win_emu->current_thread().id,
                                 static_cast<unsigned long long>(g_sldim_cmsgthread_dtor_entry_hits));
        }

        void trace_sldim_cmsgthread_dtor_check_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            const auto state_obj = emu.reg<uint32_t>(x86_register::eax);
            const auto this_ptr = emu.reg<uint32_t>(x86_register::esi);

            uint32_t field4{};
            const auto read_ok = emu.try_read_memory(state_obj + 4, &field4, sizeof(field4));
            const auto matches = read_ok && field4 == this_ptr;

            if (matches)
            {
                ++g_sldim_cmsgthread_dtor_match_hits;
            }

            c.win_emu->log.error("[sldim-cmsgthread-trace] DESTRUCTOR CHECK hit at 0x%llx state_obj=0x%x this=0x%x state_obj->field4=0x%x "
                                 "(read_ok=%d) MATCH=%d tid=%u match_hits=%llu\n",
                                 static_cast<unsigned long long>(address), state_obj, this_ptr, field4, read_ok ? 1 : 0, matches ? 1 : 0,
                                 c.win_emu->current_thread().id, static_cast<unsigned long long>(g_sldim_cmsgthread_dtor_match_hits));
        }

        void trace_sldim_pending_command_state_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            const auto state_obj = emu.reg<uint32_t>(x86_register::eax);

            if (state_obj == 0 || g_sldim_pending_command_write_watch_armed || c.win_emu->current_thread().id != 40)
            {
                return;
            }

            g_sldim_pending_command_write_watch_armed = true;
            const uint64_t field4_addr = state_obj + 4;
            c.win_emu->log.error(
                "[sldim-pendingcmd-trace] hit at 0x%llx, arming a write-watch on state_obj->field4 at 0x%llx (state_obj=0x%x) tid=%u\n",
                static_cast<unsigned long long>(address), static_cast<unsigned long long>(field4_addr), state_obj,
                c.win_emu->current_thread().id);

            emu.hook_memory_write(field4_addr, sizeof(uint32_t),
                                  [&c, field4_addr](cpu_interface&, const uint64_t write_address, const void* value, const size_t size) {
                                      uint32_t new_value{};
                                      memcpy(&new_value, value, std::min(size, sizeof(new_value)));

                                      const auto rip = c.win_emu->emu().read_instruction_pointer();
                                      const auto* writer_mod_name = c.win_emu->mod_manager.find_name(rip);
                                      const auto* writer_mod = c.win_emu->mod_manager.find_by_address(rip);
                                      const auto writer_offset = writer_mod ? rip - writer_mod->image_base : rip;

                                      c.win_emu->log.error(
                                          "[sldim-pendingcmd-trace] WRITE to state_obj->field4 0x%llx (at 0x%llx): new_value=0x%x "
                                          "size=%zu writer_rip=0x%llx (%s+0x%llx) tid=%u\n",
                                          static_cast<unsigned long long>(field4_addr), static_cast<unsigned long long>(write_address),
                                          new_value, size, static_cast<unsigned long long>(rip), writer_mod_name,
                                          static_cast<unsigned long long>(writer_offset), c.win_emu->current_thread().id);
                                  });
        }

        std::optional<uint64_t> read_x86_gp_register(x86_64_cpu& emu, const x86_reg reg)
        {
            switch (reg)
            {
            case X86_REG_EAX:
                return emu.reg<uint32_t>(x86_register::eax);
            case X86_REG_ECX:
                return emu.reg<uint32_t>(x86_register::ecx);
            case X86_REG_EDX:
                return emu.reg<uint32_t>(x86_register::edx);
            case X86_REG_EBX:
                return emu.reg<uint32_t>(x86_register::ebx);
            case X86_REG_ESP:
                return emu.reg<uint32_t>(x86_register::esp);
            case X86_REG_EBP:
                return emu.reg<uint32_t>(x86_register::ebp);
            case X86_REG_ESI:
                return emu.reg<uint32_t>(x86_register::esi);
            case X86_REG_EDI:
                return emu.reg<uint32_t>(x86_register::edi);
            default:
                uint64_t value{};
                if (read_x86_register_value(emu, reg, value))
                {
                    return value;
                }
                return std::nullopt;
            }
        }

        std::optional<uint64_t> resolve_indirect_operand_target(x86_64_cpu& emu, const cs_insn& insn)
        {
            const auto* detail = insn.detail;
            if (!detail || detail->x86.op_count == 0)
            {
                return std::nullopt;
            }

            const auto& op = detail->x86.operands[0];

            if (op.type == X86_OP_IMM)
            {
                return static_cast<uint64_t>(op.imm);
            }

            if (op.type == X86_OP_REG)
            {
                return read_x86_gp_register(emu, op.reg);
            }

            if (op.type != X86_OP_MEM)
            {
                return std::nullopt;
            }

            uint64_t address = static_cast<uint64_t>(op.mem.disp);

            if (op.mem.base == X86_REG_RIP)
            {
                address += insn.address + insn.size;
            }
            else if (op.mem.base != X86_REG_INVALID)
            {
                const auto base = read_x86_gp_register(emu, op.mem.base);
                if (!base)
                {
                    return std::nullopt;
                }
                address += *base;
            }

            if (op.mem.index != X86_REG_INVALID)
            {
                const auto index = read_x86_gp_register(emu, op.mem.index);
                if (!index)
                {
                    return std::nullopt;
                }
                address += *index * static_cast<uint64_t>(op.mem.scale);
            }

            const auto ptr_size = op.size != 0 ? static_cast<size_t>(op.size) : sizeof(uint32_t);
            uint64_t target{};
            if (ptr_size > sizeof(target) || !emu.try_read_memory(address, &target, ptr_size))
            {
                return std::nullopt;
            }

            return target;
        }

        struct shadow_reg_state
        {
            std::map<x86_reg, uint64_t> known;
            std::set<x86_reg> poisoned;
        };

        std::optional<uint64_t> shadow_read_reg(x86_64_cpu& emu, const shadow_reg_state& shadow, const x86_reg reg)
        {
            if (shadow.poisoned.contains(reg))
            {
                return std::nullopt;
            }

            const auto it = shadow.known.find(reg);
            if (it != shadow.known.end())
            {
                return it->second;
            }

            return read_x86_gp_register(emu, reg);
        }

        void shadow_write_reg(shadow_reg_state& shadow, const x86_reg reg, const uint64_t value)
        {
            shadow.poisoned.erase(reg);
            shadow.known[reg] = value;
        }

        void shadow_poison_reg(shadow_reg_state& shadow, const x86_reg reg)
        {
            shadow.known.erase(reg);
            shadow.poisoned.insert(reg);
        }

        std::optional<uint64_t> shadow_resolve_mem_address(x86_64_cpu& emu, const shadow_reg_state& shadow, const cs_x86_op& op,
                                                           const uint64_t next_insn_address)
        {
            if (op.type != X86_OP_MEM || op.mem.segment != X86_REG_INVALID)
            {
                return std::nullopt;
            }

            uint64_t address = static_cast<uint64_t>(op.mem.disp);

            if (op.mem.base == X86_REG_RIP)
            {
                address += next_insn_address;
            }
            else if (op.mem.base != X86_REG_INVALID)
            {
                const auto base = shadow_read_reg(emu, shadow, op.mem.base);
                if (!base)
                {
                    return std::nullopt;
                }
                address += *base;
            }

            if (op.mem.index != X86_REG_INVALID)
            {
                const auto index = shadow_read_reg(emu, shadow, op.mem.index);
                if (!index)
                {
                    return std::nullopt;
                }
                address += *index * static_cast<uint64_t>(op.mem.scale);
            }

            return address;
        }

        std::optional<uint64_t> shadow_read_operand(x86_64_cpu& emu, const shadow_reg_state& shadow, const cs_x86_op& op,
                                                    const size_t default_size, const uint64_t next_insn_address)
        {
            if (op.type == X86_OP_REG)
            {
                return shadow_read_reg(emu, shadow, op.reg);
            }

            if (op.type == X86_OP_IMM)
            {
                return static_cast<uint64_t>(op.imm);
            }

            if (op.type == X86_OP_MEM)
            {
                const auto addr = shadow_resolve_mem_address(emu, shadow, op, next_insn_address);
                if (!addr)
                {
                    return std::nullopt;
                }

                uint64_t value{};
                const auto read_size = op.size != 0 ? static_cast<size_t>(op.size) : default_size;
                if (read_size > sizeof(value) || !emu.try_read_memory(*addr, &value, read_size))
                {
                    return std::nullopt;
                }

                return value;
            }

            return std::nullopt;
        }

        std::optional<bool> resolve_condition_from_live_eflags(x86_64_cpu& emu, const x86_insn insn_id)
        {
            const auto eflags = emu.reg<uint32_t>(x86_register::eflags);
            const bool cf = (eflags & (1u << 0)) != 0;
            const bool pf = (eflags & (1u << 2)) != 0;
            const bool zf = (eflags & (1u << 6)) != 0;
            const bool sf = (eflags & (1u << 7)) != 0;
            const bool of = (eflags & (1u << 11)) != 0;

            switch (insn_id)
            {
            case X86_INS_JE:
                return zf;
            case X86_INS_JNE:
                return !zf;
            case X86_INS_JS:
                return sf;
            case X86_INS_JNS:
                return !sf;
            case X86_INS_JB:
                return cf;
            case X86_INS_JAE:
                return !cf;
            case X86_INS_JBE:
                return cf || zf;
            case X86_INS_JA:
                return !cf && !zf;
            case X86_INS_JL:
                return sf != of;
            case X86_INS_JGE:
                return sf == of;
            case X86_INS_JLE:
                return zf || (sf != of);
            case X86_INS_JG:
                return !zf && (sf == of);
            case X86_INS_JO:
                return of;
            case X86_INS_JNO:
                return !of;
            case X86_INS_JP:
                return pf;
            case X86_INS_JNP:
                return !pf;
            default:
                return std::nullopt;
            }
        }

        // Chases the guest's real control flow forward, one hop at a time, starting at sldim.exe's
        // NtUserGetMessage post-syscall return address (see project_solidworks_bringup.md #284/#285).
        // Each hop is only advanced once execution genuinely reaches it (handle_instruction re-fires this
        // function at the newly-armed g_sldim_dispatch_watch_va), so a WOW64 far-return's bitness switch is
        // always observed live rather than guessed. A plain RET/RETF's target is read off the live stack
        // pointer (adjusted by a simulated push/pop/add/sub delta tracked across this hop), a direct
        // CALL/JMP's target is read straight out of its immediate operand, and an indirect CALL/JMP hands
        // off to trace_sldim_dispatch_indirect_call_hit for live register resolution. A conditional branch
        // is resolved by micro-simulating the hop's own straight-line MOV/LEA/ADD/SUB/XOR-zero chain into a
        // small shadow register file (falling back to live register/memory reads for anything not yet
        // written in this hop), then evaluating the TEST/CMP that feeds it; anything not modeled poisons the
        // destination register so a later use of it correctly aborts resolution instead of guessing.
        void trace_sldim_dispatch_return_hit(const analysis_context& c, const uint64_t address)
        {
            ++g_sldim_dispatch_hop_count;

            auto& emu = c.win_emu->emu();
            const auto* mod_name = c.win_emu->mod_manager.find_name(address);
            const auto* mod = c.win_emu->mod_manager.find_by_address(address);
            const auto offset = mod ? address - mod->image_base : address;

            c.win_emu->log.error("[sldim-dispatch-trace] HOP %u: 0x%llx (%s+0x%llx) tid=%u\n", g_sldim_dispatch_hop_count,
                                 static_cast<unsigned long long>(address), mod_name, static_cast<unsigned long long>(offset),
                                 c.win_emu->current_thread().id);

            if (g_sldim_dispatch_hop_count >= SLDIM_DISPATCH_MAX_HOPS)
            {
                c.win_emu->log.error("[sldim-dispatch-trace] hop limit reached, stopping chase\n");
                g_sldim_dispatch_watch_va = 0;
                return;
            }

            std::array<uint8_t, 512> code{};
            if (!emu.try_read_memory(address, code.data(), code.size()))
            {
                c.win_emu->log.error("[sldim-dispatch-trace] failed to read guest memory, stopping chase\n");
                g_sldim_dispatch_watch_va = 0;
                return;
            }

            const auto reg_cs = emu.reg<uint16_t>(x86_register::cs);
            disassembler disasm{};
            const auto handle = disasm.resolve_handle(emu, reg_cs);
            const auto instructions = disasm.disassemble(emu, reg_cs, code, 64, address);

            const auto bitness = disassembler::get_segment_bitness(emu, reg_cs);
            const size_t ptr_size = (bitness && *bitness == disassembler::segment_bitness::bit64) ? sizeof(uint64_t) : sizeof(uint32_t);
            const auto is_stack_pointer_reg = [](const x86_reg reg) {
                return reg == X86_REG_RSP || reg == X86_REG_ESP || reg == X86_REG_SP;
            };

            shadow_reg_state shadow{};
            int64_t rsp_delta = 0;
            bool rsp_delta_unreliable = false;
            bool pending_flag_valid = false;
            bool pending_zero_flag = false;
            bool pending_carry_valid = false;
            bool pending_carry_flag = false;

            for (const auto& insn : instructions)
            {
                c.win_emu->log.error("[sldim-dispatch-trace]   0x%llx: %s %s\n", static_cast<unsigned long long>(insn.address),
                                     insn.mnemonic, insn.op_str);

                const bool is_first_instruction_in_hop = insn.address == address;
                const bool is_call = cs_insn_group(handle, &insn, CS_GRP_CALL);
                const bool is_jump = cs_insn_group(handle, &insn, CS_GRP_JUMP);
                const bool is_ret = cs_insn_group(handle, &insn, CS_GRP_RET);
                const bool is_unconditional_jump = insn.id == X86_INS_JMP || insn.id == X86_INS_LJMP;
                const bool is_flag_test = insn.id == X86_INS_TEST || insn.id == X86_INS_CMP;

                if (!is_call && !is_jump && !is_ret)
                {
                    const bool is_explicit_rsp_add_sub =
                        (insn.id == X86_INS_ADD || insn.id == X86_INS_SUB) && insn.detail && insn.detail->x86.op_count == 2 &&
                        insn.detail->x86.operands[0].type == X86_OP_REG && is_stack_pointer_reg(insn.detail->x86.operands[0].reg) &&
                        insn.detail->x86.operands[1].type == X86_OP_IMM;

                    if (insn.id == X86_INS_PUSH)
                    {
                        rsp_delta -= static_cast<int64_t>(ptr_size);
                    }
                    else if (insn.id == X86_INS_POP)
                    {
                        rsp_delta += static_cast<int64_t>(ptr_size);
                    }
                    else if (is_explicit_rsp_add_sub)
                    {
                        const auto imm = insn.detail->x86.operands[1].imm;
                        rsp_delta += (insn.id == X86_INS_ADD) ? imm : -imm;
                    }
                    else if (insn.detail)
                    {
                        for (uint8_t i = 0; i < insn.detail->regs_write_count; ++i)
                        {
                            if (is_stack_pointer_reg(static_cast<x86_reg>(insn.detail->regs_write[i])))
                            {
                                rsp_delta_unreliable = true;
                            }
                        }
                    }

                    const bool has_reg_dest =
                        insn.detail && insn.detail->x86.op_count >= 1 && insn.detail->x86.operands[0].type == X86_OP_REG;
                    const auto dest_reg = has_reg_dest ? insn.detail->x86.operands[0].reg : X86_REG_INVALID;
                    bool handled = false;

                    if (has_reg_dest && insn.id == X86_INS_MOV && insn.detail->x86.op_count == 2)
                    {
                        const auto value =
                            shadow_read_operand(emu, shadow, insn.detail->x86.operands[1], ptr_size, insn.address + insn.size);
                        if (value)
                        {
                            shadow_write_reg(shadow, dest_reg, *value);
                            handled = true;
                        }
                    }
                    else if (has_reg_dest && insn.id == X86_INS_LEA && insn.detail->x86.op_count == 2 &&
                             insn.detail->x86.operands[1].type == X86_OP_MEM)
                    {
                        const auto addr = shadow_resolve_mem_address(emu, shadow, insn.detail->x86.operands[1], insn.address + insn.size);
                        if (addr)
                        {
                            shadow_write_reg(shadow, dest_reg, *addr);
                            handled = true;
                        }
                    }
                    else if (has_reg_dest && (insn.id == X86_INS_ADD || insn.id == X86_INS_SUB) && insn.detail->x86.op_count == 2 &&
                             insn.detail->x86.operands[1].type == X86_OP_IMM && !is_stack_pointer_reg(dest_reg))
                    {
                        const auto lhs = shadow_read_reg(emu, shadow, dest_reg);
                        if (lhs)
                        {
                            const auto imm = insn.detail->x86.operands[1].imm;
                            shadow_write_reg(shadow, dest_reg, (insn.id == X86_INS_ADD) ? (*lhs + imm) : (*lhs - imm));
                            handled = true;
                        }
                    }
                    else if (has_reg_dest && insn.id == X86_INS_XOR && insn.detail->x86.op_count == 2 &&
                             insn.detail->x86.operands[1].type == X86_OP_REG && insn.detail->x86.operands[1].reg == dest_reg)
                    {
                        shadow_write_reg(shadow, dest_reg, 0);
                        handled = true;
                    }

                    if (!handled && insn.detail)
                    {
                        for (uint8_t i = 0; i < insn.detail->x86.op_count; ++i)
                        {
                            const auto& write_op = insn.detail->x86.operands[i];
                            if (write_op.type == X86_OP_REG && (write_op.access & CS_AC_WRITE) != 0)
                            {
                                shadow_poison_reg(shadow, write_op.reg);
                            }
                        }

                        for (uint8_t i = 0; i < insn.detail->regs_write_count; ++i)
                        {
                            shadow_poison_reg(shadow, static_cast<x86_reg>(insn.detail->regs_write[i]));
                        }
                    }
                }

                if (is_flag_test && insn.detail && insn.detail->x86.op_count == 2)
                {
                    pending_flag_valid = false;

                    const auto lhs = shadow_read_operand(emu, shadow, insn.detail->x86.operands[0], ptr_size, insn.address + insn.size);
                    const auto rhs = shadow_read_operand(emu, shadow, insn.detail->x86.operands[1], ptr_size, insn.address + insn.size);
                    if (lhs && rhs)
                    {
                        const uint64_t result = (insn.id == X86_INS_TEST) ? (*lhs & *rhs) : (*lhs - *rhs);
                        pending_zero_flag = result == 0;
                        pending_flag_valid = true;

                        c.win_emu->log.error("[sldim-dispatch-trace]     (resolved operands: 0x%llx, 0x%llx -> zero=%d)\n",
                                             static_cast<unsigned long long>(*lhs), static_cast<unsigned long long>(*rhs),
                                             pending_zero_flag ? 1 : 0);
                    }
                }

                const bool is_bit_test =
                    insn.id == X86_INS_BT || insn.id == X86_INS_BTR || insn.id == X86_INS_BTS || insn.id == X86_INS_BTC;

                if (is_bit_test && insn.detail && insn.detail->x86.op_count == 2)
                {
                    pending_carry_valid = false;

                    const auto base = shadow_read_operand(emu, shadow, insn.detail->x86.operands[0], ptr_size, insn.address + insn.size);
                    const auto bit_index =
                        shadow_read_operand(emu, shadow, insn.detail->x86.operands[1], ptr_size, insn.address + insn.size);
                    if (base && bit_index)
                    {
                        const auto operand_bits = static_cast<uint64_t>(
                            (insn.detail->x86.operands[0].size != 0 ? insn.detail->x86.operands[0].size : ptr_size) * 8);
                        const auto bit = *bit_index % operand_bits;
                        pending_carry_flag = ((*base >> bit) & 1) != 0;
                        pending_carry_valid = true;

                        c.win_emu->log.error("[sldim-dispatch-trace]     (resolved bit test: base=0x%llx bit=%llu -> carry=%d)\n",
                                             static_cast<unsigned long long>(*base), static_cast<unsigned long long>(bit),
                                             pending_carry_flag ? 1 : 0);
                    }
                }

                if (is_ret)
                {
                    if (rsp_delta_unreliable)
                    {
                        c.win_emu->log.error("[sldim-dispatch-trace]   RET, but an earlier instruction in this hop modified "
                                             "RSP in an unmodeled way; stopping chase\n");
                        g_sldim_dispatch_watch_va = 0;
                        return;
                    }

                    const auto rsp = static_cast<uint64_t>(static_cast<int64_t>(emu.read_stack_pointer()) + rsp_delta);

                    uint64_t next_addr{};
                    if (!emu.try_read_memory(rsp, &next_addr, ptr_size))
                    {
                        c.win_emu->log.error("[sldim-dispatch-trace]   RET, but failed to read the return address off the "
                                             "stack; stopping chase\n");
                        g_sldim_dispatch_watch_va = 0;
                        return;
                    }

                    c.win_emu->log.error("[sldim-dispatch-trace]   %s -> next hop 0x%llx (rsp=0x%llx, rsp_delta=%lld, ptr_size=%zu)\n",
                                         insn.mnemonic, static_cast<unsigned long long>(next_addr), static_cast<unsigned long long>(rsp),
                                         static_cast<long long>(rsp_delta), ptr_size);
                    g_sldim_dispatch_watch_va = next_addr;
                    return;
                }

                if (!is_call && !is_jump)
                {
                    continue;
                }

                if (!is_unconditional_jump && !is_call)
                {
                    const bool has_imm_target =
                        insn.detail && insn.detail->x86.op_count > 0 && insn.detail->x86.operands[0].type == X86_OP_IMM;

                    std::optional<bool> taken;
                    const char* resolution = "micro-simulated operands";

                    if (pending_flag_valid && (insn.id == X86_INS_JE || insn.id == X86_INS_JNE) && has_imm_target)
                    {
                        taken = (insn.id == X86_INS_JE) ? pending_zero_flag : !pending_zero_flag;
                    }
                    else if (pending_carry_valid && (insn.id == X86_INS_JB || insn.id == X86_INS_JAE) && has_imm_target)
                    {
                        taken = (insn.id == X86_INS_JB) ? pending_carry_flag : !pending_carry_flag;
                    }
                    else if (is_first_instruction_in_hop && has_imm_target)
                    {
                        taken = resolve_condition_from_live_eflags(emu, static_cast<x86_insn>(insn.id));
                        resolution = "live EFLAGS (genuinely just reached)";
                    }

                    if (taken)
                    {
                        const auto fallthrough = insn.address + insn.size;
                        const auto branch_target = static_cast<uint64_t>(insn.detail->x86.operands[0].imm);
                        const auto next_addr = *taken ? branch_target : fallthrough;

                        c.win_emu->log.error("[sldim-dispatch-trace]   conditional branch resolved via %s -> %s taken=%d, next hop "
                                             "0x%llx\n",
                                             resolution, insn.mnemonic, *taken ? 1 : 0, static_cast<unsigned long long>(next_addr));
                        g_sldim_dispatch_watch_va = next_addr;
                        return;
                    }

                    if (!is_first_instruction_in_hop)
                    {
                        c.win_emu->log.error("[sldim-dispatch-trace]   conditional branch reached mid-hop, cannot statically follow "
                                             "it -- deferring to when it's genuinely live -> next hop 0x%llx\n",
                                             static_cast<unsigned long long>(insn.address));
                        g_sldim_dispatch_watch_va = insn.address;
                        return;
                    }

                    c.win_emu->log.error("[sldim-dispatch-trace]   conditional branch reached, unresolvable even live (unsupported "
                                         "mnemonic or no immediate target); stopping chase\n");
                    g_sldim_dispatch_watch_va = 0;
                    return;
                }

                const auto has_imm_operand =
                    insn.detail && insn.detail->x86.op_count > 0 && insn.detail->x86.operands[0].type == X86_OP_IMM;
                const auto after_call = insn.address + insn.size;

                if (has_imm_operand && is_call)
                {
                    const auto target = static_cast<uint64_t>(insn.detail->x86.operands[0].imm);
                    const auto* target_mod_name = c.win_emu->mod_manager.find_name(target);
                    const auto* target_mod = c.win_emu->mod_manager.find_by_address(target);
                    const auto target_offset = target_mod ? target - target_mod->image_base : target;

                    const bool is_wndproc_invoke_target =
                        target_mod_name != nullptr &&
                        ((std::string_view(target_mod_name) == "user32.dll" &&
                          (target_offset == WNDPROC_INVOKE_RVA || target_offset == WNDPROC_INVOKE_CALLEE_RVA)) ||
                         (std::string_view(target_mod_name) == "sldim.exe" && target_offset == AFX_CALL_WND_PROC_RVA));

                    if (is_wndproc_invoke_target || g_sldim_dispatch_follow_next_indirect_call)
                    {
                        const auto reached_sldim = c.win_emu->mod_manager.executable->contains(target);
                        c.win_emu->log.error("[sldim-dispatch-trace]   CALL DIRECT target=0x%llx (%s+0x%llx), FOLLOWING INTO it "
                                             "(real WNDPROC invoke chain, not skipping) -> next hop 0x%llx%s\n",
                                             static_cast<unsigned long long>(target), target_mod_name,
                                             static_cast<unsigned long long>(target_offset), static_cast<unsigned long long>(target),
                                             reached_sldim ? " (genuinely reached sldim.exe's own image -- the real WNDPROC)" : "");
                        g_sldim_dispatch_follow_next_indirect_call = !reached_sldim;
                        g_sldim_dispatch_watch_va = target;
                        return;
                    }

                    c.win_emu->log.error("[sldim-dispatch-trace]   CALL DIRECT target=0x%llx (%s+0x%llx), skipping over (assumed to "
                                         "return) -> next hop 0x%llx\n",
                                         static_cast<unsigned long long>(target), target_mod_name,
                                         static_cast<unsigned long long>(target_offset), static_cast<unsigned long long>(after_call));
                    g_sldim_dispatch_watch_va = after_call;
                    return;
                }

                if (has_imm_operand)
                {
                    const auto target = static_cast<uint64_t>(insn.detail->x86.operands[0].imm);
                    const auto* target_mod_name = c.win_emu->mod_manager.find_name(target);
                    const auto* target_mod = c.win_emu->mod_manager.find_by_address(target);
                    const auto target_offset = target_mod ? target - target_mod->image_base : target;
                    c.win_emu->log.error("[sldim-dispatch-trace]   JMP DIRECT -> next hop 0x%llx (%s+0x%llx)\n",
                                         static_cast<unsigned long long>(target), target_mod_name,
                                         static_cast<unsigned long long>(target_offset));
                    g_sldim_dispatch_watch_va = target;
                    return;
                }

                const bool is_flat_mem_operand =
                    insn.detail && insn.detail->x86.op_count > 0 && insn.detail->x86.operands[0].type == X86_OP_MEM &&
                    insn.detail->x86.operands[0].mem.base == X86_REG_INVALID && insn.detail->x86.operands[0].mem.index == X86_REG_INVALID;

                if (is_flat_mem_operand)
                {
                    auto iat_target = resolve_indirect_operand_target(emu, insn);
                    if (iat_target)
                    {
                        resolve_jump_target(emu, *iat_target);
                    }

                    const auto* target_mod_name = iat_target ? c.win_emu->mod_manager.find_name(*iat_target) : "<unresolved>";
                    const auto* target_mod = iat_target ? c.win_emu->mod_manager.find_by_address(*iat_target) : nullptr;

                    if (target_mod)
                    {
                        const auto target_offset = *iat_target - target_mod->image_base;
                        c.win_emu->log.error("[sldim-dispatch-trace]   %s STATIC (IAT-slot) target=0x%llx (%s+0x%llx), skipping over -> "
                                             "next hop 0x%llx\n",
                                             is_call ? "CALL" : "JMP", static_cast<unsigned long long>(*iat_target), target_mod_name,
                                             static_cast<unsigned long long>(target_offset), static_cast<unsigned long long>(after_call));
                        g_sldim_dispatch_watch_va = is_call ? after_call : *iat_target;
                        return;
                    }
                }

                const auto* exe = c.win_emu->mod_manager.executable;
                const bool is_forced_windowproc_virtual_call =
                    exe != nullptr && insn.address == exe->image_base + CWND_WINDOWPROC_VIRTUAL_CALL_SITE_RVA;

                if (g_sldim_dispatch_follow_next_indirect_call || is_forced_windowproc_virtual_call)
                {
                    if (!is_first_instruction_in_hop)
                    {
                        c.win_emu->log.error("[sldim-dispatch-trace]   INDIRECT %s reached mid-hop, cannot resolve its live register "
                                             "operand yet -- deferring to when it's genuinely live -> next hop 0x%llx\n",
                                             is_call ? "CALL" : "JMP", static_cast<unsigned long long>(insn.address));
                        g_sldim_dispatch_watch_va = insn.address;
                        return;
                    }

                    auto live_target = resolve_indirect_operand_target(emu, insn);
                    if (live_target)
                    {
                        resolve_jump_target(emu, *live_target);
                        const auto* target_mod_name = c.win_emu->mod_manager.find_name(*live_target);
                        const auto* target_mod = c.win_emu->mod_manager.find_by_address(*live_target);
                        const auto target_offset = target_mod ? *live_target - target_mod->image_base : *live_target;
                        const auto reached_sldim = c.win_emu->mod_manager.executable->contains(*live_target);

                        c.win_emu->log.error("[sldim-dispatch-trace]   INDIRECT %s resolved live -> target=0x%llx (%s+0x%llx), "
                                             "FOLLOWING INTO it -> next hop 0x%llx%s\n",
                                             is_call ? "CALL" : "JMP", static_cast<unsigned long long>(*live_target), target_mod_name,
                                             static_cast<unsigned long long>(target_offset), static_cast<unsigned long long>(*live_target),
                                             reached_sldim ? " (genuinely reached sldim.exe's own image -- the real WNDPROC)" : "");

                        g_sldim_dispatch_follow_next_indirect_call = !reached_sldim;
                        g_sldim_dispatch_watch_va = *live_target;
                        return;
                    }

                    c.win_emu->log.error("[sldim-dispatch-trace]   INDIRECT %s could not be resolved even though genuinely reached "
                                         "live; falling back to the default skip-and-diagnose behavior\n",
                                         is_call ? "CALL" : "JMP");
                    g_sldim_dispatch_follow_next_indirect_call = false;
                }

                if (g_sldim_dispatch_indirect_call_va == 0)
                {
                    g_sldim_dispatch_indirect_call_va = insn.address;
                    g_sldim_dispatch_indirect_is_jmp = !is_call;
                    c.win_emu->log.error("[sldim-dispatch-trace]   INDIRECT %s, arming live register-resolution watch at 0x%llx, chase "
                                         "will resume at 0x%llx once it returns\n",
                                         is_call ? "CALL" : "JMP", static_cast<unsigned long long>(insn.address),
                                         static_cast<unsigned long long>(after_call));
                }

                g_sldim_dispatch_watch_va = is_call ? after_call : 0;
                return;
            }

            c.win_emu->log.error("[sldim-dispatch-trace]   no control transfer found in this window; stopping chase\n");
            g_sldim_dispatch_watch_va = 0;
        }

        void trace_sldim_dispatch_indirect_call_hit(const analysis_context& c, const uint64_t address)
        {
            if (c.win_emu->current_thread().id != 8)
            {
                return;
            }

            ++g_sldim_dispatch_indirect_call_hits;

            auto& emu = c.win_emu->emu();
            std::array<uint8_t, MAX_INSTRUCTION_BYTES> code{};
            if (!emu.try_read_memory(address, code.data(), code.size()))
            {
                return;
            }

            const auto reg_cs = emu.reg<uint16_t>(x86_register::cs);
            disassembler disasm{};
            const auto instructions = disasm.disassemble(emu, reg_cs, code, 1, address);
            if (instructions.empty())
            {
                return;
            }

            const auto& insn = instructions[0];
            auto target = resolve_indirect_operand_target(emu, insn);
            if (target)
            {
                resolve_jump_target(emu, *target);
            }

            const auto* target_mod_name = target ? c.win_emu->mod_manager.find_name(*target) : "<unresolved>";
            const auto* target_mod = target ? c.win_emu->mod_manager.find_by_address(*target) : nullptr;
            const auto target_offset = target_mod ? *target - target_mod->image_base : (target ? *target : 0);

            c.win_emu->log.error("[sldim-dispatch-trace] INDIRECT CALL hit #%u at 0x%llx (%s %s) tid=%u -> target=0x%llx (%s+0x%llx)\n",
                                 g_sldim_dispatch_indirect_call_hits, static_cast<unsigned long long>(address), insn.mnemonic, insn.op_str,
                                 c.win_emu->current_thread().id, target ? static_cast<unsigned long long>(*target) : 0ULL, target_mod_name,
                                 static_cast<unsigned long long>(target_offset));

            if (target)
            {
                std::array<uint8_t, 128> target_code{};
                if (emu.try_read_memory(*target, target_code.data(), target_code.size()))
                {
                    const auto target_instructions = disasm.disassemble(emu, reg_cs, target_code, 8, *target);
                    for (const auto& t_insn : target_instructions)
                    {
                        c.win_emu->log.error("[sldim-dispatch-trace]     target: 0x%llx: %s %s\n",
                                             static_cast<unsigned long long>(t_insn.address), t_insn.mnemonic, t_insn.op_str);
                    }
                }
            }

            if (g_sldim_dispatch_indirect_is_jmp)
            {
                c.win_emu->log.error("[sldim-dispatch-trace] indirect JMP (tail call, no return expected) - arming fallback watch for "
                                     "the next tid=8 instruction inside sldim.exe's own module\n");
                g_sldim_dispatch_watch_for_exe_entry = true;
            }

            g_sldim_dispatch_indirect_call_va = 0;
        }

        void handle_module_unload(const analysis_context& c, const mapped_module& mod)
        {
            c.emit_observation<module_unload_event>([&](auto& event) {
                event.path = mod.module_path.string();
                event.image_base = mod.image_base;
            });

            if (mod.name == "user32.dll" && std::getenv("SOGEN_TRACE_DISPATCHMESSAGE"))
            {
                c.win_emu->log.error("[dispatchmessage-trace] user32.dll UNLOADED at 0x%llx\n",
                                     static_cast<unsigned long long>(mod.image_base));
            }
        }

        void handle_fast_fail(const analysis_context& c, const uint32_t fail_code)
        {
            c.emit_observation<fast_fail_event>([&](auto& event) { event.fail_code = fail_code; });
        }

        bool is_thread_alive(const analysis_context& c, const uint32_t thread_id)
        {
            for (const auto& t : c.win_emu->process.threads | std::views::values)
            {
                if (t.id == thread_id)
                {
                    return true;
                }
            }

            return false;
        }

        void update_import_access(analysis_context& c, const uint64_t address)
        {
            if (c.accessed_imports.empty())
            {
                return;
            }

            const auto& t = c.win_emu->current_thread();
            for (auto entry = c.accessed_imports.begin(); entry != c.accessed_imports.end();)
            {
                auto& a = *entry;
                const auto is_same_thread = t.id == a.access_context.thread_id;

                if (is_same_thread && address == a.address)
                {
                    entry = c.accessed_imports.erase(entry);
                    continue;
                }

                constexpr auto inst_delay = 100u;
                const auto execution_delay_reached = is_same_thread && a.access_inst_count + inst_delay <= t.executed_instructions;

                if (!execution_delay_reached && is_thread_alive(c, a.access_context.thread_id))
                {
                    ++entry;
                    continue;
                }

                c.emit_observation<import_read_event>(a.access_context, [&](auto& event) {
                    event.resolved_address = a.address;
                    event.import_name = a.import_name;
                    event.import_module = a.import_module;
                });

                entry = c.accessed_imports.erase(entry);
            }
        }

        bool is_return(const disassembler& d, x86_64_cpu& emu, const uint64_t address)
        {
            std::array<uint8_t, MAX_INSTRUCTION_BYTES> instruction_bytes{};
            const auto result = emu.try_read_memory(address, instruction_bytes.data(), instruction_bytes.size());
            if (!result)
            {
                return false;
            }

            const auto reg_cs = emu.reg<uint16_t>(x86_register::cs);
            const auto instructions = d.disassemble(emu, reg_cs, instruction_bytes, 1, address);
            if (instructions.empty())
            {
                return false;
            }

            const auto handle = d.resolve_handle(emu, reg_cs);
            return cs_insn_group(handle, instructions.data(), CS_GRP_RET);
        }

        void record_instruction(analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            std::array<uint8_t, MAX_INSTRUCTION_BYTES> instruction_bytes{};
            const auto result = emu.try_read_memory(address, instruction_bytes.data(), instruction_bytes.size());
            if (!result)
            {
                return;
            }

            const auto reg_cs = emu.reg<uint16_t>(x86_register::cs);
            disassembler disasm{};
            const auto instructions = disasm.disassemble(emu, reg_cs, instruction_bytes, 1, address);
            if (instructions.empty())
            {
                return;
            }

            ++c.instructions[instructions[0].id];
        }

        uint64_t next_traced_call_count(analysis_context& c)
        {
            return ++c.traced_call_count;
        }

        bool break_before_traced_call(analysis_context& c, const uint64_t call_count)
        {
            if (!c.auto_break_before_call || *c.auto_break_before_call != call_count)
            {
                return false;
            }

            c.auto_break_before_call.reset();
            c.win_emu->stop();
            return true;
        }

        bool break_before_traced_syscall(analysis_context& c, const uint64_t call_count, const uint64_t address)
        {
            if (!break_before_traced_call(c, call_count))
            {
                return false;
            }

            c.syscall_to_resume_after_break = address;
            c.win_emu->emu().reg<uint64_t>(x86_register::rip, address - SYSCALL_INSTRUCTION_SIZE);
            return true;
        }

        void handle_section_first_execution(analysis_context& c, const mapped_module& binary, const mapped_section& section,
                                            const uint64_t address)
        {
            const auto is_main_exe = &binary == c.win_emu->mod_manager.executable;
            if (!c.has_reached_main && c.settings->concise_logging && !c.settings->silent && is_main_exe)
            {
                c.has_reached_main = true;
                c.win_emu->log.disable_output(false);
            }

            if (!c.settings->log_first_section_execution)
            {
                return;
            }

            c.emit_observation<section_first_execute_event>([&](auto& event) {
                event.module_name = binary.name;
                event.section_name = section.name;
                event.file_address = address - binary.image_base + binary.image_base_file;
            });
        }

        void handle_instruction(analysis_context& c, const uint64_t address)
        {
            if (g_accept_isolated_trace_va != 0 && address == g_accept_isolated_trace_va)
            {
                trace_accept_isolated_hit(c, address);
            }

            for (size_t i = 0; i < g_node_connect_trace_vas.size(); ++i)
            {
                if (g_node_connect_trace_vas[i] != 0 && address == g_node_connect_trace_vas[i])
                {
                    trace_node_connect_hit(c, address, NODE_CONNECT_TARGETS[i].name);
                }
            }

            if (g_establish_waiting_routers_trace_va != 0 && address == g_establish_waiting_routers_trace_va)
            {
                trace_establish_waiting_routers_hit(c, address);
            }

            for (size_t i = 0; i < g_router_link_trace_vas.size(); ++i)
            {
                if (g_router_link_trace_vas[i] != 0 && address == g_router_link_trace_vas[i])
                {
                    trace_node_connect_hit(c, address, ROUTER_LINK_TARGETS[i].name);
                }
            }

            if (g_delayload_failure_trace_va != 0 && address == g_delayload_failure_trace_va)
            {
                trace_delayload_failure_hit(c, address);
            }

            if (g_create_named_pipe_trace_va != 0 && address == g_create_named_pipe_trace_va)
            {
                trace_create_named_pipe_hit(c, address);
            }

            if (g_platform_channel_ctor_trace_va != 0 && address == g_platform_channel_ctor_trace_va)
            {
                trace_platform_channel_ctor_hit(c, address);
            }

            if (g_platform_channel_tracker_entry_trace_va != 0 && address == g_platform_channel_tracker_entry_trace_va)
            {
                trace_platform_channel_tracker_entry_hit(c, address);
            }

            if (g_platform_channel_delegate_call_trace_va != 0 && address == g_platform_channel_delegate_call_trace_va)
            {
                trace_platform_channel_delegate_call_hit(c, address);
            }

            if (g_platform_channel_delegate_return_trace_va != 0 && address == g_platform_channel_delegate_return_trace_va)
            {
                trace_platform_channel_delegate_return_hit(c, address);
            }

            if (!g_sldim_queue_check_checked && std::getenv("SOGEN_TRACE_SLDIM_QUEUE"))
            {
                const auto* exe = c.win_emu->mod_manager.executable;
                if (exe != nullptr && exe->name == "sldim.exe")
                {
                    g_sldim_queue_check_checked = true;
                    g_sldim_queue_check_trace_va_1 = exe->image_base + SLDIM_QUEUE_CHECK_RVA_1;
                    g_sldim_queue_check_trace_va_2 = exe->image_base + SLDIM_QUEUE_CHECK_RVA_2;
                    g_sldim_oncommand_trace_va = exe->image_base + SLDIM_ONCOMMAND_CALL_RVA;
                    g_sldim_oncommand_branch_trace_va = exe->image_base + SLDIM_ONCOMMAND_BRANCH_RVA;
                    g_sldim_oncmdmsg_trace_va = exe->image_base + SLDIM_ONCMDMSG_CALL_RVA;
                    g_sldim_findentry_trace_va = exe->image_base + SLDIM_FINDENTRY_RESULT_RVA;
                    g_sldim_handler_delegate_trace_va = exe->image_base + SLDIM_HANDLER_DELEGATE_CALL_RVA;
                    g_sldim_trypop_entry_trace_va = exe->image_base + SLDIM_TRYPOP_ENTRY_RVA;
                    g_sldim_trypop_count_check_trace_va = exe->image_base + SLDIM_TRYPOP_COUNT_CHECK_RVA;
                    g_sldim_cmsgthread_ctor2_trace_va = exe->image_base + SLDIM_CMSGTHREAD_CTOR2_RVA;
                    g_sldim_cmsgthread_ctor0_trace_va = exe->image_base + SLDIM_CMSGTHREAD_CTOR0_RVA;
                    g_sldim_cmsgthread_dtor_entry_trace_va = exe->image_base + SLDIM_CMSGTHREAD_DTOR_ENTRY_RVA;
                    g_sldim_cmsgthread_dtor_check_trace_va = exe->image_base + SLDIM_CMSGTHREAD_DTOR_CHECK_RVA;
                    g_sldim_get_pending_command_state_trace_va = exe->image_base + SLDIM_GET_PENDING_COMMAND_STATE_RVA;
                    c.win_emu->log.error(
                        "[sldim-queue-trace] sldim.exe running at 0x%llx, watching queue-check at 0x%llx / 0x%llx, OnCommand at "
                        "0x%llx, OnCommand branch at 0x%llx, OnCmdMsg at 0x%llx, findEntry result at 0x%llx, handler delegate at "
                        "0x%llx, trypop entry at 0x%llx, trypop count check at 0x%llx, CMessagingThread ctor2/ctor0 at "
                        "0x%llx / 0x%llx, dtor entry/check at 0x%llx / 0x%llx, get_pending_command state at 0x%llx\n",
                        static_cast<unsigned long long>(exe->image_base), static_cast<unsigned long long>(g_sldim_queue_check_trace_va_1),
                        static_cast<unsigned long long>(g_sldim_queue_check_trace_va_2),
                        static_cast<unsigned long long>(g_sldim_oncommand_trace_va),
                        static_cast<unsigned long long>(g_sldim_oncommand_branch_trace_va),
                        static_cast<unsigned long long>(g_sldim_oncmdmsg_trace_va),
                        static_cast<unsigned long long>(g_sldim_findentry_trace_va),
                        static_cast<unsigned long long>(g_sldim_handler_delegate_trace_va),
                        static_cast<unsigned long long>(g_sldim_trypop_entry_trace_va),
                        static_cast<unsigned long long>(g_sldim_trypop_count_check_trace_va),
                        static_cast<unsigned long long>(g_sldim_cmsgthread_ctor2_trace_va),
                        static_cast<unsigned long long>(g_sldim_cmsgthread_ctor0_trace_va),
                        static_cast<unsigned long long>(g_sldim_cmsgthread_dtor_entry_trace_va),
                        static_cast<unsigned long long>(g_sldim_cmsgthread_dtor_check_trace_va),
                        static_cast<unsigned long long>(g_sldim_get_pending_command_state_trace_va));
                }
            }

            if ((g_sldim_queue_check_trace_va_1 != 0 && address == g_sldim_queue_check_trace_va_1) ||
                (g_sldim_queue_check_trace_va_2 != 0 && address == g_sldim_queue_check_trace_va_2))
            {
                trace_sldim_queue_check_hit(c, address);
            }

            if (g_sldim_oncommand_trace_va != 0 && address == g_sldim_oncommand_trace_va)
            {
                trace_sldim_oncommand_hit(c, address);
            }

            if (g_sldim_oncommand_branch_trace_va != 0 && address == g_sldim_oncommand_branch_trace_va)
            {
                trace_sldim_oncommand_branch_hit(c, address);
            }

            if (g_sldim_oncmdmsg_trace_va != 0 && address == g_sldim_oncmdmsg_trace_va)
            {
                trace_sldim_oncmdmsg_hit(c, address);
            }

            if (g_sldim_findentry_trace_va != 0 && address == g_sldim_findentry_trace_va)
            {
                trace_sldim_findentry_result_hit(c, address);
            }

            if (g_sldim_handler_delegate_trace_va != 0 && address == g_sldim_handler_delegate_trace_va)
            {
                trace_sldim_handler_delegate_hit(c, address);
            }

            if (g_sldim_trypop_entry_trace_va != 0 && address == g_sldim_trypop_entry_trace_va)
            {
                trace_sldim_trypop_entry_hit(c, address);
            }

            if (g_sldim_trypop_count_check_trace_va != 0 && address == g_sldim_trypop_count_check_trace_va)
            {
                trace_sldim_trypop_count_check_hit(c, address);
            }

            if (g_sldim_cmsgthread_ctor2_trace_va != 0 && address == g_sldim_cmsgthread_ctor2_trace_va)
            {
                trace_sldim_cmsgthread_ctor_hit(c, address, true);
            }

            if (g_sldim_cmsgthread_ctor0_trace_va != 0 && address == g_sldim_cmsgthread_ctor0_trace_va)
            {
                trace_sldim_cmsgthread_ctor_hit(c, address, false);
            }

            if (g_sldim_cmsgthread_dtor_entry_trace_va != 0 && address == g_sldim_cmsgthread_dtor_entry_trace_va)
            {
                trace_sldim_cmsgthread_dtor_entry_hit(c, address);
            }

            if (g_sldim_cmsgthread_dtor_check_trace_va != 0 && address == g_sldim_cmsgthread_dtor_check_trace_va)
            {
                trace_sldim_cmsgthread_dtor_check_hit(c, address);
            }

            if (g_sldim_get_pending_command_state_trace_va != 0 && address == g_sldim_get_pending_command_state_trace_va)
            {
                trace_sldim_pending_command_state_hit(c, address);
            }

            if (is_thread_activity_traced_tid(c.win_emu->current_thread().id))
            {
                if (g_tpp_worker_wait_return_va != 0 && address == g_tpp_worker_wait_return_va)
                {
                    trace_tpp_worker_wait_return_hit(c, c.win_emu->current_thread().id);
                }

                if (g_tpp_worker_key_check_va != 0 && address == g_tpp_worker_key_check_va)
                {
                    trace_tpp_worker_key_check_hit(c, c.win_emu->current_thread().id);
                }

                if (g_tpp_worker_ebx_loaded_va != 0 && address == g_tpp_worker_ebx_loaded_va)
                {
                    trace_tpp_worker_ebx_loaded_hit(c, c.win_emu->current_thread().id);
                }

                if (g_tpp_worker_generic_call_va != 0 && address == g_tpp_worker_generic_call_va)
                {
                    trace_tpp_worker_generic_call_hit(c, c.win_emu->current_thread().id);
                }

                if (g_tpp_worker_no_local_work_helper_return_va != 0 && address == g_tpp_worker_no_local_work_helper_return_va)
                {
                    trace_tpp_worker_no_local_work_helper_return_hit(c, c.win_emu->current_thread().id);
                }

                if (g_tpp_worker_release_or_loop_va != 0 && address == g_tpp_worker_release_or_loop_va)
                {
                    trace_tpp_worker_release_or_loop_hit(c, c.win_emu->current_thread().id);
                }

                if (g_tpp_wait_callback_loaded_va != 0 && address == g_tpp_wait_callback_loaded_va)
                {
                    trace_tpp_wait_callback_loaded_hit(c, c.win_emu->current_thread().id);
                }

                if (g_tp_wait_gate_test_va != 0 && address == g_tp_wait_gate_test_va)
                {
                    trace_tp_wait_gate_test_hit(c, c.win_emu->current_thread().id);
                }

                if (g_tp_wait_gate_rearm_taken_va != 0 && address == g_tp_wait_gate_rearm_taken_va)
                {
                    trace_tp_wait_gate_rearm_taken_hit(c, c.win_emu->current_thread().id);
                }

                if (g_tp_alloc_wait_zero_check_va != 0 && address == g_tp_alloc_wait_zero_check_va)
                {
                    trace_tp_alloc_wait_zero_check_hit(c, c.win_emu->current_thread().id);
                }

                if (g_tpsetwaitex_entry_va != 0 && address == g_tpsetwaitex_entry_va)
                {
                    trace_tpsetwaitex_entry_hit(c, c.win_emu->current_thread().id);
                }

                if (g_tpsetwaitex_finalize_clear_va != 0 && address == g_tpsetwaitex_finalize_clear_va)
                {
                    trace_tpsetwaitex_finalize_clear_hit(c, c.win_emu->current_thread().id);
                }

                if (g_tp_wait_legacy_bridge_callback_loaded_va != 0 && address == g_tp_wait_legacy_bridge_callback_loaded_va)
                {
                    trace_tp_wait_legacy_bridge_callback_loaded_hit(c, c.win_emu->current_thread().id);
                }

                if (g_ebwv_post_task_call_va != 0 && address == g_ebwv_post_task_call_va)
                {
                    trace_ebwv_post_task_call_hit(c, c.win_emu->current_thread().id);
                }

                if (g_ebwv_post_task_virtual_call_va != 0 && address == g_ebwv_post_task_virtual_call_va)
                {
                    trace_ebwv_post_task_virtual_call_hit(c, c.win_emu->current_thread().id);
                }

                if (g_ebwv_signaler_delegate_dispatch_va != 0 && address == g_ebwv_signaler_delegate_dispatch_va)
                {
                    trace_ebwv_signaler_delegate_dispatch_hit(c, c.win_emu->current_thread().id);
                }

                if (g_ebwv_env_arg_validate_va != 0 && address == g_ebwv_env_arg_validate_va)
                {
                    trace_ebwv_env_arg_validate_hit(c, c.win_emu->current_thread().id);
                }

                if (g_ebwv_env_completion_invoke_va != 0 && address == g_ebwv_env_completion_invoke_va)
                {
                    trace_ebwv_env_completion_invoke_hit(c, c.win_emu->current_thread().id);
                }

                if (g_ebwv_env_getoverlappedresult_call_va != 0 && address == g_ebwv_env_getoverlappedresult_call_va)
                {
                    trace_ebwv_env_getoverlappedresult_call_hit(c, c.win_emu->current_thread().id);
                }

                if (g_ebwv_env_getoverlappedresult_return_va != 0 && address == g_ebwv_env_getoverlappedresult_return_va)
                {
                    trace_ebwv_env_getoverlappedresult_return_hit(c, c.win_emu->current_thread().id);
                }

                if (g_ebwv_env_outcome_invoke_arg_va != 0 && address == g_ebwv_env_outcome_invoke_arg_va)
                {
                    trace_ebwv_env_outcome_invoke_arg_hit(c, c.win_emu->current_thread().id);
                }

                arm_ipcz_connect_watches(c);

                if (g_ipcz_connect_node_wrapper_va != 0 && address == g_ipcz_connect_node_wrapper_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id, "ipcz::Node::ConnectNode (wrapper)");
                }

                if (g_ipcz_connect_node_real_body_va != 0 && address == g_ipcz_connect_node_real_body_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id, "ipcz::Node::ConnectNode (real body)");
                }

                if (g_ipcz_node_connector_for_referrer_connect_va != 0 && address == g_ipcz_node_connector_for_referrer_connect_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id,
                                           "ipcz::(anonymous namespace)::NodeConnectorForReferrer::Connect");
                }

                if (g_ipcz_node_connector_for_referrer_no_broker_link_branch_va != 0 &&
                    address == g_ipcz_node_connector_for_referrer_no_broker_link_branch_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id,
                                           "ipcz::(anonymous namespace)::NodeConnectorForReferrer::Connect (no-broker-link branch taken)");
                }

                if (g_ipcz_refer_non_broker_va != 0 && address == g_ipcz_refer_non_broker_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id, "ipcz::NodeLink::ReferNonBroker");
                }

                if (g_ipcz_connect_node_internal_caller_va != 0 && address == g_ipcz_connect_node_internal_caller_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id, "ConnectNode's internal caller (+0x84f76)");
                }

                if (g_ipcz_connect_node_internal_caller_equality_check_taken_va != 0 &&
                    address == g_ipcz_connect_node_internal_caller_equality_check_taken_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id,
                                           "ConnectNode's internal caller (+0x84f76) equality check TAKEN (+0x85029)");
                }

                if (g_ipcz_connect_node_internal_caller_equality_check_not_taken_va != 0 &&
                    address == g_ipcz_connect_node_internal_caller_equality_check_not_taken_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id,
                                           "ConnectNode's internal caller (+0x84f76) equality check NOT TAKEN (+0x84ff7)");
                }

                if (g_ipcz_connect_node_internal_caller_site_flush_real_va != 0 &&
                    address == g_ipcz_connect_node_internal_caller_site_flush_real_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id,
                                           "ConnectNode internal caller's flush-pending-completions call site, real item (+0x84c52)");
                }

                if (g_ipcz_connect_node_internal_caller_site_flush_empty_va != 0 &&
                    address == g_ipcz_connect_node_internal_caller_site_flush_empty_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id,
                                           "ConnectNode internal caller's flush-pending-completions call site, empty/null (+0x84c96)");
                }

                if (g_ipcz_connect_node_mutual_recursion_entry_va != 0 && address == g_ipcz_connect_node_mutual_recursion_entry_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id,
                                           "ConnectNode internal caller's mutually-recursive function entry (+0x8e120)");
                }

                if (g_ipcz_connect_node_mutual_recursion_site_1_va != 0 && address == g_ipcz_connect_node_mutual_recursion_site_1_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id,
                                           "ConnectNode internal caller's mutual-recursion call site 1 (+0x8e41a)");
                }

                if (g_ipcz_connect_node_mutual_recursion_site_2_va != 0 && address == g_ipcz_connect_node_mutual_recursion_site_2_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id,
                                           "ConnectNode internal caller's mutual-recursion call site 2 (+0x8e44f)");
                }

                if (g_ipcz_channel_win_factory_va != 0 && address == g_ipcz_channel_win_factory_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id,
                                           "mojo::core::ChannelWin virtual method that validates and constructs the "
                                           "+0x43b4ec wrapper (+0x7d9d0)");
                }

                if (g_ipcz_connect_wrapper_ctor_va != 0 && address == g_ipcz_connect_wrapper_ctor_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id, "+0x43b4ec wrapper's real constructor (+0x845f0)");
                }

                if (g_ipcz_connect_wrapper_dtor_va != 0 && address == g_ipcz_connect_wrapper_dtor_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id, "+0x43b4ec wrapper's destructor (+0x84724)");
                }

                if (g_ipcz_connect_wrapper_scalar_deleting_dtor_va != 0 && address == g_ipcz_connect_wrapper_scalar_deleting_dtor_va)
                {
                    trace_ipcz_connect_hit(c, c.win_emu->current_thread().id,
                                           "+0x43b4ec wrapper's scalar deleting destructor thunk (+0x89580)");
                }

                arm_start_watching_once_watches(c);

                for (size_t i = 0; i < START_WATCHING_ONCE_CALLER_COUNT; ++i)
                {
                    if (g_start_watching_once_caller_vas[i] != 0 && address == g_start_watching_once_caller_vas[i])
                    {
                        trace_start_watching_once_hit(c, c.win_emu->current_thread().id, i);
                    }
                }

                arm_ebwv_connect_named_pipe_watches(c);

                for (size_t i = 0; i < EBWV_CONNECT_NAMED_PIPE_CALL_COUNT; ++i)
                {
                    if (g_ebwv_connect_named_pipe_call_vas[i] != 0 && address == g_ebwv_connect_named_pipe_call_vas[i])
                    {
                        trace_ebwv_connect_named_pipe_hit(c, c.win_emu->current_thread().id, i);
                    }
                }
            }

            // These DComp-related hit checks (#338/#339/#340) were previously nested inside the
            // is_thread_activity_traced_tid(...) gate above, the same false-negative-inducing mistake
            // #332 already found and fixed for trace_module_entry_if_new: that gate only ever contains
            // the single tid that issued a mojo.-prefixed FSCTL_PIPE_LISTEN (or its worker-factory
            // extension), which has no relationship to whichever thread actually runs
            // EmbeddedBrowserWebView.dll's compositor code -- so with SOGEN_TRACE_THREAD_ACTIVITY unset
            // (the norm for a DComp-focused run), every one of these checks was unconditionally dead,
            // regardless of whether the watched code itself ran. Moved out here so they fire for
            // whichever real thread executes the watched address, matching their own arm functions
            // (which already have no thread-activity requirement).
            if (g_ebwv_dcomp_device2_wrapper_entry_va != 0 && address == g_ebwv_dcomp_device2_wrapper_entry_va)
            {
                trace_ebwv_dcomp_device2_wrapper_entry_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_dcomp_device2_resolve_call_va != 0 && address == g_ebwv_dcomp_device2_resolve_call_va)
            {
                trace_ebwv_dcomp_device2_resolve_call_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_dcomp_device2_invoke_call_va != 0 && address == g_ebwv_dcomp_device2_invoke_call_va)
            {
                trace_ebwv_dcomp_device2_invoke_call_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_dcomp_device2_invoke_return_va != 0 && address == g_ebwv_dcomp_device2_invoke_return_va)
            {
                trace_ebwv_dcomp_device2_invoke_return_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_dcomp_gate1_entry_va != 0 && address == g_ebwv_dcomp_gate1_entry_va)
            {
                trace_ebwv_dcomp_gate_entry_hit(c, c.win_emu->current_thread().id, 1);
            }

            if (g_ebwv_dcomp_gate1_args_va != 0 && address == g_ebwv_dcomp_gate1_args_va)
            {
                trace_ebwv_dcomp_gate1_args_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_dcomp_gate1_result_va != 0 && address == g_ebwv_dcomp_gate1_result_va)
            {
                trace_ebwv_dcomp_gate_result_hit(c, c.win_emu->current_thread().id, 1, EBWV_DCOMP_GATE1_OFFSET);
            }

            if (g_ebwv_gate1_lazy_helper_entry_va != 0 && address == g_ebwv_gate1_lazy_helper_entry_va)
            {
                trace_ebwv_gate1_lazy_helper_entry_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_gate1_lazy_helper_return_va != 0 && address == g_ebwv_gate1_lazy_helper_return_va)
            {
                trace_ebwv_gate1_lazy_helper_return_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_gate1_accessor_entry_va != 0 && address == g_ebwv_gate1_accessor_entry_va)
            {
                trace_ebwv_gate1_accessor_entry_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_gate1_accessor_dcomp_invoke_va != 0 && address == g_ebwv_gate1_accessor_dcomp_invoke_va)
            {
                trace_ebwv_gate1_accessor_dcomp_invoke_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_gate1_accessor_dcomp_invoke_return_va != 0 && address == g_ebwv_gate1_accessor_dcomp_invoke_return_va)
            {
                trace_ebwv_gate1_accessor_dcomp_invoke_return_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_gate1_accessor_result_va != 0 && address == g_ebwv_gate1_accessor_result_va)
            {
                trace_ebwv_gate1_accessor_result_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_dcomp_gate2_entry_va != 0 && address == g_ebwv_dcomp_gate2_entry_va)
            {
                trace_ebwv_dcomp_gate_entry_hit(c, c.win_emu->current_thread().id, 2);
            }

            if (g_ebwv_dcomp_gate2_args_va != 0 && address == g_ebwv_dcomp_gate2_args_va)
            {
                trace_ebwv_dcomp_gate23_args_hit(c, c.win_emu->current_thread().id, 2);
            }

            if (g_ebwv_dcomp_gate2_result_va != 0 && address == g_ebwv_dcomp_gate2_result_va)
            {
                trace_ebwv_dcomp_gate_result_hit(c, c.win_emu->current_thread().id, 2, EBWV_DCOMP_GATE2_OFFSET);
            }

            if (g_ebwv_dcomp_gate3_entry_va != 0 && address == g_ebwv_dcomp_gate3_entry_va)
            {
                trace_ebwv_dcomp_gate_entry_hit(c, c.win_emu->current_thread().id, 3);
            }

            if (g_ebwv_dcomp_gate3_args_va != 0 && address == g_ebwv_dcomp_gate3_args_va)
            {
                trace_ebwv_dcomp_gate23_args_hit(c, c.win_emu->current_thread().id, 3);
            }

            if (g_ebwv_dcomp_gate3_result_va != 0 && address == g_ebwv_dcomp_gate3_result_va)
            {
                trace_ebwv_dcomp_gate_result_hit(c, c.win_emu->current_thread().id, 3, EBWV_DCOMP_GATE3_OFFSET);
            }

            arm_ebwv_dcomp_gate_caller_watches(c);

            for (size_t i = 0; i < EBWV_DCOMP_GATE_CALLER_COUNT; ++i)
            {
                if (g_ebwv_dcomp_gate_caller_vas[i] != 0 && address == g_ebwv_dcomp_gate_caller_vas[i])
                {
                    trace_ebwv_dcomp_gate_caller_hit(c, c.win_emu->current_thread().id, i);
                }
            }

            if (g_ebwv_dcomp_gate1_up1_retry_status_va != 0 && address == g_ebwv_dcomp_gate1_up1_retry_status_va)
            {
                trace_ebwv_dcomp_gate1_up1_retry_status_hit(c, c.win_emu->current_thread().id);
            }

            arm_ebwv_holder_submit_watches(c);

            if (g_ebwv_holder_submit_entry_va != 0 && address == g_ebwv_holder_submit_entry_va)
            {
                trace_ebwv_holder_submit_entry_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_holder_submit_check1_return_va != 0 && address == g_ebwv_holder_submit_check1_return_va)
            {
                trace_ebwv_holder_submit_check1_return_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_holder_submit_check2_return_va != 0 && address == g_ebwv_holder_submit_check2_return_va)
            {
                trace_ebwv_holder_submit_check2_return_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_holder_submit_write1_va != 0 && address == g_ebwv_holder_submit_write1_va)
            {
                trace_ebwv_holder_submit_write1_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_holder_submit_write2_va != 0 && address == g_ebwv_holder_submit_write2_va)
            {
                trace_ebwv_holder_submit_write2_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_holder_check2_cache_result_va != 0 && address == g_ebwv_holder_check2_cache_result_va)
            {
                trace_ebwv_holder_check2_cache_result_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_holder_check2_getlasterror_return_va != 0 && address == g_ebwv_holder_check2_getlasterror_return_va)
            {
                trace_ebwv_holder_check2_getlasterror_return_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_holder_watchdog_dispatch_entry_va != 0 && address == g_ebwv_holder_watchdog_dispatch_entry_va)
            {
                trace_ebwv_holder_watchdog_dispatch_entry_hit(c, c.win_emu->current_thread().id);
            }

            arm_ebwv_prop58_accessor_watches(c);

            if (g_ebwv_prop58_setter_entry_va != 0 && address == g_ebwv_prop58_setter_entry_va)
            {
                trace_ebwv_prop58_setter_entry_hit(c, c.win_emu->current_thread().id);
            }

            if (g_ebwv_prop58_getter_entry_va != 0 && address == g_ebwv_prop58_getter_entry_va)
            {
                trace_ebwv_prop58_getter_entry_hit(c, c.win_emu->current_thread().id);
            }

            if (std::getenv("SOGEN_TRACE_EBWV_MSEDGE_XMODULE"))
            {
                trace_ebwv_msedge_xmodule_crossing(c, c.win_emu->current_thread().id, address);
            }

            if (std::getenv("SOGEN_TRACE_MODULE_ENTRY"))
            {
                trace_module_entry_if_new(c, c.win_emu->current_thread().id, address);
            }

            if (!g_dispatch_message_w_entry_armed)
            {
                for (size_t i = 0; i < g_dispatch_message_w_entry_count; ++i)
                {
                    if (g_dispatch_message_w_entry_vas[i] != 0 && address == g_dispatch_message_w_entry_vas[i])
                    {
                        g_dispatch_message_w_entry_armed = true;
                        c.win_emu->log.error("[dispatchmessage-trace] REACHED DispatchMessageW's own entry (candidate #%zu) at "
                                             "0x%llx, tid=%u, starting chase\n",
                                             i + 1, static_cast<unsigned long long>(address), c.win_emu->current_thread().id);
                        g_sldim_dispatch_hop_count = 0;
                        trace_sldim_dispatch_return_hit(c, address);
                        break;
                    }
                }
            }

            if (!g_dispatch_message_w_internal_armed)
            {
                for (size_t i = 0; i < g_dispatch_message_w_internal_count; ++i)
                {
                    if (g_dispatch_message_w_internal_vas[i] != 0 && address == g_dispatch_message_w_internal_vas[i])
                    {
                        auto& emu = c.win_emu->emu();
                        const auto lp_msg = emu.reg<uint32_t>(x86_register::ecx);
                        uint32_t message{};
                        if (emu.try_read_memory(lp_msg + 4, &message, sizeof(message)) && message == WM_COMMAND)
                        {
                            g_dispatch_message_w_internal_armed = true;
                            c.win_emu->log.error("[dispatchmessage-trace] REACHED DispatchMessageW's internal worker (candidate "
                                                 "#%zu) at 0x%llx for a WM_COMMAND message (lpMsg=0x%llx), tid=%u, starting "
                                                 "chase\n",
                                                 i + 1, static_cast<unsigned long long>(address), static_cast<unsigned long long>(lp_msg),
                                                 c.win_emu->current_thread().id);
                            g_sldim_dispatch_hop_count = 0;
                            trace_sldim_dispatch_return_hit(c, address);
                        }
                        break;
                    }
                }
            }

            if (g_sldim_dispatch_watch_va != 0 && address == g_sldim_dispatch_watch_va)
            {
                trace_sldim_dispatch_return_hit(c, address);
            }

            if (g_sldim_dispatch_indirect_call_va != 0 && address == g_sldim_dispatch_indirect_call_va)
            {
                trace_sldim_dispatch_indirect_call_hit(c, address);
            }

            if (g_sldim_dispatch_watch_for_exe_entry && c.win_emu->current_thread().id == 8 &&
                c.win_emu->mod_manager.executable->contains(address))
            {
                g_sldim_dispatch_watch_for_exe_entry = false;
                const auto* exe = c.win_emu->mod_manager.executable;
                c.win_emu->log.error("[sldim-dispatch-trace] REACHED sldim.exe's own code at 0x%llx (sldim.exe+0x%llx) after the WOW64 "
                                     "CPU-transition jump, tid=8, continuing chase\n",
                                     static_cast<unsigned long long>(address), static_cast<unsigned long long>(address - exe->image_base));
                trace_sldim_dispatch_return_hit(c, address);
            }

            auto& win_emu = *c.win_emu;
            update_import_access(c, address);

#if defined(OS_EMSCRIPTEN) && !defined(SOGEN_EMSCRIPTEN_SUPPORT_NODEJS)
            if ((win_emu.get_executed_instructions() % 0x20000) == 0)
            {
                debugger::event_context ec{.win_emu = win_emu};
                debugger::handle_events(ec);
            }
#endif

            const auto& current_thread = c.win_emu->current_thread();
            const auto previous_ip = current_thread.previous_ip;
            [[maybe_unused]] const auto current_ip = current_thread.current_ip;
            const auto is_main_exe = win_emu.mod_manager.executable->contains(address);
            const auto is_previous_main_exe = win_emu.mod_manager.executable->contains(previous_ip);

            const auto binary = utils::make_lazy([&] {
                if (is_main_exe)
                {
                    return win_emu.mod_manager.executable;
                }

                return win_emu.mod_manager.find_by_address(address); //
            });

            const auto previous_binary = utils::make_lazy([&] {
                if (is_previous_main_exe)
                {
                    return win_emu.mod_manager.executable;
                }

                return win_emu.mod_manager.find_by_address(previous_ip); //
            });

            const auto is_current_binary_interesting = utils::make_lazy([&] {
                return is_main_exe || (binary && c.settings->modules.contains(binary->name)); //
            });

            const auto is_in_interesting_module = [&] {
                if (c.settings->modules.empty())
                {
                    return false;
                }

                return is_current_binary_interesting || (previous_binary && c.settings->modules.contains(previous_binary->name));
            };

            if (c.settings->instruction_summary && (is_current_binary_interesting || !binary))
            {
                record_instruction(c, address);
            }

            const auto is_interesting_call = is_previous_main_exe                                              //
                                             || (!previous_binary && current_thread.executed_instructions > 1) //
                                             || is_in_interesting_module();

            if ((!c.settings->verbose_logging && !is_interesting_call) || !binary)
            {
                return;
            }

            const auto export_entry = binary->address_names.find(address);
            if (export_entry != binary->address_names.end())
            {
                if (!c.settings->ignored_functions.contains(export_entry->second))
                {
                    auto details = collect_function_details(c, export_entry->second);
                    const auto call_count = next_traced_call_count(c);
                    c.emit_observation<function_execution_event>([&](auto& event) {
                        event.call_count = call_count;
                        event.function_name = export_entry->second;
                        event.interesting = is_interesting_call;
                        event.details = std::move(details);
                    });
                    (void)break_before_traced_call(c, call_count);
                }
            }
            else if (address == binary->entry_point)
            {
                c.emit_observation<entry_point_execution_event>([&](auto& event) { event.interesting = is_interesting_call; });
            }
            else if (is_previous_main_exe && binary != previous_binary && !is_return(c.d, c.win_emu->emu(), previous_ip))
            {
                auto nearest_entry = binary->address_names.upper_bound(address);
                if (nearest_entry == binary->address_names.begin())
                {
                    return;
                }

                --nearest_entry;
                c.emit_observation<foreign_code_transition_event>([&](auto& event) {
                    event.function_name = nearest_entry->second;
                    event.function_offset = address - nearest_entry->first;
                    event.interesting = is_interesting_call;
                });
            }
        }

        void handle_rdtsc(analysis_context& c)
        {
            auto& win_emu = *c.win_emu;
            auto& emu = win_emu.active_cpu();

            const auto rip = emu.read_instruction_pointer();
            const auto mod = get_module_if_interesting(win_emu.mod_manager, c.settings->modules, rip);

            if (!mod.has_value() || (c.settings->concise_logging && !c.rdtsc_cache.insert(rip).second))
            {
                return;
            }

            c.emit_observation<rdtsc_event>();
        }

        void handle_rdtscp(analysis_context& c)
        {
            auto& win_emu = *c.win_emu;
            auto& emu = win_emu.active_cpu();

            const auto rip = emu.read_instruction_pointer();
            const auto mod = get_module_if_interesting(win_emu.mod_manager, c.settings->modules, rip);

            if (!mod.has_value() || (c.settings->concise_logging && !c.rdtscp_cache.insert(rip).second))
            {
                return;
            }

            c.emit_observation<rdtscp_event>();
        }

        bool dcomposition_syscall_trace_enabled()
        {
            static const bool enabled = std::getenv("SOGEN_TRACE_DCOMPOSITION_SYSCALLS") != nullptr;
            return enabled;
        }

        emulator_callbacks::continuation handle_syscall(analysis_context& c, const uint32_t syscall_id, const std::string_view syscall_name)
        {
            if (dcomposition_syscall_trace_enabled() && syscall_name.starts_with("NtDComposition"))
            {
                c.win_emu->log.error("[dcomposition-syscall-trace] tid=%u syscall %.*s (id=0x%X)\n", c.win_emu->current_thread().id,
                                     STR_VIEW_VA(syscall_name), syscall_id);
            }

            if (g_thread_activity_armed && is_thread_activity_traced_tid(c.win_emu->current_thread().id))
            {
                c.win_emu->log.error("[thread-activity-trace] tid=%u syscall %.*s (id=0x%X)\n", c.win_emu->current_thread().id,
                                     STR_VIEW_VA(syscall_name), syscall_id);
            }

            if (g_ebwv_gate1_lazy_helper_window_active && g_ebwv_gate1_lazy_helper_window_tid == c.win_emu->current_thread().id)
            {
                c.win_emu->log.error("[gate1-lazy-helper-trace] tid=%u syscall %.*s (id=0x%X) during lazy-helper window\n",
                                     c.win_emu->current_thread().id, STR_VIEW_VA(syscall_name), syscall_id);
            }

            if (c.settings->ignored_functions.contains(syscall_name))
            {
                return instruction_hook_continuation::run_instruction;
            }

            auto& win_emu = *c.win_emu;
            auto& emu = win_emu.active_cpu();

            const auto address = emu.read_instruction_pointer();
            if (c.syscall_to_resume_after_break)
            {
                const auto syscall_to_resume = std::exchange(c.syscall_to_resume_after_break, std::nullopt);
                if (*syscall_to_resume == address)
                {
                    return instruction_hook_continuation::run_instruction;
                }
            }

            const auto* mod = win_emu.mod_manager.find_by_address(address);
            const auto is_sus_module = mod != win_emu.mod_manager.ntdll && mod != win_emu.mod_manager.win32u;
            const auto previous_ip = win_emu.current_thread().previous_ip;
            const auto is_valid_32_bit_module = utils::make_lazy([&] {
                return mod                                                              //
                       && win_emu.process.is_wow64_process                              //
                       && (mod->name == "wow64cpu.dll" || mod->name == "wow64win.dll"); //
            });

            if (is_sus_module && !is_valid_32_bit_module)
            {
                const auto call_count = next_traced_call_count(c);
                c.emit_observation<syscall_event>([&](auto& event) {
                    event.call_count = call_count;
                    event.classification = syscall_classification::inline_syscall;
                    event.syscall_id = syscall_id;
                    event.syscall_name = std::string(syscall_name);
                });

                if (break_before_traced_syscall(c, call_count, address))
                {
                    return instruction_hook_continuation::skip_instruction;
                }
            }
            else if (!previous_ip || mod->contains(previous_ip))
            {
                if (!c.settings->skip_syscalls)
                {
                    const auto rsp = emu.read_stack_pointer();

                    uint64_t return_address{};
                    emu.try_read_memory(rsp, &return_address, sizeof(return_address));

                    const auto* caller_mod_name = win_emu.mod_manager.find_name(return_address);
                    const auto call_count = next_traced_call_count(c);

                    c.emit_observation<syscall_event>([&](auto& event) {
                        event.call_count = call_count;
                        event.classification = syscall_classification::regular;
                        event.syscall_id = syscall_id;
                        event.syscall_name = std::string(syscall_name);
                        event.caller_rip = return_address;
                        event.caller_module = caller_mod_name ? std::optional<std::string>{caller_mod_name} : std::nullopt;
                    });

                    if (break_before_traced_syscall(c, call_count, address))
                    {
                        return instruction_hook_continuation::skip_instruction;
                    }
                }
            }
            else
            {
                const auto* previous_mod = win_emu.mod_manager.find_by_address(previous_ip);
                const auto call_count = next_traced_call_count(c);

                c.emit_observation<syscall_event>([&](auto& event) {
                    event.call_count = call_count;
                    event.classification = syscall_classification::crafted_out_of_line;
                    event.syscall_id = syscall_id;
                    event.syscall_name = std::string(syscall_name);
                    event.caller_rip = previous_ip;
                    event.caller_module = previous_mod ? std::optional<std::string>{previous_mod->name} : std::nullopt;
                });

                if (break_before_traced_syscall(c, call_count, address))
                {
                    return instruction_hook_continuation::skip_instruction;
                }
            }

            return instruction_hook_continuation::run_instruction;
        }

        bool dialog_click_trace_enabled()
        {
            static const bool enabled = std::getenv("SOGEN_TRACE_WINDOWS") != nullptr;
            return enabled;
        }

        // Strips the '&' mnemonic marker Win32 resource compilers put in front of a control's
        // access key (e.g. "&Unzip" -> "Unzip") so a rule written against the label the user
        // actually sees matches regardless of which letter the dialog happens to underline.
        std::string strip_mnemonic(std::string text)
        {
            text.erase(std::ranges::remove(text, '&').begin(), text.end());
            return text;
        }

        void handle_event_pump(analysis_context& c)
        {
            trace_widget_window_threads(c);

            if (c.click_dialog_rules.empty())
            {
                return;
            }

            auto& proc = c.win_emu->process;

            // Prune entries whose dialog window no longer exists so a later, unrelated dialog
            // can't silently reuse the destroyed dialog's recycled HWND and get treated as
            // already clicked.
            std::erase_if(c.clicked_dialogs,
                          [&](const auto& entry) { return proc.windows.get(static_cast<hwnd>(entry.first)) == nullptr; });

            const bool trace = dialog_click_trace_enabled() || std::getenv("SOGEN_TRACE_DIALOG_TITLES_LITE") != nullptr;
            static std::unordered_map<uint64_t, std::string> last_traced_titles;

            for (auto& win : proc.windows | std::views::values)
            {
                if (!win.is_dialog())
                {
                    continue;
                }

                // A dialog's controls (and their final text, e.g. an SFX's completion message) can
                // exist before ShowWindow ever runs - WM_INITDIALOG builds the whole child tree first.
                // Clicking that early queues the notification before the dialog's own modal message
                // loop has started pumping, which - unlike a real user or UI-automation tool, neither
                // of which can interact with a window still hidden - leaves the dialog to progress
                // through its normal show/paint sequence without ever having "seen" the click. Wait
                // for the dialog to actually be shown before matching any rule against it.
                if ((win.style & WS_VISIBLE) == 0)
                {
                    continue;
                }

                const auto title = u16_to_u8(win.name);

                if (trace)
                {
                    auto& last = last_traced_titles[static_cast<uint64_t>(win.handle)];
                    if (last != title)
                    {
                        last = title;
                        c.win_emu->log.error("[dialog-click-trace] hwnd=0x%llx title='%s'\n", static_cast<unsigned long long>(win.handle),
                                             title.c_str());
                    }
                }

                // A dialog can have several matching rules (e.g. checking a checkbox before
                // clicking OK); click the first one not yet applied to this dialog instance and
                // leave the rest for the next pump call. A dialog whose title matches no rule at
                // all is left alone: it might be a real, unexpected error rather than one of the
                // known dialogs to auto-dismiss. Matching is exact, not substring: an SFX that
                // reuses a common title prefix across dialogs (e.g. "WinZip Self-Extractor" for
                // both its main dialog and a later confirmation dialog) would otherwise have a
                // rule meant for one dialog silently also fire on the other.
                const auto rule = std::ranges::find_if(c.click_dialog_rules, [&](const auto& entry) {
                    return title == entry.first && !c.clicked_dialogs.contains({win.handle, entry.second});
                });
                if (rule == c.click_dialog_rules.end())
                {
                    continue;
                }

                const auto& wanted_text = rule->second;
                const auto wanted_normalized = strip_mnemonic(wanted_text);

                // The WM_COMMAND below is posted (queued), so the owning thread need not already
                // be blocked in a message wait - a plain PeekMessage pump will still pick it up;
                // do not gate this on await_msg/await_msg_mask.
                const emulator_thread* owner = proc.find_thread_by_id(win.thread_id);
                if (!owner)
                {
                    if (trace)
                    {
                        c.win_emu->log.error("[dialog-click-trace] hwnd=0x%llx rule matched (wanted='%s') but owner thread %u not found\n",
                                             static_cast<unsigned long long>(win.handle), wanted_text.c_str(), win.thread_id);
                    }
                    continue;
                }

                // Matching by the control's own visible text (rather than a caller-guessed resource
                // ID) is what a real UI-automation tool does, and is the only reliable option here:
                // a dialog's control IDs are private to the binary that built it, so a CLI-supplied
                // guess is fragile - it silently never matches if wrong, and this project's own
                // USER_WINDOW.wID storage is reused by other, build-specific per-window bookkeeping
                // (see the "Control id offset is build-specific" note in handle_NtUserCreateWindowEx),
                // so even a correct initial guess can stop matching once that field is later
                // overwritten for an unrelated purpose. Text has neither problem.
                hwnd child_handle = 0;
                uint32_t child_control_id = 0;
                for (auto& child : proc.windows | std::views::values)
                {
                    if (child.parent_handle != win.handle)
                    {
                        continue;
                    }

                    if (strip_mnemonic(u16_to_u8(child.name)) != wanted_normalized)
                    {
                        continue;
                    }

                    child_handle = child.handle;
                    child.guest.access([&](const USER_WINDOW& gw) { child_control_id = static_cast<uint32_t>(gw.wID); });
                    break;
                }

                if (child_handle == 0)
                {
                    if (trace)
                    {
                        c.win_emu->log.error(
                            "[dialog-click-trace] hwnd=0x%llx rule matched (wanted='%s') but no child control with that text\n",
                            static_cast<unsigned long long>(win.handle), wanted_text.c_str());
                    }
                    continue;
                }

                if (trace)
                {
                    c.win_emu->log.error("[dialog-click-trace] hwnd=0x%llx clicking child hwnd=0x%llx (text='%s', id=%u)\n",
                                         static_cast<unsigned long long>(win.handle), static_cast<unsigned long long>(child_handle),
                                         wanted_text.c_str(), child_control_id);
                }

                // A checkable button's internal checked state is toggled by its own window procedure
                // processing a real click (mouse-down/up on the button itself), not by the WM_COMMAND
                // notification alone - that message only tells the parent a click happened. Since this
                // harness synthesizes the notification directly rather than routing through a real
                // click, explicitly set the checked state too; BM_SETCHECK is a no-op for buttons that
                // aren't checkable, so this is safe to send unconditionally.
                ui_event check_event{};
                check_event.window = child_handle;
                check_event.message = 0x00F1; // BM_SETCHECK
                check_event.wParam = 1;       // BST_CHECKED
                c.win_emu->handle_ui_event(check_event);

                ui_event event{};
                event.window = win.handle;
                event.message = WM_COMMAND;
                event.wParam = child_control_id & 0xFFFF;
                event.lParam = child_handle;

                c.win_emu->handle_ui_event(event);
                c.clicked_dialogs.insert({win.handle, wanted_text});
                return;
            }
        }

        void handle_stdout(analysis_context& c, const std::string_view data)
        {
            c.emit_observation<stdout_chunk_event>([&](auto& event) { event.data = std::string(data); });

            if (c.settings->buffer_stdout && !c.settings->silent)
            {
                c.output.append(data);
            }
        }

        void watch_import_table(analysis_context& c)
        {
            c.win_emu->setup_process_if_necessary();

            const auto& import_list = c.win_emu->mod_manager.executable->imports;
            if (import_list.empty())
            {
                return;
            }

            auto min = std::numeric_limits<uint64_t>::max();
            auto max = std::numeric_limits<uint64_t>::min();

            for (const auto& import_thunk : import_list | std::views::keys)
            {
                min = std::min(import_thunk, min);
                max = std::max(import_thunk, max);
            }

            c.win_emu->emu().hook_memory_write(min, max - min,
                                               [&c](cpu_interface&, const uint64_t address, const void* value, size_t size) {
                                                   const auto& watched_module = *c.win_emu->mod_manager.executable;

                                                   const auto sym = watched_module.imports.find(address);
                                                   if (sym == watched_module.imports.end())
                                                   {
                                                       // TODO: Print unaligned write accesses?
                                                       return;
                                                   }

                                                   uint64_t int_value{};
                                                   memcpy(&int_value, value, std::min(size, sizeof(int_value)));

                                                   const auto import_module = watched_module.imported_modules.at(sym->second.module_index);

                                                   c.emit_observation<import_write_event>([&](auto& event) {
                                                       event.size = size;
                                                       event.value = int_value;
                                                       event.import_name = sym->second.name;
                                                       event.import_module = import_module;
                                                   });
                                               });

            c.win_emu->emu().hook_memory_read(min, max - min, [&c](cpu_interface&, const uint64_t address, const void*, size_t) {
                const auto rip = c.win_emu->emu().read_instruction_pointer();
                const auto& watched_module = *c.win_emu->mod_manager.executable;
                const auto accessor_module = get_module_if_interesting(c.win_emu->mod_manager, c.settings->modules, rip);

                if (!accessor_module.has_value())
                {
                    return;
                }

                const auto sym = watched_module.imports.find(address);
                if (sym == watched_module.imports.end())
                {
                    return;
                }

                accessed_import access{};
                access.address = c.win_emu->emu().read_memory<uint64_t>(address);
                access.access_context = c.make_execution_context();
                access.import_name = sym->second.name;
                access.import_module = watched_module.imported_modules.at(sym->second.module_index);

                const auto& t = c.win_emu->current_thread();
                access.access_inst_count = t.executed_instructions;

                c.accessed_imports.push_back(std::move(access));
            });
        }
    }

    event_header analysis_context::make_event_header() const
    {
        return {
            .sequence = this->next_event_sequence++,
            .instruction_count = this->win_emu ? this->win_emu->get_executed_instructions() : 0,
        };
    }

    execution_context analysis_context::make_execution_context() const
    {
        auto& emu = this->win_emu->active_cpu();
        const auto rip = emu.read_instruction_pointer();
        const auto* rip_module = this->win_emu->mod_manager.find_name(rip);

        execution_context context{
            .thread_id = 0,
            .rip = rip,
            .rip_module = rip_module ? rip_module : "<N/A>",
        };

        try
        {
            const auto& thread = this->win_emu->current_thread();
            const auto previous_ip = thread.previous_ip;
            const auto* previous_module = previous_ip ? this->win_emu->mod_manager.find_name(previous_ip) : nullptr;
            context.thread_id = thread.id;
            context.previous_ip = previous_ip ? std::optional<uint64_t>{previous_ip} : std::nullopt;
            context.previous_ip_module = previous_module ? std::optional<std::string>{previous_module} : std::nullopt;
        }
        catch (...)
        {
            // Some early lifecycle events fire before a thread is active.
        }

        return context;
    }

    void analysis_context::emit_event(const analysis_event& event) const
    {
        for (auto* reporter : this->reporters)
        {
            reporter->report(event);
        }
    }

    void register_analysis_callbacks(analysis_context& c)
    {
        auto& cb = c.win_emu->callbacks;

        cb.on_stdout = make_callback(c, handle_stdout);
        cb.on_syscall = make_callback(c, handle_syscall);
        cb.on_rdtsc = make_callback(c, handle_rdtsc);
        cb.on_rdtscp = make_callback(c, handle_rdtscp);
        cb.on_ioctrl = make_callback(c, handle_ioctrl);

        cb.on_memory_protect = make_callback(c, handle_memory_protect);
        cb.on_memory_violate = make_callback(c, handle_memory_violate);
        cb.on_memory_allocate = make_callback(c, handle_memory_allocate);

        (void)cb.on_module_load.add(make_callback(c, handle_module_load));
        (void)cb.on_module_unload.add(make_callback(c, handle_module_unload));
        (void)cb.on_section_first_execution.add(make_callback(c, handle_section_first_execution));

        cb.on_thread_create = make_callback(c, handle_thread_create);
        cb.on_thread_terminated = make_callback(c, handle_thread_terminated);
        cb.on_thread_switch = make_callback(c, handle_thread_switch);
        cb.on_thread_set_name = make_callback(c, handle_thread_set_name);

        cb.on_instruction = make_callback(c, handle_instruction);
        cb.on_event_pump = make_callback(c, handle_event_pump);
        cb.on_debug_string.add(make_callback(c, handle_debug_string));
        cb.on_generic_access = make_callback(c, handle_generic_access);
        cb.on_generic_activity = make_callback(c, handle_generic_activity);
        cb.on_suspicious_activity = make_callback(c, handle_suspicious_activity);
        cb.on_fast_fail = make_callback(c, handle_fast_fail);

        watch_import_table(c);
    }

    std::optional<mapped_module*> get_module_if_interesting(module_manager& manager, const string_set& modules, const uint64_t address)
    {
        if (manager.executable->contains(address))
        {
            return manager.executable;
        }

        auto* mod = manager.find_by_address(address);
        if (!mod)
        {
            // Not being part of any module is interesting
            return nullptr;
        }

        if (modules.contains(mod->name))
        {
            return mod;
        }

        return std::nullopt;
    }

} // namespace sogen
