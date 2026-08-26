#pragma once

#include "process_control_channel.hpp"

namespace sogen
{
    class windows_emulator;

    // The pure executor for the cross-process control protocol (process_control_channel.hpp): given
    // a target emulator and a request addressed to it, mutates that emulator's own state (memory,
    // process context) exactly as if the operation had been issued by a syscall running inside it,
    // and fills in response - including response.request_id, echoed from request.request_id so a
    // caller relaying the response back over a real channel (respond()) replies to the right
    // outstanding request. Shared by pump_process_control below and by anything driving it directly,
    // e.g. a loopback process_control_channel double in tests.
    void execute_process_control_request(windows_emulator& target, const process_control_request& request,
                                         process_control_response& response);

    // Drains every request currently queued on channel (via try_receive(), non-blocking), executes
    // each against target with execute_process_control_request, and replies with respond(). Stops as
    // soon as try_receive() reports nothing pending - callers needing continuous servicing call this
    // once per scheduler tick, the same cadence windows_emulator::pump_pipe_ipc() is driven at.
    void pump_process_control(windows_emulator& target, process_control_channel& channel);

} // namespace sogen
