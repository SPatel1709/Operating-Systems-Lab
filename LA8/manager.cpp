#include "global.hpp"

int N, M, R, REQFROM, NACTIVE;
ReqType_t REQTYPE;

vector<Status_t> STATUS;

pthread_mutex_t SMTX, RMTX, AMTX;
vector<pthread_mutex_t> WMTX;

pthread_cond_t SCND, RCND, ACND;
vector<pthread_cond_t> WCND;

vector<int> AVAILABLE, TOTAL;
vector<vector<int>> ALLOCATION, REQUEST, RELEASE;

vector<int> RQ;

void *worker_thread(void *args);
void *dlhandler_thread(void *args);

void allocate_vectors()
{
    AVAILABLE.resize(M, 0);
    TOTAL.resize(M, 0);

    ALLOCATION.resize(N);
    REQUEST.resize(N);
    RELEASE.resize(N);
    WCND.resize(N);
    WMTX.resize(N);
    STATUS.resize(N, Status_t::ACTIVE);

    REQTYPE = ReqType_t::UNKNOWN;
    NACTIVE = N;

    for (int indx = 0; indx < N; ++indx)
    {
        ALLOCATION[indx].resize(M, 0);
        REQUEST[indx].resize(M, 0);
        RELEASE[indx].resize(M, 0);
    }
}

void alloc_random_resources()
{
    srand(getpid());
    for (int i = 0; i < M; ++i)
        AVAILABLE[i] = TOTAL[i] = rand() % 25 + 8;
}

inline void init_mutex(pthread_mutex_t &mtx)
{
    if (pthread_mutex_init(&mtx, NULL) != 0)
    {
        cerr << "(ERROR) managerThread: initialise_resources(): pthread_mutex_init failed\n";
        exit(EXIT_FAILURE);
    }
}

inline void destroy_mutex(pthread_mutex_t &mtx)
{
    if (pthread_mutex_destroy(&mtx) != 0)
        cerr << "(WARNING) managerThread: destroy_resources(): pthread_mutex_destroy failed\n";
}

inline void init_cond(pthread_cond_t &cnd)
{
    if (pthread_cond_init(&cnd, NULL) != 0)
    {
        cerr << "(ERROR) managerThread: initialise_resources(): pthread_cond_init failed\n";
        exit(EXIT_FAILURE);
    }
}

inline void destroy_cond(pthread_cond_t &cnd)
{
    if (pthread_cond_destroy(&cnd) != 0)
        cerr << "(WARNING) managerThread: destroy_resources(): pthread_cond_destroy failed\n";
}

void initialise_resources()
{
    init_mutex(SMTX);
    init_mutex(RMTX);
    init_mutex(AMTX);

    for (auto &mtx : WMTX)
        init_mutex(mtx);

    init_cond(SCND);
    init_cond(RCND);
    init_cond(ACND);

    for (auto &cnd : WCND)
        init_cond(cnd);
}

void destroy_resources()
{
    for (auto &cnd : WCND)
        destroy_cond(cnd);
    destroy_cond(SCND);
    destroy_cond(RCND);
    destroy_cond(ACND);

    for (auto &mtx : WMTX)
        destroy_mutex(mtx);

    destroy_mutex(SMTX);
    destroy_mutex(RMTX);
    destroy_mutex(AMTX);
}

void process_release(int worker)
{
    for (int rsc = 0; rsc < M; ++rsc)
    {
        AVAILABLE[rsc] += RELEASE[worker][rsc];
        ALLOCATION[worker][rsc] -= RELEASE[worker][rsc];
        RELEASE[worker][rsc] = 0;
    }
}

void release_all(int worker)
{
    for (int rsc = 0; rsc < M; ++rsc)
    {
        AVAILABLE[rsc] += ALLOCATION[worker][rsc];
        ALLOCATION[worker][rsc] = 0;
    }
}

bool can_allocate(int worker)
{
    for (int rsc = 0; rsc < M; ++rsc)
        if (AVAILABLE[rsc] < REQUEST[worker][rsc])
            return false;
    return true;
}

void allocate_worker(int worker)
{
    for (int rsc = 0; rsc < M; ++rsc)
    {
        ALLOCATION[worker][rsc] += REQUEST[worker][rsc];
        AVAILABLE[rsc] -= REQUEST[worker][rsc];
        REQUEST[worker][rsc] = 0;
    }
}

static string vec_to_str(const vector<int> &v, string open, string close)
{
    string s = open + " ";
    for (int i = 0; i < (int)v.size(); ++i)
    {
        s += to_string(v[i]);
        if (i < (int)v.size() - 1)
            s += ", ";
    }
    if (!v.empty())
        s += " ";
    s += close;
    return s;
}

void print_available()
{
    cout << "\t\t\t\t\t\t AVAILABLE = " << vec_to_str(AVAILABLE, "[", "]") << "\n";
}

void print_waiting()
{
    cout << "\t\t\t Workers waiting: " << vec_to_str(RQ, "(", ")") << "\n";
}

void check_RQ()
{
    for (auto it = RQ.begin(); it != RQ.end();)
    {
        if (can_allocate(*it))
        {
            int w = *it;
            vector<int> req(REQUEST[w]);
            it = RQ.erase(it);
            allocate_worker(w);
            cout << "Worker " << w << " granted pending request\t" << vec_to_str(req, "[", "]") << "\n";
            print_available();
            cond_signal_helper(WMTX[w], WCND[w]);
            // pthread_mutex_lock(&WMTX[w]);
            // pthread_cond_signal(&WCND[w]);
            // pthread_mutex_unlock(&WMTX[w]);
        }
        else
            ++it;
    }
}

void *managerThread(void *)
{
    allocate_vectors();
    initialise_resources();
    alloc_random_resources();

    cout << "\t\t\t\t\t\t\t TOTAL = " << vec_to_str(TOTAL, "[", "]") << "\n";

    vector<pthread_t> wthreads(N);
    for (int i = 0; i < N; ++i)
    {
        int *id = new int(i);
        if (pthread_create(&wthreads[i], NULL, worker_thread, id) != 0)
        {
            cerr << "(ERROR) main: pthread_create for worker " << i << " failed\n";
            exit(EXIT_FAILURE);
        }
    }

    pthread_t dlthread;
    if (pthread_create(&dlthread, NULL, dlhandler_thread, NULL) != 0)
    {
        cerr << "(ERROR) main: pthread_create for dlhandler failed\n";
        exit(EXIT_FAILURE);
    }

    pthread_mutex_lock(&SMTX);
    while (true)
    {
        pthread_cond_wait(&SCND, &SMTX); // wait untill any worker requests anything.

        if (REQTYPE == ReqType_t::QUIT)
        {
            pthread_mutex_unlock(&SMTX);
            break;
        }
        else if (REQTYPE == ReqType_t::RELEASE)
        {
            process_release(REQFROM);
            print_available();
            print_waiting();
            cond_signal_helper(AMTX, ACND);

            check_RQ();
        }
        else if (REQTYPE == ReqType_t::ALLOCATE)
        {
            if (can_allocate(REQFROM))
            {
                allocate_worker(REQFROM);
                cout << "Worker " << REQFROM << " granted request\n";
                print_available();
                cond_signal_helper(AMTX, ACND);
                cond_signal_helper(WMTX[REQFROM], WCND[REQFROM]); // signaling worker that request granted.
            }
            else
            {
                cout << "Worker " << REQFROM << " has to wait\n";
                RQ.push_back(REQFROM);
                print_waiting();

                cond_signal_helper(AMTX, ACND);
            }
        }
    }

    for (int i = 0; i < N; ++i)
        pthread_join(wthreads[i], NULL);
    pthread_join(dlthread, NULL);

    destroy_resources();
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        cerr << "Usage: " << argv[0] << " M N R\n";
        exit(EXIT_FAILURE);
    }

    M = atoi(argv[1]);
    N = atoi(argv[2]);
    R = atoi(argv[3]);

    managerThread(NULL);
    return 0;
}
