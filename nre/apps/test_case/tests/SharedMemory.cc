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
#include <subsystem/ChildManager.h>
#include <ipc/Service.h>
#include <ipc/Consumer.h>
#include <ipc/Producer.h>
#include <ipc/ClientSession.h>
#include <stream/IStringStream.h>
#include <Hip.h>
#include <CPU.h>

#include "SharedMemory.h"

using namespace nre;
using namespace nre::test;

static const size_t ITEM_COUNT  = 1000;
static const size_t WORD_COUNT  = 1;

struct Item {
    word_t words[WORD_COUNT];
};
class ShmService;

static void test_shm();

const TestCase sharedmem = {
    "Shared memory", test_shm
};
static ShmService *srv;

class ShmSession : public ServiceSession {
public:
    explicit ShmSession(Service *s, size_t id, portal_func func)
        : ServiceSession(s, id, func),
          _ec(GlobalThread::create(receiver, CPU::current().log_id(), "shm-receiver")),
          _cons(), _ds(), _sm(0) {
        _ec->set_tls<size_t>(Thread::TLS_PARAM, id);
    }
    virtual ~ShmSession() {
        if(_cons)
            _cons->stop();
        // don't delete the resources before the thread is dead
        _ec->join();
        delete _ds;
        delete _cons;
    }

    Sm &sm() {
        return _sm;
    }
    Consumer<Item> *cons() {
        return _cons;
    }

    void set_ds(DataSpace *ds) {
        _ds = ds;
        _cons = new Consumer<Item>(*ds, _sm, true);
        _ec->start();
    }

private:
    static void receiver(void *);

    Reference<GlobalThread> _ec;
    Consumer<Item> *_cons;
    DataSpace *_ds;
    Sm _sm;
};

class ShmService : public Service {
public:
    explicit ShmService() : Service("shm", CPUSet(CPUSet::ALL), reinterpret_cast<portal_func>(portal)) {
        UtcbFrameRef uf(get_thread(CPU::current().log_id())->utcb());
        uf.accept_delegates(0);
    }

private:
    PORTAL static void portal(ShmSession *sess);

    virtual ServiceSession *create_session(size_t id, const String &, portal_func func) {
        return new ShmSession(this, id, func);
    }
};

void ShmService::portal(ShmSession *sess) {
    UtcbFrameRef uf;
    try {
        capsel_t dssel = uf.get_delegated(0).offset();
        uf.finish_input();
        sess->set_ds(new DataSpace(dssel));
        uf.delegate(sess->sm().sel());
        uf << E_SUCCESS;
    }
    catch(const Exception &e) {
        uf.clear();
        uf << e;
    }
}

void ShmSession::receiver(void*) {
    size_t id = Thread::current()->get_tls<word_t>(Thread::TLS_PARAM);
    Consumer<Item> *cons = srv->get_session<ShmSession>(id)->cons();
    size_t count = 0;
    while(1) {
        Item *w = cons->get();
        if(w == 0)
            break;
        count++;
        cons->next();
    }
    WVPASSEQ(count, static_cast<size_t>(ITEM_COUNT));
    srv->stop();
}

static int shm_service(int argc, char *argv[]) {
    for(int i = 0; i < argc; ++i)
        Serial::get() << "arg " << i << ": " << argv[i] << "\n";

    srv = new ShmService();
    srv->start();
    delete srv;
    return 0;
}

static int shm_client(int, char *argv[]) {
    size_t ds_size = IStringStream::read_from<size_t>(argv[1]);
    ClientSession sess("shm");
    DataSpace ds(ds_size, DataSpaceDesc::ANONYMOUS, DataSpaceDesc::RW);
    capsel_t sm_cap = ObjCap::INVALID;
    {
        UtcbFrame uf;
        uf.accept_delegates(0, Crd::OBJ_ALL);
        uf.delegate(ds.sel());
        Pt pt(sess.caps() + CPU::current().log_id());
        pt.call(uf);
        uf.check_reply();
        sm_cap = uf.get_delegated(0).offset();
    }

    Sm sm(sm_cap, true);
    Producer<Item> prod(ds, sm);
    Item i;
    uint64_t start = Util::tsc();
    for(uint x = 0; x < ITEM_COUNT; ) {
        if(prod.produce(i))
            x++;
    }
    uint64_t end = Util::tsc();
    uint64_t total = end - start;
    uint64_t avg = total / ITEM_COUNT;
    WVPRINT("Transfered " << ITEM_COUNT << " items");
    WVPERF(total, "Cycles");
    WVPERF(avg, "Cycles");
    return 0;
}

static void test_shm() {
    char cmdline[64];
    Hip::mem_iterator self = Hip::get().mem_begin();
    size_t ds_sizes[] = {ExecEnv::PAGE_SIZE, ExecEnv::PAGE_SIZE * 2, ExecEnv::PAGE_SIZE * 4};
    for(size_t i = 0; i < ARRAY_SIZE(ds_sizes); ++i) {
        ChildManager *mng = new ChildManager();
        // map the memory of the module
        DataSpace *ds = new DataSpace(self->size, DataSpaceDesc::ANONYMOUS, DataSpaceDesc::R, self->addr);
        {
            ChildConfig cfg(0, "shm_service provides=shm");
            cfg.entry(reinterpret_cast<uintptr_t>(shm_service));
            mng->load(ds->virt(), self->size, cfg);
        }
        {
            OStringStream os(cmdline, sizeof(cmdline));
            os << "shm_client " << ds_sizes[i];
            ChildConfig cfg(0, cmdline);
            cfg.entry(reinterpret_cast<uintptr_t>(shm_client));
            mng->load(ds->virt(), self->size, cfg);
        }
        while(mng->count() > 0)
            mng->dead_sm().down();
        delete ds;
        delete mng;
    }
}
