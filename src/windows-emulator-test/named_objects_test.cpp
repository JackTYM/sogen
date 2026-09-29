#include "emulation_test_utils.hpp"

#include <named_objects.hpp>
#include <object_name.hpp>

namespace sogen::test
{
    TEST(ObjectNameTest, RelativeNamesLiveInTheSessionDirectory)
    {
        ASSERT_EQ(canonical_object_name(u"Foo"), u"\\sessions\\1\\basenamedobjects\\foo");
        ASSERT_EQ(canonical_object_name(u"Local\\Foo"), u"\\sessions\\1\\basenamedobjects\\foo");
        ASSERT_EQ(canonical_object_name(u"Session\\1\\Foo"), u"\\sessions\\1\\basenamedobjects\\foo");
        ASSERT_EQ(canonical_object_name(u"Session\\3\\Foo"), u"\\sessions\\3\\basenamedobjects\\foo");
    }

    TEST(ObjectNameTest, GlobalNamesUseTheGlobalDirectory)
    {
        ASSERT_EQ(canonical_object_name(u"Global\\Foo"), u"\\basenamedobjects\\foo");
        ASSERT_EQ(canonical_object_name(u"\\BaseNamedObjects\\Foo"), u"\\basenamedobjects\\foo");
        ASSERT_TRUE(object_names_equal(u"GLOBAL\\Foo", u"\\BaseNamedObjects\\FOO"));
        ASSERT_FALSE(object_names_equal(u"Global\\Foo", u"Foo"));
    }

    TEST(ObjectNameTest, AbsolutePathsOutsideTheDirectoriesAreKept)
    {
        ASSERT_EQ(canonical_object_name(u"\\KernelObjects\\SystemErrorPortReady"), u"\\kernelobjects\\systemerrorportready");
    }

    TEST(KernelArenaTest, RegistryFindsPublishedNamesAndReusesStaleEntries)
    {
        const auto arena = kernel_arena::create();
        ASSERT_NE(arena, nullptr);

        const auto first = arena->allocate();
        ASSERT_TRUE(first.has_value());
        ASSERT_FALSE(arena->register_named(u"name", kernel_arena::named_object_kind::event, *first).has_value());

        const auto found = arena->find_named(u"name");
        ASSERT_TRUE(found.has_value());
        ASSERT_EQ(found->slot, *first);
        ASSERT_EQ(found->kind, kernel_arena::named_object_kind::event);
        arena->release(found->slot);

        const auto second = arena->allocate();
        ASSERT_TRUE(second.has_value());
        const auto raced = arena->register_named(u"name", kernel_arena::named_object_kind::event, *second);
        ASSERT_TRUE(raced.has_value());
        ASSERT_EQ(raced->slot, *first);
        arena->release(raced->slot);
        arena->release(*second);

        arena->release(*first);
        ASSERT_FALSE(arena->find_named(u"name").has_value());

        const auto third = arena->allocate();
        ASSERT_TRUE(third.has_value());
        ASSERT_FALSE(arena->register_named(u"name", kernel_arena::named_object_kind::mutant, *third).has_value());
        const auto renamed = arena->find_named(u"name");
        ASSERT_TRUE(renamed.has_value());
        ASSERT_EQ(renamed->kind, kernel_arena::named_object_kind::mutant);
    }

    namespace
    {
        void share_arena(process_context& from, process_context& to)
        {
            ASSERT_NE(from.ensure_shared_arena(), nullptr);
            to.shared_arena = kernel_arena::adopt(from.shared_arena->backing());
            ASSERT_NE(to.shared_arena, nullptr);
        }
    }

    TEST(NamedObjectsTest, EventPublishedInOneProcessIsOpenedByNameInAnother)
    {
        auto a = create_empty_emulator();
        auto b = create_empty_emulator();
        share_arena(a.process, b.process);

        event created{};
        created.type = SynchronizationEvent;
        ASSERT_EQ(named_objects::publish(a.process, named_objects::kind::event, u"Global\\Ready", created.state), STATUS_SUCCESS);

        event opened{};
        ASSERT_EQ(named_objects::open(b.process, named_objects::kind::event, u"\\BaseNamedObjects\\ready", opened.state), STATUS_SUCCESS);

        created.set_signaled(true);
        ASSERT_TRUE(opened.is_signaled());
        ASSERT_TRUE(opened.try_consume_signal());
        ASSERT_FALSE(created.is_signaled());

        event other{};
        ASSERT_EQ(named_objects::open(b.process, named_objects::kind::event, u"Missing", other.state), STATUS_OBJECT_NAME_NOT_FOUND);
        ASSERT_EQ(named_objects::open(b.process, named_objects::kind::mutant, u"Global\\Ready", other.state), STATUS_OBJECT_TYPE_MISMATCH);
    }

    TEST(NamedObjectsTest, SecondPublisherOfANameJoinsTheFirst)
    {
        auto a = create_empty_emulator();
        auto b = create_empty_emulator();
        share_arena(a.process, b.process);

        semaphore first{};
        first.initialize(2, 5);
        ASSERT_EQ(named_objects::publish(a.process, named_objects::kind::semaphore, u"Sem", first.state), STATUS_SUCCESS);

        semaphore second{};
        second.initialize(9, 9);
        ASSERT_EQ(named_objects::publish(b.process, named_objects::kind::semaphore, u"sem", second.state), STATUS_OBJECT_NAME_EXISTS);

        ASSERT_EQ(second.current_count(), 2u);
        ASSERT_TRUE(second.try_lock());
        ASSERT_EQ(first.current_count(), 1u);
    }

#if !defined(_WIN32) && !defined(OS_EMSCRIPTEN)
    TEST(NamedObjectsTest, NamedSectionSharesItsPagesByName)
    {
        auto a = create_empty_emulator();
        auto b = create_empty_emulator();
        share_arena(a.process, b.process);

        section_object created{};
        created.name = u"Local\\Pages";
        created.maximum_size = 0x2000;
        created.section_page_protection = PAGE_READWRITE;
        created.allocation_attributes = SEC_COMMIT;
        ASSERT_TRUE(named_objects::is_shareable_named_section(created));
        ASSERT_EQ(named_objects::publish_section(a.process, created), STATUS_SUCCESS);
        ASSERT_NE(created.backing, nullptr);

        created.backing->data()[0x1234] = std::byte{0x42};

        section_object opened{};
        ASSERT_EQ(named_objects::open_section(b.process, u"Pages", opened), STATUS_SUCCESS);
        ASSERT_NE(opened.backing, nullptr);
        ASSERT_NE(opened.backing->data(), created.backing->data());
        ASSERT_EQ(opened.maximum_size, 0x2000u);
        ASSERT_EQ(opened.section_page_protection, static_cast<uint32_t>(PAGE_READWRITE));
        ASSERT_EQ(opened.backing->data()[0x1234], std::byte{0x42});

        opened.backing->data()[0x10] = std::byte{0x7a};
        ASSERT_EQ(created.backing->data()[0x10], std::byte{0x7a});

        section_object duplicate{};
        duplicate.name = u"Pages";
        duplicate.maximum_size = 0x2000;
        duplicate.section_page_protection = PAGE_READWRITE;
        duplicate.allocation_attributes = SEC_COMMIT;
        ASSERT_EQ(named_objects::publish_section(b.process, duplicate), STATUS_OBJECT_NAME_EXISTS);
        ASSERT_EQ(duplicate.backing->data()[0x1234], std::byte{0x42});
    }
#endif
}
