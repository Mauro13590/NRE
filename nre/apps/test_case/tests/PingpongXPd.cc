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

#include <ipc/Service.h>
#include <ipc/ClientSession.h>
#include <subsystem/ChildManager.h>
#include <stream/IStringStream.h>
#include <stream/OStringStream.h>
#include <kobj/Pt.h>
#include <utcb/UtcbFrame.h>
#include <util/Profiler.h>
#include <CPU.h>

#include "PingpongXPd.h"

using namespace nre;
using namespace nre::test;

class PingpongService;
static void test_pingpong();

const TestCase pingpongxpd = {
    "Cross-Pd PingPong", test_pingpong
};

typedef void (*client_func)(AvgProfiler &prof, Pt &pt, UtcbFrame &uf, uint &sum);

static const uint tries = 10000;
static PingpongService *srv;

class PingpongSession : public ServiceSession {
public:
    explicit PingpongSession(Service *s, size_t id, portal_func func)
        : ServiceSession(s, id, func) {
    }
    virtual ~PingpongSession();
};

class PingpongService : public Service {
public:
    explicit PingpongService(portal_func func) : Service("pingpong", CPUSet(CPUSet::ALL), func) {
    }

private:
    virtual ServiceSession *create_session(size_t id, const String&, portal_func func) {
        return new PingpongSession(this, id, func);
    }
};

PingpongSession::~PingpongSession() {
    srv->stop();
}

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

static int pingpong_server(int, char *argv[]) {
    uintptr_t addr = IStringStream::read_from<uintptr_t>(argv[1]);
    srv = new PingpongService(reinterpret_cast<Service::portal_func>(addr));
    srv->start();
    delete srv;
    return 0;
}

static void client_empty(AvgProfiler &prof, Pt &pt, UtcbFrame &uf, uint &sum) {
    prof.start();
    pt.call(uf);
    prof.stop();
    sum += 1 + 2 + 1 + 2 + 3;
}

static void client_data(AvgProfiler &prof, Pt &pt, UtcbFrame &uf, uint &sum) {
    uint x = 0, y = 0;
    prof.start();
    uf << 1 << 2 << 3;
    pt.call(uf);
    uf >> x >> y;
    uf.clear();
    prof.stop();
    sum += x + y;
}

static int pingpong_client(int, char *argv[]) {
    ClientSession sess("pingpong");
    Pt pt(sess.caps() + CPU::current().log_id());
    uintptr_t addr = IStringStream::read_from<uintptr_t>(argv[1]);
    client_func func = reinterpret_cast<client_func>(addr);
    uint sum = 0;
    AvgProfiler prof(tries);
    UtcbFrame uf;
    for(uint i = 0; i < tries; i++)
        func(prof, pt, uf, sum);

    WVPERF(prof.avg(), "cycles");
    WVPASSEQ(sum, (1 + 2) * tries + (1 + 2 + 3) * tries);
    WVPRINT("sum: " << sum);
    WVPRINT("min: " << prof.min());
    WVPRINT("max: " << prof.max());
    return 0;
}

static void test_pingpong() {
    ChildManager *mng = new ChildManager();
    Hip::mem_iterator self = Hip::get().mem_begin();
    Service::portal_func funcs[] = {portal_empty, portal_data};
    client_func clientfuncs[] = {client_empty, client_data};
    const char *names[] = {"empty", "data"};
    for(size_t i = 0; i < ARRAY_SIZE(funcs); ++i) {
        WVPRINT("Using the " << names[i] << " portal:");
        // map the memory of the module
        DataSpace ds(self->size, DataSpaceDesc::ANONYMOUS, DataSpaceDesc::R, self->addr);
        {
            char cmdline[64];
            OStringStream os(cmdline, sizeof(cmdline));
            os << "pingpongservice provides=pingpong " << reinterpret_cast<uintptr_t>(funcs[i]);
            ChildConfig cfg(0, cmdline);
            cfg.entry(reinterpret_cast<uintptr_t>(pingpong_server));
            mng->load(ds.virt(), self->size, cfg);
        }
        {
            char cmdline[64];
            OStringStream os(cmdline, sizeof(cmdline));
            os << "pingpongclient " << reinterpret_cast<uintptr_t>(clientfuncs[i]);
            ChildConfig cfg(0, cmdline);
            cfg.entry(reinterpret_cast<uintptr_t>(pingpong_client));
            mng->load(ds.virt(), self->size, cfg);
        }
        while(mng->count() > 0)
            mng->dead_sm().down();
    }
    delete mng;
}
