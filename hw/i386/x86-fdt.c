/*
 * Device tree generation for x86 machines
 *
 * This builds the part of the tree that every x86 machine can describe: the
 * CPUs and the interrupt controllers.  Machines with more to say add their own
 * nodes on top of it -- see microvm-dt.c for the virtio-mmio transports, the
 * PCIe ECAM and the ISA devices microvm exposes.
 *
 * The consumers are firmware that would rather not parse ACPI (edk2, Coconut
 * SVSM) and, when the tree is handed over as an IGVM device tree parameter, a
 * Linux guest booting without ACPI tables: arch/x86/kernel/devicetree.c turns
 * /cpus into registered local APICs, which is the only way such a guest ever
 * learns that it has more than one CPU.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "hw/core/boards.h"
#include "hw/i386/x86.h"
#include "hw/intc/ioapic.h"
#include "hw/core/sysbus.h"
#include "system/device_tree.h"
#include "target/i386/cpu.h"

/*
 * One node per CPU that exists, keyed by APIC ID.  #address-cells is not
 * decoration: a consumer reads the APIC ID out of "reg" using the cell count
 * declared here, and Linux's of_get_cpu_hwid() gives up without it.
 */
static void x86_fdt_add_cpus(X86MachineState *x86ms)
{
    MachineState *ms = MACHINE(x86ms);
    const CPUArchIdList *possible_cpus = ms->possible_cpus;
    int i;

    qemu_fdt_add_subnode(ms->fdt, "/cpus");
    qemu_fdt_setprop_cell(ms->fdt, "/cpus", "#address-cells", 1);
    qemu_fdt_setprop_cell(ms->fdt, "/cpus", "#size-cells", 0);

    for (i = 0; i < possible_cpus->len; i++) {
        uint64_t apic_id = possible_cpus->cpus[i].arch_id;
        char *nodename;

        /* Hotpluggable slots that are still empty have nothing to describe */
        if (!possible_cpus->cpus[i].cpu) {
            continue;
        }

        nodename = g_strdup_printf("/cpus/cpu@%" PRIx64, apic_id);
        qemu_fdt_add_subnode(ms->fdt, nodename);
        qemu_fdt_setprop_string(ms->fdt, nodename, "device_type", "cpu");
        qemu_fdt_setprop_cell(ms->fdt, nodename, "reg", apic_id);
        g_free(nodename);
    }
}

static void x86_fdt_add_lapic(X86MachineState *x86ms)
{
    MachineState *ms = MACHINE(x86ms);
    char *nodename = g_strdup_printf("/lapic@%x", APIC_DEFAULT_ADDRESS);

    qemu_fdt_add_subnode(ms->fdt, nodename);
    qemu_fdt_setprop_string(ms->fdt, nodename,
                            "compatible", "intel,ce4100-lapic");
    qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg",
                                 2, APIC_DEFAULT_ADDRESS, 2, 0x1000);
    /*
     * There is no i8259 to hand over from, so the local APIC is wired up
     * directly rather than through an IMCR.
     */
    qemu_fdt_setprop(ms->fdt, nodename, "intel,virtual-wire-mode", NULL, 0);

    g_free(nodename);
}

static void x86_fdt_add_ioapic(X86MachineState *x86ms, SysBusDevice *dev)
{
    MachineState *ms = MACHINE(x86ms);
    hwaddr base = dev->mmio[0].addr;
    char *nodename;
    uint32_t ph;
    int index;

    switch (base) {
    case IO_APIC_DEFAULT_ADDRESS:
        index = 0;
        break;
    case IO_APIC_SECONDARY_ADDRESS:
        index = 1;
        break;
    default:
        warn_report("unknown ioapic @ %" PRIx64, base);
        return;
    }

    nodename = g_strdup_printf("/ioapic%d@%" PRIx64, index + 1, base);
    qemu_fdt_add_subnode(ms->fdt, nodename);
    qemu_fdt_setprop_string(ms->fdt, nodename,
                            "compatible", "intel,ce4100-ioapic");
    qemu_fdt_setprop(ms->fdt, nodename, "interrupt-controller", NULL, 0);
    qemu_fdt_setprop_cell(ms->fdt, nodename, "#interrupt-cells", 0x2);
    qemu_fdt_setprop_cell(ms->fdt, nodename, "#address-cells", 0x2);
    qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg",
                                 2, base, 2, 0x1000);

    ph = qemu_fdt_alloc_phandle(ms->fdt);
    qemu_fdt_setprop_cell(ms->fdt, nodename, "phandle", ph);
    qemu_fdt_setprop_cell(ms->fdt, nodename, "linux,phandle", ph);
    x86ms->ioapic_phandle[index] = ph;

    g_free(nodename);
}

/*
 * The I/O APICs come first so that machines adding device nodes afterwards can
 * point "interrupt-parent" at them.
 */
static void x86_fdt_add_ioapics(X86MachineState *x86ms)
{
    BusState *bus = sysbus_get_default();
    BusChild *kid;

    QTAILQ_FOREACH(kid, &bus->children, sibling) {
        Object *dev = OBJECT(kid->child);
        /*
         * The guest sees an I/O APIC whether QEMU or KVM emulates it, but the
         * two are distinct QOM types.
         */
        Object *obj = object_dynamic_cast(dev, TYPE_IOAPIC) ?:
                      object_dynamic_cast(dev, TYPE_KVM_IOAPIC);

        if (obj) {
            x86_fdt_add_ioapic(x86ms, SYS_BUS_DEVICE(obj));
        }
    }
}

void x86_fdt_setup(X86MachineState *x86ms, const char *compatible, int *sizep)
{
    MachineState *ms = MACHINE(x86ms);

    ms->fdt = create_device_tree(sizep);

    /*
     * A root "compatible" is mandatory rather than informative: Linux treats a
     * tree without one as the empty built-in tree (of_have_populated_dt()) and
     * ignores everything in it.
     */
    qemu_fdt_setprop_string(ms->fdt, "/", "compatible", compatible);
    qemu_fdt_setprop_cell(ms->fdt, "/", "#address-cells", 0x2);
    qemu_fdt_setprop_cell(ms->fdt, "/", "#size-cells", 0x2);

    qemu_fdt_add_subnode(ms->fdt, "/chosen");

    x86_fdt_add_cpus(x86ms);
    x86_fdt_add_lapic(x86ms);
    x86_fdt_add_ioapics(x86ms);
}
