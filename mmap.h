#pragma once

#include <stdint.h>
#include <sstream>
#include <vector>

#define NOMINMAX
#include <windows.h>


struct SmallDoubleMatrix {
    static const unsigned int max_size = 500;
    unsigned int height;
    unsigned int width;
    double data[max_size];
};

struct SingleImage {
    unsigned int frame_tag;
    int16_t data[512 * 512];
};

struct SI4Image {
    unsigned int frame_tag;
    unsigned int height;
    unsigned int width;
    unsigned int n_ch;
    static const unsigned int max_size = 1024 * 1024 * 3;
    int16_t data[max_size];
};

template<class T>
class MMap {
public:
    MMap(std::string name, bool only_if_exist = false);
    ~MMap();
    bool is_valid() {
        return p_buf_;
    }
    int get(T& destination);
    int set(const T& source, unsigned int num = sizeof(T));
    const volatile T* get_pointer() {
        return p_data_;
    }
    const std::string get_error(void) {
        std::string return_string = error_.str();
        error_.str("");
        error_.clear();
        return return_string;
    }

    const unsigned int kMutexTimeout = 500;

private:
    HANDLE h_mapfile_;
    HANDLE h_mutex_;

    std::string base_name_;
    std::string mutex_name_;
    std::string mmap_name_;

    void* p_buf_; // pointer to the beginning of memory map

protected:
    std::ostringstream error_;

    volatile int* p_mmap_size_;
    volatile int* p_data_size_;
    volatile T* p_data_;

    bool wait_for_mutex(void);
    bool release_mutex(void) {
        return ReleaseMutex(h_mutex_);
    }

    void clear(void);
};

template<class T>
MMap<T>::MMap(std::string name, bool only_if_exist) :base_name_(name){
    mutex_name_ = "MMAP_" + base_name_ + "_MUTEX";
    std::wstring w_mutex_name(mutex_name_.begin(), mutex_name_.end());
    h_mutex_ = CreateMutex(NULL, false, w_mutex_name.c_str());
    if (!h_mutex_) {
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            h_mutex_ = OpenMutex(SYNCHRONIZE, false, w_mutex_name.c_str());
            if (!h_mutex_) {
                error_ << "Could not open mutex. GLE=" << GetLastError() << std::endl;
                clear();
                return;
            }
        }
        else {
            error_ << "Could not create mutex. GLE=" << GetLastError() << std::endl;
            clear();
            return;
        }
    }

    mmap_name_ = "MMAP_" + base_name_;
    std::wstring w_mmap_name(mmap_name_.begin(), mmap_name_.end());

    int required_mmap_size = sizeof(T) + 2 * sizeof(int); // p_max_size_, p_size_
    bool is_opened = true;
    h_mapfile_ = OpenFileMapping(FILE_MAP_ALL_ACCESS, false, w_mmap_name.c_str());
    if (!h_mapfile_) {
        is_opened = false;
        if (only_if_exist) {
            error_ << "Could not open MMap. GLE=" << GetLastError() << std::endl;
            clear();
            return;
        }
        else {
            h_mapfile_ = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, required_mmap_size, w_mmap_name.c_str()); // zero-initialized
            if (!h_mapfile_) {
                error_ << "Could not open or create MMap. GLE=" << GetLastError() << ", size=" << required_mmap_size <<std::endl;
                clear();
                return;
            }
        }
    }

    p_buf_ = MapViewOfFile(h_mapfile_, FILE_MAP_WRITE, 0, 0, 0);
    if (!p_buf_) {
        error_ << "Could not obtain MapView. GLE=" << GetLastError() << std::endl;
        clear();
        return;
    }
    p_mmap_size_ = reinterpret_cast<volatile int*>(p_buf_);
    p_data_size_ = reinterpret_cast<volatile int*>(p_mmap_size_ + 1);
    p_data_ = reinterpret_cast<volatile T*>(p_data_size_ +1);

    if (is_opened) {
        if (*p_mmap_size_ != required_mmap_size) {
            error_ << "Size mismatch: opened map size: " << *p_mmap_size_ << "bytes is different from: " << required_mmap_size << " bytes.\n";
            clear();
            return;
        }
    }
    else {
        *p_mmap_size_ = required_mmap_size;
    }
    return;
}

template<class T>
bool MMap<T>::wait_for_mutex(void) {
    bool locked = false;
    switch (WaitForSingleObject(h_mutex_, kMutexTimeout)) {
    case WAIT_ABANDONED:
        error_ << "Mutex was previously abandoned." << std::endl;
    case WAIT_OBJECT_0:
        locked = true;
        break;
    case WAIT_TIMEOUT:
        error_ << "Time-out before obtaining a mutex." << std::endl;
    case WAIT_FAILED:
    default:
        error_ << "Failed to obtain a mutex." << std::endl;
        locked = false;
        break;
    }
    return locked;
}

template<class T>
int MMap<T>::set(const T& source, unsigned int num) {
    if (!is_valid()) return -1;
    if (num > sizeof(T)) return -3;
    if (num < 0 ) return -4;
    if (wait_for_mutex()) {
        *p_data_size_ = num;
        memcpy((void*)(p_data_), (void*)(&source), num);
        release_mutex();
        return num;
    }
    return -2;
}

template<class T>
int MMap<T>::get(T& destination) {
    if (!is_valid()) return -1;
    if (wait_for_mutex()) {
        unsigned int num = *p_data_size_;
        if (num > sizeof(T)) {
            release_mutex();
            return -3;
        }
        memcpy((void*)(&destination), (void*)(p_data_), num);
        release_mutex();
        return num;
    }
    return -2;
}

template<class T>
void MMap<T>::clear(void) {
    if (p_buf_) {
        UnmapViewOfFile(p_buf_);
        p_buf_ = NULL;
    }
    if (h_mapfile_) {
        CloseHandle(h_mapfile_);
        h_mapfile_ = NULL;
    }
    if (h_mutex_) {
        ReleaseMutex(h_mutex_);
        CloseHandle(h_mutex_);
        h_mutex_ = NULL;
    }
}

template<class T>
MMap<T>::~MMap() {
    clear();
}
