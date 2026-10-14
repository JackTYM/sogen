#include "../std_include.hpp"
#include "assembly_resolver.hpp"

namespace sogen::sxs
{
    namespace
    {
        constexpr uint32_t status_sxs_assembly_not_found = 0xC0150004;
        constexpr uint32_t status_sxs_manifest_parse_error = 0xC0150006;

        constexpr std::u16string_view manifest_extension = u".manifest";

        std::u16string lowercase(const std::u16string_view text)
        {
            std::u16string result(text);
            for (auto& c : result)
            {
                if (c >= u'A' && c <= u'Z')
                {
                    c = static_cast<char16_t>(c + 32);
                }
            }

            return result;
        }

        bool is_wildcard(const std::u16string_view value)
        {
            return value.empty() || value == u"*";
        }

        class resolver
        {
          public:
            resolver(const resolution_request& request, assembly_store& store)
                : request_(request),
                  store_(store),
                  manifests_directory_(request.windows_directory + u"\\WinSxS\\Manifests")
            {
            }

            std::variant<activation_context_input, uint32_t> run()
            {
                auto root_manifest = parse_manifest(this->request_.root_manifest);
                if (!root_manifest)
                {
                    return status_sxs_manifest_parse_error;
                }

                resolved_assembly root{};
                root.identity = root_manifest->identity;
                root.content = std::move(*root_manifest);
                root.manifest_path = this->request_.root_manifest_path;
                root.flags = assembly_information_root_assembly | assembly_information_private_assembly;
                this->assemblies_.push_back(std::move(root));

                if (const auto status = this->bind_dependencies(0); status != 0)
                {
                    return status;
                }

                activation_context_input input{};
                input.assemblies = std::move(this->assemblies_);
                input.application_directory = this->request_.application_directory;
                input.resource_name = this->request_.resource_name;
                return input;
            }

          private:
            const resolution_request& request_;
            assembly_store& store_;
            std::u16string manifests_directory_{};
            std::optional<std::vector<std::u16string>> manifest_names_{};
            std::vector<resolved_assembly> assemblies_{};

            const std::vector<std::u16string>& manifest_names()
            {
                if (!this->manifest_names_)
                {
                    this->manifest_names_ = std::vector<std::u16string>{};
                    for (const auto& name : this->store_.list_files(this->manifests_directory_))
                    {
                        this->manifest_names_->push_back(lowercase(name));
                    }
                }

                return *this->manifest_names_;
            }

            uint32_t bind_dependencies(const size_t assembly_index)
            {
                const auto dependencies = this->assemblies_[assembly_index].content.dependencies;
                for (const auto& dependency : dependencies)
                {
                    if (dependency.identity.type == u"win32-policy")
                    {
                        continue;
                    }

                    auto bound = this->bind(dependency.identity);
                    if (!bound)
                    {
                        if (dependency.optional)
                        {
                            continue;
                        }

                        return status_sxs_assembly_not_found;
                    }

                    const auto encoded = lowercase(bound->identity.encode());
                    const bool already_bound = std::ranges::any_of(this->assemblies_, [&](const resolved_assembly& existing) {
                        return lowercase(existing.identity.encode()) == encoded;
                    });
                    if (already_bound)
                    {
                        continue;
                    }

                    this->assemblies_.push_back(std::move(*bound));
                    if (const auto status = this->bind_dependencies(this->assemblies_.size() - 1); status != 0)
                    {
                        return status;
                    }
                }

                return 0;
            }

            std::u16string architecture_of(const assembly_identity& identity) const
            {
                return is_wildcard(identity.processor_architecture) ? this->request_.processor_architecture
                                                                    : lowercase(identity.processor_architecture);
            }

            std::optional<assembly_version> apply_publisher_policy(const assembly_identity& identity, const assembly_version& requested,
                                                                   bool& policy_applied)
            {
                const auto prefix = this->architecture_of(identity) + u"_policy." + u8_to_u16(std::to_string(requested[0])) + u"." +
                                    u8_to_u16(std::to_string(requested[1])) + u"." + lowercase(identity.name) + u"_" +
                                    lowercase(identity.public_key_token) + u"_";

                std::optional<assembly_version> redirected{};
                for (const auto& name : this->manifest_names())
                {
                    if (!name.starts_with(prefix) || !name.ends_with(manifest_extension))
                    {
                        continue;
                    }

                    const auto data = this->store_.read_file(this->manifests_directory_ + u"\\" + name);
                    const auto policy = data ? parse_manifest(*data) : std::nullopt;
                    if (!policy)
                    {
                        continue;
                    }

                    for (const auto& redirect : policy->binding_redirects)
                    {
                        if (requested >= redirect.old_low && requested <= redirect.old_high &&
                            (!redirected || redirect.new_version > *redirected))
                        {
                            redirected = redirect.new_version;
                        }
                    }
                }

                policy_applied = redirected.has_value();
                return redirected ? redirected : requested;
            }

            std::optional<resolved_assembly> bind_from_store(const assembly_identity& identity)
            {
                const auto requested = parse_version(identity.version);
                if (!requested)
                {
                    return std::nullopt;
                }

                bool policy_applied = false;
                const auto version = this->apply_publisher_policy(identity, *requested, policy_applied);
                const auto language = is_wildcard(identity.language) ? std::u16string(u"none") : lowercase(identity.language);
                const auto prefix =
                    this->architecture_of(identity) + u"_" + lowercase(identity.name) + u"_" + lowercase(identity.public_key_token) + u"_";

                // Without a publisher policy, the component store binds a request to the servicing winner of its
                // major.minor family (HKLM\...\SideBySide\Winners), which is the newest installed version.
                std::optional<std::pair<assembly_version, std::u16string>> best{};
                for (const auto& name : this->manifest_names())
                {
                    if (!name.starts_with(prefix) || !name.ends_with(manifest_extension))
                    {
                        continue;
                    }

                    const auto rest = std::u16string_view(name).substr(prefix.size());
                    const auto version_end = rest.find(u'_');
                    const auto language_end = version_end == std::u16string_view::npos ? version_end : rest.find(u'_', version_end + 1);
                    if (language_end == std::u16string_view::npos ||
                        rest.substr(version_end + 1, language_end - version_end - 1) != language)
                    {
                        continue;
                    }

                    const auto candidate = parse_version(rest.substr(0, version_end));
                    const bool matches =
                        candidate &&
                        (policy_applied ? *candidate == *version : (*candidate)[0] == (*version)[0] && (*candidate)[1] == (*version)[1]);
                    if (matches && (!best || *candidate > best->first))
                    {
                        best.emplace(*candidate, name);
                    }
                }

                if (!best)
                {
                    return std::nullopt;
                }

                const auto& name = best->second;
                const auto path = this->manifests_directory_ + u"\\" + name;
                const auto data = this->store_.read_file(path);
                auto content = data ? parse_manifest(*data) : std::nullopt;
                if (!content)
                {
                    return std::nullopt;
                }

                resolved_assembly assembly{};
                assembly.identity = content->identity;
                assembly.content = std::move(*content);
                assembly.manifest_path = path;
                assembly.directory_name = name.substr(0, name.size() - manifest_extension.size());
                assembly.flags = policy_applied ? assembly_information_policy_applied : 0;
                return assembly;
            }

            std::optional<resolved_assembly> bind_private(const assembly_identity& identity)
            {
                const auto& directory = this->request_.application_directory;
                if (directory.empty())
                {
                    return std::nullopt;
                }

                const auto base = directory.ends_with(u'\\') ? directory : directory + u"\\";
                for (const auto& path : {base + identity.name + u".manifest", base + identity.name + u"\\" + identity.name + u".manifest"})
                {
                    const auto data = this->store_.read_file(path);
                    auto content = data ? parse_manifest(*data) : std::nullopt;
                    if (!content)
                    {
                        continue;
                    }

                    resolved_assembly assembly{};
                    assembly.identity = content->identity;
                    assembly.content = std::move(*content);
                    assembly.manifest_path = path;
                    assembly.flags = assembly_information_private_assembly;
                    return assembly;
                }

                return std::nullopt;
            }

            std::optional<resolved_assembly> bind(const assembly_identity& identity)
            {
                if (identity.name.empty())
                {
                    return std::nullopt;
                }

                if (!identity.public_key_token.empty())
                {
                    return this->bind_from_store(identity);
                }

                return this->bind_private(identity);
            }
        };
    }

    std::variant<activation_context_input, uint32_t> resolve_activation_context(const resolution_request& request, assembly_store& store)
    {
        return resolver(request, store).run();
    }
}
