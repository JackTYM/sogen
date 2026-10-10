#pragma once

#include <windows_emulator.hpp>

namespace sogen
{
    struct address_expression
    {
        std::optional<x86_register> base_register{};
        std::unique_ptr<address_expression> dereferenced{};
        int64_t offset{};
    };

    struct memory_dump_spec
    {
        std::string text{};
        address_expression address{};
        size_t length{};
    };

    struct named_register
    {
        std::string_view name{};
        x86_register reg{};
    };

    struct trace_hook_spec
    {
        std::string module{};
        std::variant<uint64_t, std::string> location{};
        std::string name{};
        std::vector<named_register> entry_registers{};
        std::vector<memory_dump_spec> memory_dumps{};
        std::vector<named_register> return_registers{};
    };

    // One hook per line: `<module> <rva|export> <name> [reg...] [mem=<expr>:<len>...] [ret=<reg>[,<reg>...]]`.
    // <expr> is a register, a number, or `[<expr>]` (a guest-pointer-sized load), each optionally
    // followed by `+<n>`/`-<n>` terms, e.g. `mem=[[rcx+0x28]+0x38]:8`. `#` starts a comment.
    std::vector<trace_hook_spec> parse_trace_hook_specs(std::string_view text);

    class hook_tracer
    {
      public:
        hook_tracer(windows_emulator& win_emu, std::vector<trace_hook_spec> specs);
        ~hook_tracer();

        hook_tracer(hook_tracer&&) = delete;
        hook_tracer(const hook_tracer&) = delete;
        hook_tracer& operator=(hook_tracer&&) = delete;
        hook_tracer& operator=(const hook_tracer&) = delete;

      private:
        void install_hooks(const mapped_module& mod);
        void remove_hooks(const mapped_module& mod);
        void on_entry(const trace_hook_spec& spec, const mapped_module& mod);
        void on_return(const trace_hook_spec& spec, size_t pointer_size);
        void install_return_hook(const trace_hook_spec& spec, uint64_t return_address, size_t pointer_size);
        std::optional<uint64_t> evaluate(const address_expression& expression, size_t pointer_size);
        uint64_t read_register(x86_register reg, size_t pointer_size) const;
        std::string describe_address(uint64_t address) const;
        void print(const std::string& line);

        windows_emulator& win_emu_;
        std::vector<trace_hook_spec> specs_{};
        std::map<uint64_t, std::vector<emulator_hook*>> module_hooks_{};
        std::set<uint64_t> return_hook_addresses_{};
        utils::callback_id_type load_callback_{};
        utils::callback_id_type unload_callback_{};
    };
}
