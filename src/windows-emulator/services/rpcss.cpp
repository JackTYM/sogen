#include "../std_include.hpp"
#include "rpcss.hpp"

#include "lrpc_server.hpp"
#include "../windows_emulator.hpp"

#include <random>

namespace sogen
{
    namespace
    {
        using lrpc::call;
        using lrpc::result;

        constexpr ndr::guid make_guid(const uint32_t d1, const uint16_t d2, const uint16_t d3, const std::array<uint8_t, 8>& d4)
        {
            return {
                static_cast<uint8_t>(d1),
                static_cast<uint8_t>(d1 >> 8),
                static_cast<uint8_t>(d1 >> 16),
                static_cast<uint8_t>(d1 >> 24),
                static_cast<uint8_t>(d2),
                static_cast<uint8_t>(d2 >> 8),
                static_cast<uint8_t>(d3),
                static_cast<uint8_t>(d3 >> 8),
                d4[0],
                d4[1],
                d4[2],
                d4[3],
                d4[4],
                d4[5],
                d4[6],
                d4[7],
            };
        }

        constexpr auto iid_local_object_exporter = make_guid(0xe60c73e6, 0x88f9, 0x11cf, {0x9a, 0xf1, 0x00, 0x20, 0xaf, 0x6e, 0x72, 0xf4});
        constexpr auto iid_scm = make_guid(0x412f241e, 0xc12a, 0x11ce, {0xab, 0xff, 0x00, 0x20, 0xaf, 0x6e, 0x7a, 0x17});
        constexpr auto iid_rot = make_guid(0xb9e79e60, 0x3d52, 0x11ce, {0xaa, 0xa1, 0x00, 0x00, 0x69, 0x01, 0x29, 0x3f});
        constexpr auto iid_machine_activator_control =
            make_guid(0xc6f3ee72, 0xce7e, 0x11d1, {0xb7, 0x1e, 0x00, 0xc0, 0x4f, 0xc3, 0x11, 0x1a});
        constexpr auto iid_scm_local_activator = make_guid(0x00000136, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46});

        constexpr uint32_t rpcss_process_id = 752;
        constexpr uint32_t rpcss_thread_id = 756;
        constexpr uint32_t ping_period_seconds = 120;
        constexpr uint32_t connect_flags = 0x10 | 0x40;
        constexpr uint32_t authn_level_connect = 2;
        constexpr uint32_t imp_level_identify = 2;
        constexpr uint32_t max_reserved_ids = 10;

        constexpr uint32_t or_invalid_oxid = 1910;
        constexpr uint32_t hresult_ok = 0;
        constexpr uint32_t hresult_false = 1;
        constexpr uint32_t mk_e_unavailable = 0x800401E3;
        constexpr uint32_t regdb_e_classnotreg = 0x80040154;

        struct dual_string_array
        {
            std::vector<uint16_t> entries{};
            uint16_t security_offset{};

            static dual_string_array local(const std::u16string_view endpoint)
            {
                constexpr uint16_t tower_ncalrpc = 0x10;
                constexpr uint16_t authn_winnt = 0x0A;
                constexpr uint16_t authz_none = 0xFFFF;

                dual_string_array dsa{};
                dsa.entries.push_back(tower_ncalrpc);
                dsa.entries.insert(dsa.entries.end(), endpoint.begin(), endpoint.end());
                dsa.entries.push_back(0);
                dsa.entries.push_back(0);
                dsa.security_offset = static_cast<uint16_t>(dsa.entries.size());
                dsa.entries.push_back(authn_winnt);
                dsa.entries.push_back(authz_none);
                dsa.entries.push_back(0);
                dsa.entries.push_back(0);
                return dsa;
            }

            static dual_string_array read(ndr::reader& r)
            {
                dual_string_array dsa{};
                r.read<uint32_t>();
                const auto count = r.read<uint16_t>();
                dsa.security_offset = r.read<uint16_t>();
                dsa.entries.resize(count);
                for (auto& entry : dsa.entries)
                {
                    entry = r.read<uint16_t>();
                }

                return dsa;
            }

            void write(ndr::writer& w) const
            {
                w.write(static_cast<uint32_t>(this->entries.size()));
                w.write(static_cast<uint16_t>(this->entries.size()));
                w.write(this->security_offset);
                for (const auto entry : this->entries)
                {
                    w.write(entry);
                }
            }
        };

        // HSTRING travels as a user_marshal type: the embedded pointer is USER_MARSHAL_MARKER ('User') and the pointee
        // (HSTRING_UserMarshal, cross-process form) is {8-aligned ulong marker; ulong byte length; characters}.
        constexpr uint32_t user_marshal_marker = 0x72657355;

        struct hstring_value
        {
            uint32_t marker{};
            std::vector<uint8_t> characters{};
        };

        std::optional<hstring_value> read_hstring_pointee(ndr::reader& r)
        {
            r.align(8);
            hstring_value value{};
            value.marker = r.read<uint32_t>();
            const auto length = r.read<uint32_t>();
            value.characters = r.read_bytes(length);
            return value;
        }

        void write_hstring_pointee(ndr::writer& w, const hstring_value& value)
        {
            w.align(8);
            w.write(value.marker);
            w.write(static_cast<uint32_t>(value.characters.size()));
            w.write_bytes(value.characters.data(), value.characters.size());
        }

        // INTERNAL_OXID_INFO as combase marshals it: a complex structure (8-byte aligned) whose pointers are
        // deferred to the end in member order.
        struct oxid_info
        {
            uint32_t thread_id{};
            uint32_t process_id{};
            uint32_t authn_hint{};
            uint16_t version_major{};
            uint16_t version_minor{};
            uint32_t container_version{};
            uint64_t capability_flags{};
            ndr::guid ipid_rem_unknown{};
            uint32_t flags{};
            std::optional<dual_string_array> bindings{};
            ndr::guid process_identifier{};
            uint64_t process_host_id{};
            uint16_t client_dependency{};
            std::optional<hstring_value> package_full_name{};
            std::optional<hstring_value> user_sid{};
            std::optional<hstring_value> appcontainer_sid{};
            uint64_t primary_oxid{};
            ndr::guid primary_ipid_rem_unknown{};

            static std::optional<oxid_info> read(ndr::reader& r)
            {
                oxid_info info{};
                r.align(8);
                info.thread_id = r.read<uint32_t>();
                info.process_id = r.read<uint32_t>();
                info.authn_hint = r.read<uint32_t>();
                info.version_major = r.read<uint16_t>();
                info.version_minor = r.read<uint16_t>();
                info.container_version = r.read<uint32_t>();
                info.capability_flags = r.read<uint64_t>();
                const auto extensions = r.read_pointer();
                info.ipid_rem_unknown = r.read_guid();
                info.flags = r.read<uint32_t>();
                const auto bindings = r.read_pointer();
                info.process_identifier = r.read_guid();
                info.process_host_id = r.read<uint64_t>();
                info.client_dependency = r.read<uint16_t>();
                const auto package = r.read_pointer();
                const auto user = r.read_pointer();
                const auto container = r.read_pointer();
                info.primary_oxid = r.read<uint64_t>();
                info.primary_ipid_rem_unknown = r.read_guid();

                if (extensions != 0)
                {
                    return std::nullopt;
                }

                if (bindings != 0)
                {
                    info.bindings = dual_string_array::read(r);
                }

                if (package != 0)
                {
                    info.package_full_name = read_hstring_pointee(r);
                }

                if (user != 0)
                {
                    info.user_sid = read_hstring_pointee(r);
                }

                if (container != 0)
                {
                    info.appcontainer_sid = read_hstring_pointee(r);
                }

                return r.ok() ? std::optional{info} : std::nullopt;
            }

            void write(ndr::writer& w) const
            {
                w.align(8);
                w.write(this->thread_id);
                w.write(this->process_id);
                w.write(this->authn_hint);
                w.write(this->version_major);
                w.write(this->version_minor);
                w.write(this->container_version);
                w.write(this->capability_flags);
                w.write_referent(false);
                w.write_guid(this->ipid_rem_unknown);
                w.write(this->flags);
                w.write_referent(this->bindings.has_value());
                w.write_guid(this->process_identifier);
                w.write(this->process_host_id);
                w.write(this->client_dependency);
                w.write(this->package_full_name ? user_marshal_marker : 0);
                w.write(this->user_sid ? user_marshal_marker : 0);
                w.write(this->appcontainer_sid ? user_marshal_marker : 0);
                w.write(this->primary_oxid);
                w.write_guid(this->primary_ipid_rem_unknown);

                if (this->bindings)
                {
                    this->bindings->write(w);
                }

                for (const auto* value : {&this->package_full_name, &this->user_sid, &this->appcontainer_sid})
                {
                    if (*value)
                    {
                        write_hstring_pointee(w, **value);
                    }
                }
            }
        };

        struct registered_oxid
        {
            uint64_t owner_context{};
            oxid_info info{};
        };

        struct class_registration
        {
            ndr::guid clsid{};
            uint64_t oxid{};
            ndr::guid ipid{};
            uint32_t flags{};
            uint32_t key{};
            uint64_t owner_context{};
        };

        struct client_context
        {
            uint32_t process_id{};
            uint64_t connection_id{};
            uint64_t primary_oxid{};
        };

        struct rpcss_state
        {
            uint64_t local_mid{};
            uint64_t process_signature{};
            ndr::guid process_identifier{};
            uint64_t next_id{};
            uint32_t next_registration_key{1};
            std::vector<uint8_t> security_descriptor{};

            std::map<ndr::guid, client_context> contexts{};
            std::map<uint64_t, registered_oxid> oxids{};
            std::vector<class_registration> classes{};

            uint64_t allocate_id()
            {
                return ++this->next_id;
            }
        };

        ndr::guid random_guid(std::mt19937_64& rng)
        {
            ndr::guid value{};
            for (size_t i = 0; i < value.size(); i += 8)
            {
                const auto r = rng();
                std::memcpy(value.data() + i, &r, 8);
            }

            return value;
        }

        // A self-relative security descriptor granting COM launch/activation/access to Everyone, SYSTEM and
        // Administrators, in the shape the default machine-wide COM permissions take.
        std::vector<uint8_t> make_com_security_descriptor()
        {
            const auto sid = [](const uint8_t authority, std::initializer_list<uint32_t> sub_authorities) {
                std::vector<uint8_t> bytes{1, static_cast<uint8_t>(sub_authorities.size()), 0, 0, 0, 0, 0, authority};
                for (const auto sub : sub_authorities)
                {
                    for (int i = 0; i < 4; ++i)
                    {
                        bytes.push_back(static_cast<uint8_t>(sub >> (8 * i)));
                    }
                }

                return bytes;
            };

            const auto everyone = sid(1, {0});
            const auto system = sid(5, {18});
            const auto administrators = sid(5, {32, 544});

            constexpr uint32_t com_rights_all = 0x1F;

            std::vector<uint8_t> acl{2, 0, 0, 0, 3, 0, 0, 0};
            for (const auto* trustee : {&everyone, &system, &administrators})
            {
                const auto ace_size = static_cast<uint16_t>(8 + trustee->size());
                acl.push_back(0);
                acl.push_back(0);
                acl.push_back(static_cast<uint8_t>(ace_size));
                acl.push_back(static_cast<uint8_t>(ace_size >> 8));
                for (int i = 0; i < 4; ++i)
                {
                    acl.push_back(static_cast<uint8_t>(com_rights_all >> (8 * i)));
                }

                acl.insert(acl.end(), trustee->begin(), trustee->end());
            }

            acl[2] = static_cast<uint8_t>(acl.size());
            acl[3] = static_cast<uint8_t>(acl.size() >> 8);

            constexpr uint16_t se_dacl_present = 0x0004;
            constexpr uint16_t se_self_relative = 0x8000;
            constexpr uint32_t header_size = 20;

            const auto owner_offset = header_size;
            const auto group_offset = owner_offset + static_cast<uint32_t>(administrators.size());
            const auto dacl_offset = group_offset + static_cast<uint32_t>(administrators.size());

            std::vector<uint8_t> sd(header_size);
            sd[0] = 1;
            const uint16_t control = se_dacl_present | se_self_relative;
            std::memcpy(sd.data() + 2, &control, 2);
            std::memcpy(sd.data() + 4, &owner_offset, 4);
            std::memcpy(sd.data() + 8, &group_offset, 4);
            std::memcpy(sd.data() + 16, &dacl_offset, 4);
            sd.insert(sd.end(), administrators.begin(), administrators.end());
            sd.insert(sd.end(), administrators.begin(), administrators.end());
            sd.insert(sd.end(), acl.begin(), acl.end());
            return sd;
        }

        ndr::guid read_context_handle(ndr::reader& r)
        {
            r.read<uint32_t>();
            return r.read_guid();
        }

        void write_context_handle(ndr::writer& w, const ndr::guid& uuid)
        {
            w.write(uint32_t{0});
            w.write_guid(uuid);
        }

        void write_hyper_array(ndr::writer& w, const std::vector<uint64_t>& values, const uint32_t max_count)
        {
            w.write(max_count);
            w.align(8);
            for (uint32_t i = 0; i < max_count; ++i)
            {
                w.write(i < values.size() ? values[i] : uint64_t{0});
            }
        }

        class local_object_exporter : public lrpc::rpc_interface
        {
          public:
            explicit local_object_exporter(std::shared_ptr<rpcss_state> state)
                : state_(std::move(state))
            {
            }

            ndr::guid uuid() const override
            {
                return iid_local_object_exporter;
            }

            result handle(windows_emulator& win_emu, const call& c) override
            {
                ndr::reader r{c.stub};
                switch (c.opnum)
                {
                case 0:
                    return this->connect(c, r);
                case 1:
                case 8:
                case 9:
                    return reply_status(0);
                case 2:
                    return this->default_security_permissions();
                case 3:
                    return this->allocate_reserved_ids(r);
                case 4:
                    return this->bulk_update_oids(r);
                case 5:
                    return this->client_resolve_oxid(r);
                case 6:
                    return this->server_allocate_oxid_and_oids(win_emu, c, r);
                case 7:
                    return this->server_allocate_oids(r);
                case 10:
                    return this->disconnect(r);
                case 11:
                    return this->updated_resolver_bindings();
                default:
                    return result::failure(lrpc::rpc_s_procnum_out_of_range);
                }
            }

            void on_disconnect(windows_emulator& /*win_emu*/, const uint64_t connection_id) override
            {
                std::erase_if(this->state_->contexts, [&](const auto& entry) { return entry.second.connection_id == connection_id; });
            }

          private:
            std::shared_ptr<rpcss_state> state_{};

            static result reply_status(const uint32_t status)
            {
                ndr::writer w{};
                w.write(status);
                return result::reply(w.take());
            }

            result connect(const call& c, ndr::reader& r)
            {
                r.read_unique_string();
                r.read_unique_string();
                if (r.read_pointer() != 0)
                {
                    constexpr size_t debug_info_size = 0x60;
                    r.align(8);
                    r.read_bytes(debug_info_size);
                }

                r.read<uint32_t>();
                const auto requested_ids = r.read<uint32_t>();
                if (!r.ok())
                {
                    return result::failure(lrpc::rpc_x_bad_stub_data);
                }

                std::mt19937_64 rng{this->state_->allocate_id() ^ c.message.connection_id};
                const auto context = random_guid(rng);
                this->state_->contexts[context] = client_context{
                    .process_id = c.message.sender_process_id,
                    .connection_id = c.message.connection_id,
                };

                std::vector<uint64_t> ids{};
                for (uint32_t i = 0; i < std::min(requested_ids, max_reserved_ids); ++i)
                {
                    ids.push_back(this->state_->allocate_id());
                }

                const auto& sd = this->state_->security_descriptor;

                ndr::writer w{};
                write_context_handle(w, context);
                w.write(ping_period_seconds);
                w.write_referent(true);
                dual_string_array::local(u"epmapper").write(w);
                w.write(this->state_->local_mid);
                write_hyper_array(w, ids, requested_ids);
                w.write(static_cast<uint32_t>(ids.size()));
                w.write(connect_flags);
                w.write(uint32_t{0});
                w.write(uint32_t{0});
                w.write_referent(false);
                w.write(authn_level_connect);
                w.write(imp_level_identify);
                w.write(uint32_t{0});
                w.write_referent(false);
                w.write(uint32_t{0});
                w.write_referent(false);
                w.write(int32_t{0});
                w.write_referent(false);
                w.write(c.message.sender_thread_id);
                w.write(rpcss_process_id);
                w.write(this->state_->process_signature);
                w.write_guid(this->state_->process_identifier);
                for (int i = 0; i < 5; ++i)
                {
                    w.write(static_cast<uint32_t>(sd.size()));
                }

                w.write(static_cast<uint32_t>(sd.size() * 5));
                w.write_referent(true);
                w.write(static_cast<uint32_t>(sd.size() * 5));
                for (int i = 0; i < 5; ++i)
                {
                    w.write_bytes(sd.data(), sd.size());
                }

                w.write(uint32_t{0});
                return result::reply(w.take());
            }

            result default_security_permissions()
            {
                const auto& sd = this->state_->security_descriptor;
                ndr::writer w{};
                for (int i = 0; i < 4; ++i)
                {
                    w.write(static_cast<uint32_t>(sd.size()));
                }

                w.write(static_cast<uint32_t>(sd.size() * 4));
                w.write_referent(true);
                w.write(static_cast<uint32_t>(sd.size() * 4));
                for (int i = 0; i < 4; ++i)
                {
                    w.write_bytes(sd.data(), sd.size());
                }

                w.write(uint32_t{0});
                return result::reply(w.take());
            }

            result allocate_reserved_ids(ndr::reader& r)
            {
                read_context_handle(r);
                const auto requested = r.read<uint32_t>();

                std::vector<uint64_t> ids{};
                for (uint32_t i = 0; i < std::max(1u, std::min(requested, max_reserved_ids)); ++i)
                {
                    ids.push_back(this->state_->allocate_id());
                }

                ndr::writer w{};
                write_hyper_array(w, ids, requested);
                w.write(static_cast<uint32_t>(std::min<size_t>(ids.size(), requested)));
                w.write(uint32_t{0});
                return result::reply(w.take());
            }

            static result bulk_update_oids(ndr::reader& r)
            {
                read_context_handle(r);
                const auto added = r.read<uint32_t>();

                ndr::writer w{};
                w.write(added);
                for (uint32_t i = 0; i < added; ++i)
                {
                    w.write(uint32_t{0});
                }

                w.write(uint32_t{0});
                return result::reply(w.take());
            }

            result client_resolve_oxid(ndr::reader& r)
            {
                read_context_handle(r);
                const auto oxid = r.read<uint64_t>();

                const auto entry = this->state_->oxids.find(oxid);
                ndr::writer w{};
                if (entry == this->state_->oxids.end())
                {
                    oxid_info{}.write(w);
                    w.write(uint64_t{0});
                    w.write(uint32_t{0});
                    w.write_referent(false);
                    w.write(uint16_t{0});
                    w.write(or_invalid_oxid);
                    return result::reply(w.take());
                }

                entry->second.info.write(w);
                w.write(this->state_->local_mid);
                w.write(uint32_t{0});
                w.write_referent(false);
                w.write(uint16_t{0x0A});
                w.write(uint32_t{0});
                return result::reply(w.take());
            }

            result server_allocate_oxid_and_oids(windows_emulator& win_emu, const call& c, ndr::reader& r)
            {
                const auto context = read_context_handle(r);
                const auto oid_count = r.read<uint32_t>();
                auto info = oxid_info::read(r);
                if (!info)
                {
                    win_emu.log.error("RPCSS: unsupported OXID_INFO from pid %u\n", c.message.sender_process_id);
                    return result::failure(lrpc::rpc_x_bad_stub_data);
                }

                const auto string_bindings = dual_string_array::read(r);
                const auto security_bindings = dual_string_array::read(r);
                if (!r.ok())
                {
                    return result::failure(lrpc::rpc_x_bad_stub_data);
                }

                dual_string_array combined{};
                combined.entries.assign(string_bindings.entries.begin(), string_bindings.entries.begin() + string_bindings.security_offset);
                combined.security_offset = static_cast<uint16_t>(combined.entries.size());
                combined.entries.insert(combined.entries.end(), security_bindings.entries.begin() + security_bindings.security_offset,
                                        security_bindings.entries.end());
                info->bindings = std::move(combined);

                const auto oxid = this->state_->allocate_id();
                auto& client = this->state_->contexts[context];
                if (client.primary_oxid == 0)
                {
                    client.primary_oxid = oxid;
                }

                this->state_->oxids[oxid] = registered_oxid{.owner_context = 0, .info = *info};

                std::vector<uint64_t> oids{};
                for (uint32_t i = 0; i < oid_count; ++i)
                {
                    oids.push_back(this->state_->allocate_id());
                }

                ndr::writer w{};
                w.write(oxid);
                w.write(client.primary_oxid);
                w.write(oid_count);
                w.write(uint32_t{0});
                w.write(oid_count);
                w.align(8);
                for (const auto oid : oids)
                {
                    w.write(oid);
                }

                w.write(oid_count);
                w.write(uint64_t{0});
                w.write_referent(false);
                w.write(uint32_t{0});
                return result::reply(w.take());
            }

            result server_allocate_oids(ndr::reader& r)
            {
                read_context_handle(r);
                r.read<uint64_t>();
                const auto returned = r.read<uint32_t>();
                r.read<uint32_t>();
                for (uint32_t i = 0; i < returned; ++i)
                {
                    r.read<uint64_t>();
                }

                const auto requested = r.read<uint32_t>();

                std::vector<uint64_t> oids{};
                for (uint32_t i = 0; i < requested; ++i)
                {
                    oids.push_back(this->state_->allocate_id());
                }

                ndr::writer w{};
                write_hyper_array(w, oids, requested);
                w.write(requested);
                w.write(uint32_t{0});
                return result::reply(w.take());
            }

            result disconnect(ndr::reader& r)
            {
                const auto context = read_context_handle(r);
                this->state_->contexts.erase(context);

                ndr::writer w{};
                write_context_handle(w, {});
                w.write(uint32_t{0});
                return result::reply(w.take());
            }

            static result updated_resolver_bindings()
            {
                ndr::writer w{};
                w.write_referent(true);
                dual_string_array::local(u"epmapper").write(w);
                w.write(uint64_t{1});
                w.write(uint32_t{0});
                return result::reply(w.take());
            }
        };

        class scm_interface : public lrpc::rpc_interface
        {
          public:
            explicit scm_interface(std::shared_ptr<rpcss_state> state)
                : state_(std::move(state))
            {
            }

            ndr::guid uuid() const override
            {
                return iid_scm;
            }

            result handle(windows_emulator& win_emu, const call& c) override
            {
                ndr::reader r{c.stub};
                switch (c.opnum)
                {
                case 0:
                    return this->notify_started(win_emu, r);
                case 1:
                    return this->notify_stopped(r);
                case 3:
                case 5:
                    return reply_rpcstat();
                case 4: {
                    ndr::writer w{};
                    w.write(rpcss_thread_id);
                    w.write(uint32_t{0});
                    return result::reply(w.take());
                }
                default:
                    win_emu.log.warn("RPCSS: ISCM opnum %u not implemented\n", c.opnum);
                    return reply_hresult(hresult_ok);
                }
            }

          private:
            std::shared_ptr<rpcss_state> state_{};

            static result reply_rpcstat()
            {
                ndr::writer w{};
                w.write(uint32_t{0});
                return result::reply(w.take());
            }

            static result reply_hresult(const uint32_t hresult)
            {
                ndr::writer w{};
                w.write(uint32_t{0});
                w.write(hresult);
                return result::reply(w.take());
            }

            result notify_started(windows_emulator& win_emu, ndr::reader& r)
            {
                const auto context = read_context_handle(r);
                r.read<uint32_t>();
                const auto count = r.read<uint32_t>();

                std::vector<uint32_t> keys{};
                for (uint32_t i = 0; i < count && r.ok(); ++i)
                {
                    class_registration entry{};
                    entry.clsid = r.read_guid();
                    entry.oxid = r.read<uint64_t>();
                    entry.ipid = r.read_guid();
                    entry.flags = r.read<uint32_t>();
                    r.read<uint32_t>();
                    entry.key = this->state_->next_registration_key++;
                    entry.owner_context = 0;
                    keys.push_back(entry.key);
                    this->state_->classes.push_back(entry);
                    win_emu.log.info("RPCSS: class registered by pid %u (key %u)\n",
                                     this->state_->contexts.contains(context) ? this->state_->contexts[context].process_id : 0, entry.key);
                }

                if (!r.ok())
                {
                    return result::failure(lrpc::rpc_x_bad_stub_data);
                }

                ndr::writer w{};
                w.write_referent(true);
                w.write(count);
                w.write(count);
                for (const auto key : keys)
                {
                    w.write(key);
                }

                w.write(uint32_t{0});
                w.write(hresult_ok);
                return result::reply(w.take());
            }

            result notify_stopped(ndr::reader& r)
            {
                read_context_handle(r);
                r.read<uint32_t>();
                const auto count = r.read<uint32_t>();
                for (uint32_t i = 0; i < count && r.ok(); ++i)
                {
                    r.read_guid();
                    const auto key = r.read<uint32_t>();
                    std::erase_if(this->state_->classes, [&](const class_registration& entry) { return entry.key == key; });
                }

                return reply_rpcstat();
            }
        };

        class rot_interface : public lrpc::rpc_interface
        {
          public:
            ndr::guid uuid() const override
            {
                return iid_rot;
            }

            result handle(windows_emulator& win_emu, const call& c) override
            {
                ndr::writer w{};
                switch (c.opnum)
                {
                case 2:
                    w.write(uint32_t{0});
                    w.write(hresult_false);
                    return result::reply(w.take());
                case 3:
                    w.write(uint64_t{0});
                    w.write(uint32_t{0});
                    w.write(uint32_t{0});
                    w.write_referent(false);
                    w.write(int32_t{0});
                    w.write(uint32_t{0});
                    w.write(uint32_t{0});
                    w.write(mk_e_unavailable);
                    return result::reply(w.take());
                default:
                    win_emu.log.warn("RPCSS: IROT opnum %u not implemented\n", c.opnum);
                    return result::failure(lrpc::rpc_s_procnum_out_of_range);
                }
            }
        };

        class machine_activator_control : public lrpc::rpc_interface
        {
          public:
            ndr::guid uuid() const override
            {
                return iid_machine_activator_control;
            }

            result handle(windows_emulator& /*win_emu*/, const call& /*c*/) override
            {
                ndr::writer w{};
                w.write(uint32_t{0});
                w.write(hresult_ok);
                return result::reply(w.take());
            }
        };

        class scm_local_activator : public lrpc::rpc_interface
        {
          public:
            explicit scm_local_activator(std::shared_ptr<rpcss_state> state)
                : state_(std::move(state))
            {
            }

            ndr::guid uuid() const override
            {
                return iid_scm_local_activator;
            }

            result handle(windows_emulator& win_emu, const call& c) override
            {
                win_emu.log.warn("RPCSS: activation request (opnum %u, %zu bytes) from pid %u is not supported yet\n", c.opnum,
                                 c.stub.size(), c.message.sender_process_id);
                (void)regdb_e_classnotreg;
                return result::failure(lrpc::rpc_s_procnum_out_of_range);
            }

          private:
            std::shared_ptr<rpcss_state> state_{};
        };
    }

    void start_rpcss(windows_emulator& win_emu)
    {
        std::random_device device{};
        std::mt19937_64 rng{(static_cast<uint64_t>(device()) << 32) | device()};

        auto state = std::make_shared<rpcss_state>();
        state->local_mid = rng() | 1;
        state->process_signature = rng() | 1;
        state->process_identifier = random_guid(rng);
        state->next_id = rng() & 0x00FFFFFFFFFFFFFFULL;
        state->security_descriptor = make_com_security_descriptor();

        auto server = std::make_shared<lrpc::server>();
        server->add_interface(std::make_shared<local_object_exporter>(state));
        server->add_interface(std::make_shared<scm_interface>(state));
        server->add_interface(std::make_shared<rot_interface>());
        server->add_interface(std::make_shared<machine_activator_control>());
        server->add_interface(std::make_shared<scm_local_activator>(state));

        if (!alpc::create_host_port(win_emu, u"\\RPC Control\\epmapper", std::move(server)))
        {
            win_emu.log.error("RPCSS: failed to create the epmapper endpoint\n");
        }
    }
}
