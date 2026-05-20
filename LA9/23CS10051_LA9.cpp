#include <iostream>
#include <array>
#include <time.h>
#include <iomanip>
using namespace std;

static constexpr int PAGE_FAULT_TIME = 10000;
static constexpr int MEM_ACC_TIME = 100;
static constexpr int PAGE_SZ = 4 * 1024;
static constexpr int PAGE_TABLE_SZ = 5000;
static constexpr int IMG_ROW_SZ = 1080;
static constexpr int IMG_COL_SZ = 1920;
static constexpr int PTR_SZ = 8;

int k;
long long memAccess = 0;
long long pageFault = 0;

void clear()
{
    memAccess = 0;
    pageFault = 0;
}

double calculate_time()
{
    return pageFault * (double)PAGE_FAULT_TIME * 1e-6 + memAccess * (double)MEM_ACC_TIME * 1e-9;
}

void printStats()
{
    cout << "\t\t\tTotal number of memory accesses = " << memAccess << endl;
    cout << "\t\t\tTotal number of page faults = " << pageFault << endl;
    cout << "\t\t\tPage fault rate (percentage) = " << 100 * ((double)pageFault / memAccess) << endl;
    cout << "\t\t\tTotal memory access time = " << fixed << setprecision(2) << calculate_time() << " sec" << endl;
}

typedef struct Page
{
    Page *prev;
    Page *next;
    int val;
    Page(int Val, Page *prevptr, Page *nextptr)
        : prev(prevptr), next(nextptr), val(Val) {}
} Page;

class LRU
{
    Page *head;
    Page *last;
    static array<Page *, PAGE_TABLE_SZ> PT;
    int F;
    int currSize;

public:
    LRU(int f)
        : head(nullptr), last(nullptr), F(f), currSize(0)
    {
        for (auto &page : PT)
            page = nullptr;
    }

    void reset()
    {
        while (head != last)
        {
            Page *temp = head;
            head = head->next;
            delete temp;
        }
        if (head != nullptr)
        {
            delete head;
            head = nullptr;
        }
        last = nullptr;
        currSize = 0;
        PT.fill(nullptr);
    }

    void access_page(int page)
    {
        if (PT[page] == nullptr)
        {
            ++pageFault;
            insert(page);
        }
        else
        {
            update_LRU(PT[page]);
        }
    }

    ~LRU()
    {
        while (this->head != this->last)
        {
            Page *temp = this->head;
            this->head = this->head->next;
            delete temp;
        }
        if (this->head != nullptr)
            delete this->head;
    }

private:
    void update_LRU(Page *page)
    {
        if (page == head)
            return;
        if (page->prev != nullptr)
            page->prev->next = page->next;
        if (page->next != nullptr)
            page->next->prev = page->prev;
        else
            last = page->prev;
        page->next = head;
        page->prev = nullptr;
        if (head)
            head->prev = page;
        head = page;
    }

    void insert(int val)
    {
        if (val < 0 || val >= PAGE_TABLE_SZ)
        {
            cerr << "(ERROR) LRU:insert: val not bound: " << val << "\n";
            return;
        }
        if (currSize < F)
        {
            Page *p = new Page(val, nullptr, head);
            if (head)
                head->prev = p;
            head = p;
            if (last == nullptr)
                last = p;
            ++currSize;
            PT[val] = head;
        }
        else
        {
            PT[last->val] = nullptr;
            PT[val] = last;
            last->val = val;
            update_LRU(last);
        }
    }
};

array<Page *, PAGE_TABLE_SZ> LRU::PT;

typedef struct
{
    unsigned char R, G, B;
} pixel;

inline void acc(LRU &lru, long long addr, int elemSize)
{
    ++memAccess;
    int p0 = (int)(addr / PAGE_SZ);
    int p1 = (int)((addr + elemSize - 1) / PAGE_SZ);
    for (int p = p0; p <= p1; ++p)
        lru.access_page(p);
}

bool g_isOutput = false;

void get_pages(int schemeNo, LRU &lru, int i, int j)
{
    static constexpr long long R1 = IMG_ROW_SZ * (long long)IMG_COL_SZ;
    long long ij = (long long)i * IMG_COL_SZ + j;
    long long base;
    switch (schemeNo)
    {
    case 1:
        base = g_isOutput ? 3 * R1 : 0;
        acc(lru, base + ij, 1);
        acc(lru, base + R1 + ij, 1);
        acc(lru, base + 2 * R1 + ij, 1);
        break;

    case 2:
        base = g_isOutput ? 3 * R1 : 0;
        acc(lru, base + 3 * ij, 1);
        acc(lru, base + 3 * ij + 1, 1);
        acc(lru, base + 3 * ij + 2, 1);
        break;

    case 3:
        base = g_isOutput ? 4 * R1 : 0;
        acc(lru, base + 4 * ij, 4);
        break;

    case 4:
    {
        long long ptrBlk = IMG_ROW_SZ * (long long)PTR_SZ;
        long long chunk = ptrBlk + R1;
        base = g_isOutput ? 6 * chunk : 0;
        for (int c = 0; c < 3; ++c)
        {
            acc(lru, base + c * chunk + (long long)i * PTR_SZ, PTR_SZ);
            acc(lru, base + c * chunk + ptrBlk + ij, 1);
        }
    }
    break;

    case 5:
    {
        long long ptrBlk = IMG_ROW_SZ * (long long)PTR_SZ;
        long long chunk = ptrBlk + 3 * R1;
        base = g_isOutput ? chunk : 0;
        long long pi = (long long)i * PTR_SZ;
        acc(lru, base + pi, PTR_SZ);
        acc(lru, base + ptrBlk + 3 * ij, 1);
        acc(lru, base + pi, PTR_SZ);
        acc(lru, base + ptrBlk + 3 * ij + 1, 1);
        acc(lru, base + pi, PTR_SZ);
        acc(lru, base + ptrBlk + 3 * ij + 2, 1);
    }
    break;

    case 6:
    {
        long long ptrBlk = IMG_ROW_SZ * (long long)PTR_SZ;
        long long chunk = ptrBlk + 4 * R1;
        base = g_isOutput ? chunk : 0;
        acc(lru, base + (long long)i * PTR_SZ, PTR_SZ);
        acc(lru, base + ptrBlk + 4 * ij, 4);
    }
    break;
    }
}

void scheme(int schemeNo, LRU &lru)
{
    for (int i = 0; i < IMG_ROW_SZ; ++i)
    {
        for (int j = 0; j < IMG_COL_SZ; ++j)
        {
            int rowstart = i - k;
            if (rowstart < 0)
                rowstart = 0;

            int rowend = i + k;
            if (rowend >= 1080)
                rowend = 1079;

            int colstart = j - k;
            if (colstart < 0)
                colstart = 0;

            int colend = j + k;
            if (colend >= 1920)
                colend = 1919;

            for (int row = rowstart; row <= rowend; ++row)
            {
                for (int col = colstart; col <= colend; ++col)
                {
                    if ((row - i) * (row - i) + (col - j) * (col - j) <= k * k)
                    {
                        g_isOutput = false;
                        get_pages(schemeNo, lru, row, col);
                    }
                }
            }

            g_isOutput = true;
            get_pages(schemeNo, lru, i, j);
        }
    }
}

int main(int argc, char *argv[])
{

    if (argc != 3)
    {
        cerr << "./LRU <k> <f>\n";
        exit(EXIT_FAILURE);
    }

    k = atoi(argv[1]);
    int f = atoi(argv[2]);

    cout << "+++ k = " << k << ", f = " << f << "\n";

    const char *names[] = {"Three static arrays", "One static array of char",
                           "One static array of struct", "Three dynamic arrays",
                           "One dynamic array of char", "One dynamic array of struct"};

    for (int s = 1; s <= 6; ++s)
    {
        LRU lru(f);
        clear();
        scheme(s, lru);
        cout << "\n+++ Scheme " << s << ": " << names[s - 1] << "\n";
        printStats();
    }

    return 0;
}
