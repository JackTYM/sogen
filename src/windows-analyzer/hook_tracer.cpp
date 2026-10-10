#include "std_include.hpp"
#include "hook_tracer.hpp"

#include <platform/win_pefile.hpp>

namespace sogen
{
    namespace
    {
        constexpr std::array<named_register, 17> TRACEABLE_REGISTERS{{
            {"rax", x86_register::rax},
            {"rbx", x86_register::rbx},
            {"rcx", x86_register::rcx},
            {"rdx", x86_register::rdx},
            {"rsi", x86_register::rsi},
            {"rdi", x86_register::rdi},
            {"rbp", x86_register::rbp},
            {"rsp", x86_register::rsp},
            {"r8", x86_register::r8},
            {"r9", x86_register::r9},
            {"r10", x86_register::r10},
            {"r11", x86_register::r11},
            {"r12", x86_register::r12},
            {"r13", x86_register::r13},
            {"r14", x86_register::r14},
            {"r15", x86_register::r15},
            {"rip", x86_register::rip},
        }};

        constexpr size_t max_memory_dump_length = 0x1000;

        std::optional<named_register> find_register(const std::string_view name)
        {
            for (const auto& entry : TRACEABLE_REGISTERS)
            {
                if (entry.name == name)
                {
                    return entry;
                }
            }

            return std::nullopt;
        }

        named_register parse_register(const std::string_view name)
        {
            const auto entry = find_register(name);
            if (!entry)
            {
                throw std::runtime_error("Unknown register in trace hook spec: " + std::string(name));
            }

            return *entry;
        }

        std::optional<uint64_t> try_parse_number(std::string_view text)
        {
            int base = 10;
            if (text.starts_with("0x") || text.starts_with("0X"))
            {
                text.remove_prefix(2);
                base = 16;
            }

            uint64_t value{};
            const auto* end = text.data() + text.size();
            const auto result = std::from_chars(text.data(), end, value, base);
            if (text.empty() || result.ec != std::errc{} || result.ptr != end)
            {
                return std::nullopt;
            }

            return value;
        }

        uint64_t parse_number(const std::string_view text)
        {
            const auto value = try_parse_number(text);
            if (!value)
            {
                throw std::runtime_error("Invalid number in trace hook spec: " + std::string(text));
            }

            return *value;
        }

        std::vector<std::string_view> split(const std::string_view text, const char separator)
        {
            std::vector<std::string_view> parts{};
            size_t start = 0;
            while (start <= text.size())
            {
                const auto end = std::min(text.find(separator, start), text.size());
                if (end > start)
                {
                    parts.push_back(text.substr(start, end - start));
                }
                start = end + 1;
            }

            return parts;
        }

        std::vector<std::string_view> split_whitespace(const std::string_view text)
        {
            std::vector<std::string_view> tokens{};
            size_t position = 0;
            while (position < text.size())
            {
                const auto start = text.find_first_not_of(" \t\r", position);
                if (start == std::string_view::npos)
                {
                    break;
                }

                const auto end = std::min(text.find_first_of(" \t\r", start), text.size());
                tokens.push_back(text.substr(start, end - start));
                position = end;
            }

            return tokens;
        }

        class expression_parser
        {
          public:
            explicit expression_parser(const std::string_view text)
                : text_(text)
            {
            }

            address_expression parse()
            {
                auto expression = this->parse_expression();
                if (this->position_ != this->text_.size())
                {
                    this->fail();
                }

                return expression;
            }

          private:
            address_expression parse_expression()
            {
                auto expression = this->parse_atom();

                while (this->position_ < this->text_.size() && (this->peek() == '+' || this->peek() == '-'))
                {
                    const bool negative = this->text_[this->position_++] == '-';
                    const auto value = static_cast<int64_t>(parse_number(this->take_token()));
                    expression.offset += negative ? -value : value;
                }

                return expression;
            }

            address_expression parse_atom()
            {
                address_expression expression{};

                if (this->position_ < this->text_.size() && this->peek() == '[')
                {
                    ++this->position_;
                    expression.dereferenced = std::make_unique<address_expression>(this->parse_expression());
                    if (this->position_ >= this->text_.size() || this->peek() != ']')
                    {
                        this->fail();
                    }
                    ++this->position_;
                    return expression;
                }

                const auto token = this->take_token();
                if (const auto reg = find_register(token))
                {
                    expression.base_register = reg->reg;
                }
                else
                {
                    expression.offset = static_cast<int64_t>(parse_number(token));
                }

                return expression;
            }

            std::string_view take_token()
            {
                const auto start = this->position_;
                while (this->position_ < this->text_.size() && std::isalnum(static_cast<unsigned char>(this->peek())))
                {
                    ++this->position_;
                }

                if (start == this->position_)
                {
                    this->fail();
                }

                return this->text_.substr(start, this->position_ - start);
            }

            char peek() const
            {
                return this->text_[this->position_];
            }

            [[noreturn]] void fail() const
            {
                throw std::runtime_error("Invalid address expression in trace hook spec: " + std::string(this->text_));
            }

            std::string_view text_{};
            size_t position_{};
        };

        memory_dump_spec parse_memory_dump(const std::string_view text)
        {
            const auto separator = text.rfind(':');
            if (separator == std::string_view::npos)
            {
                throw std::runtime_error("Memory dump in trace hook spec needs a length: " + std::string(text));
            }

            memory_dump_spec dump{};
            dump.text = std::string(text.substr(0, separator));
            dump.address = expression_parser(dump.text).parse();
            dump.length = parse_number(text.substr(separator + 1));
            if (dump.length == 0 || dump.length > max_memory_dump_length)
            {
                throw std::runtime_error("Memory dump length out of range in trace hook spec: " + std::string(text));
            }

            return dump;
        }

        trace_hook_spec parse_trace_hook_line(const std::string_view line)
        {
            const auto tokens = split_whitespace(line);
            if (tokens.size() < 3)
            {
                throw std::runtime_error("Trace hook spec needs at least <module> <rva|export> <name>: " + std::string(line));
            }

            trace_hook_spec spec{};
            spec.module = utils::string::to_lower(std::string(tokens[0]));
            if (spec.module.ends_with("@x64"))
            {
                spec.machine = IMAGE_FILE_MACHINE_AMD64;
            }
            else if (spec.module.ends_with("@x86"))
            {
                spec.machine = IMAGE_FILE_MACHINE_I386;
            }

            if (spec.machine)
            {
                spec.module.resize(spec.module.size() - 4);
            }
            spec.name = std::string(tokens[2]);

            if (const auto rva = try_parse_number(tokens[1]))
            {
                spec.location = *rva;
            }
            else
            {
                spec.location = std::string(tokens[1]);
            }

            for (size_t i = 3; i < tokens.size(); ++i)
            {
                const auto token = tokens[i];
                if (token.starts_with("mem="))
                {
                    spec.memory_dumps.push_back(parse_memory_dump(token.substr(4)));
                }
                else if (token.starts_with("ret="))
                {
                    for (const auto reg : split(token.substr(4), ','))
                    {
                        spec.return_registers.push_back(parse_register(reg));
                    }
                }
                else
                {
                    spec.entry_registers.push_back(parse_register(token));
                }
            }

            return spec;
        }

        std::optional<uint64_t> resolve_hook_address(const trace_hook_spec& spec, const mapped_module& mod)
        {
            if (const auto* rva = std::get_if<uint64_t>(&spec.location))
            {
                return mod.image_base + *rva;
            }

            const auto& export_name = std::get<std::string>(spec.location);
            for (const auto& symbol : mod.exports)
            {
                if (symbol.name == export_name)
                {
                    return symbol.address;
                }
            }

            return std::nullopt;
        }

        size_t guest_pointer_size(const mapped_module& mod)
        {
            return mod.machine == IMAGE_FILE_MACHINE_I386 ? 4 : 8;
        }

        std::string format_hex(const uint64_t value)
        {
            std::array<char, 32> buffer{};
            (void)snprintf(buffer.data(), buffer.size(), "0x%" PRIx64, value);
            return buffer.data();
        }

        std::string format_bytes(const std::span<const uint8_t> bytes)
        {
            std::string result{};
            result.reserve(bytes.size() * 2);
            for (const auto byte : bytes)
            {
                const auto [high, low] = utils::string::to_hex(static_cast<std::byte>(byte));
                result += high;
                result += low;
            }

            return result;
        }
    }

    std::vector<trace_hook_spec> parse_trace_hook_specs(const std::string_view text)
    {
        std::vector<trace_hook_spec> specs{};

        for (auto line : split(text, '\n'))
        {
            line = line.substr(0, line.find('#'));
            if (split_whitespace(line).empty())
            {
                continue;
            }

            specs.push_back(parse_trace_hook_line(line));
        }

        return specs;
    }

    hook_tracer::hook_tracer(windows_emulator& win_emu, std::vector<trace_hook_spec> specs)
        : win_emu_(win_emu),
          specs_(std::move(specs))
    {
        this->load_callback_ = this->win_emu_.callbacks.on_module_load.add([this](const mapped_module& mod) { this->install_hooks(mod); });
        this->unload_callback_ =
            this->win_emu_.callbacks.on_module_unload.add([this](const mapped_module& mod) { this->remove_hooks(mod); });

        for (const auto& mod : this->win_emu_.mod_manager.modules() | std::views::values)
        {
            this->install_hooks(mod);
        }
    }

    hook_tracer::~hook_tracer()
    {
        this->win_emu_.callbacks.on_module_load.remove(this->load_callback_);
        this->win_emu_.callbacks.on_module_unload.remove(this->unload_callback_);

        for (const auto& hooks : this->module_hooks_ | std::views::values)
        {
            for (auto* hook : hooks)
            {
                this->win_emu_.emu().delete_hook(hook);
            }
        }
    }

    void hook_tracer::install_hooks(const mapped_module& mod)
    {
        const auto module_name = utils::string::to_lower(mod.name);

        for (const auto& spec : this->specs_)
        {
            if (spec.module != module_name || (spec.machine && *spec.machine != mod.machine))
            {
                continue;
            }

            const auto address = resolve_hook_address(spec, mod);
            if (!address)
            {
                this->win_emu_.log.error("[hook] %s: export not found in %s\n", spec.name.c_str(), mod.name.c_str());
                continue;
            }

            auto* hook = this->win_emu_.emu().hook_memory_execution(
                *address, [this, &spec, image_base = mod.image_base](cpu_interface& cpu, const uint64_t hit_address) {
                    this->win_emu_.dispatch_on_cpu(cpu, [&] {
                        const auto* current = this->win_emu_.mod_manager.find_by_address(hit_address);
                        if (current && current->image_base == image_base)
                        {
                            this->on_entry(spec, *current);
                        }
                    });
                });

            this->module_hooks_[mod.image_base].push_back(hook);
        }
    }

    void hook_tracer::remove_hooks(const mapped_module& mod)
    {
        const auto entry = this->module_hooks_.find(mod.image_base);
        if (entry == this->module_hooks_.end())
        {
            return;
        }

        for (auto* hook : entry->second)
        {
            this->win_emu_.emu().delete_hook(hook);
        }

        this->module_hooks_.erase(entry);
        std::erase_if(this->return_hook_addresses_, [&](const uint64_t address) { return mod.contains(address); });
    }

    void hook_tracer::on_entry(const trace_hook_spec& spec, const mapped_module& mod)
    {
        const auto pointer_size = guest_pointer_size(mod);

        uint64_t return_address{};
        const auto has_return_address =
            this->win_emu_.emu().try_read_memory(this->read_register(x86_register::rsp, pointer_size), &return_address, pointer_size);

        std::string line = spec.name;
        for (const auto& reg : spec.entry_registers)
        {
            line += " " + std::string(reg.name) + "=" + format_hex(this->read_register(reg.reg, pointer_size));
        }

        line += " ret=" + (has_return_address ? this->describe_address(return_address) : std::string("?"));

        for (const auto& dump : spec.memory_dumps)
        {
            line += " [" + dump.text + "]=";

            const auto dump_address = this->evaluate(dump.address, pointer_size);
            std::vector<uint8_t> bytes(dump.length);
            if (dump_address && this->win_emu_.emu().try_read_memory(*dump_address, bytes.data(), bytes.size()))
            {
                line += format_bytes(bytes);
            }
            else
            {
                line += "?";
            }
        }

        this->print(line);

        if (!spec.return_registers.empty() && has_return_address)
        {
            this->install_return_hook(spec, return_address, pointer_size);
        }
    }

    void hook_tracer::install_return_hook(const trace_hook_spec& spec, const uint64_t return_address, const size_t pointer_size)
    {
        if (!this->return_hook_addresses_.insert(return_address).second)
        {
            return;
        }

        auto* hook = this->win_emu_.emu().hook_memory_execution(return_address, [this, &spec, pointer_size](cpu_interface& cpu, uint64_t) {
            this->win_emu_.dispatch_on_cpu(cpu, [&] { this->on_return(spec, pointer_size); });
        });

        const auto* owner = this->win_emu_.mod_manager.find_by_address(return_address);
        this->module_hooks_[owner ? owner->image_base : 0].push_back(hook);
    }

    void hook_tracer::on_return(const trace_hook_spec& spec, const size_t pointer_size)
    {
        std::string line = spec.name + " RET";
        for (const auto& reg : spec.return_registers)
        {
            line += " " + std::string(reg.name) + "=" + format_hex(this->read_register(reg.reg, pointer_size));
        }

        this->print(line);
    }

    std::optional<uint64_t> hook_tracer::evaluate(const address_expression& expression, const size_t pointer_size)
    {
        uint64_t value{};

        if (expression.base_register)
        {
            value = this->read_register(*expression.base_register, pointer_size);
        }
        else if (expression.dereferenced)
        {
            const auto pointer = this->evaluate(*expression.dereferenced, pointer_size);
            if (!pointer || !this->win_emu_.emu().try_read_memory(*pointer, &value, pointer_size))
            {
                return std::nullopt;
            }
        }

        return value + static_cast<uint64_t>(expression.offset);
    }

    uint64_t hook_tracer::read_register(const x86_register reg, const size_t pointer_size) const
    {
        const auto value = this->win_emu_.active_cpu().reg(reg);
        return pointer_size == 4 ? (value & 0xFFFFFFFF) : value;
    }

    std::string hook_tracer::describe_address(const uint64_t address) const
    {
        const auto* mod = this->win_emu_.mod_manager.find_by_address(address);
        if (!mod)
        {
            return format_hex(address);
        }

        return mod->name + "+" + format_hex(address - mod->image_base);
    }

    void hook_tracer::print(const std::string& line)
    {
        this->win_emu_.log.force_print(color::cyan, "[hook] pid=%u tid=%u %s\n", this->win_emu_.process.process_id,
                                       this->win_emu_.current_thread().id, line.c_str());
    }
}
