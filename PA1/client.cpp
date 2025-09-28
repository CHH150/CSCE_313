/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Charlie Wright
	UIN: 533009700
	Date: 9/16/25
*/
#include "common.h"
#include "FIFORequestChannel.h"

#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

using namespace std;

int main(int argc, char* argv[]) {
    int p = -1;
    double t = -1.0;
    int e = -1;
    string fname = "";
    int m = MAX_MESSAGE;
    bool want_new_channel = false;

    int c;
    while ((c = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
        switch (c) {
            case 'p': p = atoi(optarg); break;
            case 't': t = atof(optarg); break;
            case 'e': e = atoi(optarg); break;
            case 'f': fname = optarg; break;
            case 'm': m = atoi(optarg); break;
            case 'c': want_new_channel = true; break;
        }
    }

    if (!fname.empty()) {
        struct stat st{};
        if (stat("BIMDC", &st) == -1) mkdir("BIMDC", 0777);
        string src = fname;
        string dst = "BIMDC/" + fname;
        struct stat ss{}, ds{};
        bool have_src = (stat(src.c_str(), &ss) == 0);
        bool have_dst = (stat(dst.c_str(), &ds) == 0);
        if (have_src && (!have_dst || ds.st_size != ss.st_size)) {
            FILE* in = fopen(src.c_str(), "rb");
            if (!in) EXITONERROR("open src for copy");
            FILE* out = fopen(dst.c_str(), "wb");
            if (!out) EXITONERROR("open dst for copy");
            vector<char> buf(1 << 20);
            size_t nread;
            while ((nread = fread(buf.data(), 1, buf.size(), in)) > 0) {
                if (fwrite(buf.data(), 1, nread, out) != nread) EXITONERROR("copy write");
            }
            fclose(in);
            fclose(out);
        }
    }

    pid_t child = fork();
    if (child < 0) EXITONERROR("fork");
    if (child == 0) {
        string mstr = to_string(m);
        char* args[5];
        args[0] = const_cast<char*>("./server");
        args[1] = const_cast<char*>("-m");
        args[2] = const_cast<char*>(mstr.c_str());
        args[3] = nullptr;
        execvp(args[0], args);
        perror("execvp server");
        _exit(1);
    }

    FIFORequestChannel control("control", FIFORequestChannel::CLIENT_SIDE);
    FIFORequestChannel* chan = &control;
    vector<FIFORequestChannel*> opened;
    opened.push_back(&control);

    if (want_new_channel) {
        MESSAGE_TYPE nm = NEWCHANNEL_MSG;
        control.cwrite(&nm, sizeof(nm));
        char namebuf[256] = {0};
        int rn = control.cread(namebuf, sizeof(namebuf));
        if (rn <= 0) EXITONERROR("NEWCHANNEL name");
        FIFORequestChannel* nc = new FIFORequestChannel(namebuf, FIFORequestChannel::CLIENT_SIDE);
        opened.push_back(nc);
        chan = nc;
    }

    auto ensure_dir = [](const char* d){
        struct stat st{};
        if (stat(d, &st) == -1) mkdir(d, 0777);
    };

    if (!fname.empty()) {
        ensure_dir("received");
        filemsg head0(0, 0);
        int reqsz = (int)sizeof(filemsg) + (int)fname.size() + 1;
        char* req = new char[reqsz];
        memcpy(req, &head0, sizeof(filemsg));
        strcpy(req + sizeof(filemsg), fname.c_str());
        chan->cwrite(req, reqsz);
        __int64_t fsize = 0;
        int r = chan->cread(&fsize, sizeof(fsize));
        if (r != (int)sizeof(fsize)) EXITONERROR("size read");
        string outpath = "received/" + fname;
        FILE* fp = fopen(outpath.c_str(), "wb");
        if (!fp) EXITONERROR("open received/<file>");
        vector<char> buf((size_t)m);
        filemsg* pf = reinterpret_cast<filemsg*>(req);
        __int64_t off = 0;
        while (off < fsize) {
            int chunk = (int)min<__int64_t>(m, fsize - off);
            pf->offset = off;
            pf->length = chunk;
            chan->cwrite(req, reqsz);
            int got = chan->cread(buf.data(), chunk);
            if (got != chunk) EXITONERROR("chunk read");
            fseek(fp, off, SEEK_SET);
            size_t wrote = fwrite(buf.data(), 1, (size_t)got, fp);
            if ((int)wrote != got) EXITONERROR("chunk write");
            off += chunk;
        }
        fclose(fp);
        delete[] req;
    }
    else if (p != -1 && t >= 0.0 && (e == 1 || e == 2)) {
        datamsg dmsg(p, t, e);
        chan->cwrite(&dmsg, sizeof(dmsg));
        double val = 0.0;
        int rn = chan->cread(&val, sizeof(val));
        if (rn != (int)sizeof(val)) EXITONERROR("datapoint read");
        printf("For person %d, at time %.3f, the value of ecg %d is %.2f\n", p, t, e, val);
    }
    else if (p != -1 && t < 0.0 && e == -1 && fname.empty()) {
        ensure_dir("received");
        FILE* out = fopen("received/x1.csv", "wb");
        if (!out) EXITONERROR("open x1.csv");
        for (int i = 0; i < 1000; ++i) {
            double tt = i * 0.004;
            datamsg d1(p, tt, 1);
            chan->cwrite(&d1, sizeof(d1));
            double v1 = 0.0;
            if (chan->cread(&v1, sizeof(v1)) != (int)sizeof(v1)) EXITONERROR("dp v1");
            datamsg d2(p, tt, 2);
            chan->cwrite(&d2, sizeof(d2));
            double v2 = 0.0;
            if (chan->cread(&v2, sizeof(v2)) != (int)sizeof(v2)) EXITONERROR("dp v2");
            auto fmt3 = [](double x) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%.3f", x);
                int L = (int)strlen(tmp);
                while (L > 0 && tmp[L-1] == '0') { tmp[--L] = '\0'; }
                if (L > 0 && tmp[L-1] == '.') { tmp[--L] = '\0'; }
                if (L == 0) { tmp[L++] = '0'; tmp[L] = '\0'; }
                return string(tmp);
            };
            string line = fmt3(tt) + "," + fmt3(v1) + "," + fmt3(v2) + "\n";
            fwrite(line.data(), 1, line.size(), out);
        }
        fclose(out);
    }
	// closing the channel 
    MESSAGE_TYPE q = QUIT_MSG;
    for (auto* ch : opened) {
        ch->cwrite(&q, sizeof(q));
    }
    if (opened.size() > 1) delete opened[1];

    int status = 0;
    waitpid(child, &status, 0);
    return 0;
}
