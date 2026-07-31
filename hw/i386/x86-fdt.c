/*
 * Device tree generation for x86 machines
 *
 * This builds the part of the tree that every x86 machine can describe: the
 * interrupt controllers.  Machines with more to say add their own nodes on
 * top of it -- see microvm-dt.c for the virtio-mmio transports, the PCIe ECAM
 * and the ISA devices microvm exposes.
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
        Object *obj = object_dynamic_cast(OBJECT(kid->child), TYPE_IOAPIC);

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

    x86_fdt_add_ioapics(x86ms);
}
