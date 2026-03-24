/**
 * main.cpp — Minima C++ experimental node entry point.
 *
 * Usage:
 *   minimanode [options]
 *
 * Options:
 *   -port <n>          Listen port (default: 9001)
 *   -connect <host>    Connect to peer on startup
 *   -cport <n>         Peer port (default: 9001)
 *   -quiet             Suppress verbose output
 *   -help              Show help
 */
#include "Node.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>

using namespace minima;
using namespace std::chrono_literals;

static std::atomic<bool> g_stop { false };

static void sigHandler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) g_stop = true;
    // SIGPIPE — ignored (handled per-write via MSG_NOSIGNAL)
}

static void printHelp(const char* argv0) {
    printf("Minima C++ experimental node\n");
    printf("Usage: %s [options]\n\n", argv0);
    printf("  -port <n>       Listen port (default: 9001)\n");
    printf("  -connect <host> Connect to peer on startup\n");
    printf("  -cport <n>      Peer port when using -connect (default: 9001)\n");
    printf("  -quiet          Suppress verbose logging\n");
    printf("  -help           Show this help\n");
}

int main(int argc, char** argv) {
    // Unbuffered stdout so output appears immediately even when piped
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    // Ignore SIGPIPE — broken pipe on send() returns error instead of crashing
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  sigHandler);
    signal(SIGTERM, sigHandler);

    NodeConfig cfg;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0) {
            printHelp(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "-port") == 0 && i + 1 < argc)
            cfg.listenPort = (uint16_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "-connect") == 0 && i + 1 < argc)
            cfg.connectHost = argv[++i];
        else if (strcmp(argv[i], "-cport") == 0 && i + 1 < argc)
            cfg.connectPort = (uint16_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "-quiet") == 0)
            cfg.verbose = false;
    }

    Node node(cfg);
    if (!node.start()) {
        fprintf(stderr, "Failed to start node\n");
        return 1;
    }

    printf("Node started. Press Ctrl+C to stop.\n");
    fflush(stdout);

    int tick = 0;
    while (!g_stop) {
        std::this_thread::sleep_for(1s);
        if (++tick % 30 == 0) {
            printf("[Status] peers=%d  tip=%s\n",
                   node.peerCount(),
                   node.tipBlock().toString().c_str());
            fflush(stdout);
        }
    }

    printf("\nShutting down...\n");
    fflush(stdout);
    node.stop();
    return 0;
}
