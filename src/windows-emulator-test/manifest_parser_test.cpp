#include "std_include.hpp"
#include "module/manifest_parser.hpp"
#include <gtest/gtest.h>

namespace sogen
{
    namespace
    {
        constexpr auto notepad_plus_plus_manifest = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0"><assemblyIdentity version="1.0.0.0" processorArchitecture="*" name="Notepad++" type="win32"></assemblyIdentity><description>Notepad++</description><dependency><dependentAssembly><assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0" processorArchitecture="*" publicKeyToken="6595b64144ccf1df" language="*"></assemblyIdentity></dependentAssembly></dependency></assembly>)";

        constexpr auto no_dependency_manifest = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0"><assemblyIdentity version="1.0.0.0" processorArchitecture="*" name="SomeApp" type="win32"></assemblyIdentity></assembly>)";
    }

    TEST(ManifestParser, FindsSingleDependentAssembly)
    {
        const auto assemblies = parse_dependent_assemblies(notepad_plus_plus_manifest);
        ASSERT_EQ(assemblies.size(), 1);
        EXPECT_EQ(assemblies[0].name, "Microsoft.Windows.Common-Controls");
        EXPECT_EQ(assemblies[0].version, "6.0.0.0");
        EXPECT_EQ(assemblies[0].public_key_token, "6595b64144ccf1df");
        EXPECT_EQ(assemblies[0].processor_architecture, "*");
    }

    TEST(ManifestParser, NoDependenciesReturnsEmpty)
    {
        const auto assemblies = parse_dependent_assemblies(no_dependency_manifest);
        EXPECT_TRUE(assemblies.empty());
    }

    TEST(ManifestParser, ExtractsRedirectedFileNames)
    {
        constexpr auto assembly_manifest =
            R"(<assembly xmlns="urn:schemas-microsoft-com:asm.v1"><file name="comctl32.dll"></file></assembly>)";
        const auto files = parse_redirected_file_names(assembly_manifest);
        ASSERT_EQ(files.size(), 1);
        EXPECT_EQ(files[0], "comctl32.dll");
    }

    TEST(ManifestParser, ParsesSelfIdentityFromTopLevelAssemblyIdentity)
    {
        const auto identity = parse_self_identity(notepad_plus_plus_manifest);
        ASSERT_TRUE(identity.has_value());
        EXPECT_EQ(identity->name, "Notepad++");
        EXPECT_EQ(identity->version, "1.0.0.0");
        EXPECT_EQ(identity->processor_architecture, "*");
        EXPECT_TRUE(identity->public_key_token.empty());
    }

    TEST(ManifestParser, SelfIdentityIgnoresDependentAssemblyIdentity)
    {
        // The dependent assembly's own <assemblyIdentity> (Common-Controls) must never be
        // mistaken for the exe's own self-identity - only the first, top-level one counts.
        const auto identity = parse_self_identity(notepad_plus_plus_manifest);
        ASSERT_TRUE(identity.has_value());
        EXPECT_NE(identity->name, "Microsoft.Windows.Common-Controls");
    }

    TEST(ManifestParser, NoManifestTextReturnsNulloptForSelfIdentity)
    {
        EXPECT_FALSE(parse_self_identity("").has_value());
    }
}
