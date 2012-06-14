/*
 * TODO comment me
 *
 * Copyright (C) 2012, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL.
 *
 * NUL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#include <kobj/Pt.h>
#include <subsystem/ServiceRegistry.h>
#include <mem/DataSpaceManager.h>
#include <Exception.h>

namespace nul {

class ElfException : public Exception {
public:
	ElfException(ErrorCode code) : Exception(code) {
	}
};

class ChildException : public Exception {
public:
	ChildException(ErrorCode code) : Exception(code) {
	}
};

class ChildManager {
	class Portals;
	friend class Portals;

	class Portals {
	public:
		enum {
			COUNT	= 10
		};

		PORTAL static void startup(capsel_t pid);
		PORTAL static void init_caps(capsel_t pid);
		PORTAL static void reg(capsel_t pid);
		PORTAL static void unreg(capsel_t pid);
		PORTAL static void get_service(capsel_t pid);
		PORTAL static void io(capsel_t);
		PORTAL static void gsi(capsel_t);
		PORTAL static void map(capsel_t pid);
		PORTAL static void unmap(capsel_t pid);
		PORTAL static void pf(capsel_t pid);
	};

	// TODO we need a data structure that allows an arbitrary number of childs or whatsoever
	enum {
		MAX_CHILDS		= 32
	};

public:
	ChildManager();
	~ChildManager();

	void load(uintptr_t addr,size_t size,const char *cmdline);
	ServiceRegistry &registry() {
		return _registry;
	}

private:
	cpu_t get_cpu(capsel_t pid) const {
		size_t off = (pid - _portal_caps) % per_child_caps();
		return off / Hip::get().service_caps();
	}
	Child *get_child(capsel_t pid) const {
		Child *c = _childs[((pid - _portal_caps) / per_child_caps())];
		if(!c)
			throw ChildException(E_NOT_FOUND);
		return c;
	}
	void destroy_child(capsel_t pid) {
		size_t i = (pid - _portal_caps) / per_child_caps();
		Child *c = _childs[i];
		c->decrease_refs();
		if(c->refs() == 0) {
			// note that we're safe here because we only get here if there is only one Ec left and
			// this one has just caused a fault. thus, there can't be somebody else using this
			// client instance
			_childs[i] = 0;
			_registry.remove(c);
			delete c;
		}
	}

	static inline size_t per_child_caps() {
		return Util::nextpow2(Hip::get().service_caps() * _cpu_count);
	}

	ChildManager(const ChildManager&);
	ChildManager& operator=(const ChildManager&);

	size_t _child;
	Child *_childs[MAX_CHILDS];
	capsel_t _portal_caps;
	DataSpaceManager _dsm;
	ServiceRegistry _registry;
	UserSm _sm;
	Sm _regsm;
	// we need different Ecs to be able to receive a different number of caps
	LocalEc *_ecs[Hip::MAX_CPUS];
	LocalEc *_regecs[Hip::MAX_CPUS];
	LocalEc *_mapecs[Hip::MAX_CPUS];
	static size_t _cpu_count;
};

}
