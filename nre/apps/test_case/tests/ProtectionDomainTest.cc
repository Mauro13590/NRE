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

#include <kobj/Pt.h>
#include <utcb/UtcbFrame.h>
#include <util/Profiler.h>
#include <CPU.h>

#include "ProtectionDomainTest.h"

using namespace nre;
using namespace nre::test;

static void test_PD();

const TestCase ProtectionDomain = {
    "ProtectionDomain_Test", test_PD
};

static const uint value = 10000;

PORTAL static void portal_empty(void*) {
}

PORTAL static void portal_data(void*) {
    UtcbFrameRef uf;
    try {
        uint a, b, c;
        uf >> a >> b >> c;
        uf.clear();
        uf << (a + b) << (a + b + c);
    }
    catch(const Exception&) {
        uf.clear();
    }
}

static void print_result(AvgProfiler &prof, uint sum) {
    WVPERF(prof.avg(), "cycles");
    WVPASSEQ(sum, (1 + 2) * value + (1 + 2 + 3) * value);
    WVPRINT("value: "<< value);
    WVPRINT("sum: " << sum);
    //WVPRINT("min: " << prof.min());
    //WVPRINT("max: " << prof.max());
}

static void test_PD() {
    Reference<LocalThread> ec = LocalThread::create(CPU::current().log_id());

    /*{
        Pt pt(ec, portal_empty);
        AvgProfiler prof(tries);
        UtcbFrame uf;
        for(uint i = 0; i < tries; i++) {
            prof.start();
            pt.call(uf);
            prof.stop();
        }
        WVPRINT("Using portal_empty:");
        print_result(prof, (1 + 2) * tries + (1 + 2 + 3) * tries);
    }*/

    {
        Pt pt(ec, portal_data);
        AvgProfiler prof(value);
        uint sum = 0;
        UtcbFrame uf;
        for(uint i = 0; i < value; i++) {
            prof.start();
            uf << 1 << 2 << 3;
            pt.call(uf);
            uint x = 0, y = 0;
            uf >> x >> y;
            uf.clear();
            sum += x + y;
            prof.stop();
        }
        WVPRINT("Using portal_data:");
        print_result(prof, sum);
    }
}
