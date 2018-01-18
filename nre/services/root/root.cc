/*
 * Copyright (C) 2012, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NRE (NOVA runtime environment).
 *
 * NRE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NRE is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <kobj/LocalThread.h>
#include <kobj/Pt.h>
#include <utcb/UtcbFrame.h>
#include <subsystem/ChildManager.h>
#include <subsystem/ChildHip.h>
#include <ipc/Service.h>
#include <collection/Cycler.h>
#include <util/Math.h>
#include <util/Bytes.h>
#include <String.h>
#include <Hip.h>
#include <CPU.h>
#include <Exception.h>
#include <Logging.h>
#include <cstring>
#include <new>

#include "PhysicalMemory.h"
#include "VirtualMemory.h"
#include "Hypervisor.h"
#include "Admission.h"
#include "SysInfoService.h"
#include "Log.h"

using namespace nre;

static const size_t MAX_CMDLINES_LEN    = ExecEnv::PAGE_SIZE;

class CPU0Init {
    CPU0Init();
    static CPU0Init init;
};

EXTERN_C void dlmalloc_init();
static void log_thread(void*);
static void sysinfo_thread(void*);
PORTAL static void portal_service(void*);
PORTAL static void portal_pagefault(void*);
PORTAL static void portal_startup(void*);
static void start_childs();

CPU0Init CPU0Init::init INIT_PRIO_CPU0;

// stack for initial Thread (overwrites the weak-symbol defined in Startup.cc)
uchar _stack[ExecEnv::STACK_SIZE] ALIGNED(ARCH_STACK_SIZE);
// stacks for LocalThreads of first CPU
static uchar dsstack[ExecEnv::STACK_SIZE] ALIGNED(ARCH_STACK_SIZE);
static uchar ptstack[ExecEnv::STACK_SIZE] ALIGNED(ARCH_STACK_SIZE);
static uchar regptstack[ExecEnv::STACK_SIZE] ALIGNED(ARCH_STACK_SIZE);
static uchar nhip[ExecEnv::PAGE_SIZE] ALIGNED(ARCH_PAGE_SIZE);
static ChildManager *mng;

CPU0Init::CPU0Init() {
    // just init the current CPU to prevent that the startup-heap-size depends on the number of CPUs
    CPU &cpu = CPU::current();
    uintptr_t ds_utcb = VirtualMemory::alloc(Utcb::SIZE);
    // use the local stack here since we can't map dataspaces yet
    // note that we need a different thread here because otherwise we couldn't allocate any memory
    // on the heap in all portals that use the same thread (we would call us ourself).
    Reference<LocalThread> dsec = LocalThread::create(cpu.log_id(), ObjCap::INVALID,
                                            reinterpret_cast<uintptr_t>(dsstack), ds_utcb);
    cpu.ds_pt(new Pt(dsec, PhysicalMemory::portal_dataspace));
    // accept translated caps
    UtcbFrameRef dsuf(dsec->utcb());
    dsuf.accept_translates();

    uintptr_t ec_utcb = VirtualMemory::alloc(Utcb::SIZE);
    Reference<LocalThread> ec = LocalThread::create(cpu.log_id(), ObjCap::INVALID,
                                          reinterpret_cast<uintptr_t>(ptstack), ec_utcb);
    cpu.gsi_pt(new Pt(ec, Hypervisor::portal_gsi));
    cpu.io_pt(new Pt(ec, Hypervisor::portal_io));
    cpu.sc_pt(new Pt(ec, Admission::portal_sc));
    new Pt(ec, ec->event_base() + CapSelSpace::EV_STARTUP, portal_startup, Mtd(Mtd::RSP));
    new Pt(ec, ec->event_base() + CapSelSpace::EV_PAGEFAULT, portal_pagefault,
           Mtd(Mtd::RSP | Mtd::GPR_BSD | Mtd::RIP_LEN | Mtd::QUAL));
    // accept delegates and translates for portal_sc
    UtcbFrameRef defuf(ec->utcb());
    defuf.accept_delegates(0);
    defuf.accept_translates();

    // register portal for the log service
    uintptr_t regec_utcb = VirtualMemory::alloc(Utcb::SIZE);
    Reference<LocalThread> regec = LocalThread::create(cpu.log_id(), ObjCap::INVALID,
                                             reinterpret_cast<uintptr_t>(regptstack), regec_utcb);
    cpu.srv_pt(new Pt(regec, portal_service));
    UtcbFrameRef reguf(regec->utcb());
    reguf.accept_translates();
}

static void adjust_memory_map() {
    const Hip &hip = Hip::get();
    ChildHip *chip = new (nhip) ChildHip();
    for(auto it = hip.mem_begin(); it != hip.mem_end(); ++it) {
        if(it->type == HipMem::AVAILABLE) {
            // if available memory isn't page-aligned, cut it off to ensure that we don't
            // hit reserved memory. this solves problems that arise when the memory map contains
            // multiple adjacent not-page-aligned areas. we only care about page-overlaps between
            // reserved and not-reserved areas. because this is the only check that is made
            // (i.e. whether requested device memory overlaps with available memory). therefore
            // it is sufficient to make sure that the available page frames don't overlap with
            // the reserved page frames.
            uint64_t addr = it->addr;
            uint64_t size = it->size;
            if(addr & (ExecEnv::PAGE_SIZE - 1)) {
                size -= ExecEnv::PAGE_SIZE - (addr & (ExecEnv::PAGE_SIZE - 1));
                addr = Math::round_up<uint64_t>(addr, ExecEnv::PAGE_SIZE);
                size -= ExecEnv::PAGE_SIZE - (addr & (ExecEnv::PAGE_SIZE - 1));
            }
            size = Math::round_dn<uint64_t>(size, ExecEnv::PAGE_SIZE);
            if(size > 0)
                chip->add_mem(addr, size, it->aux, it->type);
        }
        else if(it->size > 0)
            chip->add_mem(it->addr, it->size, it->aux, it->type);
    }
    chip->finalize();

    // exchange hip
    _startup_info.hip = chip;
}

int main() {
    adjust_memory_map();
    const Hip &hip = Hip::get();

    LOG(PLATFORM, "Welcome to NRE rev " << NRE_REV << " built with " << COMPILER_NAME << "\n");
    LOG(PLATFORM, "Hip checksum is " << (hip.is_valid() ? "valid" : "not valid") << "\n");
    LOG(PLATFORM, "SEL: " << hip.cfg_cap << ", EXC: " << hip.cfg_exc
                          << ", VMI: " << hip.cfg_vm << ", GSI: " << hip.cfg_gsi << "\n");
    LOG(PLATFORM, "CPU runs @ " << (Hip::get().freq_tsc / 1000) << " Mhz, bus @ "
                                << (Hip::get().freq_bus / 1000) << " Mhz\n");
    // add all available memory
    for(auto it = hip.mem_begin(); it != hip.mem_end(); ++it) {
        if(it->type == HipMem::AVAILABLE) {
            // skip it if uintptr_t can't handle it
            if(it->addr + it->size > static_cast<uintptr_t>(-1))
                continue;

            PhysicalMemory::add(it->addr, it->size);
        }
    }

    // remove all not available memory
    for(auto it = hip.mem_begin(); it != hip.mem_end(); ++it) {
        // also remove the BIOS-area (make it available as device-memory)
        if(it->type != HipMem::AVAILABLE || it->addr == 0)
            PhysicalMemory::remove(it->addr, Math::round_up<size_t>(it->size, ExecEnv::PAGE_SIZE));
        // make sure that we don't overwrite the next two pages behind the cmdline
        if(it->type == HipMem::MB_MODULE && it->aux)
            PhysicalMemory::remove(it->aux & ~(ExecEnv::PAGE_SIZE - 1), ExecEnv::PAGE_SIZE * 2);
    }

    // now allocate the available memory from the hypervisor
    PhysicalMemory::map_all();

    LOG(MEM_MAP, "Virtual memory for mappings:\n");
    const RegionManager<> &vmregs = VirtualMemory::regions();
    for(auto it = vmregs.begin(); it != vmregs.end(); ++it)
        LOG(MEM_MAP, "\t" << fmt(it->addr, "p") << " .. " << fmt(it->addr + it->size - 1, "p")
                          << " (" << Bytes(it->size) << ")\n");
    LOG(MEM_MAP, "Virtual memory for RAM:\n\t"
                 << fmt(VirtualMemory::ram_begin(), "p") << " .. "
                 << fmt(VirtualMemory::ram_end() - 1, "p")
                 << " (" << Bytes(VirtualMemory::ram_end() - VirtualMemory::ram_begin()) << ")\n");
    LOG(MEM_MAP, "Physical memory:\n" << PhysicalMemory::regions());

    LOG(CPUS, "CPUs:\n");
    for(auto it = CPU::begin(); it != CPU::end(); ++it) {
        LOG(CPUS, "\tpackage=" << it->package() << ", core=" << it->core()
                               << ", thread=" << it->thread() << ", flags=" << it->flags() << "\n");
    }

    // now we can use dlmalloc (map-pt created and available memory added to pool)
    dlmalloc_init();

    // create memory mapping portals for the other CPUs
    Hypervisor::init();
    Admission::init();

    // now init the stuff for all other CPUs (using dlmalloc)
    for(auto it = CPU::begin(); it != CPU::end(); ++it) {
        if(it->log_id() != CPU::current().log_id()) {
            // again, different thread for ds portal
            Reference<LocalThread> dsec = LocalThread::create(it->log_id());
            it->ds_pt(new Pt(dsec, PhysicalMemory::portal_dataspace));
            // accept translated caps
            UtcbFrameRef dsuf(dsec->utcb());
            dsuf.accept_translates();

            Reference<LocalThread> ec = LocalThread::create(it->log_id());
            it->gsi_pt(new Pt(ec, Hypervisor::portal_gsi));
            it->io_pt(new Pt(ec, Hypervisor::portal_io));
            it->sc_pt(new Pt(ec, Admission::portal_sc));
            // accept delegates and translates for portal_sc
            UtcbFrameRef defuf(ec->utcb());
            defuf.accept_delegates(0);
            defuf.accept_translates();
            new Pt(ec, ec->event_base() + CapSelSpace::EV_STARTUP, portal_startup, Mtd(Mtd::RSP));
            new Pt(ec, ec->event_base() + CapSelSpace::EV_PAGEFAULT, portal_pagefault,
                   Mtd(Mtd::RSP | Mtd::GPR_BSD | Mtd::RIP_LEN | Mtd::QUAL));

            // register portal for the log service
            Reference<LocalThread> regec = LocalThread::create(it->log_id());
            it->srv_pt(new Pt(regec, portal_service));
            UtcbFrameRef reguf(regec->utcb());
            reguf.accept_translates();
        }
    }

    // change the Hip to allow us direct access to the mb-module-cmdlines
    char *cmdlines = new char[MAX_CMDLINES_LEN];
    char *curcmd = cmdlines;
    for(auto it = hip.mem_begin(); it != hip.mem_end(); ++it) {
        if(it->type == HipMem::MB_MODULE) {
            char *cmdline = Hypervisor::map_string(it->aux);
            size_t len = strlen(cmdline) + 1;
            if(curcmd + len > cmdlines + MAX_CMDLINES_LEN)
                Util::panic("Not enough memory for cmdlines");
            memcpy(curcmd, cmdline, len);
            HipMem *mem = const_cast<HipMem*>(&*it);
            mem->aux = reinterpret_cast<word_t>(curcmd);
            Hypervisor::unmap_string(cmdline);
            curcmd += len;
        }
    }

    LOG(MEM_MAP, "Memory map:\n");
    for(auto it = hip.mem_begin(); it != hip.mem_end(); ++it) {
        LOG(MEM_MAP, "\t" << "addr=" << fmt(it->addr, "p")
                          << " size=" << fmt(it->size, "#0x", 10)
                          << " type=" << fmt(it->type, "+")
                          << " aux=" << fmt(it->aux, "p"));
        if(it->aux)
            LOG(MEM_MAP, " (" << it->cmdline() << ")");
        LOG(MEM_MAP, '\n');
    }

    mng = new ChildManager();
    GlobalThread::create(log_thread, CPU::current().log_id(), "root-log")->start();
    GlobalThread::create(sysinfo_thread, CPU::current().log_id(), "root-sysinfo")->start();

    // wait until log and sysinfo are registered
    while(mng->registry().find("log") == nullptr || mng->registry().find("sysinfo") == nullptr)
        Util::pause();

    start_childs();
 
    Sm sm(0);
    sm.down();
    return 0;
}

static void log_thread(void*) {
    Log::get().start();
}

static void sysinfo_thread(void*) {
    SysInfoService *sysinfo = new SysInfoService(mng);
    sysinfo->start();
}

static void start_childs() {
    size_t mod = 0, i = 0;
    ForwardCycler<CPU::iterator> cpus(CPU::begin(), CPU::end());
    const Hip &hip = Hip::get();
    for(auto it = hip.mem_begin(); it != hip.mem_end(); ++it, ++mod) {
        // we are the first one :)
        if(it->type == HipMem::MB_MODULE && i++ >= 1) {
            // map the memory of the module
            uintptr_t virt = VirtualMemory::alloc(it->size);
            Hypervisor::map_mem(it->addr, virt, it->size);

            ChildConfig cfg(mod, it->cmdline(), cpus.next()->log_id());
            mng->load(virt, it->size, cfg);
            if(cfg.last())
                break;
        }
    }
}

static void portal_service(void*) {
    UtcbFrameRef uf;
    try {
        Service::Command cmd;
        uf >> cmd;
        switch(cmd) {
            case Service::REGISTER: {
                String name;
                BitField<Hip::MAX_CPUS> available;
                capsel_t cap;
                uf >> name >> available >> cap;
                uf.finish_input();

                capsel_t sm = mng->reg_service(cap, name, available);
                uf.delegate(sm);
                uf << E_SUCCESS;
            }
            break;

            case Service::OPEN_SESSION: {
                String name, args;
                uf >> name >> args;
                uf.finish_input();
                VTHROW(Exception, E_NOT_FOUND,
                       "Unable to find service '" << name << "' (args=" << args << ")");
            }
            break;

            case Service::CLOSE_SESSION:
            case Service::UNREGISTER:
                uf.clear();
                uf << E_NOT_FOUND;
                break;
        }
    }
    catch(const Exception& e) {
        uf.clear();
        uf << e;
    }
}

static void portal_pagefault(void*) {
    UtcbExcFrameRef uf;
    uintptr_t addrs[32];
    uintptr_t pfaddr = uf->qual[1];
    unsigned error = uf->qual[0];
    uintptr_t eip = uf->rip;

    if(eip != ExecEnv::THREAD_EXIT) {
        Serial::get() << "Root: Pagefault for " << fmt(pfaddr, "p") << " @ " << fmt(eip, "p")
                      << " on cpu " << CPU::current().phys_id() << ", error=" << fmt(error, "#x") << "\n";
        ExecEnv::collect_backtrace(uf->rsp & ~(ExecEnv::PAGE_SIZE - 1), uf->rbp, addrs, 32);
        Util::write_backtrace(Serial::get(), addrs);
    }

    // let the kernel kill us
    uf->rip = ExecEnv::KERNEL_START;
    uf->mtd = Mtd::RIP_LEN;
}

static void portal_startup(void*) {
    UtcbExcFrameRef uf;
    uf->mtd = Mtd::RIP_LEN;
    uf->rip = *reinterpret_cast<word_t*>(uf->rsp + sizeof(word_t));
}