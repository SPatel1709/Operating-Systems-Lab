#include "diskutils.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

unsigned char *D = nullptr;
unsigned int NBLOCKS = 0;
unsigned int NFREEBLOCKS = 0;
unsigned int RBN = 0;

namespace {
int g_shmid = -1;

unsigned int block_offset(unsigned int block_no) {
    return block_no * BLOCK_SIZE;
}

unsigned int fat_offset(unsigned int block_no) {
    return FAT_START_BLOCK * BLOCK_SIZE + block_no * 4U;
}

unsigned int read_u32(unsigned int off) {
    unsigned int value = 0;
    std::memcpy(&value, D + off, sizeof(unsigned int));
    return value;
}

void write_u32(unsigned int off, unsigned int value) {
    std::memcpy(D + off, &value, sizeof(unsigned int));
}

unsigned int read_fat(unsigned int block_no) {
    return read_u32(fat_offset(block_no));
}

void write_fat(unsigned int block_no, unsigned int next_block) {
    write_u32(fat_offset(block_no), next_block);
}

bool bitmap_is_used(unsigned int block_no) {
    const unsigned int byte_index = block_no / 8U;
    const unsigned int bit_index = block_no % 8U;
    const unsigned int off = BITMAP_START_BLOCK * BLOCK_SIZE + byte_index;
    return (D[off] & static_cast<unsigned char>(1U << bit_index)) != 0;
}

void bitmap_set_used(unsigned int block_no, bool used) {
    const unsigned int byte_index = block_no / 8U;
    const unsigned int bit_index = block_no % 8U;
    const unsigned int off = BITMAP_START_BLOCK * BLOCK_SIZE + byte_index;
    if (used) {
        D[off] = static_cast<unsigned char>(D[off] | (1U << bit_index));
    } else {
        D[off] = static_cast<unsigned char>(D[off] & ~(1U << bit_index));
    }
}

bool is_valid_name(const std::string &name) {
    if (name.empty() || name.size() > METADATA_NAME_MAX) {
        return false;
    }
    for (char ch : name) {
        if (ch == '/' || ch == '`' || std::isspace(static_cast<unsigned char>(ch)) != 0) {
            return false;
        }
    }
    return true;
}

metadata read_dir_entry(unsigned int dir_first_block, unsigned int index) {
    metadata out{};
    unsigned int current = dir_first_block;
    unsigned int hops = index / 32U;
    for (unsigned int i = 0; i < hops; ++i) {
        current = read_fat(current);
        if (current == 0) {
            return out;
        }
    }
    const unsigned int idx_in_block = index % 32U;
    const unsigned int off = block_offset(current) + idx_in_block * sizeof(metadata);
    std::memcpy(&out, D + off, sizeof(metadata));
    return out;
}

void write_dir_entry(unsigned int dir_first_block, unsigned int index, const metadata &entry) {
    unsigned int current = dir_first_block;
    unsigned int hops = index / 32U;
    for (unsigned int i = 0; i < hops; ++i) {
        current = read_fat(current);
        if (current == 0) {
            return;
        }
    }
    const unsigned int idx_in_block = index % 32U;
    const unsigned int off = block_offset(current) + idx_in_block * sizeof(metadata);
    std::memcpy(D + off, &entry, sizeof(metadata));
}

unsigned int dir_entry_count(unsigned int dir_first_block) {
    metadata dot = read_dir_entry(dir_first_block, 0);
    return dot.size;
}

void refresh_dotdot_size(unsigned int dir_first_block) {
    metadata dotdot = read_dir_entry(dir_first_block, 1);
    if (dotdot.firstblock == 0) {
        return;
    }
    const unsigned int parent_size = dir_entry_count(dotdot.firstblock);
    dotdot.size = parent_size;
    write_dir_entry(dir_first_block, 1, dotdot);
}

void sync_dir_size_with_parent(unsigned int dir_first_block) {
    metadata dot = read_dir_entry(dir_first_block, 0);
    if (dir_first_block == RBN) {
        metadata dotdot_root = read_dir_entry(dir_first_block, 1);
        dotdot_root.size = dot.size;
        write_dir_entry(dir_first_block, 1, dotdot_root);
        return;
    }
    metadata dotdot = read_dir_entry(dir_first_block, 1);
    const unsigned int parent_block = dotdot.firstblock;
    if (parent_block == 0) {
        return;
    }
    const unsigned int parent_count = dir_entry_count(parent_block);
    for (unsigned int i = 2; i < parent_count; ++i) {
        metadata child = read_dir_entry(parent_block, i);
        if (child.type == 'd' && child.firstblock == dir_first_block) {
            child.size = dot.size;
            write_dir_entry(parent_block, i, child);
            break;
        }
    }
    refresh_dotdot_size(dir_first_block);
}

bool find_entry_by_name(unsigned int dir_first_block, const std::string &name, metadata *out_meta, unsigned int *out_index) {
    const unsigned int count = dir_entry_count(dir_first_block);
    for (unsigned int i = 0; i < count; ++i) {
        metadata entry = read_dir_entry(dir_first_block, i);
        if (std::string(entry.name) == name) {
            if (out_meta != nullptr) {
                *out_meta = entry;
            }
            if (out_index != nullptr) {
                *out_index = i;
            }
            return true;
        }
    }
    return false;
}

std::vector<std::string> split_path_parts(const std::string &path) {
    std::vector<std::string> parts;
    std::string token;
    std::stringstream ss(path);
    while (std::getline(ss, token, '/')) {
        parts.push_back(token);
    }
    return parts;
}

std::vector<std::string> cwd_components(const std::string &cwd_abs) {
    if (cwd_abs.empty() || cwd_abs == "/") {
        return {};
    }
    std::vector<std::string> out;
    for (const std::string &p : split_path_parts(cwd_abs)) {
        if (!p.empty()) {
            out.push_back(p);
        }
    }
    return out;
}

std::vector<std::string> normalize_components(const std::vector<std::string> &base, const std::string &path, bool absolute) {
    std::vector<std::string> out = absolute ? std::vector<std::string>{} : base;
    for (const std::string &part : split_path_parts(path)) {
        if (part.empty() || part == ".") {
            continue;
        }
        if (part == "..") {
            if (!out.empty()) {
                out.pop_back();
            }
            continue;
        }
        out.push_back(part);
    }
    return out;
}

std::string components_to_abs(const std::vector<std::string> &components) {
    if (components.empty()) {
        return "/";
    }
    std::string out;
    for (const std::string &part : components) {
        out += "/";
        out += part;
    }
    return out;
}

bool resolve_components_to_meta(const std::vector<std::string> &components, metadata *out_meta, unsigned int *out_block) {
    metadata current = read_dir_entry(RBN, 0);
    current.type = 'd';
    current.firstblock = RBN;
    unsigned int current_block = RBN;
    for (const std::string &part : components) {
        if (current.type != 'd') {
            return false;
        }
        metadata next{};
        if (!find_entry_by_name(current_block, part, &next, nullptr)) {
            return false;
        }
        current = next;
        current_block = next.firstblock;
    }
    if (out_meta != nullptr) {
        *out_meta = current;
    }
    if (out_block != nullptr) {
        *out_block = current_block;
    }
    return true;
}

bool ensure_dir_capacity_for_index(unsigned int dir_first_block, unsigned int index) {
    const unsigned int needed_hops = index / 32U;
    unsigned int current = dir_first_block;
    for (unsigned int i = 0; i < needed_hops; ++i) {
        unsigned int next = read_fat(current);
        if (next == 0) {
            next = getfreeblock();
            if (next == 0) {
                return false;
            }
            std::memset(D + block_offset(next), 0, BLOCK_SIZE);
            write_fat(current, next);
            write_fat(next, 0);
        }
        current = next;
    }
    return true;
}

bool append_dir_entry(unsigned int dir_first_block, const metadata &entry) {
    const unsigned int count = dir_entry_count(dir_first_block);
    if (!ensure_dir_capacity_for_index(dir_first_block, count)) {
        return false;
    }
    write_dir_entry(dir_first_block, count, entry);
    metadata dot = read_dir_entry(dir_first_block, 0);
    dot.size = count + 1;
    write_dir_entry(dir_first_block, 0, dot);
    sync_dir_size_with_parent(dir_first_block);
    return true;
}

void free_chain(unsigned int first_block) {
    unsigned int current = first_block;
    while (current != 0) {
        unsigned int next = read_fat(current);
        freeblock(current);
        current = next;
    }
}

std::vector<unsigned char> read_file_data(const metadata &file_meta) {
    std::vector<unsigned char> out;
    out.resize(file_meta.size);
    unsigned int remaining = file_meta.size;
    unsigned int current = file_meta.firstblock;
    unsigned int written = 0;
    while (remaining > 0 && current != 0) {
        const unsigned int chunk = std::min(remaining, BLOCK_SIZE);
        std::memcpy(out.data() + written, D + block_offset(current), chunk);
        written += chunk;
        remaining -= chunk;
        current = read_fat(current);
    }
    return out;
}

unsigned int write_file_data(const std::vector<unsigned char> &data) {
    if (data.empty()) {
        return 0;
    }
    const unsigned int blocks_needed = static_cast<unsigned int>((data.size() + BLOCK_SIZE - 1U) / BLOCK_SIZE);
    std::vector<unsigned int> blocks;
    blocks.reserve(blocks_needed);
    for (unsigned int i = 0; i < blocks_needed; ++i) {
        unsigned int b = getfreeblock();
        if (b == 0) {
            for (unsigned int allocated : blocks) {
                freeblock(allocated);
            }
            return 0;
        }
        blocks.push_back(b);
    }
    unsigned int offset = 0;
    for (unsigned int i = 0; i < blocks_needed; ++i) {
        const unsigned int b = blocks[i];
        const unsigned int chunk = std::min(static_cast<unsigned int>(data.size() - offset), BLOCK_SIZE);
        std::memset(D + block_offset(b), 0, BLOCK_SIZE);
        std::memcpy(D + block_offset(b), data.data() + offset, chunk);
        offset += chunk;
        const unsigned int next = (i + 1U < blocks_needed) ? blocks[i + 1U] : 0U;
        write_fat(b, next);
    }
    return blocks.front();
}

std::string basename_of_path(const std::string &path) {
    const auto parts = split_path_parts(path);
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
        if (!it->empty()) {
            return *it;
        }
    }
    return "";
}

bool parse_parent_target(unsigned int cwd_block, const std::string &cwd_abs, const std::string &path, unsigned int *out_parent_block, std::string *out_name) {
    if (path.empty() || path == "/") {
        return false;
    }
    const bool abs = !path.empty() && path[0] == '/';
    const auto base = cwd_components(cwd_abs);
    auto comps = normalize_components(base, path, abs);
    if (comps.empty()) {
        return false;
    }
    const std::string name = comps.back();
    comps.pop_back();
    if (!is_valid_name(name)) {
        return false;
    }
    metadata parent_meta{};
    unsigned int parent_block = 0;
    if (comps.empty()) {
        if (abs) {
            parent_meta = read_dir_entry(RBN, 0);
            parent_meta.type = 'd';
            parent_block = RBN;
        } else {
            parent_meta = read_dir_entry(cwd_block, 0);
            parent_meta.type = 'd';
            parent_block = cwd_block;
        }
    } else if (!resolve_components_to_meta(comps, &parent_meta, &parent_block)) {
        return false;
    }
    if (parent_meta.type != 'd') {
        return false;
    }
    *out_parent_block = parent_block;
    *out_name = name;
    return true;
}

bool upsert_file_in_dir(unsigned int dir_block, const std::string &name, const std::vector<unsigned char> &bytes, std::string *err) {
    metadata existing{};
    unsigned int existing_index = 0;
    const bool found = find_entry_by_name(dir_block, name, &existing, &existing_index);

    if (found && existing.type == 'd') {
        if (err != nullptr) {
            *err = "destination is a directory";
        }
        return false;
    }

    unsigned int first_block = write_file_data(bytes);
    if (!bytes.empty() && first_block == 0) {
        if (err != nullptr) {
            *err = "not enough free blocks";
        }
        return false;
    }

    if (found) {
        if (existing.firstblock != 0) {
            free_chain(existing.firstblock);
        }
        existing.type = 'f';
        std::memset(existing.name, 0, sizeof(existing.name));
        std::strncpy(existing.name, name.c_str(), sizeof(existing.name) - 1U);
        existing.size = static_cast<unsigned int>(bytes.size());
        existing.firstblock = first_block;
        write_dir_entry(dir_block, existing_index, existing);
        return true;
    }

    metadata fresh{};
    fresh.type = 'f';
    std::strncpy(fresh.name, name.c_str(), sizeof(fresh.name) - 1U);
    fresh.size = static_cast<unsigned int>(bytes.size());
    fresh.firstblock = first_block;

    if (!append_dir_entry(dir_block, fresh)) {
        if (first_block != 0) {
            free_chain(first_block);
        }
        if (err != nullptr) {
            *err = "unable to add file entry";
        }
        return false;
    }

    return true;
}

std::string display_name(const metadata &entry) {
    std::string n = entry.name;
    if (entry.type == 'd' && n != "." && n != "..") {
        n += "/";
    }
    if (n == ".") {
        return "./";
    }
    if (n == "..") {
        return "../";
    }
    return n;
}

unsigned int display_size_for_entry(unsigned int owner_dir_block, const metadata &entry) {
    if (entry.type != 'd') {
        return entry.size;
    }
    if (std::string(entry.name) == ".") {
        return dir_entry_count(owner_dir_block);
    }
    if (std::string(entry.name) == "..") {
        if (entry.firstblock == 0) {
            return 0;
        }
        return dir_entry_count(entry.firstblock);
    }
    return entry.size;
}

void print_ls_header(unsigned int total) {
    std::cout << "Total " << total << " entries\n";
    std::cout << "-----------------------------------------------------------------------\n";
    std::cout << "TYPE                         NAME             SIZE          FIRST BLOCK\n";
    std::cout << "-----------------------------------------------------------------------\n";
}

void print_ls_row(unsigned int owner_dir_block, const metadata &entry) {
    const std::string name = display_name(entry);
    const unsigned int size = display_size_for_entry(owner_dir_block, entry);
    std::printf(" %c%31s%17u%19u\n", entry.type, name.c_str(), size, entry.firstblock);
}

// internal helpers
}

int joindisk(void) {
    if (D != nullptr) {
        return 0;
    }
    const key_t key = ftok(DISK_SHM_FTOK_PATH, DISK_SHM_FTOK_PROJ);
    if (key == -1) {
        return -1;
    }
    g_shmid = shmget(key, NBLOCKS_DEFAULT * BLOCK_SIZE, 0666);
    if (g_shmid == -1) {
        return -1;
    }
    void *addr = shmat(g_shmid, nullptr, 0);
    if (addr == reinterpret_cast<void *>(-1)) {
        D = nullptr;
        return -1;
    }
    D = static_cast<unsigned char *>(addr);
    NBLOCKS = read_u32(0);
    NFREEBLOCKS = read_u32(4);
    RBN = read_u32(8);

    std::srand(static_cast<unsigned int>(std::time(nullptr) ^ static_cast<unsigned int>(getpid())));
    return 0;
}

void leavedisk(void) {
    if (D != nullptr) {
        shmdt(D);
    }
    D = nullptr;
    g_shmid = -1;
}

unsigned int getfreeblock(void) {
    if (D == nullptr) {
        return 0;
    }

    unsigned int shared_free = read_u32(4);
    if (shared_free == 0) {
        NFREEBLOCKS = 0;
        return 0;
    }

    std::uniform_int_distribution<unsigned int> dist(DATA_START_BLOCK, NBLOCKS - 1U);
    static std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));

    for (unsigned int attempts = 0; attempts < NBLOCKS * 4U; ++attempts) {
        const unsigned int candidate = dist(rng);
        if (!bitmap_is_used(candidate)) {
            bitmap_set_used(candidate, true);
            write_fat(candidate, 0);
            std::memset(D + block_offset(candidate), 0, BLOCK_SIZE);
            shared_free = read_u32(4);
            if (shared_free > 0) {
                --shared_free;
            }
            write_u32(4, shared_free);
            NFREEBLOCKS = shared_free;
            return candidate;
        }
    }

    for (unsigned int candidate = DATA_START_BLOCK; candidate < NBLOCKS; ++candidate) {
        if (!bitmap_is_used(candidate)) {
            bitmap_set_used(candidate, true);
            write_fat(candidate, 0);
            std::memset(D + block_offset(candidate), 0, BLOCK_SIZE);
            shared_free = read_u32(4);
            if (shared_free > 0) {
                --shared_free;
            }
            write_u32(4, shared_free);
            NFREEBLOCKS = shared_free;
            return candidate;
        }
    }

    return 0;
}

void freeblock(unsigned int block_no) {
    if (D == nullptr || block_no < DATA_START_BLOCK || block_no >= NBLOCKS) {
        return;
    }
    if (!bitmap_is_used(block_no)) {
        return;
    }
    bitmap_set_used(block_no, false);
    write_fat(block_no, 0);
    std::memset(D + block_offset(block_no), 0, BLOCK_SIZE);

    unsigned int shared_free = read_u32(4);
    ++shared_free;
    write_u32(4, shared_free);
    NFREEBLOCKS = shared_free;
}

int vfs_resolve(unsigned int cwd_block, const char *cwd_abs, const char *path, metadata *out_meta, char *out_abs, size_t out_abs_size) {
    if (D == nullptr || cwd_abs == nullptr) {
        return -1;
    }

    std::string raw = (path == nullptr) ? "" : std::string(path);
    if (raw.empty()) {
        if (out_meta != nullptr) {
            *out_meta = read_dir_entry(cwd_block, 0);
            out_meta->type = 'd';
            out_meta->firstblock = cwd_block;
        }
        if (out_abs != nullptr && out_abs_size > 0) {
            std::snprintf(out_abs, out_abs_size, "%s", cwd_abs);
        }
        return 0;
    }

    const bool absolute = raw[0] == '/';
    const auto base = cwd_components(cwd_abs);
    const auto comps = normalize_components(base, raw, absolute);

    metadata result{};
    unsigned int block = 0;
    if (!resolve_components_to_meta(comps, &result, &block)) {
        return -1;
    }
    if (out_meta != nullptr) {
        *out_meta = result;
        out_meta->firstblock = block;
    }
    if (out_abs != nullptr && out_abs_size > 0) {
        const std::string abs = components_to_abs(comps);
        std::snprintf(out_abs, out_abs_size, "%s", abs.c_str());
    }
    return 0;
}

int vfs_mkdir(unsigned int cwd_block, const char *cwd_abs, const char *path) {
    if (D == nullptr || cwd_abs == nullptr || path == nullptr) {
        return -1;
    }

    std::string p(path);
    while (!p.empty() && p.back() == '/') {
        p.pop_back();
    }
    if (p.empty()) {
        std::cout << "*** Error: invalid directory name\n";
        return -1;
    }

    metadata exists{};
    if (vfs_resolve(cwd_block, cwd_abs, p.c_str(), &exists, nullptr, 0) == 0) {
        std::cout << "*** Error: file or directory already exists\n";
        return -1;
    }

    unsigned int parent_block = 0;
    std::string name;
    if (!parse_parent_target(cwd_block, cwd_abs, p, &parent_block, &name)) {
        std::cout << "*** Error: invalid directory path\n";
        return -1;
    }

    const unsigned int new_block = getfreeblock();
    if (new_block == 0) {
        std::cout << "*** Error: no free block available\n";
        return -1;
    }

    metadata dot{};
    dot.type = 'd';
    std::strncpy(dot.name, ".", sizeof(dot.name) - 1U);
    dot.size = 2;
    dot.firstblock = new_block;

    metadata dotdot{};
    dotdot.type = 'd';
    std::strncpy(dotdot.name, "..", sizeof(dotdot.name) - 1U);
    dotdot.size = dir_entry_count(parent_block);
    dotdot.firstblock = parent_block;

    std::memset(D + block_offset(new_block), 0, BLOCK_SIZE);
    write_dir_entry(new_block, 0, dot);
    write_dir_entry(new_block, 1, dotdot);

    metadata child{};
    child.type = 'd';
    std::strncpy(child.name, name.c_str(), sizeof(child.name) - 1U);
    child.size = 2;
    child.firstblock = new_block;

    if (!append_dir_entry(parent_block, child)) {
        freeblock(new_block);
        std::cout << "*** Error: unable to add entry to parent directory\n";
        return -1;
    }

    refresh_dotdot_size(new_block);
    return 0;
}

int vfs_ls(unsigned int cwd_block, const char *cwd_abs, const char *path) {
    if (D == nullptr || cwd_abs == nullptr) {
        return -1;
    }

    metadata target{};
    unsigned int target_block = 0;

    if (path == nullptr || std::strlen(path) == 0U) {
        target = read_dir_entry(cwd_block, 0);
        target.type = 'd';
        target.firstblock = cwd_block;
        target_block = cwd_block;
    } else {
        if (vfs_resolve(cwd_block, cwd_abs, path, &target, nullptr, 0) != 0) {
            std::cout << "*** Error: unable to locate path " << path << "\n";
            return -1;
        }
        target_block = target.firstblock;
    }

    if (target.type == 'f') {
        print_ls_header(1);
        print_ls_row(cwd_block, target);
        std::cout << "-----------------------------------------------------------------------\n";
        return 0;
    }

    const unsigned int count = dir_entry_count(target_block);
    print_ls_header(count);
    for (unsigned int i = 0; i < count; ++i) {
        metadata entry = read_dir_entry(target_block, i);
        print_ls_row(target_block, entry);
    }
    std::cout << "-----------------------------------------------------------------------\n";
    return 0;
}

int vfs_dir(unsigned int cwd_block) {
    if (D == nullptr) {
        return -1;
    }
    const unsigned int count = dir_entry_count(cwd_block);
    for (unsigned int i = 0; i < count; ++i) {
        metadata entry = read_dir_entry(cwd_block, i);
        std::cout << std::setw(24) << display_name(entry);
        if ((i + 1U) % 4U == 0U || i + 1U == count) {
            std::cout << "\n";
        }
    }
    return 0;
}

int vfs_prn(unsigned int cwd_block, const char *cwd_abs, const char *path) {
    if (D == nullptr || cwd_abs == nullptr || path == nullptr) {
        return -1;
    }

    metadata file_meta{};
    if (vfs_resolve(cwd_block, cwd_abs, path, &file_meta, nullptr, 0) != 0 || file_meta.type != 'f') {
        std::cout << "*** Error: unable to read file " << path << "\n";
        return -1;
    }

    const auto bytes = read_file_data(file_meta);
    if (!bytes.empty()) {
        std::cout.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    if (bytes.empty() || bytes.back() != '\n') {
        std::cout << "\n";
    }
    return 0;
}

int vfs_cp(unsigned int cwd_block, const char *cwd_abs, const char *src, const char *dst) {
    if (D == nullptr || cwd_abs == nullptr || src == nullptr || dst == nullptr) {
        return -1;
    }

    const std::string src_raw(src);
    const std::string dst_raw(dst);
    const bool src_hd = !src_raw.empty() && src_raw[0] == '`';
    const bool dst_hd = !dst_raw.empty() && dst_raw[0] == '`';

    if (src_hd && dst_hd) {
        std::cout << "*** Error: hd to hd copy is not supported\n";
        return -1;
    }

    std::vector<unsigned char> bytes;
    std::string src_name_for_vd;

    if (src_hd) {
        const std::string hd_path = src_raw.substr(1);
        std::ifstream in(hd_path, std::ios::binary);
        if (!in) {
            std::cout << "*** Error: Unable to read input file " << hd_path << "\n";
            return -1;
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        const std::string data = ss.str();
        bytes.assign(data.begin(), data.end());
        src_name_for_vd = basename_of_path(hd_path);
    } else {
        metadata src_meta{};
        if (vfs_resolve(cwd_block, cwd_abs, src_raw.c_str(), &src_meta, nullptr, 0) != 0 || src_meta.type != 'f') {
            std::cout << "*** Error: unable to locate source file " << src_raw << "\n";
            return -1;
        }
        bytes = read_file_data(src_meta);
        src_name_for_vd = basename_of_path(src_raw);
        if (src_name_for_vd.empty()) {
            src_name_for_vd = src_meta.name;
        }
    }

    if (dst_hd) {
        if (src_hd) {
            std::cout << "*** Error: hd to hd copy is not supported\n";
            return -1;
        }
        const std::string out_path = dst_raw.substr(1);
        std::ofstream out(out_path, std::ios::binary);
        if (!out) {
            std::cout << "*** Error: unable to open output file " << out_path << "\n";
            return -1;
        }
        if (!bytes.empty()) {
            out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        return 0;
    }

    metadata dst_meta{};
    const bool dst_exists = vfs_resolve(cwd_block, cwd_abs, dst_raw.c_str(), &dst_meta, nullptr, 0) == 0;

    unsigned int target_dir_block = 0;
    std::string target_name;

    if (dst_exists && dst_meta.type == 'd') {
        target_dir_block = dst_meta.firstblock;
        target_name = src_name_for_vd;
    } else if (dst_exists && dst_meta.type == 'f') {
        if (!parse_parent_target(cwd_block, cwd_abs, dst_raw, &target_dir_block, &target_name)) {
            std::cout << "*** Error: invalid destination path\n";
            return -1;
        }
    } else {
        if (!parse_parent_target(cwd_block, cwd_abs, dst_raw, &target_dir_block, &target_name)) {
            std::cout << "*** Error: invalid destination path\n";
            return -1;
        }
    }

    if (!is_valid_name(target_name)) {
        std::cout << "*** Error: invalid destination file name\n";
        return -1;
    }

    std::string err;
    if (!upsert_file_in_dir(target_dir_block, target_name, bytes, &err)) {
        std::cout << "*** Error: " << err << "\n";
        return -1;
    }

    return 0;
}
