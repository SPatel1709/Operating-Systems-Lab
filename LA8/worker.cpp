#include "global.hpp"

extern "C"
{
    int genallocationreq(int TOT[], int ALLOC[], int REQ[], int m);
    int genreleasereq(int ALLOC[], int REL[], int m);
}

static string vec_to_str_w(const vector<int> &v)
{
    string s = "[ ";
    for (int i = 0; i < (int)v.size(); ++i)
    {
        s += to_string(v[i]);
        if (i < (int)v.size() - 1)
            s += ", ";
    }
    s += " ]";
    return s;
}

void *worker_thread(void *args)
{
    int workerId = *(int *)args;
    delete (int *)args;

    int reqsMade = 0;
    bool firstReq = true;

    while (reqsMade < R)
    {
        usleep(200000 + rand() % 300001);

        bool doAlloc = firstReq ? true : (rand() % 3 == 0); // 1/3 alloc, 2/3 release
        pthread_mutex_lock(&RMTX);

        int status = 0;
        if (doAlloc)
            status = genallocationreq(TOTAL.data(), ALLOCATION[workerId].data(),
                                      REQUEST[workerId].data(), M);
        else
            status = genreleasereq(ALLOCATION[workerId].data(),
                                   RELEASE[workerId].data(), M);

        if (status == 0)
        {
            pthread_mutex_unlock(&RMTX);
            continue;
        }

        pthread_mutex_lock(&AMTX);

        if (doAlloc)
            pthread_mutex_lock(&WMTX[workerId]);

        REQFROM = workerId;
        REQTYPE = doAlloc ? ReqType_t::ALLOCATE : ReqType_t::RELEASE;

        if (doAlloc)
            cout << "Worker " << workerId << " makes allocation request\t" << vec_to_str_w(REQUEST[workerId]) << "\n";
        else
            cout << "Worker " << workerId << " makes release request\t\t" << vec_to_str_w(RELEASE[workerId]) << "\n";

        cond_signal_helper(SMTX, SCND); // wake manager

        pthread_cond_wait(&ACND, &AMTX);
        pthread_mutex_unlock(&AMTX);
        pthread_mutex_unlock(&RMTX);

        if (doAlloc)
        {
            pthread_cond_wait(&WCND[workerId], &WMTX[workerId]);
            pthread_mutex_unlock(&WMTX[workerId]);
        }

        ++reqsMade;
        firstReq = false;
    }

    cout << "\t\t\t Worker " << workerId << " going to quit\n";

    pthread_mutex_lock(&RMTX);

    for (int r = 0; r < M; ++r)
        RELEASE[workerId][r] = ALLOCATION[workerId][r];

    cout << "\t\t\t Releasing allocation\t\t" << vec_to_str_w(RELEASE[workerId]) << "\n";

    pthread_mutex_lock(&AMTX);

    REQFROM = workerId;
    REQTYPE = ReqType_t::RELEASE;

    cond_signal_helper(SMTX, SCND);

    pthread_cond_wait(&ACND, &AMTX);
    pthread_mutex_unlock(&AMTX);

    STATUS[workerId] = Status_t::EXITED;
    --NACTIVE;
    if (NACTIVE == 0)
        cout << "\t\t\t\t\t\t\t\t All workers left\n";

    pthread_mutex_unlock(&RMTX);

    return NULL;
}
