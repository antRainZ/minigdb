#ifndef FAULT_INJECT_ELF_COMMON_H
#define FAULT_INJECT_ELF_COMMON_H

#include "../hex.hpp"
#include "data.hpp"
#include <string>
#include <type_traits>

namespace elf
{

std::string to_string(elfclass v)
{
    switch (v) {
    case elfclass::_32:
        return "32";
    case elfclass::_64:
        return "64";
    }
    return "(elfclass)0x" + to_hex((int)v);
}

std::string to_string(elfdata v)
{
    switch (v) {
    case elfdata::lsb:
        return "lsb";
    case elfdata::msb:
        return "msb";
    }
    return "(elfdata)0x" + to_hex((int)v);
}

std::string to_string(elfosabi v)
{
    switch (v) {
    case elfosabi::sysv:
        return "sysv";
    case elfosabi::hpux:
        return "hpux";
    case elfosabi::standalone:
        return "standalone";
    }
    return "(elfosabi)0x" + to_hex((int)v);
}

std::string to_string(et v)
{
    switch (v) {
    case et::none:
        return "none";
    case et::rel:
        return "rel";
    case et::exec:
        return "exec";
    case et::dyn:
        return "dyn";
    case et::core:
        return "core";
    case et::loos:
        break;
    case et::hios:
        break;
    case et::loproc:
        break;
    case et::hiproc:
        break;
    }
    return "(et)0x" + to_hex((int)v);
}

std::string to_string(sht v)
{
    switch (v) {
    case sht::null:
        return "null";
    case sht::progbits:
        return "progbits";
    case sht::symtab:
        return "symtab";
    case sht::strtab:
        return "strtab";
    case sht::rela:
        return "rela";
    case sht::hash:
        return "hash";
    case sht::dynamic:
        return "dynamic";
    case sht::note:
        return "note";
    case sht::nobits:
        return "nobits";
    case sht::rel:
        return "rel";
    case sht::shlib:
        return "shlib";
    case sht::dynsym:
        return "dynsym";
    case sht::loos:
        break;
    case sht::hios:
        break;
    case sht::loproc:
        break;
    case sht::hiproc:
        break;
    }
    return "(sht)0x" + to_hex((int)v);
}

std::string to_string(shf v)
{
    std::string res;
    if ((v & shf::write) == shf::write) {
        res += "write|";
        v &= ~shf::write;
    }
    if ((v & shf::alloc) == shf::alloc) {
        res += "alloc|";
        v &= ~shf::alloc;
    }
    if ((v & shf::execinstr) == shf::execinstr) {
        res += "execinstr|";
        v &= ~shf::execinstr;
    }
    if ((v & shf::maskos) == shf::maskos) {
        res += "maskos|";
        v &= ~shf::maskos;
    }
    if ((v & shf::maskproc) == shf::maskproc) {
        res += "maskproc|";
        v &= ~shf::maskproc;
    }
    if (res.empty() || v != (shf)0)
        res += "(shf)0x" + to_hex((int)v);
    else
        res.pop_back();
    return res;
}

std::string to_string(pt v)
{
    switch (v) {
    case pt::null:
        return "null";
    case pt::load:
        return "load";
    case pt::dynamic:
        return "dynamic";
    case pt::interp:
        return "interp";
    case pt::note:
        return "note";
    case pt::shlib:
        return "shlib";
    case pt::phdr:
        return "phdr";
    case pt::loos:
        break;
    case pt::hios:
        break;
    case pt::loproc:
        break;
    case pt::hiproc:
        break;
    }
    return "(pt)0x" + to_hex((int)v);
}

std::string to_string(pf v)
{
    std::string res;
    if ((v & pf::x) == pf::x) {
        res += "x|";
        v &= ~pf::x;
    }
    if ((v & pf::w) == pf::w) {
        res += "w|";
        v &= ~pf::w;
    }
    if ((v & pf::r) == pf::r) {
        res += "r|";
        v &= ~pf::r;
    }
    if ((v & pf::maskos) == pf::maskos) {
        res += "maskos|";
        v &= ~pf::maskos;
    }
    if ((v & pf::maskproc) == pf::maskproc) {
        res += "maskproc|";
        v &= ~pf::maskproc;
    }
    if (res.empty() || v != (pf)0)
        res += "(pf)0x" + to_hex((int)v);
    else
        res.pop_back();
    return res;
}

std::string to_string(stb v)
{
    switch (v) {
    case stb::local:
        return "local";
    case stb::global:
        return "global";
    case stb::weak:
        return "weak";
    case stb::loos:
        break;
    case stb::hios:
        break;
    case stb::loproc:
        break;
    case stb::hiproc:
        break;
    }
    return "(stb)0x" + to_hex((int)v);
}

std::string to_string(stt v)
{
    switch (v) {
    case stt::notype:
        return "notype";
    case stt::object:
        return "object";
    case stt::func:
        return "func";
    case stt::section:
        return "section";
    case stt::file:
        return "file";
    case stt::loos:
        break;
    case stt::hios:
        break;
    case stt::loproc:
        break;
    case stt::hiproc:
        break;
    }
    return "(stt)0x" + to_hex((int)v);
}
} // namespace elf

#ifndef