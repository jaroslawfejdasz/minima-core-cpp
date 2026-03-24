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
 *
 * Examples:
 *   minimanode -port 9001
 *   minimanode -connect 91.107.194.118 -cport 9001
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

static void sigHandler(int) {
    g_stop = true;
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
    NodeConfig cfg;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0) {
            printHelp(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
            cfg.listenPort = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-connect") == 0 && i + 1 < argc) {
            cfg.connectHost = argv[++i];
        } else if (strcmp(argv[i], "-cport") == 0 && i + 1 < argc) {
            cfg.connectPort = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-quiet") == 0) {
            cfg.verbose = false;
        }
    }

    // Handle Ctrl+C
    signal(SIGINT,  sigHandler);
    signal(SIGTERM, sigHandler);

    Node node(cfg);
    if (!node.start()) {
        fprintf(stderr, "Failed to start node\n");
        return 1;
    }

    printf("Node started. Press Ctrl+C to stop.\n");

    // Status loop — print stats every 30s
    while (!g_stop) {
        std::this_thread::sleep_for(1s);
        // Check every second but print every 30
        static int counter = 0;
        if (++counter % 30 == 0) {
            printf("[Status] peers=%d  tip=%s\n",
                   node.peerCount(),
                   node.tipBlock().toString().c_str());
        }
    }

    printf("\nShutting down...\n");
    node.stop();
    return 0;
}
