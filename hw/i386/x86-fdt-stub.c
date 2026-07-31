/*
 * Device tree generation for x86 machines, for builds without libfdt.
 *
 * Machines call x86_fdt_setup() unconditionally; without device tree support
 * MachineState::fdt simply stays NULL, and the things that consume it -- the
 * dumpdtb machine property and the IGVM device tree parameter -- report that
 * no tree is available.
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
#include "hw/i386/x86.h"

void x86_fdt_setup(X86MachineState *x86ms, const char *compatible, int *sizep)
{
    *sizep = 0;
}
