#include "std_include.hpp"
#include "module/sxs_resolver.hpp"
#include <gtest/gtest.h>

namespace sogen
{
    TEST(SxsResolver, ResolvesViaWinnersTable)
    {
        const assembly_identity identity{.name = "Microsoft.Windows.Common-Controls",
                                         .version = "6.0.0.0",
                                         .processor_architecture = "amd64",
                                         .public_key_token = "6595b64144ccf1df",
                                         .language = "*"};

        const std::vector<std::string> manifests = {
            "amd64_microsoft.windows.common-controls_6595b64144ccf1df_5.82.26100.32860_none_7573b06a63bfff31.manifest",
            "amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.26100.33438_none_ee36e391daefe08a.manifest",
        };

        const auto winners = [](const std::string& key) -> std::optional<std::string> {
            if (key == "amd64_microsoft.windows.common-controls_6595b64144ccf1df")
            {
                return "amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.26100.33438_none_ee36e391daefe08a";
            }
            return std::nullopt;
        };

        const auto resolved = resolve_assembly(identity, winners, manifests, false);
        ASSERT_TRUE(resolved.has_value());
        EXPECT_EQ(*resolved, "amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.26100.33438_none_ee36e391daefe08a");
    }

    TEST(SxsResolver, FallsBackToHighestVersionWithoutWinnersEntry)
    {
        const assembly_identity identity{.name = "Microsoft.Windows.Common-Controls",
                                         .version = "6.0.0.0",
                                         .processor_architecture = "amd64",
                                         .public_key_token = "6595b64144ccf1df",
                                         .language = "*"};

        const std::vector<std::string> manifests = {
            "amd64_microsoft.windows.common-controls_6595b64144ccf1df_5.82.26100.32860_none_7573b06a63bfff31.manifest",
            "amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.26100.33438_none_ee36e391daefe08a.manifest",
        };

        const auto no_winners = [](const std::string&) -> std::optional<std::string> { return std::nullopt; };

        const auto resolved = resolve_assembly(identity, no_winners, manifests, false);
        ASSERT_TRUE(resolved.has_value());
        EXPECT_EQ(*resolved, "amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.26100.33438_none_ee36e391daefe08a");
    }

    TEST(SxsResolver, ArchitectureMismatchNeverResolves)
    {
        const assembly_identity identity{.name = "Microsoft.Windows.Common-Controls",
                                         .version = "6.0.0.0",
                                         .processor_architecture = "x86",
                                         .public_key_token = "6595b64144ccf1df",
                                         .language = "*"};

        const std::vector<std::string> manifests = {
            "amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.26100.33438_none_ee36e391daefe08a.manifest",
        };

        const auto no_winners = [](const std::string&) -> std::optional<std::string> { return std::nullopt; };

        EXPECT_FALSE(resolve_assembly(identity, no_winners, manifests, false).has_value());
    }

    TEST(SxsResolver, WildcardArchitectureFollowsProcessBitness)
    {
        const assembly_identity identity{.name = "Microsoft.Windows.Common-Controls",
                                         .version = "6.0.0.0",
                                         .processor_architecture = "*",
                                         .public_key_token = "6595b64144ccf1df",
                                         .language = "*"};

        const std::vector<std::string> manifests = {
            "amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.26100.33438_none_ee36e391daefe08a.manifest",
            "x86_microsoft.windows.common-controls_6595b64144ccf1df_6.0.26100.33296_none_35e18d58ef6e765a.manifest",
        };

        const auto no_winners = [](const std::string&) -> std::optional<std::string> { return std::nullopt; };

        EXPECT_EQ(resolve_assembly(identity, no_winners, manifests, false),
                  "amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.26100.33438_none_ee36e391daefe08a");
        EXPECT_EQ(resolve_assembly(identity, no_winners, manifests, true),
                  "x86_microsoft.windows.common-controls_6595b64144ccf1df_6.0.26100.33296_none_35e18d58ef6e765a");
    }

    TEST(SxsResolver, NoMatchReturnsNullopt)
    {
        const assembly_identity identity{.name = "Something.Nobody.Collected",
                                         .version = "1.0.0.0",
                                         .processor_architecture = "amd64",
                                         .public_key_token = "0000000000000000",
                                         .language = "*"};

        const std::vector<std::string> manifests = {
            "amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.26100.33438_none_ee36e391daefe08a.manifest",
        };

        const auto no_winners = [](const std::string&) -> std::optional<std::string> { return std::nullopt; };

        EXPECT_FALSE(resolve_assembly(identity, no_winners, manifests, false).has_value());
    }
}
