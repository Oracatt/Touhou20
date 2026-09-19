#include "wave_reader.hpp"
#include <cstring>
#include <stdexcept>

namespace th20::source::audio {
WaveReader::WaveReader() noexcept {std::memset(this,0,sizeof *this);}
WaveReader::~WaveReader() {close();}
HRESULT WaveReader::open_file(const char* path,TrackFormat* format,std::uint32_t mode,std::uint32_t base_offset) {
    file_mode=mode;memory_mode=0;
    if(file_mode!=1 || !path) return E_INVALIDARG;
    wchar_t wide[262]{};
    if(!MultiByteToWideChar(932,0,path,-1,wide,260)) return E_INVALIDARG;
    file=CreateFileW(wide,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0x08000080,nullptr);
    if(file==INVALID_HANDLE_VALUE) return E_FAIL;
    track=format;filename=path;reset(false,0,base_offset);initial_remaining=remaining;looped=0;return S_OK;
}
HRESULT WaveReader::open_memory(const std::uint8_t* bytes,std::uint32_t size,TrackFormat* format,int flag) {
    track=format;memory_size=size;memory_start=bytes;memory_current=memory_start;memory_mode=1;
    return flag==1?S_OK:E_NOTIMPL; // original returns E_NOTIMPL after initializing when flag=0
}
HRESULT WaveReader::reopen(TrackFormat* format,std::uint32_t position,std::uint32_t base_offset) {
    if(memory_mode) return E_FAIL;
    if(file==INVALID_HANDLE_VALUE) open_file(filename,format,1,base_offset);
    if(file==INVALID_HANDLE_VALUE) return E_FAIL;
    looped=0;track=format;reset(false,position,base_offset);initial_remaining=remaining;return S_OK;
}
HRESULT WaveReader::read(void* output,std::uint32_t requested,std::uint32_t* received) {
    if(memory_mode) {
        if(!memory_current) return CO_E_NOTINITIALIZED;
        if(received) *received=0;
        const auto consumed=static_cast<std::uint32_t>(memory_current-memory_start);
        if(consumed>memory_size) throw std::out_of_range("Audio memory cursor exceeds allocation");
        if(requested>memory_size-consumed) requested=memory_size-consumed;
        std::memcpy(output,memory_current,requested);memory_current+=requested;
        if(received) *received=requested;return S_OK;
    }
    if(!file) return CO_E_NOTINITIALIZED;
    if(!output || !received) return E_INVALIDARG;
    const auto amount=requested>remaining?remaining:requested;
    remaining-=amount;
    DWORD actual=0;ReadFile(file,output,amount,&actual,nullptr);*received=actual;
    return S_OK; // original ignores the BOOL return; failed I/O's byte count is outside the proven domain
}
HRESULT WaveReader::reset(bool loop,std::uint32_t position,std::uint32_t base_offset) {
    if(memory_mode) {
        memory_current=memory_start;
        if(static_cast<std::int32_t>(track->total_bytes)>0) memory_size=track->total_bytes;
        if(loop && static_cast<std::int32_t>(track->loop_start)>0) memory_current+=track->loop_start;
        return S_OK; // original memory path ignores position
    }
    if(file==INVALID_HANDLE_VALUE || !file) return CO_E_NOTINITIALIZED;
    if(!loop || static_cast<std::int32_t>(track->loop_start)<1) {
        if(track->total_bytes<=position) position-=track->total_bytes-track->loop_start;
        SetFilePointer(file,static_cast<LONG>(base_offset+track->file_offset+position),nullptr,FILE_BEGIN);
        remaining=track->total_bytes-position;
    } else {
        SetFilePointer(file,static_cast<LONG>(base_offset+track->file_offset+track->loop_start),nullptr,FILE_BEGIN);
        remaining=track->total_bytes-track->loop_start;
    }
    return S_OK;
}
HRESULT WaveReader::close() {
    if(file_mode==1) {CloseHandle(file);file=INVALID_HANDLE_VALUE;}
    return S_OK;
}
std::uint32_t WaveReader::tell() const {return SetFilePointer(file,0,nullptr,FILE_CURRENT);}
}
