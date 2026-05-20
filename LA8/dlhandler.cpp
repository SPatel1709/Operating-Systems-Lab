#include "global.hpp"

extern void check_RQ();
extern void release_all(int worker);
extern void print_available();
extern void print_waiting();

static bool detect_deadlock(vector<int> &finishSeq)
{
    vector<int> work(AVAILABLE);
    vector<bool> finish(N, false);

    for (int i = 0; i < N; ++i)
        if (STATUS[i] == Status_t::EXITED)
            finish[i] = true;

    finishSeq.clear();
    bool progress = true;
    while (progress)
    {
        progress = false;
        for (int i = 0; i < N; ++i)
        {
            if (finish[i])
                continue;
            bool ok = true;
            for (int r = 0; r < M; ++r)
                if (REQUEST[i][r] > work[r])
                {
                    ok = false;
                    break;
                }
            if (ok)
            {
                finish[i] = true;
                for (int r = 0; r < M; ++r)
                    work[r] += ALLOCATION[i][r];
                finishSeq.push_back(i);
                progress = true;
            }
        }
    }

    for (int i = 0; i < N; ++i)
        if (STATUS[i] == Status_t::ACTIVE && !finish[i])
            return true;
    return false;
}

void *dlhandler_thread(void *)
{
    while (true)
    {
        sleep(1);

        pthread_mutex_lock(&RMTX);

        if (NACTIVE == 0)
        {
            REQTYPE = ReqType_t::QUIT;
            pthread_mutex_lock(&SMTX);
            pthread_cond_signal(&SCND);
            pthread_mutex_unlock(&SMTX);
            pthread_mutex_unlock(&RMTX);
            return NULL;
        }

        vector<int> finishSeq;
        bool deadlock = detect_deadlock(finishSeq);

        cout << "\t\t\t\t\t\t Deadlock detection in progress\n";
        cout << "\t\t\t\t\t\t Finish sequence:";
        for (int t : finishSeq)
            cout << " " << t;
        cout << "\n";

        if (!deadlock)
        {
            cout << "\t\t\t\t\t\t No deadlock detected\n";
            pthread_mutex_unlock(&RMTX);
            continue;
        }

        while (deadlock)
        {
            cout << "\t\t\t\t\t\t Deadlock detected\n";

            cout << "\t\t\t Allocation status:";
            for (int i = 0; i < N; ++i)
            {
                if (STATUS[i] == Status_t::EXITED)  continue;

                int total = 0;
                for (int r = 0; r < M; ++r)     total += ALLOCATION[i][r];

                if (total == 0) continue; //only those workers that have non-zero total resources.
                cout << " " << i << ":" << total;
            }
            cout << "\n";

            int victim = -1;
            int maxAlloc = -1;

            for (int w : RQ)
            {
                if (STATUS[w] != Status_t::ACTIVE)
                    continue;
                int total = 0;
                for (int r = 0; r < M; ++r)
                    total += ALLOCATION[w][r];
                if (total > maxAlloc || (total == maxAlloc && (victim == -1 || w < victim)))
                {
                    maxAlloc = total;
                    victim = w;
                }
            }

            if (victim == -1)   break;

            cout << "\t\t\t Preempting resources from worker " << victim << " with " << maxAlloc << " resources\n";

            release_all(victim);
            check_RQ();

            finishSeq.clear();
            deadlock = detect_deadlock(finishSeq);

            cout << "\t\t\t\t\t\t Deadlock detection in progress\n";
            cout << "\t\t\t\t\t\t Finish sequence:";
            for (int t : finishSeq)   cout << " " << t;
            cout << "\n";

            if (!deadlock)     cout << "\t\t\t\t\t\t No deadlock detected\n";
        }

        pthread_mutex_unlock(&RMTX);
    }

    return NULL;
}
