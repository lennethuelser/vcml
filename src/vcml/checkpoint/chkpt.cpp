#include "vcml/checkpoint/chkpt.h"

#include "vcml/logging/logger.h"

#include <unistd.h>
#include <criu/criu.h>
#include <fcntl.h>
#include <filesystem>

#include <chrono>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

namespace vcml {
namespace checkpoint {

chkpt chkpt::s_chkpt;//("chkpt");

chkpt::chkpt():
    m_socket_path("criu_service.socket"),  
    m_log_lvl(LOG_LVL::LOG_DEBUG),         
    m_log_file_path("criu.log"),           
    m_img_dir("checkpoints"),              
    m_img_dir_fd(-1),                      
    m_initialized(false)
{}


void chkpt::init() {
    if (m_initialized) {
        return;
    }

    log_info("Initizalizing chkpt");
    VCML_ERROR_ON(criu_init_opts() != 0, "failed to initialize criu");

    VCML_ERROR_ON(!std::filesystem::is_socket(m_socket_path),
                  "The criu socket `%s` does not exist",
                  m_socket_path.c_str());
    criu_set_service_address(m_socket_path.c_str());

    if (!std::filesystem::is_directory(m_img_dir)) {
        log_warn("criu image dir `%s` does not exist - creating dir...",
                 m_img_dir.c_str());
        std::filesystem::create_directories(m_img_dir);
    }

    VCML_ERROR_ON(!std::filesystem::is_directory(m_img_dir),
                  "criu image dir `%s` does not exist", m_img_dir.c_str());
    m_img_dir_fd = open(m_img_dir.c_str(), O_DIRECTORY);
    VCML_ERROR_ON(m_img_dir_fd < 0, "failed to open criu image dir `%s`",
                  m_img_dir.c_str());
    criu_set_images_dir_fd(m_img_dir_fd);

    criu_set_log_file(m_log_file_path.c_str());
    criu_set_log_level(m_log_lvl);

    m_initialized = true;
}

chkpt& chkpt::instance() {
    return s_chkpt;
}

pid_t get_tracer_pid(pid_t pid) {
    std::stringstream proc_status_path;
    proc_status_path << "/proc/" << pid << "/status";

    std::ifstream status_fs;
    status_fs.open(proc_status_path.str(), std::ios_base::in);
    for (std::string line; std::getline(status_fs, line);) {
        const std::string search_str("TracerPid:\t");
        size_t pos = line.find(search_str);
        if (pos != std::string::npos) {
            std::string pid_str = line.substr(pos + search_str.length());
            return std::stoi(pid_str);
        }
    }
    return -1;
}

void chkpt::checkpoint() {
    pid_t sim_pid = getpid();
    pid_t pid = fork(); // fork to not have the criu unix socket connected

 
    if (pid > 0) {    // parent: wait for child to exit before continue
		int status;
		waitpid(pid, &status, 0);
    }
    else{
        if (!m_initialized) {
            init();
        }

        pid_t tracer_pid = get_tracer_pid(sim_pid);
        VCML_ERROR_ON(tracer_pid == -1, "Failed to read tracer pid");
        VCML_ERROR_ON(tracer_pid != 0, "traced by %d", tracer_pid);

        criu_set_pid(sim_pid);

        /*
        * The simulation shares common resources with the shell (e.g., session,
        * terminal). `--shell-job` needs to be enabled, because only a part of the
        * session is dumped (e.g., slave end of a tty pair)
        */
        criu_set_shell_job(true);

        /*
        * leave simulation running after dump because of pipes
        */
        criu_set_leave_running(true);

        /*
        * Checkpoint established TCP connections
        */
        criu_set_tcp_established(true);
        
        VCML_ERROR_ON(is_error(criu_dump()), "Failed to create checkpoint");

        struct stat st;
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/ns/ipc", getpid());

        if (stat(path, &st) == 0) {
            log_info("(VP) IPC namespace inode: %ld", static_cast<long>(st.st_ino));
        } else {
            log_info("Failed to stat IPC namespace");
        }

        pid_t ppid = getppid();
        log_info("(VP) This is parent ID: %d",  static_cast<int>(ppid));
    
        log_info("Finished dump");
        exit(0); // exit forked child process
    }
}

const std::string& chkpt::get_img_dir() const {
    return m_img_dir;
}

bool chkpt::is_error(const int retval) const {
    switch (retval) {
    case -EBADE:
        log_error("RPC has returned fail");
        return true;
    case -ECONNREFUSED:
        log_error("Unable to connect to CRIU");
        return true;
    case -ECOMM:
        log_error("Unable to send/recv msg to/from CRIU");
        return true;
    case -EINVAL:
        log_error(
            "CRIU doesn't support this type of request. You should probably "
            "update CRIU.");
        return true;
    case -EBADMSG:
        log_error(
            "Unexpected response from CRIU. You should probably update CRIU.");
        return true;
    default:
        if (retval < 0) {
            log_error("Unknown CRIU error");
            return true;
        }
        return false;
    }
}

/*void chkpt::reset() {
    // nothing to do
}*/


} // namespace checkpoint
} // namespace vcml
