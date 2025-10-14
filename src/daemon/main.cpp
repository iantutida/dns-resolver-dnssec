/*
 * ----------------------------------------
 * Arquivo: main.cpp (daemon)
 * Propósito: Interface de linha de comando para gerenciamento do daemon de cache DNS
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver Recursivo Validante com Cache e DNSSEC
 * ----------------------------------------
 */

#include "CacheDaemon.h"
#include <iostream>
#include <fstream>
#include <csignal>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

const char* PID_FILE = "/tmp/dns_cache.pid";
const char* SOCKET_PATH = "/tmp/dns_cache.sock";

// Envia comando ao daemon via Unix socket
std::string sendCommand(const std::string& command) {
    // Conectar ao socket
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        return "ERROR: Failed to create socket";
    }
    
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(sock);
        return "ERROR: Daemon not running";
    }
    
    // Enviar comando
    send(sock, command.c_str(), command.size(), 0);
    
    // Receber resposta
    char buffer[4096];
    ssize_t n = recv(sock, buffer, sizeof(buffer) - 1, 0);
    close(sock);
    
    if (n <= 0) {
        return "ERROR: No response from daemon";
    }
    
    buffer[n] = '\0';
    return std::string(buffer);
}

// Parse resposta do daemon (formato: OK|message ou ERROR|message)
void parseResponse(const std::string& response) {
    size_t pos = response.find('|');
    if (pos == std::string::npos) {
        std::cout << response;
        return;
    }
    
    std::string status = response.substr(0, pos);
    std::string message = response.substr(pos + 1);
    
    if (status == "ERROR") {
        std::cerr << "Error: " << message;
    } else {
        std::cout << message;
    }
}

// Ativa o daemon (fork para background)
void activate() {
    // Verificar se já está rodando
    std::ifstream check_pidfile(PID_FILE);
    if (check_pidfile.is_open()) {
        pid_t pid;
        check_pidfile >> pid;
        check_pidfile.close();
        
        if (kill(pid, 0) == 0) {
            std::cout << "Daemon already running (PID: " << pid << ")" << std::endl;
            return;
        }
        
        // PID file obsoleto
        unlink(PID_FILE);
    }
    
    // Fork para background
    pid_t pid = fork();
    
    if (pid < 0) {
        std::cerr << "Failed to fork" << std::endl;
        exit(1);
    }
    
    if (pid > 0) {
        // Processo pai
        std::cout << "Cache daemon activated (PID: " << pid << ")" << std::endl;
        exit(0);
    }
    
    // Processo filho (daemon)
    // Salvar PID
    std::ofstream pidfile(PID_FILE);
    if (pidfile.is_open()) {
        pidfile << getpid();
        pidfile.close();
    }
    
    // Redirecionar stdout/stderr para /dev/null (daemon silencioso)
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    
    // Iniciar daemon
    dns_cache::CacheDaemon daemon;
    daemon.run();
    
    // Cleanup ao terminar
    unlink(PID_FILE);
}

// Desativa o daemon (envia SIGTERM)
void deactivate() {
    std::ifstream pidfile(PID_FILE);
    if (!pidfile.is_open()) {
        std::cout << "Daemon not running" << std::endl;
        return;
    }
    
    pid_t pid;
    pidfile >> pid;
    pidfile.close();
    
    // Enviar SIGTERM
    if (kill(pid, SIGTERM) == 0) {
        std::cout << "Cache daemon deactivated" << std::endl;
        
        // Aguardar um pouco e remover PID file
        sleep(1);
        unlink(PID_FILE);
    } else {
        std::cout << "Failed to stop daemon (PID: " << pid << ")" << std::endl;
        // Remover PID file obsoleto
        unlink(PID_FILE);
    }
}

// Verifica status do daemon
void status() {
    std::ifstream pidfile(PID_FILE);
    if (!pidfile.is_open()) {
        std::cout << "Daemon: Not running" << std::endl;
        return;
    }
    
    pid_t pid;
    pidfile >> pid;
    pidfile.close();
    
    // Verificar se processo existe (kill com signal 0)
    if (kill(pid, 0) == 0) {
        std::cout << "Daemon: Running (PID: " << pid << ")" << std::endl;
        
        // Pegar informações detalhadas do daemon
        std::string response = sendCommand("STATUS");
        parseResponse(response);
    } else {
        std::cout << "Daemon: Not running (stale PID file)" << std::endl;
        unlink(PID_FILE);
    }
}

// Limpa todo o cache
void flush() {
    std::string response = sendCommand("FLUSH");
    parseResponse(response);
}

// Lista cache
void list(const std::string& type) {
    std::string response = sendCommand("LIST " + type);
    parseResponse(response);
}

// Purge cache
void purge(const std::string& type) {
    std::string response = sendCommand("PURGE " + type);
    parseResponse(response);
}

// Configura tamanho de cache
void setCache(const std::string& type, size_t size) {
    std::string cmd;
    if (type == "positive") {
        cmd = "SET_POSITIVE " + std::to_string(size);
    } else if (type == "negative") {
        cmd = "SET_NEGATIVE " + std::to_string(size);
    } else {
        std::cerr << "Invalid cache type: " << type << std::endl;
        return;
    }
    
    std::string response = sendCommand(cmd);
    parseResponse(response);
}

// Mostra ajuda
void showHelp(const char* prog_name) {
    std::cout << "Cache Daemon - DNS Cache Management\n\n";
    std::cout << "USAGE:\n\n";
    std::cout << "  Lifecycle:\n";
    std::cout << "    " << prog_name << " --activate           Start daemon in background\n";
    std::cout << "    " << prog_name << " --deactivate         Stop daemon\n";
    std::cout << "    " << prog_name << " --status             Check daemon status\n\n";
    std::cout << "  Management:\n";
    std::cout << "    " << prog_name << " --flush              Clear all cache\n";
    std::cout << "    " << prog_name << " --set positive N     Set positive cache size\n";
    std::cout << "    " << prog_name << " --set negative N     Set negative cache size\n";
    std::cout << "    " << prog_name << " --purge positive     Clear positive cache\n";
    std::cout << "    " << prog_name << " --purge negative     Clear negative cache\n";
    std::cout << "    " << prog_name << " --purge all          Clear all cache\n";
    std::cout << "    " << prog_name << " --list positive      List positive cache\n";
    std::cout << "    " << prog_name << " --list negative      List negative cache\n";
    std::cout << "    " << prog_name << " --list all           List all cache\n\n";
    std::cout << "EXAMPLES:\n\n";
    std::cout << "  # Start daemon\n";
    std::cout << "  " << prog_name << " --activate\n\n";
    std::cout << "  # Configure cache\n";
    std::cout << "  " << prog_name << " --set positive 100\n";
    std::cout << "  " << prog_name << " --set negative 50\n\n";
    std::cout << "  # Check status\n";
    std::cout << "  " << prog_name << " --status\n\n";
    std::cout << "  # Stop daemon\n";
    std::cout << "  " << prog_name << " --deactivate\n\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        showHelp(argv[0]);
        return 1;
    }
    
    std::string cmd = argv[1];
    
    // Comandos de ciclo de vida
    if (cmd == "--activate") {
        activate();
    } else if (cmd == "--deactivate") {
        deactivate();
    } else if (cmd == "--status") {
        status();
    } else if (cmd == "--flush") {
        flush();
    } else if (cmd == "--help" || cmd == "-h") {
        showHelp(argv[0]);
    }
    // Comandos com argumentos
    else if (cmd == "--set" && argc >= 4) {
        std::string type = argv[2];
        size_t size = std::atoi(argv[3]);
        setCache(type, size);
    } else if (cmd == "--purge" && argc >= 3) {
        std::string type = argv[2];
        purge(type);
    } else if (cmd == "--list" && argc >= 3) {
        std::string type = argv[2];
        list(type);
    } else {
        std::cerr << "Unknown command or missing arguments\n";
        std::cerr << "Use --help for usage information\n";
        return 1;
    }
    
    return 0;
}

