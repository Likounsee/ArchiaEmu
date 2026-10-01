#include "Memory.hpp"

#include <cstring>
#include <algorithm>
#include <limits>
#include <utility>

namespace myps5emu {

namespace {

bool RangeValid(std::uint64_t address, std::size_t size) noexcept
{
    if (size == 0) {
        return false;
    }

    const auto size64 = static_cast<std::uint64_t>(size);
    return size64 <= std::numeric_limits<std::uint64_t>::max() - address;
}

} // namespace

bool Memory::Map(std::uint64_t virtual_address, std::size_t size)
{
    return Map(virtual_address, size,
               MemoryPermission::Read | MemoryPermission::Write |
               MemoryPermission::Execute);
}

bool Memory::Map(std::uint64_t virtual_address,
                 std::size_t size,
                 MemoryPermission permissions)
{
    if (!RangeValid(virtual_address, size)) {
        SetFault(MemoryFault::InvalidRange);
        return false;
    }

    // Mappings are page-based: callers must describe complete pages.
    if ((virtual_address % PageSize) != 0 || (size % PageSize) != 0) {
        SetFault(MemoryFault::Unaligned);
        return false;
    }

    if (HasOverlappingRegion(virtual_address, size)) {
        SetFault(MemoryFault::Overlap);
        return false;
    }

    Region region;
    region.base = virtual_address;
    region.data.resize(size, 0);
    region.permissions = permissions;
    regions_.push_back(std::move(region));
    SetFault(MemoryFault::None);
    return true;
}

bool Memory::IsMapped(std::uint64_t virtual_address,
                      std::size_t size) const
{
    return HasPermissionAt(virtual_address, size, MemoryPermission::None);
}

bool Memory::HasPermissionAt(std::uint64_t virtual_address,
                             std::size_t size,
                             MemoryPermission permission) const
{
    if (!RangeValid(virtual_address, size)) return false;
    std::uint64_t cursor = virtual_address;
    const std::uint64_t end = virtual_address + static_cast<std::uint64_t>(size);
    while (cursor < end) {
        const Region* found = nullptr;
        std::uint64_t regionEnd = cursor;
        for (const auto& region : regions_) {
            const std::uint64_t candidateEnd = region.base + static_cast<std::uint64_t>(region.data.size());
            if (cursor >= region.base && cursor < candidateEnd) { found=&region; regionEnd=candidateEnd; break; }
        }
        if (!found) return false;
        if (permission != MemoryPermission::None && !HasPermission(found->permissions, permission)) return false;
        cursor = regionEnd < end ? regionEnd : end;
    }
    return true;
}
bool Memory::Write(std::uint64_t virtual_address,
                   const std::uint8_t* data,
                   std::size_t size)
{
    if (!data || !RangeValid(virtual_address, size)) { SetFault(MemoryFault::InvalidRange); return false; }
    if (!HasPermissionAt(virtual_address, size, MemoryPermission::None)) { SetFault(MemoryFault::Unmapped); return false; }
    if (!HasPermissionAt(virtual_address, size, MemoryPermission::Write)) { SetFault(MemoryFault::PermissionDenied); return false; }
    std::uint64_t cursor=virtual_address; std::size_t sourceOffset=0; const std::uint64_t end=virtual_address+static_cast<std::uint64_t>(size);
    while(cursor<end){ for(auto& region:regions_){const auto regionEnd=region.base+static_cast<std::uint64_t>(region.data.size()); if(cursor>=region.base&&cursor<regionEnd){const auto chunk=static_cast<std::size_t>(std::min<std::uint64_t>(end-cursor,regionEnd-cursor)); const auto off=static_cast<std::size_t>(cursor-region.base); std::memcpy(region.data.data()+off,data+sourceOffset,chunk); cursor+=chunk; sourceOffset+=chunk; break;}} }
    SetFault(MemoryFault::None); return true;
}
bool Memory::Read(std::uint64_t virtual_address,
                  std::uint8_t* data,
                  std::size_t size) const
{
    if (!data || !RangeValid(virtual_address, size)) { SetFault(MemoryFault::InvalidRange); return false; }
    if (!HasPermissionAt(virtual_address, size, MemoryPermission::None)) { SetFault(MemoryFault::Unmapped); return false; }
    if (!HasPermissionAt(virtual_address, size, MemoryPermission::Read)) { SetFault(MemoryFault::PermissionDenied); return false; }
    std::uint64_t cursor=virtual_address; std::size_t destinationOffset=0; const std::uint64_t end=virtual_address+static_cast<std::uint64_t>(size);
    while(cursor<end){ for(const auto& region:regions_){const auto regionEnd=region.base+static_cast<std::uint64_t>(region.data.size()); if(cursor>=region.base&&cursor<regionEnd){const auto chunk=static_cast<std::size_t>(std::min<std::uint64_t>(end-cursor,regionEnd-cursor)); const auto off=static_cast<std::size_t>(cursor-region.base); std::memcpy(data+destinationOffset,region.data.data()+off,chunk); cursor+=chunk; destinationOffset+=chunk; break;}} }
    SetFault(MemoryFault::None); return true;
}
bool Memory::ExecuteRead(std::uint64_t virtual_address,
                         std::uint8_t* data,
                         std::size_t size) const
{
    if (!data || !RangeValid(virtual_address, size)) { SetFault(MemoryFault::InvalidRange); return false; }
    if (!HasPermissionAt(virtual_address, size, MemoryPermission::None)) { SetFault(MemoryFault::Unmapped); return false; }
    if (!HasPermissionAt(virtual_address, size, MemoryPermission::Execute)) { SetFault(MemoryFault::PermissionDenied); return false; }
    std::uint64_t cursor=virtual_address; std::size_t destinationOffset=0; const std::uint64_t end=virtual_address+static_cast<std::uint64_t>(size);
    while(cursor<end){ for(const auto& region:regions_){const auto regionEnd=region.base+static_cast<std::uint64_t>(region.data.size()); if(cursor>=region.base&&cursor<regionEnd){const auto chunk=static_cast<std::size_t>(std::min<std::uint64_t>(end-cursor,regionEnd-cursor)); const auto off=static_cast<std::size_t>(cursor-region.base); std::memcpy(data+destinationOffset,region.data.data()+off,chunk); cursor+=chunk; destinationOffset+=chunk; break;}} }
    SetFault(MemoryFault::None); return true;
}
MemoryFault Memory::LastFault() const noexcept
{
    return last_fault_;
}

void Memory::SetFault(MemoryFault fault) const noexcept
{
    last_fault_ = fault;
}

bool Memory::HasOverlappingRegion(std::uint64_t virtual_address,
                                  std::size_t size) const noexcept
{
    if (!RangeValid(virtual_address, size)) {
        return true;
    }

    const auto size64 = static_cast<std::uint64_t>(size);
    const std::uint64_t end = virtual_address + size64;

    for (const auto& region : regions_) {
        const std::uint64_t region_end =
            region.base + static_cast<std::uint64_t>(region.data.size());
        if (virtual_address < region_end && end > region.base) {
            return true;
        }
    }

    return false;
}

bool Memory::Protect(std::uint64_t virtual_address,
                     std::size_t size,
                     MemoryPermission permissions)
{
    if (!RangeValid(virtual_address,size)) { SetFault(MemoryFault::InvalidRange); return false; }
    if ((virtual_address%PageSize)!=0 || (size%PageSize)!=0) { SetFault(MemoryFault::Unaligned); return false; }
    const auto end=virtual_address+static_cast<std::uint64_t>(size);
    if (!HasPermissionAt(virtual_address,size,MemoryPermission::None)) { SetFault(MemoryFault::Unmapped); return false; }
    for(std::size_t i=0;i<regions_.size();){
        Region region=std::move(regions_[i]);
        const auto regionEnd=region.base+static_cast<std::uint64_t>(region.data.size());
        const auto begin=std::max(region.base,virtual_address), finish=std::min(regionEnd,end);
        if(begin>=finish){regions_[i]=std::move(region);++i;continue;}
        std::vector<Region> repl;
        const auto prefix=static_cast<std::size_t>(begin-region.base), middle=static_cast<std::size_t>(finish-begin);
        if(prefix){Region r; r.base=region.base;r.permissions=region.permissions;r.data.assign(region.data.begin(),region.data.begin()+prefix);repl.push_back(std::move(r));}
        {Region r;r.base=begin;r.permissions=permissions;r.data.assign(region.data.begin()+prefix,region.data.begin()+prefix+middle);repl.push_back(std::move(r));}
        if(finish<regionEnd){Region r;r.base=finish;r.permissions=region.permissions;r.data.assign(region.data.begin()+prefix+middle,region.data.end());repl.push_back(std::move(r));}
        regions_.erase(regions_.begin()+static_cast<std::ptrdiff_t>(i));
        regions_.insert(regions_.begin()+static_cast<std::ptrdiff_t>(i),repl.begin(),repl.end());
        i+=repl.size();
    }
    SetFault(MemoryFault::None); return true;
}
void Memory::Clear()
{
    regions_.clear();
    SetFault(MemoryFault::None);
}

} // namespace myps5emu
