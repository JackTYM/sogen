#pragma once

// Apple ships <mach/mach_vm.h> for macOS but has the iOS/iOS-Simulator SDKs #error out of it
// entirely ("mach_vm.h unsupported."). The routines this backend calls
// (mach_vm_allocate/mach_vm_deallocate/mach_vm_remap/mach_vm_region/mach_vm_read_overwrite) are
// still real, linkable Mach traps on iOS - confirmed present in the iOS/iOS-Simulator SDKs'
// libSystem.tbd export list - only the convenience header is withheld. Declare just those
// routines ourselves, with the same signatures the withheld header would have provided
// (mach/mach_vm.h on the macOS SDK).
#include <mach/mach_types.h>
#include <mach/vm_types.h>
#include <mach/vm_region.h>
#include <mach/vm_inherit.h>
#include <mach/vm_prot.h>
#include <mach/kern_return.h>
#include <mach/message.h>

extern "C"
{
    kern_return_t mach_vm_allocate(vm_map_t target, mach_vm_address_t* address, mach_vm_size_t size, int flags);

    kern_return_t mach_vm_deallocate(vm_map_t target, mach_vm_address_t address, mach_vm_size_t size);

    kern_return_t mach_vm_remap(vm_map_t target_task, mach_vm_address_t* target_address, mach_vm_size_t size, mach_vm_offset_t mask,
                                int flags, vm_map_t src_task, mach_vm_address_t src_address, boolean_t copy, vm_prot_t* cur_protection,
                                vm_prot_t* max_protection, vm_inherit_t inheritance);

    kern_return_t mach_vm_region(vm_map_read_t target_task, mach_vm_address_t* address, mach_vm_size_t* size, vm_region_flavor_t flavor,
                                 vm_region_info_t info, mach_msg_type_number_t* infoCnt, mach_port_t* object_name);

    kern_return_t mach_vm_read_overwrite(vm_map_read_t target_task, mach_vm_address_t address, mach_vm_size_t size, mach_vm_address_t data,
                                         mach_vm_size_t* outsize);
}
