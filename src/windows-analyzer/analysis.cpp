#include "std_include.hpp"

#include "analysis.hpp"
#include "analysis_reporter.hpp"
#include "disassembler.hpp"
#include "windows_emulator.hpp"
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

        void handle_ioctrl(const analysis_context& c, const io_device&, const std::u16string_view device_name, const ULONG code)
        {
            if (!c.settings->skip_generic_activity)
            {
                c.emit_observation<io_control_event>([&](auto& event) {
                    event.device_name = u16_to_u8(device_name);
                    event.code = static_cast<uint32_t>(code);
                });
            }
        }

        void handle_thread_create(const analysis_context& c, handle, emulator_thread& t)
        {
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
        }

        void handle_module_load(const analysis_context& c, const mapped_module& mod)
        {
            c.emit_observation<module_load_event>([&](auto& event) {
                event.path = mod.module_path.string();
                event.image_base = mod.image_base;
            });

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

            if (mod.name == "msedge.dll" && std::getenv("SOGEN_TRACE_DELAYLOAD_FAILURE"))
            {
                g_delayload_failure_trace_va = mod.image_base + DELAYLOAD_FAILURE_RVA;
                c.win_emu->log.error("[delayload-failure-trace] watching HandleDelayLoadFailureCommon at 0x%llx\n",
                                     static_cast<unsigned long long>(g_delayload_failure_trace_va));
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

        void trace_platform_channel_delegate_call_hit(const analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();

            const auto rsi = emu.reg<uint64_t>(x86_register::rsi);
            const auto rcx = emu.reg<uint64_t>(x86_register::rcx);
            const auto rax = emu.reg<uint64_t>(x86_register::rax);

            const auto* target_mod_name = c.win_emu->mod_manager.find_name(rax);
            const auto* target_mod = c.win_emu->mod_manager.find_by_address(rax);
            const auto target_offset = target_mod ? rax - target_mod->image_base : rax;

            c.win_emu->log.error("[named-pipe-create-trace] delegate-call hit at 0x%llx, tracker_this=0x%llx delegate_this=0x%llx "
                                 "target=0x%llx (%s+0x%llx) tid=%u\n",
                                 static_cast<unsigned long long>(address), static_cast<unsigned long long>(rsi),
                                 static_cast<unsigned long long>(rcx), static_cast<unsigned long long>(rax), target_mod_name,
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

        emulator_callbacks::continuation handle_syscall(analysis_context& c, const uint32_t syscall_id, const std::string_view syscall_name)
        {
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
